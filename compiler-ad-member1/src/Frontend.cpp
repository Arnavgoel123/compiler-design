#include "Frontend.h"

#include "Lexer.h"
#include "Parser.h"
#include "SemanticAnalyzer.h"

namespace frontend {

std::unique_ptr<ast::Program> parseAndValidate(const std::string& source,
                                               std::vector<SymbolInfo>* symbolsOut) {
    Lexer lexer(source);
    Parser parser(lexer.tokenize());
    auto program = parser.parseProgram();

    SemanticAnalyzer semantic;
    semantic.analyzeOrThrow(*program);
    if (symbolsOut) *symbolsOut = semantic.symbolTable();
    return program;
}

} // namespace frontend
