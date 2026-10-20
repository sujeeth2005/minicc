// codegen.hpp - IR -> assembly-like code for a virtual-register machine, plus
// a tiny interpreter so generated code can actually be executed and tested.
#pragma once
#include <string>
#include <vector>

#include "ir.hpp"

namespace minicc {

struct AOperand {
    enum Kind { None, Reg, Imm } kind = None;
    int v = 0;  // register number or immediate value
    static AOperand reg(int r) { AOperand o; o.kind = Reg; o.v = r; return o; }
    static AOperand imm(int x) { AOperand o; o.kind = Imm; o.v = x; return o; }
};

// Machine instruction. Opcodes share the IR's Op enum, but operands are
// registers/immediates and the encoding rules differ (see docs/ISA.md).
struct AsmInstr {
    Op op = Op::Copy;
    AOperand d, a, b;
    std::string label;
};

struct AsmProgram {
    std::string name;
    std::vector<AsmInstr> code;
    std::vector<std::string> regNames;  // regNames[r] = source variable / temp held by register r
};

AsmProgram generateAsm(const IRFunction& f);
std::string toString(const AsmProgram& p);

struct RunResult {
    bool ok = false;
    int value = 0;
    std::string error;
    long long steps = 0;
};
RunResult run(const AsmProgram& p, long long maxSteps = 10000000);

}  // namespace minicc
