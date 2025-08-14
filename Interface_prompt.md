# Driver-Simulator Communication Interface Design

## Project Overview

This communication interface implements transparent communication between drivers and hardware simulators. Core functionality includes:
- **Driver-initiated read/write operations to simulator**: Intercepting register access through memory protection mechanisms
- **Simulator-initiated interrupts to driver**: Sending interrupts from Python model to C driver through signal mechanisms

## 1. Overall Architecture Design

### 1.1 Layered Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    Application Layer                    │
│              System initialization + Test execution     │
│                main.c - System init + Test execution    │
└─────────────────────────────┬───────────────────────────┘
                              │ Application calls CMSIS driver APIs
┌─────────────────────────────┴───────────────────────────┐
│                   Driver Layer                          │
│      CMSIS-compliant drivers + register definitions     │
│         (Transparent register access + IRQ handling)    │
└─────────────────────────────┬───────────────────────────┘
                              │ Register access triggers SIGSEGV
┌─────────────────────────────┴───────────────────────────┐
│              Interface Layer                            │
│   Memory protection + Signal handling + Socket comm.    │
│              + Interrupt management                     │
└─────────────────────────────┬───────────────────────────┘
                              │ Unix Socket + Protocol Messages
┌─────────────────────────────┴───────────────────────────┐
│               Device Model Layer                        │
│          Python-based + Hardware behavior simulation    │
└─────────────────────────────────────────────────────────┘
```

### 1.2 Main Functional Flows

#### Read/Write Operation Flow (Driver → Simulator)
```
1. Driver code: *register_ptr = value
2. Memory protection: Triggers SIGSEGV signal 
3. Signal handler: Parse x86-64 instruction to determine read/write operation
4. Protocol wrapping: Create protocol_message_t
5. Socket communication: Send to Python simulator
6. Simulator processing: Simulate hardware behavior
7. Response return: Return result through Socket
8. Register update: Update CPU register (read operation)
9. Instruction continuation: Advance RIP register, continue execution
```

#### Interrupt Trigger Flow (Simulator → Driver)
```
1. Python model: Hardware event triggers interrupt
2. Get driver PID: Read from /tmp/interface_driver_{pid}
3. Interrupt info: Write to temporary file /tmp/interface_interrupt_{pid}
4. Signal sending: kill(pid, SIGUSR1)
5. Signal handling: C driver receives SIGUSR1
6. Interrupt info reading: Parse device_id and interrupt_id from temporary file
7. Callback execution: Call registered interrupt_handler_t
8. Cleanup: Delete temporary file
```

## 2. Core Technical Implementation

### 2.1 Memory Protection Mechanism

#### Device Registration and Address Mapping
```c
int register_device(uint32_t device_id, uint32_t base_address, uint32_t size) {
    // Create protected memory region using mmap
    void *mapped_memory = mmap((void*)base_address, size, PROT_NONE, 
                              MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    
    ...
}
```

#### SIGSEGV Signal Handling
```c
static void segv_handler(int sig, siginfo_t *si, void *context) {
    uint64_t fault_addr = (uint64_t)si->si_addr;
    ucontext_t *uctx = (ucontext_t *)context;
    
    // Find device and parse instruction
    device_info_t *device = find_device_by_addr(fault_addr);
    if (!device) return;
    
    instruction_info_t inst_info = parse_instruction(uctx);
    
    // Create and send message
    protocol_message_t msg = {
        .device_id = device->device_id,
        .command = inst_info.is_write ? CMD_WRITE : CMD_READ,
        .address = (uint32_t)fault_addr,
        .length = inst_info.size
    };
    
    if (inst_info.is_write) {
        extract_write_data(&msg, uctx);
    }
    
    protocol_message_t response;
    send_message_to_model(&msg, &response);
    
    if (!inst_info.is_write) {
        update_cpu_register(uctx, &response);
    }
    
    // Skip instruction and continue
    uctx->uc_mcontext.gregs[REG_RIP] += inst_info.length;
}
```

### 2.2 Simplified Instruction Parsing

```c
typedef struct {
    bool is_write;
    int size;
    int length;
} instruction_info_t;

// Basic instruction parsing for common MOV operations
instruction_info_t parse_instruction(ucontext_t *uctx) {
    uint8_t *inst = (uint8_t *)uctx->uc_mcontext.gregs[REG_RIP];
    instruction_info_t info = {0};
    
    // Skip prefixes and REX if present
    while (is_prefix(*inst)) inst++;
    
    switch (*inst) {
        case 0x89: // MOV [mem], reg32
            info.is_write = true;
            info.size = 4;
            break;
        case 0x8B: // MOV reg32, [mem]  
            info.is_write = false;
            info.size = 4;
            break;
        case 0x88: // MOV [mem], reg8
            info.is_write = true;
            info.size = 1;
            break;
        case 0x8A: // MOV reg8, [mem]
            info.is_write = false;
            info.size = 1;
            break;
        default:
            info.size = 4; // Default to 32-bit
    }
    
    info.length = calculate_instruction_length(inst);
    return info;
}
```

### 2.3 Simplified Communication Protocol

```c
// Basic command types
typedef enum {
    CMD_READ = 1,
    CMD_WRITE = 2
} command_t;

// Simplified message structure  
typedef struct {
    uint32_t device_id;
    command_t command;
    uint32_t address;
    uint32_t data;
    int result;
} message_t;

// Simplified socket communication
int send_message_to_model(const message_t *msg, message_t *resp)；
```

### 2.4 Simplified Interrupt Handling

```c
// Interrupt handler type
typedef void (*interrupt_handler_t)(uint32_t interrupt_id);
static interrupt_handler_t handlers[16]; // Support up to 16 devices

// Register interrupt handler
int register_interrupt_handler(uint32_t device_id, interrupt_handler_t handler)；

// Signal handler for interrupts
static void interrupt_handler(int sig) {
    // Read interrupt info from shared file
    ...
}
```

#### Python-side Interrupt Trigger Implementation
```python
class ModelInterface:
    def handle_message(self, msg):
        """Handle read/write requests from C driver"""
        offset = msg['address'] - 0x40000000
        
        if msg['command'] == 1:  # READ
            return {'result': 0, 'data': self.registers.get(offset, 0)}
        elif msg['command'] == 2:  # WRITE
            self.registers[offset] = msg['data']
            return {'result': 0}
            
    def trigger_interrupt(self, interrupt_id):
        """Send interrupt to driver"""
        with open("/tmp/interrupt_info", "w") as f:
            f.write(f"{self.device_id},{interrupt_id}")
        os.kill(self.get_driver_pid(), signal.SIGUSR1)
```
