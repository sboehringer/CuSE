#ifndef _sequencer_h_
#define _sequencer_h_

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sys/time.h>
#include <pthread.h>
#include <curses.h>
#include <math.h>
#include <cdk/cdk.h>
#include <netinet/in.h>

#include "midi_event.h"
#include "pool_container.h"
#include "sequencer_globals.h"
#include "track.h"

using namespace std;

class Sequencer
{
protected:

  // io

  midi_in_stream&  midi_in;
  midi_out_stream& midi_out;

  // states of sequencer

  bool          midi_rec;
  bool          midi_rec_overwrite;                               // over notes between punch-in and punch-out
  bool          midi_play;
  bool          midi_play_step_by_step;
  bool          midi_rec_step_by_step;
  Track         *p_active_track;
  PoolContainer *p_pool_container;

  bool          play_beep;
  bool          rec_beep;

  // states for the keyboard command mode

  enum KeyboardCommandModes {KCM_INITIAL, KCM_CHANGE_MIDI_PLAYBACK_CHAN, KCM_CHANGE_MIDI_RECORD_CHAN, KCM_QUANTIZE_TRACK};

  KeyboardCommandModes keyboard_command_mode;

  // threads

  pthread_t     rec_thread;
  pthread_t     play_thread;
  pthread_t     metronome_thread;

  // timing related variables

  timeval       song_start_time;
  double        quarters_per_minute;
  double        live_speed_change;                                // speed changed via pitch wheel or keyboard
  long          beat;
  long          current_song_time;                                // updated time while playing
  long          system_time_of_passed_event;                      // system (real) time the last event was stored or played back
  midi_event*   p_passed_event;                                   // last event being stored or played back
  long          song_offset;
  long          punch_in, punch_out;
  long          step_by_step_exit_time;                           // stop play here in step-by-step mode
  long          step_by_step_note_len;                            // length of note added in step-by-step mode
  long          step_by_step_advance_time;                        // time to advance in step-by-step mode (need not be note length)
  long          resolve_quarters;                                 // resolve quarters that often in note display (= time resolution granularity)
  long          number_to_play;                                   // number to be played by PlayNumberSequence()

  bool          keyboard_esc_mode;                                // the next note/accord is interpreted as command if in this mode
  bool          speed_change_mode;                                // true if the pitch bend is used to alter play-back speed
  bool          midi_device_detected;                             // a keep alive message from a connected device was detected
  bool          midi_device_detected_reported;                    // we did not yet tell the user about the detection

  // sequencer control via music-keyboard

  unsigned char *p_current_active_keys;
  unsigned char *p_command_key_combinations[NO_KEY_COMBINATIONS];
  int           index_key_combination;                            // next p_command_key_combination to define

public:

  class Beeper {

  protected:
    uint8 metronome_channel;
    uint8 channel_byte;
    uint8 metronome_key1;
    uint8 metronome_key2;
    long  last_metronome_key;

    Track *p_last_active_track;
    int   blink;

  public:

    enum BeepState {BEEP_FIRST, BEEP_REST, BEEP_OFF};

    Beeper();

    void Init();
    void Beep(BeepState);
    bool Playing() {return (last_metronome_key != -1);};
    void DisplayStatusLine(Sequencer*);
    void DecLoudness();

  } beeper;
  
  bool          escape_metronome;                                 // make PlayMetronome return
  
 public:
  Sequencer(midi_in_stream&, midi_out_stream&);
  ~Sequencer();
  
