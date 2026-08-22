#-------------------------------------------------------------------------------
# WiiXLaunch Wii U (WUPS/Aroma) plugin Makefile.
#
# Not run in-place: build_wiiu.bat stages src/, include/, the generated headers
# (as generated/include/), and this file into a temp dir and runs make there -
# devkitPro's make rules cannot handle spaces in paths, and the repo may live
# in one. Based on WUPS's example_plugin_cpp Makefile.
#-------------------------------------------------------------------------------
.SUFFIXES:

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR ?= $(CURDIR)

include $(DEVKITPRO)/wups/share/wups_rules

WUT_ROOT  := $(DEVKITPRO)/wut
WUMS_ROOT := $(DEVKITPRO)/wums

# TARGET is passed in on the make command line by build_wiiu.bat/.sh, which read
# it from wiiu.plugin_name in wiixlaunch.json. It is deliberately NOT defaulted
# here: the plugin name has to agree between this Makefile's output
# ($(TARGET).wps) and the copy step in those scripts, and a hardcoded name in
# both places is a name that eventually drifts - which is exactly what happened
# when the project was renamed.
ifeq ($(strip $(TARGET)),)
$(error TARGET not set - build via build_wiiu.bat/.sh, which read wiiu.plugin_name from wiixlaunch.json. To run make directly: make TARGET=YourPluginName)
endif
BUILD		:=	build
SOURCES		:=	src
DATA		:=
# $(wildcard modules/*/include): optional WiiXLaunch modules (e.g.
# vendor/wiixlaunch-botw) - not part of base WiiXLaunch, staged here by
# build_wiiu.bat under modules/<name>/include if this mod added one as a
# submodule. Picked up automatically; nothing to edit here per-project.
INCLUDES	:=	include generated/include $(wildcard modules/*/include)

#-------------------------------------------------------------------------------
# options for code generation
#-------------------------------------------------------------------------------
CFLAGS	:=	-g -Wall -O2 -ffunction-sections \
			$(MACHDEP)

CFLAGS	+=	$(INCLUDE) -D__WIIU__ -D__WUT__ -D__WUPS__

CXXFLAGS	:= $(CFLAGS) -std=c++20

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-g $(ARCH) $(RPXSPECS) -Wl,-Map,$(notdir $*.map) $(WUPSSPECS)

# wums client libs before wut: they depend on coreinit symbols from libwut
LIBS	:= -lwups -lfunctionpatcher -lnotifications -lwut

#-------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level
# containing include and lib
#-------------------------------------------------------------------------------
LIBDIRS	:= $(PORTLIBS) $(WUPS_ROOT) $(WUMS_ROOT) $(WUT_ROOT)

#-------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add
# additional rules for different file extensions
#-------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#-------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#-------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#-------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
	export LD	:=	$(CC)
else
	export LD	:=	$(CXX)
endif

export OFILES_BIN	:=	$(addsuffix .o,$(BINFILES))
export OFILES_SRC	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES 	:=	$(OFILES_BIN) $(OFILES_SRC)
export HFILES_BIN	:=	$(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean all

#-------------------------------------------------------------------------------
all: $(BUILD)

$(BUILD):
	@$(shell [ ! -d $(BUILD) ] && mkdir -p $(BUILD))
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#-------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).wps $(TARGET).elf

#-------------------------------------------------------------------------------
else
.PHONY:	all

DEPENDS	:=	$(OFILES:.o=.d)

#-------------------------------------------------------------------------------
# main targets
#-------------------------------------------------------------------------------
all	:	$(OUTPUT).wps

$(OUTPUT).wps	:	$(OUTPUT).elf
$(OUTPUT).elf	:	$(OFILES)

$(OFILES_SRC)	: $(HFILES_BIN)

-include $(DEPENDS)

#-------------------------------------------------------------------------------
endif
#-------------------------------------------------------------------------------
