# Local Device OS build; see README "Build". DEVICE_OS points at a v2.3.1 checkout.
DEVICE_OS ?= ../device-os

all:
	$(MAKE) -C $(DEVICE_OS)/modules/photon/user-part PLATFORM=photon APPDIR=$(CURDIR) COMPILE_LTO=n all

cloud:
	particle compile photon --saveTo aerisFirmware.bin

.PHONY: all cloud
