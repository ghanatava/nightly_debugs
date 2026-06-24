BUILD_DIR := build
BPF_SRCS  := $(wildcard *.bpf.c)
BPF_OBJS  := $(patsubst %.bpf.c,$(BUILD_DIR)/%.bpf.o,$(BPF_SRCS))
NAMES     := $(patsubst %.bpf.c,%,$(BPF_SRCS))

all: $(BPF_OBJS)

$(NAMES): %: $(BUILD_DIR)/%.bpf.o
	@:

.PHONY: all clean $(NAMES)

$(BUILD_DIR)/%.bpf.o: %.bpf.c | $(BUILD_DIR)
	clang -target bpf -I/usr/include/$(shell uname -m)-linux-gnu -g -O2 -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)