# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an embedded C project implementing a **Communication Manager** — a middleware layer between application, transport, and protocol layers. The implementation does not yet exist; `Communication_manager_design.md` is the authoritative design specification.

## Architecture

Three strictly separated layers, each with a single responsibility:

| Layer | Responsibility | Must NOT |
|---|---|---|
| **Queue** (`comm_queue_t`) | Circular buffer for byte streams | Parse, send, retry |
| **Transport** (`comm_transport_t`) | Send/receive raw bytes, fire callbacks | Parse protocol, manage queues |
| **Protocol** (`comm_protocol_t`) | Pack frames, feed/parse bytes, CRC validation | Access hardware, send data |

The **Communication Manager** (`communication_manager_t`) owns instances of all three and orchestrates them. It does NOT manage timeouts or retries — those belong to the application.

## Key Design Constraints

- Hardware-independent, protocol-independent
- Non-blocking; `comm_process()` is the single poll-based driver called from a main loop or RTOS task
- TX flow: `comm_send()` → `protocol.pack()` → push TX queue → `comm_process()` → `transport.send()` → send-done callback → app callback
- RX flow: transport IRQ/DMA → `comm_push_rx()` → `comm_process()` → `protocol.feed(byte)` per byte → packet-complete callback → app callback
- `tx_busy` flag prevents concurrent sends; the transport calls the send-done callback to clear it

## Packet Format

```
| START (0xAC) | LENGTH (2B) | PAYLOAD (N bytes) | CRC-16/MCRF4XX (2B) | STOP (0xBB) |
```

Protocol parser is a state machine: `WAIT_START → READ_LENGTH → READ_PAYLOAD → READ_CRC → READ_STOP`.

## Intended File Structure

```
communication/
├── communication_manager.c / .h
├── queue/comm_queue.c / .h
├── protocol/protocol.c / .h
├── transport/transport_uart.c, transport_ble.c, transport_tcp.c
└── utils/crc.c / .h
```

## Core API

```c
void comm_init(communication_manager_t *comm, comm_transport_t *transport,
               comm_protocol_t *protocol, comm_event_cb_t event_cb);
int  comm_send(communication_manager_t *comm, const uint8_t *data, uint16_t len);
void comm_push_rx(communication_manager_t *comm, const uint8_t *data, uint16_t len);
void comm_process(communication_manager_t *comm);   // call from main loop / RTOS task
```

## Events

`comm_event_t`: `RX_DONE`, `TX_DONE`, `TX_FAIL`, `TX_BUSY`, `PROTOCOL_ERROR`, `QUEUE_OVERFLOW`, `QUEUE_FULL`

`protocol_event_t`: `PACKET_SUCCESS`, `CRC_ERROR`, `FORMAT_ERROR`, `OVERFLOW`
