#include <pspmodulemgr.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <psppower.h>
#include <pspusb.h>
#include <pspusbstor.h>
#include <pspumd.h>
#include <pspreg.h>
#include <pspinit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pspopenpsid.h>
#include <pspctrl_kernel.h>
#include <pspdisplay_kernel.h>
#include <pspimpose_driver.h>
#include "common.h"

extern SceUID thid;
extern Cheat *cheats;
extern Block *blocks;
extern int cheat_total;

extern char game_name[];

extern Config cfg;
extern Colors colors;
extern Language lang;
extern Search search;

MenuState menu;

char tab_selected = 0;
extern char buffer[128];
extern char bootPath[];
extern char plug_path[];
extern char plug_drive[];
int resume_count = 0;

extern char usb_started;

int decDelta[] = {1000000000, 100000000, 10000000, 1000000, 100000, 10000, 1000, 100, 10, 1};

int guiderow = 0;

/* helper functions */

void *get_vram() {
	uidControlBlock *SceImpose_FrameBuffer = find_uid_control_block_by_name("SceImpose_FrameBuffer", NULL);

	log("get_vram: SceImpose_FrameBuffer (0x%08X)\n", SceImpose_FrameBuffer);

	if(SceImpose_FrameBuffer) {
		menu.options.force_pause = 0;
		return (void *)(*(u32*)((u32)SceImpose_FrameBuffer + 0x34) | 0xA0000000);
	} else {
		menu.options.force_pause = 1;
		return (void*)0x44000000;
	}
}

void menu_show() {
	// hide and disable the home screen
	log("show_menu: hiding home screen\n");
	sceImposeHomeButton(0);
	int home_enabled = sceImposeGetHomePopup();
	sceImposeSetHomePopup(0);
	
	// delay
	log("show_menu: delaying...\n");
	sceKernelDelayThread(150000);

	// stop the game from receiving input (user mode input block)
	log("show_menu: setting button masks\n");
	sceCtrlSetButtonMasks(0xFFFF, 1);  // mask lower 16 bits
	sceCtrlSetButtonMasks(0x10000, 2); // always return home key

	// update the resume count
	log("show_menu: updating resume count\n");
	resume_count = scePowerGetResumeCount();

	// setup controller
	log("show_menu: setting up controller\n");
	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

	// setup vram
	log("show_menu: setting up vram\n");
	void *vram = get_vram();
	log("show_menu: vram (0x%08X)\n", vram);
	pspDebugScreenClearLineDisable();
	pspDebugScreenSetBackColor(colors.bgcolor);
	pspDebugScreenInitEx(vram, PSP_DISPLAY_PIXEL_FORMAT_565, 0);
	sceDisplaySetFrameBufferInternal(0, vram, 512, PSP_DISPLAY_PIXEL_FORMAT_565, PSP_DISPLAY_SETBUF_NEXTFRAME);

	// pause game
	if(cfg.cheat_pause || menu.options.force_pause) {
		log("show_menu: pausing game\n");
		gamePause(thid);
	}

	// draw menu
	log("show_menu: showing menu\n");
	layout_tab();
	menu.visible = 0;
	log("show_menu: menu done\n");

	// resume game
	log("show_menu: resuming game\n");
	gameResume(thid);

	// allow the game to receive input
	log("show_menu: un-setting button masks\n");
	sceCtrlSetButtonMasks(0x10000, 0); // unset home key
	sceCtrlSetButtonMasks(0xFFFF, 0);  // unset mask

	// hide and enable the home screen
	log("show_menu: hiding home screen\n");
	sceImposeSetHomePopup(1);
	sceImposeHomeButton(0);

	// return display to the main screen
	log("show_menu: returning screen to normal frame buffer\n");
	sceDisplaySetFrameBufferInternal(0, 0, 512, PSP_DISPLAY_PIXEL_FORMAT_565, PSP_DISPLAY_SETBUF_NEXTFRAME);

	if(vram != 0x44000000) {
		// delay
		log("show_menu: delaying...\n");
		sceKernelDelayThread(150000);

		// clear home screen
		log("show_menu: clearing screen\n");
		pspDebugScreenSetBackColor(0xFF9C9E39);
		pspDebugScreenClear();
	}
}

void menu_init() {
	memset(&menu, 0, sizeof(menu));
	menu.cheater.show_game_name = 0;
}

int get_menu_ctrl(int delay, int allow_empty) {
	#define MENU_KEY (PSP_CTRL_SELECT | PSP_CTRL_START | PSP_CTRL_UP | PSP_CTRL_RIGHT | PSP_CTRL_DOWN | PSP_CTRL_LEFT | PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_TRIANGLE | PSP_CTRL_CIRCLE | PSP_CTRL_CROSS | PSP_CTRL_SQUARE | PSP_CTRL_HOME | PSP_CTRL_NOTE | PSP_CTRL_SCREEN | PSP_CTRL_VOLUP | PSP_CTRL_VOLDOWN)

	SceCtrlData ctrl1;
	SceCtrlData ctrl2;

	do {
		sceCtrlPeekBufferPositive(&ctrl1, 1);
		sceKernelDelayThread(delay);
		sceCtrlPeekBufferPositive(&ctrl2, 1);
	} while(ctrl1.Buttons != ctrl2.Buttons || (ctrl1.Buttons & MENU_KEY) == 0);

	if((ctrl1.Buttons & PSP_CTRL_CIRCLE) && allow_empty) {
		return 0;
	}

	return (ctrl1.Buttons & MENU_KEY);
}

void get_print_start_end(int *start, int *end, int num_items, int display_items, int sel) {
	*start = sel - (display_items / 2);
	*end = *start + display_items;

	if(*end > num_items) {
		*end = num_items;
		*start = *end - display_items;
	}

	if(*start < 0) {
		*start = 0;
		*end = (display_items > num_items ? num_items : display_items);
	}

	*end--;
}

u32 percentage_to_color(int percent) {
	char red = (percent <= 50 ? 0xFF : (0xFF - (percent - 50) * 5) + 1);
	char green = (percent >= 50 ? 0xFF : ((percent * 5) + 1));
	char blue = 0;

	return (((red << 0) + (green << 8) + (blue << 16) + (0xFF << 24)) & 0xFF00FFFF);
}

void decode_str(char *str, char key) {
	do {
		*str ^= key;
	} while(*str++ != 0);
}

void line_clear(int line) {
	if(line < 0) {
		int i = 68 - pspDebugScreenGetX();
		while(i-- > 0) {
			puts(" ");
		}
	} else {
		pspDebugScreenSetXY(0, line);
		printf("%-68s", "");
		pspDebugScreenSetXY(0, line);
	}
}

void line_print(int  line) {
	const char l[69] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
		0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
		0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
		0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
		0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0};

	if(pspDebugScreenGetX() > 0) {
		line_clear(-1);
	}

	if(line >= 0) {
		pspDebugScreenSetXY(0, line);
	}

	pspDebugScreenSetTextColor(colors.linecolor);
	puts(l);
}

void line_cursor(int index, u32 color) {
	pspDebugScreenSetTextColor(color);

	int i;
	for(i = 0; i < 68; i++) {
		puts(i == index ? "^" : " ");
	}
}

void show_error(char *error) {
	line_clear(33);
	pspDebugScreenSetTextColor(colors.color01);
	printf("%s: %s", lang.errors.error, error);
	sceKernelDelayThread(1 * 1000 * 1000);
}

/* dialogs */

void layout_cheatmenu(Cheat *cheat) {
	u32 ctrl, ret = 0;
	int i, sel = 0;

	while(ret == 0) {
		pspDebugScreenSetXY(0, 3);

		for(i = 0; i < 5; i++) {
			pspDebugScreenSetBackColor(colors.bgcolor + (i * 0x06)); // fade the background
			pspDebugScreenSetTextColor(i == sel ? colors.color01 : colors.color02); // highlight selected line

			puts("  ");

			switch(i) {
				case 0:
					puts(lang.cheat_menu.edit_cheat);
					break;
				case 1:
					puts(lang.cheat_menu.rename_cheat);
					break;
				case 2:
					puts(lang.cheat_menu.copy_cheat);
					break;
				case 3:
					puts(lang.cheat_menu.insert_cheat);
					break;
				case 4:
					puts(lang.cheat_menu.delete_cheat);
					break;
			}

			line_clear(-1);
		}

		pspDebugScreenSetBackColor(colors.bgcolor);
		line_print(-1);

		switch(ctrl = ctrl_waitmask(0, 0, PSP_CTRL_UP | PSP_CTRL_DOWN | PSP_CTRL_CROSS | PSP_CTRL_CIRCLE)) {
			case PSP_CTRL_UP:
				sel = (sel > 0 ? sel - 1 : 4);
				break;
			case PSP_CTRL_DOWN:
				sel = (sel < 4 ? sel + 1 : 0);
				break;
			case PSP_CTRL_CROSS:
				switch(sel) {
					case 0:
						if(cheat != NULL) {
							layout_cheatedit(cheat);
						}
						break;
					case 1:
						if(cheat != NULL) {
							ctrl_waitrelease();
							pspDebugKbInit(cheat->name, sizeof(cheat->name));
						}
						break;
					case 2:
						if(cheat != NULL) {
							if((cheat = cheat_copy(cheat)) != NULL) {
								menu.cheater.selected = cheat;
							}
						}
						break;
					case 3:
						if((cheat = cheat_new(cheat_get_index(cheat) + 1, 0, 0, 1, cheat_get_engine(cheat), 0)) != NULL) {
							if(cheat->parent != NULL) {
								cheat->parent->flags &= ~CHEAT_HIDDEN;
							}
							menu.cheater.selected = cheat;
						}
						break;
					case 4:
						if(cheat != NULL) {
							if(cheat_get_index(cheat) != 0) {
								menu.cheater.selected = cheat_previous_visible(cheat);
							}
							cheat_delete(cheat);
						}
						break;
				}

				ret = ctrl;
				break;
			case PSP_CTRL_CIRCLE:
				ret = ctrl;
				break;
		}
	}

	ctrl_waitrelease();
}

void layout_copymenu(u32 *address, u32 *value, int flags) {
	u32 ctrl, ret = 0;
	int i, sel = 0;

	Copier *copier = &menu.copier;

	while(ret == 0) {
		for(i = 0; i < 5; i++) {
			line_clear(i + 3);
			pspDebugScreenSetBackColor(colors.bgcolor + (i * 0x06)); // fade the background
			pspDebugScreenSetTextColor(i == sel ? colors.color01 : colors.color02); // highlight selected line

			puts("  ");

			switch(i) {
				case 0:
					printf("%-33s", lang.copy_menu.copy_address);
					pspDebugScreenSetTextColor(colors.color02);
					printf("%19s = 0x%08X", lang.misc.address, copier->copier[copier->no].address);
					break;
				case 1:
					printf("%-33s", lang.copy_menu.paste_address);
					pspDebugScreenSetTextColor(colors.color02);
					printf("%19s = 0x%08X", lang.misc.value, copier->copier[copier->no].value);
					break;
				case 2:
					puts(lang.copy_menu.copy_value);
					break;
				case 3:
					puts(lang.copy_menu.paste_value);
					break;
				case 4:
					printf("%-33s", lang.copy_menu.copy_to_new_cheat);
					pspDebugScreenSetTextColor(colors.color02);
					sprintf(buffer, lang.copy_menu.copier_num, copier->no + 1);
					printf("%32s", buffer);
					break;
			}

			puts("\n");
		}

		pspDebugScreenSetBackColor(colors.bgcolor);
		line_print(-1);

		switch(ctrl = ctrl_waitmask(0, 0, PSP_CTRL_UP | PSP_CTRL_DOWN | PSP_CTRL_LEFT | PSP_CTRL_RIGHT |
			PSP_CTRL_CROSS | PSP_CTRL_CIRCLE)) {
			case PSP_CTRL_UP:
				sel = (sel > 0 ? sel - 1 : 4);
				break;
			case PSP_CTRL_DOWN:
				sel = (sel < 4 ? sel + 1 : 0);
				break;
			case PSP_CTRL_LEFT:
				copier->no = (copier->no > 0 ? copier->no - 1 : 9);
				break;
			case PSP_CTRL_RIGHT:
				copier->no = (copier->no < 9 ? copier->no + 1 : 0);
				break;
			case PSP_CTRL_CROSS:
				switch(sel) {
					case 0:
						if(flags & COPY_ADDRESS) {
							if(address != NULL) {
								copier->copier[copier->no].address  = *address;
							}
						}
						break;
					case 1:
						if(flags & PASTE_ADDRESS) {
							if(address != NULL) {
								*address = copier->copier[copier->no].address;
							}
						}
						break;
					case 2:
						if(flags & COPY_VALUE) {
							if(value != NULL) {
								switch((u32)value % 4) {
									case 3:
										copier->copier[copier->no].value = *(u8*)value;
										break;
									case 2:
										copier->copier[copier->no].value = *(u16*)value;
										break;
									case 1:
										copier->copier[copier->no].value = *(u8*)value;
										break;
									case 0:
										copier->copier[copier->no].value = *(u32*)value;
										break;
								}
							}
						}
						break;
					case 3:
						if(flags & PASTE_VALUE) {
							if(value != NULL) {
								switch((u32)value % 4) {
									case 3:
										*(u8*)value = copier->copier[copier->no].value;
										break;
									case 2:
										*(u16*)value = copier->copier[copier->no].value;
										break;
									case 1:
										*(u8*)value = copier->copier[copier->no].value;
										break;
									case 0:
										*(u32*)value = copier->copier[copier->no].value;
										break;
								}
							}
						}
						break;
					case 4: {
						Cheat *new_cheat = cheat_new_from_memory(copier->address_start, copier->address_end);
						if(new_cheat != NULL) {
							menu.cheater.selected = new_cheat;
						}
						break; }
				}
				ret = ctrl;
				break;
			case PSP_CTRL_CIRCLE:
				ret = ctrl;
				break;
		}
	}

	ctrl_waitrelease();

	// clear the screen
	for(i = 0; i < 6; i++) {
		line_clear(i + 3);
	}
}

