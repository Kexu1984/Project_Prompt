#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <fcntl.h>
#include <ucontext.h>
#include "interface_layer.h"

// Global state
static device_info_t devices[16];
static int device_count = 0;
static interrupt_handler_t interrupt_handlers[16];
static pid_t driver_pid;

// Socket path for communication
#define SOCKET_PATH "/tmp/driver_simulator_socket"

// Internal helper functions
static bool is_prefix(uint8_t byte) {
    return (byte == 0x66 || byte == 0x67 || byte == 0xF2 || byte == 0xF3 ||
            (byte >= 0x40 && byte <= 0x4F)); // REX prefixes
}

static int calculate_instruction_length(uint8_t *inst) {
    // Simplified instruction length calculation for common MOV operations
    // This is a basic implementation - real x86-64 decoding is more complex
    uint8_t *orig_inst = inst;
    
    if (!inst) return 3; // Default safe length
    
    // Skip prefixes
    while (is_prefix(*inst)) inst++;
    
    uint8_t opcode = *inst;
    
    // Basic opcode
    inst++;
    
    // For MOV instructions, we typically have ModR/M byte
    if (inst - orig_inst < 10) { // Safety check
        // ModR/M byte analysis for addressing mode
        uint8_t modrm = *inst;
        uint8_t mod = (modrm >> 6) & 0x3;
        uint8_t rm = modrm & 0x7;
        
        inst++; // ModR/M byte
        
        // Check for SIB byte
        if (mod != 0x3 && rm == 0x4) {
            inst++; // SIB byte
        }
        
        // Displacement
        if (mod == 0x1) {
            inst++; // 8-bit displacement
        } else if (mod == 0x2 || (mod == 0x0 && rm == 0x5)) {
            inst += 4; // 32-bit displacement
        }
        
        // Immediate operand for MOV immediate instructions
        if (opcode == 0xC7) {
            inst += 4; // 32-bit immediate
        } else if (opcode == 0xC6) {
            inst += 1; // 8-bit immediate
        }
    }
    
    int len = (int)(inst - orig_inst);
    return (len > 0 && len < 16) ? len : 6; // Reasonable bounds check, default to 6 for immediate ops
}

static void extract_write_data(message_t *msg, ucontext_t *uctx) {
    // Extract data from CPU registers or immediate values based on instruction
    uint8_t *inst = (uint8_t *)uctx->uc_mcontext.gregs[REG_RIP];
    
    // Skip prefixes
    while (is_prefix(*inst)) inst++;
    
    uint8_t opcode = *inst;
    
    if (opcode == 0xC7 || opcode == 0xC6) {
        // MOV [mem], immediate - extract immediate value from instruction
        inst++; // Skip opcode
        inst++; // Skip ModR/M byte
        
        // Extract immediate value based on size
        switch (msg->length) {
            case 1:
                msg->data = *inst;
                break;
            case 2:
                msg->data = *(uint16_t*)inst;
                break;
            case 4:
            default:
                msg->data = *(uint32_t*)inst;
                break;
        }
    } else {
        // MOV [mem], reg - extract from CPU registers
        switch (msg->length) {
            case 1:
                msg->data = uctx->uc_mcontext.gregs[REG_RAX] & 0xFF;
                break;
            case 2:
                msg->data = uctx->uc_mcontext.gregs[REG_RAX] & 0xFFFF;
                break;
            case 4:
            default:
                msg->data = uctx->uc_mcontext.gregs[REG_RAX] & 0xFFFFFFFF;
                break;
        }
    }
}

static void update_cpu_register(ucontext_t *uctx, const message_t *response) {
    // Update CPU register with read data
    // This is simplified - real implementation would identify the target register
    switch (response->length) {
        case 1:
            uctx->uc_mcontext.gregs[REG_RAX] = 
                (uctx->uc_mcontext.gregs[REG_RAX] & ~0xFF) | (response->data & 0xFF);
            break;
        case 2:
            uctx->uc_mcontext.gregs[REG_RAX] = 
                (uctx->uc_mcontext.gregs[REG_RAX] & ~0xFFFF) | (response->data & 0xFFFF);
            break;
        case 4:
        default:
            uctx->uc_mcontext.gregs[REG_RAX] = response->data;
            break;
    }
}

