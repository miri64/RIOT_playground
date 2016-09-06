USEMODULE += ipv6_addr
USEMODULE += lwip_conn_udp
USEMODULE += lwip_ipv6
USEMODULE += lwip_netdev2
USEMODULE += lwip_netif
USEMODULE += lwip_sixlowpan
USEMODULE += lwip_udp

include ../Makefile.common

ifeq (1,$(RPL_STACK))
  $(error "lwIP has no RPL support")
endif

CFLAGS += -DLWIP_IPV6_FRAG=0
CFLAGS += -DLWIP_IPV6_REASS=0
