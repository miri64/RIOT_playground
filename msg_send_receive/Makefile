# name of your application
APPLICATION = msg_send_receive

# If no BOARD is found in the environment, use this default:
BOARD ?= native

BOARD_WHITELIST := native

QUIET := 1

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../../RIOT/

# Modules to include:

USEMODULE += uart0
USEMODULE += vtimer

include $(RIOTBASE)/Makefile.include
