#include <pspsdk.h>
#include <pspreg.h>
#include <pspctrl_kernel.h>
#include <pspdisplay.h>
#include <pspdisplay_kernel.h>
#include <string.h>
#include <psputils.h>
#include "common.h"

u32 *find_export(const char *szMod, const char *szLib, u32 nid) {
	int i = 0;

	SceModule2 *pMod = sceKernelFindModuleByName(szMod);

	if(!pMod) {
		return 0;
	}

	void *entTab = pMod->ent_top;
	int entLen = pMod->ent_size;

	while(i < entLen) {
		int count;
		int total;
		u32 *vars;

		SceLibraryEntryTable *entry = (SceLibraryEntryTable *)(entTab + i);

		if(entry->libname && (!szLib || !strcmp( entry->libname, szLib))) {
			total = entry->stubcount + entry->vstubcount;
			vars = entry->entrytable;

			for(count = 0; count < entry->stubcount; count++) {
				if(vars[count] == nid) {
					log("find_export: export [%s, %s, 0x%08X] at 0x%08X found\n", szMod, szLib, nid, &vars[count + total]);
					return &vars[count + total];	
				}				
			}
		}

		i += (entry->len  *4);
	}

	return NULL;
}

void *map_function(const char *szMod, const char *szLib, u32 nid, void *default_func) {
	u32 *export = find_export(szMod, szLib, nid);

	if(export) {
		log("map_export: export [%s, %s, 0x%08X] at 0x%08X found\n", szMod, szLib, nid, *export);
		return (void*)*export;
	}

	return default_func;
}

void hook_export_bynid(const char *szMod, const char *szLib, u32 nid, void *func) {
	u32 *address = find_export(szMod, szLib, nid);

	if(address) {
		//address = *((u32*)address);
		_sw(MAKE_JUMP(func), *address);
		_sw(0, (u32)(*address + 4));

		sceKernelDcacheWritebackInvalidateRange((void*)address, 8);
		sceKernelIcacheInvalidateRange((void*)address, 8);
	}
}

int hook_import_bynid(SceModule *pMod, char *szLib, unsigned int nid, void *func) {
	PspModuleImport *pImp;
	void *stubTab;
	int stubLen;
	int i = 0;

	if(pMod == NULL)
		return -1;

	stubTab = pMod->stub_top;
	stubLen = pMod->stub_size;

	while(i<stubLen) {
		pImp = (PspModuleImport*)(stubTab+i);

		if((pImp->name) && (strcmp(pImp->name, szLib) == 0)) {
			int j;

			for(j=0; j<pImp->funcCount; j++) {
				if(pImp->fnids[j] == nid) {
					void *addr = (void*)(&pImp->funcs[j*2]);

					_sw(MAKE_JUMP(func), (u32)addr);
					_sw(0, (u32)(addr + 4));

					sceKernelDcacheWritebackInvalidateRange(addr, 8);
					sceKernelIcacheInvalidateRange(addr, 8);
				}
			}

		}

		i += (pImp->entLen * 4);
	}

	return 0;
}

int hook_import_byfunc(SceModule *pMod, char *library, void *orig_func, void *func) {
	PspModuleImport *pImp;
	void *stubTab;
	int stubLen;
	int i = 0;

	if(pMod == NULL)
		return -1;

	stubTab = pMod->stub_top;
	stubLen = pMod->stub_size;

	while(i<stubLen) {
		pImp = (PspModuleImport*)(stubTab+i);

		if((pImp->name) && (strcmp(pImp->name, library) == 0)) {
			int j;

			for(j=0; j<pImp->funcCount; j++) {
				if(*(u32*)((void*)(&pImp->funcs[j*2])) == *(u32*)(orig_func)) {
					void *addr = (void*)(&pImp->funcs[j*2]);

					_sw(MAKE_JUMP(func), (u32)addr);
					_sw(0, (u32)(addr + 4));

					sceKernelDcacheWritebackInvalidateRange(addr, 8);
					sceKernelIcacheInvalidateRange(addr, 8);
				}
			}

		}

		i += (pImp->entLen * 4);
	}

	return 0;
}

