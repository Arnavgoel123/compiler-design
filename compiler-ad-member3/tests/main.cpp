#include "../src/reverse_ad/ReverseAD.h"
#include "../../compiler-ad-member4/src/ir/ASTToIR.h"
#include "../../compiler-ad-member4/src/optimizer/Optimizer.h"
#include "../../compiler-ad-member4/src/codegen/CodeGenerator.h"
#include "../../compiler-ad-member4/src/validation/Validator.h"

#include <iostream>
#include <cassert>
#include <cmath>

using namespace std;
using namespace ir;
using namespace ast;
using namespace reverse_ad;
using namespace optimizer;
using namespace codegen;
using namespace validation;

// Helper to quickly define tests without boilerplate
static int g_testsRun = 0;
static int g_testsPassed = 0;

#define ASSERT_TRUE(condition, message) \
    do { \
        g_testsRun++; \
        if (!(condition)) { \
            cerr << "FAIL: " << message << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
        } else { \
            g_testsPassed++; \
            cout << "PASS: " << message << "\n"; \
        } \
    } while (0)

unique_ptr<Node> num(double v) { return make_unique<Number>(v); }
unique_ptr<Node> var(const string& n) { return make_unique<Variable>(n); }
unique_ptr<Node> bin(BinaryOp op, unique_ptr<Node> l, unique_ptr<Node> r) {
    return make_unique<BinaryExpression>(op, std::move(l), std::move(r));
}
unique_ptr<Node> un(UnaryOp op, unique_ptr<Node> v) {
    return make_unique<UnaryExpression>(op, std::move(v));
}

void runReverseADTest(const string& testName, Assignment& astAssig, const string& wrt, const vector<double>& testPoints) {
    cout << "\n=========================================\n";
    cout << "Testing: " << testName << "\n";
    cout << "=========================================\n";

    // 1. Convert AST to original IR
    IRFunction originalIR = astToIr::convert(astAssig, "f");
    
    // 2. Reverse AD Pass
    IRFunction rawDerivIR = reverseDifferentiate(originalIR, wrt);
    
    // 3. Optimize the generated derivative IR
    OptimizationStats stats;
    IRFunction optDerivIR = Optimizer::optimize(rawDerivIR, &stats);
    
    cout << "Optimized Reverse AD IR:\n" << optDerivIR.toString() << "\n";
    
    // 4. Verify Structural Validity
    IRValidationResult valResult = IRValidator::validate(optDerivIR);
    if (!valResult.valid) {
        for (const auto& err : valResult.errors) {
            cerr << "IR Validation Error: " << err << "\n";
        }
    }
    ASSERT_TRUE(valResult.valid, "Optimized Reverse AD IR is structurally valid");

    // 5. Numerical Validation vs Central Differences
    auto f_callable = [&](double x) { return ir::evaluate(originalIR, {{wrt, x}}); };
    auto rev_callable = [&](double x) { return ir::evaluate(optDerivIR, {{wrt, x}}); };
    
    NumericalValidationReport report = NumericalValidator::validate(
        f_callable,
        nullopt,          // No Forward AD to compare against yet
        rev_callable,
        testPoints,
        1e-5,             // step size
        1e-4              // tolerance
    );
    
    cout << report.toString() << "\n";
    ASSERT_TRUE(report.allPassed, "Numerical Validation passes within tolerance");
    
    // Print C++ Code (optional check)
    cout << "Generated C++ Code:\n" << CodeGenerator::generateFunction(optDerivIR) << "\n";
}

void test_square() {
    // y = x * x
    auto expr = bin(BinaryOp::MUL, var("x"), var("x"));
    Assignment assig("y", std::move(expr));
    runReverseADTest("y = x^2", assig, "x", {-2.0, 0.0, 1.5, 3.0});
}

void test_trig_exp() {
    // y = sin(x) * exp(x)
    auto expr = bin(BinaryOp::MUL,
                    un(UnaryOp::SIN, var("x")),
                    un(UnaryOp::EXP, var("x")));
    Assignment assig("y", std::move(expr));
    runReverseADTest("y = sin(x) * exp(x)", assig, "x", {-1.0, 0.0, 1.0, 3.1415});
}

void test_nested_power() {
    // y = (x + 3)^3
    auto add3 = bin(BinaryOp::ADD, var("x"), num(3.0));
    auto expr = bin(BinaryOp::POW, std::move(add3), num(3.0));
    Assignment assig("y", std::move(expr));
    runReverseADTest("y = (x + 3)^3", assig, "x", {-2.0, -1.0, 0.0, 2.0});
}

