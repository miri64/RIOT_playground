APPLICATION = lndw22_doc_client

BOARD ?= samr21-xpro

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../RIOT

USEMODULE += auto_init_gnrc_netif
USEMODULE += event_thread
USEMODULE += fmt
USEMODULE += ipv4_addr
USEMODULE += ipv6_addr
USEMODULE += gcoap_dns
USEMODULE += gcoap_dns_oscore
USEMODULE += gnrc_ipv6_default
USEMODULE += gnrc_ipv6_nib_dns
USEMODULE += netdev_default
USEMODULE += od
USEMODULE += shell
USEMODULE += shell_commands
USEMODULE += sock_dns

FEATURES_REQUIRED += periph_gpio_irq

CFLAGS += -DOSCORE_NANOCOAP_MEMMOVE_MODE

DEFAULT_CHANNEL ?= 25

include $(RIOTBASE)/Makefile.include

# Set a custom channel if needed
include $(RIOTMAKE)/default-radio-settings.inc.mk
