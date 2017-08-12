#include <pspinit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"

extern Language lang;
extern Colors colors;
extern char plug_path[];

FileBrowser *file_browser;

int filebrowser_display(char *path, char **ext, char ext_count) {
	u32 ctrl = 0, ret = 0;
	int i, sel = 0;

	// allocate memory for file names/vars etc.
	file_browser = kmalloc(0, PSP_SMEM_Low, sizeof(FileBrowser));
	if(file_browser == NULL) {
		return NULL;
	}

	// set current folder and cache files
	if(strchr(path, ':') != NULL) {
		strcpy(file_browser->path, path);
	} else if(strlen(path) == 0) {
		strcpy(file_browser->path, plug_path);
	} else {
		sprintf(file_browser->path, "%s/%s", plug_path, path);
	}

	filebrowser_cache(file_browser->path, ext, ext_count);

	if(file_browser->num_files > 0) {
		// clear the screen
		pspDebugScreenClear();

		while(ret == 0) {
			// heading
			line_print(0);
			pspDebugScreenSetTextColor(colors.color01);
			puts("  ");
			printf(lang.file_browser.title, file_browser->path, file_browser->num_files);
			line_print(-1);

			for(i = 0; i < file_browser->num_files; i++) {
				if((int)(i) < (int)(((sel)-14) - ((((int)(sel)+14) - ((int)file_browser->num_files))>0? abs(((int)(sel)+14) - ((int)file_browser->num_files)): 0))) {continue;}
				if((int)(i) > (int)(((sel)+14) + (((int)(sel)-14)<0? abs((int)((sel)-14)): 0))) {continue;}

				pspDebugScreenSetTextColor(i == sel ? colors.color01 : colors.color02);
				printf("  %s", file_browser->files[i].name);
				line_clear(-1);
			}

			// helper
			line_print(32);
			pspDebugScreenSetTextColor(colors.color01);
			printf(lang.helper.select_cancel);

			switch(ctrl = ctrl_waitmask(0, 0, PSP_CTRL_UP | PSP_CTRL_DOWN | PSP_CTRL_CROSS | PSP_CTRL_CIRCLE)) {
				case PSP_CTRL_UP:
					sel = (sel > 0 ? sel - 1 : file_browser->num_files - 1);
					break;
				case PSP_CTRL_DOWN:
					sel = (sel < file_browser->num_files - 1 ? sel + 1 : 0);
					break;
				case PSP_CTRL_CROSS:
					if(strcmp(file_browser->files[sel].name, "..") == 0) {
						*strrchr(file_browser->path, '/') = 0;
					} else {
						strcat(file_browser->path, "/");
						strcat(file_browser->path, file_browser->files[sel].name);

						if(!file_browser->files[sel].is_dir) {
							ret = ctrl;
						}
					}

					if(ret == 0) {
						filebrowser_cache(file_browser->path, ext, ext_count);
						sel = 0;
						pspDebugScreenClear();
					}
					break;
				case PSP_CTRL_CIRCLE:
					ret = ctrl;
					break;
			}
		}

		strcpy(path, file_browser->path);

		kfree(file_browser);
		ctrl_waitrelease();
		pspDebugScreenClear();

		return 0;
	}

	return -1;
}

void filebrowser_cache(char *path, char **ext, char ext_count) {
	file_browser->num_files = 0;

	SceUID dfd = sceIoDopen(path);

	if(dfd < 0) {
		*strrchr(path, '/') = 0;
		dfd = sceIoDopen(path);
	}

	if(dfd >= 0) {
		SceIoDirent sid;
		memset(&sid, 0, sizeof(SceIoDirent));

		while(sceIoDread(dfd, &sid) > 0) {
			int file_name_length = strlen(sid.d_name);
			int file_visible = 0;

			if(file_name_length < MAX_NAME_LEN) {
				if(sid.d_stat.st_attr & 0x18) {
					if(file_name_length > 1 || sid.d_name[0] != '.') {
						file_visible = 1;
					}
				} else {
					if(ext == NULL) {
						file_visible = 1;
					} else {
						int i;
						for(i = 0; i < ext_count; i++) {
							if(strcasecmp(&sid.d_name[file_name_length - 4], ext[i]) == 0) {
								file_visible = 1;
								break;
							}
						}
					}
				}

				if(!file_visible) {
					continue;
				}
			
				strcpy(file_browser->files[file_browser->num_files].name, sid.d_name);
				file_browser->files[file_browser->num_files].is_dir = ((sid.d_stat.st_attr & 0x18) > 0);

				if(++file_browser->num_files >= MAX_FILES) {
					break;
				}
			}
		}

		sceIoDclose(dfd);
	}
}