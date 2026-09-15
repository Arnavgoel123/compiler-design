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
#include "AST.h" // Shared AST definitions from Member 1

namespace astToIr {

using namespace std;

// Thrown when the AST references something the converter cannot handle
// (e.g. an unknown function name, or a malformed node).
struct ConversionError : public runtime_error {
    explicit ConversionError(const string& msg) : runtime_error(msg) {}
};

// Converts a complete Member 1 Program AST (including input declarations and assignment)
// into a structured IRFunction.
ir::IRFunction convertProgram(const ast::Program& program,
                              const string& functionName = "f");

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
