SOURCES := utils.c disasm.c emulator.c riscv.c pipeline.c cache.c
HEADERS := types.h utils.h riscv.h pipeline.h stage_helpers.h cache.h config.h
PWD := $(shell pwd)
CUNIT := -L $(PWD)/CUnit-install/lib -I $(PWD)/CUnit-install/include -llibcunit
CFLAGS := -g -Wall

# ms4: pick the branch predictor at build time with BP_MODE=0, 1 or 2
ifdef BP_MODE
CFLAGS += -DBRANCH_PREDICTOR_MODE=$(BP_MODE)
CFLAGS += -DBRANCH_PREDICTOR_ENTRIES=64 -DTWO_BIT_INITIAL_STATE=0
CFLAGS += -DPRINT_BRANCH_PREDICTOR_STATS
endif

# ms4: build with the ms4 features no matter what config.h is set to
ifdef MS4
CFLAGS += -DPRINT_STATS -DICACHE_ENABLE -DPRINT_ICACHE_STATS
endif

# lets run_all_tests.sh build each milestone profile without editing config.h
ifdef PROFILE_FLAGS
CFLAGS += $(PROFILE_FLAGS)
endif

all: riscv

riscv: $(SOURCES) $(HEADERS)
	gcc $(CFLAGS) -o $@ $(SOURCES)

test-utils: test_utils.c utils.c $(HEADERS)
	gcc $(CFLAGS) -DTESTING -o test-utils test_utils.c utils.c $(CUNIT)
	./test-utils
	rm -f test-utils

clean:
	rm -f riscv
	rm -f *.o *~
	rm -f test-utils
	rm -f code/ms*/out/*.solution code/ms*/out/*/*.solution
	rm -f code/ms*/out/*.trace code/ms*/out/*/*.trace

deepclean: clean
	rm -rf CUnit-install

# ms4 part 4, simple text menu for building and running the simulator
tui:
	./ms4_tests/terminal_gui.sh

# quick check that the gui and needed tools are there
test-tui:
	./ms4_tests/terminal_gui.sh --check
