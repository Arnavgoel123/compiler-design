#pragma once

#include "AST.h"
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace frontend {

struct SemanticError {
    std::string message;
    std::size_t line = 1;
    std::size_t column = 1;
};

class SemanticErrorException : public std::runtime_error {
public:
    explicit SemanticErrorException(const std::string& message) : std::runtime_error(message) {}
};

class SemanticAnalyzer {
public:
    // Returns true when the complete program is semantically valid.
    // Errors are collected so callers can display all errors at once.
    bool analyze(ast::Program& program);

    // Same analysis, but throws on the first semantic error.
    void analyzeOrThrow(ast::Program& program);

    const std::vector<SemanticError>& errors() const { return errors_; }

private:
    std::unordered_map<std::string, ast::SymbolKind> symbols_;
    std::vector<SemanticError> errors_;

    void reset();
    void report(const ast::Node& node, const std::string& message);
    ast::ValueType checkExpression(ast::Node& expression);
    void checkInputDeclaration(const ast::InputDeclaration& declaration);
    void checkAssignment(ast::Assignment& assignment);
};

} // namespace frontend
