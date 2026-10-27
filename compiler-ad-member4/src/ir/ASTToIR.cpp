// ASTToIR.cpp
#include "ASTToIR.h"
#include <functional>
#include <algorithm>
#include <map>
#include <set>

namespace astToIr {

using namespace std;
using namespace ir;

namespace {

ir::Op toIrOp(ast::BinaryOp op) {
    switch (op) {
        case ast::BinaryOp::ADD: return ir::Op::ADD;
        case ast::BinaryOp::SUB: return ir::Op::SUB;
        case ast::BinaryOp::MUL: return ir::Op::MUL;
        case ast::BinaryOp::DIV: return ir::Op::DIV;
        case ast::BinaryOp::POW: return ir::Op::POW;
    }
    throw ConversionError("Unknown ast::BinaryOp");
}

ir::Op toIrOp(ast::UnaryOp op) {
    switch (op) {
        case ast::UnaryOp::NEG: return ir::Op::NEG;
        case ast::UnaryOp::SIN: return ir::Op::SIN;
        case ast::UnaryOp::COS: return ir::Op::COS;
        case ast::UnaryOp::EXP: return ir::Op::EXP;
        case ast::UnaryOp::LOG: return ir::Op::LOG;
    }
    throw ConversionError("Unknown ast::UnaryOp");
}

// Maps a function-call name (sin/cos/exp/log/pow/neg) onto an ir::Op.
ir::Op functionNameToOp(const string& name, size_t argCount) {
    string lower = name;
    transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "sin") return ir::Op::SIN;
    if (lower == "cos") return ir::Op::COS;
    if (lower == "exp") return ir::Op::EXP;
    if (lower == "log") return ir::Op::LOG;
    if (lower == "neg") return ir::Op::NEG;
    if (lower == "pow" && argCount == 2) return ir::Op::POW;

    throw ConversionError("Unsupported function call in AST: " + name);
}

// Recursive-descent conversion of an ast::Node into an ir::Operand, emitting
// instructions into `builder` as needed, and collecting free-variable names
// into `discoveredParams` (in first-seen order) when `inferParams` is true.
Operand convertNode(const ast::Node& node,
                     IRBuilder& builder,
                     bool inferParams,
                     vector<string>& discoveredParams,
                     const map<string, Operand>* definedValues = nullptr) {

    if (const auto* num = dynamic_cast<const ast::Number*>(&node)) {
        return Operand::makeConstant(num->value);
    }

    if (const auto* var = dynamic_cast<const ast::Variable*>(&node)) {
        // A variable assigned by an earlier equation: reuse the value that
        // computed it instead of treating it as a program input.
        if (definedValues) {
            auto defined = definedValues->find(var->name);
            if (defined != definedValues->end()) return defined->second;
        }
        if (inferParams &&
            find(discoveredParams.begin(), discoveredParams.end(), var->name) == discoveredParams.end()) {
            discoveredParams.push_back(var->name);
            builder.addParam(var->name);
        }
        return Operand::makeVariable(var->name);
    }

    if (const auto* bin = dynamic_cast<const ast::BinaryExpression*>(&node)) {
        Operand lhs = convertNode(*bin->left, builder, inferParams, discoveredParams, definedValues);
        Operand rhs = convertNode(*bin->right, builder, inferParams, discoveredParams, definedValues);
        return builder.emit(toIrOp(bin->op), {lhs, rhs});
    }

    if (const auto* un = dynamic_cast<const ast::UnaryExpression*>(&node)) {
        Operand operand = convertNode(*un->operand, builder, inferParams, discoveredParams, definedValues);
        return builder.emit(toIrOp(un->op), {operand});
    }

    if (const auto* call = dynamic_cast<const ast::FunctionCall*>(&node)) {
        ir::Op op = functionNameToOp(call->callee, call->args.size());
        vector<Operand> operands;
        operands.reserve(call->args.size());
        for (const auto& argNode : call->args) {
            operands.push_back(convertNode(*argNode, builder, inferParams, discoveredParams, definedValues));
        }
        if (static_cast<int>(operands.size()) != opArity(op)) {
            throw ConversionError("Function call '" + call->callee + "' has wrong argument count");
        }
        return builder.emit(op, operands);
    }

    throw ConversionError("Unsupported AST node type encountered during AST->IR conversion");
}

// Keeps only the instructions the return value depends on and renumbers the
// surviving temps t1, t2, ... in order. Parameters are left untouched.
IRFunction pruneAndRenumber(const IRFunction& in) {
    set<int> live;
    if (in.returnValue.isTemp()) live.insert(in.returnValue.tempId);
    for (auto it = in.instructions.rbegin(); it != in.instructions.rend(); ++it) {
        if (!live.count(it->resultId)) continue;
        for (const auto& operand : it->operands) {
            if (operand.isTemp()) live.insert(operand.tempId);
        }
    }

    IRFunction out;
    out.name = in.name;
    out.params = in.params;

    map<int, int> newId;
    auto remap = [&](const Operand& operand) {
        return operand.isTemp() ? Operand::makeTemp(newId.at(operand.tempId)) : operand;
    };

    int next = 1;
    for (const auto& instr : in.instructions) {
        if (!live.count(instr.resultId)) continue;
        Instruction copy;
        copy.op = instr.op;
        for (const auto& operand : instr.operands) copy.operands.push_back(remap(operand));
        copy.resultId = next;
        newId[instr.resultId] = next++;
        out.instructions.push_back(copy);
    }
    out.returnValue = remap(in.returnValue);
    return out;
}

} // namespace

