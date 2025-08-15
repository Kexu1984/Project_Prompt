#ifndef INTERFACE_LAYER_H
#define INTERFACE_LAYER_H

#include <stdint.h>
#include <stdbool.h>
#include <signal.h>

// Command types for communication protocol
typedef enum {
    CMD_READ = 1,
    CMD_WRITE = 2
} command_t;

// Message structure for driver-simulator communication
typedef struct {
    uint32_t device_id;
    command_t command;
    uint32_t address;
    uint32_t data;
    uint32_t length;
    int result;
} message_t;

// Device information structure
typedef struct {
    uint32_t device_id;
    uint32_t base_address;
    uint32_t size;
    void *mapped_memory;
} device_info_t;

// Instruction parsing information
typedef struct {
    bool is_write;
    int size;
    int length;
} instruction_info_t;

// Interrupt handler function type
typedef void (*interrupt_handler_t)(uint32_t interrupt_id);

// Core interface functions
int interface_init(void);
int register_device(uint32_t device_id, uint32_t base_address, uint32_t size);
int unregister_device(uint32_t device_id);
int register_interrupt_handler(uint32_t device_id, interrupt_handler_t handler);
void interface_cleanup(void);

// Internal functions (exposed for testing)
device_info_t *find_device_by_addr(uint64_t address);
instruction_info_t parse_instruction(ucontext_t *uctx);
int send_message_to_model(const message_t *msg, message_t *resp);

#endif // INTERFACE_LAYER_H