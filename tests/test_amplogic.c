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

static UInt8 fakeRead(UInt32 offset) { return regs[offset & 0xFF]; }
static void fakeWrite(UInt32 offset, UInt8 value)
{
    regs[offset & 0xFF] = value;
    if (nWrites < 16) { writes[nWrites].pin = offset; writes[nWrites].value = value; }
    nWrites++;
}
static void resetRegs(UInt8 v6E, UInt8 v6F, UInt8 v70, UInt8 v79)
{
    memset(regs, 0, sizeof(regs));
    regs[0x6E] = v6E; regs[0x6F] = v6F; regs[0x70] = v70; regs[0x79] = v79;
    nWrites = 0;
}

static void test_headphones_in_bit(void)
{
    CHECK(AmpHeadphonesIn(0x80) == 1);
    CHECK(AmpHeadphonesIn(0x82) == 0);
}

static void test_desired_table(void)
{
    int x;
    CHECK(AmpDesired(kPinSpeaker, 0, 0) == kAmpOn);
    CHECK(AmpDesired(kPinSpeaker, 1, 0) == kAmpOff);
    CHECK(AmpDesired(kPinHeadphone, 0, 0) == kAmpOff);
    CHECK(AmpDesired(kPinHeadphone, 1, 0) == kAmpOn);
    for (x = 0; x <= 1; x++) {
        CHECK(AmpDesired(kPinAmp6E, x, 0) == kAmpOn);
        CHECK(AmpDesired(kPinAmp79, x, 0) == kAmpOn);
    }
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
    CHECK(AmpApply(0, 0, fakeRead, fakeWrite) == 2);
    CHECK(nWrites == 2);
    CHECK(writes[0].pin == kPinSpeaker && writes[0].value == kAmpOn);
    CHECK(writes[1].pin == kPinAmp79 && writes[1].value == kAmpOn);
}

static void test_silent_turns_both_outputs_off(void)
{
    int hp;
    for (hp = 0; hp <= 1; hp++) {
        CHECK(AmpDesired(kPinSpeaker, hp, 1) == kAmpOff);
        CHECK(AmpDesired(kPinHeadphone, hp, 1) == kAmpOff);
        CHECK(AmpDesired(kPinAmp6E, hp, 1) == kAmpOn);
        CHECK(AmpDesired(kPinAmp79, hp, 1) == kAmpOn);
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
    test_silent_turns_both_outputs_off();
    test_unmute_restores_the_right_output();
    if (gFailures) { printf("test_amplogic: %d failed\n", gFailures); return 1; }
    printf("test_amplogic: all passed\n");
    return 0;
}
