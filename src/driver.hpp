// driver.hpp - runs the whole pipeline: lex -> parse -> sema -> IR -> optimize -> codegen.
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "codegen.hpp"
#include "ir.hpp"
#include "lexer.hpp"
#include "optimizer.hpp"

namespace minicc {

struct CompileResult {
    bool ok = false;
    std::vector<Diagnostic> errors, warnings;
    std::vector<Token> tokens;
    std::unique_ptr<Program> ast;
    IRFunction ir;       // straight from the IR generator
    IRFunction finalIR;  // after optimization (== ir when optimization is off)
    OptStats stats;
    AsmProgram assembly;
};

CompileResult compile(const std::string& source, bool optimize);

}  // namespace minicc
