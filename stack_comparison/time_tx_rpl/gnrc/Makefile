USEMODULE += gnrc_conn_udp
USEMODULE += gnrc_ipv6_router_default
USEMODULE += gnrc_netdev2
USEMODULE += gnrc_rpl
USEMODULE += gnrc_sixlowpan_iphc_nhc
USEMODULE += gnrc_sixlowpan_router_default
USEMODULE += gnrc_udp

CFLAGS += -DGNRC_PKTBUF_SIZE=1676
CFLAGS += -DGNRC_IPV6_FIB_TABLE_SIZE=10 # emb6 has the same config
CFLAGS += -DSTACK_MULTIHOP
CFLAGS += -DSTACK_RPL
CFLAGS += -DSTACK_RPL_ROOT

include ../Makefile.common
