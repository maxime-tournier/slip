BUILD=../build
SLAP=$(BUILD)/slap

first: all

all: pass fail

pass: $(wildcard pass/*.el)
pass: $(wildcard fail/*.el)

FORCE:

pass/%.el: FORCE
	@echo $@; $(SLAP) $@

fail/%.el: FORCE
	@echo $@; if $(SLAP) $@; then exit 1; fi


