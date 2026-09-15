#pragma once

#include "AST.h"
#include <string>

namespace frontend {

// Human-readable tree representation of the validated AST.
std::string printAST(const ast::Program& program);

} // namespace frontend
