#include <pspinit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"

extern Config cfg;
extern MenuState menu;
extern char boot_path[];

static Cheat *cheats;
static Block *blocks;
static u32 *original_values;
char game_id[10];
char game_name[65];

u32 cheat_max;
u32 cheat_total;
u32 cheat_new_no;

u32 block_max;
u32 block_total;

SceCtrlData game_pad;
u16 psx_pad;
u32 dx_run_counter;

/* Cheat apply functions */

u32 real_address(u32 address) {
	address &= 0x0FFFFFFF;

	if(address >= 0 && address <= cfg.address_end - cfg.address_start) {
		address += cfg.address_start;
	} else if(address > cfg.address_end) {
		address = cfg.address_end;
	}

	return address;
}

u32 address(u32 address) {
	return real_address(address) - cfg.address_format;
}

u32 address_load(u32 address, u8 type) {
	if((address & 0xFF000000) == 0x0A000000) {
		u32 value = 0;

		// check if value should be handeled by a fake address
		switch(address & 0xFFFFFFFC) {
			case 0x0A000000:
				value = game_pad.Buttons;
				break;
			case 0x0A000004:
				value = game_pad.Lx;
				break;
			case 0x0A000008:
				value = game_pad.Ly;
				break;
		}

		value >>= (8 * (address % 4));

		switch(type & 0xF) {
			case 0:
				return MEM(value);
				break;
			case 1:
				return MEM_SHORT(value);
				break;
			case 2:
				return MEM_INT(value);
				break;
		}
	} else {
		// no fake address found, get value from real address
		if(!(type & CHEAT_REAL_ADDRESS) || address < cfg.address_start) {
			address = real_address(address);
		}

		switch(type & 0xF) {
			case 0:
				return _lb(address);
				break;
			case 1:
				return _lh(address & 0xFFFFFFFE);
				break;
			case 2:
				return _lw(address & 0xFFFFFFFC);
				break;
		}
	}

	return 0;
}

void address_set(u32 value, u32 address, u8 type) {
	if(!(type & CHEAT_REAL_ADDRESS) || address < cfg.address_start) {
		address = real_address(address);
	}

	switch(type & 0xF) {
		case 0:
			_sb(value, address);
			sceKernelDcacheWritebackInvalidateRange((void*)address, 1);
			sceKernelIcacheInvalidateRange((void*)address, 1);
			break;
		case 1:
			address &= 0xFFFFFFFE;
			_sh(value, address);
			sceKernelDcacheWritebackInvalidateRange((void*)address, 2);
			sceKernelIcacheInvalidateRange((void*)address, 2);
			break;
		case 2:
			address &= 0xFFFFFFFC;
			_sw(value, address);
			sceKernelDcacheWritebackInvalidateRange((void*)address, 4);
			sceKernelIcacheInvalidateRange((void*)address, 4);
			break;
	}
}

void memory_copy(u32 to, u32 from, u32 bytes) {
	to = real_address(to);
	from = real_address(from);

	memcpy((void*)to, (void*)from, bytes);

	sceKernelDcacheWritebackInvalidateRange((void*)to, bytes);
	sceKernelIcacheInvalidateRange((void*)to, bytes);
}

void cheat_apply(int action) {
	u32 cheat_applied = 0;

	cheat_pad_update();

	int i;
	for(i = 0; i < cheat_total; i++) {
		Cheat *cheat = cheat_get(i);

		#ifdef _AUTOOFF_
		if(original_values != NULL) {
			if((cheat->flags & CHEAT_FRESH) || (action & CHEAT_TOGGLE_ALL)) {
				if(cheat->flags & CHEAT_SELECTED) {
					if(action & CHEAT_TOGGLE_ALL) {
						if(cfg.cheat_status) {
							cheat_backup_restore(cheat, CHEAT_BACKUP);
						} else {
							cheat_backup_restore(cheat, CHEAT_RESTORE);
						}
					} else {
						cheat_backup_restore(cheat, CHEAT_BACKUP);
					}
				} else if(cheat->flags & CHEAT_CONSTANT) {
					if(!(action & CHEAT_TOGGLE_ALL)) {
						cheat_backup_restore(cheat, CHEAT_BACKUP);
					}
				} else {
					if(!(action & CHEAT_TOGGLE_ALL)) {
						cheat_backup_restore(cheat, CHEAT_RESTORE);
					}
				}
			}
		}
		#endif

		if((cfg.cheat_status && (cheat->flags & CHEAT_SELECTED)) || (cheat->flags & CHEAT_CONSTANT)) {
			cheat_applied =  1;
			
			if(cheat->flags & CHEAT_CWCHEAT) {
				if(sceKernelInitKeyConfig() == PSP_INIT_KEYCONFIG_POPS) {
					cheat_apply_psx_gs(cheat);
				} else {
					cheat_apply_cwcheat(cheat);
				}
			} else {
				cheat_apply_pspar(cheat);
			}
		}
	}

	dx_run_counter += cheat_applied;
}

