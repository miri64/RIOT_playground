# name of your application
APPLICATION = default

# If no BOARD is found in the environment, use this default:
BOARD ?= native

BOARD_WHITELIST := native

QUIET := 1

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../../RIOT/

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
CFLAGS += -DDEVELHELP

# Modules to include:

USEMODULE += basic_mac
USEMODULE += nativenet
USEMODULE += net_help
USEMODULE += random
USEMODULE += shell
USEMODULE += sixlowpan
USEMODULE += sixlowpan_hc
USEMODULE += uart0

include $(RIOTBASE)/Makefile.include
