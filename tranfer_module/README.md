# Communication Manager

Hardware-independent, protocol-independent middleware for embedded C.  
Sits between the application, transport (UART / BLE / TCP), and a packet protocol.

---

## Architecture

```
Application
    │  comm_send() / event_cb
    ▼
Communication Manager          ← orchestration only
    ├── TX queue  ──────────►  Transport (UART / BLE / TCP)
    └── RX queue  ◄──────────  Transport ISR / DMA
            │
            ▼
        Protocol (pack / feed byte-by-byte)
            │
            ▼
    Packet: [0xAC][LEN 2B][PAYLOAD N B][CRC-16 2B][0xBB]
```

Three layers with strict separation:

| Layer | Responsibility |
|---|---|
| **Queue** | Circular byte-stream buffer |
| **Transport** | Raw byte TX/RX, hardware callbacks |
| **Protocol** | Frame packing, byte-by-byte parsing, CRC-16/MCRF4XX |

The manager does **not** handle timeouts, retries, or flow control — those belong to the application.

---

## File Structure

```
communication/
├── communication_manager.h / .c   ← main API
├── queue/comm_queue.h / .c
├── protocol/protocol.h / .c
├── transport/transport_uart.h / .c
└── utils/crc.h / .c

tests/
├── utest.h / utest.c              ← minimal test framework
├── test_crc.c
├── test_queue.c
├── test_protocol.c
├── test_comm_manager.c
├── test_runner.c
└── Makefile

example/
└── example_loopback.c             ← self-contained runnable demo
```

---

## Getting Started

### 1 — Copy the `communication/` directory into your project

### 2 — Implement your transport

```c
// Provide a send function that writes to hardware
static comm_status_t my_uart_send(const uint8_t *data, uint16_t len)
{
    // e.g. HAL_UART_Transmit_DMA(&huart1, data, len);
    return COMM_OK;
}

comm_transport_t transport = {
    .send = my_uart_send,
    /* comm_init does not register transport callbacks; the transport
       should invoke comm_on_receive() and comm_send_done_handler()
       directly from ISR or DMA callbacks. */
};
```

### 3 — Allocate static buffers and initialize

```c
static uint8_t           tx_buf[1024];
static uint8_t           rx_buf[1024];
static communication_manager_t comm;
static comm_protocol_t         proto;

void system_init(void)
{
    protocol_init(&proto);

    /* comm_init calls comm_queue_init internally — no separate queue setup needed. */
    comm_init(&comm, &transport, &proto, app_event_cb,
              tx_buf, sizeof(tx_buf),
              rx_buf,  sizeof(rx_buf));
}
```

### 4 — Wire your hardware ISRs

```c
/* UART RX ISR / DMA callback — transport forwards bytes into the comm manager. */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    comm_on_receive(&comm, rx_dma_buf, rx_dma_len);
}

/* UART TX complete ISR / DMA callback */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    comm_send_done_handler(&comm);
}
```

### 5 — Call `comm_process()` from your main loop

```c
while (1)
{
    comm_process(&comm);   // drives TX and RX
    // ... other tasks
}
```

### 6 — Send data and handle events

```c
void app_event_cb(comm_event_t event, const uint8_t *data, uint16_t len)
{
    switch (event) {
        case COMM_EVENT_RX_DONE:
            // data[0..len-1] is the decoded payload
            break;
        case COMM_EVENT_TX_DONE:
            break;
        case COMM_EVENT_PROTOCOL_ERROR:
            break;
        default: break;
    }
}

// Anywhere in your application:
const uint8_t payload[] = {0x01, 0x02, 0x03};
comm_send(&comm, payload, sizeof(payload));
```

---

## API Reference

| Function | Description |
|---|---|
| `comm_init(comm, transport, protocol, event_cb, tx_buf, tx_size, rx_buf, rx_size)` | Initialize; inits queues, sets transport parent context, registers protocol callbacks |
| `comm_send(comm, data, len)` | Pack and enqueue a frame for TX |
| `comm_on_receive(comm, data, len)` | ISR entry point for raw RX bytes |
| `comm_send_done_handler(comm)` | ISR entry point for TX completion |
| `comm_process(comm)` | Drive TX and RX; call from main loop / RTOS task |

### Return codes (`comm_status_t`)

| Code | Meaning |
|---|---|
| `COMM_OK` | Success |
| `COMM_ERR_QUEUE_FULL` | TX queue has no space for the packed frame |
| `COMM_ERR_INVALID_ARG` | NULL pointer or zero length |
| `COMM_ERR_BUSY` | Reserved |

### Events (`comm_event_t`)

| Event | When fired | `data` / `len` valid? |
|---|---|---|
| `COMM_EVENT_RX_DONE` | Complete valid packet decoded | Yes — payload |
| `COMM_EVENT_TX_DONE` | Transport TX transfer complete | No |
| `COMM_EVENT_TX_FAIL` | Transport or pack error | No |
| `COMM_EVENT_PROTOCOL_ERROR` | CRC / format error in received frame | No |
| `COMM_EVENT_QUEUE_FULL` | TX queue full on `comm_send` | No |
| `COMM_EVENT_QUEUE_OVERFLOW` | RX queue overflow on `comm_push_rx` | No |

---

## Packet Format

```
+--------+----------+----------+------+--------+
| START  |  LENGTH  | PAYLOAD  | CRC  |  STOP  |
+--------+----------+----------+------+--------+
| 0xAC   |  2 bytes |  N bytes |  2B  | 0xBB   |
+--------+----------+----------+------+--------+
```

- **LENGTH**: little-endian, payload bytes only (max 512)
- **CRC**: CRC-16/MCRF4XX over LENGTH + PAYLOAD (poly=0x1021, init=0xFFFF, RefIn/RefOut=true)
- If payload may contain `0xAC` or `0xBB`, encode it at the application layer before calling `comm_send`

---

## Building and Running Tests

Requires GCC (MinGW on Windows, or any POSIX toolchain).

```bash
cd tests
make run
```

Expected output:

```
[CRC-16/MCRF4XX]
  test_crc_check_value                    OK
  ...

[Queue]
  ...

[Protocol]
  ...

[Communication Manager]
  ...

=== 35 passed, 0 failed ===
```

### Run a single test file (example)

```bash
gcc -Wall -std=c99 -I.. \
    ../communication/utils/crc.c \
    utest.c test_crc.c \
    -DTEST_STANDALONE -o test_crc && ./test_crc
```

---

## Running the Loopback Example

```bash
cd example
gcc -Wall -std=c99 -I.. \
    ../communication/utils/crc.c \
    ../communication/queue/comm_queue.c \
    ../communication/protocol/protocol.c \
    ../communication/communication_manager.c \
    example_loopback.c -o loopback

./loopback
```

---

## ISR / Thread Safety Notes

| Function | Context |
|---|---|
| `comm_on_receive()` | Called from ISR — pushes raw RX bytes into the RX queue |
| `comm_send_done_handler()` | Called from ISR — clears `tx_busy` and fires `COMM_EVENT_TX_DONE` |
| `comm_push_rx()` | ISR-safe — only advances queue `head` |
| `comm_process()` | Non-ISR only — single task/main loop |
| `comm_send()` | Non-ISR only |

`tx_busy` is declared `volatile bool` to prevent compiler reordering across ISR boundaries.
