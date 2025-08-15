#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/wait.h>
#include "interface_layer.h"

// UART register definitions (example device)
#define UART_BASE_ADDR    0x40000000
#define UART_SIZE         0x1000
#define UART_TX_REG       (UART_BASE_ADDR + 0x00)
#define UART_STATUS_REG   (UART_BASE_ADDR + 0x04)
#define UART_CTRL_REG     (UART_BASE_ADDR + 0x08)
#define UART_RX_REG       (UART_BASE_ADDR + 0x0C)

// Global variables for testing
static volatile int interrupt_received = 0;

// Interrupt handler for testing
void test_interrupt_handler(uint32_t interrupt_id) {
    printf("Interrupt received: ID = %u\n", interrupt_id);
    interrupt_received = 1;
}

// Test register access
int test_register_access() {
    printf("Testing register access...\n");
    
    // Test write operation
    printf("Writing 0x55 to UART TX register...\n");
    uint32_t write_val = 0x55;
    *(volatile uint32_t*)UART_TX_REG = write_val;
    
    // Test read operation
    printf("Reading from UART status register...\n");
    volatile uint32_t status = *(volatile uint32_t*)UART_STATUS_REG;
    printf("Status register value: 0x%08X\n", status);
    
    // Test control register
    printf("Writing 0x01 to UART control register (enable)...\n");
    *(volatile uint32_t*)UART_CTRL_REG = 0x01;
    
    // Read back status to see if device was enabled
    status = *(volatile uint32_t*)UART_STATUS_REG;
    printf("Status after enable: 0x%08X\n", status);
    
    return 0;
}

// Test interrupt handling
int test_interrupt_handling() {
    printf("Testing interrupt handling...\n");
    
    // Wait for interrupt (should come from UART TX complete simulation)
    int timeout = 50;  // 5 seconds timeout
    while (!interrupt_received && timeout > 0) {
        usleep(100000);  // 100ms
        timeout--;
    }
    
    if (interrupt_received) {
        printf("Interrupt test passed!\n");
        return 0;
    } else {
        printf("Interrupt test failed - no interrupt received\n");
        return -1;
    }
}

int main() {
    printf("=== Driver-Simulator Interface Test ===\n");
    
    // Initialize interface
    printf("Initializing interface layer...\n");
    if (interface_init() != 0) {
        fprintf(stderr, "Failed to initialize interface\n");
        return 1;
    }
    
    // Register UART device
    printf("Registering UART device (ID: 0, Base: 0x%08X, Size: 0x%X)...\n", 
           UART_BASE_ADDR, UART_SIZE);
    if (register_device(0, UART_BASE_ADDR, UART_SIZE) != 0) {
        fprintf(stderr, "Failed to register UART device\n");
        interface_cleanup();
        return 1;
    }
    
    // Register interrupt handler
    printf("Registering interrupt handler for device 0...\n");
    if (register_interrupt_handler(0, test_interrupt_handler) != 0) {
        fprintf(stderr, "Failed to register interrupt handler\n");
        interface_cleanup();
        return 1;
    }
    
    // Give simulator time to start (if running in background)
    printf("Waiting for simulator to be ready...\n");
    sleep(1);
    
    // Test register access
    if (test_register_access() != 0) {
        fprintf(stderr, "Register access test failed\n");
        interface_cleanup();
        return 1;
    }
    
    // Test interrupt handling
    if (test_interrupt_handling() != 0) {
        fprintf(stderr, "Interrupt handling test failed\n");
        interface_cleanup();
        return 1;
    }
    
    printf("=== All tests passed! ===\n");
    
    // Cleanup
    interface_cleanup();
    return 0;
}