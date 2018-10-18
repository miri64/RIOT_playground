APPLICATION = dino

BOARD ?= samr21-xpro

RIOTBASE ?= $(CURDIR)/../../../RIOT

FEATURE_REQUIRED += periph_gpio_irq

USEMODULE += gnrc_netdev_default
USEMODULE += auto_init_gnrc_netif
USEMODULE += gcoap
USEMODULE += cord_ep
USEMODULE += gnrc_icmpv6_echo
USEMODULE += gnrc_ipv6_default
USEMODULE += shell
USEMODULE += shell_commands

DEVELHELP       ?= 0

DEFAULT_CHANNEL ?= 26
DINO_MOVE_PIN   ?= GPIO_PIN(PB, 2)

CFLAGS += "-DDINO_MOVE_PIN=$(DINO_MOVE_PIN)"
CFLAGS += -DIEEE802154_DEFAULT_CHANNEL=$(DEFAULT_CHANNEL)
CFLAGS += -DGNRC_IPV6_NIB_CONF_SLAAC=1

include $(RIOTBASE)/Makefile.include