void cheat_apply_pspar(Cheat *cheat) {
#ifdef _PSPAR_
	u32 i = 0;

	// general variables
	u8 type = 0;
	u32 address = 0;
	u32 value = 0;
	u32 loop = 0;

	// cheat engine temporary variables
	u32 dx_offset = 0;
	u32 dx_repeat_value = 0;
	u32 dx_repeat_begin = 0;
	u32 dx_data_register = 0;
	u32 dx_execution_status = 0;
	u32 dx_execution_status_old = 0;

	for(i = cheat->block; i < (cheat->block + cheat->length); i++) {
		Block *block = block_get(i);

		switch(block->address >> 28) {
			case 0x0C:
			case 0x0D:
				type = block->address >> 24;
				address = block->address & 0x00FFFFFF;
				break;
			default:
				type = block->address >> 28;
				address = block->address & 0x0FFFFFFF;
				break;
		}

		value = block->value;

		// only process code if cheat engine status is enabled or code is a terminator code
		if((dx_execution_status & 1) == 0 || type == 0xD0 || type == 0xD1 || type == 0xD2 || (type >= 0x03 && type <= 0x0A) || type == 0xC5) {
			if(type >= 0x00 && type <= 0x02) {
				// constant ram write codes (0x01 - 0x02)
				address_set(value, address + dx_offset, (0x02 - type) | CHEAT_REAL_ADDRESS);
			} else if(type >= 0x03 && type <= 0x0A) {
				// conditional 32-bit codes (0x03 - 0x06) and conditional 16-bit + masking codes (0x07 - 0x0A)

				// hijack the pspar button press codes
				if(cfg.hijack_pspar_buttons) {
					if(address >= 0x09F00000 && (type == 0x09 || type == 0x0A)) {
						if((value & 0xFFFF) > 0) {
							address = 0x0A000000;
							value = (value & 0xFFFF) + (~(value & 0xFFFF) << 16);
						} else {
							continue;
						}
					}
				}

				if((address & 1) && (cheat->flags & CHEAT_PSPAR_EXT)) {
					address += dx_offset;
				} else if(address == 0) {
					address = dx_offset;
				}

				if(((dx_execution_status & 1) != 0) ||
					(type == 0x03 && !(value > address_load(address, 2 | CHEAT_REAL_ADDRESS))) ||
					(type == 0x04 && !(value < address_load(address, 2 | CHEAT_REAL_ADDRESS))) ||
					(type == 0x05 && !(value == address_load(address, 2 | CHEAT_REAL_ADDRESS))) ||
					(type == 0x06 && !(value != address_load(address, 2 | CHEAT_REAL_ADDRESS))) ||
					(type == 0x07 && !((value & 0xFFFF) > ((~value >> 16) & address_load(address, 1 | CHEAT_REAL_ADDRESS)))) ||
					(type == 0x08 && !((value & 0xFFFF) < ((~value >> 16) & address_load(address, 1 | CHEAT_REAL_ADDRESS)))) ||
					(type == 0x09 && !((value & 0xFFFF) == ((~value >> 16) & address_load(address, 1 | CHEAT_REAL_ADDRESS)))) ||
					(type == 0x0A && !((value & 0xFFFF) != ((~value >> 16) & address_load(address, 1 | CHEAT_REAL_ADDRESS))))) {
						dx_execution_status = (dx_execution_status << 1) | 1;
				} else {
					dx_execution_status = (dx_execution_status << 1);
				}
			} else if(type == 0x0B) {
				// load offset (0x0B)
				dx_offset = address_load(address + dx_offset, 2 | CHEAT_REAL_ADDRESS);
			} else if(type == 0xC0) {
				// loop code (0xC0)
				dx_repeat_value = value;
				dx_repeat_begin = i;
				dx_execution_status_old = dx_execution_status;
			} else if(type == 0xC1) {
				// call function with arguments (0xC1)
				if(cheat->flags & CHEAT_PSPAR_EXT) {
					Block *block_next = block_get(i + 1);
					Block *block_next2 = block_get(i + 2);

					void (*function_pointer)(u32, u32, u32, u32) = (void*)value;
					function_pointer(block_next->address, block_next->value, block_next2->address, block_next2->value);
				}
			} else if(type == 0xC2) {
				// run code from cheat list (0xC2)
				if(cheat->flags & CHEAT_PSPAR_EXT) {
					void (*function_pointer)(u32, u32, u32, u32) = (void*)block + 8;
					function_pointer(0, 0, 0, 0);
				}
			} else if(type == 0xC4) {
				// safe data store (0xC4)
				dx_offset = (u32)block;
			} else if(type == 0xC5) {
				// counter (0xC5)
				if(cheat->flags & CHEAT_PSPAR_EXT) {
					if(((dx_execution_status & 1) != 0) ||
						!(((~value >> 16) & dx_run_counter) == (value & 0xFFFF))) {
							dx_execution_status = (dx_execution_status << 1) | 1;
					} else {
						dx_execution_status = (dx_execution_status << 1);
					}
				}
			} else if(type == 0xC6) {
				// write offset (0xC6)
				address_set(dx_offset, value, 2 | CHEAT_REAL_ADDRESS);
			} else if(type == 0xD0) {
				// terminator (0xD0)
				dx_execution_status >>= 1;
			} else if(type == 0xD1) {
				// loop execute variant (0xD1)
				if(dx_repeat_value > 0) {
					dx_execution_status = dx_execution_status_old;
					i = dx_repeat_begin;
					dx_repeat_value--;
				} else {
					dx_execution_status = 0;
				}
			} else if(type == 0xD2) {
				// loop execute variant / full terminator (0xD2)
				if(dx_repeat_value > 0) {
					dx_execution_status = dx_execution_status_old;
					i = dx_repeat_begin;
					dx_repeat_value--;
				} else {
					dx_execution_status_old = 0;
					dx_execution_status = 0;
					dx_offset = 0;
					dx_data_register = 0;
					dx_repeat_begin = 0;
				}
			} else if(type == 0xD3) {
				// set offset (0xD3)
				dx_offset = value;
			} else if(type == 0xD4) {
				// data register codes (0xD4)
				if(cheat->flags & CHEAT_PSPAR_EXT) {
					if(address == 0x01) {
						dx_data_register |= value;
					} else if(address == 0x02) {
						dx_data_register &= value;
					} else if(address == 0x03) {
						dx_data_register ^= value;
					} else if(address == 0x04) {
						dx_data_register <<= value;
					} else if(address == 0x05) {
						dx_data_register >>= value;
					} else if(address == 0x06) {
						dx_data_register = (dx_data_register >> value) | (dx_data_register << (32 - value));
					} else if(address == 0x07) {
						if((dx_data_register & 0x80000000) == 0x80000000) {
							for(loop = 0; loop < value; loop++) {
								dx_data_register = (dx_data_register >> 1) | 0x80000000;
							}
						} else {
							dx_data_register >>= value;
						}
					} else if(address == 0x08) {
						dx_data_register *= value;
					} else {
						dx_data_register += value;
					}
				} else {
					dx_data_register += value;
				}
			} else if(type == 0xD5) {
				// set value (0xD5)
				dx_data_register = value;
			} else if(type == 0xD6) {
				// 32-bit incrementive write (0xD6)
				address_set(dx_data_register, value + dx_offset, 2 | CHEAT_REAL_ADDRESS);
				dx_offset += 4;
			} else if(type == 0xD7) {
				// 16-bit incrementive write (0xD7)
				address_set(dx_data_register, value + dx_offset, 1 | CHEAT_REAL_ADDRESS);
				dx_offset += 2;
			} else if(type == 0xD8) {
				// 8-bit incrementive write (0xD8)
				address_set(dx_data_register, value + dx_offset, 0 | CHEAT_REAL_ADDRESS);
				dx_offset += 1;
			} else if(type == 0xD9) {
				// 32-bit load (0xD9)
				dx_data_register = address_load(value + dx_offset, 2 | CHEAT_REAL_ADDRESS);
			} else if(type == 0xDA) {
				// 16-bit load (0xDA)
				dx_data_register = address_load(value + dx_offset, 1 | CHEAT_REAL_ADDRESS);
			} else if(type == 0xDB) {
				// 8-bit load (0xDB)
				dx_data_register = address_load(value + dx_offset, 0 | CHEAT_REAL_ADDRESS);
			} else if(type == 0xDC) {
				// add offset (0xDC)
				dx_offset += value;
			} else if(type == 0x0E) {
				// patch code (0x0E)
				memcpy((void*)real_address(address), (void*)block + 8, value);
			} else if(type == 0x0F) {
				// memory copy code (0x0F)
				memory_copy(address, dx_offset, value);
			}
		}

		// always skip 0x0E code lines so they aren't detected as code lines if
		// cheat engine is disabled
		if(type == 0x0E || type == 0xC2) {
			i += (value / 8) + ((value % 8) > 0);
		} else if(type == 0xC1) {
			i += 2;
		}

		// reset temporary flags or continue loop at end of cheat
		if(i + 1 >= cheat->block + cheat->length) {
			if(dx_repeat_value > 0) {
				dx_execution_status = dx_execution_status_old;
				i = dx_repeat_begin;
				dx_repeat_value--;
			} else {
				dx_execution_status_old = 0;
				dx_execution_status = 0;
				dx_offset = 0;
				dx_data_register = 0;
				dx_repeat_begin = 0;
			}
		}
	}
#endif
}

