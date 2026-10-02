CXX ?= c++
HOST_FLAGS = -std=c++14 -Wall -Wextra -Werror -pedantic -pthread
HEADERS = $(wildcard core/*.h)
.PHONY: test sanitize firmware libdaisy
test: build/core_test build/protocol_test build/synth_test build/forge_probe
	./build/core_test
	./build/protocol_test
	./build/synth_test
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
libdaisy:
	mkdir -p build
	$(MAKE) -C ../chompi-wave/code/libs/libDaisy BUILD_DIR=$(abspath build/libdaisy)
firmware: libdaisy
	$(MAKE) -C src
