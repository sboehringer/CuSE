#include "sequencer.h"
#include <stdint.h>

extern Track     *p_first_displayed_track, *p_last_displayed_track;

extern bool      popup_active;
extern Track     *p_first_displayed_track, *p_last_displayed_track;
extern short     cursor_column, track_no_display_digits;
extern Sequencer *p_seq;
extern short     active_status_line;

Sequencer::Beeper::Beeper()
{
  metronome_channel  = 0;
  channel_byte       = 0x90 | metronome_channel;
  metronome_key1     = 111;
  metronome_key2     = 105;
  last_metronome_key = -1; // no metronome played yet
} // Sequencer::Beeper::Beep

void Sequencer::Beeper::Init()
{
  blink = 1;
  p_last_active_track = p_seq->GetActiveTrack();
  last_metronome_key = -1; // no metronome played yet  
} // Sequencer::Beeper::Init

void Sequencer::Beeper::Beep(BeepState beep)
{
  if(last_metronome_key != -1) beep = BEEP_OFF;

  switch(beep) {
  case BEEP_FIRST:
    p_seq->midi_out.put(channel_byte);
    p_seq->midi_out.put(metronome_key1);
    p_seq->midi_out.put(64);
    p_seq->midi_out.flush();
    
    last_metronome_key = metronome_key1;	      
    break;
    
  case BEEP_REST:
    p_seq->midi_out.put(channel_byte);
    p_seq->midi_out.put(metronome_key2);
    p_seq->midi_out.put(64);
    p_seq->midi_out.flush();
    
    last_metronome_key = metronome_key2;
    break;
    
  case BEEP_OFF:
    p_seq->midi_out.put(channel_byte);
    p_seq->midi_out.put(last_metronome_key);
    p_seq->midi_out.put(0);
    p_seq->midi_out.flush();
    last_metronome_key = -1;
    break;
    
  }; // switch    

  // this is the right place to decrease to loudness of all tracks temporarily

  DecLoudness();  
} // Sequencer::Beeper::Beep

void Sequencer::Beeper::DecLoudness()
{
  Track *p_loudness_track = Track::GetFirst();
  long  tmp_loudness;  

  while(p_loudness_track) {
    tmp_loudness = p_loudness_track->GetLoudness();
    tmp_loudness = (long)(tmp_loudness - 250);
    if(tmp_loudness < 0) tmp_loudness = 0;
    p_loudness_track->SetLoudness(tmp_loudness);
    p_loudness_track = p_loudness_track->GetNext();
  } // while
} // Sequencer::Beeper::DecLoudness

