include ../../.env
export

# ===== Docker image settings =====
IMAGE          ?= smizuoch/kfs:1.0.2
DOCKER         ?= docker
ISA	           ?= i386
ROOT_DIR       := $(shell cd ../.. && pwd)
DOCKER_RUN     = $(DOCKER) run --platform $(DOCKER_PLATFORM) --rm -v "$(ROOT_DIR)":/work -w /work/test/unit $(IMAGE)

# ===== Toolchain (used inside container) =====
CROSS          ?= i686-elf
CC             := $(CROSS)-gcc
INCLUDE_DIRS   = ../../include ../../include/kfs ../../include/asm-i386 .
CFLAGS         = $(addprefix -I,$(INCLUDE_DIRS)) -ffreestanding -Wall -Wextra -Werror -m32 -fno-builtin -fno-stack-protector -nostdlib -nodefaultlibs -nostdinc
LDFLAGS        = -T ./arch/$(ISA)/boot/linker_unit_test.ld -ffreestanding -m32 -fno-builtin -fno-stack-protector -nostdlib -nodefaultlibs -nostdinc
DEPFLAGS       := -MMD -MP -MF $(BUILD_DIR)/$*.d

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
COVERAGE_C    := ./coverage/simple_coverage.c
BOOTSTRAP     := $(shell find ./arch/ -name "boot*.S")
SRCS_S        := $(shell find ../../ -name '*.S' ! -name "*boot*.S" ! -path '*/build/*')
OBJS          := $(patsubst ../../%,$(BUILD_DIR)/%,$(patsubst %.c,%.o,$(SRCS_C))) \
                 $(patsubst ../../%,$(BUILD_DIR)/%,$(patsubst %.S,%.o,$(SRCS_S))) \
                 $(patsubst ./%,$(BUILD_DIR)/%,$(patsubst %.S,%.o,$(BOOTSTRAP))) \
                 $(patsubst ./%.c,$(BUILD_DIR)/%.o,$(COVERAGE_C))
DEPS          := $(patsubst %.o,%.d,$(OBJS))
KERNEL        := kfs_unit_test.bin
ISO           := kfs_unit_test.iso

# Coverage build flag - when set, use instrumented sources from build/coverage
COVERAGE_BUILD ?= 0

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
$(KERNEL): $(OBJS) ./arch/$(ISA)/boot/linker_unit_test.ld
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

# Compile rules (inside container)
$(BUILD_DIR)/%.o: ../../%.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# For C files, check if instrumented version exists in build/coverage when COVERAGE_BUILD=1
$(BUILD_DIR)/%.o: ../../%.c
	@mkdir -p $(dir $@) $(dir $(BUILD_DIR)/$*.d)
	@if [ "$(COVERAGE_BUILD)" = "1" ] && [ -f "build/coverage/$*.c" ]; then \
		echo "Using instrumented: build/coverage/$*.c"; \
		$(CC) $(CFLAGS) $(DEPFLAGS) -c "build/coverage/$*.c" -o $@; \
	else \
		$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@; \
	fi

$(BUILD_DIR)/%.o: ./%.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Coverage support files
$(BUILD_DIR)/coverage/%.o: ./coverage/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

# Log directory
$(LOGDIR):
	@mkdir -p $(LOGDIR)

# Report directory
$(REPORT_DIR):
	@mkdir -p $(REPORT_DIR)

-include $(DEPS)

kernel: $(KERNEL)

iso: kernel ./grub.cfg
	mkdir -p isodir/boot/grub
	cp $(KERNEL) isodir/boot/
	cp ./grub.cfg isodir/boot/grub/
	grub-mkrescue -o $(ISO) isodir --modules="multiboot normal configfile" --compress=xz

unit: iso run

clean:
	@rm -rf isodir build $(LOGDIR) $(REPORT_DIR)

fclean: clean
	@rm -f $(KERNEL) $(ISO)

re: fclean all

else

# --- Wrapper: run the same targets inside Docker ---
kernel: ensure-image
	@$(DOCKER_RUN) /bin/bash -lc 'IN_DOCKER=1 COVERAGE_BUILD=$(COVERAGE_BUILD) make kernel'

iso: ensure-image
	@$(DOCKER_RUN) /bin/bash -lc 'IN_DOCKER=1 COVERAGE_BUILD=$(COVERAGE_BUILD) make iso'

unit: ensure-image
	@$(DOCKER_RUN) /bin/bash -lc 'IN_DOCKER=1 COVERAGE_BUILD=$(COVERAGE_BUILD) make unit'

clean:
	@rm -rf isodir build $(LOGDIR) $(REPORT_DIR)

fclean: clean
	@rm -f $(KERNEL) $(ISO)

re: fclean all

endif

# ===== Run with QEMU (prefer host, fallback to container) =====
run: run-iso

# Bashと明示した理由はPIPESTATUSがBash特有の機能であるため
run-iso: $(ISO) | $(LOGDIR) $(REPORT_DIR)
	@ bash -c \
	'set -e; \
	LOGFILE=$$([ "$(COVERAGE_BUILD)" = "1" ] && echo "coverage/log/qemu_serial.log" || echo "coverage/log/qemu_serial.log"); \
	{ qemu-system-$(ISA) \
		-display none \
		-device isa-debug-exit,iobase=0xf4,iosize=0x04 \
		-cdrom $(ISO) \
		-serial stdio 2>&1 | tee $$LOGFILE; } ; \
	raw_status=$${PIPESTATUS[0]}; \
	decoded_status=$$(( (raw_status - 1) / 2 )); \
	echo "qemu exit raw=$$raw_status decoded_status=$$decoded_status"; \
	exit $$decoded_status'

# Updated to use simple_coverage instead of lcov/gcov
# Now builds with instrumented sources from build/coverage without modifying production code
coverage: fclean coverage-instrument
	@echo "Building and running tests with coverage (using instrumented sources from build/coverage)..."
	@$(MAKE) unit COVERAGE_BUILD=1
	@echo "Generating coverage summary report..."
	@./coverage/coverage.sh
	@echo ""
	@echo "Generating diff-style coverage report..."
	@./coverage/generate_diff_report.sh
	@echo ""
	@tail -n +$$(grep -n "^=== Summary ===" coverage/log/summary.diff | cut -d: -f1) coverage/log/summary.diff

# Instrument production code before building for coverage
coverage-instrument:
	@echo "Instrumenting production code..."
	@python3 coverage/insert_coverage_func.py ../../ build/coverage/ coverage/log/coverage_manifest.txt

.PHONY: all kernel iso run run-iso clean fclean re ensure-image unit test coverage coverage-instrument
