#include <stdio.h>
#include <string.h>
#include <pspctrl.h>
#include <pspthreadman.h>
#include "common.h"

extern Config cfg;
extern Colors colors;
extern Language lang;

Search search;

void search_init() {
	// set the search variables
	search_reset(0, 1);

	// load the previous search
	while(1) {
		char buffer[64];
		sprintf(buffer, "searches/search%i.dat", ++search.number);
		SceUID fd = sceIoOpen(buffer, PSP_O_RDONLY, 0777);
		if(fd >= 0) {
			sceIoClose(fd);
		} else {
			search.number--;
			break;
		}
	}

	if(search.number > 0) {
		search.started = 1;
		search_load_results();
	} else {
		SceUID fd = sceIoOpen("searches/search.ram", PSP_O_RDONLY, 0777);
		if(fd >= 0) {
			sceIoClose(fd);
			search.started = 1;
		}
	}
}

void search_start(char search_mode) {
	SceUID fd = -1, fd2 = -1;
	u32 address = 0, value = 0;
	char search_bytes = 0;
	char search_ftype = SEARCH_FILE_NONE;
	char buffer[64];

	search.num_results = 0;
	search.history[0].flags = (search.history[0].flags & ~0xF) | search_mode;

	// open the output file
	sprintf(buffer, "searches/search%i.dat", search.number + 1);
	sceIoRemove(buffer);
	fd = fileIoOpen(buffer, PSP_O_WRONLY | PSP_O_TRUNC | PSP_O_CREAT, 0777);

	// open the input file
	if(search.started) {
		if(search.number == 0) {
			fd2 = fileIoOpen("searches/search.ram", PSP_O_RDONLY, 0777);
			search_ftype = SEARCH_FILE_DUMP;
		} else {
			sprintf(buffer, "searches/search%i.dat", search.number);
			fd2 = fileIoOpen(buffer, PSP_O_RDONLY, 0777);
			search_ftype = SEARCH_FILE_DAT;
		}
	}

	if(fd >= 0 && (search_ftype == SEARCH_FILE_NONE || fd2 >= 0)) {
		if(fileIoWrite(fd, &search.history[0], sizeof(SearchHistoryItem)) != sizeof(SearchHistoryItem)) {
			goto FinishSearchError;
		}

		if(search_ftype == SEARCH_FILE_DAT) {
			fileIoRead(fd2, &search.history[1], sizeof(SearchHistoryItem));
		}

		switch(search.history[0].flags & FLAG_DWORD) {
			case FLAG_DWORD:
				search_bytes = 4;
				break;
			case FLAG_WORD:
				search_bytes = 2;
				search.history[0].value &= 0xFFFF;
				break;
			case FLAG_BYTE:
				search_bytes = 1;
				search.history[0].value &= 0xFF;
				break;
		}

		value = search.history[0].value;

		for(address = search.address_start & 0xFFFFFFFC; address <= search.address_end; address += search_bytes) {
			if(!(address & 0xFFFF)) {
				if(!cfg.cheat_pause) { // delay search so we don't crash
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
					printf(lang.misc.task_progress, (address - search.address_start) / (((search.address_end - search.address_start) / 100) + 1));
					line_clear(-1);
				}
			}

			if(search_ftype != SEARCH_FILE_NONE) {
				// load address from file if it is open
				if(search_ftype == SEARCH_FILE_DAT) {
					if(fileIoRead(fd2, &address, sizeof(address)) != sizeof(address)) {
						break;
					}
				}

				// load/skip value from file if it is open
				if((search.history[1].flags & 0xF) != 6) {
					if(search_mode == 6) { // known search
						fileIoLseek(fd2, search_bytes, SEEK_CUR);
					} else { // unknown search
						fileIoRead(fd2, &value, search_bytes);
					}
				}
			}

			// check
			u32 check;
			switch(search_bytes) {
				case 4:
					check = *(u32*)address;
					break;
				case 2:
					check = *(u16*)address;
					break;
				default:
					check = *(u8*)address;
					break;
			}

			switch(search_mode) {
				case 0:
					if(value != check) continue;
					break;
				case 1:
					if(value == check) continue;
					break;
				case 2:
					if(value >= check) continue;
					break;
				case 3:
					if(value <= check) continue;
					break;
				case 4:
					if(value + search.history[0].value != check) continue;
					break;
				case 5:
					if(value - search.history[0].value != check) continue;
					break;
				case 6:
					if(search.history[0].value != check) continue;
					break;
				case 7:
					if(search.history[0].value == check) continue;
					break;
				case 8:
					if(search.history[0].value >= check) continue;
					break;
				case 9:
					if(search.history[0].value <= check) continue;
					break;
			}

			if(fileIoWrite(fd, &address, sizeof(address)) != sizeof(address)) {
				goto FinishSearchError;
			}
			if(search_mode != 6) {
				fileIoWrite(fd, &check, search_bytes);
			}

			if(search.max_results > 0) {
				if(++search.num_results >= search.max_results) {
					break;
				}
			}
		}
	}

FinishSearch:
	if(search.num_history < SEARCH_HISTORY_MAX - 1) {
		search.num_history++;
	}
	memmove(&search.history[1], &search.history[0], sizeof(search.history) - sizeof(SearchHistoryItem));
	search.started = 1;
	search.number++;
	goto CloseFiles;

FinishSearchError:
	show_error(lang.errors.storage_full);
	goto CloseFiles;

CloseFiles:
	fileIoClose(fd);
	fileIoClose(fd2);
}

