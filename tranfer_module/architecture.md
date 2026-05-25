# 1. Revision History

| Version | Date       | Author               | Description              |
|---------|------------|----------------------|--------------------------|
| 1.0     | 2025-02-21 | NGUYEN THANH Minh    |              |
| 2.0     | 2026-05-25 | NGUYEN VAN Tiem    | Restructure architecture  |

---

# 2. Introduction

This document describes the design of the **CMCELL** module — a C library that allows embedded applications to communicate with cellular modems over UART. It covers the system architecture, key design decisions, and how each feature works.

The document is intended for:
- **Developers integrating CMCELL** into a new product who need to understand how to call the APIs and handle events.
- **Developers extending CMCELL** who need to understand the internal structure to add new features or support new modems.

## 2.1. Purpose

This document helps developers who use or maintain the CMCELL module:
- Understand the overall architecture and how layers communicate.
- Know where each feature lives in the codebase.
- Learn how to integrate CMCELL into an application, handle errors, and design a high-level application around it.
- Understand how to add support for a new modem or a new feature.

## 2.2. Scope

This document covers the CMCELL library (`cellular/`, `modem/`, `com/` directories). It does not cover:
- The modem hardware itself (BG95 AT command manual is an external reference).
- The RTOS or OS environment (the application provides that).
- The UART driver (the application provides that).

Features in scope: Connection Management, Monitoring, DFOTA (Firmware Update), Socket, Logging, URC Handling, Power Management, AT Command engine.

## 2.3. Definitions and Acronyms

| Term         | Definition |
|--------------|------------|
| **APN**      | Access Point Name — identifies the network gateway for data sessions |
| **AT command** | A text command sent over UART to control a modem (Hayes command set) |
| **CAT-M1**   | LTE Cat-M1 — a low-power LTE standard for IoT devices |
| **CME**      | Mobile Equipment error — AT command error type |
| **CMS**      | Message service error — AT command error type |
| **CS**       | Circuit-Switched — traditional voice/data network domain |
| **DFOTA**    | Delta Firmware Over-The-Air — modem firmware update |
| **eDRX**     | Extended Discontinuous Reception — power-saving mechanism |
| **EMM**      | EPS Mobility Management — LTE connection management protocol |
| **ICCID**    | Integrated Circuit Card Identifier — unique SIM card number |
| **IMEI**     | International Mobile Equipment Identity — unique modem identifier |
| **IMSI**     | International Mobile Subscriber Identity — unique subscriber number |
| **ISR**      | Interrupt Service Routine |
| **MCC/MNC**  | Mobile Country Code / Mobile Network Code |
| **NB-IoT**   | Narrowband IoT — low-power wide-area network standard |
| **PDN**      | Packet Data Network — the data channel between modem and internet |
| **PLMN**     | Public Land Mobile Network — identified by MCC+MNC |
| **PS**       | Packet-Switched — data network domain |
| **PSM**      | Power Saving Mode — puts modem in deep sleep between transmissions |
| **RAT**      | Radio Access Technology — GSM, CAT-M1, NB-IoT, LTE |
| **RSSI**     | Received Signal Strength Indicator |
| **RSRP**     | Reference Signal Received Power (LTE) |
| **RSRQ**     | Reference Signal Received Quality (LTE) |
| **RTOS**     | Real-Time Operating System |
| **RXNE**     | Receive Not Empty — UART interrupt flag |
| **SIM**      | Subscriber Identity Module |
| **TAC**      | Tracking Area Code |
| **TCP/UDP**  | Transport Control Protocol / User Datagram Protocol |
| **URC**      | Unsolicited Result Code — a message the modem sends without being asked |
| **UART**     | Universal Asynchronous Receiver-Transmitter — serial communication bus |

---

# 3. Requirements

The following is a summary of the key functional requirements CMCELL must satisfy:

| ID  | Requirement |
|-----|-------------|
| R01 | Support the BG95 modem (HL7900 is planned for future support) |
| R02 | Establish a 4G (LTE/CAT-M1/NB-IoT) or 2G (GSM) cellular data connection |
| R03 | Automatically fall back to 2G if 4G is not available |
| R04 | All application-facing APIs must be non-blocking |
| R05 | Notify the application of modem events via registered callbacks |
| R06 | Support updating modem firmware locally (MCU pushes blocks) and wirelessly (HTTP, CoAP, FTP) |
| R07 | Support TCP and UDP socket communication (client and server) with up to 12 simultaneous sockets |
| R08 | Provide network monitoring: signal quality, cell info, registration status, network info |
| R09 | Provide modem and SIM information on demand |
| R10 | Support Power Saving Mode (PSM) and eDRX configuration |
| R11 | Portable across different RTOS environments — OS services are abstracted |
| R12 | UART driver is abstracted — the application provides the driver implementation |
| R13 | Configurable logging with four verbosity levels |
| R14 | Support up to 6 simultaneous application contexts |

