#ifndef BULLETMLRUNNER_HPP_
#define BULLETMLRUNNER_HPP_

#include <cstdint>

#include "bulletmlparser_blb.hpp"
#include <srl_log.hpp>

class BulletMLRunnerImpl;

/// BulletML Runner - Base class for BulletML execution.
class BulletMLRunner {
public:
    explicit BulletMLRunner(BulletMLParserBLB* parser);
    explicit BulletMLRunner(BulletMLState* state);
    virtual ~BulletMLRunner();

    /// Get bullet direction (degrees)
    virtual double getBulletDirection() { return 0.0; }

    /// Get aim direction (degrees)
    virtual double getAimDirection() { return 0.0; }

    /// Get bullet speed
    virtual double getBulletSpeed() { return 0.0; }

    /// Get default speed
    virtual double getDefaultSpeed() { return 1.0; }

    /// Get difficulty rank [0..1]
    virtual double getRank() { return 0.0; }

    /// Random source for formulas
    virtual double getRand() { return 0.0; }

    /// Create a simple bullet
    virtual void createSimpleBullet(double direction, double speed) {
        (void)direction;
        (void)speed;
    }

    /// Create a bullet with action state
    virtual void createBullet(BulletMLState* state, double direction, double speed) {
        (void)state;
        (void)direction;
        (void)speed;
    }

    /// Current turn/frame
    virtual int getTurn() { return 0; }

    /// Vanish callback
    virtual void doVanish() {}

    /// Optional motion/accel hooks
    virtual void doChangeDirection(double) {}
    virtual void doChangeSpeed(double) {}
    virtual void doAccelX(double) {}
    virtual void doAccelY(double) {}
    virtual double getBulletSpeedX() { return 0.0; }
    virtual double getBulletSpeedY() { return 0.0; }

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
          nodes_(nullptr),
          node_count_(0),
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
          spd_(0.0),
          dir_(0.0),
          prev_spd_(0.0),
          prev_dir_(0.0) {
        if (!state || !runner_) {
            end_ = true;
            return;
        }

        parser_ = state->getParser();

        node_count_ = state->getNodeCount();
        if (node_count_ > 0 && state->getNodes()) {
            nodes_ = lwnew BulletMLNode*[node_count_];
            if (!nodes_) {
                end_ = true;
                delete state;
                return;
            }
            for (uint16_t i = 0; i < node_count_; ++i) {
                nodes_[i] = state->getNodes()[i];
            }
        }

        copyParameters(state->getParameters(), state->getParameterCount());

        delete state;

        if (!ensureTaskCapacity(static_cast<uint16_t>(node_count_ + 8))) {
            end_ = true;
            return;
        }

        for (int i = static_cast<int>(node_count_) - 1; i >= 0; --i) {
            pushNodeTask(nodes_[i]);
        }
        wait_until_turn_ = runner_->getTurn();
    }

    ~BulletMLRunnerImpl() {
        delete[] nodes_;
        nodes_ = nullptr;
        node_count_ = 0;

        delete[] tasks_;
        tasks_ = nullptr;
        task_count_ = 0;
        task_capacity_ = 0;

        if (owns_params_) {
            delete[] params_;
        }
        params_ = nullptr;
        param_count_ = 0;
        owns_params_ = false;
    }

    bool isEnd() const { return end_; }

