# Reproducible GBA ROM for Ambient Granulator.

.SUFFIXES:

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM. Run scripts/setup.sh and scripts/build.sh, or install devkitARM natively")
endif

include $(DEVKITARM)/gba_rules

TARGET      := ambient-granulator-for-gba
BUILD       := build
SOURCES     := source
INCLUDES    := source
DATA        := assets
MUSIC       :=

GAME_TITLE  := AMBGRANULAR
GAME_CODE   := AGRN
MAKER_CODE  := 00

ARCH        := -mthumb -mthumb-interwork
CFLAGS      := -g -Wall -Wextra -Werror -O2 \
               -mcpu=arm7tdmi -mtune=arm7tdmi \
               -ffunction-sections -fdata-sections $(ARCH)
CFLAGS      += $(INCLUDE)
ifeq ($(MAX_LOAD),1)
CFLAGS      += -DAMBIENT_MAX_LOAD_PROFILE
endif
ifeq ($(MAX_LOAD_NO_FILTERS),1)
CFLAGS      += -DAMBIENT_MAX_LOAD_NO_FILTERS
endif
ifeq ($(MAX_LOAD_LPF_ONLY),1)
CFLAGS      += -DAMBIENT_MAX_LOAD_LPF_ONLY
endif
ifeq ($(PROFILE_GRAINS_ONLY),1)
CFLAGS      += -DAMBIENT_PROFILE_GRAINS_ONLY
endif
ifeq ($(PROFILE_EFFECTS_ONLY),1)
CFLAGS      += -DAMBIENT_PROFILE_EFFECTS_ONLY
endif
ifeq ($(FIFO_TEST),1)
CFLAGS      += -DAMBIENT_FIFO_CONTINUITY_PROFILE
endif
CXXFLAGS    := $(CFLAGS) -fno-rtti -fno-exceptions
ASFLAGS     := -g $(ARCH)
LDFLAGS     := -g $(ARCH) -Wl,-Map,$(OUTPUT).map,--gc-sections
LIBS        := -lgba
LIBDIRS     := $(LIBGBA)

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT := $(CURDIR)/$(TARGET)
export VPATH  := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                 $(foreach dir,$(DATA),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES       := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
SFILES       := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES     := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))
OFILES_BIN   := $(addsuffix .o,$(BINFILES))
OFILES_SOURCES := $(CFILES:.c=.o) $(SFILES:.s=.o)
OFILES       := $(OFILES_BIN) $(OFILES_SOURCES)
HFILES       := $(addsuffix .h,$(subst .,_,$(BINFILES)))

export OFILES
export INCLUDE := $(foreach dir,$(INCLUDES),-iquote $(CURDIR)/$(dir)) \
                  $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                  -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
export LD := $(CC)

.PHONY: all $(BUILD) clean

all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $@ -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -rf $(BUILD) $(TARGET).elf $(TARGET).gba $(TARGET).map

else

$(OUTPUT).gba: $(OUTPUT).elf

$(OUTPUT).elf: $(OFILES)

# Compiler-flag changes in this file must invalidate source objects. Without
# this prerequisite an optimisation/profile build can silently reuse stale DSP
# code from an earlier invocation.
$(filter-out %.bin.o,$(OFILES)): $(firstword $(MAKEFILE_LIST))

# The profiled mixer benefits substantially from the ARM7TDMI's full ARM
# instruction set; interworking keeps the rest of the ROM compact Thumb code.
dsp.o: CFLAGS += -marm -O3 -fno-unroll-loops -fno-peel-loops

%.bin.o %_bin.h: %.bin
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPSDIR)/*.d

endif
