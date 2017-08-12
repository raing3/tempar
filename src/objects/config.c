#include <stdio.h>
#include <stdlib.h>
#include <pspinit.h>
#include <pspctrl.h>
#include <pspctrl_kernel.h>
#include "common.h"

Config cfg;
Colors colors;

int config_load(const char *file) {
	config_reset();

	// open the config file
	SceUID fd = sceIoOpen(file, PSP_O_RDONLY, 0777);

	if(fd > -1) {
		// read the config file
		u8 config_ver = 0;
		sceIoRead(fd, &config_ver, sizeof(u8));
		if(config_ver == CONFIG_VER) {
			sceIoRead(fd, (void*)&cfg + 1, sizeof(Config) - 1);
		}
		sceIoClose(fd);

		// load the colorss
		char color_path[64];
		sprintf(color_path, "colors/color%i.txt", cfg.color_file);
		color_load(color_path);

		// update the button callback
		sceCtrlRegisterButtonCallback(3, cfg.menu_key | cfg.screen_key | cfg.trigger_key, button_callback, NULL);

		return 0;
	}

	return -1;
}

int config_save(const char *file) {
	SceUID fd = sceIoOpen(file, PSP_O_CREAT | PSP_O_WRONLY, 0777);

	if(fd > -1) {
		sceIoWrite(fd, &cfg, sizeof(cfg));
		sceIoClose(fd);

		return 0;
	}

	return -1;
}

void config_reset() {
	// set the default config
	cfg = (Config) {
		.mac_enable = 0,
		.ver = CONFIG_VER,
		.mac = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
		.menu_key = PSP_CTRL_HOME | PSP_CTRL_RTRIGGER,
		.trigger_key = PSP_CTRL_NOTE,
		.screen_key = PSP_CTRL_SELECT | PSP_CTRL_VOLDOWN,
		.cheat_hz = 15000,
		.max_text_rows = 5000,
		.max_cheats = 1024,
		.max_blocks = 2048,
		.auto_off = 0,
		.address_start = 0x08800000,
		.address_end = 0x09FFFFFF
	};

	// set the default colors
	colors = (Colors) {
		.bgcolor = 0xFF442F18,			// background color
		.linecolor = 0xFFFFFFFF,		// line color
		.color01 = 0xFF0072FF,			// selected color
		.color01_to = 0x00000202,		// selected color fade amount
		.color02 = 0xFFFFFFFF,			// non-selected color
		.color02_to = 0x00020202,		// non-selected color fade amount
		.color03 = 0xFF0072FF,			// selected menu color
		.color04 = 0xFFFFFFFF,			// non-seleted menu color
		.color05 = 0xFF0072FF,			// off cheat color
		.color06 = 0xFFFF0000,			// always on cheat color
		.color07 = 0xFF00FF00,			// normal on cheat color
		.color08 = 0xFF21FF00,			// multi-select folder color
		.color09 = 0xFF21FF00,			// single-select folder color
		.color10 = 0xFFFFFF00			// comments/empty codes
	};

	// set the pops memory start/end and colors
	if(sceKernelInitKeyConfig() == PSP_INIT_KEYCONFIG_POPS) {
		cfg.address_start = 0x09800000;
		cfg.address_end = 0x09FFFFFF;
	}

	pspDebugScreenSetBackColor(colors.bgcolor); // set the background
}

int color_load(const char *file) {
	SceUID fd = fileIoOpen(file, PSP_O_RDONLY, 0777);

	if(fd > -1) {
		u32 i;

		for(i = 0; i < sizeof(Colors); i += 4) {
			while(_strnicmp(fileIoGet(), "0x", 2) != 0) {
				fileIoLseek(fd, 1, SEEK_CUR);
			}

			*(u32*)((void*)&colors + i) = strtoul(fileIoGet(), NULL, 16);

			fileIoSkipLine(fd);
		}

		fileIoClose(fd);

		pspDebugScreenSetBackColor(colors.bgcolor); // set the background

		return 0;
	}

	return -1;
}