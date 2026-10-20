#include "optimizer.hpp"

#include <unordered_map>
#include <unordered_set>

namespace minicc {
namespace {

bool isImm(const Operand& o, int v) { return o.isConst && o.value == v; }

Instr makeCopy(const std::string& dst, Operand a) {
    Instr i;
    i.op = Op::Copy;
    i.dst = dst;
    i.a = std::move(a);
    return i;
}

// Algebraic identities. Rewrites `in` into a Copy and returns true on success.
bool simplify(Instr& in) {
    auto toCopy = [&](Operand v) {
        in = makeCopy(in.dst, std::move(v));
        return true;
    };
    switch (in.op) {
        case Op::Add:
            if (isImm(in.a, 0)) return toCopy(in.b);
            if (isImm(in.b, 0)) return toCopy(in.a);
            break;
        case Op::Sub:
            if (isImm(in.b, 0)) return toCopy(in.a);
            break;
        case Op::Mul:
            if (isImm(in.a, 1)) return toCopy(in.b);
            if (isImm(in.b, 1)) return toCopy(in.a);
            if (isImm(in.a, 0) || isImm(in.b, 0)) return toCopy(Operand::imm(0));
            break;
        case Op::Div:
            if (isImm(in.b, 1)) return toCopy(in.a);
            break;
        default: break;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Pass 1: constant propagation + copy propagation + constant folding.
// Works on one basic block at a time: the knowledge table is cleared at every
// label and after every unconditional transfer of control, which keeps the
// pass trivially correct in the presence of loops.
// ---------------------------------------------------------------------------
bool constantPropagation(IRFunction& f, OptStats& st) {
    std::vector<Instr> out;
    out.reserve(f.code.size());
    bool changed = false;
    std::unordered_map<std::string, Operand> known;  // name -> constant or other variable

    auto resolve = [&](Operand& o) {
        if (o.isConst) return;
        auto it = known.find(o.name);
        if (it != known.end()) {
            o = it->second;
            ++st.propagated;
            changed = true;
        }
    };
    // `d` is being redefined: forget it and everything that was a copy of it.
    auto kill = [&](const std::string& d) {
        known.erase(d);
        for (auto it = known.begin(); it != known.end();) {
            if (!it->second.isConst && it->second.name == d) it = known.erase(it);
            else ++it;
        }
    };

    for (Instr in : f.code) {
        switch (in.op) {
            case Op::Label:
                known.clear();
                out.push_back(in);
                continue;
            case Op::Jmp:
                out.push_back(in);
                known.clear();
                continue;
            case Op::Ret:
                resolve(in.a);
                out.push_back(in);
                known.clear();
                continue;
            case Op::Jz:
            case Op::Jnz: {
                resolve(in.a);
                if (in.a.isConst) {  // branch decided at compile time
                    bool taken = (in.op == Op::Jz) == (in.a.value == 0);
                    ++st.branchesFolded;
                    changed = true;
                    if (taken) {
                        Instr j;
                        j.op = Op::Jmp;
                        j.label = in.label;
                        out.push_back(j);
                        known.clear();
                    }
                    continue;  // not taken: the branch simply disappears
                }
                out.push_back(in);
                continue;
            }
            default: break;
        }

        resolve(in.a);
        if (usesB(in.op)) resolve(in.b);

        if (isBinaryOp(in.op)) {
            int r;
            if (in.a.isConst && in.b.isConst && evalBinary(in.op, in.a.value, in.b.value, r)) {
                in = makeCopy(in.dst, Operand::imm(r));
                ++st.folded;
                changed = true;
            } else if (simplify(in)) {
                ++st.simplified;
                changed = true;
            }
        } else if ((in.op == Op::Neg || in.op == Op::Not) && in.a.isConst) {
            in = makeCopy(in.dst, Operand::imm(evalUnary(in.op, in.a.value)));
            ++st.folded;
            changed = true;
        }

        if (in.op == Op::Copy && !in.a.isConst && in.a.name == in.dst) {  // x = x
            ++st.deadRemoved;
            changed = true;
            continue;
        }

        kill(in.dst);
        if (in.op == Op::Copy) known[in.dst] = in.a;
        out.push_back(in);
    }
    f.code = std::move(out);
    return changed;
}

// ---------------------------------------------------------------------------
// Pass 2: fold "t = a op b; x = t" into "x = a op b" when t is used only there.
// ---------------------------------------------------------------------------
bool forwardTemps(IRFunction& f, OptStats& st) {
    std::unordered_map<std::string, int> uses, defs;
    for (const Instr& i : f.code) {
        if (usesA(i.op) && !i.a.isConst) ++uses[i.a.name];
        if (usesB(i.op) && !i.b.isConst) ++uses[i.b.name];
        if (definesDst(i.op)) ++defs[i.dst];
    }
    std::vector<Instr> out;
    bool changed = false;
    const auto& c = f.code;
    for (size_t i = 0; i < c.size(); ++i) {
        if (i + 1 < c.size() && definesDst(c[i].op) && isTempName(c[i].dst) &&
            c[i + 1].op == Op::Copy && !c[i + 1].a.isConst && c[i + 1].a.name == c[i].dst &&
            uses[c[i].dst] == 1 && defs[c[i].dst] == 1) {
            Instr merged = c[i];
            merged.dst = c[i + 1].dst;
            out.push_back(merged);
            ++i;
            ++st.deadRemoved;
            changed = true;
        } else {
            out.push_back(c[i]);
        }
    }
    f.code = std::move(out);
    return changed;
}

// ---------------------------------------------------------------------------
// Pass 3: remove instructions that control flow can never reach.
// ---------------------------------------------------------------------------
bool removeUnreachable(IRFunction& f, OptStats& st) {
    const auto& c = f.code;
    size_t n = c.size();
    if (n == 0) return false;
    std::unordered_map<std::string, size_t> labelAt;
    for (size_t i = 0; i < n; ++i)
        if (c[i].op == Op::Label) labelAt[c[i].label] = i;

    std::vector<char> seen(n, 0);
    std::vector<size_t> work{0};
    while (!work.empty()) {
        size_t i = work.back();
        work.pop_back();
        if (i >= n || seen[i]) continue;
        seen[i] = 1;
        const Instr& in = c[i];
        if (in.op == Op::Ret) continue;
        if (in.op == Op::Jmp) { work.push_back(labelAt.at(in.label)); continue; }
        if (in.op == Op::Jz || in.op == Op::Jnz) work.push_back(labelAt.at(in.label));
        work.push_back(i + 1);
    }
    std::vector<Instr> out;
    for (size_t i = 0; i < n; ++i) {
        if (seen[i]) out.push_back(c[i]);
        else ++st.unreachable;
    }
    bool changed = out.size() != n;
    f.code = std::move(out);
    return changed;
}

// ---------------------------------------------------------------------------
// Pass 4: delete "goto L" right before "L:" and labels nobody jumps to.
// ---------------------------------------------------------------------------
bool cleanupJumps(IRFunction& f, OptStats& st) {
    bool changed = false;
    const auto& c = f.code;
    std::vector<Instr> out;
    for (size_t i = 0; i < c.size(); ++i) {
        if (c[i].op == Op::Jmp) {
            bool redundant = false;
            for (size_t j = i + 1; j < c.size() && c[j].op == Op::Label; ++j)
                if (c[j].label == c[i].label) { redundant = true; break; }
            if (redundant) { ++st.jumpsRemoved; changed = true; continue; }
        }
        out.push_back(c[i]);
    }
    std::unordered_set<std::string> targeted;
    for (const Instr& i : out)
        if (i.op == Op::Jmp || i.op == Op::Jz || i.op == Op::Jnz) targeted.insert(i.label);
    std::vector<Instr> out2;
    for (const Instr& i : out) {
        if (i.op == Op::Label && !targeted.count(i.label)) { ++st.jumpsRemoved; changed = true; continue; }
        out2.push_back(i);
    }
    f.code = std::move(out2);
    return changed;
}

// ---------------------------------------------------------------------------
// Pass 5: dead-code elimination. A pure instruction whose result is never read
// anywhere in the function is removed (repeated until nothing changes).
// Division/remainder are kept unless the divisor is a non-zero constant,
// because removing them would hide a run-time trap.
// ---------------------------------------------------------------------------
bool eliminateDeadCode(IRFunction& f, OptStats& st) {
    bool any = false;
    for (;;) {
        std::unordered_map<std::string, int> uses;
        for (const Instr& i : f.code) {
            if (usesA(i.op) && !i.a.isConst) ++uses[i.a.name];
            if (usesB(i.op) && !i.b.isConst) ++uses[i.b.name];
        }
        std::vector<Instr> out;
        bool removed = false;
        for (const Instr& i : f.code) {
            bool mayTrap = (i.op == Op::Div || i.op == Op::Rem) && !(i.b.isConst && i.b.value != 0);
            if (definesDst(i.op) && !mayTrap && uses[i.dst] == 0) {
                ++st.deadRemoved;
                removed = true;
                continue;
            }
            out.push_back(i);
        }
        f.code = std::move(out);
        if (!removed) break;
        any = true;
    }
    return any;
}

}  // namespace

void optimizeIR(IRFunction& f, OptStats* stats) {
    OptStats local;
    OptStats& st = stats ? *stats : local;
    for (int iter = 0; iter < 100; ++iter) {
        bool changed = false;
        changed |= constantPropagation(f, st);
        changed |= forwardTemps(f, st);
        changed |= removeUnreachable(f, st);
        changed |= cleanupJumps(f, st);
        changed |= eliminateDeadCode(f, st);
        if (!changed) break;
        ++st.iterations;
    }
}

}  // namespace minicc
