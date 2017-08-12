/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * disasm.h - PSPLINK disassembler code
 *
 * Copyright (c) 2006 James F <tyranid@gmail.com>
 *
 * $HeadURL: http://psp.jim.sh/svn/psp/trunk/psplink/psplink/disasm.h $
 * $Id: disasm.h 2021 2006-10-12 22:22:21Z tyranid $
 */
#ifndef DISASM_H
#define DISASM_H

#define DISASM_OPT_MAX       6
#define DISASM_OPT_HEXINTS   'x'
#define DISASM_OPT_MREGS     'r'
#define DISASM_OPT_SYMADDR   's'
#define DISASM_OPT_MACRO     'm'
#define DISASM_OPT_PRINTREAL 'p'
#define DISASM_OPT_PRINTREGS 'g'

/* Enable hexadecimal integers for immediates */
void disasmSetHexInts(int hexints);
/* Enable mnemonic MIPS registers */
void disasmSetMRegs(int mregs);
/* Enable resolving of PC to a symbol if available */
void disasmSetSymAddr(int symaddr);
/* Enable instruction macros */
void disasmSetMacro(int macro);
void disasmSetPrintReal(int printreal);
void disasmSetOpts(const char *opts, int set);
const char *disasmGetOpts(void);
void disasmPrintOpts(void);
const char *disasmInstruction(unsigned int opcode, unsigned int PC, unsigned int *realregs, unsigned int *regmask);

/* Symbol resolver function type */
typedef int (*SymResolve)(unsigned int addr, char *output, int size);
/* Set the symbol resolver function */
void disasmSetSymResolver(SymResolve symresolver);

void mipsDecode(unsigned int opcode, unsigned int PC);

#endif
