// Self-contained regression tests (no external framework). Run: ctest, or ./minicc_tests
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "ast.hpp"
#include "driver.hpp"
#include "lexer.hpp"

using namespace minicc;

static int g_checks = 0, g_failures = 0;

#define CHECK(cond)                                                                      \
    do {                                                                                 \
        ++g_checks;                                                                      \
        if (!(cond)) {                                                                   \
            ++g_failures;                                                                \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << ": " #cond << '\n';  \
        }                                                                                \
    } while (0)

#define CHECK_MSG(cond, msg)                                                             \
    do {                                                                                 \
        ++g_checks;                                                                      \
        if (!(cond)) {                                                                   \
            ++g_failures;                                                                \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << ": " #cond << "\n  " << msg << '\n'; \
        }                                                                                \
    } while (0)

// Compile and run; returns false (with `err` set) on compile or runtime error.
static bool runProgram(const std::string& src, bool opt, int& value, std::string& err) {
    CompileResult r = compile(src, opt);
    if (!r.ok) { err = "compile error: " + r.errors[0].message; return false; }
    RunResult rr = run(r.assembly);
    if (!rr.ok) { err = "runtime error: " + rr.error; return false; }
    value = rr.value;
    return true;
}

// Program must produce `expected` both without and with optimization.
static void expectValue(const std::string& name, const std::string& src, int expected) {
    for (bool opt : {false, true}) {
        int v = 0;
        std::string err;
        bool ok = runProgram(src, opt, v, err);
        CHECK_MSG(ok && v == expected, name << (opt ? " [-O1]" : " [-O0]") << ": expected " << expected
                                            << ", got " << (ok ? std::to_string(v) : err));
    }
}

static void expectCompileError(const std::string& name, const std::string& src, const std::string& fragment) {
    CompileResult r = compile(src, true);
    bool found = false;
    for (auto& d : r.errors) found |= d.message.find(fragment) != std::string::npos;
    CHECK_MSG(!r.ok && found, name << ": expected error containing '" << fragment << "', got "
                                   << (r.errors.empty() ? "no error" : r.errors[0].message));
}

static void expectRuntimeError(const std::string& name, const std::string& src, const std::string& fragment) {
    for (bool opt : {false, true}) {
        int v;
        std::string err;
        bool ok = runProgram(src, opt, v, err);
        CHECK_MSG(!ok && err.find(fragment) != std::string::npos, name << ": expected runtime error '" << fragment << "', got " << err);
    }
}

static std::string optIR(const std::string& src) { return toString(compile(src, true).finalIR); }
static std::string rawIR(const std::string& src) { return toString(compile(src, false).ir); }

// ---------------------------------------------------------------------------
static void testLexer() {
    auto toks = lex("int x = 10; // comment\n/* block */ x <= 3 && y != 4 || !z");
    std::vector<Tok> kinds;
    for (auto& t : toks) kinds.push_back(t.kind);
    std::vector<Tok> expected = {Tok::KwInt, Tok::Ident, Tok::Assign, Tok::Number, Tok::Semi, Tok::Ident,
                                 Tok::Le, Tok::Number, Tok::AndAnd, Tok::Ident, Tok::Ne, Tok::Number,
                                 Tok::OrOr, Tok::Not, Tok::Ident, Tok::End};
    CHECK(kinds == expected);
    CHECK(toks[3].value == 10);
    CHECK(toks[5].loc.line == 2);  // line tracking across comments

    for (const char* bad : {"int x = @;", "int x = 99999999999;", "int x = 1 & 2;", "/* never closed", "int x = 12ab;"}) {
        bool threw = false;
        try { lex(bad); } catch (const CompileError&) { threw = true; }
        CHECK_MSG(threw, "lexer should reject: " << bad);
    }
}

static void testParserPrecedence() {
    CompileResult r = compile("int main() { return 1 + 2 * 3; }", false);
    CHECK(r.ok);
    CHECK(dump(*r.ast).find("      Binary +\n        Number 1\n        Binary *\n          Number 2\n          Number 3") != std::string::npos);

    expectValue("mul-before-add", "int main() { return 1 + 2 * 3; }", 7);
    expectValue("parens", "int main() { return (1 + 2) * 3; }", 9);
    expectValue("left-assoc-sub", "int main() { return 10 - 3 - 2; }", 5);
    expectValue("left-assoc-div", "int main() { return 100 / 10 / 5; }", 2);
    expectValue("unary-minus", "int main() { return -2 * 3; }", -6);
    expectValue("not", "int main() { return !0 + !5; }", 1);
    expectValue("rel-vs-eq", "int main() { return 1 < 2 == 1; }", 1);
    expectValue("arith-vs-rel", "int main() { return 2 + 3 < 4 + 5; }", 1);
    expectValue("and-before-or", "int main() { return 1 || 0 && 0; }", 1);
    expectValue("rem", "int main() { return 17 % 5; }", 2);
    expectValue("neg-div-trunc", "int main() { return -7 / 2; }", -3);
    expectValue("int-wraparound", "int main() { int x = 2147483647; return x + 1 < 0; }", 1);
}

