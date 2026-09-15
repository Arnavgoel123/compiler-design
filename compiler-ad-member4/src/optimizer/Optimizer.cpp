// Optimizer.cpp
#include "Optimizer.h"
#include <cmath>
#include <map>
#if __has_include(<optional>)
#include <optional>
#elif __has_include(<experimental/optional>)
#include <experimental/optional>
namespace std {
    using experimental::optional;
    using experimental::nullopt;
}
#endif
#include <sstream>

namespace optimizer {

using namespace std;
using namespace ir;

string OptimizationStats::toString() const {
    ostringstream oss;
    oss << "Optimization stats:\n"
        << "  instructions before : " << instructionsBefore << "\n"
        << "  instructions after  : " << instructionsAfter << "\n"
        << "  constants folded    : " << constantsFolded << "\n"
        << "  algebraic simplified: " << algebraicSimplifications << "\n"
        << "  CSE eliminations    : " << commonSubexpressionsEliminated;
    return oss.str();
}

namespace {

// Resolves an operand through the substitution map. Operands produced by
// earlier steps in this pass are already fully resolved (they never point
// at an old, pre-optimization temp id), so a single lookup suffices.
Operand resolve(const Operand& operand, const map<int, Operand>& subst) {
    if (operand.kind != OperandKind::Temp) return operand;
    auto it = subst.find(operand.tempId);
    if (it == subst.end()) {
        // Should not happen for a well-formed SSA-like IR, but fall back to
        // the original operand rather than crashing.
        return operand;
    }
    return it->second;
}

// Attempts to evaluate `op` over already-constant operands. Returns nullopt
// if folding is not safe (e.g. division by zero), in which case the
// instruction is emitted normally instead.
optional<double> tryFoldConstant(Op op, const vector<Operand>& resolved) {
    for (const auto& o : resolved) {
        if (!o.isConstant()) return nullopt;
    }
    switch (op) {
        case Op::ADD: return resolved[0].constValue + resolved[1].constValue;
        case Op::SUB: return resolved[0].constValue - resolved[1].constValue;
        case Op::MUL: return resolved[0].constValue * resolved[1].constValue;
        case Op::DIV:
            if (resolved[1].constValue == 0.0) return nullopt; // avoid div-by-zero at compile time
            return resolved[0].constValue / resolved[1].constValue;
        case Op::NEG: return -resolved[0].constValue;
        case Op::SIN: return sin(resolved[0].constValue);
        case Op::COS: return cos(resolved[0].constValue);
        case Op::EXP: return exp(resolved[0].constValue);
        case Op::LOG:
            if (resolved[0].constValue <= 0.0) return nullopt; // avoid domain error at compile time
            return log(resolved[0].constValue);
        case Op::POW: return pow(resolved[0].constValue, resolved[1].constValue);
    }
    return nullopt;
}

bool isConstZero(const Operand& o) { return o.isConstant() && o.constValue == 0.0; }
bool isConstOne(const Operand& o)  { return o.isConstant() && o.constValue == 1.0; }

// Algebraic identities. Returns the simplified operand if a rule applies.
optional<Operand> tryAlgebraicSimplify(Op op, const vector<Operand>& resolved) {
    switch (op) {
        case Op::ADD:
            if (isConstZero(resolved[0])) return resolved[1];
            if (isConstZero(resolved[1])) return resolved[0];
            break;
        case Op::SUB:
            if (isConstZero(resolved[1])) return resolved[0];
            break;
        case Op::MUL:
            if (isConstZero(resolved[0]) || isConstZero(resolved[1])) return Operand::makeConstant(0.0);
            if (isConstOne(resolved[0])) return resolved[1];
            if (isConstOne(resolved[1])) return resolved[0];
            break;
        case Op::DIV:
            if (isConstOne(resolved[1])) return resolved[0];
            break;
        default:
            break;
    }
    return nullopt;
}

// Key used to detect common subexpressions: same operation over the same
// (already-resolved) operands.
struct CseEntry {
    Op op;
    vector<Operand> operands;
    Operand resultOperand;
};

bool sameKey(Op op, const vector<Operand>& operands, const CseEntry& entry) {
    if (entry.op != op) return false;
    if (entry.operands.size() != operands.size()) return false;
    for (size_t i = 0; i < operands.size(); ++i) {
        if (!(entry.operands[i] == operands[i])) return false;
    }
    return true;
}

} // namespace

ir::IRFunction Optimizer::optimize(const ir::IRFunction& input, OptimizationStats* statsOut) {
    OptimizationStats stats;
    stats.instructionsBefore = static_cast<int>(input.instructions.size());

    IRFunction output;
    output.name = input.name;
    output.params = input.params;

    map<int, Operand> subst;   // old tempId -> resolved (final) Operand
    vector<CseEntry> cseTable; // linear scan; program sizes here are small

    int nextId = 1;

    for (const auto& instr : input.instructions) {
        vector<Operand> resolved;
        resolved.reserve(instr.operands.size());
        for (const auto& o : instr.operands) {
            resolved.push_back(resolve(o, subst));
        }

        // 1. Constant folding.
        if (auto folded = tryFoldConstant(instr.op, resolved)) {
            subst[instr.resultId] = Operand::makeConstant(*folded);
            stats.constantsFolded++;
            continue;
        }

        // 2. Algebraic simplification.
        if (auto simplified = tryAlgebraicSimplify(instr.op, resolved)) {
            subst[instr.resultId] = *simplified;
            stats.algebraicSimplifications++;
            continue;
        }

        // 3. Common subexpression elimination.
        bool reused = false;
        for (const auto& entry : cseTable) {
            if (sameKey(instr.op, resolved, entry)) {
                subst[instr.resultId] = entry.resultOperand;
                stats.commonSubexpressionsEliminated++;
                reused = true;
                break;
            }
        }
        if (reused) continue;

        // 4. Otherwise, emit the instruction as-is (with resolved operands).
        Instruction newInstr;
        newInstr.resultId = nextId++;
        newInstr.op = instr.op;
        newInstr.operands = resolved;
        output.instructions.push_back(newInstr);

        Operand resultOperand = Operand::makeTemp(newInstr.resultId);
        subst[instr.resultId] = resultOperand;
        cseTable.push_back(CseEntry{instr.op, resolved, resultOperand});
    }

    output.returnValue = resolve(input.returnValue, subst);
    stats.instructionsAfter = static_cast<int>(output.instructions.size());

    if (statsOut) *statsOut = stats;
    return output;
}

} // namespace optimizer
