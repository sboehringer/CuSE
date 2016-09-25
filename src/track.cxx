#include <netinet/in.h>
#include <curses.h>

#include "sequencer_globals.h"
#include "pool_container.h"
#include "track.h"

extern const int  LENGTH_DISPLAY_TEXT;

Track* Track::p_first_track = NULL;
Track* Track::p_last_track  = NULL;

Track::~Track()
{
  DeleteAllEvents();
} // Track::~Track

Track* Track::GetNewTrack(Sequencer* p_sequencer, uint16 rec_bitstring)
{
  Track* p_new_track = new Track();

  // each bit determines if the corresponding midi
  // channel will be recorded by this track or not

  p_new_track->p_seq                  = p_sequencer;
  p_new_track->number                 = 1;
  p_new_track->loudness               = 0;
  p_new_track->mute                   = false;
  p_new_track->midi_rec_channels      = rec_bitstring;
  p_new_track->midi_play_channel      = 0;
  p_new_track->p_first_midi_event     = NULL;
  p_new_track->p_current_midi_event   = NULL;
  p_new_track->p_last_midi_event      = NULL;
  p_new_track->last_current_song_time = -1;

  p_new_track->p_next        = NULL;
  p_new_track->p_prev        = p_last_track;

  for(int note = 0; note < 128; note++)
    p_new_track->SetOnKeys(note, 0); // no note pressed so far

  if(p_first_track == NULL) p_first_track = p_new_track;
  if(p_last_track != NULL) {
    p_last_track->p_next = p_new_track;
    p_new_track->number = p_last_track->number + 1;
    p_new_track->midi_play_channel = 0;
  } // if

  p_new_track->p_name = new char [20];
  sprintf(p_new_track->p_name, "track %d", p_new_track->number);

  p_last_track = p_new_track;

  return p_new_track;
} // Track::GetNewTrack

void Track::DisplayNextNotes(int x_start, int display_length, long current_song_time, double quarters_per_minute, long resolve_quarters, bool highlight_first_note)
{
  char notation[2048], note_string[32];
  long atomic_time_unit = (long)(10000*60/(100*resolve_quarters));
  long x_pos, sub_x_pos, time;
  bool first_time_slot;

  // atomic time unit is a fraction of the time a quarter note lasts
  // (while the fractional part is defined by resolve_quarters)

  x_pos = x_start;

  // clear lines

  strcpy(notation, "");

  for(int i = 0; i < display_length; i++)
    strcat(notation, " ");
  
  mvaddstr(current_display_line, x_pos, notation);      
  mvaddstr(current_display_line+1, x_pos, notation);      

  midi_event *p_work_midi_event = PeekNextEvent();

  // step back half a time unit because quantization means that notes half a 
  // time unit after AND before are displayed. This is also true for the
  // few notes that have already been played back
  
  if(p_work_midi_event != NULL) {

    while((p_work_midi_event->p_prev != NULL) && 
	  (p_work_midi_event->p_prev->time >= current_song_time - atomic_time_unit/2)) {
      p_work_midi_event = p_work_midi_event->p_prev;
    } // while
  } // if
  
  first_time_slot = false;
  if(highlight_first_note) first_time_slot = true;
  
  for(int note = 0; x_pos < display_length+x_start; note++) {

    time = current_song_time - atomic_time_unit/2 + 
           (long)((long)(10000*60*note/(100*resolve_quarters)));

    sub_x_pos = 0;

    while((p_work_midi_event != NULL) &&
	  (p_work_midi_event->time < time)) {

      // so far, only key on events are displayed

      if(midi_event::IsKeyOn(p_work_midi_event->channel_byte, p_work_midi_event->data[1])) {

	// this is a key on event (data[1] == 0x00 would be key-off)

	MidiNoteToString(p_work_midi_event->data[0], note_string);
	strcpy(notation, note_string);
	strcat(notation, " ");

	if(sub_x_pos == 0) {

	  // display first note in current line

	  if(first_time_slot) attrset(A_REVERSE);

	  mvaddstr(current_display_line, x_pos, notation);
	  attrset(A_NORMAL);

	  sub_x_pos = x_pos;
	} // if
	else {

	  // display following notes for same time slot in next line

	  if(first_time_slot) {
	    SetSeqColor(SCOL_INVERSE);
	  } // if

	  if(sub_x_pos > x_pos) mvaddstr(current_display_line+1, sub_x_pos-1, ">");
	  mvaddstr(current_display_line+1, sub_x_pos, notation);

	  SetSeqColor(SCOL_NORMAL);

	  sub_x_pos += strlen(notation);
	} // else

	if(x_pos >= x_start+display_length-4) goto exit_display;
      } // if
      
      p_work_midi_event = p_work_midi_event->p_next;

      if(x_pos > display_length+x_start) break;
    } // while

    if((sub_x_pos == 0)) {
      if(first_time_slot)
	SetSeqColor(SCOL_INVERSE);
      mvaddstr(current_display_line, x_pos, ".");
      SetSeqColor(SCOL_NORMAL);
    } // if
      
    x_pos += 5;
    first_time_slot = false;
  } // for

 exit_display:

  // display loudness

  DisplayVolumeMeter();

  return;
} // Track::DisplayNextNotes

