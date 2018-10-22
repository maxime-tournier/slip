
BUILD?=build
CONFIG?=release
MESON_OPTS?=

export SLIP=$(abspath $(BUILD)/slip)

first: compile

all: compile tests

debug:
	$(eval CONFIG=debug)
	$(eval MESON_OPTS+=-Db_sanitize=address)


$(BUILD):
	meson $(BUILD) --buildtype $(CONFIG) $(MESON_OPTS)

compile: $(BUILD)
	ninja -C $(BUILD)

FORCE:

tests: FORCE
	$(MAKE) -C tests


clean:
	rm -rf $(BUILD)
