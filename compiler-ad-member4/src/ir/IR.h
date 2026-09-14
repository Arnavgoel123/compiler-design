// IR.h
// -----------------------------------------------------------------------------
// Structured, strongly-typed Intermediate Representation (IR) for the
// Compiler with Automatic Differentiation project.
//
// Owner: Member 4 (IR / Optimizer / CodeGen / Validation / Integration)
//
// Design goals:
//   * The IR is NOT strings. It is made of real structs/classes.
//   * The IR is a stable, shared contract between:
//         - Member 1's AST  (via ASTToIR.h/.cpp)
//         - Member 2's Forward AD
//         - Member 3's Reverse AD
//         - This module's Optimizer / CodeGenerator / Validator
//   * Instructions are in a simple SSA-like form: every instruction defines
//     exactly one new temporary, and may only reference operands that were
//     already defined earlier in the instruction list (or are variables /
//     constants). This makes validation, optimization and code generation
//     straightforward and keeps the IR easy for the AD members to traverse.
// -----------------------------------------------------------------------------
#pragma once

#include <string>
#include <vector>
#include <map>
#include <stdexcept>

namespace ir {

using namespace std;

// -----------------------------------------------------------------------------
// Supported operations.
// -----------------------------------------------------------------------------
enum class Op {
    ADD,   // binary
    SUB,   // binary
    MUL,   // binary
    DIV,   // binary
    NEG,   // unary
    SIN,   // unary
    COS,   // unary
    EXP,   // unary
    LOG,   // unary
    POW    // binary  (base, exponent)
};

// Returns how many operands the given operation requires (1 or 2).
int opArity(Op op);

// Human readable operation name, e.g. Op::ADD -> "ADD".
string opName(Op op);

// -----------------------------------------------------------------------------
// Operand: a value used by an instruction. It is exactly one of:
//   - a compile time Constant (double)
//   - a named Variable (an input to the IR function, e.g. "x")
//   - a Temp, i.e. the result of a previous instruction ("t3")
// -----------------------------------------------------------------------------
enum class OperandKind { Constant, Variable, Temp };

struct Operand {
    OperandKind kind = OperandKind::Constant;
    double constValue = 0.0;
    string varName;      // valid when kind == Variable
    int tempId = -1;     // valid when kind == Temp

    static Operand makeConstant(double value);
    static Operand makeVariable(const string& name);
    static Operand makeTemp(int id);

    bool isConstant() const { return kind == OperandKind::Constant; }
    bool isVariable() const { return kind == OperandKind::Variable; }
    bool isTemp() const { return kind == OperandKind::Temp; }

    string toString() const;

    bool operator==(const Operand& other) const;
    bool operator!=(const Operand& other) const { return !(*this == other); }
};

// -----------------------------------------------------------------------------
// Instruction: "t<resultId> = OP operand1 [operand2]"
// -----------------------------------------------------------------------------
struct Instruction {
    int resultId = -1;          // this instruction defines temp t<resultId>
    Op op = Op::ADD;
    vector<Operand> operands;   // size 1 for unary ops, 2 for binary ops

    string toString() const;
};

// -----------------------------------------------------------------------------
// IRFunction: a single-output function over named input variables (params).
// This is the unit that flows through: AST->IR, Forward/Reverse AD,
// Optimizer, CodeGenerator, Validator.
// -----------------------------------------------------------------------------
struct IRFunction {
    string name = "f";
    vector<string> params;             // input variable names, in order
    vector<Instruction> instructions;  // body, in evaluation order (SSA-like)
    Operand returnValue = Operand::makeConstant(0.0);

    string toString() const;

    // Returns true if `name` is one of this function's parameters.
    bool hasParam(const string& name) const;

    // Returns the next unused temp id (1 + max existing resultId, or 1).
    int nextTempId() const;
};

// -----------------------------------------------------------------------------
// IRBuilder: convenience helper for constructing an IRFunction instruction
// by instruction. Used by ASTToIR, and available to Forward/Reverse AD
// implementers (Members 2 & 3) so they can build derivative IR without
// hand-managing temp ids.
// -----------------------------------------------------------------------------
class IRBuilder {
public:
    explicit IRBuilder(string functionName = "f");

    void addParam(const string& name);

    // Emits a new instruction computing `op` over `operands` and returns
    // an Operand referring to its result temp.
    Operand emit(Op op, const vector<Operand>& operands);

    // Convenience wrappers.
    Operand add(const Operand& a, const Operand& b) { return emit(Op::ADD, {a, b}); }
    Operand sub(const Operand& a, const Operand& b) { return emit(Op::SUB, {a, b}); }
    Operand mul(const Operand& a, const Operand& b) { return emit(Op::MUL, {a, b}); }
    Operand div(const Operand& a, const Operand& b) { return emit(Op::DIV, {a, b}); }
    Operand neg(const Operand& a) { return emit(Op::NEG, {a}); }
    Operand sin_(const Operand& a) { return emit(Op::SIN, {a}); }
    Operand cos_(const Operand& a) { return emit(Op::COS, {a}); }
    Operand exp_(const Operand& a) { return emit(Op::EXP, {a}); }
    Operand log_(const Operand& a) { return emit(Op::LOG, {a}); }
    Operand pow_(const Operand& a, const Operand& b) { return emit(Op::POW, {a, b}); }

    void setReturn(const Operand& value) { func_.returnValue = value; }

    const IRFunction& peek() const { return func_; }
    IRFunction build() const { return func_; }

private:
    IRFunction func_;
    int tempCounter_ = 1;
};

// -----------------------------------------------------------------------------
// AD integration interfaces.
// -----------------------------------------------------------------------------
// Members 2 and 3 do NOT need to inherit these or use std::function directly;
// this is documentation of the *shape* their code should have so it plugs
// into the pipeline below. See README.md, section "Final Integration
// Contract", for the full contract.
//
//   ir::IRFunction forwardDifferentiate(const ir::IRFunction& original,
//                                        const string& withRespectTo);
//
//   ir::IRFunction reverseDifferentiate(const ir::IRFunction& original,
//                                        const string& withRespectTo);
//
// Both take the original IR (produced by ASTToIR) and the name of the
// input variable to differentiate with respect to, and return a NEW
// IRFunction (the "derivative IR") whose returnValue evaluates to
// d(original.returnValue) / d(withRespectTo). That derivative IR is then
// fed into this module's Optimizer -> CodeGenerator -> Validator.
using ForwardADSignature = IRFunction(*)(const IRFunction&, const string&);
using ReverseADSignature = IRFunction(*)(const IRFunction&, const string&);

// -----------------------------------------------------------------------------
// Reference IR interpreter.
//
// Evaluates an IRFunction directly (no code generation involved). Useful for:
//   - unit-testing the IR / optimizer (before vs after must agree numerically)
//   - wrapping an IRFunction as a ScalarFunction for NumericalValidator
//     without needing a real compiled binary.
//
// `inputs` must provide a value for every name in func.params.
// Throws std::runtime_error on undefined variables/temps or unsupported ops.
// -----------------------------------------------------------------------------
double evaluate(const IRFunction& func, const map<string, double>& inputs);

} // namespace ir
