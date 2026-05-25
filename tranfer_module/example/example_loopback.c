

#include <stdio.h>
#include <string.h>
#include "../communication/communication_manager.h"

/* ---- Backing buffers (static; no heap required) ---- */
static uint8_t tx_queue_buf[1024];
static uint8_t rx_queue_buf[1024];

/* Captured frame for loopback injection. */
static uint8_t  loopback_frame[PROTOCOL_MAX_PAYLOAD + PROTOCOL_FRAME_OVERHEAD];
static uint16_t loopback_frame_len;

static comm_transport_t transport;
static comm_protocol_t         proto;
static communication_manager_t comm;


static comm_status_t loopback_send(const uint8_t *data, uint16_t len)
{
    memcpy(loopback_frame, data, len);
    loopback_frame_len = len;

    printf("[transport] TX %u bytes: ", len);
    for (uint16_t i = 0; i < len; i++) printf("%02X ", data[i]);
    printf("\n");
    return COMM_OK;
}

void transport_uart_on_receive(communication_manager_t *comm, uint8_t *data, uint16_t len)
{
    comm_on_receive(comm, data, len);
}

void transport_uart_on_send_done()
{
    comm_send_done_handler(&comm);
}


/* ---- Application event callback ---- */

static void app_event(comm_event_t event, const uint8_t *data, uint16_t len)
{
    switch (event) {
        case COMM_EVENT_TX_DONE:
            printf("[app] TX_DONE\n");
            break;
        case COMM_EVENT_RX_DONE:
            printf("[app] RX_DONE %u bytes: ", len);
            for (uint16_t i = 0; i < len; i++) printf("%02X ", data[i]);
            printf("  (\"%.*s\")\n", (int)len, (const char *)data);
            break;
        case COMM_EVENT_PROTOCOL_ERROR:
            printf("[app] PROTOCOL_ERROR\n");
            break;
        case COMM_EVENT_QUEUE_FULL:
            printf("[app] QUEUE_FULL\n");
            break;
        default:
            printf("[app] event %d\n", event);
            break;
    }
}

/* ---- Main ---- */

int main(void)
{

    transport.send = loopback_send;
    /* Transport should call comm_on_receive() and comm_send_done_handler() directly.
       comm_init still initializes queues, protocol, and transport parent_context. */

    /* comm_init handles queue initialization internally. */
    comm_init(&comm, &transport, &proto, app_event,
              tx_queue_buf, sizeof(tx_queue_buf),
              rx_queue_buf, sizeof(rx_queue_buf));

    printf("=== Loopback example ===\n\n");

    /* --- Send message 1 --- */
    const char *msg = "Hello, World!";
    printf("[app] Sending: \"%s\"\n", msg);
    comm_send(&comm, (const uint8_t *)msg, (uint16_t)strlen(msg));
    comm_process(&comm);      /* TX: calls transport.send → loopback_send captures frame */

    /* Simulate send_done from ISR. */ 
    transport_uart_on_send_done(&comm);
    /* Inject the captured frame back as incoming bytes via on_receive (ISR path). */
    transport_uart_on_receive(&comm, loopback_frame, loopback_frame_len);
    comm_process(&comm);      /* RX: parses frame → fires COMM_EVENT_RX_DONE */

    printf("\n");

    /* --- Send message 2 (binary payload) --- */
    const uint8_t binary[] = {0x01, 0x02, 0x03, 0xDE, 0xAD, 0xBE, 0xEF};
    printf("[app] Sending binary (%u bytes)\n", (unsigned)sizeof(binary));
    comm_send(&comm, binary, sizeof(binary));
    comm_process(&comm);
        /* Simulate send_done from ISR. */
    transport_uart_on_send_done(&comm);
   transport_uart_on_receive(&comm, loopback_frame, loopback_frame_len);
    comm_process(&comm);

    printf("\n=== Done ===\n");
    return 0;
}
