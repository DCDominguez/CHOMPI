CXX ?= c++
HOST_FLAGS = -std=c++14 -Wall -Wextra -Werror -pedantic -pthread
HEADERS = $(wildcard core/*.h)
.PHONY: test sanitize firmware libdaisy browser-test bench
test: build/core_test build/protocol_test build/synth_test build/v3_test build/forge_probe
	./build/core_test
	./build/protocol_test
	./build/synth_test
	./build/v3_test
	python3 -m unittest discover -s tests -p 'test_*.py' -v
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
build/forge_probe: host/forge_probe.cpp $(HEADERS)
	mkdir -p build
	$(CXX) $(HOST_FLAGS) -O2 host/forge_probe.cpp -o $@
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
libdaisy:
	mkdir -p build
	$(MAKE) -C ../chompi-wave/code/libs/libDaisy BUILD_DIR=$(abspath build/libdaisy)
firmware: libdaisy
	$(MAKE) -C src

# Real Chromium end-to-end test of the webapp against the simulated device.
# Needs: pip install playwright && python3 -m playwright install chromium
browser-test: build/forge_probe
	python3 tests/browser_e2e.py

# CPU benchmark: Forge vs stock TAPE / TEMPO / WAVE DSP, as ARM instruction
# counts in an emulator (see bench/run_bench.py for what this does and does not
# measure). Upstream sources are only read; their libraries build into build/bench.
# Needs arm-none-eabi-gcc (GCC_PATH or PATH) and: pip install unicorn pyelftools
BENCH_CXX = $(if $(GCC_PATH),$(GCC_PATH)/,)arm-none-eabi-g++
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
	mkdir -p build/bench
	$(MAKE) -C $(TAPE)/libs/DaisySP BUILD_DIR=$(abspath build/bench/tape-daisysp)
	$(MAKE) -C $(TEMPO)/libs/DaisySP BUILD_DIR=$(abspath build/bench/tempo-daisysp)
	$(MAKE) -C $(TEMPO)/libs/libDaisy BUILD_DIR=$(abspath build/bench/tempo-libdaisy)
	$(MAKE) -C $(WAVE_CODE)/libs/DaisySP BUILD_DIR=$(abspath build/bench/wave-daisysp)
	$(BENCH_CXX) $(BENCH_FLAGS) $(BENCH_LINK) bench/forge_bench.cpp -o build/bench/forge_bench.elf -lm
	$(BENCH_CXX) $(BENCH_FLAGS) $(call bench_includes,$(TAPE)) $(BENCH_LINK) bench/tape_bench.cpp \
		build/bench/tape-daisysp/libdaisysp.a -o build/bench/tape_bench.elf -lm
	$(BENCH_CXX) $(BENCH_FLAGS) $(call bench_includes,$(TEMPO)) $(BENCH_LINK) bench/tempo_bench.cpp \
		$(TEMPO)/src/FileStreamingManager.cpp build/bench/tempo-libdaisy/libdaisy.a \
		build/bench/tempo-daisysp/libdaisysp.a -o build/bench/tempo_bench.elf -lm
	$(BENCH_CXX) $(BENCH_FLAGS) $(call bench_includes,$(WAVE_CODE)) $(BENCH_LINK) bench/wave_bench.cpp \
		$(WAVE_CODE)/src/FileStreamingManager.cpp build/libdaisy/libdaisy.a \
		build/bench/wave-daisysp/libdaisysp.a -o build/bench/wave_bench.elf -lm
	python3 bench/run_bench.py --check

