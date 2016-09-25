#include "pool_container.h"

const long PoolContainer::MEMORY_BLOCK_SIZE = sizeof(MemoryBlock) + MemoryBlock::NO_POOL_EVENTS * sizeof(midi_event);

static inline void exchange(MemoryBlock *p_first, MemoryBlock *p_second)
{
  p_first->p_next = p_second->p_next;
  if (p_second->p_next)
    p_second->p_next->p_prev = p_first;

  p_second->p_prev = p_first->p_prev;
  if (p_first->p_prev)
    p_first->p_prev->p_next = p_second;

  p_first->p_prev = p_second;
  p_second->p_next = p_first;
} // exchange

static inline void unlink(MemoryBlock *p_block) {
  if (p_block->p_next)
    p_block->p_next->p_prev = p_block->p_prev;
  if (p_block->p_prev)
    p_block->p_prev->p_next = p_block->p_next;
} // unlink

PoolContainer::PoolContainer()
{
  MemoryBlock *p_temp = new MemoryBlock();

  p_temp->p_next = NULL;
  p_temp->p_prev = NULL;

  p_last = p_current = p_temp;
  total_memory_used  = MEMORY_BLOCK_SIZE;
} // PoolContainer::PoolContainer

PoolContainer::~PoolContainer()
{
  MemoryBlock *p_temp;

  do {
    p_temp = p_last;
    p_last = p_last->p_prev;
    delete p_last;
  } while (p_last != NULL);
} // PoolContainer::~PoolContainer

midi_event* PoolContainer::GetEventFromPool()
{
  midi_event *p_event;
  
  if (p_current->i_free_events == 0) {
    MemoryBlock *p_temp = new MemoryBlock();

    p_last->p_next = p_temp;
    p_temp->p_prev = p_last;
    p_last = p_current = p_temp;

    total_memory_used += MEMORY_BLOCK_SIZE;
  }

  p_event = p_current->p_free_events;
  p_current->p_free_events = p_current->p_free_events->p_next;
  p_current->i_free_events--;

  if ((p_current->i_free_events == 0) && (p_current->p_next != NULL)) {
    p_current = p_current->p_next;
  }

  return p_event;
} // PoolContainer::GetEventFromPool

void PoolContainer::ReturnUnusedEvent(midi_event *p_event)
{
  if(p_event == NULL) return;

  MemoryBlock *p_owner = p_event->p_owner;

  p_event->p_next = p_owner->p_free_events;
  p_owner->p_free_events = p_event;
  p_owner->i_free_events++;

  if ((p_owner->i_free_events == MemoryBlock::NO_POOL_EVENTS) &&
     ((p_owner->p_prev != NULL) || (p_owner->p_next != NULL))) {
    if (p_owner == p_last)
      p_last = p_owner->p_prev;
    if (p_owner == p_current)
      if (p_owner == p_last)
        p_current = p_owner->p_prev;
      else
        p_current = p_owner->p_next;

    unlink(p_owner);

    total_memory_used -= MEMORY_BLOCK_SIZE;
    delete p_owner;
    return;
  }

  while ((p_owner->p_next != NULL) && (p_owner->i_free_events > p_owner->p_next->i_free_events)) {
    if (p_owner == p_current)
      p_current = p_owner->p_next;
    if (p_owner->p_next == p_last)
      p_last = p_owner;

    exchange(p_owner, p_owner->p_next);
  }

  if (p_owner->p_next == p_current)
    p_current = p_owner;
} // PoolContainer::ReturnUnusedEvent

#ifdef DEBUG

bool PoolContainer::VerifyPool(ostream *stream) {
  long count_mem, count_bwd, count_fwd;
  long current_free, last_free;
  MemoryBlock *p_temp_block, *p_first_block;
  midi_event  *p_temp_event;

  // memory used
  count_mem = total_memory_used / MEMORY_BLOCK_SIZE;

  // count backwards
  count_bwd = 1;
  p_temp_block = p_last;
  while (p_temp_block->p_prev != NULL) {
    p_temp_block = p_temp_block->p_prev;
    count_bwd++;
  }

  // count forwards
  count_fwd = 1;
  p_first_block = p_temp_block;
  while (p_temp_block->p_next != NULL) {
    p_temp_block = p_temp_block->p_next;
    count_fwd++;
  }

  // print results
  *stream << count_mem << " blocks ";
  if (count_bwd != count_mem) {
    *stream << "(bwd)" << endl;
    return false;
  }
  if (count_fwd != count_mem) {
    *stream << "(fwd)" << endl;
    return false;
  }
  if (p_temp_block != p_last) {
    *stream << "(ptr)" << endl;
    return false;
  }

  // verify memory blocks
  current_free = 0;
  p_temp_block = p_first_block;
  while (p_temp_block != NULL) {
    // print number of free events
    *stream << "[" << p_temp_block->i_free_events << ((p_temp_block == p_current) ? "]* " : "] ");
    
    // count free events
    last_free = current_free;
    current_free = 0;
    p_temp_event = p_temp_block->p_free_events;
    while (p_temp_event != NULL) {
      p_temp_event = p_temp_event->p_next;
      current_free++;
    }

    // print results
    if (p_temp_block->i_free_events != current_free) {
      *stream << "(cnt)" << endl;
      return false;
    }
    if (current_free < last_free) {
      *stream << "(ord)" << endl;
      return false;
    }
    if ((p_temp_block->p_prev != NULL) && (p_temp_block == p_current) && (p_temp_block->p_prev->i_free_events != 0)) {
      *stream << "(ptr)" << endl;
      return false;
    }

    p_temp_block = p_temp_block->p_next;
  }
  
  // success
  *stream << endl;
  return true;
} // PoolContainer::VerifyPool

#endif

