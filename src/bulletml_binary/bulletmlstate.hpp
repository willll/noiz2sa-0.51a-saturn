#ifndef BULLETMLSTATE_HPP_
#define BULLETMLSTATE_HPP_

#include <cstddef>
#include <cstdint>
#include <new>
#include <srl.hpp>
#include <srl_log.hpp>

#include "../bulletml_runtime_factory.h"

using SRL::Math::Types::Fxp;

class BulletMLParserBLB;
class BulletMLNode;

namespace bulletml_state_pool {
struct StateFreeNode {
    StateFreeNode* next;
};

struct StatePoolState {
    static inline StateFreeNode* freeList = nullptr;
    static inline std::size_t cachedCount = 0;
    static inline bool startupOnlyAllocation = false;
    static inline bool startupPreallocationPhase = false;
};

template <typename T>
struct ArrayFreeNode {
    ArrayFreeNode* next;
};

template <typename T>
struct ArrayPoolState {
    static inline ArrayFreeNode<T>* freeLists[8] = {nullptr, nullptr, nullptr, nullptr,
                                                    nullptr, nullptr, nullptr, nullptr};
    static inline std::size_t cachedCounts[8] = {0, 0, 0, 0, 0, 0, 0, 0};
};

inline uint16_t getBucketedCapacity(uint16_t count) {
    if (count == 0) {
        return 0;
    }

    const uint16_t rounded = static_cast<uint16_t>(((count + 3u) / 4u) * 4u);
    return (rounded <= 32u) ? rounded : 0u;
}

template <typename T>
inline T* createPooledArray(uint16_t count, uint16_t& outCapacity) {
    outCapacity = getBucketedCapacity(count);
    if (outCapacity > 0u) {
        const uint16_t bucketIndex = static_cast<uint16_t>((outCapacity / 4u) - 1u);
        ArrayFreeNode<T>*& freeList = ArrayPoolState<T>::freeLists[bucketIndex];
        if (freeList) {
            ArrayFreeNode<T>* node = freeList;
            freeList = node->next;
            if (ArrayPoolState<T>::cachedCounts[bucketIndex] > 0u) {
                ArrayPoolState<T>::cachedCounts[bucketIndex]--;
            }
            return reinterpret_cast<T*>(node);
        }

        if (StatePoolState::startupOnlyAllocation && !StatePoolState::startupPreallocationPhase) {
            return nullptr;
        }

        return createBulletMlRuntimeArray<T>(outCapacity);
    }

    if (StatePoolState::startupOnlyAllocation && !StatePoolState::startupPreallocationPhase) {
        outCapacity = count;
        return nullptr;
    }

    outCapacity = count;
    return createBulletMlRuntimeArray<T>(count);
}

template <typename T>
inline void recyclePooledArray(T*& ptr, uint16_t capacity) {
    if (!ptr) {
        return;
    }

    const uint16_t bucketedCapacity = getBucketedCapacity(capacity);
    if (bucketedCapacity > 0u && bucketedCapacity == capacity) {
        const uint16_t bucketIndex = static_cast<uint16_t>((bucketedCapacity / 4u) - 1u);
        ArrayFreeNode<T>* node = reinterpret_cast<ArrayFreeNode<T>*>(ptr);
        node->next = ArrayPoolState<T>::freeLists[bucketIndex];
        ArrayPoolState<T>::freeLists[bucketIndex] = node;
        ArrayPoolState<T>::cachedCounts[bucketIndex]++;
        ptr = nullptr;
        return;
    }

    destroyBulletMlRuntimeArray(ptr, capacity);
}

template <typename T>
inline void releaseArrayPool() {
    for (uint16_t bucketIndex = 0; bucketIndex < 8u; ++bucketIndex) {
        ArrayFreeNode<T>* node = ArrayPoolState<T>::freeLists[bucketIndex];
        while (node) {
            ArrayFreeNode<T>* next = node->next;
            T* ptr = reinterpret_cast<T*>(node);
            const uint16_t capacity = static_cast<uint16_t>((bucketIndex + 1u) * 4u);
            destroyBulletMlRuntimeArray(ptr, capacity);
            node = next;
        }
        ArrayPoolState<T>::freeLists[bucketIndex] = nullptr;
        ArrayPoolState<T>::cachedCounts[bucketIndex] = 0;
    }
}

template <typename T>
inline std::size_t getArrayPoolCachedCount(uint16_t capacity) {
    const uint16_t bucketedCapacity = getBucketedCapacity(capacity);
    if (bucketedCapacity == 0u || bucketedCapacity != capacity) {
        return 0u;
    }
    const uint16_t bucketIndex = static_cast<uint16_t>((bucketedCapacity / 4u) - 1u);
    return ArrayPoolState<T>::cachedCounts[bucketIndex];
}
}

