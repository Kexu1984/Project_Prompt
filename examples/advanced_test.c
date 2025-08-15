#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include "interface_layer.h"

// Device base addresses
#define UART_BASE_ADDR    0x40000000
#define TIMER_BASE_ADDR   0x40001000
#define DEVICE_SIZE       0x1000

// UART registers
#define UART_TX_REG       (UART_BASE_ADDR + 0x00)
#define UART_STATUS_REG   (UART_BASE_ADDR + 0x04)
#define UART_CTRL_REG     (UART_BASE_ADDR + 0x08)
#define UART_RX_REG       (UART_BASE_ADDR + 0x0C)
#define UART_BAUD_REG     (UART_BASE_ADDR + 0x10)
#define UART_IRQ_EN_REG   (UART_BASE_ADDR + 0x14)

// Timer registers
#define TIMER_COUNTER_REG (TIMER_BASE_ADDR + 0x00)
#define TIMER_RELOAD_REG  (TIMER_BASE_ADDR + 0x04)
#define TIMER_CTRL_REG    (TIMER_BASE_ADDR + 0x08)
#define TIMER_STATUS_REG  (TIMER_BASE_ADDR + 0x0C)

// Global interrupt flags
static volatile int uart_tx_complete = 0;
static volatile int uart_rx_received = 0;
static volatile int timer_expired = 0;

// Interrupt handlers
void uart_interrupt_handler(uint32_t interrupt_id) {
    printf("UART Interrupt ID: %u\n", interrupt_id);
    switch (interrupt_id) {
        case 1: // TX complete
            uart_tx_complete = 1;
            printf("  - TX Complete\n");
            break;
        case 2: // RX received
            uart_rx_received = 1;
            printf("  - RX Data Available\n");
            break;
    }
}

void timer_interrupt_handler(uint32_t interrupt_id) {
    printf("Timer Interrupt ID: %u\n", interrupt_id);
    if (interrupt_id == 1) {
        timer_expired = 1;
        printf("  - Timer Expired\n");
    }
}

int test_uart_advanced() {
    printf("\n=== Advanced UART Test ===\n");
    
    // Enable UART with interrupts
    printf("Enabling UART with interrupts...\n");
    *(volatile uint32_t*)UART_CTRL_REG = 0x01;  // Enable UART
    *(volatile uint32_t*)UART_IRQ_EN_REG = 0x03; // Enable TX and RX interrupts
    
    // Set baud rate
    printf("Setting baud rate...\n");
    *(volatile uint32_t*)UART_BAUD_REG = 12;  // 115200/12 ≈ 9600 baud
    
    // Send some data
    printf("Sending 'Hello' via UART...\n");
    const char *message = "Hello";
    for (int i = 0; message[i]; i++) {
        // Wait for TX ready
        while (!(*(volatile uint32_t*)UART_STATUS_REG & 0x01)) {
            usleep(1000); // 1ms
        }
        
        *(volatile uint32_t*)UART_TX_REG = message[i];
        
        // Wait for TX complete interrupt
        int timeout = 100; // 1 second timeout
        while (!uart_tx_complete && timeout > 0) {
            usleep(10000); // 10ms
            timeout--;
        }
        
        if (uart_tx_complete) {
            printf("  Character '%c' sent successfully\n", message[i]);
            uart_tx_complete = 0; // Reset flag
        } else {
            printf("  Timeout waiting for TX complete\n");
        }
    }
    
    // Wait for RX data (simulated injection)
    printf("Waiting for RX data...\n");
    int rx_timeout = 1000; // 10 seconds
    while (!uart_rx_received && rx_timeout > 0) {
        usleep(10000); // 10ms
        rx_timeout--;
    }
    
    if (uart_rx_received) {
        uint32_t rx_data = *(volatile uint32_t*)UART_RX_REG;
        printf("Received data: 0x%02X ('%c')\n", rx_data, (char)rx_data);
        uart_rx_received = 0; // Reset flag
    } else {
        printf("No RX data received (this is normal if no injection occurred)\n");
    }
    
    return 0;
}