    void run() {
        if (end_) return;

        applyChanges();

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
                        delete[] params_;
                    }
                    params_ = task.saved_params;
                    param_count_ = task.saved_count;
                    owns_params_ = task.saved_owns;
                    break;
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
        double* saved_params;
        uint16_t saved_count;
        bool saved_owns;
    };

    struct LinearChange {
        int start_turn;
        int end_turn;
        double first;
        double last;
        double gradient;
    };

    bool ensureTaskCapacity(uint16_t needed) {
        if (needed <= task_capacity_) return true;

        uint16_t new_cap = (task_capacity_ == 0) ? 16 : task_capacity_;
        while (new_cap < needed) {
            uint16_t grown = static_cast<uint16_t>(new_cap * 2);
            if (grown <= new_cap) {
                new_cap = needed;
                break;
            }
            new_cap = grown;
        }

        Task* t = lwnew Task[new_cap];
        if (!t) return false;
        for (uint16_t i = 0; i < task_count_; ++i) {
            t[i] = tasks_[i];
        }
        delete[] tasks_;
        tasks_ = t;
        task_capacity_ = new_cap;
        return true;
    }

    bool pushTask(const Task& task) {
        if (!ensureTaskCapacity(static_cast<uint16_t>(task_count_ + 1))) {
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

    bool pushPopParamsTask(double* saved_params, uint16_t saved_count, bool saved_owns) {
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

    double parseNumberLiteral(const char*& p) const {
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

        double value = 0.0;
        while (isDigit(*q)) {
            value = value * 10.0 + static_cast<double>(*q - '0');
            ++q;
        }

        if (*q == '.') {
            ++q;
            double base = 0.1;
            while (isDigit(*q)) {
                value += static_cast<double>(*q - '0') * base;
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
            double scale = 1.0;
            while (exp-- > 0) scale *= 10.0;
            if (exp_neg) value /= scale;
            else value *= scale;
        }

        return neg ? -value : value;
    }

    double parseExpression(const char*& p) {
        double v = parseTerm(p);
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

    double parseTerm(const char*& p) {
        double v = parseFactor(p);
        while (true) {
            skipSpaces(p);
            if (*p == '*') {
                ++p;
                v *= parseFactor(p);
            } else if (*p == '/') {
                ++p;
                const double rhs = parseFactor(p);
                if (rhs != 0.0) v /= rhs;
            } else {
                break;
            }
        }
        return v;
    }

    double parseVariable(const char*& p) {
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

    double parseFactor(const char*& p) {
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
            const double v = parseExpression(p);
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

    double evalNodeValue(const BulletMLNode* node) {
        if (!node) return 0.0;
        const char* s = node->getValue();
        if (!s || s[0] == '\0') return 0.0;
        const char* p = s;
        return parseExpression(p);
    }

    void copyParameters(const double* src, uint16_t count) {
        if (owns_params_) {
            delete[] params_;
            owns_params_ = false;
        }
        params_ = nullptr;
        param_count_ = 0;

        if (!src || count == 0) return;

        double* p = lwnew double[count];
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

    double getDirection(BulletMLNode* dir_node, bool update_prev) {
        if (!dir_node) return runner_->getAimDirection();

        double dir = evalNodeValue(dir_node);
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

    double getSpeed(BulletMLNode* spd_node) {
        if (!spd_node) return runner_->getDefaultSpeed();

        double spd = evalNodeValue(spd_node);
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

    static double linearValue(const LinearChange& lc, int turn) {
        return lc.first + lc.gradient * static_cast<double>(turn - lc.start_turn);
    }

    static void setLinear(LinearChange& lc, bool& active,
                          int start_turn, int end_turn,
                          double first, double last) {
        lc.start_turn = start_turn;
        lc.end_turn = end_turn;
        lc.first = first;
        lc.last = last;
        if (end_turn != start_turn) {
            lc.gradient = (last - first) / static_cast<double>(end_turn - start_turn);
        } else {
            lc.gradient = 0.0;
        }
        active = true;
    }

    static double absd(double v) {
        return (v < 0.0) ? -v : v;
    }

    void calcChangeDirection(double direction, int term, bool seq) {
        const int now = runner_->getTurn();
        if (term <= 0) {
            runner_->doChangeDirection(direction);
            change_dir_active_ = false;
            return;
        }

        const int final_turn = now + term;
        const double first = runner_->getBulletDirection();

        if (seq) {
            setLinear(change_dir_, change_dir_active_, now, final_turn,
                      first, first + direction * static_cast<double>(term));
            return;
        }

        const double d1 = direction - first;
        const double d2 = (d1 > 0.0) ? (d1 - 360.0) : (d1 + 360.0);
        const double d = (absd(d1) < absd(d2)) ? d1 : d2;

        setLinear(change_dir_, change_dir_active_, now, final_turn, first, first + d);
    }

    void calcChangeSpeed(double speed, int term) {
        const int now = runner_->getTurn();
        if (term <= 0) {
            runner_->doChangeSpeed(speed);
            change_speed_active_ = false;
            return;
        }
        setLinear(change_speed_, change_speed_active_, now, now + term,
                  runner_->getBulletSpeed(), speed);
    }

    void calcAccelX(double value, int term, BulletMLNode::Type type) {
        const int now = runner_->getTurn();
        if (term <= 0) {
            runner_->doAccelX(value);
            accel_x_active_ = false;
            return;
        }

        const double first = runner_->getBulletSpeedX();
        double last = value;
        if (type == BulletMLNode::type_sequence) {
            last = first + value * static_cast<double>(term);
        } else if (type == BulletMLNode::type_relative) {
            last = first + value;
        }

        setLinear(accel_x_, accel_x_active_, now, now + term, first, last);
    }

    void calcAccelY(double value, int term, BulletMLNode::Type type) {
        const int now = runner_->getTurn();
        if (term <= 0) {
            runner_->doAccelY(value);
            accel_y_active_ = false;
            return;
        }

        const double first = runner_->getBulletSpeedY();
        double last = value;
        if (type == BulletMLNode::type_sequence) {
            last = first + value * static_cast<double>(term);
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

    double* collectRefParameters(BulletMLNode* ref_node, uint16_t& out_count) {
        out_count = 0;
        if (!ref_node) return nullptr;

        const uint16_t param_nodes = countChildren(ref_node, BulletMLNode::param);
        if (param_nodes == 0) return nullptr;

        const uint16_t total = static_cast<uint16_t>(param_nodes + 1);
        double* out = lwnew double[total];
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

                BulletMLNode** acts = lwnew BulletMLNode*[total];
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

                BulletMLState* st = lwnew BulletMLState(parser_, acts, total, params_, param_count_);
                if (!st) {
                    delete[] acts;
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
                int frame = static_cast<int>(evalNodeValue(node));
                if (frame < 0) frame = 0;
                wait_until_turn_ = runner_->getTurn() + frame;
                return;
            }

            case BulletMLNode::repeat: {
                BulletMLNode* times = findChild(node, BulletMLNode::times);
                BulletMLNode* action = findChild(node, BulletMLNode::action);
                if (!action) action = findChild(node, BulletMLNode::actionRef);
                if (!times || !action) return;

                int times_num = static_cast<int>(evalNodeValue(times));
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

                uint16_t new_count = 0;
                double* new_params = collectRefParameters(node, new_count);

                if (!pushPopParamsTask(params_, param_count_, owns_params_)) {
                    delete[] new_params;
                    SRL::Logger::LogWarning("[BML-RUNNER] Failed to push param restore task for ref id=%u", id);
                    return;
                }

                params_ = new_params;
                param_count_ = new_count;
                owns_params_ = (new_params != nullptr);
                pushNodeTask(target);
                return;
            }

            case BulletMLNode::changeDirection: {
                BulletMLNode* term = findChild(node, BulletMLNode::term);
                BulletMLNode* dir_node = findChild(node, BulletMLNode::direction);
                if (!term || !dir_node) return;

                int t = static_cast<int>(evalNodeValue(term));
                if (t < 0) t = 0;
                const BulletMLNode::Type type = dir_node->getType();

                double d;
                if (type != BulletMLNode::type_sequence) d = getDirection(dir_node, false);
                else d = evalNodeValue(dir_node);
                calcChangeDirection(d, t, type == BulletMLNode::type_sequence);
                return;
            }

            case BulletMLNode::changeSpeed: {
                BulletMLNode* term = findChild(node, BulletMLNode::term);
                BulletMLNode* spd_node = findChild(node, BulletMLNode::speed);
                if (!term || !spd_node) return;

                int t = static_cast<int>(evalNodeValue(term));
                if (t < 0) t = 0;
                const BulletMLNode::Type type = spd_node->getType();

                double s;
                if (type != BulletMLNode::type_sequence) s = getSpeed(spd_node);
                else s = evalNodeValue(spd_node) * static_cast<double>(t) + runner_->getBulletSpeed();

                calcChangeSpeed(s, t);
                return;
            }

            case BulletMLNode::accel: {
                BulletMLNode* term = findChild(node, BulletMLNode::term);
                if (!term) return;
                int t = static_cast<int>(evalNodeValue(term));
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

    BulletMLNode** nodes_;
    uint16_t node_count_;

    Task* tasks_;
    uint16_t task_count_;
    uint16_t task_capacity_;

    double* params_;
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
    double spd_;
    double dir_;
    double prev_spd_;
    double prev_dir_;

    BulletMLRunnerImpl(const BulletMLRunnerImpl&);
    BulletMLRunnerImpl& operator=(const BulletMLRunnerImpl&);
};

inline BulletMLRunner::BulletMLRunner(BulletMLParserBLB* parser)
    : parser_(parser), state_(nullptr), impls_(nullptr), impl_count_(0) {
    if (!parser_) return;

    uint32_t top_count_u32 = 0;
    BulletMLNode** top_actions = parser_->getTopActions(&top_count_u32);
    if (!top_actions || top_count_u32 == 0) return;

    uint16_t top_count = (top_count_u32 > 65535U) ? 65535U : static_cast<uint16_t>(top_count_u32);
    impls_ = lwnew BulletMLRunnerImpl*[top_count];
    if (!impls_) return;

    for (uint16_t i = 0; i < top_count; ++i) impls_[i] = nullptr;

    for (uint16_t i = 0; i < top_count; ++i) {
        BulletMLNode** nodes = lwnew BulletMLNode*[1];
        if (!nodes) break;
        nodes[0] = top_actions[i];

        BulletMLState* st = lwnew BulletMLState(parser_, nodes, 1, nullptr, 0);
        if (!st) {
            delete[] nodes;
            break;
        }

        BulletMLRunnerImpl* impl = lwnew BulletMLRunnerImpl(st, this);
        if (!impl) break;

        impls_[impl_count_++] = impl;
    }
}

inline BulletMLRunner::BulletMLRunner(BulletMLState* state)
    : parser_(nullptr), state_(state), impls_(nullptr), impl_count_(0) {
    if (!state_) return;

    parser_ = state_->getParser();
    impls_ = lwnew BulletMLRunnerImpl*[1];
    if (!impls_) return;

    impls_[0] = lwnew BulletMLRunnerImpl(state_, this);
    if (!impls_[0]) {
        delete[] impls_;
        impls_ = nullptr;
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
    delete[] impls_;
    impls_ = nullptr;
    impl_count_ = 0;

    delete state_;
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
