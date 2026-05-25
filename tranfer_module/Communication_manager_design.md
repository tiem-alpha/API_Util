# Communication Manager Design

---

# 1. Overview

Communication Manager is a middleware layer between:
- Application layer
- Transport layer
- Protocol layer

The module is responsible for:
- managing TX/RX queues
- sending data
- receiving data
- protocol parsing
- protocol packet building
- event callbacks to the application
- TX busy state management

The module is hardware-independent.

---

# 2. Scope and Constraints

## In Scope
- TX/RX queue management
- Protocol pack / parse orchestration
- Event notification to application

## Out of Scope
```
Does NOT manage timeouts.
Does NOT manage retries.
Does NOT implement flow control.
Does NOT allocate memory dynamically.
```

---

# 3. Design Goals

- Hardware independent
- Protocol independent
- Reusable
- Scalable
- Event-driven
- Non-blocking
- RTOS/Baremetal compatible
- Stream-based architecture

---

# 4. Communication Flow

## TX Flow

```text
APP
 ↓
comm_send()
 ↓
protocol.pack()
 ↓
push TX queue
 ↓
comm_process()
 ↓
transport.send()  →  transport ISR/DMA calls comm_send_done_handler()  →  COMM_EVENT_TX_DONE  →  app callback
```

## RX Flow

```text
Transport RX IRQ/DMA
 ↓
comm_push_rx()       ← ISR-safe entry point
 ↓
push RX queue
 ↓
comm_process()
 ↓
protocol.feed(byte)
 ↓
packet complete
 ↓
COMM_EVENT_RX_DONE  →  APP callback
```

---

# 5. Queue Design

The queue uses a Ring Buffer / Circular Buffer structure.

The queue only manages byte streams.

## Queue Structure

```c
typedef struct
{
    uint8_t  *buffer;
    uint16_t  size;
    uint16_t  head;
    uint16_t  tail;
    bool      overflow;
} comm_queue_t;
```

## Queue Responsibilities

- Buffer TX streams
- Buffer RX streams
- Circular buffer management
- Overflow detection

---

# 6. Transport Layer Design

The Transport Layer is an abstraction layer for hardware communication.

The transport layer only handles raw bytes.

## Transport Responsibilities

- Send raw bytes
- Receive raw bytes
- Notify Communication Manager by directly calling `comm_on_receive()` and `comm_send_done_handler()`
- No protocol or queue logic

## Transport MUST NOT

- Parse protocol
- Validate CRC
- Manage queues
- Retry packets
- Manage application logic

---

# 7. Queue API

```c
/* Bind a backing buffer to a queue. Must be called before any queue use. */
void comm_queue_init(comm_queue_t *q, uint8_t *buffer, uint16_t size);

/* Push bytes into the queue. Returns number of bytes written (may be less than len on overflow). */
uint16_t comm_queue_push(comm_queue_t *q, const uint8_t *data, uint16_t len);

/* Pop one byte. Returns true if a byte was available. */
bool comm_queue_pop(comm_queue_t *q, uint8_t *byte_out);

/* Number of bytes currently in the queue. */
uint16_t comm_queue_count(const comm_queue_t *q);
```

Buffer ownership: the caller allocates the backing buffer. The queue never allocates memory.

---

# 8. ISR / Thread Safety Contracts

| Function | Context | Notes |
|---|---|---|
| `comm_on_receive()` | ISR context | Pushes raw RX bytes into RX queue via `comm_push_rx()` |
| `comm_send_done_handler()` | ISR context | Clears `tx_busy` and fires `COMM_EVENT_TX_DONE` |
| `comm_push_rx()` | ISR-safe | Only advances `head`; use `volatile` + memory barrier |
| `comm_process()` | Non-ISR only | Must be called from a single task or main loop |
| `comm_send()` | Non-ISR only | Calls `protocol.pack()` and queue push |

`tx_busy` must be declared `volatile` to prevent compiler reordering:

```c
volatile bool tx_busy;
```

---

# 9. Transport Interface

```c
typedef struct
{
    comm_status_t (*send)(const uint8_t *data, uint16_t len);
    void         *parent_context; /* Optional pointer for transport internal use. */
} comm_transport_t;
```

The transport implementation is responsible for directly invoking the comm manager's ISR entry points:
- `comm_on_receive(comm, data, len)` when raw bytes arrive
- `comm_send_done_handler(comm)` when a TX transfer completes

