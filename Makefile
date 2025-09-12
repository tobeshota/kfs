# ===== User-facing targets =====
# - make          : kfs.iso まで作成
# - make kernel      : kfs.bin まで作成
# - make iso      : ISO を作成
# - make run      : qemu-system-$(ISA) -cdrom kfs.iso を実行
# - make run-kernel  : qemu-system-$(ISA) -kernel kfs.bin を実行

# ===== Docker image settings =====
IMAGE ?= smizuoch/kfs:1.0.1
DOCKER ?= docker
ISA	?= i386
PWD := $(shell pwd)
DOCKER_RUN = $(DOCKER) run --rm -v "$(PWD)":/work -w /work $(IMAGE)

# ===== Toolchain (used inside container) =====
CROSS   ?= i686-elf
CC      := $(CROSS)-gcc
CFLAGS  := -ffreestanding -Wall -Wextra -Werror -m32 -fno-builtin -fno-stack-protector -nostdlib -nodefaultlibs
LDFLAGS := -T arch/$(ISA)/boot/linker.ld -ffreestanding -m32 -fno-builtin -fno-stack-protector -nostdlib -nodefaultlibs

# Sources and objects
SRCS := $(shell find arch init -name '*.c' -o -name '*.S')
OBJS := $(patsubst %.S,%.o,$(patsubst %.c,%.o,$(SRCS)))

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
		  $(DOCKER) build -f arch/$(ISA)/compile.dockerfile -t $(IMAGE) . ); \
	fi; \
	echo "Using Docker image: $(IMAGE)"

# ===== Wrapper rules (host side) =====
ifeq ($(IN_DOCKER),1)

# --- Build inside container ---
$(KERNEL): $(OBJS) arch/$(ISA)/boot/linker.ld
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

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
	rm -f $(KERNEL) $(OBJS)

re: fclean all

else

# --- Wrapper: run the same targets inside Docker ---
kernel: ensure-image
	@$(DOCKER_RUN) /bin/bash -lc 'IN_DOCKER=1 make kernel'

iso: ensure-image
	@$(DOCKER_RUN) /bin/bash -lc 'IN_DOCKER=1 make iso'

clean:
	@rm -rf isodir $(OBJS)

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

.PHONY: all kernel iso run run-iso run-kernel clean fclean re ensure-image test