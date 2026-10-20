#include "driver.hpp"

#include "parser.hpp"
#include "sema.hpp"

namespace minicc {

CompileResult compile(const std::string& source, bool optimize) {
    CompileResult r;
    try {
        r.tokens = lex(source);
        r.ast = parse(r.tokens);
    } catch (const CompileError& e) {
        r.errors.push_back({e.loc, e.what()});
        return r;
    }
    SemaResult s = analyze(*r.ast);
    r.warnings = std::move(s.warnings);
    if (!s.errors.empty()) {
        r.errors = std::move(s.errors);
        return r;
    }
    r.ir = generateIR(*r.ast);
    r.finalIR = r.ir;
    if (optimize) optimizeIR(r.finalIR, &r.stats);
    r.assembly = generateAsm(r.finalIR);
    r.ok = true;
    return r;
}

}  // namespace minicc
