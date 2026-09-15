#include "../src/ForwardAD.h"

#include "../../compiler-ad-member4/src/ir/IR.h"
#include "../../compiler-ad-member4/src/validation/Validator.h"

#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    ir::IRBuilder builder("f");
    builder.addParam("x");
    ir::Operand x = ir::Operand::makeVariable("x");
    ir::Operand square = builder.mul(x, x);
    ir::Operand sine = builder.sin_(x);
    builder.setReturn(builder.add(square, sine));
    ir::IRFunction primal = builder.build();

    ir::IRFunction derivative = forward_ad::forwardDifferentiate(primal, "x");
    assert(validation::IRValidator::validate(derivative).valid);

    double xValue = 0.7;
    double expected = 2.0 * xValue + std::cos(xValue);
    double actual = ir::evaluate(derivative, {{"x", xValue}});
    assert(std::abs(actual - expected) < 1e-9);

    ad::ForwardAD compatibilityApi;
    ir::IRFunction compatibilityDerivative = compatibilityApi.differentiate(primal, "x");
    assert(std::abs(ir::evaluate(compatibilityDerivative, {{"x", xValue}}) - expected) < 1e-9);

    std::cout << "Member 2 Forward AD tests passed.\n";
    return 0;
}
