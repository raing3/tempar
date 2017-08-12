#ifndef MEM_H
#define MEM_H

#define FLAG_BYTE      0x40
#define FLAG_WORD      0x80
#define FLAG_DWORD     0xC0

#define MEM_VALUE(x) (*(u8*)(x))
#define MEM_VALUE_SHORT(x) (*(u16*)(x))
#define MEM_VALUE_INT(x) (*(u32*)(x))

#define MEM(x) (u8)(x)
#define MEM_SHORT(x) (u16)(x)
#define MEM_INT(x) (u32)(x)

#define MEM_ASCII(x) (x < (u8)0x20 ? '.' : x)

#endif
