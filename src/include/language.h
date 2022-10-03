#ifndef LANGUAGE_H
#define LANGUAGE_H

typedef struct language_info {
	char *name;
	char *author;
} language_info;

typedef struct language_misc {
	char *_true;
	char *_false;
	char *on;
	char *off;
	char *y;
	char *n;
	char *start;
	char *stop;
	char *address;
	char *value;
	char *hex;
	char *dec;
	char *ascii;
	char *_float;
	char *text;
	char *opcode;
	char *args;
	char *mode;
	char *bit;
	char *not_supported;
	char *working;
	char *done;
	char *task_progress;
	char *task_aborted;
} language_misc;

typedef struct language_tabs {
	char *cheater;
	char *searcher;
	char *prx;
	char *browser;
	char *decoder;
	char *cheats_on;
	char *cheats_off;
} language_tabs;

typedef struct language_cheat_menu {
	char *edit_cheat;
	char *rename_cheat;
	char *delete_cheat;
	char *copy_cheat;
	char *insert_cheat;
} language_cheat_menu;

typedef struct language_copy_menu {
	char *copy_address;
	char *paste_address;
	char *copy_value;
	char *paste_value;
	char *copy_to_new_cheat;
	char *copier_num;
} language_copy_menu;

typedef struct language_cheater {
	char *help;
	char *bat;
	char *temp;
	char *cheats;
} language_cheater;

typedef struct language_cheat_edit {
	char *title;
	char *help;
} language_cheat_edit;

typedef struct language_options {
	char *help_increment;
	char *pause_game;
	char *dump_ram;
	char *dump_kmem;
	char *real_addressing;
	char *cheat_hz;
	char *hijack_pspar_buttons;
	char *hb_all_pbps;
	char *load_color_file;
	char *max_text_viewer_lines;
	char *swap_xo;
	char *usb;
	char *corrupt_psid;
	char *load_memory_patch;
	char *change_language;
	char *change_menu_key;
	char *change_trigger_key;
	char *change_screenshot_key;
	char *max_cheats;
	char *max_blocks;
	char *enable_autooff;
	char *save_cheats;
	char *load_cheats;
	char *reset_settings;
	char *save_settings;
	char *system_tools;
	char *kill_tempar;
	char *press_buttons;
} language_options;

typedef struct language_searcher {
	char *find_exact_value;
	char *find_unknown_value;
	char *find_text;
	char *search_settings;
	char *reset_search;
	char *search_history;
	char *no_search_history;
} language_searcher;

typedef struct language_search_modes {
	char *same;
	char *different;
	char *greater;
	char *less;
	char *inc_by;
	char *dec_by;
	char *equal_to;
	char *not_equal_to;
	char *greater_than;
	char *less_than;
} language_search_modes;

typedef struct language_search_edit {
	char *title;
	char *help;
	char *search;
	char *undo_search;
	char *add_results;
	char *results_count;
	char *no_results;
} language_search_edit;

typedef struct language_search_text {
	char *title;
	char *help;
} language_search_text;

typedef struct language_search_settings {
	char *title;
	char *help;
	char *message;
	char *max_results;
} language_search_settings;

typedef struct language_browser_disassembler {
	char *help;
} language_browser_disassembler;

typedef struct language_system_tools {
	char *title;
	char *module_list;
	char *thread_list;
	char *umd_dumper;
} language_system_tools;

typedef struct language_module_list {
	char *title;
	char *heading;
} language_module_list;

typedef struct language_module_info {
	char *title;
	char *help;
} language_module_info;

typedef struct language_thread_list {
	char *title;
	char *heading;
} language_thread_list;

typedef struct language_thread_info {
	char *title;
	char *help;
} language_thread_info;

typedef struct language_umd_dumper {
	char *title;
	char *dump_umd;
	char *message;
} language_umd_dumper;

typedef struct language_file_browser {
	char *title;
} language_file_browser;

typedef struct language_helper {
	char *select_cancel;
} language_helper;

typedef struct language_errors {
	char *error;
	char *storage_full;
	char *memory_full;
	char *no_game_loaded;
	char *file_exists;
	char *file_cant_open;
	char *file_cant_read;
	char *file_cant_write;
} language_errors;

typedef struct Language {
	struct language_info					info;
	struct language_misc					misc;
	struct language_tabs					tabs;
	struct language_cheat_menu				cheat_menu;
	struct language_copy_menu				copy_menu;
	struct language_cheater					cheater;
	struct language_cheat_edit				cheat_edit;
	struct language_options					options;
	struct language_searcher				searcher;
	struct language_search_modes			search_modes;
	struct language_search_edit				search_edit;
	struct language_search_text				search_text;
	struct language_search_settings			search_settings;
	struct language_browser_disassembler	browser_disassembler;
	struct language_system_tools			system_tools;
	struct language_module_list				module_list;
	struct language_module_info				module_info;
	struct language_thread_list				thread_list;
	struct language_thread_info				thread_info;
	struct language_umd_dumper				umd_dumper;
	struct language_file_browser			file_browser;
	struct language_helper					helper;
	struct language_errors					errors;
} Language;

void language_init();
void language_deinit();
int language_load();
int language_load_file(const char *file);
void font_patch();

#endif