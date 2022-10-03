#ifndef CHEAT_H
#define CHEAT_H

#include <pspkerneltypes.h>

#define CHEAT_BACKUP      0x01
#define CHEAT_RESTORE     0x02
#define CHEAT_TOGGLE_ALL  0x04

#define CHEAT_REAL_ADDRESS 0x10

/**
 * Cheat flags.
*/
enum CheatFlags {
	/* *Cheat folder is hidden (collapsed).*/
	CHEAT_HIDDEN = 0x01,
	/* *Cheat was recently activated and has yet to be applied.*/
	CHEAT_FRESH = 0x02,
	/* *Cheat is enabled/disabled with trigger key.*/
	CHEAT_SELECTED = 0x04,
	/* *Cheat is always on.*/
	CHEAT_CONSTANT = 0x08,
	/* *Cheat uses CWCheat code types.*/
	CHEAT_CWCHEAT = 0x10,
	/* *Cheat uses PSPAR code types.*/
	CHEAT_PSPAR = 0x20,
	/* *Cheat uses PSPAR extended (NitroHax) code types.*/
	CHEAT_PSPAR_EXT = 0x40
};

/**
 * Folder flags.
*/
enum FolderFlags {
	/* *Folder is expanded.*/
	FOLDER_EXPANDED = 0x01,
	/* *Folder is collapsed.*/
	FOLDER_COLLAPSED = 0x02,
	/* *Folder is a single-select folder.*/
	FOLDER_SINGLE_SELECT = 0x10,
	/* *Folder is a comment folder.*/
	FOLDER_COMMENT = 0x20,
	/* *Folder is a multi-select folder.*/
	FOLDER_MULTI_SELECT = 0x40,
	/* *Folder status flags.*/
	FOLDER_STATUSES = (FOLDER_EXPANDED | FOLDER_COLLAPSED),
	/* *Folder type flags.*/
	FOLDER_TYPES = (FOLDER_SINGLE_SELECT | FOLDER_COMMENT | FOLDER_MULTI_SELECT)
};

/**
 *Game header of PSPAR code file.
*/
typedef struct ARGameHeader {
	/* *Size of the game entry in the code file, 0 for last game entry (multiply by 4 to get actual size in bytes).*/
	u16 game_size;
	/* *Size of the previous game entry in the code file, 0 for first game entry (multiply by 4 to get actual size in bytes).*/
	u16 prev_game_size;
	/* *Size of the game header in bytes.*/
	u8 header_size;
	/* *Number of items (cheats/folders/comments) in the game entry.*/
	u16 item_count;
	/* *Game ID of the game entry.*/
	char game_id[11];
} __attribute__((__packed__)) ARGameHeader;

/**
 * Item header of PSPAR code file.
*/
typedef struct ARItemHeader {
	/* *Number of code lines in the item entry.*/
	u8 code_count;
	/* *Length of the item name in bytes (excludes padding).*/
	u8 name_len;
	/* *Full length of the item name (includes padding) (subtract 1 and multiply by 4 to get actual size in bytes).*/
	u8 name_len_full;
	/* *Unknown */
	u8 unk;
} __attribute__((__packed__)) ARItemHeader;

/**
 * Cheat structure.
*/
typedef struct Cheat {
	/* *Displayed name of the cheat.*/
	char name[32];
	/* *Pointer to parent folder of the cheat.*/
	struct Cheat *parent;
	/* *Start index of code lines in cheat.*/
	u16 block;
	/* *Number of code lines in cheat.*/
	u8 length;
	/* *Cheat flags (one or more of CheatFlags or'ed together).*/
	u8 flags;
} Cheat;

/**
 * Block structure.
*/
typedef struct Block {
	/* *Address of the code.*/
	u32 address;
	/* *Value of the code.*/
	u32 value;
	#ifdef _NITEPR_
	u8 flags;
	#endif
} Block;

// TODO
u32 real_address(u32 address);

// TODO
u32 address(u32 address);

/**
 * Loads the value from a memory address.
*
 * @param address Address to read from.
 * @param type 0 for 8-bit, 1 for 16-bit, 2 for 32-bit. OR with CHEAT_REAL_ADDRESS to allow reading from address outside of user memory.
 * @return The value stored at the address.
*/
u32 address_load(u32 address, u8 type);

/**
 * Stores a value to a memory address.
*
 * @param value Value to store.
 * @param address Address to write to.
 * @param type 0 for 8-bit, 1 for 16-bit, 2 for 32-bit. OR with CHEAT_REAL_ADDRESS to allow reading from address outside of user memory.
*/
void address_set(u32 value, u32 address, u8 type);

