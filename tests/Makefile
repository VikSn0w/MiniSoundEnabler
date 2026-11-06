# Host unit tests for the pure-logic modules in ../src
CC      = cc
CFLAGS  = -std=c99 -Wall -Werror -Ihost -I../src
TESTS   = test_amplogic test_volumelogic test_heapcheck test_machinelogic

all: $(TESTS)

test_amplogic: test_amplogic.c ../src/AmpLogic.c ../src/AmpLogic.h host/MacTypes.h
	$(CC) $(CFLAGS) -o $@ test_amplogic.c ../src/AmpLogic.c

test_volumelogic: test_volumelogic.c ../src/VolumeLogic.c ../src/VolumeLogic.h host/MacTypes.h
	$(CC) $(CFLAGS) -o $@ test_volumelogic.c ../src/VolumeLogic.c

test_heapcheck: test_heapcheck.c ../src/HeapCheck.c ../src/HeapCheck.h host/MacTypes.h
	$(CC) $(CFLAGS) -o $@ test_heapcheck.c ../src/HeapCheck.c

test_machinelogic: test_machinelogic.c ../src/MachineLogic.c ../src/MachineLogic.h host/MacTypes.h
	$(CC) $(CFLAGS) -o $@ test_machinelogic.c ../src/MachineLogic.c

test: $(TESTS)
	@for t in $(TESTS); do ./$$t || exit 1; done

clean:
	rm -f $(TESTS)

.PHONY: all test clean
