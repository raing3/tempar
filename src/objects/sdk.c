#include <stdio.h>
#include <pspinit.h>
#include "sdk.h"
#include "common.h"

int add_cheat_pspcheat_prx(char cheat_name[31], char enabled) {
	Cheat *cheat = cheat_add(NULL);

	if(cheat) {
		cheat->flags = CHEAT_CWCHEAT;
		cheat_set_name(cheat, cheat_name, 0);

		switch(enabled) {
			case 1:
				cheat_set_status(cheat, CHEAT_SELECTED);
				break;
			case 2:
				cheat_set_status(cheat, CHEAT_CONSTANT);
				break;
		}

		return cheat_get_index(cheat);
	}

	return -1;
}

int add_codeline_pspcheat_prx(int cheatnum, u32 adress, u32 value) {
	Cheat *cheat = cheat_get(cheatnum);

	if(cheat) {
		Block *block = block_insert(cheat, NULL, cheat->length);

		if(block) {
			block->address = adress;
			block->value = value;
			return 0;
		}
	}

	return -1;
}

int read_config(void) {
	return config_load(sceKernelInitKeyConfig() == PSP_INIT_KEYCONFIG_POPS ? "config_pops.bin" : "config.bin");
}

int config_saver(void) {
	return config_save(sceKernelInitKeyConfig() == PSP_INIT_KEYCONFIG_POPS ? "config_pops.bin" : "config.bin");
}

void setcodes(int Buttons) {
	cheat_apply(0);
}

int readdb(char game_id[], char dbnum) {
	cheat_load(game_id, dbnum, 0);
	return 0;
}