// SIGSEGV signal handler
static void segv_handler(int sig, siginfo_t *si, void *context) {
    (void)sig; // Suppress unused parameter warning
    uint64_t fault_addr = (uint64_t)si->si_addr;
    ucontext_t *uctx = (ucontext_t *)context;
    
    // Find device and parse instruction
    device_info_t *device = find_device_by_addr(fault_addr);
    if (!device) {
        fprintf(stderr, "SIGSEGV: Unknown address 0x%lx\n", fault_addr);
        exit(EXIT_FAILURE);
    }
    
    instruction_info_t inst_info = parse_instruction(uctx);
    
    // Create and send message
    message_t msg = {
        .device_id = device->device_id,
        .command = inst_info.is_write ? CMD_WRITE : CMD_READ,
        .address = (uint32_t)fault_addr,
        .length = inst_info.size,
        .data = 0,
        .result = 0
    };
    
    if (inst_info.is_write) {
        extract_write_data(&msg, uctx);
    }
    
    message_t response;
    if (send_message_to_model(&msg, &response) != 0) {
        fprintf(stderr, "Failed to communicate with simulator\n");
        exit(EXIT_FAILURE);
    }
    
    if (!inst_info.is_write) {
        update_cpu_register(uctx, &response);
    }
    
    // Skip instruction and continue
    uctx->uc_mcontext.gregs[REG_RIP] += inst_info.length;
}

// SIGUSR1 signal handler for interrupts
static void interrupt_signal_handler(int sig) {
    (void)sig; // Suppress unused parameter warning
    char filename[64];
    snprintf(filename, sizeof(filename), "/tmp/interrupt_info_%d", driver_pid);
    
    FILE *f = fopen(filename, "r");
    if (!f) return;
    
    uint32_t device_id, interrupt_id;
    if (fscanf(f, "%u,%u", &device_id, &interrupt_id) == 2) {
        if (device_id < 16 && interrupt_handlers[device_id]) {
            interrupt_handlers[device_id](interrupt_id);
        }
    }
    
    fclose(f);
    unlink(filename);
}

// Public API implementation
int interface_init(void) {
    driver_pid = getpid();
    
    // Set up SIGSEGV handler
    struct sigaction sa_segv;
    sa_segv.sa_sigaction = segv_handler;
    sigemptyset(&sa_segv.sa_mask);
    sa_segv.sa_flags = SA_SIGINFO;
    
    if (sigaction(SIGSEGV, &sa_segv, NULL) == -1) {
        perror("sigaction SIGSEGV");
        return -1;
    }
    
    // Set up SIGUSR1 handler for interrupts
    struct sigaction sa_usr1;
    sa_usr1.sa_handler = interrupt_signal_handler;
    sigemptyset(&sa_usr1.sa_mask);
    sa_usr1.sa_flags = 0;
    
    if (sigaction(SIGUSR1, &sa_usr1, NULL) == -1) {
        perror("sigaction SIGUSR1");
        return -1;
    }
    
    // Write PID file for simulator to find us
    char pid_filename[64];
    snprintf(pid_filename, sizeof(pid_filename), "/tmp/interface_driver_%d", driver_pid);
    FILE *pid_file = fopen(pid_filename, "w");
    if (pid_file) {
        fprintf(pid_file, "%d", driver_pid);
        fclose(pid_file);
    }
    
    return 0;
}

