# Makefile for the Midi-Fader Bootloader
#
# Kevin Cuzner
#

#
# This Makefile generates the following targets:
# 
# bin/midi-fader-bootloader.elf
#
# bin/midi-fader-bootloader.[bin|hex]
#  - Does not contain a storage segment
#

include ../common/common-top.mak

PROJECT = midi-fader-bootloader

# Project Structure
CSRCDIRS = src ../common/src ../cmsis/src
SSRCDIRS = src ../common/src
GENSRCDIRS = src ../common/src
INCDIRS = include ../cmsis/include ../common/include
LDINCDIRS = ../
SCRIPTDIR = ../scripts

# Sources
GENERATE = USB_DESCRIPTOR STORAGE

# Linker
LSCRIPT = STM32F042X6_Bootloader.ld

# Flag modifications
OBJCOPY_FLAGS = --remove-section .storage
GCFLAGS += -DUSB_DEBUG

all:: $(BINDIR)/$(PROJECT).bin $(BINDIR)/$(PROJECT).hex

include ../common/common-bot.mak

