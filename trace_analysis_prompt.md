**Description:**

When running MCU driver code on the simulator, a trace log file is generated. To help analyze this log and diagnose potential issues, we need to build a visualization tool.

I recommend using a **web-based interface** to display the trace data in an interactive and user-friendly format.

---

### 🔍 Visualization Requirements

* Display a **time-based horizontal timeline**.
* Events should be plotted on the timeline with **distinct colors** based on event type.
* Support **zooming in/out** on the timeline using mouse interactions.
* **On-hover tooltip** should show full event details, including fields such as timestamp, operation, device name, address, etc.

---

### 📦 Supported Event Types

Trace events are classified into three types:

```python
class EventType:
    BUS_TRANSACTION = 'BUS_TRANSACTION'  # Bus Read/Write operations
    IRQ_EVENT = 'IRQ_EVENT'              # Interrupt events
    DEVICE_EVENT = 'DEVICE_EVENT'        # Device-level operations and state transitions
```

#### Bus Operations:

```python
class BusOperation:
    READ = 'READ'
    WRITE = 'WRITE'
```

#### Device Operations:

```python
class DeviceOperation:
    READ = 'READ'
    WRITE = 'WRITE'
    READ_FAILED = 'READ_FAILED'
    WRITE_FAILED = 'WRITE_FAILED'
    RESET = 'RESET'
    ENABLE = 'ENABLE'
    DISABLE = 'DISABLE'
    IRQ_TRIGGER = 'IRQ_TRIGGER'
    IRQ_TRIGGER_FAILED = 'IRQ_TRIGGER_FAILED'
    INIT_START = 'INIT_START'
    INIT_COMPLETE = 'INIT_COMPLETE'
    RESET_START = 'RESET_START'
    RESET_COMPLETE = 'RESET_COMPLETE'
    SHUTDOWN_START = 'SHUTDOWN_START'
    SHUTDOWN_COMPLETE = 'SHUTDOWN_COMPLETE'
```

---

### 📄 Trace Log Format

The trace log is a JSON file structured with a `trace_info` header and a list of `events`. Each event includes a timestamp, module name, event type, and corresponding data.

Example:

```json
{
  "trace_info": {
    "total_events": 13,
    "saved_at": "2025-08-04T11:25:30.770201",
    "trace_manager": "GlobalTraceManager"
  },
  "events": [
    {
      "timestamp": 1754306730.7695093,
      "formatted_time": "2025-08-04 11:25:30.769",
      "module_name": "MainRAM",
      "event_type": "DEVICE_EVENT",
      "event_data": {
        "device_name": "MainRAM",
        "operation": "WRITE",
        "address": "0x20000000",
        "offset": "0x00000000",
        "value": "0xDEADBEEF",
        "width": 4
      }
    },
    {
      "timestamp": 1754306730.7695498,
      "formatted_time": "2025-08-04 11:25:30.769",
      "module_name": "MainBus",
      "event_type": "BUS_TRANSACTION",
      "event_data": {
        "master_id": 1,
        "address": "0x20000000",
        "operation": "WRITE",
        "value": "0xDEADBEEF",
        "width": 4,
        "device_name": "MainRAM",
        "success": true,
        "error_message": null
      }
    }
  ]
}
```

---

### 🧪 Testing

A sample log file `unified_trace_demo.json` is provided for testing and development purposes.

---

Let me know if you'd like the frontend to be implemented in a specific framework (e.g. React, Plotly Dash, etc.), or if Python-based visualization (e.g. using Plotly, Streamlit) is preferred.