/// BulletML state passed to child bullets; no STL and compact allocations.
class BulletMLState {
public:
    BulletMLState(BulletMLParserBLB* parser,
                  BulletMLNode** nodes,
                  uint16_t node_count,
                  const Fxp* parameters,
                  uint16_t parameter_count)
        : parser_(parser),
          nodes_(nodes),
          node_count_(node_count),
          node_capacity_(bulletml_state_pool::getBucketedCapacity(node_count) > 0u
              ? bulletml_state_pool::getBucketedCapacity(node_count)
              : node_count),
          parameters_(nullptr),
          parameter_count_(0),
          parameter_capacity_(0) {
        if (parameter_count > 0 && parameters) {
            static uint32_t sParameterCopyLogs = 0;
            if (sParameterCopyLogs < 12 || (sParameterCopyLogs % 64) == 0) {
                SRL::Logger::LogInfo("[BML-STATE] param copy count=%u node_count=%u",
                                     static_cast<unsigned>(parameter_count),
                                     static_cast<unsigned>(node_count));
            }
            ++sParameterCopyLogs;
            parameters_ = bulletml_state_pool::createPooledArray<Fxp>(parameter_count, parameter_capacity_);
            if (parameters_) {
                for (uint16_t i = 0; i < parameter_count; ++i) {
                    parameters_[i] = parameters[i];
                }
                parameter_count_ = parameter_count;
            }
            else {
                SRL::Logger::LogWarning("[BML-STATE] param copy allocation failed count=%u node_count=%u",
                                        static_cast<unsigned>(parameter_count),
                                        static_cast<unsigned>(node_count));
            }
        }
    }

    explicit BulletMLState(BulletMLParserBLB* parser = nullptr)
        : parser_(parser), nodes_(nullptr), node_count_(0),
                    node_capacity_(0), parameters_(nullptr), parameter_count_(0), parameter_capacity_(0) {}

    ~BulletMLState() {
                bulletml_state_pool::recyclePooledArray(nodes_, node_capacity_);
        nodes_ = nullptr;
        node_count_ = 0;
                node_capacity_ = 0;
                bulletml_state_pool::recyclePooledArray(parameters_, parameter_capacity_);
        parameters_ = nullptr;
        parameter_count_ = 0;
                parameter_capacity_ = 0;
    }

    BulletMLParserBLB* getParser() const { return parser_; }
    void setParser(BulletMLParserBLB* parser) { parser_ = parser; }

    BulletMLNode** getNodes() const { return nodes_; }
    uint16_t getNodeCount() const { return node_count_; }

    const Fxp* getParameters() const { return parameters_; }
    uint16_t getParameterCount() const { return parameter_count_; }

private:
    BulletMLParserBLB* parser_;
    BulletMLNode** nodes_;
    uint16_t node_count_;
    uint16_t node_capacity_;
    Fxp* parameters_;
    uint16_t parameter_count_;
    uint16_t parameter_capacity_;

    BulletMLState(const BulletMLState&);
    BulletMLState& operator=(const BulletMLState&);
};

inline BulletMLNode** createBulletMlStateNodeArray(uint16_t count)
{
    uint16_t capacity = 0;
    return bulletml_state_pool::createPooledArray<BulletMLNode*>(count, capacity);
}

inline void destroyBulletMlStateNodeArray(BulletMLNode**& ptr, uint16_t count)
{
    const uint16_t capacity = bulletml_state_pool::getBucketedCapacity(count) > 0u
        ? bulletml_state_pool::getBucketedCapacity(count)
        : count;
    bulletml_state_pool::recyclePooledArray(ptr, capacity);
}

inline BulletMLState* createBulletMlState(BulletMLParserBLB* parser,
                                          BulletMLNode** nodes,
                                          uint16_t node_count,
                                          const Fxp* parameters,
                                          uint16_t parameter_count)
{
    using bulletml_state_pool::StateFreeNode;
    using bulletml_state_pool::StatePoolState;

    if (StatePoolState::freeList) {
        StateFreeNode* node = StatePoolState::freeList;
        StatePoolState::freeList = node->next;
        if (StatePoolState::cachedCount > 0u) {
            StatePoolState::cachedCount--;
        }
        return new (node) BulletMLState(parser, nodes, node_count, parameters, parameter_count);
    }

    if (StatePoolState::startupOnlyAllocation && !StatePoolState::startupPreallocationPhase) {
        return nullptr;
    }

    return createBulletMlRuntimeObject<BulletMLState>(parser, nodes, node_count, parameters, parameter_count);
}

inline void destroyBulletMlState(BulletMLState*& ptr)
{
    using bulletml_state_pool::StateFreeNode;
    using bulletml_state_pool::StatePoolState;

    if (!ptr) {
        return;
    }

    ptr->~BulletMLState();
    StateFreeNode* node = reinterpret_cast<StateFreeNode*>(ptr);
    node->next = StatePoolState::freeList;
    StatePoolState::freeList = node;
    StatePoolState::cachedCount++;
    ptr = nullptr;
}