---

# 4. System Overview

## 4.1. System Architecture

CMCELL sits between the application and the modem hardware. It hides all the complexity of AT command sequences, timing, and modem-specific behavior behind a clean, event-driven API.

```
┌──────────────────────────────────────┐
│          Application Layer           │  ← User code: calls cell_* APIs,
│  (calls APIs, receives callbacks)    │    receives events via callbacks
└────────────────┬─────────────────────┘
                 │  cell_api.h (Public API)
┌────────────────▼─────────────────────┐
│            CMCELL Module             │
│  ┌──────────────────────────────┐    │
│  │     cellular/ layer          │    │  ← Feature logic:
│  │  Connection │ DFOTA │ Socket │    │    Connection, Monitoring,
│  │  Monitoring │ Log   │ OS     │    │    DFOTA, Socket, Logging
│  └──────────────────────────────┘    │
│  ┌──────────────────────────────┐    │
│  │       modem/ layer           │    │  ← AT command engine,
│  │  AT Engine │ BG95 │ URC      │    │    modem-specific drivers,
│  └──────────────────────────────┘    │    URC parsing
│  ┌──────────────────────────────┐    │
│  │        com/ layer            │    │  ← UART abstraction
│  │  cell_com │ driver_interface │    │
│  └──────────────────────────────┘    │
└────────────────┬─────────────────────┘
                 │  UART (AT commands / responses)
┌────────────────▼─────────────────────┐
│           Modem Hardware             │  ← BG95 (or future modems)
│               (BG95)                 │
└──────────────────────────────────────┘
```

## 4.2. Design Goals

### 4.2.1. Reusability

The modem-specific code is isolated behind the `modem_driver_t` struct — a table of function pointers. Each modem (e.g., BG95) provides its own implementation. Upper layers only use this interface, so adding a new modem requires only a new driver implementation without touching any feature logic.

### 4.2.2. Maintainability

All feature processing runs in a single RTOS task (`cell_task`). Features communicate via a message queue instead of calling each other directly. This removes the need for mutexes and makes the execution flow easy to trace: every action starts with a message and ends with a callback.

### 4.2.3. Portability

Two abstraction layers separate CMCELL from the target hardware:
- **`cell_os.h`** — abstracts RTOS services: queue, timer, sleep, watchdog, time.
- **`cell_driver_interface.h`** — abstracts the UART hardware driver.

The application provides the implementations for these. CMCELL itself contains no OS-specific or hardware-specific code.

### 4.2.4. Scalability

- Up to **6 application contexts** can register independently (`MAXIMUM_SUPPORT_CONTEXT = 6`).
- Up to **12 simultaneous sockets** (`MAXIMUM_SUPPORT_SOCKET = 12`).
- Adding a new feature requires only defining a new `message_type_t` value and adding a handler in `process_message()`.

### 4.2.5. Safety

- The RXNE interrupt is **disabled** while the task processes a message, preventing a race condition between the ISR and the AT command reader.
- All AT command requests run on the same task, so there is never more than one AT command in flight at a time.
- The codebase supports formal verification with TrustInSoft Analyzer.

---

# 5. Features

## 5.1. Supported Modems

| Modem   | Status        |
|---------|---------------|
| BG95    | Fully supported |
| HL7900  | Planned (TBD) |

Each modem is implemented as a `modem_driver_t` struct that fills in function pointers for all modem-specific AT commands. The common and feature layers use only these function pointers, never calling modem-specific functions directly.

## 5.2. Connection Management

CMCELL manages the full lifecycle of a cellular data connection: from powering on the modem to obtaining an IP address.

### 2G/4G Connectivity

The application configures which Radio Access Technologies (RATs) to use and in what order. Supported RATs:
- **GSM** (2G)
- **CAT-M1** (4G LTE for IoT)
- **NB-IoT** (4G narrowband IoT)
- **LTE** (standard 4G mobile broadband)

The modem searches for networks in the configured RAT sequence. The first RAT with network coverage is used.

### 2G Fallback

If the application configures a RAT sequence that includes both 4G and 2G (e.g., `CAT-M1 → GSM`), the modem automatically tries each RAT in order. If 4G is unavailable, it falls back to 2G without any action from the application. A `CELL_MODEM_EVENT_NETWORK_REGISTERED` event is sent when the modem registers on any network.

## 5.3. AT Command Retry Mechanism

Each AT command sent by CMCELL has a configured timeout. If a command times out or returns an error, the feature module that sent it handles the retry. For example, the connection state machine can re-enter a step (such as PDN activation) after a failure, up to a configured number of attempts, before giving up and reporting an error event to the application.

