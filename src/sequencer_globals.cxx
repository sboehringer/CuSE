#include "sequencer_globals.h"
#include "track.h"

using namespace std;

long ReadVariableLength(ifstream* p_midi_file, uint32 &var_length_quantity)
{
  uint8 byte;
  long  bytes_read = 0;

  p_midi_file->read((char*)&byte, 1);
  bytes_read++;
  
  var_length_quantity = (byte & 127);
  
  while((byte & 128) != 0) {
    
    // another byte will follow
    
    var_length_quantity <<= 7;
    
    p_midi_file->read((char*)&byte, 1);
    bytes_read++;
    var_length_quantity |= (byte & 127);
  } // while

  return bytes_read;
} // ReadVariableLength

long WriteVariableLength(ofstream* p_midi_file, uint32 value)
{
  uint8  result[4];
  uint32 index = 0;

  do {
    result[index] = (uint8)value | 128; // copy first 7 bits and set last bit

    value >>= 7; // advance to the next 7 bits
    index++;
  } while(value != 0); // unless there is nothing left

  result[0] &= (uint32)~128;

  for(int i = index-1; i >= 0; i--)
    p_midi_file->write((char*)&(result[i]), 1);

  return index;
} // WriteVariableLength

void MidiNoteToString(uint8 note, char* text)
{
  char octave[20];

  switch(note % 12) {

  case 0:
    strcpy(text, "C");
    break;

  case 1:
    strcpy(text, "C#");
    break;

  case 2:
    strcpy(text, "D");
    break;

  case 3:
    strcpy(text, "D#");
    break;

  case 4:
    strcpy(text, "E");
    break;

  case 5:
    strcpy(text, "F");
    break;

  case 6:
    strcpy(text, "F#");
    break;

  case 7:
    strcpy(text, "G");
    break;

  case 8:
    strcpy(text, "G#");
    break;

  case 9:
    strcpy(text, "A");
    break;

  case 10:
    strcpy(text, "A#");
    break;

  case 11:
    strcpy(text, "B");
    break;

  default:
    strcpy(text, "INV");
    break;
  } // switch

  sprintf(octave, "%d", (note / 12) - 2);
  strcat(text, octave);
} // MidiNoteToString

void CalcOptBoxPosition(Track* p_active_track, Track* p_first_displayed_track, int &x_pos, int &y_pos)
{
  y_pos = 2;

  while((p_first_displayed_track != p_active_track) && (p_first_displayed_track != NULL)) {

    y_pos += 2;
    p_first_displayed_track = p_first_displayed_track->GetNext();

  } // while 
} // CalcOptBoxPosition

void SetSeqColor(SeqColor state)
{
  switch(state) {
  case SCOL_NORMAL:
    attrset(A_NORMAL);
    color_set(2, NULL);
    break;

  case SCOL_STATUS_LINE:
    attrset(A_BOLD);
    color_set(3, NULL);
    break;

  case SCOL_VALUE:
    attrset(A_BOLD);
    color_set(2, NULL);
    break;
    
  case SCOL_INVERSE:
    attrset(A_BOLD);
    color_set(4, NULL);
    break;
  }; // switch
} // SetSeqColor

long GetNoFollowingDataBytes(uint8 message)
{
  long  bytes_to_follow;
  uint8 message_without_channel;

  message_without_channel = message & ~0x0F;
  bytes_to_follow         = 2;

  if((message_without_channel == 0xC0) || (message_without_channel == 0xD0))
    bytes_to_follow = 1;
  
  if(message_without_channel == 0xF0) {
    switch(message) {
    case 0xF0:

      // in the midi specification, and arbitrary number of bytes can
      // follow a system exclusive message. However, we will store each
      // byte of the sys exclusive message with a separate 0xF0 byte so
      // we return 1 were. This only refers to our own implementation,
      // not to the midi standard. It enables us to store many bytes
      // by allocating a midi-event for each byte. Memory consuming?
      // yes, but a "keep it simple and stupid" solution for a rare event

      bytes_to_follow = 1; // variable length
      break;
      
    case 0xF1:
      bytes_to_follow = 0;  // undefined
      break;
      
    case 0xF2:
      bytes_to_follow = 0;  // undefined
      break;
      
    case 0xF3:
      bytes_to_follow = 1;  // song select
      break;
      
    case 0xF4:
      bytes_to_follow = 0;  // undefined
      break;
      
    case 0xF5:
      bytes_to_follow = 0;  // undefined
      break;
      
    case 0xF6:
      bytes_to_follow = 0;  // tune request
      break;
      
    case 0xF7:
      bytes_to_follow = 0;  // EOX (terminator) or sys. exclusive (0xF0)
      break;
      
    case 0xF8:
      bytes_to_follow = 0;  // midi clock
      break;
      
    case 0xFE:
      bytes_to_follow = 0;  // active sense with nothing else 
      break;
      
    default:
      break;
    } // switch
  } // if

  return bytes_to_follow;
} // GetNoFollowingDataBytes