void search_undo() {
	if(search.number > 1) {
		char buffer[64];
		sprintf(buffer, "searches/search%i.dat", search.number--);
		sceIoRemove(buffer);
		search_load_results();
	} else {
		search_reset(1, 0);
	}
}

void search_reset(char remove_files, char reset_history) {
	// create the search directory
	sceIoMkdir("searches", 0777);

	// remove search files
	if(remove_files) {
		sceIoRemove("searches/search.ram");

		if(search.number == 0) {
			search.number = 15;
		}

		do {
			char buffer[64];
			sprintf(buffer, "searches/search%i.dat", search.number);
			sceIoRemove(buffer);
		} while(search.number-- > 0);
	}
	
	// reset variables
	search.number = search.num_results = search.started = 0;

	if(reset_history) {
		search.num_history = 0;
		memset(&search.history, 0, sizeof(search.history));
		search.history[0].flags = FLAG_DWORD;
	}

	if(search.address_start == 0 || search.address_end == 0) {
		search.address_start = cfg.address_start;
		search.address_end = cfg.address_end;
	}
}

void search_load_results() {
	// load the search results
	search.num_results = 0;

	if(search.number != 0) {
		char buffer[64];
		sprintf(buffer, "searches/search%i.dat", search.number);

		SceUID fd = fileIoOpen(buffer, PSP_O_RDONLY, 0777);
		if(fd >= 0) {
			SearchHistoryItem search_item;
			if(fileIoRead(fd, &search_item, sizeof(SearchHistoryItem)) == sizeof(SearchHistoryItem)) {
				// parse the file header
				search.history[0].flags = search_item.flags;

				int search_bytes = 0;
				switch(search_item.flags & FLAG_DWORD) {
					case FLAG_DWORD:
						search_bytes = 4;
						break;
					case FLAG_WORD:
						search_bytes = 2;
						break;
					default:
						search_bytes = 1;
						break;
				}

				// get the number of results
				search.num_results = (fileIoLseek(fd, 0, SEEK_END) - sizeof(SearchHistoryItem)) / (sizeof(int) + ((search_item.flags & 0xF) == 6 ? 0 : search_bytes));
				fileIoLseek(fd, sizeof(SearchHistoryItem), SEEK_SET);

				// only load the first X results
				int i;
				for(i = 0; i < (search.num_results > SEARCH_ADDRESSES_MAX ? SEARCH_ADDRESSES_MAX : search.num_results); i++) {
					fileIoRead(fd, &search.results[i], sizeof(int));
					if((search_item.flags & 0xF) != 6) {
						fileIoLseek(fd, search_bytes, SEEK_CUR);
					}
				}
			}
			
			fileIoClose(fd);
		}
	}
}

Cheat *search_add_loaded_results(u8 flags) {
	int results = (search.num_results > SEARCH_ADDRESSES_MAX ? SEARCH_ADDRESSES_MAX : search.num_results);

	Cheat *cheat = cheat_add_folder("Search Results", FOLDER_MULTI_SELECT);

	int i;
	for(i = 0; i < results; i++) {
		switch(search.history[0].flags & FLAG_DWORD) {
			case FLAG_DWORD:
				cheat_new(-1, search.results[i], MEM_VALUE_INT(search.results[i]), 1, flags, sizeof(int));
				break;
			case FLAG_WORD:
				cheat_new(-1, search.results[i], MEM_VALUE_SHORT(search.results[i]), 1, flags, sizeof(short));
				break;
			case FLAG_BYTE:
				cheat_new(-1, search.results[i], MEM_VALUE(search.results[i]), 1, flags, sizeof(char));
				break;
		}
	}

	cheat_unparent(cheat);

	return cheat;
}

Cheat *search_add_result(u32 address, u8 flags) {
	Cheat *cheat;

	switch(search.history[0].flags & FLAG_DWORD) {
		case FLAG_DWORD:
			cheat = cheat_new(-1, address, MEM_VALUE_INT(address), 1, flags, 4);
			break;
		case FLAG_WORD:
			cheat = cheat_new(-1, address, MEM_VALUE_SHORT(address), 1, flags, 2);
			break;
		case FLAG_BYTE:
			cheat = cheat_new(-1, address, MEM_VALUE(address), 1, flags, 1);
			break;
	}

	cheat_unparent(cheat);

	return cheat;
}