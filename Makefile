SOURCES := utils.c disasm.c emulator.c riscv.c pipeline.c cache.c
HEADERS := types.h utils.h riscv.h pipeline.h stage_helpers.h cache.h config.h
PWD := $(shell pwd)
CUNIT := -L $(PWD)/CUnit-install/lib -I $(PWD)/CUnit-install/include -llibcunit
CFLAGS := -g -Wall

# MS4: select predictor at build time with BP_MODE=0, 1, or 2.
ifdef BP_MODE
CFLAGS += -DBRANCH_PREDICTOR_MODE=$(BP_MODE)
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

# MS4 PART 4 START
# Launch the dependency-free terminal GUI for the MS4 project. This GUI is a simple text-based interface that allows you to run the emulator and view its output in a more user-friendly way.
tui:
	./terminal_gui.sh

# Verify that the terminal GUI and required project tools are available.
test-tui:
	./terminal_gui.sh --check
#  MS4 PART 4 END 
