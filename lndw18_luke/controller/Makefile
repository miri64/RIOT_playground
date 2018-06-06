APPLICATION = luke_controller

BOARD ?= samr21-xpro

RIOTBASE ?= $(CURDIR)/../../../RIOT

USEMODULE += gnrc_netdev_default
USEMODULE += auto_init_gnrc_netif
USEMODULE += xtimer
USEMODULE += gcoap
USEMODULE += gnrc_ipv6_default

FEATURES_REQUIRED += periph_gpio

DEVELHELP ?= 0
QUIET ?= 1

CFLAGS += -DGCOAP_REQ_WAITING_MAX=64

include $(RIOTBASE)/Makefile.include
