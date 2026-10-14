#!/usr/bin/env python3
"""ADSP-21xx disassembler for the DCS sound board's OS images (original/partition/F_OSYS.BIN at
PM 0, F_COMP.BIN at PM 0x2800: big-endian 24-bit words). A straight port of MAME's 2100dasm.cpp
(BSD-3-Clause, Aaron Giles). usage: adsp21xx_dasm.py FILE BASE_HEX > listing"""
import sys

def BIT(v, s, n=1): return (v >> s) & ((1 << n) - 1)

flag_change = ["", "TOGGLE %s ", "RESET %s ", "SET %s "]
mode_change = ["", "", "DIS %s ", "ENA %s "]
alu_xop = ["AX0", "AX1", "AR", "MR0", "MR1", "MR2", "SR0", "SR1"]
alu_yop = ["AY0", "AY1", "AF", "0"]
alu_dst = ["AR", "AF", "NONE"]
mac_xop = ["MX0", "MX1", "AR", "MR0", "MR1", "MR2", "SR0", "SR1"]
mac_yop = ["MY0", "MY1", "MF", "0"]
mac_dst = ["MR", "MF", "NONE"]
shift_xop = ["SI", "??", "AR", "MR0", "MR1", "MR2", "SR0", "SR1"]
reg_grp = [
    ["AX0", "AX1", "MX0", "MX1", "AY0", "AY1", "MY0", "MY1", "SI", "SE", "AR", "MR0", "MR1", "MR2", "SR0", "SR1"],
    ["I0", "I1", "I2", "I3", "M0", "M1", "M2", "M3", "L0", "L1", "L2", "L3", "??", "??", "PMOVLAY", "DMOVLAY"],
    ["I4", "I5", "I6", "I7", "M4", "M5", "M6", "M7", "L4", "L5", "L6", "L7", "??", "??", "??", "??"],
    ["ASTAT", "MSTAT", "SSTAT", "IMASK", "ICNTL", "CNTR", "SB", "PX", "RX0", "TX0", "RX1", "TX1", "IFC", "OWRCNTR", "??", "??"],
]
dual_xreg = ["AX0", "AX1", "MX0", "MX1"]
dual_yreg = ["AY0", "AY1", "MY0", "MY1"]
condition = ["IF EQ ", "IF NE ", "IF GT ", "IF LE ", "IF LT ", "IF GE ", "IF AV ", "IF NOT AV ", "IF AC ", "IF NOT AC ",
             "IF NEG ", "IF POS ", "IF MV ", "IF NOT MV ", "IF NOT CE ", ""]
do_condition = ["NE", "EQ", "LE", "GT", "GE", "LT", "NOT AV", "AV", "NOT AC", "AC", "POS", "NEG", "NOT MV", "MV", "CE", "FOREVER"]
alumac_op = [
    ["", ""],
    ["%s = %s * %s (RND)", "%s = %s * %s (RND)"],
    ["%s = MR + %s * %s (RND)", "%s = MR + %s * %s (RND)"],
    ["%s = MR - %s * %s (RND)", "%s = MR - %s * %s (RND)"],
    ["%s = %s * %s (SS)", "%s = 0"],
    ["%s = %s * %s (SU)", "%s = %s * %s (SU)"],
    ["%s = %s * %s (US)", "%s = %s * %s (US)"],
    ["%s = %s * %s (UU)", "%s = %s * %s (UU)"],
    ["%s = MR + %s * %s (SS)", "%s = MR + %s * %s (SS)"],
    ["%s = MR + %s * %s (SU)", "%s = MR + %s * %s (SU)"],
    ["%s = MR + %s * %s (US)", "%s = MR + %s * %s (US)"],
    ["%s = MR + %s * %s (UU)", "%s = MR + %s * %s (UU)"],
    ["%s = MR - %s * %s (SS)", "%s = MR - %s * %s (SS)"],
    ["%s = MR - %s * %s (SS)", "%s = MR - %s * %s (SS)"],
    ["%s = MR - %s * %s (US)", "%s = MR - %s * %s (US)"],
    ["%s = MR - %s * %s (UU)", "%s = MR - %s * %s (UU)"],
    ["!%s = %s", "%s = 0"],
    ["!%s = %s + 1", "%s = 1"],
    ["%s = %s + %s + C", "%s = %s + %s + C"],
    ["%s = %s + %s", "%s = %s"],
    ["!%s = NOT %s", "!%s = NOT %s"],
    ["!%s = -%s", "!%s = -%s"],
    ["%s = %s - %s + C - 1", "%s = %s + C - 1"],
    ["%s = %s - %s", "%s = %s - %s"],
    ["!%s = %s - 1", "%s = -1"],
    ["!%s = %s - %s", "%s = -%s"],
    ["!%s = %s - %s + C - 1", "%s = -%s + C - 1"],
    ["%s = NOT %s", "%s = NOT %s"],
    ["%s = %s AND %s", "%s = %s AND %s"],
    ["%s = %s OR %s", "%s = %s OR %s"],
    ["%s = %s XOR %s", "%s = %s XOR %s"],
    ["%s = ABS %s", "%s = ABS %s"],
]
shift_op = ["SR = LSHIFT %s (HI)", "SR = SR OR LSHIFT %s (HI)", "SR = LSHIFT %s (LO)", "SR = SR OR LSHIFT %s (LO)",
            "SR = ASHIFT %s (HI)", "SR = SR OR ASHIFT %s (HI)", "SR = ASHIFT %s (LO)", "SR = SR OR ASHIFT %s (LO)",
            "SR = NORM %s (HI)", "SR = SR OR NORM %s (HI)", "SR = NORM %s (LO)", "SR = SR OR NORM %s (LO)",
            "SE = EXP %s (HI)", "SE = EXP %s (HIX)", "SE = EXP %s (LO)", "SB = EXPADJ %s"]
