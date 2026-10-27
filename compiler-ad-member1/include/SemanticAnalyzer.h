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

// One row of the symbol table, in order of definition.
struct SymbolInfo {
    std::string name;
    ast::SymbolKind kind = ast::SymbolKind::Input;
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

    // Symbol table of the last analyzed program: every input, then every
    // assigned variable, in order of definition.
    const std::vector<SymbolInfo>& symbolTable() const { return symbolTable_; }

private:
    std::unordered_map<std::string, ast::SymbolKind> symbols_;
    std::vector<SymbolInfo> symbolTable_;
    std::vector<SemanticError> errors_;
    std::string currentTarget_;  // variable being defined (for self-reference errors)

    void reset();
    void report(const ast::Node& node, const std::string& message);
    ast::ValueType checkExpression(ast::Node& expression);
    void checkInputDeclaration(const ast::InputDeclaration& declaration);
    void checkAssignment(ast::Assignment& assignment);
};

} // namespace frontend
