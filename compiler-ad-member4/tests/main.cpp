// tests/main.cpp
// -----------------------------------------------------------------------------
// Standalone test harness for Member 4's module (IR / Optimizer / CodeGen /
// Validation). This is intentionally separate from any "final team app" -
// Member 1's lexer/parser/AST and Members 2/3's AD are NOT required to run
// this harness. Where the pipeline needs an AST, this harness builds one by
// hand using the ast::* node types from ASTToIR.h. Where it needs a
// derivative, it builds a small stand-in IR "as if" Forward/Reverse AD had
// produced it, to prove the optimizer/codegen/validator work on arbitrary
// derivative IR too.
// -----------------------------------------------------------------------------
#include <iostream>
#include <iomanip>
#include <cassert>
#include <memory>
#include <cmath>

#include "../src/ir/IR.h"
#include "../src/ir/ASTToIR.h"
#include "../src/optimizer/Optimizer.h"
#include "../src/codegen/CodeGenerator.h"
#include "../src/validation/Validator.h"

using namespace std;
using namespace ir;

namespace {

int g_testsRun = 0;
int g_testsFailed = 0;

void section(const string& title) {
    cout << "\n============================================================\n";
    cout << title << "\n";
    cout << "============================================================\n";
}

void check(bool condition, const string& description) {
    g_testsRun++;
    if (condition) {
        cout << "  [PASS] " << description << "\n";
    } else {
        g_testsFailed++;
        cout << "  [FAIL] " << description << "\n";
    }
}

void checkNear(double actual, double expected, double tol, const string& description) {
    check(fabs(actual - expected) <= tol,
          description + " (expected " + to_string(expected) + ", got " + to_string(actual) + ")");
}

// ---- small AST-building helpers (mirrors what Member 1's parser would build) ----
unique_ptr<ast::Node> num(double v) { return make_unique<ast::Number>(v); }
unique_ptr<ast::Node> var(const string& n) { return make_unique<ast::Variable>(n); }
unique_ptr<ast::Node> bin(ast::BinaryOp op, unique_ptr<ast::Node> l, unique_ptr<ast::Node> r) {
    return make_unique<ast::BinaryExpression>(op, std::move(l), std::move(r));
}
unique_ptr<ast::Node> un(ast::UnaryOp op, unique_ptr<ast::Node> operand) {
    return make_unique<ast::UnaryExpression>(op, std::move(operand));
}

} // namespace

// =============================================================================
// TEST 1: y = x*x + 3*x
// =============================================================================
void test_basic_polynomial() {
    section("TEST 1: AST -> IR for y = x*x + 3*x");

    // BinaryExpression(ADD, BinaryExpression(MUL, x, x), BinaryExpression(MUL, 3, x))
    auto expr = bin(ast::BinaryOp::ADD,
                     bin(ast::BinaryOp::MUL, var("x"), var("x")),
                     bin(ast::BinaryOp::MUL, num(3), var("x")));
    ast::Assignment assignment("y", std::move(expr));

    IRFunction f = astToIr::convert(assignment, "y_of_x");
    cout << f.toString() << "\n";

    check(f.params.size() == 1 && f.params[0] == "x", "parameter list is [x]");
    check(f.instructions.size() == 3, "produced exactly 3 instructions");
    check(f.instructions[0].op == Op::MUL, "instr 1 is MUL");
    check(f.instructions[1].op == Op::MUL, "instr 2 is MUL");
    check(f.instructions[2].op == Op::ADD, "instr 3 is ADD");

    auto validation = validation::IRValidator::validate(f);
    cout << validation.toString() << "\n";
    check(validation.valid, "generated IR passes structural validation");

    double result = evaluate(f, {{"x", 5.0}});
    checkNear(result, 5.0 * 5.0 + 3.0 * 5.0, 1e-9, "IR evaluates y(5) correctly");
}