void layout_cheatedit(Cheat *cheat) {
	u32 ctrl = 0, ret = 0;
	int i, j, sel = 0;
	int x = 0, x2 = 0, x3 = 0; // selected block, cursor position, edit mode
	Block *block;

	pspDebugScreenClear();
	
	while(ret == 0) {
		// heading
		line_print(0);
		pspDebugScreenSetTextColor(colors.color01);
		puts("  ");
		printf(lang.cheat_edit.title, cheat->name, cheat_get_engine_string(cheat));
		line_print(-1);

		pspDebugScreenSetTextColor(colors.color02);
		
		// draw the headings
		if(menu.cheater.edit_format) {
			printf("  %-11s %-11s %-9s %s", lang.misc.address, lang.misc.hex, lang.misc.opcode, lang.misc.args);
		} else {
			printf("  %-11s %-11s %-11s %-6s %s", lang.misc.address, lang.misc.hex, lang.misc.dec, lang.misc.ascii, lang.misc._float);
		}

		line_clear(-1);

		int start, end;
		get_print_start_end(&start, &end, cheat->length, 27, sel);
		for(i = start; i < end; i++) {
			block = block_get(cheat->block + i);

			// apply the row color
			pspDebugScreenSetTextColor(i == sel ? colors.color01 : colors.color02);

			// print out the address and the hex
			printf("  0x%08X  0x%08X  ", block->address, block->value);

			// print opcode or ascii and decimal
			if(menu.cheater.edit_format) {
				// print out the opcode
				mipsDecode(block->value, 0);
			} else {
				// print out the decimal
				printf("%010u  ", block->value);

				// print out the ascii
				buffer[0] = MEM_ASCII((block->value & 0x000000FF) >> 0);
				buffer[1] = MEM_ASCII((block->value & 0x0000FF00) >> 8);
				buffer[2] = MEM_ASCII((block->value & 0x00FF0000) >> 16);
				buffer[3] = MEM_ASCII((block->value & 0xFF000000) >> 24);
				buffer[4] = 0;
				puts(buffer);
				puts("   ");

				// print out the float
				f_cvt(&block->value, buffer, sizeof(buffer), 6, MODE_GENERIC);
				puts(buffer);
			}

			// go to the next line
			line_clear(-1);

			// draw the cursor
			if(i == sel) {
				j = 4 + x2;
				switch(x) {
					case 3:
						j += 12;
					case 2:
						j += 10;
					case 1:
						j += 12;
						break;
				}

				line_cursor(j, x3 ? colors.color01 : colors.color06);
			}
		}

		// helper
		line_clear(-1);
		line_print(32);
		pspDebugScreenSetTextColor(colors.color01);
		puts(lang.cheat_edit.help);

		block = block_get(cheat->block + sel);

		switch(ctrl = ctrl_waitmask(0, 0, PSP_CTRL_UP | PSP_CTRL_DOWN | PSP_CTRL_LEFT | PSP_CTRL_RIGHT |
			PSP_CTRL_TRIANGLE | PSP_CTRL_CROSS | PSP_CTRL_CIRCLE | PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_SELECT | PSP_CTRL_START)) {
			case PSP_CTRL_LTRIGGER:
				block_delete(cheat, sel + 1);
				if(sel >= cheat->length) {
					sel--;
				}
				break;
			case PSP_CTRL_RTRIGGER:
				if(block_insert(cheat, NULL, sel + 1)) {
					if(cheat->length == 1) {
						sel = 0;
					} else {
						sel++;
					}
				}
				break;
			case PSP_CTRL_SELECT:
				switch(cheat_get_engine(cheat)) {
					case CHEAT_PSPAR_EXT:
						cheat_set_engine(cheat, CHEAT_CWCHEAT);
						break;
					case CHEAT_CWCHEAT:
						cheat_set_engine(cheat, CHEAT_PSPAR);
						break;
					default:
						cheat_set_engine(cheat, CHEAT_PSPAR_EXT);
						break;
				}
				break;
			case PSP_CTRL_START:
				menu.cheater.edit_format = !menu.cheater.edit_format;
				if(menu.cheater.edit_format) { // opcode mode
					if(x > 1) {
						x = 1;
						x2 = 7;
					}
				}
				break;
			case PSP_CTRL_TRIANGLE:
				layout_copymenu(&block->address, &block->value, COPY_PASTE_ALL);
				break;
			case PSP_CTRL_CROSS:
				x3 = !x3;
				break;
			case PSP_CTRL_UP:
				if(x3) { // edit mode
					switch(x) {
						case 0:
							block->address += (1 << (4 * (7 - x2)));
							break;
						case 1:
							block->value += (1 << (4 * (7 - x2)));
							break;
						case 2:
							block->value += decDelta[x2];
							break;
						case 3:
							block->value += (1 << (8 * x2));
							break;
					}
				} else { // view mode
					sel = (sel > 0 ? sel - 1 : cheat->length - 1);
				}
				break;
			case PSP_CTRL_DOWN:
				if(x3) { // edit mode
					switch(x) {
						case 0:
							block->address -= (1 << (4 * (7 - x2)));
							break;
						case 1:
							block->value -= (1 << (4 * (7 - x2)));
							break;
						case 2:
							block->value -= decDelta[x2];
							break;
						case 3:
							block->value -= (1 << (8 * x2));
							break;
					}
				} else { // view mode
					sel = (sel < cheat->length - 1 ? sel + 1 : 0);
				}
				break;
			case PSP_CTRL_LEFT:
				x2--;

				switch(x) {
					case 0:
						if(x2 < 0) {
							x2 = 0;
						}
						break;
					case 1:
						if(x2 < 0) {
							x--;
							x2 = 7;
						}
						break;
					case 2:
						if(x2 < 0) {
							x--;
							x2 = 7;
						}
						break;
					case 3:
						if(x2 < 0) {
							x--;
							x2 = 9;
						}
						break;
				}
				break;
			case PSP_CTRL_RIGHT:
				x2++;

				switch(x) {
					case 0:
						if(x2 > 7) {
							x++;
							x2 = 0;
						}
						break;
					case 1:
						if(x2 > 7) {
							x++;
							x2 = 0;
						}
						break;
					case 2:
						if(x2 > 9) {
							x++;
							x2 = 0;
						}
						break;
					case 3:
						if(x2 > 3) {
							x2 = 3;
						}
						break;
				}

				if(menu.cheater.edit_format && x > 1) {
					x = 1;
					x2 = 7;
				}
				break;
			case PSP_CTRL_CIRCLE:
				ret = ctrl;
				break;
		}
	}

	cheat_reparent(0, 0);

	ctrl_waitrelease();
	pspDebugScreenClear();
}

void layout_searchsettings() {
	u32 ctrl = 0, ret = 0;
	int i;

	int x = 0, x2 = 0, x3 = 0; // selected block, cursor position, edit mode

	pspDebugScreenClear();

	// heading
	line_print(0);
	pspDebugScreenSetTextColor(colors.color01);
	printf("  %s", lang.search_settings.title);
	line_print(-1);
	
	while(ret == 0) {
		// make sure addresses aren't out of boundaries
		search.address_start &= 0x0FFFFFFF;
		if((int)search.address_start < cfg.address_start) {
			search.address_start = cfg.address_start;
		}
		if((int)search.address_start > cfg.address_end) {
			search.address_start = cfg.address_end;
		}

		search.address_end &= 0x0FFFFFFF;
		if((int)search.address_end < cfg.address_start) {
			search.address_end = cfg.address_start;
		}
		if((int)search.address_end > cfg.address_end) {
			search.address_end = cfg.address_end;
		}

		pspDebugScreenSetXY(0, 3);

		pspDebugScreenSetTextColor(colors.color01);
		puts(lang.search_settings.message);

		pspDebugScreenSetTextColor(colors.color02);
		printf("  %-11s %-11s %s\n", lang.misc.start, lang.misc.stop, lang.search_settings.max_results);

		pspDebugScreenSetTextColor(colors.color01);
		printf("  0x%08X  0x%08X  %010u\n", address(search.address_start), address(search.address_end), search.max_results);

		// draw the cursor
		i = 4 + x2;
		switch(x) {
			case 2:
				i += 10;
			case 1:
				i += 12;
				break;
		}

		line_cursor(i, x3 ? colors.color01 : colors.color06);

		// helper
		line_print(32);
		pspDebugScreenSetTextColor(colors.color01);
		puts(lang.search_settings.help);

		switch(ctrl = ctrl_waitmask(0, 0, PSP_CTRL_UP | PSP_CTRL_DOWN | PSP_CTRL_LEFT | PSP_CTRL_RIGHT |
			PSP_CTRL_TRIANGLE | PSP_CTRL_CROSS | PSP_CTRL_CIRCLE)) {
			case PSP_CTRL_UP:
				if(x3) {
					switch(x) {
						case 0:
							search.address_start += (1 << (4 * (7 - x2)));
							break;
						case 1:
							search.address_end += (1 << (4 * (7 - x2)));
							break;
						case 2:
							search.max_results += decDelta[x2];
							break;
					}
				}
				break;
			case PSP_CTRL_DOWN:
				if(x3) {
					switch(x) {
						case 0:
							search.address_start -= (1 << (4 * (7 - x2)));
							break;
						case 1:
							search.address_end -= (1 << (4 * (7 - x2)));
							break;
						case 2:
							search.max_results -= decDelta[x2];
							break;
					}
				}
				break;
			case PSP_CTRL_LEFT:
				x2--;
				switch(x) {
					case 0:
						if(x2 < 0) {
							x2 = 0;
						}
						break;
					case 1:
						if(x2 < 0) {
							x--;
							x2 = 7;
						}
						break;
					case 2:
						if(x2 < 0) {
							x--;
							x2 = 7;
						}
						break;
				}
				break;
			case PSP_CTRL_RIGHT:
				x2++;
				switch(x) {
					case 0:
						if(x2 > 7) {
							x++;
							x2 = 0;
						}
						break;
					case 1:
						if(x2 > 7) {
							x++;
							x2 = 0;
						}
						break;
					case 2:
						if(x2 > 9) {
							x2 = 9;
						}
						break;
				}
				break;
			case PSP_CTRL_TRIANGLE:
				switch(x) {
					case 0:
						layout_copymenu(&search.address_start, NULL, COPY_PASTE_ADDRESS);
						break;
					case 1:
						layout_copymenu(&search.address_end, NULL, COPY_PASTE_ADDRESS);
						break;
				}
				break;
			case PSP_CTRL_CROSS:
				x3 = !x3;
				break;
			case PSP_CTRL_CIRCLE:
				ret = ctrl;
				break;
		}
	}

	ctrl_waitrelease();
}

