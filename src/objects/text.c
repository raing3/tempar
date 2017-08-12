/* 
 * Copyright (C) 2006 aeolusc <soarchin@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef _GUIDE_

#include <pspkernel.h>
#include <pspsysmem_kernel.h>
#include <pspsuspend.h>
#include "common.h"

typedef struct {
	int size;
	char *buf;
	int row_count;
	t_textrow *rows;
} t_text, *p_text;

static int maxrowcount;

static int text_id = -1;
static int rows_id = -1;
static int buf_id = -1;
static p_text txt;

extern int text_open(const char * filename, int rowbytes, int rowcount)
{
	maxrowcount = (rowcount == 0 ? 5000 : rowcount);

	int fd = sceIoOpen(filename, PSP_O_RDONLY, 0777);
	if(fd < 0)
		return -1;
	int l = sceIoLseek32(fd, 0, PSP_SEEK_END);
	if(l < rowbytes) l = rowbytes;

	text_close();

  	text_id = sceKernelAllocPartitionMemory(choose_alloc(sizeof(t_text)), "", 0, sizeof(t_text), NULL);
	if(text_id < 0) {
		text_close();
		sceIoClose(fd);
		return -2;
	}
	txt = (p_text)sceKernelGetBlockHeadAddr(text_id);

	rows_id = sceKernelAllocPartitionMemory(choose_alloc(sizeof(t_textrow) * maxrowcount), "", 0, sizeof(t_textrow) * maxrowcount, NULL);
	if(rows_id < 0) {
		text_close();
		sceIoClose(fd);
		return -2;
	}
	txt->rows = (t_textrow *)sceKernelGetBlockHeadAddr(rows_id);
	
	int temp = choose_alloc(l);
	if(temp == 0) {
		text_close();
		sceIoClose(fd);
		return -2;
	}
 	buf_id = sceKernelAllocPartitionMemory(temp, "", 0, l + 2, NULL);	
	txt->buf = (char *)sceKernelGetBlockHeadAddr(buf_id);

	memset(txt->buf, 0, l + 2);
 
	txt->buf[l] = 0;
	sceIoLseek32(fd, 0, PSP_SEEK_SET);
	sceIoRead(fd, txt->buf, l);
	sceIoClose(fd);
	txt->buf[l] = 0;
	txt->size = l;
	
 	char * pos = txt->buf, * posend = pos + txt->size;
	txt->row_count = 0;
	while(txt->row_count < maxrowcount && pos + 1 < posend)
	{
		txt->rows[txt->row_count].start = pos;
		char * startp = pos, * endp = pos + rowbytes;
		if(endp > posend)
			endp = posend;
		while(pos < endp && *pos != 0 && *pos != '\r' && *pos != '\n')
			if((*(unsigned char *)pos) >= 0x80)
				pos += 2;
			else
				++ pos;
		if(pos > endp)
			pos -= 2;
		if(pos + 1 < posend && ((*pos >= 'A' && *pos <= 'Z') || (*pos >= 'a' && *pos <= 'z')))
		{
			char * curp = pos - 1;
			while(curp > startp)
			{
				if(*(unsigned char *)(curp - 1) >= 0x80 || *curp == ' ' || * curp == '\t')
				{
					pos = curp + 1;
					break;
				}
				curp --;
			}
		}
		txt->rows[txt->row_count].count = pos - startp;
		if(*pos == 0) {
			txt->row_count ++;
			break;
		}
		if(*pos == 0 || *pos == '\r' || *pos =='\n')
		{
			if(*pos == '\r' && *(pos + 1) == '\n')
				pos += 2;
			else
				++ pos;
		}
		txt->row_count ++;
	}

	return 0;
}

extern int text_rows()
{
	if(txt == NULL)
		return 0;
	return txt->row_count;
}

extern p_textrow text_read(int row)
{
	if(txt == NULL || row >= txt->row_count)
		return NULL;
	return &txt->rows[row];
}

extern void text_close()
{
   	if(buf_id >= 0) {
		sceKernelFreePartitionMemory(buf_id);
		txt->buf = NULL;
		buf_id = -1;
	}
	if(rows_id >= 0) {
		sceKernelFreePartitionMemory(rows_id);
		txt->rows = NULL;
		rows_id = -1;
	}
 	if(text_id >= 0) {
		sceKernelFreePartitionMemory(text_id);
		txt = NULL;
		text_id = -1;
	}
}

#endif