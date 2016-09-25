#ifndef _midi_event_h_
#define _midi_event_h_

#include "sequencer_globals.h"

class MemoryBlock;              // forward declaration

struct midi_event {
  long          time;           // time to wait till channel_byte-event
  uint8         channel_byte;
  uint8         data[2];        // data bytes following channel_byte
  midi_event    *p_next;
  midi_event    *p_prev;
  MemoryBlock   *p_owner;

  static inline bool IsKeyOff(uint8 voice_message, uint8 velocity)
  {
    if(((((voice_message & ~0x0F) == 0x90) && (velocity == 0))) || 
       ((voice_message & ~0x0F) == 0x80)) return true;
    
    return false;
  } // IsKeyOff
  
  static inline bool IsKeyOn(uint8 voice_message, uint8 velocity) {
    if(((voice_message & ~0x0F) == 0x90) && (velocity > 0)) return true;

    return false;
  } // IsKeyOn

}; // struct midi_event

#endif

