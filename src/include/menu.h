#ifndef MENU_H
#define MENU_H

#include <pspkerneltypes.h>
#include <pspdebug.h>

// functions
#define printf(...) pspDebugScreenKprintf(__VA_ARGS__)
#define puts pspDebugScreenPuts

// copy menu flags
#define COPY_ADDRESS       0x01
#define PASTE_ADDRESS      0x02
#define COPY_VALUE         0x04
#define PASTE_VALUE        0x08
#define COPY_PASTE_ADDRESS COPY_ADDRESS | PASTE_ADDRESS
#define COPY_PASTE_VALUE   COPY_VALUE | PASTE_VALUE
#define COPY_ALL		   COPY_ADDRESS | COPY_VALUE
#define PASTE_ALL		   PASTE_ADDRESS | PASTE_VALUE
#define COPY_PASTE_ALL     COPY_ADDRESS | PASTE_ADDRESS | COPY_VALUE | PASTE_VALUE

typedef struct BrowserDecoder {
	struct {
		s32 address;
		u32 X;
		u32 Y;
		u32 C;
	} decoder[10], browser[10];
	int no;
	u32 decoder_jump_address;
	char decoder_format;
	char browser_lines;
	char view;
} BrowserDecoder;

typedef struct Cheater {
	char show_game_name;
	char edit_format;
	Cheat *selected;
} Cheater;

typedef struct Copier {
	struct {
		u32 address;
		u32 value;
	} copier[10];
	int no;
	u32 address_start;
	u32 address_end;
	char toggle;
} Copier;

typedef struct Options {
	u8 force_pause;
	u8 dump_no;
	u8 cheat_index;
} Options;

typedef struct MenuState {
	char visible;
	BrowserDecoder bd;
	Cheater cheater;
	Options options;
	Copier copier;
	char guide_path[512];
} MenuState;

void *get_vram();
void menu_show();
void menu_init();
int get_menu_ctrl(int delay, int allow_empty);
u32 percentage_to_color(int percent);
void decode_str(char *str, char key);
void line_clear(int line);
void line_print(int line);
void line_cursor(int index, u32 color);
void show_error(char *error);
void layout_cheatmenu(Cheat *cheat);
void layout_copymenu(u32 *address, u32 *value, int flags);
void layout_cheatedit(Cheat *cheat);
void layout_searchsettings();
void layout_search(char show_search_mode);
void layout_textsearch();
void layout_systools();
void layout_modlist();
void layout_modinfo(SceUID uid);
void layout_thlist();
void layout_thinfo(SceUID uid);
void layout_umddumper();
int layout_tab();
void layout_heading();
u32 layout_cheats();
u32 layout_searcher();
u32 layout_options();
u32 layout_browser();
u32 layout_credits();
u32 layout_credits2();
u32 layout_guide();

#endif