/*
 *  MacTypes.h  —  host stand-in for unit tests
 *  Just enough of the classic Mac types for the pure-logic modules.
 */
#pragma once
#include <stdint.h>

typedef uint8_t  UInt8;
typedef uint16_t UInt16;
typedef uint32_t UInt32;
typedef int16_t  SInt16;
typedef int32_t  SInt32;
typedef unsigned char Boolean;
typedef uint32_t OSType;

#ifndef true
#define true  1
#define false 0
#endif