// TODO
void memory_copy(u32 to, u32 from, u32 bytes);

/**
 * Executes the enabled cheats from the cheat list.
*
 * @param action TODO
*/
void cheat_apply(int action);

/**
 * Executes a cheat using the PSPAR/PSPAR_EXT cheat engine.
*
 * @param cheat Pointer to a cheat.
*/
void cheat_apply_pspar(Cheat *cheat);

// TODO
void cheat_apply_cwcheat(Cheat *cheat);

/**
 * Executes a cheat using the PSX GameShark cheat engine.
*
 * @param cheat Pointer to a cheat.
*/
void cheat_apply_psx_gs(Cheat *cheat);

// TODO
void cheat_pad_update();

/**
 * Performs a backup or restore of memory writen to by a cheat.
*
 * @param cheat Point to a cheat.
 * @param action TODO
*/
void cheat_backup_restore(Cheat *cheat, int action);

/**
 * Loads the cheats and blocks from a file.
*
 *@param game_id ID of the currently loaded game.
 *@param dbnum Number of the database to load from.
 *@param index Index to load cheats from the database.
*/
void cheat_load(const char *game_id, char dbnum, char index);

// TODO
void cheat_load_db(const char *file, const char *game_id, char index);

// TODO
void cheat_load_bin(const char *file, const char *game_id, char index);

// TODO
void cheat_load_npr(const char *file);

/**
 * Saves the cheats and blocks to a file.
*
 * @param game_id ID of the currently loaded game.
*/
void cheat_save(const char *game_id);

// TODO
int gameid_matches(const char *id1, const char *id2);

// TODO
char *gameid_get(char force_refresh);

/**
 * Allocates cheats and blocks.
 *
 * @param num_cheats The number of cheats to try to allocate.
 * @param num_blocks The number of blocks to try to allocate.
 * @param auto_off Enable cheat backup/restore functionality.
 * @return 0 on success, -1 if unable to allocate the cheats or blocks.
*/
int cheat_init(int num_cheats, int num_blocks, int auto_off);

/**
 * Deallocates cheats and blocks and resets associated variables.
*/
void cheat_deinit();

/**
 * Gets a cheat from the cheat list.
 *
 * @param index The index of the cheat to return.
 * @return Pointer to the cheat if it exists, otherwise NULL.
*/
Cheat *cheat_get(int index);

/**
 * Gets the next cheat from the cheat list.
 *
 * @param cheat Pointer to a cheat in the cheat list.
 * @return Pointer to the next cheat in the cheat list if it exists, otherwise NULL.
*/
Cheat *cheat_get_next(Cheat *cheat);

/**
 * Gets the previous cheat from the cheat list.
 *
 * @param cheat Pointer to a cheat in the cheat list.
 * @return Pointer to the previous cheat in the cheat list if it exists, otherwise NULL.
*/
Cheat *cheat_get_previous(Cheat *cheat);

/**
 * Adds a cheat to the end of the cheat list.
 *
 * @param cheat Pointer to the cheat to add to the cheat list, or NULL to add an empty cheat.
 * @return Pointer to the newly added cheat on success, otherwise NULL.
*/
Cheat *cheat_add(Cheat *cheat);

/**
 * Adds a cheat (folder) to the end of the cheat list.
 *
 * @param name Pointer to a string holding the name of the folder.
 * @param type The type of the folder.
 * @return Pointer to the newly added cheat on success, otherwise NULL.
*/
Cheat *cheat_add_folder(char *name, u32 type);

/**
 * Adds a cheat (and blocks) to the end of the cheat list.
 *
 * @param address The address to use for the blocks.
 * @param value The value to use for the blocks.
 * @param length The number of blocks to add.
 * @param flags The flags to use for the cheat, ie. cheat engine to use.
 * @param size The bit type to use for the cheats, 1 for u8, 2 for u16, 4 for u32.
 * @return Pointer to the newly added cheat on success, otherwise NULL.
*/
Cheat *cheat_new(int index, u32 address, u32 value, u8 length, u8 flags, u32 size);

/**
 * Adds a cheat (and blocks) from memory to the end of the cheat list.
 * @param address_start The memory address to start adding blocks from.
 * @param address_end The memory address to finish ending blocks at.
 * @return Pointer to the newly added cheat on success, otherwise NULL.
*/
Cheat *cheat_new_from_memory(u32 address_start, u32 address_end);

