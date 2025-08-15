#!/bin/bash
# Demo script for Driver-Simulator Communication Interface

echo "=== Driver-Simulator Interface Demo ==="
echo

# Build the system
echo "Building the interface system..."
make clean
make all

if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

echo "Build successful!"
echo

# Start Python simulator in background
echo "Starting Python device simulator..."
python3 python/device_model.py 0 0x40000000 &
SIMULATOR_PID=$!

# Give simulator time to start
sleep 2

# Run the test
echo "Running driver test..."
./build/test_interface

TEST_RESULT=$?

# Clean up
echo "Cleaning up..."
kill $SIMULATOR_PID 2>/dev/null || true
wait $SIMULATOR_PID 2>/dev/null

# Clean temporary files
rm -f /tmp/interface_driver_*
rm -f /tmp/interrupt_info_*
rm -f /tmp/driver_simulator_socket

echo
if [ $TEST_RESULT -eq 0 ]; then
    echo "=== Demo completed successfully! ==="
else
    echo "=== Demo failed with errors ==="
fi

exit $TEST_RESULT