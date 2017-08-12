#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pspopenpsid.h>
#include "common.h"

#ifdef _PSID_

extern Config cfg;

PspOpenPSID psid;

static void psid_resolve(PspOpenPSID *dest) {
	int i;
	for(i = 0; i < sizeof(dest->data); i++) {
		dest->data[i] = (1103515245 * dest->data[i] + 12345) & 0xFF;
	}
}

void psid_init() {
	// patch the mac address
	if(cfg.mac_enable) {
		hook_export_bynid("sceWlan_Driver", "sceWlanDrv", 0x0C622081, pspWlanGetEtherAddr);
	}

	// open the psid file
	SceUID fd = sceIoOpen("psid.bin", PSP_O_RDONLY, 0777);
	if(fd > -1) {
		sceIoRead(fd, &psid, sizeof(PspOpenPSID));
		sceIoClose(fd);
	} else {
		sceOpenPSIDGetOpenPSID(&psid);
	}

	hook_export_bynid("sceOpenPSID_Service", "sceOpenPSID", 0xC69BEBCE, pspOpenPSIDGetOpenPSID);
	hook_export_bynid("sceOpenPSID_Service", "sceOpenPSID_driver", 0xC69BEBCE, pspOpenPSIDGetOpenPSID);
}

void psid_corrupt() {
	SceUID fd = sceIoOpen("psid.bin", PSP_O_WRONLY | PSP_O_CREAT, 0777);
	if(fd > -1) {
		psid_resolve(&psid);
		sceIoWrite(fd, &psid, sizeof(PspOpenPSID));
		sceIoClose(fd);
	}
}

int pspOpenPSIDGetOpenPSID(PspOpenPSID *openpsid) {
	memcpy(openpsid, &psid, sizeof(PspOpenPSID));
	return 0;
}

int pspWlanGetEtherAddr(u8 *etherAddr) {
	memcpy(etherAddr, cfg.mac, 6);
	return 0;
}

#endif