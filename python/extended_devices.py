#!/usr/bin/env python3
"""
Extended Device Models - Examples of more complex device simulations

This module demonstrates how to create more sophisticated device models
by extending the ModelInterface base class.
"""

import time
import threading
from device_model import ModelInterface


class UARTWithBuffer(ModelInterface):
    """Enhanced UART model with TX/RX buffers and baud rate simulation"""
    
    def __init__(self, device_id=0, base_address=0x40000000, baud_rate=9600):
        super().__init__(device_id, base_address)
        self.baud_rate = baud_rate
        self.tx_buffer = []
        self.rx_buffer = []
        self.tx_busy = False
        
        # UART Register map
        self.registers = {
            0x00: 0x00,  # TX Data Register
            0x04: 0x01,  # Status Register (TX Ready)
            0x08: 0x00,  # Control Register
            0x0C: 0x00,  # RX Data Register
            0x10: 0x00,  # Baud Rate Register
            0x14: 0x00,  # Interrupt Enable Register
        }
    
    def on_register_write(self, offset, value):
        """Handle UART register writes with enhanced behavior"""
        if offset == 0x00:  # TX Data Register
            if not self.tx_busy:
                self.tx_buffer.append(value)
                print(f"UART TX: 0x{value:02X} ('{chr(value)}' if printable)")
                self._start_transmission()
            else:
                print(f"UART TX buffer full, dropping data: 0x{value:02X}")
                
        elif offset == 0x08:  # Control Register
            if value & 0x01:  # Enable bit
                print("UART enabled")
                self.registers[0x04] |= 0x02  # Set enabled flag in status
            if value & 0x02:  # Reset bit
                print("UART reset")
                self.tx_buffer.clear()
                self.rx_buffer.clear()
                self.tx_busy = False
                self.registers[0x04] = 0x01  # Reset to TX ready
                
        elif offset == 0x10:  # Baud Rate Register
            if value > 0:
                self.baud_rate = 115200 // value  # Simple baud rate calculation
                print(f"UART baud rate set to {self.baud_rate}")
                
        elif offset == 0x14:  # Interrupt Enable Register
            print(f"UART interrupt enable: 0x{value:02X}")
    
    def _start_transmission(self):
        """Simulate transmission time based on baud rate"""
        if self.tx_busy:
            return
            
        self.tx_busy = True
        self.registers[0x04] &= ~0x01  # Clear TX ready flag
        
        # Calculate transmission time (10 bits per byte at given baud rate)
        tx_time = 10.0 / self.baud_rate
        
        def complete_transmission():
            time.sleep(tx_time)
            self.tx_busy = False
            self.registers[0x04] |= 0x01  # Set TX ready flag
            
            # Check if interrupt is enabled
            if self.registers[0x14] & 0x01:  # TX complete interrupt enabled
                self.trigger_interrupt(1)  # TX complete interrupt
        
        threading.Thread(target=complete_transmission).start()
    
    def inject_rx_data(self, data):
        """Inject received data (for simulation purposes)"""
        if len(self.rx_buffer) < 16:  # Simple 16-byte buffer
            self.rx_buffer.append(data)
            self.registers[0x0C] = data  # Update RX register
            self.registers[0x04] |= 0x04  # Set RX available flag
            
            # Check if RX interrupt is enabled
            if self.registers[0x14] & 0x02:  # RX interrupt enabled
                self.trigger_interrupt(2)  # RX interrupt
            
            print(f"UART RX: 0x{data:02X} ('{chr(data)}' if printable)")
        else:
            print(f"UART RX buffer overflow, dropping data: 0x{data:02X}")


class SimpleTimerModel(ModelInterface):
    """Simple timer device model with periodic interrupts"""
    
    def __init__(self, device_id=1, base_address=0x40001000):
        super().__init__(device_id, base_address)
        self.timer_thread = None
        self.running = False
        
        # Timer Register map
        self.registers = {
            0x00: 0x00000000,  # Counter Register (32-bit)
            0x04: 0x00000000,  # Reload Value Register
            0x08: 0x00000000,  # Control Register
            0x0C: 0x00000000,  # Status Register
        }
    
    def on_register_write(self, offset, value):
        """Handle timer register writes"""
        if offset == 0x04:  # Reload Value Register
            print(f"Timer reload value set to: {value}")
            
        elif offset == 0x08:  # Control Register
            if value & 0x01:  # Enable bit
                if not self.running:
                    print("Timer started")
                    self._start_timer()
            else:
                if self.running:
                    print("Timer stopped")
                    self._stop_timer()
                    
            if value & 0x02:  # Reset bit
                print("Timer reset")
                self.registers[0x00] = self.registers[0x04]  # Load counter with reload value
    
    def _start_timer(self):
        """Start the timer"""
        if self.running:
            return
            
        self.running = True
        self.registers[0x08] |= 0x01  # Set running bit in control register
        
        def timer_loop():
            counter = self.registers[0x04] if self.registers[0x04] > 0 else 1000
            self.registers[0x00] = counter
            
            while self.running and counter > 0:
                time.sleep(0.001)  # 1ms tick
                counter -= 1
                self.registers[0x00] = counter
                
                if counter == 0:
                    # Timer expired
                    self.registers[0x0C] |= 0x01  # Set timeout flag
                    print("Timer expired - triggering interrupt")
                    self.trigger_interrupt(1)  # Timer interrupt
                    
                    # Auto-reload if enabled
                    if self.registers[0x08] & 0x04:
                        counter = self.registers[0x04]
                        self.registers[0x00] = counter
                        print("Timer auto-reloaded")
                    else:
                        self.running = False
                        self.registers[0x08] &= ~0x01  # Clear running bit
        
        self.timer_thread = threading.Thread(target=timer_loop)
        self.timer_thread.start()
    
    def _stop_timer(self):
        """Stop the timer"""
        self.running = False
        self.registers[0x08] &= ~0x01  # Clear running bit
        if self.timer_thread:
            self.timer_thread.join(timeout=1.0)


def main():
    """Demonstration of multiple device types"""
    import sys
    
    if len(sys.argv) < 2:
        print("Usage: python extended_devices.py <device_type> [device_id] [base_address]")
        print("Device types: uart, timer")
        sys.exit(1)
    
    device_type = sys.argv[1].lower()
    device_id = int(sys.argv[2]) if len(sys.argv) > 2 else 0
    base_address = int(sys.argv[3], 0) if len(sys.argv) > 3 else (0x40000000 + device_id * 0x1000)
    
    if device_type == "uart":
        model = UARTWithBuffer(device_id, base_address)
        
        # Simulate some RX data injection for testing
        def inject_test_data():
            time.sleep(5)  # Wait 5 seconds
            model.inject_rx_data(ord('H'))
            time.sleep(1)
            model.inject_rx_data(ord('i'))
            time.sleep(1)
            model.inject_rx_data(ord('!'))
        
        threading.Thread(target=inject_test_data, daemon=True).start()
        
    elif device_type == "timer":
        model = SimpleTimerModel(device_id, base_address)
        
    else:
        print(f"Unknown device type: {device_type}")
        sys.exit(1)
    
    try:
        model.start_server()
    except KeyboardInterrupt:
        print(f"\nShutting down {device_type} simulator...")
    finally:
        model.stop_server()


if __name__ == "__main__":
    main()