void Sequencer::Beeper::DisplayStatusLine(Sequencer* p_sequencer)
{
  if(!popup_active) {
    
    // display location in song in bars and quarters
    
    long quarters;
    
    if((p_sequencer->GetPlay()) || (p_sequencer->GetRec()))
      quarters = (long)(((double)p_sequencer->current_song_time) / (100*60.0));
    else 
      quarters = (long)(((double)p_sequencer->GetSongOffset() * 100) / (10000.0*60.0));
    
    char display_text[20];

    if(p_sequencer->GetQuartersPerBeat() != 0.0)
      sprintf(display_text, "%03ld:%ld", quarters/p_sequencer->GetQuartersPerBeat(), quarters % p_sequencer->GetQuartersPerBeat() + 1);
    else sprintf(display_text, "???:?");

    mvaddstr(4, 2, "locator:        ");      
    
    if((active_status_line == 2) && (cursor_column == 1)) SetSeqColor(SCOL_INVERSE); // highlight if status line is addressed
    else SetSeqColor(SCOL_VALUE);

    mvaddstr(4, 11, display_text);
    SetSeqColor(SCOL_NORMAL);
    
    // display locators
    
    long quarters_in, quarters_out;
    
    quarters_in = (long)((p_sequencer->GetPunchIn() * 100.0) / (10000.0*60.0));
    quarters_out = (long)((p_sequencer->GetPunchOut() * 100.0) / (10000.0*60.0));
    
    mvaddstr(4, 49, "punch in/out:      /          ");
    
    sprintf(display_text, "%03ld:%ld", quarters_in/p_sequencer->GetQuartersPerBeat(), 
	    quarters_in % p_sequencer->GetQuartersPerBeat() + 1);
    
    if((active_status_line == 2) && (cursor_column == 4)) SetSeqColor(SCOL_INVERSE); // highlight if status line is addressed
    else SetSeqColor(SCOL_VALUE);

    mvaddstr(4, 63, display_text);      
    SetSeqColor(SCOL_NORMAL);
    
    sprintf(display_text, "%03ld:%ld", quarters_out/p_sequencer->GetQuartersPerBeat(), 
	    quarters_out % p_sequencer->GetQuartersPerBeat() + 1);
    
    if((active_status_line == 2) && (cursor_column == 5)) SetSeqColor(SCOL_INVERSE); // highlight if status line is addressed
    else SetSeqColor(SCOL_VALUE);

    mvaddstr(4, 69, display_text);      
    SetSeqColor(SCOL_NORMAL);
    
    // display record mode
    
    mvaddstr(5, 49, "record mode :            ");
    
    if(p_sequencer->GetRecOverwrite()) sprintf(display_text, "replace");
    else sprintf(display_text, "add notes");
    if((active_status_line == 1) && (cursor_column == 3)) SetSeqColor(SCOL_INVERSE); // highlight if status line is addressed
    else SetSeqColor(SCOL_VALUE);

    mvaddstr(5, 63, display_text);      
    SetSeqColor(SCOL_NORMAL);
    
    // display time signature
    
    mvaddstr(5, 20, "time signature: __/4");
    
    if((active_status_line == 1) && (cursor_column == 2)) SetSeqColor(SCOL_INVERSE); // highlight if status line is addressed
    else SetSeqColor(SCOL_VALUE);

    sprintf(display_text, "%2ld", p_sequencer->GetQuartersPerBeat());
    mvaddstr(5, 36, display_text);      
    SetSeqColor(SCOL_NORMAL);
    
    // display speed
    
    sprintf(display_text, "speed  :");
    mvaddstr(5, 2, display_text);
    if((active_status_line == 1) && (cursor_column == 1)) SetSeqColor(SCOL_INVERSE); // highlight if status line is addressed
    else SetSeqColor(SCOL_VALUE);

    sprintf(display_text, "%.1f", p_sequencer->GetQuartersPerMinute());
    mvaddstr(5, 11, display_text);
    SetSeqColor(SCOL_NORMAL);

    // display beeper states

    mvaddstr(4, 20, "play/rec beep : [ ] [ ]");      

    if((active_status_line == 2) && (cursor_column == 2)) SetSeqColor(SCOL_INVERSE);
    else SetSeqColor(SCOL_VALUE);

    if(p_sequencer->GetPlayBeep()) mvaddstr(4, 37, "X");
    else mvaddstr(4, 37, "-");
    SetSeqColor(SCOL_NORMAL);
    
    if((active_status_line == 2) && (cursor_column == 3)) SetSeqColor(SCOL_INVERSE);
    else SetSeqColor(SCOL_VALUE);

    if(p_sequencer->GetRecBeep()) mvaddstr(4, 41, "X");
    else mvaddstr(4, 41, "-");
    SetSeqColor(SCOL_NORMAL);
    
    // display Step-By-Step timing

    mvaddstr(2, 34, "step-by-step (note/advance): [1/??] [1/??]");      

    if((active_status_line == 3) && (cursor_column == 1)) SetSeqColor(SCOL_INVERSE);
    else SetSeqColor(SCOL_VALUE);

    sprintf(display_text, "%2ld", p_sequencer->GetRecStepByStepNoteLength());
    mvaddstr(2, 66, display_text);      

    if((active_status_line == 3) && (cursor_column == 2)) SetSeqColor(SCOL_INVERSE);
    else SetSeqColor(SCOL_VALUE);

    sprintf(display_text, "%2ld", p_sequencer->GetRecStepByStepAdvanceTime());
    mvaddstr(2, 73, display_text);      
    SetSeqColor(SCOL_NORMAL);
    
    int display_height, display_width;
    
    getmaxyx(stdscr, display_height, display_width);
    
    // update track display
        
    Track *p_work_track = Track::GetFirst();
    while(p_work_track) {
      if(p_work_track->GetCurrentDisplayLine() >= 0)
	if(p_sequencer->GetPlay()) {
	  
	  bool highlight_next_note;
	  
	  if((p_work_track == p_sequencer->GetActiveTrack()) && (active_status_line == 0)) highlight_next_note = true;
	  highlight_next_note = false;
	  
	  p_work_track->DisplayNextNotes(40, display_width - 40, (long)(((double)p_sequencer->current_song_time)),
					 p_sequencer->GetQuartersPerMinute(), p_sequencer->GetResolveQuarters(), highlight_next_note);
	} // if
      
      p_work_track = p_work_track->GetNext();
    } // while
    
    // update status text
    RefreshStatusText();
    
    // blink cursor

    if(!p_last_active_track) {
      p_last_active_track = p_sequencer->GetActiveTrack();   
      blink = 1;      
    } // if
      
    if((p_last_active_track) && (p_last_active_track != p_sequencer->GetActiveTrack()) && (active_status_line == 0)) {
      if(p_sequencer->GetPlay())
	p_last_active_track->DisplayStatus((long)(((double)p_sequencer->current_song_time)), track_no_display_digits, SCOL_NORMAL, cursor_column, -1);
      else
	p_last_active_track->DisplayStatus((long)(p_sequencer->GetSongOffset()), track_no_display_digits, SCOL_NORMAL, cursor_column, -1);

      blink = 1;
      p_last_active_track = p_sequencer->GetActiveTrack();   
    } // if

    if(p_sequencer->GetActiveTrack() != NULL) {
      
      SeqColor local_highlight = SCOL_INVERSE;
      if(active_status_line > 0) local_highlight = SCOL_NORMAL;
      
      if(p_sequencer->GetPlay())
	p_sequencer->GetActiveTrack()->DisplayStatus((long)p_sequencer->current_song_time, track_no_display_digits, local_highlight, cursor_column, blink);
      else
	p_sequencer->GetActiveTrack()->DisplayStatus((long)(p_sequencer->GetSongOffset()), track_no_display_digits, local_highlight, cursor_column, blink);
      
    } // if
    
    blink *= -1;
  } // if	
} // Sequencer::Beeper::DisplayStatusLine

Sequencer::Sequencer(midi_in_stream& in, midi_out_stream& out) : midi_in(in), midi_out(out)
{
  p_pool_container = new PoolContainer();
  midi_play          = false; 
  midi_rec           = false;
  escape_metronome   = false;
  midi_rec_overwrite = false;
  live_speed_change  = 0.0;
  speed_change_mode  = true;
  play_beep          = false; 
  rec_beep           = true;
  p_passed_event     = NULL;

  keyboard_command_mode         = KCM_INITIAL; // keyboard command mode in initial state
  keyboard_esc_mode             = false;
  midi_device_detected          = false;
  midi_device_detected_reported = false; // we did not yet tell the user that we have detected one
  
  p_active_track        = NULL;
  song_offset           = 0;

  p_current_active_keys = new unsigned char [128];

  for(int i = 0; i < NO_KEY_COMBINATIONS; i++) {
    p_current_active_keys[i] = 0;
  } // if
  
  for(int i = 0; i < NO_KEY_COMBINATIONS; i++)
    p_command_key_combinations[i] = new unsigned char [128];
  
  index_key_combination = -1;
  
  for(int i = 0; i < 128; i++) {
    p_current_active_keys[i] = 0;
  } // for
  
  if(LoadKeyCombination() == false) {
    for(int i = 0; i < NO_KEY_COMBINATIONS; i++)      
      for(int j = 0; j < 128; j++)
	p_command_key_combinations[i][j] = 1;
  } // if

  gettimeofday(&song_start_time, NULL);

} // Sequencer::Sequencer

