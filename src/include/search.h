#ifndef SEARCH_H
#define SEARCH_H

#define SEARCH_HISTORY_MAX 16
#define SEARCH_ADDRESSES_MAX 100

enum SearchFileTypes {
	SEARCH_FILE_NONE = 0,
	SEARCH_FILE_DUMP = 1,
	SEARCH_FILE_DAT = 2
};

typedef struct SearchHistoryItem {
	u32 value;
	u8 flags;
} SearchHistoryItem;

typedef struct Search {
	u32 results[SEARCH_ADDRESSES_MAX];
	struct SearchHistoryItem history[SEARCH_HISTORY_MAX];
	u32 num_history;
	u32 num_results;
	u32 max_results;
	u32 address_start;
	u32 address_end;
	char started;
	char number;
	char mode;
} Search;

void search_init();

void search_start(char search_mode);

void search_undo();

void search_reset(char remove_files, char reset_history);

void search_load_results();

Cheat *search_add_loaded_results(u8 flags);

Cheat *search_add_result(u32 address, u8 flags);

#endif