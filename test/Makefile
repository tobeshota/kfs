CC ?= gcc
CFLAGS = -I../include -I. -Wall -Wextra -O0 -m32
TEST_SRCS = $(shell find ../ -name '*.c')
TEST_OBJS = $(TEST_SRCS:.c=.o)
TEST_BIN = ./unit_test

all: test

test: unit integration

$(TEST_BIN): $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $(TEST_OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

unit:
	docker run -it -v ../:/work i386/ubuntu bash -c \
		"apt update \
		&& apt install -y gcc-multilib make \
		&& cd /work/test \
		&& make ./unit_test \
		&& ./unit_test"

integration:
	@echo "integration test" && \
	bash -eu ./integration_test.sh

clean:
	rm -rf build $(TEST_OBJS)

.PHONY: all test unit integration clean
