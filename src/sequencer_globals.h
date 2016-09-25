
#ifndef _sequencer_globals_
#define _sequencer_globals_

#ifdef __CYGWIN__
  #define boolean boolean_1165935432
  #define Beep Beep_1165935460
  #include <windows.h>
  #undef boolean
  #undef Beep
  #define MIDI_IN_BUFFER_SIZE 64
  #define MIDI_OUT_BUFFER_SIZE 64
#else
  #include <fstream>
#endif

#include <iostream>

//
// global
//

using namespace std;

extern bool sequencer_debug_info;

typedef unsigned char  uint8;
typedef unsigned short uint16;
typedef unsigned int   uint32;

const uint16 ALL_CHANNELS               = (0xFF << 8) | (0xFF);
const int    NO_CHAR_CHANNEL_NAME       = 80;
const int    MAX_CHAR_CHAN_NAME_DISPLAY = 12;    // display that many characters of channel name
const int    NO_KEY_COMBINATIONS        = 40;
const int    KEY_COMMAND_MODULO_BASE    = 4;     // Base of numbers played by PlayNumberSequence
const int    LENGTH_DISPLAY_TEXT        = 512;
const int    FIRST_LINE_DISPLAY_TRACK   = 7;

enum SeqColor {SCOL_NORMAL, SCOL_STATUS_LINE, SCOL_VALUE, SCOL_INVERSE};

long ReadVariableLength(std::ifstream*, uint32&);
long WriteVariableLength(std::ofstream*, uint32);
long GetNoFollowingDataBytes(uint8 message);

void* RecMidiEvents(void*);
void* PlayMidiEvents(void*);
void* PlayMetronome(void*);

void RedrawScreen();
void RefreshStatusText();
void SetStatusText(char*);

void MidiNoteToString(uint8, char*);

void CalcOptBoxPosition(class Track*, class Track*, int&, int&);

void CheckCommandKeyCombination();
void StoreKeyCombination();

void SetSeqColor(SeqColor);

//
// MIDI streams
//

#ifdef __CYGWIN__

void CALLBACK midi_in_cb(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2);
void CALLBACK midi_out_cb(HMIDIOUT hMidiOut, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2);

class midi_in_stream
{
  private:
    MMRESULT Res;
    MIDIHDR MidiHdr;
    char* SysexBuffer;

  protected:
    HMIDIIN Handle;

  public:
    char* MidiBuffer;
    long Offset;
    long BytesRead;
    long BytesRequired;
    long BytesWaiting;
    pthread_mutex_t ReadWriteMutex;
    pthread_cond_t ReadCond;
    pthread_cond_t WriteCond;
    bool Cancel;

  public:
    midi_in_stream(UINT num)
    {
      MidiBuffer  = new char[MIDI_IN_BUFFER_SIZE];
      SysexBuffer = new char[MIDI_IN_BUFFER_SIZE >> 1];
      Res = midiInOpen(&Handle, num, (DWORD_PTR) &midi_in_cb, (DWORD_PTR) this, CALLBACK_FUNCTION);

      if (Res == MMSYSERR_NOERROR)
      {
        MidiHdr.lpData = (CHAR*) &SysexBuffer[0];
        MidiHdr.dwBufferLength = MIDI_IN_BUFFER_SIZE >> 1;

        MidiHdr.dwFlags = 0;
        Res = midiInPrepareHeader(Handle, &MidiHdr, sizeof(MIDIHDR));
        if (Res == MMSYSERR_NOERROR)
        {
          midiInAddBuffer(Handle, &MidiHdr, sizeof(MIDIHDR));
          if (Res == MMSYSERR_NOERROR)
            midiInStart(Handle);
        }
      }
      else
      {
        Handle = 0;
      }

      pthread_mutex_init(&ReadWriteMutex, NULL);
      pthread_cond_init(&ReadCond, NULL);
      pthread_cond_init(&WriteCond, NULL);
      Cancel = false;
      BytesWaiting = 0;
      BytesRequired = 0;
      BytesRead = 0;
      Offset = 0;
    };

