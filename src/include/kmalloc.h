#ifndef KMALLOC_H
#define KMALLOC_H

#include <pspkerneltypes.h>

// TODO
int choose_alloc(SceSize size);

// TODO
void *kmalloc_align(SceUID partitionid, int type, SceSize size, int align);

/**
 * Same as malloc() but for kernel mode
 *
 * @param size = size of the memory to allocate
 */
void *kmalloc(SceUID partitionid, int type, SceSize size);

/**
 * Same as free() but for kernel mode
 *
 * @param mem_addr = Memory address to free
 */
void kfree(void *mem_addr);

#endif