uidControlBlock *find_uid_control_block_by_name(const char *name, const char *parent) {
	uidControlBlock *entry;
	uidControlBlock *end;

	entry = SysMemForKernel_536AD5E1();
	
	entry = entry->parent;
	end = entry;
	entry = entry->nextEntry;

	do {
		if(entry->nextChild != entry) {
			do {
				uidControlBlock  *ret = NULL;
				entry = entry->nextChild;

				if(name) {
					if (strcmp(entry->name, name) == 0) {
						ret = entry;
					}
				}

				if(ret) {
					if(parent && ret->type) {
						if(strcmp(parent, ret->type->name) == 0) {
							return ret;
						}
					} else {
						return ret;
					}
				}
			} while (entry->nextChild != entry->type);
			entry = entry->nextChild;
		}

		entry = entry->nextEntry;
	} while (entry->nextEntry != end);

	return 0;
}

SceUID load_start_module(const char *file) {
	SceUID mod_id = sceKernelLoadModule(file, 0, NULL);
	log("load_start_module: module at %s loaded (module id: 0x%08X)\n", file, mod_id);

	if(mod_id >= 0) {
		int ret = sceKernelStartModule(mod_id, 0, NULL, NULL, NULL);
		log("load_start_module: module 0x%08X started (return: 0x%08X)\n", mod_id, ret);
	}

	return mod_id;
}

int _strnicmp(const char *pStr1, const char *pStr2, unsigned int count) {
    u8 c1, c2;
    int v;

    if(count == 0)
        return 0;

    do {
        c1 = *pStr1++;
        c2 = *pStr2++;
        if(c1 == '_') c1 = '-';
		if(c2 == '_') c2 = '-';
        v = tolower(c1) - tolower(c2);
    } while ((v == 0) && (c1 != '\0') && (--count > 0));

    return v;
}

int dump_memory(const char *file, void *addr, int len) {
	SceUID fd = sceIoOpen(file, PSP_O_CREAT | PSP_O_TRUNC | PSP_O_WRONLY, 0777);
	int write = 0;

	if(fd > -1) {
		write = sceIoWrite(fd, addr, len);
		sceIoClose(fd);
	}

	if(fd < 0 || write < len) {
		sceIoRemove(file);
	}

	return !(fd < 0 || write < len);
}

#ifdef _UMDDUMP_
int get_registry_value(const char *dir, const char *name, int *val) {
	int ret = 0;
	struct RegParam reg;
	REGHANDLE h;

	memset(&reg, 0, sizeof(reg));
	reg.regtype = 1;
	reg.namelen = strlen("/system");
	reg.unk2 = 1;
	reg.unk3 = 1;
	strcpy(reg.name, "/system");
	if(sceRegOpenRegistry(&reg, 2, &h) == 0) {
		REGHANDLE hd;
		if(sceRegOpenCategory(h, dir, 2, &hd) == 0) {
			REGHANDLE hk;
			u32 type, size;

			if(sceRegGetKeyInfo(hd, name, &hk, &type, &size) == 0) {
				if(sceRegGetKeyValue(hd, hk, val, 4) == 0) {
					ret = 1;
					sceRegFlushCategory(hd);
				}
			}

			sceRegCloseCategory(hd);
		}
		sceRegFlushRegistry(h);
		sceRegCloseRegistry(h);
	}

  return ret;
}

int set_registry_value(const char *dir, const char *name, int *val) {
	int ret = 0;
	struct RegParam reg;
	REGHANDLE h;

	memset(&reg, 0, sizeof(reg));
	reg.regtype = 1;
	reg.namelen = strlen("/system");
	reg.unk2 = 1;
	reg.unk3 = 1;
	strcpy(reg.name, "/system");
	if(sceRegOpenRegistry(&reg, 2, &h) == 0) {
		REGHANDLE hd;
		if(sceRegOpenCategory(h, dir, 2, &hd) == 0) {
			if(sceRegSetKeyValue(hd, name, val, 4) == 0) {
				ret = 1;
				sceRegFlushCategory(hd);
			}
			sceRegCloseCategory(hd);
		}
		sceRegFlushRegistry(h);
		sceRegCloseRegistry(h);
	}

	return ret;
}
#endif