$(shell [ -f ../../.env ] || cp -a ../../.env.sample ../../.env)
include ../../.env
export

# ===== Docker image settings =====
IMAGE          ?= kfs-$(ISA)-toolchain
DOCKER         ?= docker
ISA	           ?= i386
ROOT_DIR       := $(shell cd ../.. && pwd)
DOCKER_RUN     = $(DOCKER) run --platform $(DOCKER_PLATFORM) --rm -u $(shell id -u):$(shell id -g) -v "$(ROOT_DIR)":/work -w /work/test/unit $(IMAGE)

# ===== Toolchain (used inside container) =====
CROSS          ?= i686-elf
CC             := $(CROSS)-gcc
INCLUDE_DIRS   = ../../include ../../include/kfs ../../include/asm-i386 .
CFLAGS         = $(addprefix -I,$(INCLUDE_DIRS)) -ffreestanding -Wall -Wextra -Werror -m32 -fno-builtin -fno-stack-protector -nostdlib -nodefaultlibs -nostdinc
LDFLAGS        = -T ../../arch/$(ISA)/boot/linker.ld -ffreestanding -m32 -fno-builtin -fno-stack-protector -nostdlib -nodefaultlibs -nostdinc -no-pie
DEPFLAGS       = -MMD -MP -MF $(BUILD_DIR)/$*.d

# Build directories
BUILD_DIR      := build/obj
LOGDIR         := coverage/log
REPORT_DIR     := coverage/report

# Coverage
LCOV           := lcov --rc lcov_branch_coverage=1
GENHTML        := genhtml --rc lcov_branch_coverage=1
COVERAGE_FILES := $(shell find $(BUILD_DIR) -name '*.gcno' -o -name '*.gcda' 2>/dev/null)

# Sources and objects
# Exclude build/ and coverage/ directories from source search
SRCS_C        := $(shell find ../../ -name '*.c' ! -path '*/build/*' ! -path '*/coverage/*')
COVERAGE_C    := ./coverage/coverage.c
BOOTSTRAP     := $(shell find ./arch/ -name "boot*.S")
SRCS_S        := $(shell find ../../ -name '*.S' ! -name "*boot*.S" ! -path '*/build/*')
OBJS          := $(patsubst ../../%,$(BUILD_DIR)/%,$(patsubst %.c,%.o,$(SRCS_C))) \
                 $(patsubst ../../%,$(BUILD_DIR)/%,$(patsubst %.S,%.o,$(SRCS_S))) \
                 $(patsubst ./%,$(BUILD_DIR)/%,$(patsubst %.S,%.o,$(BOOTSTRAP))) \
                 $(patsubst ./%.c,$(BUILD_DIR)/%.o,$(COVERAGE_C))
DEPS          := $(patsubst %.o,%.d,$(OBJS))
KERNEL        := testImage
ISO_BIOS      := test-bios.iso
ISO_UEFI      := test-uefi.iso
ISO           := $(ISO_BIOS)

ifeq ($(strip $(BOOT_MODE)),UEFI)
BOOT_ISO_TARGET := iso-uefi
BOOT_RUN_TARGET := run-iso-uefi
BOOT_UNIT_TARGET := unit-uefi
else
BOOT_ISO_TARGET := iso-bios
BOOT_RUN_TARGET := run-iso-bios
BOOT_UNIT_TARGET := unit-bios
endif

# Coverage build flag - when set, use instrumented sources from build/coverage
COVERAGE_BUILD ?= 0

# ===== Default =====
all: unit

# ===== Ensure Docker image (local build only) =====
ensure-image:
	@ set -e; \
	if ! $(DOCKER) image inspect $(IMAGE) >/dev/null 2>&1; then \
		echo "Building local image from arch/$(ISA)/compile.dockerfile..."; \
		$(DOCKER) build --platform $(DOCKER_PLATFORM) -f ../../arch/$(ISA)/compile.dockerfile -t $(IMAGE) .; \
	fi; \
	echo "Using Docker image: $(IMAGE)"

# ===== Wrapper rules (host side) =====
ifeq ($(IN_DOCKER),1)

# --- Build inside container ---
$(KERNEL): $(OBJS) ../../arch/$(ISA)/boot/linker.ld
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

