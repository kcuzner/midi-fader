# Makefile for the USB Midi-Fader firmware
#
# Kevin Cuzner
#

#
# Settings
#

PROJECT = midi-fader

# Project Structure
SRCDIRS = src cmsis/src
BINDIR = bin
OBJDIR = obj
INCDIRS = include cmsis/include

# Project target
CPU = cortex-m0

# Sources
SRC = $(foreach DIR,$(SRCDIRS),$(wildcard $(DIR)/*.c))
ASM = $(foreach DIR,$(SRCDIRS),$(wildcard $(DIR)/*.s))

# Include directories
INCLUDE  = $(foreach DIR,$(INCDIRS),-I$(DIR))

# Linker
LSCRIPT = STM32F042X6.ld

# C Flags
GCFLAGS  = -std=c99 -Wall -Os -fno-common -mthumb -mcpu=$(CPU) -DSTM32F042x6 --specs=nosys.specs --specs=nano.specs -g -Wa,-ahlms=$(addprefix $(OBJDIR)/,$(notdir $(<:.c=.lst)))
GCFLAGS += $(INCLUDE)
LDFLAGS += -T$(LSCRIPT) -mthumb -mcpu=$(CPU) --specs=nosys.specs --specs=nano.specs -Wl,-Map,$(BINDIR)/$(PROJECT).map -Wl,--gc-sections
ASFLAGS += -mcpu=$(CPU)

# Flashing
OCDFLAGS = -f openocd/openocd.cfg

# Tools
CC = arm-none-eabi-gcc
AS = arm-none-eabi-as
AR = arm-none-eabi-ar
LD = arm-none-eabi-ld
OBJCOPY = arm-none-eabi-objcopy
SIZE = arm-none-eabi-size --format=SysV
OBJDUMP = arm-none-eabi-objdump
GDB = arm-none-eabi-gdb
OCD = openocd

RM = rm -rf

#
# Build process
#

OBJ := $(addprefix $(OBJDIR)/,$(notdir $(SRC:.c=.o)))
OBJ += $(addprefix $(OBJDIR)/,$(notdir $(ASM:.s=.o)))
DEP := $(addprefix $(OBJDIR)/,$(notdir $(SRC:.c=.d)))

all:: $(BINDIR)/$(PROJECT).bin $(BINDIR)/$(PROJECT).hex

Build: $(BINDIR)/$(PROJECT).bin

install: $(BINDIR)/$(PROJECT).bin $(BINDIR)/openocd.pid
	cat openocd/flash.cfg | nc localhost 4444

gdb: $(BINDIR)/$(PROJECT).elf $(BINDIR)/openocd.pid
	$(GDB) -ex "target remote localhost:3333" $(BINDIR)/$(PROJECT).elf

stop:
	-echo shutdown | nc localhost 4444

macros:
	$(CC) $(GCFLAGS) -dM -E - < /dev/null

cleanBuild: clean

clean: stop
	$(RM) $(BINDIR)
	$(RM) $(OBJDIR)

size:
	$(SIZE) $(BINDIR)/$(PROJECT).elf

#
# Debug
#

$(BINDIR)/openocd.pid:
	openocd/run-openocd-server.sh $(OCD) $@
#
# Compilation
#

$(BINDIR)/$(PROJECT).hex: $(BINDIR)/$(PROJECT).elf
	$(OBJCOPY) -O ihex $(BINDIR)/$(PROJECT).elf $(BINDIR)/$(PROJECT).hex

$(BINDIR)/$(PROJECT).bin: $(BINDIR)/$(PROJECT).elf
	$(OBJCOPY) -O binary $(BINDIR)/$(PROJECT).elf $(BINDIR)/$(PROJECT).bin

$(BINDIR)/$(PROJECT).elf: $(OBJ) $(LSCRIPT)
	@mkdir -p $(dir $@)
	$(CC) $(OBJ) $(LDFLAGS) -o $(BINDIR)/$(PROJECT).elf
	$(OBJDUMP) -D $(BINDIR)/$(PROJECT).elf > $(BINDIR)/$(PROJECT).lst
	$(SIZE) $(BINDIR)/$(PROJECT).elf


# Generates compilation rules for the directory in $1
#
# Note to self: The double-dollar sign escapes the $ so that it doesn't get
# evaluated when this function is generated, but instead gets evaluated when
# Make is actually making.
define build_compile_rules
$$(OBJDIR)/%.o: $1/%.c Makefile
	@mkdir -p $$(dir $$@)
	$$(CC) $$(GCFLAGS) -MMD -c $$< -o $$@
$$(OBJDIR)/%.o: $1/%.s Makefile
	@mkdir -p $$(dir $$@)
	$$(AS) $$(ASFLAGS) -o $$@ $$<
endef

# Generate rules for each source directory
$(foreach DIR,$(SRCDIRS),$(eval $(call build_compile_rules,$(DIR))))

-include $(DEP)

