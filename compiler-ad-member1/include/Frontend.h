#pragma once

#include "AST.h"
#include "SemanticAnalyzer.h"
#include <memory>
#include <string>
#include <vector>

namespace frontend {

// Complete Member 1 pipeline:
// source -> lexer -> parser -> semantic/type/scope checking.
// On success the returned Program is the validated AST.
//
// The program may contain several equations; later equations can use the
// variables defined by earlier ones. If `symbolsOut` is non-null it receives
// the symbol table (inputs, then assigned variables, in definition order).
std::unique_ptr<ast::Program> parseAndValidate(const std::string& source,
                                               std::vector<SymbolInfo>* symbolsOut = nullptr);

} // namespace frontend