/**
 * Gets the next visible cheat (cheat not in a collapsed folder) from the cheat list.
 *
 * @param cheat Pointer to a cheat in the cheat list.
 * @return Pointer to the next visible cheat in the cheat list if it exists, otherwise NULL.
*/
Cheat *cheat_next_visible(Cheat *cheat);

/**
 * Gets the previous visible cheat (cheat not in a collapsed folder) from the cheat list.
 *
 * @param cheat Pointer to a cheat in the cheat list.
 * @return Pointer to the previous visible cheat in the cheat list if it exists, otherwise NULL.
*/
Cheat *cheat_previous_visible(Cheat *cheat);

// TODO
void cheat_collapse_folders(int collapse);

// TODO
void cheat_reparent(int start, int end);

/**
 * Deletes a cheat (and it's blocks) from the cheat list.
 *
 * @param cheat Pointer to a cheat in the cheat list.
 * @return 0 on success, -1 if unable to delete the cheat.
*/
void cheat_delete(Cheat *cheat);

/**
 * Copies a cheat (and it's blocks) to the end of the cheat list.
 *
 * @param cheat Pointer to a cheat in the cheat list.
 * @return Pointer to the newly added cheat on success, otherwise NULL.
*/
Cheat *cheat_copy(Cheat *cheat);

// TODO
void cheat_delete_range(int start, int number);

// TODO
Cheat *cheat_copy_range(int start, int number);

/**
 * Gets the index of a cheat from the cheat list.
 *
 * @param cheat Pointer to a cheat in the cheat list.
 * @return Index of the cheat in the cheat list, -1 on error.
*/
int cheat_get_index(Cheat *cheat);

// TODO
int cheat_exists(Cheat *cheat);

/**
 * Sets the name of a cheat.
 *
 * @param cheat Pointer to a cheat.
 * @param name New name of the cheat.
 * @param name_length Length of the name.
 * @return 0 on success, -1 on error.
*/
int cheat_set_name(Cheat *cheat, char *name, int name_length);

/**
 * Gets the folder type and status.
 *
 * @param cheat Pointer to a cheat.
 * @return Folder flags on success, 0 on error.
 * @see FolderFlags
*/
int cheat_is_folder(Cheat *cheat);

int cheat_is_visible(Cheat *cheat);

/**
 * Gets the number of sub-items in a folder.
 *
 * @param cheat Pointer to a cheat.
 * @return Number of items in a folder on success, 0 on error.
*/
int cheat_folder_size(Cheat *cheat);

// TODO
void cheat_folder_toggle(Cheat *folder);

// TODO
void cheat_set_status(Cheat *cheat, u32 flags);

/**
 * Gets the cheat engine used by a cheat.
 *
 * @param cheat Pointer to a cheat.
 * @return Cheat engine used by a cheat on success, NULL on error.
 * @see CheatFlags
*/
int cheat_get_engine(Cheat *cheat);

void cheat_set_engine(Cheat *cheat, int engine);

/**
 * Gets a string of the cheat engine used by a cheat.
 *
 * @param cheat Pointer to a cheat.
 * @return Pointer to a string holding the cheat engine string on success, NULL on error.
*/
char *cheat_get_engine_string(Cheat *cheat);

int cheat_get_level(Cheat *cheat);

Cheat *cheat_insert(Cheat *cheat, int index);

void cheat_unparent(Cheat *cheat);

/**
 * Gets a block from the block list.
 *
 * @param index The index of the block to return.
 * @return Pointer to the block if it exists, otherwise NULL.
*/
Block *block_get(int index);

/**
 * Gets the next block from the block list.
 *
 * @param block Pointer to a block in the block list.
 * @return Pointer to the next block in the block list if it exists, otherwise NULL.
*/
Block *block_get_next(Block *block);

/**
 * Gets the previous block from the block list.
 *
 * @param block Pointer to a block in the block list.
 * @return Pointer to the previous block in the block list if it exists, otherwise NULL.
*/
Block *block_get_previous(Block *block);

/**
 * Adds a block to the end of the block list.
 *
 * @param block Pointer to the block to add to the block list, or NULL to add an empty block.
 * @return Pointer to the newly added block on success, otherwise NULL.
*/
Block *block_add(Block *block);

// TODO
Block *block_insert(Cheat *cheat, Block *block, int index);

Block *block_delete(Cheat *cheat, int index);

/**
 * Gets the index of a block from the block list.
 *
 * @param cheat Pointer to a block in the block list.
 * @return Index of the block in the block list, -1 on error.
*/
int block_get_index(Block *block);

void patch_apply(const char *path);

#endif