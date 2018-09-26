# Makefile for the USB Midi-Fader firmware
#
# Kevin Cuzner
#

#
# Settings
#

include common/common-top.mak

PROJECT = midi-fader

# Project Structure
SRCDIRS = src cmsis/src common/src
GENSRCDIRS = src common/src
INCDIRS = include cmsis/include common/include

# Sources
GENERATE = USB_DESCRIPTOR STORAGE

# Linker
LSCRIPT = STM32F042X6.ld

# Flashing
OCDFLAGS = -f openocd/openocd.cfg

GCFLAGS += -DUSB_DEBUG

all:: $(BINDIR)/$(PROJECT).bin $(BINDIR)/$(PROJECT).hex

install: $(BINDIR)/$(PROJECT).bin $(BINDIR)/openocd.pid
	cat openocd/flash.cfg | nc localhost 4444

gdb: $(BINDIR)/$(PROJECT).elf $(BINDIR)/openocd.pid
	$(GDB) -ex "target remote localhost:3333" $(BINDIR)/$(PROJECT).elf

stop:
	-echo shutdown | nc localhost 4444

clean: stop
#
# Debug
#

$(BINDIR)/openocd.pid:
	openocd/run-openocd-server.sh $(OCD) $@

include common/common-bot.mak