# Compile rules (inside container)
$(BUILD_DIR)/%.o: ../../%.S
	@ mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# For C files, check if instrumented version exists in build/coverage when COVERAGE_BUILD=1
$(BUILD_DIR)/%.o: ../../%.c
	@ mkdir -p $(dir $@) $(dir $(BUILD_DIR)/$*.d)
	@ SRC_FILE="$<"; \
	if [ "$(COVERAGE_BUILD)" = "1" ] && [ -f "build/coverage/$*.c" ]; then \
		echo "Using instrumented: build/coverage/$*.c"; \
		SRC_FILE="build/coverage/$*.c"; \
	fi; \
	echo '$(CC) $(CFLAGS) $(DEPFLAGS) -c "'"$$SRC_FILE"'" -o $@'; \
	$(CC) $(CFLAGS) $(DEPFLAGS) -c "$$SRC_FILE" -o $@

$(BUILD_DIR)/%.o: ./%.S
	@ mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Coverage support files
$(BUILD_DIR)/coverage/%.o: ./coverage/%.c
	@ mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

-include $(DEPS)

kernel: $(KERNEL)

iso-bios: kernel ./grub-bios.cfg
	mkdir -p isodir-bios/boot/grub
	cp $(KERNEL) isodir-bios/boot/
	cp ./grub-bios.cfg isodir-bios/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO_BIOS) isodir-bios --modules="multiboot multiboot2 normal configfile part_msdos part_gpt" --compress=xz

iso-uefi: kernel ./grub-uefi.cfg
	mkdir -p isodir-uefi/boot/grub
	cp $(KERNEL) isodir-uefi/boot/
	cp ./grub-uefi.cfg isodir-uefi/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO_UEFI) isodir-uefi --modules="multiboot multiboot2 normal configfile part_msdos part_gpt" --compress=xz

iso: $(BOOT_ISO_TARGET)

unit: $(BOOT_UNIT_TARGET)

unit-bios: iso-bios
	$(MAKE) run-iso-bios

unit-uefi: iso-uefi
	$(MAKE) run-iso-uefi

else

define ensure_image
	@ if ! $(DOCKER) image inspect $(IMAGE) >/dev/null 2>&1; then \
		echo "Building Docker image from arch/$(ISA)/compile.dockerfile..."; \
		$(DOCKER) build --platform $(DOCKER_PLATFORM) -f ../../arch/$(ISA)/compile.dockerfile -t $(IMAGE) .; \
	fi
	@echo "Using Docker image: $(IMAGE)"
endef

$(KERNEL): $(SRCS_C) $(SRCS_S) $(BOOTSTRAP) $(COVERAGE_C) ../../arch/$(ISA)/boot/linker.ld
	$(call ensure_image)
	@ $(DOCKER_RUN) /bin/bash -lc 'IN_DOCKER=1 COVERAGE_BUILD=$(COVERAGE_BUILD) make -j$(shell nproc) kernel'

kernel: $(KERNEL)

$(ISO_BIOS): $(KERNEL) ./grub-bios.cfg
	$(call ensure_image)
	@ $(DOCKER_RUN) /bin/bash -lc 'IN_DOCKER=1 COVERAGE_BUILD=$(COVERAGE_BUILD) make -j$(shell nproc) iso-bios'

iso-bios: $(ISO_BIOS)

$(ISO_UEFI): $(KERNEL) ./grub-uefi.cfg
	$(call ensure_image)
	@ $(DOCKER_RUN) /bin/bash -lc 'IN_DOCKER=1 COVERAGE_BUILD=$(COVERAGE_BUILD) make -j$(shell nproc) iso-uefi'

iso-uefi: $(ISO_UEFI)

iso: $(BOOT_ISO_TARGET)

unit: $(BOOT_UNIT_TARGET)

unit-bios: $(ISO_BIOS)
	@ $(MAKE) run-iso-bios

unit-uefi: $(ISO_UEFI)
	@ $(MAKE) run-iso-uefi

endif

clean:
	@ rm -rf isodir-bios isodir-uefi build $(LOGDIR) $(REPORT_DIR)

fclean: clean
	@ rm -f $(KERNEL) $(ISO_BIOS) $(ISO_UEFI)

