#pragma once

#include "AST.h"
#include "SemanticAnalyzer.h"
#include <string>
#include <vector>

namespace frontend {

// Human-readable tree representation of the validated AST.
std::string printAST(const ast::Program& program);

// Tabular dump of the semantic analyzer's symbol table.
std::string printSymbolTable(const std::vector<SymbolInfo>& symbols);

} // namespace frontend
