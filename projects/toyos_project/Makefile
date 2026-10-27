# Toy OS - 32-bit x86 BIOS boot image build
# Run using GNU Make inside WSL (Linux).

CC       := gcc
NASM     := nasm
LD       := ld
OBJCOPY  := objcopy

CFLAGS   := -m32 -ffreestanding -fno-pie -fno-pic -fno-stack-protector -MMD -MP
LDFLAGS  := -m elf_i386 -T linker.ld

BOOT_SRC     := boot12.asm
BOOT_BIN     := boot12.bin
BOOT_BYTES   := 32768
KERNEL_BYTES := 16384
IMAGE        := os12.img

# Automatically include new kernel .c and .asm sources.
# Exclude all boot*.asm files: they belong to bootloader experiments,
# not to the linked 32-bit C kernel.
C_SRCS    := $(wildcard *.c)
ASM_SRCS  := $(filter-out boot%.asm,$(wildcard *.asm))
C_OBJS    := $(C_SRCS:.c=.o)
ASM_OBJS  := $(ASM_SRCS:.asm=.o)
OBJS      := kernel_entry.o $(filter-out kernel_entry.o,$(ASM_OBJS)) $(C_OBJS)
DEPFILES  := $(C_OBJS:.o=.d)

.DEFAULT_GOAL := all
.PHONY: all clean rebuild

all: $(IMAGE)

# Build C sources and generate dependency files for their #includes.
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Build 32-bit ELF object files from kernel assembly sources.
%.o: %.asm
	$(NASM) -f elf32 $< -o $@

kernel.elf: $(OBJS) linker.ld
	$(LD) $(LDFLAGS) $(OBJS) -o $@

kernel.bin: kernel.elf
	$(OBJCOPY) -O binary $< $@

# IMPORTANT: A too-large kernel must never be silently truncated.
kernel_padded.bin: kernel.bin
	@size=$$(wc -c < $<); \
	if [ "$$size" -gt $(KERNEL_BYTES) ]; then \
		echo "ERROR: kernel.bin is $$size bytes; boot12.asm currently loads only $(KERNEL_BYTES) bytes." >&2; \
		exit 1; \
	fi
	cp $< $@
	truncate -s $(KERNEL_BYTES) $@

$(BOOT_BIN): $(BOOT_SRC)
	$(NASM) -f bin $< -o $@
	@size=$$(wc -c < $@); \
	if [ "$$size" -ne $(BOOT_BYTES) ]; then \
		echo "ERROR: $(BOOT_BIN) must be $(BOOT_BYTES) bytes, but is $$size bytes." >&2; \
		exit 1; \
	fi

$(IMAGE): $(BOOT_BIN) kernel_padded.bin
	cat $^ > $@
	@echo "Built $(IMAGE) ($$(wc -c < $@) bytes)"

clean:
	rm -f *.o *.d kernel.elf kernel.bin kernel_padded.bin $(BOOT_BIN) $(IMAGE)

rebuild:
	$(MAKE) clean
	$(MAKE) all

-include $(DEPFILES)
