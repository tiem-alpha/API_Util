#ifndef COMM_QUEUE_H
#define COMM_QUEUE_H

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    uint8_t  *buffer;
    uint16_t  size;
    uint16_t  head;
    uint16_t  tail;
    bool      overflow;
} comm_queue_t;

void     comm_queue_init(comm_queue_t *q, uint8_t *buffer, uint16_t size);
uint16_t comm_queue_push(comm_queue_t *q, const uint8_t *data, uint16_t len);
bool     comm_queue_pop(comm_queue_t *q, uint8_t *byte_out);
uint16_t comm_queue_count(const comm_queue_t *q);
bool     comm_queue_is_empty(const comm_queue_t *q);

#endif /* COMM_QUEUE_H */