void Track::SetName(char *name)
{
  long length_name;
 
  if(p_name) delete p_name;
  
  length_name = strlen(name)+1;
  if(length_name > 1024) length_name = 1024;

  p_name = new char[length_name];

  strncpy(p_name, name, length_name);
} // Track::SetName

void Track::SaveMidiFile(ofstream* midi_file, bool first_track)
{
  uint32     size_track, file_byte_ordered;
  uint32     delta_time, abs_last_time;
  long       length_name;
  streampos  size_position, temp_pos;
  midi_event *p_work_event;
  uint8      part_time[5];
  int        index;
  bool       system_exclusive_mode = false;

  // write track header

  PrintError("Save MIDI file");

  midi_file->write("MTrk", 4);
  
  size_track = 0;
  size_position = midi_file->tellp();
  midi_file->write((char*)&size_track, sizeof(size_track));

  // write track name

  midi_file->put(0x00); // no time to wait for meta events
  midi_file->put(0xFF); // meta event following
  midi_file->put(0x03); // meta event = track filename

  size_track += 3;

  length_name = strlen(p_name)+1; // with terminating /0
  size_track += WriteVariableLength(midi_file, length_name);
  midi_file->write((char*)p_name, length_name);

  size_track += length_name;

  p_work_event = GetFirstMidiEventPtr();

  abs_last_time = 0;

  // write timing info into first track

  if(first_track) {

    // tempo definition

    midi_file->put(0x00); // no time to wait for meta events
    midi_file->put(0xFF);
    midi_file->put(0x51);
    midi_file->put(0x03);

    long time_for_one_quarter = (long)((10000*60*100/p_seq->GetQuartersPerMinute())); // in microseconds (10^(-6)

    midi_file->put((time_for_one_quarter >> 16) & 0xFF);
    midi_file->put((time_for_one_quarter >> 8) & 0xFF);
    midi_file->put((time_for_one_quarter >> 0) & 0xFF);

    size_track += 7;
  } // if

  while(p_work_event != NULL) {

    // idle time units before playing the event

    delta_time = p_work_event->time - abs_last_time;
    index      = 0;

    if(!system_exclusive_mode) {
      PrintError("delta time:", false); PrintErrorInt(delta_time);

      abs_last_time = p_work_event->time;
      
      while(true) {
	
	// most significant bit of byte has to be set
	// in midi files to indicate that another byte
	// will follow
	
	part_time[index] = (uint8)(delta_time) | 128;
	delta_time >>= 7;
	index++;
	if(delta_time == 0) break;
      } // while

      part_time[0] &= (255-128); // the least sig. byte has no follower

      for(int i = index-1; i >= 0; i--) {
	midi_file->write((char*)(&part_time[i]), 1);
	size_track++;
      } // for
    } // if

    // internally (within this application) the system exclusive message
    // in coded in one byte chunk each of which has its own 0xF0 channel byte.
    // In the file, however, only a single 0xF0 is preceeding the sys. excl.
    // message and a 0xF7 is terminating (or another channel byte with the 
    // most sig. bit set).

    if(!((system_exclusive_mode == true) && (p_work_event->channel_byte == 0xF0))) {
      midi_file->write((char*)&(p_work_event->channel_byte), 1);
      PrintError("channel byte:", false); PrintErrorHex(p_work_event->channel_byte);
      size_track++;
    } // if
    
    if(p_work_event->channel_byte == 0xF0) {
      system_exclusive_mode = true;
    } // if
    else 
      if(p_work_event->channel_byte & 128) system_exclusive_mode = false;

    for(int i = 0; i < GetNoFollowingDataBytes(p_work_event->channel_byte); i++) {
      midi_file->write((char*)&(p_work_event->data[i]), 1);
      PrintError("data byte:", false); PrintErrorHex(p_work_event->data[i]);
      size_track++;
    } // for
      
    p_work_event = p_work_event->p_next;
  } // while

  // write end-of-channel meta event

  midi_file->put(0x00); // no time to wait for meta events
  midi_file->put(0xFF); // meta event following
  midi_file->put(0x2F); // meta event: 'end of track'
  midi_file->put(0x00); // needed for 'end of track'?

  size_track += 4;

  temp_pos = midi_file->tellp();
  midi_file->seekp(size_position);
  file_byte_ordered = htonl(size_track);
  midi_file->write((char*)&file_byte_ordered, 4);
  midi_file->seekp(temp_pos);
} // Track::SaveMidiFile

