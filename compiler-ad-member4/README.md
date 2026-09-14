# Compiler with Automatic Differentiation — Member 4 Module

This repository contains **only Member 4's part** of the team project
*"Compiler with Automatic Differentiation."* It is a self-contained,
independently buildable and testable C++17 module. It does **not** contain
the lexer, parser, semantic analyzer, AST (Member 1), Forward AD (Member 2),
or Reverse AD (Member 3) — those are owned by the other members and will be
integrated later.

## Responsibility (Member 4)

- IR design
- AST → IR conversion
- IR infrastructure (builder, printer, reference interpreter)
- IR validation
- Optimization (constant folding, algebraic simplification, CSE)
- Code generation (IR → plain C++)
- Numerical validation (finite-difference checking)
- Integration interfaces for Forward AD / Reverse AD
- Test harness for all of the above

## Project structure

```
compiler-ad-member4/
├── CMakeLists.txt
├── README.md
├── src/
│   ├── ir/
│   │   ├── IR.h / IR.cpp            # structured IR + builder + reference interpreter
│   │   ├── ASTToIR.h / ASTToIR.cpp  # isolated AST shape + AST -> IR converter
│   ├── optimizer/
│   │   ├── Optimizer.h / Optimizer.cpp   # constant folding, algebraic simp., CSE
│   ├── codegen/
│   │   ├── CodeGenerator.h / CodeGenerator.cpp  # IR -> C++ source
│   └── validation/
│       ├── Validator.h / Validator.cpp   # IR structural validation + finite-diff validation
└── tests/
    └── main.cpp                     # standalone test harness (14 test groups, 59 checks)
```

## Architecture / pipeline

```
User Input
   │
   ▼
Member 1: Lexer → Parser → Semantic Analysis → AST
   │
   ▼
MEMBER 4: ASTToIR::convert(...)              <-- src/ir/ASTToIR.*
   │
   ▼
Structured IR  (ir::IRFunction)              <-- src/ir/IR.*
   │
   ▼
Member 2: Forward AD   OR   Member 3: Reverse AD
   │        (both consume ir::IRFunction and
   │         produce a new "derivative" ir::IRFunction)
   ▼
Derivative IR
   │
   ▼
MEMBER 4: optimizer::Optimizer::optimize(...)     <-- src/optimizer/*
   │        (constant folding, algebraic simplification, CSE)
   ▼
Optimized Derivative IR
   │
   ▼
MEMBER 4: codegen::CodeGenerator::generateFunction(...)  <-- src/codegen/*
   │
   ▼
Generated C++ derivative function (plain text, no external AD lib)
   │
   ▼
MEMBER 4: validation::NumericalValidator::validate(...)  <-- src/validation/*
   │        (compares Forward/Reverse derivative vs. finite differences)
   ▼
PASS / FAIL report
```

## The IR (`src/ir/IR.h`, `IR.cpp`)

The IR is a real, structured representation — not strings. Core types:

