TARGET = FORGE
DEBUG = 0
OPT = -O3
APP_TYPE = BOOT_SRAM
WAVE = ../../chompi-wave/code
LIBDAISY_DIR = $(WAVE)/libs/libDaisy
LIBDIR = -L../build/libdaisy
LDSCRIPT = $(WAVE)/src/chompi_sram.lds
CPP_SOURCES = forge_main.cpp $(WAVE)/src/encoder.cpp
C_INCLUDES += -I$(WAVE)/src -I../core
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
# Keep the foundation build-only without modifying the vendored Makefile.
ifneq ($(filter program% flash%,$(MAKECMDGOALS)),)
$(error Forge foundation is build-only; see ../README.md for hardware validation gates)
endif
include $(SYSTEM_FILES_DIR)/Makefile

build/FORGE.elf: ../build/libdaisy/libdaisy.a
../build/libdaisy/libdaisy.a:
	$(MAKE) -C .. libdaisy
