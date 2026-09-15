#include "ForwardAD.h"
#include <iostream>
#include <cassert>

int main() {
    std::cout << "=== Running Member 2 Forward AD Unit Tests ===" << std::endl;

    // Build primal IR for f(x) = x * x + sin(x)
    ir::IRFunction func("test_func");
    func.addParameter("x");

    func.addInstruction(ir::Instruction(ir::Opcode::MUL, "t0", "x", "x"));
    func.addInstruction(ir::Instruction(ir::Opcode::SIN, "t1", "x"));
    func.addInstruction(ir::Instruction(ir::Opcode::ADD, "t2", "t0", "t1"));
    func.addInstruction(ir::Instruction(ir::Opcode::RETURN, "t2"));

    // Run Forward AD pass w.r.t 'x'
    ad::ForwardAD forwardEngine;
    ir::IRFunction forwardIR = forwardEngine.differentiate(func, "x");

    std::cout << "Original IR Instruction Count: " << func.getInstructions().size() << std::endl;
    std::cout << "Forward Derivative IR Instruction Count: " << forwardIR.getInstructions().size() << std::endl;

    // Verification: Derivative IR should be larger due to injected dual calculations
    assert(forwardIR.getInstructions().size() > func.getInstructions().size());
    std::cout << "✔ Forward AD Pass Executed Successfully!" << std::endl;

    return 0;
}
//feat: add ForwardADTest.cpp
