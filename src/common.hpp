// common.hpp - shared diagnostics types used by every compiler phase.
#pragma once
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace minicc {

struct SourceLoc {
    int line = 1;
    int col = 1;
};

struct Diagnostic {
    SourceLoc loc;
    std::string message;
};

// Thrown by the lexer and parser, which stop at the first error.
class CompileError : public std::runtime_error {
public:
    CompileError(SourceLoc l, const std::string& m) : std::runtime_error(m), loc(l) {}
    SourceLoc loc;
};

// True for names the IR generator uses for compiler temporaries: t1, t2, ...
// The semantic analyzer renames user variables that collide with this pattern.
inline bool isTempName(const std::string& s) {
    if (s.size() < 2 || s[0] != 't') return false;
    for (size_t i = 1; i < s.size(); ++i)
        if (s[i] < '0' || s[i] > '9') return false;
    return true;
}

// "file:line:col: error: message" followed by the source line and a caret.
inline std::string formatDiagnostic(const std::string& file, const std::string& src,
                                    const Diagnostic& d, const char* severity) {
    std::ostringstream os;
    os << file << ':' << d.loc.line << ':' << d.loc.col << ": " << severity << ": "
       << d.message << '\n';
    int line = 1;
    size_t pos = 0;
    while (line < d.loc.line && pos < src.size()) {
        size_t nl = src.find('\n', pos);
        if (nl == std::string::npos) break;
        pos = nl + 1;
        ++line;
    }
    if (line == d.loc.line && pos <= src.size()) {
        size_t end = src.find('\n', pos);
        if (end == std::string::npos) end = src.size();
        std::string text = src.substr(pos, end - pos);
        if (!text.empty() && text.back() == '\r') text.pop_back();
        os << "  " << text << "\n  "
           << std::string(d.loc.col > 0 ? static_cast<size_t>(d.loc.col - 1) : 0, ' ')
           << "^\n";
    }
    return os.str();
}

}  // namespace minicc