//
// MIDI callback
//
#ifdef __CYGWIN__

void midi_in_add(midi_in_stream* lpStream, char* lpData, long length)
{
  if (length > MIDI_IN_BUFFER_SIZE)
    return;

  lpStream->BytesWaiting = length;

  while ((lpStream->BytesRead + lpStream->BytesWaiting > MIDI_IN_BUFFER_SIZE) && !lpStream->Cancel)
    pthread_cond_wait(&lpStream->WriteCond, &lpStream->ReadWriteMutex);

  if (lpStream->Offset + lpStream->BytesRead + lpStream->BytesWaiting <= MIDI_IN_BUFFER_SIZE)
  {
    memcpy(&lpStream->MidiBuffer[lpStream->Offset + lpStream->BytesRead], lpData, lpStream->BytesWaiting);
  }
  else if (lpStream->Offset + lpStream->BytesRead >= MIDI_IN_BUFFER_SIZE)
  {
    memcpy(&lpStream->MidiBuffer[lpStream->Offset + lpStream->BytesRead - MIDI_IN_BUFFER_SIZE], lpData, lpStream->BytesWaiting);
  }
  else
  {
    memcpy(&lpStream->MidiBuffer[lpStream->Offset + lpStream->BytesRead], lpData, MIDI_IN_BUFFER_SIZE - lpStream->Offset - lpStream->BytesRead);
    memcpy(&lpStream->MidiBuffer[0], lpData + MIDI_IN_BUFFER_SIZE - lpStream->Offset - lpStream->BytesRead, lpStream->BytesWaiting - MIDI_IN_BUFFER_SIZE + lpStream->Offset + lpStream->BytesRead);
  }

  lpStream->BytesRead += lpStream->BytesWaiting;
  lpStream->BytesWaiting = 0;
}

// dwParam2 is arrival time of midi-event in case of wMsg == MIM_DATA or MIM_LONGDATA

void CALLBACK midi_in_cb(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
  midi_in_stream* lpStream = (midi_in_stream*) dwInstance;

  pthread_mutex_lock(&lpStream->ReadWriteMutex);

  switch (wMsg)
  {
    case MIM_DATA:
    {
      midi_in_add(lpStream, (char*) &dwParam1, GetNoFollowingDataBytes((uint8) dwParam1) + 1);
      break;
    }

    case MIM_LONGDATA:
    {
      LPMIDIHDR lpMidiHdr = (LPMIDIHDR) dwParam1;
      midi_in_add(lpStream, (char*) lpMidiHdr->lpData, lpMidiHdr->dwBytesRecorded);

      lpMidiHdr->dwBytesRecorded = 0;
      if (!lpStream->Cancel)
        midiInAddBuffer(hMidiIn, lpMidiHdr, sizeof(MIDIHDR));
      break;
    }
  }

  if ((lpStream->BytesRead >= lpStream->BytesRequired) && !lpStream->Cancel)
    pthread_cond_signal(&lpStream->ReadCond);
  pthread_mutex_unlock(&lpStream->ReadWriteMutex);
};

void CALLBACK midi_out_cb(HMIDIOUT hMidiOut, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
  midi_out_stream* lpStream = (midi_out_stream*) dwInstance;

  pthread_mutex_lock(&lpStream->SysexMutex);
  if ((wMsg == MOM_DONE) && !lpStream->Cancel)
    pthread_cond_signal(&lpStream->SysexCond);
  pthread_mutex_unlock(&lpStream->SysexMutex);
}

#endif

