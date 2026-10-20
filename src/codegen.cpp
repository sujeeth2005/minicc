#include "codegen.hpp"

#include <iomanip>
#include <sstream>
#include <unordered_map>

namespace minicc {

static const char* mnemonic(Op op) {
    switch (op) {
        case Op::Copy: return "mov";
        case Op::Add: return "add";
        case Op::Sub: return "sub";
        case Op::Mul: return "mul";
        case Op::Div: return "div";
        case Op::Rem: return "rem";
        case Op::Lt: return "setlt";
        case Op::Le: return "setle";
        case Op::Gt: return "setgt";
        case Op::Ge: return "setge";
        case Op::Eq: return "seteq";
        case Op::Ne: return "setne";
        case Op::Neg: return "neg";
        case Op::Not: return "not";
        case Op::Jmp: return "bra";
        case Op::Jz: return "brz";
        case Op::Jnz: return "brnz";
        case Op::Ret: return "ret";
        case Op::Label: return "";
    }
    return "?";
}

AsmProgram generateAsm(const IRFunction& f) {
    AsmProgram p;
    p.name = f.name;
    std::unordered_map<std::string, int> regOf;

    // One virtual register per IR variable/temporary, assigned in order of first use.
    auto regFor = [&](const std::string& n) {
        auto it = regOf.find(n);
        if (it != regOf.end()) return it->second;
        int r = static_cast<int>(p.regNames.size());
        p.regNames.push_back(n);
        regOf[n] = r;
        return r;
    };
    auto emit = [&](Op op, AOperand d, AOperand a, AOperand b, const std::string& label = "") {
        AsmInstr i;
        i.op = op; i.d = d; i.a = a; i.b = b; i.label = label;
        p.code.push_back(i);
    };
    // The first source operand of every instruction must be a register: constants are
    // materialized with a `mov` into a fresh scratch register first.
    auto needReg = [&](const Operand& o) {
        if (!o.isConst) return AOperand::reg(regFor(o.name));
        int r = static_cast<int>(p.regNames.size());
        p.regNames.push_back("#" + std::to_string(o.value));
        emit(Op::Copy, AOperand::reg(r), AOperand::imm(o.value), {});
        return AOperand::reg(r);
    };
    auto regOrImm = [&](const Operand& o) {
        return o.isConst ? AOperand::imm(o.value) : AOperand::reg(regFor(o.name));
    };

    for (const Instr& in : f.code) {
        switch (in.op) {
            case Op::Copy:
                emit(Op::Copy, AOperand::reg(regFor(in.dst)), regOrImm(in.a), {});
                break;
            case Op::Neg:
            case Op::Not: {
                AOperand a = needReg(in.a);
                emit(in.op, AOperand::reg(regFor(in.dst)), a, {});
                break;
            }
            case Op::Label: emit(Op::Label, {}, {}, {}, in.label); break;
            case Op::Jmp: emit(Op::Jmp, {}, {}, {}, in.label); break;
            case Op::Jz:
            case Op::Jnz: {
                AOperand a = needReg(in.a);
                emit(in.op, {}, a, {}, in.label);
                break;
            }
            case Op::Ret: emit(Op::Ret, {}, regOrImm(in.a), {}); break;
            default: {  // binary operators
                AOperand a = needReg(in.a);
                emit(in.op, AOperand::reg(regFor(in.dst)), a, regOrImm(in.b));
                break;
            }
        }
    }
    return p;
}

static std::string fmt(const AOperand& o) {
    return (o.kind == AOperand::Reg ? "r" : "") + std::to_string(o.v);
}

std::string toString(const AsmProgram& p) {
    std::ostringstream os;
    os << "; minicc output - virtual-register machine (see docs/ISA.md)\n";
    os << ".func " << p.name << "    ; " << p.regNames.size() << " virtual registers\n";
    for (size_t r = 0; r < p.regNames.size(); ++r) os << "; r" << r << " = " << p.regNames[r] << '\n';
    for (const AsmInstr& i : p.code) {
        if (i.op == Op::Label) { os << i.label << ":\n"; continue; }
        os << "    " << std::left << std::setw(6) << mnemonic(i.op);
        std::string args;
        auto add = [&](const std::string& s) { args += (args.empty() ? "" : ", ") + s; };
        if (i.d.kind != AOperand::None) add(fmt(i.d));
        if (i.a.kind != AOperand::None) add(fmt(i.a));
        if (i.b.kind != AOperand::None) add(fmt(i.b));
        if (!i.label.empty()) add(i.label);
        os << args << '\n';
    }
    return os.str();
}

RunResult run(const AsmProgram& p, long long maxSteps) {
    RunResult res;
    std::vector<int> regs(p.regNames.size(), 0);
    std::unordered_map<std::string, size_t> labelAt;
    for (size_t i = 0; i < p.code.size(); ++i)
        if (p.code[i].op == Op::Label) labelAt[p.code[i].label] = i;
    auto val = [&](const AOperand& o) { return o.kind == AOperand::Imm ? o.v : regs[static_cast<size_t>(o.v)]; };

    size_t pc = 0;
    while (pc < p.code.size()) {
        if (++res.steps > maxSteps) { res.error = "step limit exceeded (possible infinite loop)"; return res; }
        const AsmInstr& i = p.code[pc++];
        switch (i.op) {
            case Op::Label: break;
            case Op::Copy: regs[static_cast<size_t>(i.d.v)] = val(i.a); break;
            case Op::Neg:
            case Op::Not: regs[static_cast<size_t>(i.d.v)] = evalUnary(i.op, val(i.a)); break;
            case Op::Jmp: pc = labelAt.at(i.label); break;
            case Op::Jz: if (val(i.a) == 0) pc = labelAt.at(i.label); break;
            case Op::Jnz: if (val(i.a) != 0) pc = labelAt.at(i.label); break;
            case Op::Ret: res.ok = true; res.value = val(i.a); return res;
            default: {
                int out;
                if (!evalBinary(i.op, val(i.a), val(i.b), out)) {
                    res.error = "division by zero";
                    return res;
                }
                regs[static_cast<size_t>(i.d.v)] = out;
            }
        }
    }
    res.error = "execution fell off the end of the function";
    return res;
}

}  // namespace minicc
