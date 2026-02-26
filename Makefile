# ===== User-facing targets =====
# - make             : make iso と同じ
# - make kernel      : Image まで作成
# - make iso         : kfs.iso まで作成
# - make run         : qemu-system-$(ISA) -cdrom kfs.iso を実行
# - make run-kernel  : qemu-system-$(ISA) -kernel Image を実行

include .env
export

# PC スピーカー音声バックエンド（make run / make run-kernel 専用）
ifeq ($(shell uname -s),Darwin)
    QEMU_AUDIO ?= -audiodev coreaudio,id=snd -machine pc,pcspk-audiodev=snd
else
    QEMU_AUDIO ?=
endif

# ===== Docker image settings =====
IMAGE ?= $(ISA)-compile-toolchain
DOCKER ?= docker
ISA	?= i386
PWD := $(shell pwd)
DOCKER_RUN = $(DOCKER) run --platform $(DOCKER_PLATFORM) --rm -u $(shell id -u):$(shell id -g) -v "$(PWD)":/work -w /work $(IMAGE)

# ===== Toolchain (used inside container) =====
CROSS        ?= i686-elf
CC           := $(CROSS)-gcc
INCLUDE_DIRS := include include/kfs include/asm-i386
CFLAGS       := $(addprefix -I,$(INCLUDE_DIRS)) -ffreestanding -Wall -Wextra -Werror -m32 -fno-builtin -fno-stack-protector -nostdlib -nodefaultlibs -nostdinc
DEPFLAGS     = -MMD -MP -MF $(BUILD_DIR)/$*.d
LDFLAGS      := -T arch/$(ISA)/boot/linker.ld -ffreestanding -m32 -fno-builtin -fno-stack-protector -nostdlib -nodefaultlibs -nostdinc -no-pie

# Sources and objects
# Explicit kernel C sources (collect then filter out legacy *_test_shim.c that must not ship)
RAW_KERNEL_SRCS_C := $(shell find ./ -path ./test -prune -o -name '*.c' -print)
KERNEL_SRCS_C     := $(filter-out %_test_shim.c,$(RAW_KERNEL_SRCS_C))
KERNEL_SRCS_H     := $(shell find ./ -path ./test -prune -o -name '*.h' -print)
KERNEL_SRCS_S     := $(shell find ./ -path ./test -prune -o -name '*.S' -print)
TEST_SRCS_C       := $(shell find ./test -name '*.c' -print)
TEST_SRCS_H       := $(shell find ./test -name '*.h' -print)
TEST_SRCS_SH      := $(shell find ./test -name '*.sh' -print)

KERNEL_SRCS := $(KERNEL_SRCS_C) $(KERNEL_SRCS_S)
BUILD_DIR   := build/obj
KERNEL_OBJS := $(patsubst %.S,$(BUILD_DIR)/%.o,$(patsubst %.c,$(BUILD_DIR)/%.o,$(KERNEL_SRCS)))
KERNEL_DEPS := $(patsubst %.c,$(BUILD_DIR)/%.d,$(KERNEL_SRCS_C))

KERNEL   := Image
ISO_BIOS := kfs.iso
ISO_UEFI := kfs-uefi.iso

# ===== Default =====
all: iso-bios

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

iso: iso-bios

iso-bios: kernel grub-bios.cfg
	mkdir -p isodir-bios/boot/grub
	cp $(KERNEL) isodir-bios/boot/Image
	cp grub-bios.cfg isodir-bios/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO_BIOS) isodir-bios --modules="multiboot normal configfile" --compress=xz

iso-uefi: kernel grub-uefi.cfg
	mkdir -p isodir-uefi/boot/grub
	cp $(KERNEL) isodir-uefi/boot/Image
	cp grub-uefi.cfg isodir-uefi/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO_UEFI) isodir-uefi --modules="multiboot2 normal configfile part_msdos part_gpt" --compress=xz

else

define ensure_image
	@if ! $(DOCKER) image inspect $(IMAGE) >/dev/null 2>&1; then \
		echo "Building Docker image from arch/$(ISA)/compile.dockerfile..."; \
		$(DOCKER) build --platform $(DOCKER_PLATFORM) -f arch/$(ISA)/compile.dockerfile -t $(IMAGE) .; \
	fi
	@echo "Using Docker image: $(IMAGE)"
endef

$(KERNEL): $(KERNEL_SRCS_C) $(KERNEL_SRCS_S) $(KERNEL_SRCS_H) arch/$(ISA)/boot/linker.ld
	$(call ensure_image)
	@$(DOCKER_RUN) /bin/bash -lc 'IN_DOCKER=1 make -j$(shell nproc) kernel'

kernel: $(KERNEL)

iso: iso-bios

$(ISO_BIOS): $(KERNEL) grub-bios.cfg
	$(call ensure_image)
	@$(DOCKER_RUN) /bin/bash -lc 'IN_DOCKER=1 make -j$(shell nproc) iso-bios'

iso-bios: $(ISO_BIOS)

$(ISO_UEFI): $(KERNEL) grub-uefi.cfg
	$(call ensure_image)
	@$(DOCKER_RUN) /bin/bash -lc 'IN_DOCKER=1 make -j$(shell nproc) iso-uefi'

iso-uefi: $(ISO_UEFI)

endif

clean:
	@ rm -rf isodir-bios isodir-uefi $(BUILD_DIR)
	@ make clean -C test/

fclean: clean
	@ rm -f $(KERNEL) $(ISO_BIOS) $(ISO_UEFI)
	@ make fclean -C test/

re: fclean all

# ===== Run with QEMU (prefer host, fallback to container) =====
run: run-iso-bios

run-iso-bios: $(ISO_BIOS)
	qemu-system-$(ISA) -cdrom $(ISO_BIOS) -serial stdio $(QEMU_AUDIO)

run-kernel: $(KERNEL)
	qemu-system-$(ISA) -kernel $(KERNEL) -serial stdio $(QEMU_AUDIO)

# QEMU上でUEFIで起動する
# 備考: OVMFはQEMUパッケージ内に含まれる
OVMF_FD ?= $(shell find /usr/share/ovmf /usr/share/OVMF /usr/share/qemu /opt/homebrew /usr/local \( -name "OVMF.fd" -o -name "edk2-x86_64-code.fd" \) 2>/dev/null | head -1)
run-iso-uefi: $(ISO_UEFI)
	qemu-system-x86_64 \
		-drive if=pflash,format=raw,readonly=on,file=$(OVMF_FD) \
		-cdrom $(ISO_UEFI) \
		-serial stdio \
		-display curses \

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

.PHONY: all iso run run-iso-bios run-kernel run-iso-uefi clean fclean re ensure-image test unit integration coverage fmt
