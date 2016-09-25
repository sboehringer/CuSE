#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <curses.h>

using namespace std;

#include "sequencer.h"
#include "sequencer_globals.h"
#include "pool_container.h"
#include "track.h"

extern bool      popup_active;
extern Track     *p_first_displayed_track, *p_last_displayed_track;
extern short     cursor_column, track_no_display_digits;
extern Sequencer *p_seq;
extern short     active_status_line;

void RedrawTrackDisplay();

bool MidiChannelMatch(uint8 channel, uint16 channel_bitstring)
{
  for(int i = 0; i < 16; i++) {
    if((((channel_bitstring >> i) & 0x01) != 0) && (channel == i)) return true;
  } // for

  return false;
} // MidiChannelMatch

void* RecMidiEvents(void* p_midi_in)
{
  timeval         current_time;
  char            byte = 0, dummy;
  uint8           channel_byte, data_byte[2];
  uint8           midi_channel = 0, voice_message = 0;
  long            time_passed, index = -1;        // we do not expect a data byte
  midi_in_stream* midi_in = (midi_in_stream*)p_midi_in;
  bool            last_time_key_pressed = false;
  bool            command_matched;
  bool            system_exclusive_mode = false;

  do {

    midi_in->read(&byte, 1);

    // here we calculate the time which has passed since the
    // last event was either played or recorded. Being based on
    // the last event allows to change the speed at any time.
    
    // formerly, all event times had been based on the start
    // of the play

    gettimeofday(&current_time, NULL);

    // this is system time (real time) since the last event happened

    time_passed = (p_seq->GetPassedSystemTime(current_time) - p_seq->GetSystemTimePassedEvent());

    // this is song time (virtual time) since the last event happened

    time_passed = (long)(((double)time_passed)*(p_seq->GetQuartersPerMinute())/100.0);

    // now, the new event is based on the last event

    if(p_seq->GetPassedEvent())
      time_passed += p_seq->GetPassedEvent()->time;

    channel_byte = (uint8)byte;
    
    if(channel_byte >= 0xF0) {

      // I am a real-time message

	switch(channel_byte) {

	case 0xF0: // start of system exclusive message
	  system_exclusive_mode = true;
	  PrintError("DebugInfo (RecMidiEvents): 0xF0 START system exclusive");
	  break;

	case 0xF1: // undefined system message (not specified - valid??!?)
	  PrintError("ERROR (RecMidiEvents): undefined");
	  break;
	  
	case 0xF2:
	  PrintError("ERROR (RecMidiEvents): Song position messages are not yet handled");
	  break;
	  
	case 0xF3:
	  PrintError("ERROR (RecMidiEvents): Song select messages are not yet handled");
	  midi_in->read(&dummy, 1);
	  break;

	case 0xF4: // undefined system message (not specified - valid??!?)
	  PrintError("ERROR (RecMidiEvents): undefined");
	  break;

	case 0xF5: // undefined system message (not specified - valid??!?)
	  PrintError("ERROR (RecMidiEvents): undefined");	  
	  break;

	case 0xF6: // tune request
	  PrintError("ERROR (RecMidiEvents): tune request");	  
	  break;

	case 0xF7:

	  // end of system exclusive mode

	  // the channel_byte is a dummy only to meet the function's signature

	  p_seq->GetActiveTrack()->AddEvent(0xF7, &channel_byte, time_passed, p_seq->GetPassedSystemTime(current_time));
	  system_exclusive_mode = false;
	  PrintError("DebugInfo (RecMidiEvents): 0xF7 END system exclusive");
	  break;

	case 0xFE: // MIDI is alive message
	  p_seq->SetDeviceDetected();
	  break;

	default:
	  PrintError("ERROR (RecMidiEvents): Unknown system message: ");
	  break;

	}; // switch
    } // if
    else
      if(channel_byte & 128) {
	
	// I am a status byte
	
	if(system_exclusive_mode) {

	  // there was something wrong with the last system exclusive message
	  // normally it should be terminated by a 0xF7. If another status byte
	  // follows we can conclude that the sys. excl. message is over.
	  
	  // the channel_byte is a dummy only to meet the function's signature
	  
	  p_seq->GetActiveTrack()->AddEvent(0xF7, &channel_byte, time_passed, p_seq->GetPassedSystemTime(current_time));
	  system_exclusive_mode = false;	  
	  PrintError("ERROR: RecMidiEvents: Running sys. ex. message terminated with status-byte >= 0x80");
	} // if
	
      	voice_message = channel_byte;
	midi_channel = channel_byte & 0x0F;

	if(index > 0) {
	  PrintError("UNEXPECTED CONDITION: One or more data bytes were missing");
	} // if
	
	index = GetNoFollowingDataBytes(voice_message & ~0x0F);

	if((p_seq->GetRec()) && (index == 0)) {
          p_seq->GetActiveTrack()->AddEvent(channel_byte, data_byte, time_passed, p_seq->GetPassedSystemTime(current_time));
	  index = -1;
	  PrintError("UNEXPECTED CONDITION: A status byte with no data byte was encountered");
	} // if

	if(index > 2) {
	  PrintError("UNEXPECTED CONDITION: A status byte more that two data bytes was encountered");
	} // if

      } // if
      else {
	
	// I am a data byte

	if(system_exclusive_mode) {
	  PrintError("DebugInfo: RecMidiEvents: Sys. Ex. byte added");
	  p_seq->GetActiveTrack()->AddEvent(0xF0, &channel_byte, time_passed, p_seq->GetPassedSystemTime(current_time));
	} // if
	else {
	  
	  if(index <= 0) {
	    
	    // running status byte so we await the same number
	    // of data bytes as we did previously
	    
	    index = GetNoFollowingDataBytes(voice_message);
	  } // if
	  
	  if(index > 0) {
	    index--;
	    data_byte[index] = channel_byte;
	  } // if
	  
	  if(index == 0) {
	    
	    command_matched = false;
	    
	    // pitch bend was altered. if no other key as yet been pressed
	    // a change of the pitch bend wheel is interpreted as speed-change
	    // for the current play

	    if(((voice_message & ~0x0F) == 0xE0) && (p_seq->GetSpeedChangeMode())) {
	      double speed_change = (log((double)((long)data_byte[0]+(long)data_byte[1]*128L))/log(2)-6.0)*10;
	      p_seq->SetLiveSpeedChange(speed_change);
	    } // if

	    if(midi_event::IsKeyOn(voice_message, data_byte[0])) {
	      
	      // increase temporary channel volume by velocity of last note
	      
	      if(MidiChannelMatch(midi_channel, p_seq->GetActiveTrack()->GetMidiRecChannels()) == true)
		p_seq->GetActiveTrack()->AddLoudness(data_byte[0]);
	      
	      // this is a key-on event
	      
	      p_seq->ChangeCurrentActiveKey(data_byte[1], 1);
	      if(p_seq->CheckCommandKeyCombination()) command_matched = true;
	      
	      last_time_key_pressed = true;
	      p_seq->SetSpeedChangeMode(false); // after the first key the user may want to change
	                                        // the key's pitch and not the speed anymore
	    } // if
	    
	    if(midi_event::IsKeyOff(voice_message, data_byte[0])) {
	      
	      // this is a key-off event
	      
	      if(last_time_key_pressed) p_seq->StoreKeyCombination();
	      
	      p_seq->ChangeCurrentActiveKey(data_byte[1], 0);
	      last_time_key_pressed = false;
	    } // if
	    
	    if((!command_matched && p_seq->GetRec()) || (p_seq->GetRecStepByStep() && !(p_seq->GetPlay()))) {
	      
	      // store only if midi-channel mask matches
	      
	      if(MidiChannelMatch(midi_channel, p_seq->GetActiveTrack()->GetMidiRecChannels()) == true) {

		if(p_seq->GetRecStepByStep() && !(p_seq->GetPlay())) {

		  // record key in step-by-step mode

		  // a key-on event triggers the storage of...

		  if(midi_event::IsKeyOn(voice_message, data_byte[0])) {

		    // a new key...

		    p_seq->GetActiveTrack()->SortInEvent(voice_message, data_byte, p_seq->GetSongOffset());
		    
		    // together with its key-off event.

		    p_seq->GetActiveTrack()->SortInEvent(0x80, data_byte, p_seq->GetSongOffset() + 10000*60/(100*p_seq->GetRecStepByStepNoteLength()/4));
		  } // if

		  if(midi_event::IsKeyOff(voice_message, data_byte[0])) {

		    // A key-off event with no more keys pressed means
		    // that we want to advance one user-defined step in time

		    if(p_seq->AllCurrentActiveKeysOff()) {		    
		      p_seq->SetSongOffset(p_seq->GetSongOffset() + 10000*60/(100*p_seq->GetRecStepByStepAdvanceTime()/4));
		      p_seq->GetActiveTrack()->MarkNoteDisplayDirty();  refresh();
		      RedrawTrackDisplay();
		    } // if 
		  } // if

		} // if
		else {

		  // record key in realtime mode
		  PrintError("DebugInfo: RecMidiEvents: realtime midi-event added. status_byte =", false); PrintErrorInt(voice_message); PrintError("");

		  p_seq->GetActiveTrack()->AddEvent(voice_message, data_byte, time_passed, p_seq->GetPassedSystemTime(current_time));
		} // else
	      } // if
	      
	    } // if
	    
	    index = -1; // we do not expect a data byte
	    
	  } // if	  
	} // else
      } // else
  } while(!midi_in->eof());

  return NULL;
} // RecMidiEvents

