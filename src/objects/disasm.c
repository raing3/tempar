/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * disasm.c - PSPLINK disassembler code
 *
 * Copyright (c) 2006 James F <tyranid@gmail.com>
 *
 * $HeadURL: http://psp.jim.sh/svn/psp/trunk/psplink/psplink/disasm.c $
 * $Id: disasm.c 2021 2006-10-12 22:22:21Z tyranid $
 */

#include <stdio.h>
#include <string.h>
#include "common.h"

extern Colors colors;

#ifndef _MIPS_
 
void mipsDecode(unsigned int opcode, unsigned int PC) {
	pspDebugScreenPuts("???      ");
	pspDebugScreenSetTextColor(colors.color02);
}

#else

/* Format codes
 * %d - Rd
 * %t - Rt
 * %s - Rs
 * %i - 16bit signed immediate
 * %I - 16bit unsigned immediate (always printed in hex)
 * %o - 16bit signed offset (rt base)
 * %O - 16bit signed offset (PC relative)
 * %V - 16bit signed offset (rs base)
 * %j - 26bit absolute offset
 * %J - Register jump
 * %a - SA
 * %0 - Cop0 register
 * %1 - Cop1 register
 * %p - General cop (i.e. numbered) register
 * %n - ins/ext size
 * %r - Debug register
 * %k - Cache function
 * %D - Fd
 * %T - Ft
 * %S - Fs
 * %x? - Vt (? is (s/scalar, p/pair, t/triple, q/quad, m/matrix pair, n/matrix triple, o/matrix quad)
 * %y? - Vs
 * %z? - Vd
 * %X? - Vo (? is (s, q))
 * %Y - VFPU offset
 * %Z - VFPU condition code
 * %v? - VFPU immediate, ? (3, 5, 8)
 * %c - code (for break)
 * %C - code (for syscall)
 * %? - Indicates vmmul special exception
 */

#define RT(op) ((op >> 16) & 0x1F)
#define RS(op) ((op >> 21) & 0x1F)
#define RD(op) ((op >> 11) & 0x1F)
#define FT(op) ((op >> 16) & 0x1F)
#define FS(op) ((op >> 11) & 0x1F)
#define FD(op) ((op >> 6) & 0x1F)
#define SA(op) ((op >> 6)  & 0x1F)
#define IMM(op) ((signed short) (op & 0xFFFF))
#define IMMU(op) ((unsigned short) (op & 0xFFFF))
#define JUMP(op, pc) ((pc & 0xF0000000) | ((op & 0x3FFFFFF) << 2))
#define CODE(op) ((op >> 6) & 0xFFFFF)
#define SIZE(op) ((op >> 11) & 0x1F)
#define POS(op)  ((op >> 6) & 0x1F)
#define VO(op)   (((op & 3) << 5) | ((op >> 16) & 0x1F))
#define VCC(op)  ((op >> 18) & 7)
#define VD(op)   (op & 0x7F)
#define VS(op)   ((op >> 8) & 0x7F)
#define VT(op)   ((op >> 16) & 0x7F)

struct Instruction
{
	const char *name;
	unsigned int opcode;
	unsigned int mask;
	const char *fmt;
};

struct Instruction macro[] =
{
	/* Macro instructions */
	{ "nop",		0x00000000, 0xFFFFFFFF, "" 			},
	{ "li",     	0x24000000, 0xFFE00000, "%t, %I" 	},// { "li",     	0x24000000, 0xFFE00000, "%t, %i" 	},
	{ "li",			0x34000000, 0xFFE00000, "%t, %I"	},
	{ "mov", 		0x00000021, 0xFC1F07FF, "%d, %s"	},// { "move", 		0x00000021, 0xFC1F07FF, "%d, %s"	},
	{ "mov",		0x00000025, 0xFC1F07FF, "%d, %s"	},// { "move",   	0x00000025, 0xFC1F07FF, "%d, %s"	},
	/*{ "b",			0x10000000, 0xFFFF0000, "%O"		},
	{ "b",			0x04010000, 0xFFFF0000, "%O"		},
	{ "bal",		0x04110000, 0xFFFF0000, "%O"		},
	{ "bnez",		0x14000000, 0xFC1F0000,	"%s, %O"	},
	{ "bnezl",		0x54000000, 0xFC1F0000,	"%s, %O"	},
	{ "neg",		0x00000022, 0xFFE007FF,	"%d, %t"	},
	{ "negu",		0x00000023, 0xFFE007FF,	"%d, %t"	},
	{ "not",		0x00000027, 0xFC1F07FF,	"%d, %s"	},*/
	{ "jalr",		0x0000F809, 0xFC1FFFFF,	"%J"},
};

