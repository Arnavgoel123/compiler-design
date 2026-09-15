#include "ASTPrinter.h"

#include <iomanip>
#include <sstream>

namespace frontend {
namespace {

std::string binaryName(ast::BinaryOp op) {
    switch (op) {
        case ast::BinaryOp::ADD: return "ADD";
        case ast::BinaryOp::SUB: return "SUB";
        case ast::BinaryOp::MUL: return "MUL";
        case ast::BinaryOp::DIV: return "DIV";
        case ast::BinaryOp::POW: return "POW";
    }
    return "UNKNOWN";
}

std::string unaryName(ast::UnaryOp op) {
    switch (op) {
        case ast::UnaryOp::NEG: return "NEG";
        case ast::UnaryOp::SIN: return "SIN";
        case ast::UnaryOp::COS: return "COS";
        case ast::UnaryOp::EXP: return "EXP";
        case ast::UnaryOp::LOG: return "LOG";
    }
    return "UNKNOWN";
}

void printNode(const ast::Node& node, std::ostringstream& out, const std::string& prefix) {
    if (const auto* number = dynamic_cast<const ast::Number*>(&node)) {
        out << prefix << "Number(" << std::setprecision(15) << number->value << ")\n";
        return;
    }

    if (const auto* variable = dynamic_cast<const ast::Variable*>(&node)) {
        out << prefix << "Variable(" << variable->name << ": Number)\n";
        return;
    }

    if (const auto* binary = dynamic_cast<const ast::BinaryExpression*>(&node)) {
        out << prefix << "Binary(" << binaryName(binary->op) << ")\n";
        printNode(*binary->left, out, prefix + "  left: ");
        printNode(*binary->right, out, prefix + "  right: ");
        return;
    }

    if (const auto* unary = dynamic_cast<const ast::UnaryExpression*>(&node)) {
        out << prefix << "Unary(" << unaryName(unary->op) << ")\n";
        printNode(*unary->operand, out, prefix + "  ");
        return;
    }

    if (const auto* call = dynamic_cast<const ast::FunctionCall*>(&node)) {
        out << prefix << "Call(" << call->callee << ")\n";
        for (std::size_t i = 0; i < call->args.size(); ++i) {
            printNode(*call->args[i], out, prefix + "  arg" + std::to_string(i + 1) + ": ");
        }
        return;
    }

    out << prefix << "<unknown node>\n";
}

} // namespace

std::string printAST(const ast::Program& program) {
    std::ostringstream out;
    out << "Program\n";

    if (!program.inputs.empty()) {
        out << "  Inputs:\n";
        for (const auto& declaration : program.inputs) {
            for (const auto& name : declaration->names) {
                out << "    " << name << ": Number\n";
            }
        }
    }

    if (program.assignment) {
        out << "  Assignment(" << program.assignment->targetName << ")\n";
        if (program.assignment->value) {
            printNode(*program.assignment->value, out, "    ");
        }
    }

    return out.str();
}

} // namespace frontend