void* PlayMidiEvents(void *p_midi_out)
{
  timeval         current_time_stamp;
  timespec        delta_sleep;
  long            current_song_time, time_to_sleep;
  long            precount;
  int             width, height;
  Track           *p_early_bird_track = NULL;
  midi_out_stream *midi_out = (midi_out_stream*)p_midi_out;
  midi_event      *p_event = NULL, *p_passed_event, *p_delete_event = NULL;
  bool            skip_next_note;
  uint8           work_byte;
  long            tmp_time;
  bool            system_exclusive_mode = false, beep_next;
  long            atomic_time_unit = (long)(10000*60/(100*2));

  if(p_seq->GetRec())
    precount = p_seq->GetQuartersPerBeat()*10000*60/100;
  else
    precount = 0;

  getmaxyx(stdscr, height, width);

  gettimeofday(&current_time_stamp, NULL);
  current_song_time = (long)(((double)p_seq->GetPassedSystemTime(current_time_stamp)+p_seq->GetSongOffset())) - precount;

  // there has been no prior event (right??!?)

  p_seq->SetSystemTimePassedEvent(NULL, precount*100/120);
  p_seq->beeper.Init();

  if((p_seq->GetPlay() && p_seq->GetPlayBeep()) || (p_seq->GetRec() && p_seq->GetRecBeep()))
    p_seq->beeper.Beep(Sequencer::Beeper::BEEP_FIRST);

  if(midi_out->good()) {

    while(p_seq->GetPlay()) {

      p_early_bird_track = NULL;
      time_to_sleep = LONG_MAX;
      
      Track* p_work_track = Track::GetFirst();
      
      while(p_work_track != NULL) {

	if((p_work_track->PeekNextEvent() != NULL)) {
	  
	  tmp_time = (long)((double)(p_work_track->PeekNextEvent()->time) - (double)current_song_time);
	  
	  if(time_to_sleep > tmp_time) {
	    time_to_sleep = tmp_time;
	    p_early_bird_track = p_work_track;
	  } // if
	} // if
	
	p_work_track = p_work_track->GetNext();
      } // while

      p_passed_event = p_event;

      if(p_early_bird_track != NULL) {

	p_event = p_early_bird_track->PeekNextEvent(); 

	if((p_seq->GetPlayStepByStep()) && ((p_event->time > p_seq->GetPlayStepByStepExitTime()) || 
					((p_seq->GetCurrentSongTime()+p_seq->GetSongOffset()) >= p_seq->GetPlayStepByStepExitTime()))) break;
      } // if

      // after time_of_passed_event and before p_event->time
      // a metronome beep may occure

      timeval ts_before_sleep, ts_after_sleep;
      long    time_of_passed_event, time_waited_for_beeping;
      long    true_sleeping_time;

      time_waited_for_beeping = 0;

      if(p_passed_event) time_of_passed_event = p_passed_event->time;
      else time_of_passed_event = 0;

      gettimeofday(&ts_before_sleep, NULL);
      true_sleeping_time = 0;

      while((time_to_sleep-true_sleeping_time) > 0) {
	
	long partial_sleep = atomic_time_unit - ((time_of_passed_event + true_sleeping_time) % atomic_time_unit);

	// hypothetical time after sleep. Usually we will sleep a bit
	// longer but this time is needed for the beeping

	long ideal_time_after_sleep = time_of_passed_event + true_sleeping_time + partial_sleep;

	if(partial_sleep > (time_to_sleep-true_sleeping_time)) {

	  // here, the end of the sleeping time coincides with the beginning of
	  // the next event. So we do not beep the metronome
	  
	  partial_sleep = (time_to_sleep-true_sleeping_time);

	  beep_next = false;
	} // if
	else {
	  
	  // the waiting time till the next event was longer than the next metronome
	  // beep. so we stop waiting to beep and continue to wait for the next event

	  beep_next = true;
	  time_waited_for_beeping += partial_sleep;
	} // else

	// transform time_to_sleep back from imaginary time
	// to realtime

	long real_time_sleep = (long)((double)partial_sleep*100.0/p_seq->GetQuartersPerMinute());

	delta_sleep.tv_sec = real_time_sleep/10000;
	delta_sleep.tv_nsec = (real_time_sleep % 10000)*100000;
	nanosleep(&delta_sleep, NULL);

	gettimeofday(&ts_after_sleep, NULL);

	true_sleeping_time = (long)((double)(p_seq->GetPassedSystemTime(ts_after_sleep) - 
					     p_seq->GetPassedSystemTime(ts_before_sleep))*p_seq->GetQuartersPerMinute()/100.0);

	p_seq->SetCurrentSongTime(current_song_time+true_sleeping_time);

	if(!p_seq->GetPlay()) goto exit_play; // user might have stoped music while we slept

	if(beep_next) {

	  if((p_seq->GetPlay() && p_seq->GetPlayBeep()) || (p_seq->GetRec() && p_seq->GetRecBeep())) {
	    if((ideal_time_after_sleep % (atomic_time_unit*2)) == 0) {
	      if(((ideal_time_after_sleep % (atomic_time_unit*p_seq->GetQuartersPerBeat()*2)) == 0) || (p_seq->GetCurrentSongTime() < 0))
		p_seq->beeper.Beep(Sequencer::Beeper::BEEP_FIRST);
	      else
		p_seq->beeper.Beep(Sequencer::Beeper::BEEP_REST);
	    } // if
	    else
	      p_seq->beeper.Beep(Sequencer::Beeper::BEEP_OFF);
	  } // if

	  p_seq->beeper.DisplayStatusLine(p_seq);
	  p_seq->beeper.DecLoudness();
	} // if
      } // while

      if(p_early_bird_track == NULL) break; // no more event to play

      // resynchronize with the next note to be played right now

      current_song_time = p_event->time;
      p_seq->SetCurrentSongTime(current_song_time);

      skip_next_note = false;

      if((p_seq->GetRec() == true) && (p_seq->GetRecOverwrite() == true) && 
	 (p_early_bird_track == p_seq->GetActiveTrack()) && 
	 (p_event->time >= p_seq->GetPunchIn()) && (p_event->time < p_seq->GetPunchOut())) {
	
	// we are in between the punch-in and punch-out time. in overwriting mode
	// the notes in between have to be deleted...

	// unless they are key-off events of notes being pressed before the
	// punch-in time. if these were deleted, they would sustain forever

	if(midi_event::IsKeyOff(p_event->channel_byte, p_event->data[1]) && 
	   (p_early_bird_track->GetOnKeys(p_event->data[0]) != 0)) {

	  // do not delete note

	  skip_next_note = false;
	} // if
        else {
          // remember event for deletion
          p_delete_event = p_seq->GetActiveTrack()->PeekNextEvent();
	  p_seq->GetActiveTrack()->DeleteUpcomingEvent();
	  p_seq->GetActiveTrack()->SetOnKeys(p_event->data[0], 0);

	  skip_next_note = true;
	} // else
      } // if

      // this may be a muted track

      if(p_early_bird_track->GetMute()) {

	// usually a muted track does not issue notes

	skip_next_note = true;

	// ... unless there are some key-off events left
	// while mute notes which are still continueing

	if(midi_event::IsKeyOff(p_event->channel_byte, p_event->data[1]) &&
	   (p_early_bird_track->GetOnKeys(p_event->data[0]) != 0)) {

	  // do not delete note
	  
	  skip_next_note = false;
	} // if

      } // if

      if(!skip_next_note) {
      
	// keep track of all notes being pressed at the moment
	
	if(midi_event::IsKeyOn(p_event->channel_byte, p_event->data[1])) {
	  
	  // store the key-on event to keep track of all currently pressed keys
	  
	  p_early_bird_track->SetOnKeys((int)(p_event->data[0]), 1);
	} // if
	
	if(midi_event::IsKeyOff(p_event->channel_byte, p_event->data[1])) {
	  
	  // store the key-on event to keep track of all currently pressed keys
	  
	  p_early_bird_track->SetOnKeys((int)(p_event->data[0]), 0);
	} // if

	// do not play events being too late for more than 20 msec
	
	//	if(time_to_sleep > -10*20) {
	if(true) {
	  
          work_byte = p_event->channel_byte;

	  // if the user has set a specific channel we copy this channel over the
	  // one that might still be included in the message originating from the
	  // recording.

	  // But take care, the SYSTEM MESSAGES 0xF? contain no midi-channel

	  if(work_byte < 0xF0) {
	    if(p_early_bird_track->GetMidiPlayChannel() >= 0)
	      work_byte = (work_byte & ~0x0F) | p_early_bird_track->GetMidiPlayChannel();
	  } // if
	  else {
	    
	    // we are dealing with a SYSTEM MESSAGE

	  } // else

	  // the system exclusive message 0xF0 must only be transmitted
	  // once. Then the data bytes follow. We only internally tag each
	  // event with a 0xF0 to know that we are in a sys. excl. message 
	  // stream.

	  if(work_byte == 0xF7) system_exclusive_mode = false;

	  if(system_exclusive_mode == false)
	    midi_out->put((char)work_byte);

	  if(work_byte == 0xF0) system_exclusive_mode = true;
	  
	  for(int i = 0; i < GetNoFollowingDataBytes(p_event->channel_byte); i++)
	    midi_out->put((char)p_event->data[i]);
	  
	  midi_out->flush();
	  
	  // increase temporary channel volume by velocity of last note
	  
	  p_early_bird_track->AddLoudness(p_event->data[1]);
	} // if
      } // else

      // event has been play and is history now
      // the time of newly inserted events will be
      // based on this one

      gettimeofday(&current_time_stamp, NULL);
      p_seq->SetSystemTimePassedEvent(p_event, p_seq->GetPassedSystemTime(current_time_stamp));
      
      p_early_bird_track->JumpNextEvent();
      
      // finally delete event

      p_seq->GetPoolContainer()->ReturnUnusedEvent(p_delete_event);
      p_delete_event = NULL;
      
      p_early_bird_track->MarkNoteDisplayDirty();
    } // while

  exit_play:

    p_seq->SetCurrentSongTime(current_song_time);
    p_seq->beeper.Beep(Sequencer::Beeper::BEEP_OFF);

   // clear all volume-meters and set dirty flag for redisplay

    for(Track* p_work_track = Track::GetFirst(); p_work_track != NULL; 
	 p_work_track = p_work_track->GetNext()) {
      if((!p_seq->GetPlayStepByStep()) && (p_seq->GetRec() == false))
	p_work_track->SetLoudness(0);

    } // for

    if(!p_seq->GetRec()) SetStatusText("Sequencer stopped");
    p_seq->SetPlay(false);

    if((p_early_bird_track == NULL) && (!p_seq->GetRec())) p_seq->StopPlay(false); // regular end, so rewind

    if(p_seq->GetPlayStepByStep()) p_seq->SetPlayStepByStep(false);
  } // if
  else {
    PrintError("Could not open midi device for output (device)");
    PrintError("exiting...");
    exit(-1);
  } // else

  return NULL;
} // PlayMidiEvents

