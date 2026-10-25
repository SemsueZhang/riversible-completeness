CXX ?= c++
CPPFLAGS += -Iinclude
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -Wpedantic
BUILD := build
SOURCES := $(wildcard src/*.cpp)
OBJECTS := $(patsubst src/%.cpp,$(BUILD)/%.o,$(SOURCES))
PROGRAMS := bench_normalize bench_equivalence generate_data
.PHONY: all clean verify
all: $(addprefix $(BUILD)/,$(PROGRAMS))
$(BUILD):
	mkdir -p $(BUILD)
$(BUILD)/%.o: src/%.cpp | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@
$(BUILD)/%: apps/%.cpp $(OBJECTS) | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< $(OBJECTS) -o $@
$(BUILD)/verify_binary: tests/verify_binary.cpp $(OBJECTS) | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< $(OBJECTS) -o $@
$(BUILD)/verify_sparse_path: tests/verify_sparse_path.cpp $(OBJECTS) | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< $(OBJECTS) -o $@
verify: $(BUILD)/verify_binary $(BUILD)/verify_sparse_path
	$(BUILD)/verify_binary --extended
	$(BUILD)/verify_sparse_path
clean:
	rm -rf $(BUILD)
-include $(OBJECTS:.o=.d)
.SECONDARY: $(OBJECTS)