## 5.4. Error Handling

CMCELL distinguishes three levels of errors:

1. **API errors** (synchronous): Returned immediately by the API call as a `cell_err_t` value. These indicate programming errors such as passing a NULL pointer or calling the API before `cell_init()`.

2. **Operation errors** (asynchronous): The operation was submitted successfully but failed during execution (e.g., modem timeout, SIM not ready). These are reported to the application via the registered callbacks.

3. **Modem errors (CME/CMS)**: Errors returned by the modem in response to AT commands. CMCELL maps these to internal error types (`cme_error_t`, `cms_error_t`) and uses them to decide the next action (e.g., retry, skip, or report failure).

## 5.5. URC Handling

URCs (Unsolicited Result Codes) are messages the modem sends on its own, without being asked — for example, when the network registration status changes or when a socket receives data.

**How URCs are handled:**

1. The modem sends a URC string over UART.
2. The UART interrupt fires (RXNE), and the ISR posts a `MESSAGE_DATA_INCOMING` message to the task queue.
3. `cell_task` reads the message and calls `process_remaining_data()`, which reads lines from the UART buffer.
4. Each line is checked against a URC token table. If it matches, `urc_parse_unsolicited_response()` dispatches it to the registered handler function.
5. The handler may trigger an internal state change (e.g., network registration update for the connection module) or notify the application via a callback.

**Key URC categories:**

| Category | Examples | Used by |
|----------|----------|---------|
| Network registration | `+CREG`, `+CGREG`, `+CEREG` | Connection module |
| PDN events | `+CGEV` | Connection module |
| Socket events | `+QIOPEN`, `+QIURC` | Socket module |
| DFOTA events | `+QIND: "FOTA"` | DFOTA module |
| Power/SIM | `RDY`, `+CPIN`, `+SIMSTAT` | Connection module |
| Signal quality | `+QIND: "csq"` | Application (via URC callback) |

## 5.6. Power Management (TBD)

PSM (Power Saving Mode) and eDRX configuration are exposed through the connection settings and can also be queried via `cell_psm_settings_get` and `cell_edrx_settings_get`. Full power management lifecycle (entering/exiting PSM) is planned for a future version.

## 5.7. Firmware Over-The-Air (FOTA)

CMCELL supports two firmware update methods:

**Local FOTA** — The MCU (host processor) holds the new firmware and sends it to the modem block by block:
1. `cell_dfota_start()` — initializes the DFOTA session and prepares the modem to receive firmware.
2. `cell_dfota_write()` — sends one block of firmware data (repeated until all blocks are sent).
3. `cell_dfota_activate()` — tells the modem to apply the new firmware and reboot.
4. `cell_dfota_abort()` — cancels the session at any point.

**Wireless FOTA** — The modem downloads the firmware directly from a remote server (HTTP, CoAP, or FTP):
1. `cell_dfota_start()` with a URL — the modem starts downloading from that URL.
2. The application receives progress events via the DFOTA callback.
3. `cell_dfota_activate()` — triggers the update after the download completes.

DFOTA states: `IDLE → INITIALIZING → READY → TRANSFERRING → SUCCESS / FAILED`

## 5.8. Modem Power-Up Management

The modem is powered and controlled through hardware pins managed by the `com/` layer:
- **Power pin**: `cell_com_set_power_pin()` — turns the modem on or off.
- **Reset pin**: `cell_com_set_reset_pin()` — resets the modem hardware.

The connection module also supports two software reset modes:
- **Soft reset** (`CELL_MODEM_RESET_SOFT`): Sends an AT command to reset the modem.
- **Hard reset** (`CELL_MODEM_RESET_HARD`): Restarts the full connection procedure from the beginning.

When a reset occurs, all registered contexts receive the `CELL_MODEM_EVENT_HW_RESET` or `CELL_MODEM_EVENT_SW_RESET` event.

## 5.9. Connection Rejection Management

When the network refuses to register the modem (e.g., due to a network policy), CMCELL captures the rejection cause and makes it available to the application:
- `cell_reject_cause_get()` returns the rejection cause, which includes:
  - **EMM cause code** (standard 3GPP cause, e.g., "PLMN not allowed", "Illegal UE").
  - **Manufacturer-specific cause** (modem-specific additional information).
  - **Rejected PLMN** (which operator rejected the connection).

The application can use this information to switch SIM cards, change operators, or alert the user.

---

# 6. Architecture Design

## 6.1. Software Architecture

The software is organized into three directories, each representing one layer.

![Software Architecture](/Imgs/sofware_architecture.svg)

