#ifndef CTRL_H
#define CTRL_H

#define CTRL_REPEAT_TIME 145000
#define CTRL_REPEAT_INTERVAL 9000

#define CTRL_REPEAT_TIME_QUICK 17000
#define CTRL_REPEAT_INTERVAL_QUICK 1

#define CTRL_REPEAT_INCREMENT 50000

enum CtrlFlags {
	PSP_CTRL_ANALOG_LEFT = 0x00010000,
	PSP_CTRL_ANALOG_RIGHT = 0x00020000,
	PSP_CTRL_ANALOG_UP = 0x00040000,
	PSP_CTRL_ANALOG_DOWN = 0x00080000,
	PSP_CTRL_SLIDE = 0x20000000
};

enum PSXCtrlFlags {
	PSX_CTRL_SELECT = 0x0100,
	PSX_CTRL_START = 0x0800,
	PSX_CTRL_TRIANGLE = 0x0010,
	PSX_CTRL_CIRCLE = 0x0020,
	PSX_CTRL_CROSS = 0x0040,
	PSX_CTRL_SQUARE = 0x0080,
	PSX_CTRL_UP = 0x1000,
	PSX_CTRL_RIGHT = 0x2000,
	PSX_CTRL_DOWN = 0x4000,
	PSX_CTRL_LEFT = 0x8000,
	PSX_CTRL_L1 = 0x0004,
	PSX_CTRL_L2 = 0x0001,
	PSX_CTRL_R1 = 0x0008,
	PSX_CTRL_R2 = 0x0002
};

u32 ctrl_read();
u32 ctrl_waitany(int repeat_time, int repeat_interval);
u32 ctrl_waitkey(int repeat_time, int repeat_interval, u32 keyw);
u32 ctrl_waitmask(int repeat_time, int repeat_interval, u32 keymask);
void ctrl_waitrelease();

#endif
