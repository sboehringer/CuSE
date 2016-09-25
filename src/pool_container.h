#ifndef _pool_container_h_
#define _pool_container_h_

#include "memory_block.h"

class PoolContainer {
  public:
  static const long MEMORY_BLOCK_SIZE;

  protected:
    MemoryBlock *p_current;
    MemoryBlock *p_last;
    long        total_memory_used;

  public:
    PoolContainer();
    virtual ~PoolContainer();
  
    midi_event* GetEventFromPool();
    void        ReturnUnusedEvent(midi_event*);
    long        GetMemUsed() { return total_memory_used; };

#ifdef DEBUG
    bool        VerifyPool(ostream*);
#endif

}; // class PoolContainer

#endif