void layout_search(char show_search_mode) {
	u32 ctrl = 0, ret = 0;
	int i, sel = 0;

	int x = (show_search_mode ? 0 : 1), x2 = 0, x3 = 0; // selected block, cursor position, edit mode
	u32 conv_total;
	int search_mode = (show_search_mode ? search.mode : 6);

	pspDebugScreenClear();

	while(ret == 0) {
		// heading
		line_print(0);
		pspDebugScreenSetTextColor(colors.color01);
		printf("  %s", lang.search_edit.title);
		line_print(-1);

		// heading
		puts("  ");

		if(show_search_mode) {
			printf("%-16s", lang.misc.mode);
		}

		if(search_mode > 3) {
			if(menu.cheater.edit_format) {
				printf("%-11s %-9s %s", lang.misc.hex, lang.misc.opcode, lang.misc.args);
			} else {
				printf("%-11s %-11s %-6s %s", lang.misc.hex, lang.misc.dec, lang.misc.ascii, lang.misc._float);
			}
		}

		line_clear(-1);

		pspDebugScreenSetTextColor(sel == 0 ? colors.color01 : colors.color02);

		puts("  ");

		if(show_search_mode) {
			printf("%i=%-14s", search_mode, (char*)(*(u32*)(((u8*)&lang.search_modes) + (search_mode * 4))));
		}

		if(search_mode > 3) {
			// print out the hex
			switch(search.history[0].flags & FLAG_DWORD) {
				case FLAG_DWORD:
					printf("0x%08X  ", MEM_INT(search.history[0].value));
					break;
				case FLAG_WORD:
					printf("0x____%04X  ", MEM_SHORT(search.history[0].value));
					break;
				case FLAG_BYTE:
					printf("0x______%02X  ", MEM(search.history[0].value));
					break;
			}

			if(menu.cheater.edit_format) {
				// print out the opcode
				mipsDecode(search.history[0].value, 0);
			} else {
				// print out the decimal
				switch(search.history[0].flags & FLAG_DWORD) {
					case FLAG_DWORD:
						printf("%010u  ", MEM_INT(search.history[0].value));
						break;
					case FLAG_WORD:
						printf("%010u  ", MEM_SHORT(search.history[0].value));
						break;
					case FLAG_BYTE:
						printf("%010u  ", MEM(search.history[0].value));
						break;
				}

				// print out the ascii
				buffer[0] = MEM_ASCII((search.history[0].value & 0x000000FF) >> 0);
				buffer[1] = MEM_ASCII((search.history[0].value & 0x0000FF00) >> 8);
				buffer[2] = MEM_ASCII((search.history[0].value & 0x00FF0000) >> 16);
				buffer[3] = MEM_ASCII((search.history[0].value & 0xFF000000) >> 24);
				buffer[4] = 0;
				puts(buffer);

				// print out the float
				puts("   ");
				f_cvt(&search.history[0].value, buffer, sizeof(buffer), 6, MODE_GENERIC);
				puts(buffer);
			}
		}

		line_clear(-1);

		if(sel == 0) { // draw the cursor
			i = 4 + x2;
			switch(x) {
				case 3:
					i += 12;
				case 2:
					i += 10;
				case 1:
					if(show_search_mode) {
						i += 16;
					}
					break;
			}

			line_cursor(i, x3 ? colors.color01 : colors.color06);
		} else { // skip a line
			line_clear(-1);
		}

		// menus
		for(i = 1; i < 4; i++) {
			pspDebugScreenSetTextColor(sel == i ? colors.color01 : colors.color02);
			puts("  ");

			switch(i) {
				case 1:
					puts(lang.search_edit.search);
					break;
				case 2:
					puts(lang.search_edit.undo_search);
					break;
				case 3:
					puts(lang.search_edit.add_results);
					break;
			}

			puts("\n");
		}

		puts("\n");

		// search results
		line_print(-1);
		pspDebugScreenSetTextColor(colors.color01);
		puts("  ");
		printf(lang.search_edit.results_count, search.num_results);
		puts("\n");
		line_print(-1);

		pspDebugScreenSetTextColor(colors.color02);

		conv_total = (search.num_results > SEARCH_ADDRESSES_MAX ? SEARCH_ADDRESSES_MAX : search.num_results);

		if(search.num_results) {
			if(menu.cheater.edit_format) {
				printf("  %-11s %-11s %-9s %s", lang.misc.address, lang.misc.hex, lang.misc.opcode, lang.misc.args);
			} else {
				printf("  %-11s %-11s %-11s %-6s %s", lang.misc.address, lang.misc.hex, lang.misc.dec, lang.misc.ascii, lang.misc._float);
			}
			line_clear(-1);

			int start, end;
			get_print_start_end(&start, &end, conv_total, 18, sel - 4);
			for(i = start; i < end; i++) {
				// apply the row color
				pspDebugScreenSetTextColor(i == (sel - 4) ? colors.color01 : colors.color02);

				// print out the address
				printf("  0x%08X  ", address(search.results[i]));

				// print out the hex
				switch(search.history[0].flags & FLAG_DWORD) {
					case FLAG_DWORD:
						printf("0x%08X  ", MEM_VALUE_INT(search.results[i]));
						break;
					case FLAG_WORD:
						printf("0x____%04X  ", MEM_VALUE_SHORT(search.results[i]));
						break;
					case FLAG_BYTE:
						printf("0x______%02X  ", MEM_VALUE(search.results[i]));
						break;
				}

				if(menu.cheater.edit_format) {
					// print out the opcode
					mipsDecode(MEM_VALUE_INT(search.results[i] & 0xFFFFFFFC), search.results[i] & 0xFFFFFFFC);
				} else {
					// print out the decimal
					switch(search.history[0].flags & FLAG_DWORD) {
						case FLAG_DWORD:
							printf("%010u  ", MEM_VALUE_INT(search.results[i]));
							break;
						case FLAG_WORD:
							printf("%010u  ", MEM_VALUE_SHORT(search.results[i]));
							break;
						case FLAG_BYTE:
							printf("%010u  ", MEM_VALUE(search.results[i]));
							break;
					}

					// print out the ascii
					memset(buffer, '.', 4);
					buffer[0] = MEM_ASCII(MEM_VALUE(search.results[i]));
					if(!(search.history[0].flags & FLAG_BYTE)) {
						buffer[1] = MEM_ASCII(MEM_VALUE(search.results[i] + 1));
						if(!(search.history[0].flags & FLAG_WORD)) {
							buffer[2] = MEM_ASCII(MEM_VALUE(search.results[i] + 2));
							buffer[3] = MEM_ASCII(MEM_VALUE(search.results[i] + 3));
						}
					}
					buffer[4] = 0;
					puts(buffer);
					puts("   ");

					// print out the float
					f_cvt(search.results[i], buffer, sizeof(buffer), 6, MODE_GENERIC);
					puts(buffer);
				}

				// goto the next cheat down under
				line_clear(-1);
			}
		} else {
			printf("  %s", lang.search_edit.no_results);
		}

		// clear the last line
		line_clear(-1);

		// helper
		line_print(32);
		pspDebugScreenSetTextColor(colors.color01);
		puts(lang.search_edit.help);
		line_clear(-1);

		switch(ctrl = ctrl_waitmask(0, 0, PSP_CTRL_UP | PSP_CTRL_DOWN | PSP_CTRL_LEFT | PSP_CTRL_RIGHT |
			PSP_CTRL_SQUARE | PSP_CTRL_TRIANGLE | PSP_CTRL_CROSS | PSP_CTRL_CIRCLE |
			PSP_CTRL_START)) {
			case PSP_CTRL_UP:
				if(x3) { // edit search value
					switch(x) {
						case 0:
							search_mode = search.mode = (search_mode < 9 ? search_mode + 1 : 0);
							break;
						case 1:
							search.history[0].value += (1 << (4 * (7 - x2)));
							break;
						case 2:
							search.history[0].value += decDelta[x2];
							break;
						case 3:
							search.history[0].value += (1 << (8 * x2));
							break;
					}
				} else { // scroll list
					sel = (sel > 0 ? sel - 1 : conv_total + 3);
				}
				break;
			case PSP_CTRL_DOWN:
				if(x3) { // edit search value
					switch(x) {
						case 0:
							search_mode = search.mode = (search_mode > 0 ? search_mode - 1 : 9);
							break;
						case 1:
							search.history[0].value -= (1 << (4 * (7 - x2)));
							break;
						case 2:
							search.history[0].value -= decDelta[x2];
							break;
						case 3:
							search.history[0].value -= (1 << (8 * x2));
							break;
					}
				} else { // scroll list
					sel = (sel < conv_total + 3 ? sel + 1 : 0);
				}
				break;
			case PSP_CTRL_LEFT:
				if(sel == 0 && search_mode > 3) {
					x2--;

					switch(x) {
						case 0:
							if(x2 < 0) {
								x2 = 0;
							}
							break;
						case 1:
							if(x2 < 0) {
								if(show_search_mode) {
									x--;
								}
								x2 = 0;
							}
							break;
						case 2:
							if(x2 < 0) {
								x--;
								x2 = 7;
							}
							break;
						case 3:
							if(x2 < 0) {
								x--;
								x2 = 9;
							}
							break;
					}
				}
				break;
			case PSP_CTRL_RIGHT:
				if(sel == 0 && search_mode > 3) {
					x2++;

					switch(x) {
						case 0:
							if(x2 > 0) {
								x++;
								x2 = 0;
							}
							break;
						case 1:
							if(x2 > 7) {
								x++;
								x2 = 0;
							}
							break;
						case 2:
							if(x2 > 9) {
								x++;
								x2 = 0;
							}
							break;
						case 3:
							if(x2 > 3) {
								x2 = 3;
							}
							break;
					}

					if(menu.cheater.edit_format && x > 1) {
						x = 1;
						x2 = 7;
					}
				}
				break;
			case PSP_CTRL_START:
				menu.cheater.edit_format = !menu.cheater.edit_format;
				if(menu.cheater.edit_format) { // opcode mode
					if(x > 1) {
						x = 1;
						x2 = 7;
					}
				}
				break;
			case PSP_CTRL_TRIANGLE:
				if(sel == 0) {
					layout_copymenu(NULL, &search.history[0].value, COPY_PASTE_VALUE);
				} else if(sel > 3) {
					layout_copymenu(&search.results[sel - 4], &MEM_VALUE_INT(search.results[sel - 4]), COPY_ALL | PASTE_VALUE);
				}
				break;
			case PSP_CTRL_SQUARE:
				switch(sel) {
					case 0: // change search size
						if(search.number == 0 && search.num_results == 0) {
							switch(search.history[0].flags & FLAG_DWORD) {
								case FLAG_BYTE:
									search.history[0].flags = (search.history[0].flags & ~FLAG_DWORD) | FLAG_WORD;
									break;
								case FLAG_WORD:
									search.history[0].flags = (search.history[0].flags & ~FLAG_DWORD) | FLAG_DWORD;
									break;
								case FLAG_DWORD:
									search.history[0].flags = (search.history[0].flags & ~FLAG_DWORD) | FLAG_BYTE;
									break;
							}
						}
						break;
					case 1:
					case 2:
						break;
					case 3: // add all results
						if(conv_total > 0) {
							menu.cheater.selected = search_add_loaded_results(CHEAT_CWCHEAT);
							tab_selected = 0;
							ret = ctrl;
						}
						break;
					default: // search results
						menu.cheater.selected = search_add_result(search.results[sel - 4], CHEAT_CWCHEAT);
						tab_selected = 0;
						ret = ctrl;
						break;
				}
				break;
			case PSP_CTRL_CROSS:
				switch(sel) {
					case 0: // search value
						x3 = !x3;
						break;
					case 1: // search
						search_start(search_mode);
						search_load_results();
						pspDebugScreenClear();
						sel = 0;
						break;
					case 2: // undo search
						search_undo();
						pspDebugScreenClear();
						sel = 0;
						break;
					case 3: // add all results
						if(conv_total > 0) {
							menu.cheater.selected = search_add_loaded_results(CHEAT_PSPAR);
							tab_selected = 0;
							ret = ctrl;
						}
						break;
					default: // search results
						menu.cheater.selected = search_add_result(search.results[sel - 4], CHEAT_PSPAR);
						tab_selected = 0;
						ret = ctrl;
						break;
				}
				break;
			case PSP_CTRL_CIRCLE:
				ret = ctrl;
				break;
		}
	}

	ctrl_waitrelease();
}

void layout_textsearch() {
	u32 ctrl = 0, ret = 0;
	int i, j, sel = 0;
	int x = 0, x2 = 0; // cursor position, edit mode
	u32 _address, conv_total;
	char search_text[41];

	memset(search_text, 0, sizeof(search_text));

	pspDebugScreenClear();
	
	while(ret == 0) {
		// fixup search string
		if(search_text[0] == 0) {
			search_text[0] = 'A';
			search_text[1] = 0;
		}
		search_text[40] = 0;
		if(x > strlen(search_text)) {
			x = strlen(search_text) - 1;
		}

		// heading
		line_print(0);
		pspDebugScreenSetTextColor(colors.color01);
		printf("  %s", lang.search_text.title);
		line_print(-1);

		// heading
		printf("  %s\n", lang.misc.text);

		pspDebugScreenSetTextColor(sel == 0 ? colors.color01 : colors.color02);

		printf("  '%s'", search_text);
		line_clear(-1);

		if(sel == 0) { // draw the cursor
			line_cursor(3 + x, x2 ? colors.color01 : colors.color06);
		} else { // skip a line
			line_clear(-1);
		}

		// menus
		pspDebugScreenSetTextColor(sel == 1 ? colors.color01 : colors.color02);
		printf("  %s\n\n", lang.search_edit.search);

		// search results
		line_print(-1);
		pspDebugScreenSetTextColor(colors.color01);
		puts("  ");
		printf(lang.search_edit.results_count, search.num_results);
		line_print(-1);

		pspDebugScreenSetTextColor(colors.color02);

		conv_total = (search.num_results > SEARCH_ADDRESSES_MAX ? SEARCH_ADDRESSES_MAX : search.num_results);

		if(search.num_results) {
			printf("  %-11s %s\n", lang.misc.address, lang.misc.text);

			int start, end;
			get_print_start_end(&start, &end, conv_total, 20, sel);
			for(i = start; i < end; i++) {
				// apply the row color
				pspDebugScreenSetTextColor(i == (sel - 2) ? colors.color01 : colors.color02);

				// print out the address
				printf("  0x%08X  ", address(search.results[i]));

				// print out the hex
				for(j = 0; j < 40; j++) {
					if((search.results[i] + j) > cfg.address_end) {
						break;
					}
					buffer[j] = MEM_ASCII(MEM_VALUE(search.results[i] + j));
				}
				buffer[j + 1] = 0;
				puts(buffer);

				// goto the next cheat down under
				line_clear(-1);
			}

			line_clear(-1);
		} else {
			printf("  %s", lang.search_edit.no_results);
		}

		// helper
		line_print(32);
		pspDebugScreenSetTextColor(colors.color01);
		puts(lang.search_text.help);
		line_clear(-1);

		switch(ctrl = ctrl_waitmask(0, 0, PSP_CTRL_UP | PSP_CTRL_DOWN | PSP_CTRL_LEFT | PSP_CTRL_RIGHT |
			PSP_CTRL_SQUARE | PSP_CTRL_TRIANGLE | PSP_CTRL_CROSS | PSP_CTRL_CIRCLE |
			PSP_CTRL_START)) {
			case PSP_CTRL_UP:
				if(x2) { // edit search value
					search_text[x]++;
					switch(search_text[x]) {
						case 0x01:
							search_text[x] = 0x20;
							break;
						case 0x21:
							search_text[x] = 0x41;
							break;
						case 0x5B:
							search_text[x] = 0x30;
							break;
						case 0x3A:
							search_text[x] = 0x21;
							break;
						case 0x30:
							search_text[x] = 0x3A;
							break;
						case 0x41:
							search_text[x] = 0x5B;
							break;
						case 0x61:
							search_text[x] = 0x7B;
							break;
						case 0x7F:
							search_text[x] = 0x20;
							break;
					}
				} else { // scroll list
					sel = (sel > 0 ? sel - 1 : conv_total + 1);
				}
				break;
			case PSP_CTRL_DOWN:
				if(x2) { // edit search value
					search_text[x]--;
					switch(search_text[x]) {
						case 0x7A:
							search_text[x] = 0x60;
							break;
						case 0x5A:
							search_text[x] = 0x40;
							break;
						case 0x39:
							search_text[x] = 0x2F;
							break;
						case 0x20:
							search_text[x] = 0x39;
							break;
						case 0x2F:
							search_text[x] = 0x5A;
							break;
						case 0x40:
							search_text[x] = 0x20;
							break;
						case 0x1F:
							search_text[x] = 0x7E;
							break;
					}
				} else { // scroll list
					sel = (sel < conv_total + 1 ? sel + 1 : 0);
				}
				break;
			case PSP_CTRL_LEFT:
				if(sel == 0) { // search value
					x = (x > 0 ? x - 1 : 0);
				} else if(sel > 1) { // search results
					search.results[sel - 2] = (search.results[sel - 2] < search.address_start ? search.address_start : search.results[sel - 2] - 1);
				}
				break;
			case PSP_CTRL_RIGHT:
				if(sel == 0) {
					x = (x < 39 ? x + 1 : x);
					if(search_text[x] == 0) {
						search_text[x] = 'A';
						search_text[x + 1] = 0;
					}
				} else if(sel > 1) { // search results
					search.results[sel - 2] = (search.results[sel - 2] > search.address_end ? search.address_end : search.results[sel - 2] + 1);
				}
				break;
			case PSP_CTRL_TRIANGLE:
				if(sel > 1) {
					layout_copymenu(&search.results[sel - 2], NULL, COPY_PASTE_ADDRESS);
				}
				break;
			case PSP_CTRL_SQUARE:
				if(sel == 0) {
					search_text[x + 1] = 0;
				}
				break;
			case PSP_CTRL_CROSS:
				switch(sel) {
					case 0: // search value
						x2 = !x2;
						break;
					case 1: // search
						sel = 0;
						search.history[0].flags = FLAG_BYTE;
						search.num_results = 0;
						_address = search.address_start;

						while(_address < search.address_end) {
							if(!((_address - search.address_start) & 0xFFFF)) {
								if(!cfg.cheat_pause) {
									sceKernelDelayThread(1500);
								}

								pspDebugScreenSetXY(0, 33);
								pspDebugScreenSetTextColor(colors.color02);

								if(ctrl_read() & PSP_CTRL_CIRCLE) {
									puts(lang.misc.task_aborted); 
									line_clear(-1);
									ctrl_waitrelease();
									break;
								} else {
									printf(lang.misc.task_progress, (_address - search.address_start) / (((search.address_end - search.address_start) / 100) + 1));
									line_clear(-1);
								}
							}

							// check
							for(j = 0; j < 40; j++) {
								if((_address + j) > cfg.address_end) {
									j++;
									break;
								} else {
									char letter = MEM_VALUE(_address + j);
									if((letter >= 0x61) && (letter <= 0x7A)) {
										letter -= 0x20;
									}
									if(letter == search_text[j]) {
										if(search_text[j + 1] == 0) {
											search.results[search.num_results] = _address; // add result
											search.num_results++;
											j++;
											break;
										}
									} else {
										if(j == 0) {
											j = 1;
										}
										break;
									}
								}

							}

							_address += j;
							
							if(search.num_results >= SEARCH_ADDRESSES_MAX) {
								break;
							}
						}

						pspDebugScreenClear();
						break;
				}
				break;
			case PSP_CTRL_START:
				ctrl_waitrelease();
				pspDebugKbInit(search_text, sizeof(search_text));
				x = strlen(search_text) - 1;
				pspDebugScreenClear();
				break;
			case PSP_CTRL_CIRCLE:
				ret = ctrl;
				break;
		}
	}

	ctrl_waitrelease();
}