inline void releaseBulletMlStatePools()
{
    using bulletml_state_pool::StateFreeNode;
    using bulletml_state_pool::StatePoolState;

    while (StatePoolState::freeList) {
        StateFreeNode* node = StatePoolState::freeList;
        StatePoolState::freeList = node->next;
        freeBulletMlRuntimeRaw(node, sizeof(BulletMLState), false);
    }
    StatePoolState::cachedCount = 0;
    StatePoolState::startupPreallocationPhase = false;

    bulletml_state_pool::releaseArrayPool<BulletMLNode*>();
    bulletml_state_pool::releaseArrayPool<Fxp>();
}

inline void enableBulletMlStateStartupOnlyAllocation(bool enabled)
{
    bulletml_state_pool::StatePoolState::startupOnlyAllocation = enabled;
}

inline bool isBulletMlStateStartupOnlyAllocationEnabled()
{
    return bulletml_state_pool::StatePoolState::startupOnlyAllocation;
}

inline void beginBulletMlStateStartupPreallocation()
{
    bulletml_state_pool::StatePoolState::startupPreallocationPhase = true;
}

inline void endBulletMlStateStartupPreallocation()
{
    bulletml_state_pool::StatePoolState::startupPreallocationPhase = false;
}

inline bool isBulletMlStateStartupPreallocationPhase()
{
    return bulletml_state_pool::StatePoolState::startupPreallocationPhase;
}

inline bool preallocateBulletMlStatePools(uint16_t stateCount,
                                          const uint16_t* nodeArrayCounts,
                                          const uint16_t* parameterArrayCounts)
{
    using bulletml_state_pool::StateFreeNode;
    using bulletml_state_pool::StatePoolState;

    beginBulletMlStateStartupPreallocation();

    for (uint16_t i = 0; i < stateCount; ++i) {
        BulletMLState* state = createBulletMlRuntimeObject<BulletMLState>(nullptr);
        if (!state) {
            endBulletMlStateStartupPreallocation();
            return false;
        }

        StateFreeNode* node = reinterpret_cast<StateFreeNode*>(state);
        node->next = StatePoolState::freeList;
        StatePoolState::freeList = node;
        StatePoolState::cachedCount++;
    }

    for (uint16_t bucketIndex = 0; bucketIndex < 8u; ++bucketIndex) {
        const uint16_t capacity = static_cast<uint16_t>((bucketIndex + 1u) * 4u);
        const uint16_t nodeCount = nodeArrayCounts ? nodeArrayCounts[bucketIndex] : 0u;
        const uint16_t paramCount = parameterArrayCounts ? parameterArrayCounts[bucketIndex] : 0u;

        for (uint16_t i = 0; i < nodeCount; ++i) {
            BulletMLNode** ptr = createBulletMlRuntimeArray<BulletMLNode*>(capacity);
            if (!ptr) {
                endBulletMlStateStartupPreallocation();
                return false;
            }

            auto* node = reinterpret_cast<bulletml_state_pool::ArrayFreeNode<BulletMLNode*>*>(ptr);
            node->next = bulletml_state_pool::ArrayPoolState<BulletMLNode*>::freeLists[bucketIndex];
            bulletml_state_pool::ArrayPoolState<BulletMLNode*>::freeLists[bucketIndex] = node;
            bulletml_state_pool::ArrayPoolState<BulletMLNode*>::cachedCounts[bucketIndex]++;
        }

        for (uint16_t i = 0; i < paramCount; ++i) {
            Fxp* ptr = createBulletMlRuntimeArray<Fxp>(capacity);
            if (!ptr) {
                endBulletMlStateStartupPreallocation();
                return false;
            }

            auto* node = reinterpret_cast<bulletml_state_pool::ArrayFreeNode<Fxp>*>(ptr);
            node->next = bulletml_state_pool::ArrayPoolState<Fxp>::freeLists[bucketIndex];
            bulletml_state_pool::ArrayPoolState<Fxp>::freeLists[bucketIndex] = node;
            bulletml_state_pool::ArrayPoolState<Fxp>::cachedCounts[bucketIndex]++;
        }
    }

    endBulletMlStateStartupPreallocation();
    return true;
}

inline std::size_t getBulletMlStateCachedCount()
{
    return bulletml_state_pool::StatePoolState::cachedCount;
}

inline std::size_t getBulletMlStateNodeArrayCachedCount(uint16_t capacity)
{
    return bulletml_state_pool::getArrayPoolCachedCount<BulletMLNode*>(capacity);
}

inline std::size_t getBulletMlStateParameterArrayCachedCount(uint16_t capacity)
{
    return bulletml_state_pool::getArrayPoolCachedCount<Fxp>(capacity);
}

inline uint16_t getBulletMlStateBucketedCapacity(uint16_t count)
{
    return bulletml_state_pool::getBucketedCapacity(count);
}

#endif // BULLETMLSTATE_HPP_