- `ir::Op` — enum of supported operations: `ADD, SUB, MUL, DIV, NEG, SIN, COS, EXP, LOG, POW`.
- `ir::Operand` — a tagged union of `Constant` (double), `Variable` (name),
  or `Temp` (id of a previous instruction's result).
- `ir::Instruction` — `t<resultId> = OP operand1 [operand2]`.
- `ir::IRFunction` — a name, an ordered list of parameters, an ordered list
  of instructions (SSA-like: each instruction defines exactly one new temp
  and may only reference already-defined values), and a single `returnValue`.
- `ir::IRBuilder` — a small helper for constructing an `IRFunction`
  instruction-by-instruction without manually tracking temp ids. Both the
  AST→IR converter and (eventually) Forward/Reverse AD can use this.
- `ir::evaluate(const IRFunction&, const map<string,double>&)` — a reference
  interpreter that directly evaluates an `IRFunction`. Used by the test
  harness to check IR/optimizer correctness, and to turn any `IRFunction`
  into a `std::function<double(double)>` for the numerical validator.

Printing an `IRFunction` (`IRFunction::toString()`) produces output like:

```
===== IR FUNCTION: y_of_x(x) =====
t1 = MUL x x
t2 = MUL 3 x
t3 = ADD t1 t2
======================================
RETURN t3
```

## AST → IR (`src/ir/ASTToIR.h`, `ASTToIR.cpp`)

Member 1 will eventually hand over the real AST. To avoid depending on his
exact node layout ahead of time, this module defines its own minimal,
conventional AST shape in the `ast` namespace:

- `ast::Number`, `ast::Variable`
- `ast::BinaryExpression` (`ADD, SUB, MUL, DIV, POW`)
- `ast::UnaryExpression` (`NEG, SIN, COS, EXP, LOG`)
- `ast::FunctionCall` (generic `sin(x)`, `cos(x)`, `exp(x)`, `log(x)`, `pow(x,y)` form)
- `ast::Assignment` (`y = <expr>`)

`astToIr::convert(...)` recursively walks this AST and emits structured IR
using `IRBuilder`. Parameters are inferred automatically from every
`ast::Variable` encountered (first-seen order), or can be fixed explicitly.

**This AST definition and its conversion logic are isolated to
`ASTToIR.h`/`ASTToIR.cpp`.** When Member 1 delivers his real AST, only these
two files need to change (either by adapting his node types to this shape,
or rewriting the converter to walk his tree directly). `IR.h/.cpp`, the
optimizer, the code generator, and the validator all operate purely on
`ir::IRFunction` and require **no changes**.

## IR validation (`src/validation/Validator.h/.cpp` → `IRValidator`)

`IRValidator::validate(const IRFunction&)` checks for:

- undefined operands (temp used before it is defined / never defined)
- wrong operand count for an operation (arity mismatch)
- invalid/unrecognized operations
- duplicate temp definitions
- invalid/undeclared variable references
- malformed result ids
- NaN/Inf constants
- invalid `RETURN` (referencing an undefined temp/variable)

It returns an `IRValidationResult` with `valid` and a list of human-readable
`errors`.

## Optimization (`src/optimizer/Optimizer.h/.cpp`)

`Optimizer::optimize(const IRFunction&, OptimizationStats* = nullptr)` runs a
single forward pass implementing three optimizations together:

1. **Constant folding** — e.g. `3 + 5 → 8`, `2 * 4 → 8`. Skips folding when
   it would be unsafe at compile time (division by zero, `log` of a
   non-positive constant), leaving those as ordinary instructions.
2. **Algebraic simplification** — `x + 0 → x`, `x - 0 → x`, `x * 1 → x`,
   `x * 0 → 0`, `x / 1 → x`.
3. **Common Subexpression Elimination (CSE)** — if the same operation over
   the same (already-resolved) operands appears more than once, later
   occurrences reuse the first result instead of recomputing it.

The optimizer preserves mathematical correctness — this is checked directly
in the test harness by evaluating both the original and optimized IR at the
same inputs and asserting the results match.

Print `before.toString()` and `after.toString()` (or use
`OptimizationStats::toString()`) to see IR before/after optimization.

## Code generation (`src/codegen/CodeGenerator.h/.cpp`)

`CodeGenerator::generateFunction(const IRFunction&)` turns **any** IR
function (original or derivative) into a plain C++ function definition,
e.g.:

```cpp
double derivative(double x) {
    double t1 = x * x;
    double t2 = 3 * x;
    double t3 = t1 + t2;
    return t3;
}
```

`CodeGenerator::generateStandaloneSource(...)` additionally prepends
`#include <cmath>` so the output can be written straight to a `.cpp` file.
No external AD or symbolic-math library is required to compile or run the
generated code.

## AD integration interfaces

Members 2 and 3 are expected to write functions shaped like:

```cpp
ir::IRFunction forwardDifferentiate(const ir::IRFunction& original,
                                     const std::string& withRespectTo);

ir::IRFunction reverseDifferentiate(const ir::IRFunction& original,
                                     const std::string& withRespectTo);
```

Both receive the original `IRFunction` (as produced by `astToIr::convert`)
and return a **new** `IRFunction` — the derivative IR — using the exact same
`ir::Op` / `ir::Operand` / `ir::Instruction` / `IRBuilder` types. That
derivative IR then flows, unmodified in shape, into
`Optimizer::optimize(...)` → `CodeGenerator::generateFunction(...)` →
`NumericalValidator::validate(...)`.

`tests/main.cpp` (Test 13) demonstrates this exact pipeline shape using two
hand-built stand-in functions (`fakeForwardAD_square`, `fakeReverseAD_square`)
that emit derivative IR the same way a real Forward/Reverse AD pass would —
proving the interfaces and downstream stages work correctly without
requiring Members 2/3's actual implementations.

## Numerical validation (`src/validation/Validator.h/.cpp` → `NumericalValidator`)

`NumericalValidator::validate(f, forwardDeriv, reverseDeriv, xs, h, tolerance)`
computes, for each `x` in `xs`, the central-difference estimate
`f'(x) ≈ (f(x+h) - f(x-h)) / (2h)` and compares it against the supplied
Forward AD and/or Reverse AD derivative callables (either may be
`std::nullopt` if that member's module isn't wired in yet). It reports, per
`x`: the numerical derivative, each supplied analytic derivative, the
absolute error, the tolerance, and PASS/FAIL — plus an overall PASS/FAIL.

Any `ir::IRFunction` (original, or a derivative from Forward/Reverse AD) can
be wrapped as a validator-compatible callable via `ir::evaluate`:

```cpp
auto asCallable = [&](double x) { return ir::evaluate(someIRFunction, {{"x", x}}); };
```

## Building

Requires a C++17 compiler and CMake 3.15+.

```bash
cmake -S . -B build
cmake --build build
```

This builds a static library `compiler_ad_core` (all of `src/`) and an
executable `member4_tests` (from `tests/main.cpp`), and registers the test
harness with CTest:

```bash
ctest --test-dir build --output-on-failure
```

On Windows with Visual Studio:

```bash
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

### Without CMake (e.g. quick g++ check)

```bash
g++ -std=c++17 -Wall -Wextra -o member4_tests \
    tests/main.cpp \
    src/ir/IR.cpp src/ir/ASTToIR.cpp \
    src/optimizer/Optimizer.cpp \
    src/codegen/CodeGenerator.cpp \
    src/validation/Validator.cpp
./member4_tests
```

## Testing

Run the built executable directly:

```bash
./build/member4_tests      # or build/Release/member4_tests.exe on Windows
```

The harness contains 14 test groups covering:

1. `y = x*x + 3*x` (AST→IR, structural validation, evaluation)
2. `y = 3*x^2 + sin(x)` (POWER + trig)
3. `y = exp(sin(x))` (nested functions)
4. `y = log(x)`
5. `y = x/(x+1)` (nested arithmetic / division)
6. Negative expressions: `y = (-x) * (-x)`
7. Pure constant expressions: `y = 3 + 5*2` (folds to a single constant)
8. Constant folding inside a larger expression: `y = (3+5)*x`
9. Algebraic simplification identities: `x+0, x-0, x*1, x*0, x/1`
10. Common Subexpression Elimination: `y = x*x + x*x`
11. IR validator correctly **rejecting** malformed IR (undefined temp,
    wrong arity, duplicate definition, undeclared variable) and correctly
    **accepting** well-formed IR
12. Code generation for several expressions
13. The full Forward/Reverse AD **integration pipeline** shape, using
    stand-in derivative-IR-producing functions
14. Standalone finite-difference validation, including a deliberately wrong
    derivative to prove failures are detected correctly

All 59 individual checks pass. The process exits with code `0` on full
success and `1` if any check fails, so it is CI/CTest-friendly.

## Final Integration Contract

### What Member 1 needs to give me (AST)

Either of the following works:

- **Option A (preferred, minimal change):** shape his real AST node types to
  match (or be trivially adaptable to) the `ast::*` types in `ASTToIR.h`:
  - `Number { double value; }`
  - `Variable { string name; }`
  - `BinaryExpression { BinaryOp op; Node* left; Node* right; }` with
    `BinaryOp ∈ {ADD, SUB, MUL, DIV, POW}`
  - `UnaryExpression { UnaryOp op; Node* operand; }` with
    `UnaryOp ∈ {NEG, SIN, COS, EXP, LOG}`
  - `FunctionCall { string callee; vector<Node*> args; }` for
    `sin/cos/exp/log/pow` if his parser prefers call-syntax over dedicated
    unary nodes
  - `Assignment { string targetName; Node* value; }`
- **Option B:** give me his real AST header, and I rewrite the body of
  `astToIr::convert`/`convertNode` (in `ASTToIR.cpp` only) to walk his tree
  instead. `IR.h/.cpp`, `Optimizer`, `CodeGenerator`, and `Validator` will
  **not** need to change either way.

### What Member 2 needs to give me (Forward AD)

A function with this shape:

```cpp
ir::IRFunction forwardDifferentiate(const ir::IRFunction& original,
                                     const std::string& withRespectTo);
```

- Input: the original IR produced by `astToIr::convert`, and the name of the
  variable to differentiate with respect to.
- Output: a **new** `ir::IRFunction` (build it with `ir::IRBuilder`) whose
  `returnValue`, when evaluated, equals `d(original.returnValue)/d(withRespectTo)`.
- I will pass this output straight into `Optimizer::optimize(...)`, then
  `CodeGenerator::generateFunction(...)`, then `NumericalValidator::validate(...)`.

### What Member 3 needs to give me (Reverse AD)

Same contract, different name:

```cpp
ir::IRFunction reverseDifferentiate(const ir::IRFunction& original,
                                     const std::string& withRespectTo);
```

Identical input/output shape to Forward AD above — just built via reverse
accumulation internally. Both can be validated against each other and
against finite differences using `NumericalValidator::validate(f,
forwardDeriv, reverseDeriv, xs)`.

## Notes

- `using namespace std;` is used throughout, per project convention.
- No external automatic differentiation or symbolic math library is used
  anywhere in this module.
- The lexer, parser, semantic analyzer, AST, Forward AD, and Reverse AD are
  intentionally **not** implemented here — they belong to Members 1, 2, and 3.
