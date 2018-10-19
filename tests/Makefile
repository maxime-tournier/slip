SLIP?=$(error undefined SLIP variable)

PASS=pass
FAIL=fail

first: all

all: $(PASS) $(FAIL)

$(PASS): $(wildcard $(PASS)/*.el)
$(FAIL): $(wildcard $(FAIL)/*.el)

FORCE:

$(PASS)/%.el: FORCE
	@echo $@; $(SLIP) $@ > $@.out

$(FAIL)/%.el: FORCE
	@echo $@; if ($(SLIP) $@ > $@.out); then exit 1; fi