```
cellular/          ← Feature and API layer
  inc/
    cell_api.h         Public API (used by application)
    cell_types.h       All public data types
    cell_connect.h     Connection feature (internal)
    cell_monitoring.h  Monitoring feature (internal)
    cell_dfota.h       DFOTA feature (internal)
    cell_socket.h      Socket feature (internal)
    cell_internal.h    Message queue types and helpers (internal)
    cell_task.h        Main RTOS task (internal)
    cell_os.h          OS abstraction (weak functions — app must implement)
    cell_log.h         Logging (internal)
  src/
    cell_api.c         API entry points → posts messages to queue
    cell_task.c        Main loop: reads queue, dispatches to features
    cell_connect.c     Connection state machine
    cell_monitoring.c  Monitoring request handlers
    cell_dfota.c       DFOTA state machine
    cell_socket.c      Socket management
    cell_os.c          OS abstraction wrappers
    cell_log.c         Logging implementation

modem/             ← AT command and modem-specific layer
  inc/
    cell_at.h          AT command engine (send command, read response)
    cell_urc.h         URC type definitions and dispatch
    cell_modem_common.h  Modem driver interface (modem_driver_t)
    cell_modem_bg95.h    BG95-specific implementation declarations
  src/
    cell_at.c          AT engine implementation
    cell_urc.c         URC parsing and dispatch
    cell_modem_common.c  Common AT commands, driver selection
    cell_modem_bg95.c    BG95 driver implementation

com/               ← UART communication layer
  inc/
    cell_com.h             Communication interface (buffer read/write, IRQ)
    cell_driver_interface.h  Hardware driver interface (weak — app must implement)
  src/
    cell_com.c             Ring buffer management, UART control
    cell_driver_interface.c  Weak stubs for hardware driver functions
```

## 6.2. Data Flow

This section describes what happens from the moment the application calls an API to the moment it receives a callback result.

**Outgoing request (application → modem):**

```
Application calls cell_xyz()
       │
       ▼
cell_api.c validates parameters
       │
       ▼
send_msg_with_value() / send_msg_with_data()
  → writes message_t to the task queue
       │
       ▼ (task wakes up)
cell_task() reads message from queue
       │
       ▼
process_message() dispatches by msg.type
       │
       ▼
Feature processor (e.g., monitoring_process)
  → calls AT command functions (modem layer)
  → parses response
  → calls application callback (e.g., get_cb)
```

**Incoming URC (modem → application):**

```
Modem sends URC string over UART
       │
       ▼
UART RXNE interrupt fires
       │
       ▼
cell_com_notify_task_on_rxne() (ISR context)
  → posts MESSAGE_DATA_INCOMING to queue
       │
       ▼
cell_task() reads message
       │
       ▼
read_and_parse_urc()
  → at_read_line() reads from UART buffer
  → urc_parse_unsolicited_response() identifies URC type
  → calls registered urc_handling_fn_t handler
       │
       ▼
Handler updates internal state or triggers callback
  (e.g., connection_notify_urc → connection state machine step)
```

## 6.3. CMCELL APIs

The public API is declared in `cell_api.h`. All functions take a `cell_context_t *` as their first parameter (except `cell_init`, `cell_deinit`, `cell_log_config`).

![CMCELL API Overview](/Imgs/cmcell_api_overview.drawio.svg)

| API Group       | Functions |
|-----------------|-----------|
| **Setup**       | `cell_init`, `cell_deinit`, `cell_context_create`, `cell_connect`, `cell_tcp_config` |
| **Network Config** | `cell_PDN_config`, `cell_DNS_config`, `cell_ping_answer_config` |
| **Monitoring**  | `cell_modem_info_get`, `cell_simcard_info_get`, `cell_cellular_info_get`, `cell_network_info_get`, `cell_service_status_get`, `cell_reject_cause_get` |
| **Power Saving**| `cell_psm_config`, `cell_psm_settings_get`, `cell_edrx_config`, `cell_edrx_settings_get` |
| **Time**        | `cell_time_sync`, `cell_time_get` |
| **DFOTA**       | `cell_dfota_start`, `cell_dfota_write`, `cell_dfota_activate`, `cell_dfota_abort` |
| **Socket**      | `cell_socket_create`, `cell_socket_send`, `cell_socket_receive`, `cell_socket_close`, `cell_socket_verify_data` |
| **SIM**         | `cell_simcard_info_get`, `cell_simcard_swap_start`, `cell_simcard_swap_abort` |
| **Logging**     | `cell_log_config` |
| **Test**        | `cell_test` |

**API call lifecycle:**

Most APIs are **asynchronous**:
- The call posts a message to the internal queue and returns `CELL_SUCCESS` immediately if the message was queued.
- The result arrives later via the callback registered in `cell_context_create()`.

