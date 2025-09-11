# kfs.bin(カーネル) と kfs.iso(ブート可能ISO) をDocker上で作成する

# ===== User-facing targets =====
# - make          : kfs.iso まで作成
# - make bin      : kfs.bin まで作成
# - make iso      : ISO を作成
# - make run      : qemu-system-i386 -cdrom kfs.iso を実行
# - make run-bin  : qemu-system-i386 -kernel kfs.bin を実行

# ===== Docker image settings =====
IMAGE ?= smizuoch/kfs:1.0.1
DOCKER ?= docker
PWD := $(shell pwd)
DOCKER_RUN = $(DOCKER) run --rm -v "$(PWD)":/work -w /work $(IMAGE)

# ===== Toolchain (used inside container) =====
CROSS   ?= i686-elf
CC      := $(CROSS)-gcc
CFLAGS  := -std=gnu99 -ffreestanding -O2 -Wall -Wextra -m32
LDFLAGS := -T arch/i386/boot/linker.ld -ffreestanding -O2 -nostdlib -m32
LDLIBS  := -lgcc

OBJS := \
  arch/i386/boot/boot.o \
  init/main.o

KERNEL := kfs.bin
ISO    := kfs.iso

.PHONY: all bin iso run run-bin clean check ensure-image docker-fallback \
	test docker-clean

# ===== Default =====
all: iso

# ===== Ensure Docker image (pull or build fallback) =====
ensure-image:
	@set -e; \
	if ! $(DOCKER) image inspect $(IMAGE) >/dev/null 2>&1; then \
		( $(DOCKER) pull $(IMAGE) >/dev/null 2>&1 ) || \
		( echo "Pull failed. Building local image from i686-elf-gcc.dockerfile..."; \
		  $(DOCKER) build -f i686-elf-gcc.dockerfile -t $(IMAGE) . ); \
	fi; \
	echo "Using Docker image: $(IMAGE)"

# ===== Wrapper rules (host side) =====
ifeq ($(IN_DOCKER),1)

# --- Build inside container ---
$(KERNEL): $(OBJS) arch/i386/boot/linker.ld
	$(CC) -o $@ $(OBJS) $(LDFLAGS) $(LDLIBS)

arch/i386/boot/boot.o: arch/i386/boot/boot.S
	$(CC) $(CFLAGS) -c $< -o $@

init/main.o: init/main.c
	$(CC) $(CFLAGS) -c $< -o $@

bin: $(KERNEL)

iso: bin grub.cfg
	mkdir -p isodir/boot/grub
	cp $(KERNEL) isodir/boot/kfs.bin
	cp grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) isodir

check: $(KERNEL)
	@command -v grub-file >/dev/null 2>&1 && \
	  grub-file --is-x86-multiboot $(KERNEL) && echo "multiboot confirmed" || \
	  echo "(grub-file not found or not multiboot)" || true

clean:
	rm -rf isodir $(ISO)

fclean: clean
	rm -f $(KERNEL) $(OBJS)

else

# --- Wrapper: run the same targets inside Docker ---
bin: ensure-image
	@$(DOCKER_RUN) /bin/bash -lc 'IN_DOCKER=1 make bin'

iso: ensure-image
	@$(DOCKER_RUN) /bin/bash -lc 'IN_DOCKER=1 make iso'

check: ensure-image
	@$(DOCKER_RUN) /bin/bash -lc 'IN_DOCKER=1 make check'

clean:
	@rm -rf isodir $(OBJS)

fclean: clean
	@rm -f $(KERNEL) $(ISO)

endif

# ===== Run with QEMU (prefer host, fallback to container) =====
run: $(ISO)
	@if command -v qemu-system-i386 >/dev/null 2>&1; then \
	  qemu-system-i386 -cdrom $(ISO) -serial stdio; \
	else \
	  echo "qemu-system-i386 not found on host. Trying in Docker..."; \
	  $(DOCKER_RUN) qemu-system-i386 -cdrom $(ISO) -serial stdio; \
	fi

run-bin: $(KERNEL)
	@if command -v qemu-system-i386 >/dev/null 2>&1; then \
	  qemu-system-i386 -kernel $(KERNEL) -serial stdio; \
	else \
	  echo "qemu-system-i386 not found on host. Trying in Docker..."; \
	  $(DOCKER_RUN) qemu-system-i386 -kernel $(KERNEL) -serial stdio; \
	fi

# ===== Tests passthrough =====
test:
	@ make -C test/
