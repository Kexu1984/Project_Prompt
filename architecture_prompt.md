# Hardware Simulator Framework Architecture Requirements

You are an engineer proficient in embedded systems, with extensive knowledge of chip hardware implementation principles and driver programs. This is a hardware simulator framework implemented in Python, designed to simulate a SoC/MCU system that includes bus, DMA, and other peripherals internally.

I need to create a hardware module framework that is implemented in **Python (Python3)** and runs on a **Linux environment**, with the following requirements:

## Framework Components

1. This framework should include at least several main components: **Top model**, **bus model**, and **device model**;

2. **Top model** is used to combine all the components described below, and should include a bus, a memory model, and at least one DMA device model

3. **Bus model** is mainly used to simulate bus behavior

4. **Device model** is used to simulate the behavior of various peripherals, such as: UART, CRC, DMA, memory, etc.

5. Should include a common **trace class** for recording access activities of top/bus and each device

By combining all these components mentioned above, I can generate a nearly complete MCU simulation framework!!!

## Bus Model Requirements

1. **Bus model** should have the capability to manage device models, such as: `add_device` and `remove_device`; These two functions are used to add devices to the bus model, and should also be able to pass the bus instance itself to the device, so that the corresponding device can call the bus interface

2. Each device should contain at least the following information: **address space**. The bus should check whether the address space of the device to be added conflicts with existing configurations before adding the corresponding device

3. **Bus** should include a **global lock** to manage multithreaded access requests from external sources (C language programs)

4. **Bus** should provide two external interfaces: **read** and **write**:

   4.1) **Read interface** needs to recognize the parameter: `master_id` (each device should have an independent master ID)

   4.2) **Read interface** needs to recognize the parameter: `address`. If the address is not within any registered device address space, return response error; If a matching address space is found, continue to call the found device.read interface to dispatch the read request downward

   4.3) **Write interface** is similar to the read interface, except that write is used to dispatch write requests

   4.4) **Read interface** should return the content read by the device, **write interface** should return the status of the device write operation

## Device Model Requirements

1. There should be a **common class**: **base class**, and any specific device model should inherit this base class

   1.1) **Base class** should include the following basic functions: `init`, `read`, `write`, `register_irq_callback`;

   1.2) `read` and `write` typically correspond to device register read and write operations, which will eventually call the read_callback and write_callback of the register manager mentioned in section 2.1 below

   1.3) If the corresponding device is a memory model, then read and write correspond to memory read and write operations, in which case read_callback and write_callback are not needed

   1.4) `init` typically performs the following operations: register addition, status setting, etc.;

   1.5) `register_irq_callback` function purpose: If external implementation of interrupt sending functionality exists, the send irq function can be passed to this device through register_irq_callback;

2. There should be a **common class**: **register manager class**, and any specific device model should include this class to describe all registers of the device

   2.1) **Register manager class** should include a function: `add_register`, used to add a register, with the following function prototype:

   ```python
   def define_register(self, offset: int, name: str, register_type: RegisterType = RegisterType.READ_WRITE,
                      reset_value: int = 0, mask: int = 0xFFFFFFFF,
                      read_callback: Optional[Callable[[self, int, int], int]] = None,
                      write_callback: Optional[Callable[[self, int, int], None]] = None) -> None
   ```

   - **offset**: register offset address
   - **read_callback**: If some registers' read behavior triggers additional operations, then the corresponding register needs to register this read_callback. For example: some status registers have "read-to-clear" functionality, so when external attempts to read this register, the read_callback should clear the corresponding status bits;
   - **write_callback**: If some registers' write behavior triggers additional operations, then the corresponding register needs to register this write_callback. For example: if enable bits of some control registers are set during write operations, it means triggering this device to start working, so the write_callback should include the specific workflow of the device
   - **read_callback** and **write_callback** should be able to access all registers of the current device, because read/write operations on some registers may affect the values of other registers

3. **Device_model** should be able to be instantiated multiple times to represent multiple identical devices. For example: a SoC may contain multiple UART units

4. If **device_model** has the capability to trigger interrupts, it should send interrupts externally through the send irq callback after a corresponding device job is completed;

5. If **device_model** has the capability to access DMA, it should also support the **dma interface** class for actively operating DMA devices;

6. Memory read and write can support non-4-byte widths, so memory read and write should add an additional parameter: **width**; Therefore, the width parameter for read/write of other devices defaults to 4 bytes

7. For devices that can connect to peripherals (such as: UART, SPI, CAN, etc.), an **IO Interface class** needs to be integrated, which has the following specifications:

   7.1) Support **Input** and **Output** functions to support device data input and output functionality: It is known that C driver code can only operate registers to implement device Output functionality. For example: after the driver writes data to the UART TX register, UART can send data to external devices;

   7.2) For **Input**, the device needs to have a **connect interface** to indicate "there is a peripheral connected to this device". If it requires the current device's output functionality to connect, external devices need to create threads to wait for output data. If it requires the current device's input functionality to connect, then the device needs to create a thread to obtain data that external devices may send at any time;

   7.3) Whether input or output, there are at least two parameters: **data** and **width**, representing the data and width of input/output respectively;

## Top Model Requirements

- Support input parameters to select **config.yaml/config.json** to describe which components are included in the current system;

- **Top model** should first initialize its environment, then create components sequentially according to the config file: bus, memory, device, etc., and then add devices to the bus as mentioned earlier

- I also need a **test model**. The test model does the same thing as the top model, except that the test model creates fewer devices, and will also implement access to device registers through read and write operations on addresses, thereby implementing device operation processes

- A complete test communication link should at least include: `test_model` → `bus_model` → `device_model` → `register manager`, and be able to get correct return results;

- I can provide an example: In the test model, configure CRC module and DMA module to implement DMA's mem2peri mode functionality. After inputting memory content, CRC should be able to automatically calculate the final result through DMA and match the expected result; It should also be able to directly configure DMA to implement its basic mem2mem memory copy functionality

- The test model also needs to verify the trace functionality of each model

## Trace Class Requirements

1. **Trace functionality** should be made into a common class, so that top/bus/device components all support this trace functionality;

2. **Trace** supports module-based on/off switching, which can save space and facilitate analysis of specific module functionality;

3. The **timestamp** in existing trace information has poor readability, optimize it to standard time

4. **Trace information** needs to be saved in files: All trace information needs to be recorded in one file, meaning the trace buffer is shared, uniformly managed by the trace manager;

## Other Notes

- Each module should generate a corresponding **README file**, which should at least include description information for each register