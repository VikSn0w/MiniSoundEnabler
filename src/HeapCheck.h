/*
 *  HeapCheck.h  —  is an address inside a heap zone? (no Toolbox calls)
 *
 *  Used to confirm the INIT's globals were placed in the system heap
 *  before anything that runs after startup depends on them.
 */
#pragma once
#include <MacTypes.h>

Boolean AddressInRange(const void *p, const void *start, const void *limit);
