
BUILD?=build
CONFIG?=release

export SLIP=$(abspath $(BUILD)/slip)

first: compile

all: compile tests

$(BUILD): meson.build
	meson $(BUILD) --buildtype $(CONFIG)

compile: $(BUILD)
	ninja -C $(BUILD)

FORCE:

tests: FORCE
	$(MAKE) -C tests

