#
# Makefile
# Martine Lenders, 2016-03-05 22:56
#

DIRS=$(shell find . -mindepth 1 -maxdepth 1 -type d)

all:
	@$(foreach DIR, $(DIRS), $(MAKE) -C $(DIR) $@;)

clean:
	@$(foreach DIR, $(DIRS), $(MAKE) -C $(DIR) $@;)

distclean:
	@$(foreach DIR, $(DIRS), $(MAKE) -C $(DIR) $@;)
