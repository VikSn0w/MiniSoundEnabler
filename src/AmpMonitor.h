/*
 *  AmpMonitor.h  —  resident speaker/headphone switching and amp keep-alive
 */
#pragma once
#include <MacTypes.h>
#include "Status.h"

OSStatus AmpMonitorStart(MmAfStatus *status);   /* applies table now, arms timer */