struct Instruction inst[] =
{
	/* MIPS instructions */
	{ "add",		0x00000020, 0xFC0007FF, "%d, %s, %t"},
	{ "addi",		0x20000000, 0xFC000000, "%t, %s, %i"},
	{ "addiu",		0x24000000, 0xFC000000, "%t, %s, %i"},
	{ "addu",		0x00000021, 0xFC0007FF, "%d, %s, %t"},
	{ "and",		0x00000024, 0xFC0007FF,	"%d, %s, %t"},
	{ "andi",		0x30000000, 0xFC000000,	"%t, %s, %I"},
	{ "beq",		0x10000000, 0xFC000000,	"%s, %t, %O"},
	{ "beql",		0x50000000, 0xFC000000,	"%s, %t, %O"},
	{ "bgez",		0x04010000, 0xFC1F0000,	"%s, %O"},
	{ "bgezal",		0x04110000, 0xFC1F0000,	"%s, %O"},
	{ "bgezl",		0x04030000, 0xFC1F0000,	"%s, %O"},
	{ "bgtz",		0x1C000000, 0xFC1F0000,	"%s, %O"},
	{ "bgtzl",		0x5C000000, 0xFC1F0000,	"%s, %O"},
	{ "bitrev",		0x7C000520, 0xFFE007FF, "%d, %t"},
	{ "blez",		0x18000000, 0xFC1F0000,	"%s, %O"},
	{ "blezl",		0x58000000, 0xFC1F0000,	"%s, %O"},
	{ "bltz",		0x04000000, 0xFC1F0000,	"%s, %O"},
	{ "bltzl",		0x04020000, 0xFC1F0000,	"%s, %O"},
	{ "bltzal",		0x04100000, 0xFC1F0000,	"%s, %O"},
	{ "bltzall",	0x04120000, 0xFC1F0000,	"%s, %O"},
	{ "bne",		0x14000000, 0xFC000000,	"%s, %t, %O"},
	{ "bnel",		0x54000000, 0xFC000000,	"%s, %t, %O"},
	{ "break",		0x0000000D, 0xFC00003F,	"%c"},
	{ "cache",		0xBC000000, 0xfc000000, "%k, %o"},
	{ "cfc0",		0x40400000, 0xFFE007FF,	"%t, %p"},
	{ "clo",		0x00000017, 0xFC1F07FF, "%d, %s"},
	{ "clz",		0x00000016, 0xFC1F07FF, "%d, %s"},
	{ "ctc0",		0x40C00000, 0xFFE007FF,	"%t, %p"},
	{ "max",		0x0000002C, 0xFC0007FF, "%d, %s, %t"},
	{ "min",		0x0000002D, 0xFC0007FF, "%d, %s, %t"},
	{ "dbreak",		0x7000003F, 0xFFFFFFFF,	""},
	{ "div",		0x0000001A, 0xFC00FFFF, "%s, %t"},
	{ "divu",		0x0000001B, 0xFC00FFFF, "%s, %t"},
	{ "dret",		0x7000003E, 0xFFFFFFFF,	""},
	{ "eret",		0x42000018, 0xFFFFFFFF, ""},
	{ "ext",		0x7C000000, 0xFC00003F, "%t, %s, %a, %n"},
	{ "ins",		0x7C000004, 0xFC00003F, "%t, %s, %a, %n"},
	{ "j",			0x08000000, 0xFC000000,	"%j"},
	{ "jr",			0x00000008, 0xFC1FFFFF,	"%J"},
	{ "jalr",		0x00000009, 0xFC1F07FF,	"%J, %d"},
	{ "jal",		0x0C000000, 0xFC000000,	"%j"},
	{ "lb",			0x80000000, 0xFC000000,	"%t, %o"},
	{ "lbu",		0x90000000, 0xFC000000,	"%t, %o"},
	{ "lh",			0x84000000, 0xFC000000,	"%t, %o"},
	{ "lhu",		0x94000000, 0xFC000000,	"%t, %o"},
	{ "ll",			0xC0000000, 0xFC000000,	"%t, %O"},
	{ "lui",		0x3C000000, 0xFFE00000,	"%t, %I"},
	{ "lw",			0x8C000000, 0xFC000000,	"%t, %o"},
	{ "lwl",		0x88000000, 0xFC000000,	"%t, %o"},
	{ "lwr",		0x98000000, 0xFC000000,	"%t, %o"},
	{ "madd",		0x0000001C, 0xFC00FFFF, "%s, %t"},
	{ "maddu",		0x0000001D, 0xFC00FFFF, "%s, %t"},
	{ "mfc0",		0x40000000, 0xFFE007FF,	"%t, %0"},
	{ "mfdr",		0x7000003D, 0xFFE007FF,	"%t, %r"},
	{ "mfhi",		0x00000010, 0xFFFF07FF, "%d"},
	{ "mfic",		0x70000024, 0xFFE007FF, "%t, %p"},
	{ "mflo",		0x00000012, 0xFFFF07FF, "%d"},
	{ "movn",		0x0000000B, 0xFC0007FF, "%d, %s, %t"},
	{ "movz",		0x0000000A, 0xFC0007FF, "%d, %s, %t"},
	{ "msub",		0x0000002e, 0xfc00ffff, "%d, %t"},
	{ "msubu",		0x0000002f, 0xfc00ffff, "%d, %t"},
	{ "mtc0",		0x40800000, 0xFFE007FF,	"%t, %0"},
	{ "mtdr",		0x7080003D, 0xFFE007FF,	"%t, %r"},
	{ "mtic",		0x70000026, 0xFFE007FF, "%t, %p"},
	{ "halt",       0x70000000, 0xFFFFFFFF, "" },
	{ "mthi",		0x00000011, 0xFC1FFFFF,	"%s"},
	{ "mtlo",		0x00000013, 0xFC1FFFFF,	"%s"},
	{ "mult",		0x00000018, 0xFC00FFFF, "%s, %t"},
	{ "multu",		0x00000019, 0xFC0007FF, "%s, %t"},
	{ "nor",		0x00000027, 0xFC0007FF,	"%d, %s, %t"},
	{ "or",			0x00000025, 0xFC0007FF,	"%d, %s, %t"},
	{ "ori",		0x34000000, 0xFC000000,	"%t, %s, %I"},
	{ "rotr",		0x00200002, 0xFFE0003F, "%d, %t, %a"},
	{ "rotv",		0x00000046, 0xFC0007FF, "%d, %t, %s"},
	{ "seb",		0x7C000420, 0xFFE007FF,	"%d, %t"},
	{ "seh",		0x7C000620, 0xFFE007FF,	"%d, %t"},
	{ "sb",			0xA0000000, 0xFC000000,	"%t, %o"},
	{ "sh",			0xA4000000, 0xFC000000,	"%t, %o"},
	{ "sllv",		0x00000004, 0xFC0007FF,	"%d, %t, %s"},
	{ "sll",		0x00000000, 0xFFE0003F,	"%d, %t, %a"},
	{ "slt",		0x0000002A, 0xFC0007FF,	"%d, %s, %t"},
	{ "slti",		0x28000000, 0xFC000000,	"%t, %s, %i"},
	{ "sltiu",		0x2C000000, 0xFC000000,	"%t, %s, %i"},
	{ "sltu",		0x0000002B, 0xFC0007FF,	"%d, %s, %t"},
	{ "sra",		0x00000003, 0xFFE0003F,	"%d, %t, %a"},
	{ "srav",		0x00000007, 0xFC0007FF,	"%d, %t, %s"},
	{ "srlv",		0x00000006, 0xFC0007FF,	"%d, %t, %s"},
	{ "srl",		0x00000002, 0xFFE0003F,	"%d, %t, %a"},
	{ "sw",			0xAC000000, 0xFC000000,	"%t, %o"},
	{ "swl",		0xA8000000, 0xFC000000,	"%t, %o"},
	{ "swr",		0xB8000000, 0xFC000000,	"%t, %o"},
	{ "sub",		0x00000022, 0xFC0007FF,	"%d, %s, %t"},
	{ "subu",		0x00000023, 0xFC0007FF,	"%d, %s, %t"},
	{ "sync",		0x0000000F, 0xFFFFFFFF,	""},
	{ "syscall",	0x0000000C, 0xFC00003F,	"%C"},
	{ "xor",		0x00000026, 0xFC0007FF,	"%d, %s, %t"},
	{ "xori",		0x38000000, 0xFC000000,	"%t, %s, %I"},
	{ "wsbh",		0x7C0000A0, 0xFFE007FF,	"%d, %t"},
	{ "wsbw",		0x7C0000E0, 0xFFE007FF, "%d, %t"}, 

