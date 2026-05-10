#ifndef BULLETMLSTATE_HPP_
#define BULLETMLSTATE_HPP_

#include <cstddef>
#include <cstdint>
#include <srl.hpp>
#include <srl_log.hpp>

using SRL::Math::Types::Fxp;

class BulletMLParserBLB;
class BulletMLNode;

/// BulletML state passed to child bullets; no STL and compact allocations.
class BulletMLState {
public:
    static constexpr uint16_t kMaxNodes = 8;
    static constexpr uint16_t kMaxParameters = 16;
    static constexpr uint16_t kPoolCapacity = 384;

    static uint32_t getPoolRejectCount() { return sPoolRejectCount; }
    static void resetPoolRejectCount() { sPoolRejectCount = 0; }

    BulletMLState(BulletMLParserBLB* parser,
                  BulletMLNode* const* nodes,
                  uint16_t node_count,
                  const Fxp* parameters,
                  uint16_t parameter_count)
        : parser_(parser),
          node_count_(0),
          parameter_count_(0) {
        if (node_count > kMaxNodes) {
            SRL::Logger::LogWarning("[BML-STATE] Node count %u exceeds cap %u; truncating", node_count, kMaxNodes);
            node_count = kMaxNodes;
        }

        if (nodes && node_count > 0) {
            for (uint16_t i = 0; i < node_count; ++i) {
                nodes_[i] = nodes[i];
            }
            node_count_ = node_count;
        }

        if (parameter_count > kMaxParameters) {
            SRL::Logger::LogWarning("[BML-STATE] Param count %u exceeds cap %u; truncating", parameter_count, kMaxParameters);
            parameter_count = kMaxParameters;
        }

        if (parameter_count > 0 && parameters) {
            for (uint16_t i = 0; i < parameter_count; ++i) {
                parameters_[i] = parameters[i];
            }
            parameter_count_ = parameter_count;
        }
    }

    explicit BulletMLState(BulletMLParserBLB* parser = nullptr)
        : parser_(parser), node_count_(0), parameter_count_(0) {}

    ~BulletMLState() {
        node_count_ = 0;
        parameter_count_ = 0;
    }

    static void* operator new(std::size_t) noexcept;
    static void operator delete(void* ptr) noexcept;

    BulletMLParserBLB* getParser() const { return parser_; }
    void setParser(BulletMLParserBLB* parser) { parser_ = parser; }

    BulletMLNode** getNodes() const { return const_cast<BulletMLNode**>(nodes_); }
    uint16_t getNodeCount() const { return node_count_; }

    const Fxp* getParameters() const { return parameters_; }
    uint16_t getParameterCount() const { return parameter_count_; }

private:
    static inline uint32_t sPoolRejectCount = 0;

    BulletMLParserBLB* parser_;
    BulletMLNode* nodes_[kMaxNodes] = {};
    uint16_t node_count_;
    Fxp parameters_[kMaxParameters] = {};
    uint16_t parameter_count_;

    BulletMLState(const BulletMLState&);
    BulletMLState& operator=(const BulletMLState&);
};

namespace bulletml_state_pool_internal {
static constexpr uint32_t kPoolGuardHead = 0xB5117E01u;
static constexpr uint32_t kPoolGuardTail = 0xE107511Bu;

struct Storage {
    uint32_t guardHead;
    alignas(std::max_align_t) unsigned char pool[BulletMLState::kPoolCapacity][sizeof(BulletMLState)];
    bool used[BulletMLState::kPoolCapacity];
    uint32_t guardTail;
};

inline Storage& getStorage();

inline void verifyStorageGuards(const char* tag)
{
    auto& storage = getStorage();
    if (storage.guardHead == 0u && storage.guardTail == 0u)
    {
        storage.guardHead = kPoolGuardHead;
        storage.guardTail = kPoolGuardTail;
        return;
    }

    if (storage.guardHead != kPoolGuardHead || storage.guardTail != kPoolGuardTail)
    {
        SRL::Logger::LogFatal("[BML-STATE] Pool guard fail (%s)", tag);
        SRL::System::Exit(1);
    }
}

inline Storage& getStorage()
{
    static Storage storage = {};
    return storage;
}
} // namespace bulletml_state_pool_internal

inline void* BulletMLState::operator new(std::size_t) noexcept
{
    bulletml_state_pool_internal::verifyStorageGuards("new");
    auto& sStorage = bulletml_state_pool_internal::getStorage();

    for (uint16_t i = 0; i < kPoolCapacity; ++i)
    {
        if (!sStorage.used[i])
        {
            sStorage.used[i] = true;
            return (void*)&sStorage.pool[i][0];
        }
    }

    sPoolRejectCount++;
    SRL::Logger::LogWarning("[BML-STATE] Pool exhausted (%u); rejecting allocation", kPoolCapacity);
    return nullptr;
}

inline void BulletMLState::operator delete(void* ptr) noexcept
{
    if (!ptr)
    {
        return;
    }

    bulletml_state_pool_internal::verifyStorageGuards("del");
    auto& sStorage = bulletml_state_pool_internal::getStorage();

    const uintptr_t poolStart = (uintptr_t)&sStorage.pool[0][0];
    const uintptr_t poolEnd = (uintptr_t)&sStorage.pool[kPoolCapacity - 1][0] + sizeof(sStorage.pool[0]);
    const uintptr_t p = (uintptr_t)ptr;
    if (p < poolStart || p >= poolEnd)
    {
        return;
    }

    const uintptr_t offset = p - poolStart;
    if ((offset % sizeof(sStorage.pool[0])) != 0u)
    {
        SRL::Logger::LogFatal("[BML-STATE] Bad free ptr=%08lx", (unsigned long)p);
        SRL::System::Exit(1);
    }

    const uint16_t slot = (uint16_t)(offset / sizeof(sStorage.pool[0]));
    if (slot < kPoolCapacity)
    {
        sStorage.used[slot] = false;
    }
}

#endif // BULLETMLSTATE_HPP_
