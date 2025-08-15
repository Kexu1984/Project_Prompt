# Driver-Simulator Communication Interface

This project implements a transparent communication interface between CMSIS drivers and Python-based hardware simulators, as specified in `Interface_prompt.md`.

## Architecture

The system implements a layered architecture:

```
┌─────────────────────────────────────────────────────────┐
│                    Application Layer                    │
│              System initialization + Test execution     │
│                examples/test_interface.c                │
└─────────────────────────────┬───────────────────────────┘
                              │ Application calls driver APIs
┌─────────────────────────────┴───────────────────────────┐
│                   Driver Layer                          │
│      CMSIS-compliant drivers + register definitions     │
│         (Transparent register access + IRQ handling)    │
└─────────────────────────────┬───────────────────────────┘
                              │ Register access triggers SIGSEGV
┌─────────────────────────────┴───────────────────────────┐
│              Interface Layer                            │
│   Memory protection + Signal handling + Socket comm.    │
│              src/interface_layer.c                      │
└─────────────────────────────┬───────────────────────────┘
                              │ Unix Socket + Protocol Messages
┌─────────────────────────────┴───────────────────────────┐
│               Device Model Layer                        │
│          Python-based + Hardware behavior simulation    │
│              python/device_model.py                     │
└─────────────────────────────────────────────────────────┘
```

## Components

### Interface Layer (C)
- **Memory Protection**: Uses `mmap()` with `PROT_NONE` to create protected memory regions
- **Signal Handling**: Intercepts `SIGSEGV` signals to handle register access
- **Instruction Parsing**: Basic x86-64 MOV instruction parsing to determine read/write operations
- **Socket Communication**: Unix domain socket communication with Python simulator
- **Interrupt Management**: Handles interrupts from simulator via `SIGUSR1` signals

### Device Model Layer (Python)
- **Base Interface**: `ModelInterface` class for device simulation
- **Register Management**: Dictionary-based register storage and access
- **Socket Server**: Unix domain socket server to receive messages from C layer
- **Interrupt Generation**: Can trigger interrupts back to C driver
- **Example Implementation**: `SimpleUARTModel` for testing

## Key Features

1. **Transparent Operation**: Drivers can access device registers normally using standard pointer dereferences
2. **Memory Protection**: Invalid access is caught and redirected to simulator
3. **Bidirectional Communication**: 
   - Driver → Simulator: Register read/write operations
   - Simulator → Driver: Hardware interrupts
4. **Multiple Device Support**: Can handle up to 16 different devices
5. **Extensible**: Easy to add new device models by extending `ModelInterface`

## Building and Running

### Prerequisites
- GCC compiler with GNU extensions support
- Python 3.6+
- Linux environment (uses Linux-specific system calls)

### Quick Start
```bash
# Run the complete demo
./demo.sh
```

### Manual Build and Test
```bash
# Build the system
make all

# Start simulator in one terminal
python3 python/device_model.py 0 0x40000000

# Run test in another terminal
./build/test_interface
```

### Makefile Targets
- `make all` - Build library and examples
- `make clean` - Remove build artifacts  
- `make test` - Run test (requires manual simulator start)
- `make run-test` - Run test with automatic simulator management

## Usage Example

```c
#include "interface_layer.h"

int main() {
    // Initialize interface
    interface_init();
    
    // Register a device (e.g., UART at 0x40000000)
    register_device(0, 0x40000000, 0x1000);
    
    // Register interrupt handler
    register_interrupt_handler(0, my_interrupt_handler);
    
    // Now you can access device registers transparently
    volatile uint32_t *uart_tx = (uint32_t*)0x40000000;
    *uart_tx = 0x55;  // This will be intercepted and sent to simulator
    
    uint32_t status = *(uint32_t*)0x40000004;  // Read operation
    
    // Cleanup
    interface_cleanup();
    return 0;
}
```

## Communication Protocol

The interface uses a simple message structure for C-Python communication:

```c
typedef struct {
    uint32_t device_id;    // Device identifier
    uint32_t command;      // CMD_READ or CMD_WRITE
    uint32_t address;      // Register address
    uint32_t data;         // Data value (for writes)
    uint32_t length;       // Access size (1, 2, or 4 bytes)
    int result;            // Operation result
} message_t;
```

## Extending the System

### Adding New Device Models

1. Create a new Python class extending `ModelInterface`
2. Override `on_register_write()` for write side effects
3. Implement device-specific behavior
4. Use `trigger_interrupt()` to send interrupts to driver

Example:
```python
class MyDeviceModel(ModelInterface):
    def __init__(self, device_id, base_address):
        super().__init__(device_id, base_address)
        # Initialize device-specific registers
        
    def on_register_write(self, offset, value):
        # Handle register write side effects
        if offset == 0x00 and value & 0x01:
            # Device enable bit set
            self.trigger_interrupt(1)
```

### Adding New Register Access Patterns

The instruction parser in `interface_layer.c` can be extended to support additional x86-64 instructions beyond the basic MOV operations currently implemented.

## Limitations

1. **Instruction Parsing**: Only supports basic MOV instructions (can be extended)
2. **x86-64 Only**: Uses x86-64 specific CPU context manipulation
3. **Single Threaded**: Simulator processes one request at a time per connection
4. **Linux Only**: Uses Linux-specific system calls and signal handling

## Testing

The system includes a comprehensive test that validates:
- Device registration
- Memory protection setup
- Register read/write operations
- Instruction parsing and handling
- Socket communication
- Interrupt delivery
- Resource cleanup

Run `./demo.sh` to see the complete system in action.