	/* FPU instructions */
	{"abs.s",	0x46000005, 0xFFFF003F, "%D, %S"},
	{"add.s",	0x46000000, 0xFFE0003F,	"%D, %S, %T"},
	{"bc1f",	0x45000000, 0xFFFF0000,	"%O"},
	{"bc1fl",	0x45020000, 0xFFFF0000,	"%O"},
	{"bc1t",	0x45010000, 0xFFFF0000,	"%O"},
	{"bc1tl",	0x45030000, 0xFFFF0000,	"%O"},
	{"c.f.s",	0x46000030, 0xFFE007FF, "%S, %T"},
	{"c.un.s",	0x46000031, 0xFFE007FF, "%S, %T"},
	{"c.eq.s",	0x46000032, 0xFFE007FF, "%S, %T"},
	{"c.ueq.s",	0x46000033, 0xFFE007FF, "%S, %T"},
	{"c.olt.s",	0x46000034, 0xFFE007FF,	"%S, %T"},
	{"c.ult.s",	0x46000035, 0xFFE007FF, "%S, %T"},
	{"c.ole.s",	0x46000036, 0xFFE007FF, "%S, %T"},
	{"c.ule.s",	0x46000037, 0xFFE007FF, "%S, %T"},
	{"c.sf.s",	0x46000038, 0xFFE007FF, "%S, %T"},
	{"c.ngle.s",0x46000039, 0xFFE007FF, "%S, %T"},
	{"c.seq.s",	0x4600003A, 0xFFE007FF, "%S, %T"},
	{"c.ngl.s",	0x4600003B, 0xFFE007FF, "%S, %T"},
	{"c.lt.s",	0x4600003C, 0xFFE007FF,	"%S, %T"},
	{"c.nge.s",	0x4600003D, 0xFFE007FF, "%S, %T"},
	{"c.le.s",	0x4600003E, 0xFFE007FF,	"%S, %T"},
	{"c.ngt.s",	0x4600003F, 0xFFE007FF, "%S, %T"},
	{"ceil.w.s",0x4600000E, 0xFFFF003F, "%D, %S"},
	{"cfc1",	0x44400000, 0xFFE007FF, "%t, %p"},
	{"ctc1",	0x44c00000, 0xFFE007FF, "%t, %p"},
	{"cvt.s.w",	0x46800020, 0xFFFF003F, "%D, %S"},
	{"cvt.w.s",	0x46000024, 0xFFFF003F, "%D, %S"},
	{"div.s",	0x46000003, 0xFFE0003F, "%D, %S, %T"},
	{"floor.w.s",0x4600000F, 0xFFFF003F,"%D, %S"},
	{"lwc1",	0xc4000000, 0xFC000000, "%T, %o"},
	{"mfc1",	0x44000000, 0xFFE007FF, "%t, %1"},
	{"mov.s",	0x46000006, 0xFFFF003F, "%D, %S"},
	{"mtc1",	0x44800000, 0xFFE007FF, "%t, %1"},
	{"mul.s",	0x46000002, 0xFFE0003F, "%D, %S, %T"},
	{"neg.s",	0x46000007, 0xFFFF003F, "%D, %S"},
	{"round.w.s",0x4600000C, 0xFFFF003F,"%D, %S"},
	{"sqrt.s",	0x46000004, 0xFFFF003F, "%D, %S"},
	{"sub.s",	0x46000001, 0xFFE0003F, "%D, %S, %T"},
	{"swc1",	0xe4000000, 0xFC000000, "%T, %o"},
	{"trunc.w.s",0x4600000D, 0xFFFF003F,"%D, %S"},
	
