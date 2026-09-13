#ifndef BULLETMLRUNNER_HPP_
#define BULLETMLRUNNER_HPP_

#include <cstdint>
#include <cstddef>
#include <srl.hpp>

#include "bulletmlparser_blb.hpp"
#include "bulletml_alloc_latch.h"
#include <srl_log.hpp>

namespace bulletml_runner_trace {
struct RunnerAllocStats {
    std::size_t objectAllocCalls = 0;
    std::size_t objectFreeCalls = 0;
    std::size_t arrayAllocCalls = 0;
    std::size_t arrayFreeCalls = 0;
    std::size_t objectLiveBytes = 0;
    std::size_t arrayLiveBytes = 0;
};

inline RunnerAllocStats& stats() {
    static RunnerAllocStats s{};
    return s;
}

inline void noteAlloc(std::size_t bytes, bool arrayAlloc) {
    RunnerAllocStats& s = stats();
    if (arrayAlloc) {
        s.arrayAllocCalls++;
        s.arrayLiveBytes += bytes;
    } else {
        s.objectAllocCalls++;
        s.objectLiveBytes += bytes;
    }
}

inline void noteFree(std::size_t bytes, bool arrayFree) {
    RunnerAllocStats& s = stats();
    if (arrayFree) {
        s.arrayFreeCalls++;
        if (s.arrayLiveBytes >= bytes) {
            s.arrayLiveBytes -= bytes;
        } else {
            s.arrayLiveBytes = 0;
        }
    } else {
        s.objectFreeCalls++;
        if (s.objectLiveBytes >= bytes) {
            s.objectLiveBytes -= bytes;
        } else {
            s.objectLiveBytes = 0;
        }
    }
}
}

/**
 * BulletML Task Stack Memory Optimization (Hardware Stress Test Hardening):
 *
 * The task stack now uses a two-part growth strategy to reduce HWRAM pressure:
 * 1. Initial capacity set to 32 (instead of 16) to avoid immediate growth during
 *    typical patterns. Cost: +512 bytes per foe (~1% HWRAM overhead).
 * 2. Growth by +32 per expansion (instead of doubling) to reduce fragmentation and
 *    avoid oversizing when only a few additional tasks are needed.
 *    Example: 32 → 64 → 96 → 128 (instead of 32 → 64 → 128 → 256)
 *
 * Peak task utilization is now tracked (getPeakTaskCount()) to validate that the
 * optimization prevents future allocation failures under high-fanout BulletML patterns.
 * A single failure was observed at capacity=16 when an action fanout of 114 children
 * needed to grow to 128 tasks; starting at 32 with linear growth eliminates that spike.
 */


inline void logBulletMlAllocFailure(const char* tag, uint32_t count = 0) {
    const uint32_t failCount = recordBulletMlAllocFailure();
    if (failCount <= 8 || (failCount % 64) == 0) {
        if (count > 0) {
            SRL::Logger::LogWarning("[BML-ALLOC] failed tag=%s count=%lu total_fail=%lu",
                                    tag,
                                    (unsigned long)count,
                                    (unsigned long)failCount);
        } else {
            SRL::Logger::LogWarning("[BML-ALLOC] failed tag=%s total_fail=%lu",
                                    tag,
                                    (unsigned long)failCount);
        }
    }
}

template <typename T>
inline T* allocBulletMlArray(const char* tag, uint32_t count) {
    if (count == 0) return nullptr;
    if (hasBulletMlAllocFailureLatched()) {
        return nullptr;
    }
    bulletml_runner_trace::noteAlloc(sizeof(T) * count, true);
    T* ptr = lwnew T[count];
    if (!ptr) {
        bulletml_runner_trace::noteFree(sizeof(T) * count, true);
        logBulletMlAllocFailure(tag, count);
    }
    return ptr;
}

template <typename T, typename... Args>
inline T* allocBulletMlObject(const char* tag, Args... args) {
    if (hasBulletMlAllocFailureLatched()) {
        return nullptr;
    }
    bulletml_runner_trace::noteAlloc(sizeof(T), false);
    T* ptr = lwnew T(args...);
    if (!ptr) {
        bulletml_runner_trace::noteFree(sizeof(T), false);
        logBulletMlAllocFailure(tag);
    }
    return ptr;
}

template <typename T>
inline void freeBulletMlObject(T*& ptr) {
    if (!ptr) {
        return;
    }
    bulletml_runner_trace::noteFree(sizeof(T), false);
    delete ptr;
    ptr = nullptr;
}

template <typename T>
inline void freeBulletMlArray(T*& ptr, uint32_t count) {
    if (!ptr) {
        return;
    }
    bulletml_runner_trace::noteFree(sizeof(T) * count, true);
    delete[] ptr;
    ptr = nullptr;
}

inline std::size_t getBulletMlRunnerObjectLiveBytes() {
    return bulletml_runner_trace::stats().objectLiveBytes;
}

inline std::size_t getBulletMlRunnerArrayLiveBytes() {
    return bulletml_runner_trace::stats().arrayLiveBytes;
}

inline std::size_t getBulletMlRunnerLiveBytes() {
    return getBulletMlRunnerObjectLiveBytes() + getBulletMlRunnerArrayLiveBytes();
}

inline void getBulletMlRunnerObjectCallCounts(std::size_t& allocCalls, std::size_t& freeCalls) {
    const bulletml_runner_trace::RunnerAllocStats& s = bulletml_runner_trace::stats();
    allocCalls = s.objectAllocCalls;
    freeCalls = s.objectFreeCalls;
}

inline void getBulletMlRunnerArrayCallCounts(std::size_t& allocCalls, std::size_t& freeCalls) {
    const bulletml_runner_trace::RunnerAllocStats& s = bulletml_runner_trace::stats();
    allocCalls = s.arrayAllocCalls;
    freeCalls = s.arrayFreeCalls;
}

using SRL::Math::Types::Fxp;

class BulletMLRunnerImpl;

/// BulletML Runner - Base class for BulletML execution.
class BulletMLRunner {
public:
    explicit BulletMLRunner(BulletMLParserBLB* parser);
    explicit BulletMLRunner(BulletMLState* state);
    virtual ~BulletMLRunner();

    /// Get bullet direction (degrees)
    virtual Fxp getBulletDirection() { return Fxp::Convert(0); }

    /// Get aim direction (degrees)
    virtual Fxp getAimDirection() { return Fxp::Convert(0); }

    /// Get bullet speed
    virtual Fxp getBulletSpeed() { return Fxp::Convert(0); }

    /// Get default speed
    virtual Fxp getDefaultSpeed() { return Fxp::Convert(1); }

    /// Get difficulty rank [0..1]
    virtual Fxp getRank() { return Fxp::Convert(0); }

    /// Random source for formulas
    virtual Fxp getRand() { return Fxp::Convert(0); }

    /// Create a simple bullet
    virtual void createSimpleBullet(Fxp direction, Fxp speed) {
        (void)direction;
        (void)speed;
    }

    /// Create a bullet with action state
    virtual void createBullet(BulletMLState* state, Fxp direction, Fxp speed) {
        (void)state;
        (void)direction;
        (void)speed;
    }

    /// Current turn/frame
    virtual int getTurn() { return 0; }

    /// Vanish callback
    virtual void doVanish() {}

    /// Optional motion/accel hooks
    virtual void doChangeDirection(Fxp) {}
    virtual void doChangeSpeed(Fxp) {}
    virtual void doAccelX(Fxp) {}
    virtual void doAccelY(Fxp) {}
    virtual Fxp getBulletSpeedX() { return Fxp::Convert(0); }
    virtual Fxp getBulletSpeedY() { return Fxp::Convert(0); }

    /// Execute one frame worth of commands
    virtual void run();

    /// Check if all command streams are done
    virtual bool isEnd();