Sequencer::~Sequencer()
{
  SaveKeyCombination();

  for(int i = 0; i < NO_KEY_COMBINATIONS; i++)
    delete[] p_command_key_combinations[i];

  delete[] p_current_active_keys;
  delete   p_pool_container;
} // Sequencer::~Sequencer

void Sequencer::Init()
{
  pthread_create(&rec_thread, NULL, RecMidiEvents, (void*)(&midi_in));
  escape_metronome = false;
  pthread_create(&metronome_thread, NULL, PlayMetronome, (void*)(&midi_out));
} // Sequencer::Init

long Sequencer::GetPassedSystemTime(timeval current_time)
{
  return ((current_time.tv_sec - song_start_time.tv_sec)*10000L+
	  (current_time.tv_usec - song_start_time.tv_usec)/100L);
} // Sequencer::GetPassedSystemTime

long Sequencer::ReadNextMidiByte(ifstream* p_midi_file, uint8 &next_valid_char)
{
  long bytes_read = 0;

  do {
    if(p_midi_file->eof()) return 0;
    p_midi_file->read((char*)&next_valid_char, 1);
    bytes_read++;
  } while ((next_valid_char >= 0xF8) && (next_valid_char < 0xFF));  // read next none-realtime message
  
  return bytes_read;
} // Sequencer::ReadNextMidiByte

void Sequencer::StartRecord()
{
  escape_metronome = true;
  pthread_join(metronome_thread, NULL);
  escape_metronome = false;
  speed_change_mode = false; // changing the speed during record mode
                             // still causes timing problems
  live_speed_change = 0.0;

  SetCurrentSongTime(0);

  gettimeofday(&song_start_time, NULL);
  SetRec(true);
  SetPlay(true);

  // pthread_create(&metronome_thread, NULL, PlayMetronome, (void*)(&midi_out));
  pthread_create(&play_thread, NULL, PlayMidiEvents, (void*)(&midi_out));
} // Sequencer::StartRecord

void Sequencer::StartPlay()
{
  escape_metronome  = true;
  pthread_join(metronome_thread, NULL);
  escape_metronome  = false;
  speed_change_mode = true;
  live_speed_change = 0.0;

  gettimeofday(&song_start_time, NULL);
  SetRec(false);
  SetPlay(true);

  // pthread_create(&metronome_thread, NULL, PlayMetronome, (void*)(&midi_out));
  pthread_create(&play_thread, NULL, PlayMidiEvents, (void*)(&midi_out));
} // Sequencer::StartPlay

void Sequencer::StopPlay(bool midi_reset, int notes_to_delete)
{
  if(GetRec() == true) {
    SetRec(false);

    // notes_to_delete > 0 only indicates that we come from CheckKeyCommand

    if(notes_to_delete > 0) {
      
      // in case of a stop by the key-commands:
      
      // the last notes have been responsable for
      // stoping the recording but they have also
      // been recorded so delete them
      
      GetActiveTrack()->DeleteLastEnduringNotes();
      
    } // if
  } // if

  if(GetPlay() == true) {
    SetPlay(false);
    pthread_join(play_thread, NULL);
  } // if

  SetCurrentSongTime(0);
  SetSongOffset(0);
  SetSystemTimePassedEvent(NULL, 0); // no prior event

  for(Track* p_work_track = Track::GetFirst(); p_work_track != NULL; 
      p_work_track = p_work_track->GetNext()) {
    if(midi_reset)
      p_work_track->MidiReset(midi_out);
    p_work_track->JumpToStart();
  } // for

  escape_metronome = true;
  pthread_join(metronome_thread, NULL);
  escape_metronome = false;
  pthread_create(&metronome_thread, NULL, PlayMetronome, (void*)(&midi_out));
} // Sequencer::StopPlay

void Sequencer::Pause()
{
  SetRec(false);
  SetPlay(false);
  pthread_join(play_thread, NULL);
  SetSongOffset(GetCurrentSongTime()+GetSongOffset());
  SetCurrentSongTime(0);
  escape_metronome = false;
  pthread_create(&metronome_thread, NULL, PlayMetronome, (void*)(&midi_out));
} // Sequencer::Pause

void Sequencer::StepForward()
{
  // first quantize the end of the current song time otherwise
  // the inaccuracy caused by the previous step may accumulate

  long start_time, exit_time, delta, modulo;
  long atomic_time_unit = 10000*60/((long)(100*GetResolveQuarters()));

  start_time = GetSongOffset();
  
  modulo = (start_time) % atomic_time_unit;
  delta = atomic_time_unit - modulo;

  if(delta < atomic_time_unit/2) start_time -= delta;
  else start_time += (atomic_time_unit-delta);

  SetSongOffset(start_time);

  exit_time = GetSongOffset()+atomic_time_unit;
  modulo = (exit_time) % atomic_time_unit;
  delta = atomic_time_unit - modulo;

  if(delta < atomic_time_unit/2) exit_time -= delta;
  else exit_time += (atomic_time_unit-delta);

  SetPlayStepByStep(true);
  SetPlayStepByStepExitTime(exit_time);
  StartPlay();
  SetStatusText("Sequencer steping...");
  pthread_join(play_thread, NULL);
  SetSongOffset(exit_time);
  Track::MarkAllTracksDirty();
} // Sequencer::StepForward

void Sequencer::StepBackward()
{
  long song_pos = GetSongOffset()-(long)(10000*60/(100*GetResolveQuarters()));

  if(song_pos < 0) song_pos = 0;

  SetSongOffset(song_pos);
  Track::MarkAllTracksDirty();
} // Sequencer::StepBackward

