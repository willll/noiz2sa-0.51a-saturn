#ifndef BULLETMLRUNNER_HPP_
#define BULLETMLRUNNER_HPP_

// Forward declarations for lightweight runner interface.
class BulletMLNode;

#include "bulletmlstate.hpp"

/// BulletML Runner - Base class for BulletML execution
class BulletMLRunner {
public:
    /// Constructor from parser
    explicit BulletMLRunner(BulletMLParserBLB* parser)
        : parser_(parser), state_(nullptr) {}

    /// Constructor from state
    explicit BulletMLRunner(BulletMLState* state)
        : parser_(nullptr), state_(state) {}

    /// Virtual destructor
    virtual ~BulletMLRunner() {}

    /// Get bullet direction (degrees)
    virtual double getBulletDirection() { return 0.0; }

    /// Get aim direction (degrees)
    virtual double getAimDirection() { return 0.0; }

    /// Get bullet speed
    virtual double getBulletSpeed() { return 0.0; }

    /// Get default speed
    virtual double getDefaultSpeed() { return 1.0; }

    /// Get difficulty rank
    virtual double getRank() { return 0.0; }

    /// Create a simple bullet
    virtual void createSimpleBullet(double direction, double speed) { (void)direction; (void)speed; }

    /// Create a bullet
    virtual void createBullet(BulletMLNode* node, double direction, double speed) { (void)node; (void)direction; (void)speed; }

    /// Add a command
    virtual int addCommand(BulletMLNode* node) { (void)node; return 0; }

    /// Execute commands
    virtual void execute() {}

    /// Check if execution is finished
    virtual bool isEnd() { return true; }

    /// Run one execution cycle
    virtual void run() {}

    /// Get the parser
    BulletMLParserBLB* getParser() const { return parser_; }

    /// Get the state
    BulletMLState* getState() const { return state_; }

protected:
    BulletMLParserBLB* parser_;
    BulletMLState* state_;
};

#endif // BULLETMLRUNNER_HPP_