static void testSyntaxErrors() {
    expectCompileError("missing-semicolon", "int main() { int x = 1 return x; }", "expected ';'");
    expectCompileError("missing-rparen", "int main() { return (1 + 2; }", "expected ')'");
    expectCompileError("missing-brace", "int main() { return 1;", "expected '}'");
    expectCompileError("bad-statement", "int main() { 5; }", "expected a statement");
    expectCompileError("missing-expr", "int main() { return ; }", "expected an expression");
    expectCompileError("decl-as-if-body", "int main() { if (1) int x = 1; return 0; }", "wrap it");
    expectCompileError("trailing-garbage", "int main() { return 0; } int", "after the end of function");
    expectCompileError("lex-error-propagates", "int main() { return 1 @ 2; }", "unexpected character");
}

static void testSemantics() {
    expectCompileError("undeclared-use", "int main() { return y; }", "undeclared variable 'y'");
    expectCompileError("undeclared-assign", "int main() { y = 1; return 0; }", "undeclared variable 'y'");
    expectCompileError("redeclaration", "int main() { int x = 1; int x = 2; return x; }", "redeclaration of 'x'");
    expectCompileError("self-init", "int main() { int x = x; return x; }", "undeclared variable 'x'");
    expectCompileError("scope-ends", "int main() { { int a = 1; } return a; }", "undeclared variable 'a'");
    expectCompileError("div-zero-literal", "int main() { return 1 / 0; }", "division by zero");
    expectCompileError("rem-zero-literal", "int main() { return 1 % 0; }", "remainder by zero");
    expectCompileError("not-main", "int foo() { return 0; }", "must define 'int main()'");

    CompileResult many = compile("int main() { return a + b; }", true);
    CHECK(many.errors.size() == 2);  // semantic errors are collected, not just the first

    expectValue("shadowing", "int main() { int x = 5; int y = 0; { int x = 10; y = x; } return x + y; }", 15);
    expectValue("inner-assign-outer", "int main() { int x = 1; { x = 2; } return x; }", 2);
    expectValue("zero-initialized", "int main() { int x; return x; }", 0);
    expectValue("temp-name-collision", "int main() { int t1 = 4; int t2 = t1 * 2 + 1; return t2 + t1; }", 13);

    CompileResult w = compile("int main() { int x = 1; }", true);
    CHECK(w.ok && w.warnings.size() == 1);
    expectValue("implicit-return-0", "int main() { int x = 1; }", 0);
}

static void testIR() {
    const char* prog = "int main() { int x = 10; int y = 20; int result = x + y * 2; return result; }";
    CHECK(rawIR(prog) ==
          "    x = 10\n    y = 20\n    t1 = y * 2\n    t2 = x + t1\n    result = t2\n    return result\n    return 0\n");
    CHECK(optIR(prog) == "    return 50\n");

    CHECK(rawIR("int main() { int a = 1; if (a) { a = 2; } return a; }").find("ifz a goto L1") != std::string::npos);
}

static void testOptimizer() {
    CHECK(optIR("int main() { int a = 5; int b = a * 2; return 3; }") == "    return 3\n");                 // dead code
    CHECK(optIR("int main() { if (1) { return 1; } else { return 2; } }") == "    return 1\n");             // branch folding
    CHECK(optIR("int main() { if (0) { return 1; } return 2; }") == "    return 2\n");
    CHECK(optIR("int main() { int x = 7; return x + 0; }") == "    return 7\n");
    CHECK(optIR("int main() { int x = 3; int y = x; int z = y * 1; return z + 2; }") == "    return 5\n");  // copy prop
    CHECK(optIR("int main() { int x = 1; while (0) { x = 2; } return x; }") == "    return 1\n");          // dead loop
    CHECK(optIR("int main() { return -(2 + 3) * 4; }") == "    return -20\n");

    // Loops must NOT be folded away: values change across iterations.
    std::string loop = optIR("int main() { int s = 0; int i = 1; while (i <= 10) { s = s + i; i = i + 1; } return s; }");
    CHECK(loop.find("goto") != std::string::npos);
    CHECK(loop.find("return 55") == std::string::npos);

    // Division that may trap is never deleted...
    CHECK(optIR("int main() { int z = 0; int u = 5 / z; return 1; }").find("/") != std::string::npos);
    // ...but a division by a non-zero constant is removed when unused.
    CHECK(optIR("int main() { int z = 4; int u = 5 / z; return 1; }") == "    return 1\n");

    CompileResult r = compile("int main() { int a = 2 * 3; int b = a + 0; return b; }", true);
    CHECK(r.stats.folded > 0 && r.stats.propagated > 0);

    // The code generator is independent of the optimizer: -O0 keeps everything.
    CHECK(compile("int main() { return 1 + 2; }", false).assembly.code.size() >
          compile("int main() { return 1 + 2; }", true).assembly.code.size());
}

