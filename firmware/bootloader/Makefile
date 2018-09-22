# Makefile for the LED Wristwatch Firmware
#
# Kevin Cuzner
#

include ../common/common-top.mak

PROJECT = midi-fader-bootloader

# Project Structure
SRCDIRS = src ../common/src ../cmsis/src
GENSRCDIRS = src ../common/src
INCDIRS = include ../cmsis/include ../common/include
LDINCDIRS = ../
SCRIPTDIR = ../scripts

# Sources
GENERATE = USB_DESCRIPTOR STORAGE

# Linker
LSCRIPT = STM32F042X6_Bootloader.ld

# Flashing
OCDFLAGS = -f ../openocd/openocd.cfg

all:: $(BINDIR)/$(PROJECT).bin $(BINDIR)/$(PROJECT).hex

install: $(BINDIR)/$(PROJECT).bin $(BINDIR)/openocd.pid
	cat openocd/flash.cfg | nc localhost 4444

gdb: $(BINDIR)/$(PROJECT).elf $(BINDIR)/openocd.pid
	$(GDB) -ex "target remote localhost:3333" $(BINDIR)/$(PROJECT).elf

stop:
	echo shutdown | nc localhost 4444

clean: stop

# Debug

$(BINDIR)/openocd.pid:
	openocd/run-openocd-server.sh $(OCD) $@

include ../common/common-bot.mak

