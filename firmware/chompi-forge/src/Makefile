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
# Keep the foundation build-only without modifying the vendored Makefile.
ifneq ($(filter program% flash%,$(MAKECMDGOALS)),)
$(error Forge foundation is build-only; see ../README.md for hardware validation gates)
endif
include $(SYSTEM_FILES_DIR)/Makefile

build/FORGE.elf: ../build/libdaisy/libdaisy.a
../build/libdaisy/libdaisy.a:
	$(MAKE) -C .. libdaisy
