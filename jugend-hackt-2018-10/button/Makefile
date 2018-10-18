APPLICATION = button

BOARD ?= samr21-xpro

RIOTBASE ?= $(CURDIR)/../../../RIOT

FEATURES_REQUIRED += periph_gpio_irq

USEMODULE += gnrc_netdev_default
USEMODULE += auto_init_gnrc_netif
USEMODULE += cord_ep
USEMODULE += gcoap
USEMODULE += gnrc_icmpv6_echo
USEMODULE += gnrc_ipv6_default
USEMODULE += xtimer

USEPKG += jsmn

DEVELHELP       ?= 0
DEFAULT_CHANNEL ?= 26
BUTTON_PIN      ?= GPIO_PIN(PB, 2)

CFLAGS += "-DBUTTON_PIN=$(BUTTON_PIN)"
CFLAGS += -DIEEE802154_DEFAULT_CHANNEL=$(DEFAULT_CHANNEL)
CFLAGS += -DGNRC_IPV6_NIB_CONF_SLAAC=1

include $(RIOTBASE)/Makefile.include
