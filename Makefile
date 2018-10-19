
BUILD?=build
CONFIG?=release

TARGET=slip

first: all

all: compile

$(BUILD): meson.build
	meson $(BUILD) --buildtype $(CONFIG)

compile: $(BUILD)
	ninja -C $(BUILD)

