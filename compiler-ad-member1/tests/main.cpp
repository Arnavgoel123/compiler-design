#include "ASTPrinter.h"
#include "Frontend.h"
#include "Lexer.h"
#include "Parser.h"
#include "SemanticAnalyzer.h"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace frontend;

namespace {

// assert() is compiled out in Release builds; these checks always run.
void expect(bool condition, const char* description) {
    if (!condition) {
        std::cerr << "[FAIL] " << description << '\n';
        std::exit(1);
    }
    std::cout << "[PASS] " << description << '\n';
}

bool analyzes(const std::string& source, SemanticAnalyzer& semantic) {
    Lexer lexer(source);
    Parser parser(lexer.tokenize());
    auto program = parser.parseProgram();
    return semantic.analyze(*program);
}

}  // namespace

int main() {
    {
        Lexer lexer("// inputs\ninput x; y = 3*x^2 + sin(x);");
        const auto tokens = lexer.tokenize();
        assert(tokens.size() > 1);
        assert(tokens[0].type == TokenType::Input);
        assert(tokens[1].type == TokenType::Identifier);
        assert(tokens[2].type == TokenType::Semicolon);
    }

    {
        auto program = parseAndValidate("input x; y = 3*x^2 + sin(x);");
        assert(program->assignment);
        assert(program->assignment->targetName == "y");
        std::cout << printAST(*program) << '\n';
    }

    {
        Lexer lexer("input x; y = unknown(x);");
        Parser parser(lexer.tokenize());
        auto program = parser.parseProgram();
        SemanticAnalyzer semantic;
        assert(!semantic.analyze(*program));
    }

    {
        Lexer lexer("input x; y = pow(x, 2) + log(x);");
        Parser parser(lexer.tokenize());
        auto program = parser.parseProgram();
        SemanticAnalyzer semantic;
        assert(semantic.analyze(*program));
    }

    {
        Lexer lexer("input x, x; y = x;");
        Parser parser(lexer.tokenize());
        auto program = parser.parseProgram();
        SemanticAnalyzer semantic;
        assert(!semantic.analyze(*program));
    }

    {
        Lexer lexer("input x; y = z + x;");
        Parser parser(lexer.tokenize());
        auto program = parser.parseProgram();
        SemanticAnalyzer semantic;
        assert(!semantic.analyze(*program));
    }

    {
        Lexer lexer("input x; y = sin(x, x);");
        Parser parser(lexer.tokenize());
        auto program = parser.parseProgram();
        SemanticAnalyzer semantic;
        assert(!semantic.analyze(*program));
    }

    // ---- Multiple equations / symbol table ----
    {
        std::vector<SymbolInfo> symbols;
        auto program = parseAndValidate(
            "input x, z;\n"
            "a = x * z;\n"
            "y = sin(a) + x;\n"
            "w = y / (1 + a);\n", &symbols);
        expect(program->assignments.size() == 3, "multi-equation program parses 3 assignments");
        expect(program->assignment == program->assignments.back().get(),
               "program->assignment points at the last equation");
        expect(program->assignment->targetName == "w", "last equation is 'w'");
        expect(symbols.size() == 5, "symbol table has 2 inputs + 3 assigned variables");
        expect(symbols[0].name == "x" && symbols[0].kind == ast::SymbolKind::Input,
               "symbol table starts with the inputs");
        expect(symbols[2].name == "a" && symbols[2].kind == ast::SymbolKind::Output,
               "assigned variables follow in definition order");
        expect(printSymbolTable(symbols).find("assigned") != std::string::npos,
               "symbol table printer lists assigned variables");
        expect(printAST(*program).find("Assignment(a)") != std::string::npos &&
               printAST(*program).find("Assignment(w)") != std::string::npos,
               "AST printer prints every equation");
    }

    {
        SemanticAnalyzer semantic;
        expect(analyzes("input x; a = x*x; b = a + x; c = a*b;", semantic),
               "chained equations using earlier results are valid");
    }

    {
        SemanticAnalyzer semantic;
        expect(!analyzes("input x; y = x; y = x + 1;", semantic),
               "assigning the same variable twice is rejected");
        expect(semantic.errors().front().message.find("more than once") != std::string::npos,
               "duplicate assignment error is descriptive");
    }

    {
        SemanticAnalyzer semantic;
        expect(!analyzes("input x; y = y + x;", semantic), "self-referential equation is rejected");
        expect(semantic.errors().front().message.find("own definition") != std::string::npos,
               "self-reference error is descriptive");
    }

    {
        SemanticAnalyzer semantic;
        expect(!analyzes("input x; y = b + x; b = x;", semantic),
               "using a variable before it is assigned is rejected");
    }

    {
        SemanticAnalyzer semantic;
        expect(!analyzes("input x; x = 2;", semantic), "assigning to an input is rejected");
    }

    {
        bool threw = false;
        try {
            Lexer lexer("input x; y = x; input z; w = z;");
            Parser parser(lexer.tokenize());
            parser.parseProgram();
        } catch (const ParserError&) {
            threw = true;
        }
        expect(threw, "'input' after an assignment is a parse error");
    }

    std::cout << "All Member 1 frontend tests passed.\n";
    return 0;
}
