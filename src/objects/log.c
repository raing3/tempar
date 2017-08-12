#include <stdio.h>
#include <stdlib.h>

#ifdef _DEBUG_

void _log(char *fmt, ...) {
	va_list list;
	char data[1024];	

	va_start(list, fmt);
	vsnprintf(data, 1024, fmt, list);
	va_end(list);

	_log_file(data);
	//_log_psplink(data);
}

void _log_file(char *data) {
	SceUID fd = sceIoOpen("ms0:/tempar_log.txt", PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
	if(fd > -1) {
		sceIoWrite(fd, data, strlen(data));
		sceIoClose(fd);
	}
}

void _log_psplink(char *data) {
	SceUID fd = sceKernelStdout();
    sceIoWrite(fd, data, strlen(data));
}

#endif