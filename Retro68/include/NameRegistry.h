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
typedef char RegCStrPathName;

/* e.g. RegistryCStrEntryLookup(NULL, "Devices:device-tree", &root) */
OSStatus RegistryCStrEntryLookup(const RegEntryID *searchPointID,
                                 const RegCStrPathName *pathName,
                                 RegEntryID *foundEntry);
OSStatus RegistryPropertyGetSize(const RegEntryID *entryID,
                                 const RegPropertyName *propertyName,
                                 RegPropertyValueSize *propertySize);
OSStatus RegistryPropertyGet(const RegEntryID *entryID,
                             const RegPropertyName *propertyName,
                             void *propertyValue,
                             RegPropertyValueSize *propertySize);

/* Walking the tree and listing properties */
typedef UInt32 RegPathNameSize;
typedef struct OpaqueRegPropertyIter *RegPropertyIter;
enum { kRegMaximumPropertyNameLength = 31 };

OSStatus RegistryEntryIterate(RegEntryIter *cookie,
                              RegEntryIterationOp relationship,
                              RegEntryID *foundEntry, Boolean *done);
OSStatus RegistryEntryIDDispose(RegEntryID *id);
OSStatus RegistryCStrEntryToPath(const RegEntryID *entryID,
                                 RegCStrPathName *pathName,
                                 RegPathNameSize pathSize);
OSStatus RegistryPropertyIterateCreate(const RegEntryID *entry,
                                       RegPropertyIter *cookie);
OSStatus RegistryPropertyIterateDispose(RegPropertyIter *cookie);
OSStatus RegistryPropertyIterate(RegPropertyIter *cookie,
                                 RegPropertyName *foundProperty,
                                 Boolean *done);