#if defined(_MODLIST_) || defined(_THLIST_) || defined(_UMDDUMP_)
void layout_systools() {
	u32 ctrl = 0, ret = 0;
	int i, sel = 0;

	pspDebugScreenClear();

	while(ret == 0) {
		line_print(0);
		pspDebugScreenSetTextColor(colors.color01);
		printf("  %s", lang.system_tools.title);
		line_print(-1);

		for(i = 0; i < 3; i++) {
			pspDebugScreenSetTextColor(i == sel ? colors.color01 : colors.color02); // highlight selected line
			puts("  ");

			switch(i) {
				case 0:
					puts(lang.system_tools.module_list);
					#ifndef _MODLIST_
					printf(" %s", lang.misc.not_supported);
					#endif
					break;
				case 1:
					puts(lang.system_tools.thread_list);
					#ifndef _THLIST_
					printf(" %s", lang.misc.not_supported);
					#endif
					break;
				case 2:
					puts(lang.system_tools.umd_dumper);
					#ifndef _UMDDUMP_
					printf(" %s", lang.misc.not_supported);
					#endif
					break;
			}

			puts("\n");
		}

		// helper
		line_print(32);
		pspDebugScreenSetTextColor(colors.color01);
		puts(lang.helper.select_cancel);

		switch(ctrl = ctrl_waitmask(0, 0, PSP_CTRL_UP | PSP_CTRL_DOWN | PSP_CTRL_CROSS | PSP_CTRL_CIRCLE)) {
			case PSP_CTRL_UP:
				sel = (sel > 0 ? sel - 1 : 2);
				break;
			case PSP_CTRL_DOWN:
				sel = (sel < 2 ? sel + 1 : 0);
				break;
			case PSP_CTRL_CROSS:
				switch(sel) {
					case 0:
						#ifdef _MODLIST_
						layout_modlist();
						#endif
						break;
					case 1:
						#ifdef _THLIST_
						layout_thlist();
						#endif
						break;
					case 2:
						#ifdef _UMDDUMP_
						layout_umddumper();
						#endif
						break;
				}
				break;
			case PSP_CTRL_CIRCLE:
				ret = ctrl;
				break;
		}
	}

	pspDebugScreenClear();
}
#endif

#ifdef _MODLIST_
void layout_modlist() {
	u32 ctrl = 0, ret = 0;
	int i, sel = 0;

	pspDebugScreenClear();

	// module variables
	SceUID mod_uid[100];
	int mod_count = 0;
	SceModule2 *mod_info;

	sceKernelGetModuleIdList(mod_uid, sizeof(mod_uid), &mod_count);

	while(ret == 0) {
		// heading
		line_print(0);
		pspDebugScreenSetTextColor(colors.color01);
		puts("  ");
		printf(lang.module_list.title, mod_count);
		line_print(-1);

		// sub-heading
		pspDebugScreenSetTextColor(colors.color02);
		printf("  %s", lang.module_list.heading);

		line_clear(-1);

		int start, end;
		get_print_start_end(&start, &end, mod_count, 28, sel);
		for(i = start; i < end; i++) {
			mod_info = sceKernelFindModuleByUID(mod_uid[i]);
			if(mod_info) {
				//pspDebugScreenSetTextColor(i == sel ? colors.color01 - (mod_info->entry_addr == 0x08804000 ?  (10 * colors.color01_to) : 0) : colors.color02 - (mod_info->entry_addr == 0x08804000 ?  (10 * colors.color02_to) : 0)); // highlight selected line
				pspDebugScreenSetTextColor(i == sel ? colors.color01 : colors.color02); // highlight selected line
				printf("  0x%08X  %04X  %s", mod_info->modid, mod_info->attribute, mod_info->modname);
				line_clear(-1);
			}
		}

		line_clear(-1);

		// helper
		line_print(32);
		pspDebugScreenSetTextColor(colors.color01);

		puts(lang.helper.select_cancel);

		switch(ctrl = ctrl_waitmask(0, 0, PSP_CTRL_UP | PSP_CTRL_DOWN |
			PSP_CTRL_CROSS | PSP_CTRL_CIRCLE)) {
			case PSP_CTRL_UP:
				sel = (sel > 0 ? sel - 1 : mod_count - 1);
				break;
			case PSP_CTRL_DOWN:
				sel = (sel < mod_count - 1 ? sel + 1 : 0);
				break;
			case PSP_CTRL_CROSS:
				layout_modinfo(mod_uid[sel]);
				break;
			case PSP_CTRL_CIRCLE:
				ret = ctrl;
				break;
		}
	}

	ctrl_waitrelease();
	pspDebugScreenClear();
}
void layout_modinfo(SceUID uid) {
	u32 ctrl, ret = 0;
	int i, j;

	SceModule2 *mod_info;
	mod_info = sceKernelFindModuleByUID(uid);

	if(mod_info) {
		pspDebugScreenClear();

		// heading
		line_print(0);
		pspDebugScreenSetTextColor(colors.color01);
		puts("  ");
		printf(lang.module_info.title, mod_info->modname);
		line_print(-1);

		// modinfo
		printf("  Name:     %s\n", mod_info->modname);
		printf("  Version:  %i.%i\n", mod_info->version[1], mod_info->version[0]);
		printf("  UID:      0x%08X\n", mod_info->modid);
		printf("  Attr:     0x%04X\n\n", mod_info->attribute);
		printf("  Entry:    0x%08X  GP:       0x%08X\n", mod_info->entry_addr, mod_info->gp_value);
		printf("  TextAddr: 0x%08X  TextSize: 0x%08X\n", mod_info->text_addr, mod_info->text_size);
		printf("  DataSize: 0x%08X  BssSize:  0x%08X\n\n", mod_info->data_size, mod_info->bss_size);

		j = 0;
		for(i = 0; i < 4; i++) {
			if(mod_info->segmentsize[i] > 0) {
				printf("  Segment %i\n", j);
				printf("  Addr:     0x%08X  Size:     0x%08X\n\n", mod_info->segmentaddr[i], mod_info->segmentsize[i]);
				j++;
			}
		}

		// helper
		line_print(32);
		pspDebugScreenSetTextColor(colors.color01);
		puts(lang.module_info.help);

		switch(ctrl = ctrl_waitmask(0, 0, PSP_CTRL_CIRCLE)) {
			case PSP_CTRL_CIRCLE:
				ret = ctrl;
				break;
		}

		ctrl_waitrelease();
	}
}
#endif

#ifdef _THLIST_
void layout_thlist() {
	u32 ctrl = 0, ret = 0;
	int i, sel = 0;

	pspDebugScreenClear();

	// module variables
	SceUID th_uid[100];
	int th_count = 0;
	SceKernelThreadInfo th_info;

	sceKernelGetThreadmanIdList(SCE_KERNEL_TMID_Thread, &th_uid, 100, &th_count);

	while(ret == 0) {
		// heading
		line_print(0);
		pspDebugScreenSetTextColor(colors.color01);
		puts("  ");
		printf(lang.thread_list.title, th_count);
		line_print(-1);

		if(th_count > 0) {
			// heading
			pspDebugScreenSetTextColor(colors.color02);
			printf("  %s", lang.thread_list.heading);

			line_clear(-1);

			int start, end;
			get_print_start_end(&start, &end, th_count, 28, sel);
			for(i = start; i < end; i++) {
				th_info.size = sizeof(SceKernelThreadInfo);
				pspDebugScreenSetTextColor(i == sel ? colors.color01 : colors.color02); // highlight selected line
				printf("  0x%08X  ", th_uid[i]);
				puts(sceKernelReferThreadStatus(th_uid[i], &th_info) >= 0 ? th_info.name : "-");
				line_clear(-1);
			}

			line_clear(-1);
		}

		// helper
		line_print(32);
		pspDebugScreenSetTextColor(colors.color01);
		printf(lang.helper.select_cancel);

		switch(ctrl = ctrl_waitmask(0, 0, PSP_CTRL_UP | PSP_CTRL_DOWN |
			PSP_CTRL_CROSS | PSP_CTRL_CIRCLE)) {
			case PSP_CTRL_UP:
				sel = (sel > 0 ? sel - 1 : th_count - 1);
				break;
			case PSP_CTRL_DOWN:
				sel = (sel < th_count - 1 ? sel + 1 : 0);
				break;
			case PSP_CTRL_CROSS:
				layout_thinfo(th_uid[sel]);
				break;
			case PSP_CTRL_CIRCLE:
				ret = ctrl;
				break;
		}
	}

	ctrl_waitrelease();
	pspDebugScreenClear();
}
void layout_thinfo(SceUID uid) {
	u32 ctrl, ret = 0;

	SceKernelThreadInfo th_info;
	th_info.size = sizeof(SceKernelThreadInfo);

	if(sceKernelReferThreadStatus(uid, &th_info) >= 0) {
		pspDebugScreenClear();

		// heading
		line_print(0);
		pspDebugScreenSetTextColor(colors.color01);
		puts("  ");
		printf(lang.thread_info.title, th_info.name);
		line_print(-1);

		// thinfo
		printf("  Name:         %s\n", th_info.name);
		printf("  UID:          0x%08X\n", uid);
		printf("  Attr:         0x%08X\n\n", th_info.attr);
		printf("  Status:       %i (", th_info.status);

		switch(th_info.status) {
			case PSP_THREAD_RUNNING:
				puts("RUNNING");
				break;
			case PSP_THREAD_READY:
				puts("READY");
				break;
			case PSP_THREAD_WAITING:
				puts("WAITING");
				break;
			case PSP_THREAD_SUSPEND:
				puts("SUSPEND");
				break;
			case PSP_THREAD_STOPPED:
				puts("STOPPED");
				break;
			case PSP_THREAD_KILLED:
				puts("KILLED");
				break;
			default:
				puts("UNKNOWN");
				break;
		}

		printf(")\n");

		printf("  Entry:        0x%08X  GP:           0x%08X\n", (u32)th_info.entry, (u32)th_info.gpReg);
		printf("  Stack:        0x%08X  StackSize:    0x%08X\n", (u32)th_info.stack, th_info.stackSize);
		printf("  InitPri:      % 10i  CurrPri:      % 10i\n", th_info.initPriority, th_info.currentPriority);
		printf("  WaitType:     % 10i  WaitId:       0x%08X\n", th_info.waitType, th_info.waitId);
		printf("  WakeupCount:  % 10i  ExitStatus:   0x%08X\n", th_info.wakeupCount, th_info.exitStatus);
		printf("  RunClocks:    % 10i  IntrPrempt:   % 10i\n", th_info.runClocks.low, th_info.intrPreemptCount);
		printf("  ThreadPrempt: % 10i  ReleaseCount: % 10i\n", th_info.threadPreemptCount, th_info.releaseCount);
		printf("  StackFree:    % 10i\n", sceKernelGetThreadStackFreeSize(uid));

		// helper
		line_print(32);
		pspDebugScreenSetTextColor(colors.color01);
		puts(lang.thread_info.help);

		switch(ctrl = ctrl_waitmask(0, 0, PSP_CTRL_CIRCLE)) {
			case PSP_CTRL_CIRCLE:
				ret = ctrl;
				break;
		}

		ctrl_waitrelease();
	}
}
#endif

#ifdef _UMDDUMP_
void layout_umddumper() {
	u32 ctrl = 0, ret = 0;
	void *buf = NULL;
	SceUID fd_i = -1, fd_o = -1;
	int umd_sector, now_sector, read_sec, suspend_interval, new_suspend_interval = 0, max_buf = 512;

	pspDebugScreenClear();

	while(ret == 0) {
		// heading
		line_print(-1);
		pspDebugScreenSetTextColor(colors.color01);
		printf("  %s\n", lang.umd_dumper.title);
		line_print(-1);

		pspDebugScreenSetTextColor(colors.color01);
		printf("  %s", lang.umd_dumper.dump_umd);

		pspDebugScreenSetTextColor(colors.color02);
		puts(lang.umd_dumper.message);

		// helper
		line_print(32);
		pspDebugScreenSetTextColor(colors.color01);

		puts(lang.helper.select_cancel);
		line_clear(-1);

		switch(ctrl = ctrl_waitmask(0, 0, PSP_CTRL_CROSS | PSP_CTRL_CIRCLE)) {
			case PSP_CTRL_CROSS:
				// disable sleep mode
				get_registry_value("/CONFIG/SYSTEM/POWER_SAVING", "suspend_interval", &suspend_interval);
				set_registry_value("/CONFIG/SYSTEM/POWER_SAVING", "suspend_interval", &new_suspend_interval);

				if(sceKernelInitKeyConfig() == PSP_INIT_KEYCONFIG_VSH) {
					show_error(lang.errors.no_game_loaded);
				} else {
					// allocate the umd buffer
					do {
						buf = kmalloc(0, PSP_SMEM_Low, max_buf * 0x800);
						if(buf == NULL) {
							max_buf -= 2;
						}
					} while(buf == NULL && max_buf > 0);

					if(max_buf <= 0) {
						show_error(lang.errors.memory_full);
					} else {
						// get the number of sectors
						fd_i = sceIoOpen("umd:", PSP_O_RDONLY, 0777);
						sceIoLseek(fd_i, 16, SEEK_SET);
						sceIoRead(fd_i, buf, 1);
						sceIoClose(fd_i);
						umd_sector = *(u32*)(buf + 0x50);
						now_sector = 0;
				
						sprintf(buffer, "%s/ISO/%s.iso", plug_drive, gameid_get(0));

						// make sure output file doesn't exist
						fd_i = sceIoOpen(buffer, PSP_O_RDONLY, 0777);
						if(fd_i >= 0) {
							sceIoClose(fd_i);
							show_error(lang.errors.file_exists);
						} else {
							// open the input and output file
							fd_i = sceIoOpen("umd:", PSP_O_RDONLY, 0777);
							fd_o = sceIoOpen(buffer, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);

							if(fd_i < 0 || fd_o < 0) {
								show_error(lang.errors.file_cant_open);
							} else {
								read_sec = sceIoRead(fd_i, buf, max_buf);

								do {
									// write from buffer to file
									if(sceIoWrite(fd_o, buf, read_sec * 0x800) != read_sec * 0x800) {
										show_error(lang.errors.file_cant_write);
										break;
									}

									now_sector += read_sec;

									// read to buffer from disc0
									read_sec = sceIoRead(fd_i, buf, max_buf);
									if((read_sec < 0) && (now_sector < umd_sector)) {
										show_error(lang.errors.file_cant_read);
										break;
									}

									// print the status
									line_clear(33);
									pspDebugScreenSetTextColor(colors.color02);
									printf(lang.misc.task_progress, (now_sector * 100 / umd_sector));

									// quit the dump
									if(!(now_sector & 100)) {
										if(!cfg.cheat_pause) { // delay dump so we don't crash
											sceKernelDelayThread(1500);
										}

										if(ctrl_read() & PSP_CTRL_CIRCLE) {
											line_clear(33);
											pspDebugScreenSetTextColor(colors.color02);
											puts(lang.misc.task_aborted);
											ctrl_waitrelease();
											break;
										}
									}
								} while(read_sec > 0);
							}
						}
					}
				}

				// enable sleep mode
				set_registry_value("/CONFIG/SYSTEM/POWER_SAVING", "suspend_interval", &suspend_interval);

				// free the buffer
				kfree(buf);

				// close the files
				if(fd_i >= 0) {
					sceIoClose(fd_i);
					fd_i = -1;
				}

				if(fd_o >= 0) {
					sceIoClose(fd_o);
					if((umd_sector == 0) || (now_sector < umd_sector)) {
						sceIoRemove(buffer);
					}
					fd_o = -1;
				}
				break;
			case PSP_CTRL_CIRCLE:
				ret = ctrl;
				break;
		}
	}

	ctrl_waitrelease();
	pspDebugScreenClear();
}
#endif

/* tabs */

