/*
 *  test_amplogic.c  —  host tests for AmpLogic (pin rules, debounce)
 */
#include <stdio.h>
#include <string.h>
#include "AmpLogic.h"

static int gFailures;
#define CHECK(cond) do { if (!(cond)) { \
    printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); gFailures++; } } while (0)

/* fake mac-io GPIO block */
static UInt8 regs[256];
static struct { UInt32 pin; UInt8 value; } writes[16];
static int nWrites;

static int nOtherPinAccess;     /* reads or writes outside 0x6F/0x70 */
static int OtherPin(UInt32 offset)
{
    return offset != kPinHeadphone && offset != kPinSpeaker;
}
static UInt8 fakeRead(UInt32 offset)
{
    if (OtherPin(offset)) nOtherPinAccess++;
    return regs[offset & 0xFF];
}
static void fakeWrite(UInt32 offset, UInt8 value)
{
    if (OtherPin(offset)) nOtherPinAccess++;
    regs[offset & 0xFF] = value;
    if (nWrites < 16) { writes[nWrites].pin = offset; writes[nWrites].value = value; }
    nWrites++;
}
static void resetRegs(UInt8 v6E, UInt8 v6F, UInt8 v70, UInt8 v79)
{
    memset(regs, 0, sizeof(regs));
    regs[0x6E] = v6E; regs[0x6F] = v6F; regs[0x70] = v70; regs[0x79] = v79;
    nWrites = 0;
    nOtherPinAccess = 0;
}

static void test_headphones_in_bit(void)
{
    CHECK(AmpHeadphonesIn(0x80) == 1);
    CHECK(AmpHeadphonesIn(0x82) == 0);
}

static void test_desired_table(void)
{
    CHECK(AmpDesired(kPinSpeaker, 0, 0) == kAmpOn);
    CHECK(AmpDesired(kPinSpeaker, 1, 0) == kAmpOff);
    CHECK(AmpDesired(kPinHeadphone, 0, 0) == kAmpOff);
    CHECK(AmpDesired(kPinHeadphone, 1, 0) == kAmpOn);
    CHECK(kAmpOn == 0x05 && kAmpOff == 0x04);
}

static void test_debounce_needs_two(void)
{
    AmpDebounce d;
    AmpDebounceInit(&d, 0x82);
    CHECK(AmpDebounceUpdate(&d, 0x80) == 0);
    CHECK(AmpDebounceUpdate(&d, 0x80) == 1);
}

static void test_single_glitch_ignored(void)
{
    AmpDebounce d;
    AmpDebounceInit(&d, 0x82);
    CHECK(AmpDebounceUpdate(&d, 0x80) == 0);
    CHECK(AmpDebounceUpdate(&d, 0x82) == 0);
    CHECK(AmpDebounceUpdate(&d, 0x82) == 0);
}

static void test_install_with_headphones_in(void)
{
    AmpDebounce d;
    resetRegs(0x07, 0x07, 0x07, 0x07);
    AmpDebounceInit(&d, 0x80);
    CHECK(d.stable == 1);
    CHECK(AmpApply(d.stable, 0, fakeRead, fakeWrite) == 1);
    CHECK(nWrites == 1);
    CHECK(writes[0].pin == kPinSpeaker && writes[0].value == kAmpOff);
}

static void test_apply_counts_only_mismatches(void)
{
    resetRegs(0x07, 0x04, 0x04, 0x04);
    CHECK(AmpApply(0, 0, fakeRead, fakeWrite) == 1);
    CHECK(nWrites == 1);
    CHECK(writes[0].pin == kPinSpeaker && writes[0].value == kAmpOn);
}

/* 0x6E and 0x79 aren't audio pins (the device tree lists only 0x67,
   0x6F and 0x70), and forcing them on breaks FireWire: never touch
   them, whatever state they're in */
static void test_leaves_6E_and_79_alone(void)
{
    int hp, silent;
    for (hp = 0; hp <= 1; hp++)
        for (silent = 0; silent <= 1; silent++) {
            resetRegs(0x04, 0x04, 0x04, 0x04);
            AmpApply(hp, silent, fakeRead, fakeWrite);
            CHECK(nOtherPinAccess == 0);
            CHECK(regs[0x6E] == 0x04 && regs[0x79] == 0x04);
            resetRegs(0x07, 0x07, 0x07, 0x07);
            AmpApply(hp, silent, fakeRead, fakeWrite);
            CHECK(nOtherPinAccess == 0);
            CHECK(regs[0x6E] == 0x07 && regs[0x79] == 0x07);
        }
}

static void test_silent_turns_both_outputs_off(void)
{
    int hp;
    for (hp = 0; hp <= 1; hp++) {
        CHECK(AmpDesired(kPinSpeaker, hp, 1) == kAmpOff);
        CHECK(AmpDesired(kPinHeadphone, hp, 1) == kAmpOff);
    }
}

static void test_unmute_restores_the_right_output(void)
{
    /* headphones out, all amps on: mute turns the speaker off ... */
    resetRegs(0x07, 0x04, 0x07, 0x07);
    CHECK(AmpApply(0, 1, fakeRead, fakeWrite) == 1);
    CHECK(writes[0].pin == kPinSpeaker && writes[0].value == kAmpOff);
    /* ... and unmuting turns it back on, headphone amp still off */
    nWrites = 0;
    regs[kPinSpeaker] = 0x04;
    CHECK(AmpApply(0, 0, fakeRead, fakeWrite) == 1);
    CHECK(writes[0].pin == kPinSpeaker && writes[0].value == kAmpOn);
}

int main(void)
{
    test_headphones_in_bit();
    test_desired_table();
    test_debounce_needs_two();
    test_single_glitch_ignored();
    test_install_with_headphones_in();
    test_apply_counts_only_mismatches();
    test_leaves_6E_and_79_alone();
    test_silent_turns_both_outputs_off();
    test_unmute_restores_the_right_output();
    if (gFailures) { printf("test_amplogic: %d failed\n", gFailures); return 1; }
    printf("test_amplogic: all passed\n");
    return 0;
}
