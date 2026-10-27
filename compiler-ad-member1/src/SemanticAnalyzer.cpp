#include "SemanticAnalyzer.h"

#include <cmath>
#include <sstream>
#include <unordered_map>

namespace frontend {

namespace {

struct FunctionInfo {
    int arity;
};

const std::unordered_map<std::string, FunctionInfo> functions = {
    {"sin", {1}}, {"cos", {1}}, {"exp", {1}},
    {"log", {1}}, {"pow", {2}}
};

} // namespace

void SemanticAnalyzer::reset() {
    symbols_.clear();
    symbolTable_.clear();
    errors_.clear();
    currentTarget_.clear();
}

void SemanticAnalyzer::report(const ast::Node& node, const std::string& message) {
    errors_.push_back({message, node.location.line, node.location.column});
}

bool SemanticAnalyzer::analyze(ast::Program& program) {
    reset();

    for (const auto& declaration : program.inputs) {
        checkInputDeclaration(*declaration);
    }

    if (program.assignments.empty()) {
        report(program, "Program has no output assignment");
    }
    for (const auto& assignment : program.assignments) {
        if (assignment) checkAssignment(*assignment);
    }

    return errors_.empty();
}

void SemanticAnalyzer::analyzeOrThrow(ast::Program& program) {
    if (analyze(program)) return;

    std::ostringstream out;
    out << "Semantic analysis failed:\n";
    for (const auto& error : errors_) {
        out << "  " << error.line << ':' << error.column << ": " << error.message << '\n';
    }
    throw SemanticErrorException(out.str());
}

void SemanticAnalyzer::checkInputDeclaration(const ast::InputDeclaration& declaration) {
    for (const std::string& name : declaration.names) {
        if (symbols_.count(name)) {
            report(declaration, "Duplicate input declaration: '" + name + "'");
            continue;
        }
        symbols_[name] = ast::SymbolKind::Input;
        symbolTable_.push_back({name, ast::SymbolKind::Input,
                                declaration.location.line, declaration.location.column});
    }
}

void SemanticAnalyzer::checkAssignment(ast::Assignment& assignment) {
    if (assignment.targetName.empty()) {
        report(assignment, "Output variable name cannot be empty");
        return;
    }

    // The right-hand side is checked BEFORE the target is defined, so an
    // equation can use earlier results but never its own value.
    if (!assignment.value) {
        report(assignment, "Assignment has no expression");
    } else {
        currentTarget_ = assignment.targetName;
        assignment.type = checkExpression(*assignment.value);
        currentTarget_.clear();
        if (assignment.type != ast::ValueType::Number) {
            report(assignment, "Output expression must have numeric type");
        }
    }

    const auto existing = symbols_.find(assignment.targetName);
    if (existing != symbols_.end()) {
        if (existing->second == ast::SymbolKind::Input) {
            report(assignment, "Output variable '" + assignment.targetName +
                               "' conflicts with an input variable");
        } else {
            report(assignment, "Variable '" + assignment.targetName +
                               "' is assigned more than once (each variable may be defined only once)");
        }
        return;
    }

    symbols_[assignment.targetName] = ast::SymbolKind::Output;
    symbolTable_.push_back({assignment.targetName, ast::SymbolKind::Output,
                            assignment.location.line, assignment.location.column});
}

ast::ValueType SemanticAnalyzer::checkExpression(ast::Node& expression) {
    if (auto* number = dynamic_cast<ast::Number*>(&expression)) {
        if (!std::isfinite(number->value)) {
            report(*number, "Numeric literal must be finite");
            return ast::ValueType::Unknown;
        }
        return ast::ValueType::Number;
    }

    if (auto* variable = dynamic_cast<ast::Variable*>(&expression)) {
        const auto it = symbols_.find(variable->name);
        if (it == symbols_.end()) {
            if (variable->name == currentTarget_) {
                report(*variable, "Variable '" + variable->name +
                                  "' is used in its own definition");
            } else {
                report(*variable, "Use of undeclared variable '" + variable->name +
                                  "' (inputs must be declared with 'input', and a variable "
                                  "must be assigned before it is used)");
            }
            variable->type = ast::ValueType::Unknown;
            return variable->type;
        }
        // Both inputs and previously assigned variables are valid operands.
        variable->type = ast::ValueType::Number;
        return variable->type;
    }

    if (auto* binary = dynamic_cast<ast::BinaryExpression*>(&expression)) {
        const ast::ValueType left = checkExpression(*binary->left);
        const ast::ValueType right = checkExpression(*binary->right);
        if (left == ast::ValueType::Number && right == ast::ValueType::Number) {
            return ast::ValueType::Number;
        }
        return ast::ValueType::Unknown;
    }

    if (auto* unary = dynamic_cast<ast::UnaryExpression*>(&expression)) {
        return checkExpression(*unary->operand) == ast::ValueType::Number
            ? ast::ValueType::Number : ast::ValueType::Unknown;
    }

    if (auto* call = dynamic_cast<ast::FunctionCall*>(&expression)) {
        const auto function = functions.find(call->callee);
        if (function == functions.end()) {
            report(*call, "Unknown function '" + call->callee + "'");
        } else if (static_cast<int>(call->args.size()) != function->second.arity) {
            report(*call, "Function '" + call->callee + "' expects " +
                          std::to_string(function->second.arity) + " argument(s), got " +
                          std::to_string(call->args.size()));
        }

        bool allNumeric = true;
        for (auto& arg : call->args) {
            if (checkExpression(*arg) != ast::ValueType::Number) allNumeric = false;
        }
        return (function != functions.end() && allNumeric &&
                static_cast<int>(call->args.size()) == function->second.arity)
            ? ast::ValueType::Number : ast::ValueType::Unknown;
    }

    report(expression, "Unknown AST expression node");
    return ast::ValueType::Unknown;
}

} // namespace frontend