void cheat_apply_cwcheat(Cheat *cheat) {
#ifdef _CWCHEAT_
	int i;

	// general variables
	unsigned int type = 0;
	unsigned int address = 0, value = 0, loop = 0;

	// temporary general variables
	unsigned int tmp_type = 0, tmp_address = 0, tmp_value = 0, tmp = 0;

	// cwcheat related variables
	unsigned int cw_skip_lines = 0;

	for(i = cheat->block; i < (cheat->block + cheat->length); i++) {
		Block *block = block_get(i);
		Block *block_next = block_get(i + 1);

		if(cheat->flags & CHEAT_CWCHEAT) { // cwcheat code engine
			type = block->address >> 28;
			address = block->address & 0x0FFFFFFF;
			value = block->value;

			// only process codes if we don't have to skip any lines
			if(cw_skip_lines <= 0) {
				switch(type) {
					case 0x00:
					case 0x01:
					case 0x02:
						// Constant Write Commands
						address_set(value, address, type);
						break;
					case 0x03:
						// Increment/Decrement Commands
						switch(address & 0x00F00000) {
							case 0x00100000:
								address_set(address_load(value, 0) + (address & 0xFF), value, 0);
								break;
							case 0x00200000:
								address_set(address_load(value, 0) - (address & 0xFF), value, 0);
								break;
							case 0x00300000:
								address_set(address_load(value, 1) + (address & 0xFFFF), value, 1);
								break;
							case 0x00400000:
								address_set(address_load(value, 1) - (address & 0xFFFF), value, 1);
								break;
							case 0x00500000:
								address_set(address_load(value, 2) + (block_next->address), value, 2);
								cw_skip_lines++;
								break;
							case 0x00600000:
								address_set(address_load(value, 2) - (block_next->address), value, 2);
								cw_skip_lines++;
								break;
						}
						break;
					case 0x0D:
						switch(value & 0xFFFF0000) {
							// 16-bit Test Commands
							case 0x00000000:
								if(!(address_load(address, 1) == (value & 0xFFFF))) {
									cw_skip_lines++;
								}
								break;
							case 0x00100000:
								if(!(address_load(address, 1) != (value & 0xFFFF))) {
									cw_skip_lines++;
								}
								break;
							case 0x00200000:
								if(!(address_load(address, 1) < (value & 0xFFFF))) {
									cw_skip_lines++;
								}
								break;
							case 0x00300000:
								if(!(address_load(address, 1) > (value & 0xFFFF))) {
									cw_skip_lines++;
								}
								break;
							// 8-bit Test Commands
							case 0x20000000:
								if(!(address_load(address, 0) == (value & 0xFF))) {
									cw_skip_lines++;
								}
								break;
							case 0x20100000:
								if(!(address_load(address, 0) != (value & 0xFF))) {
									cw_skip_lines++;
								}
								break;
							case 0x20200000:
								if(!(address_load(address, 0) < (value & 0xFF))) {
									cw_skip_lines++;
								}
								break;
							case 0x20300000:
								if(!(address_load(address, 0) > (value & 0xFF))) {
									cw_skip_lines++;
								}
								break;
							default:
								switch(value & 0xF0000000) {
									// 32-bit Multiskip Test Commands
									case 0x40000000:
										if(!(address_load(address, (block_next->value & 0xF)) == address_load((value & 0x0FFFFFFF), (block_next->value & 0xF)))) {
											cw_skip_lines += block_next->address;
										}
										cw_skip_lines++;
										break;
									case 0x50000000:
										if(!(address_load(address, (block_next->value & 0xF)) != address_load((value & 0x0FFFFFFF), (block_next->value & 0xF)))) {
											cw_skip_lines += block_next->address;
										}
										cw_skip_lines++;
										break;
									case 0x60000000:
										if(!(address_load(address, (block_next->value & 0xF)) < address_load((value & 0x0FFFFFFF), (block_next->value & 0xF)))) {
											cw_skip_lines += block_next->address;
										}
										cw_skip_lines++;
										break;
									case 0x70000000:
										if(!(address_load(address, (block_next->value & 0xF)) > address_load((value & 0x0FFFFFFF), (block_next->value & 0xF)))) {
											cw_skip_lines += block_next->address;
										}
										cw_skip_lines++;
										break;
									// Joker Codes
									case 0x10000000:
										if(!((game_pad.Buttons & (value & 0x0FFFFFFF)) == (value & 0x0FFFFFFF))) {
											cw_skip_lines += (address & 0xFF) + 1;
										}
										break;
									case 0x30000000:
										if(!((game_pad.Buttons & (value & 0x0FFFFFFF)) != (value & 0x0FFFFFFF))) {
											cw_skip_lines += (address & 0xFF) + 1;
										}
										break;
								}
								break;
						}
						break;
					case 0x0E:
						switch(address & 0x0F000000) {
							// 8-bit Multiskip Test Commands
							case 0x01000000:
								switch(value & 0xF0000000) {
									case 0x00000000:
										if(!(address_load((value & 0x0FFFFFFF), 0) == (address & 0x000000FF))) {
											cw_skip_lines += (address & 0x00FF0000) / 0xFFFF;
										}
										break;
									case 0x10000000:
										if(!(address_load((value & 0x0FFFFFFF), 0) != (address & 0x000000FF))) {
											cw_skip_lines += (address & 0x00FF0000) / 0xFFFF;
										}
										break;
									case 0x20000000:
										if(!(address_load((value & 0x0FFFFFFF), 0) < (address & 0x000000FF))) {
											cw_skip_lines += (address & 0x00FF0000) / 0xFFFF;
										}
										break;
									case 0x30000000:
										if(!(address_load((value & 0x0FFFFFFF), 0) > (address & 0x000000FF))) {
											cw_skip_lines += (address & 0x00FF0000) / 0xFFFF;
										}
										break;
								}
								break;
							// 16-bit Multiskip Test Commands
							default:
								switch(value & 0xF0000000) {
									case 0x00000000:
										if(!(address_load((value & 0x0FFFFFFF), 1) == (address & 0x0000FFFF))) {
											cw_skip_lines += (address & 0x00FF0000) / 0xFFFF;
										}
										break;
									case 0x10000000:
										if(!(address_load((value & 0x0FFFFFFF), 1) != (address & 0x0000FFFF))) {
											cw_skip_lines += (address & 0x00FF0000) / 0xFFFF;
										}
										break;
									case 0x20000000:
										if(!(address_load((value & 0x0FFFFFFF), 1) < (address & 0x0000FFFF))) {
											cw_skip_lines += (address & 0x00FF0000) / 0xFFFF;
										}
										break;
									case 0x30000000:
										if(!(address_load((value & 0x0FFFFFFF), 1) > (address & 0x0000FFFF))) {
											cw_skip_lines += (address & 0x00FF0000) / 0xFFFF;
										}
										break;
								}
								break;
						}
						break;
					case 0x07:
						// Bolean Commands
						switch(value & 0xFFFF0000) {
							case 0x00000000:
								address_set((address_load(address, 0) | (value & 0xFF)), address, 0);
								break;
							case 0x00020000:
								address_set((address_load(address, 0) & (value & 0xFF)), address, 0);
								break;
							case 0x00040000:
								address_set((address_load(address, 0) ^ (value & 0xFF)), address, 0);
								break;
							case 0x00010000:
								address_set((address_load(address, 1) | (value & 0xFFFF)), address, 1);
								break;
							case 0x00030000:
								address_set((address_load(address, 1) & (value & 0xFFFF)), address, 1);
								break;
							case 0x00050000:
								address_set((address_load(address, 1) ^ (value & 0xFFFF)), address, 1);
								break;
						}
						break;
					case 0x06:
						tmp_address = address_load(address, 2); // pointer address
						tmp_type = (block_next->address & 0x000FFFFF); // type and number of times (nnnn)
						tmp_value = block_next->value; // base (iiiiiiii)

						if((tmp_type & 0xFFFF) == 1) { // normal pointer
							if(tmp_address >= cfg.address_start && tmp_address <= cfg.address_end) {
								if((tmp_type & 0xF0000) <= 0x20000){
									address_set(value, tmp_address + tmp_value, (tmp_type & 0xF0000) / 0xFFFF);
								} else if((tmp_type & 0xF0000) <= 0x50000) {
									address_set(value, tmp_address - tmp_value, ((tmp_type - 0x30000) & 0xF0000) / 0xFFFF);
								}
							}

							cw_skip_lines++;
						} else {
							Block *tmp_block = block_get(i + 2);

							if((tmp_type & 0xFFFF) == 2 && (tmp_block->address >> 28) == 0x1) { // copy byte pointer
								if(tmp_address >= cfg.address_start && tmp_address <= cfg.address_end){
									memory_copy(address_load(address + (4 * (block_next->address >> 20)), 2) + (tmp_block->address & 0x0FFFFFFF), tmp_address + tmp_value, value);
								}

								cw_skip_lines+=2;
							} else if((tmp_type & 0xFFFF) > 1 && ((tmp_block->address >> 28) == 0x2 || (tmp_block->address >> 28) == 0x3)) { // pointer walk
								for(loop = 0; loop < (tmp_type & 0xFFFF) - 1; loop++) {
									if(tmp_address >= cfg.address_start && tmp_address <= cfg.address_end) {
										tmp_block = block_get(i + (loop / 2) + 2);
										if(loop % 2 == 0) {
											if((tmp_block->address & 0xF0000000) == 0x20000000) {
												tmp_address = address_load(tmp_address + (tmp_block->address & 0x0FFFFFFF), 2);
											} else if((tmp_block->address & 0xF0000000) == 0x30000000) {
												tmp_address = address_load(tmp_address - (tmp_block->address & 0x0FFFFFFF), 2);
											}
										} else {
											if((tmp_block->value & 0xF0000000) == 0x20000000) {
												tmp_address = address_load(tmp_address + (tmp_block->value & 0x0FFFFFFF), 2);
											} else if((tmp_block->value & 0xF0000000) == 0x30000000) {
												tmp_address = address_load(tmp_address - (tmp_block->value & 0x0FFFFFFF), 2);
											}
										}
									} else {
										break;
									}
								}

								if(tmp_address >= cfg.address_start && tmp_address <= cfg.address_end) {
									if((tmp_type & 0xF0000) <= 0x20000) {
										address_set(value, tmp_address + tmp_value, (tmp_type & 0xF0000) / 0xFFFF);
									} else if((tmp_type & 0xF0000) <= 0x50000) {
										address_set(value, tmp_address - tmp_value, ((tmp_type - 0x30000) & 0xF0000) / 0xFFFF);
									}
								}

								cw_skip_lines += ((tmp_type & 0xFFFF) / 2) + 1;
							} else if((tmp_type & 0xFFFF) > 1 && (tmp_block->address >> 28) == 0x9) { // multi-address write
								if(tmp_address >= cfg.address_start && tmp_address <= cfg.address_end) {
									switch(tmp_type & 0xF0000) {
										case 0x00000: case 0x30000:
											tmp = 1;
											break;
										case 0x10000: case 0x40000:
											tmp = 2;
											break;
										case 0x20000: case 0x50000:
											tmp = 4;
											break;
									}

									for(loop = 0; loop < (tmp_type & 0xFFFF); loop++) {
										if((block_next->address >> 20) > 0) {
											tmp_address = address_load(address + (loop * ((block_next->address >> 20) * 4)), 2);
										}

										switch(tmp_type & 0xF0000) {
											case 0x00000: case 0x10000: case 0x20000:
												address_set(value + (tmp_block->value * loop), tmp_address + tmp_value + ((tmp_block->address & 0x0FFFFFFF) * tmp * loop), (tmp_type & 0xF0000) / 0xFFFF);
												break;
											case 0x30000: case 0x40000: case 0x50000:
												address_set(value + (tmp_block->value * loop), tmp_address - tmp_value - ((tmp_block->address & 0x0FFFFFFF) * tmp * loop), ((tmp_type - 0x30000) & 0xF0000) / 0xFFFF);
												break;
										}
									}
								}

								cw_skip_lines += 2;
							}
						}
						break;
					case 0x05:
						memory_copy((block_next->address & 0x0FFFFFFF), address, value);
						cw_skip_lines++;
						break;
					case 0x04:
						for(loop = 0; loop < (value >> 16); loop++) {
							address_set(block_next->address + (block_next->value * loop), address + (((value & 0x0000FFFF) * 4) * loop), 2);
						}
						cw_skip_lines++;
						break;
					case 0x08:
						for(loop = 0; loop < (value >> 16); loop++) {
							address_set((block_next->address & 0x0000FFFF) + (block_next->value * loop), address + (((value & 0x0000FFFF) * ((block_next->address & 0xF0000000) == 0x10000000 ? 2 : 1)) * loop), (block_next->address & 0xF0000000) / 0xFFFFFFF);
						}
						cw_skip_lines++;
						break;
					case 0x0C:
						if(!(address_load(address, 2) == value)) {
							i = (cheat->block + cheat->length);
						}
						break;
					case 0x0B:
						sceKernelDelayThread(value);
						break;
				}
			} else {
				cw_skip_lines--;
			}
		}
	}
#endif
}

