#ifndef _track_h
#define _track_h

#include <stdlib.h>
#include <fstream>
#include <stdio.h>

#include "sequencer_globals.h"
#include "midi_event.h"
#include "sequencer.h"

// using namespace std;

class Track
{
  protected:

    static Track  *p_first_track;
    static Track  *p_last_track;

    Track         *p_prev, *p_next;

    class Sequencer* p_seq;                   // one and only global sequencer object
    uint32           number;
    long             loudness;
    bool             mute;                    // playing or not?
    int              current_display_line;
    int              blink_state;
    char             *p_name;
    uint16           midi_rec_channels;
    int16_t          midi_play_channel;
    long             last_current_song_time;  // update note-display only if time changed
    midi_event       *p_first_midi_event;
    midi_event       *p_current_midi_event;
    midi_event       *p_last_midi_event;
    unsigned char    on_keys[128];            // keys being "pressed" by song playback
    bool             note_display_dirty_flag; // redraw DisplayNextNotes() if true

  public:
    //    enum TrackStates {mute, rec, play};

    // TrackStates state;

  public:
  
    ~Track();

    static Track* GetNewTrack(class Sequencer*, uint16);
    static Track* GetFirst() {return p_first_track;};
    static Track* GetLast() {return p_last_track;};
    static Track* GetFirstFreeTrack(class Sequencer*);
    Track*        GetNext() {return p_next;};
    Track*        GetPrev() {return p_prev;};
    uint32        GetNumber() {return number;};
    char*         GetName() {return p_name;};
    uint16        GetMidiRecChannels() {return midi_rec_channels;};
    int16_t       GetMidiPlayChannel() {return midi_play_channel;};
    int           GetCurrentDisplayLine() {return current_display_line;};
    midi_event*&  GetFirstMidiEventPtr() {return p_first_midi_event;};
    midi_event*&  GetLastMidiEventPtr() {return p_last_midi_event;};
    long          GetLoudness() {return loudness;};
    uint8         GetOnKeys(int index) {return on_keys[index];};
    int           GetContainedMidiChannel();
    bool          GetMute() {return mute;};

    void          SetCurrentDisplayLine(int line) {current_display_line = line;};
    void          SetName(char*);
    void          SetMidiRecChannels(uint16 channels) {midi_rec_channels = channels;}; // bits represent channels
    void          SetMidiPlayChannel(int16_t channel) {midi_play_channel = channel;};  // number represents single channel
    void          SetLoudness(long value) {loudness = value;};
    void          SetOnKeys(int, uint8);
    void          SetMute(bool value) {mute = value;};

    void          MarkNoteDisplayDirty() {note_display_dirty_flag = true;};
    void          AddLoudness(long value) {loudness += abs(value);};
    void          JumpToStart();
    void          JumpNextEvent() {p_current_midi_event = p_current_midi_event->p_next;};
    midi_event*   PeekNextEvent() {return p_current_midi_event;};
    void          AddEvent(uint8, uint8[], long, long);
    void          SortInEvent(uint8, uint8[], long);
    void          Transpose(long, long, char);
    void          TimeShift(long, long, long);
    void          DeleteUpcomingEvent();
    void          DeleteLastEnduringNotes();
    void          DeleteAllEvents();
    void          EraseBeats(long, long);
    void          CopyBeats(long, long, long);
    void          DisplayNextNotes(int, int, long, double, long, bool);
    void          DisplayStatus(long, short, SeqColor, short, int blink = 0);
    void          SaveMidiFile(ofstream*, bool);
    void          MidiReset(midi_out_stream&);
    void          Quantize(long, long start_time = 0, long end_time = LONG_MAX);
    static void   MarkAllTracksDirty();
    void          CopyTrackTo(Track*);

  protected:
    void          GetChannelNumber(uint16, char*);
    void          DisplayVolumeMeter();
    void          SortList();
}; // class Track
#endif
