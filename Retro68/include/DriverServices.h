/*
 *  DriverServices.h  —  minimal stand-in for Retro68
 *  Only what the extension uses; links against DriverServicesLib.
 */
#pragma once
#include <MacTypes.h>

#ifndef __OSSTATUS_DEFINED
#define __OSSTATUS_DEFINED
typedef SInt32 OSStatus;
#endif

void SynchronizeIO(void);