void cheat_apply_psx_gs(Cheat *cheat) {
	int i;

	for(i = cheat->block; i < (cheat->block + cheat->length); i++) {
		Block *block = block_get(i);
		u8 type = block->address >> 24;
		u32 address = block->address & 0x00FFFFFF;
		u32 value = block->value & 0xFFFF;

		if(type == 0x80) {
			// 16-bit constant write
			address_set(value, address, 1);
		} else if(type == 0x30) {
			// 8-bit constant write
			address_set(value, address, 0);
		} else if(type == 0x10) {
			// 16-bit increment
			address_set(address_load(address, 1) + value, address, 1);
		} else if(type == 0x11) {
			// 16-bit decrement
			address_set(address_load(address, 1) - value, address, 1);
		} else if(type == 0x20) {
			// 8-bit increment
			address_set(address_load(address, 0) + value, address, 0);
		} else if(type == 0x21) {
			// 8-bit decrement
			address_set(address_load(address, 0) - value, address, 0);
		} else if(type == 0xE0) {
			// 8-bit equal
			if(!(address_load(address, 0) == (value & 0xFF)))
				i++;
		} else if(type == 0xE1) {
			// 8-bit not equal
			if(!(address_load(address, 0) != (value & 0xFF)))
				i++;
		} else if(type == 0xE2) {
			// 8-bit less than
			if(!(address_load(address, 0) < (value & 0xFF)))
				i++;
		} else if(type == 0xE3) {
			// 8-bit greater than
			if(!(address_load(address, 0) > (value & 0xFF)))
				i++;
		} else if(type == 0xD0 || type == 0x70) {
			// 16-bit equal
			if(!(address_load(address, 1) == value))
				i++;
		} else if(type == 0xD1 || type == 0x90) {
			// 16-bit not equal
			if(!(address_load(address, 1) != value))
				i++;
		} else if(type == 0xD2) {
			// 16-bit less than
			if(!(address_load(address, 1) < value))
				i++;
		} else if(type == 0xD3) {
			// 16-bit greater than
			if(!(address_load(address, 1) > value))
				i++;
		} else if(type == 0xD4) {
			// universal joker code
			if(!((psx_pad & value) == value))
				i++;
		} else if(type == 0xC2) {
			// copy bytes
			Block *block_next = block_get(i + 1);
			memory_copy((block_next->address & 0x00FFFFFF), address, value);
			i++;
		} else if(type == 0x50) {
			// serial code
			u32 loop;
			Block *block_next = block_get(i + 1);
			for(loop = 0; loop < (address >> 8); loop++) {
				address_set(block_next->value + (loop * value), (block_next->address & 0x00FFFFFF) + (loop * (address & 0xFF)), ((block_next->address >> 24) == 0x80 ? 1 : 0));
			}
			i++;
		} else if(type == 0xC0) {
			// code stopper (master code)
			//if(!(address_load(address, 1) == value))
			//	i++;
		} else if(type == 0xC1) {
			// code delay
			sceKernelDelayThread(value * 500);
		} else if(type == 0xD5) {
			// all codes on/off
		}
	}
}

void cheat_pad_update() {
	memset(&game_pad, 0, sizeof(SceCtrlData));
	psx_pad = 0;

	if(!menu.visible) {
		// update the pad data
		sceCtrlPeekBufferPositive(&game_pad, 1); 

		// update the psx pad data
		if(sceKernelInitKeyConfig() == PSP_INIT_KEYCONFIG_POPS) {
			if(game_pad.Buttons & PSP_CTRL_SELECT) {
				psx_pad |= PSX_CTRL_SELECT;
			}
			if(game_pad.Buttons & PSP_CTRL_START) {
				psx_pad |= PSX_CTRL_START;
			}
			if(game_pad.Buttons & PSP_CTRL_TRIANGLE) {
				psx_pad |= PSX_CTRL_TRIANGLE;
			}
			if(game_pad.Buttons & PSP_CTRL_CIRCLE) {
				psx_pad |= PSX_CTRL_CIRCLE;
			}
			if(game_pad.Buttons & PSP_CTRL_CROSS) {
				psx_pad |= PSX_CTRL_CROSS;
			}
			if(game_pad.Buttons & PSP_CTRL_SQUARE) {
				psx_pad |= PSX_CTRL_SQUARE;
			}
			if(game_pad.Buttons & PSP_CTRL_UP) {
				psx_pad |= PSX_CTRL_UP;
			}
			if(game_pad.Buttons & PSP_CTRL_RIGHT) {
				psx_pad |= PSX_CTRL_RIGHT;
			}
			if(game_pad.Buttons & PSP_CTRL_DOWN) {
				psx_pad |= PSX_CTRL_DOWN;
			}
			if(game_pad.Buttons & PSP_CTRL_LEFT) {
				psx_pad |= PSX_CTRL_LEFT;
			}
			if(game_pad.Buttons & PSP_CTRL_LTRIGGER) {
				psx_pad |= (PSX_CTRL_L1 | PSX_CTRL_L2);
			}
			if(game_pad.Buttons & PSP_CTRL_RTRIGGER) {
				psx_pad |= (PSX_CTRL_R1 | PSX_CTRL_R2);
			}
		}
	}
}

void cheat_backup_restore(Cheat *cheat, int action) {
#ifdef _AUTOOFF_
	if(original_values != NULL) {
		// unset fresh
		cheat->flags &= ~CHEAT_FRESH;

		// general variables
		u32 i;
		u32 type = 0;
		u32 address = 0;
		u32 value = 0;

		// pspar related variables
		u32 dx_execution_status = 0;

		for(i = cheat->block; i < (cheat->block + cheat->length); i++) {
			Block *block = block_get(i);
			Block *block_next = block_get(i + 1);

			#ifdef _PSPAR_
			if((cheat->flags & (CHEAT_PSPAR | CHEAT_PSPAR_EXT))) { // pspar code engine
				switch(block->address >> 28) {
					case 0x0C:
					case 0x0D:
						type = block->address >> 24;
						address = block->address & 0x00FFFFFF;
						break;
					default:
						type = block->address >> 28;
						address = block->address & 0x0FFFFFFF;
						break;
				}
				value = block->value;

				// only process codes if execution is enabled or we are doing a terminator
				if((dx_execution_status & 1) == 0 || type == 0xD0 || type == 0xD1 || type == 0xD2) {
					if(type >= 0x00 && type <= 0x02) {
						if(action & CHEAT_BACKUP) {
							original_values[i] = address_load(address, (2 - type) | CHEAT_REAL_ADDRESS);
						} else if(action & CHEAT_RESTORE) {
							address_set(original_values[i], address, (2 - type) | CHEAT_REAL_ADDRESS);
						}
					} else if((type >= 0x03 && type <= 0x0A) || type == 0x0B || type == 0x0F || type == 0xC0 || type == 0xC4 || type == 0xC5 || type == 0xC6 ||
						type == 0xD3 || type == 0xD4 || type == 0xD5 || type == 0xD6 || type == 0xD7 || type == 0xD8 || type == 0xD9 ||
						type == 0xDA || type == 0xDB || type == 0xDC) {
						dx_execution_status = (dx_execution_status << 1) | 1;
					} else if(type == 0xD0) {
						dx_execution_status >>= 1;
					} else if(type == 0xD1 || type == 0xD2) {
						dx_execution_status = 0;
					}
				}

				if(type == 0x0E || type == 0xC2) {
					i += (value / 8) + ((value % 8) > 0);
				} else if(type == 0xC1) {
					i += 2;
				}
			}
			#endif

			#ifdef _CWCHEAT_
			if((cheat->flags & CHEAT_CWCHEAT)) { // cwcheat code engine
				type = block->address >> 28;
				address = block->address & 0x0FFFFFFF;
				value = block->value;

				if(type >= 0x00 && type <= 0x02) {
					if(action & CHEAT_BACKUP) {
						original_values[i] = address_load(address, type);
					} else if(action & CHEAT_RESTORE) {
						address_set(original_values[i], address, type);
					}
				} else if(type == 0x0D) {
					switch(value & 0xF0000000) {
						case 0x00000000:
						case 0x20000000:
							i++;
							break;
						case 0x40000000:
						case 0x50000000:
						case 0x60000000:
						case 0x70000000:
							i += block_next->address + 1;
							break;
						case 0x10000000:
						case 0x30000000:
							i += (address & 0xFF) + 1;
							break;
					}
				} else if(type == 0x0E) {
					i += (address >> 16) & 0xFF;
				} else if(type == 0x06) {
					if((block_next->address & 0xFFFF) == 0x01) { // normal pointer
						i++;
					} else if((block_next->address & 0xFFFF) == 0x02 && (block_next->address >> 28) == 0x01) { // copy byte pointer
						i += 2;
					} else if((block_next->address & 0xFFFF) > 1) {
						if((block_next->address >> 28 == 0x02) || (block_next->address >> 28) == 0x03) { // pointer walk
							i += ((block_next->address & 0xFFFF) / 2) + 1;
						} else if((block_next->address >> 28) == 0x09) { // multi address write
							i += 2;
						}
					}
				} else if(type == 0x05 || type == 0x04 || type == 0x08) {
					i++;
				}
			}
			#endif
		}
	}
#endif
}

