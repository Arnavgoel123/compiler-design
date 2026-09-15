# Compiler AD - Member 1 Frontend

This module implements the compiler frontend required for the project:

1. Grammar definition
2. Lexer
3. Parser
4. AST construction
5. Semantic/type/scope checking
6. Validated AST output

## Pipeline

```text
Source code
    |
    v
  Lexer -----> Tokens
    |
    v
  Parser ----> AST
    |
    v
Semantic Analyzer
    |
    v
Validated AST
    |
    v
Member 4 AST -> IR
```

## Source language

Input variables are declared with `input` and the program contains one output
assignment:

```text
input x, z;
y = 3*x^2 + sin(x) + pow(z, 2);
```

Supported operators:

- `+`
- `-`
- `*`
- `/`
- `^`
- unary `-`

Supported functions:

- `sin(x)`
- `cos(x)`
- `exp(x)`
- `log(x)`
- `pow(x, y)`

Single-line comments beginning with `//` are supported.

## Files

- `include/Token.h` - token types and token representation
- `include/Lexer.h`, `src/Lexer.cpp` - lexical analysis
- `include/AST.h` - shared AST definitions used by Member 4
- `include/Parser.h`, `src/Parser.cpp` - recursive-descent parser
- `include/SemanticAnalyzer.h`, `src/SemanticAnalyzer.cpp` - semantic analysis
- `include/Frontend.h`, `src/Frontend.cpp` - complete frontend pipeline
- `include/ASTPrinter.h`, `src/ASTPrinter.cpp` - validated AST pretty-printer
- `grammar.md` - formal grammar
- `tests/main.cpp` - frontend tests

## Using the frontend

```cpp
#include "Frontend.h"
#include "ASTPrinter.h"

int main() {
    auto program = frontend::parseAndValidate(
        "input x; y = 3*x^2 + sin(x);"
    );

    std::cout << frontend::printAST(*program);
}
```

`parseAndValidate()` throws `LexerError`, `ParserError`, or
`SemanticErrorException` if the source is invalid.

## Building

```bash
cmake -S compiler-ad-member1 -B build-member1
cmake --build build-member1
ctest --test-dir build-member1 --output-on-failure
```

## Member 4 integration

`compiler-ad-member4/src/ir/ASTToIR.h` includes this module's `AST.h`, making
Member 1's AST the shared AST contract. The existing Member 4 IR, optimizer,
code generator and validator continue to use their existing `ir::*` API.
