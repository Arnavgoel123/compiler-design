# Automatic Differentiation DSL Grammar

The frontend accepts one or more equations. Input variables are declared with
`input` declarations so semantic scope checking can distinguish parameters from
assigned variables. A later equation may use any variable defined by an earlier
one (single assignment: each name is defined at most once).

```text
program       ::= input_declaration* assignment+ EOF ;

input_declaration
              ::= "input" identifier ("," identifier)* ";" ;

assignment    ::= identifier "=" expression ";" ;

expression    ::= additive ;

additive      ::= multiplicative (("+" | "-") multiplicative)* ;

multiplicative
              ::= unary (("*" | "/") unary)* ;

unary         ::= "-" unary | power ;

power         ::= primary ("^" unary)? ;

primary       ::= number
                | identifier
                | function_call
                | "(" expression ")" ;

function_call ::= function_name "(" argument_list? ")" ;

function_name ::= "sin" | "cos" | "exp" | "log" | "pow"
                | identifier ;

argument_list ::= expression ("," expression)* ;

identifier    ::= letter (letter | digit | "_")* ;

number        ::= digit+ ("." digit*)? ([eE] [+-]? digit+)?
                | "." digit+ ([eE] [+-]? digit+)? ;
```

## Built-in functions

| Function | Arity | Meaning |
|---|---:|---|
| `sin(x)` | 1 | sine |
| `cos(x)` | 1 | cosine |
| `exp(x)` | 1 | exponential |
| `log(x)` | 1 | natural logarithm |
| `pow(x, y)` | 2 | `x^y` |

The `^` operator maps to the same AST `POW` operation as `pow(x, y)`.

## Examples

```text
input x;
y = x*x + 3*x;
```

```text
input x;
y = 3*x^2 + sin(x);
```

```text
input x, z;
y = exp(sin(x)) + pow(z, 2);
```

```text
input x, z;
a = x * z;
y = sin(a) + x^2;
w = exp(y) / (1 + z^2);
```

Semantic rules for multiple equations:

- every variable must be declared (`input`) or assigned **before** it is used;
- a variable can be assigned only once, and cannot reuse the name of an input;
- an equation cannot use its own result (`y = y + 1;` is an error);
- `input` declarations must come before the first equation.

The parser builds the AST, and the semantic analyzer validates declarations,
function names/arity, numeric types, and output/input scope before the AST is
handed to the AST-to-IR stage.
