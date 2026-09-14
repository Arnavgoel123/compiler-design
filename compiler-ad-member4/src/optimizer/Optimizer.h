// Optimizer.h
// -----------------------------------------------------------------------------
// IR-level optimization pipeline:
//   1. Constant folding          (3 + 5 -> 8)
//   2. Algebraic simplification  (x + 0 -> x, x * 1 -> x, x * 0 -> 0, ...)
//   3. Common Subexpression Elimination (CSE)
//
// All three passes run together in a single forward pass over the
// instruction list (the IR is SSA-like: each instruction defines exactly
// one new temp, and only references already-defined values), which keeps
// the optimizer simple while still being correct.
//
// The optimizer operates purely on ir::IRFunction, so it works equally on
// the original IR (from ASTToIR) and on derivative IR (from Forward/Reverse
// AD) without any changes.
// -----------------------------------------------------------------------------
#pragma once

#include "../ir/IR.h"
#include <string>

namespace optimizer {

using namespace std;

struct OptimizationStats {
    int instructionsBefore = 0;
    int instructionsAfter = 0;
    int constantsFolded = 0;
    int algebraicSimplifications = 0;
    int commonSubexpressionsEliminated = 0;

    string toString() const;
};

class Optimizer {
public:
    // Runs the full pipeline (constant folding + algebraic simplification +
    // CSE) and returns the optimized function. Optionally reports stats.
    static ir::IRFunction optimize(const ir::IRFunction& input,
                                    OptimizationStats* statsOut = nullptr);
};

} // namespace optimizer