The transport struct itself does not carry receive/send-done callbacks.

---

# 10. Protocol Design

The Protocol Layer is responsible for:

- packet framing
- packet parsing
- CRC validation
- packet generation

## Protocol MUST NOT

- Access hardware
- Send data
- Manage queues

---

# 11. Packet Format

```text
+--------+--------+----------+------+--------+
| START  | LENGTH | PAYLOAD  | CRC  | STOP   |
+--------+--------+----------+------+--------+
| 0xAC   | 2B     | N bytes  | 2B   | 0xBB   |
+--------+--------+----------+------+--------+
```

## Field Definitions

- **START**: fixed framing byte `0xAC`
- **LENGTH**: 2 bytes, little-endian. Counts PAYLOAD bytes only (excludes START, LENGTH, CRC, STOP). Recommended max payload: 512 bytes.
- **PAYLOAD**: raw application data
- **CRC**: CRC-16/MCRF4XX — polynomial `0x1021`, init `0xFFFF`, no final XOR. Computed over LENGTH + PAYLOAD bytes.
- **STOP**: fixed framing byte `0xBB`

## Framing Byte Constraint

If payload data may contain `0xAC` or `0xBB`, the application layer must encode it before passing to `comm_send()`. The protocol layer does NOT implement byte stuffing.

---

# 12. Protocol State Machine

```c
typedef enum
{
    PROTOCOL_STATE_WAIT_START,
    PROTOCOL_STATE_READ_LENGTH,
    PROTOCOL_STATE_READ_PAYLOAD,
    PROTOCOL_STATE_READ_CRC,
    PROTOCOL_STATE_READ_STOP,
} protocol_state_t;
```

---

# 13. Protocol Event

```c
typedef enum
{
    PROTOCOL_EVENT_PACKET_SUCCESS,
    PROTOCOL_EVENT_CRC_ERROR,
    PROTOCOL_EVENT_FORMAT_ERROR,
    PROTOCOL_EVENT_OVERFLOW,
} protocol_event_t;
```

---

# 14. Protocol Callbacks

Protocol callbacks notify the Communication Manager of parse/pack outcomes. The comm manager translates these into `comm_event_t` events for the application.

All four callbacks are registered by `comm_init` (not by the application). The protocol calls them; the comm manager acts on them.

| Callback | Trigger | Comm Manager action |
|---|---|---|
| `on_parser_done` | Full valid packet received | Fire `COMM_EVENT_RX_DONE` with payload |
| `on_parser_fail` | CRC error, format error, or overflow | Fire `COMM_EVENT_PROTOCOL_ERROR` |
| `on_pack_done` | Frame successfully packed | Push packed bytes (length-prefixed) to TX queue |
| `on_pack_fail` | Output buffer too small | Set `_pack_status = COMM_ERR_INVALID_ARG`, fire `COMM_EVENT_TX_FAIL` |

---

# 15. Protocol Interface

```c
typedef struct
{
    /* Pack input bytes into a framed packet.
       Returns bytes written to output, or negative on error. */
    int32_t (*pack)(comm_protocol_t *ctx, uint8_t *output, uint16_t output_size,
                    const uint8_t *input, uint16_t input_len);

    /* Feed one byte into the parser state machine.
       Returns bytes written to output on frame complete, 0 if in progress, negative on error. */
    int32_t (*feed)(comm_protocol_t *ctx, uint8_t *output, uint16_t output_size, uint8_t byte);

    /* Reset parser state to WAIT_START. Must be called by comm_init(). */
    void (*reset)(comm_protocol_t *ctx);

    void (*on_parser_fail)(comm_protocol_t *ctx, uint8_t *output, uint16_t output_size,
                           protocol_event_t event);
    void (*on_parser_done)(comm_protocol_t *ctx, uint8_t *output, uint16_t output_size);
    void (*on_pack_fail)(comm_protocol_t *ctx, uint8_t *output, uint16_t output_size,
                         protocol_event_t event);
    void (*on_pack_done)(comm_protocol_t *ctx, uint8_t *output, uint16_t output_size);

    protocol_state_t state;       /* current parser state */
    uint16_t         unpack_crc;
    uint16_t         unpack_offset;
    uint16_t         unpack_length;
} comm_protocol_t;
```

Output buffer ownership: the caller (Communication Manager) provides `output` and `output_size`. The protocol layer never allocates memory.

---

# 16. Status Codes

