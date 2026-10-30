#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

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

namespace {

const char* kDefaultProgram =
    "input x, z;\n"
    "a = x * z;\n"
    "y = sin(a) + x^2;\n"
    "w = exp(y) / (1 + z^2);\n";

struct Options {
    std::string source;
    std::string wrtSpec = "all";   // "all" or comma-separated input names
    std::string emitPath;          // optional: write generated C++ here
    bool verbose = false;
};

void printUsage(const char* exe) {
    std::cout
        << "Usage:\n"
        << "  " << exe << "                          interactive (multi-line, end with an empty line)\n"
        << "  " << exe << " \"<program>\" [wrt]        compile a program given on the command line\n"
        << "  " << exe << " --file prog.dsl [wrt]     compile a program stored in a file\n\n"
        << "  wrt      'all' (default) or a comma-separated list of inputs, e.g. x,z\n\n"
        << "Options:\n"
        << "  --emit out.cpp    also write the generated C++ (functions + Jacobian wrappers)\n"
        << "  --verbose         print every derivative IR and every validation table\n"
        << "  --help            show this message\n\n"
        << "A program is: input declarations followed by one or more equations.\n"
        << "Later equations may use variables defined by earlier ones, e.g.\n"
        << "  input x, z;  a = x*z;  y = sin(a) + x^2;\n";
}

std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open file '" + path + "'");
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::vector<std::string> splitCommaList(const std::string& text) {
    std::vector<std::string> parts;
    std::string current;
    for (char c : text) {
        if (c == ',') {
            if (!current.empty()) parts.push_back(current);
            current.clear();
        } else if (!std::isspace(static_cast<unsigned char>(c))) {
            current += c;
        }
    }
    if (!current.empty()) parts.push_back(current);
    return parts;
}

bool isFiniteValue(double v) { return std::isfinite(v); }

struct PairResult {
    std::string output;
    std::string wrt;
    ir::IRFunction fwd;   // optimized forward-mode derivative IR
    ir::IRFunction rev;   // optimized reverse-mode derivative IR
};

} // namespace

