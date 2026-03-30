ARCHS = arm64 arm64e

# Updated target to iOS 15 to ensure modern toolchains and headers are used
TARGET = iphone:15.2:14.0

# Removed hardcoded Xcode 11.7 paths. iOS 15+ arm64e compilation requires 
# modern Xcode toolchains and standard PAC compliance provided by updated Theos.

DEBUG=0
STRIP=1
FINALPACKAGE=1
THEOS_PLATFORM_DEB_COMPRESSION_TYPE = gzip

include $(THEOS)/makefiles/common.mk

TWEAK_NAME = libH5GGApp
libH5GGApp_FILES = globalview.mm ContextHostManager.mm ../RootBridge/RootBridge.m
libH5GGApp_CFLAGS = -fobjc-arc -fvisibility=hidden 
libH5GGApp_CCFLAGS = -fobjc-arc -fvisibility=hidden -std=c++17

include $(THEOS_MAKE_PATH)/tweak.mk

clean::
	rm -rf ./packages/*
	rm -rf ./layout

before-package::
	find . -name '*.DS_Store' -type f -delete
	cp -R ./H5GGApp_layout/ ./.theos/_/
	ldid -S../app.entitlements ./.theos/_/Applications/h5ggapp.app/h5ggapp
	cd .. && make 
	cp ./.theos/obj/H5GG.dylib ./.theos/_/Applications/h5ggapp.app/