int layout_tab() {
	u32 ctrl = 0;

	tab_selected = 0;

	while(!(ctrl & PSP_CTRL_CIRCLE)) {
		pspDebugScreenClear(); // clear the screen
		layout_heading(); // draw the tabs

		switch(tab_selected) {
			case 0:
				ctrl = layout_cheats();
				break;
			case 1:
				ctrl = layout_searcher();
				break;
			case 2:
				ctrl = layout_options();
				break;
			case 3:
				ctrl = layout_browser();
				break;
			case 4:
				ctrl = layout_credits();
				break;
		}

		if(ctrl & PSP_CTRL_LTRIGGER) { // previous tab
			if(tab_selected > 0) {
				tab_selected--;
			}
		} else if(ctrl & PSP_CTRL_RTRIGGER) { // next tab
			if(tab_selected < 4) {
				tab_selected++;
			}
		}
	}

	return ctrl;
}

void layout_heading() {
	int i;

	line_print(0);

	puts("  ");

	for(i = 0; i < 5; i++) {
		pspDebugScreenSetTextColor(i == tab_selected ? colors.color03 : colors.color04); // highlight selected tab

		switch(i) {
			case 0:
				puts(lang.tabs.cheater);
				break;
			case 1:
				puts(lang.tabs.searcher);
				break;
			case 2:
				puts(lang.tabs.prx);
				break;
			case 3:
				printf((menu.bd.view ? lang.tabs.browser : lang.tabs.decoder), menu.bd.no);
				break;
			case 4:
				puts(gameid_get(0));
				break;
		}

		puts(" ");
	}

	if(cfg.cheat_status) {
		pspDebugScreenSetTextColor(0xFF00FF00);
		puts(lang.tabs.cheats_on);
	} else {
		pspDebugScreenSetTextColor(colors.color03);
		puts(lang.tabs.cheats_off);
	}

	line_print(-1);
}

u32 layout_cheats()  {
	u32 ctrl = 0, ret = 0;
	int repeat_time = 0, repeat_interval = 0;
	int i, j, k;

	int battery_temp = scePowerGetBatteryTemp();
	int battery_life_percent = scePowerGetBatteryLifePercent();
	int battery_life_percent_color = percentage_to_color(battery_life_percent);

	if(menu.cheater.selected == NULL) {
		menu.cheater.selected = cheat_get(0);
	}

	while(ret == 0) {
		i = j = k = 0;

		if(cheat_total > 0) {
			// get the code to finish printing at
			for(i = cheat_get_index(menu.cheater.selected); i < cheat_total; i++) {
				if(cheat_is_visible(cheat_get(i))) {
					if(j++ >= 12) {
						break;
					}
				}
			}

			// get code to start printing at
			for(i = cheat_get_index(menu.cheater.selected); i > 0; i--) {
				if(cheat_is_visible(cheat_get(i))) {
					if(k++ >= 25 - j) {
						break;
					}
				}
			}

			pspDebugScreenSetXY(0, 3);

			for(k = 0; k < 25; k++) {
				Cheat *cheat = cheat_get(i);

				if(cheat == menu.cheater.selected) {
					pspDebugScreenSetTextColor(colors.color01);
					puts(" > ");
				} else {
					puts("   ");
				}

				// force hightlight cheat if it is folder or comment
				if(cheat->length == 0) {
					pspDebugScreenSetTextColor(colors.color10);
				} else if(cheat_is_folder(cheat)) {
					switch(cheat_is_folder(cheat) & FOLDER_TYPES) {
						case FOLDER_SINGLE_SELECT: // one-hot
							pspDebugScreenSetTextColor(colors.color09);
							break;
						case FOLDER_MULTI_SELECT: // multi-select
							pspDebugScreenSetTextColor(colors.color08);
							break;
						default: // comment
							pspDebugScreenSetTextColor(colors.color10);
							break;
					}
				} else {
					//cheat status info
					switch(cheat->flags & (CHEAT_SELECTED | CHEAT_CONSTANT)) {
						case CHEAT_SELECTED:
							pspDebugScreenSetTextColor(colors.color07);
							printf("[%s] ", lang.misc.y);
							break;
						case CHEAT_CONSTANT:
							pspDebugScreenSetTextColor(colors.color06);
							printf("[%s] ", lang.misc.y);
							break;
						default:
							pspDebugScreenSetTextColor(colors.color05);
							printf("[%s] ", lang.misc.n);
							break;
					}

					// if selected highlight else don't
					if(cheat != menu.cheater.selected) {
						pspDebugScreenSetTextColor(colors.color02);
					}
				}

				puts(cheat->name);

				if(cheat_folder_size(cheat)) {
					printf(" (%i)", cheat_folder_size(cheat));
				}

				line_clear(-1);

				if(cheat->flags & CHEAT_HIDDEN) {
					i += cheat_folder_size(cheat);
				}

				if(++i >= cheat_total) {
					break;
				}
			}
		} else {
			pspDebugScreenSetXY(0, 3);
		}

		// clear left over rubbish off the screen
		for(i = pspDebugScreenGetY(); i < 28; i++) {
			line_clear(-1);
		}

		line_print(-1);
		pspDebugScreenSetTextColor(colors.color01);

		if(menu.cheater.show_game_name && game_name[0] != 0) {
			printf("  %s", game_name);
		} else {
			printf("  "VER_STR" %s", (sceKernelInitKeyConfig() == PSP_INIT_KEYCONFIG_POPS ? "POPS " : ""));

			// battery percentage
			pspDebugScreenSetTextColor(colors.color02);
			puts(lang.cheater.bat);
			if(battery_life_percent < 0) {
				pspDebugScreenSetTextColor(colors.color01);
				puts("N/A ");
			} else {
				pspDebugScreenSetTextColor(battery_life_percent_color);
				printf("%i%% ", battery_life_percent);
			}

			// battery temperature
			pspDebugScreenSetTextColor(colors.color02);
			puts(lang.cheater.temp);
			pspDebugScreenSetTextColor(colors.color01); 
			if(battery_temp < 0) {
				puts("N/A ");
			} else {
				printf("%i C ", battery_temp);
			}

			// cheat count
			pspDebugScreenSetTextColor(colors.color02);
			puts(lang.cheater.cheats);
			pspDebugScreenSetTextColor(colors.color01);
			printf("%i ", cheat_total);
		}

		// helper
		line_print(30);

		pspDebugScreenSetTextColor(colors.color01);
		puts(lang.cheater.help);

		// wait until keys released on initial menu draw
		if(ctrl == 0) {
			ctrl_waitrelease();
		}

		ctrl = ctrl_waitmask(repeat_time, repeat_interval,
			PSP_CTRL_UP | PSP_CTRL_DOWN | PSP_CTRL_LEFT | PSP_CTRL_RIGHT |
			PSP_CTRL_ANALOG_UP | PSP_CTRL_ANALOG_DOWN | PSP_CTRL_ANALOG_LEFT | PSP_CTRL_ANALOG_RIGHT |
			PSP_CTRL_SQUARE | PSP_CTRL_TRIANGLE | PSP_CTRL_CROSS | PSP_CTRL_CIRCLE |
			PSP_CTRL_START | PSP_CTRL_VOLDOWN | PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | cfg.trigger_key);

		repeat_time = CTRL_REPEAT_TIME;
		repeat_interval = CTRL_REPEAT_INTERVAL;

		// dpad/analog
		switch(ctrl & (PSP_CTRL_UP | PSP_CTRL_DOWN | PSP_CTRL_LEFT | PSP_CTRL_RIGHT |
			PSP_CTRL_ANALOG_UP | PSP_CTRL_ANALOG_DOWN | PSP_CTRL_ANALOG_LEFT | PSP_CTRL_ANALOG_RIGHT)) {
			case PSP_CTRL_UP:
				menu.cheater.selected = cheat_previous_visible(menu.cheater.selected);
				break;
			case PSP_CTRL_DOWN:
				menu.cheater.selected = cheat_next_visible(menu.cheater.selected);
				break;
			case PSP_CTRL_LEFT:
			case PSP_CTRL_ANALOG_UP:
			case PSP_CTRL_ANALOG_LEFT:
				repeat_time = CTRL_REPEAT_TIME_QUICK;
				repeat_interval = CTRL_REPEAT_INTERVAL_QUICK;
				menu.cheater.selected = cheat_previous_visible(menu.cheater.selected);
				break;
			case PSP_CTRL_RIGHT:
			case PSP_CTRL_ANALOG_DOWN:
			case PSP_CTRL_ANALOG_RIGHT:
				repeat_time = CTRL_REPEAT_TIME_QUICK;
				repeat_interval = CTRL_REPEAT_INTERVAL_QUICK;
				menu.cheater.selected = cheat_next_visible(menu.cheater.selected);
				break;
		}

		// face buttons
		switch(ctrl & (PSP_CTRL_TRIANGLE | PSP_CTRL_SQUARE | PSP_CTRL_CROSS)) {
			case PSP_CTRL_TRIANGLE:
				layout_cheatmenu(menu.cheater.selected);
				layout_heading();
				break;
			case PSP_CTRL_SQUARE:
			case PSP_CTRL_CROSS:
				repeat_time += CTRL_REPEAT_INCREMENT;

				if(cheat_is_folder(menu.cheater.selected)) { // toggle folder
					cheat_folder_toggle(menu.cheater.selected);
				} else if(menu.cheater.selected->length > 0) { // toggle cheat
					if(ctrl & PSP_CTRL_CROSS) {
						cheat_set_status(menu.cheater.selected, ((menu.cheater.selected->flags & CHEAT_SELECTED) ? 0 : CHEAT_SELECTED));
					} else if(ctrl & PSP_CTRL_SQUARE) {
						cheat_set_status(menu.cheater.selected, ((menu.cheater.selected->flags & CHEAT_CONSTANT) ? 0 : CHEAT_CONSTANT));
					}
				}
				break;
		}
		
		// misc
		if(ctrl & PSP_CTRL_START) {
			cheat_collapse_folders(-1);
			while(!cheat_is_visible(menu.cheater.selected)) {
				menu.cheater.selected = menu.cheater.selected->parent;
			}
		} else if(ctrl & PSP_CTRL_VOLDOWN) {
			menu.cheater.show_game_name = !menu.cheater.show_game_name;
		} else if(ctrl & (PSP_CTRL_RTRIGGER | PSP_CTRL_CIRCLE)) {
			ret = ctrl;
		} else if(ctrl & (cfg.trigger_key | PSP_CTRL_LTRIGGER)) {
			if(cfg.trigger_key || (ctrl & PSP_CTRL_LTRIGGER)) {
				cfg.cheat_status = !cfg.cheat_status;
				cheat_apply(CHEAT_TOGGLE_ALL);
				layout_heading();
			}
		}
	}

	return ret;
}

u32 layout_searcher() {
	u32 ctrl, ret = 0;
	int i, sel = 0;

	while(ret == 0) {
		pspDebugScreenSetXY(0, 3);

		for(i = 0; i < 3 + (search.started ? 0 : 4); i++) {
			// highlight selected
			pspDebugScreenSetTextColor(i == sel ? colors.color01 : colors.color02);

			puts("  ");

			if(!search.started) { // new serch
				switch(i) {
					case 0:
						puts(lang.searcher.find_exact_value);
						break;
					case 1:
						printf("%s - %i%s", lang.searcher.find_unknown_value, 8, lang.misc.bit);
						break;
					case 2:
						printf("%s - %i%s", lang.searcher.find_unknown_value, 16, lang.misc.bit);
						break;
					case 3:
						printf("%s - %i%s", lang.searcher.find_unknown_value, 32, lang.misc.bit);
						break;
					case 4:
						puts(lang.searcher.find_text);
						break;
					case 5:
						puts(lang.searcher.search_settings);
						break;
					case 6:
						puts(lang.searcher.reset_search);
						break;
				}
			} else { // continue search
				switch(i) {
					case 0:
						puts(lang.searcher.find_exact_value);
						break;
					case 1:
						puts(lang.searcher.find_unknown_value);
						break;
					case 2:
						puts(lang.searcher.reset_search);
						break;
				}
			}

			puts("\n");
		}

		puts("\n");

		// print out search history
		line_print(-1);
		pspDebugScreenSetTextColor(colors.color01);
		printf("  %s\n", lang.searcher.search_history);
		line_print(-1);

		pspDebugScreenSetTextColor(colors.color02);

		if(search.num_history) {
			printf("  %-15s %-11s %-11s %-6s %-5s\n", lang.misc.mode, lang.misc.hex, lang.misc.dec, lang.misc.ascii, lang.misc._float);

			for(i = 1; i < search.num_history + 1; i++) {
				// apply the row color
				pspDebugScreenSetTextColor(colors.color01 - (i * colors.color01_to));

				// print out the mode
				printf("  %i=%-14s", (search.history[i].flags & 0xF), (char*)(*(u32*)(((u8*)&lang.search_modes) + ((search.history[i].flags & 0xF) * 4))));

				if((search.history[i].flags & 0xF) > 3) {
					// print out the hex
					switch(search.history[i].flags & FLAG_DWORD) {
						case FLAG_DWORD:
							printf("0x%08X  ", MEM_INT(search.history[i].value));
							break;
						case FLAG_WORD:
							printf("0x____%04X  ", MEM_SHORT(search.history[i].value));
							break;
						case FLAG_BYTE:
							printf("0x______%02X  ", MEM(search.history[i].value));
							break;
					}

					// print out the decimal
					switch(search.history[i].flags & FLAG_DWORD) {
						case FLAG_DWORD:
							printf("%010u  ", MEM_INT(search.history[i].value));
							break;
						case FLAG_WORD:
							printf("%010u  ", MEM_SHORT(search.history[i].value));
							break;
						case FLAG_BYTE:
							printf("%010u  ", MEM(search.history[i].value));
							break;
					}

					// print out the ascii
					buffer[0] = MEM_ASCII((search.history[i].value & 0x000000FF) >> 0);
					buffer[1] = MEM_ASCII((search.history[i].value & 0x0000FF00) >> 8);
					buffer[2] = MEM_ASCII((search.history[i].value & 0x00FF0000) >> 16);
					buffer[3] = MEM_ASCII((search.history[i].value & 0xFF000000) >> 24);
					buffer[4] = 0;
					puts(buffer);

					// print out the float
					puts("   ");
					f_cvt(&search.history[i].value, buffer, sizeof(buffer), 6, MODE_GENERIC);
					puts(buffer);
					
				}

				// goto the next line
				puts("\n");
			}
		} else {
			printf("  %s", lang.searcher.no_search_history);
		}

		//Helper
		line_print(32);
		pspDebugScreenSetTextColor(colors.color01);
		puts(lang.helper.select_cancel);  

		switch(ctrl = ctrl_waitmask(0, 0, PSP_CTRL_UP | PSP_CTRL_DOWN |
			PSP_CTRL_CROSS | PSP_CTRL_CIRCLE | PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER)) {
			case PSP_CTRL_UP:
				sel = (sel > 0 ? sel - 1 : (2 + (search.started ? 0 : 4)));
				break;
			case PSP_CTRL_DOWN:
				sel = (sel < (2 + (search.started ? 0 : 4)) ? sel + 1 : 0);
				break;
			case PSP_CTRL_CROSS:
				if(!search.started) { // new search
					switch(sel) {
						case 0: // exact
							layout_search(0);
							break;
						case 1: // unknown
						case 2:
						case 3:
							line_clear(33);
							pspDebugScreenSetTextColor(colors.color02);
							printf("%s... ", lang.misc.working);

							search_reset(0, 0);

							if(dump_memory("searches/search.ram", (void*)search.address_start, (search.address_end - search.address_start))) {
								search.started = 1;
								search.history[0].flags &= ~FLAG_DWORD;

								switch(sel) {
									case 1:
										search.history[0].flags |= FLAG_BYTE;
										break;
									case 2:
										search.history[0].flags |= FLAG_WORD;
										break;
									case 3:
										search.history[0].flags |= FLAG_DWORD;
										break;
								}

								puts(lang.misc.done);
							} else {
								show_error(lang.errors.storage_full);
							}
							sceKernelDelayThread(1 * 1000 * 1000);
							break;
						case 4: // text
							layout_textsearch();
							break;
						case 5: // search range
							layout_searchsettings();
							break;
						case 6: // reset search
							search_reset(1, 1);
							break;
					}
				} else { // continue search
					switch(sel) {
						case 0: // exact
							layout_search(0);
							break;
						case 1: // unknown
							layout_search(1);
							break;
						case 2: // reset search
							search_reset(1, 1);
							break;
					}
				}
				sel = 0;
			case PSP_CTRL_LTRIGGER:
			case PSP_CTRL_RTRIGGER:
			case PSP_CTRL_CIRCLE:
				ret = ctrl;
				break;
		}
	}

	return ret;
}