void test_division_log() {
    // y = log(x) / (x + 1)
    auto den = bin(BinaryOp::ADD, var("x"), num(1.0));
    auto expr = bin(BinaryOp::DIV, un(UnaryOp::LOG, var("x")), std::move(den));
    Assignment assig("y", std::move(expr));
    runReverseADTest("y = log(x) / (x + 1)", assig, "x", {0.5, 1.0, 2.0, 5.0});
}

void test_multivariable_gradient() {
    // y = x^2 * y + sin(y)
    // dy/dx = 2 * x * y
    // dy/dy = x^2 + cos(y)
    cout << "\n=========================================\n";
    cout << "Testing: y = x^2 * y + sin(y) (Multi-variable Gradient)\n";
    cout << "=========================================\n";

    auto x2 = bin(BinaryOp::MUL, var("x"), var("x"));
    auto x2_y = bin(BinaryOp::MUL, std::move(x2), var("y"));
    auto sin_y = un(UnaryOp::SIN, var("y"));
    auto expr = bin(BinaryOp::ADD, std::move(x2_y), std::move(sin_y));
    Assignment assig("z", std::move(expr));

    IRFunction originalIR = astToIr::convert(assig, "f");
    auto grad = reverseGradient(originalIR);

    ASSERT_TRUE(grad.find("x") != grad.end(), "Gradient contains df/dx");
    ASSERT_TRUE(grad.find("y") != grad.end(), "Gradient contains df/dy");

    IRFunction optDx = Optimizer::optimize(grad["x"]);
    IRFunction optDy = Optimizer::optimize(grad["y"]);

    // Numerical checks at (x=2.0, y=3.0)
    map<string, double> pt = {{"x", 2.0}, {"y", 3.0}};
    double val_dx = ir::evaluate(optDx, pt);
    double val_dy = ir::evaluate(optDy, pt);

    // Exact: df/dx = 2 * 2 * 3 = 12
    // Exact: df/dy = 2^2 + cos(3) = 4 + cos(3) = ~3.0100075
    double expected_dx = 12.0;
    double expected_dy = 4.0 + cos(3.0);

    ASSERT_TRUE(abs(val_dx - expected_dx) < 1e-4, "Exact df/dx evaluates to 12.0");
    ASSERT_TRUE(abs(val_dy - expected_dy) < 1e-4, "Exact df/dy evaluates to 4.0 + cos(3.0)");
}

void test_shared_subexpression_fanout() {
    // y = (x + 1) * (x + 1) + sin(x + 1)
    // Tests that adjoint accumulation correctly sums contributions from multiple fan-outs
    cout << "\n=========================================\n";
    cout << "Testing: Shared subexpression fanout y = (x+1)^2 + sin(x+1)\n";
    cout << "=========================================\n";

    auto t1 = bin(BinaryOp::ADD, var("x"), num(1.0));
    auto t2 = bin(BinaryOp::ADD, var("x"), num(1.0));
    auto sq = bin(BinaryOp::MUL, std::move(t1), std::move(t2));
    auto s = un(UnaryOp::SIN, bin(BinaryOp::ADD, var("x"), num(1.0)));
    auto expr = bin(BinaryOp::ADD, std::move(sq), std::move(s));
    Assignment assig("y", std::move(expr));

    runReverseADTest("y = (x + 1)^2 + sin(x + 1)", assig, "x", {-2.0, -1.0, 0.0, 1.5});
}

void test_dead_subexpression_pruning() {
    // Manually create an IR with a dead instruction that doesn't feed the return value
    cout << "\n=========================================\n";
    cout << "Testing: Dead instruction pruning in Reverse AD\n";
    cout << "=========================================\n";

    IRBuilder builder("f_dead");
    builder.addParam("x");
    Operand deadOp = builder.sin_(Operand::makeVariable("x")); // Never used
    (void)deadOp;
    Operand useful = builder.mul(Operand::makeVariable("x"), Operand::makeConstant(3.0)); // 3 * x
    builder.setReturn(useful);

    IRFunction orig = builder.build();
    IRFunction deriv = reverseDifferentiate(orig, "x");
    IRFunction opt = Optimizer::optimize(deriv);

    // Evaluating df/dx at x=5 should be 3.0
    double res = ir::evaluate(opt, {{"x", 5.0}});
    ASSERT_TRUE(abs(res - 3.0) < 1e-5, "Dead node does not affect derivative");
}

int main() {
    cout << "Running Member 3 Reverse AD Tests...\n";
    
    test_square();
    test_trig_exp();
    test_nested_power();
    test_division_log();
    test_multivariable_gradient();
    test_shared_subexpression_fanout();
    test_dead_subexpression_pruning();
    
    cout << "\n=========================================\n";
    cout << "Test Summary: " << g_testsPassed << " / " << g_testsRun << " passed.\n";
    
    return (g_testsPassed == g_testsRun) ? 0 : 1;
}