    BulletMLParserBLB* getParser() const { return parser_; }

protected:
    BulletMLParserBLB* parser_;
    BulletMLState* state_;
    BulletMLRunnerImpl** impls_;
    uint16_t impl_count_;
    uint16_t impl_capacity_;

private:
    BulletMLRunner(const BulletMLRunner&);
    BulletMLRunner& operator=(const BulletMLRunner&);
};

class BulletMLRunnerImpl {
public:
    BulletMLRunnerImpl(BulletMLState* state, BulletMLRunner* runner)
        : parser_(nullptr),
          runner_(runner),
          end_(false),
          wait_until_turn_(0),
          tasks_(nullptr),
          task_count_(0),
          task_capacity_(0),
          params_(nullptr),
          param_count_(0),
          owns_params_(false),
          change_dir_active_(false),
          change_speed_active_(false),
          accel_x_active_(false),
          accel_y_active_(false),
          has_spd_(false),
          has_dir_(false),
          has_prev_spd_(false),
          has_prev_dir_(false),
          spd_(Fxp::Convert(0)),
          dir_(Fxp::Convert(0)),
          prev_spd_(Fxp::Convert(0)),
          prev_dir_(Fxp::Convert(0)),
          expand_name_(0),
          expand_ref_id_(0),
          expand_fanout_(0),
          expand_child_count_(0),
          peak_task_count_(0) {
        if (!state || !runner_) {
            end_ = true;
            return;
        }

        if (!isLikelySh2RamPointer(runner_)) {
            SRL::Logger::LogWarning(
                "[BML-RUNNER] invalid runner pointer in ctor this=%p runner=%p",
                static_cast<void*>(this),
                static_cast<void*>(runner_));
            end_ = true;
            destroyBulletMlState(state);
            return;
        }

        parser_ = state->getParser();

        const uint16_t nc = static_cast<uint16_t>(state->getNodeCount());
        BulletMLNode** const stateNodes = (nc > 0) ? state->getNodes() : nullptr;

        copyParameters(state->getParameters(), state->getParameterCount());

        if (!ensureTaskCapacity(static_cast<uint16_t>(nc + 8))) {
            destroyBulletMlState(state);
            end_ = true;
            return;
        }

        if (nc > 0 && stateNodes) {
            for (int i = static_cast<int>(nc) - 1; i >= 0; --i) {
                pushNodeTask(stateNodes[i]);
            }
        }

        destroyBulletMlState(state);
        wait_until_turn_ = runner_->getTurn();
    }

    ~BulletMLRunnerImpl() {
        recycleTaskBuffer(tasks_, task_capacity_);
        tasks_ = nullptr;
        task_count_ = 0;
        task_capacity_ = 0;

        if (owns_params_) {
            bulletml_state_pool::recyclePooledArray(params_, bulletml_state_pool::getBucketedCapacity(param_count_));
        }
        params_ = nullptr;
        param_count_ = 0;
        owns_params_ = false;
    }

    bool isEnd() const { return end_; }

    uint16_t getPeakTaskCount() const { return peak_task_count_; }

    // Initialise the task-buffer slab pool.  Call once at startup after LWRAM
    // is otherwise settled (background buffers allocated, BML parsers loaded).
    // Returns true on success; false if the LWRAM allocation failed.
    static bool InitTaskPool() {
        const bool ok = getTaskPool().init();
        if (ok) {
            SRL::Logger::LogInfo(
                "[BML-POOL] task-buffer pool ready: slots=%u slot_cap=%u lwram_bytes=%lu",
                (unsigned)TaskBufferPool::kSlotCount,
                (unsigned)TaskBufferPool::kSlotCapacity,
                (unsigned long)(static_cast<uint32_t>(TaskBufferPool::kSlotCount) *
                                TaskBufferPool::kSlotCapacity * sizeof(Task)));
        } else {
            SRL::Logger::LogWarning("[BML-POOL] task-buffer pool init FAILED (LWRAM exhausted?)");
        }
        return ok;
    }

    // Free the pool LWRAM block.  Only call during a full program restart;
    // do NOT call during normal clearFoes/latch-recovery cycles.
    static void DeinitTaskPool() {
        getTaskPool().deinit();
    }

    // Number of pool slots currently available (for heartbeat diagnostics).
    static uint16_t GetTaskPoolFreeSlots() {
        return getTaskPool().availableSlots();
    }

    static uint32_t GetTaskPoolReservedBytes() {
        return static_cast<uint32_t>(TaskBufferPool::kSlotCount) *
               TaskBufferPool::kSlotCapacity * sizeof(Task);
    }

    static uint32_t GetTaskBufferCacheBytes() {
        return static_cast<uint32_t>(getTaskBufferCache().capacity) * sizeof(Task);
    }

    static void ReleaseTaskBufferCache() {
        // Clear the single-slot cache used for grown (>kSlotCapacity) buffers.
        // Pool slots are returned incrementally by impl destructors — no pool
        // reset is needed here.
        TaskBufferCache& cache = getTaskBufferCache();
        if (cache.ptr) {
            freeBulletMlArray(cache.ptr, cache.capacity);
            cache.ptr = nullptr;
            cache.capacity = 0;
        }
    }

    static bool PreallocateTaskBufferCache(uint16_t capacity) {
        if (capacity == 0) {
            return true;
        }

        TaskBufferCache& cache = getTaskBufferCache();
        if (cache.ptr && cache.capacity >= capacity) {
            return true;
        }

        Task* ptr = allocBulletMlArray<Task>("runner.tasks.prealloc", capacity);
        if (!ptr) {
            return false;
        }

        if (cache.ptr) {
            freeBulletMlArray(cache.ptr, cache.capacity);
        }

        cache.ptr = ptr;
        cache.capacity = capacity;
        return true;
    }

    static uint16_t GetTaskBufferCacheCapacity() {
        return getTaskBufferCache().capacity;
    }

    static void EnableStartupOnlyAllocation(bool enabled) {
        isStartupOnlyAllocationEnabled() = enabled;
    }

    static bool IsStartupOnlyAllocationEnabled() {
        return isStartupOnlyAllocationEnabled();
    }

    static void BeginStartupPreallocation() {
        isStartupPreallocationPhase() = true;
    }

    static void EndStartupPreallocation() {
        isStartupPreallocationPhase() = false;
    }

    static bool IsStartupPreallocationPhase() {
        return isStartupPreallocationPhase();
    }

    void run() {
        if (end_) return;
        if (!isLikelySh2RamPointer(runner_) || safeRunnerTurn() < 0) {
            if (capacity_fail_logs_ < 12) {
                ++capacity_fail_logs_;
                SRL::Logger::LogWarning(
                    "[BML-RUNNER] invalid runner pointer in run this=%p runner=%p",
                    static_cast<void*>(this),
                    static_cast<void*>(runner_));
            }
            end_ = true;
            task_count_ = 0;
            return;
        }
        if (hasBulletMlAllocFailureLatched()) {
            end_ = true;
            task_count_ = 0;
            return;
        }

        applyChanges();

        const int now = runner_->getTurn();
        if (task_count_ == 0) {
            if (!change_dir_active_ && !change_speed_active_ && !accel_x_active_ && !accel_y_active_) {
                end_ = true;
            }
            return;
        }

        if (now < wait_until_turn_) return;

        // Track peak task stack utilization for telemetry
        if (task_count_ > peak_task_count_) {
            peak_task_count_ = task_count_;
        }

        int safety = 0;
        while (task_count_ > 0 && runner_->getTurn() >= wait_until_turn_) {
            Task task;
            if (!popTask(task)) break;

            switch (task.type) {
                case TASK_NODE:
                    executeNode(task.node);
                    break;
                case TASK_REPEAT:
                    if (task.repeat_remaining > 0 && task.repeat_action) {
                        if (!pushRepeatTask(task.repeat_action, task.repeat_remaining - 1)) {
                            end_ = true;
                            return;
                        }
                        if (!pushNodeTask(task.repeat_action)) {
                            end_ = true;
                            return;
                        }
                    }
                    break;
                case TASK_POP_PARAMS:
                    if (owns_params_) {
                        bulletml_state_pool::recyclePooledArray(params_, bulletml_state_pool::getBucketedCapacity(param_count_));
                    }
                    params_ = task.saved_params;
                    param_count_ = task.saved_count;
                    owns_params_ = task.saved_owns;
                    break;
            }

            if (end_) {
                task_count_ = 0;
                return;
            }

            // Guard against pathological no-wait scripts.
            ++safety;
            if (safety > 8192) {
                SRL::Logger::LogWarning("[BML-RUNNER] Safety break triggered (tasks=%u)", task_count_);
                break;
            }
        }

        if (task_count_ == 0) {
            if (!change_dir_active_ && !change_speed_active_ && !accel_x_active_ && !accel_y_active_) {
                end_ = true;
            }
        }
    }

private:
    static constexpr uint16_t kMaxTaskCapacity = 2048;