	/* VPU instructions */
	/*{ "bvf",	 0x49000000, 0xFFE30000, "%Z, %O" },
	{ "bvfl",	 0x49020000, 0xFFE30000, "%Z, %O" },
	{ "bvt",	 0x49010000, 0xFFE30000, "%Z, %O" },
	{ "bvtl",	 0x49030000, 0xFFE30000, "%Z, %O" },
	{ "lv.q",	 0xD8000000, 0xFC000002, "%Xq, %Y" },
	{ "lv.s",	 0xC8000000, 0xFC000000, "%Xs, %Y" },
	{ "lvl.q",	 0xD4000000, 0xFC000002, "%Xq, %Y" },
	{ "lvr.q",	 0xD4000002, 0xFC000002, "%Xq, %Y" },
	{ "mfv",	 0x48600000, 0xFFE0FF80, "" },
	{ "mfvc",	 0x48600000, 0xFFE0FF00, "" },
	{ "mtv",	 0x48E00000, 0xFFE0FF80, "" },
	{ "mtvc",	 0x48E00000, 0xFFE0FF00, "" },
	{ "sv.q",	 0xF8000000, 0xFC000002, "%Xq, %Y" },
	{ "sv.s",	 0xE8000000, 0xFC000000, "%Xs, %Y" },
	{ "svl.q",	 0xF4000000, 0xFC000002, "%Xq, %Y" },
	{ "svr.q",	 0xF4000002, 0xFC000002, "%Xq, %Y" },
	{ "vabs.p",	 0xD0010080, 0xFFFF8080, "%zp, %yp" },
	{ "vabs.q",	 0xD0018080, 0xFFFF8080, "%zq, %yq" },
	{ "vabs.s",	 0xD0010000, 0xFFFF8080, "%zs, %ys" },
	{ "vabs.t",	 0xD0018000, 0xFFFF8080, "%zt, %yt" },
	{ "vadd.p",	 0x60000080, 0xFF808080, "%zp, %yp, %xp" },
	{ "vadd.q",	 0x60008080, 0xFF808080, "%zq, %yq, %xq" },
	{ "vadd.s",	 0x60000000, 0xFF808080, "%zs, %yz, %xs" },
	{ "vadd.t",	 0x60008000, 0xFF808080, "%zt, %yt, %xt" },
	{ "vasin.p", 0xD0170080, 0xFFFF8080, "%zp, %yp" },
	{ "vasin.q", 0xD0178080, 0xFFFF8080, "%zq, %yq" },
	{ "vasin.s", 0xD0170000, 0xFFFF8080, "%zs, %ys" },
	{ "vasin.t", 0xD0178000, 0xFFFF8080, "%zt, %yt" },
	{ "vavg.p",	 0xD0470080, 0xFFFF8080, "%zp, %yp" },
	{ "vavg.q",	 0xD0478080, 0xFFFF8080, "%zq, %yq" },
	{ "vavg.t",	 0xD0478000, 0xFFFF8080, "%zt, %yt" },
	{ "vbfy1.p", 0xD0420080, 0xFFFF8080, "%zp, %yp" },
	{ "vbfy1.q", 0xD0428080, 0xFFFF8080, "%zq, %yq" },
	{ "vbfy2.q", 0xD0438080, 0xFFFF8080, "%zq, %yq" },
	{ "vcmovf.p", 0xD2A80080, 0xFFF88080, "" },
	{ "vcmovf.q",0xD2A88080, 0xFFF88080, "" },
	{ "vcmovf.s", 0xD2A80000, 0xFFF88080, "" },
	{ "vcmovf.t",0xD2A88000, 0xFFF88080, "" },
	{ "vcmovt.p", 0xD2A00080, 0xFFF88080, "" },
	{ "vcmovt.q",0xD2A08080, 0xFFF88080, "" },
	{ "vcmovt.s", 0xD2A00000, 0xFFF88080, "" },
	{ "vcmovt.t",0xD2A08000, 0xFFF88080, "" },
	{ "vcmp.p",	 0x6C000080, 0xFF8080F0, "" },
	{ "vcmp.p",	 0x6C000080, 0xFFFF80F0, "" },
	{ "vcmp.p",	 0x6C000080, 0xFFFFFFF0, "" },
	{ "vcmp.q",	 0x6C008080, 0xFF8080F0, "" },
	{ "vcmp.q",	 0x6C008080, 0xFFFF80F0, "" },
	{ "vcmp.q",	 0x6C008080, 0xFFFFFFF0, "" },
	{ "vcmp.s",	 0x6C000000, 0xFF8080F0, "" },
	{ "vcmp.s",	 0x6C000000, 0xFFFF80F0, "" },
	{ "vcmp.s",	 0x6C000000, 0xFFFFFFF0, "" },
	{ "vcmp.t",	 0x6C008000, 0xFF8080F0, "" },
	{ "vcmp.t",	 0x6C008000, 0xFFFF80F0, "" },
	{ "vcmp.t",	 0x6C008000, 0xFFFFFFF0, "" },
	{ "vcos.p",	 0xD0130080, 0xFFFF8080, "%zp, %yp" },
	{ "vcos.q",	 0xD0138080, 0xFFFF8080, "%zq, %yq" },
	{ "vcos.s",	 0xD0130000, 0xFFFF8080, "%zs, %ys" },
	{ "vcos.t",	 0xD0138000, 0xFFFF8080, "%zt, %yt" },
	{ "vcrs.t",	 0x66808000, 0xFF808080, "%zt, %yt, %xt" },
	{ "vcrsp.t", 0xF2808000, 0xFF808080, "%zt, %yt, %xt" },
	{ "vcst.p",	 0xD0600080, 0xFFE0FF80, "%zp, %yp, %xp" },
	{ "vcst.q",	 0xD0608080, 0xFFE0FF80, "%zq, %yq, %xq" },
	{ "vcst.s",	 0xD0600000, 0xFFE0FF80, "%zs, %ys, %xs" },
	{ "vcst.t",	 0xD0608000, 0xFFE0FF80, "%zt, %yt, %xt" },
	{ "vdet.p",	 0x67000080, 0xFF808080, "%zs, %yp, %xp" },
	{ "vdiv.p",	 0x63800080, 0xFF808080, "%zp, %yp, %xp" },
	{ "vdiv.q",	 0x63808080, 0xFF808080, "%zq, %yq, %xq" },
	{ "vdiv.s",	 0x63800000, 0xFF808080, "%zs, %yz, %xs" },
	{ "vdiv.t",	 0x63808000, 0xFF808080, "%zt, %yt, %xt" },
	{ "vdot.p",	 0x64800080, 0xFF808080, "%zs, %yp, %xp" },
	{ "vdot.q",	 0x64808080, 0xFF808080, "%zs, %yq, %xq" },
	{ "vdot.t",	 0x64808000, 0xFF808080, "%zs, %yt, %xt" },
	{ "vexp2.p", 0xD0140080, 0xFFFF8080, "%zp, %yp" },
	{ "vexp2.q", 0xD0148080, 0xFFFF8080, "%zq, %yq" },
	{ "vexp2.s", 0xD0140000, 0xFFFF8080, "%zs, %ys" },
	{ "vexp2.t", 0xD0148000, 0xFFFF8080, "%zt, %yt" },
	{ "vf2h.p",	 0xD0320080, 0xFFFF8080, "%zp, %yp" },
	{ "vf2h.q",	 0xD0328080, 0xFFFF8080, "%zq, %yq" },
	{ "vf2id.p", 0xD2600080, 0xFFE08080, "" },
	{ "vf2id.q", 0xD2608080, 0xFFE08080, "" },
	{ "vf2id.s", 0xD2600000, 0xFFE08080, "" },
	{ "vf2id.t", 0xD2608000, 0xFFE08080, "" },
	{ "vf2in.p", 0xD2000080, 0xFFE08080, "" },
	{ "vf2in.q", 0xD2008080, 0xFFE08080, "" },
	{ "vf2in.s", 0xD2000000, 0xFFE08080, "" },
	{ "vf2in.t", 0xD2008000, 0xFFE08080, "" },
	{ "vf2iu.p", 0xD2400080, 0xFFE08080, "" },
	{ "vf2iu.q", 0xD2408080, 0xFFE08080, "" },
	{ "vf2iu.s", 0xD2400000, 0xFFE08080, "" },
	{ "vf2iu.t", 0xD2408000, 0xFFE08080, "" },
	{ "vf2iz.p", 0xD2200080, 0xFFE08080, "" },
	{ "vf2iz.q", 0xD2208080, 0xFFE08080, "" },
	{ "vf2iz.s", 0xD2200000, 0xFFE08080, "" },
	{ "vf2iz.t", 0xD2208000, 0xFFE08080, "" },
	{ "vfad.p",	 0xD0460080, 0xFFFF8080, "%zp, %yp" },
	{ "vfad.q",	 0xD0468080, 0xFFFF8080, "%zq, %yq" },
	{ "vfad.t",	 0xD0468000, 0xFFFF8080, "%zt, %yt" },
	{ "vfim.s",	 0xDF800000, 0xFF800000, "" },
	{ "vflush",	 0xFFFF040D, 0xFFFFFFFF, "" },
	{ "vh2f.p",	 0xD0330080, 0xFFFF8080, "%zp, %yp" },
	{ "vh2f.s",	 0xD0330000, 0xFFFF8080, "%zs, %ys" },
	{ "vhdp.p",	 0x66000080, 0xFF808080, "" },
	{ "vhdp.q",	 0x66008080, 0xFF808080, "" },
	{ "vhdp.t",	 0x66008000, 0xFF808080, "" },
	{ "vhtfm2.p", 0xF0800000, 0xFF808080, "" },
	{ "vhtfm3.t",0xF1000080, 0xFF808080, "" },
	{ "vhtfm4.q",0xF1808000, 0xFF808080, "" },
	{ "vi2c.q",	 0xD03D8080, 0xFFFF8080, "" },
	{ "vi2f.p",	 0xD2800080, 0xFFE08080, "" },
	{ "vi2f.q",	 0xD2808080, 0xFFE08080, "" },
	{ "vi2f.s",	 0xD2800000, 0xFFE08080, "" },
	{ "vi2f.t",	 0xD2808000, 0xFFE08080, "" },
	{ "vi2s.p",	 0xD03F0080, 0xFFFF8080, "" },
	{ "vi2s.q",	 0xD03F8080, 0xFFFF8080, "" },
	{ "vi2uc.q", 0xD03C8080, 0xFFFF8080, "%zq, %yq" },
	{ "vi2us.p", 0xD03E0080, 0xFFFF8080, "%zq, %yq" },
	{ "vi2us.q", 0xD03E8080, 0xFFFF8080, "%zq, %yq" },
	{ "vidt.p",	 0xD0030080, 0xFFFFFF80, "%zp" },
	{ "vidt.q",	 0xD0038080, 0xFFFFFF80, "%zq" },
	{ "viim.s",	 0xDF000000, 0xFF800000, "" },
	{ "vlgb.s",	 0xD0370000, 0xFFFF8080, "%zs, %ys" },
	{ "vlog2.p", 0xD0150080, 0xFFFF8080, "%zp, %yp" },
	{ "vlog2.q", 0xD0158080, 0xFFFF8080, "%zq, %yq" },
	{ "vlog2.s", 0xD0150000, 0xFFFF8080, "%zs, %ys" },
	{ "vlog2.t", 0xD0158000, 0xFFFF8080, "%zt, %yt" },
	{ "vmax.p",	 0x6D800080, 0xFF808080, "%zp, %yp, %xp" },
	{ "vmax.q",	 0x6D808080, 0xFF808080, "%zq, %yq, %xq" },
	{ "vmax.s",	 0x6D800000, 0xFF808080, "%zs, %ys, %xs" },
	{ "vmax.t",	 0x6D808000, 0xFF808080, "%zt, %yt, %xt" },
	{ "vmfvc",	 0xD0500000, 0xFFFF0080, "" },
	{ "vmidt.p", 0xF3830080, 0xFFFFFF80, "%zp" },
	{ "vmidt.q", 0xF3838080, 0xFFFFFF80, "%zq" },
	{ "vmidt.t", 0xF3838000, 0xFFFFFF80, "%zt" },
	{ "vmin.p",	 0x6D000080, 0xFF808080, "%zp, %yp, %xp" },
	{ "vmin.q",	 0x6D008080, 0xFF808080, "%zq, %yq, %xq" },
	{ "vmin.s",	 0x6D000000, 0xFF808080, "%zs, %ys, %xs" },
	{ "vmin.t",	 0x6D008000, 0xFF808080, "%zt, %yt, %xt" },
	{ "vmmov.p", 0xF3800080, 0xFFFF8080, "" },
	{ "vmmov.q", 0xF3808080, 0xFFFF8080, "%zo, %yo" },
	{ "vmmov.t", 0xF3808000, 0xFFFF8080, "" },
	{ "vmmul.p", 0xF0000080, 0xFF808080, "" },
	{ "vmmul.q", 0xF0008080, 0xFF808080, "%?%zo, %yo, %xo" },
	{ "vmmul.t", 0xF0008000, 0xFF808080, "" },
	{ "vmone.p", 0xF3870080, 0xFFFFFF80, "%zp" },
	{ "vmone.q", 0xF3878080, 0xFFFFFF80, "%zq" },
	{ "vmone.t", 0xF3878000, 0xFFFFFF80, "%zt" },
	{ "vmov.p",	 0xD0000080, 0xFFFF8080, "%zp, %yp" },
	{ "vmov.q",	 0xD0008080, 0xFFFF8080, "%zq, %yq" },
	{ "vmov.s",	 0xD0000000, 0xFFFF8080, "%zs, %ys" },
	{ "vmov.t",	 0xD0008000, 0xFFFF8080, "%zt, %yt" },
	{ "vmscl.p", 0xF2000080, 0xFF808080, "%zp, %yp, %xp" },
	{ "vmscl.q", 0xF2008080, 0xFF808080, "%zq, %yq, %xq" },
	{ "vmscl.t", 0xF2008000, 0xFF808080, "%zt, %yt, %xt" },
	{ "vmtvc",	 0xD0510000, 0xFFFF8000, "" },
	{ "vmul.p",	 0x64000080, 0xFF808080, "%zp, %yp, %xp" },
	{ "vmul.q",	 0x64008080, 0xFF808080, "%zq, %yq, %xq" },
	{ "vmul.s",	 0x64000000, 0xFF808080, "%zs, %ys, %xs" },
	{ "vmul.t",	 0x64008000, 0xFF808080, "%zt, %yt, %xt" },
	{ "vmzero.p", 0xF3860080, 0xFFFFFF80, "%zp" },
	{ "vmzero.q",0xF3868080, 0xFFFFFF80, "%zq" },
	{ "vmzero.t",0xF3868000, 0xFFFFFF80, "%zt" },
	{ "vneg.p",	 0xD0020080, 0xFFFF8080, "%zp, %yp" },
	{ "vneg.q",	 0xD0028080, 0xFFFF8080, "%zq, %yq" },
	{ "vneg.s",	 0xD0020000, 0xFFFF8080, "%zs, %ys" },
	{ "vneg.t",	 0xD0028000, 0xFFFF8080, "%zt, %yt" },
	{ "vnop",	 0xFFFF0000, 0xFFFFFFFF, "" },
	{ "vnrcp.p", 0xD0180080, 0xFFFF8080, "%zp, %yp" },
	{ "vnrcp.q", 0xD0188080, 0xFFFF8080, "%zq, %yq" },
	{ "vnrcp.s", 0xD0180000, 0xFFFF8080, "%zs, %ys" },
	{ "vnrcp.t", 0xD0188000, 0xFFFF8080, "%zt, %yt" },
	{ "vnsin.p", 0xD01A0080, 0xFFFF8080, "%zp, %yp" },
	{ "vnsin.q", 0xD01A8080, 0xFFFF8080, "%zq, %yq" },
	{ "vnsin.s", 0xD01A0000, 0xFFFF8080, "%zs, %ys" },
	{ "vnsin.t", 0xD01A8000, 0xFFFF8080, "%zt, %yt" },
	{ "vocp.p",	 0xD0440080, 0xFFFF8080, "%zp, %yp" },
	{ "vocp.q",	 0xD0448080, 0xFFFF8080, "%zq, %yq" },
	{ "vocp.s",	 0xD0440000, 0xFFFF8080, "%zs, %ys" },
	{ "vocp.t",	 0xD0448000, 0xFFFF8080, "%zt, %yt" },
	{ "vone.p",	 0xD0070080, 0xFFFFFF80, "%zp" },
	{ "vone.q",	 0xD0078080, 0xFFFFFF80, "%zq" },
	{ "vone.s",	 0xD0070000, 0xFFFFFF80, "%zs" },
	{ "vone.t",	 0xD0078000, 0xFFFFFF80, "%zt" },
	{ "vpfxd",	 0xDE000000, 0xFF000000, "" },
	{ "vpfxs",	 0xDC000000, 0xFF000000, "" },
	{ "vpfxt",	 0xDD000000, 0xFF000000, "" },
	{ "vqmul.q", 0xF2808080, 0xFF808080, "" },
	{ "vrcp.p",	 0xD0100080, 0xFFFF8080, "%zp, %yp" },
	{ "vrcp.q",	 0xD0108080, 0xFFFF8080, "%zq, %yq" },
	{ "vrcp.s",	 0xD0100000, 0xFFFF8080, "%zs, %ys" },
	{ "vrcp.t",	 0xD0108000, 0xFFFF8080, "%zt, %yt" },
	{ "vrexp2.p",0xD01C0080, 0xFFFF8080, "%zp, %yp" },
	{ "vrexp2.q",0xD01C8080, 0xFFFF8080, "%zq, %yq" },
	{ "vrexp2.s", 0xD01C0000, 0xFFFF8080, "%zs, %ys" },
	{ "vrexp2.t",0xD01C8000, 0xFFFF8080, "%zt, %yt" },
	{ "vrndf1.p", 0xD0220080, 0xFFFFFF80, "%zp" },
	{ "vrndf1.q",0xD0228080, 0xFFFFFF80, "%zq" },
	{ "vrndf1.s", 0xD0220000, 0xFFFFFF80, "%zs" },
	{ "vrndf1.t",0xD0228000, 0xFFFFFF80, "%zt" },
	{ "vrndf2.p", 0xD0230080, 0xFFFFFF80, "%zp" },
	{ "vrndf2.q",0xD0238080, 0xFFFFFF80, "%zq" },
	{ "vrndf2.s", 0xD0230000, 0xFFFFFF80, "%zs" },
	{ "vrndf2.t",0xD0238000, 0xFFFFFF80, "%zt" },
	{ "vrndi.p", 0xD0210080, 0xFFFFFF80, "%zp" },
	{ "vrndi.q", 0xD0218080, 0xFFFFFF80, "%zq" },
	{ "vrndi.s", 0xD0210000, 0xFFFFFF80, "%zs" },
	{ "vrndi.t", 0xD0218000, 0xFFFFFF80, "%zt" },
	{ "vrnds.s", 0xD0200000, 0xFFFF80FF, "%ys" },
	{ "vrot.p",	 0xF3A00080, 0xFFE08080, "" },
	{ "vrot.q",	 0xF3A08080, 0xFFE08080, "" },
	{ "vrot.t",	 0xF3A08000, 0xFFE08080, "" },
	{ "vrsq.p",	 0xD0110080, 0xFFFF8080, "%zp, %yp" },
	{ "vrsq.q",	 0xD0118080, 0xFFFF8080, "%zq, %yq" },
	{ "vrsq.s",	 0xD0110000, 0xFFFF8080, "%zs, %ys" },
	{ "vrsq.t",	 0xD0118000, 0xFFFF8080, "%zt, %yt" },
	{ "vs2i.p",	 0xD03B0080, 0xFFFF8080, "%zp, %yp" },
	{ "vs2i.s",	 0xD03B0000, 0xFFFF8080, "%zs, %ys" },
	{ "vsat0.p", 0xD0040080, 0xFFFF8080, "%zp, %yp" },
	{ "vsat0.q", 0xD0048080, 0xFFFF8080, "%zq, %yq" },
	{ "vsat0.s", 0xD0040000, 0xFFFF8080, "%zs, %ys" },
	{ "vsat0.t", 0xD0048000, 0xFFFF8080, "%zt, %yt" },
	{ "vsat1.p", 0xD0050080, 0xFFFF8080, "%zp, %yp" },
	{ "vsat1.q", 0xD0058080, 0xFFFF8080, "%zq, %yq" },
	{ "vsat1.s", 0xD0050000, 0xFFFF8080, "%zs, %ys" },
	{ "vsat1.t", 0xD0058000, 0xFFFF8080, "%zt, %yt" },
	{ "vsbn.s",	 0x61000000, 0xFF808080, "%zs, %ys, %xs" },
	{ "vsbz.s",	 0xD0360000, 0xFFFF8080, "%zs, %ys" },
	{ "vscl.p",	 0x65000080, 0xFF808080, "%zp, %yp, %xp" },
	{ "vscl.q",	 0x65008080, 0xFF808080, "%zq, %yq, %xq" },
	{ "vscl.t",	 0x65008000, 0xFF808080, "%zt, %yt, %xt" },
	{ "vscmp.p", 0x6E800080, 0xFF808080, "%zp, %yp, %xp" },
	{ "vscmp.q", 0x6E808080, 0xFF808080, "%zq, %yq, %xq" },
	{ "vscmp.s", 0x6E800000, 0xFF808080, "%zs, %ys, %xs" },
	{ "vscmp.t", 0x6E808000, 0xFF808080, "%zt, %yt, %xt" },
	{ "vsge.p",	 0x6F000080, 0xFF808080, "%zp, %yp, %xp" },
	{ "vsge.q",	 0x6F008080, 0xFF808080, "%zq, %yq, %xq" },
	{ "vsge.s",	 0x6F000000, 0xFF808080, "%zs, %ys, %xs" },
	{ "vsge.t",	 0x6F008000, 0xFF808080, "%zt, %yt, %xt" },
	{ "vsgn.p",	 0xD04A0080, 0xFFFF8080, "%zp, %yp" },
	{ "vsgn.q",	 0xD04A8080, 0xFFFF8080, "%zq, %yq" },
	{ "vsgn.s",	 0xD04A0000, 0xFFFF8080, "%zs, %ys" },
	{ "vsgn.t",	 0xD04A8000, 0xFFFF8080, "%zt, %yt" },
	{ "vsin.p",	 0xD0120080, 0xFFFF8080, "%zp, %yp" },
	{ "vsin.q",	 0xD0128080, 0xFFFF8080, "%zq, %yq" },
	{ "vsin.s",	 0xD0120000, 0xFFFF8080, "%zs, %ys" },
	{ "vsin.t",	 0xD0128000, 0xFFFF8080, "%zt, %yt" },
	{ "vslt.p",	 0x6F800080, 0xFF808080, "%zp, %yp, %xp" },
	{ "vslt.q",	 0x6F808080, 0xFF808080, "%zq, %yq, %xq" },
	{ "vslt.s",	 0x6F800000, 0xFF808080, "%zs, %ys, %xs" },
	{ "vslt.t",	 0x6F808000, 0xFF808080, "%zt, %yt, %xt" },
	{ "vsocp.p", 0xD0450080, 0xFFFF8080, "%zp, %yp" },
	{ "vsocp.s", 0xD0450000, 0xFFFF8080, "%zs, %ys" },
	{ "vsqrt.p", 0xD0160080, 0xFFFF8080, "%zp, %yp" },
	{ "vsqrt.q", 0xD0168080, 0xFFFF8080, "%zq, %yq" },
	{ "vsqrt.s", 0xD0160000, 0xFFFF8080, "%zs, %ys" },
	{ "vsqrt.t", 0xD0168000, 0xFFFF8080, "%zt, %yt" },
	{ "vsrt1.q", 0xD0408080, 0xFFFF8080, "%zq, %yq" },
	{ "vsrt2.q", 0xD0418080, 0xFFFF8080, "%zq, %yq" },
	{ "vsrt3.q", 0xD0488080, 0xFFFF8080, "%zq, %yq" },
	{ "vsrt4.q", 0xD0498080, 0xFFFF8080, "%zq, %yq" },
	{ "vsub.p",	 0x60800080, 0xFF808080, "%zp, %yp, %xp" },
	{ "vsub.q",	 0x60808080, 0xFF808080, "%zq, %yq, %xq" },
	{ "vsub.s",	 0x60800000, 0xFF808080, "%zs, %ys, %xs" },
	{ "vsub.t",	 0x60808000, 0xFF808080, "%zt, %yt, %xt" },
	{ "vsync",	 0xFFFF0000, 0xFFFF0000, "%I" },
	{ "vsync",	 0xFFFF0320, 0xFFFFFFFF, "" },
	{ "vt4444.q",0xD0598080, 0xFFFF8080, "%zq, %yq" },
	{ "vt5551.q",0xD05A8080, 0xFFFF8080, "%zq, %yq" },
	{ "vt5650.q",0xD05B8080, 0xFFFF8080, "%zq, %yq" },
	{ "vtfm2.p", 0xF0800080, 0xFF808080, "" },
	{ "vtfm3.t", 0xF1008000, 0xFF808080, "" },
	{ "vtfm4.q", 0xF1808080, 0xFF808080, "" },
	{ "vus2i.p", 0xD03A0080, 0xFFFF8080, "" },
	{ "vus2i.s", 0xD03A0000, 0xFFFF8080, "" },
	{ "vwb.q",	 0xF8000002, 0xFC000002, "" },
	{ "vwbn.s",	 0xD3000000, 0xFF008080, "" },
	{ "vzero.p", 0xD0060080, 0xFFFFFF80, "%zp" },
	{ "vzero.q", 0xD0068080, 0xFFFFFF80, "%zq" },
	{ "vzero.s", 0xD0060000, 0xFFFFFF80, "%zs" },
	{ "vzero.t", 0xD0068000, 0xFFFFFF80, "%zt" },*/
	};

