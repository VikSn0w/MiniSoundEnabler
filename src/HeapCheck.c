/*
 *  HeapCheck.c  —  is an address inside a heap zone? (no Toolbox calls)
 */
#include "HeapCheck.h"

Boolean AddressInRange(const void *p, const void *start, const void *limit)
{
    const char *c = (const char *)p;
    return c >= (const char *)start && c < (const char *)limit;
}