void Sequencer::PlayNumberSequence(int number)
{
  pthread_t number_sequence_thread;

  p_seq->SetNumberToPlay(number);
  pthread_create(&number_sequence_thread, NULL, ThreadPlayNumberSequence, (void*)(this));
} // Sequencer::PlayNumberSequence

void* Sequencer::ThreadPlayNumberSequence(void* p_arg)
{
  timespec delta_sleep;

  int      most_sig_digit    = (long)(p_seq->GetNumberToPlay()) / KEY_COMMAND_MODULO_BASE;
  int      least_sig_digit   = (long)(p_seq->GetNumberToPlay()) % KEY_COMMAND_MODULO_BASE;

  uint8    metronome_channel = 0;
  uint8    channel_byte      = 0x90 | metronome_channel;
  uint8    msd_key           = 70;
  uint8    lsd_key           = 75;

  delta_sleep.tv_sec = 0;
  delta_sleep.tv_nsec = (1000)*100000;
 
  for(int i = 0; i < most_sig_digit; i++) {
    nanosleep(&delta_sleep, NULL);

    p_seq->GetMidiOutStream()->put(channel_byte);
    p_seq->GetMidiOutStream()->put(msd_key);
    p_seq->GetMidiOutStream()->put(64);
    p_seq->GetMidiOutStream()->flush();
    
    nanosleep(&delta_sleep, NULL);

    p_seq->GetMidiOutStream()->put(channel_byte);
    p_seq->GetMidiOutStream()->put(msd_key);
    p_seq->GetMidiOutStream()->put(0);
    p_seq->GetMidiOutStream()->flush();
  } // for

  for(int i = 0; i < least_sig_digit; i++) {
    nanosleep(&delta_sleep, NULL);

    p_seq->GetMidiOutStream()->put(channel_byte);
    p_seq->GetMidiOutStream()->put(lsd_key);
    p_seq->GetMidiOutStream()->put(64);
    p_seq->GetMidiOutStream()->flush();
    
    nanosleep(&delta_sleep, NULL);

    p_seq->GetMidiOutStream()->put(channel_byte);
    p_seq->GetMidiOutStream()->put(lsd_key);
    p_seq->GetMidiOutStream()->put(0);
    p_seq->GetMidiOutStream()->flush();
  } // for

  return NULL;
} // Sequencer::ThreadPlayNumberSequence

