// Validator.cpp
#include "Validator.h"
#include <sstream>
#include <set>
#include <cmath>

namespace validation {

using namespace std;
using namespace ir;

// ---------------------------------------------------------------------------
// IRValidationResult
// ---------------------------------------------------------------------------
string IRValidationResult::toString() const {
    ostringstream oss;
    if (valid) {
        oss << "IR VALID";
    } else {
        oss << "IR INVALID (" << errors.size() << " error(s)):\n";
        for (size_t i = 0; i < errors.size(); ++i) {
            oss << "  [" << (i + 1) << "] " << errors[i] << "\n";
        }
    }
    return oss.str();
}

// ---------------------------------------------------------------------------
// IRValidator
// ---------------------------------------------------------------------------
IRValidationResult IRValidator::validate(const ir::IRFunction& func) {
    IRValidationResult result;

    if (func.name.empty()) {
        result.addError("Function name is empty.");
    }

    // Duplicate parameter names.
    {
        set<string> seenParams;
        for (const auto& p : func.params) {
            if (p.empty()) {
                result.addError("Function has an empty parameter name.");
                continue;
            }
            if (seenParams.count(p)) {
                result.addError("Duplicate parameter name: '" + p + "'.");
            }
            seenParams.insert(p);
        }
    }

    set<int> definedTemps;      // temps defined so far (for forward-reference checks)
    set<int> allDefinedTemps;   // temps defined anywhere (for duplicate-definition checks)

    for (size_t i = 0; i < func.instructions.size(); ++i) {
        const Instruction& instr = func.instructions[i];
        string where = "instruction #" + to_string(i + 1) + " (t" + to_string(instr.resultId) + ")";

        if (instr.resultId <= 0) {
            result.addError(where + ": invalid (non-positive) result temp id.");
        } else if (allDefinedTemps.count(instr.resultId)) {
            result.addError(where + ": duplicate definition of temp t" + to_string(instr.resultId) + ".");
        } else {
            allDefinedTemps.insert(instr.resultId);
        }

        int expectedArity = opArity(instr.op);
        if (expectedArity < 0) {
            result.addError(where + ": unrecognized/invalid operation.");
        } else if (static_cast<int>(instr.operands.size()) != expectedArity) {
            result.addError(where + ": " + opName(instr.op) + " expects " +
                             to_string(expectedArity) + " operand(s) but got " +
                             to_string(instr.operands.size()) + ".");
        }

        for (const auto& operand : instr.operands) {
            switch (operand.kind) {
                case OperandKind::Constant:
                    if (std::isnan(operand.constValue) || std::isinf(operand.constValue)) {
                        result.addError(where + ": constant operand is NaN/Inf.");
                    }
                    break;
                case OperandKind::Variable:
                    if (!func.hasParam(operand.varName)) {
                        result.addError(where + ": undefined variable operand '" +
                                         operand.varName + "' (not a function parameter).");
                    }
                    break;
                case OperandKind::Temp:
                    if (!definedTemps.count(operand.tempId)) {
                        result.addError(where + ": operand references temp t" +
                                         to_string(operand.tempId) +
                                         " which is undefined at this point (forward reference or never defined).");
                    }
                    break;
            }
        }

        // This instruction's result becomes available to subsequent instructions.
        if (instr.resultId > 0) {
            definedTemps.insert(instr.resultId);
        }
    }

    // Validate RETURN.
    switch (func.returnValue.kind) {
        case OperandKind::Constant:
            if (std::isnan(func.returnValue.constValue) || std::isinf(func.returnValue.constValue)) {
                result.addError("RETURN: constant return value is NaN/Inf.");
            }
            break;
        case OperandKind::Variable:
            if (!func.hasParam(func.returnValue.varName)) {
                result.addError("RETURN: undefined variable '" + func.returnValue.varName + "'.");
            }
            break;
        case OperandKind::Temp:
            if (!definedTemps.count(func.returnValue.tempId)) {
                result.addError("RETURN: references temp t" + to_string(func.returnValue.tempId) +
                                 " which is never defined.");
            }
            break;
    }

    if (func.instructions.empty() && func.returnValue.kind != OperandKind::Constant &&
        func.returnValue.kind != OperandKind::Variable) {
        result.addError("Function has no instructions but RETURN references a temp.");
    }

    return result;
}

// ---------------------------------------------------------------------------
// NumericalCheckRow / NumericalValidationReport
// ---------------------------------------------------------------------------
string NumericalCheckRow::toString() const {
    ostringstream oss;
    oss << "x = " << x << " | numerical = " << numericalDerivative;
    if (forwardDerivative) {
        oss << " | forward = " << *forwardDerivative
            << " | fwd_err = " << *forwardError
            << " | tol = " << tolerance
            << " | " << (forwardPass ? "PASS" : "FAIL");
    }
    if (reverseDerivative) {
        oss << " | reverse = " << *reverseDerivative
            << " | rev_err = " << *reverseError
            << " | tol = " << tolerance
            << " | " << (reversePass ? "PASS" : "FAIL");
    }
    return oss.str();
}

string NumericalValidationReport::toString() const {
    ostringstream oss;
    oss << "===== NUMERICAL VALIDATION REPORT =====\n";
    for (const auto& row : rows) {
        oss << row.toString() << "\n";
    }
    oss << "OVERALL: " << (allPassed ? "PASS" : "FAIL") << "\n";
    oss << "========================================";
    return oss.str();
}

// ---------------------------------------------------------------------------
// NumericalValidator
// ---------------------------------------------------------------------------
double NumericalValidator::centralDifference(const ScalarFunction& f, double x, double h) {
    return (f(x + h) - f(x - h)) / (2.0 * h);
}

NumericalValidationReport NumericalValidator::validate(
    const ScalarFunction& f,
    optional<ScalarFunction> forwardDeriv,
    optional<ScalarFunction> reverseDeriv,
    const vector<double>& xs,
    double h,
    double tolerance) {

    NumericalValidationReport report;

    for (double x : xs) {
        NumericalCheckRow row;
        row.x = x;
        row.tolerance = tolerance;
        row.numericalDerivative = centralDifference(f, x, h);

        if (forwardDeriv) {
            double fd = (*forwardDeriv)(x);
            row.forwardDerivative = fd;
            double err = fabs(fd - row.numericalDerivative);
            row.forwardError = err;
            row.forwardPass = err <= tolerance;
            if (!row.forwardPass) report.allPassed = false;
        }

        if (reverseDeriv) {
            double rd = (*reverseDeriv)(x);
            row.reverseDerivative = rd;
            double err = fabs(rd - row.numericalDerivative);
            row.reverseError = err;
            row.reversePass = err <= tolerance;
            if (!row.reversePass) report.allPassed = false;
        }

        report.rows.push_back(row);
    }

    return report;
}

} // namespace validation
