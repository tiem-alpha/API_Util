CC     = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -I..

# Detect Windows for binary extension and clean command
ifeq ($(OS),Windows_NT)
    EXE = .exe
    RM  = del /Q
else
    EXE =
    RM  = rm -f
endif

COMM = ../communication

SRCS = \
    $(COMM)/utils/crc.c          \
    $(COMM)/queue/comm_queue.c   \
    $(COMM)/protocol/protocol.c  \
    $(COMM)/communication_manager.c

TEST_SRCS = \
    utest.c                  \
    test_crc.c               \
    test_queue.c             \
    test_protocol.c          \
    test_comm_manager.c      \
    test_runner.c

TARGET = test_runner$(EXE)

all: $(TARGET)

$(TARGET): $(SRCS) $(TEST_SRCS)
	$(CC) $(CFLAGS) $^ -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	$(RM) $(TARGET)

.PHONY: all run clean