// =============================================================================
// TEST 2: y = 3*x^2 + sin(x)
// =============================================================================
void test_power_and_trig() {
    section("TEST 2: AST -> IR for y = 3*x^POW(2) + sin(x)");

    auto expr = bin(ast::BinaryOp::ADD,
                     bin(ast::BinaryOp::MUL, num(3), bin(ast::BinaryOp::POW, var("x"), num(2))),
                     un(ast::UnaryOp::SIN, var("x")));
    ast::Assignment assignment("y", std::move(expr));

    IRFunction f = astToIr::convert(assignment, "y_of_x");
    cout << f.toString() << "\n";

    auto validation = validation::IRValidator::validate(f);
    check(validation.valid, "IR passes structural validation");

    double x = 2.0;
    double expected = 3.0 * pow(x, 2) + sin(x);
    double actual = evaluate(f, {{"x", x}});
    checkNear(actual, expected, 1e-9, "IR evaluates y(2) correctly");
}

// =============================================================================
// TEST 3: y = exp(sin(x))  (nested functions)
// =============================================================================
void test_nested_functions() {
    section("TEST 3: AST -> IR for y = exp(sin(x)) (nested functions)");

    auto expr = un(ast::UnaryOp::EXP, un(ast::UnaryOp::SIN, var("x")));
    ast::Assignment assignment("y", std::move(expr));

    IRFunction f = astToIr::convert(assignment, "y_of_x");
    cout << f.toString() << "\n";

    check(validation::IRValidator::validate(f).valid, "nested-function IR is valid");

    double x = 0.7;
    double expected = exp(sin(x));
    checkNear(evaluate(f, {{"x", x}}), expected, 1e-9, "IR evaluates exp(sin(x)) correctly");

    string code = codegen::CodeGenerator::generateFunction(f);
    cout << "Generated code:\n" << code << "\n";
    check(code.find("exp(") != string::npos && code.find("sin(") != string::npos,
          "generated code contains both exp( and sin(");
}

// =============================================================================
// TEST 4: y = log(x)
// =============================================================================
void test_log() {
    section("TEST 4: AST -> IR for y = log(x)");

    auto expr = un(ast::UnaryOp::LOG, var("x"));
    ast::Assignment assignment("y", std::move(expr));
    IRFunction f = astToIr::convert(assignment, "y_of_x");
    cout << f.toString() << "\n";

    check(validation::IRValidator::validate(f).valid, "log IR is valid");
    checkNear(evaluate(f, {{"x", 4.0}}), log(4.0), 1e-9, "IR evaluates log(4) correctly");
}

// =============================================================================
// TEST 5: y = x / (x + 1)
// =============================================================================
void test_division_nested_arithmetic() {
    section("TEST 5: AST -> IR for y = x / (x + 1)  (nested arithmetic)");

    auto expr = bin(ast::BinaryOp::DIV, var("x"), bin(ast::BinaryOp::ADD, var("x"), num(1)));
    ast::Assignment assignment("y", std::move(expr));
    IRFunction f = astToIr::convert(assignment, "y_of_x");
    cout << f.toString() << "\n";

    check(validation::IRValidator::validate(f).valid, "division IR is valid");
    double x = 3.0;
    checkNear(evaluate(f, {{"x", x}}), x / (x + 1.0), 1e-9, "IR evaluates x/(x+1) correctly");
}

// =============================================================================
// TEST 6: negative expressions - y = -x * -x  (== x*x)
// =============================================================================
void test_negative_expressions() {
    section("TEST 6: negative expressions - y = (-x) * (-x)");

    auto expr = bin(ast::BinaryOp::MUL, un(ast::UnaryOp::NEG, var("x")), un(ast::UnaryOp::NEG, var("x")));
    ast::Assignment assignment("y", std::move(expr));
    IRFunction f = astToIr::convert(assignment, "y_of_x");
    cout << f.toString() << "\n";

    check(validation::IRValidator::validate(f).valid, "negative-expression IR is valid");
    double x = 6.0;
    checkNear(evaluate(f, {{"x", x}}), (-x) * (-x), 1e-9, "IR evaluates (-x)*(-x) correctly");
}