## 6.4. CMCELL Project Structure

![Module Structure](/Imgs/module_structure.svg)

## 6.5. Component Descriptions

### 6.5.1. Connection

The connection component manages the entire sequence needed to get the modem online — from powering it up through to obtaining an IP address.

#### Connection State Machine

The top-level state machine has two main states: `DISCONNECTED` and `CONNECTED`. When the application calls `cell_connect()`, the machine begins the connection procedure. If the modem loses network registration, it returns to `DISCONNECTED` and attempts to reconnect automatically.

![Connection State Machine](/Imgs/Connection_state_machine.svg)

#### Connection Action State Machine

Inside the connection procedure, several action sub-states are executed in sequence:

![Connection Action State Machine](/Imgs/conection_action_state_machine.drawio.svg)

| Action | What it does |
|--------|-------------|
| **INIT** | Checks that the modem is alive (`AT`), turns off echo, enables CME error reporting, identifies the modem type (BG95), checks SIM readiness |
| **SETUP** | Applies modem-specific feature flags; enables URC reporting for network registration and PDN events |
| **QUERY CONFIG** | Reads the current modem configuration (RAT, PDN, operator) and compares with the requested settings |
| **CONFIG** | Applies only the settings that differ from what the modem already has — avoids unnecessary modem restarts |
| **NETWORK CONNECT** | Waits for network registration URC, then activates the PDN context to get an IP address |

#### Init Action

![Init Flowchart](/Imgs/init_flow_chart.drawio.svg)

#### Setup Action

![Setup Flowchart](/Imgs/connection_setup_flow_chart.drawio.svg)

#### Query Configuration

![Query Config Flowchart](/Imgs/connection_query_config_flow_chart.drawio.svg)

#### Config Action

![Config Flowchart](/Imgs/connection_configuration_flow_chart.drawio.svg)

#### Network Connection

![Network Connect Flowchart Part 1](/Imgs/connection_network_connect_flow_chart.drawio.svg)

![Network Connect Flowchart Part 2](/Imgs/connection_network_connect_flow_chart_part2.drawio.svg)

---

### 6.5.2. FOTA (Firmware Over-The-Air)

The DFOTA component manages modem firmware updates. It uses a state machine to track whether a DFOTA session is active and what phase it is in.

![DFOTA Overview](/Imgs/FOTA_overview_flowchart.drawio.svg)

**Local FOTA**

The MCU transfers the firmware to the modem block by block. This is used when the MCU holds the firmware image (e.g., downloaded from a cloud service separately).

- **Init**: The modem is prepared to receive a new firmware image. The total file size and block count are provided.

  ![Local FOTA Init](/Imgs/FOTA_local_initial_flowchart.drawio.svg)

- **Write**: Each block is sent one at a time. CMCELL writes the block to the modem's file system and tracks progress.

  ![Local FOTA Write](/Imgs/FOTA_local_write_flowchart.drawio.svg)

- **Activation**: After all blocks are written, the modem is instructed to apply the new firmware. The modem reboots and reports the result via URC.

  ![Local FOTA Activation](/Imgs/FOTA_local_activation_flowchart.drawio.svg)

**Wireless FOTA**

The modem downloads the firmware from a remote server directly (HTTP, CoAP, or FTP). CMCELL initiates the download and monitors progress via URCs.

![Wireless FOTA](/Imgs/FOTA_wireless_flowchart.drawio.svg)

---

### 6.5.3. Monitoring

The monitoring component handles all "get information" requests from the application. These requests are asynchronous: the application calls the API, and the result comes back via the `get_cb` callback registered in the context.

**Supported monitoring operations:**

| API | Data returned | Callback event |
|-----|--------------|----------------|
| `cell_modem_info_get` | IMEI, firmware version, manufacturer, model | `CELL_GET_EVENT_MODEM_INFO` |
| `cell_simcard_info_get` | ICCID, IMSI, PLMN | `CELL_GET_EVENT_SIMCARD_INFO` |
| `cell_cellular_info_get` | Serving cell RAT, TAC, Cell ID, signal (RSSI/RSRP/RSRQ/SINR), neighbor cells | `CELL_GET_EVENT_CELLULAR_INFO` |
| `cell_service_status_get` | CS/PS registration status, operator info | `CELL_GET_EVENT_SERVICE_STATUS` |
| `cell_network_info_get` | PDN context state, IPv4/IPv6 address | `CELL_GET_EVENT_NETWORK_INFO` |
| `cell_psm_settings_get` | PSM timers (T3312, T3314, T3412, T3324) | `CELL_GET_EVENT_PSM_SETTINGS` |
| `cell_reject_cause_get` | EMM cause code, manufacturer cause, rejected PLMN | `CELL_GET_EVENT_NETWORK_REJECTION_CAUSE` |
| `cell_time_get` | Current network time (year, month, day, hour, minute, second, timezone) | `CELL_GET_EVENT_TIME_INFO` |

