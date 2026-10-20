# minicc assembly (virtual-register ISA)

The back end emits assembly for a small **virtual-register machine**. It is deliberately
PTX-flavoured: an unbounded set of typed-by-convention 32-bit virtual registers (`r0`, `r1`, ...),
three-address arithmetic, set-on-compare instead of flags, and predicated-style branches. There is
no register allocation and no stack; mapping virtual registers to physical ones is the natural
next compiler stage.

All values are 32-bit two's-complement integers with wrap-around arithmetic. All registers start at 0.

## Operands

| Form  | Meaning |
|-------|---------|
| `rN`  | virtual register N |
| `123` | immediate (decimal, may be negative) |
| `L3`  | code label |

**Encoding rule:** the first source operand must be a register; the second may be a register or an
immediate. When the IR has a constant as first operand (only possible at `-O0`), the code generator
first emits `mov rS, imm` into a fresh scratch register.

## Instructions

| Instruction | Effect |
|-------------|--------|
| `mov  rd, rs/imm` | `rd = src` |
| `add  rd, ra, rb/imm` | `rd = ra + rb` |
| `sub  rd, ra, rb/imm` | `rd = ra - rb` |
| `mul  rd, ra, rb/imm` | `rd = ra * rb` |
| `div  rd, ra, rb/imm` | `rd = ra / rb` (truncates toward zero; trap if divisor is 0) |
| `rem  rd, ra, rb/imm` | `rd = ra % rb` (sign follows dividend; trap if divisor is 0) |
| `setlt/setle/setgt/setge/seteq/setne rd, ra, rb/imm` | `rd = (ra <cmp> rb) ? 1 : 0` |
| `neg  rd, ra` | `rd = -ra` |
| `not  rd, ra` | `rd = (ra == 0) ? 1 : 0` |
| `bra  L` | unconditional branch |
| `brz  ra, L` | branch if `ra == 0` |
| `brnz ra, L` | branch if `ra != 0` |
| `ret  ra/imm` | stop and return the value |

`INT_MIN / -1` is defined as `INT_MIN` and `INT_MIN % -1` as `0` (no trap).

## IR instruction vs. machine instruction

| Three-address IR | Assembly |
|------------------|----------|
| Operands are named variables/temporaries or constants | Operands are registers or immediates |
| `t1 = i <= 10` / `ifz t1 goto L2` | `setle r2, r1, 10` / `brz r2, L2` |
| Constants allowed anywhere | First source operand must be a register |
| Target-independent; optimizations happen here | Target-specific encoding |

## Example

```
int main() { int sum = 0; int i = 1; while (i <= 10) { sum = sum + i; i = i + 1; } return sum; }
```

```
.func main    ; 3 virtual registers
; r0 = sum
; r1 = i
; r2 = t1
    mov   r0, 0
    mov   r1, 1
L1:
    setle r2, r1, 10
    brz   r2, L2
    add   r0, r0, r1
    add   r1, r1, 1
    bra   L1
L2:
    ret   r0
```