// =============================================================================
// TEST 7: constant expressions - y = 3 + 5 * 2  (no variables at all)
// =============================================================================
void test_constant_expression() {
    section("TEST 7: pure constant expression - y = 3 + 5 * 2");

    auto expr = bin(ast::BinaryOp::ADD, num(3), bin(ast::BinaryOp::MUL, num(5), num(2)));
    ast::Assignment assignment("y", std::move(expr));
    IRFunction f = astToIr::convert(assignment, "const_expr");
    cout << f.toString() << "\n";

    check(f.params.empty(), "no parameters discovered for a constant expression");
    check(validation::IRValidator::validate(f).valid, "constant-expression IR is valid");

    optimizer::OptimizationStats stats;
    IRFunction optimized = optimizer::Optimizer::optimize(f, &stats);
    cout << "BEFORE:\n" << f.toString() << "\n\nAFTER:\n" << optimized.toString() << "\n";
    cout << stats.toString() << "\n";

    check(optimized.instructions.empty(), "fully constant expression folds down to zero instructions");
    check(optimized.returnValue.isConstant() && optimized.returnValue.constValue == 13.0,
          "constant-folded result equals 3 + 5*2 = 13");
}

// =============================================================================
// TEST 8: constant folding on a mixed expression - 3 + 5 stays folded even
// when combined with a variable: y = (3 + 5) * x  ->  y = 8 * x
// =============================================================================
void test_constant_folding_mixed() {
    section("TEST 8: constant folding inside a larger expression - y = (3 + 5) * x");

    auto expr = bin(ast::BinaryOp::MUL, bin(ast::BinaryOp::ADD, num(3), num(5)), var("x"));
    ast::Assignment assignment("y", std::move(expr));
    IRFunction f = astToIr::convert(assignment, "y_of_x");

    IRFunction optimized = optimizer::Optimizer::optimize(f);
    cout << "BEFORE:\n" << f.toString() << "\n\nAFTER:\n" << optimized.toString() << "\n";

    check(optimized.instructions.size() == 1, "(3+5)*x folds down to a single MUL instruction");
    check(optimized.instructions[0].op == Op::MUL, "remaining instruction is MUL");
    bool hasConst8 = false;
    for (auto& o : optimized.instructions[0].operands) {
        if (o.isConstant() && o.constValue == 8.0) hasConst8 = true;
    }
    check(hasConst8, "folded constant 8 appears as an operand");

    checkNear(evaluate(optimized, {{"x", 4.0}}), 32.0, 1e-9, "optimized IR still evaluates correctly (8*4=32)");
    check(evaluate(f, {{"x", 4.0}}) == evaluate(optimized, {{"x", 4.0}}),
          "optimized IR and original IR agree numerically");
}

// =============================================================================
// TEST 9: algebraic simplification - x+0, x*1, x*0, x/1, x-0
// =============================================================================
void test_algebraic_simplification() {
    section("TEST 9: algebraic simplification identities");

    struct Case { unique_ptr<ast::Node> expr; string label; double x; };
    vector<Case> cases;
    cases.push_back({bin(ast::BinaryOp::ADD, var("x"), num(0)), "x + 0", 7.0});
    cases.push_back({bin(ast::BinaryOp::SUB, var("x"), num(0)), "x - 0", 7.0});
    cases.push_back({bin(ast::BinaryOp::MUL, var("x"), num(1)), "x * 1", 7.0});
    cases.push_back({bin(ast::BinaryOp::MUL, var("x"), num(0)), "x * 0", 7.0});
    cases.push_back({bin(ast::BinaryOp::DIV, var("x"), num(1)), "x / 1", 7.0});

    for (auto& c : cases) {
        ast::Assignment assignment("y", std::move(c.expr));
        IRFunction f = astToIr::convert(assignment, "y_of_x");
        IRFunction optimized = optimizer::Optimizer::optimize(f);
        cout << c.label << "  ->  optimized instructions: " << optimized.instructions.size()
             << ", return = " << optimized.returnValue.toString() << "\n";
        check(optimized.instructions.empty(),
              c.label + " simplifies away to zero instructions");
        checkNear(evaluate(optimized, {{"x", c.x}}), evaluate(f, {{"x", c.x}}), 1e-9,
                  c.label + " optimized result matches un-optimized result");
    }
}