/* Mnemonic register names */
const char *regName[32] =
{
    "zero", "at", "v0", "v1", "a0", "a1", "a2", "a3",
    "t0",   "t1", "t2", "t3", "t4", "t5", "t6", "t7", 
    "s0",   "s1", "s2", "s3", "s4", "s5", "s6", "s7",
    "t8",   "t9", "k0", "k1", "gp", "sp", "fp", "ra"
};

unsigned int regColors[33] =
{
	0xFFBBBBBB, 0xFFCCCCCC, 0xFFFFFF00, 0xFFBBBB00, 0xFFEE0000, 0xFFBB0000, 0xFF880000, 0xFF550000,
	0xFF00C010, 0xFF00B020, 0xFF00A030, 0xFF009040, 0xFF008050, 0xFF007060, 0xFF006070, 0xFF005080,
	0xFF1000F0, 0xFF2000E0, 0xFF3000D0, 0xFF4000C0, 0xFF5000B0, 0xFF6000A0, 0xFF700090, 0xFF800080,
	0xFF004090, 0xFF0030A0, 0xFF0000B0, 0xFF0000D0, 0xFF008888, 0xFF00BBBB, 0xFF900070, 0xFF00FFFF,
	0xFF999999
};

static const char *cop0_regs[32] = 
{
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
	"BadVaddr", "Count", NULL, "Compare", "Status", "Cause", "EPC", "PrID",
	"Config", NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, "EBase", NULL, NULL, "TagLo", "TagHi", "ErrorPC", NULL
};

