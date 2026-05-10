#ifndef BULLETMLRUNNER_HPP_
#define BULLETMLRUNNER_HPP_

#include <cstddef>
#include <cstdint>
#include <new>
#include <srl.hpp>

#include "bulletmlparser_blb.hpp"
#include <srl_log.hpp>

using SRL::Math::Types::Fxp;

class BulletMLRunnerImpl;

/// BulletML Runner - Base class for BulletML execution.
class BulletMLRunner {
public:
    static constexpr uint16_t kMaxImpls = 8;

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
    void requestStop() { stop_requested_ = true; }

    BulletMLParserBLB* parser_;
    BulletMLState* state_;
    BulletMLRunnerImpl** impls_;
    BulletMLRunnerImpl* impls_local_[kMaxImpls];
    uint16_t impl_count_;
    bool stop_requested_;

private:
    BulletMLRunner(const BulletMLRunner&);
    BulletMLRunner& operator=(const BulletMLRunner&);
};

class BulletMLRunnerImpl {
public:
    static constexpr uint16_t kMaxTasks = 128;
    static constexpr uint16_t kMaxParamDepth = 8;
    static constexpr uint16_t kPoolCapacity = 256;

    static uint32_t getImplPoolRejectCount() { return sImplPoolRejectCount; }
    static uint32_t getTaskCapRejectCount() { return sTaskCapRejectCount; }
    static uint32_t getNodeCapRejectCount() { return sNodeCapRejectCount; }
    static uint32_t getParamDepthRejectCount() { return sParamDepthRejectCount; }
    static uint32_t getRefParamCapRejectCount() { return sRefParamCapRejectCount; }
    static void resetRejectCounters() {
        sImplPoolRejectCount = 0;
        sTaskCapRejectCount = 0;
        sNodeCapRejectCount = 0;
        sParamDepthRejectCount = 0;
        sRefParamCapRejectCount = 0;
    }

    // Must be called once at startup (preloader initializes LWRAM before main()).
    static void initPool();

    BulletMLRunnerImpl(BulletMLState* state, BulletMLRunner* runner)
        : parser_(nullptr),
          runner_(runner),
          end_(false),
          wait_until_turn_(0),
          nodes_(nodes_local_),
          node_count_(0),
          tasks_(tasks_local_),
          task_count_(0),
          task_capacity_(kMaxTasks),
          tasks_owned_(false),
          params_(nullptr),
          param_count_(0),
          ref_param_depth_(0),
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
          prev_dir_(Fxp::Convert(0)) {
#if HW_DEBUG
        static uint32_t sImplCtorProbeCount = 0u;
        const bool probeImplCtor = (sImplCtorProbeCount < 8u) || ((sImplCtorProbeCount % 240u) == 0u);
        if (probeImplCtor) {
            SRL::Logger::LogInfo("[BML-IMPL] init idx=%lu state=%08lx",
                                 (unsigned long)sImplCtorProbeCount,
                                 (unsigned long)(uintptr_t)state);
        }
#endif
        if (!state || !runner_) {
            end_ = true;
#if HW_DEBUG
            sImplCtorProbeCount++;
#endif
            return;
        }

        parser_ = state->getParser();
        params_ = local_params_;

        const uint16_t stateNodeCount = state->getNodeCount();
        BulletMLNode** stateNodes = state->getNodes();
        uint16_t copyNodeCount = stateNodeCount;
        if (copyNodeCount > BulletMLState::kMaxNodes) {
            sNodeCapRejectCount++;
            SRL::Logger::LogWarning("[BML-RUNNER] Node count %u exceeds cap %u; truncating",
                                    (unsigned int)copyNodeCount,
                                    (unsigned int)BulletMLState::kMaxNodes);
            copyNodeCount = BulletMLState::kMaxNodes;
        }

        const uint16_t seededNodeCount = (stateNodes && copyNodeCount > 0) ? copyNodeCount : 0;
        node_count_ = seededNodeCount;

        copyParameters(state->getParameters(), state->getParameterCount());

        const uint32_t requiredTaskCount32 = static_cast<uint32_t>(seededNodeCount) + 16u;
        const uint16_t requiredTaskCount = (requiredTaskCount32 > 0xFFFFu)
                                               ? 0xFFFFu
                                               : static_cast<uint16_t>(requiredTaskCount32);

        if (requiredTaskCount > kMaxTasks) {
            task_capacity_ = requiredTaskCount;
            tasks_ = static_cast<Task*>(SRL::Memory::LowWorkRam::Malloc(sizeof(Task) * task_capacity_));
            if (!tasks_) {
                tasks_ = static_cast<Task*>(SRL::Memory::HighWorkRam::Malloc(sizeof(Task) * task_capacity_));
            }
            if (!tasks_) {
                sTaskCapRejectCount++;
                SRL::Logger::LogWarning("[BML-RUNNER] Task buffer alloc failed (%u)", task_capacity_);
                end_ = true;
#if HW_DEBUG
                sImplCtorProbeCount++;
#endif
                return;
            }
            tasks_owned_ = true;
        } else {
            tasks_ = tasks_local_;
            task_capacity_ = kMaxTasks;
        }

        for (int i = static_cast<int>(seededNodeCount) - 1; i >= 0; --i) {
            pushNodeTask(stateNodes ? stateNodes[i] : nullptr);
        }
        // wait_until_turn_ initialized to 0 in member initializer list.
        // Avoid calling runner_->getTurn() here: virtual dispatch on a partially-
        // constructed object (vtable is still base-class during base-ctor execution).

        delete state;

#if HW_DEBUG
        if (probeImplCtor) {
            SRL::Logger::LogInfo("[BML-IMPL] done idx=%lu nodes=%u tasks=%u",
                                 (unsigned long)sImplCtorProbeCount,
                                 (unsigned int)seededNodeCount,
                                 (unsigned int)task_count_);
        }
        sImplCtorProbeCount++;
#endif
    }

