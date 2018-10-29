
CONFIG?=release
BUILD?=$(CONFIG)
MESON_OPTS?=

export SLIP=$(abspath $(BUILD)/slip)

first: compile

all: compile tests


debug: FORCE
	$(eval CONFIG=debug)
	$(eval BUILD=$(CONFIG))
	$(eval MESON_OPTS+=-Db_sanitize=address)


$(BUILD): configure

build: $(BUILD)
	ninja -C $(BUILD)

FORCE:

tests: FORCE
	$(MAKE) -C tests

clean:
	rm -rf $(BUILD)

configure:
	meson $(MESON_OPTS) --buildtype $(CONFIG) $(BUILD)
