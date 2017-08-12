#ifdef _SCREENSHOT_

#include <stdio.h>
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspdisplay_kernel.h>
#include "common.h"

char CAPTURE_DIR [] __attribute__((aligned(1), section(".data"))) = "ms0:/PICTURE";

static int linesize;
static void *vram;

void screenshot_init(const char *path) {
	sceIoMkdir(CAPTURE_DIR, 0777);
}

static void dot8888(void *vram, int x, int y, unsigned char *r, unsigned char *g, unsigned char *b) {
	u32 c32 = ((u32 *)vram)[x + y * linesize];
	*r = c32 & 0xFF;
	*g = (c32 >> 8) & 0xFF;
	*b = (c32 >> 16) & 0xFF;
}

static void dot565(void *vram, int x, int y, unsigned char *r, unsigned char *g, unsigned char *b) {
	u16 color = ((u16 *)vram)[x + y * linesize];
	*r = (color & 0x1f) << 3;
	*g = ((color >> 5) & 0x3f) << 2;
	*b = ((color >> 11) & 0x1f) << 3;
}

static void dot5551(void *vram, int x, int y, unsigned char *r, unsigned char *g, unsigned char *b) {
	u16 color = ((u16 *)vram)[x + y * linesize];
	*r = (color & 0x1f) << 3;
	*g = ((color >> 5) & 0x1f) << 3;
	*b = ((color >> 10) & 0x1f) << 3;
}

static void dot4444(void *vram, int x, int y, unsigned char *r, unsigned char *g, unsigned char *b) {
	u16 color = ((u16 *)vram)[x + y * linesize];
	*r = (color & 0x0f) << 4;
	*g = ((color >> 4) & 0x0f) << 4;
	*b = ((color >> 8) & 0x0f) << 4;
}

typedef struct tagBITMAPFILEHEADER {
	unsigned long bfSize;
	unsigned long bfReserved;
	unsigned long bfOffBits;
} BITMAPFILEHEADER;

typedef struct tagBITMAPINFOHEADER{
	unsigned long biSize;
	long biWidth;
	long biHeight;
	unsigned short biPlanes;
	unsigned short biBitCount;
	unsigned long biCompression;
	unsigned long biSizeImage;
	long biXPelsPerMeter;
	long biYPelsPerMeter;
	unsigned long biClrUsed;
	unsigned long biClrImportant;
} BITMAPINFOHEADER;

void screenshot(int priority) {
	BITMAPFILEHEADER h1;
	BITMAPINFOHEADER h2;
	const unsigned char bm[2] = {0x42, 0x4D};
	int x, y, unk, pixelformat, pw, ph, ss_fcount = 0;
	SceUID fd;
	char path[64];
	
	void (*dotcb)(void *vram, int x, int y, unsigned char *r, unsigned char *g, unsigned char *b);

	sceDisplayGetMode(&unk, &pw, &ph);

	if(pw > 480) {
		return;
	}

	if(sceDisplayGetFrameBufferInternal(priority, &vram, &linesize, &pixelformat, PSP_DISPLAY_SETBUF_IMMEDIATE) < 0 || vram == NULL) {
		return;
	}

	switch(pixelformat) {
		case PSP_DISPLAY_PIXEL_FORMAT_565:
			dotcb = dot565;
			break;
		case PSP_DISPLAY_PIXEL_FORMAT_5551:
			dotcb = dot5551;
			break;
		case PSP_DISPLAY_PIXEL_FORMAT_4444:
			dotcb = dot4444;
			break;
		case PSP_DISPLAY_PIXEL_FORMAT_8888:
			dotcb = dot8888;
			break;
		default:
			return;
	}

	do {
		ss_fcount++;
		sprintf(path, "%s/snap%03i.bmp", CAPTURE_DIR, ss_fcount);
		fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
		if(fd >= 0) {
			sceIoClose(fd);
		}
	} while(fd >= 0);

	fd = fileIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);

	if(fd < 0) {
		return;
	}

	fileIoWrite(fd, bm, sizeof(bm));

	h1.bfSize = 3 * pw * ph + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + 2;
	h1.bfReserved = 0;
	h1.bfOffBits = 2 + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

	h2.biSize = sizeof(BITMAPINFOHEADER);
	h2.biPlanes = 1;
	h2.biBitCount = 24;
	h2.biCompression = 0;
	h2.biWidth = pw;
	h2.biHeight = ph;
	h2.biSizeImage = 3 * pw * ph;
	h2.biXPelsPerMeter = 0xEC4;
	h2.biYPelsPerMeter = 0xEC4;
	h2.biClrUsed = 0;
	h2.biClrImportant = 0;

	fileIoWrite(fd, &h1, sizeof(BITMAPFILEHEADER));
	fileIoWrite(fd, &h2, sizeof(BITMAPINFOHEADER));

	for(y = (ph - 1); y >= 0; y--) {
		for(x = 0; x < pw; x++) {
			unsigned char buf[3];
			dotcb(vram, x, y, &buf[2], &buf[1], &buf[0]);
			fileIoWrite(fd, buf, 3);
		}
	}

	fileIoClose(fd);
}

#endif