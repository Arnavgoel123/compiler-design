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

    // Same, for several functions in one translation unit (function names
    // must be unique). `banner` is emitted as a leading comment block.
    static string generateStandaloneSource(const vector<ir::IRFunction>& funcs,
                                           const string& banner = "");

    // Emits a Jacobian wrapper over already-generated derivative functions:
    //     void <wrapperName>(double x, double z, double J[M][N]);
    // where J[i][j] = d(outputNames[i]) / d(params[j]) and
    // derivativeFunctionNames[i][j] names the generated function computing it.
    // Every derivative function must take all `params`, in order.
    static string generateJacobianWrapper(const string& wrapperName,
                                          const vector<string>& params,
                                          const vector<string>& outputNames,
                                          const vector<vector<string>>& derivativeFunctionNames);
};

} // namespace codegen
