#include "Parser.h"

#include <sstream>

namespace frontend {

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

const Token& Parser::peek() const { return tokens_[current_]; }
const Token& Parser::previous() const { return tokens_[current_ - 1]; }

bool Parser::check(TokenType type) const { return peek().type == type; }

bool Parser::match(TokenType type) {
    if (!check(type)) return false;
    ++current_;
    return true;
}

const Token& Parser::consume(TokenType type, const std::string& message) {
    if (check(type)) return tokens_[current_++];
    errorAt(peek(), message);
}

std::unique_ptr<ast::Program> Parser::parseProgram() {
    auto program = std::make_unique<ast::Program>();

    while (match(TokenType::Input)) {
        --current_;
        program->inputs.push_back(parseInputDeclaration());
    }

    if (check(TokenType::EndOfFile)) {
        errorAt(peek(), "Expected an assignment such as 'y = x * x;'");
    }

    program->assignment = parseAssignment();
    consume(TokenType::EndOfFile, "Expected end of input after assignment");
    return program;
}

std::unique_ptr<ast::InputDeclaration> Parser::parseInputDeclaration() {
    const Token& input = consume(TokenType::Input, "Expected 'input'");
    std::vector<std::string> names;

    const Token& first = consume(TokenType::Identifier,
                                 "Expected an identifier after 'input'");
    names.push_back(first.lexeme);

    while (match(TokenType::Comma)) {
        const Token& name = consume(TokenType::Identifier,
                                     "Expected an identifier after ','");
        names.push_back(name.lexeme);
    }

    consume(TokenType::Semicolon, "Expected ';' after input declaration");
    return std::make_unique<ast::InputDeclaration>(std::move(names),
        ast::SourceLocation{input.line, input.column});
}

std::unique_ptr<ast::Assignment> Parser::parseAssignment() {
    const Token& target = consume(TokenType::Identifier,
                                  "Expected output variable name");
    consume(TokenType::Assign, "Expected '=' after output variable name");
    auto value = parseExpression();
    consume(TokenType::Semicolon, "Expected ';' after expression");

    return std::make_unique<ast::Assignment>(target.lexeme, std::move(value),
        ast::SourceLocation{target.line, target.column});
}

std::unique_ptr<ast::Node> Parser::parseExpression() {
    return parseAdditive();
}

std::unique_ptr<ast::Node> Parser::parseAdditive() {
    auto expression = parseMultiplicative();

    while (true) {
        ast::BinaryOp op;
        if (match(TokenType::Plus)) op = ast::BinaryOp::ADD;
        else if (match(TokenType::Minus)) op = ast::BinaryOp::SUB;
        else break;

        auto right = parseMultiplicative();
        expression = std::make_unique<ast::BinaryExpression>(
            op, std::move(expression), std::move(right));
    }
    return expression;
}

std::unique_ptr<ast::Node> Parser::parseMultiplicative() {
    auto expression = parseUnary();

    while (true) {
        ast::BinaryOp op;
        if (match(TokenType::Star)) op = ast::BinaryOp::MUL;
        else if (match(TokenType::Slash)) op = ast::BinaryOp::DIV;
        else break;

        auto right = parseUnary();
        expression = std::make_unique<ast::BinaryExpression>(
            op, std::move(expression), std::move(right));
    }
    return expression;
}

std::unique_ptr<ast::Node> Parser::parseUnary() {
    if (match(TokenType::Minus)) {
        const Token& token = previous();
        return std::make_unique<ast::UnaryExpression>(
            ast::UnaryOp::NEG, parseUnary(),
            ast::SourceLocation{token.line, token.column});
    }
    return parsePower();
}

std::unique_ptr<ast::Node> Parser::parsePower() {
    auto expression = parsePrimary();

    if (match(TokenType::Caret)) {
        auto right = parseUnary();
        expression = std::make_unique<ast::BinaryExpression>(
            ast::BinaryOp::POW, std::move(expression), std::move(right));
    }
    return expression;
}

std::unique_ptr<ast::Node> Parser::parsePrimary() {
    if (match(TokenType::Number)) {
        const Token& token = previous();
        return std::make_unique<ast::Number>(
            token.numberValue, ast::SourceLocation{token.line, token.column});
    }

    if (match(TokenType::Identifier)) {
        const Token& identifier = previous();
        if (match(TokenType::LeftParen)) {
            auto args = parseArguments();
            return std::make_unique<ast::FunctionCall>(
                identifier.lexeme, std::move(args),
                ast::SourceLocation{identifier.line, identifier.column});
        }
        return std::make_unique<ast::Variable>(
            identifier.lexeme, ast::SourceLocation{identifier.line, identifier.column});
    }

    if (match(TokenType::Sin) || match(TokenType::Cos) ||
        match(TokenType::Exp) || match(TokenType::Log) || match(TokenType::Pow)) {
        const Token& function = previous();
        consume(TokenType::LeftParen, "Expected '(' after function name");
        auto args = parseArguments();
        return std::make_unique<ast::FunctionCall>(
            function.lexeme, std::move(args),
            ast::SourceLocation{function.line, function.column});
    }

    if (match(TokenType::LeftParen)) {
        auto expression = parseExpression();
        consume(TokenType::RightParen, "Expected ')' after expression");
        return expression;
    }

    errorAt(peek(), "Expected a number, identifier, function call, or parenthesized expression");
}

std::vector<std::unique_ptr<ast::Node>> Parser::parseArguments() {
    std::vector<std::unique_ptr<ast::Node>> args;
    if (!check(TokenType::RightParen)) {
        do {
            args.push_back(parseExpression());
        } while (match(TokenType::Comma));
    }
    consume(TokenType::RightParen, "Expected ')' after function arguments");
    return args;
}

[[noreturn]] void Parser::errorAt(const Token& token, const std::string& message) const {
    std::ostringstream out;
    out << "Parser error at " << token.line << ':' << token.column << ": " << message;
    throw ParserError(out.str());
}

} // namespace frontend