// =============================================================================
// TEST 10: Common Subexpression Elimination - t1 = x*x; t2 = x*x (repeated)
// =============================================================================
void test_cse() {
    section("TEST 10: Common Subexpression Elimination - y = x*x + x*x");

    auto expr = bin(ast::BinaryOp::ADD,
                     bin(ast::BinaryOp::MUL, var("x"), var("x")),
                     bin(ast::BinaryOp::MUL, var("x"), var("x")));
    ast::Assignment assignment("y", std::move(expr));
    IRFunction f = astToIr::convert(assignment, "y_of_x");

    optimizer::OptimizationStats stats;
    IRFunction optimized = optimizer::Optimizer::optimize(f, &stats);
    cout << "BEFORE:\n" << f.toString() << "\n\nAFTER:\n" << optimized.toString() << "\n";
    cout << stats.toString() << "\n";

    check(f.instructions.size() == 3, "un-optimized IR has 3 instructions (MUL, MUL, ADD)");
    check(stats.commonSubexpressionsEliminated >= 1, "CSE eliminated at least one duplicate MUL");
    check(optimized.instructions.size() == 2, "optimized IR has 2 instructions (one MUL reused, one ADD)");

    double x = 9.0;
    checkNear(evaluate(optimized, {{"x", x}}), x * x + x * x, 1e-9, "CSE-optimized IR still evaluates correctly");
}

// =============================================================================
// TEST 11: invalid IR is correctly rejected by the validator
// =============================================================================
void test_invalid_ir_detection() {
    section("TEST 11: IR validator correctly rejects malformed IR");

    // Case A: instruction references an undefined temp.
    {
        IRFunction f;
        f.name = "bad_undefined_temp";
        f.params = {"x"};
        Instruction instr;
        instr.resultId = 1;
        instr.op = Op::ADD;
        instr.operands = {Operand::makeVariable("x"), Operand::makeTemp(99)}; // t99 never defined
        f.instructions.push_back(instr);
        f.returnValue = Operand::makeTemp(1);

        auto result = validation::IRValidator::validate(f);
        cout << result.toString() << "\n";
        check(!result.valid, "IR referencing an undefined temp is rejected");
    }

    // Case B: wrong operand count (ADD needs 2, given 1).
    {
        IRFunction f;
        f.name = "bad_arity";
        f.params = {"x"};
        Instruction instr;
        instr.resultId = 1;
        instr.op = Op::ADD;
        instr.operands = {Operand::makeVariable("x")}; // missing 2nd operand
        f.instructions.push_back(instr);
        f.returnValue = Operand::makeTemp(1);

        auto result = validation::IRValidator::validate(f);
        cout << result.toString() << "\n";
        check(!result.valid, "IR with wrong operand arity is rejected");
    }

    // Case C: duplicate temp definition.
    {
        IRFunction f;
        f.name = "bad_duplicate";
        f.params = {"x"};
        Instruction i1; i1.resultId = 1; i1.op = Op::NEG; i1.operands = {Operand::makeVariable("x")};
        Instruction i2; i2.resultId = 1; i2.op = Op::SIN; i2.operands = {Operand::makeVariable("x")}; // reuses t1
        f.instructions = {i1, i2};
        f.returnValue = Operand::makeTemp(1);

        auto result = validation::IRValidator::validate(f);
        cout << result.toString() << "\n";
        check(!result.valid, "IR with a duplicate temp definition is rejected");
    }

    // Case D: undefined variable (not a declared parameter).
    {
        IRFunction f;
        f.name = "bad_undefined_var";
        f.params = {"x"};
        Instruction instr;
        instr.resultId = 1;
        instr.op = Op::ADD;
        instr.operands = {Operand::makeVariable("x"), Operand::makeVariable("y")}; // y not a param
        f.instructions.push_back(instr);
        f.returnValue = Operand::makeTemp(1);

        auto result = validation::IRValidator::validate(f);
        cout << result.toString() << "\n";
        check(!result.valid, "IR referencing an undeclared variable is rejected");
    }

    // Case E: a perfectly valid IR must still pass (sanity check on the validator itself).
    {
        auto expr = bin(ast::BinaryOp::ADD, var("x"), num(1));
        ast::Assignment assignment("y", std::move(expr));
        IRFunction good = astToIr::convert(assignment, "good_fn");
        check(validation::IRValidator::validate(good).valid, "a well-formed IR still passes validation");
    }
}