static const char *dr_regs[16] = 
{
	"DRCNTL", "DEPC", "DDATA0", "DDATA1", "IBC", "DBC", NULL, NULL, 
	"IBA", "IBAM", NULL, NULL, "DBA", "DBAM", "DBD", "DBDM"
};

static const char *special_regs[32] =
{
	"ixltg", "ixldt", "bxlbt", "$3", "ixstg", "ixsdt", "bxsbt", "ixin",
	"bpr", "$9", "bhinbt", "ihin", "bfh", "$13", "ifl", "$15", "dxltg",
	"dxldt", "dxstg", "dxsdt", "dxwbin", "$21", "dxin", "$23", "dhwbinv",
	"$25", "dhin", "$27", "dhwoin", "$29", "$30", "$31"
};

/* TODO: Add a register state block so we can convert lui/addiu to li */

static int g_hexints = 0;
static int g_mregs = 0;
static int g_symaddr = 0;
static int g_macro = 0;
static int g_printreal = 0;
static int g_printregs = 0;
static int g_regmask = 0;
static SymResolve g_symresolver = NULL;

void print_dollar() {
	pspDebugScreenSetTextColor(regColors[32]);
	pspDebugScreenPrintf("$");
	pspDebugScreenSetTextColor(colors.color02);
}

