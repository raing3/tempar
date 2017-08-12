#include <stdio.h>
#include <string.h>
#include <pspinit.h>
#include <psputility_sysparam.h>
#include "common.h"
#include "font.h"
#include "../languages/english.h"

Language lang;
static void *lang_buffer = NULL;
extern Config cfg;

static void language_process_buffer(void *lang_string, int size) {
	int i = 0;
	int index = 0;

	// parse the language file
	for(i = 0; i < sizeof(lang); i += 4) {
		char *string = lang_string + index;
		*(u32*)((void*)&lang + i) = (u32)string;
		index += strlen(string) + 1;
		if(index > size) {
			break;
		}
	}

	if(i < sizeof(lang)) {
		language_init();
	}
}

void language_init() {
	// free language file buffer and clear language structure
	language_deinit();

	// load default font
	memcpy(msx, language_english, sizeof(msx));
	font_patch();

	// load default strings
	language_process_buffer((language_english + sizeof(msx)), sizeof(language_english) - sizeof(msx));
}

void language_deinit() {
#ifdef _MULTILANGUAGE_
	kfree(lang_buffer);
	memset(&lang, 0, sizeof(Language));
#endif
}

int language_load() {
	int ret = 0;
	int language_system = 1, languages_count = 12;
	char *languages[] = {"ja", "en", "fr", "es", "de", "it", "nl", "pt", "ru", "ko", "ch1", "ch2"};
	char file[64];

	sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &language_system);

	if(cfg.language_file == 0) {
		sprintf(file, "languages/%s.bin", languages[language_system]);
		ret = language_load_file(file);
	} else {
		while(cfg.language_file - 1 < languages_count) {
			if(cfg.language_file - 1 != language_system) {
				sprintf(file, "languages/%s.bin", languages[cfg.language_file - 1]);
				ret = language_load_file(file);
				if(ret) {
					break;
				}
			}
			cfg.language_file++;
		}

		if(!ret) {
			sprintf(file, "languages/language%i.bin", cfg.language_file - languages_count);
			ret = language_load_file(file);
		}
	}

	if(!ret) {
		cfg.language_file = 0;
		language_init();
	}

	return ret;
}

int language_load_file(const char *file) {
#ifdef _MULTILANGUAGE_
	SceUID fd = sceIoOpen(file, PSP_O_RDONLY, 0777);

	if(fd > -1) {
		// check if the language file has a font
		char font;
		sceIoRead(fd, &font, sizeof(char));
		font = !font;

		// get the language file size
		u32 size = sceIoLseek(fd, 0, SEEK_END) + 1 - (font ? sizeof(msx) : 0);
		sceIoLseek(fd, 0, SEEK_SET);

		if(font) {
			sceIoRead(fd, &msx, sizeof(msx));
		} else {
			memcpy(msx, language_english, sizeof(msx));
		}

		font_patch();

		// read the language file
		if((lang_buffer = kmalloc(1, PSP_SMEM_Low, size)) != NULL) {
			sceIoRead(fd, lang_buffer, size);
			language_process_buffer(lang_buffer, size);
		}

		// close the language file
		sceIoClose(fd);

		return 1;
	}

	return 0;
#endif
}

void font_patch() {
	// patch in the special characters
	u8 msx_special[] = {
		0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, // 0x10	Special Line
		0x10, 0x38, 0x54, 0x10, 0x10, 0x10, 0x10, 0x00, // 0x11 TEXT_CTRL_UP
		0x10, 0x10, 0x10, 0x10, 0x54, 0x38, 0x10, 0x00, // 0x12 TEXT_CTRL_DOWN
		0x00, 0x20, 0x40, 0xFE, 0x40, 0x20, 0x00, 0x00, // 0x13 TEXT_CTRL_LEFT
		0x00, 0x08, 0x04, 0xFE, 0x04, 0x08, 0x00, 0x00, // 0x14 TEXT_CTRL_RIGHT
		0x38, 0x44, 0x82, 0x82, 0x82, 0x44, 0x38, 0x00, // 0x15 TEXT_CTRL_CIRCLE
		0x00, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x00, // 0x16 TEXT_CTRL_CROSS
		0x10, 0x28, 0x28, 0x44, 0x44, 0x82, 0xFE, 0x00, // 0x17 TEXT_CTRL_TRIANGLE
		0xFE, 0x82, 0x82, 0x82, 0x82, 0x82, 0xFE, 0x00, // 0x18 TEXT_CTRL_SQUARE
	};

	memcpy((void*)(msx + (0x10 * 8)), msx_special, sizeof(msx_special));
}