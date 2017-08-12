#include <stdio.h>
#include <string.h>
#include <pspkernel.h>
#include <pspsysmem_kernel.h>
#include "common.h"

int choose_alloc(SceSize size) {
	//SceUID partitionids[] = {9, 1, 6};
	SceUID partitionids[] = {1, 6};

	int i;
	for(i = 0; i < 3; i++) {
		PspSysmemPartitionInfo info;
		memset(&info, 0, sizeof(info));
		info.size = sizeof(info);

		if(sceKernelQueryMemoryPartitionInfo(partitionids[i], &info) == 0) {
			if(sceKernelPartitionMaxFreeMemSize(partitionids[i]) > size + 512) {
				return partitionids[i];
			}
		}
	}

	return 0;
}

void *kmalloc_align(SceUID partitionid, int type, SceSize size, int align) {
	size = size + align + sizeof(SceUID);

    SceUID mem_id = sceKernelAllocPartitionMemory((partitionid == 0 ? choose_alloc(size) : partitionid), "", type, size, NULL);

	if(mem_id >= 0) {    
		void *mem_addr = sceKernelGetBlockHeadAddr(mem_id);

		if(mem_addr != NULL) {
			mem_addr += sizeof(SceUID);

			if(align > 0) {
				mem_addr = (void*)(((u32)mem_addr & (~(align - 1))) + align);
			}

			*(u32*)(mem_addr - sizeof(SceUID)) = mem_id;
    
			return mem_addr;
		}
	}

	return NULL;
}

void *kmalloc(SceUID partitionid, int type, SceSize size) {
	return kmalloc_align(partitionid, type, size, 64);
}

void kfree(void *mem_addr) {
	if(mem_addr != NULL) {
		sceKernelFreePartitionMemory(*(u32*)(mem_addr - 4));
	}
}