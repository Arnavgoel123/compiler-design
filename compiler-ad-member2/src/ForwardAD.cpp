#include "ForwardAD.h"

#include <map>
#include <stdexcept>

namespace forward_ad {

namespace {

struct Dual {
    ir::Operand primal;
    ir::Operand tangent;
};

ir::Operand tangentOf(const ir::Operand& operand,
                      const std::map<std::string, Dual>& variables,
                      const std::map<int, Dual>& temporaries) {
    if (operand.isConstant()) {
        return ir::Operand::makeConstant(0.0);
    }
    if (operand.isVariable()) {
        auto it = variables.find(operand.varName);
        if (it == variables.end()) {
            throw std::runtime_error("Forward AD encountered an unknown variable: " +
                                     operand.varName);
        }
        return it->second.tangent;
    }

    auto it = temporaries.find(operand.tempId);
    if (it == temporaries.end()) {
        throw std::runtime_error("Forward AD encountered an unknown temporary");
    }
    return it->second.tangent;
}

ir::Operand primalOf(const ir::Operand& operand,
                     const std::map<int, Dual>& temporaries) {
    if (!operand.isTemp()) {
        return operand;
    }

    auto it = temporaries.find(operand.tempId);
    if (it == temporaries.end()) {
        throw std::runtime_error("Forward AD encountered an unknown temporary");
    }
    return it->second.primal;
}

} // namespace

ir::IRFunction forwardDifferentiate(const ir::IRFunction& original,
                                    const std::string& withRespectTo) {
    if (!original.hasParam(withRespectTo)) {
        throw std::invalid_argument("Forward AD variable is not a function parameter: " +
                                    withRespectTo);
    }

    ir::IRBuilder builder("d_" + original.name + "_d_" + withRespectTo);
    std::map<std::string, Dual> variables;
    std::map<int, Dual> temporaries;

    for (const auto& parameter : original.params) {
        builder.addParam(parameter);
        variables.emplace(parameter, Dual{
            ir::Operand::makeVariable(parameter),
            ir::Operand::makeConstant(parameter == withRespectTo ? 1.0 : 0.0)
        });
    }

    for (const auto& instruction : original.instructions) {
        std::vector<ir::Operand> primalOperands;
        primalOperands.reserve(instruction.operands.size());
        for (const auto& operand : instruction.operands) {
            primalOperands.push_back(primalOf(operand, temporaries));
        }

        ir::Operand primal = builder.emit(instruction.op, primalOperands);
        ir::Operand tangent;
        ir::Operand da = tangentOf(instruction.operands[0], variables, temporaries);
        ir::Operand a = primalOperands[0];

        switch (instruction.op) {
            case ir::Op::ADD:
                tangent = builder.add(da, tangentOf(instruction.operands[1], variables, temporaries));
                break;
            case ir::Op::SUB:
                tangent = builder.sub(da, tangentOf(instruction.operands[1], variables, temporaries));
                break;
            case ir::Op::MUL: {
                ir::Operand b = primalOperands[1];
                ir::Operand db = tangentOf(instruction.operands[1], variables, temporaries);
                tangent = builder.add(builder.mul(da, b), builder.mul(a, db));
                break;
            }
            case ir::Op::DIV: {
                ir::Operand b = primalOperands[1];
                ir::Operand db = tangentOf(instruction.operands[1], variables, temporaries);
                ir::Operand numerator = builder.sub(builder.mul(da, b), builder.mul(a, db));
                tangent = builder.div(numerator, builder.mul(b, b));
                break;
            }
            case ir::Op::NEG:
                tangent = builder.neg(da);
                break;
            case ir::Op::SIN:
                tangent = builder.mul(builder.cos_(a), da);
                break;
            case ir::Op::COS:
                tangent = builder.mul(builder.neg(builder.sin_(a)), da);
                break;
            case ir::Op::EXP:
                tangent = builder.mul(primal, da);
                break;
            case ir::Op::LOG:
                tangent = builder.div(da, a);
                break;
            case ir::Op::POW: {
                ir::Operand b = primalOperands[1];
                ir::Operand db = tangentOf(instruction.operands[1], variables, temporaries);
                ir::Operand logarithmicTerm = builder.mul(db, builder.log_(a));
                ir::Operand exponentTerm = builder.mul(b, builder.div(da, a));
                tangent = builder.mul(primal, builder.add(logarithmicTerm, exponentTerm));
                break;
            }
        }

        temporaries.emplace(instruction.resultId, Dual{primal, tangent});
    }

    builder.setReturn(tangentOf(original.returnValue, variables, temporaries));
    return builder.build();
}

} // namespace forward_ad

namespace ad {

ir::IRFunction ForwardAD::differentiate(const ir::IRFunction& original,
                                         const std::string& withRespectTo) const {
    return forward_ad::forwardDifferentiate(original, withRespectTo);
}

} // namespace ad
