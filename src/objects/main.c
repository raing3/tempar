/*
 * NitePR Original Source by: SANiK + imk + telazorn , 
 * MKULTRA By:
 * 		RedHate dmonchild@hotmail.com
* 		NoEffex unigaming.net
 *     (we're still holding down pizzle nizzle.)
 * TempAR By:
* 		raing3
*/

/*
 * Some code borrowed from these mighty fine people also.
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 * main.c - PSPLINK Debug/Impose menu
 * Copyright (c) 2006 James F <tyranid@gmail.com>
 * $HeadURL: file:///home/svn/psp/trunk/psplink/tools/debugmenu/main.c $
 * $Id: main.c 2018 2006-10-07 16:54:19Z tyranid $
*/

#include <pspmodulemgr.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <psppower.h>
#include <pspumd.h>
#include <pspreg.h>
#include <pspinit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pspsysmem_kernel.h>
#include <psputility.h>
#include <pspctrl_kernel.h>
#include <pspthreadman.h>
#include "common.h"
#include "menu.h"

PSP_MODULE_INFO(MOD_NAME, 0x3007, VER_MAIN, VER_SUB);
PSP_MAIN_THREAD_ATTR(0);

//Globals
#define MAX_THREAD 64
SceUID thid;
static int thread_count_start, thread_count_now, pause_thid = -1;
static SceUID thread_buf_start[MAX_THREAD], thread_buf_now[MAX_THREAD];

extern int cheat_total;
extern Config cfg;
extern MenuState menu;

char running = 1;
char buffer[128];
char screenTime = 0;
char boot_path[255];
char plug_path[255];
char plug_drive[10];

void button_callback(int curr_but, int last_but, void *arg) {
	if(!menu.visible) {
		if((curr_but & cfg.menu_key) == cfg.menu_key && cfg.menu_key) {
			menu.visible = 1;
		#ifdef _SCREENSHOT_
		} else if((curr_but & cfg.screen_key) == cfg.screen_key && cfg.screen_key) {
			screenTime = 1;
		#endif
		} else if((curr_but &cfg.trigger_key) == cfg.trigger_key && cfg.trigger_key) {
			cfg.cheat_status = !cfg.cheat_status;
				cheat_apply(CHEAT_TOGGLE_ALL);
		}
	}
}

void gamePause(SceUID thread_id) {
	if(pause_thid <= 0) {
		pause_thid = thread_id;
		sceKernelGetThreadmanIdList(SCE_KERNEL_TMID_Thread, thread_buf_now, MAX_THREAD, &thread_count_now);
		int x, y, match;
		for(x = 0; x < thread_count_now; x++) {
			match = 0;
			SceUID tmp_thid = thread_buf_now[x];
			for(y = 0; y < thread_count_start; y++) {
				if((tmp_thid == thread_buf_start[y]) || (tmp_thid == thread_id)) {
					match = 1;
					break;
				}
			}
			if(match == 0) {
				sceKernelSuspendThread(tmp_thid);
			}
		}
	}
}

void gameResume(SceUID thread_id) {
	if(pause_thid == thread_id) {
		pause_thid = -1;
		int x, y, match;
		for(x = 0; x < thread_count_now; x++) {
			match = 0;
			SceUID tmp_thid = thread_buf_now[x];
			for(y = 0; y < thread_count_start; y++) {
				if((tmp_thid == thread_buf_start[y]) || (tmp_thid == thread_id)) {
					match = 1;
					break;
				}
			}
			if(match == 0) {
				sceKernelResumeThread(tmp_thid);
			}
		}
	}
} 