u32 layout_options() {
	u32 ctrl = 0, ret = 0;
	int i, sel = 0;
	int add = 0;

	while(ret == 0) {
		pspDebugScreenSetXY(0, 3);

		int start, end;
		get_print_start_end(&start, &end, 31, 29, sel);
		for(i = start; i < end; i++) {
			pspDebugScreenSetTextColor(i == sel ? colors.color01 : colors.color02); // highlight selected line
			puts("  ");

			switch(i) {
				case 0:
					printf(lang.options.pause_game, (cfg.cheat_pause || menu.options.force_pause ? lang.misc._true : lang.misc._false));
					break;
				case 1:
					printf(lang.options.dump_ram, menu.options.dump_no);
					break;
				case 2:
					puts(lang.options.dump_kmem);
					break;
				case 3:
					printf(lang.options.real_addressing, (cfg.address_format == 0 ? lang.misc._true : lang.misc._false));
					break;
				case 4:
					printf(lang.options.cheat_hz, (cfg.cheat_hz / 1000));
					break;
				case 5:
					printf(lang.options.hijack_pspar_buttons, (cfg.hijack_pspar_buttons ? lang.misc._true : lang.misc._false));
					break;
				case 6:
					printf(lang.options.hb_all_pbps, (cfg.hbid_pbp_force ? lang.misc._true : lang.misc._false));
					break;
				case 7:
					printf(lang.options.load_color_file, cfg.color_file);
					break;
				case 8:
					printf(lang.options.max_text_viewer_lines, cfg.max_text_rows);
					break;
				case 9:
					printf(lang.options.swap_xo, (cfg.swap_xo ? lang.misc._true : lang.misc._false));
					break;
				case 10:
					puts(lang.options.usb);
					#ifdef _USB_
					printf(" %s", (usb_started ? lang.misc.on : lang.misc.off));
					#else
					printf(" %s", lang.misc.not_supported);
					#endif
					break;
				case 11:
					puts(lang.options.corrupt_psid);
					#ifndef _PSID_
					printf(" %s", lang.misc.not_supported);
					#endif
					break;
				case 12:
					puts(lang.options.load_memory_patch);
					break;
				case 13:
					printf(lang.options.change_language, lang.info.name, lang.info.author);
					break;
				case 15:
					puts(lang.options.change_menu_key);
					break;
				case 16:
					puts(lang.options.change_trigger_key);
					break;
				case 17:
					puts(lang.options.change_screenshot_key);
					#ifndef _SCREENSHOT_
					printf(" %s", lang.misc.not_supported);
					#endif
					break;
				case 19:
					printf(lang.options.max_cheats, cfg.max_cheats);
					break;
				case 20:
					printf(lang.options.max_blocks, cfg.max_blocks);
					break;
				case 21:
					#ifdef _AUTOOFF_
					printf(lang.options.enable_autooff, (cfg.auto_off ? lang.misc._true : lang.misc._false));
					#else
					printf(lang.options.enable_autooff, lang.misc._false);
					printf(" %s", lang.misc.not_supported);
					#endif
					break;
				case 23:
					puts(lang.options.save_cheats);
					printf(" (%s.db)", gameid_get(0));
					break;
				case 24:
					puts(lang.options.load_cheats);
					switch(cfg.cheat_file) {
						case -1:
							printf(" (%s (.db/.bin/.txt), index %i)", gameid_get(0), menu.options.cheat_index + 1);
							break;
						case 0:
							printf(" (cheat (.db/.bin), index %i)", menu.options.cheat_index + 1);
							break;
						default:
							printf(" (cheat%i (.db/.bin), index %i)", cfg.cheat_file, menu.options.cheat_index + 1);
							break;
					}
					break;
				case 26:
					puts(lang.options.reset_settings);
					break;
				case 27:
					puts(lang.options.save_settings);
					break;
				case 29:
					puts(lang.options.system_tools);
					#if !defined(_MODLIST_) && !defined(_THLIST_) && !defined(_UMDDUMP_)
					printf(" %s", lang.misc.not_supported);
					#endif
					break;
				case 30:
					puts(lang.options.kill_tempar);
					break;
			}

			line_clear(-1);
		}

		line_clear(-1);

		// helper
		line_print(32);
		pspDebugScreenSetTextColor(colors.color01);

		if(sel == 1 || sel == 4 || sel == 7 || sel == 8 || sel == 18 || sel == 19 || sel == 22 || sel == 23) {
			puts(lang.options.help_increment);
		} else {
			puts(lang.helper.select_cancel);
		}

		line_clear(-1);

		switch(ctrl = ctrl_waitmask(0, 0, PSP_CTRL_UP | PSP_CTRL_DOWN | PSP_CTRL_LEFT | PSP_CTRL_RIGHT |
			PSP_CTRL_SQUARE | PSP_CTRL_TRIANGLE | PSP_CTRL_CROSS | PSP_CTRL_CIRCLE |
			PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER)) {
			case PSP_CTRL_UP:
				sel = (sel > 0 ? sel - 1 : 30);
				if(sel == 14 || sel == 18 || sel == 22 || sel == 25 || sel == 28) { // skip the blank lines
					sel--;
				}
				break;
			case PSP_CTRL_DOWN:
				sel = (sel < 30 ? sel + 1 : 0);
				if(sel == 14 || sel == 18 || sel == 22 || sel == 25 || sel == 28) { // skip the blank lines
					sel++;
				}
				break;
			case PSP_CTRL_LEFT:
			case PSP_CTRL_RIGHT:
				add = (ctrl & PSP_CTRL_LEFT ? -1 : 1);
				switch(sel) {
					case 1: // dump number
						menu.options.dump_no = ((menu.options.dump_no > 0 || add > 0) ? menu.options.dump_no + add : 0);
						break;
					case 4: // cheat speed
						cfg.cheat_hz = ((cfg.cheat_hz > 0 || add > 0) ? cfg.cheat_hz + (add * 5000) : 0);
						break;
					case 7: // color file
						cfg.color_file = ((cfg.color_file > 0 || add > 0) ? cfg.color_file + add : 0);
						break;
					case 8: // text viewer lines
						cfg.max_text_rows = ((cfg.max_text_rows > 0 || add > 0) ? cfg.max_text_rows + (add * 100) : 0);
						break;
					case 19:
						cfg.max_cheats = ((cfg.max_cheats > 8 || add > 0) ? cfg.max_cheats + (add * 8) : 8);
						break;
					case 20:
						cfg.max_blocks = ((cfg.max_blocks > 8 || add > 0) ? cfg.max_blocks + (add * 8) : 8);
						break;
					//case 23: // cheat file
					case 24:
						cfg.cheat_file = ((cfg.cheat_file > -1 || add > 0) ? cfg.cheat_file + add : 0);
						break;
				}
				break;
			case PSP_CTRL_CROSS:
				switch(sel) {
					case 0:
						if(!menu.options.force_pause) {
							cfg.cheat_pause = !cfg.cheat_pause;
							if(cfg.cheat_pause) {
								gamePause(thid);
							} else {
								gameResume(thid);
							}
						}
						break;
					case 1:
						line_clear(33);
						pspDebugScreenSetTextColor(colors.color01);
						printf("%s... ", lang.misc.working);
						sprintf(buffer, "dump%i.ram", menu.options.dump_no);
						if(dump_memory(buffer, (void*)0x08800000, (24 * 1024 * 1024))) {
							menu.options.dump_no++;
							puts(lang.misc.done);
						} else {
							show_error(lang.errors.storage_full);
						}
						sceKernelDelayThread(1 * 1000 * 1000);
						break;
					case 2:
						// http://wololo.net/talk/viewtopic.php?f=5&t=475
						dump_memory("kmem.bin", (void*)0x88000000, (4 * 1024 * 1024));
						dump_memory("internal_ram.bin", (void*)0xBFC00000, (1 * 1024 * 1024));
						break;
					case 3:
						cfg.address_format = (cfg.address_format == cfg.address_start ? 0x00000000 : cfg.address_start);
						break;
					case 5:
						cfg.hijack_pspar_buttons = !cfg.hijack_pspar_buttons;
						break;
					case 6:
						cfg.hbid_pbp_force = !cfg.hbid_pbp_force;
						gameid_get(1);
						break;
					case 7:
						sprintf(buffer, "colors/color%i.txt", cfg.color_file);
						color_load(buffer);
						break;
					case 9:
						cfg.swap_xo = !cfg.swap_xo;
						break;
					case 10:
						#ifdef _USB_
						usb_connect(2);
						#endif
						break;
					case 11:
						#ifdef _PSID_
						if(sceKernelInitKeyConfig() == PSP_INIT_KEYCONFIG_GAME) {
							psid_corrupt();
							line_clear(33);
							pspDebugScreenSetTextColor(colors.color01);
							printf("%s... %s", lang.misc.working, lang.misc.done);
							sceKernelDelayThread(1 * 1000 * 1000);
						}
						#endif
						break;
					case 12:
						line_clear(33);
						pspDebugScreenSetTextColor(colors.color01);
						printf("%s... ", lang.misc.working);

						sprintf(buffer, "patch/%s.pat", gameid_get(0));
						patch_apply(buffer);

						puts(lang.misc.done);
						break;
					case 13:
						cfg.language_file++;
						language_load();
						break;
					case 15:
						line_clear(33);
						pspDebugScreenSetTextColor(colors.color01);
						puts(lang.options.press_buttons);
						cfg.menu_key = get_menu_ctrl(200000, 0);
						sceCtrlRegisterButtonCallback(3, cfg.trigger_key | cfg.menu_key | cfg.screen_key, button_callback, NULL);
						break;
					case 16:
						line_clear(33);
						pspDebugScreenSetTextColor(colors.color01);
						puts(lang.options.press_buttons);
						cfg.trigger_key = get_menu_ctrl(200000, 0);
						sceCtrlRegisterButtonCallback(3, cfg.trigger_key | cfg.menu_key | cfg.screen_key, button_callback, NULL);
						break;
					case 17:
						#ifdef _SCREENSHOT_
						line_clear(33);
						pspDebugScreenSetTextColor(colors.color01);
						puts(lang.options.press_buttons);
						cfg.screen_key = get_menu_ctrl(200000, 1);
						sceCtrlRegisterButtonCallback(3, cfg.trigger_key | cfg.menu_key | cfg.screen_key, button_callback, NULL);
						#endif
						break;
					case 21:
						#ifdef _AUTOOFF_
						cfg.auto_off = !cfg.auto_off;
						#endif
						break;
					case 23:
						line_clear(33);
						pspDebugScreenSetTextColor(colors.color01);
						printf("%s... ", lang.misc.working);
						cheat_save(gameid_get(0));
						puts(lang.misc.done);
						break;
					case 24:
						line_clear(33);
						pspDebugScreenSetTextColor(colors.color01);
						printf("%s... ", lang.misc.working);
						cheat_deinit();
						cheat_init(cfg.max_cheats, cfg.max_blocks, cfg.auto_off);
						cheat_load(NULL, cfg.cheat_file, menu.options.cheat_index);
						menu.cheater.selected = NULL;
						puts(lang.misc.done);
						break;
					case 26:
						sceIoRemove(sceKernelInitKeyConfig() == PSP_INIT_KEYCONFIG_POPS ? "config_pops.bin" : "config.bin");
						config_load(sceKernelInitKeyConfig() == PSP_INIT_KEYCONFIG_POPS ? "config_pops.bin" : "config.bin");
						break;
					case 27:
						config_save(sceKernelInitKeyConfig() == PSP_INIT_KEYCONFIG_POPS ? "config_pops.bin" : "config.bin");
						break;
					case 29:
						#if defined(_MODLIST_) || defined(_THLIST_) || defined(_UMDDUMP_)
						layout_systools();
						layout_heading();
						#endif
						break;
					case 30:
						module_stop(0, NULL);
						break;
				}
				break;
			case PSP_CTRL_SQUARE:
				switch(sel) {
					case 24:
						if(menu.options.cheat_index) {
							menu.options.cheat_index--;
						}
						break;
				}
				break;
			case PSP_CTRL_TRIANGLE:
				switch(sel) {
					case 24:
						menu.options.cheat_index++;
						break;
				}
				break;
			case PSP_CTRL_LTRIGGER:
			case PSP_CTRL_RTRIGGER:
			case PSP_CTRL_CIRCLE:
				ret = ctrl;
				break;
		}
	}

	return ret;
}