    ~BulletMLRunnerImpl() {
        Task* ownedTasks = tasks_;
        const bool owned = tasks_owned_;
        nodes_ = nullptr;
        node_count_ = 0;

        task_count_ = 0;
        task_capacity_ = 0;
        if (owned && ownedTasks) {
            SRL::Memory::Free(ownedTasks);
        }
        tasks_ = nullptr;
        tasks_owned_ = false;

        params_ = local_params_;
        param_count_ = 0;
        ref_param_depth_ = 0;
    }

    static void* operator new(std::size_t) noexcept;
    static void operator delete(void* ptr) noexcept;

    bool isEnd() const { return end_; }

#if HW_DEBUG
    void debugLogRawState(uint16_t idx) const {
        const uintptr_t self = (uintptr_t)this;
        const uintptr_t vptr = *(const uintptr_t*)this;
        const uintptr_t runner = (uintptr_t)runner_;
        SRL::Logger::LogInfo("[BML-RAW] idx=%u self=%08x vptr=%08x runner=%08x end=%d tasks=%u wait=%d nodes=%u",
                             idx,
                             (unsigned int)self,
                             (unsigned int)vptr,
                             (unsigned int)runner,
                             end_ ? 1 : 0,
                             (unsigned int)task_count_,
                             wait_until_turn_,
                             (unsigned int)node_count_);
    }
#endif