// =============================================================================
// TEST 12: Code generation produces compilable-looking C++ for several exprs
// =============================================================================
void test_code_generation() {
    section("TEST 12: Code generation for several expressions");

    vector<pair<string, unique_ptr<ast::Node>>> exprs;
    exprs.emplace_back("y = x*x + 3*x",
        bin(ast::BinaryOp::ADD, bin(ast::BinaryOp::MUL, var("x"), var("x")),
                                  bin(ast::BinaryOp::MUL, num(3), var("x"))));
    exprs.emplace_back("y = exp(sin(x))", un(ast::UnaryOp::EXP, un(ast::UnaryOp::SIN, var("x"))));
    exprs.emplace_back("y = log(x)", un(ast::UnaryOp::LOG, var("x")));
    exprs.emplace_back("y = x/(x+1)", bin(ast::BinaryOp::DIV, var("x"), bin(ast::BinaryOp::ADD, var("x"), num(1))));

    for (auto& [label, exprNode] : exprs) {
        ast::Assignment assignment("y", std::move(exprNode));
        IRFunction f = astToIr::convert(assignment, "generated_fn");
        IRFunction optimized = optimizer::Optimizer::optimize(f);
        string code = codegen::CodeGenerator::generateStandaloneSource(optimized);
        cout << "----- " << label << " -----\n" << code << "\n";
        check(code.find("double generated_fn(") != string::npos,
              label + ": generated code declares the function with correct name");
        check(code.find("return") != string::npos, label + ": generated code contains a return statement");
    }
}

// =============================================================================
// TEST 13: AD integration interfaces - simulate a Forward AD & Reverse AD
// pass producing derivative IR, then run it through Optimizer/CodeGen/
// Validator exactly like the real Member 2 / Member 3 modules eventually
// will. This proves the pipeline shape without implementing Forward/Reverse
// AD (which are NOT Member 4's responsibility).
// =============================================================================

// Stand-in "Forward AD" for y = x*x  ->  dy/dx = 2*x. (Hand-built IR, exactly
// the shape a real forward-mode implementation would emit: it receives the
// original IR and returns a brand new derivative IRFunction.)
IRFunction fakeForwardAD_square(const IRFunction& /*original*/, const string& wrt) {
    IRBuilder b("d_square_dx");
    b.addParam(wrt);
    Operand two = Operand::makeConstant(2.0);
    Operand result = b.mul(two, Operand::makeVariable(wrt));
    b.setReturn(result);
    return b.build();
}

// Stand-in "Reverse AD" - same mathematical result via a different (slightly
// less simplified) IR shape, to prove the optimizer normalizes it too.
IRFunction fakeReverseAD_square(const IRFunction& /*original*/, const string& wrt) {
    IRBuilder b("d_square_dx_rev");
    b.addParam(wrt);
    // (x + x) instead of (2*x) - different derivation path, same value.
    Operand result = b.add(Operand::makeVariable(wrt), Operand::makeVariable(wrt));
    b.setReturn(result);
    return b.build();
}

