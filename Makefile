CXX ?= c++
CPPFLAGS := -Iinclude
CXXFLAGS ?= -O3 -DNDEBUG
CXXFLAGS += -std=c++17 -Wall -Wextra -Wpedantic -MMD -MP

SRC := $(wildcard src/*.cpp src/plain/*.cpp src/encoded/*.cpp)
OBJ := $(patsubst src/%.cpp,build/%.o,$(SRC))

.PHONY: all test clean

all: bin/cp_algo

bin/cp_algo: $(OBJ) | bin
	$(CXX) $(CXXFLAGS) $^ -o $@

build/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

bin:
	mkdir -p bin

test: bin/cp_algo
	python3 tests/integration.py

clean:
	rm -rf build bin

-include $(OBJ:.o=.d)
