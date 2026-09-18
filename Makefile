CC := gcc
CFLAGS := -std=c11 -O2 -Wall -Wextra -Wpedantic -D_POSIX_C_SOURCE=200809L
BIN := bin

.PHONY: all clean test

all: $(BIN)/sender $(BIN)/receiver $(BIN)/pipe_pc $(BIN)/semaphore_pc

$(BIN):
	mkdir -p $(BIN)

$(BIN)/sender: sinais/sender.c | $(BIN)
	$(CC) $(CFLAGS) $< -o $@

$(BIN)/receiver: sinais/receiver.c | $(BIN)
	$(CC) $(CFLAGS) $< -o $@

$(BIN)/pipe_pc: pipes/producer_consumer_pipe.c | $(BIN)
	$(CC) $(CFLAGS) $< -o $@

$(BIN)/semaphore_pc: semaforos/producer_consumer_threads.c | $(BIN)
	$(CC) $(CFLAGS) $< -o $@ -pthread

test: all
	./scripts/test_all.sh

clean:
	rm -f $(BIN)/sender $(BIN)/receiver $(BIN)/pipe_pc $(BIN)/semaphore_pc

