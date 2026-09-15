#include "Lexer.h"

#include <cctype>
#include <cstdlib>
#include <sstream>
#include <unordered_map>

namespace frontend {

namespace {

const std::unordered_map<std::string, TokenType> keywords = {
    {"input", TokenType::Input},
    {"sin", TokenType::Sin},
    {"cos", TokenType::Cos},
    {"exp", TokenType::Exp},
    {"log", TokenType::Log},
    {"pow", TokenType::Pow}
};

bool isIdentifierStart(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool isIdentifierPart(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

} // namespace

std::string tokenTypeName(TokenType type) {
    switch (type) {
        case TokenType::EndOfFile: return "end of file";
        case TokenType::Identifier: return "identifier";
        case TokenType::Number: return "number";
        case TokenType::Input: return "input";
        case TokenType::Sin: return "sin";
        case TokenType::Cos: return "cos";
        case TokenType::Exp: return "exp";
        case TokenType::Log: return "log";
        case TokenType::Pow: return "pow";
        case TokenType::Plus: return "'+'";
        case TokenType::Minus: return "'-'";
        case TokenType::Star: return "'*'";
        case TokenType::Slash: return "'/'";
        case TokenType::Caret: return "'^'";
        case TokenType::Assign: return "'='";
        case TokenType::Comma: return "','";
        case TokenType::Semicolon: return "';'";
        case TokenType::LeftParen: return "'('";
        case TokenType::RightParen: return "')'";
    }
    return "unknown token";
}

Lexer::Lexer(std::string source) : source_(std::move(source)) {}

char Lexer::peek() const {
    return atEnd() ? '\0' : source_[current_];
}

char Lexer::peekNext() const {
    return current_ + 1 >= source_.size() ? '\0' : source_[current_ + 1];
}

char Lexer::advance() {
    const char c = source_[current_++];
    if (c == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }
    return c;
}

bool Lexer::atEnd() const {
    return current_ >= source_.size();
}

void Lexer::skipWhitespaceAndComments() {
    while (!atEnd()) {
        const char c = peek();
        if (std::isspace(static_cast<unsigned char>(c))) {
            advance();
            continue;
        }

        if (c == '/' && peekNext() == '/') {
            while (!atEnd() && peek() != '\n') advance();
            continue;
        }

        break;
    }
}

Token Lexer::makeToken(TokenType type, std::size_t start,
                       std::size_t startLine, std::size_t startColumn) const {
    return Token(type, source_.substr(start, current_ - start), startLine, startColumn);
}

Token Lexer::identifierOrKeyword() {
    const std::size_t start = current_;
    const std::size_t startLine = line_;
    const std::size_t startColumn = column_;

    advance();
    while (isIdentifierPart(peek())) advance();

    const std::string text = source_.substr(start, current_ - start);
    const auto it = keywords.find(text);
    if (it != keywords.end()) {
        return makeToken(it->second, start, startLine, startColumn);
    }
    return makeToken(TokenType::Identifier, start, startLine, startColumn);
}

Token Lexer::number() {
    const std::size_t start = current_;
    const std::size_t startLine = line_;
    const std::size_t startColumn = column_;

    bool sawDigits = false;
    while (std::isdigit(static_cast<unsigned char>(peek()))) {
        sawDigits = true;
        advance();
    }

    if (peek() == '.') {
        advance();
        while (std::isdigit(static_cast<unsigned char>(peek()))) {
            sawDigits = true;
            advance();
        }
    }

    if (!sawDigits) {
        error("Invalid numeric literal");
    }

    if (peek() == 'e' || peek() == 'E') {
        advance();
        if (peek() == '+' || peek() == '-') advance();
        bool exponentDigits = false;
        while (std::isdigit(static_cast<unsigned char>(peek()))) {
            exponentDigits = true;
            advance();
        }
        if (!exponentDigits) error("Invalid exponent in numeric literal");
    }

    Token token = makeToken(TokenType::Number, start, startLine, startColumn);
    try {
        token.numberValue = std::stod(token.lexeme);
    } catch (...) {
        error("Numeric literal is out of range: " + token.lexeme);
    }
    return token;
}

Token Lexer::nextToken() {
    skipWhitespaceAndComments();

    if (atEnd()) return Token(TokenType::EndOfFile, "", line_, column_);

    const std::size_t start = current_;
    const std::size_t startLine = line_;
    const std::size_t startColumn = column_;
    const char c = peek();

    if (isIdentifierStart(c)) return identifierOrKeyword();
    if (std::isdigit(static_cast<unsigned char>(c)) ||
        (c == '.' && std::isdigit(static_cast<unsigned char>(peekNext())))) {
        return number();
    }

    advance();
    switch (c) {
        case '+': return makeToken(TokenType::Plus, start, startLine, startColumn);
        case '-': return makeToken(TokenType::Minus, start, startLine, startColumn);
        case '*': return makeToken(TokenType::Star, start, startLine, startColumn);
        case '/': return makeToken(TokenType::Slash, start, startLine, startColumn);
        case '^': return makeToken(TokenType::Caret, start, startLine, startColumn);
        case '=': return makeToken(TokenType::Assign, start, startLine, startColumn);
        case ',': return makeToken(TokenType::Comma, start, startLine, startColumn);
        case ';': return makeToken(TokenType::Semicolon, start, startLine, startColumn);
        case '(': return makeToken(TokenType::LeftParen, start, startLine, startColumn);
        case ')': return makeToken(TokenType::RightParen, start, startLine, startColumn);
        default:
            error(std::string("Unexpected character '") + c + "'");
    }
}

std::vector<Token> Lexer::tokenize() const {
    Lexer copy(*this);
    std::vector<Token> tokens;
    while (!copy.atEnd()) tokens.push_back(copy.nextToken());
    tokens.push_back(copy.nextToken());
    return tokens;
}

[[noreturn]] void Lexer::error(const std::string& message) const {
    std::ostringstream out;
    out << "Lexer error at " << line_ << ':' << column_ << ": " << message;
    throw LexerError(out.str());
}

} // namespace frontend