shift_by_op = ["SR = LSHIFT %s BY %d (HI)", "SR = SR OR LSHIFT %s BY %d (HI)", "SR = LSHIFT %s BY %d (LO)", "SR = SR OR LSHIFT %s BY %d (LO)",
               "SR = ASHIFT %s BY %d (HI)", "SR = SR OR ASHIFT %s BY %d (HI)", "SR = ASHIFT %s BY %d (LO)", "SR = SR OR ASHIFT %s BY %d (LO)"] + ["???"] * 8
constants = ["$0001", "$FFFE", "$0002", "$FFFD", "$0004", "$FFFB", "$0008", "$FFF7", "$0010", "$FFEF", "$0020", "$FFDF", "$0040", "$FFBF",
             "$0080", "$FF7F", "$0100", "$FEFF", "$0200", "$FDFF", "$0400", "$FBFF", "$0800", "$F7FF", "$1000", "$EFFF", "$2000", "$DFFF",
             "$4000", "$BFFF", "$8000", "$7FFF"]

def fmt(s, *a):
    n = s.count('%')
    return s % a[:n] if n else s

def alumac(dest, op):
    opindex = BIT(op, 13, 5)
    if BIT(opindex, 4):
        xop = alu_xop[BIT(op, 8, 3)]; yop = alu_yop[BIT(op, 11, 2)]; dst = alu_dst[dest]
    else:
        xop = mac_xop[BIT(op, 8, 3)]; yop = mac_yop[BIT(op, 11, 2)]; dst = mac_dst[dest]
    s = alumac_op[opindex][1 if BIT(op, 11, 2) == 3 else 0]
    if s.startswith('!'): return fmt(s[1:], dst, yop, xop)
    return fmt(s, dst, xop, yop)

def aluconst(dest, op):
    opindex = BIT(op, 13, 5)
    if BIT(opindex, 4):
        xop = alu_xop[BIT(op, 8, 3)]; cval = constants[BIT(op, 5, 3) | (BIT(op, 11, 2) << 3)]; dst = alu_dst[dest]
    else:
        xop = mac_xop[BIT(op, 8, 3)]; cval = xop; dst = mac_dst[dest]
    s = alumac_op[opindex][1 if BIT(op, 11, 2) == 3 else 0]
    if s.startswith('!'): return fmt(s[1:], dst, cval, xop)
    return fmt(s, dst, xop, cval)

