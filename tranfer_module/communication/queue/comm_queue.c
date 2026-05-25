#include "comm_queue.h"

void comm_queue_init(comm_queue_t *q, uint8_t *buffer, uint16_t size)
{
    q->buffer   = buffer;
    q->size     = size;
    q->head     = 0;
    q->tail     = 0;
    q->overflow = false;
}

uint16_t comm_queue_push(comm_queue_t *q, const uint8_t *data, uint16_t len)
{
    if (q->size == 0) {
        return 0;
    }

    uint16_t written = 0;
    for (uint16_t i = 0; i < len; i++) {
        uint16_t next = (uint16_t)((q->head + 1u) % q->size);
        if (next == q->tail) {
            q->overflow = true;
            break;
        }
        q->buffer[q->head] = data[i];
        q->head = next;
        written++;
    }
    return written;
}

bool comm_queue_pop(comm_queue_t *q, uint8_t *byte_out)
{
    if (q->size == 0 || q->head == q->tail) {
        return false;
    }
    *byte_out = q->buffer[q->tail];
    q->tail = (uint16_t)((q->tail + 1u) % q->size);
    return true;
}

uint16_t comm_queue_count(const comm_queue_t *q)
{
    if (q->size == 0) {
        return 0;
    }
    return (uint16_t)((q->head - q->tail + q->size) % q->size);
}

bool comm_queue_is_empty(const comm_queue_t *q)
{
    return (q->size == 0) || (q->head == q->tail);
}