/* Cheat load/save functions */

void cheat_load(const char *game_id, char dbnum, char index) {
	char buffer[64];

	if(game_id == NULL) {
		game_id = gameid_get(0);
	}

	// load cwcheat db
	if(cheat_total == 0) {
		if(dbnum == -1) {
			sprintf(buffer, "cheats/%s.db", game_id);
		} else if(dbnum == 0) {
			sprintf(buffer, "cheat.db");
		} else {
			sprintf(buffer, "cheat%i.db", dbnum);
		}

		cheat_load_db(buffer, game_id, index);
	}

	// load pspar db
	if(cheat_total == 0) {
		if(dbnum == -1) {
			sprintf(buffer, "cheats/%s.bin", game_id);
		} else if(dbnum == 0) {
			sprintf(buffer, "cheat.bin");
		} else {
			sprintf(buffer, "cheat%i.bin", dbnum);
		}

		cheat_load_bin(buffer, game_id, index);
	}

	// load pspar db
	if(cheat_total == 0) {
		sprintf(buffer, "pspar_codes%i.bin", dbnum + 1);
		cheat_load_bin(buffer, game_id, index);
	}

	// load nitepr db
	if(cheat_total == 0) {
		if(dbnum == -1) {
			if(cheat_total == 0) {
				sprintf(buffer, "cheats/%s.txt", game_id);
				cheat_load_npr(buffer);
			}

			if(cheat_total == 0) {
				sprintf(buffer, "ms0:/seplugins/nitePR/%s.txt", game_id);
				cheat_load_npr(buffer);
			}
		}
	}
}

void cheat_load_db(const char *file, const char *game_id, char index) {
	#define CODE_LINE_S 0x01
	#define CODE_LINE_G 0x02
	#define CODE_LINE_C0 0x04
	#define CODE_LINE_C1 0x08
	#define CODE_LINE_C2 0x10
	#define CODE_LINE_L 0x20
	#define CODE_LINE_M 0x40
	#define CODE_LINE_N 0x80

	// load the cheats
	SceUID fd = fileIoOpen(file, PSP_O_RDONLY, 0777);

	if(fd > -1) {
		Cheat *cheat = NULL;
		Block *block = NULL;

		u32 current_cheat_index = 0;
		u32 line_type = 0;
		u32 game_parse = 0;
		u32 i = 0;

		while(fileIoSkipBlank(fd) != 0) {
			line_type = 0;

			if(_strnicmp(fileIoGet(), "_S ", 3) == 0) {
				line_type = CODE_LINE_S;
			} else if(game_parse) {
				if(_strnicmp(fileIoGet(), "_G ", 3) == 0) {
					line_type = CODE_LINE_G;
				} else if(_strnicmp(fileIoGet(), "_C0 ", 4) == 0 || _strnicmp(fileIoGet(), "_C ", 3) == 0) {
					line_type = CODE_LINE_C0;
				} else if(_strnicmp(fileIoGet(), "_C1 ", 4) == 0) {
					line_type = CODE_LINE_C1;
				} else if(_strnicmp(fileIoGet(), "_C2 ", 4) == 0) {
					line_type = CODE_LINE_C2;
				} else if(_strnicmp(fileIoGet(), "_L ", 3) == 0) {
					line_type = CODE_LINE_L;
				} else if(_strnicmp(fileIoGet(), "_M ", 3) == 0) {
					line_type = CODE_LINE_M;
				} else if(_strnicmp(fileIoGet(), "_N ", 3) == 0) {
					line_type = CODE_LINE_N;
				}
			}

			if(line_type != 0) {
				fileIoSkipWord(fd);
			}

			if(line_type & CODE_LINE_S) {
				if(cheat_total > 0)
					break;

				if(gameid_matches(game_id, fileIoGet()) && current_cheat_index++ >= index) { // if game id matches allow parse
					game_parse = 1;
					game_name[0] = 0;
				}
			} else if(game_parse) {
				if(line_type & CODE_LINE_G) {
					// set the game name
					memcpy(game_name, fileIoGet(), sizeof(game_name));
					for(i = 0; i < sizeof(game_name); i++) {
						if((u8)game_name[i] < (u8)' ') {
							game_name[i] = 0;
							break;
						}
					}
				} else if(line_type & (CODE_LINE_C0 | CODE_LINE_C1 | CODE_LINE_C2)) {
					cheat = cheat_add(NULL);
					if(!cheat)
						break;

					// set the code name
					cheat_set_name(cheat, fileIoGet(), 0);
					fileIoLseek(fd, strlen(cheat->name), SEEK_CUR);

					// enable the code
					if(line_type & CODE_LINE_C1) {
						cheat->flags |= (CHEAT_SELECTED | CHEAT_FRESH);
					} else if(line_type & CODE_LINE_C2) {
						cheat->flags |= (CHEAT_CONSTANT | CHEAT_FRESH);
					}
				} else if(line_type & (CODE_LINE_L | CODE_LINE_M | CODE_LINE_N)) {
					block = block_add(NULL);
					if(!block)
						break;

					// parse the address
					block->address = strtoul(fileIoGet(), NULL, (sceKernelInitKeyConfig() == PSP_INIT_KEYCONFIG_POPS ? 16 : 0));

					// parse the value
					fileIoSkipWord(fd);
					block->value = strtoul(fileIoGet(), NULL, (sceKernelInitKeyConfig() == PSP_INIT_KEYCONFIG_POPS ? 16 : 0));

					// update the code type
					cheat->flags &= ~(CHEAT_CWCHEAT | CHEAT_PSPAR | CHEAT_PSPAR_EXT);
					if(line_type & CODE_LINE_L) {
						cheat->flags |= CHEAT_CWCHEAT;
					} else if(line_type & CODE_LINE_M) {
						cheat->flags |= CHEAT_PSPAR;
					} else if(line_type & CODE_LINE_N) {
						cheat->flags |= CHEAT_PSPAR_EXT;
					}
				}
			}

			if(fileIoSkipLine(fd) == 0) {
				break;
			}
		}

		fileIoClose(fd);
	}
}

void cheat_load_bin(const char *file, const char *game_id, char index) {
	SceUID fd = fileIoOpen(file, PSP_O_RDONLY, 0777);

	if(fd > -1) {
		Cheat *cheat;
		Block *block;

		u32 current_cheat_index = 0;
		u32 i = 0;

		ARGameHeader game_header;
		ARItemHeader item_header;

		// skip the file header
		fileIoLseek(fd, 28, SEEK_CUR);

		while(fileIoRead(fd, &game_header, sizeof(ARGameHeader)) == sizeof(ARGameHeader)) {
			// skip game and continue loop if game id doesn't match
			if(!gameid_matches(game_id, game_header.game_id) || current_cheat_index++ < index) {
				if(game_header.game_size == 0) {
					break;
				} else {
					fileIoLseek(fd, (game_header.game_size * 4) - sizeof(ARGameHeader), SEEK_CUR);
					continue;
				}
			}

			// set the game name
			memcpy(game_name, fileIoGet(), sizeof(game_name));
			for(i = 0; i < sizeof(game_name); i++) {
				if((u8)game_name[i] < (u8)' ') {
					game_name[i] = 0;
					break;
				}
			}

			// skip the remainder of the game header
			fileIoLseek(fd, game_header.header_size - sizeof(game_header), SEEK_CUR);

			while(game_header.item_count-- > 0) {
				cheat = cheat_add(NULL);

				if(cheat) {
					fileIoRead(fd, &item_header, sizeof(item_header));

					// read/skip the cheat name
					cheat_set_name(cheat, fileIoGet(), 0);
					fileIoLseek(fd, (item_header.name_len_full - 1) * 4, SEEK_CUR);

					// set cheat info
					cheat->flags |= CHEAT_PSPAR;

					// add the code line
					while(item_header.code_count-- > 0) {
						block = block_add(NULL);

						if(block) {
							fileIoRead(fd, &block->address, sizeof(u32));
							fileIoRead(fd, &block->value, sizeof(u32));
						} else {
							game_header.item_count = 0;
							item_header.code_count = 0;
							break;
						}
					}
				} else {
					game_header.item_count = 0;
					break;
				}
			}

			break;
		}

		fileIoClose(fd);
	}
}

