# ===== User-facing targets =====
# - make          : kfs.iso まで作成
# - make kernel      : kfs.bin まで作成
# - make iso      : ISO を作成
# - make run      : qemu-system-$(ISA) -cdrom kfs.iso を実行
# - make run-kernel  : qemu-system-$(ISA) -kernel kfs.bin を実行

include .env
export

# ===== Docker image settings =====
IMAGE ?= smizuoch/kfs:1.0.2
DOCKER ?= docker
ISA	?= i386
PWD := $(shell pwd)
DOCKER_RUN = $(DOCKER) run --platform $(DOCKER_PLATFORM) --rm -v "$(PWD)":/work -w /work $(IMAGE)

# ===== Toolchain (used inside container) =====
CROSS   ?= i686-elf
CC      := $(CROSS)-gcc
CFLAGS  := -ffreestanding -Wall -Wextra -Werror -m32 -fno-builtin -fno-stack-protector -nostdlib -nodefaultlibs
LDFLAGS := -T arch/$(ISA)/boot/linker.ld -ffreestanding -m32 -fno-builtin -fno-stack-protector -nostdlib -nodefaultlibs

# Sources and objects
KERNEL_SRCS_C := $(shell find ./ -path ./test -prune -o -name '*.c' -print)
KERNEL_SRCS_H := $(shell find ./ -path ./test -prune -o -name '*.h' -print)
KERNEL_SRCS_S := $(shell find ./ -path ./test -prune -o -name '*.S' -print)
TEST_SRCS_C   := $(shell find ./test -name '*.c' -print)
TEST_SRCS_H   := $(shell find ./test -name '*.h' -print)
TEST_SRCS_SH  := $(shell find ./test -name '*.sh' -print)

KERNEL_SRCS := $(KERNEL_SRCS_C) $(KERNEL_SRCS_S)
TEST_SRCS := $(TEST_SRCS_C) $(TEST_SRCS_SH)
KERNEL_OBJS := $(patsubst %.S,%.o,$(patsubst %.c,%.o,$(KERNEL_SRCS)))

KERNEL := kfs.bin
ISO    := kfs.iso

# ===== Default =====
all: iso

# ===== Ensure Docker image (pull or build fallback) =====
ensure-image:
	@set -e; \
	if ! $(DOCKER) image inspect $(IMAGE) >/dev/null 2>&1; then \
		( $(DOCKER) pull $(IMAGE) >/dev/null 2>&1 ) || \
		( echo "Pull failed. Building local image from arch/$(ISA)/compile.dockerfile..."; \
		  $(DOCKER) build --platform $(DOCKER_PLATFORM) -f arch/$(ISA)/compile.dockerfile -t $(IMAGE) . ); \
	fi; \
	echo "Using Docker image: $(IMAGE)"

# ===== Wrapper rules (host side) =====
ifeq ($(IN_DOCKER),1)

# --- Build inside container ---
$(KERNEL): $(KERNEL_OBJS) arch/$(ISA)/boot/linker.ld
	$(CC) -o $@ $(KERNEL_OBJS) $(LDFLAGS)

# Compile rules (inside container)
%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel: $(KERNEL)

iso: kernel grub.cfg
	mkdir -p isodir/boot/grub
	cp $(KERNEL) isodir/boot/kfs.bin
	cp grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) isodir

clean:
	rm -rf isodir $(ISO)

fclean: clean
	rm -f $(KERNEL) $(KERNEL_OBJS)

re: fclean all

else

# --- Wrapper: run the same targets inside Docker ---
kernel: ensure-image
	@$(DOCKER_RUN) /bin/bash -lc 'IN_DOCKER=1 make kernel'

iso: ensure-image
	@$(DOCKER_RUN) /bin/bash -lc 'IN_DOCKER=1 make iso'

clean:
	@rm -rf isodir $(KERNEL_OBJS)

fclean: clean
	@rm -f $(KERNEL) $(ISO)

re: fclean all

endif

# ===== Run with QEMU (prefer host, fallback to container) =====
run: run-iso

run-iso: $(ISO)
	@if command -v qemu-system-$(ISA) >/dev/null 2>&1; then \
	  qemu-system-$(ISA) -cdrom $(ISO) -serial stdio; \
	else \
	  echo "qemu-system-$(ISA) not found on host. Trying in Docker..."; \
	  $(DOCKER_RUN) qemu-system-$(ISA) -cdrom $(ISO) -serial stdio; \
	fi

run-kernel: $(KERNEL)
	@if command -v qemu-system-$(ISA) >/dev/null 2>&1; then \
	  qemu-system-$(ISA) -kernel $(KERNEL) -serial stdio; \
	else \
	  echo "qemu-system-$(ISA) not found on host. Trying in Docker..."; \
	  $(DOCKER_RUN) qemu-system-$(ISA) -kernel $(KERNEL) -serial stdio; \
	fi

# ===== Tests passthrough =====
test:
	@ make -C test/

coverage:
	@ make coverage -C test/

fmt:
	docker run -v ./:/work -w /work ubuntu:22.04 bash -c \
	'apt-get update && apt-get install -y make clang-format shfmt \
	&& clang-format -i -style="{BasedOnStyle: Microsoft, IndentWidth: 4, TabWidth: 4, UseTab: Always}" $(KERNEL_SRCS_C) $(TEST_SRCS_C) $(KERNEL_SRCS_H) $(TEST_SRCS_H) \
	&& shfmt -w $(TEST_SRCS_SH)'

.PHONY: all kernel iso run run-iso run-kernel clean fclean re ensure-image test