re: fclean all

# Log / report directories (needed by run-iso-bios on both host and container)
$(LOGDIR):
	@ mkdir -p $(LOGDIR)

$(REPORT_DIR):
	@ mkdir -p $(REPORT_DIR)

# ===== Run with QEMU (prefer host, fallback to container) =====
run: $(BOOT_RUN_TARGET)

# Bashと明示した理由はPIPESTATUSがBash特有の機能であるため
run-iso-bios: $(ISO_BIOS) | $(LOGDIR) $(REPORT_DIR)
	@ bash -c \
	'set -e; \
	LOGFILE=$$([ "$(COVERAGE_BUILD)" = "1" ] && echo "coverage/log/qemu_serial.log" || echo "coverage/log/qemu_serial.log"); \
	{ qemu-system-x86_64 \
		-display none \
		-device isa-debug-exit,iobase=0xf4,iosize=0x04 \
		-no-reboot \
		-cdrom $(ISO_BIOS) \
		-serial stdio 2>&1 | tee $$LOGFILE; } ; \
	raw_status=$${PIPESTATUS[0]}; \
	decoded_status=$$(( (raw_status - 1) / 2 )); \
	echo "qemu exit raw=$$raw_status decoded_status=$$decoded_status"; \
	if ! grep -q "== SUMMARY: .*all passed ==" "$$LOGFILE"; then \
		echo "unit test summary not found or not all tests passed"; \
		exit 1; \
	fi; \
	exit $$decoded_status'


# QEMU上でUEFIで起動する
OVMF_FD ?= $(shell find /usr/share/ovmf /usr/share/OVMF /usr/share/qemu /opt/homebrew /usr/local \( -name "OVMF.fd" -o -name "edk2-x86_64-code.fd" \) 2>/dev/null | head -1)
run-iso-uefi: $(ISO_UEFI) | $(LOGDIR) $(REPORT_DIR)
	@ bash -c \
	'set -e; \
	if [ -z "$(OVMF_FD)" ] || [ ! -r "$(OVMF_FD)" ]; then \
		echo "OVMF_FD is not set or not readable: $(OVMF_FD)"; \
		exit 1; \
	fi; \
	LOGFILE=$$([ "$(COVERAGE_BUILD)" = "1" ] && echo "coverage/log/qemu_serial.log" || echo "coverage/log/qemu_serial.log"); \
	{ qemu-system-x86_64 \
		-display none \
		-device isa-debug-exit,iobase=0xf4,iosize=0x04 \
		-no-reboot \
		-drive if=pflash,format=raw,readonly=on,file=$(OVMF_FD) \
		-cdrom $(ISO_UEFI) \
		-serial stdio 2>&1 | tee $$LOGFILE; } ; \
	raw_status=$${PIPESTATUS[0]}; \
	decoded_status=$$(( (raw_status - 1) / 2 )); \
	echo "qemu exit raw=$$raw_status decoded_status=$$decoded_status"; \
	if ! grep -q "== SUMMARY: .*all passed ==" "$$LOGFILE"; then \
		echo "unit test summary not found or not all tests passed"; \
		exit 1; \
	fi; \
	exit $$decoded_status'

# Now builds with instrumented sources from build/coverage without modifying production code
coverage: fclean coverage-instrument
	@ echo "Building and running tests with coverage (using instrumented sources from build/coverage)..."
	@ $(MAKE) unit COVERAGE_BUILD=1
	@ echo "Generating coverage summary report..."
	@ ./coverage/generate_coverage_summary.sh
	@ echo ""
	@ echo "Generating diff-style coverage report..."
	@ ./coverage/generate_diff_report.sh
	@ echo ""
	@ tail -n +$$(grep -n "^=== Summary ===" coverage/log/summary.txt | cut -d: -f1) coverage/log/summary.txt

# Instrument production code before building for coverage
coverage-instrument:
	@ echo "Instrumenting production code..."
	@ python3 coverage/insert_coverage_func.py ../../ build/coverage/ coverage/log/coverage_manifest.txt

.PHONY: all iso run run-iso-bios run-iso-uefi clean fclean re ensure-image unit unit-bios unit-uefi test coverage coverage-instrument