    ~midi_in_stream()
    {
      pthread_mutex_lock(&ReadWriteMutex);
      Cancel = true;

      if (Handle != 0)
      {
        midiInStop(Handle);
        midiInReset(Handle);
        while (midiInUnprepareHeader(Handle, &MidiHdr, sizeof(MIDIHDR)) == MIDIERR_STILLPLAYING);
        midiInClose(Handle);
      }
      delete[] SysexBuffer;
      delete[] MidiBuffer;

      pthread_cond_signal(&ReadCond);
      pthread_cond_signal(&WriteCond);
      pthread_mutex_unlock(&ReadWriteMutex);
      pthread_cond_destroy(&ReadCond);
      pthread_cond_destroy(&WriteCond);
      pthread_mutex_destroy(&ReadWriteMutex);
    };

    void read(char* lpData, long length)
    {
      if ((Handle == 0) || (length > MIDI_IN_BUFFER_SIZE))
        return;
      
      pthread_mutex_lock(&ReadWriteMutex);
      BytesRequired = length;
      while ((BytesRead < BytesRequired) && !Cancel)
        pthread_cond_wait(&ReadCond, &ReadWriteMutex);
      
      if (Offset + BytesRequired <= MIDI_IN_BUFFER_SIZE)
      {
        memcpy(lpData, &MidiBuffer[Offset], BytesRequired);
        BytesRead -= BytesRequired;
        Offset    += BytesRequired;
        if (Offset == MIDI_IN_BUFFER_SIZE) Offset = 0;
      }
      else
      {
        memcpy(lpData, &MidiBuffer[Offset], MIDI_IN_BUFFER_SIZE - Offset);
        memcpy(lpData + MIDI_IN_BUFFER_SIZE - Offset, &MidiBuffer[0], BytesRequired - MIDI_IN_BUFFER_SIZE + Offset);
        BytesRead -= BytesRequired;
        Offset     = BytesRequired - MIDI_IN_BUFFER_SIZE + Offset;
      }
      BytesRequired = 0;

      if ((BytesRead + BytesWaiting <= MIDI_IN_BUFFER_SIZE) && !Cancel)
        pthread_cond_signal(&WriteCond);
      pthread_mutex_unlock(&ReadWriteMutex);
    };

    bool eof() { return (Handle == 0); };
    bool bad() { return (Handle == 0); };
}; // midi_in_stream

class midi_out_stream
{
  private:
    MMRESULT Res;
    MIDIHDR MidiHdr;
    unsigned char* MidiBuffer;
    bool IsSysex;
    long BytesRead;
    long BytesRequired;

  protected:
    HMIDIOUT Handle;

  public:
    pthread_mutex_t SysexMutex;
    pthread_cond_t SysexCond;
    bool Cancel;

  public:
    midi_out_stream(UINT num)
    {
      Res = midiOutOpen(&Handle, num, (DWORD_PTR) &midi_out_cb, (DWORD_PTR) this, CALLBACK_FUNCTION);
      if (Res != MMSYSERR_NOERROR)
        Handle = 0;

      MidiBuffer = new unsigned char[MIDI_OUT_BUFFER_SIZE];
      MidiHdr.lpData = (CHAR*) &MidiBuffer[0];
      MidiHdr.dwBufferLength = MIDI_OUT_BUFFER_SIZE;

      pthread_mutex_init(&SysexMutex, NULL);
      pthread_cond_init(&SysexCond, NULL);
      Cancel = false;
      BytesRequired = 0;
      BytesRead = 0;
      IsSysex = false;
    };

    ~midi_out_stream()
    {
      pthread_mutex_lock(&SysexMutex);
      Cancel = true;

      if (Handle != 0)
      {
        while (midiOutUnprepareHeader(Handle, &MidiHdr, sizeof(MIDIHDR)) == MIDIERR_STILLPLAYING);
        midiOutReset(Handle);
        midiOutClose(Handle);
      }
      delete[] MidiBuffer;

      pthread_cond_signal(&SysexCond);
      pthread_mutex_unlock(&SysexMutex);
      pthread_cond_destroy(&SysexCond);
      pthread_mutex_destroy(&SysexMutex);
    };

