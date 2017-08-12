#ifndef FILEBUFFER_H
#define FILEBUFFER_H

#define FILE_BUFFER_LIMIT 8128
#define FILE_BUFFER_SIZE 8192

typedef struct ReadBuffer {
	SceUID fd;
	void *buffer;
	int buffer_size;
	int buffer_offset;
	u32 file_offset;
} ReadBuffer;

typedef struct WriteBuffer {
	SceUID fd;
	void *buffer;
	int buffer_offset;
	void *buffer_ptr;
} WriteBuffer;

/**
 * Allocates a temporary buffer for file data and opens a file.
 *
 * @param file Pointer to a string holding the name of the file to open.
 * @param type Libc styled flags that are or'ed together, use PSP_O_RDONLY for read buffer or PSP_O_WRONLY for write buffer (max 1 file per mode).
 * @param mode File access mode.
 * @return A non-negative integer is a valid fd, anything else an error.
 */
SceUID fileIoOpen(const char *file, int flags, SceMode mode);

/**
 * Reposition read file descriptor offset.
 *
 * @param fd Opened read file descriptor with which to seek.
 * @param size Amount to seek ahead in file.
 */
int fileIoLseek(SceUID fd, int offset, int whence);

int fileIoRead(SceUID fd, void *data, SceSize size);

void *fileIoGet();

int fileIoSkipBlank(SceUID fd);

int fileIoSkipWord(SceUID fd);

int fileIoSkipLine(SceUID fd);

int fileIoWrite(SceUID fd, const void *data, SceSize size);

int fileIoClose(SceUID fd);

#endif