void* PlayMetronome(void *p_midi_out)
{
  timeval          current_time; // temporary
  midi_out_stream* midi_out = (midi_out_stream*)p_midi_out;
  long             current_song_time;
  long             atomic_time_unit;

  Track *p_last_active_track;
  int   blink;

  blink = 1;
  p_last_active_track = p_seq->GetActiveTrack();

  atomic_time_unit    = (long)(100*60/1);
  current_song_time   = 0;

  if(midi_out->good()) {

    gettimeofday(&current_time, NULL); // temporary
      
    current_song_time = p_seq->GetPassedSystemTime(current_time);

    while(!p_seq->GetESCMetronome()) {
      

      // update display 32 times per bar
      
      long time_to_sleep;
      
      time_to_sleep = atomic_time_unit - (current_song_time % atomic_time_unit);
      current_song_time += time_to_sleep;    
      
      if(p_seq->GetQuartersPerMinute() > 0)
	time_to_sleep = (long)(time_to_sleep*100.0/p_seq->GetQuartersPerMinute());
      
      timespec delta_sleep;
      
      delta_sleep.tv_sec = time_to_sleep/10000;
      delta_sleep.tv_nsec = (time_to_sleep % 10000)*100000;
      nanosleep(&delta_sleep, NULL);

      p_seq->beeper.DisplayStatusLine(p_seq);

      p_seq->beeper.DecLoudness();  
          
      if(!popup_active) refresh();
    } // while
    
    //    if(p_seq->beeper.Playing()) p_seq->beeper.Beep(Sequencer::Beeper::BEEP_OFF);
    
  } // if
  else {
    PrintError("Could not open midi device for output (device)");
    PrintError("exiting...");
    exit(-1);
  } // else

  return NULL;
} // PlayMetronome