void print_cpureg(int reg)
{
	if(!g_mregs)
	{
		pspDebugScreenSetTextColor(regColors[reg]);
		pspDebugScreenPuts(regName[reg]);
		pspDebugScreenSetTextColor(colors.color02);
	}
	else
	{
		print_dollar();
		pspDebugScreenPrintf("%02d", reg);
	}

	if(g_printregs)
	{
		g_regmask |= (1 << reg);
	}
}

void print_int(int i)
{
	//print_dollar();
	pspDebugScreenPrintf("%i", i);
}

void print_hex(int i)
{
	print_dollar();
	pspDebugScreenPrintf("%08X", i);
}

void print_imm(int ofs)
{
	print_dollar();

	if(g_hexints)
	{
		unsigned int val = ofs;
		val &= 0xFFFF;
		pspDebugScreenPrintf("%04X", MEM_SHORT(val));
	}
	else
	{
		pspDebugScreenPrintf("%04X", MEM_SHORT(ofs));
	}
}

void print_jump(unsigned int addr)
{
	char symbol[128];
	int symfound = 0;

	if(g_symresolver)
	{
		symfound = g_symresolver(addr, symbol, sizeof(symbol));
	}

	if(symfound)
	{
		pspDebugScreenPrintf("%s", symbol);
	}
	else
	{
		print_dollar();
		pspDebugScreenPrintf("%08X", addr);
	}
}

void print_ofs(int ofs, int reg, unsigned int *realregs)
{
	if((g_printreal) && (realregs))
	{
		print_jump(realregs[reg] + ofs);
	}
	else
	{
		print_dollar();

		if(g_hexints)
		{
			unsigned int val = ofs;
			val &= 0xFFFF;
			pspDebugScreenPrintf("%04X(", MEM_SHORT(val));
		}
		else
		{
			pspDebugScreenPrintf("%04X(", MEM_SHORT(ofs));
		}

		print_cpureg(reg);
		pspDebugScreenPrintf(")");
	}
}

void print_pcofs(int ofs, unsigned int PC)
{
	if(PC == 0)
	{
		print_dollar();
		pspDebugScreenPrintf("%04X", MEM_SHORT(ofs));
	}
	else
	{
		ofs = (ofs + 1) * 4;
		print_jump(PC + ofs);
	}
}

void print_jumpr(int reg, unsigned int *realregs)
{
	if((g_printreal) && (realregs))
	{
		return print_jump(realregs[reg]);
	}
	else
	{
		return print_cpureg(reg);
	}
}

void print_syscall(unsigned int syscall)
{
	print_dollar();
	pspDebugScreenPrintf("%X", syscall);
}

void print_cop0(int reg)
{
	if(cop0_regs[reg])
	{
		pspDebugScreenSetTextColor(regColors[reg]);
		pspDebugScreenPrintf("%s", cop0_regs[reg]);
		pspDebugScreenSetTextColor(colors.color02);
	}
	else
	{
		print_dollar();
		pspDebugScreenPrintf("%d", reg);
	}
}

void print_cop1(int reg)
{
	pspDebugScreenSetTextColor(regColors[reg]);
	pspDebugScreenPrintf("$f%d", reg);
	pspDebugScreenSetTextColor(colors.color02);
}

void print_fpureg(int reg)
{
	pspDebugScreenSetTextColor(regColors[reg]);
	pspDebugScreenPrintf("$f%d", reg);
	pspDebugScreenSetTextColor(colors.color02);
}

void print_specialreg(int reg)
{
	pspDebugScreenPrintf("%s", special_regs[reg]);
}

void print_debugreg(int reg)
{
	if((reg < 16) && (dr_regs[reg]))
	{
		//pspDebugScreenSetTextColor(regColors[reg]);
		pspDebugScreenPrintf("%s", dr_regs[reg]);
	}
	else
	{
		print_dollar();
		pspDebugScreenPrintf("%02d\n", reg);
	}
}

