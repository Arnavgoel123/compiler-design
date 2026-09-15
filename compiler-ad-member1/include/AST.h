#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ast {

enum class BinaryOp { ADD, SUB, MUL, DIV, POW };
enum class UnaryOp  { NEG, SIN, COS, EXP, LOG };

enum class ValueType { Number, Unknown };

enum class SymbolKind { Input, Output };

struct SourceLocation {
    std::size_t line = 1;
    std::size_t column = 1;
};

struct Node {
    SourceLocation location;
    virtual ~Node() = default;
    explicit Node(SourceLocation loc = {}) : location(loc) {}
};

struct Expression : Node {
    using Node::Node;
};

struct Number : Expression {
    double value;
    explicit Number(double v, SourceLocation loc = {}) : Expression(loc), value(v) {}
};

struct Variable : Expression {
    std::string name;
    ValueType type = ValueType::Unknown;
    explicit Variable(std::string n, SourceLocation loc = {})
        : Expression(loc), name(std::move(n)) {}
};

struct BinaryExpression : Expression {
    BinaryOp op;
    std::unique_ptr<Node> left;
    std::unique_ptr<Node> right;

    BinaryExpression(BinaryOp o, std::unique_ptr<Node> l,
                     std::unique_ptr<Node> r, SourceLocation loc = {})
        : Expression(loc), op(o), left(std::move(l)), right(std::move(r)) {}
};

struct UnaryExpression : Expression {
    UnaryOp op;
    std::unique_ptr<Node> operand;

    UnaryExpression(UnaryOp o, std::unique_ptr<Node> operand_, SourceLocation loc = {})
        : Expression(loc), op(o), operand(std::move(operand_)) {}
};

struct FunctionCall : Expression {
    std::string callee;
    std::vector<std::unique_ptr<Node>> args;

    FunctionCall(std::string name, std::vector<std::unique_ptr<Node>> a,
                 SourceLocation loc = {})
        : Expression(loc), callee(std::move(name)), args(std::move(a)) {}
};

struct InputDeclaration : Node {
    std::vector<std::string> names;
    explicit InputDeclaration(std::vector<std::string> n, SourceLocation loc = {})
        : Node(loc), names(std::move(n)) {}
};

struct Assignment : Node {
    std::string targetName;
    std::unique_ptr<Node> value;
    ValueType type = ValueType::Unknown;

    Assignment(std::string target, std::unique_ptr<Node> v, SourceLocation loc = {})
        : Node(loc), targetName(std::move(target)), value(std::move(v)) {}
};

struct Program : Node {
    std::vector<std::unique_ptr<InputDeclaration>> inputs;
    std::unique_ptr<Assignment> assignment;

    explicit Program(SourceLocation loc = {}) : Node(loc) {}
};

} // namespace ast
