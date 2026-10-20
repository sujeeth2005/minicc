// ir.hpp - three-address code (TAC) intermediate representation.
#pragma once
#include <string>
#include <vector>

#include "ast.hpp"

namespace minicc {

// An operand is either an integer constant or a named variable/temporary.
struct Operand {
    bool isConst = true;
    int value = 0;
    std::string name;

    static Operand imm(int v) { Operand o; o.isConst = true; o.value = v; return o; }
    static Operand var(std::string n) { Operand o; o.isConst = false; o.name = std::move(n); return o; }
    std::string str() const { return isConst ? std::to_string(value) : name; }
};

enum class Op {
    Copy,                                   // dst = a
    Add, Sub, Mul, Div, Rem,                // dst = a <op> b
    Lt, Le, Gt, Ge, Eq, Ne,                 // dst = (a <op> b) ? 1 : 0
    Neg, Not,                               // dst = -a | !a
    Label,                                  // label:
    Jmp,                                    // goto label
    Jz, Jnz,                                // if a == 0 / a != 0 goto label
    Ret                                     // return a
};

inline bool isBinaryOp(Op o) { return o >= Op::Add && o <= Op::Ne; }
inline bool definesDst(Op o) { return o <= Op::Not; }
inline bool usesA(Op o) { return definesDst(o) || o >= Op::Jz; }
inline bool usesB(Op o) { return isBinaryOp(o); }

struct Instr {
    Op op = Op::Copy;
    std::string dst;
    Operand a, b;
    std::string label;  // for Label / Jmp / Jz / Jnz
};

struct IRFunction {
    std::string name;
    std::vector<Instr> code;
};

IRFunction generateIR(const Program& prog);

std::string toString(const Instr& i);
std::string toString(const IRFunction& f);
const char* opSymbol(Op op);  // "+", "<=", ...

// Constant evaluation with 32-bit two's-complement wrap-around.
// Returns false if the operation cannot be evaluated (division by zero).
bool evalBinary(Op op, int a, int b, int& out);
int evalUnary(Op op, int a);

}  // namespace minicc