void test_ad_integration_pipeline() {
    section("TEST 13: AD integration pipeline (Forward/Reverse IR -> Optimizer -> CodeGen -> Validator)");

    auto expr = bin(ast::BinaryOp::MUL, var("x"), var("x"));
    ast::Assignment assignment("y", std::move(expr));
    IRFunction original = astToIr::convert(assignment, "square");
    cout << "Original IR:\n" << original.toString() << "\n";

    IRFunction fwdDeriv = fakeForwardAD_square(original, "x");
    IRFunction revDeriv = fakeReverseAD_square(original, "x");

    IRFunction fwdOptimized = optimizer::Optimizer::optimize(fwdDeriv);
    IRFunction revOptimized = optimizer::Optimizer::optimize(revDeriv);

    cout << "Forward derivative IR (optimized):\n" << fwdOptimized.toString() << "\n";
    cout << "Reverse derivative IR (optimized):\n" << revOptimized.toString() << "\n";

    check(validation::IRValidator::validate(fwdOptimized).valid, "optimized forward-derivative IR is valid");
    check(validation::IRValidator::validate(revOptimized).valid, "optimized reverse-derivative IR is valid");

    string fwdCode = codegen::CodeGenerator::generateFunction(fwdOptimized);
    string revCode = codegen::CodeGenerator::generateFunction(revOptimized);
    cout << "Forward derivative code:\n" << fwdCode << "\nReverse derivative code:\n" << revCode << "\n";

    // Wrap IR-driven functions as ScalarFunction for the NumericalValidator.
    auto originalFn = [&](double x) { return evaluate(original, {{"x", x}}); };
    auto forwardFn  = [&](double x) { return evaluate(fwdOptimized, {{"x", x}}); };
    auto reverseFn  = [&](double x) { return evaluate(revOptimized, {{"x", x}}); };

    auto report = validation::NumericalValidator::validate(
        originalFn, optional<validation::ScalarFunction>(forwardFn),
        optional<validation::ScalarFunction>(reverseFn),
        {1.0, 2.0, 3.5, -2.0, 0.5});

    cout << report.toString() << "\n";
    check(report.allPassed, "finite-difference validation passes for both forward and reverse derivative IR");
}

// =============================================================================
// TEST 14: Numerical validator standalone (no AD at all - just f and its
// known analytic derivative), across multiple x, including a deliberately
// wrong derivative to prove the validator can FAIL correctly.
// =============================================================================
void test_numerical_validator_standalone() {
    section("TEST 14: NumericalValidator - correct derivative PASSES, wrong derivative FAILS");

    auto f = [](double x) { return sin(x) * exp(x); };
    auto correctDeriv = [](double x) { return cos(x) * exp(x) + sin(x) * exp(x); };
    auto wrongDeriv = [](double x) { return cos(x); }; // deliberately incorrect

    vector<double> xs = {0.1, 0.5, 1.0, 1.5, 2.0};

    auto goodReport = validation::NumericalValidator::validate(
        f, optional<validation::ScalarFunction>(correctDeriv), nullopt, xs);
    cout << goodReport.toString() << "\n";
    check(goodReport.allPassed, "correct analytic derivative passes finite-difference check");

    auto badReport = validation::NumericalValidator::validate(
        f, optional<validation::ScalarFunction>(wrongDeriv), nullopt, xs);
    cout << badReport.toString() << "\n";
    check(!badReport.allPassed, "deliberately wrong derivative correctly FAILS finite-difference check");
}

int main() {
    cout << fixed << setprecision(6);

    test_basic_polynomial();
    test_power_and_trig();
    test_nested_functions();
    test_log();
    test_division_nested_arithmetic();
    test_negative_expressions();
    test_constant_expression();
    test_constant_folding_mixed();
    test_algebraic_simplification();
    test_cse();
    test_invalid_ir_detection();
    test_code_generation();
    test_ad_integration_pipeline();
    test_numerical_validator_standalone();

    cout << "\n============================================================\n";
    cout << "TOTAL: " << g_testsRun << " checks run, " << g_testsFailed << " failed.\n";
    cout << "============================================================\n";

    return g_testsFailed == 0 ? 0 : 1;
}
