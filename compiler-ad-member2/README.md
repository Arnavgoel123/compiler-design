# Compiler with Automatic Differentiation - Member 2

Member 2 owns the forward-mode automatic differentiation pass.

## Responsibility

- Tangent/dual propagation over Member 4's structured IR
- Forward derivative IR generation
- Unit tests for numerical correctness and IR validation

## Structure

```
compiler-ad-member2/
|- CMakeLists.txt
|- README.md
|- src/
|  `- ForwardAD.h / ForwardAD.cpp
`- tests/
   `- main.cpp
```

## Integration contract

`forward_ad::forwardDifferentiate` consumes an `ir::IRFunction` produced by
Member 4's AST-to-IR stage and returns a new `ir::IRFunction` whose
`returnValue` is the derivative with respect to the requested parameter.

The returned derivative IR uses the same `ir::Op`, `ir::Operand`, and
`ir::IRBuilder` types as the input. It can therefore flow directly through
Member 4's optimizer, code generator, and numerical validator.

```cpp
ir::IRFunction derivative = forward_ad::forwardDifferentiate(original, "x");
```

The `ad::ForwardAD` class remains as a compatibility wrapper for the earlier
Member 2 API.

## Building and testing

From the repository root:

```bash
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The derivative pass supports `ADD`, `SUB`, `MUL`, `DIV`, `NEG`, `SIN`, `COS`,
`EXP`, `LOG`, and `POW` using standard forward-mode rules.