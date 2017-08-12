#ifndef UTILS_H
#define UTILS_H

#include <pspctrl.h>
#include <pspiofilemgr.h>
#include <pspsysmem_kernel.h>

#define MAKE_JUMP(f) (0x08000000 | (((u32)(f) >> 2) & 0x03ffffff))
#define MAKE_CALL(f) (0x0C000000 | (((u32)(f) >> 2) & 0x03ffffff)) 
#define NOP 0

typedef struct {
	const char *name;
	unsigned short version;
	unsigned short attribute;
	unsigned char entLen;
	unsigned char varCount;
	unsigned short funcCount;
	unsigned int *fnids;
	unsigned int *funcs;
	unsigned int *vnids;
	unsigned int *vars;
} PspModuleImport;

void *sceKernelGetGameInfo();

u32 *find_export(const char *szMod, const char *szLib, u32 nid);

void *map_function(const char *szMod, const char *szLib, u32 nid, void *default_func);

void hook_export_bynid(const char *szMod, const char *szLib, u32 nid, void *func);

int hook_import_bynid(SceModule *pMod, char *szLib, unsigned int nid, void *func);

int hook_import_byfunc(SceModule *pMod, char *library, void *orig_func, void *func);

uidControlBlock *find_uid_control_block_by_name(const char *name, const char *parent);

SceUID load_start_module(const char *file);

void map_functions();

int _strnicmp(const char *pStr1, const char *pStr2, unsigned int count);

int dump_memory(const char *file, void *addr, int len);

int get_registry_value(const char *dir, const char *name, int *val);

int set_registry_value(const char *dir, const char *name, int *val);

#endif