u32 layout_browser() {
	u32 ctrl, ret = 0;
	int repeat_time = 0, repeat_interval = 0;
	int i, j;

	char x = 0; // edit mode
	u32 _address, _value; // temporary disassembler/browser variables

	while(ret == 0) {
		pspDebugScreenSetXY(0, 3);

		pspDebugScreenSetTextColor(colors.color02);
				
		if(menu.bd.view) {
			// make sure browser lines > 0
			if(menu.bd.browser_lines == 0) {
				menu.bd.browser_lines = 16;
			}

			// make sure browser address isn't out of boundaries
			if(menu.bd.browser[menu.bd.no].address < (int)cfg.address_start) {
				menu.bd.browser[menu.bd.no].address = cfg.address_start;
			}
			if(menu.bd.browser[menu.bd.no].address > ((cfg.address_end + 1) - (26 * menu.bd.browser_lines))) {
				menu.bd.browser[menu.bd.no].address = ((cfg.address_end + 1) - (26 * menu.bd.browser_lines));
			}

			printf("  %-11s ", lang.misc.address);
			for(i = 0; i < menu.bd.browser_lines; i++) {
				printf("%02X", i);
			}
			printf("  %s", lang.misc.ascii);
		} else {
			// make sure disassembler address isn't out of boundaries
			if(menu.bd.decoder[menu.bd.no].address < (int)cfg.address_start) {
				menu.bd.decoder[menu.bd.no].address = cfg.address_start;
			}
			if(menu.bd.decoder[menu.bd.no].address > ((cfg.address_end + 1) - (26 * sizeof(int)))) {
				menu.bd.decoder[menu.bd.no].address = ((cfg.address_end + 1) - (26 * sizeof(int)));
			}

			if(menu.bd.decoder_format) {
				printf("  %-11s %-11s %-11s %-6s %s", lang.misc.address, lang.misc.hex, lang.misc.dec, lang.misc.ascii, lang.misc._float);
			} else {
				printf("  %-11s %-11s %-9s %s", lang.misc.address, lang.misc.hex, lang.misc.opcode, lang.misc.args);
			}
		}

		line_clear(-1);

		// write out the ram
		for(i = 0; i < 26; i++) {
			if(menu.bd.view) {
				// apply the row color
				pspDebugScreenSetTextColor(i == menu.bd.browser[menu.bd.no].Y ? colors.color01 : colors.color02 - (i * colors.color02_to));
			
				// print out the address
				printf("  0x%08X  ", address(menu.bd.browser[menu.bd.no].address + (i * menu.bd.browser_lines)));
			  
				// print out the hex
				for(j = 0; j < menu.bd.browser_lines; j++) {
					// apply the column color
					if(menu.bd.browser[menu.bd.no].Y == i) {
						pspDebugScreenSetTextColor(colors.color01);
					} else {
						pspDebugScreenSetTextColor((j & 1 ? colors.color01 - (i * colors.color01_to) : colors.color02 - (i * colors.color02_to)));
					}

					printf("%02X", MEM_VALUE(menu.bd.browser[menu.bd.no].address + (i * menu.bd.browser_lines) + j));
					buffer[j] = MEM_ASCII(MEM_VALUE(menu.bd.browser[menu.bd.no].address + (i * menu.bd.browser_lines) + j));
					buffer[j + 1] = 0;
				}
			  
				// apply the row color
				pspDebugScreenSetTextColor((i == menu.bd.browser[menu.bd.no].Y ? colors.color01 : colors.color02 - (i * colors.color02_to)));
			  
				// print out the ascii
				puts("  ");
				puts(buffer);

				// goto the next line
				line_clear(-1);
			  
				// draw the cursor
				if(i == menu.bd.browser[menu.bd.no].Y) {
					j = 4 + menu.bd.browser[menu.bd.no].X;
					switch(menu.bd.browser[menu.bd.no].C) {
						case 2:
							j += (menu.bd.browser_lines * 2) + 2;
						case 1:
							j += 10;
							break;
					}

					line_cursor(j, x ? colors.color01 : colors.color06);
				}
			} else {
				if(i == menu.bd.decoder[menu.bd.no].Y) {
					pspDebugScreenSetTextColor(colors.color01);
				} else {
					if((menu.bd.decoder[menu.bd.no].address + (i * 4)) == menu.copier.address_start) {
						pspDebugScreenSetTextColor(colors.color01);
					} else if((menu.bd.decoder[menu.bd.no].address + (i * 4)) == menu.copier.address_end) {
						pspDebugScreenSetTextColor(colors.color06);
					} else {
						pspDebugScreenSetTextColor(colors.color02 - (i * colors.color02_to));
					}
				}

				// print out the address and hex value
				printf("  0x%08X  0x%08X  ", address(menu.bd.decoder[menu.bd.no].address + (i * 4)), MEM_VALUE_INT(menu.bd.decoder[menu.bd.no].address + (i * 4)));

				if(menu.bd.decoder_format) {
					// print out the decimal
					printf("%010u  ", MEM_VALUE_INT(menu.bd.decoder[menu.bd.no].address + (i * 4)));

					// print out the ascii
					buffer[0] = MEM_ASCII(MEM_VALUE(menu.bd.decoder[menu.bd.no].address + (i * 4) + 0));
					buffer[1] = MEM_ASCII(MEM_VALUE(menu.bd.decoder[menu.bd.no].address + (i * 4) + 1));
					buffer[2] = MEM_ASCII(MEM_VALUE(menu.bd.decoder[menu.bd.no].address + (i * 4) + 2));
					buffer[3] = MEM_ASCII(MEM_VALUE(menu.bd.decoder[menu.bd.no].address + (i * 4) + 3));
					buffer[4] = 0;
					puts(buffer);
					puts("   ");

					// print out the float
					f_cvt(menu.bd.decoder[menu.bd.no].address + (i * 4), buffer, sizeof(buffer), 6, MODE_GENERIC);
					puts(buffer);
				} else {
					// print out the opcode
					mipsDecode(MEM_VALUE_INT(menu.bd.decoder[menu.bd.no].address + (i * 4)), (menu.bd.decoder[menu.bd.no].address + (i * 4)));
				}

				// goto the next line
				line_clear(-1);

				// draw the cursor
				if(i == menu.bd.decoder[menu.bd.no].Y){
					j = 4 + menu.bd.decoder[menu.bd.no].X;
					switch(menu.bd.decoder[menu.bd.no].C) {
						case 1:
							j += 12;
							break;
					}

					line_cursor(j, x ? colors.color01 : colors.color06);
				}
			}
		}

		// helper
		line_print(31);
		pspDebugScreenSetTextColor(colors.color01);
		puts(lang.browser_disassembler.help);

		ctrl = ctrl_waitmask(repeat_time, repeat_interval,
			PSP_CTRL_UP | PSP_CTRL_DOWN | PSP_CTRL_LEFT | PSP_CTRL_RIGHT |
			PSP_CTRL_ANALOG_UP | PSP_CTRL_ANALOG_DOWN | PSP_CTRL_ANALOG_LEFT | PSP_CTRL_ANALOG_RIGHT |
			PSP_CTRL_TRIANGLE | PSP_CTRL_CROSS | PSP_CTRL_CIRCLE |
			PSP_CTRL_VOLUP | PSP_CTRL_VOLDOWN | PSP_CTRL_NOTE |
			PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_START | PSP_CTRL_SELECT);


		repeat_time = CTRL_REPEAT_TIME;
		repeat_interval = CTRL_REPEAT_INTERVAL;

		switch(ctrl & (PSP_CTRL_START | PSP_CTRL_VOLDOWN | PSP_CTRL_VOLUP | PSP_CTRL_CROSS)) {
			case PSP_CTRL_START:
				menu.bd.view = !menu.bd.view;
				layout_heading();
				break;
			case PSP_CTRL_VOLDOWN:
				menu.bd.no = (menu.bd.no > 0 ? menu.bd.no - 1 : 9);
				layout_heading();
				break;
			case PSP_CTRL_VOLUP:
				menu.bd.no = (menu.bd.no < 9 ? menu.bd.no + 1 : 0);
				layout_heading();
				break;
			case PSP_CTRL_CROSS:
				x = !x;
				break;
		}

		if(menu.bd.view) { // menu.bd.browser
			// dpad
			switch(ctrl & (PSP_CTRL_UP | PSP_CTRL_DOWN | PSP_CTRL_LEFT | PSP_CTRL_RIGHT)) {
				case PSP_CTRL_UP:
					if(x) { // edit mode
						switch(menu.bd.browser[menu.bd.no].C) {
							case 0: // address
								menu.bd.browser[menu.bd.no].address += (1 << (4 * (7 - menu.bd.browser[menu.bd.no].X)));
								break;
							case 1: // value hex
								MEM_VALUE((menu.bd.browser[menu.bd.no].address + (menu.bd.browser[menu.bd.no].Y * menu.bd.browser_lines)) + (menu.bd.browser[menu.bd.no].X / 2)) += (menu.bd.browser[menu.bd.no].X & 1 ? 0x01 : 0x10);
								break;
							case 2: // value ascii
								MEM_VALUE((menu.bd.browser[menu.bd.no].address + (menu.bd.browser[menu.bd.no].Y * menu.bd.browser_lines)) + menu.bd.browser[menu.bd.no].X) += 1;
								break;
						}
					} else { // view mode
						if(ctrl & PSP_CTRL_SQUARE) {
							menu.bd.browser[menu.bd.no].address -= menu.bd.browser_lines;
						} else {
							if(menu.bd.browser[menu.bd.no].Y > 0) {
								menu.bd.browser[menu.bd.no].Y--;
							} else {
								menu.bd.browser[menu.bd.no].address -= menu.bd.browser_lines;
							}
						}
					}
					break;
				case PSP_CTRL_DOWN:
					if(x) { // edit mode
						switch(menu.bd.browser[menu.bd.no].C) {
							case 0: // address
								menu.bd.browser[menu.bd.no].address -= (1 << (4 * (7 - menu.bd.browser[menu.bd.no].X)));
								break;
							case 1: // value hex
								MEM_VALUE((menu.bd.browser[menu.bd.no].address + (menu.bd.browser[menu.bd.no].Y * menu.bd.browser_lines)) + (menu.bd.browser[menu.bd.no].X / 2)) -= (menu.bd.browser[menu.bd.no].X & 1 ? 0x01 : 0x10);
								break;
							case 2: // value ascii
								MEM_VALUE((menu.bd.browser[menu.bd.no].address + (menu.bd.browser[menu.bd.no].Y * menu.bd.browser_lines)) + menu.bd.browser[menu.bd.no].X) -= 1;
								break;
						}
					} else { // view mode
						if(ctrl & PSP_CTRL_SQUARE) {
							menu.bd.browser[menu.bd.no].address += menu.bd.browser_lines;
						} else {
							if(menu.bd.browser[menu.bd.no].Y < 25) {
								menu.bd.browser[menu.bd.no].Y++;
							} else {
								menu.bd.browser[menu.bd.no].address += menu.bd.browser_lines;
							}
						}
					}
					break;
				case PSP_CTRL_LEFT:
					menu.bd.browser[menu.bd.no].X--;
					switch(menu.bd.browser[menu.bd.no].C) {
						case 0:
							if((int)menu.bd.browser[menu.bd.no].X == -1){
								menu.bd.browser[menu.bd.no].X = 0;
							}
							break;
						case 1:
							if((int)menu.bd.browser[menu.bd.no].X == -1){
								menu.bd.browser[menu.bd.no].X = 7;
								menu.bd.browser[menu.bd.no].C--;
							}
							break;
						case 2:
							if((int)menu.bd.browser[menu.bd.no].X == -1){
								menu.bd.browser[menu.bd.no].X = (2 * menu.bd.browser_lines) - 1;
								menu.bd.browser[menu.bd.no].C--;
							}
							break;
					}
					break;
				case PSP_CTRL_RIGHT:
					menu.bd.browser[menu.bd.no].X++;
					switch(menu.bd.browser[menu.bd.no].C) {
						case 0:
							if((int)menu.bd.browser[menu.bd.no].X > 7) {
								menu.bd.browser[menu.bd.no].X = 0;
								menu.bd.browser[menu.bd.no].C++;
							}
							break;
						case 1:
							if((int)menu.bd.browser[menu.bd.no].X > ((2 * menu.bd.browser_lines) - 1)) {
								menu.bd.browser[menu.bd.no].X = 0;
								menu.bd.browser[menu.bd.no].C++;
							}
							break;
						case 2:
							if((int)menu.bd.browser[menu.bd.no].X > (menu.bd.browser_lines - 1)) {
								menu.bd.browser[menu.bd.no].X = menu.bd.browser_lines - 1;
							}
							break;
					}
					break;
			}

			// face butons
			switch(ctrl & (PSP_CTRL_TRIANGLE | PSP_CTRL_SQUARE)) {
				case PSP_CTRL_TRIANGLE:
					_address = address(menu.bd.browser[menu.bd.no].address + (menu.bd.browser[menu.bd.no].Y * menu.bd.browser_lines));
					_value = MEM_VALUE(menu.bd.browser[menu.bd.no].address + (menu.bd.browser[menu.bd.no].Y * menu.bd.browser_lines));
					layout_copymenu(&_address, &_value, COPY_PASTE_ALL);
					MEM_VALUE(menu.bd.browser[menu.bd.no].address + (menu.bd.browser[menu.bd.no].Y * menu.bd.browser_lines)) = _value;
					if(_address != address(menu.bd.browser[menu.bd.no].address + (menu.bd.browser[menu.bd.no].Y * menu.bd.browser_lines))) {
						menu.bd.browser[menu.bd.no].address = real_address(_address & 0x0FFFFFFC) - (menu.bd.browser[menu.bd.no].Y * 4);
					}
					break;
				case PSP_CTRL_SQUARE:
					if(ctrl & PSP_CTRL_ANALOG_UP) {
						repeat_time = CTRL_REPEAT_TIME_QUICK;
						repeat_interval = CTRL_REPEAT_INTERVAL_QUICK;
						menu.bd.browser[menu.bd.no].address -= menu.bd.browser_lines * 25;
					} else if(ctrl & PSP_CTRL_ANALOG_DOWN) {
						repeat_time = CTRL_REPEAT_TIME_QUICK;
						repeat_interval = CTRL_REPEAT_INTERVAL_QUICK;
						menu.bd.browser[menu.bd.no].address += menu.bd.browser_lines * 25;
					} else if(x) { // edit mode
						if(menu.bd.browser[menu.bd.no].C == 1) {
							menu.bd.browser[menu.bd.no].C = 2;
							menu.bd.browser[menu.bd.no].X /= 2;
						} else if(menu.bd.browser[menu.bd.no].C == 2) {
							menu.bd.browser[menu.bd.no].C = 1;
							menu.bd.browser[menu.bd.no].X *= 2;
						}
					}
					break;
			}

			// misc buttons
			switch(ctrl & (PSP_CTRL_SELECT)) {
				case PSP_CTRL_SELECT:
					menu.bd.browser_lines = (menu.bd.browser_lines == 8 ? 16 : 8);
					menu.bd.browser[menu.bd.no].X = 0;
					break;
			}
		} else { // disassembler
			// dpad
			switch(ctrl & (PSP_CTRL_UP | PSP_CTRL_DOWN | PSP_CTRL_LEFT | PSP_CTRL_RIGHT)) {
				case PSP_CTRL_UP:
					if(x) { // edit mode
						switch(menu.bd.decoder[menu.bd.no].C) {
							case 0:
								menu.bd.decoder[menu.bd.no].address += (menu.bd.decoder[menu.bd.no].X == 7 ? 4 : (1 << (4 * (7 - menu.bd.decoder[menu.bd.no].X))));
								break;
							case 1:
								MEM_VALUE_INT(menu.bd.decoder[menu.bd.no].address + (menu.bd.decoder[menu.bd.no].Y * 4)) += (1 << (4 * (7 - menu.bd.decoder[menu.bd.no].X)));
								break;
						}
					} else if(ctrl & PSP_CTRL_SQUARE) {
						menu.bd.decoder[menu.bd.no].address -= 4;
					} else { // view mode
						if(menu.bd.decoder[menu.bd.no].Y > 0) {
							menu.bd.decoder[menu.bd.no].Y--;
						} else if(menu.bd.decoder[menu.bd.no].Y == 0) {
							menu.bd.decoder[menu.bd.no].address -= 4;
						}
					}
					break;
				case PSP_CTRL_DOWN:
					if(x) { // edit mode
						switch(menu.bd.decoder[menu.bd.no].C) {
							case 0:
								menu.bd.decoder[menu.bd.no].address -= (menu.bd.decoder[menu.bd.no].X == 7 ? 4 : (1 << (4 * (7 - menu.bd.decoder[menu.bd.no].X))));
								break;
							case 1:
								MEM_VALUE_INT(menu.bd.decoder[menu.bd.no].address + (menu.bd.decoder[menu.bd.no].Y * 4)) -= (1 << (4 * (7 - menu.bd.decoder[menu.bd.no].X)));
								break;
						}
					} else if(ctrl & PSP_CTRL_SQUARE) {
						menu.bd.decoder[menu.bd.no].address += 4;
					} else { // view mode
						if(menu.bd.decoder[menu.bd.no].Y < 25) {
							menu.bd.decoder[menu.bd.no].Y++;
						} else if(menu.bd.decoder[menu.bd.no].Y == 25) {
							menu.bd.decoder[menu.bd.no].address += 4;
						}
					}
					break;
				case PSP_CTRL_LEFT:
					if(ctrl & PSP_CTRL_SQUARE) { // return to pointer
						if(menu.bd.decoder_jump_address) {
							menu.bd.decoder[menu.bd.no].address = menu.bd.decoder_jump_address - (menu.bd.decoder[menu.bd.no].Y * 4);;
						}
					} else {
						menu.bd.decoder[menu.bd.no].X--;
						switch(menu.bd.decoder[menu.bd.no].C) {
							case 0:
								if((int)menu.bd.decoder[menu.bd.no].X == -1) {
									menu.bd.decoder[menu.bd.no].X=0;
								}
								break;
							case 1:
								if((int)menu.bd.decoder[menu.bd.no].X == -1) {
									menu.bd.decoder[menu.bd.no].X = 7;
									menu.bd.decoder[menu.bd.no].C--;
								}
								break;
							case 2:
								if((int)menu.bd.decoder[menu.bd.no].X == -1) {
									menu.bd.decoder[menu.bd.no].X = 7;
									menu.bd.decoder[menu.bd.no].C--;
								}
								break;
						}
					}
					break;
				case PSP_CTRL_RIGHT:
					if(ctrl & PSP_CTRL_SQUARE) { // jump to pointer
						u32 foobar = MEM_VALUE_INT(menu.bd.decoder[menu.bd.no].address + (menu.bd.decoder[menu.bd.no].Y * 4));
						u32 new_address = 0;

						if((foobar >= cfg.address_start) && (foobar <= cfg.address_end)) { // handle pointers
							new_address = foobar;
						} else {
							if((foobar & 0xFC000000) == 0x08000000 || (foobar & 0xFC000000) == 0x0C000000) {
								new_address = (foobar & 0x3FFFFFF) << 2;
							} else if((foobar & 0xF0000000) == 0x10000000 || (foobar & 0xF0000000) == 0x50000000 || (foobar & 0xFC000000) == 0x04000000 || (foobar & 0xFF000000) == 0x45000000) {
								new_address = (menu.bd.decoder[menu.bd.no].address + (menu.bd.decoder[menu.bd.no].Y * 4)) + (((short)(foobar & 0xFFFF) + 1) * 4);
							}
						}

						new_address &= 0xFFFFFFFC;

						if(new_address != 0) {
							menu.bd.decoder_jump_address = (menu.bd.decoder[menu.bd.no].address + (menu.bd.decoder[menu.bd.no].Y * 4));
							menu.bd.decoder[menu.bd.no].address = new_address - (menu.bd.decoder[menu.bd.no].Y * 4);
						}
					} else {
						menu.bd.decoder[menu.bd.no].X++;
						switch(menu.bd.decoder[menu.bd.no].C) {
							case 0:
								if(menu.bd.decoder[menu.bd.no].X > 7) {
									menu.bd.decoder[menu.bd.no].X = 0;
									menu.bd.decoder[menu.bd.no].C++;
								}
								break;
							case 1:
								if(menu.bd.decoder[menu.bd.no].X > 7) {
									menu.bd.decoder[menu.bd.no].X = 7;
								}
								break;
							case 2:
								if(menu.bd.decoder[menu.bd.no].X > 7) {
									menu.bd.decoder[menu.bd.no].X = 7;
								}
								break;
						}
					}
					break;
			}

			// face buttons
			switch(ctrl & (PSP_CTRL_TRIANGLE | PSP_CTRL_SQUARE)) {
				case PSP_CTRL_TRIANGLE:
					_address = address(menu.bd.decoder[menu.bd.no].address + (menu.bd.decoder[menu.bd.no].Y * 4));
					_value = MEM_VALUE_INT(menu.bd.decoder[menu.bd.no].address + (menu.bd.decoder[menu.bd.no].Y * 4));
					layout_copymenu(&_address, &_value, COPY_PASTE_ALL);
					MEM_VALUE_INT(menu.bd.decoder[menu.bd.no].address + (menu.bd.decoder[menu.bd.no].Y * 4)) = _value;
					if(_address != address(menu.bd.decoder[menu.bd.no].address + (menu.bd.decoder[menu.bd.no].Y * 4))) {
						menu.bd.decoder[menu.bd.no].address = real_address(_address & 0x0FFFFFFC) - (menu.bd.decoder[menu.bd.no].Y * 4);
					}
					break;
				case PSP_CTRL_SQUARE:
					if(ctrl & PSP_CTRL_ANALOG_UP) {
						repeat_time = CTRL_REPEAT_TIME_QUICK;
						repeat_interval = CTRL_REPEAT_INTERVAL_QUICK;
						menu.bd.decoder[menu.bd.no].address -= 500;
					} else if(ctrl & PSP_CTRL_ANALOG_DOWN) {
						repeat_time = CTRL_REPEAT_TIME_QUICK;
						repeat_interval = CTRL_REPEAT_INTERVAL_QUICK;
						menu.bd.decoder[menu.bd.no].address += 500;
					}
					break;
			}

			// misc buttons
			switch(ctrl & (PSP_CTRL_SELECT | PSP_CTRL_NOTE)) {
				case PSP_CTRL_SELECT:
					menu.bd.decoder_format = !menu.bd.decoder_format;
					break;
				case PSP_CTRL_NOTE:
					if(!menu.copier.toggle) {
						menu.copier.address_start = menu.bd.decoder[menu.bd.no].address + (menu.bd.decoder[menu.bd.no].Y * 4);
						menu.copier.address_end = menu.bd.decoder[menu.bd.no].address + (menu.bd.decoder[menu.bd.no].Y * 4);
						menu.copier.toggle = 1;
					} else if((menu.copier.toggle) && (menu.bd.decoder[menu.bd.no].address + (menu.bd.decoder[menu.bd.no].Y * 4) > menu.copier.address_start)) {
						menu.copier.address_end = menu.bd.decoder[menu.bd.no].address + (menu.bd.decoder[menu.bd.no].Y * 4);
						menu.copier.toggle = 0;
					} else if((menu.copier.toggle) && (menu.bd.decoder[menu.bd.no].address + (menu.bd.decoder[menu.bd.no].Y * 4) < menu.copier.address_start)) {
						menu.copier.address_start = menu.bd.decoder[menu.bd.no].address + (menu.bd.decoder[menu.bd.no].Y * 4);
						menu.copier.toggle = 1;
					}
					break;
			}
		}

		// return buttons
		if(ctrl & (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_CIRCLE)) {
			ret = ctrl;
		}
	}

	return ret;
}

