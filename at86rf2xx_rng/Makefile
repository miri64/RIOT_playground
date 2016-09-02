APPLICATION = at86rf2xx_rng

BOARD ?= iotlab-m3
RIOTBASE ?= $(CURDIR)/../../RIOT
QUIET ?= 1

FEATURES_REQUIRED = periph_spi periph_gpio

BOARD_WHITELIST := fox iotlab-m3 iotlab-a8-m3 samr21-xpro

DISABLE_MODULE += auto_init

USEMODULE += xtimer

# define the driver to be used for selected boards
ifneq (,$(filter samr21-xpro,$(BOARD)))
  DRIVER := at86rf233
endif

# use the at86rf231 as fallback device
DRIVER ?= at86rf231

# include the selected driver
USEMODULE += $(DRIVER)

CFLAGS += -DDEVELHELP

include $(RIOTBASE)/Makefile.include
