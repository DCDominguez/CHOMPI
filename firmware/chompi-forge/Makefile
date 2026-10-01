CXX ?= c++
HOST_FLAGS = -std=c++14 -Wall -Wextra -Werror -pedantic -pthread
.PHONY: test sanitize firmware libdaisy
test: build/core_test
	./build/core_test
build/core_test: tests/core_test.cpp core/parameters.h core/command_queue.h core/engine.h
	mkdir -p build
	$(CXX) $(HOST_FLAGS) -O2 tests/core_test.cpp -o $@
sanitize:
	mkdir -p build
	$(CXX) $(HOST_FLAGS) -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer tests/core_test.cpp -o build/core_test_sanitize
	./build/core_test_sanitize
libdaisy:
	mkdir -p build
	$(MAKE) -C ../chompi-wave/code/libs/libDaisy BUILD_DIR=$(abspath build/libdaisy)
firmware: libdaisy
	$(MAKE) -C src