int Sequencer::LoadMidiFile(ifstream* p_midi_file)
{
  char     text[6];
  uint8    byte, channel_byte, voice_message = 0, dummy, meta_event;
  uint8    data_byte[2];
  uint16   format, no_tracks, delta_time_ticks;
  uint32   time_to_wait, total_time_elapsed, size_none_midi_message;
  long     no_bytes_in_track, index = -1, no_expedited_data;
  long     time_for_one_quarter, qpm;
  uint8    *none_midi_message = NULL;
  unsigned long length_none_midi_buffer = 0;
  uint16_t i_sem;

  // Read midi-chunk header

  p_midi_file->read(text, 4);
  text[4] = 0;

  PrintError("Load MIDI file");
  PrintError("Midi Chunk Header: ", false); PrintError(text);

  if(strcmp(text, "MThd") != 0) {
    PrintError("ERROR (LoadMidiFile): File-header not midi-compliant");
    return -1;
  } // if

  // read header size (not used)

  p_midi_file->read(text, 4);
  text[4] = 0;

  PrintError("Size Chunk Header: ", false); PrintErrorChar(text[0], false); PrintErrorChar(text[1], false); PrintErrorChar(text[2], false); PrintErrorChar(text[3]);

  // read file format

  p_midi_file->read((char*)&format, 2);
  format = ntohs(format);

  PrintError("Midi file format: ", false); PrintErrorInt((int)format);

  if(format > 2) {
    SetStatusText("ERRRO (LoadMidiFile): format not midi-compliant");
    goto exit_LoadMidiFile;
  } // if 
  
  if(format == 2) {
    SetStatusText("ERRRO (LoadMidiFile): Midi file-format == 2 not yet supported");
    goto exit_LoadMidiFile;
  } // if 
  
  // read number of tracks to follow

  p_midi_file->read((char*)&no_tracks, 2);
  no_tracks = ntohs(no_tracks);

  PrintError("No Tracks: ", false); PrintErrorInt((int)no_tracks);

  // read delta time ticks

  p_midi_file->read((char*)&delta_time_ticks, 2);
  delta_time_ticks = ntohs(delta_time_ticks);

  PrintError("Delta Time Ticks: "); PrintErrorInt((int)delta_time_ticks);

  p_active_track = Track::GetFirst();

  for(int track = 0; track < no_tracks; track++) {

    PrintError("-----------------------------------------------------");
    PrintError("Reading track: ", false); PrintErrorInt(track);

    total_time_elapsed = 0;

    // Read track-chunk header

    p_midi_file->read(text, 4);

    text[4] = 0;

    PrintError("Track header: ", false); PrintError(text);

    p_midi_file->read((char*)&no_bytes_in_track, 4);
    no_bytes_in_track = ntohl(no_bytes_in_track);

    if(strcmp(text, "MTrk") != 0) {
      PrintError("HINT (LoadMidiFile): Track was not midi-compliant");

      PrintError("no bytes in none-midi track: ", false); PrintErrorInt((int)no_bytes_in_track);
      p_midi_file->ignore(no_bytes_in_track);
    } // if
    else {
      PrintError("no bytes in track: ", false); PrintErrorInt(no_bytes_in_track);

      index = -1;
      
      while(no_bytes_in_track > 0) {
	
	if(p_midi_file->eof()) {
	  SetStatusText("ERROR (LoadMidiFile): Unexpected end of track");
	  goto exit_LoadMidiFile;
	} // if
	
	if(index == -1) {
	  
	  no_bytes_in_track -= ReadVariableLength(p_midi_file, time_to_wait);
	  PrintError("time_to_wait = ", false); PrintErrorInt(time_to_wait);
	  
	  total_time_elapsed += time_to_wait;
	} // if
	
	no_expedited_data = ReadNextMidiByte(p_midi_file, byte);
	no_bytes_in_track -= no_expedited_data;
	if(no_expedited_data > 1) PrintError("      EXPEDIED DATA read: ", false); PrintErrorInt(no_expedited_data);

	channel_byte = (uint8)byte;
	
	if(channel_byte >= 0xF0) {
	  
	  // I am a real-time message
	  
	  // so far, realtime messages are not (yet) processed
	  
	  PrintError("realtime message: ", false); PrintErrorHex((int)channel_byte);
	  
	  switch(channel_byte) {
	    
	  case 0xF0:
	    PrintError("HINT (LoadMidiFile): System exclusive messages");

	    // now, a number of system exclusive messages arrive. Internally, we store
	    // each byte as a separate message and tag it with a 0xF0. However, only the
	    // first 0xF0 is send to a midi-device or stored in a file.

	    do {
	      p_midi_file->read((char*)&dummy, 1);
	      no_bytes_in_track -= 1;

	      if((dummy > 127) && (dummy != 0xF7)) {
		PrintError("ERROR (LoadMidiFile): system exclusive message was not terminated by 0xF7");
	      } // if
	      
	      if(dummy <= 127)
		p_active_track->AddEvent(0xF0, &dummy, (long)((int64_t)(total_time_elapsed)*(int64_t)(6000)/(int64_t)delta_time_ticks), 0);
	      else
		p_active_track->AddEvent(0xF7, &dummy, (long)((int64_t)(total_time_elapsed)*(int64_t)(6000)/(int64_t)delta_time_ticks), 0);
	      
	    } while(dummy <= 127);
	    break;
	    
	  case 0xF1:
	    PrintError("ERROR (LoadMidiFile): undefined");
	    // undefined system message (not specified - valid??!?)
	    break;
	  
	  case 0xF2:
	    PrintError("ERROR (LoadMidiFile): Song position messages are not yet handled");
	    break;
	    
	  case 0xF3:
	    PrintError("ERROR (LoadMidiFile): Song select messages are not yet handled");
	    p_midi_file->read((char*)&dummy, 1);
	    no_bytes_in_track--;
	    break;
	    
	  case 0xF4:
	    PrintError("ERROR (LoadMidiFile): undefined");
	    // undefined system message (not specified - valid??!?)
	    break;
	    
	  case 0xF5:
	    PrintError("ERROR (LoadMidiFile): undefined");
	    // undefined system message (not specified - valid??!?)
	    break;
	    
	  case 0xF6:
	    PrintError("ERROR (LoadMidiFile): tune request");
	    // tune request
	    break;
	    
	  case 0xF7:

	    // EOX (terminator) - end of system exclusive message

	    // internally we code the end of a series of many
	    // sys-ex. bytes with an entire midi_event of type
	    // 0xF7 with no following data

	    p_active_track->AddEvent(0xF7, &dummy, (long)((int64_t)(total_time_elapsed)*(int64_t)(6000)/(int64_t)delta_time_ticks), 0);
	    break;
	    
	  case 0xFF:
	    PrintError("HINT (LoadMidiFile): None-midi event");

	    p_midi_file->read((char*)&meta_event, 1);
	    no_bytes_in_track--;
	    	    
	    no_bytes_in_track -= ReadVariableLength(p_midi_file, size_none_midi_message);
	    if(length_none_midi_buffer < size_none_midi_message) {
	      if(none_midi_message != NULL) delete none_midi_message;
	      none_midi_message = new uint8[size_none_midi_message+1];
	      length_none_midi_buffer = size_none_midi_message;
	    } // if

	    switch(meta_event) {
	    case 03:
	      p_midi_file->read((char*)none_midi_message, size_none_midi_message);
	      none_midi_message[size_none_midi_message] = 0;
	      p_active_track->SetName((char*)none_midi_message);
	      break;

	    case 0x2F:
	      // end-of-track meta event

	      no_bytes_in_track = 0;

	      if(size_none_midi_message > 0) PrintError("ERROR (LoadMidiFile): meta event end-of-tack has some payload");
	      break;

	    case 0x51:
	      // tempo definition

	      PrintError("  Meta event: tempo change to (qpm): ");

	      p_midi_file->read((char*)none_midi_message, size_none_midi_message);
	      time_for_one_quarter = (((long)(none_midi_message[0]) << 16) | ((long)(none_midi_message[1]) << 8) | ((long)(none_midi_message[0]))/100);
	      qpm = (10000*60*100)/time_for_one_quarter;
	      PrintErrorInt(time_for_one_quarter);
	      p_seq->SetQuartersPerMinute(qpm);
	      break;

	    default:
	      PrintError("  Meta event not yet handled: "); PrintErrorInt((int)meta_event);
	      p_midi_file->ignore(size_none_midi_message);
	      break;

	    } // switch

	    no_bytes_in_track -= size_none_midi_message;

	    PrintError("     type NME: ", false); PrintErrorInt((int)dummy);
	    PrintError("     Size NME: ", false); PrintErrorInt(size_none_midi_message);
	    PrintError("     (no_bytes_in_track): ", false); PrintErrorInt(no_bytes_in_track);           
	    break;
	    
	  default:
	    PrintError("ERROR (LoadMidiFile): Unknown system message");
	    break;
	    
	  }; // switch
	  
	  index = -1;
	} // if
	else
	  if(channel_byte & 128) {
	    
	    // I am a status byte
	    
	    uint8 prev_message = voice_message;
	    voice_message = channel_byte;
	    
	    PrintError("voice message: ", false); PrintErrorInt((int)voice_message);
	    
	    if(index > 0) {
	      PrintError("UNEXPECTED CONDITION: One or more data bytes missing. "); PrintErrorInt(no_bytes_in_track);
	      PrintError("voice message: " ); PrintErrorInt((int)voice_message);
	      PrintError("prev. voice message: "); PrintErrorInt((int)prev_message);
	      PrintError("index: "); PrintErrorInt((int)index);           
	      // exit(-1);
	    } // if
	    
	    index = GetNoFollowingDataBytes(voice_message);
	    
	    PrintError("index = ", false); PrintErrorInt(index);
	    
	    if(index == 0) {
              p_active_track->AddEvent(channel_byte, data_byte, (long)((int64_t)(total_time_elapsed)*(int64_t)(6000)/(int64_t)delta_time_ticks), 0);
	      index = -1;
	      PrintError("UNEXPECTED CONDITION: A status byte with no data byte was encountered");
	    } // if
	    
	    if(index > 2) {
	      PrintError("UNEXPECTED CONDITION: A status byte more that two data bytes was encountered");
	    } // if
	    
	  } // if
	  else {
	    
	    // I am a data byte
	    
	    PrintError("data byte (", false); PrintErrorInt(no_bytes_in_track, false); PrintError("): ", false); PrintErrorInt((int)channel_byte);
	    
	    if(index == -1) {
	      
	      // this is an instance of a running status byte
	      
	      index = GetNoFollowingDataBytes(voice_message);
	    } // if
	    
	    if(index > 0) {
	      index--;
	      data_byte[index] = channel_byte;
	    } // if
	    
	    if(index == 0) {	 

	      // all notes within this application are based on
	      // 100 QPM and a second is resolved into 10000
	      // ticks. As a consequence, a quarters is resolved
	      // into 6000 ticks but the midi file is based on
	      // delta_time_ticks ticks (each quarters resp. beat)

	      p_active_track->AddEvent(voice_message, data_byte, (long)((int64_t)(total_time_elapsed)*(int64_t)(6000)/(int64_t)delta_time_ticks), 0);
	      index = -1; // we do not expect a data byte
	    } // if
	    
	  } // else
      } // while
      
      p_active_track->SetMidiPlayChannel(p_active_track->GetContainedMidiChannel());

      p_active_track->MarkNoteDisplayDirty();
      p_active_track = p_active_track->GetNext();
      if(p_active_track == NULL) p_active_track = Track::GetNewTrack(this, ALL_CHANNELS);
    } // else
  } // for

  SetStatusText("Midi file loaded!");

 exit_LoadMidiFile:
  if(none_midi_message) delete[] none_midi_message;

  p_active_track = Track::GetFirst();

  SetCurrentSongTime(0);

  // TODO: different return value on errors
  return 0;
} // Sequencer::LoadMidiFile

