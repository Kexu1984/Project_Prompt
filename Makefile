# Makefile for Driver-Simulator Communication Interface

CC = gcc
CFLAGS = -Wall -Wextra -g -std=c99 -D_GNU_SOURCE
INCLUDES = -Iinclude
SRCDIR = src
OBJDIR = build
EXAMPLEDIR = examples

# Source files
SOURCES = $(SRCDIR)/interface_layer.c
OBJECTS = $(OBJDIR)/interface_layer.o

# Targets
LIBRARY = $(OBJDIR)/libinterface.a
EXAMPLE = $(OBJDIR)/test_interface

.PHONY: all clean test run-test

all: $(LIBRARY) $(EXAMPLE)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(LIBRARY): $(OBJECTS)
	ar rcs $@ $^

$(EXAMPLE): $(EXAMPLEDIR)/test_interface.c $(LIBRARY) | $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(OBJDIR) -linterface -o $@

clean:
	rm -rf $(OBJDIR)
	rm -f /tmp/interface_driver_*
	rm -f /tmp/interrupt_info_*
	rm -f /tmp/driver_simulator_socket

test: $(EXAMPLE)
	@echo "Running interface test..."
	@echo "Note: This test requires the Python simulator to be running"
	@echo "Start the simulator with: python3 python/device_model.py 0"
	$(EXAMPLE)

run-test: $(EXAMPLE)
	@echo "Starting Python simulator in background..."
	@python3 python/device_model.py 0 &
	@SIMULATOR_PID=$$!; \
	sleep 2; \
	echo "Running test..."; \
	$(EXAMPLE); \
	TEST_RESULT=$$?; \
	echo "Stopping simulator..."; \
	kill $$SIMULATOR_PID 2>/dev/null || true; \
	exit $$TEST_RESULT

help:
	@echo "Available targets:"
	@echo "  all       - Build library and examples"
	@echo "  clean     - Remove build artifacts"
	@echo "  test      - Run test (requires manual simulator start)"
	@echo "  run-test  - Run test with automatic simulator management"
	@echo "  help      - Show this help message"