Each operation sends a `MESSAGE_MONITORING` message to `cell_task`, which calls `monitoring_process()`. That function sends the appropriate AT command(s) to the modem, parses the response, and invokes the `get_cb` with the result.

> **Note:** Monitoring functions should only be called after the modem is connected to the network. If called while DFOTA is running, the request is ignored and the callback returns `false`.

---

### 6.5.4. Socket

The socket component provides TCP and UDP communication via the modem's built-in TCP/IP stack.

**Supported service types:**

| Service Type | Description |
|-------------|-------------|
| `TCP_CLIENT` | Connect to a remote TCP server |
| `UDP_CLIENT` | Send/receive UDP datagrams to/from a remote host |
| `TCP_SERVER` | Listen for incoming TCP connections |
| `UDP_SERVICE` | Receive UDP datagrams from any remote host |

**Key behaviors:**

- Up to **12 simultaneous sockets** are supported (`MAXIMUM_SUPPORT_SOCKET = 12`).
- CMCELL uses **buffer access mode**: when the modem receives data, it stores it in its internal buffer and sends a URC (`+QIURC: "recv"`) to notify CMCELL. The application then calls `cell_socket_receive()` to explicitly pull the data out.
- Socket events (connected, data received, closed, error) are delivered to the application via the `event_cb` callback registered in `cell_socket_settings_t`.
- `cell_socket_verify_data()` allows the application to compare total sent bytes with acknowledged bytes to verify reliable delivery (TCP only).

**Socket lifecycle:**

```
cell_socket_create() → [SOCKET_EVENT_CONNECTED] → cell_socket_send()
                                                 → cell_socket_receive() (after SOCKET_EVENT_DATA_RECEIVED)
                                                 → cell_socket_close()
```

---

### 6.5.5. Logging

CMCELL has an internal logging system with four levels, configurable at runtime via `cell_log_config()`:

| Level | Value | Description |
|-------|-------|-------------|
| `CELL_LOG_LEVEL_OFF` | 0 | No logging |
| `CELL_LOG_LEVEL_NORMAL` | 1 | Normal operation messages |
| `CELL_LOG_LEVEL_DEBUG` | 2 | Detailed debug information |
| `CELL_LOG_LEVEL_VERBOSE` | 3 | Very detailed trace output |

The application provides the actual log output function (e.g., printing to a UART console). CMCELL calls that function through the `cell_log.h` interface.

---

### 6.5.6. URC Handling

The URC handler acts as a central router for all modem-initiated messages. It uses a table of function pointers (`urc_map_handler_t`) indexed by `urc_type_t`. Each entry in the table points to the handler function for that specific URC.

**Dispatch flow:**

```
at_read_line() reads a raw line from UART buffer
       │
       ▼
urc_is_without_prefix() checks if it is a prefix-less URC (e.g., "RDY")
       │
urc_parse_unsolicited_response() matches the prefix to a urc_type_t
       │
       ▼
Calls urc_map_handler_t[urc_type].handler(urc_data)
       │
       ▼
Feature-specific handler:
  - Connection URCs  → connection_notify_urc()
  - Socket URCs      → socket_process_urc_*()
  - DFOTA URCs       → dfota_notify_urc()
  - Other URCs       → cell_urc_event_callback (application-facing)
```

Each modem driver (e.g., BG95) registers its own URC handler table via `urc_set_handler_table()`. This allows different modems to support different URCs without changing the dispatch logic.

---

### 6.5.7. AT Command Engine

The AT command engine (`cell_at.c`) is the lowest-level communication component in CMCELL. It is the only place where AT commands are sent to the modem and responses are read back.

**Key functions:**

| Function | Purpose |
|----------|---------|
| `at_command_request()` | Sends an AT command, waits for the final response within a timeout, calls a parser callback for intermediate lines |
| `at_read_line()` | Reads one line from the UART ring buffer, respecting a timeout |
| `is_line_final_resp()` | Checks if a line is `OK`, `ERROR`, `+CME ERROR`, etc. |
| `process_remaining_data()` | Reads and dispatches any leftover data in the UART buffer — called before each AT command and after processing URC messages |

**AT command flow:**