    enum TaskType {
        TASK_NODE = 0,
        TASK_REPEAT = 1,
        TASK_POP_PARAMS = 2,
    };

    struct Task {
        TaskType type;
        BulletMLNode* node;

        // repeat task fields
        BulletMLNode* repeat_action;
        int repeat_remaining;

        // param restore task fields
        Fxp* saved_params;
        uint16_t saved_count;
        bool saved_owns;
    };

    struct LinearChange {
        int start_turn;
        int end_turn;
        Fxp first;
        Fxp last;
        Fxp gradient;
    };

    struct TaskBufferCache {
        Task* ptr;
        uint16_t capacity;
    };

    // Pre-allocated slab pool for standard-capacity (32-task) task buffers.
    //
    // All concurrent task buffers are served from a single contiguous LWRAM
    // allocation (one lwnew at InitTaskPool() time, never freed during gameplay).
    // acquire()/release() are O(1) HWRAM stack operations — zero LWRAM heap
    // churn across clearFoes() cycles, eliminating the fragmentation that caused
    // the entity-count degradation (186→64→28→9) observed on real hardware.
    //
    // kSlotCount=400 covers the observed peak of ~382 concurrent runners with a
    // safety margin.  Pool memory: 400×32×24B = 307 200B (~300KB LWRAM).
    struct TaskBufferPool {
        static constexpr uint16_t kSlotCapacity = 32;
        static constexpr uint16_t kSlotCount    = 400;

        Task*    base;
        uint16_t freeStack[kSlotCount];
        uint16_t freeTop;

        TaskBufferPool() : base(nullptr), freeTop(0) {
            for (uint16_t i = 0; i < kSlotCount; ++i) {
                freeStack[i] = 0;
            }
        }

        bool init() {
            if (base) return true;
            base = lwnew Task[static_cast<uint32_t>(kSlotCount) * kSlotCapacity];
            if (!base) return false;
            bulletml_runner_trace::noteAlloc(
                sizeof(Task) * static_cast<uint32_t>(kSlotCount) * kSlotCapacity,
                true);
            freeTop = kSlotCount;
            for (uint16_t i = 0; i < kSlotCount; ++i) {
                freeStack[i] = i;
            }
            return true;
        }

        void deinit() {
            if (base) {
                bulletml_runner_trace::noteFree(
                    sizeof(Task) * static_cast<uint32_t>(kSlotCount) * kSlotCapacity,
                    true);
                delete[] base;
            }
            base     = nullptr;
            freeTop  = 0;
        }

        bool isInitialized() const { return base != nullptr; }

        Task* acquire(uint16_t& outCapacity) {
            if (!base || freeTop == 0) {
                outCapacity = 0;
                return nullptr;
            }
            const uint16_t slot = freeStack[--freeTop];
            outCapacity = kSlotCapacity;
            return base + static_cast<uint32_t>(slot) * kSlotCapacity;
        }

        bool release(Task* ptr) {
            if (!base || !ptr) return false;
            const ptrdiff_t offset = ptr - base;
            if (offset < 0 || static_cast<uint32_t>(offset) >=
                    static_cast<uint32_t>(kSlotCount) * kSlotCapacity) return false;
            if ((static_cast<uint32_t>(offset) % kSlotCapacity) != 0) return false;
            if (freeTop >= kSlotCount) return false;
            freeStack[freeTop++] = static_cast<uint16_t>(offset / kSlotCapacity);
            return true;
        }

        uint16_t availableSlots() const { return freeTop; }
    };

    static TaskBufferPool& getTaskPool() {
        static TaskBufferPool pool;
        return pool;
    }

    static TaskBufferCache& getTaskBufferCache() {
        static TaskBufferCache cache = {nullptr, 0};
        return cache;
    }

    static bool& isStartupOnlyAllocationEnabled() {
        static bool enabled = false;
        return enabled;
    }

    static bool& isStartupPreallocationPhase() {
        static bool prealloc = false;
        return prealloc;
    }

    static Task* takeCachedTaskBuffer(uint16_t needed, uint16_t& outCapacity) {
        // Serve standard-capacity requests from the pool (no LWRAM heap churn).
        if (needed <= TaskBufferPool::kSlotCapacity) {
            Task* t = getTaskPool().acquire(outCapacity);
            if (t) return t;
        }

        // Fall back to the single-slot cache for grown (>kSlotCapacity) buffers.
        TaskBufferCache& cache = getTaskBufferCache();
        if (cache.ptr && cache.capacity >= needed) {
            Task* out   = cache.ptr;
            outCapacity = cache.capacity;
            cache.ptr      = nullptr;
            cache.capacity = 0;
            return out;
        }

        outCapacity = 0;
        return nullptr;
    }

    static void recycleTaskBuffer(Task* ptr, uint16_t capacity) {
        if (!ptr || capacity == 0) return;

        // Return standard-capacity buffers to the pool.
        if (capacity == TaskBufferPool::kSlotCapacity) {
            if (getTaskPool().release(ptr)) {
                return;
            }
        }

        // Single-slot cache for grown (>kSlotCapacity) buffers.
        TaskBufferCache& cache = getTaskBufferCache();

        if (!cache.ptr) {
            cache.ptr      = ptr;
            cache.capacity = capacity;
            return;
        }

        if (capacity > cache.capacity) {
            freeBulletMlArray(cache.ptr, cache.capacity);
            cache.ptr      = ptr;
            cache.capacity = capacity;
            return;
        }

        freeBulletMlArray(ptr, capacity);
    }

    static bool isLikelySh2RamPointer(const void* ptr) {
        if (!ptr) {
            return false;
        }

        const std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(ptr);
        const bool isUncachedSh2Ram = (addr >= 0x06000000u && addr < 0x06100000u);
        const bool isCachedSh2Ram = (addr >= 0x26000000u && addr < 0x26100000u);
        const bool isLowWorkRam = (addr >= 0x00200000u && addr < 0x00300000u);
        if (!(isUncachedSh2Ram || isCachedSh2Ram || isLowWorkRam)) {
            return false;
        }

        return (addr & 0x3u) == 0u;
    }

    int safeRunnerTurn() const {
        if (!isLikelySh2RamPointer(runner_)) {
            return -1;
        }

        return runner_->getTurn();
    }

    void setExpansionContext(BulletMLNode::Name name, uint32_t ref_id, uint32_t fanout, uint32_t child_count) {
        expand_name_ = static_cast<uint8_t>(name);
        expand_ref_id_ = ref_id;
        expand_fanout_ = fanout;
        expand_child_count_ = child_count;
    }

