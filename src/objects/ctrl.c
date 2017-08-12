/* 
 * Copyright (C) 2006 aeolusc <soarchin@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <pspkernel.h>
#include <pspctrl.h>
#include "common.h"

// menu stuff
extern Config cfg;
extern MenuState menu;
extern int resume_count;

static u32 last_btn = 0;
static u32 repeat_count = 0;
static u32 key = 0;

static void ctrl_delay(int repeat_time, int repeat_interval) {
	if(menu.visible) {
		if(repeat_time == 0) {
			repeat_time = CTRL_REPEAT_TIME;
		}
		if(repeat_interval == 0) {
			repeat_interval = CTRL_REPEAT_INTERVAL;
		}

		sceKernelDelayThread(repeat_time - (repeat_interval * repeat_count));

		if(key == last_btn) {
			if(repeat_count < 12) {
				repeat_count++;
			}
		} else if(key == 0) {
			repeat_count = 0;
		}

		last_btn = key;
	}
}

u32 ctrl_read() {
	// read the buttons
	SceCtrlData ctl;
	sceCtrlPeekBufferPositive(&ctl, 1);

	if(ctl.Buttons & PSP_CTRL_HOME || resume_count != scePowerGetResumeCount()) {
		menu.visible = 0;
		resume_count = scePowerGetResumeCount();
	}

	// do screenshot
	#ifdef _SCREENSHOT_
	//if(ctl.Buttons & cfg.screen_key && cfg.screen_key) {
	//	screenshot(0);
	//}
	#endif

	// add in fake buttons
	u32 fake_pad = 0;
	ctl.Buttons &= 0xF0FFFF;

	if(cfg.swap_xo) {
		if(ctl.Buttons & PSP_CTRL_CROSS) {
			ctl.Buttons &= ~PSP_CTRL_CROSS;
			fake_pad |= PSP_CTRL_CIRCLE;
		}

		if(ctl.Buttons & PSP_CTRL_CIRCLE) {
			ctl.Buttons &= ~PSP_CTRL_CIRCLE;
			fake_pad |= PSP_CTRL_CROSS;
		}
	}

	if(ctl.Lx < 50)  fake_pad |= PSP_CTRL_ANALOG_LEFT;
	if(ctl.Lx > 200) fake_pad |= PSP_CTRL_ANALOG_RIGHT;
	if(ctl.Ly < 50)  fake_pad |= PSP_CTRL_ANALOG_UP;
	if(ctl.Ly > 200) fake_pad |= PSP_CTRL_ANALOG_DOWN;
	if(!menu.visible) fake_pad |= PSP_CTRL_CIRCLE;

	ctl.Buttons |= fake_pad;

	return ctl.Buttons;
}

u32 ctrl_waitany(int repeat_time, int repeat_interval) {
	ctrl_delay(repeat_time, repeat_interval);

	while((key = ctrl_read()) == 0) {
		sceKernelDelayThread(1000);
		repeat_count = 0;
	}

	return key;
}

u32 ctrl_waitkey(int repeat_time, int repeat_interval, u32 keyw) {
	ctrl_delay(repeat_time, repeat_interval);

	while((key = (ctrl_read() & keyw)) != keyw)	{
		sceKernelDelayThread(1000);
		repeat_count = 0;
	}

	return key;
}

u32 ctrl_waitmask(int repeat_time, int repeat_interval, u32 keymask) {
	ctrl_delay(repeat_time, repeat_interval);

	while(((key = ctrl_read()) & keymask) == 0)	{
		sceKernelDelayThread(1000);
		repeat_count = 0;
	}

	return key;
}

void ctrl_waitrelease() {
	if(menu.visible) {
		SceCtrlData ctl;
		do {
			sceKernelDelayThread(1000);
			sceCtrlPeekBufferPositive(&ctl, 1);
		} while((ctl.Buttons & 0xF1FFFF) != 0);
	}
}