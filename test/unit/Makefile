include ../../.env
export

# ===== Docker image settings =====
IMAGE         ?= smizuoch/kfs:1.0.2
DOCKER        ?= docker
ISA	          ?= i386
ROOT_DIR      := $(shell cd ../.. && pwd)
DOCKER_RUN    = $(DOCKER) run --platform $(DOCKER_PLATFORM) --rm -v "$(ROOT_DIR)":/work -w /work/test/unit $(IMAGE)

# ===== Toolchain (used inside container) =====
CROSS         ?= i686-elf
CC            := $(CROSS)-gcc
INCLUDE_DIRS  = ../../include ../../include/kfs ../../include/asm-i386 .
CFLAGS        = $(addprefix -I,$(INCLUDE_DIRS)) -ffreestanding -Wall -Wextra -Werror -m32 -fno-builtin -fno-stack-protector -nostdlib -nodefaultlibs -nostdinc -O0 -g
# CFLAGS      += --coverage
LDFLAGS       = -T ../../arch/$(ISA)/boot/linker.ld -ffreestanding -m32 -fno-builtin -fno-stack-protector -nostdlib -nodefaultlibs -nostdinc -O0 -g
# LDFLAGS     += --coverage
DEPFLAGS      := -MMD -MP -MF $(BUILD_DIR)/$*.d

# Build directories
BUILD_DIR     := build/obj
COVERAGE_DIR  := _artifacts/coverage

# Sources and objects
# Explicit kernel C sources (collect then filter out legacy *_test_shim.c that must not ship)
SRCS_C        := $(shell find ../../ -name '*.c')
BOOTSTRAP     := $(shell find ./arch/ -name "boot*.S")
SRCS_S        := $(shell find ../../ -name '*.S' ! -name "*boot*.S")
OBJS          := $(patsubst ../../%,$(BUILD_DIR)/%,$(patsubst %.c,%.o,$(SRCS_C))) \
                 $(patsubst ../../%,$(BUILD_DIR)/%,$(patsubst %.S,%.o,$(SRCS_S))) \
                 $(patsubst ./%,$(BUILD_DIR)/%,$(patsubst %.S,%.o,$(BOOTSTRAP)))
DEPS          := $(patsubst %.o,%.d,$(OBJS))
KERNEL        := kfs_unit_test.bin
ISO           := kfs_unit_test.iso

# ===== Default =====
all: unit

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
$(KERNEL): $(OBJS) ../../arch/$(ISA)/boot/linker.ld
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

# Compile rules (inside container)
$(BUILD_DIR)/%.o: ../../%.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: ../../%.c
	@mkdir -p $(dir $@) $(dir $(BUILD_DIR)/$*.d)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: ./%.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

-include $(DEPS)

kernel: $(KERNEL)

iso: kernel ./grub.cfg
	mkdir -p isodir/boot/grub
	cp $(KERNEL) isodir/boot/
	cp ./grub.cfg isodir/boot/grub/
	grub-mkrescue -o $(ISO) isodir --modules="multiboot normal configfile" --compress=xz

unit: fclean iso

clean:
	@rm -rf isodir $(BUILD_DIR) $(COVERAGE_DIR)

fclean: clean
	@rm -f $(KERNEL) $(ISO)

re: fclean all

else

# --- Wrapper: run the same targets inside Docker ---
kernel: ensure-image
	@$(DOCKER_RUN) /bin/bash -lc 'IN_DOCKER=1 make kernel'

iso: ensure-image
	@$(DOCKER_RUN) /bin/bash -lc 'IN_DOCKER=1 make iso'

unit: ensure-image
	@$(DOCKER_RUN) /bin/bash -lc 'IN_DOCKER=1 make unit'
	make run

clean:
	@rm -rf isodir $(BUILD_DIR) $(COVERAGE_DIR)

fclean: clean
	@rm -f $(KERNEL) $(ISO)

re: fclean all

endif

# ===== Run with QEMU (prefer host, fallback to container) =====
run: run-iso

run-iso: $(ISO)
	@set -e; \
	qemu-system-$(ISA) -device isa-debug-exit,iobase=0xf4,iosize=0x04 -cdrom $(ISO) -serial stdio -display none -no-reboot && raw_status=$$? || raw_status=$$?; \
	decoded_status=$$(( (raw_status - 1) / 2 )); \
	echo "qemu exit raw=$$raw_status decoded_status=$$decoded_status"; \
	exit $$decoded_status

# ===== Tests passthrough =====

# $(COVERAGE_DIR)を都度再生成している理由は，カバレッジの測定が以前の結果に影響されないようにするため
coverage: unit
	@ $(DOCKER_SH) "\
		rm -rf $(COVERAGE_DIR) && mkdir -p $(COVERAGE_DIR)/html; \
		$(LCOV) --capture --directory $(BUILD_DIR) --output-file $(COVERAGE_DIR)/coverage.info; \
		$(LCOV) --remove $(COVERAGE_DIR)/coverage.info '/work/test/*' '/usr/*' '*.h' --output-file $(COVERAGE_DIR)/coverage.filtered.info; \
		$(GENHTML) $(COVERAGE_DIR)/coverage.filtered.info --output-directory $(COVERAGE_DIR)/html; \
		$(LCOV) --summary $(COVERAGE_DIR)/coverage.filtered.info"
	@ echo "\nCoverage HTML:\n./test/$(COVERAGE_DIR)/html/index.html\n"

.PHONY: all kernel iso run run-iso clean fclean re ensure-image unit test coverage
