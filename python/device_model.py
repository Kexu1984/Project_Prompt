#!/usr/bin/env python3
"""
Device Model Layer for Driver-Simulator Communication Interface

This module implements the Python-based hardware simulator that communicates
with the C driver through Unix socket and handles register read/write operations.
"""

import os
import sys
import socket
import signal
import struct
import threading
import time
from typing import Dict, Optional, Callable


class ModelInterface:
    """Base interface for device models to communicate with C drivers"""
    
    def __init__(self, device_id: int, base_address: int = 0x40000000):
        self.device_id = device_id
        self.base_address = base_address
        self.registers: Dict[int, int] = {}
        self.driver_pid: Optional[int] = None
        self.socket_path = "/tmp/driver_simulator_socket"
        self.server_socket: Optional[socket.socket] = None
        self.running = False
        
    def handle_message(self, msg: Dict) -> Dict:
        """Handle read/write requests from C driver"""
        offset = msg['address'] - self.base_address
        
        if msg['command'] == 1:  # READ
            data = self.registers.get(offset, 0)
            return {'result': 0, 'data': data, 'device_id': self.device_id, 
                   'command': msg['command'], 'address': msg['address'], 'length': msg['length']}
        elif msg['command'] == 2:  # WRITE
            self.registers[offset] = msg['data']
            self.on_register_write(offset, msg['data'])
            return {'result': 0, 'data': 0, 'device_id': self.device_id,
                   'command': msg['command'], 'address': msg['address'], 'length': msg['length']}
        else:
            return {'result': -1, 'data': 0, 'device_id': self.device_id,
                   'command': msg['command'], 'address': msg['address'], 'length': msg['length']}
    
    def on_register_write(self, offset: int, value: int):
        """Override this method to handle register write side effects"""
        pass
    
    def trigger_interrupt(self, interrupt_id: int):
        """Send interrupt to driver"""
        if not self.driver_pid:
            self.driver_pid = self.get_driver_pid()
            
        if self.driver_pid:
            filename = f"/tmp/interrupt_info_{self.driver_pid}"
            try:
                with open(filename, "w") as f:
                    f.write(f"{self.device_id},{interrupt_id}")
                os.kill(self.driver_pid, signal.SIGUSR1)
            except (OSError, ProcessLookupError) as e:
                print(f"Failed to send interrupt: {e}")
    
    def get_driver_pid(self) -> Optional[int]:
        """Find driver PID from temporary file"""
        import glob
        pid_files = glob.glob("/tmp/interface_driver_*")
        if pid_files:
            try:
                with open(pid_files[0], "r") as f:
                    return int(f.read().strip())
            except (ValueError, IOError):
                pass
        return None
    
    def start_server(self):
        """Start the socket server to communicate with C driver"""
        # Remove existing socket file
        try:
            os.unlink(self.socket_path)
        except OSError:
            pass
        
        self.server_socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.server_socket.bind(self.socket_path)
        self.server_socket.listen(1)
        self.running = True
        
        print(f"Device {self.device_id} simulator started, listening on {self.socket_path}")
        
        while self.running:
            try:
                client_socket, _ = self.server_socket.accept()
                threading.Thread(target=self.handle_client, args=(client_socket,)).start()
            except OSError:
                if self.running:
                    print("Socket accept failed")
                break
    
    def handle_client(self, client_socket: socket.socket):
        """Handle client connection and message processing"""
        try:
            while self.running:
                # Receive message (40 bytes: device_id, command, address, data, length, result)
                data = client_socket.recv(24)  # sizeof(message_t) in C
                if len(data) != 24:
                    break
                
                # Unpack message
                device_id, command, address, data_value, length, result = struct.unpack('IIIIII', data)
                
                msg = {
                    'device_id': device_id,
                    'command': command,
                    'address': address,
                    'data': data_value,
                    'length': length,
                    'result': result
                }
                
                # Handle message
                response = self.handle_message(msg)
                
                # Pack and send response
                response_data = struct.pack('IIIIII',
                    response['device_id'],
                    response['command'], 
                    response['address'],
                    response['data'],
                    response['length'],
                    response['result']
                )
                
                client_socket.send(response_data)
                
        except Exception as e:
            print(f"Error handling client: {e}")
        finally:
            client_socket.close()
    
    def stop_server(self):
        """Stop the socket server"""
        self.running = False
        if self.server_socket:
            self.server_socket.close()
        try:
            os.unlink(self.socket_path)
        except OSError:
            pass


class SimpleUARTModel(ModelInterface):
    """Simple UART device model for testing"""
    
    def __init__(self, device_id: int = 0, base_address: int = 0x40000000):
        super().__init__(device_id, base_address)
        # Initialize UART registers
        self.registers[0x00] = 0x00  # TX Data Register
        self.registers[0x04] = 0x01  # Status Register (TX Ready)
        self.registers[0x08] = 0x00  # Control Register
        self.registers[0x0C] = 0x00  # RX Data Register
        
    def on_register_write(self, offset: int, value: int):
        """Handle UART register writes"""
        if offset == 0x00:  # TX Data Register
            print(f"UART TX: 0x{value:02X} ('{chr(value)}' if printable)")
            # Simulate TX complete - trigger interrupt after delay
            threading.Timer(0.1, lambda: self.trigger_interrupt(1)).start()
        elif offset == 0x08:  # Control Register
            if value & 0x01:  # Enable bit
                print("UART enabled")
                self.registers[0x04] |= 0x02  # Set enabled flag in status


def main():
    """Main function to run the device simulator"""
    if len(sys.argv) < 2:
        print("Usage: python device_model.py <device_id> [base_address]")
        sys.exit(1)
    
    device_id = int(sys.argv[1])
    base_address = int(sys.argv[2], 0) if len(sys.argv) > 2 else 0x40000000
    
    # Create device model
    if device_id == 0:
        model = SimpleUARTModel(device_id, base_address)
    else:
        model = ModelInterface(device_id, base_address)
    
    try:
        model.start_server()
    except KeyboardInterrupt:
        print("\nShutting down simulator...")
    finally:
        model.stop_server()


if __name__ == "__main__":
    main()