int main_thread(SceSize args, void *argp) {
	// wait for sceKernelLibrary to load
	while(!sceKernelFindModuleByName("sceKernelLibrary")) {
		sceKernelDelayThread(1000000);
	}
	sceKernelDelayThread(1000000);
	log("main_thread: sceKernelLibrary loaded\n");

	// set the tempar directory
	sceIoChdir(plug_path);

	// load the config
	config_load(sceKernelInitKeyConfig() == PSP_INIT_KEYCONFIG_POPS ? "config_pops.bin" : "config.bin");
	log("main_thread: config loaded\n");

	// get the game id
	gameid_get(1);
	log("main_thread: gameid found\n");

	// initialize the cheats and blocks
	cheat_init(cfg.max_cheats, cfg.max_blocks, cfg.auto_off);
	log("main_thread: cheats initialized\n");

	// load the cheats
	cheat_load(NULL, -1, 0);
	if(cheat_total == 0) {
		cheat_load(NULL, cfg.cheat_file, 0);
	}
	log("main_thread: cheats initialized\n");

	// load the language file
	language_init();
	language_load();
	log("main_thread: language loaded\n");

	// corrupt the psid
  	#ifdef _PSID_
	if(sceKernelInitKeyConfig() == PSP_INIT_KEYCONFIG_GAME) {
		psid_init();
		log("main_thread: psid corrupt initialized\n");
	}
	#endif

	#ifdef _SCREENSHOT_
	screenshot_init(plug_path);
	log("main_thread: screenshot initialized\n");
  	#endif

	// load the search results
	search_init();
	log("main_thread: search initialized\n");

	// initialize the menu setings
	menu_init();

	// register the button callback
	sceCtrlRegisterButtonCallback(3, cfg.menu_key | cfg.screen_key | cfg.trigger_key, button_callback, NULL);
	log("main_thread: button callback registered\n");

	while(running) {
		// handle menu
		if(menu.visible) {
			menu_show();
		} else if(cfg.cheat_hz != 0) {
			cheat_apply(0);
		}
		
		// handle screenshot
		#ifdef _SCREENSHOT_
		if(screenTime) {
			gamePause(thid);
			screenTime = 0;
			screenshot(2);
			gameResume(thid);
		}
		#endif
		
		// wait a certain amount of seconds before reapplying cheats again
		sceKernelDelayThread(cfg.cheat_hz ? cfg.cheat_hz : 15000);
	}

	return sceKernelExitDeleteThread(0);
}

int module_start(int argc, char *argv[]) {
	log("module_start: module started\n");

	char *path = (char*)argv;
	int last_trail = -1, first_trail = -1;
	int i;

	if(path) {
		for(i = 0; path[i]; i++){
			if(path[i] == '/') {
				if(first_trail < 0) {
					first_trail = i;
				}
				last_trail = i;
			}
		}

		// get the plugin path
		if(last_trail >= 0 && last_trail < sizeof(plug_path)) {
			memcpy(plug_path, path, last_trail);
			plug_path[last_trail] = 0;
		}

		// get the drive
		if(first_trail >= 0 && first_trail < sizeof(plug_drive)) {
			memcpy(plug_drive, path, first_trail);
			plug_drive[first_trail] = 0;
		}
	}

	strcpy(boot_path, sceKernelInitFileName());

	log("module_start: sceKernelInitFileName (%s)\n", boot_path);

	// get thread list
	sceKernelGetThreadmanIdList(SCE_KERNEL_TMID_Thread, thread_buf_start, MAX_THREAD, &thread_count_start);
	// create and start main thread
	thid = sceKernelCreateThread("TempAR_thread", &main_thread, 0x18, 0x1000, 0, NULL);
	log("module_start: sceKernelCreateThread (0x%08X)\n", thid);
	if(thid >= 0) {
		sceKernelStartThread(thid, 0, NULL);
		log("module_start: sceKernelStartThread\n");
	}

	return 0;
}

int module_stop(int argc, char *argv[]) {
	menu.visible = 0;
	running = 0;

	// wait for the thread to finish
	SceUInt time = 100 * 1000;
	int ret = sceKernelWaitThreadEnd(thid, &time);

	// force terminate the thread
	if(ret < 0) {
		sceKernelTerminateDeleteThread(thid);
	}

	// free cheats memory
	cheat_deinit();

	// free language memory
	language_deinit();

	// free text viewer memory
	#ifdef _GUIDE_
	text_close();
	#endif

	// disconnect usb
	#ifdef _USB_
	usb_connect(0);
	#endif

	return 0;
}