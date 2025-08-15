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
ADVANCED_EXAMPLE = $(OBJDIR)/advanced_test

.PHONY: all clean test run-test

all: $(LIBRARY) $(EXAMPLE) $(ADVANCED_EXAMPLE)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(LIBRARY): $(OBJECTS)
	ar rcs $@ $^

$(EXAMPLE): $(EXAMPLEDIR)/test_interface.c $(LIBRARY) | $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(OBJDIR) -linterface -o $@

$(ADVANCED_EXAMPLE): $(EXAMPLEDIR)/advanced_test.c $(LIBRARY) | $(OBJDIR)
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
	@echo "  all          - Build library and examples"
	@echo "  clean        - Remove build artifacts"
	@echo "  test         - Run basic test (requires manual simulator start)"
	@echo "  run-test     - Run basic test with automatic simulator management"
	@echo "  advanced     - Run advanced multi-device test"
	@echo "  help         - Show this help message"

advanced: $(ADVANCED_EXAMPLE)
	@echo "Starting advanced multi-device test..."
	@echo "This requires UART and Timer simulators to be running:"
	@echo "  Terminal 1: python3 python/extended_devices.py uart 0"
	@echo "  Terminal 2: python3 python/extended_devices.py timer 1"
	@echo "Then run: $(ADVANCED_EXAMPLE)"