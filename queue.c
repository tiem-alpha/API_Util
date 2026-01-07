/*
 * queue.c
 *
 *  Created on: Jun 9, 2024
 *      Author: nguye
 */

#include "queue.h"
#include <stdlib.h>

static uint16_t queue_get_tail(queue *mQueue); 
static uint16_t queue_get_head(queue *mQueue);

void queue_init(queue * mQueue, uint8_t* buff, uint16_t size){
  if(buff == NULL){
    return; 
  }
  mQueue->_buffer = buff;
  mQueue->_size = size;
  mQueue->_head = mQueue->_tail = 0; 
  mQueue->_overwrite =0; 
}


uint8_t queue_pop_byte(queue * mQueue, uint8_t *byte)
{
  if(queue_is_empty(mQueue)){
    return QUEUE_EMPTY;
  }
  // head++
  
  mQueue->_overwrite =0; 
}

uint16_t queue_pop(queue * mQueue, uint8_t *buffer, uint16_t length)
{

}


bool queue_push_byte(queue * mQueue, uint8_t value);
uint16_t queue_push(queue * mQueue, uint8_t *buff, uint16_t length);
uint8_t queue_peek(queue * mQueue);

bool queue_is_full(queue * mQueue);

bool queue_is_empty(queue * mQueue);

uint16_t queue_get_space(queue * mQueue);

bool queue_reset_push(queue *mQueue, uint8_t idx);
uint16_t queue_poll(queue *mQueue, uint8_t *buffOut, uint16_t buffOutSize); 




bool queue_is_full(queue *mQueue)
{
  if (mQueue->_head == mQueue->_tail && mQueue->_overwrite == 1)
  {
    return true;
  }
  return false;
}

bool queue_is_empty(queue *mQueue)
{
  if (mQueue->_head == mQueue->_tail && mQueue->_overwrite == 0)
  {
    return true;
  }
  return false;
}

uint16_t queue_get_space(queue *mQueue)
{
  uint16_t size = mQueue->_size;
  if(mQueue->_overwrite == 1) return 0;
  uint16_t head = queue_get_head(mQueue);
  uint16_t tail = queue_get_tail(mQueue);
  return size -  (tail + size - head) % size; 
}

uint16_t queue_get_tail(queue *mQueue)
{
  uint16_t tail =0;
  uint16_t temp = mQueue->_tail; // interupt here
  uint16_t temp1 = mQueue->_tail; 
  uint16_t temp2 = mQueue->_tail; // interupt here
  if(temp == temp1) // interupt in temp2
  {
    tail = temp;
  }
  else // temp1!= temp2 
  {
    if(temp1 == temp2) // interupt 0
    {
      tail = temp1;
    }
    else // interupt temp 1
    {
      tail = temp2;
    }
  } 
  return tail; 
}


uint16_t queue_get_head(queue *mQueue)
{
  uint16_t head =0;
  uint16_t temp = mQueue->_head; // interupt here
  uint16_t temp1 = mQueue->_head; 
  uint16_t temp2 = mQueue->_head; // interupt here
  if(temp == temp1) // interupt in temp2
  {
    head = temp;
  }
  else // temp1!= temp2 
  {
    if(temp1 == temp2) // interupt 0
    {
      head = temp1;
    }
    else // interupt temp 1
    {
      head = temp2;
    }
  } 
  return head; 
}


uint16_t queue_get_data_length(queue *mQueue){
  uint16_t size = mQueue->_size;
  if(mQueue->_overwrite == 1) return size; 
  uint16_t head = queue_get_head(mQueue);
  uint16_t tail = queue_get_tail(mQueue);
  return (tail + size - head) % size; 
}

bool queue_reset_push(queue *mQueue, uint8_t idx)
{
  if (mQueue->head < mQueue->tail)
  {
    if (idx >= mQueue->head && idx <= mQueue->tail)
    {
      mQueue->tail = idx;
      return true;
    }
  }

  if (mQueue->head < mQueue->tail)
  {
    if ((idx >=0 &&idx <= mQueue->tail) || (idx <=  QUEUE_MAX_SIZE && idx >= mQueue->head))
    {
      mQueue->tail = idx;
      return true;
    }
  }

  if (mQueue->head == mQueue->tail && idx == mQueue->head)
  {
    return true;
  }
  return false;
}
