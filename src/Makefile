VERSION      = 0.1
MCU          = atmega32u4
ARCH         = AVR8
F_CPU        = 8000000
F_USB        = $(F_CPU)
OPTIMIZATION = s
TARGET       = ecu
SRC          = main.c interface.c immo.c map.c params.c Descriptors.c $(LUFA_SRC_USB) $(LUFA_SRC_USBCLASS)
LUFA_PATH    = ../../LUFA
CC_FLAGS     = -DUSE_LUFA_CONFIG_HEADER -IConfig/ -DFW_VERSION=\"$(VERSION)\"
LD_FLAGS     =
AVRDUDE_PROGRAMMER = flip1

all:
	@true

install: avrdude
	@true

include $(LUFA_PATH)/Build/lufa_core.mk
include $(LUFA_PATH)/Build/lufa_sources.mk
include $(LUFA_PATH)/Build/lufa_build.mk
include $(LUFA_PATH)/Build/lufa_cppcheck.mk
include $(LUFA_PATH)/Build/lufa_avrdude.mk
