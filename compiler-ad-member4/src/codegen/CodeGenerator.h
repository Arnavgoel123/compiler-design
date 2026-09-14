// CodeGenerator.h
// -----------------------------------------------------------------------------
// Translates a structured ir::IRFunction into plain, self-contained C++
// source code (no external AD/symbolic-math libraries; only <cmath>).
//
// Works on ANY ir::IRFunction - original IR or derivative IR coming from
// Forward AD / Reverse AD - since it only depends on the IR contract.
// -----------------------------------------------------------------------------
#pragma once

#include "../ir/IR.h"
#include <string>

namespace codegen {

using namespace std;

class CodeGenerator {
public:
    // Generates a complete, compilable C++ function definition as a string,
    // e.g.:
    //   double derivative(double x) {
    //       double t1 = x * x;
    //       double t2 = 3 * x;
    //       double t3 = t1 + t2;
    //       return t3;
    //   }
    static string generateFunction(const ir::IRFunction& func);

    // Wraps generateFunction() output with #include <cmath>, suitable for
    // writing directly to a .cpp/.h file.
    static string generateStandaloneSource(const ir::IRFunction& func);
};

} // namespace codegen