int register_device(uint32_t device_id, uint32_t base_address, uint32_t size) {
    if (device_count >= 16) {
        return -1; // Too many devices
    }
    
    // Create protected memory region using mmap
    void *mapped_memory = mmap((void*)(uintptr_t)base_address, size, PROT_NONE, 
                              MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    
    if (mapped_memory == MAP_FAILED) {
        perror("mmap");
        return -1;
    }
    
    devices[device_count].device_id = device_id;
    devices[device_count].base_address = base_address;
    devices[device_count].size = size;
    devices[device_count].mapped_memory = mapped_memory;
    device_count++;
    
    return 0;
}

int unregister_device(uint32_t device_id) {
    for (int i = 0; i < device_count; i++) {
        if (devices[i].device_id == device_id) {
            munmap(devices[i].mapped_memory, devices[i].size);
            // Shift remaining devices
            memmove(&devices[i], &devices[i+1], (device_count-i-1) * sizeof(device_info_t));
            device_count--;
            return 0;
        }
    }
    return -1;
}

int register_interrupt_handler(uint32_t device_id, interrupt_handler_t handler) {
    if (device_id >= 16) return -1;
    interrupt_handlers[device_id] = handler;
    return 0;
}

void interface_cleanup(void) {
    // Cleanup devices
    for (int i = 0; i < device_count; i++) {
        munmap(devices[i].mapped_memory, devices[i].size);
    }
    device_count = 0;
    
    // Remove PID file
    char pid_filename[64];
    snprintf(pid_filename, sizeof(pid_filename), "/tmp/interface_driver_%d", driver_pid);
    unlink(pid_filename);
}

device_info_t *find_device_by_addr(uint64_t address) {
    for (int i = 0; i < device_count; i++) {
        uint32_t base = devices[i].base_address;
        if (address >= base && address < base + devices[i].size) {
            return &devices[i];
        }
    }
    return NULL;
}

instruction_info_t parse_instruction(ucontext_t *uctx) {
    uint8_t *inst = (uint8_t *)uctx->uc_mcontext.gregs[REG_RIP];
    instruction_info_t info = {0};
    uint8_t *orig_inst = inst;
    
    // Skip prefixes and REX if present
    while (is_prefix(*inst)) {
        inst++;
    }
    
    uint8_t opcode = *inst;
    
    switch (opcode) {
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
        case 0xC7: // MOV [mem], imm32
            info.is_write = true;
            info.size = 4;
            break;
        case 0xC6: // MOV [mem], imm8
            info.is_write = true;
            info.size = 1;
            break;
        case 0x66: // 16-bit prefix
            inst++;
            if (*inst == 0x89) {
                info.is_write = true;
                info.size = 2;
            } else if (*inst == 0x8B) {
                info.is_write = false;
                info.size = 2;
            } else if (*inst == 0xC7) {
                info.is_write = true;
                info.size = 2;
            }
            break;
        default:
            // For unknown instructions, default to 32-bit read
            info.size = 4;
            info.is_write = false;
    }
    
    info.length = calculate_instruction_length(orig_inst);
    return info;
}

int send_message_to_model(const message_t *msg, message_t *resp) {
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    
    // Create a new socket for each request (simple approach)
    int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (client_fd == -1) {
        perror("socket");
        return -1;
    }
    
    // Try to connect to simulator
    if (connect(client_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        if (errno == ECONNREFUSED || errno == ENOENT) {
            // Simulator not ready, return default response
            close(client_fd);
            memset(resp, 0, sizeof(*resp));
            resp->result = 0;
            resp->data = 0;
            return 0;
        }
        perror("connect");
        close(client_fd);
        return -1;
    }
    
    // Send message
    if (send(client_fd, msg, sizeof(*msg), 0) != sizeof(*msg)) {
        perror("send");
        close(client_fd);
        return -1;
    }
    
    // Receive response
    if (recv(client_fd, resp, sizeof(*resp), 0) != sizeof(*resp)) {
        perror("recv");
        close(client_fd);
        return -1;
    }
    
    close(client_fd);
    return 0;
}