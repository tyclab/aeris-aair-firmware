# Local Device OS build; see README "Build". DEVICE_OS points at a v2.3.1 checkout.
DEVICE_OS ?= ../device-os

# The build stamp (__DATE__/__TIME__) is compiled into one object under
# $(DEVICE_OS)/build, which only recompiles when its source changes; touch it so
# every build carries its own stamp and a flashed unit can be told apart.
all:
	touch src/net/web_config_server_api.cpp
	$(MAKE) -C $(DEVICE_OS)/modules/photon/user-part PLATFORM=photon APPDIR=$(CURDIR) COMPILE_LTO=n all

cloud:
	particle compile photon --saveTo aerisFirmware.bin

.PHONY: all cloud