void Track::GetChannelNumber(uint16 channel_bits, char* text)
{
  uint16 no_bits, bit_set;

  no_bits = 0;
  bit_set = 0;

  for(int i = 0; i < 16; i++) {
    if((channel_bits & 1) != 0) {
      no_bits++;
      bit_set = i;
    } // if
    channel_bits >>= 1;
  }

  if(no_bits == 16)
    sprintf(text, " ALL ");
  else
    if(no_bits > 1)
      sprintf(text, " MUL ");
    else
      if(no_bits == 1)
	sprintf(text, " %3d ", bit_set+1);
      else
	sprintf(text, " NON ");
} // Track::GetChannelNumber

void Track::DisplayStatus(long current_song_time, short track_no_display_digits, SeqColor highlight_attribute, short cursor_column, int blink)
{
  char   display_text[LENGTH_DISPLAY_TEXT], format_string[256];
  uint16 pos_x;
  int    display_height, display_width;
  bool   highlight_next_note;

  getmaxyx(stdscr, display_height, display_width);

  pos_x = 2;

  if(blink != 0) blink_state = blink;

  sprintf(format_string, "#%%%ud", track_no_display_digits);
  snprintf(display_text, LENGTH_DISPLAY_TEXT, format_string, GetNumber());

  if(blink_state == 1) SetSeqColor(SCOL_INVERSE);
  else SetSeqColor(SCOL_NORMAL);

  mvaddstr(current_display_line, pos_x, display_text);  
  pos_x += strlen(display_text)+1;

  snprintf(display_text, MAX_CHAR_CHAN_NAME_DISPLAY+1, "%s                            ", GetName());
  
  //  strcpy(&(display_text[13]), "]");
  
  SetSeqColor(SCOL_NORMAL);
  mvaddstr(current_display_line, pos_x++, "[");  

  if(cursor_column == 1) SetSeqColor(highlight_attribute);
  if((highlight_attribute != SCOL_INVERSE) || (cursor_column != 1)) SetSeqColor(SCOL_VALUE);
  
  mvaddstr(current_display_line, pos_x, display_text);  
  pos_x += strlen(display_text);
  
  SetSeqColor(SCOL_NORMAL);

  mvaddstr(current_display_line, pos_x++, "]");  
  
  // display record channels
  
  GetChannelNumber(GetMidiRecChannels(), display_text);
  
  if(cursor_column == 2) SetSeqColor(highlight_attribute);
  if((highlight_attribute != SCOL_INVERSE) || (cursor_column != 2)) SetSeqColor(SCOL_VALUE);
  
  mvaddstr(current_display_line, 21, display_text);  
  pos_x += strlen(display_text);
  
  SetSeqColor(SCOL_NORMAL);

  mvaddstr(current_display_line, 21, "[");  
  mvaddstr(current_display_line, 25, "]");  
  
  // display play channels
  
  if(GetMidiPlayChannel() >= 0)
    sprintf(display_text, " %3d ", GetMidiPlayChannel()+1);
  else
    sprintf(display_text, " MUL ");

  if(cursor_column == 3) SetSeqColor(highlight_attribute);
  if((highlight_attribute != SCOL_INVERSE) || (cursor_column != 3)) SetSeqColor(SCOL_VALUE);
  
  mvaddstr(current_display_line, 27, display_text);
  pos_x += strlen(display_text);
  
  SetSeqColor(SCOL_NORMAL);

  mvaddstr(current_display_line, 27, "[");  
  mvaddstr(current_display_line, 31, "]");  
  
  // display mute status

  if(cursor_column == 4) SetSeqColor(highlight_attribute);
  if((highlight_attribute != SCOL_INVERSE) || (cursor_column != 4)) SetSeqColor(SCOL_VALUE);

  if(GetMute())
    mvaddstr(current_display_line, 34, "MUT");
  else
    mvaddstr(current_display_line, 34, "PLY");
  
  SetSeqColor(SCOL_NORMAL);

  mvaddstr(current_display_line, 33, "[");  
  mvaddstr(current_display_line, 37, "]");  

  // display notes
  
  if(note_display_dirty_flag) {
    if((cursor_column == 5) && (p_seq->GetActiveTrack() == this)) highlight_next_note = true;
    else highlight_next_note = false;

    DisplayNextNotes(40, display_width - 40, current_song_time, p_seq->GetQuartersPerMinute(), p_seq->GetResolveQuarters(), highlight_next_note);

    last_current_song_time = p_seq->GetCurrentSongTime();
    note_display_dirty_flag = false;
  } // if
  
  // display loudness

  DisplayVolumeMeter();
} // Track::DisplayStatus

