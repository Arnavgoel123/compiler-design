// Validator.h
// -----------------------------------------------------------------------------
// Two distinct kinds of validation, both owned by Member 4:
//
//   1. IRValidator        - structural validation of an ir::IRFunction
//                            (undefined operands, bad arity, duplicate
//                            definitions, malformed return, etc.)
//
//   2. NumericalValidator - runtime finite-difference validation, comparing
//                            Forward AD / Reverse AD derivatives against a
//                            numerical estimate f'(x) ~= (f(x+h)-f(x-h))/2h.
// -----------------------------------------------------------------------------
#pragma once

#include "../ir/IR.h"
#include <string>
#include <vector>
#include <functional>
#include <optional>

namespace validation {

using namespace std;

// ---------------------------------------------------------------------------
// IR structural validation
// ---------------------------------------------------------------------------
struct IRValidationResult {
    bool valid = true;
    vector<string> errors;

    void addError(const string& msg) {
        valid = false;
        errors.push_back(msg);
    }

    string toString() const;
};

class IRValidator {
public:
    static IRValidationResult validate(const ir::IRFunction& func);
};

// ---------------------------------------------------------------------------
// Numerical (finite-difference) validation
// ---------------------------------------------------------------------------
using ScalarFunction = function<double(double)>;

struct NumericalCheckRow {
    double x = 0.0;
    double numericalDerivative = 0.0;
    optional<double> forwardDerivative;
    optional<double> reverseDerivative;
    optional<double> forwardError;
    optional<double> reverseError;
    double tolerance = 0.0;
    bool forwardPass = true;
    bool reversePass = true;

    string toString() const;
};

struct NumericalValidationReport {
    vector<NumericalCheckRow> rows;
    bool allPassed = true;

    string toString() const;
};

class NumericalValidator {
public:
    // f               : the original scalar function being differentiated.
    // forwardDeriv    : optional callable implementing Forward AD's result
    //                   (nullopt if Member 2's module isn't wired in yet).
    // reverseDeriv    : optional callable implementing Reverse AD's result
    //                   (nullopt if Member 3's module isn't wired in yet).
    // xs              : sample input points to check.
    // h               : finite-difference step size.
    // tolerance       : max allowed |analytic - numerical| to PASS.
    static NumericalValidationReport validate(
        const ScalarFunction& f,
        optional<ScalarFunction> forwardDeriv,
        optional<ScalarFunction> reverseDeriv,
        const vector<double>& xs,
        double h = 1e-5,
        double tolerance = 1e-4);

    // Central-difference numerical derivative estimate at a single point.
    static double centralDifference(const ScalarFunction& f, double x, double h = 1e-5);
};

} // namespace validation