    void run() {
        if (end_) return;

#if HW_DEBUG
        static uint32_t sProbeRuns = 0u;
        const bool probeRun = (sProbeRuns < 8u) && (runner_ != nullptr) && (runner_->getTurn() < 2);
        if (probeRun) {
            SRL::Logger::LogInfo("[BML-RUN] begin turn=%d tasks=%u wait=%d end=%d",
                                 runner_->getTurn(), task_count_, wait_until_turn_, end_ ? 1 : 0);
        }
#endif

        applyChanges();

#if HW_DEBUG
        if (probeRun) {
            SRL::Logger::LogInfo("[BML-RUN] post-apply turn=%d tasks=%u wait=%d",
                                 runner_->getTurn(), task_count_, wait_until_turn_);
        }
#endif

        const int now = runner_->getTurn();
        if (task_count_ == 0) {
            if (!change_dir_active_ && !change_speed_active_ && !accel_x_active_ && !accel_y_active_) {
                end_ = true;
            }
            return;
        }

        if (now < wait_until_turn_) return;

        int safety = 0;
        while (task_count_ > 0 && runner_->getTurn() >= wait_until_turn_) {
            Task task;
            if (!popTask(task)) break;

#if HW_DEBUG
            if (probeRun) {
                SRL::Logger::LogInfo("[BML-RUN] task type=%d node=%d remain=%u",
                                     static_cast<int>(task.type),
                                     task.node ? static_cast<int>(task.node->getNameAsName()) : -1,
                                     task_count_);
            }
#endif

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
                    params_ = task.saved_params;
                    param_count_ = task.saved_count;
                    ref_param_depth_ = task.saved_ref_param_depth;
                    break;
            }

#if HW_DEBUG
            if (probeRun) {
                SRL::Logger::LogInfo("[BML-RUN] task-done type=%d remain=%u wait=%d",
                                     static_cast<int>(task.type), task_count_, wait_until_turn_);
            }
#endif

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

#if HW_DEBUG
        if (probeRun) {
            SRL::Logger::LogInfo("[BML-RUN] end tasks=%u wait=%d end=%d",
                                 task_count_, wait_until_turn_, end_ ? 1 : 0);
            sProbeRuns++;
        }
#endif
    }

private:
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
        uint8_t saved_ref_param_depth;
    };

    struct LinearChange {
        int start_turn;
        int end_turn;
        Fxp first;
        Fxp last;
        Fxp gradient;
    };

    bool pushTask(const Task& task) {
        if (task_count_ >= task_capacity_) {
            sTaskCapRejectCount++;
            SRL::Logger::LogWarning("[BML-RUNNER] Task cap reached (%u)", task_capacity_);
            return false;
        }
        tasks_[task_count_++] = task;
        return true;
    }

    bool popTask(Task& out) {
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
        t.saved_ref_param_depth = ref_param_depth_;
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
        t.saved_ref_param_depth = ref_param_depth_;
        return pushTask(t);
    }

    bool pushPopParamsTask(Fxp* saved_params, uint16_t saved_count, uint8_t saved_ref_param_depth) {
        Task t;
        t.type = TASK_POP_PARAMS;
        t.node = nullptr;
        t.repeat_action = nullptr;
        t.repeat_remaining = 0;
        t.saved_params = saved_params;
        t.saved_count = saved_count;
        t.saved_ref_param_depth = saved_ref_param_depth;
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
        params_ = local_params_;
        param_count_ = 0;
        ref_param_depth_ = 0;

        if (!src || count == 0) return;

        if (count > BulletMLState::kMaxParameters) {
            SRL::Logger::LogWarning("[BML-RUNNER] Param count %u exceeds cap %u; truncating", count, BulletMLState::kMaxParameters);
            count = BulletMLState::kMaxParameters;
        }

        for (uint16_t i = 0; i < count; ++i) {
            local_params_[i] = src[i];
        }

        param_count_ = count;
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

    bool collectRefParameters(BulletMLNode* ref_node, Fxp* out_params, uint16_t& out_count) {
        out_count = 0;
        if (!ref_node || !out_params) return false;

        const uint16_t param_nodes = countChildren(ref_node, BulletMLNode::param);
        if (param_nodes == 0) return true;

        const uint16_t total = static_cast<uint16_t>(param_nodes + 1);
        if (total > BulletMLState::kMaxParameters) {
            sRefParamCapRejectCount++;
            SRL::Logger::LogWarning("[BML-RUNNER] Ref param count %u exceeds cap %u", total, BulletMLState::kMaxParameters);
            return false;
        }

        out_params[0] = 0.0;  // 1-based parameters
        uint16_t w = 1;
        const uint32_t child_count = ref_node->getChildCount();
        for (uint32_t i = 0; i < child_count && w < total; ++i) {
            BulletMLNode* c = ref_node->getChild(i);
            if (c && c->getNameAsName() == BulletMLNode::param) {
                out_params[w++] = evalNodeValue(c);
            }
        }

        out_count = total;
        return true;
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

                if (total > BulletMLState::kMaxNodes) {
                    sNodeCapRejectCount++;
                    SRL::Logger::LogWarning("[BML-RUNNER] Bullet action count %u exceeds cap %u; dropping bullet", total, BulletMLState::kMaxNodes);
                    return;
                }

                BulletMLNode* acts[BulletMLState::kMaxNodes] = {};

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

                BulletMLState* st = hwnew BulletMLState(parser_, acts, total, params_, param_count_);
                if (!st) {
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
                    pushNodeTask(bullet);
                }
                return;
            }

            case BulletMLNode::action: {
                const uint32_t child_count = node->getChildCount();
                for (int i = static_cast<int>(child_count) - 1; i >= 0; --i) {
                    pushNodeTask(node->getChild(static_cast<uint32_t>(i)));
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

                pushRepeatTask(action, times_num - 1);
                pushNodeTask(action);
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

                if (ref_param_depth_ >= kMaxParamDepth) {
                    sParamDepthRejectCount++;
                    SRL::Logger::LogWarning("[BML-RUNNER] Ref param depth exceeded cap %u", kMaxParamDepth);
                    return;
                }

                uint16_t new_count = 0;
                Fxp* new_params = ref_params_[ref_param_depth_];
                if (!collectRefParameters(node, new_params, new_count)) {
                    return;
                }

                if (!pushPopParamsTask(params_, param_count_, ref_param_depth_)) {
                    SRL::Logger::LogWarning("[BML-RUNNER] Failed to push param restore task for ref id=%u", id);
                    return;
                }

                ref_param_depth_++;
                params_ = new_params;
                param_count_ = new_count;
                pushNodeTask(target);
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
    static inline uint32_t sImplPoolRejectCount = 0;
    static inline uint32_t sTaskCapRejectCount = 0;
    static inline uint32_t sNodeCapRejectCount = 0;
    static inline uint32_t sParamDepthRejectCount = 0;
    static inline uint32_t sRefParamCapRejectCount = 0;

    BulletMLParserBLB* parser_;
    BulletMLRunner* runner_;

    bool end_;
    int wait_until_turn_;

    BulletMLNode** nodes_;
    uint16_t node_count_;
    BulletMLNode* nodes_local_[BulletMLState::kMaxNodes];

    Task* tasks_;
    uint16_t task_count_;
    uint16_t task_capacity_;
    Task tasks_local_[kMaxTasks];
    bool tasks_owned_;

    Fxp* params_;
    uint16_t param_count_;
    uint8_t ref_param_depth_;
    Fxp local_params_[BulletMLState::kMaxParameters];
    Fxp ref_params_[kMaxParamDepth][BulletMLState::kMaxParameters];

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

    BulletMLRunnerImpl(const BulletMLRunnerImpl&);
    BulletMLRunnerImpl& operator=(const BulletMLRunnerImpl&);
};

namespace bulletml_runner_impl_pool_internal {
struct Storage {
    alignas(std::max_align_t) unsigned char pool[BulletMLRunnerImpl::kPoolCapacity][sizeof(BulletMLRunnerImpl)];
    bool used[BulletMLRunnerImpl::kPoolCapacity];
};

inline Storage* gStoragePtr = nullptr;

inline Storage& getStorage()
{
    return *gStoragePtr;
}
} // namespace bulletml_runner_impl_pool_internal

inline void* BulletMLRunnerImpl::operator new(std::size_t) noexcept
{
    auto& sStorage = bulletml_runner_impl_pool_internal::getStorage();
    for (uint16_t i = 0; i < kPoolCapacity; ++i)
    {
        if (!sStorage.used[i])
        {
            sStorage.used[i] = true;
            return (void*)&sStorage.pool[i][0];
        }
    }

    sImplPoolRejectCount++;
    SRL::Logger::LogWarning("[BML-RUNNER] Impl pool exhausted (%u)", kPoolCapacity);
    return nullptr;
}

inline void BulletMLRunnerImpl::operator delete(void* ptr) noexcept
{
    if (!ptr)
    {
        return;
    }

    auto& sStorage = bulletml_runner_impl_pool_internal::getStorage();
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
        SRL::Logger::LogFatal("[BML-RUNNER] Bad free ptr=%08lx", (unsigned long)p);
        SRL::System::Exit(1);
    }

    const uint16_t slot = (uint16_t)(offset / sizeof(sStorage.pool[0]));
    if (slot < kPoolCapacity)
    {
        sStorage.used[slot] = false;
    }
}

inline void BulletMLRunnerImpl::initPool() {
    // SGL preloader (runs before main()) calls SRL::Memory::Initialize() which sets up
    // LWRAM TLSF at 0x00200000. Allocate pool here so it doesn't consume HWRAM BSS.
    // sizeof(Storage) ~389KB; LWRAM has ~410KB free after screen buffers.
    const size_t storageSize = sizeof(bulletml_runner_impl_pool_internal::Storage);

    void* mem = SRL::Memory::LowWorkRam::Malloc(storageSize);
    if (!mem)
    {
        SRL::Logger::LogWarning("[BML-RUNNER] LWRAM pool alloc failed (%lu), trying HWRAM", (unsigned long)storageSize);
        mem = SRL::Memory::HighWorkRam::Malloc(storageSize);
    }

    if (!mem)
    {
        SRL::Logger::LogFatal("[BML-RUNNER] Pool alloc failed (%lu)", (unsigned long)storageSize);
        SRL::System::Exit(1);
    }

    bulletml_runner_impl_pool_internal::gStoragePtr =
        new (mem) bulletml_runner_impl_pool_internal::Storage{};
}

inline BulletMLRunner::BulletMLRunner(BulletMLParserBLB* parser)
    : parser_(parser), state_(nullptr), impls_(impls_local_), impl_count_(0), stop_requested_(false) {
    if (!parser_) return;

    for (uint16_t i = 0; i < kMaxImpls; ++i) impls_local_[i] = nullptr;

    uint32_t top_count_u32 = 0;
    BulletMLNode** top_actions = parser_->getTopActions(&top_count_u32);
    if (!top_actions || top_count_u32 == 0) return;

    uint16_t top_count = (top_count_u32 > BulletMLState::kMaxNodes)
                             ? BulletMLState::kMaxNodes
                             : static_cast<uint16_t>(top_count_u32);

    BulletMLState* st = hwnew BulletMLState(parser_, top_actions, top_count, nullptr, 0);
    if (!st) return;

    BulletMLRunnerImpl* impl = hwnew BulletMLRunnerImpl(st, this);
    if (!impl) {
        delete st;
        return;
    }

    impls_[impl_count_++] = impl;
}

inline BulletMLRunner::BulletMLRunner(BulletMLState* state)
    : parser_(nullptr), state_(state), impls_(impls_local_), impl_count_(0), stop_requested_(false) {
    if (!state_) return;

    for (uint16_t i = 0; i < kMaxImpls; ++i) impls_local_[i] = nullptr;

    parser_ = state_->getParser();
    impls_[0] = hwnew BulletMLRunnerImpl(state_, this);
    if (!impls_[0]) {
        return;
    }

    impl_count_ = 1;
    state_ = nullptr;  // ownership moved to impl
}

inline BulletMLRunner::~BulletMLRunner() {
    for (uint16_t i = 0; i < impl_count_; ++i) {
        delete impls_[i];
        impls_[i] = nullptr;
    }
    impls_ = impls_local_;
    impl_count_ = 0;

    delete state_;
    state_ = nullptr;
}

inline void BulletMLRunner::run() {
    for (uint16_t i = 0; i < impl_count_; ++i) {
        if (stop_requested_) break;
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
