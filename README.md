# Compiler with Automatic Differentiation

A modular, lightweight compiler infrastructure written in C++17 that parses a mathematical Domain Specific Language (DSL), transforms Abstract Syntax Trees (ASTs) into an Intermediate Representation (IR), and performs both **Forward-Mode** and **Reverse-Mode Automatic Differentiation (AD)** to statically generate optimized, dependency-free C/C++ target code.

---

## 📌 Project Architecture & Pipeline Flow

The compiler processes input mathematical expressions through a strict 4-stage pipeline, passing structured data sequentially across modules before unified integration:

```text
+-----------------------------------------------------------------------------------+
| Stage 1: Member 1 — Lexer, Parser & Semantic Analysis                             |
| Grammar Definition -> Lexer -> Parser -> AST Construction -> Semantic Checking    |
+-----------------------------------------------------------------------------------+
                                         |
                                         v  [Deliverable: Validated AST]
+-----------------------------------------------------------------------------------+
| Stage 2: Member 4 — IR & Code Generation Infrastructure                           |
| AST-to-IR Translation -> Three-Address Code IR -> Module Interfaces & Codegen     |
+-----------------------------------------------------------------------------------+
                                         |
                                         v  [Deliverable: Working IR + Codegen Framework]
                   +---------------------+---------------------+
                   |                                           |
                   v                                           v
+---------------------------------------+   +---------------------------------------+
| Stage 3: Member 2 — Forward-Mode AD   |   | Stage 4: Member 3 — Reverse-Mode AD   |
| Tangent / Dual Propagation            |   | Computational Graph Construction      |
| Forward Derivative IR Generation      |   | Reverse Pass & Gradient Accumulation  |
+---------------------------------------+   +---------------------------------------+
                   |                                           |
                   v [Forward Derivative IR]                   v [Reverse Derivative IR]
                   +---------------------+---------------------+
                                         |
                                         v
+-----------------------------------------------------------------------------------+
| All Members — Integration, Optimization, Testing & Benchmarking                   |
| AST/IR Optimization Passes -> Target Code Emission -> Automated Validation Suite |
+-----------------------------------------------------------------------------------+

---

## Multiple equations and the Jacobian

The compiler is no longer limited to a single equation. A program is a list of
`input` declarations followed by one or more equations; later equations can use
earlier results, and **every assigned variable is differentiated with respect to
every input** (the full Jacobian), in both forward and reverse mode.

```text
input x, z;
a = x * z;
y = sin(a) + x^2;
w = exp(y) / (1 + z^2);
```

What the pipeline does with it:

1. **Frontend** - parses all equations and builds a **symbol table** (inputs, then
   assigned variables) while checking declare-before-use, single assignment and
   no self-reference.
2. **IR** - each equation becomes its own pruned `IRFunction` (shared sub-expressions
   are computed once, then dead code from unrelated equations is removed).
3. **AD** - Forward and Reverse AD run for every (output, input) pair.
4. **Codegen** - generates the primal functions, all derivative functions and
   `jacobian_forward(...)` / `jacobian_reverse(...)` wrappers.
5. **Validation** - every derivative is checked against finite differences, with the
   other inputs held fixed; sample points outside a function's domain (e.g. `log` of a
   negative) are skipped rather than reported as failures.

### Usage

```bash
cmake -S . -B build
cmake --build build

# program on the command line (optional 2nd argument: 'all' or e.g. x,z)
build/compiler_ad "input x, z; a = x*z; y = sin(a) + x^2;"

# program from a file, and write the generated C++ to disk
build/compiler_ad --file examples/neuron.dsl --emit neuron_ad.cpp

# interactive: type several lines, finish with an empty line
build/compiler_ad

# run every test (module tests + end-to-end examples + invalid-program checks)
ctest --test-dir build --output-on-failure
```

Use a non-Release build type (the default above, or `-DCMAKE_BUILD_TYPE=Debug`) when
running the older module tests: they use `assert`, which Release builds compile out.

`examples/` contains `chain.dsl`, `rosenbrock.dsl`, `neuron.dsl` (a sigmoid neuron with
squared-error loss) and `single.dsl` (the original one-equation case).
