#ifndef BULLETMLSTATE_HPP_
#define BULLETMLSTATE_HPP_

class BulletMLParserBLB;

#ifndef BULLETMLPARSER_ALIAS_DEFINED
#define BULLETMLPARSER_ALIAS_DEFINED
// Keep BulletMLParser as stable API name while concrete parser type is BLB.
typedef BulletMLParserBLB BulletMLParser;
#endif

/// BulletML State - Minimal state object for BulletML execution
class BulletMLState {
public:
    explicit BulletMLState(BulletMLParser* parser = nullptr)
        : parser_(parser) {}

    virtual ~BulletMLState() {}

    BulletMLParser* getParser() const { return parser_; }
    void setParser(BulletMLParser* parser) { parser_ = parser; }

private:
    BulletMLParser* parser_;
};

#endif // BULLETMLSTATE_HPP_
