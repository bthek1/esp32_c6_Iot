# Host-side unit tests — plain gcc + the Unity vendored in lib/esp-idf.
# No ESP32, no idf.py: builds and runs on the dev machine for fast feedback on
# the pure-logic library code (currently lib/web's form parsers).
#
#   make            # build + run
#   make clean
#
# Add a new suite by dropping test_<name>.c here and extending TESTS below.

ROOT  := ../..
UNITY := $(ROOT)/lib/esp-idf/components/unity/unity/src

CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -g -O0
# Stubs dir first so our esp_http_server.h shadows the real one.
CFLAGS  += -I stubs -I $(ROOT)/lib/web -I $(UNITY)
# Unity config: enable the printf-based output it expects on a host.
CFLAGS  += -D UNITY_OUTPUT_COLOR

BUILD := build
TESTS := web_form

BINS := $(addprefix $(BUILD)/test_,$(TESTS))

# Sources every suite links against: the lib under test, the http-server stubs,
# and Unity itself.
COMMON_SRC := $(ROOT)/lib/web/web.c stubs/stubs.c $(UNITY)/unity.c

.PHONY: all run clean
all: run

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/test_%: test_%.c $(COMMON_SRC) | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@

run: $(BINS)
	@fail=0; \
	for t in $(BINS); do \
	  echo "── $$t ─────────────────────────────────"; \
	  ./$$t || fail=1; \
	done; \
	exit $$fail

clean:
	rm -rf $(BUILD)
