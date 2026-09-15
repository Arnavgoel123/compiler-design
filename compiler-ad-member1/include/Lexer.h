#pragma once

#include "Token.h"
#include <stdexcept>
#include <string>
#include <vector>

namespace frontend {

class LexerError : public std::runtime_error {
public:
    explicit LexerError(const std::string& message) : std::runtime_error(message) {}
};

class Lexer {
public:
    explicit Lexer(std::string source);

    std::vector<Token> tokenize() const;
    Token nextToken();
    bool atEnd() const;

private:
    std::string source_;
    std::size_t current_ = 0;
    std::size_t line_ = 1;
    std::size_t column_ = 1;

    char peek() const;
    char peekNext() const;
    char advance();
    void skipWhitespaceAndComments();
    Token identifierOrKeyword();
    Token number();
    Token makeToken(TokenType type, std::size_t start, std::size_t startLine,
                    std::size_t startColumn) const;
    [[noreturn]] void error(const std::string& message) const;
};

} // namespace frontend
