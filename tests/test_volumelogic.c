/*
 *  test_volumelogic.c  —  host tests for VolumeLogic (volume clamping)
 */
#include <stdio.h>
#include "VolumeLogic.h"

static int gFailures;
#define CHECK(cond) do { if (!(cond)) { \
    printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); gFailures++; } } while (0)

static void test_clamp_full(void)
{
    CHECK(VolumeClamp(0x01000100) == 0x01000100);
}

static void test_clamp_per_channel(void)
{
    /* right:left halves clamp independently and never swap */
    CHECK(VolumeClamp(0x02000080) == 0x01000080);
    CHECK(VolumeClamp(0x00400300) == 0x00400100);
}

static void test_clamp_zero(void)
{
    CHECK(VolumeClamp(0) == 0);
}

static void test_wrapper_flags_drop_register_message(void)
{
    /* awgc probed as 8000000C: cmpWantsRegisterMessage must not be copied */
    CHECK(WrapperComponentFlags(0x8000000C) == 0x0000000C);
    CHECK(WrapperComponentFlags(0x0000000C) == 0x0000000C);
}

static void test_combine_full(void)
{
    CHECK(VolumeCombine(0x01000100, 0x01000100, false) == 0x01000100);
}

static void test_combine_scales_each_channel(void)
{
    /* main volume x plain volume, right and left on their own */
    CHECK(VolumeCombine(0x00800080, 0x01000100, false) == 0x00800080);
    CHECK(VolumeCombine(0x01000100, 0x00400080, false) == 0x00400080);
    CHECK(VolumeCombine(0x00800080, 0x00800080, false) == 0x00400040);
}

static void test_combine_muted_is_silent(void)
{
    CHECK(VolumeCombine(0x01000100, 0x01000100, true) == 0);
}

static void test_scale_stereo_per_channel(void)
{
    /* interleaved L,R; volume high word = right, low word = left */
    SInt16 s[4] = { 1000, -1000, 2000, -2000 };
    VolumeScale16(s, 2, 2, 0x00400080);
    CHECK(s[0] == 500 && s[1] == -250 && s[2] == 1000 && s[3] == -500);
}

static void test_scale_unity_unchanged(void)
{
    SInt16 s[2] = { -32768, 32767 };
    VolumeScale16(s, 1, 2, 0x01000100);
    CHECK(s[0] == -32768 && s[1] == 32767);
}

static void test_scale_mono_uses_left(void)
{
    SInt16 s[2] = { 1000, 1000 };
    VolumeScale16(s, 2, 1, 0x01000080);
    CHECK(s[0] == 500 && s[1] == 500);
}

static void test_scale_zero_silences(void)
{
    SInt16 s[2] = { 1234, -1234 };
    VolumeScale16(s, 1, 2, 0);
    CHECK(s[0] == 0 && s[1] == 0);
}

int main(void)
{
    test_clamp_full();
    test_clamp_per_channel();
    test_clamp_zero();
    test_wrapper_flags_drop_register_message();
    test_combine_full();
    test_combine_scales_each_channel();
    test_combine_muted_is_silent();
    test_scale_stereo_per_channel();
    test_scale_unity_unchanged();
    test_scale_mono_uses_left();
    test_scale_zero_silences();
    if (gFailures) { printf("test_volumelogic: %d failed\n", gFailures); return 1; }
    printf("test_volumelogic: all passed\n");
    return 0;
}