void Sequencer::GenerateMidiFile(ofstream* p_midi_file)
{
  Track     *p_work_track;
  uint32    size_header;
  uint16    no_tracks, format, quarter_note_time_units, file_byte_ordered;
  streampos format_pos, track_pos;

  p_midi_file->write("MThd", 4);
  
  size_header = htonl(6L);
  p_midi_file->write((char*)&size_header, sizeof(size_header));

  format_pos = p_midi_file->tellp();
  format = htons(0xFFFF);
  p_midi_file->write((char*)&format, sizeof(format));

  track_pos = p_midi_file->tellp();
  no_tracks = htons(0);
  p_midi_file->write((char*)&no_tracks, sizeof(no_tracks));

  quarter_note_time_units = htons((uint16)6000);
  p_midi_file->write((char*)&quarter_note_time_units, sizeof(quarter_note_time_units));

  p_work_track = Track::GetFirst();

  while(p_work_track != NULL) {
    
    if(p_work_track->GetFirstMidiEventPtr() != NULL) {
      p_work_track->SaveMidiFile(p_midi_file, (no_tracks == 0));
      no_tracks++;
    } // if

    p_work_track = p_work_track->GetNext();
  } // while

  if(no_tracks > 1) format = htons(1); // multiple tracks, synchroneous
  else format = htons(0);              // single track
  p_midi_file->seekp(format_pos);
  p_midi_file->write((char*)&format, sizeof(format));

  file_byte_ordered = htons(no_tracks);
  p_midi_file->seekp(track_pos);
  p_midi_file->write((char*)&file_byte_ordered, sizeof(file_byte_ordered));
} // Sequencer::GenerateMidiFile

void Sequencer::MoveNextTrack()
{
  if(p_active_track->GetNext() == NULL) p_active_track = Track::GetNewTrack(this, ALL_CHANNELS);
  else
    p_active_track = p_active_track->GetNext();
  
  if(p_active_track == p_last_displayed_track->GetNext()) {
    p_first_displayed_track = p_first_displayed_track->GetNext();
  } // if
  RedrawScreen();
} // Sequencer::MoveNextTrack  

bool Sequencer::MovePrevTrack()
{
  if(p_active_track->GetPrev() != NULL) {
    p_active_track = p_active_track->GetPrev();
    if(p_active_track == p_first_displayed_track->GetPrev()) {
      p_first_displayed_track = p_first_displayed_track->GetPrev();
    } // if
    RedrawScreen();
    return true;
  } // if

  return false;
} // Sequencer::MovePrevTrack