int test_timer() {
    printf("\n=== Timer Test ===\n");
    
    // Configure timer for 100ms (assuming 1ms tick)
    printf("Setting timer for 100ms...\n");
    *(volatile uint32_t*)TIMER_RELOAD_REG = 100;
    
    // Start timer with auto-reload
    printf("Starting timer with auto-reload...\n");
    *(volatile uint32_t*)TIMER_CTRL_REG = 0x05; // Enable + Auto-reload
    
    // Wait for a few timer interrupts
    printf("Waiting for timer interrupts...\n");
    for (int i = 0; i < 3; i++) {
        timer_expired = 0;
        int timeout = 200; // 2 second timeout
        
        while (!timer_expired && timeout > 0) {
            usleep(10000); // 10ms
            timeout--;
        }
        
        if (timer_expired) {
            uint32_t counter = *(volatile uint32_t*)TIMER_COUNTER_REG;
            printf("Timer interrupt %d received, counter: %u\n", i+1, counter);
        } else {
            printf("Timer interrupt %d timeout\n", i+1);
        }
    }
    
    // Stop timer
    printf("Stopping timer...\n");
    *(volatile uint32_t*)TIMER_CTRL_REG = 0x00;
    
    return 0;
}

int test_multiple_devices() {
    printf("\n=== Multiple Device Interaction Test ===\n");
    
    // Use timer to trigger periodic UART transmissions
    printf("Setting up timer-driven UART transmission...\n");
    
    // Configure timer for 200ms intervals
    *(volatile uint32_t*)TIMER_RELOAD_REG = 200;
    *(volatile uint32_t*)TIMER_CTRL_REG = 0x05; // Enable + Auto-reload
    
    const char *test_chars = "ABC";
    int char_index = 0;
    
    for (int i = 0; i < 3; i++) {
        // Wait for timer interrupt
        timer_expired = 0;
        int timeout = 300; // 3 second timeout
        
        while (!timer_expired && timeout > 0) {
            usleep(10000); // 10ms
            timeout--;
        }
        
        if (timer_expired) {
            printf("Timer triggered - sending '%c'\n", test_chars[char_index]);
            *(volatile uint32_t*)UART_TX_REG = test_chars[char_index];
            char_index = (char_index + 1) % 3;
            
            // Wait for UART TX complete
            uart_tx_complete = 0;
            timeout = 100;
            while (!uart_tx_complete && timeout > 0) {
                usleep(10000);
                timeout--;
            }
            
            if (uart_tx_complete) {
                printf("  - UART transmission complete\n");
            }
        }
    }
    
    // Stop timer
    *(volatile uint32_t*)TIMER_CTRL_REG = 0x00;
    
    return 0;
}

int main() {
    printf("=== Advanced Driver-Simulator Interface Test ===\n");
    
    // Initialize interface
    if (interface_init() != 0) {
        fprintf(stderr, "Failed to initialize interface\n");
        return 1;
    }
    
    // Register UART device
    printf("Registering UART device...\n");
    if (register_device(0, UART_BASE_ADDR, DEVICE_SIZE) != 0) {
        fprintf(stderr, "Failed to register UART device\n");
        interface_cleanup();
        return 1;
    }
    
    // Register Timer device
    printf("Registering Timer device...\n");
    if (register_device(1, TIMER_BASE_ADDR, DEVICE_SIZE) != 0) {
        fprintf(stderr, "Failed to register Timer device\n");
        interface_cleanup();
        return 1;
    }
    
    // Register interrupt handlers
    if (register_interrupt_handler(0, uart_interrupt_handler) != 0 ||
        register_interrupt_handler(1, timer_interrupt_handler) != 0) {
        fprintf(stderr, "Failed to register interrupt handlers\n");
        interface_cleanup();
        return 1;
    }
    
    printf("Initialization complete. Waiting for simulators to be ready...\n");
    sleep(2);
    
    // Run tests
    if (test_uart_advanced() != 0) {
        printf("UART test failed\n");
        interface_cleanup();
        return 1;
    }
    
    if (test_timer() != 0) {
        printf("Timer test failed\n");
        interface_cleanup();
        return 1;
    }
    
    if (test_multiple_devices() != 0) {
        printf("Multiple device test failed\n");
        interface_cleanup();
        return 1;
    }
    
    printf("\n=== All advanced tests completed successfully! ===\n");
    
    // Cleanup
    interface_cleanup();
    return 0;
}