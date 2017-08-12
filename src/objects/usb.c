#include <pspkernel.h>
#include <pspusb.h>
#include <pspusbstor.h>
#include "common.h"

#ifdef _USB_

char usb_started = 0;
static char usb_module_status = 0;

void usb_connect(int enable) {
	if(enable == 2) {
		enable = !usb_started;
	}

	if(enable == 0) {
		if(usb_started) {
			sceUsbDeactivate(0x1c8);
			sceUsbStop(PSP_USBSTOR_DRIVERNAME, 0, 0);
			sceUsbStop(PSP_USBBUS_DRIVERNAME, 0, 0);
			sceIoDevctl("fatms0:", 0x0240D81E, NULL, 0, NULL, 0);
			usb_started = 0;
		}
	} else {
		if(!usb_module_status) {
			load_start_module("flash0:/kd/semawm.prx");
			load_start_module("flash0:/kd/usbstor.prx"); 
			load_start_module("flash0:/kd/usbstormgr.prx");
			load_start_module("flash0:/kd/usbstorms.prx");
			load_start_module("flash0:/kd/usbstorboot.prx");
			usb_module_status = 1;
		}

		sceUsbStart(PSP_USBBUS_DRIVERNAME, 0, 0);
		sceUsbStart(PSP_USBSTOR_DRIVERNAME, 0, 0);
		sceUsbstorBootSetCapacity(0x800000);
		sceUsbActivate(0x1C8);
		usb_started = 1;
	}
}

#endif