void cheat_load_npr(const char *file) {
	#define CODE_LINE_OFF 0x01
	#define CODE_LINE_ON 0x02
	#define CODE_LINE_ALWAYSON 0x04
	#define CODE_LINE_CODE 0x08

	// load the cheats
	SceUID fd = fileIoOpen(file, PSP_O_RDONLY, 0777);

	if(fd > -1) {
		Cheat *cheat;
		Block *block;

		u32 line_type = 0;
		u32 pointer_address = 0;

		while(fileIoSkipBlank(fd) != 0) {
			line_type = 0;

			if(_strnicmp(fileIoGet(), "#!!", 3) == 0) {
				line_type = CODE_LINE_ALWAYSON;
			} else if(_strnicmp(fileIoGet(), "#!", 2) == 0) {
				line_type = CODE_LINE_ON;
			} else if(_strnicmp(fileIoGet(), "#", 1) == 0) {
				line_type = CODE_LINE_OFF;
			} else if(_strnicmp(fileIoGet(), "0x", 2) == 0) {
				line_type = CODE_LINE_CODE;
			}

			if(line_type & (CODE_LINE_OFF | CODE_LINE_ON | CODE_LINE_ALWAYSON)) {
				// close off previous pointer first
				if(pointer_address != 0) {
					block = block_add(NULL);
					if(!block)
						break;

					block->address = 0xD2000000;
					pointer_address = 0;
				}

				// add new cheat
				cheat = cheat_add(NULL);
				if(!cheat)
					break;
				
				if(line_type & CODE_LINE_ALWAYSON) {
					cheat->flags |= (CHEAT_CONSTANT | CHEAT_FRESH);
					fileIoLseek(fd, 3, SEEK_CUR);
				} else if(line_type & CODE_LINE_ON) {
					cheat->flags |= (CHEAT_SELECTED | CHEAT_FRESH);
					fileIoLseek(fd, 2, SEEK_CUR);
				} else {
					fileIoLseek(fd, 1, SEEK_CUR);
				}

				cheat_set_name(cheat, fileIoGet(), 0);
				cheat->flags |= CHEAT_PSPAR;
			} else if(line_type & CODE_LINE_CODE) {
				block = block_add(NULL);
				if(!block)
					break;

				if(strtoul(fileIoGet(), NULL, 0) == 0xFFFFFFFF) {
					// close off previous pointer first
					if(pointer_address != 0) {
						block->address = 0xD2000000;
						pointer_address = 0;
						block = block_add(NULL);
						if(!block)
							break;
					}

					// skip the address
					fileIoSkipWord(fd);
						
					// set the pointer address
					pointer_address = real_address(strtoul(fileIoGet(), NULL, 0));

					// add the pointer address
					block->address = 0x60000000 | pointer_address;
					block = block_add(NULL);
					if(!block)
						break;

					block->address = 0xB0000000 | pointer_address;
				} else {
					// set/add the address
					if(pointer_address == 0) {
						block->address = real_address(strtoul(fileIoGet(), NULL, 0));
					} else {
						block->address = strtoul(fileIoGet(), NULL, 0) & 0x0FFFFFFF;
					}

					// skip the address
					fileIoSkipWord(fd);
						
					// set/add the value
					char *value_size;
					block->value = strtoul(fileIoGet(), &value_size, 0);
						
					// update the code type
					switch((u32)value_size - (u32)fileIoGet()) {
						case 4: // char
							block->address |= 0x20000000;
							break;
						case 6: // short
							block->address |= 0x10000000;
							break;
					}
				}
			}

			fileIoSkipLine(fd);
		}

		fileIoClose(fd);
	}
}

void cheat_save(const char *game_id) {
	int i, j;
	char buffer[128];

	Cheat *cheat = NULL;
	Block *block = NULL;

	sceIoMkdir("cheats", 0777);

	sprintf(buffer, "cheats/%s.db", game_id);
	sceIoRemove(buffer);

	if(cheat_total > 0) {
		SceUID fd = fileIoOpen(buffer, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);

		if(fd > -1) {
			sprintf(buffer, "_S %s\n", game_id); // write game id
			fileIoWrite(fd, buffer, strlen(buffer));

			if(strlen(game_name) > 0) { // write game name
				sprintf(buffer, "_G %s\n", game_name);
				fileIoWrite(fd, buffer, strlen(buffer));
			}

			for(i = 0; i < cheat_total; i++) {
				cheat = cheat_get(i);

				// write out the cheat name
				if(cheat->flags & CHEAT_CONSTANT) {
					sprintf(buffer, "_C2 %s\n", cheat->name);
				} else if(cheat->flags & CHEAT_SELECTED) {
					sprintf(buffer, "_C1 %s\n", cheat->name);
				} else {
					sprintf(buffer, "_C0 %s\n", cheat->name);
				}

				// write out the code name
				fileIoWrite(fd, buffer, strlen(buffer));

				for(j = cheat->block; j < (cheat->block + cheat->length); j++) {
					block = block_get(j);

					switch(cheat->flags & (CHEAT_CWCHEAT | CHEAT_PSPAR | CHEAT_PSPAR_EXT)) {
						case CHEAT_CWCHEAT:
							if(sceKernelInitKeyConfig() == PSP_INIT_KEYCONFIG_POPS) {
								sprintf(buffer, "_L %08X %04X\n", block->address, MEM_SHORT(block->value));
							} else {
								sprintf(buffer, "_L 0x%08X 0x%08X\n", block->address, block->value);
							}
							break;
						case CHEAT_PSPAR:
							sprintf(buffer, "_M 0x%08X 0x%08X\n", block->address, block->value);
							break;
						case CHEAT_PSPAR_EXT:
							sprintf(buffer, "_N 0x%08X 0x%08X\n", block->address, block->value);
							break;
					}

					fileIoWrite(fd, buffer, strlen(buffer));
				}
			}

			// close the file
			fileIoClose(fd);
		}
	}
}

int gameid_matches(const char *id1, const char *id2) {
	#define GAME_ID_LENGTH 10

	if(*id1 == '*' || *id2 == '*') {
		return 1;
	}

	return (_strnicmp(id1, id2, GAME_ID_LENGTH) == 0);
}

char *gameid_get(char force_refresh) {
	if(game_id[0] == 0 || force_refresh) {
		SceUID fd;
		char *game_info = (char*)sceKernelGetGameInfo();

		// get the default game id
		memcpy(game_id, (char*)(game_info + 0x44), 4);
		game_id[4] = '-';
		memcpy(game_id + 5, (char*)(game_info + 0x48), 5);

		switch(sceKernelInitKeyConfig()) {
			case PSP_INIT_KEYCONFIG_GAME:
				// create homebrew id
				if((_strnicmp((char*)(boot_path + strlen(boot_path) - 4), ".PBP", 4) == 0) && (cfg.hbid_pbp_force || _strnicmp("UCJS-10041", game_id, 10) == 0 || game_id[0] == 0)) {
					fd = fileIoOpen(boot_path, PSP_O_RDONLY, 0777);
					if(fd > -1) {
						u8 md5[16];
						sceKernelUtilsMd5Digest(fileIoGet(), 2048, md5);
						fileIoClose(fd);
						sprintf(game_id, "HB%08X", *(u32*)(md5 + 4) ^ *(u32*)(md5) ^ *(u32*)(md5 + 8) ^ *(u32*)(md5 + 12));
					}
				}

				// check the homebrew blacklist
				fd = fileIoOpen("hbblacklist.bin", PSP_O_RDONLY, 0777);
				if(fd > -1) {
					char *module_name;

					do {
						module_name = fileIoGet();
						if(sceKernelFindModuleByName(module_name)) {
							game_id[0] = 0;
							break;
						}
					} while(fileIoLseek(fd, strlen(module_name) + 1, SEEK_CUR) == strlen(module_name) + 1);

					fileIoClose(fd);
				}

				// game the gameid from the umd
				while(game_id[0] == 0) {
					fd = sceIoOpen("disc0:/UMD_DATA.BIN", PSP_O_RDONLY, 0777); 
					if(fd > -1) {
						sceIoRead(fd, game_id, 10);
						sceIoClose(fd);
					}
					sceKernelDelayThread(1000);
				}
				break;
			case PSP_INIT_KEYCONFIG_POPS:
				while(game_id[0] == 0) {
					memcpy(game_id, (char*)0x09E80400, 4);
					game_id[4] = '-';
					memcpy(game_id + 5, (char*)0x09E80404, 5);
					sceKernelDelayThread(1000);
				}
				break;
			case PSP_INIT_KEYCONFIG_VSH:
				strcpy(game_id, "VSH");
				break;
		}

		if(game_id[0] == 0) {
			strcpy(game_id, "UNKNOWN");
		}
	}

	return game_id;
}

/* Cheat (de)initialization functions */

int cheat_init(int num_cheats, int num_blocks, int auto_off) {
	cheat_deinit();

	cheats = kmalloc(0, PSP_SMEM_Low, num_cheats * sizeof(Cheat));
	if(cheats == NULL) {
		cheat_deinit();
		return -1;
	}

	blocks = kmalloc(0, PSP_SMEM_Low, num_blocks * sizeof(Block));
	if(blocks == NULL) {
		cheat_deinit();
		return -1;
	}

	#ifdef _AUTOOFF_
	if(auto_off) {
		original_values = kmalloc(0, PSP_SMEM_Low, num_blocks * sizeof(u32));
		if(original_values == NULL) {
			cheat_deinit();
			return -1;
		}
	}
	#endif

	memset(cheats, 0, num_cheats * sizeof(Cheat));
	memset(blocks, 0, num_blocks * sizeof(Block));

	cheat_max = num_cheats;
	block_max = num_blocks;

	return 0;
}

void cheat_deinit() {
	if(cheats != NULL) {
		kfree(cheats);
		cheats = NULL;
	}

	if(blocks != NULL) {
		kfree(blocks);
		blocks = NULL;
	}

	if(original_values != NULL) {
		kfree(original_values);
		original_values = NULL;
	}

	cheat_max = 0;
	block_max = 0;

	cheat_new_no = 0;
	cheat_total = 0;
	block_total = 0;

	dx_run_counter = 0;
}