    void put(unsigned char c)
    {
      if (Handle == 0)
        return;

      if (BytesRead == 0)
      {
        if (c == 0xF0)
        {
          BytesRequired = MidiHdr.dwBufferLength;
          IsSysex = true;
        }
        else
        {
          BytesRequired = GetNoFollowingDataBytes(c) + 1;
        }
      }

      MidiBuffer[BytesRead++] = c;

      if (!IsSysex && (BytesRead == BytesRequired))
      {
        DWORD Msg = 0;
        while (BytesRead > 0)
          Msg = (Msg << 8) | MidiBuffer[--BytesRead];
        midiOutShortMsg(Handle, Msg);
      }
      else if (IsSysex && (c == 0xF7))
      {
        pthread_mutex_lock(&SysexMutex);
        MidiHdr.dwBytesRecorded = BytesRead;
        MidiHdr.dwFlags = 0;
        Res = midiOutPrepareHeader(Handle, &MidiHdr, sizeof(MIDIHDR));
        if (Res == MMSYSERR_NOERROR)
          midiOutLongMsg(Handle, &MidiHdr, sizeof(MIDIHDR));

        while ((midiOutUnprepareHeader(Handle, &MidiHdr, sizeof(MIDIHDR)) == MIDIERR_STILLPLAYING) && !Cancel)
          pthread_cond_wait(&SysexCond, &SysexMutex);

        BytesRead = 0;
        IsSysex = false;
        pthread_mutex_unlock(&SysexMutex);
      }
      else if (IsSysex && (BytesRead == BytesRequired))
      {
        unsigned char* OldBuffer = MidiBuffer;
        MidiHdr.dwBufferLength += MIDI_OUT_BUFFER_SIZE;
        BytesRequired = MidiHdr.dwBufferLength;
        MidiBuffer = new unsigned char[MidiHdr.dwBufferLength];
        MidiHdr.lpData = (CHAR*) &MidiBuffer[0];
        memcpy(MidiBuffer, OldBuffer, BytesRead);
        delete[] OldBuffer;
      }
    };

    bool good()  { return (Handle != 0); };
    bool bad()   { return (Handle == 0); };
    void flush() { };
}; // midi_out_stream

#else

class midi_in_stream
{
 protected:
  ifstream* p_in_stream;

 public:
  midi_in_stream(char* p_name) {p_in_stream = new ifstream(p_name);};
  ~midi_in_stream() {p_in_stream->close(); delete p_in_stream; p_in_stream = NULL;};

  void read(char* p_data, long length) {p_in_stream->read(p_data, length);};
  bool eof() {return p_in_stream->eof();};
  bool bad() {return p_in_stream->bad();};
}; // midi_in_stream

class midi_out_stream
{
 protected:
  ofstream *p_out_stream;

 public:
  midi_out_stream(char* p_name) {p_out_stream = new ofstream(p_name);};
  ~midi_out_stream() {p_out_stream->close(); delete p_out_stream; p_out_stream = NULL;};

  void put(unsigned char c) {p_out_stream->put(c);};
  bool good() {return p_out_stream->good();};
  bool bad() {return p_out_stream->bad();};
  void flush() {p_out_stream->flush();};
}; // midi_out_stream

#endif

//
// PrintError
//

inline void PrintError(char* text, bool line_feed = true)
{
  if(sequencer_debug_info) {
    cerr << text << flush;
    if(line_feed) cerr << endl;
  } // if
}

inline void PrintErrorInt(int value, bool line_feed = true)
{
  if(sequencer_debug_info) {
    cerr << dec << (int)value << flush;
    if(line_feed) cerr << endl;
  } // if
}

inline void PrintErrorHex(int value, bool line_feed = true)
{
  if(sequencer_debug_info) {
    cerr << hex << value << flush;
    if(line_feed) cerr << endl;
  } // if
}

inline void PrintErrorChar(char c, bool line_feed = true)
{
  if(sequencer_debug_info) {
    cerr << dec << (int)c << flush;
    if(line_feed) cerr << endl;
  } // if
}

#endif