static void testCodegen() {
    CHECK(toString(compile("int main() { return 1 + 2; }", true).assembly).find("    ret   3\n") != std::string::npos);
    // At -O0 a constant first operand is materialized into a scratch register.
    std::string o0 = toString(compile("int main() { int x = 4; return 5 - x; }", false).assembly);
    CHECK(o0.find("mov") != std::string::npos && o0.find("sub") != std::string::npos);

    expectValue("short-circuit-and", "int main() { int z = 0; return z && (10 / z); }", 0);
    expectValue("short-circuit-or", "int main() { int z = 0; return 1 || (10 / z); }", 1);
    expectRuntimeError("runtime-div-zero", "int main() { int z = 0; return 10 / z; }", "division by zero");
    expectRuntimeError("infinite-loop", "int main() { while (1) { } return 0; }", "step limit");
}

static void testExamples() {
    namespace fs = std::filesystem;
    int count = 0;
    for (auto& e : fs::directory_iterator(MINICC_EXAMPLES_DIR)) {
        if (e.path().extension() != ".mc") continue;
        std::ifstream in(e.path());
        std::stringstream ss;
        ss << in.rdbuf();
        std::string src = ss.str();
        auto pos = src.find("// expect:");
        CHECK_MSG(pos != std::string::npos, e.path().filename().string() << " has no '// expect:' header");
        if (pos == std::string::npos) continue;
        expectValue(e.path().filename().string(), src, std::stoi(src.substr(pos + 10)));
        ++count;
    }
    CHECK_MSG(count >= 5, "expected example programs in " << MINICC_EXAMPLES_DIR);
}

// ---------------------------------------------------------------------------
// Differential testing: random programs must give identical results with and
// without optimization. This is how optimizer bugs get caught.
// ---------------------------------------------------------------------------
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed) {}
    unsigned next(unsigned n) {
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<unsigned>((s >> 33) % n);
    }
};

static std::string genExpr(Rng& r, int depth, const std::vector<std::string>& vars) {
    if (depth == 0 || r.next(4) == 0)
        return r.next(2) ? std::to_string(r.next(10)) : vars[r.next(static_cast<unsigned>(vars.size()))];
    switch (r.next(5)) {
        case 0: return "-" + genExpr(r, depth - 1, vars);
        case 1: return "!" + genExpr(r, depth - 1, vars);
        case 2: return "(" + genExpr(r, depth - 1, vars) + (r.next(2) ? " / " : " % ") + std::to_string(1 + r.next(5)) + ")";
        default: {
            static const char* ops[] = {"+", "-", "*", "<", ">", "<=", ">=", "==", "!=", "&&", "||"};
            return "(" + genExpr(r, depth - 1, vars) + " " + ops[r.next(11)] + " " + genExpr(r, depth - 1, vars) + ")";
        }
    }
}

static std::string genProgram(Rng& r) {
    std::vector<std::string> vars = {"a", "b", "c"};
    std::ostringstream os;
    os << "int main() {\n  int a = " << r.next(10) << "; int b = " << r.next(10) << "; int c;\n";
    int loops = 0;
    auto assign = [&]() { return vars[r.next(3)] + " = " + genExpr(r, 3, vars) + ";"; };
    for (int n = 3 + static_cast<int>(r.next(5)); n > 0; --n) {
        switch (r.next(4)) {
            case 0: os << "  " << assign() << "\n"; break;
            case 1: os << "  if (" << genExpr(r, 2, vars) << ") { " << assign() << " } else { " << assign() << " }\n"; break;
            case 2: {
                std::string i = "i" + std::to_string(loops++);
                os << "  { int " << i << " = 0; while (" << i << " < " << 1 + r.next(6) << ") { " << assign() << " "
                   << i << " = " << i << " + 1; } }\n";
                break;
            }
            default: os << "  { int a = " << genExpr(r, 2, vars) << "; b = a + 1; }\n"; break;  // shadowing
        }
    }
    os << "  return " << genExpr(r, 3, vars) << ";\n}\n";
    return os.str();
}

static void testDifferential() {
    Rng rng(12345);
    for (int n = 0; n < 1000; ++n) {
        std::string src = genProgram(rng);
        int v0 = 0, v1 = 0;
        std::string e0, e1;
        bool ok0 = runProgram(src, false, v0, e0), ok1 = runProgram(src, true, v1, e1);
        CHECK_MSG(ok0 && ok1 && v0 == v1, "optimizer changed behaviour (-O0=" << (ok0 ? std::to_string(v0) : e0)
                                           << ", -O1=" << (ok1 ? std::to_string(v1) : e1) << ") for:\n" << src);
        if (g_failures > 3) return;  // don't flood the log
    }
}

int main() {
    testLexer();
    testParserPrecedence();
    testSyntaxErrors();
    testSemantics();
    testIR();
    testOptimizer();
    testCodegen();
    testExamples();
    testDifferential();
    std::cout << (g_failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED") << " (" << g_checks << " checks, "
              << g_failures << " failures)\n";
    return g_failures == 0 ? 0 : 1;
}
