ISA			?= i386
CC			?=	gcc
CFLAGS		= -I../include -I. -Wall -Wextra -O0 -m32
UNIT_SRCS	= $(shell find ../ -name '*.c')
UNIT_OBJS	= $(UNIT_SRCS:.c=.o)
UNIT_BIN	= ./unit_test

all: test

test: unit integration

unit:
	docker build --platform linux/386 -f ./arch/$(ISA)/compile.dockerfile -t i386/ubuntu ../
	docker run --platform linux/386 -it -v $(realpath ../):/work i386/ubuntu bash -c \
	"cd /work/test && make ./unit_test && ./unit_test"

integration:
	@echo "integration test" && \
	bash -eu ./integration_test.sh

clean:
	rm -rf build $(UNIT_OBJS)

$(UNIT_BIN): $(UNIT_OBJS)
	$(CC) $(CFLAGS) -o $@ $(UNIT_OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: all test unit integration clean
