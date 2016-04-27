# name of your application
APPLICATION = empty

DISABLE_MODULE = auto_init

# If no BOARD is found in the environment, use this default:
BOARD ?= iotlab-m3

QUIET ?= 1

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../../RIOT/

IDLE ?= 0

ifneq (0,$(IDLE))
  CFLAGS += -DIDLE_TEST
endif
ifneq (0,$(SLEEPY_RADIO))
  USEMODULE += at86rf231
endif

include $(RIOTBASE)/Makefile.include
