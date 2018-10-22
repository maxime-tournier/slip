
BUILD?=build
CONFIG?=release
MESON_OPTS?=

export SLIP=$(abspath $(BUILD)/slip)

first: compile

all: compile tests

debug: FORCE
	$(eval CONFIG=debug)
	$(eval MESON_OPTS+=-Db_sanitize=address)


$(BUILD): meson.build
	meson $(BUILD) --buildtype $(CONFIG) $(MESON_OPTS)

compile: $(BUILD)
	ninja -C $(BUILD)

FORCE:

tests: FORCE
	$(MAKE) -C tests


clean:
	rm -rf $(BUILD)
