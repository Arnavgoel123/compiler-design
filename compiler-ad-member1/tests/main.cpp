#include "ASTPrinter.h"
#include "Frontend.h"
#include "Lexer.h"
#include "Parser.h"
#include "SemanticAnalyzer.h"

#include <cassert>
#include <iostream>

using namespace frontend;

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

    std::cout << "All Member 1 frontend tests passed.\n";
    return 0;
}