void Track::DisplayVolumeMeter()
{
  char display_text[31];
  int  max_vol = loudness/15;

  if(max_vol > 30) max_vol = 30;

  strcpy(display_text, "");

  for(int i = 0; i < max_vol; i++)
    strcat(display_text, "-");

  mvaddstr(current_display_line+1, 2, "                               ");
  mvaddstr(current_display_line+1, 2, display_text);
} // Track::DisplayVolumeMeter

void Track::MidiReset(midi_out_stream& midi_out)
{
  uint8 byte;

  byte = 0xB0 | midi_play_channel;
  midi_out.put(byte);
  
  byte = 120; // all sound off
  midi_out.put(byte);
  
  byte = 0;
  midi_out.put(byte);
  
  midi_out.flush();
  
  byte = 0xB0 | midi_play_channel;
  midi_out.put(byte);
  
  byte = 121; // all controllers off
  midi_out.put(byte);
  
  byte = 0;
  midi_out.put(byte);
  
  midi_out.flush();
  
  byte = 0xB0 | midi_play_channel;
  midi_out.put(byte);
  
  byte = 123; // all notes off
  midi_out.put(byte);
  
  byte = 0;
  midi_out.put(byte);
  
  midi_out.flush();
  
  for(uint8 note = 0; note < 128; note++)
    on_keys[note] = 0;
  
  // sustain pedal off
  
  byte = 176 | midi_play_channel;
  midi_out.put(byte);
  
  byte = 64;
  midi_out.put(byte);
  
  byte = 0;
  midi_out.put(byte);
  
  midi_out.flush();

} // Track::MidiReset

