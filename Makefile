TARGET := iphone:clang:latest:14.0
INSTALL_TARGET_PROCESSES = SpringBoard

include $(THEOS)/makefiles/common.mk

TWEAK_NAME = XiangqiAssist

XiangqiAssist_FILES = Tweak.xm
XiangqiAssist_CFLAGS = -fobjc-arc
XiangqiAssist_FRAMEWORKS = Foundation UIKit

include $(THEOS_MAKE_PATH)/tweak.mk