```
at_command_request(at_req, timeout_ms, parser_handler)
       │
       ▼
Sends at_req.p_send_buff over UART via cell_com_send_buffer()
       │
       ▼  (loop until final response or timeout)
at_read_line() — reads one line
       │
       ├── Is it a URC? → hand to process_remaining_data()
       │
       ├── Is it an intermediate line with at_req.resp_prefix?
       │       → call parser_handler() to extract data
       │
       └── Is it a final response (OK / ERROR)?
               → return AT_SUCCESS / AT_TIMEOUT / AT_ERROR_*
```

---

# 7. Other Dependencies

CMCELL requires the application to provide implementations for two abstraction layers:

## 7.1. OS Abstraction (`cell_os.h`)

All OS functions are declared as weak functions. The application must override them with a real RTOS implementation.

| Function | Purpose |
|----------|---------|
| `cell_os_msg_create()` | Create a message queue |
| `cell_os_msg_write()` | Write a message to the queue (normal context) |
| `cell_os_msg_write_from_isr()` | Write a message to the queue (from ISR) |
| `cell_os_msg_read()` | Read a message from the queue (blocking) |
| `cell_os_timer_create()` | Create a software timer |
| `cell_os_timer_start()` | Start a timer with a duration |
| `cell_os_timer_stop()` | Stop a running timer |
| `cell_os_sleep_ms()` | Delay for a given number of milliseconds |
| `cell_os_feed_wdg()` | Feed the hardware watchdog |
| `cell_os_get_time_ms()` | Return elapsed time in milliseconds since boot |

> `cell_task` runs as a single RTOS task with stack size `TASK_STACK_CMCELL = 350` words and priority `TASK_PRIORITY_CMCELL = 4`. The application is responsible for creating and starting this task.

## 7.2. UART Driver (`cell_driver_interface.h`)

The application must implement the hardware-specific UART functions. CMCELL uses these to send AT commands and receive data from the modem.

Key behaviors the driver must support:
- **DMA or interrupt-driven receive**: Incoming bytes are written to the circular buffer provided by `cell_init(buffer, size)`.
- **RXNE interrupt**: When data arrives, the driver calls `cell_com_notify_task_on_rxne()` (already implemented in CMCELL) to wake the task.
- **Power and reset pin control**: The driver implements the weak functions `cell_com_set_power_pin()` and `cell_com_set_reset_pin()`.

## 7.3. Logging Output

The application provides a log output backend (e.g., a printf over UART). CMCELL calls this through the log interface. The verbosity level is controlled at runtime using `cell_log_config()`.

---

# 8. API Design

## 8.1. API Overview