void Track::AddEvent(uint8 channel_byte, uint8 data_bytes[], long time_passed, long current_system_time)
{
  midi_event *p_mevent = p_seq->GetPoolContainer()->GetEventFromPool();
  int no_data_bytes    = GetNoFollowingDataBytes(channel_byte);

  p_mevent->channel_byte = channel_byte;

  for(int i = 0; i <= no_data_bytes; i++)
    p_mevent->data[i] = data_bytes[no_data_bytes-i-1];

  p_mevent->time = time_passed;
  p_mevent->p_next = NULL;

  // remember this event as the latest in the list. All succeeding
  // time stamps will be based on this time stamp
  
  p_seq->SetSystemTimePassedEvent(p_mevent, current_system_time);

  if(p_first_midi_event == NULL) {
    p_first_midi_event = p_mevent;

    // if no first_event was known before there
    // can also be not (younger) last_midi_event

    p_last_midi_event = NULL;
  } // if

  if(p_first_midi_event == p_current_midi_event) p_first_midi_event = p_mevent;

  // current event is the next event to be played

  if(p_current_midi_event != NULL) {
    p_mevent->p_prev = p_current_midi_event->p_prev;
    if(p_current_midi_event->p_prev != NULL)
      p_current_midi_event->p_prev->p_next = p_mevent;
    p_current_midi_event->p_prev = p_mevent;
    p_mevent->p_next = p_current_midi_event;

    return;
  } // if
  
  // last event is more meant to be the lastest being written

  if(p_last_midi_event != NULL)
    p_last_midi_event->p_next = p_mevent;    

  p_mevent->p_prev  = p_last_midi_event;
  p_last_midi_event = p_mevent;

  return;
} // Track::AddEvent

void Track::SortInEvent(uint8 channel_byte, uint8 data_byte[], long time)
{
  if(p_current_midi_event == NULL) p_current_midi_event = p_first_midi_event;

  while(p_current_midi_event != NULL) {

    if(p_current_midi_event->time < time) p_current_midi_event = p_current_midi_event->p_next;
    else {

      // once we reach this place we have advanced far enough
      // in the list of event only only need to step back or escape

      if(p_current_midi_event->p_prev == NULL) break;
      
      if(p_current_midi_event->p_prev->time > time) p_current_midi_event = p_current_midi_event->p_prev;
      else break;
    } // if
    
  } // while

  AddEvent(channel_byte, data_byte, time, 0);
} // Track::SortInEvent

void Track::Transpose(long start, long end, char delta_notes)
{
  midi_event *p_work_event = GetFirstMidiEventPtr();

  while(p_work_event != NULL) {

    if(p_work_event->time > end) break;

    if(p_work_event->time >= start) {
      
      if((midi_event::IsKeyOn(p_work_event->channel_byte, p_work_event->data[1])) || 
	 (midi_event::IsKeyOff(p_work_event->channel_byte, p_work_event->data[1]))) {
	p_work_event->data[0] += delta_notes;
      } // if
    } // if

    p_work_event = p_work_event->p_next;
  } // while
} // Track::Transpose

void Track::TimeShift(long start, long end, long delta_time)
{
  midi_event *p_work_event = GetFirstMidiEventPtr();

  while(p_work_event != NULL) {

    if(p_work_event->time > end) break;

    if(p_work_event->time >= start)    
	p_work_event->time += delta_time;

    p_work_event = p_work_event->p_next;
  } // while
} // Track::TimeShift

void Track::DeleteUpcomingEvent()
{
  if(p_current_midi_event != NULL) {
    if(p_current_midi_event->p_prev != NULL) {
      p_current_midi_event->p_prev->p_next = p_current_midi_event->p_next;
    } // if

    if(p_current_midi_event->p_next != NULL) {
      p_current_midi_event->p_next->p_prev = p_current_midi_event->p_prev;
    } // if

    if(p_first_midi_event == p_current_midi_event)
      p_first_midi_event = p_current_midi_event->p_next;

    if(p_last_midi_event == p_current_midi_event)
      p_last_midi_event = p_current_midi_event->p_prev;

  } // if
} // Track::DeleteUpcomingEvent

