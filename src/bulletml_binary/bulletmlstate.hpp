#ifndef BULLETMLSTATE_HPP_
#define BULLETMLSTATE_HPP_

class BulletMLParserBLB;

/// BulletML State - Minimal state object for BulletML execution
class BulletMLState {
public:
    explicit BulletMLState(BulletMLParserBLB* parser = nullptr)
        : parser_(parser) {}

    virtual ~BulletMLState() {}

    BulletMLParserBLB* getParser() const { return parser_; }
    void setParser(BulletMLParserBLB* parser) { parser_ = parser; }

private:
    BulletMLParserBLB* parser_;
};

#endif // BULLETMLSTATE_HPP_
