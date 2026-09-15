#pragma once

#include "AST.h"
#include <memory>
#include <string>

namespace frontend {

// Complete Member 1 pipeline:
// source -> lexer -> parser -> semantic/type/scope checking.
// On success the returned Program is the validated AST.
std::unique_ptr<ast::Program> parseAndValidate(const std::string& source);

} // namespace frontend