/* Cheat functions */

Cheat *cheat_get(int index) {
	if(index >= 0 && index < cheat_total) {
		return &cheats[index];
	}

	return NULL;
}

Cheat *cheat_get_next(Cheat *cheat) {
	return cheat_get(cheat_get_index(cheat) + 1);
}

Cheat *cheat_get_previous(Cheat *cheat) {
	return cheat_get(cheat_get_index(cheat) - 1);
}

Cheat *cheat_add(Cheat *cheat) {
	if(cheat_total < cheat_max) {
		if(cheat) {
			memcpy(&cheats[cheat_total], cheat, sizeof(Cheat));
		}

		cheats[cheat_total].block = block_total;
		cheats[cheat_total].length = 0;
		cheat_total++;

		cheat_reparent(0, 0);

		return &cheats[cheat_total - 1];
	}

	return NULL;
}

Cheat *cheat_add_folder(char *name, u32 type) {
	Cheat *cheat = cheat_add(NULL);

	if(cheat) {
		cheat->flags = CHEAT_PSPAR;
		cheat_set_name(cheat, name, 0);

		Block *block = block_add(NULL);

		if(block) {
			block->address = 0xCF000000 | type;
			return cheat;
		}
	}

	return NULL;
}

Cheat *cheat_new(int index, u32 address, u32 value, u8 length, u8 flags, u32 size) {
	Cheat *cheat = cheat_insert(NULL, index);

	if(cheat) {
		sprintf(cheat->name, "NEW CHEAT %i", cheat_new_no++);
		cheat->flags = flags;

		address = real_address(address);

		if(flags & CHEAT_CWCHEAT) {
			address -= cfg.address_start;
		}

		switch(flags & (CHEAT_CWCHEAT | CHEAT_PSPAR | CHEAT_PSPAR_EXT)) {
			case CHEAT_CWCHEAT:
				if(sceKernelInitKeyConfig() == PSP_INIT_KEYCONFIG_POPS) {
					if(size == sizeof(int) || size == sizeof(short)) {
						address |= 0x80000000;
					} else if(size == sizeof(char)) {
						address |= 0x30000000;
					}
				} else {
					if(size == sizeof(int)) {
						address |= 0x20000000;
					} else if(size == sizeof(short)) {
						address |= 0x10000000;
					} else if(size == sizeof(char)) {
						address |= 0;
					}
				}
				break;
			case CHEAT_PSPAR:
			case CHEAT_PSPAR_EXT:
				if(size == sizeof(int)) {
					address |= 0;
				} else if(size == sizeof(short)) {
					address |= 0x10000000;
				} else if(size == sizeof(char)) {
					address |= 0x20000000;
				}
				break;
		}

		while(length-- > 0) {
			Block *block = block_insert(cheat, NULL, 0);
			if(!block)
				break;
			block->address = address;
			block->value = value;
		}

		return cheat;
	}

	return NULL;
}

void cheat_unparent(Cheat *cheat) {
	if(cheat != NULL) {
		Cheat *parent = cheat->parent;
		cheat->parent = NULL;
		while(parent != NULL) {
			Block *block = block_get(parent->block);
			block->value--;
			parent = parent->parent;
		}
	}
}

Cheat *cheat_new_from_memory(u32 address_start, u32 address_end) {
	address_start &= 0xFFFFFFFC;
	address_end &= 0xFFFFFFFC;

	if(address_start == address_end || address_end < address_start) {
		return NULL;
	}
	
	Cheat *cheat = cheat_add(NULL);

	if(cheat) {
		sprintf(cheat->name, "NEW CHEAT %i", cheat_new_no);
		cheat->flags = CHEAT_PSPAR;

		int i;
		for(i = address_start; i < address_end; i += 4) {
			Block *block = block_add(NULL);
			if(!block)
				break;
			block->address = i;
			block->value = MEM_VALUE_INT(i);
		}

		cheat_new_no++;
		return cheat;
	}

	return NULL;
}

Cheat *cheat_next_visible(Cheat *cheat) {
	if(cheat != NULL && cheat_total > 0) {
		int index = cheat_get_index(cheat);

		do {
			index = (index < cheat_total - 1 ? index + 1 : 0);
		} while(!cheat_is_visible(&cheats[index]) && index != 0);

		return &cheats[index];
	}

	return NULL;
}

Cheat *cheat_previous_visible(Cheat *cheat) {
	if(cheat != NULL && cheat_total > 0) {
		int index = cheat_get_index(cheat);

		do {
			index = (index > 0 ? index - 1 : cheat_total - 1);
		} while(!cheat_is_visible(&cheats[index]) && index != 0);

		return &cheats[index];
	}

	return NULL;
}

void cheat_collapse_folders(int collapse) {
	int i;
	for(i = 0; i < cheat_total; i++) {
		Cheat *cheat = cheat_get(i);
		if(cheat_folder_size(cheat)) {
			if(collapse < 0) {
				collapse = !(cheat->flags & CHEAT_HIDDEN);
			}

			if(collapse) {
				cheat->flags |= CHEAT_HIDDEN;
			} else {
				cheat->flags &= ~CHEAT_HIDDEN;
			}
		}
	}
}

void cheat_reparent(int start, int end) {
	Cheat *cheat = NULL, *parent = NULL;
	int parent_size = 0;
	int i;

	if(end <= start) {
		end = cheat_total;
	}

	// get initial parent
	for(i = start; i > 0; i++) {
		cheat = &cheats[i];

		if(cheat_folder_size(cheat) >= i - start) {
			parent = cheat;
			parent_size = cheat_folder_size(cheat) - (i - 1 - start);
		}
	}

	// update parents
	for(i = start; i < end; i++) {
		cheat = &cheats[i];

		if(parent_size > 0) {
			cheat->parent = parent;
			parent_size--;
		} else {
			cheat->parent = NULL;
		}

		if(cheat_folder_size(cheat)) {
			parent = cheat;
			parent_size = cheat_folder_size(cheat);
		} else {
			while(parent_size == 0 && parent != NULL) {
				parent = parent->parent;
				if(parent != NULL) {
					parent_size = cheat_folder_size(parent) - (i - cheat_get_index(parent));
				}
			}
		}
	}
}

void cheat_delete(Cheat *cheat) {
	if(cheat != NULL) {
		if(cheat_folder_size(cheat)) {
			cheat_delete_range(cheat_get_index(cheat), cheat_folder_size(cheat) + 1);
		} else {
			cheat_delete_range(cheat_get_index(cheat), 1);
		}
	}
}

void cheat_delete_range(int start, int number) {
	Cheat *cheat, *parent;
	u32 i, block_count = 0;

	// don't allow deletion of non-existent cheats
	if(start + number > cheat_total) {
		number = cheat_total - start;
	}

	for(i = start; i < start + number; i++) {
		cheat = cheat_get(i);

		// update the block count
		block_count += cheat->length;

		// update the parent item counts
		parent = cheat->parent;
		while(parent != NULL) {
			blocks[parent->block].value--;
			parent = parent->parent;
		}
	}

	for(i = start + number; i < cheat_total; i++) {
		cheat = cheat_get(i);

		// update the block indexes
		cheat->block -= block_count;
	}

	// move the cheats and blocks up in the list
	cheat = cheat_get(start);
	memmove(&blocks[cheat->block], &blocks[cheat->block + block_count], (block_total - cheat->block) * sizeof(Block));
	memmove(&cheats[start], &cheats[start + number], (cheat_total - start) * sizeof(Cheat));

	// update the block and cheat totals
	cheat_total -= number;
	block_total -= block_count;

	// update the cheat parents
	cheat_reparent(0, 0);
}

Cheat *cheat_copy(Cheat *cheat) {
	if(cheat != NULL) {
		if(cheat_folder_size(cheat)) {
			return cheat_copy_range(cheat_get_index(cheat), cheat_folder_size(cheat) + 1);
		} else {
			return cheat_copy_range(cheat_get_index(cheat), 1);
		}
	}

	return NULL;
}

Cheat *cheat_copy_range(int start, int number) {
	Cheat *cheat = NULL, *new_cheat = NULL, *first_cheat = NULL;
	u32 i, j;

	for(i = start; i < start + number; i++) {
		cheat = &cheats[i];
		new_cheat = cheat_add(cheat);

		if(!new_cheat) {
			break;
		}

		if(!first_cheat) {
			first_cheat = new_cheat;
		}

		for(j = 0; j < cheat->length; j++) {
			if(!block_add(block_get(cheat->block + j))) {
				break;
			}
		}
	}

	cheat_reparent(0, 0);

	return first_cheat;
}

int cheat_get_index(Cheat *cheat) {
	if(cheat != NULL) {
		int index = ((u32)cheat - (u32)cheats) / sizeof(Cheat);
		if(index >= 0 && index < cheat_total) {
			return index;
		}
	}

	return -1;
}

int cheat_exists(Cheat *cheat) {
	return (cheat_get_index(cheat) >= 0);
}

