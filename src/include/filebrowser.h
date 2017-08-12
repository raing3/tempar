#ifndef FILEBROWSER_H
#define FILEBROWSER_H

#define MAX_FILES 500
#define MAX_NAME_LEN 49

typedef struct FileBrowser {
	int num_files;
	struct {
		char is_dir;
		char name[MAX_NAME_LEN];
	} files[MAX_FILES];
	char path[512];
} FileBrowser;

// TODO
int filebrowser_display(char *path, char **ext, char ext_count);

// TODO
void filebrowser_cache(char *path, char **ext, char ext_count);

// TODO
void filebrowser_updir(char *path);

#endif