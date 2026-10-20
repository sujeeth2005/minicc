#include "lexer.hpp"

#include <cctype>
#include <climits>
#include <unordered_map>

namespace minicc {

const char* tokName(Tok t) {
    switch (t) {
        case Tok::KwInt: return "int";
        case Tok::KwReturn: return "return";
        case Tok::KwIf: return "if";
        case Tok::KwElse: return "else";
        case Tok::KwWhile: return "while";
        case Tok::Ident: return "identifier";
        case Tok::Number: return "number";
        case Tok::Plus: return "+";
        case Tok::Minus: return "-";
        case Tok::Star: return "*";
        case Tok::Slash: return "/";
        case Tok::Percent: return "%";
        case Tok::Lt: return "<";
        case Tok::Gt: return ">";
        case Tok::Le: return "<=";
        case Tok::Ge: return ">=";
        case Tok::EqEq: return "==";
        case Tok::Ne: return "!=";
        case Tok::Assign: return "=";
        case Tok::Not: return "!";
        case Tok::AndAnd: return "&&";
        case Tok::OrOr: return "||";
        case Tok::LParen: return "(";
        case Tok::RParen: return ")";
        case Tok::LBrace: return "{";
        case Tok::RBrace: return "}";
        case Tok::Semi: return ";";
        case Tok::End: return "end of file";
    }
    return "?";
}

std::string describe(const Token& t) {
    switch (t.kind) {
        case Tok::Ident: return "identifier '" + t.text + "'";
        case Tok::Number: return "number '" + t.text + "'";
        case Tok::End: return "end of file";
        default: return std::string("'") + tokName(t.kind) + "'";
    }
}

std::vector<Token> lex(const std::string& s) {
    static const std::unordered_map<std::string, Tok> keywords = {
        {"int", Tok::KwInt}, {"return", Tok::KwReturn}, {"if", Tok::KwIf},
        {"else", Tok::KwElse}, {"while", Tok::KwWhile}};

    std::vector<Token> out;
    size_t i = 0;
    int line = 1, col = 1;
    auto peek = [&](size_t k = 0) -> char { return i + k < s.size() ? s[i + k] : '\0'; };
    auto adv = [&]() {
        if (s[i] == '\n') { ++line; col = 1; } else { ++col; }
        ++i;
    };
    auto isIdStart = [](char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; };
    auto isIdChar = [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; };

    while (i < s.size()) {
        char c = s[i];
        if (std::isspace(static_cast<unsigned char>(c))) { adv(); continue; }
        if (c == '/' && peek(1) == '/') {
            while (i < s.size() && s[i] != '\n') adv();
            continue;
        }
        if (c == '/' && peek(1) == '*') {
            SourceLoc start{line, col};
            adv(); adv();
            bool closed = false;
            while (i < s.size()) {
                if (s[i] == '*' && peek(1) == '/') { adv(); adv(); closed = true; break; }
                adv();
            }
            if (!closed) throw CompileError(start, "unterminated block comment");
            continue;
        }

        SourceLoc loc{line, col};

        if (std::isdigit(static_cast<unsigned char>(c))) {
            long long v = 0;
            std::string text;
            while (std::isdigit(static_cast<unsigned char>(peek()))) {
                v = v * 10 + (peek() - '0');
                if (v > INT_MAX) throw CompileError(loc, "integer literal is too large for 'int'");
                text += peek();
                adv();
            }
            if (isIdStart(peek()))
                throw CompileError(loc, "invalid suffix on integer literal '" + text + peek() + "'");
            out.push_back({Tok::Number, text, static_cast<int>(v), loc});
            continue;
        }

        if (isIdStart(c)) {
            std::string text;
            while (isIdChar(peek())) { text += peek(); adv(); }
            auto it = keywords.find(text);
            out.push_back({it == keywords.end() ? Tok::Ident : it->second, text, 0, loc});
            continue;
        }

        Tok k = Tok::End;
        size_t len = 1;
        switch (c) {
            case '+': k = Tok::Plus; break;
            case '-': k = Tok::Minus; break;
            case '*': k = Tok::Star; break;
            case '/': k = Tok::Slash; break;
            case '%': k = Tok::Percent; break;
            case '(': k = Tok::LParen; break;
            case ')': k = Tok::RParen; break;
            case '{': k = Tok::LBrace; break;
            case '}': k = Tok::RBrace; break;
            case ';': k = Tok::Semi; break;
            case '<': if (peek(1) == '=') { k = Tok::Le; len = 2; } else k = Tok::Lt; break;
            case '>': if (peek(1) == '=') { k = Tok::Ge; len = 2; } else k = Tok::Gt; break;
            case '=': if (peek(1) == '=') { k = Tok::EqEq; len = 2; } else k = Tok::Assign; break;
            case '!': if (peek(1) == '=') { k = Tok::Ne; len = 2; } else k = Tok::Not; break;
            case '&':
                if (peek(1) == '&') { k = Tok::AndAnd; len = 2; }
                else throw CompileError(loc, "unexpected '&' (bitwise operators are not supported; did you mean '&&'?)");
                break;
            case '|':
                if (peek(1) == '|') { k = Tok::OrOr; len = 2; }
                else throw CompileError(loc, "unexpected '|' (bitwise operators are not supported; did you mean '||'?)");
                break;
            default:
                throw CompileError(loc, std::string("unexpected character '") + c + "'");
        }
        std::string text = s.substr(i, len);
        for (size_t n = 0; n < len; ++n) adv();
        out.push_back({k, text, 0, loc});
    }
    out.push_back({Tok::End, "", 0, SourceLoc{line, col}});
    return out;
}

}  // namespace minicc