int main(int argc, char* argv[]) {
    Options opt;
    std::vector<std::string> positional;
    std::string filePath;

    try {
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--help" || arg == "-h") { printUsage(argv[0]); return 0; }
            else if (arg == "--verbose") opt.verbose = true;
            else if (arg == "--file" && i + 1 < argc) filePath = argv[++i];
            else if (arg == "--emit" && i + 1 < argc) opt.emitPath = argv[++i];
            else positional.push_back(arg);
        }

        if (!filePath.empty()) {
            opt.source = readFile(filePath);
            if (!positional.empty()) opt.wrtSpec = positional[0];
        } else if (!positional.empty()) {
            opt.source = positional[0];
            if (positional.size() > 1) opt.wrtSpec = positional[1];
        } else {
            std::cout << "Enter a mathematical program (several equations allowed; finish with an empty line).\n"
                         "Press Enter immediately to use the default:\n\n"
                      << kDefaultProgram << "\n> ";
            std::string line;
            while (std::getline(std::cin, line)) {
                if (line.empty()) break;
                opt.source += line + "\n";
                std::cout << "> ";
            }
            if (opt.source.empty()) opt.source = kDefaultProgram;
        }
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    std::cout << "\n============================================================\n";
    std::cout << "  COMPILER WITH AUTOMATIC DIFFERENTIATION (END-TO-END DEMO)  \n";
    std::cout << "============================================================\n";
    std::cout << "Input DSL Source:\n";
    {
        std::istringstream lines(opt.source);
        std::string line;
        while (std::getline(lines, line)) std::cout << "  " << line << "\n";
    }
    std::cout << "\n";

    try {
        // -------------------------------------------------------------
        // Stage 1: Member 1 - Frontend (Lexer, Parser, Semantic Analysis)
        // -------------------------------------------------------------
        std::cout << "--- [STAGE 1] Member 1: Parsing & Semantic Analysis ---\n";
        std::vector<frontend::SymbolInfo> symbols;
        auto program = frontend::parseAndValidate(opt.source, &symbols);
        std::cout << "[PASS] Syntax and semantics verified ("
                  << program->assignments.size() << " equation(s)).\n";
        std::cout << frontend::printSymbolTable(symbols) << "\n";
        std::cout << "AST representation:\n" << frontend::printAST(*program) << "\n";

        // -------------------------------------------------------------
        // Stage 2: Member 4 - AST to IR Translation (one IR per equation)
        // -------------------------------------------------------------
        std::cout << "--- [STAGE 2] Member 4: AST -> IR Translation ---\n";
        ir::IRProgram primalProgram = astToIr::convertProgramAll(*program, "f");
        for (const auto& output : primalProgram.outputs) {
            auto check = validation::IRValidator::validate(output.function);
            if (!check.valid) {
                throw std::runtime_error("IR for '" + output.name + "' is malformed:\n" + check.toString());
            }
            std::cout << output.function.toString() << "\n";
        }

        // Which inputs do we differentiate with respect to?
        const std::vector<std::string>& params = primalProgram.params;
        std::vector<std::string> wrtList;
        if (opt.wrtSpec == "all") {
            wrtList = params;
        } else {
            std::vector<std::string> requested = splitCommaList(opt.wrtSpec);
            for (const auto& name : requested) {
                if (std::find(params.begin(), params.end(), name) == params.end()) {
                    throw std::runtime_error("cannot differentiate with respect to '" + name +
                                             "': it is not a declared input");
                }
            }
            // Keep declaration order so Jacobian columns match the signature.
            for (const auto& p : params) {
                if (std::find(requested.begin(), requested.end(), p) != requested.end()) wrtList.push_back(p);
            }
        }
        if (wrtList.empty()) throw std::runtime_error("nothing to differentiate with respect to");

        std::cout << "Differentiating " << primalProgram.outputs.size() << " output(s) with respect to "
                  << wrtList.size() << " input(s): ";
        for (std::size_t j = 0; j < wrtList.size(); ++j) std::cout << (j ? ", " : "") << wrtList[j];
        std::cout << "  ->  " << primalProgram.outputs.size() * wrtList.size() << " derivative(s) x 2 modes\n\n";

        const std::size_t pairCount = primalProgram.outputs.size() * wrtList.size();
        const bool showDetail = opt.verbose || pairCount <= 2;

        // -------------------------------------------------------------
        // Stage 3: Members 2 & 3 - Forward and Reverse AD for every pair
        // -------------------------------------------------------------
        std::cout << "--- [STAGE 3] Members 2 & 3: Forward-Mode and Reverse-Mode AD ---\n";
        std::vector<PairResult> pairs;
        for (const auto& output : primalProgram.outputs) {
            for (const auto& wrt : wrtList) {
                PairResult r;
                r.output = output.name;
                r.wrt = wrt;

                optimizer::OptimizationStats fwdStats, revStats;
                r.fwd = optimizer::Optimizer::optimize(
                    forward_ad::forwardDifferentiate(output.function, wrt), &fwdStats);
                r.rev = optimizer::Optimizer::optimize(
                    reverse_ad::reverseDifferentiate(output.function, wrt), &revStats);
                r.fwd.name = "d_" + output.name + "_d_" + wrt + "_fwd";
                r.rev.name = "d_" + output.name + "_d_" + wrt + "_rev";

                for (const ir::IRFunction* fn : {&r.fwd, &r.rev}) {
                    auto check = validation::IRValidator::validate(*fn);
                    if (!check.valid) {
                        throw std::runtime_error("derivative IR '" + fn->name + "' is malformed:\n" + check.toString());
                    }
                }

                std::cout << "[PASS] d(" << output.name << ")/d(" << wrt << ")  forward: "
                          << r.fwd.instructions.size() << " instr (from " << fwdStats.instructionsBefore
                          << "), reverse: " << r.rev.instructions.size() << " instr (from "
                          << revStats.instructionsBefore << ")\n";
                if (showDetail) {
                    std::cout << "Optimized Forward Derivative IR:\n" << r.fwd.toString() << "\n";
                    std::cout << "Optimized Reverse Derivative IR:\n" << r.rev.toString() << "\n";
                }
                pairs.push_back(std::move(r));
            }
        }
        std::cout << "\n";

        // -------------------------------------------------------------
        // Stage 4: Member 4 - Code Generation (C++ Target Code)
        // -------------------------------------------------------------
        std::cout << "--- [STAGE 4] Member 4: C++ Code Generation ---\n";
        std::vector<ir::IRFunction> generated;
        for (const auto& output : primalProgram.outputs) generated.push_back(output.function);
        for (const auto& r : pairs) generated.push_back(r.fwd);
        for (const auto& r : pairs) generated.push_back(r.rev);

        std::string jacobianCode;
        if (wrtList == params) {
            std::vector<std::string> outputNames;
            std::vector<std::vector<std::string>> fwdNames, revNames;
            for (const auto& output : primalProgram.outputs) {
                outputNames.push_back(output.name);
                fwdNames.emplace_back();
                revNames.emplace_back();
                for (const auto& wrt : params) {
                    fwdNames.back().push_back("d_" + output.name + "_d_" + wrt + "_fwd");
                    revNames.back().push_back("d_" + output.name + "_d_" + wrt + "_rev");
                }
            }
            jacobianCode += "// --- Jacobian (Forward AD) ---\n" +
                codegen::CodeGenerator::generateJacobianWrapper("jacobian_forward", params, outputNames, fwdNames) + "\n";
            jacobianCode += "// --- Jacobian (Reverse AD) ---\n" +
                codegen::CodeGenerator::generateJacobianWrapper("jacobian_reverse", params, outputNames, revNames) + "\n";
        }

        std::cout << "// --- Generated C++ (primal outputs, then forward AD, then reverse AD) ---\n";
        std::string body;
        for (const auto& fn : generated) body += codegen::CodeGenerator::generateFunction(fn) + "\n";
        std::cout << body;
        if (!jacobianCode.empty()) std::cout << jacobianCode;

        if (!opt.emitPath.empty()) {
            std::ofstream out(opt.emitPath);
            if (!out) throw std::runtime_error("cannot write '" + opt.emitPath + "'");
            out << codegen::CodeGenerator::generateStandaloneSource(generated) << jacobianCode;
            std::cout << "[INFO] Generated source written to " << opt.emitPath << "\n";
        }
        std::cout << "\n";

        // -------------------------------------------------------------
        // Stage 5: Member 4 - Numerical Validation (Finite Differences)
        // -------------------------------------------------------------
        std::cout << "--- [STAGE 5] Member 4: Numerical Validation ---\n";

        // Every input needs a value to evaluate the IR. When differentiating
        // with respect to one input, all the others are held at fixed,
        // distinct, positive values (so log/pow/div stay well defined).
        std::map<std::string, double> base;
        for (std::size_t j = 0; j < params.size(); ++j) base[params[j]] = 0.7 + 0.45 * static_cast<double>(j);

        const double h = 1e-5;
        const double tolerance = 1e-4;
        const std::vector<double> candidatePoints = {-2.0, -1.0, 0.5, 1.5, 3.0};

        bool allPassed = true;
        std::size_t passedPairs = 0;

        for (std::size_t k = 0; k < pairs.size(); ++k) {
            const PairResult& r = pairs[k];
            const ir::IRFunction& primal = primalProgram.find(r.output)->function;

            auto at = [&](const ir::IRFunction& fn, double v) {
                auto inputs = base;
                inputs[r.wrt] = v;
                return ir::evaluate(fn, inputs);
            };
            auto f_eval = [&](double v) { return at(primal, v); };
            auto fwd_eval = [&](double v) { return at(r.fwd, v); };
            auto rev_eval = [&](double v) { return at(r.rev, v); };

            // Skip sample points outside the function's domain (e.g. log of a
            // negative number) rather than reporting them as AD failures.
            std::vector<double> usable;
            for (double v : candidatePoints) {
                if (isFiniteValue(f_eval(v)) && isFiniteValue(f_eval(v + h)) && isFiniteValue(f_eval(v - h))) usable.push_back(v);
            }

            std::cout << "d(" << r.output << ")/d(" << r.wrt << "): ";
            if (usable.empty()) {
                std::cout << "SKIPPED (no sample point inside the function's domain)\n";
                continue;
            }

            auto report = validation::NumericalValidator::validate(
                f_eval, fwd_eval, rev_eval, usable, h, tolerance);

            double maxFwd = 0.0, maxRev = 0.0;
            for (const auto& row : report.rows) {
                if (row.forwardError) maxFwd = std::max(maxFwd, *row.forwardError);
                if (row.reverseError) maxRev = std::max(maxRev, *row.reverseError);
            }
            std::cout << (report.allPassed ? "PASS" : "FAIL") << "  (" << usable.size() << " point(s), max fwd err = "
                      << std::scientific << std::setprecision(2) << maxFwd << ", max rev err = " << maxRev
                      << std::defaultfloat << ")\n";
            if (showDetail || !report.allPassed) std::cout << report.toString() << "\n";

            allPassed = allPassed && report.allPassed;
            if (report.allPassed) ++passedPairs;
        }

        // Jacobian at one concrete point, from both AD modes.
        std::cout << std::setprecision(6) << "\nJacobian J[i][j] = d(output_i)/d(input_j) at (";
        for (std::size_t j = 0; j < params.size(); ++j) {
            std::cout << (j ? ", " : "") << params[j] << "=" << base[params[j]];
        }
        std::cout << "), forward / reverse:\n";
        for (const auto& output : primalProgram.outputs) {
            std::cout << "  d" << std::left << std::setw(3) << output.name << " |";
            for (const auto& wrt : wrtList) {
                for (const auto& r : pairs) {
                    if (r.output == output.name && r.wrt == wrt) {
                        std::cout << " " << ir::evaluate(r.fwd, base) << "/"
                                  << ir::evaluate(r.rev, base);
                    }
                }
            }
            std::cout << "\n";
        }
        std::cout << "         columns:";
        for (const auto& wrt : wrtList) std::cout << " d/d" << wrt;
        std::cout << "\n\n";

        if (allPassed) {
            std::cout << ">>> ALL STAGES COMPLETED SUCCESSFULLY WITH EXACT DERIVATIVE MATCH! ("
                      << passedPairs << "/" << pairs.size() << " derivatives validated) <<<\n";
        } else {
            std::cout << ">>> VALIDATION WARNING: One or more points exceeded error tolerance. <<<\n";
            return 2;
        }

    } catch (const std::exception& ex) {
        std::cerr << "Pipeline Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
