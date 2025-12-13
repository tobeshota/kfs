# ===== User-facing targets =====
# - make             : make iso と同じ
# - make kernel      : kfs.bin まで作成
# - make iso         : kfs.iso まで作成
# - make run         : qemu-system-$(ISA) -cdrom kfs.iso を実行
# - make run-kernel  : qemu-system-$(ISA) -kernel kfs.bin を実行

include .env
export

# ===== Docker image settings =====
IMAGE ?= $(ISA)-compile-toolchain
DOCKER ?= docker
ISA	?= i386
PWD := $(shell pwd)
DOCKER_RUN = $(DOCKER) run --platform $(DOCKER_PLATFORM) --rm -v "$(PWD)":/work -w /work $(IMAGE)

# ===== Toolchain (used inside container) =====
CROSS   ?= i686-elf
CC      := $(CROSS)-gcc
INCLUDE_DIRS := include include/kfs include/asm-i386
CFLAGS  := $(addprefix -I,$(INCLUDE_DIRS)) -ffreestanding -Wall -Wextra -Werror -m32 -fno-builtin -fno-stack-protector -nostdlib -nodefaultlibs -nostdinc
DEPFLAGS := -MMD -MP -MF $(BUILD_DIR)/$*.d
LDFLAGS := -T arch/$(ISA)/boot/linker.ld -ffreestanding -m32 -fno-builtin -fno-stack-protector -nostdlib -nodefaultlibs -nostdinc

# Sources and objects
# Explicit kernel C sources (collect then filter out legacy *_test_shim.c that must not ship)
RAW_KERNEL_SRCS_C := $(shell find ./ -path ./test -prune -o -name '*.c' -print)
KERNEL_SRCS_C := $(filter-out %_test_shim.c,$(RAW_KERNEL_SRCS_C))
KERNEL_SRCS_H := $(shell find ./ -path ./test -prune -o -name '*.h' -print)
KERNEL_SRCS_S := $(shell find ./ -path ./test -prune -o -name '*.S' -print)
TEST_SRCS_C   := $(shell find ./test -name '*.c' -print)
TEST_SRCS_H   := $(shell find ./test -name '*.h' -print)
TEST_SRCS_SH  := $(shell find ./test -name '*.sh' -print)

KERNEL_SRCS := $(KERNEL_SRCS_C) $(KERNEL_SRCS_S)
BUILD_DIR   := build/obj
KERNEL_OBJS := $(patsubst %.S,$(BUILD_DIR)/%.o,$(patsubst %.c,$(BUILD_DIR)/%.o,$(KERNEL_SRCS)))
KERNEL_DEPS := $(patsubst %.c,$(BUILD_DIR)/%.d,$(KERNEL_SRCS_C))

KERNEL := kfs.bin
ISO    := kfs.iso

# ===== Default =====
all: iso

# ===== Ensure Docker image (local build only) =====
ensure-image:
	@set -e; \
	if ! $(DOCKER) image inspect $(IMAGE) >/dev/null 2>&1; then \
		echo "Building local image from arch/$(ISA)/compile.dockerfile..."; \
		$(DOCKER) build --platform $(DOCKER_PLATFORM) -f arch/$(ISA)/compile.dockerfile -t $(IMAGE) .; \
	fi; \
	echo "Using Docker image: $(IMAGE)"

# ===== Wrapper rules (host side) =====
ifeq ($(IN_DOCKER),1)

# --- Build inside container ---
$(KERNEL): $(KERNEL_OBJS) arch/$(ISA)/boot/linker.ld
	$(CC) -o $@ $(KERNEL_OBJS) $(LDFLAGS)

# Compile rules (inside container)
$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@) $(dir $(BUILD_DIR)/$*.d)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

-include $(KERNEL_DEPS)

kernel: $(KERNEL)

iso: kernel grub.cfg
	mkdir -p isodir/boot/grub
	cp $(KERNEL) isodir/boot/kfs.bin
	cp grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) isodir --modules="multiboot normal configfile" --compress=xz

clean:
	rm -rf isodir $(ISO) $(BUILD_DIR)

fclean: clean
	rm -f $(KERNEL)

re: fclean all

else

# --- Wrapper: run the same targets inside Docker ---
kernel: ensure-image
	@$(DOCKER_RUN) /bin/bash -lc 'IN_DOCKER=1 make kernel'

iso: ensure-image
	@$(DOCKER_RUN) /bin/bash -lc 'IN_DOCKER=1 make iso'

clean:
	@make clean -C test/
	@rm -rf isodir $(BUILD_DIR)

fclean: clean
	@rm -f $(KERNEL) $(ISO)

re: fclean all

endif

# ===== Run with QEMU (prefer host, fallback to container) =====
run: run-iso

run-iso: $(ISO)
	qemu-system-$(ISA) -cdrom $(ISO) -serial stdio

run-kernel: $(KERNEL)
	qemu-system-$(ISA) -kernel $(KERNEL) -serial stdio

# ===== Tests passthrough =====
test:
	@ make test -C test/

unit:
	@ make unit -C test/

integration:
	@ make integration -C test/

coverage:
	@ make coverage -C test/

fmt:
	@ docker run -v ./:/work -w /work ubuntu:24.04 bash -c \
		'export DEBIAN_FRONTEND=noninteractive \
		&& apt-get update \
		&& apt-get upgrade -y \
		&& apt-get install -y clang-format shfmt \
		&& clang-format -i -style="{BasedOnStyle: Microsoft, IndentWidth: 4, TabWidth: 4, UseTab: Always, InsertBraces: true}" $(KERNEL_SRCS_C) $(TEST_SRCS_C) $(KERNEL_SRCS_H) $(TEST_SRCS_H) \
		&& shfmt -w $(TEST_SRCS_SH)'

.PHONY: all kernel iso run run-iso run-kernel clean fclean re ensure-image test coverage fmt
