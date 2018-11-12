APPLICATION = lwip_v2_1_1_tcp_mintest

RIOTBASE = $(CURDIR)/../../RIOT/

BOARD ?= native

DEVELHELP = 1

# including lwip_ipv6_mld would currently break this test on at86rf2xx radios
USEMODULE += lwip lwip_ipv6_autoconfig lwip_netdev
USEMODULE += lwip_tcp lwip_sock_tcp
USEMODULE += netdev_default
USEMODULE += xtimer

include $(RIOTBASE)/Makefile.include
