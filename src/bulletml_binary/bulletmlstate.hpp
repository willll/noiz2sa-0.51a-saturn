#ifndef BULLETMLSTATE_HPP_
#define BULLETMLSTATE_HPP_

#include <cstdint>
#include <srl.hpp>
#include <srl_log.hpp>

#include "../bulletml_runtime_factory.h"

using SRL::Math::Types::Fxp;

class BulletMLParserBLB;
class BulletMLNode;

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
          parameters_(nullptr),
          parameter_count_(0) {
        if (parameter_count > 0 && parameters) {
            static uint32_t sParameterCopyLogs = 0;
            if (sParameterCopyLogs < 12 || (sParameterCopyLogs % 64) == 0) {
                SRL::Logger::LogInfo("[BML-STATE] param copy count=%u node_count=%u",
                                     static_cast<unsigned>(parameter_count),
                                     static_cast<unsigned>(node_count));
            }
            ++sParameterCopyLogs;
            parameters_ = createBulletMlRuntimeArray<Fxp>(parameter_count);
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
          parameters_(nullptr), parameter_count_(0) {}

    ~BulletMLState() {
        delete[] nodes_;
        nodes_ = nullptr;
        node_count_ = 0;
        delete[] parameters_;
        parameters_ = nullptr;
        parameter_count_ = 0;
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
    Fxp* parameters_;
    uint16_t parameter_count_;

    BulletMLState(const BulletMLState&);
    BulletMLState& operator=(const BulletMLState&);
};

#endif // BULLETMLSTATE_HPP_
