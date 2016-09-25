#ifndef _memory_block_h_
#define _memory_block_h_

#include "midi_event.h"

class MemoryBlock {
  public:
    static const long   NO_POOL_EVENTS = 256;
    midi_event          *p_free_events;
    MemoryBlock         *p_next;
    MemoryBlock         *p_prev;
    long                i_free_events;

  protected:
    midi_event          *p_event_pool;

  public:
    MemoryBlock();
    virtual ~MemoryBlock();
}; // class MemoryBlock

#endif