    bool ensureTaskCapacity(uint32_t needed) {
        if (needed <= task_capacity_) return true;

        const int safeTurn = safeRunnerTurn();
        if (safeTurn < 0 && capacity_fail_logs_ < 12) {
            ++capacity_fail_logs_;
            SRL::Logger::LogWarning(
                "[BML-RUNNER] invalid runner pointer in ensureTaskCapacity this=%p runner=%p needed=%u cap=%u count=%u",
                static_cast<void*>(this),
                static_cast<void*>(runner_),
                static_cast<unsigned>(needed),
                static_cast<unsigned>(task_capacity_),
                static_cast<unsigned>(task_count_));
            end_ = true;
            task_count_ = 0;
            return false;
        }

        if (capacity_fail_logs_ < 12) {
            SRL::Logger::LogInfo(
                "[BML-RUNNER] task growth request needed=%u current=%u turn=%d wait=%d ctx_name=%u ctx_ref=%u ctx_fanout=%u ctx_children=%u",
                static_cast<unsigned>(needed),
                static_cast<unsigned>(task_capacity_),
                safeTurn,
                wait_until_turn_,
                static_cast<unsigned>(expand_name_),
                static_cast<unsigned>(expand_ref_id_),
                static_cast<unsigned>(expand_fanout_),
                static_cast<unsigned>(expand_child_count_));
        }

        if (needed > kMaxTaskCapacity) {
            if (capacity_fail_logs_ < 12) {
                ++capacity_fail_logs_;
                SRL::Logger::LogWarning(
                    "[BML-RUNNER] Task capacity hard limit (needed=%u cap=%u count=%u curr_cap=%u turn=%d wait=%d ctx_name=%u ctx_ref=%u ctx_fanout=%u ctx_children=%u)",
                    static_cast<unsigned>(needed),
                    static_cast<unsigned>(kMaxTaskCapacity),
                    static_cast<unsigned>(task_count_),
                    static_cast<unsigned>(task_capacity_),
                    safeTurn,
                    wait_until_turn_,
                    static_cast<unsigned>(expand_name_),
                    static_cast<unsigned>(expand_ref_id_),
                    static_cast<unsigned>(expand_fanout_),
                    static_cast<unsigned>(expand_child_count_));
            }
            return false;
        }

        // Start with 32 instead of 16 to reduce initial allocation spike
        uint16_t new_cap = (task_capacity_ == 0) ? 32 : task_capacity_;
        // Grow by +32 instead of doubling to avoid wasting capacity
        while (new_cap < needed) {
            uint16_t grown = static_cast<uint16_t>(new_cap + 32);
            if (grown <= new_cap) {
                new_cap = needed;
                break;
            }
            new_cap = grown;
        }

        if (new_cap > kMaxTaskCapacity) {
            new_cap = kMaxTaskCapacity;
        }

        if (new_cap < needed) {
            SRL::Logger::LogWarning("[BML-RUNNER] Task capacity clamp insufficient (needed=%u, capped=%u)",
                                    static_cast<unsigned>(needed), new_cap);
            return false;
        }

        uint16_t cached_cap = 0;
        Task* t = takeCachedTaskBuffer(new_cap, cached_cap);
        if (!t) {
            if (isStartupOnlyAllocationEnabled() && !isStartupPreallocationPhase()) {
                if (capacity_fail_logs_ < 12) {
                    ++capacity_fail_logs_;
                    SRL::Logger::LogWarning(
                        "[BML-RUNNER] startup-only allocation policy blocked task growth needed=%u current=%u turn=%d",
                        static_cast<unsigned>(needed),
                        static_cast<unsigned>(task_capacity_),
                        safeTurn);
                }
                return false;
            }
            if (capacity_fail_logs_ < 12) {
                SRL::Logger::LogInfo("[BML-RUNNER] task buffer cache miss needed=%u new_cap=%u",
                                     static_cast<unsigned>(needed),
                                     static_cast<unsigned>(new_cap));
            }
            t = allocBulletMlArray<Task>("runner.tasks", new_cap);
        }
        else if (capacity_fail_logs_ < 12) {
            SRL::Logger::LogInfo("[BML-RUNNER] task buffer cache hit needed=%u cached_cap=%u",
                                 static_cast<unsigned>(needed),
                                 static_cast<unsigned>(cached_cap));
        }
        if (!t) return false;

        if (cached_cap > 0 && cached_cap >= new_cap) {
            new_cap = cached_cap;
        }

        for (uint16_t i = 0; i < task_count_; ++i) {
            t[i] = tasks_[i];
        }
        recycleTaskBuffer(tasks_, task_capacity_);
        tasks_ = t;
        task_capacity_ = new_cap;
        return true;
    }

    bool validateTaskStack(const char* where) {
        if (task_count_ <= task_capacity_) return true;
        SRL::Logger::LogWarning("[BML-RUNNER] Task stack corruption at %s (count=%u, cap=%u)",
                                where, task_count_, task_capacity_);
        end_ = true;
        task_count_ = 0;
        return false;
    }

    bool pushTask(const Task& task) {
        if (!validateTaskStack("push")) {
            return false;
        }

        const uint32_t needed = static_cast<uint32_t>(task_count_) + 1U;
        if (!ensureTaskCapacity(needed)) {
            end_ = true;
            return false;
        }

        if (!validateTaskStack("push-post-alloc")) {
            return false;
        }

        tasks_[task_count_++] = task;
        return true;
    }

    bool popTask(Task& out) {
        if (!validateTaskStack("pop")) {
            return false;
        }
        if (task_count_ == 0) return false;
        out = tasks_[--task_count_];
        return true;
    }

    bool pushNodeTask(BulletMLNode* node) {
        Task t;
        t.type = TASK_NODE;
        t.node = node;
        t.repeat_action = nullptr;
        t.repeat_remaining = 0;
        t.saved_params = nullptr;
        t.saved_count = 0;
        t.saved_owns = false;
        return pushTask(t);
    }

    bool pushRepeatTask(BulletMLNode* action, int remaining) {
        Task t;
        t.type = TASK_REPEAT;
        t.node = nullptr;
        t.repeat_action = action;
        t.repeat_remaining = remaining;
        t.saved_params = nullptr;
        t.saved_count = 0;
        t.saved_owns = false;
        return pushTask(t);
    }

    bool pushPopParamsTask(Fxp* saved_params, uint16_t saved_count, bool saved_owns) {
        Task t;
        t.type = TASK_POP_PARAMS;
        t.node = nullptr;
        t.repeat_action = nullptr;
        t.repeat_remaining = 0;
        t.saved_params = saved_params;
        t.saved_count = saved_count;
        t.saved_owns = saved_owns;
        return pushTask(t);
    }

