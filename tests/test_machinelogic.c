#include <stdio.h>
#include <string.h>
#include "MachineLogic.h"

static int failures;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL line %d: %s\n", __LINE__, #cond); failures++; } } while (0)

static Boolean Is(const char *rom, const char *root)
{
    return MachineIsMacMiniG4(rom, rom ? (UInt32)strlen(rom) + 1 : 0,
                              root, root ? (UInt32)strlen(root) + 1 : 0);
}

int main(void)
{
    /* Mac mini G4 under MacOS9Lives: root spoofed, boot ROM real */
    CHECK(Is("Apple PowerMac10,1 4.8.9f4 BootROM built on 03/23/05 at 14:22:23",
             "PowerMac5,1"));
    CHECK(Is("Apple PowerMac10,2 4.9.0f1 BootROM", "PowerMac5,1"));
    /* root not spoofed */
    CHECK(Is(NULL, "PowerMac10,1"));
    /* G4 Cube */
    CHECK(!Is("Apple PowerMac5,1 4.2.8f1 BootROM", "PowerMac5,1"));
    /* QEMU mac99 (no boot-rom node) */
    CHECK(!Is(NULL, "PowerMac3,1"));
    /* nothing readable */
    CHECK(!Is(NULL, NULL));
    /* other machines that start the same way */
    CHECK(!Is("Apple PowerMac1,1", "PowerMac1,1"));
    CHECK(!Is("PowerMac10,", NULL));          /* no digit */
    CHECK(!Is("PowerMac11,2", "PowerMac11,2"));
    /* length is honoured: match must lie inside the given bytes */
    CHECK(!MachineIsMacMiniG4("PowerMac10,1", 11, NULL, 0));
    CHECK(MachineIsMacMiniG4("PowerMac10,1", 12, NULL, 0));

    if (failures) return 1;
    printf("test_machinelogic: all passed\n");
    return 0;
}