void Track::DeleteLastEnduringNotes()
{
  midi_event *p_prev_event;

  if(p_current_midi_event == NULL)
    p_current_midi_event = GetLastMidiEventPtr();

  // if we are not at the end (there is something comming
  // after p_current_midi_event), we step one back because
  // p_current_midi_event addresses the next note after
  // the last to be deleted
  
  if(p_current_midi_event != GetLastMidiEventPtr())
    p_current_midi_event = p_current_midi_event->p_prev;
  
  while(p_current_midi_event) {

    // each time we encounter a key-on event we know that we have
    // deleted another note. we stop in case of a key-off event

    if(midi_event::IsKeyOff(p_current_midi_event->channel_byte, p_current_midi_event->data[1])) break;

    DeleteUpcomingEvent();
    p_prev_event = p_current_midi_event->p_prev;
    p_seq->GetPoolContainer()->ReturnUnusedEvent(p_current_midi_event);
    p_current_midi_event = p_prev_event;
  } // while

  if(p_current_midi_event)
    p_current_midi_event = p_current_midi_event->p_next;
} // Track::DeleteLastEnduringNotes

void Track::DeleteAllEvents()
{
  midi_event* p_next_event;

  p_current_midi_event = GetFirstMidiEventPtr();

  while(p_current_midi_event != NULL) {
    DeleteUpcomingEvent();
    p_next_event = p_current_midi_event->p_next;
    p_seq->GetPoolContainer()->ReturnUnusedEvent(p_current_midi_event);
    p_current_midi_event = p_next_event;
  } // while
  
  note_display_dirty_flag = true;
} // Track::DeleteAllEvents

void Track::JumpToStart()
{
  for(int note = 0; note < 128; note++)
    on_keys[note] = 0; // no note pressed so far

  p_current_midi_event = p_first_midi_event; 
  last_current_song_time = -1;
} // Track::JumpToStart

void Track::SetOnKeys(int index, uint8 value) 
{
  on_keys[index] = value;
} // Track::SetOnKeys

void Track::Quantize(long interval_length, long start_time, long end_time)
{
  midi_event *p_work_event;
  long       delta[128], delta_time;
  bool       key_is_on[128], time_correctable_event; // not every event must be manipulated
  int        note;

  if(end_time == 0) end_time = LONG_MAX;

  for(note = 0; note < 128; note++) {
    key_is_on[note] = false;
    delta[note] = 0;
  } // for

  p_work_event = p_first_midi_event;

  while(p_work_event != NULL) {

    time_correctable_event = false;
    
    if(midi_event::IsKeyOn(p_work_event->channel_byte, p_work_event->data[1])) {
    
      time_correctable_event = true;

      note = p_work_event->data[0];

      if(key_is_on[note] == false) {

	// key-on event
	
	delta_time = p_work_event->time % interval_length;
	
	if(delta_time < interval_length/2) delta[note] = -delta_time;
	else delta[note] = (interval_length - delta_time);
	
	key_is_on[note] = true;
      } // if
    } // if

    if((p_work_event->channel_byte & ~0x0F) == 0xA0) {

      time_correctable_event = true;
      note = p_work_event->data[0];
    } // if

    if(midi_event::IsKeyOff(p_work_event->channel_byte, p_work_event->data[1])) {
  
      time_correctable_event = true;

      note = p_work_event->data[0];

      // key-off event
  
      key_is_on[note] = false;

    } // if

    if((time_correctable_event) && (p_work_event->time > start_time) && (p_work_event->time < end_time)) {

      // there was a preceeding key-on event which defines all
      // following child-events until the key is released

      p_work_event->time += delta[note];
    } // if    

    p_work_event = p_work_event->p_next;
  } // while

  SortList();

} // Track::Quantize

void Track::SortList()
{
  bool       swaped;
  midi_event *p_event_1, *p_event_2, *p_event_3, *p_event_4;
  midi_event *p_work_event;

  // swap order or event with successor earlier than predecessor
  // in a bubble-sort like fashion until no more need to sort

  do {
    swaped = false;

    p_work_event = p_first_midi_event;
    
    while(p_work_event != NULL) {
      if(p_work_event->p_next != NULL) {
	if(p_work_event->time > p_work_event->p_next->time) {
	   
	  p_event_1 = p_work_event->p_prev;
	  p_event_2 = p_work_event;
	  p_event_3 = p_work_event->p_next;
	  p_event_4 = p_work_event->p_next->p_next;
	  
	  if(p_event_1 != NULL)
	    p_event_1->p_next = p_event_3;
	  
	  if(p_event_2 != NULL) {
	    p_event_2->p_next = p_event_4;
	    p_event_2->p_prev = p_event_3;
	  } // if
	  
	  if(p_event_3 != NULL) {
	    p_event_3->p_next = p_event_2;
	    p_event_3->p_prev = p_event_1;
	  } // if
	  
	  if(p_event_4 != NULL)
	    p_event_4->p_prev = p_event_2;
	  
	  swaped = true;
	  
	} // if
      } // if
      
      p_work_event = p_work_event->p_next;
    }
  } while(swaped == true);
} // Track::SortList

