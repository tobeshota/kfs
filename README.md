<p align="center">
  <img src="Documentation/images/kfs_qemu.gif" width="420">
</p>

**kfs stands for _Kernel From Scratch_ 💻**  
**The goal is to accompany _processes_ from their birth to their volatilization.**

---

# Technical Specifications

### Target Architecture
- ISA: i386
- Platform: QEMU
- Toolchain: GCC cross-compiler (i686-elf)
- Linker: [Custom ELF script](arch/i386/boot/linker.ld) for [Higher Half Kernel](https://wiki.osdev.org/Higher_Half_Kernel)

### Memory Management
```.txt
Virtual Address Space          Physical RAM
───────────────────           ─────────────
0xFFFFFFFF ┐
           │ vmalloc
0xD0000000 ├
           │ Kernel Space      0x00400000 ┐
0xC0100000 ┼ (1GiB, Ring 0)    0x00100000 ├ Kernel (4MB)
0xC0000000 ┴────────────────→  0x00000000 ┘
0xBFFFFFFF ┐
           │ User Space
           │ (3GiB, Ring 3)
0x00000000 ┘
```

##### Allocators
- Physical: [`kmalloc()`](mm/slab.c#L228) / [`kfree()`](mm/slab.c#L290)
- Virtual: [`vmalloc()`](mm/vmalloc.c#L53) / [`vfree()`](mm/vmalloc.c#L180)
- Paging: [4KB pages allocation](mm/page_alloc.c#L239), [page direcotry](include/asm-i386/pgtable.h#L33), [page table](include/asm-i386/pgtable.h#L28)

### Interrupt and Exception Handling
- IDT: [256 entries](include/asm-i386/desc.h#L63) for exceptions and interrupts
- IRQ Controller: [Intel 8259A PIC](arch/i386/kernel/i8259.c#L120)

### Device Drivers
- Console: [VGA text mode](drivers/video/terminal.c)
- Serial Port: [16550 UART](drivers/char/serial.c)
- Keyboard: [PS/2 keyboard controller](drivers/tty/keyboard.c)

### Testing
- Unit Tests: [Custom framework](test/unit/unit_test_framework.h) with assertion macros
- Integration Tests: [QEMU-based](test/integration/integration_test.sh) serial port output validation
- Coverage: [Automated instrumentation](test/unit/coverage/coverage.h#L42) with [`COVERAGE_LINE()`](test/unit/coverage/coverage.h#L42) macro injection

# Usage

```shell
# Clone this repository
git clone https://github.com/tobeshota/kfs
# Change directory to this repository
cd kfs
# Build the kernel and generate a bootable ISO image with GRUB (kfs.iso)
make
# Boot from the generated ISO image in QEMU
make run
```