/*void print_vfpusingle(int reg)
{
	pspDebugScreenPrintf("S%d%d%d", (reg >> 2) & 7, reg & 3, (reg >> 5) & 3);
}

void print_vfpu_reg(int reg, int offset, char one, char two)
{
	if((reg >> 5) & 1)
	{
		pspDebugScreenPrintf("%c%d%d%d", two, (reg >> 2) & 7, offset, reg & 3);
	}
	else
	{
		pspDebugScreenPrintf("%c%d%d%d", one, (reg >> 2) & 7, reg & 3, offset);
	}
}

void print_vfpuquad(int reg)
{
	return print_vfpu_reg(reg, 0, 'C', 'R');
}

void print_vfpupair(int reg)
{
	if((reg >> 6) & 1)
	{
		return print_vfpu_reg(reg, 2, 'C', 'R');
	}
	else
	{
		return print_vfpu_reg(reg, 0, 'C', 'R');
	}
}

void print_vfputriple(int reg)
{
	if((reg >> 6) & 1)
	{
		return print_vfpu_reg(reg, 1, 'C', 'R');
	}
	else
	{
		return print_vfpu_reg(reg, 0, 'C', 'R');
	}
}

void print_vfpumpair(int reg)
{
	if((reg >> 6) & 1)
	{
		return print_vfpu_reg(reg, 2, 'M', 'E');
	}
	else
	{
		return print_vfpu_reg(reg, 0, 'M', 'E');
	}
}

void print_vfpumtriple(int reg)
{
	if((reg >> 6) & 1)
	{
		return print_vfpu_reg(reg, 1, 'M', 'E');
	}
	else
	{
		return print_vfpu_reg(reg, 0, 'M', 'E');
	}
}

void print_vfpumatrix(int reg)
{
	return print_vfpu_reg(reg, 0, 'M', 'E');
}

void print_vfpureg(int reg, char type)
{
	switch(type)
	{
		case 's': return print_vfpusingle(reg);
				  break;
		case 'q': return print_vfpuquad(reg);
				  break;
		case 'p': return print_vfpupair(reg);
				  break;
		case 't': return print_vfputriple(reg);
				  break;
		case 'm': return print_vfpumpair(reg);
				  break;
		case 'n': return print_vfpumtriple(reg);
				  break;
		case 'o': return print_vfpumatrix(reg);
				  break;
		default: break;
	};
}*/

static void decode_args(unsigned int opcode, unsigned int PC, const char *fmt, unsigned int *realregs)
{
	int i = 0;
	int vmmul = 0;

	while(fmt[i])
	{
		if(fmt[i] == '%')
		{
			i++;
			switch(fmt[i])
			{
				case 'd': print_cpureg(RD(opcode));
						  break;
				case 't': print_cpureg(RT(opcode));
						  break;
				case 's': print_cpureg(RS(opcode));
						  break;
				case 'i': print_imm(IMM(opcode));
						  break;
				case 'I': print_imm(IMMU(opcode));
						  break;
				case 'o': print_ofs(IMM(opcode), RS(opcode), realregs);
						  break;
				case 'O': print_pcofs(IMM(opcode), PC);
						  break;
				case 'V': print_ofs(IMM(opcode), RS(opcode), realregs);
						  break;
				case 'j': print_jump(JUMP(opcode, PC));
						  break;
				case 'J': print_jumpr(RS(opcode), realregs);
						  break;
				case 'a': print_int(SA(opcode));
						  break;
				case '0': print_cop0(RD(opcode));
						  break;
				case '1': print_cop1(RD(opcode));
						  break;
				case 'p': print_int(RD(opcode));
						  break;
				case 'k': print_specialreg(RT(opcode));
						  break;
				case 'D': print_fpureg(FD(opcode));
						  break;
				case 'T': print_fpureg(FT(opcode));
						  break;
				case 'S': print_fpureg(FS(opcode));
						  break;
				case 'r': print_debugreg(RD(opcode));
						  break;
				case 'n': print_int(RD(opcode) + 1);
						  break;
				/*case 'x': if(fmt[i+1]) { print_vfpureg(VT(opcode), fmt[i+1]); i++; }break;
				case 'y': if(fmt[i+1]) { 
							  int reg = VS(opcode);
							  if(vmmul) { if(reg & 0x20) { reg &= 0x5F; } else { reg |= 0x20; } }
							  print_vfpureg(reg, fmt[i+1]); i++; 
							  }
							  break;
				case 'z': if(fmt[i+1]) { print_vfpureg(VD(opcode), fmt[i+1]); i++; }break;
				case 'v': break;
				case 'X': if(fmt[i+1]) { print_vfpureg(VO(opcode), fmt[i+1]); i++; }
						  break;*/
				case 'Z': print_imm(VCC(opcode));
						  break;
				case 'c': print_hex(CODE(opcode));
						  break;
				case 'C': print_syscall(CODE(opcode));
						  break;
				case 'Y': print_ofs(IMM(opcode) & ~3, RS(opcode), realregs);
						  break;
				case '?': vmmul = 1;
						  break;
				case 0: goto end;
				default: break;
			};
			i++;
		}
		else
		{
			pspDebugScreenPrintf("%c", fmt[i++]);
			//*output++ = fmt[i++];
		}
	}
end:
	return;
}

const char *disasmInstruction(unsigned int opcode, unsigned int PC, unsigned int *realregs, unsigned int *regmask)
{
	static char code[256];
	char addr[128];
	int size;
	int i;
	struct Instruction *ix = NULL;

	if((g_symresolver) && (g_symaddr))
	{
		char addrtemp[128];
		/* Symbol resolver shouldn't touch addr unless it finds symbol */
		if(g_symresolver(PC, addrtemp, sizeof(addrtemp)))
		{
			sprintf(addr, "%-20s", addrtemp);
		}
	}

	g_regmask = 0;

	if(!g_macro)
	{
		size = sizeof(macro) / sizeof(struct Instruction);
		for(i = 0; i < size; i++)
		{
			if((opcode & macro[i].mask) == macro[i].opcode)
			{
				ix = &macro[i];
			}
		}
	}

	if(!ix)
	{
		size = sizeof(inst) / sizeof(struct Instruction);
		for(i = 0; i < size; i++)
		{
			if((opcode & inst[i].mask) == inst[i].opcode)
			{
				ix = &inst[i];
			}
		}
	}

	if(ix)
	{
		pspDebugScreenPrintf("%-10s", ix->name);
		pspDebugScreenSetTextColor(colors.color02);
		decode_args(opcode, PC, ix->fmt, realregs);

		if(regmask) 
		{
			*regmask = g_regmask;
		}
	}
	else
	{
		pspDebugScreenPrintf("???");
	}

	return code;
}

void mipsDecode(unsigned int opcode, unsigned int PC) {
	unsigned int realregs[20];
	unsigned int regmask;

	disasmInstruction(opcode, (PC & 0x0FFFFFFF), &realregs, &regmask);
	pspDebugScreenSetTextColor(colors.color02);
}

#endif