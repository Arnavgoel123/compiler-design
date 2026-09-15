#include <iostream>
#include <string>
#include <vector>
#include <memory>

// Member 1: Frontend
#include "Frontend.h"
#include "ASTPrinter.h"

// Member 4: Core IR, ASTToIR, Optimizer, CodeGen, Validator
#include "ir/IR.h"
#include "ir/ASTToIR.h"
#include "optimizer/Optimizer.h"
#include "codegen/CodeGenerator.h"
#include "validation/Validator.h"

// Member 2: Forward AD
#include "ForwardAD.h"

// Member 3: Reverse AD
#include "reverse_ad/ReverseAD.h"

int main(int argc, char* argv[]) {
    std::string source;
    std::string wrt = "x";

    if (argc > 1) {
        source = argv[1];
        if (argc > 2) {
            wrt = argv[2];
        }
    } else {
        std::cout << "Enter mathematical program (or press Enter for default: 'input x; y = 3*x^2 + sin(x);'):\n> ";
        std::getline(std::cin, source);
        if (source.empty()) {
            source = "input x; y = 3*x^2 + sin(x);";
        }
    }

    std::cout << "\n============================================================\n";
    std::cout << "  COMPILER WITH AUTOMATIC DIFFERENTIATION (END-TO-END DEMO)  \n";
    std::cout << "============================================================\n";
    std::cout << "Input DSL Source:\n  " << source << "\n";
    std::cout << "Differentiating with respect to: '" << wrt << "'\n\n";

    try {
        // -------------------------------------------------------------
        // Stage 1: Member 1 - Frontend (Lexer, Parser, Semantic Analysis)
        // -------------------------------------------------------------
        std::cout << "--- [STAGE 1] Member 1: Parsing & Semantic Analysis ---\n";
        auto program = frontend::parseAndValidate(source);
        std::cout << "[PASS] Syntax and semantics verified.\n";
        std::cout << "AST representation:\n" << frontend::printAST(*program) << "\n";

        // -------------------------------------------------------------
        // Stage 2: Member 4 - AST to IR Translation
        // -------------------------------------------------------------
        std::cout << "--- [STAGE 2] Member 4: AST -> IR Translation ---\n";
        ir::IRFunction primalIR = astToIr::convertProgram(*program, "f");
        std::cout << primalIR.toString() << "\n";

        // -------------------------------------------------------------
        // Stage 3A: Member 2 - Forward-Mode AD
        // -------------------------------------------------------------
        std::cout << "--- [STAGE 3A] Member 2: Forward-Mode AD ---\n";
        ir::IRFunction rawFwdIR = forward_ad::forwardDifferentiate(primalIR, wrt);
        optimizer::OptimizationStats fwdStats;
        ir::IRFunction optFwdIR = optimizer::Optimizer::optimize(rawFwdIR, &fwdStats);
        std::cout << "[PASS] Forward AD pass generated.\n";
        std::cout << "Optimized Forward Derivative IR:\n" << optFwdIR.toString() << "\n";

        // -------------------------------------------------------------
        // Stage 3B: Member 3 - Reverse-Mode AD
        // -------------------------------------------------------------
        std::cout << "--- [STAGE 3B] Member 3: Reverse-Mode AD ---\n";
        ir::IRFunction rawRevIR = reverse_ad::reverseDifferentiate(primalIR, wrt);
        optimizer::OptimizationStats revStats;
        ir::IRFunction optRevIR = optimizer::Optimizer::optimize(rawRevIR, &revStats);
        std::cout << "[PASS] Reverse AD pass generated.\n";
        std::cout << "Optimized Reverse Derivative IR:\n" << optRevIR.toString() << "\n";

        // -------------------------------------------------------------
        // Stage 4: Member 4 - Code Generation (C++ Target Code)
        // -------------------------------------------------------------
        std::cout << "--- [STAGE 4] Member 4: C++ Code Generation ---\n";
        std::cout << "// --- Generated C++ Function (Forward AD) ---\n";
        std::cout << codegen::CodeGenerator::generateFunction(optFwdIR) << "\n\n";

        std::cout << "// --- Generated C++ Function (Reverse AD) ---\n";
        std::cout << codegen::CodeGenerator::generateFunction(optRevIR) << "\n\n";

        // -------------------------------------------------------------
        // Stage 5: Member 4 - Numerical Validation (Finite Differences)
        // -------------------------------------------------------------
        std::cout << "--- [STAGE 5] Member 4: Numerical Validation ---\n";
        auto f_eval = [&](double val) { return ir::evaluate(primalIR, {{wrt, val}}); };
        auto fwd_eval = [&](double val) { return ir::evaluate(optFwdIR, {{wrt, val}}); };
        auto rev_eval = [&](double val) { return ir::evaluate(optRevIR, {{wrt, val}}); };

        std::vector<double> testPoints = {-2.0, -1.0, 0.5, 1.5, 3.0};
        auto report = validation::NumericalValidator::validate(
            f_eval,
            fwd_eval,
            rev_eval,
            testPoints,
            1e-5,
            1e-4
        );
        std::cout << report.toString() << "\n";

        if (report.allPassed) {
            std::cout << "\n>>> ALL STAGES COMPLETED SUCCESSFULLY WITH EXACT DERIVATIVE MATCH! <<<\n";
        } else {
            std::cout << "\n>>> VALIDATION WARNING: One or more points exceeded error tolerance. <<<\n";
        }

    } catch (const std::exception& ex) {
        std::cerr << "Pipeline Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