    static bool isSpace(char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    static bool isDigit(char c) {
        return c >= '0' && c <= '9';
    }

    static bool isAlpha(char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    }

    void skipSpaces(const char*& p) const {
        while (*p && isSpace(*p)) ++p;
    }

    Fxp parseNumberLiteral(const char*& p) const {
        const char* start = p;

        if (*p == '+' || *p == '-') ++p;
        while (isDigit(*p)) ++p;
        if (*p == '.') {
            ++p;
            while (isDigit(*p)) ++p;
        }
        if (*p == 'e' || *p == 'E') {
            ++p;
            if (*p == '+' || *p == '-') ++p;
            while (isDigit(*p)) ++p;
        }

        bool neg = false;
        const char* q = start;
        if (*q == '+') {
            ++q;
        } else if (*q == '-') {
            neg = true;
            ++q;
        }

        Fxp value = 0.0;
        while (isDigit(*q)) {
            value = value * 10.0 + Fxp::Convert(*q - '0');
            ++q;
        }

        if (*q == '.') {
            ++q;
            Fxp base = 0.1;
            while (isDigit(*q)) {
                value += Fxp::Convert(*q - '0') * base;
                base *= 0.1;
                ++q;
            }
        }

        if (*q == 'e' || *q == 'E') {
            ++q;
            bool exp_neg = false;
            if (*q == '+') {
                ++q;
            } else if (*q == '-') {
                exp_neg = true;
                ++q;
            }
            int exp = 0;
            while (isDigit(*q)) {
                exp = exp * 10 + (*q - '0');
                ++q;
            }
            Fxp scale = 1.0;
            while (exp-- > 0) scale *= 10.0;
            if (exp_neg) value /= scale;
            else value *= scale;
        }

        return neg ? -value : value;
    }

    Fxp parseExpression(const char*& p) {
        Fxp v = parseTerm(p);
        while (true) {
            skipSpaces(p);
            if (*p == '+') {
                ++p;
                v += parseTerm(p);
            } else if (*p == '-') {
                ++p;
                v -= parseTerm(p);
            } else {
                break;
            }
        }
        return v;
    }

    Fxp parseTerm(const char*& p) {
        Fxp v = parseFactor(p);
        while (true) {
            skipSpaces(p);
            if (*p == '*') {
                ++p;
                v *= parseFactor(p);
            } else if (*p == '/') {
                ++p;
                const Fxp rhs = parseFactor(p);
                if (rhs != 0.0) v /= rhs;
            } else {
                break;
            }
        }
        return v;
    }

    Fxp parseVariable(const char*& p) {
        if (*p != '$') return 0.0;
        ++p;

        if (isDigit(*p)) {
            int idx = 0;
            while (isDigit(*p)) {
                idx = idx * 10 + (*p - '0');
                ++p;
            }
            if (idx >= 0 && static_cast<uint16_t>(idx) < param_count_ && params_) {
                return params_[idx];
            }
            return 0.0;
        }

        char name[16];
        uint8_t n = 0;
        while (*p && isAlpha(*p) && n < sizeof(name) - 1) {
            name[n++] = *p++;
        }
        name[n] = '\0';

        if (n == 4 && name[0] == 'r' && name[1] == 'a' && name[2] == 'n' && name[3] == 'k') {
            return runner_->getRank();
        }
        if (n == 4 && name[0] == 'r' && name[1] == 'a' && name[2] == 'n' && name[3] == 'd') {
            return runner_->getRand();
        }
        return 0.0;
    }

    Fxp parseFactor(const char*& p) {
        skipSpaces(p);

        if (*p == '\0') return 0.0;

        if (*p == '+') {
            ++p;
            return parseFactor(p);
        }
        if (*p == '-') {
            ++p;
            return -parseFactor(p);
        }
        if (*p == '(') {
            ++p;
            const Fxp v = parseExpression(p);
            skipSpaces(p);
            if (*p == ')') ++p;
            return v;
        }
        if (*p == '$') {
            return parseVariable(p);
        }
        if (isDigit(*p) || *p == '.') {
            return parseNumberLiteral(p);
        }

        // Unsupported token in this compact evaluator.
        ++p;
        return 0.0;
    }

    Fxp evalNodeValue(const BulletMLNode* node) {
        if (!node) return 0.0;
        const char* s = node->getValue();
        if (!s || s[0] == '\0') return 0.0;
        const char* p = s;
        return parseExpression(p);
    }

    void copyParameters(const Fxp* src, uint16_t count) {
        if (owns_params_) {
            bulletml_state_pool::recyclePooledArray(params_, bulletml_state_pool::getBucketedCapacity(param_count_));
            owns_params_ = false;
        }
        params_ = nullptr;
        param_count_ = 0;

        if (!src || count == 0) return;

        uint16_t dummy_cap = 0;
        Fxp* p = bulletml_state_pool::createPooledArray<Fxp>(count, dummy_cap);
        if (!p) return;
        for (uint16_t i = 0; i < count; ++i) {
            p[i] = src[i];
        }
        params_ = p;
        param_count_ = count;
        owns_params_ = true;
    }

    BulletMLNode* findChild(BulletMLNode* node, BulletMLNode::Name name) {
        if (!node) return nullptr;
        const uint32_t count = node->getChildCount();
        for (uint32_t i = 0; i < count; ++i) {
            BulletMLNode* c = node->getChild(i);
            if (c && c->getNameAsName() == name) return c;
        }
        return nullptr;
    }

    uint16_t countChildren(BulletMLNode* node, BulletMLNode::Name name) {
        if (!node) return 0;
        uint16_t n = 0;
        const uint32_t count = node->getChildCount();
        for (uint32_t i = 0; i < count; ++i) {
            BulletMLNode* c = node->getChild(i);
            if (c && c->getNameAsName() == name) ++n;
        }
        return n;
    }

    void shotInit() {
        has_spd_ = false;
        has_dir_ = false;
    }

    Fxp getDirection(BulletMLNode* dir_node, bool update_prev) {
        if (!dir_node) return runner_->getAimDirection();

        Fxp dir = evalNodeValue(dir_node);
        bool is_default = true;

        const BulletMLNode::Type type = dir_node->getType();
        if (type != BulletMLNode::type_none) {
            is_default = false;
            if (type == BulletMLNode::type_absolute) {
                if (parser_ && parser_->isHorizontal()) dir -= 90.0;
            } else if (type == BulletMLNode::type_relative) {
                dir += runner_->getBulletDirection();
            } else if (type == BulletMLNode::type_sequence) {
                if (!has_prev_dir_) {
                    dir = 0.0;
                    is_default = true;
                } else {
                    dir += prev_dir_;
                }
            } else {
                is_default = true;
            }
        }

        if (is_default) {
            dir += runner_->getAimDirection();
        }

        while (dir > 360.0) dir -= 360.0;
        while (dir < 0.0) dir += 360.0;

        if (update_prev) {
            prev_dir_ = dir;
            has_prev_dir_ = true;
        }

        return dir;
    }

    Fxp getSpeed(BulletMLNode* spd_node) {
        if (!spd_node) return runner_->getDefaultSpeed();

        Fxp spd = evalNodeValue(spd_node);
        const BulletMLNode::Type type = spd_node->getType();
        if (type != BulletMLNode::type_none) {
            if (type == BulletMLNode::type_relative) {
                spd += runner_->getBulletSpeed();
            } else if (type == BulletMLNode::type_sequence) {
                if (!has_prev_spd_) spd = 1.0;
                else spd += prev_spd_;
            }
        }

        prev_spd_ = spd;
        has_prev_spd_ = true;
        return spd;
    }

    void setDirection(BulletMLNode* node) {
        BulletMLNode* d = findChild(node, BulletMLNode::direction);
        if (!d) return;
        dir_ = getDirection(d, true);
        has_dir_ = true;
    }

    void setSpeed(BulletMLNode* node) {
        BulletMLNode* s = findChild(node, BulletMLNode::speed);
        if (!s) return;
        spd_ = getSpeed(s);
        has_spd_ = true;
    }

    static Fxp linearValue(const LinearChange& lc, int turn) {
        return lc.first + lc.gradient * Fxp::Convert(turn - lc.start_turn);
    }

    static void setLinear(LinearChange& lc, bool& active,
                          int start_turn, int end_turn,
                          Fxp first, Fxp last) {
        lc.start_turn = start_turn;
        lc.end_turn = end_turn;
        lc.first = first;
        lc.last = last;
        if (end_turn != start_turn) {
            lc.gradient = (last - first) / Fxp::Convert(end_turn - start_turn);
        } else {
            lc.gradient = 0.0;
        }
        active = true;
    }

    static Fxp absd(Fxp v) {
        return (v < 0.0) ? -v : v;
    }

    void calcChangeDirection(Fxp direction, int term, bool seq) {
        const int now = runner_->getTurn();
        if (term <= 0) {
            runner_->doChangeDirection(direction);
            change_dir_active_ = false;
            return;
        }

        const int final_turn = now + term;
        const Fxp first = runner_->getBulletDirection();

        if (seq) {
            setLinear(change_dir_, change_dir_active_, now, final_turn,
                      first, first + direction * Fxp::Convert(term));
            return;
        }

        const Fxp d1 = direction - first;
        const Fxp d2 = (d1 > 0.0) ? (d1 - 360.0) : (d1 + 360.0);
        const Fxp d = (absd(d1) < absd(d2)) ? d1 : d2;

        setLinear(change_dir_, change_dir_active_, now, final_turn, first, first + d);
    }

    void calcChangeSpeed(Fxp speed, int term) {
        const int now = runner_->getTurn();
        if (term <= 0) {
            runner_->doChangeSpeed(speed);
            change_speed_active_ = false;
            return;
        }
        setLinear(change_speed_, change_speed_active_, now, now + term,
                  runner_->getBulletSpeed(), speed);
    }

    void calcAccelX(Fxp value, int term, BulletMLNode::Type type) {
        const int now = runner_->getTurn();
        if (term <= 0) {
            runner_->doAccelX(value);
            accel_x_active_ = false;
            return;
        }

        const Fxp first = runner_->getBulletSpeedX();
        Fxp last = value;
        if (type == BulletMLNode::type_sequence) {
            last = first + value * Fxp::Convert(term);
        } else if (type == BulletMLNode::type_relative) {
            last = first + value;
        }

        setLinear(accel_x_, accel_x_active_, now, now + term, first, last);
    }

    void calcAccelY(Fxp value, int term, BulletMLNode::Type type) {
        const int now = runner_->getTurn();
        if (term <= 0) {
            runner_->doAccelY(value);
            accel_y_active_ = false;
            return;
        }

        const Fxp first = runner_->getBulletSpeedY();
        Fxp last = value;
        if (type == BulletMLNode::type_sequence) {
            last = first + value * Fxp::Convert(term);
        } else if (type == BulletMLNode::type_relative) {
            last = first + value;
        }

        setLinear(accel_y_, accel_y_active_, now, now + term, first, last);
    }

    void applyChanges() {
        const int now = runner_->getTurn();

        if (change_dir_active_) {
            if (now >= change_dir_.end_turn) {
                runner_->doChangeDirection(change_dir_.last);
                change_dir_active_ = false;
            } else {
                runner_->doChangeDirection(linearValue(change_dir_, now));
            }
        }

        if (change_speed_active_) {
            if (now >= change_speed_.end_turn) {
                runner_->doChangeSpeed(change_speed_.last);
                change_speed_active_ = false;
            } else {
                runner_->doChangeSpeed(linearValue(change_speed_, now));
            }
        }

        if (accel_x_active_) {
            if (now >= accel_x_.end_turn) {
                runner_->doAccelX(accel_x_.last);
                accel_x_active_ = false;
            } else {
                runner_->doAccelX(linearValue(accel_x_, now));
            }
        }

        if (accel_y_active_) {
            if (now >= accel_y_.end_turn) {
                runner_->doAccelY(accel_y_.last);
                accel_y_active_ = false;
            } else {
                runner_->doAccelY(linearValue(accel_y_, now));
            }
        }
    }

    Fxp* collectRefParameters(BulletMLNode* ref_node, uint16_t& out_count) {
        out_count = 0;
        if (!ref_node) return nullptr;

        const uint16_t param_nodes = countChildren(ref_node, BulletMLNode::param);
        if (param_nodes == 0) return nullptr;

        const uint16_t total = static_cast<uint16_t>(param_nodes + 1);
        uint16_t dummy_cap = 0;
        Fxp* out = bulletml_state_pool::createPooledArray<Fxp>(total, dummy_cap);
        if (!out) return nullptr;

        out[0] = 0.0;  // 1-based parameters
        uint16_t w = 1;
        const uint32_t child_count = ref_node->getChildCount();
        for (uint32_t i = 0; i < child_count && w < total; ++i) {
            BulletMLNode* c = ref_node->getChild(i);
            if (c && c->getNameAsName() == BulletMLNode::param) {
                out[w++] = evalNodeValue(c);
            }
        }

        out_count = total;
        return out;
    }

    void executeNode(BulletMLNode* node) {
        if (!node) return;

        const BulletMLNode::Name name = node->getNameAsName();
        switch (name) {
            case BulletMLNode::bullet: {
                setSpeed(node);
                setDirection(node);
                if (!has_spd_) {
                    spd_ = runner_->getDefaultSpeed();
                    prev_spd_ = spd_;
                    has_prev_spd_ = true;
                    has_spd_ = true;
                }
                if (!has_dir_) {
                    dir_ = runner_->getAimDirection();
                    prev_dir_ = dir_;
                    has_prev_dir_ = true;
                    has_dir_ = true;
                }

                const uint16_t action_count = countChildren(node, BulletMLNode::action);
                const uint16_t action_ref_count = countChildren(node, BulletMLNode::actionRef);
                const uint16_t total = static_cast<uint16_t>(action_count + action_ref_count);

                if (total == 0) {
                    runner_->createSimpleBullet(dir_, spd_);
                    return;
                }

                BulletMLNode** acts = createBulletMlStateNodeArray(total);
                if (!acts) return;

                uint16_t w = 0;
                const uint32_t child_count = node->getChildCount();
                for (uint32_t i = 0; i < child_count && w < total; ++i) {
                    BulletMLNode* c = node->getChild(i);
                    if (c && c->getNameAsName() == BulletMLNode::action) acts[w++] = c;
                }
                for (uint32_t i = 0; i < child_count && w < total; ++i) {
                    BulletMLNode* c = node->getChild(i);
                    if (c && c->getNameAsName() == BulletMLNode::actionRef) acts[w++] = c;
                }

                BulletMLState* st = createBulletMlState(parser_, acts, total, params_, param_count_);
                if (!st) {
                    destroyBulletMlStateNodeArray(acts, total);
                    SRL::Logger::LogWarning("[BML-RUNNER] Failed to allocate child state for bullet actions=%u", total);
                    return;
                }
                runner_->createBullet(st, dir_, spd_);
                return;
            }

            case BulletMLNode::fire: {
                shotInit();
                setSpeed(node);
                setDirection(node);

                BulletMLNode* bullet = findChild(node, BulletMLNode::bullet);
                if (!bullet) bullet = findChild(node, BulletMLNode::bulletRef);
                if (bullet) {
                    if (!pushNodeTask(bullet)) {
                        end_ = true;
                    }
                }
                return;
            }

            case BulletMLNode::action: {
                const uint32_t child_count = node->getChildCount();
                setExpansionContext(name, 0, child_count, child_count);
                if (child_count >= 512 || (task_capacity_ > task_count_ && (task_capacity_ - task_count_) < child_count)) {
                    SRL::Logger::LogWarning(
                        "[BML-RUNNER] action fanout children=%u pending=%u cap=%u turn=%d",
                        static_cast<unsigned>(child_count),
                        static_cast<unsigned>(task_count_),
                        static_cast<unsigned>(task_capacity_),
                        runner_ ? runner_->getTurn() : -1);
                }
                for (int i = static_cast<int>(child_count) - 1; i >= 0; --i) {
                    if (!pushNodeTask(node->getChild(static_cast<uint32_t>(i)))) {
                        end_ = true;
                        return;
                    }
                }
                return;
            }

            case BulletMLNode::wait: {
                int frame = evalNodeValue(node).As<int>();
                if (frame < 0) frame = 0;
                wait_until_turn_ = runner_->getTurn() + frame;
                return;
            }

            case BulletMLNode::repeat: {
                BulletMLNode* times = findChild(node, BulletMLNode::times);
                BulletMLNode* action = findChild(node, BulletMLNode::action);
                if (!action) action = findChild(node, BulletMLNode::actionRef);
                if (!times || !action) return;

                int times_num = evalNodeValue(times).As<int>();
                if (times_num <= 0) return;

                setExpansionContext(name, 0, 2, static_cast<uint32_t>(times_num));
                if (times_num >= 512) {
                    SRL::Logger::LogWarning(
                        "[BML-RUNNER] repeat fanout times=%d pending=%u cap=%u turn=%d",
                        times_num,
                        static_cast<unsigned>(task_count_),
                        static_cast<unsigned>(task_capacity_),
                        runner_ ? runner_->getTurn() : -1);
                }

                if (!pushRepeatTask(action, times_num - 1) || !pushNodeTask(action)) {
                    end_ = true;
                }
                return;
            }

            case BulletMLNode::actionRef:
            case BulletMLNode::fireRef:
            case BulletMLNode::bulletRef: {
                if (!parser_) return;

                BulletMLNode* target = nullptr;
                const uint32_t id = node->getRefID();
                if (name == BulletMLNode::actionRef) {
                    target = parser_->getAction(id);
                } else if (name == BulletMLNode::fireRef) {
                    target = parser_->getFire(id);
                } else {
                    target = parser_->getBullet(id);
                }
                if (!target) {
                    SRL::Logger::LogWarning("[BML-RUNNER] Missing ref target name=%d id=%u", static_cast<int>(name), id);
                    return;
                }

                setExpansionContext(name, id, target->getChildCount(), target->getChildCount());
                if (target->getChildCount() >= 256) {
                    SRL::Logger::LogWarning(
                        "[BML-RUNNER] ref fanout name=%u id=%u target_children=%u pending=%u cap=%u turn=%d",
                        static_cast<unsigned>(name),
                        static_cast<unsigned>(id),
                        static_cast<unsigned>(target->getChildCount()),
                        static_cast<unsigned>(task_count_),
                        static_cast<unsigned>(task_capacity_),
                        runner_ ? runner_->getTurn() : -1);
                }

                uint16_t new_count = 0;
                Fxp* new_params = collectRefParameters(node, new_count);

                Fxp* saved_params = params_;
                uint16_t saved_count = param_count_;
                bool saved_owns = owns_params_;

                if (!pushPopParamsTask(saved_params, saved_count, saved_owns)) {
                    if (new_params) {
                        bulletml_state_pool::recyclePooledArray(new_params, bulletml_state_pool::getBucketedCapacity(new_count));
                    }
                    SRL::Logger::LogWarning("[BML-RUNNER] Failed to push param restore task for ref id=%u", id);
                    end_ = true;
                    return;
                }

                params_ = new_params;
                param_count_ = new_count;
                owns_params_ = (new_params != nullptr);

                if (!pushNodeTask(target)) {
                    if (task_count_ > 0 && tasks_[task_count_ - 1].type == TASK_POP_PARAMS) {
                        --task_count_;
                    }
                    if (owns_params_) {
                        bulletml_state_pool::recyclePooledArray(params_, bulletml_state_pool::getBucketedCapacity(param_count_));
                    }
                    params_ = saved_params;
                    param_count_ = saved_count;
                    owns_params_ = saved_owns;
                    end_ = true;
                }
                return;
            }

            case BulletMLNode::changeDirection: {
                BulletMLNode* term = findChild(node, BulletMLNode::term);
                BulletMLNode* dir_node = findChild(node, BulletMLNode::direction);
                if (!term || !dir_node) return;

                int t = evalNodeValue(term).As<int>();
                if (t < 0) t = 0;
                const BulletMLNode::Type type = dir_node->getType();

                Fxp d;
                if (type != BulletMLNode::type_sequence) d = getDirection(dir_node, false);
                else d = evalNodeValue(dir_node);
                calcChangeDirection(d, t, type == BulletMLNode::type_sequence);
                return;
            }

            case BulletMLNode::changeSpeed: {
                BulletMLNode* term = findChild(node, BulletMLNode::term);
                BulletMLNode* spd_node = findChild(node, BulletMLNode::speed);
                if (!term || !spd_node) return;

                int t = evalNodeValue(term).As<int>();
                if (t < 0) t = 0;
                const BulletMLNode::Type type = spd_node->getType();

                Fxp s;
                if (type != BulletMLNode::type_sequence) s = getSpeed(spd_node);
                else s = evalNodeValue(spd_node) * Fxp::Convert(t) + runner_->getBulletSpeed();

                calcChangeSpeed(s, t);
                return;
            }

            case BulletMLNode::accel: {
                BulletMLNode* term = findChild(node, BulletMLNode::term);
                if (!term) return;
                int t = evalNodeValue(term).As<int>();
                if (t < 0) t = 0;

                BulletMLNode* h = findChild(node, BulletMLNode::horizontal);
                BulletMLNode* v = findChild(node, BulletMLNode::vertical);

                if (parser_ && parser_->isHorizontal()) {
                    if (v) calcAccelX(evalNodeValue(v), t, v->getType());
                    if (h) calcAccelY(-evalNodeValue(h), t, h->getType());
                } else {
                    if (h) calcAccelX(evalNodeValue(h), t, h->getType());
                    if (v) calcAccelY(evalNodeValue(v), t, v->getType());
                }
                return;
            }

            case BulletMLNode::vanish:
                runner_->doVanish();
                return;

            default:
                return;
        }
    }

private:
    BulletMLParserBLB* parser_;
    BulletMLRunner* runner_;

    bool end_;
    int wait_until_turn_;

    Task* tasks_;
    uint16_t task_count_;
    uint16_t task_capacity_;

    Fxp* params_;
    uint16_t param_count_;
    bool owns_params_;

    LinearChange change_dir_;
    LinearChange change_speed_;
    LinearChange accel_x_;
    LinearChange accel_y_;
    bool change_dir_active_;
    bool change_speed_active_;
    bool accel_x_active_;
    bool accel_y_active_;

    bool has_spd_;
    bool has_dir_;
    bool has_prev_spd_;
    bool has_prev_dir_;
    Fxp spd_;
    Fxp dir_;
    Fxp prev_spd_;
    Fxp prev_dir_;

    uint8_t expand_name_;
    uint32_t expand_ref_id_;
    uint32_t expand_fanout_;
    uint32_t expand_child_count_;
    // Shared across every instance (not per-object) so this genuinely caps
    // total log volume for the game session, matching the sParameterCopyLogs
    // / sPoolExhaustedLogs pattern used elsewhere for the same purpose. A
    // per-instance counter here would reset to 0 on every pattern-loop
    // restart (i.e. constantly), defeating the "log only the first few
    // times" intent entirely.
    static inline uint16_t capacity_fail_logs_ = 0;
    uint16_t peak_task_count_;  // Track peak utilization for telemetry

    BulletMLRunnerImpl(const BulletMLRunnerImpl&);
    BulletMLRunnerImpl& operator=(const BulletMLRunnerImpl&);
};

// Pool for BulletMLRunnerImpl objects and the BulletMLRunnerImpl* arrays that
// point at them. Every FoeCommand recreation (every bullet-pattern loop
// restart) used to allocate/free these raw via lwnew/delete, unpooled -
// unlike BulletMLState/task-buffers/FoeCommand, which all went through a
// startup-preallocated free-list. That raw churn progressively fragmented
// the LWRAM heap over a long play session, causing lwnew to get slower and
// slower - the confirmed root cause of the observed FPS collapse. This pool
// closes that gap using the same free-list technique as the sibling pools,
// and reuses bulletml_state_pool::StatePoolState's startup-only-allocation
// flags so one master switch still governs every BulletML allocation site.
namespace bulletml_runner_impl_pool {
struct FreeNode {
    FreeNode* next;
};
struct PoolState {
    static inline FreeNode* freeList = nullptr;
    static inline std::size_t cachedCount = 0;
};
}

inline BulletMLRunnerImpl* createPooledBulletMlRunnerImpl(BulletMLState* state, BulletMLRunner* runner) {
    using bulletml_runner_impl_pool::FreeNode;
    using bulletml_runner_impl_pool::PoolState;

    if (PoolState::freeList) {
        FreeNode* node = PoolState::freeList;
        PoolState::freeList = node->next;
        if (PoolState::cachedCount > 0) {
            PoolState::cachedCount--;
        }
        return new (node) BulletMLRunnerImpl(state, runner);
    }

    if (bulletml_state_pool::StatePoolState::startupOnlyAllocation &&
        !bulletml_state_pool::StatePoolState::startupPreallocationPhase) {
        return nullptr;
    }

    return allocBulletMlObject<BulletMLRunnerImpl>("runner.impl", state, runner);
}

inline void destroyPooledBulletMlRunnerImpl(BulletMLRunnerImpl*& impl) {
    using bulletml_runner_impl_pool::FreeNode;
    using bulletml_runner_impl_pool::PoolState;

    if (!impl) {
        return;
    }
    impl->~BulletMLRunnerImpl();
    FreeNode* node = reinterpret_cast<FreeNode*>(impl);
    node->next = PoolState::freeList;
    PoolState::freeList = node;
    PoolState::cachedCount++;
    impl = nullptr;
}

// Pre-allocates a guaranteed pool of BulletMLRunnerImpl storage at startup.
// A null state/runner pair makes the constructor a safe, fully-inert no-op
// (end_=true immediately, no tasks/params touched), so its memory can be
// reclaimed straight back onto the free list - mirrors how
// preallocateBulletMlStatePools() preallocates BulletMLState the same way.
inline bool preallocateBulletMlRunnerImplPool(uint16_t count) {
    using bulletml_runner_impl_pool::FreeNode;
    using bulletml_runner_impl_pool::PoolState;

    for (uint16_t i = 0; i < count; ++i) {
        BulletMLRunnerImpl* obj = allocBulletMlObject<BulletMLRunnerImpl>("runner.impl.prealloc", nullptr, nullptr);
        if (!obj) {
            return false;
        }
        obj->~BulletMLRunnerImpl();
        FreeNode* node = reinterpret_cast<FreeNode*>(obj);
        node->next = PoolState::freeList;
        PoolState::freeList = node;
        PoolState::cachedCount++;
    }
    return true;
}

// Pre-allocates the BulletMLRunnerImpl* array-pool buckets (same bucketed
// capacities 4,8,...,32 as the BulletMLNode*/Fxp* array pools in
// bulletmlstate.hpp). impls_ is almost always capacity 1 (a single top-level
// action), so counts should be heavily weighted toward bucket 0.
inline bool preallocateBulletMlRunnerImplArrayPool(const uint16_t* counts) {
    for (uint16_t bucketIndex = 0; bucketIndex < 8u; ++bucketIndex) {
        const uint16_t capacity = static_cast<uint16_t>((bucketIndex + 1u) * 4u);
        const uint16_t n = counts ? counts[bucketIndex] : 0u;
        for (uint16_t i = 0; i < n; ++i) {
            BulletMLRunnerImpl** ptr = createBulletMlRuntimeArray<BulletMLRunnerImpl*>(capacity);
            if (!ptr) {
                return false;
            }
            auto* node = reinterpret_cast<bulletml_state_pool::ArrayFreeNode<BulletMLRunnerImpl*>*>(ptr);
            node->next = bulletml_state_pool::ArrayPoolState<BulletMLRunnerImpl*>::freeLists[bucketIndex];
            bulletml_state_pool::ArrayPoolState<BulletMLRunnerImpl*>::freeLists[bucketIndex] = node;
            bulletml_state_pool::ArrayPoolState<BulletMLRunnerImpl*>::cachedCounts[bucketIndex]++;
        }
    }
    return true;
}

inline std::size_t getBulletMlRunnerImplCachedCount() {
    return bulletml_runner_impl_pool::PoolState::cachedCount;
}

inline void releaseBulletMlRunnerImplPool() {
    using bulletml_runner_impl_pool::FreeNode;
    using bulletml_runner_impl_pool::PoolState;

    while (PoolState::freeList) {
        FreeNode* node = PoolState::freeList;
        PoolState::freeList = node->next;
        freeBulletMlRuntimeRaw(node, sizeof(BulletMLRunnerImpl), false);
    }
    PoolState::cachedCount = 0;
}

inline BulletMLRunner::BulletMLRunner(BulletMLParserBLB* parser)
    : parser_(parser), state_(nullptr), impls_(nullptr), impl_count_(0), impl_capacity_(0) {
    if (!parser_) return;

    uint32_t top_count_u32 = 0;
    BulletMLNode** top_actions = parser_->getTopActions(&top_count_u32);
    if (!top_actions || top_count_u32 == 0) return;

    uint16_t top_count = (top_count_u32 > 65535U) ? 65535U : static_cast<uint16_t>(top_count_u32);
    uint16_t impls_capacity = 0;
    impls_ = bulletml_state_pool::createPooledArray<BulletMLRunnerImpl*>(top_count, impls_capacity);
    if (!impls_) return;
    impl_capacity_ = impls_capacity;

    for (uint16_t i = 0; i < top_count; ++i) impls_[i] = nullptr;

    for (uint16_t i = 0; i < top_count; ++i) {
        BulletMLNode** nodes = createBulletMlStateNodeArray(1);
        if (!nodes) break;
        nodes[0] = top_actions[i];

        BulletMLState* st = createBulletMlState(parser_, nodes, 1, nullptr, 0);
        if (!st) {
            destroyBulletMlStateNodeArray(nodes, 1);
            break;
        }

        BulletMLRunnerImpl* impl = createPooledBulletMlRunnerImpl(st, this);
        if (!impl) {
            destroyBulletMlState(st);
            break;
        }

        impls_[impl_count_++] = impl;
    }
}

inline BulletMLRunner::BulletMLRunner(BulletMLState* state)
    : parser_(nullptr), state_(state), impls_(nullptr), impl_count_(0), impl_capacity_(0) {
    if (!state_) return;

    parser_ = state_->getParser();
    uint16_t impls_capacity = 0;
    impls_ = bulletml_state_pool::createPooledArray<BulletMLRunnerImpl*>(1, impls_capacity);
    if (!impls_) return;
    impl_capacity_ = impls_capacity;

    impls_[0] = createPooledBulletMlRunnerImpl(state_, this);
    if (!impls_[0]) {
        destroyBulletMlState(state_);
        bulletml_state_pool::recyclePooledArray(impls_, impl_capacity_);
        impls_ = nullptr;
        return;
    }

    impl_count_ = 1;
    state_ = nullptr;  // ownership moved to impl
}

inline BulletMLRunner::~BulletMLRunner() {
    for (uint16_t i = 0; i < impl_count_; ++i) {
        destroyPooledBulletMlRunnerImpl(impls_[i]);
    }
    bulletml_state_pool::recyclePooledArray(impls_, impl_capacity_);
    impl_count_ = 0;
    impl_capacity_ = 0;

    destroyBulletMlState(state_);
    state_ = nullptr;
}

inline void BulletMLRunner::run() {
    for (uint16_t i = 0; i < impl_count_; ++i) {
        if (impls_[i]) impls_[i]->run();
    }
}

inline bool BulletMLRunner::isEnd() {
    for (uint16_t i = 0; i < impl_count_; ++i) {
        if (impls_[i] && !impls_[i]->isEnd()) {
            return false;
        }
    }
    return true;
}

#endif // BULLETMLRUNNER_HPP_
