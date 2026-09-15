#include "ReverseAD.h"
#include <map>
#include <stdexcept>

using namespace std;
using namespace ir;

namespace reverse_ad {

IRFunction reverseDifferentiate(const IRFunction& original, const string& withRespectTo) {
    // We will build a new IRFunction that computes both the forward pass
    // (needed to get intermediate values) and the reverse backward sweep.
    IRBuilder builder("d_" + original.name + "_d_" + withRespectTo);

    // 1. Re-declare all original parameters.
    for (const string& p : original.params) {
        builder.addParam(p);
    }

    // 2. Forward Sweep: Emit the original instructions to get our primal values.
    // We must track the mapping from the original function's temp IDs to our new builder's Operands.
    map<int, Operand> primalTemps;

    auto mapPrimalOperand = [&](const Operand& op) -> Operand {
        if (op.isConstant()) return op;
        if (op.isVariable()) return op; // Variables are already added as params.
        if (op.isTemp()) return primalTemps.at(op.tempId);
        throw runtime_error("Unknown operand kind in forward sweep.");
    };

    for (const auto& inst : original.instructions) {
        vector<Operand> mappedOperands;
        for (const auto& op : inst.operands) {
            mappedOperands.push_back(mapPrimalOperand(op));
        }
        Operand newTemp = builder.emit(inst.op, mappedOperands);
        primalTemps[inst.resultId] = newTemp;
    }

    // 3. Initialize Adjoints (derivatives):
    // Adjoints accumulate the derivatives of the final output with respect to each node.
    map<int, Operand> adjointTemps;
    map<string, Operand> adjointVars;

    auto addAdjointVar = [&](const string& varName, const Operand& value) {
        if (adjointVars.find(varName) == adjointVars.end()) {
            adjointVars[varName] = value;
        } else {
            adjointVars[varName] = builder.add(adjointVars[varName], value);
        }
    };

    auto addAdjointTemp = [&](int tempId, const Operand& value) {
        if (adjointTemps.find(tempId) == adjointTemps.end()) {
            adjointTemps[tempId] = value;
        } else {
            adjointTemps[tempId] = builder.add(adjointTemps[tempId], value);
        }
    };

    auto addAdjoint = [&](const Operand& origOperand, const Operand& value) {
        if (origOperand.isConstant()) return; // Constants have 0 derivative
        if (origOperand.isVariable()) {
            addAdjointVar(origOperand.varName, value);
        } else if (origOperand.isTemp()) {
            addAdjointTemp(origOperand.tempId, value);
        }
    };

    auto getAdjoint = [&](const Operand& origOperand) -> Operand {
        if (origOperand.isConstant()) {
            return Operand::makeConstant(0.0);
        }
        if (origOperand.isVariable()) {
            auto it = adjointVars.find(origOperand.varName);
            if (it != adjointVars.end()) return it->second;
            return Operand::makeConstant(0.0);
        }
        if (origOperand.isTemp()) {
            auto it = adjointTemps.find(origOperand.tempId);
            if (it != adjointTemps.end()) return it->second;
            return Operand::makeConstant(0.0);
        }
        return Operand::makeConstant(0.0);
    };

    // Seed the output adjoint to 1.0 (dy/dy = 1)
    addAdjoint(original.returnValue, Operand::makeConstant(1.0));

    // 4. Backward Sweep: Traverse the original instructions in reverse order.
    for (auto it = original.instructions.rbegin(); it != original.instructions.rend(); ++it) {
        const Instruction& inst = *it;

        Operand t_bar = getAdjoint(Operand::makeTemp(inst.resultId));

        Operand orig_a = inst.operands[0];
        Operand a = mapPrimalOperand(orig_a);

        Operand orig_b;
        Operand b;
        if (inst.operands.size() > 1) {
            orig_b = inst.operands[1];
            b = mapPrimalOperand(orig_b);
        }

        // Apply chain rule for the specific operation.
        // t_i = OP(a, b)  ==>  a_bar += t_bar * d(OP)/da, b_bar += t_bar * d(OP)/db
        switch (inst.op) {
            case Op::ADD:
                addAdjoint(orig_a, t_bar);
                addAdjoint(orig_b, t_bar);
                break;
            case Op::SUB:
                addAdjoint(orig_a, t_bar);
                addAdjoint(orig_b, builder.neg(t_bar));
                break;
            case Op::MUL:
                addAdjoint(orig_a, builder.mul(t_bar, b));
                addAdjoint(orig_b, builder.mul(t_bar, a));
                break;
            case Op::DIV:
                // t = a / b
                // d(t)/da = 1/b
                // d(t)/db = -a / (b^2)
                addAdjoint(orig_a, builder.div(t_bar, b));
                addAdjoint(orig_b, builder.mul(builder.neg(t_bar), builder.div(a, builder.mul(b, b))));
                break;
            case Op::NEG:
                addAdjoint(orig_a, builder.neg(t_bar));
                break;
            case Op::SIN:
                // t = sin(a) -> d(t)/da = cos(a)
                addAdjoint(orig_a, builder.mul(t_bar, builder.cos_(a)));
                break;
            case Op::COS:
                // t = cos(a) -> d(t)/da = -sin(a)
                addAdjoint(orig_a, builder.mul(t_bar, builder.neg(builder.sin_(a))));
                break;
            case Op::EXP:
                // t = exp(a) -> d(t)/da = exp(a)
                // Since t_i = exp(a) was already computed in the forward pass, we can reuse it!
                addAdjoint(orig_a, builder.mul(t_bar, primalTemps[inst.resultId]));
                break;
            case Op::LOG:
                // t = log(a) -> d(t)/da = 1/a
                addAdjoint(orig_a, builder.div(t_bar, a));
                break;
            case Op::POW:
                // t = a^b
                // d(t)/da = b * a^(b-1)
                // d(t)/db = a^b * ln(a)
                {
                    Operand one = Operand::makeConstant(1.0);
                    Operand b_minus_1 = builder.sub(b, one);
                    Operand a_pow_b_minus_1 = builder.pow_(a, b_minus_1);
                    Operand da = builder.mul(b, a_pow_b_minus_1);
                    addAdjoint(orig_a, builder.mul(t_bar, da));

                    Operand a_pow_b = primalTemps[inst.resultId]; // reuse forward t_i = a^b
                    Operand ln_a = builder.log_(a);
                    Operand db = builder.mul(a_pow_b, ln_a);
                    addAdjoint(orig_b, builder.mul(t_bar, db));
                }
                break;
        }
    }

    // 5. Finalize the derivative.
    Operand finalDerivative = getAdjoint(Operand::makeVariable(withRespectTo));
    builder.setReturn(finalDerivative);

    return builder.build();
}

} // namespace reverse_ad