ir::IRFunction convertExpression(const ast::Node& expr,
                                  const string& functionName,
                                  const vector<string>& knownParams) {
    IRBuilder builder(functionName);

    bool inferParams = knownParams.empty();
    vector<string> discovered;

    if (!inferParams) {
        for (const auto& p : knownParams) builder.addParam(p);
    }

    Operand result = convertNode(expr, builder, inferParams, discovered);
    builder.setReturn(result);
    return builder.build();
}

ir::IRFunction convert(const ast::Assignment& assignment,
                        const string& functionName,
                        const vector<string>& knownParams) {
    if (!assignment.value) {
        throw ConversionError("Assignment has no value expression");
    }
    return convertExpression(*assignment.value, functionName, knownParams);
}

ir::IRProgram convertProgramAll(const ast::Program& program,
                                const string& functionPrefix) {
    if (program.assignments.empty()) {
        throw ConversionError("Program has no assignment statement");
    }

    vector<string> params;
    for (const auto& inputDecl : program.inputs) {
        if (inputDecl) {
            for (const auto& name : inputDecl->names) {
                params.push_back(name);
            }
        }
    }

    // One shared builder: every equation appends to the same SSA stream, so a
    // sub-expression that several equations use is emitted exactly once.
    IRBuilder builder(functionPrefix);
    const bool inferParams = params.empty();
    vector<string> discovered;
    for (const auto& p : params) builder.addParam(p);

    map<string, Operand> definedValues;   // assigned variable -> SSA value
    ir::IRProgram result;

    for (const auto& assignment : program.assignments) {
        if (!assignment || !assignment->value) {
            throw ConversionError("Assignment has no value expression");
        }
        if (definedValues.count(assignment->targetName)) {
            throw ConversionError("Variable '" + assignment->targetName + "' is assigned more than once");
        }

        Operand value = convertNode(*assignment->value, builder, inferParams,
                                    discovered, &definedValues);
        definedValues[assignment->targetName] = value;

        // Snapshot the stream so far as this output's function, then prune
        // away everything this particular output does not depend on.
        IRFunction snapshot = builder.peek();
        snapshot.name = functionPrefix + "_" + assignment->targetName;
        snapshot.returnValue = value;
        result.outputs.push_back({assignment->targetName, pruneAndRenumber(snapshot)});
    }

    result.params = builder.peek().params;
    // Parameters discovered after earlier snapshots were taken must be visible
    // on every output so all functions share one signature.
    for (auto& output : result.outputs) output.function.params = result.params;
    return result;
}

ir::IRFunction convertProgram(const ast::Program& program,
                              const string& functionName) {
    if (!program.assignment && program.assignments.empty()) {
        throw ConversionError("Program has no assignment statement");
    }
    ir::IRProgram all = convertProgramAll(program, functionName);
    IRFunction last = all.outputs.back().function;
    last.name = functionName;
    return last;
}

} // namespace astToIr