  void             Init();
  bool             GetRec() {return midi_rec;};
  bool             GetRecOverwrite() {return midi_rec_overwrite;};
  bool             GetPlay() {return midi_play;};
  bool             GetPlayStepByStep() {return midi_play_step_by_step;};
  bool             GetRecStepByStep() {return midi_rec_step_by_step;};
  bool             GetPlayBeep() {return play_beep;};
  bool             GetRecBeep() {return rec_beep;};
  bool             GetSpeedChangeMode() {return speed_change_mode;};
  double           GetQuartersPerMinute() {return quarters_per_minute + live_speed_change;};
  long             GetResolveQuarters() {return resolve_quarters;};
  long             GetQuartersPerBeat() {return beat;};
  Track*           GetActiveTrack() {return p_active_track;};
  PoolContainer*   GetPoolContainer() {return p_pool_container;};
  long             GetCurrentSongTime() {return current_song_time;};
  long             GetSystemTimePassedEvent() {return system_time_of_passed_event;};
  midi_event*      GetPassedEvent() {return p_passed_event;};
  long             GetSongOffset() {return song_offset;};
  long             GetPassedSystemTime(timeval);
  long             GetPunchIn() {return punch_in;};
  long             GetPunchOut() {return punch_out;};
  long             GetPlayStepByStepExitTime() {return step_by_step_exit_time;};
  long             GetRecStepByStepNoteLength() {return step_by_step_note_len;};
  long             GetRecStepByStepAdvanceTime() {return step_by_step_advance_time;};
  midi_out_stream* GetMidiOutStream() {return &midi_out;};
  int              GetNumberToPlay() {return number_to_play;};
  bool             GetESCMetronome() {return escape_metronome;};
  bool             GetDeviceDetected();

  void SetRec(bool state) {midi_rec = state;};
  void SetRecOverwrite(bool value) {midi_rec_overwrite = value;};
  void SetPlay(bool state) {midi_play = state;};
  void SetPlayStepByStep(bool value) {midi_play_step_by_step = value;};
  void SetRecStepByStep(bool value) {midi_rec_step_by_step = value;};
  void SetCurrentSongTime(long value) {current_song_time = value;};
  void SetSystemTimePassedEvent(midi_event* p_event, long value) {p_passed_event = p_event; system_time_of_passed_event = value;};
  void SetSongOffset(long value) {song_offset = value; if(song_offset < 0) song_offset = 0;};
  void SetPunchIn(long value) {punch_in = value; if(punch_in < 0) punch_in = 0;};
  void SetPunchOut(long value) {punch_out = value; if(punch_out < 0) punch_out = 0;};
  void SetPlayStepByStepExitTime(long value) {step_by_step_exit_time = value;};
  void SetRecStepByStepNoteLength(long value); // 1=1/1, 2=1/2, 4=1/4=quarter note, ...
  void SetRecStepByStepAdvanceTime(long value);
  void SetPlayBeep(bool state) {play_beep = state;};
  void SetRecBeep(bool state) {rec_beep = state;};
  void SetSpeedChangeMode(bool state) {speed_change_mode = state;};
  void SetQuartersPerMinute(double value) {quarters_per_minute = value;};
  void SetLiveSpeedChange(double value) {live_speed_change = value;};
  void SetResolveQuarters(long value) {resolve_quarters = value;};
  void SetQuartersPerBeat(long value) {beat = value;};
  void SetNextKeyCombination(int index) {index_key_combination = index;};
  void SetActiveTrack(Track* p_track) {p_active_track = p_track;};
  void SetDeviceDetected() {midi_device_detected = true;};
  void SetDeviceDetectedNoReport() {midi_device_detected_reported = true;};
  void ChangeCurrentActiveKey(int index, unsigned char value) {p_current_active_keys[index] = value;};
  bool AllCurrentActiveKeysOff();
  bool CheckCommandKeyCombination();
  void StoreKeyCombination();
  void SetNumberToPlay(int value) {number_to_play = value;};

  void AddSongOffset(long delta) {song_offset += delta; if(song_offset < 0) song_offset = 0;};

         void  StartRecord();
         void  StartPlay();
         void  StopPlay(bool midi_reset = true, int notes_to_delete = 0);
         void  Pause();
         void  StepForward();
         void  StepBackward();
	 void  PlayNumberSequence(int);
         int   LoadMidiFile(ifstream*);
         void  GenerateMidiFile(ofstream*);

  bool MovePrevTrack();
  void MoveNextTrack();
  void MidiReset();

protected:

  bool LoadKeyCombination();
  int  GetKeyCommandNoNotes(int);
  void SaveKeyCombination();
  long ReadNextMidiByte(ifstream*, uint8 &);
 static void* ThreadPlayNumberSequence(void*); // only used internally for creating a thread
}; // class Sequencer

#endif
