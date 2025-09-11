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
CFLAGS  := -ffreestanding -Wall -Wextra -Werror -m32 -fno-builtin -fno-stack-protector -nostdlib -nodefaultlibs
LDFLAGS := -T arch/i386/boot/linker.ld -ffreestanding -m32 -fno-builtin -fno-stack-protector -nostdlib -nodefaultlibs
LDLIBS  := -lgcc

# Sources and objects
SRCS := $(shell find arch init -name '*.c' -o -name '*.S')
OBJS := $(patsubst %.S,%.o,$(patsubst %.c,%.o,$(SRCS)))

KERNEL := kfs.bin
ISO    := kfs.iso

.PHONY: all bin iso run run-bin clean fclean check ensure-image docker-fallback \
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

# Compile rules (inside container)
%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

bin: $(KERNEL)

iso: bin grub.cfg
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
bin: ensure-image
	@$(DOCKER_RUN) /bin/bash -lc 'IN_DOCKER=1 make bin'

iso: ensure-image
	@$(DOCKER_RUN) /bin/bash -lc 'IN_DOCKER=1 make iso'

clean:
	@rm -rf isodir $(OBJS)

fclean: clean
	@rm -f $(KERNEL) $(ISO)

re: fclean all

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
