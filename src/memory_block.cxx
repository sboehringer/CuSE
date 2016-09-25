#include "memory_block.h"

MemoryBlock::MemoryBlock() {
  p_event_pool = new midi_event[NO_POOL_EVENTS];

  for (long i = 0; i < NO_POOL_EVENTS; i++) {
    p_event_pool[i].p_owner = this;
    p_event_pool[i].p_next  = &p_event_pool[i+1];
  }
  p_event_pool[NO_POOL_EVENTS-1].p_next = NULL;

  p_next = NULL;
  p_prev = NULL;

  p_free_events = p_event_pool;
  i_free_events = NO_POOL_EVENTS;
}

MemoryBlock::~MemoryBlock() {
  delete[] p_event_pool;
}

