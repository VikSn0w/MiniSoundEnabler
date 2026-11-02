/*
 *  test_heapcheck.c  —  host tests for HeapCheck (residency guard)
 */
#include <stdio.h>
#include "HeapCheck.h"

static int gFailures;
#define CHECK(cond) do { if (!(cond)) { \
    printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); gFailures++; } } while (0)

static char heap[64];

static void test_inside_range(void)
{
    CHECK(AddressInRange(heap, heap, heap + 64));
    CHECK(AddressInRange(heap + 63, heap, heap + 64));
}

static void test_outside_range(void)
{
    CHECK(!AddressInRange(heap + 64, heap, heap + 64));    /* limit excluded */
    CHECK(!AddressInRange(heap, heap + 1, heap + 64));
}

int main(void)
{
    test_inside_range();
    test_outside_range();
    if (gFailures) { printf("test_heapcheck: %d failed\n", gFailures); return 1; }
    printf("test_heapcheck: all passed\n");
    return 0;
}