The public API is documented at:
[CMCELL API Reference](https://common-modules.pages.gitlab-produits.rmm.scom/cellular/cellular/cell__api_8h.html)

**Usage pattern:**

```c
/* 1. Allocate the DMA receive buffer */
char dma_buf[2048];

/* 2. Initialize CMCELL */
cell_init(dma_buf, sizeof(dma_buf));

/* 3. Create an application context with callbacks */
cell_context_settings_t ctx_settings = {
    .modem_cb = on_modem_event,
    .get_cb   = on_get_result,
    .dfota_cb = on_dfota_event,
};
cell_context_t ctx;
cell_context_create(&ctx_settings, &ctx);

/* 4. Start connection */
cell_settings_t settings = { /* fill in RAT, PDN, SIM, ... */ };
cell_connect(&ctx, &settings);

/* 5. Wait for CELL_MODEM_EVENT_PDP_CONTEXT_OPENED in on_modem_event */
/* 6. Use sockets, monitoring, DFOTA, etc. */
```

## 8.2. Error Handling

### Application Errors (Synchronous)

These are returned immediately by the API call:

| Code | Meaning |
|------|---------|
| `CELL_SUCCESS` | The request was accepted and queued successfully |
| `CELL_ERR_GENERIC` | The request was rejected — invalid parameter, wrong state, queue full, or feature busy |

When `CELL_ERR_GENERIC` is returned, the application should not expect any callback for that request.

### Operation Results (Asynchronous)

The result of each operation is delivered via the appropriate callback:

| Callback | Event type | Used for |
|----------|-----------|---------|
| `modem_cb` | `cell_modem_event_t` | Connection lifecycle events (connected, disconnected, reset, errors) |
| `get_cb` | `cell_get_event_t` + `bool result` | Monitoring operations (`cell_*_get`) |
| `dfota_cb` | `cell_dfota_event_t` | DFOTA progress and result |
| `test_cb` | `cell_test_event_t` | Test operations |
| `event_cb` (socket) | `cell_socket_event_t` | Socket events per socket |

### State Machine Recovery

| Modem Event | Meaning | Recommended Application Action |
|-------------|---------|-------------------------------|
| `CELL_MODEM_EVENT_INIT_FAILED` | Modem did not respond during init | Check hardware, retry `cell_connect()` |
| `CELL_MODEM_EVENT_SIMCARD_FAILED` | SIM not detected or PIN rejected | Check SIM insertion, check PIN |
| `CELL_MODEM_EVENT_NETWORK_REGISTRATION_FAILED` | Could not register on any network | Check antenna, check operator coverage |
| `CELL_MODEM_EVENT_PDP_CONTEXT_OPENED` | Successfully connected | Application may now use sockets, monitoring |
| `CELL_MODEM_EVENT_PDP_CONTEXT_DESTROYED` | Data connection was lost | Wait for automatic reconnection or call `cell_connect()` again |
| `CELL_MODEM_EVENT_HW_RESET` | Modem rebooted | CMCELL restarts the connection procedure automatically |
| `CELL_MODEM_EVENT_SW_RESET` | Connection procedure reset by application | CMCELL restarts the connection procedure |
| `CELL_MODEM_EVENT_SIGNAL_QUALITY_LOW` | Signal below threshold | Application may choose to alert or move device |

### Error Codes (DFOTA)

| Code | Value | Meaning |
|------|-------|---------|
| `CELL_DFOTA_ERR_UPDATE_SUCCESSFUL` | 0 | Success |
| `CELL_DFOTA_ERR_TIMEOUT` | 421 | Operation timed out |
| `CELL_DFOTA_ERR_MEMORY_ALLOCATE_FAILED` | 427 | Modem out of memory |
| `CELL_DFOTA_ERR_UPDATE_FAILED` | 504 | General update failure |
| `CELL_DFOTA_ERR_UPDATE_PACKAGE_NOT_EXIST` | 505 | Firmware file not found |
| `CELL_DFOTA_ERR_UPDATE_PACKAGE_CHECK_FAILED` | 506 | Firmware integrity check failed |
| `CELL_DFOTA_ERR_UPDATE_PACKAGE_MISMATCHED` | 511 | Firmware not compatible with current modem version |
| `CELL_DFOTA_ERR_INVALID_PARAM` | 590 | Invalid parameter |
| `CELL_DFOTA_ERR_ONGOING` | — | Another DFOTA session is already active |

---

# 9. Limitations

- Only one modem is supported per CMCELL instance.
- Only one CMCELL task exists in the system — features cannot run concurrently.
- The monitoring APIs (`cell_*_get`) should not be called during an active DFOTA session; requests will be ignored.
- Socket buffer access mode is the only supported mode — direct push mode is not supported.
- A TCP server socket accepts only one incoming connection at a time.
- HL7900 modem support is planned but not yet implemented.
- Full power management lifecycle (PSM wake-up, modem sleep tracking) is not yet implemented.

---

# 10. Future Improvements

- Add HL7900 modem driver.
- Implement full power management lifecycle (track PSM entry/exit via URC, coordinate with application).
- Add SIM swap support (dual SIM management).
- Support concurrent AT commands across multiple independent channels (requires architectural change to the single-task model).
- Add TLS/SSL socket support.

---

# 11. References

- [CMCELL Public API Documentation](https://common-modules.pages.gitlab-produits.rmm.scom/cellular/cellular/cell__api_8h.html)
- Quectel BG95 AT Commands Manual
- 3GPP TS 27.007 — AT Commands for User Equipment

---

# 12. Appendix

## A. Message Types

| Message Type | Value | Description | Processed by |
|-------------|-------|-------------|-------------|
| `MESSAGE_INVALID` | 0 | Not used | — |
| `MESSAGE_DATA_INCOMING` | 1 | New data in UART buffer (from ISR) | `read_and_parse_urc()` |
| `MESSAGE_CONTEXT_CREATION` | 2 | Create application context | `create_cellular_context()` |
| `MESSAGE_CONNECT` | 3 | Start/reset connection procedure | `connection_process()` |
| `MESSAGE_MONITORING` | 4 | Monitoring request (get info) | `monitoring_process()` |
| `MESSAGE_DFOTA` | 5 | DFOTA operation | `dfota_process()` |
| `MESSAGE_SOCKET` | 6 | Socket operation | `socket_process()` |

## B. Key Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `MAXIMUM_SUPPORT_CONTEXT` | 6 | Maximum number of application contexts |
| `MAXIMUM_SUPPORT_SOCKET` | 12 | Maximum number of simultaneous sockets |
| `CELL_TASK_MSG_QUEUE_LEN` | 32 | Maximum messages in the task queue |
| `TASK_STACK_CMCELL` | 350 | Task stack size in words |
| `TASK_PRIORITY_CMCELL` | 4 | Task RTOS priority |
| `AT_MAX_LINE_LEN` | 128 | Maximum length of one AT response line (bytes) |
| `CELL_NUMBER_OF_TIMERS` | 3 | Number of software timers used by CMCELL |
