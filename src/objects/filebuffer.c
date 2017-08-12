#include <stdio.h>
#include <string.h>
#include "common.h"

static ReadBuffer read_buffer;
static WriteBuffer write_buffer;

SceUID fileIoOpen(const char *file, int flags, SceMode mode) {
	SceUID fd = sceIoOpen(file, flags, mode);

	if(fd > -1) {
		if(flags & PSP_O_RDONLY) {
			if(read_buffer.fd <= 0) {
				read_buffer.buffer = kmalloc_align(0, PSP_SMEM_Low, FILE_BUFFER_SIZE, 64);
				if(read_buffer.buffer != NULL) {
					read_buffer.fd = fd;
					read_buffer.buffer_size = 0;
					read_buffer.buffer_offset = FILE_BUFFER_SIZE;
					read_buffer.file_offset = 0;
					fileIoRead(read_buffer.fd, NULL, 0);
				}
			}

			if(read_buffer.fd != fd) {
				sceIoClose(fd);
				fd = -1;
			}
		} else if(flags & PSP_O_WRONLY) {
			if(write_buffer.fd <= 0) {
				write_buffer.buffer = kmalloc_align(0, PSP_SMEM_Low, FILE_BUFFER_SIZE, 64);
				if(write_buffer.buffer != NULL) {
					write_buffer.fd = fd;
					write_buffer.buffer_offset = 0;
				}
			}

			if(write_buffer.fd != fd) {
				sceIoClose(fd);
				fd = -1;
			}
		}
	}

	return fd;
}

int fileIoLseek(SceUID fd, int offset, int whence) {
	if(fd == read_buffer.fd) {
		int seeked = 0;

		switch(whence) {
			case SEEK_SET:
				seeked = sceIoLseek32(fd, offset, whence);
				break;
			case SEEK_CUR:
				if(read_buffer.buffer_offset + offset < read_buffer.buffer_size) {
					read_buffer.buffer_offset += offset;
					read_buffer.file_offset += offset;
					return read_buffer.file_offset;
				}
				seeked = sceIoLseek32(fd, offset - (read_buffer.buffer_size - read_buffer.buffer_offset), SEEK_CUR) + (read_buffer.buffer_size - read_buffer.buffer_offset);
				break;
			case SEEK_END:
				seeked = sceIoLseek32(fd, offset, whence);
				break;
		}

		read_buffer.buffer_offset = FILE_BUFFER_SIZE;
		read_buffer.file_offset = seeked;
		return seeked;
	}

	return sceIoLseek32(fd, offset, whence);
}

int fileIoRead(SceUID fd, void *data, SceSize size) {
	if(fd == read_buffer.fd) {
		if(read_buffer.buffer_offset >= FILE_BUFFER_LIMIT) {
			if(read_buffer.buffer_offset < FILE_BUFFER_SIZE) {
				sceIoLseek(fd, (int)((FILE_BUFFER_SIZE - read_buffer.buffer_offset) * -1), SEEK_CUR);
			}

			read_buffer.buffer_size = sceIoRead(fd, read_buffer.buffer, FILE_BUFFER_SIZE);
			if(read_buffer.buffer_size < FILE_BUFFER_SIZE) {
				*(u8*)(read_buffer.buffer + read_buffer.buffer_size) = 0;
			}
			read_buffer.buffer_offset = 0;
		}
	
		memcpy(data, (void*)(read_buffer.buffer + read_buffer.buffer_offset), size);
		read_buffer.buffer_offset += size;
		read_buffer.file_offset += size;

		return (read_buffer.buffer_offset > read_buffer.buffer_size ? size - (read_buffer.buffer_offset - read_buffer.buffer_size) : size);
	}

	return sceIoRead(fd, data, size);
}

void *fileIoGet() {
	if(read_buffer.buffer_offset >= FILE_BUFFER_SIZE) {
		fileIoRead(read_buffer.fd, NULL, 0);
	}
	return (read_buffer.buffer + read_buffer.buffer_offset);
}

int fileIoSkipBlank(SceUID fd) {
	while(*(u8*)(read_buffer.buffer + read_buffer.buffer_offset) == ' ') {
		if(fileIoRead(fd, NULL, 1) != 1) {
			return 0;
		}
	}

	return 1;
}

int fileIoSkipWord(SceUID fd) {
	while(*(u8*)(read_buffer.buffer + read_buffer.buffer_offset) > (u8)' ') {
		if(fileIoRead(fd, NULL, 1) != 1) {
			return 0;
		}
	}

	return fileIoSkipBlank(fd);
}

int fileIoSkipLine(SceUID fd) {
	while(*(u8*)(read_buffer.buffer + read_buffer.buffer_offset) != '\r' && *(u8*)(read_buffer.buffer + read_buffer.buffer_offset) != '\n') {
		if(fileIoRead(fd, NULL, 1) != 1) {
			return 0;
		}
	}
	while(*(u8*)(read_buffer.buffer + read_buffer.buffer_offset) == '\r' || *(u8*)(read_buffer.buffer + read_buffer.buffer_offset) == '\n') {
		if(fileIoRead(fd, NULL, 1) != 1) {
			return 0;
		}
	}

	return 1;
}

int fileIoWrite(SceUID fd, const void *data, SceSize size) {
	if(fd == write_buffer.fd) {
		if(write_buffer.buffer_offset > FILE_BUFFER_LIMIT) {
			if(sceIoWrite(fd, write_buffer.buffer, write_buffer.buffer_offset) != write_buffer.buffer_offset) {
				return 0;
			}
			write_buffer.buffer_offset = 0;
		}

		if(size > FILE_BUFFER_SIZE) {
			if(sceIoWrite(fd, data, size) != size) {
				return 0;
			}
		} else {
			memcpy((void*)(write_buffer.buffer + write_buffer.buffer_offset), data, size);
			write_buffer.buffer_offset += size;
		}

		return size;
	}

	return sceIoWrite(fd, data, size);
}

int fileIoClose(SceUID fd) {
	if(fd == read_buffer.fd) {
		read_buffer.fd = -1;
		kfree(read_buffer.buffer);
	} else if(fd == write_buffer.fd) {
		if(write_buffer.buffer_offset > 0) {
			sceIoWrite(fd, write_buffer.buffer, write_buffer.buffer_offset);
		}
		write_buffer.fd = -1;
		kfree(write_buffer.buffer);
	}

	return sceIoClose(fd);
}