def dasm(op):
    hi = BIT(op, 16, 8)
    if hi == 0x00: return "NOP"
    if hi == 0x01:
        if not BIT(op, 15): return "%s = IO($%X)" % (reg_grp[0][BIT(op, 0, 4)], BIT(op, 4, 11))
        return "IO($%X) = %s" % (BIT(op, 4, 11), reg_grp[0][BIT(op, 0, 4)])
    if hi == 0x02:
        if BIT(op, 12, 4) == 0:
            s = condition[BIT(op, 0, 4)]
            s += fmt(flag_change[BIT(op, 4, 2)], "FLAG_OUT") + fmt(flag_change[BIT(op, 6, 2)], "FL0") + fmt(flag_change[BIT(op, 8, 2)], "FL1") + fmt(flag_change[BIT(op, 10, 2)], "FL2")
            return s
        if BIT(op, 4, 12) == 0x800:
            return "IDLE" if BIT(op, 0, 4) == 0 else "IDLE(%d)" % (BIT(op, 0, 4) << 4)
        return "??? (%06X)" % op
    if hi == 0x03:
        s = "IF FLAG_IN " if BIT(op, 1) else "IF NOT FLAG_IN "
        s += "CALL " if BIT(op, 0) else "JUMP "
        return s + "$%04X" % (BIT(op, 4, 12) | (BIT(op, 2, 2) << 12))
    if hi == 0x04:
        if BIT(op, 5, 11) == 0:
            s = ""
            if BIT(op, 4): s += "POP PC "
            if BIT(op, 3): s += "POP LOOP "
            if BIT(op, 2): s += "POP CNTR "
            if BIT(op, 0, 2) == 2: s += "PUSH STAT "
            elif BIT(op, 0, 2) == 3: s += "POP STAT "
            return s.strip()
        return "??? (%06X)" % op
    if hi == 0x05: return "IF MV SAT MR" if BIT(op, 0, 16) == 0 else "??? (%06X)" % op
    if hi == 0x06: return "DIVS %s,%s" % (alu_yop[BIT(op, 11, 2)], alu_xop[BIT(op, 8, 3)]) if (op & 0x00e0ff) == 0 else "??? (%06X)" % op
    if hi == 0x07: return "DIVQ %s" % alu_xop[BIT(op, 8, 3)] if (op & 0x00f8ff) == 0x001000 else "??? (%06X)" % op
    if hi == 0x08: return "??? (%06X)" % op
    if hi == 0x09:
        if BIT(op, 5, 11) == 0:
            t = BIT(op, 2, 3); return "MODIFY (I%d,M%d)" % (t, (t & 4) + BIT(op, 0, 2))
        return "??? (%06X)" % op
    if hi == 0x0a:
        if BIT(op, 5, 11) == 0: return condition[BIT(op, 0, 4)] + ("RTI" if BIT(op, 4) else "RTS")
        return "??? (%06X)" % op
    if hi == 0x0b:
        if BIT(op, 8, 8) == 0:
            return condition[BIT(op, 0, 4)] + ("CALL (I%d)" if BIT(op, 4) else "JUMP (I%d)") % (4 + BIT(op, 6, 2))
        return "??? (%06X)" % op
    if hi == 0x0c:
        return (fmt(mode_change[BIT(op, 4, 2)], "SEC_REG") + fmt(mode_change[BIT(op, 6, 2)], "BIT_REV") + fmt(mode_change[BIT(op, 8, 2)], "AV_LATCH")
                + fmt(mode_change[BIT(op, 10, 2)], "AR_SAT") + fmt(mode_change[BIT(op, 12, 2)], "M_MODE") + fmt(mode_change[BIT(op, 14, 2)], "TIMER")
                + fmt(mode_change[BIT(op, 2, 2)], "G_MODE")).strip()
    if hi == 0x0d:
        if BIT(op, 12, 4) == 0: return "%s = %s" % (reg_grp[BIT(op, 10, 2)][BIT(op, 4, 4)], reg_grp[BIT(op, 8, 2)][BIT(op, 0, 4)])
        return "??? (%06X)" % op
    if hi == 0x0e:
        if (op & 0x0080f0) == 0: return condition[BIT(op, 0, 4)] + fmt(shift_op[BIT(op, 11, 4)], shift_xop[BIT(op, 8, 3)])
        return "??? (%06X)" % op
    if hi == 0x0f:
        if not BIT(op, 15):
            v = op & 0xff; v = v - 256 if v >= 128 else v
            return fmt(shift_by_op[BIT(op, 11, 4)], shift_xop[BIT(op, 8, 3)], v)
        return "??? (%06X)" % op
    if hi == 0x10:
        if not BIT(op, 15): return fmt(shift_op[BIT(op, 11, 4)], shift_xop[BIT(op, 8, 3)]) + ", %s = %s" % (reg_grp[0][BIT(op, 4, 4)], reg_grp[0][BIT(op, 0, 4)])
        return "??? (%06X)" % op
    if hi == 0x11:
        s = fmt(shift_op[BIT(op, 11, 4)], shift_xop[BIT(op, 8, 3)])
        if BIT(op, 15): return s + ", PM(I%d,M%d) = %s" % (4 + BIT(op, 2, 2), 4 + BIT(op, 0, 2), reg_grp[0][BIT(op, 4, 4)])
        return s + ", %s = PM(I%d,M%d)" % (reg_grp[0][BIT(op, 4, 4)], 4 + BIT(op, 2, 2), 4 + BIT(op, 0, 2))
    if hi in (0x12, 0x13):
        s = fmt(shift_op[BIT(op, 11, 4)], shift_xop[BIT(op, 8, 3)]); t = 4 if BIT(op, 16) else 0
        if BIT(op, 15): return s + ", DM(I%d,M%d) = %s" % (t + BIT(op, 2, 2), t + BIT(op, 0, 2), reg_grp[0][BIT(op, 4, 4)])
        return s + ", %s = DM(I%d,M%d)" % (reg_grp[0][BIT(op, 4, 4)], t + BIT(op, 2, 2), t + BIT(op, 0, 2))
    if 0x14 <= hi <= 0x17: return "DO $%04X UNTIL %s" % (BIT(op, 4, 14), do_condition[BIT(op, 0, 4)])
    if 0x18 <= hi <= 0x1f:
        return condition[BIT(op, 0, 4)] + ("CALL $%04X" if BIT(op, 18) else "JUMP $%04X") % BIT(op, 4, 14)
    if 0x20 <= hi <= 0x27:
        return condition[BIT(op, 0, 4)] + (alumac(BIT(op, 18), op) if not BIT(op, 4) else aluconst(BIT(op, 18), op))
    if 0x28 <= hi <= 0x2f:
        if (op & 0x0600ff) == 0x0200aa: return alumac(2, op)
        s = alumac(BIT(op, 18), op) + ", " if BIT(op, 13, 5) else ""
        return s + "%s = %s" % (reg_grp[0][BIT(op, 4, 4)], reg_grp[0][BIT(op, 0, 4)])
    if 0x30 <= hi <= 0x3f: return "%s = $%04X" % (reg_grp[BIT(op, 18, 2)][BIT(op, 0, 4)], BIT(op, 4, 14))
    if 0x40 <= hi <= 0x4f: return "%s = $%04X" % (reg_grp[0][BIT(op, 0, 4)], BIT(op, 4, 16))
    if 0x50 <= hi <= 0x5f:
        s = alumac(BIT(op, 18), op) + ", " if BIT(op, 13, 5) else ""
        if BIT(op, 19): return s + "PM(I%d,M%d) = %s" % (4 + BIT(op, 2, 2), 4 + BIT(op, 0, 2), reg_grp[0][BIT(op, 4, 4)])
        return s + "%s = PM(I%d,M%d)" % (reg_grp[0][BIT(op, 4, 4)], 4 + BIT(op, 2, 2), 4 + BIT(op, 0, 2))
    if 0x60 <= hi <= 0x7f:
        s = alumac(BIT(op, 18), op) + ", " if BIT(op, 13, 5) else ""
        t = 4 if BIT(op, 20) else 0
        if BIT(op, 19): return s + "DM(I%d,M%d) = %s" % (t + BIT(op, 2, 2), t + BIT(op, 0, 2), reg_grp[0][BIT(op, 4, 4)])
        return s + "%s = DM(I%d,M%d)" % (reg_grp[0][BIT(op, 4, 4)], t + BIT(op, 2, 2), t + BIT(op, 0, 2))
    if 0x80 <= hi <= 0x9f:
        if BIT(op, 20): return "DM($%04X) = %s" % (BIT(op, 4, 14), reg_grp[BIT(op, 18, 2)][BIT(op, 0, 4)])
        return "%s = DM($%04X)" % (reg_grp[BIT(op, 18, 2)][BIT(op, 0, 4)], BIT(op, 4, 14))
    if 0xa0 <= hi <= 0xbf:
        t = 4 if BIT(op, 20) else 0
        return "DM(I%d,M%d) = $%04X" % (t + BIT(op, 2, 2), t + BIT(op, 0, 2), BIT(op, 4, 16))
    s = alumac(0, op) + ", " if BIT(op, 13, 5) else ""
    return s + "%s = DM(I%d,M%d), %s = PM(I%d,M%d)" % (dual_xreg[BIT(op, 18, 2)], BIT(op, 2, 2), BIT(op, 0, 2), dual_yreg[BIT(op, 20, 2)], 4 + BIT(op, 6, 2), 4 + BIT(op, 4, 2))

if __name__ == '__main__':
    d = open(sys.argv[1], 'rb').read()
    base = int(sys.argv[2], 16)
    for i in range(0, len(d) - 2, 3):
        op = (d[i] << 16) | (d[i + 1] << 8) | d[i + 2]
        print("%04X  %06X  %s" % (base + i // 3, op, dasm(op)))
