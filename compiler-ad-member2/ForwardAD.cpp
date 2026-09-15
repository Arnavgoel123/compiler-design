#include "ForwardAD.h"
#include <iostream>
#include <stdexcept>

namespace ad {

std::string ForwardAD::getTangentName(const std::string& varName) {
    if (tangentMap.find(varName) == tangentMap.end()) {
        tangentMap[varName] = "d_" + varName;
    }
    return tangentMap[varName];
}

std::string ForwardAD::resolveOperandTangent(const ir::Operand& op, ir::IRFunction& newFunc, double seedValue) {
    if (op.isConstant()) {
        std::string constTan = "d_const_" + std::to_string(op.getConstantValue());
        // Derivative of a constant is always 0.0
        newFunc.addInstruction(ir::Instruction(ir::Opcode::ASSIGN, constTan, 0.0));
        return constTan;
    }
    return getTangentName(op.getVariable());
}

ir::IRFunction ForwardAD::differentiate(const ir::IRFunction& original, const std::string& wrtVariable) {
    ir::IRFunction forwardIR;
    forwardIR.setName(original.getName() + "_forward_ad");

    tangentMap.clear();

    // Step 1: Initialize Parameters & Seed Tangents
    for (const auto& param : original.getParameters()) {
        forwardIR.addParameter(param);
        std::string tanParam = getTangentName(param);
        forwardIR.addParameter(tanParam);

        // Seed tangent: 1.0 for target variable, 0.0 for others
        double seed = (param == wrtVariable) ? 1.0 : 0.0;
        forwardIR.addInstruction(ir::Instruction(ir::Opcode::ASSIGN, tanParam, seed));
    }

    // Step 2: Tangent & Dual Propagation over IR Instructions
    for (const auto& instr : original.getInstructions()) {
        // Emit original primal instruction first
        forwardIR.addInstruction(instr);

        std::string dest = instr.getDestination();
        std::string tanDest = getTangentName(dest);

        switch (instr.getOpcode()) {
            case ir::Opcode::ASSIGN: {
                auto srcOp = instr.getOperand1();
                if (srcOp.isConstant()) {
                    forwardIR.addInstruction(ir::Instruction(ir::Opcode::ASSIGN, tanDest, 0.0));
                } else {
                    std::string tanSrc = getTangentName(srcOp.getVariable());
                    forwardIR.addInstruction(ir::Instruction(ir::Opcode::ASSIGN, tanDest, tanSrc));
                }
                break;
            }

            case ir::Opcode::ADD: {
                std::string tanOp1 = resolveOperandTangent(instr.getOperand1(), forwardIR, 0.0);
                std::string tanOp2 = resolveOperandTangent(instr.getOperand2(), forwardIR, 0.0);
                // Rule: d(u + v) = du + dv
                forwardIR.addInstruction(ir::Instruction(ir::Opcode::ADD, tanDest, tanOp1, tanOp2));
                break;
            }

            case ir::Opcode::SUB: {
                std::string tanOp1 = resolveOperandTangent(instr.getOperand1(), forwardIR, 0.0);
                std::string tanOp2 = resolveOperandTangent(instr.getOperand2(), forwardIR, 0.0);
                // Rule: d(u - v) = du - dv
                forwardIR.addInstruction(ir::Instruction(ir::Opcode::SUB, tanDest, tanOp1, tanOp2));
                break;
            }

            case ir::Opcode::MUL: {
                std::string u = instr.getOperand1().getVariable();
                std::string v = instr.getOperand2().getVariable();
                std::string du = resolveOperandTangent(instr.getOperand1(), forwardIR, 0.0);
                std::string dv = resolveOperandTangent(instr.getOperand2(), forwardIR, 0.0);

                // Product Rule: d(u * v) = du * v + u * dv
                std::string t1 = "tmp_mul_1_" + dest;
                std::string t2 = "tmp_mul_2_" + dest;

                forwardIR.addInstruction(ir::Instruction(ir::Opcode::MUL, t1, du, v));
                forwardIR.addInstruction(ir::Instruction(ir::Opcode::MUL, t2, u, dv));
                forwardIR.addInstruction(ir::Instruction(ir::Opcode::ADD, tanDest, t1, t2));
                break;
            }

            case ir::Opcode::SIN: {
                std::string u = instr.getOperand1().getVariable();
                std::string du = resolveOperandTangent(instr.getOperand1(), forwardIR, 0.0);

                // Chain Rule: d(sin(u)) = cos(u) * du
                std::string cosVal = "tmp_cos_" + dest;
                forwardIR.addInstruction(ir::Instruction(ir::Opcode::COS, cosVal, u));
                forwardIR.addInstruction(ir::Instruction(ir::Opcode::MUL, tanDest, cosVal, du));
                break;
            }

            case ir::Opcode::COS: {
                std::string u = instr.getOperand1().getVariable();
                std::string du = resolveOperandTangent(instr.getOperand1(), forwardIR, 0.0);

                // Chain Rule: d(cos(u)) = -sin(u) * du
                std::string sinVal = "tmp_sin_" + dest;
                std::string negSin = "tmp_neg_sin_" + dest;
                forwardIR.addInstruction(ir::Instruction(ir::Opcode::SIN, sinVal, u));
                forwardIR.addInstruction(ir::Instruction(ir::Opcode::MUL, negSin, -1.0, sinVal));
                forwardIR.addInstruction(ir::Instruction(ir::Opcode::MUL, tanDest, negSin, du));
                break;
            }

            case ir::Opcode::RETURN: {
                std::string retVal = instr.getOperand1().getVariable();
                std::string retTan = getTangentName(retVal);
                forwardIR.addInstruction(ir::Instruction(ir::Opcode::RETURN, retVal, retTan));
                break;
            }

            default:
                break;
        }
    }

    return forwardIR;
}

} // namespace ad
feat: add ForwardAD.cpp