```c
typedef enum
{
    COMM_OK = 0,
    COMM_ERR_BUSY,
    COMM_ERR_QUEUE_FULL,
    COMM_ERR_INVALID_ARG,
} comm_status_t;
```

---

# 17. Communication Event

```c
typedef enum
{
    COMM_EVENT_RX_DONE,
    COMM_EVENT_TX_DONE,
    COMM_EVENT_TX_FAIL,
    COMM_EVENT_TX_BUSY,
    COMM_EVENT_PROTOCOL_ERROR,
    COMM_EVENT_QUEUE_OVERFLOW,
    COMM_EVENT_QUEUE_FULL,
} comm_event_t;
```

---

# 18. Communication Callback

```c
typedef void (*comm_event_cb_t)(
    comm_event_t    event,
    const uint8_t  *data,
    uint16_t        len);
```

A single callback handles all events. `data` and `len` are valid only for `COMM_EVENT_RX_DONE` and `COMM_EVENT_TX_DONE`; pass `NULL` / `0` for all other events.

---

# 19. Communication Manager Structure

```c
typedef struct
{
    comm_transport_t *transport;
    comm_protocol_t  *protocol;
    comm_queue_t      tx_queue;
    comm_queue_t      rx_queue;
    comm_event_cb_t   event_cb;
    volatile bool     tx_busy;

    /* Return channel written by on_pack_done / on_pack_fail before comm_send returns. */
    comm_status_t _pack_status;

    /* Internal scratch buffers — do not access directly. */
    uint8_t pack_buf[PROTOCOL_MAX_PAYLOAD + PROTOCOL_FRAME_OVERHEAD];
    uint8_t rx_buf[PROTOCOL_MAX_PAYLOAD];
} communication_manager_t;
```

---

# 20. Communication APIs

## Init

```c
void comm_init(
    communication_manager_t *comm,
    comm_transport_t         *transport,
    comm_protocol_t          *protocol,
    comm_event_cb_t           event_cb,
    uint8_t *tx_buf, uint16_t tx_size,
    uint8_t *rx_buf, uint16_t rx_size);
```

`comm_init` must:
1. Call `comm_queue_init()` for both TX and RX queues using the provided buffers
2. Set `transport->parent_context = comm` if the transport needs comm context
3. Register all four protocol callbacks (`on_pack_done`, `on_pack_fail`, `on_parser_done`, `on_parser_fail`) into the protocol struct, setting `protocol->user_data = comm`
4. Call `protocol->reset()` to initialize parser state

## Send Data

```c
comm_status_t comm_send(
    communication_manager_t *comm,
    const uint8_t           *data,
    uint16_t                 len);
```

Returns `COMM_ERR_BUSY` if `tx_busy`, `COMM_ERR_QUEUE_FULL` if TX queue is full.

## Push RX Data

```c
void comm_push_rx(
    communication_manager_t *comm,
    const uint8_t           *data,
    uint16_t                 len);
```

ISR-safe. Called from the transport RX interrupt or DMA callback to push raw bytes into the RX queue.

## Process

```c
void comm_process(communication_manager_t *comm);
```

Must be called from the main loop or an RTOS task (non-ISR context).

---

# 21. Process Logic

## TX Processing

```text
IF NOT tx_busy
    IF tx queue has data
        pop frame bytes from tx queue
        set tx_busy = true
        transport.send()
```

## RX Processing

```text
WHILE rx queue has data
    pop byte
    protocol.feed(byte)
```

---

# 22. Design Characteristics

- hardware independent
- protocol independent
- stream-based
- reusable
- scalable
- low RAM usage
- non-blocking
- DMA-friendly
- RTOS compatible
- baremetal compatible

---

# 23. Suggested File Structure

```text
communication/
│
├── communication_manager.c
├── communication_manager.h
│
├── queue/
│   ├── comm_queue.c
│   └── comm_queue.h
│
├── protocol/
│   ├── protocol.c
│   └── protocol.h
│
├── transport/
│   ├── transport_uart.c
│   ├── transport_uart.h
│   ├── transport_ble.c
│   └── transport_tcp.c
│
└── utils/
    ├── crc.c
    └── crc.h
```

---

# 24. Final Notes

The Communication Manager is responsible only for orchestration.

## Transport
Only sends/receives raw bytes.

## Protocol
Only packs frames and parses byte streams.

## Queue
Only buffers byte streams.

## Application
Handles business logic.
