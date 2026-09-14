// IR.cpp
#include "IR.h"
#include <sstream>
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace ir {

using namespace std;

int opArity(Op op) {
    switch (op) {
        case Op::ADD:
        case Op::SUB:
        case Op::MUL:
        case Op::DIV:
        case Op::POW:
            return 2;
        case Op::NEG:
        case Op::SIN:
        case Op::COS:
        case Op::EXP:
        case Op::LOG:
            return 1;
    }
    return -1;
}

string opName(Op op) {
    switch (op) {
        case Op::ADD: return "ADD";
        case Op::SUB: return "SUB";
        case Op::MUL: return "MUL";
        case Op::DIV: return "DIV";
        case Op::NEG: return "NEG";
        case Op::SIN: return "SIN";
        case Op::COS: return "COS";
        case Op::EXP: return "EXP";
        case Op::LOG: return "LOG";
        case Op::POW: return "POW";
    }
    return "UNKNOWN";
}

// ---------------------------------------------------------------- Operand --

Operand Operand::makeConstant(double value) {
    Operand o;
    o.kind = OperandKind::Constant;
    o.constValue = value;
    return o;
}

Operand Operand::makeVariable(const string& name) {
    Operand o;
    o.kind = OperandKind::Variable;
    o.varName = name;
    return o;
}

Operand Operand::makeTemp(int id) {
    Operand o;
    o.kind = OperandKind::Temp;
    o.tempId = id;
    return o;
}

string Operand::toString() const {
    ostringstream oss;
    switch (kind) {
        case OperandKind::Constant: {
            // Print integers without a trailing ".0" clutter, keep doubles readable.
            double intPart;
            if (modf(constValue, &intPart) == 0.0) {
                oss << static_cast<long long>(intPart);
            } else {
                oss << constValue;
            }
            break;
        }
        case OperandKind::Variable:
            oss << varName;
            break;
        case OperandKind::Temp:
            oss << "t" << tempId;
            break;
    }
    return oss.str();
}

bool Operand::operator==(const Operand& other) const {
    if (kind != other.kind) return false;
    switch (kind) {
        case OperandKind::Constant: return constValue == other.constValue;
        case OperandKind::Variable: return varName == other.varName;
        case OperandKind::Temp:     return tempId == other.tempId;
    }
    return false;
}

// ------------------------------------------------------------- Instruction --

string Instruction::toString() const {
    ostringstream oss;
    oss << "t" << resultId << " = " << opName(op);
    for (const auto& operand : operands) {
        oss << " " << operand.toString();
    }
    return oss.str();
}

// -------------------------------------------------------------- IRFunction --

bool IRFunction::hasParam(const string& name) const {
    return find(params.begin(), params.end(), name) != params.end();
}

int IRFunction::nextTempId() const {
    int maxId = 0;
    for (const auto& instr : instructions) {
        maxId = max(maxId, instr.resultId);
    }
    return maxId + 1;
}

string IRFunction::toString() const {
    ostringstream oss;
    oss << "===== IR FUNCTION: " << name << "(";
    for (size_t i = 0; i < params.size(); ++i) {
        oss << params[i];
        if (i + 1 < params.size()) oss << ", ";
    }
    oss << ") =====\n";
    for (const auto& instr : instructions) {
        oss << instr.toString() << "\n";
    }
    oss << "RETURN " << returnValue.toString() << "\n";
    oss << "======================================" ;
    return oss.str();
}

// --------------------------------------------------------------- IRBuilder --

IRBuilder::IRBuilder(string functionName) {
    func_.name = std::move(functionName);
}

void IRBuilder::addParam(const string& name) {
    if (!func_.hasParam(name)) {
        func_.params.push_back(name);
    }
}

Operand IRBuilder::emit(Op op, const vector<Operand>& operands) {
    if (static_cast<int>(operands.size()) != opArity(op)) {
        throw invalid_argument("IRBuilder::emit - wrong operand count for " + opName(op));
    }
    Instruction instr;
    instr.resultId = tempCounter_++;
    instr.op = op;
    instr.operands = operands;
    func_.instructions.push_back(instr);
    return Operand::makeTemp(instr.resultId);
}

// -----------------------------------------------------------------------------
// Reference IR interpreter.
// -----------------------------------------------------------------------------
namespace {

double resolveOperandValue(const Operand& operand,
                            const map<string, double>& inputs,
                            const unordered_map<int, double>& tempValues) {
    switch (operand.kind) {
        case OperandKind::Constant:
            return operand.constValue;
        case OperandKind::Variable: {
            auto it = inputs.find(operand.varName);
            if (it == inputs.end()) {
                throw runtime_error("ir::evaluate - missing input value for variable '" +
                                    operand.varName + "'");
            }
            return it->second;
        }
        case OperandKind::Temp: {
            auto it = tempValues.find(operand.tempId);
            if (it == tempValues.end()) {
                throw runtime_error("ir::evaluate - temp t" + to_string(operand.tempId) +
                                    " used before being defined");
            }
            return it->second;
        }
    }
    throw runtime_error("ir::evaluate - malformed operand");
}

double applyOp(Op op, const vector<double>& args) {
    switch (op) {
        case Op::ADD: return args[0] + args[1];
        case Op::SUB: return args[0] - args[1];
        case Op::MUL: return args[0] * args[1];
        case Op::DIV: return args[0] / args[1];
        case Op::NEG: return -args[0];
        case Op::SIN: return sin(args[0]);
        case Op::COS: return cos(args[0]);
        case Op::EXP: return exp(args[0]);
        case Op::LOG: return log(args[0]);
        case Op::POW: return pow(args[0], args[1]);
    }
    throw runtime_error("ir::evaluate - unsupported operation " + opName(op));
}

} // namespace

double evaluate(const IRFunction& func, const map<string, double>& inputs) {
    unordered_map<int, double> tempValues;
    tempValues.reserve(func.instructions.size() * 2);

    for (const auto& instr : func.instructions) {
        vector<double> args;
        args.reserve(instr.operands.size());
        for (const auto& operand : instr.operands) {
            args.push_back(resolveOperandValue(operand, inputs, tempValues));
        }
        tempValues[instr.resultId] = applyOp(instr.op, args);
    }

    return resolveOperandValue(func.returnValue, inputs, tempValues);
}

} // namespace ir
