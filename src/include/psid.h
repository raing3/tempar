#ifndef PSID_H
#define PSID_H

#include <pspopenpsid.h>
#include <pspkerneltypes.h>

void psid_init();

void psid_corrupt();

int pspOpenPSIDGetOpenPSID(PspOpenPSID *openpsid);

int pspWlanGetEtherAddr(u8 *etherAddr);

#endif