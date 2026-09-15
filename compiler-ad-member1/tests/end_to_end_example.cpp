#include "ASTPrinter.h"
#include "Frontend.h"

#include "../../compiler-ad-member4/src/ir/ASTToIR.h"
#include "../../compiler-ad-member4/src/ir/IR.h"

#include <iostream>

int main() {
    auto program = frontend::parseAndValidate(
        "input x; y = 3*x^2 + sin(x);"
    );

    std::cout << "=== Validated AST ===\n";
    std::cout << frontend::printAST(*program) << '\n';

    ir::IRFunction function = astToIr::convert(
        *program->assignment, "y_of_x"
    );

    std::cout << "=== IR ===\n";
    std::cout << function.toString() << '\n';
    return 0;
}