bool Sequencer::CheckCommandKeyCombination()
{
  int  i, j, first_index, last_index;
  int  number_played = -1; 

  first_index = 0;
  last_index  = NO_KEY_COMBINATIONS;             // test all known command key-combinations
  if(keyboard_esc_mode == false) last_index = 1; // if not in keyboard esc-mode we only need
                                                 // to detect the esc sequence

  if(last_index > 1) {
    if(keyboard_command_mode == KCM_INITIAL)
      last_index = 15;     // we don't expect a number
    else first_index = 15; // we only expect numbers
  } // if

  for(i = first_index; i < last_index; i++) {

    for(j = 0; j < 128; j++) {
      if(p_current_active_keys[j] != p_command_key_combinations[i][j])
	break;
    } // for

    if(j == 128) goto process_key_comb;

  } // for

  return false;

 process_key_comb:

  if(i == 0) { // the first key command indicates the keyboard_esc_mode

    keyboard_command_mode = KCM_INITIAL;

    // if playing or recording, the esc sequence is always interpreted as stop

    if(GetPlay() && !GetRec()) {
      Pause();
      keyboard_esc_mode = false;
      return true;
    } // if

    if(GetRec()) {
      StopPlay(false, GetKeyCommandNoNotes(0));
      keyboard_esc_mode = false;
      Track::MarkAllTracksDirty();
      return true;
    } // if
    
    keyboard_esc_mode = true;
    return true;
  } // if

  if(keyboard_esc_mode) {
    switch(i) {
    case 1:            // play song
      StartPlay();
      keyboard_esc_mode = false;
      return true;
      
    case 2:            // start recording
      keyboard_esc_mode = false;
      StartRecord();
      return true;
      
    case 3:           // stop/rewind
      keyboard_esc_mode = false;
      StopPlay();
      return true;
      break;

    case 4:           // duplicate track      
      PlayNumberSequence(Track::GetFirstFreeTrack(p_seq)->GetNumber());
      p_seq->GetActiveTrack()->CopyTrackTo(Track::GetFirstFreeTrack(p_seq));
      SetStatusText("Active track duplicated");
      RedrawScreen();
      keyboard_esc_mode = false;
      break;
      
    case 5:            // quantize track
      PlayNumberSequence(1);
      keyboard_command_mode = KCM_QUANTIZE_TRACK;
      return true;
      // break;
      
    case 6:            // set punch-in
      break;
      
    case 7:            // set punch-out
      break;
      
    case 8:            // move to next track
      MoveNextTrack();
      PlayNumberSequence(p_seq->GetActiveTrack()->GetNumber());
      keyboard_esc_mode = false;
      break;
      
    case 9:            // move to previous track
      MovePrevTrack(); 
      PlayNumberSequence(p_seq->GetActiveTrack()->GetNumber());
      keyboard_esc_mode = false;
      break;
      
    case 10:            // toggle rec mode
      if(p_seq->GetRecOverwrite()) PlayNumberSequence(1);
      else PlayNumberSequence(KEY_COMMAND_MODULO_BASE);

      p_seq->SetRecOverwrite(!p_seq->GetRecOverwrite());
      keyboard_esc_mode = false;
      break;
      
    case 11:            // toggle play beep
      if(GetPlayBeep())
	PlayNumberSequence(KEY_COMMAND_MODULO_BASE); // beep for false
      else
	PlayNumberSequence(1); // beep for true

      SetPlayBeep(!GetPlayBeep());
      break;

    case 12:
      if(GetRecBeep())
	PlayNumberSequence(KEY_COMMAND_MODULO_BASE); // beep for false
      else
	PlayNumberSequence(1); // beep for true

      SetRecBeep(!GetRecBeep());
      break;

    case 13: // change midi-channel for recording (we listen to)
      keyboard_command_mode = KCM_CHANGE_MIDI_RECORD_CHAN;
      PlayNumberSequence(1);
      break;

    case 14: // change midi-channel for playback
      keyboard_command_mode = KCM_CHANGE_MIDI_PLAYBACK_CHAN;
      PlayNumberSequence(2);
      break;

    case 15:
      number_played = 0;
      break;

    case 16:
      number_played = 1;
      break;

    case 17:
      number_played = 2;
      break;

    case 18:
      number_played = 3;
      break;

    case 19:
      number_played = 4;
      break;

    case 20:
      number_played = 5;
      break;

    case 21:
      number_played = 6;
      break;

    case 22:
      number_played = 7;
      break;

    case 23:
      number_played = 8;
      break;

    case 24:
      number_played = 9;
      break;

    case 25:
      number_played = 10;
      break;

    case 26:
      number_played = 11;
      break;

    case 27:
      number_played = 12;
      break;

    case 28:
      number_played = 13;
      break;

    case 29:
      number_played = 14;
      break;

    case 30:
      number_played = 15;
      break;

    default:
      PrintError("ERROR (CheckCommandKeyCombination): unknown key-combination");
      break;
      
    } // switch

    switch(keyboard_command_mode) {

    case KCM_CHANGE_MIDI_RECORD_CHAN:
      if(number_played >= 0) {
	GetActiveTrack()->SetMidiRecChannels(1 << number_played);
	keyboard_command_mode = KCM_INITIAL;
	keyboard_esc_mode = false;
	PlayNumberSequence(number_played);
      } // if
      break;

    case KCM_CHANGE_MIDI_PLAYBACK_CHAN:
      if(number_played >= 0) {
	GetActiveTrack()->SetMidiPlayChannel(number_played);
	keyboard_command_mode = KCM_INITIAL;
	keyboard_esc_mode = false;
	PlayNumberSequence(number_played);
      } // if
      break;
 
    case KCM_QUANTIZE_TRACK:
      p_seq->GetActiveTrack()->Quantize((long)(4*10000*60/(100.0*((long)pow(2, number_played)))), 
					p_seq->GetPunchIn(), p_seq->GetPunchOut());
      keyboard_command_mode = KCM_INITIAL;
      keyboard_esc_mode = false;
      PlayNumberSequence(number_played);
      break;

    default:
      break;
    }; // switch
  } // if

  return false;
} // Sequencer::CheckCommandKeyCombination

bool Sequencer::AllCurrentActiveKeysOff()
{
  for(int i = 0; i < 128; i++) {
    if(p_current_active_keys[i] != 0) return false;
  } // for

  return true;
} // Sequencer::AllCurrentActiveKeysOff

void Sequencer::StoreKeyCombination()
{
  if((index_key_combination >= 0) && (index_key_combination < NO_KEY_COMBINATIONS)) {

    for(int i = 0; i < 128; i++) {
      p_command_key_combinations[index_key_combination][i] = p_current_active_keys[i];
    } // for

    index_key_combination = -1;
  } // if
} // Sequencer::StoreKeyCombination

