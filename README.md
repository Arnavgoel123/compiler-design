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
