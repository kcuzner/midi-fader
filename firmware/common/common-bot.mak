# Common build elements for this project
#
# It is intended that this is used for both the bootloader project and the
# regular firmware project. There's a lot of shared logic.

#
# Build process
#

GENERATE_USB_DESCRIPTOR=USB_DESCRIPTOR
GENERATE_USB_DESCRIPTOR_SRC=_gen_usb_desc.c
GENERATE_USB_DESCRIPTOR_HDR=_gen_usb_desc.h

GENERATE_STORAGE=STORAGE
GENERATE_STORAGE_SRC=_gen_storage.c
GENERATE_STORAGE_HDR=_gen_storage.h

OBJ := $(addprefix $(OBJDIR)/,$(notdir $(SRC:.c=.o)))
OBJ += $(addprefix $(OBJDIR)/,$(notdir $(ASM:.s=.o)))
ifneq ($(filter $(GENERATE), $(GENERATE_USB_DESCRIPTOR)),)
	GEN_OBJ += $(GENDIR)/$(GENERATE_USB_DESCRIPTOR_SRC:.c=.o)
	GEN_TARGETS += $(GENERATE_USB_DESCRIPTOR)
endif
ifneq ($(filter $(GENERATE), $(GENERATE_STORAGE)),)
	GEN_OBJ += $(GENDIR)/$(GENERATE_STORAGE_SRC:.c=.o)
	GEN_TARGETS += $(GENDIR)/$(GENERATE_STORAGE_SRC:.c=.o)
endif
ALL_OBJ := $(OBJ) $(GEN_OBJ)
DEP := $(addprefix $(OBJDIR)/,$(notdir $(SRC:.c=.d)))

macros:
	$(CC) $(GCFLAGS) -dM -E - < /dev/null

cleanBuild: clean

clean:
	$(RM) $(BINDIR)
	$(RM) $(OBJDIR)

size:
	$(SIZE) $(BINDIR)/$(PROJECT).elf

#
# Code generation
#
$(GENERATE_USB_DESCRIPTOR):
	@mkdir -p $(GENDIR)
	$(DESCRIPTORGEN) -os $(GENDIR)/$(GENERATE_USB_DESCRIPTOR_SRC) \
		-oh $(GENDIR)/$(GENERATE_USB_DESCRIPTOR_HDR) \
		$(GENSRC)

$(GENDIR)/$(GENERATE_STORAGE_SRC): $(STORAGESRC) $(STORAGEGEN_SCRIPT)
	@mkdir -p $(GENDIR)
	$(STORAGEGEN) -os $(GENDIR)/$(GENERATE_STORAGE_SRC) \
		-oh $(GENDIR)/$(GENERATE_STORAGE_HDR) \
		$(STORAGESRC)

#
# Compilation
#

$(BINDIR)/$(PROJECT).hex: $(BINDIR)/$(PROJECT).elf
	$(OBJCOPY) -O ihex $(BINDIR)/$(PROJECT).elf $(BINDIR)/$(PROJECT).hex

$(BINDIR)/$(PROJECT).bin: $(BINDIR)/$(PROJECT).elf
	$(OBJCOPY) -O binary $(BINDIR)/$(PROJECT).elf $(BINDIR)/$(PROJECT).bin

$(BINDIR)/$(PROJECT).elf: $(ALL_OBJ) $(LSCRIPT)
	@mkdir -p $(dir $@)
	$(CC) $(ALL_OBJ) $(LDFLAGS) -o $(BINDIR)/$(PROJECT).elf
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

# Ensure generated objects get run first
$(OBJ): | $(GEN_TARGETS)

