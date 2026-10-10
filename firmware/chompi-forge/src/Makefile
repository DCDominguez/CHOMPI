TARGET = FORGE
DEBUG = 0
OPT = -O3
APP_TYPE = BOOT_SRAM
WAVE = ../../chompi-wave/code
LIBDAISY_DIR = $(WAVE)/libs/libDaisy
LIBDIR = -L../build/libdaisy
# TEMPO's code/data split (see forge_sram.lds).
LDSCRIPT = forge_sram.lds
CPP_SOURCES = forge_main.cpp $(WAVE)/src/encoder.cpp
# Device presets on the SD card.
USE_FATFS = 1
C_INCLUDES += -I$(WAVE)/src -I../core
C_INCLUDES += -I$(LIBDAISY_DIR)/Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
# Development build for the test bridge: `make firmware-dev` (opcodes 0A/0B).
# Never ship it: it lets any connected host press keys.
# The top-level target passes BUILD_DIR=build-dev on the command line
# (libDaisy's Makefile would otherwise reset it).
ifeq ($(TEST_HOOKS),1)
C_DEFS += -DFORGE_TEST_HOOKS
endif
# Build identity (status reply, Inspector page 2): the commit and whether Forge's sources had
# uncommitted changes. Override with FORGE_BUILD_ID=<7 hex> (0 = unknown, e.g. a source ZIP).
# The header is rewritten only when the identity changes, so forge_main.o rebuilds with it.
FORGE_BUILD_ID ?= $(or $(shell git rev-parse --short=7 HEAD 2>/dev/null),0)
FORGE_BUILD_DIRTY ?= $(if $(shell git status --porcelain --untracked-files=no -- .. 2>/dev/null),1,0)
BUILD_DIR ?= build
IDENTITY_HEADER = $(BUILD_DIR)/forge_build_id.h
IDENTITY_TEXT = \#define FORGE_BUILD_ID 0x$(FORGE_BUILD_ID)u\n\#define FORGE_BUILD_DIRTY $(FORGE_BUILD_DIRTY)
$(shell mkdir -p $(BUILD_DIR); printf '$(IDENTITY_TEXT)\n' > $(IDENTITY_HEADER).new; \
  cmp -s $(IDENTITY_HEADER).new $(IDENTITY_HEADER) && rm -f $(IDENTITY_HEADER).new || mv -f $(IDENTITY_HEADER).new $(IDENTITY_HEADER))
# Keep the foundation build-only without modifying the vendored Makefile.
ifneq ($(filter program% flash%,$(MAKECMDGOALS)),)
$(error Forge foundation is build-only; see ../README.md for hardware validation gates)
endif
include $(SYSTEM_FILES_DIR)/Makefile

# Only forge_main.cpp sees the identity (core/protocol.h EncodeIdentity).
$(BUILD_DIR)/forge_main.o: CPPFLAGS += -include $(IDENTITY_HEADER)
$(BUILD_DIR)/forge_main.o: $(IDENTITY_HEADER)

# Relink when the linker script changes (libDaisy's rule only lists objects and Makefile).
$(BUILD_DIR)/FORGE.elf: ../build/libdaisy/libdaisy.a $(LDSCRIPT)
../build/libdaisy/libdaisy.a:
	$(MAKE) -C .. libdaisy
