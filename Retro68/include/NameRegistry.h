/*
 *  NameRegistry.h  —  minimal stand-in for Retro68
 *  The Multiversal Interfaces have no Name Registry header; these
 *  declarations follow Apple's Universal Interfaces and link against
 *  Retro68's NameRegistryLib import library.
 */
#pragma once
#include <MacTypes.h>

#ifndef __OSSTATUS_DEFINED
#define __OSSTATUS_DEFINED
typedef SInt32 OSStatus;
#endif

enum { gestaltNameRegistryVersion = 'nreg' };

typedef struct RegEntryID { UInt32 contents[4]; } RegEntryID;
typedef struct OpaqueRegEntryIter *RegEntryIter;
typedef UInt32 RegEntryIterationOp;
typedef UInt32 RegPropertyValueSize;
typedef char RegPropertyName;

enum {
    kRegIterContinue    = 0x1,
    kRegIterRoot        = 0x2,
    kRegIterParents     = 0x3,
    kRegIterChildren    = 0x4,
    kRegIterSubTrees    = 0x5,
    kRegIterDescendants = 0x5,
    kRegIterSibling     = 0x6
};

OSStatus RegistryEntryIterateCreate(RegEntryIter *cookie);
OSStatus RegistryEntryIterateDispose(RegEntryIter *cookie);
OSStatus RegistryEntrySearch(RegEntryIter *cookie,
                             RegEntryIterationOp relationship,
                             RegEntryID *foundEntry, Boolean *done,
                             const RegPropertyName *propertyName,
                             const void *propertyValue,
                             RegPropertyValueSize propertyValueSize);
OSStatus RegistryPropertyGet(const RegEntryID *entryID,
                             const RegPropertyName *propertyName,
                             void *propertyValue,
                             RegPropertyValueSize *propertySize);
