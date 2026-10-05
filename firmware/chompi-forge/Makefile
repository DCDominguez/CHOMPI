CXX ?= c++
HOST_FLAGS = -std=c++14 -Wall -Wextra -Werror -pedantic -pthread
HEADERS = $(wildcard core/*.h)
.PHONY: test sanitize firmware firmware-dev libdaisy browser-test bench
test: build/core_test build/protocol_test build/synth_test build/v3_test build/preset_test build/sampler_test build/panel_test build/inspector_test build/looper_test build/file_transfer_test build/power_test build/slot_settings_test build/harmony_test build/knob_audio_test build/starter_patches.txt build/forge_probe
	./build/core_test
	./build/protocol_test
	./build/synth_test
	./build/v3_test
	./build/preset_test
	./build/sampler_test
	./build/panel_test
	./build/inspector_test
	./build/looper_test
	./build/file_transfer_test
	./build/power_test
	./build/slot_settings_test
	./build/harmony_test
	./build/knob_audio_test
	python3 -m unittest discover -s tests -p 'test_*.py' -v
build/slot_settings_test: tests/slot_settings_test.cpp $(HEADERS)
	mkdir -p build
	$(CXX) $(HOST_FLAGS) -O2 tests/slot_settings_test.cpp -o $@
build/harmony_test: tests/harmony_test.cpp $(HEADERS)
	mkdir -p build
	$(CXX) $(HOST_FLAGS) -O2 tests/harmony_test.cpp -o $@
build/knob_audio_test: tests/knob_audio_test.cpp $(HEADERS)
	mkdir -p build
	$(CXX) $(HOST_FLAGS) -DFORGE_TEST_HOOKS -O2 tests/knob_audio_test.cpp -o $@
build/starter_patches.txt: tests/dump_starter_patches.py host/forge_host.py host/forge_bridge.py $(wildcard presets/*.json)
	mkdir -p build
	python3 tests/dump_starter_patches.py > $@
build/core_test: tests/core_test.cpp $(HEADERS)
	mkdir -p build
	$(CXX) $(HOST_FLAGS) -O2 tests/core_test.cpp -o $@
build/protocol_test: tests/protocol_test.cpp $(HEADERS)
	mkdir -p build
	$(CXX) $(HOST_FLAGS) -O2 tests/protocol_test.cpp -o $@
build/synth_test: tests/synth_test.cpp $(HEADERS)
	mkdir -p build
	$(CXX) $(HOST_FLAGS) -O2 tests/synth_test.cpp -o $@
build/v3_test: tests/v3_test.cpp $(HEADERS)
	mkdir -p build
	$(CXX) $(HOST_FLAGS) -O2 tests/v3_test.cpp -o $@
build/preset_test: tests/preset_test.cpp $(HEADERS)
	mkdir -p build
	$(CXX) $(HOST_FLAGS) -O2 tests/preset_test.cpp -o $@
build/sampler_test: tests/sampler_test.cpp $(HEADERS)
	mkdir -p build
	$(CXX) $(HOST_FLAGS) -O2 tests/sampler_test.cpp -o $@
build/panel_test: tests/panel_test.cpp $(HEADERS)
	mkdir -p build
	$(CXX) $(HOST_FLAGS) -DFORGE_TEST_HOOKS -O2 tests/panel_test.cpp -o $@
build/looper_test: tests/looper_test.cpp $(HEADERS)
	mkdir -p build
	$(CXX) $(HOST_FLAGS) -O2 tests/looper_test.cpp -o $@
build/file_transfer_test: tests/file_transfer_test.cpp $(HEADERS)
	mkdir -p build
	$(CXX) $(HOST_FLAGS) -O2 tests/file_transfer_test.cpp -o $@
build/power_test: tests/power_test.cpp $(HEADERS)
	mkdir -p build
	$(CXX) $(HOST_FLAGS) -O2 tests/power_test.cpp -o $@
build/inspector_test: tests/inspector_test.cpp tests/sample_card.h $(HEADERS)
	mkdir -p build
	$(CXX) $(HOST_FLAGS) -DFORGE_TEST_HOOKS -O2 tests/inspector_test.cpp -o $@
build/forge_probe: host/forge_probe.cpp tests/sample_card.h $(HEADERS)
	mkdir -p build
	$(CXX) $(HOST_FLAGS) -DFORGE_TEST_HOOKS -O2 host/forge_probe.cpp -o $@
sanitize:
	mkdir -p build
	$(CXX) $(HOST_FLAGS) -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer tests/core_test.cpp -o build/core_test_sanitize
	./build/core_test_sanitize
	$(CXX) $(HOST_FLAGS) -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer tests/protocol_test.cpp -o build/protocol_test_sanitize
	./build/protocol_test_sanitize
	$(CXX) $(HOST_FLAGS) -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer tests/synth_test.cpp -o build/synth_test_sanitize
	./build/synth_test_sanitize
	$(CXX) $(HOST_FLAGS) -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer tests/v3_test.cpp -o build/v3_test_sanitize
	./build/v3_test_sanitize
	$(CXX) $(HOST_FLAGS) -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer tests/preset_test.cpp -o build/preset_test_sanitize
	./build/preset_test_sanitize
	$(CXX) $(HOST_FLAGS) -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer tests/sampler_test.cpp -o build/sampler_test_sanitize
	./build/sampler_test_sanitize
	$(CXX) $(HOST_FLAGS) -DFORGE_TEST_HOOKS -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer tests/panel_test.cpp -o build/panel_test_sanitize
	./build/panel_test_sanitize
	$(CXX) $(HOST_FLAGS) -DFORGE_TEST_HOOKS -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer tests/inspector_test.cpp -o build/inspector_test_sanitize
	./build/inspector_test_sanitize
	$(CXX) $(HOST_FLAGS) -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer tests/looper_test.cpp -o build/looper_test_sanitize
	./build/looper_test_sanitize
	$(CXX) $(HOST_FLAGS) -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer tests/file_transfer_test.cpp -o build/file_transfer_test_sanitize
	./build/file_transfer_test_sanitize
	$(CXX) $(HOST_FLAGS) -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer tests/power_test.cpp -o build/power_test_sanitize
	./build/power_test_sanitize
	$(CXX) $(HOST_FLAGS) -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer tests/slot_settings_test.cpp -o build/slot_settings_test_sanitize
	./build/slot_settings_test_sanitize
	$(CXX) $(HOST_FLAGS) -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer tests/harmony_test.cpp -o build/harmony_test_sanitize
	./build/harmony_test_sanitize
libdaisy:
	mkdir -p build
	$(MAKE) -C ../chompi-wave/code/libs/libDaisy BUILD_DIR=$(abspath build/libdaisy)
firmware: libdaisy
	$(MAKE) -C src
	python3 host/check_firmware_layout.py src/build/FORGE.elf src/build/FORGE.bin
# Development firmware with the bridge's panel injection and probe (src/build-dev/).
firmware-dev: libdaisy
	$(MAKE) -C src TEST_HOOKS=1 BUILD_DIR=build-dev
	python3 host/check_firmware_layout.py src/build-dev/FORGE.elf src/build-dev/FORGE.bin

# Real Chromium end-to-end test of the webapp against the simulated device.
# Needs: pip install playwright && python3 -m playwright install chromium
browser-test: build/forge_probe
	python3 tests/browser_e2e.py
	python3 tests/bridge_browser_e2e.py

# CPU benchmark: Forge vs stock TAPE / TEMPO / WAVE DSP, as ARM instruction
# counts in an emulator (see bench/run_bench.py for what this does and does not
# measure). Upstream sources are only read; their libraries build into build/bench.
# Needs arm-none-eabi-gcc (GCC_PATH or PATH) and: pip install unicorn pyelftools
# TEMPO's upstream build uses GCC 13.3.rel1 (TAPE, WAVE and Forge use 10.3), so
# its bench builds with TEMPO_GCC_PATH when given, into its own directories.
BENCH_CXX = $(if $(GCC_PATH),$(GCC_PATH)/,)arm-none-eabi-g++
TEMPO_GCC_PATH ?= $(GCC_PATH)
TEMPO_CXX = $(if $(TEMPO_GCC_PATH),$(TEMPO_GCC_PATH)/,)arm-none-eabi-g++
TEMPO_BENCH = build/bench/tempo-$(shell $(TEMPO_CXX) -dumpfullversion 2>/dev/null || echo unknown)
BENCH_FLAGS = -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard -O3 -fno-exceptions -fno-rtti \
	-ffunction-sections -fdata-sections -fno-threadsafe-statics -std=gnu++14 -w \
	-DSTM32H750xx -DCORE_CM7 -DUSE_HAL_DRIVER -DHSE_VALUE=16000000 -DSTM32H750IB -DARM_MATH_CM7 -DUSE_FULL_LL_DRIVER
BENCH_LINK = --specs=nano.specs --specs=nosys.specs -nostartfiles -Wl,--gc-sections -T bench/bench.ld \
	-Wl,-u,bench_init -Wl,-u,bench_block -Wl,-u,request -Wl,-u,request_size \
	-Wl,-u,out_l -Wl,-u,out_r -Wl,-u,out0 -Wl,-u,out1
bench_includes = -include stm32h7xx.h -I$(1)/src -I$(1)/libs/coreJSON/source/include -I$(1)/libs/libDaisy \
	-I$(1)/libs/libDaisy/src -I$(1)/libs/libDaisy/src/sys -I$(1)/libs/libDaisy/src/usbd -I$(1)/libs/libDaisy/src/usbh \
	-I$(1)/libs/libDaisy/Drivers/CMSIS/Include -I$(1)/libs/libDaisy/Drivers/CMSIS/Device/ST/STM32H7xx/Include \
	-I$(1)/libs/libDaisy/Drivers/STM32H7xx_HAL_Driver/Inc -I$(1)/libs/libDaisy/Middlewares/ST/STM32_USB_Device_Library/Core/Inc \
	-I$(1)/libs/libDaisy/Middlewares/ST/STM32_USB_Host_Library/Core/Inc \
	-I$(1)/libs/libDaisy/Middlewares/ST/STM32_USB_Host_Library/Class/MSC/Inc -I$(1)/libs/libDaisy/core \
	-I$(1)/libs/libDaisy/Middlewares/Third_Party/FatFs/src -I$(1)/libs/DaisySP/Source
TAPE = ../chompi-tape/code
TEMPO = ../chompi-tempo/code
WAVE_CODE = ../chompi-wave/code
bench: libdaisy
	mkdir -p build/bench $(TEMPO_BENCH)
	$(MAKE) -C $(TAPE)/libs/DaisySP BUILD_DIR=$(abspath build/bench/tape-daisysp)
	$(MAKE) -C $(TEMPO)/libs/DaisySP BUILD_DIR=$(abspath $(TEMPO_BENCH)/daisysp) $(if $(TEMPO_GCC_PATH),GCC_PATH=$(TEMPO_GCC_PATH),)
	$(MAKE) -C $(TEMPO)/libs/libDaisy BUILD_DIR=$(abspath $(TEMPO_BENCH)/libdaisy) $(if $(TEMPO_GCC_PATH),GCC_PATH=$(TEMPO_GCC_PATH),)
	$(MAKE) -C $(WAVE_CODE)/libs/DaisySP BUILD_DIR=$(abspath build/bench/wave-daisysp)
	$(BENCH_CXX) $(BENCH_FLAGS) $(BENCH_LINK) bench/forge_bench.cpp -o build/bench/forge_bench.elf -lm
	$(BENCH_CXX) $(BENCH_FLAGS) $(call bench_includes,$(TAPE)) $(BENCH_LINK) bench/tape_bench.cpp \
		build/bench/tape-daisysp/libdaisysp.a -o build/bench/tape_bench.elf -lm
	$(TEMPO_CXX) $(BENCH_FLAGS) $(call bench_includes,$(TEMPO)) $(BENCH_LINK) bench/tempo_bench.cpp \
		$(TEMPO)/src/FileStreamingManager.cpp $(TEMPO_BENCH)/libdaisy/libdaisy.a \
		$(TEMPO_BENCH)/daisysp/libdaisysp.a -o build/bench/tempo_bench.elf -lm
	$(BENCH_CXX) $(BENCH_FLAGS) $(call bench_includes,$(WAVE_CODE)) $(BENCH_LINK) bench/wave_bench.cpp \
		$(WAVE_CODE)/src/FileStreamingManager.cpp build/libdaisy/libdaisy.a \
		build/bench/wave-daisysp/libdaisysp.a -o build/bench/wave_bench.elf -lm
	python3 bench/run_bench.py --check