u32 layout_credits() {
	u32 ctrl, ret = 0;

	while(ret == 0) {
		#ifdef _GUIDE_
		if(strlen(menu.guide_path) > 0 && text_open(menu.guide_path, 60 + 2, cfg.max_text_rows) >= 0) {
			ctrl = layout_guide();
		} else {
			ctrl = layout_credits2();
		}
		#else
		ctrl = layout_credits2();
		#endif

		#ifdef _GUIDE_
		if(ctrl & PSP_CTRL_START) {
			char *exts[] = {".txt"};
			filebrowser_display(menu.guide_path, exts,  1);
			layout_heading();
		}
		#endif
	
		if(ctrl & (PSP_CTRL_START | PSP_CTRL_CIRCLE | PSP_CTRL_LTRIGGER)) {
			ret = ctrl;
		}
	}
}

u32 layout_credits2() {
	u32 ctrl, ret = 0;

	while(ret == 0) {
		pspDebugScreenSetXY(0, 8);

		pspDebugScreenSetTextColor(colors.color02);
		puts("                      Modifications by "); pspDebugScreenSetTextColor(colors.color07); puts("raing3"); puts("\n\n\n");

		pspDebugScreenSetTextColor(colors.color02);
		puts("           Based on MKULTRA by "); pspDebugScreenSetTextColor(colors.color01 - (colors.color01_to * 1)); puts("RedHate");

		pspDebugScreenSetTextColor(colors.color02);
		puts(" and NitePR by "); pspDebugScreenSetTextColor(colors.color01 - (colors.color01_to * 2)); puts("SaNiK"); puts("\n\n\n");

		pspDebugScreenSetTextColor(colors.color02);
		puts("           Thanks to "); pspDebugScreenSetTextColor(colors.color01 - (colors.color01_to * 3)); puts("NoEffex");
		pspDebugScreenSetTextColor(colors.color02);
		puts(" for the corrupt PSID function\n\n\n");

		pspDebugScreenSetTextColor(colors.color02);
		puts("                Thanks to "); pspDebugScreenSetTextColor(colors.color01 - (colors.color01_to * 4)); puts("Haro");
		pspDebugScreenSetTextColor(colors.color02);
		puts(" for all the bug reports\n\n\n");

		pspDebugScreenSetTextColor(colors.color02);
		puts("      Thanks to "); pspDebugScreenSetTextColor(colors.color01 - (colors.color01_to * 5)); puts("Datel");

		pspDebugScreenSetTextColor(colors.color02);
		puts(" and "); pspDebugScreenSetTextColor(colors.color01 - (colors.color01_to * 6)); puts("weltall");

		pspDebugScreenSetTextColor(colors.color02);
		puts(" for implementing such great\n                  code types in their cheat devices.\n\n\n\n");

		pspDebugScreenSetTextColor(colors.color06);
		puts("                       http://raing3.gshi.org/");

		line_print(32);
		pspDebugScreenSetTextColor(colors.color01);

		#ifdef _LITE_
		puts(VER_STR" (Lite)");
		#else
		puts(VER_STR);
		#endif
		pspDebugScreenSetXY(33, pspDebugScreenGetY());
		puts("Build Date: "__DATE__" ("__TIME__")");

		ctrl_waitrelease();
		ctrl = ctrl_waitmask(0, 0, PSP_CTRL_START | PSP_CTRL_CIRCLE | PSP_CTRL_LTRIGGER);
		ret = ctrl;
	}

	return ret;
}

#ifdef _GUIDE_
u32 layout_guide() {
	#define ROW_DISPLAY 29
	#define READ_ROW 60

	u32 ctrl, ret = 0;
	int repeat_time = 0, repeat_interval = 0;
	int sel = guiderow;
	int i, j;
	int rp = 1;
	p_textrow row;

	while(ret == 0) {
		if(rp) {
			if(guiderow > text_rows()) {
				sel = guiderow = 0;
			}

			pspDebugScreenSetXY(0, 3);

			for(i = 0; i < ROW_DISPLAY; i ++) {
				if(guiderow + i < text_rows()) {
					row = text_read(guiderow + i);

					if(guiderow + i == sel) {
						pspDebugScreenSetTextColor(colors.color01);
						puts(" > ");
					} else {
						puts("   ");
						pspDebugScreenSetTextColor(colors.color02);
					}

					for(j = 0; j < row->count; j++) {
						buffer[j] = (row->start[j] < (u8)0x20 ? 0x20 : row->start[j]);
					}
					buffer[j] = 0;
					puts(buffer);
				}

				line_clear(-1);
			}

			line_print(32);
			pspDebugScreenSetTextColor(colors.color01);
			printf("Line: %d/%d", sel + 1, text_rows());
			line_clear(-1);

			rp = 0;
		}

		ctrl = ctrl_waitmask(repeat_time, repeat_interval, PSP_CTRL_UP | PSP_CTRL_DOWN | PSP_CTRL_LEFT | PSP_CTRL_RIGHT |
			PSP_CTRL_ANALOG_UP | PSP_CTRL_ANALOG_DOWN | PSP_CTRL_ANALOG_LEFT | PSP_CTRL_ANALOG_RIGHT | PSP_CTRL_START |
			PSP_CTRL_SQUARE | PSP_CTRL_TRIANGLE | PSP_CTRL_LTRIGGER | PSP_CTRL_CIRCLE);

		repeat_time = CTRL_REPEAT_TIME;
		repeat_interval = CTRL_REPEAT_INTERVAL;

		// scroll speedup for analog
		if(ctrl & (PSP_CTRL_ANALOG_UP | PSP_CTRL_ANALOG_DOWN | PSP_CTRL_ANALOG_LEFT | PSP_CTRL_ANALOG_RIGHT)) {
			repeat_time = CTRL_REPEAT_TIME_QUICK;
			repeat_interval = CTRL_REPEAT_INTERVAL_QUICK;
		}

		// guide scroll buttons
		if(ctrl & (PSP_CTRL_UP | PSP_CTRL_ANALOG_UP)) {
			if(sel > 0) {
				sel--;
				rp = 1;
			}
			if(guiderow > 0) {
				if(sel <= guiderow + (ROW_DISPLAY / 2)) {
					guiderow--;
					rp = 1;
				}
			}
		} else if(ctrl & (PSP_CTRL_DOWN | PSP_CTRL_ANALOG_DOWN)) {
			if(sel < text_rows() - 1) {
				sel++;
				rp = 1;
			}
			if(guiderow + ROW_DISPLAY < text_rows()) {
				if(sel >= guiderow + (ROW_DISPLAY / 2)) {
					guiderow++;
					rp = 1;
				}
			}
		} else if(ctrl & (PSP_CTRL_LEFT | PSP_CTRL_ANALOG_LEFT)) {
			if(sel > 0) {
				sel -= 14;
				guiderow -= 14;
				if(guiderow < 0) {
					sel = guiderow = 0;
				}
				rp = 1;
			}
			//read_text_back(&guiderow, &rp, 14);
		} else if(ctrl & (PSP_CTRL_RIGHT | PSP_CTRL_ANALOG_RIGHT)) {
			if(guiderow + ROW_DISPLAY + 14 < text_rows()) {
				sel += 14;
				guiderow += 14;
				rp = 1;
			} else if(text_rows() > ROW_DISPLAY) {
				sel = text_rows() - 1;
				guiderow = sel - ROW_DISPLAY + 2;
				rp = 1;
			}
		}

		// face button
		if(ctrl & PSP_CTRL_SQUARE) {
			if(sel > 0) {
				sel -= text_rows() >> 3;
				guiderow -= text_rows() >> 3;
				if(guiderow < 0) {
					sel = guiderow = 0;
				}
				rp = 1;
			}
		} else if(ctrl & PSP_CTRL_TRIANGLE) {
			if(guiderow + ROW_DISPLAY + (text_rows() >> 3) < text_rows()) {
				sel += text_rows() >> 3;
				guiderow += text_rows() >> 3;
				rp = 1;
			} else if(text_rows() > ROW_DISPLAY) {
				sel = text_rows() - 1;
				guiderow = sel - ROW_DISPLAY + 2;
				rp = 1;
			}
		}

		// return buttons
		if(ctrl & (PSP_CTRL_START | PSP_CTRL_CIRCLE | PSP_CTRL_LTRIGGER)) {
			ret = ctrl;
		}
	}

	text_close();

	return ret;
}
#endif