#pragma once

#include <cstddef>
#include <string>

namespace frontend {

enum class TokenType {
    EndOfFile,
    Identifier,
    Number,

    Input,
    Sin,
    Cos,
    Exp,
    Log,
    Pow,

    Plus,
    Minus,
    Star,
    Slash,
    Caret,
    Assign,
    Comma,
    Semicolon,
    LeftParen,
    RightParen
};

struct Token {
    TokenType type = TokenType::EndOfFile;
    std::string lexeme;
    double numberValue = 0.0;
    std::size_t line = 1;
    std::size_t column = 1;

    Token() = default;
    Token(TokenType t, std::string text, std::size_t l, std::size_t c)
        : type(t), lexeme(std::move(text)), line(l), column(c) {}
};

std::string tokenTypeName(TokenType type);

} // namespace frontend
