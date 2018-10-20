# Der Name der Applikation
APPLICATION = button

# Die Plattform (oder das Board) für das wir die Applikation implementiert haben
BOARD ?= samr21-xpro

# Der Pfad zum RIOT-Repository
RIOTBASE ?= $(CURDIR)/../../../RIOT

# Wir brauchen RIOTs GPIO-Unterstützung mit IRQ um das Button-Drück-Ereignis
# abzugreifen
FEATURES_REQUIRED += periph_gpio_irq

# Und außerdem folgende Module:
USEMODULE += gnrc_netdev_default    # Das standardmäßig bereitgestellte Netzwerkgerät des Boards
USEMODULE += auto_init_gnrc_netif   # Die Auto-Initializierung für Netzwerk-Schnittstellen
USEMODULE += cord_ep                # Einen Endpunkt für das Ressourcen-Verzeichnis
USEMODULE += gcoap                  # Eine CoAP-Bibliothek
USEMODULE += gnrc_icmpv6_echo       # Support um den Knoten anpingbar zu machen (immer hilfreich)
USEMODULE += gnrc_ipv6_default      # IPv6-Support (mit Standardkonfiguration)
USEMODULE += xtimer                 # Timer zum Debouncing des Buttons

# Außerdem folgendes externes Paket
USEPKG += jsmn                      # Eine JSON-Parsing-Bibliothek

# Statische Konfiguration
DEVELHELP       ?= 0                # Deaktivier Hilfe für Entwickler, um die App klein zu halten
DEFAULT_CHANNEL ?= 26               # Verwende Kanal 26 zur Radio-Kommunikation
BUTTON_PIN      ?= GPIO_PIN(PB, 2)  # Der Pin auf den wir den Button gesetzt haben

# Gebe die statische Konfiguration weiter an den Compiler
CFLAGS += "-DBUTTON_PIN=$(BUTTON_PIN)"
CFLAGS += -DIEEE802154_DEFAULT_CHANNEL=$(DEFAULT_CHANNEL)
CFLAGS += -DGNRC_IPV6_NIB_CONF_SLAAC=1  # Zur Interaktion mit dem RPi wird SLAAC benötigt

include $(RIOTBASE)/Makefile.include
