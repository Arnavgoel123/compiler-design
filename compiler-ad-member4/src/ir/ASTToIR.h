// ASTToIR.h
// -----------------------------------------------------------------------------
// AST -> IR conversion.
//
// IMPORTANT INTEGRATION NOTE:
// Member 1 owns the real Lexer/Parser/Semantic Analyzer/AST. This file
// currently defines a small, conventional AST shape (namespace `ast`) that
// mirrors what almost every expression-based AST looks like: Number,
// Variable, BinaryExpression, UnaryExpression, FunctionCall, Assignment.
//
// This AST definition is intentionally isolated in this single header.
// When Member 1 delivers the real AST:
//   - Only this file (ASTToIR.h/.cpp) needs to change.
//   - The `ast::*` node types below get replaced/adapted to Member 1's
//     real node types (or a thin adapter layer is added here that maps
//     his nodes to calls into ASTToIR::convertExpr-equivalent logic).
//   - IR.h/.cpp, Optimizer, CodeGenerator and Validator are NOT touched,
//     because they only ever operate on ir::IRFunction / ir::Operand.
// -----------------------------------------------------------------------------
#pragma once

#include <string>
#include <vector>
#include <memory>
#include "IR.h"

namespace ast {

using namespace std;

// Binary/unary operator kinds at the AST level. These map onto ir::Op
// during conversion (see ASTToIR.cpp).
enum class BinaryOp { ADD, SUB, MUL, DIV, POW };
enum class UnaryOp  { NEG, SIN, COS, EXP, LOG };

// Base class for all AST expression nodes.
struct Node {
    virtual ~Node() = default;
};

struct Number : Node {
    double value;
    explicit Number(double v) : value(v) {}
};

struct Variable : Node {
    string name;
    explicit Variable(string n) : name(std::move(n)) {}
};

struct BinaryExpression : Node {
    BinaryOp op;
    unique_ptr<Node> left;
    unique_ptr<Node> right;
    BinaryExpression(BinaryOp o, unique_ptr<Node> l, unique_ptr<Node> r)
        : op(o), left(std::move(l)), right(std::move(r)) {}
};

struct UnaryExpression : Node {
    UnaryOp op;
    unique_ptr<Node> operand;
    UnaryExpression(UnaryOp o, unique_ptr<Node> operand_)
        : op(o), operand(std::move(operand_)) {}
};

// Generic function-call form, e.g. sin(x), cos(x), exp(x), log(x), pow(x, y).
// Provided as an alternative to UnaryExpression/BinaryOp::POW in case
// Member 1's parser represents built-in functions as calls instead of
// dedicated unary nodes.
struct FunctionCall : Node {
    string callee; // "sin", "cos", "exp", "log", "pow"
    vector<unique_ptr<Node>> args;
    FunctionCall(string name, vector<unique_ptr<Node>> a)
        : callee(std::move(name)), args(std::move(a)) {}
};

// Top level "y = <expr>" statement.
struct Assignment : Node {
    string targetName;   // e.g. "y" (the output name)
    unique_ptr<Node> value;
    Assignment(string target, unique_ptr<Node> v)
        : targetName(std::move(target)), value(std::move(v)) {}
};

} // namespace ast

namespace astToIr {

using namespace std;

// Thrown when the AST references something the converter cannot handle
// (e.g. an unknown function name, or a malformed node).
struct ConversionError : public runtime_error {
    explicit ConversionError(const string& msg) : runtime_error(msg) {}
};

// Converts a top level Assignment AST into a structured IRFunction.
// `functionName` becomes the generated IR function's name.
// `knownParams`, if non-empty, fixes the parameter list/order explicitly;
// otherwise, parameters are inferred from every ast::Variable encountered,
// in first-seen order.
ir::IRFunction convert(const ast::Assignment& assignment,
                        const string& functionName = "f",
                        const vector<string>& knownParams = {});

// Convenience overload: converts a bare expression (no assignment) into an
// IRFunction that simply returns the expression's value.
ir::IRFunction convertExpression(const ast::Node& expr,
                                  const string& functionName = "f",
                                  const vector<string>& knownParams = {});

} // namespace astToIr
