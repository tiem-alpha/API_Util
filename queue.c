/*
 * queue.c
 *
 *  Created on: Jan 9, 2026
 *      Author: nguye
 */


/*
 * queue.c
 *
 *  Created on: Jun 9, 2024
 *      Author: nguye
 */

#include "queue.h"
#include <stdlib.h>
/// queue is full when tail+1 % size = head
// queue is empty when head == tail

void queue_init(queue *mQueue, uint8_t *buff, uint16_t size)
{
  if (buff == NULL)
  {
    return;
  }
  mQueue->_buffer = buff;
  mQueue->_size = size;
  mQueue->_head = mQueue->_tail = 0;
  mQueue->_overwrite = 0;
}

uint8_t queue_pop_byte(queue *mQueue, uint8_t *byte) // run on main
{
  if (mQueue->_head == mQueue->_tail)
  {
    return QUEUE_EMPTY;
  }
  // head++
  *byte = mQueue->_buffer[mQueue->_head];
  mQueue->_head = (mQueue->_head + 1) % mQueue->_size;
  mQueue->_overwrite = 0;
  return QUEUE_SUCCESS;
}

uint16_t queue_pop(queue *mQueue, uint8_t *buffer, uint16_t length)
{
  uint16_t dataLength = queue_get_data_length(mQueue);
  // if (dataLength == 0)
  // {
  //   return 0;
  // }
  dataLength = dataLength < length ? dataLength : length;
  for (int i = 0; i < dataLength; i++)
  {
    buffer[i] = mQueue->_buffer[mQueue->_head];
    mQueue->_head = (mQueue->_head + 1) % mQueue->_size;
  }
  mQueue->_overwrite = 0;
  return dataLength;
}

uint8_t queue_push_byte(queue *mQueue, uint8_t value)
{
  if((mQueue->_tail +1) % mQueue->_size == mQueue->_head)
  {
    mQueue->_overwrite = 1;
    return QUEUE_FULL;
  }
  mQueue->_buffer[mQueue->_tail] = value;
  mQueue->_tail = (mQueue->_tail + 1) % mQueue->_size;
  return QUEUE_SUCCESS;
}

uint16_t queue_push(queue *mQueue, uint8_t *buff, uint16_t length)
{
  uint16_t i =0;
  for ( i = 0; i < length; i++)
  {
    if(queue_push_byte(mQueue, buff[i-1])  == QUEUE_FULL)
    {
      return i;
    }
  }
  return i;
}

uint8_t queue_peek(queue *mQueue, uint8_t *value)
{
  if (queue_is_empty(mQueue))
  {
    return QUEUE_EMPTY;
  }
  *value = mQueue->_buffer[mQueue->_head];
  return QUEUE_SUCCESS;
}


bool queue_is_full(queue *mQueue)
{
 if((mQueue->_tail +1) % mQueue->_size == mQueue->_head)
  {
    return true;
  }
  return false;
}

bool queue_is_empty(queue *mQueue)
{
  if (mQueue->_head == mQueue->_tail)
  {
    return true;
  }
  return false;
}

uint16_t queue_get_space(queue *mQueue)
{
  return mQueue->_size  - 1 - (mQueue->_tail + mQueue->_size - mQueue->_head) % mQueue->_size ;
}

uint16_t queue_get_data_length(queue *mQueue)
{
  return (mQueue->_tail + mQueue->_size - mQueue->_head) % mQueue->_size;
}