void Track::MarkAllTracksDirty()
{
  Track *p_work_track = GetFirst();

  while(p_work_track != NULL) {
    p_work_track->MarkNoteDisplayDirty();
    p_work_track = p_work_track->GetNext();
  } // while
} // Track::MarkAllTracksDirty

int Track::GetContainedMidiChannel()
{
  midi_event *p_work_event = GetFirstMidiEventPtr();
  int        channel_used  = -2;

  while(p_work_event) {
    
    if((p_work_event->channel_byte & 128) != 0) {

      // we consider a channel-byte, not a meta event

      if(channel_used == -2) channel_used = p_work_event->channel_byte & 0x0F;
      else {
	if(channel_used != (p_work_event->channel_byte & 0x0F)) return -1; // there are multiple channels
      } // else
    } // if

    p_work_event = p_work_event->p_next;
  } // while

  return channel_used;
} // Track::GetContainedMidiChannel

void Track::CopyTrackTo(Track* p_dest_track)
{
  midi_event* p_event = GetFirstMidiEventPtr();
  uint8       my_data[2];

  while(p_event) {
    my_data[0] = p_event->data[1];
    my_data[1] = p_event->data[0];

    p_dest_track->AddEvent(p_event->channel_byte, my_data, p_event->time, 0);

    p_event = p_event->p_next;
  } // while

  SetMidiRecChannels(p_dest_track->GetMidiRecChannels());
  SetMidiPlayChannel(p_dest_track->GetMidiPlayChannel());
  SetMute(p_dest_track->GetMute());
  SetName(p_dest_track->GetName());

  p_dest_track->JumpToStart();
  p_dest_track->MarkNoteDisplayDirty();
} // Track::CopyTrackTo

Track* Track::GetFirstFreeTrack(Sequencer* p_seq)
{
  Track* p_work_track = GetFirst();

  while(p_work_track != NULL) {
    if(p_work_track->GetFirstMidiEventPtr() == NULL) break;
    p_work_track = p_work_track->GetNext();
  } // while

  if(p_work_track == NULL) p_work_track = GetNewTrack(p_seq, 0);

  return p_work_track;
} // Track::GetFirstFreeTrack

void Track::EraseBeats(long start, long end)
{
  midi_event *p_next_event;

  p_current_midi_event = GetFirstMidiEventPtr();

  while((p_current_midi_event != NULL) && (p_current_midi_event->time <= end)) {
    p_next_event = p_current_midi_event->p_next;
    if(p_current_midi_event->time >= start) DeleteUpcomingEvent();

    p_current_midi_event = p_next_event;
  } // while
} // Track::EraseBeats

void Track::CopyBeats(long dest, long start, long end)
{
  uint8 data_copy[2];

  // set current-pointer to dest-time

  p_current_midi_event = GetFirstMidiEventPtr();

  while((p_current_midi_event != NULL) && (p_current_midi_event->time < dest))
    p_current_midi_event = p_current_midi_event->p_next;

  // set source-pointer the start-time

  midi_event *p_source_event = GetFirstMidiEventPtr();

  while((p_source_event != NULL) && (p_source_event->time < start))
    p_source_event = p_source_event->p_next;

  // copy events

  while((p_source_event != NULL) && (p_source_event->time <= end)) {

    data_copy[0] = p_source_event->data[1];
    data_copy[1] = p_source_event->data[0];
    SortInEvent(p_source_event->channel_byte, data_copy, p_source_event->time+dest-start);

    p_source_event = p_source_event->p_next;
  } // while

} // Track::CopyBeats