int cheat_set_name(Cheat *cheat, char *name, int name_length) {
	if(cheat != NULL) {
		if(name_length == 0) {
			// remove all \r \n and other rubbish
			for(name_length = 0; name_length + 1 < sizeof(cheat->name); name_length++) {
				if((u8)name[name_length] < (u8)' ')
					break;
			}
		} else if(name_length >= sizeof(cheat->name)) {
			name_length = sizeof(cheat->name) - 1;
		}

		memcpy(&cheat->name, name, name_length);

		return 0;
	}

	return -1;
}

int cheat_is_folder(Cheat *cheat) {
	if(cheat != NULL) {
		if(cheat->length == 1 && ((blocks[cheat->block].address >> 24) == 0xCF)) {
			int folder = 0;

			folder |= (cheat->flags & CHEAT_HIDDEN ? FOLDER_COLLAPSED : FOLDER_EXPANDED);

			switch(blocks[cheat->block].address & 0xFF) {
				case 0:
					folder |= FOLDER_SINGLE_SELECT;
					break;
				case 1:
					folder |= FOLDER_COMMENT;
					break;
				default:
					folder |= FOLDER_MULTI_SELECT;
					break;
			}

			return folder;
		}
	}

	return 0;
}

int cheat_is_visible(Cheat *cheat) {
	while(cheat->parent != NULL) {
		if(cheat->parent->flags & CHEAT_HIDDEN) {
			return 0;
		}

		cheat = cheat->parent;
	}

	return 1;
}

int cheat_folder_size(Cheat *cheat) {
	if(cheat != NULL) {
		if(cheat->length == 1 && ((blocks[cheat->block].address >> 24) == 0xCF)) {
			return blocks[cheat->block].value;
		}
	}

	return 0;
}

void cheat_folder_toggle(Cheat *folder) {
	if(cheat_is_folder(folder) & FOLDER_COLLAPSED) { // show
		folder->flags &= ~CHEAT_HIDDEN;
	} else { // hide
		folder->flags |= CHEAT_HIDDEN;
	}
}

void cheat_set_status(Cheat *cheat, u32 flags) {
	if(flags & (CHEAT_SELECTED | CHEAT_CONSTANT)) {
		// if value goes from CONSTANT > SELECTED and cheat engine off restore value (hacky fix)
		if(!cfg.cheat_status) {
			if((cheat->flags & CHEAT_CONSTANT) && (flags & CHEAT_SELECTED)) {
				cheat_backup_restore(cheat, CHEAT_RESTORE);
			}
		}

		// if cheat wasn't previously enabled set fresh
		if(!(cheat->flags & (CHEAT_SELECTED | CHEAT_CONSTANT))) {
			cheat->flags |= CHEAT_FRESH;
		}

		// unset selected and constant flags
		cheat->flags &= ~(CHEAT_SELECTED | CHEAT_CONSTANT);

		// set new flags
		cheat->flags |= flags;

		// disable all other cheats if single select folder
		if(cheat_is_folder(cheat->parent) & FOLDER_SINGLE_SELECT) {
			u32 index = cheat_get_index(cheat->parent) + 1;
			u32 size = cheat_folder_size(cheat->parent);
			int i;
			for(i = index; i < index + size; i++) {
				if(&cheats[i] != cheat && (cheats[i].flags & (CHEAT_SELECTED | CHEAT_CONSTANT))) {
					cheats[i].flags |= CHEAT_FRESH;
					cheats[i].flags &= ~(CHEAT_SELECTED | CHEAT_CONSTANT);
				}
			}
		}
	} else {
		// if cheat was previously enabled set fresh
		if(cheat->flags & (CHEAT_SELECTED | CHEAT_CONSTANT)) {
			cheat->flags |= CHEAT_FRESH;
		}

		// unset selected and constant flags
		cheat->flags &= ~(CHEAT_SELECTED | CHEAT_CONSTANT);
	}

	// reapply cheats
	cheat_apply(0);
}

int cheat_get_engine(Cheat *cheat) {
	if(cheat != NULL) {
		return (cheat->flags & (CHEAT_CWCHEAT | CHEAT_PSPAR | CHEAT_PSPAR_EXT));
	}

	return 0;
}

void cheat_set_engine(Cheat *cheat, int engine) {
	if(cheat != NULL) {
		cheat->flags &= ~(CHEAT_CWCHEAT | CHEAT_PSPAR | CHEAT_PSPAR_EXT);
		cheat->flags |= engine;
	}
}

char *cheat_get_engine_string(Cheat *cheat) {
	if(cheat != NULL) {
		switch(cheat->flags & (CHEAT_CWCHEAT | CHEAT_PSPAR | CHEAT_PSPAR_EXT)) {
			case CHEAT_CWCHEAT:
				if(sceKernelInitKeyConfig() == PSP_INIT_KEYCONFIG_POPS) {
					return "PSX GS";
				} else {
					return "CWCheat";
				}
				break;
			case CHEAT_PSPAR_EXT:
				return "PSPAR EXT";
				break;
			default:
				return "PSPAR";
				break;
		}
	}

	return NULL;
}

int cheat_get_level(Cheat *cheat) {
	if(cheat != NULL) {
		int level = 0;

		while(cheat->parent != NULL) {
			cheat = cheat->parent;
			level++;
		}

		return level;
	}

	return -1;
}

Cheat *cheat_insert(Cheat *cheat, int index) {
	if(cheat_total < cheat_max) {
		if(index < 0) {
			index = cheat_total;
		}

		// move the cheats down in the list
		memmove(&cheats[index + 1], &cheats[index], (cheat_total - index) * sizeof(Cheat));
		cheat_total++;

		// setup the new cheat
		Cheat *new_cheat = cheat_get(index);
		if(cheat != NULL) {
			new_cheat = &cheat;
			new_cheat->length = 0;
		} else {
			memset(new_cheat, 0, sizeof(Cheat));
		}

		Cheat *previous = cheat_get(index - 1);
		if(previous != NULL) {
			new_cheat->block = previous->block + previous->length;
			if(cheat_is_folder(previous)) {
				new_cheat->parent = previous;
			} else if(previous->parent) {
				new_cheat->parent = previous->parent;
			}
		}

		// update the parents
		Cheat *parent = new_cheat->parent;
		while(parent != NULL) {
			blocks[parent->block].value++;
			parent = parent->parent;
		}

		// update the cheat parents
		cheat_reparent(0, 0);

		return new_cheat;
	}

	return NULL;
}

/* Block functions */

Block *block_get(int index) {
	if(index >= 0 && index < block_total) {
		return &blocks[index];
	}

	return NULL;
}

Block *block_get_next(Block *block) {
	return block_get(block_get_index(block) + 1);
}

Block *block_get_previous(Block *block) {
	return block_get(block_get_index(block) - 1);
}

Block *block_add(Block *block) {
	Cheat *cheat = cheat_get(cheat_total - 1);
	return block_insert(cheat, block, cheat->length);
}

Block *block_insert(Cheat *cheat, Block *block, int index) {
	if(cheat != NULL) {
		if(cheat->length < 0xFF && block_total < block_max) {
			if(index > cheat->length) {
				index = cheat->length;
			}

			memmove(&blocks[cheat->block + index], &blocks[cheat->block + index - 1], (block_total - (cheat->block + index) + 1) * sizeof(Block));
			if(original_values != NULL) {
				memmove(&original_values[cheat->block + index], &original_values[cheat->block + index - 1], (block_total - (cheat->block + index) + 1) * sizeof(u32));
			}

			if(block) {
				blocks[cheat->block + index] = *block;
			} else {
				memset(&blocks[cheat->block + index], 0, sizeof(Block));
			}

			int i;
			for(i = cheat_get_index(cheat) + 1; i < cheat_total; i++) {
				cheats[i].block++;
			}

			cheat->length++;
			block_total++;

			return &blocks[cheat->block + index];
		}
	}

	return NULL;
}

Block *block_delete(Cheat *cheat, int index) {
	if(cheat != NULL) {
		if(cheat->length > 0) {
			if(index > cheat->length) {
				index = cheat->length;
			}

			memmove(&blocks[cheat->block + index - 1], &blocks[cheat->block + index], (block_total - (cheat->block + index) + 1) * sizeof(Block));
			if(original_values != NULL) {
				memmove(&original_values[cheat->block + index - 1], &original_values[cheat->block + index], (block_total - (cheat->block + index) + 1) * sizeof(u32));
			}

			memset(&blocks[block_total], 0, sizeof(Block));

			int i;
			for(i = cheat_get_index(cheat) + 1; i < cheat_total; i++) {
				cheats[i].block--;
			}

			cheat->length--;
			block_total--;
		}
	}

	return NULL;
}

int block_get_index(Block *block) {
	if(block != NULL) {
		return ((u32)block - (u32)blocks) / sizeof(Block);
	}

	return 0;
}

/* Patch functions */

void patch_apply(const char *path) {
	SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0777);

	if(fd > -1) {
		int length;
		int patch_address;

		// read the address to start the patch
		sceIoRead(fd, &patch_address, 4);

		// get the patch size
		length = sceIoLseek(fd, 0, SEEK_END);

		// seek back to the start of the file and apply the patch
		sceIoLseek(fd, 4, SEEK_SET);
		sceIoRead(fd, (void*)patch_address, length);

		sceIoClose(fd);
	}
}