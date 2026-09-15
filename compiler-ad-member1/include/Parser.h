#pragma once

#include "AST.h"
#include "Token.h"
#include <stdexcept>
#include <string>
#include <vector>

namespace frontend {

class ParserError : public std::runtime_error {
public:
    explicit ParserError(const std::string& message) : std::runtime_error(message) {}
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    std::unique_ptr<ast::Program> parseProgram();

private:
    std::vector<Token> tokens_;
    std::size_t current_ = 0;

    const Token& peek() const;
    const Token& previous() const;
    bool check(TokenType type) const;
    bool match(TokenType type);
    const Token& consume(TokenType type, const std::string& message);

    std::unique_ptr<ast::InputDeclaration> parseInputDeclaration();
    std::unique_ptr<ast::Assignment> parseAssignment();
    std::unique_ptr<ast::Node> parseExpression();
    std::unique_ptr<ast::Node> parseAdditive();
    std::unique_ptr<ast::Node> parseMultiplicative();
    std::unique_ptr<ast::Node> parseUnary();
    std::unique_ptr<ast::Node> parsePower();
    std::unique_ptr<ast::Node> parsePrimary();
    std::vector<std::unique_ptr<ast::Node>> parseArguments();

    [[noreturn]] void errorAt(const Token& token, const std::string& message) const;
};

} // namespace frontend
