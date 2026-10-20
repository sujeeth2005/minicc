// minicc - command-line driver.
#include <fstream>
#include <iostream>
#include <sstream>

#include "driver.hpp"
#include "ast.hpp"

using namespace minicc;

static void usage() {
    std::cout <<
        "usage: minicc <file.mc> [options]\n"
        "  -O0 | -O1       disable / enable the optimizer (default: -O1)\n"
        "  --tokens        print the token stream\n"
        "  --ast           print the abstract syntax tree\n"
        "  --ir            print three-address code before optimization\n"
        "  --opt-ir        print three-address code after optimization\n"
        "  --asm           print generated assembly (default if nothing else is selected)\n"
        "  --stats         print optimizer statistics\n"
        "  --run           execute the generated code on the built-in VM and print the result\n"
        "  -h, --help      show this help\n";
}

int main(int argc, char** argv) {
    std::string path;
    bool optimize = true, tokens = false, ast = false, ir = false, optIr = false, asmOut = false,
         stats = false, doRun = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-O0") optimize = false;
        else if (a == "-O1") optimize = true;
        else if (a == "--tokens") tokens = true;
        else if (a == "--ast") ast = true;
        else if (a == "--ir") ir = true;
        else if (a == "--opt-ir") optIr = true;
        else if (a == "--asm") asmOut = true;
        else if (a == "--stats") stats = true;
        else if (a == "--run") doRun = true;
        else if (a == "-h" || a == "--help") { usage(); return 0; }
        else if (!a.empty() && a[0] == '-') { std::cerr << "minicc: unknown option '" << a << "'\n"; usage(); return 2; }
        else path = a;
    }
    if (path.empty()) { usage(); return 2; }
    if (!tokens && !ast && !ir && !optIr && !asmOut && !stats && !doRun) asmOut = true;

    std::ifstream in(path, std::ios::binary);
    if (!in) { std::cerr << "minicc: cannot open '" << path << "'\n"; return 2; }
    std::stringstream buf;
    buf << in.rdbuf();
    std::string src = buf.str();

    CompileResult r = compile(src, optimize);
    for (const Diagnostic& d : r.warnings) std::cerr << formatDiagnostic(path, src, d, "warning");
    for (const Diagnostic& d : r.errors) std::cerr << formatDiagnostic(path, src, d, "error");
    if (!r.ok) {
        std::cerr << r.errors.size() << (r.errors.size() == 1 ? " error" : " errors") << " generated.\n";
        return 1;
    }

    if (tokens) {
        for (const Token& t : r.tokens)
            std::cout << t.loc.line << ':' << t.loc.col << "\t" << tokName(t.kind)
                      << (t.kind == Tok::Ident || t.kind == Tok::Number ? "\t" + t.text : "") << '\n';
    }
    if (ast) std::cout << dump(*r.ast);
    if (ir) std::cout << "; --- three-address code (unoptimized) ---\n" << toString(r.ir);
    if (optIr) std::cout << "; --- three-address code (" << (optimize ? "optimized" : "-O0") << ") ---\n" << toString(r.finalIR);
    if (stats) {
        const OptStats& s = r.stats;
        std::cout << "; optimizer: " << s.iterations << " iteration(s), " << s.propagated << " propagated, "
                  << s.folded << " folded, " << s.simplified << " simplified, " << s.branchesFolded
                  << " branches folded, " << s.unreachable << " unreachable removed, " << s.jumpsRemoved
                  << " jumps/labels removed, " << s.deadRemoved << " dead removed\n";
    }
    if (asmOut) std::cout << toString(r.assembly);
    if (doRun) {
        RunResult rr = run(r.assembly);
        if (!rr.ok) { std::cerr << "runtime error: " << rr.error << '\n'; return 3; }
        std::cout << "result: " << rr.value << "  (" << rr.steps << " VM steps)\n";
    }
    return 0;
}