void Sequencer::SaveKeyCombination()
{
  ofstream key_comb_file(".cuse");

  if(key_comb_file.good()) {
       
    for(int index = 0; index < NO_KEY_COMBINATIONS; index++) {

      key_comb_file << index << ": ";

      for(int i = 0; i < 128; i++) {
	if(p_command_key_combinations[index][i] != 0) {
	  key_comb_file << i << " ";
	} // if
      } // for
      
      key_comb_file << endl;
    } // for
  } // if
} // Sequencer::SaveKeyCombination

bool Sequencer::LoadKeyCombination()
{
  char in_text[4096];
  int  index, i;

  // clear array
  
  for(index = 0; index < NO_KEY_COMBINATIONS; index++)
    for(int j = 0; j < 128; j++)
      p_command_key_combinations[index][j] = 0;
  
  ifstream key_comb_file(".cuse");

  if(key_comb_file.good()) {
   
    // load file

    index = i = -1;

    while(!key_comb_file.eof()) {
      key_comb_file >> in_text;

      if(in_text[strlen(in_text)-1] == ':') {
	index = atoi(in_text);

	if((index < 0) || (index >= NO_KEY_COMBINATIONS)) {
	  index = -1;
	} // if
	
	i = -1;
      } // if
      else {
	i = atoi(in_text);

	if((i < 0) || (i >= 128)) {
	  i = -1;
	} // if
      } // else

      if((index != -1) && (i != -1)) {
	p_command_key_combinations[index][i] = 1;
      } // if
    } // while  
  } // if
  else {

    // the file does not yet exist so let's create the default

    p_command_key_combinations[0][61] = 1; // ESC/Pause
    p_command_key_combinations[0][63] = 1;
    p_command_key_combinations[0][73] = 1;
    p_command_key_combinations[0][75] = 1;
    p_command_key_combinations[1][67] = 1;
    p_command_key_combinations[2][70] = 1;
    p_command_key_combinations[3][60] = 1;
    p_command_key_combinations[4][77] = 1;
    p_command_key_combinations[5][79] = 1;
    p_command_key_combinations[6][78] = 1;
    p_command_key_combinations[7][80] = 1;
    p_command_key_combinations[8][68] = 1;
    p_command_key_combinations[9][66] = 1;
    p_command_key_combinations[10][82] = 1;
    p_command_key_combinations[11][81] = 1;
    p_command_key_combinations[12][83] = 1;
    p_command_key_combinations[13][76] = 1;
    p_command_key_combinations[14][74] = 1;

    p_command_key_combinations[15][60] = 1; // #00
    p_command_key_combinations[15][67] = 1;

    p_command_key_combinations[16][60] = 1; // #01
    p_command_key_combinations[16][69] = 1;

    p_command_key_combinations[17][60] = 1; // #02
    p_command_key_combinations[17][71] = 1;

    p_command_key_combinations[18][60] = 1; // #03
    p_command_key_combinations[18][72] = 1;

    p_command_key_combinations[19][62] = 1; // #04
    p_command_key_combinations[19][67] = 1;

    p_command_key_combinations[20][62] = 1;
    p_command_key_combinations[20][69] = 1;

    p_command_key_combinations[21][62] = 1;
    p_command_key_combinations[21][71] = 1;

    p_command_key_combinations[22][62] = 1;
    p_command_key_combinations[22][72] = 1;

    p_command_key_combinations[23][64] = 1;
    p_command_key_combinations[23][67] = 1;

    p_command_key_combinations[24][64] = 1;
    p_command_key_combinations[24][69] = 1;

    p_command_key_combinations[25][64] = 1;
    p_command_key_combinations[25][71] = 1;

    p_command_key_combinations[26][64] = 1;
    p_command_key_combinations[26][72] = 1;

    p_command_key_combinations[27][65] = 1;
    p_command_key_combinations[27][67] = 1;

    p_command_key_combinations[28][65] = 1;
    p_command_key_combinations[28][69] = 1;

    p_command_key_combinations[29][65] = 1;
    p_command_key_combinations[29][71] = 1;

    p_command_key_combinations[30][65] = 1;
    p_command_key_combinations[30][72] = 1;
  } // else

  // are there any command-key-combinations with no entry?
  // since we do not want to match with a "no-key-combination",
  // we set all keys to prevent this
  
  for(int j = 0; j < NO_KEY_COMBINATIONS; j++) {
    
    bool no_key_set = true;
    
    for(int i = 0; i < 128; i++) {
      if(p_command_key_combinations[j][i] == 1) {
	no_key_set = false;
	break;
      } // if
    } // for
    
    if(no_key_set) {
      for(int i = 0; i < 128; i++)
	p_command_key_combinations[j][i] = 1;	
    } // if
  } // for
  
  return true;
} // Sequencer::LoadKeyCombination

int Sequencer::GetKeyCommandNoNotes(int index)
{
  int no_keys = 0;

  for(int i = 0; i < 128; i++) {
    if(p_command_key_combinations[index][i] != 0) no_keys++;
  } // for

  return no_keys;
} // Sequencer::GetKeyCommandNoNotes

void Sequencer::SetRecStepByStepNoteLength(long value)
{
  step_by_step_note_len = value;

  if(step_by_step_note_len < 1) step_by_step_note_len = 1;
  if(step_by_step_note_len > 32) step_by_step_note_len = 32;
} // Sequencer::SetRecStepByStepNoteLength

void Sequencer::SetRecStepByStepAdvanceTime(long value) 
{
  step_by_step_advance_time = value;

  if(step_by_step_advance_time < 1) step_by_step_advance_time = 1;
  if(step_by_step_advance_time > 32) step_by_step_advance_time = 32;
} // Sequencer::SetRecStepByStepAdvanceTime

bool Sequencer::GetDeviceDetected() 
{
  if(midi_device_detected_reported == false) {
    return midi_device_detected; 
  } // if

  return false;
} // Sequencer::GetDeviceDetected
