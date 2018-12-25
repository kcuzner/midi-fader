# Makefile for the USB Midi-Fader firmware
#
# Kevin Cuzner
#

#
# This makefile has two purposes: Build the firmware and build binaries
# which contain various images. The outputs are as follows:
#
# bin/midi-fader.[bin|hex]: Flash images containing:
#   - The bootloader image (no storage) at 0x08000000
#   - The firmware's storage segment
#
# bin/midi-fader-firmware.[bin|hex}: Flash images containing:
#   - The firmware's image (no storage) at 0x08002000
#
# The intent is that the midi-fader files will be flashed to the
# microcontroller initially. Then, the midi-fader-firmware image will be
# programmed through the bootloader.
#
# The "install" target will completely erase the device and perform the
# full flash and bootloader update sequence.
#
# The "update" target will only perform the bootloader update sequence
# using the midi-fader-firmware image
#
# In addition, this makefile invokes a submake in the "bootloader"
# directory which builds the bootloader binaries. See that directory for
# details on the various images there. This Makefile uses the following
# bootloader targets:
#  - bootloader/bin/midi-fader-bootloader.bin: Used for generating
#  bin/midi-fader.bin.
#  - bootloader/bin/midi-fader-bootloader.elf: Added at 0x08000000 when
#  using GDB to give a full picture.
#


#
# Settings
#

include common/common-top.mak

PROJECT = midi-fader
BINARY = $(PROJECT)-firmware
BOOT_BINARY = $(PROJECT)-bootloader

# Project Structure
SRCDIRS = src cmsis/src common/src
GENSRCDIRS = src common/src
INCDIRS = include cmsis/include common/include
BOOTDIR = bootloader
BOOT_BINDIR = $(BOOTDIR)/bin

# Sources
GENERATE = USB_DESCRIPTOR STORAGE

# Linker
LSCRIPT = STM32F042X6.ld

# Flashing
OCDFLAGS = -f openocd/openocd.cfg

# Output manipulation
OBJCOPY_FLAGS = --remove-section .storage
BUILD_OBJCOPY_FLAGS = --add-section .bootloader=$(BOOT_BINDIR)/$(BOOT_BINARY).bin\
	--set-section-flags .bootloader=alloc,contents,load,readonly\
	--change-section-address .bootloader=0x08000000\
	--remove-section .text\
	--remove-section .rodata\
	--remove-section .init_array\
	--remove-section .fini_array\
	--remove-section .data

# Tools
GDB = arm-none-eabi-gdb

GCFLAGS += -DUSB_DEBUG

all:: $(BINDIR)/$(PROJECT).bin $(BINDIR)/$(PROJECT).hex $(BINDIR)/$(BINARY).bin $(BINDIR)/$(BINARY).hex

install: flash update

flash: $(BINDIR)/$(PROJECT).bin $(BINDIR)/openocd.pid
	cat openocd/flash.cfg | nc localhost 4444

update: $(BINDIR)/$(BINARY).hex
	$(PYTHON) $(SCRIPTDIR)/bootloader program $(BINDIR)/$(BINARY).hex

run:
	$(PYTHON) $(SCRIPTDIR)/bootloader abort

gdb: $(BINDIR)/openocd.pid
	$(GDB) -ex "target remote localhost:3333" -ex "add-symbol-file $(BOOT_BINDIR)/$(BOOT_BINARY).elf 0x08000000" $(BINDIR)/$(BINARY).elf

gdb-bootloader: $(BINDIR)/openocd.pid
	$(GDB) -ex "target remote localhost:3333" $(BOOT_BINDIR)/$(BOOT_BINARY).elf

start: $(BINDIR)/openocd.pid

stop:
	-echo shutdown | nc localhost 4444

clean: bootloaderClean stop

bootloaderClean:
	cd $(BOOTDIR) && make clean

#
# Debug
#

$(BINDIR)/openocd.pid:
	openocd/run-openocd-server.sh $(OCD) $@

#
# File building
#
$(BINDIR)/$(PROJECT).bin: $(BINDIR)/$(BINARY).elf $(BOOT_BINDIR)/$(BOOT_BINARY).elf
	$(OBJCOPY) -O binary $(BUILD_OBJCOPY_FLAGS)\
		$(BINDIR)/$(BINARY).elf $@
	
$(BINDIR)/$(PROJECT).hex: $(BINDIR)/$(BINARY).elf $(BOOT_BINDIR)/$(BOOT_BINARY).elf
	$(OBJCOPY) -O ihex $(BUILD_OBJCOPY_FLAGS)\
		$(BINDIR)/$(BINARY).elf $@

$(BOOT_BINDIR)/$(BOOT_BINARY).elf: FORCE
	cd $(BOOTDIR) && make

FORCE:


include common/common-bot.mak

