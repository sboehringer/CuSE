/*

- So far, single events can not yet be deleted and event pools are not
  deleted when the application terminates

- When starting the record for a second time start_rec_time is set to wrong value by case 'r'

- Is playing again without ending the last play allowed?

- Before exiting the application a midi-reset should be send (at least in some cases)

- LoadMidiFile() has to be able to handle corrupted files

- Before loading, all tracks should be erased

- Loading third-party midi files does not yet work

- Not all meta events are implemented

- Lines of Tracks with numbers > 99 are not rendered correctly
  because subsequent mvaddstr() do not (yet) accound for more digits

- Midi-Channels should by default be selected exclusively in ChannelSelect()

- Loading .keycomb leads to failure if file not present

- Track::DeleteUpcommingEvent() does not yet delete events,
  it only removes them from the queue

- How often is each track redisplayed? (unnecessarily often?)

- Is MULT displayed as PlayChannel (in track display) is track contains
  more than one midi channel

- System exclusive messages might potentially be interrupted by events
  on another track during playback

- System exclusive messages can not yet be stored correctly

- What happens to system exclusive messages in the quantization?

- No speed changes while recording

- When stoping a recording by the key-commands in the middle of a song,
  the wrong but the key-command-notes are deleted!

- Quantize does not yet care for the border punch-in and punch-out

*/

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
#include "sequencer.h"
#include "sequencer_globals.h"
#include "track.h"

using namespace std;

const int  MAX_FILENAME = 4096;
char       filename[MAX_FILENAME];
bool       sequencer_debug_info;

Sequencer *p_seq;

  /* GUI-Components */

Track       *p_first_displayed_track, *p_last_displayed_track;
static char *menulist[MAX_MENU_ITEMS][MAX_SUB_ITEMS];
char        status_text[LENGTH_DISPLAY_TEXT];
bool        display_top_line, popup_active;
short       cursor_column; // current cursor column in track display
short       track_no_display_digits;
short       active_status_line; // highlighted status line

  /* GUI-Programming */

void quit(void)
{
  endwin();
}

void RedrawTrackDisplay()
{
  int      width, height;
  int      current_line = FIRST_LINE_DISPLAY_TRACK;
  Track    *p_work_track;
  SeqColor highlight_attribute;
 
  getmaxyx(stdscr, height, width);

  p_work_track = Track::GetFirst();

  while(p_work_track) {
    p_work_track->SetCurrentDisplayLine(-1);
    p_work_track = p_work_track->GetNext();
  } // while

  if(p_first_displayed_track != NULL) p_work_track = p_first_displayed_track;
  else p_work_track = Track::GetFirst();
  
  while(current_line < (height-3)) {

    // Create enought tracks to cover the screen

    if(p_work_track == NULL) {
      p_work_track = Track::GetNewTrack(p_seq, ALL_CHANNELS);
      p_work_track->MarkNoteDisplayDirty();
      if(p_seq->GetActiveTrack() == NULL) p_seq->SetActiveTrack(p_work_track);
    } // if

    if((p_work_track == p_seq->GetActiveTrack()) && (active_status_line == 0)) highlight_attribute = SCOL_NORMAL;
    else highlight_attribute = SCOL_NORMAL;

    p_work_track->SetCurrentDisplayLine(current_line);

    if(p_seq->GetActiveTrack() == NULL) p_seq->SetActiveTrack(p_work_track);

    // determine number of digits needed for display of track number

    track_no_display_digits = (int)log10(Track::GetLast()->GetNumber())+1;
    if(track_no_display_digits < 2) track_no_display_digits = 2;

    p_work_track->DisplayStatus(p_seq->GetSongOffset()+p_seq->GetCurrentSongTime(), track_no_display_digits, highlight_attribute, cursor_column);

    current_line += 2;

    if(p_first_displayed_track == NULL) p_first_displayed_track = p_work_track;

    if(p_work_track->GetNumber() >= 99999) break;
    p_last_displayed_track = p_work_track;

    p_work_track = p_work_track->GetNext();

  } // while
} // RedrawTrackDisplay

void RefreshStatusText()
{
  int width, height, size_text_mem_usage;
  char display_text[LENGTH_DISPLAY_TEXT];

  getmaxyx(stdscr, height, width);
  
  // color_set(3, NULL);

  // clear line

  SetSeqColor(SCOL_STATUS_LINE);

  for(int i = 0; i < width; i++)
    mvaddstr(height-1, i, " ");

  snprintf(display_text, sizeof(display_text)-1, "%s", status_text);
  mvaddstr(height-1, 1, display_text);  
  snprintf(display_text, sizeof(display_text), "Mem usage (kB): %ld", p_seq->GetPoolContainer()->GetMemUsed()/1024);
  size_text_mem_usage = strlen(display_text)-1;
  mvaddstr(height-1, width-size_text_mem_usage-2, display_text);  

  switch(active_status_line) {

  case 1:
    strcpy(display_text, "use keys: ");

    switch(cursor_column) {
    case 1:
      strcat(display_text, "+ - space ret");
      break;
      
    case 3:
      strcat(display_text, "ret");
      break;

    default:
      break;
      
    }; // switch
    break;

  case 2:
    strcpy(display_text, "use keys: ");

    switch(cursor_column) {
    case 1:
      strcat(display_text, "+ - space ret");
      break;
      
    case 2:
    case 3:
      strcat(display_text, "ret");
      break;
      
    case 4:
      strcat(display_text, "+ - space ret");
      break;
      
    case 5:
      strcat(display_text, "+ - space ret");
      break;
      
    }; // switch
    break;

  case 3:
    strcpy(display_text, "use keys: 4 8 6");
    break;

  default:
    strcpy(display_text, "use keys: ");
    
    switch(cursor_column) {
    case 1:
      strcat(display_text, "+ - ret del Q");
      break;
      
    case 2:
      strcat(display_text, "+ - ret del Q");
      break;
      
    case 3:
      strcat(display_text, "+ - ret del Q");
      break;
      
    case 4:
      strcat(display_text, "+ - ret del Q");
      break;
      
    case 5:
      strcat(display_text, "+ - ret del Q");
      break;
      
    default:
      break;
      
    }; // switch
    break;
    
  }; // switch

  strcat(display_text, " | ");

  mvaddstr(height-1, width-size_text_mem_usage-strlen(display_text)-2, display_text);
  SetSeqColor(SCOL_STATUS_LINE);

  if((active_status_line == 0) && (cursor_column == 5))
    snprintf(status_text, sizeof(status_text)-1, "(Step-by-Step mode)");
  else {
    if(strcmp(status_text, "(Step-by-Step mode)") == 0)
      snprintf(status_text, sizeof(status_text)-1, "");
  } // else

  if(p_seq->GetDeviceDetected()) {
    p_seq->SetDeviceDetectedNoReport();
    snprintf(status_text, sizeof(status_text)-1, "Midi device detected");
    } // if

  refresh();
} // RefreshStatusText

void SetStatusText(char* p_text)
{
  snprintf(status_text, sizeof(status_text)-1, "%s", p_text);

  RefreshStatusText();
} // SetStatusText

void RedrawScreen()
{
  int width, height;

  getmaxyx(stdscr, height, width);

  curs_set(0); // hide cursor

  // draw top line according to menu being active/inactive

  if(display_top_line)
    SetSeqColor(SCOL_STATUS_LINE);

  for(int i = 0; i < width; i++)
    mvaddstr(0, i, " ");

  if(display_top_line) {
    mvaddstr(0, 1, "Press 2 x ESC for menu");  
  } // if

  SetSeqColor(SCOL_NORMAL);

  RefreshStatusText();
  RedrawTrackDisplay();
  refresh();
} // RedrawScreen

static int displayCallback (EObjectType cdktype GCC_UNUSED, void *object, void *clientData, chtype input GCC_UNUSED)
{
  CDKMENU *menu        = (CDKMENU *)object;

  RedrawScreen();
  drawCDKMenu (menu, true);
  drawCDKMenuSubwin(menu);

  return 0;
} // displayCallback

uint16 ChannelRecSelect(CDKSCREEN *p_cdk_screen, uint16 channels)
{
  uint16 ret_channels = channels;
  char*  selection_list[] = {"Channel  1", "Channel  2", "Channel  3", "Channel  4", "Channel  5", "Channel  6", "Channel  7", "Channel  8", "Channel  9", "Channel 10", "Channel 11", "Channel 12", "Channel 13", "Channel 14", "Channel 15", "Channel 16"};
  char*  choices[] = {"    [ ] ", "    [*] "};

  CDKSELECTION *widget = newCDKSelection (p_cdk_screen, CENTER, CENTER, NONE, 20, 26, "  Recorded midi-channels\n", selection_list, 16, choices, 2, A_REVERSE, TRUE, FALSE);

  // set selection entries according to existing channel bits

  for(int i = 0; i < 16; i++) {
    if(channels & 0x01)
      setCDKSelectionChoice(widget, i, 1);

    channels >>= 1;
  } // for

  activateCDKSelection (widget, 0);

  if(widget->exitType == vESCAPE_HIT) goto exit_ChanSel;

  // set midid channel bits according to selection

  ret_channels = 0;

  for(int i = 15; i >= 0; i--) {
    if(getCDKSelectionChoice(widget, i))
      ret_channels |= 1;
      
    if(i > 0) ret_channels <<= 1;
  } // for

exit_ChanSel:

  eraseCDKSelection(widget);
  destroyCDKSelection(widget);
  RedrawScreen();

  return ret_channels;
} // ChannelRecSelect

int PlaySelectCallback(EObjectType type, void* object, void* clientData, chtype)
{
  for(int i = 0; i < 16; i++)
    setCDKSelectionChoice((CDKSELECTION*)object, i, 0);

  setCDKSelectionChoice((CDKSELECTION*)object, getCDKSelectionCurrent((CDKSELECTION*)object), 1);
 
  return 1;
} // PlaySelectCallback

int16_t ChannelPlaySelect(CDKSCREEN *p_cdk_screen, int16_t channel)
{
  int16_t ret_channel = channel;
  char*   selection_list[] = {"Multi channel", "Channel  1", "Channel  2", "Channel  3", "Channel  4", 
			      "Channel  5", "Channel  6", "Channel  7", "Channel  8", "Channel  9", 
			      "Channel 10", "Channel 11", "Channel 12", "Channel 13", "Channel 14", 
			      "Channel 15", "Channel 16"};
  char*   choices[] = {"    [ ] ", "    [*] "};

  CDKSELECTION *widget = newCDKSelection (p_cdk_screen, CENTER, CENTER, NONE, 20, 26, 
					  "  Send on midi-channel\n", selection_list, 16, choices, 2, 
					  A_REVERSE, TRUE, FALSE);

  // set selection entries according to existing channel bits

  setCDKSelectionPostProcess (widget, PlaySelectCallback, NULL);
  setCDKSelectionChoice(widget, channel+1, 1);

  activateCDKSelection (widget, 0);

  if(widget->exitType == vESCAPE_HIT) goto exit_ChanSel2;

  // set midid channel bits according to selection

  ret_channel = 0;

  for(int i = 16; i >= 0; i--) {
    if(getCDKSelectionChoice(widget, i)) {
      ret_channel = i-1;
      break;
    } // if
  } // for

exit_ChanSel2:

  eraseCDKSelection(widget);
  destroyCDKSelection(widget);
  RedrawScreen();

  return ret_channel;
} // ChannelPlaySelect

int QuantizeCallback(EObjectType type, void* object, void* clientData, chtype)
{
  setCDKRadioSelectedItem((CDKRADIO*)object, getCDKRadioCurrentItem((CDKRADIO*)object));

  return 1;
} // QuantizeCallback

void QuantizeTrack(CDKSCREEN *p_cdk_screen)
{
  int   selection;
  long  denum;
  char* selection_list[] = {" 1/1", " 1/2", " 1/4", " 1/8", " 1/16", " 1/32"};

  CDKRADIO *widget = newCDKRadio(p_cdk_screen, CENTER, CENTER, NONE, 12, 20, "     Quantize track\n", selection_list, 6, '*', 3, A_REVERSE, TRUE, FALSE);

  setCDKRadioCurrentItem (widget, 3);

  setCDKRadioPostProcess (widget, QuantizeCallback, NULL);
  setCDKRadioLeftBrace(widget, '(');
  setCDKRadioRightBrace(widget, ')');

  selection = activateCDKRadio (widget, 0);

  if(widget->exitType == vESCAPE_HIT) goto exit_Quantization;

  // quantize current track according to selection

  denum = (long)pow(2, selection);

  p_seq->GetActiveTrack()->Quantize((long)(4*10000*60/(100*denum)), 
					p_seq->GetPunchIn(), p_seq->GetPunchOut());

exit_Quantization:

  eraseCDKSelection(widget);
  destroyCDKSelection(widget);
  RedrawScreen();
} // QuantizeTrack

long GetSongLocation(long location, char* text, CDKSCREEN* p_cdk_screen)
{
  CDKENTRY  *p_entry;
  char      temp_string[80];
  long      quarters, beats;

  quarters = location/(100*60);

 cont_GetSongLocation:
  p_entry = newCDKEntry (p_cdk_screen, 10, 3,
			 "", text, A_NORMAL, '.', vMIXED,
			 16, 0, 256, TRUE, FALSE);
  drawCDKEntry(p_entry, true);
  sprintf(temp_string, "%03ld:%ld", quarters / p_seq->GetQuartersPerBeat(), quarters % p_seq->GetQuartersPerBeat()+1);
  setCDKEntryValue(p_entry, temp_string);
  curs_set(1);
  popup_active = true;
  activateCDKEntry(p_entry, NULL);
  popup_active = false;
  sscanf(getCDKEntryValue(p_entry), "%ld:%ld", &location, &beats);

  if((beats < 1) || (beats > 4)) goto cont_GetSongLocation;
  location = location*(100*60*p_seq->GetQuartersPerBeat()) + (beats-1)*100*60;

  if(location < 0) goto cont_GetSongLocation;

  eraseCDKEntry(p_entry);
  destroyCDKEntry(p_entry);
  RedrawScreen();
  curs_set(0);

  return location;
} // GetSongLocation

bool PopupYesCancel(char* message, int pos_x, int pos_y, CDKSCREEN* p_cdk_screen, bool ok_only = false)
{
  CDKDIALOG* p_dialog;
  char* buttons[] = {"Ok", "Cancel"};
  int   selection;
  bool  ret = true;
  int   no_buttons;

  if(ok_only) no_buttons = 1;
  else no_buttons = 2;

  p_dialog = newCDKDialog(p_cdk_screen, 
			  pos_x, pos_y, 
			  &message, 1, buttons, no_buttons,
			  COLOR_PAIR(2)|A_REVERSE,
			  TRUE, TRUE, FALSE);

  popup_active = true;
  selection = activateCDKDialog(p_dialog, NULL);
  popup_active = false;

  if((p_dialog->exitType == vESCAPE_HIT) || (selection == 1)) ret = false; 

  destroyCDKDialog(p_dialog);
  RedrawScreen();

  return ret;
} // PopupYesCancel

void PopupAbout(int pos_x, int pos_y, CDKSCREEN* p_cdk_screen)
{
  CDKDIALOG* p_dialog;
  char* buttons[] = {"Ok"};
  char  *message[8];

  message[0] = "<C></B>Cursed Sequencer<!B>, version 0.5,";
  message[1] = "<C>Copyright (C) 2006 by Thomas Haenselmann";
  message[2] = "";
  message[3] = "<C>This program is distributed free of charge under the terms of";
  message[4] = "<C>the GNU General Public Licence with ABSOLUTELY NO WARRANTY.";
  message[5] = "";
  message[6] = "<C>Please send questions and bug ";
  message[7] = "<C>reports to thomas at haenselmann.de";

  p_dialog = newCDKDialog(p_cdk_screen, 
			  pos_x, pos_y, 
			  message, 8, buttons, 1,
			  COLOR_PAIR(2)|A_REVERSE,
			  TRUE, TRUE, FALSE);

  popup_active = true;
  activateCDKDialog(p_dialog, NULL);
  popup_active = false;

  destroyCDKDialog(p_dialog);
  RedrawScreen();
} // PopupAbout

void OpenMidiFile(CDKSCREEN* p_cdk_screen)
{
  Track      *p_work_track;
  CDKFSELECT *fSelect = 0;
  char       *p_filename;
  ifstream   *p_midi_file;

  fSelect = newCDKFselect (p_cdk_screen,
			   CENTER, CENTER, 20, 65, "<C>Open midi file\n", "File: ", A_NORMAL, '_', A_REVERSE,
			       "", "", "", "", TRUE, FALSE);
  popup_active = true;
  p_filename = activateCDKFselect (fSelect, 0);
  popup_active = false;
  
  if(p_filename) strncpy(filename, p_filename, MAX_FILENAME);
  
  eraseCDKMenu (fSelect);
  destroyCDKMenu (fSelect);
  
  if(p_filename != 0) {
    
    p_midi_file = new ifstream(filename);
    p_seq->LoadMidiFile(p_midi_file);
    
    for(p_work_track = Track::GetFirst(); p_work_track != NULL; 
	p_work_track = p_work_track->GetNext())
      p_work_track->JumpToStart();
  } // if
} // OpenMidiFile

void SaveMidiFile(CDKSCREEN* p_cdk_screen)
{
  CDKFSELECT *fSelect = 0;
  char       *p_filename;

  fSelect = newCDKFselect (p_cdk_screen,
			   CENTER, CENTER, 20, 65, "<C>Save midi file\n", "File: ", A_NORMAL, '_', A_REVERSE,
			       "", "", "", "", TRUE, FALSE);
  popup_active = true;
  p_filename = activateCDKFselect (fSelect, 0);
  popup_active = false;

  if(p_filename) strncpy(filename, p_filename, MAX_FILENAME);
  
  eraseCDKMenu (fSelect);
  destroyCDKMenu (fSelect);
  
  if(p_filename != 0) {
    
    if(strcmp(&(filename[strlen(filename)-4]), ".mid") != 0)
      strcat(filename, ".mid");

    ifstream test_file(filename);

    if(test_file.good()) {

      int x_pos, y_pos;

      x_pos = 10;
      CalcOptBoxPosition(p_seq->GetActiveTrack(), p_first_displayed_track, x_pos, y_pos);
      if(PopupYesCancel(" Overwrite existing file?", 
			x_pos, y_pos, p_cdk_screen) == false) return;
    } // if

    ofstream midi_file(filename);
    p_seq->GenerateMidiFile(&midi_file);
  } // if
} // SaveMidiFile

void CheckCursorLocation(short& status_line, short& cursor_col)
{
  if(status_line > 3) status_line = 3;
  if(status_line < 0) status_line = 0;

  switch(status_line) {
  case 0:
    if(cursor_col < 1) cursor_col = 5;
    if(cursor_col > 5) cursor_col = 1;
    break;

  case 1:
    if(cursor_col < 1) cursor_col = 3;
    if(cursor_col > 3) cursor_col = 1;
    break;

  case 2:
    if(cursor_col < 1) cursor_col = 5;
    if(cursor_col > 5) cursor_col = 1;
    break;

  case 3:
    if(cursor_col < 1) cursor_col = 2;
    if(cursor_col > 2) cursor_col = 1;
    break;
  }; // switch

  // if we are in a track-line and in column 5 this
  // means automatically that we are in step-by-step
  // record mode

  if((cursor_col == 5) && (status_line == 0)) {
    p_seq->SetRecStepByStep(true);
  } // if
  else p_seq->SetRecStepByStep(false);
} // CheckCursorLocation

int main(int argc, char** argv)
{
  char      *midi_device_file, temp_string[80];
  int       delta_notes;
  float     delta_time;
  short     key_esc_seq;
  WINDOW    *p_curses_win;
  CDKSCREEN *p_cdk_screen;
  CDKENTRY  *p_entry;
  int       x_pos, y_pos, key;
  Track     *p_work_track;

  CDKMENU   *menu = 0;
  int       submenusize[7], menuloc[7];
  int       selection;
  float     temp_qpm;
  long      temp_qpb;

  p_seq                = NULL;
  sequencer_debug_info = false;

  strcpy(filename, ""); // no filename opened or saved so far
  strcpy(status_text, "\0");

  #ifdef __CYGWIN__
  
  if (argc != 3)
  {
    UINT iNumDevs, i;
    MMRESULT Res;
    MIDIINCAPS InDevCaps;
    MIDIOUTCAPS OutDevCaps;

    cerr << "usage: " << argv[0] << " <input_device> <output_device>" << endl;
    
    cerr << endl << "available input devices:" << endl;
    iNumDevs = midiInGetNumDevs();
    for (i = 0; i < iNumDevs; i++)
    {
        Res = midiInGetDevCaps(i, &InDevCaps, sizeof(MIDIINCAPS));
        if (Res == MMSYSERR_NOERROR)
            cerr << " " << i << " " << InDevCaps.szPname << endl;
    }
  
    cerr << endl << "available output devices:" << endl;
    iNumDevs = midiOutGetNumDevs();
    for (i = 0; i < iNumDevs; i++)
    {
        Res = midiOutGetDevCaps(i, &OutDevCaps, sizeof(MIDIOUTCAPS));
        if (Res == MMSYSERR_NOERROR)
            cerr << " " << i << " " << OutDevCaps.szPname << endl;
    }

    exit(-1);
  }

  midi_in_stream midi_in(atoi(argv[1]));
  midi_out_stream midi_out(atoi(argv[2]));

  #else

  if(argc != 2) {
    cerr << "usage: " << argv[0] << " /dev/midi_device" << endl;
    cerr << "if you have no MIDI device you may use /dev/null" << endl;
    exit(-1);
  } // if

  midi_device_file = argv[1];
  midi_in_stream midi_in(midi_device_file);

  if(midi_in.bad()) {
    cerr << "Could not open midi device for input: " << midi_device_file << endl;
    cerr << "exiting..." << endl;
    exit(1);
  } // if

  midi_out_stream midi_out(midi_device_file);

  if(!midi_out.good()) {
    cerr << "Could not open midi device for output: " << midi_device_file << endl;
    cerr << "exiting..." << endl;
    exit(1);
  } // if

  #endif

  p_seq = new Sequencer(midi_in, midi_out);

  p_seq->SetQuartersPerMinute(120);      // 120 quarters per minute default metronome speed
  p_seq->SetResolveQuarters(2);          // resolve note display in 8 parts per beat
  p_seq->SetQuartersPerBeat(4);          // 4 quarters beat is default
  p_seq->SetSongOffset(0);               // start song from beginning
  p_seq->SetRecStepByStepNoteLength(8);  // add 1/8 notes
  p_seq->SetRecStepByStepAdvanceTime(4); // advance by 1/4
  // p_seq->SetKbdESCMode(false);        // key are not interpreted as commands

  if(!p_seq) {
    cerr << "Could not allocate Sequencer()" << endl;
    exit(-1);
  } // if

  p_seq->Init();

  display_top_line   = true;
  popup_active       = false;
  cursor_column      = 1;
  key_esc_seq        = 0; // no special key pressed so far
  active_status_line = 0;

  /* Set up the menu. */

  menulist[0][0] = "File  "; 
  menulist[0][1] = "Open      (C-o)"; 
  menulist[0][2] = "Save      (C-f)"; 
  menulist[0][3] = "Save As        "; 
  menulist[0][4] = "Quit      (q)  ";

  menulist[1][0] = "Realtime  ";
  menulist[1][1] = "Stop           (s)";
  menulist[1][2] = "Play/Pause     (p)";
  menulist[1][3] = "Record         (r)";
  menulist[1][4] = "Step forward   (+)";
  menulist[1][5] = "Step backward  (-)";
  menulist[1][6] = "Set Punch-In   (1)";
  menulist[1][7] = "Set Punch-Out  (2)";

  menulist[2][0] = "Track  ";
  menulist[2][1] = "Duplicate   (d)";
  menulist[2][2] = "Delete      (Del)";
  menulist[2][3] = "Quantize    (Q)";
  menulist[2][4] = "Erase beats (e)";
  menulist[2][5] = "Copy beats  (c)";
  menulist[2][6] = "Transpose   (t)";
  menulist[2][7] = "Time shift  (i)";

  menulist[3][0] = "Settings  ";
  menulist[3][1] = "Toggle Play beep    (b)";
  menulist[3][2] = "Toggle Rec beep     (B)";
  menulist[3][3] = "Toggle rec mode     (o)";
  menulist[3][4] = "Cursor to speed     (C-p)";
  menulist[3][5] = "Cursor to locator   (C-l)";
  menulist[3][6] = "Cursor to Punch-In  (C-i)";
  menulist[3][7] = "Cursor to Punch-Out (C-u)";

  menulist[4][0]  = "Accord-CMD  ";
  menulist[4][1]  = "ESC/Pause            ";
  menulist[4][2]  = "Play                 ";
  menulist[4][3]  = "Rec                  ";
  menulist[4][4]  = "Stop/Rewind          ";
  menulist[4][5]  = "Track duplicate      ";
  menulist[4][6]  = "Track quantize       ";
  menulist[4][7]  = "Set Punch-In         ";
  menulist[4][8]  = "Set Punch-Out        ";

  menulist[4][9]   = "Track forward        ";
  menulist[4][10]  = "Track backward       ";
  menulist[4][11]  = "Toggle rec mode      ";
  menulist[4][12]  = "Toggle play beep     ";
  menulist[4][13]  = "Toggle rec  beep     ";
  menulist[4][14]  = "Set midi rec channel";
  menulist[4][15]  = "Set midi play channel ";

  menulist[4][16]  = "No  #0";
  menulist[4][17]  = "No  #1";
  menulist[4][18]  = "No  #2";
  menulist[4][19]  = "No  #3";
  menulist[4][20]  = "No  #4";
  menulist[4][21]  = "No  #5";
  menulist[4][22]  = "No  #6";
  menulist[4][23]  = "No  #7";
  menulist[4][24]  = "No  #8";
  menulist[4][25]  = "No  #9";
  menulist[4][26]  = "No #10";
  menulist[4][27]  = "No #11";
  menulist[4][28]  = "No #12";
  menulist[4][29]  = "No #13";
  menulist[4][30]  = "No #14";
  menulist[4][31]  = "No #15";

  menulist[5][0] = "Help  ";
  menulist[5][1] = "About...";

  submenusize[0] =  5;   menuloc[0] = LEFT;
  submenusize[1] =  8;   menuloc[1] = LEFT;
  submenusize[2] =  8;   menuloc[2] = LEFT;
  submenusize[3] =  8;   menuloc[3] = LEFT;
  submenusize[4] =  32;  menuloc[4] = LEFT;
  submenusize[5] =  2;   menuloc[5] = RIGHT;

  p_first_displayed_track = p_last_displayed_track = NULL;
  
  p_curses_win = initscr();
  p_cdk_screen = initCDKScreen(p_curses_win);
  // atexit(quit);
  curs_set(0); // hide cursor
  noecho();
  clear();

  start_color();

  // 0 = std. char. color / 1 = red / 2 = green / 3 = rot / 4 = blau / 5 = magenta / 6 = turquoise / 7 = gray / 8 = blue

  init_pair(2, COLOR_WHITE, COLOR_BLACK);
  init_pair(3, COLOR_YELLOW, COLOR_BLUE);
  init_pair(4, COLOR_YELLOW, COLOR_BLUE);

  SetSeqColor(SCOL_NORMAL);

  RedrawScreen();
  refresh();

  while(true) {

    if(key_esc_seq == 3) key_esc_seq = 0;

    // while(true) {
    key = getch(); // p_curses_win);

    //    cerr << (int)key << endl;
    
    //    if(key_esc_seq == 1) {
    // cout << "(" << (int)key << ")" << endl;
      //      exit(0);
      //    } // if
    
    // } // while
    
    if(key_esc_seq > 0) key_esc_seq++;
    else
      if(key == 27) key_esc_seq = 1;

    switch(key) {

    case 9: // ctrl-i (Cursor to Punch-In)
      active_status_line = 2;
      cursor_column = 4;
      break;

    case 12: // ctrl-l (Cursor to locator)
      active_status_line = 2;
      cursor_column = 1;
      break;

    case 16: // ctrl-p (Cursor to speed)
      active_status_line = 1;
      cursor_column = 1;
      break;

    case 21: // ctrl-u (Cursor to Punch-Out)
      active_status_line = 2;
      cursor_column = 5;
      break;

    case 6:
      SaveMidiFile(p_cdk_screen);
      break;

    case 10:                   // return hit
      
      switch(active_status_line) {

      case 0:
	
	// return was hit over the track display
	
	switch(cursor_column) {
	case 1:
	  ungetch('n');
	  break;
	  
	case 2:
	  ungetch('C');
	  break;
	  
	case 3:
	  ungetch('h');
	  break;

	case 4:
	  ungetch('m');
	  break;
	}; // switch
	break;

      case 1:

	// return was hit over a status control

	switch(cursor_column) {
	case 1:
	  ungetch('S');
	  break;

	case 2:
	  ungetch(73); // issue key for time signature
	  break;

	case 3:
	  ungetch('o');
	  break;

	}; // switch
	break;

      case 2:

	// return was hit over a status control

	switch(cursor_column) {
	case 1:
	  p_seq->SetSongOffset(GetSongLocation(p_seq->GetSongOffset(), " location: ", p_cdk_screen));
	  break;
	  
	case 2:
	  ungetch('b');
	  break;

	case 3:
	  ungetch('B');
	  break;

	case 4:
	  p_seq->SetPunchIn(GetSongLocation(p_seq->GetPunchIn(), " punch in: ", p_cdk_screen));
	  break;
	  
	case 5:
	  p_seq->SetPunchOut(GetSongLocation(p_seq->GetPunchOut(), " punch out: ", p_cdk_screen));
	  break;

	}; // switch
	break;
      }; // switch
      break;

    case 15:                   // CRTL + o
      OpenMidiFile(p_cdk_screen);
      RedrawScreen();      
      break;

    case 24:                   // q
      goto terminate_program;

    case 68:                   // cursor left
      if(key_esc_seq == 3) {
	cursor_column--;
	CheckCursorLocation(active_status_line, cursor_column);

	p_seq->GetActiveTrack()->MarkNoteDisplayDirty();
	RedrawScreen();
      } // if
      break;
      
    case 67:                   
      if(key_esc_seq == 3) {   // cursor right
	cursor_column++;
	CheckCursorLocation(active_status_line, cursor_column);

	p_seq->GetActiveTrack()->MarkNoteDisplayDirty();
	RedrawScreen();
      } // if
      else {                   // 'C'

	// Select midi record channels

	if(p_seq->GetActiveTrack()) {
	  popup_active = true;
	  p_seq->GetActiveTrack()->SetMidiRecChannels(ChannelRecSelect(p_cdk_screen, p_seq->GetActiveTrack()->GetMidiRecChannels()));
	  popup_active = false;
	} // if
	
      } // else
      break;
      
    case 66:                   // cursor down
      if(key_esc_seq == 3) {

	// active status line points to a line above the track display

	active_status_line--;
	if(active_status_line < 0) {
	  CheckCursorLocation(active_status_line, cursor_column);

	  p_work_track = p_seq->GetActiveTrack();
	  p_seq->MoveNextTrack();
	  p_work_track->MarkNoteDisplayDirty();
	  p_seq->GetActiveTrack()->MarkNoteDisplayDirty();
	  
	  // redisplay is necessary because we may be scrolled
	  
	  Track::MarkAllTracksDirty();
	} // if
      } // if
      else {                   // B hit
	p_seq->SetRecBeep(!p_seq->GetRecBeep());
	p_seq->beeper.Beep(Sequencer::Beeper::BEEP_OFF);
      } // else
      break;
      
    case 65:                   // cursor up
      if(key_esc_seq == 3) {
	p_work_track = p_seq->GetActiveTrack();
	if(p_seq->MovePrevTrack() == false) {
	  active_status_line++;
	  CheckCursorLocation(active_status_line, cursor_column);
	} // if
	p_work_track->MarkNoteDisplayDirty();
	p_seq->GetActiveTrack()->MarkNoteDisplayDirty();

	// redisplay is necessary because we may be scrolled

	Track::MarkAllTracksDirty();

      } // if
      break;
      
    case 27:                   // esc pressed
      if(key_esc_seq == 2) {
	key_esc_seq = 0;

	popup_active = true;

	/* Create the menu. */

	menu = newCDKMenu (p_cdk_screen, menulist, 6, submenusize, menuloc,
			   TOP, A_UNDERLINE, A_REVERSE);

	setCDKMenuPostProcess (menu, displayCallback, NULL);

	refreshCDKScreen (p_cdk_screen);
	
	display_top_line = false;
       	RedrawScreen();

	selection = activateCDKMenu (menu, 0);

	popup_active = false;
	display_top_line = true;

	switch(selection / 100) {
 	case 0:                               // File Menu
	  switch(selection % 100) {
	  case 0:                             // open midi file
	    ungetch(15);
	    break;

	  case 1:                             // save midi file
	    if(strlen(filename) > 0)
	      p_seq->GenerateMidiFile(&(ofstream(filename)));
	    else
	      ungetch(6);
	    break;

	  case 2:                             // Save midi file as
	    ungetch(6);
	    break;

	  case 3:
	    ungetch('q');
	    //	    goto terminate_program;
	    break;

	  }; // switch
	  break;

 	case 1:                               // Realtime Menu
	  switch(selection % 100) {

	  case 0:
	    ungetch('s');
	    break;
	    
	  case 1:
	    ungetch('p');
	    break;
	    
	  case 2:
	    ungetch('r');
	    break;
	    
	  case 3:
	    ungetch('+');
	    break;
	    
	  case 4:
	    ungetch('-');
	    break;
	    
	  case 5:
	    ungetch('1');
	    break;

	  case 6:
	    ungetch('2');
	    break;

	  } // switch
	  break;
	  break;

	case 2:                             // track menu
	  switch(selection % 100) {

	  case 0:
	    ungetch('d');
	    break;
	    
	  case 1:
	    ungetch(127);
	    break;
	    
	  case 2:
	    ungetch('Q');
	    break;
	    
	  case 3:                           // erase beats
	    ungetch('e');
	    break;
	    
	  case 4:                           // copy beats
	    ungetch('c');
	    break;
	    
	  case 5:
	    ungetch('t');                   // transpose
	    break;

	  case 6:
	    ungetch('i');                   // time shift
	    break;

	  } // switch
	  break;

	case 3:                             // Settings Menu
	  switch(selection % 100) {

	  case 0:
	    ungetch('b');
	    break;
	    
	  case 1:
	    ungetch('B');
	    break;
	    
	  case 2:
	    ungetch('o'); // toggle record mode
	    break;
	    
	  case 3:
	    ungetch(16); // cursor to speed
	    break;
	    
	  case 4:
	    ungetch(12); // cursor to locator
	    break;
	    
	  case 5:
	    ungetch(9); // cursor to punch-in
	    break;
	    
	  case 6:
	    ungetch(21); // cursor to punch-out
	    break;
	    
	  } // switch
	  break;

	case 4:                             // Accord-CMD Menu
	  p_seq->SetNextKeyCombination(selection % 100);
	  break;

	case 5:                             // Help Menu
	  PopupAbout(3, 3, p_cdk_screen);
	  break;

	}; // switch

	eraseCDKMenu (menu);
	destroyCDKMenu (menu);

       	RedrawScreen();
	key_esc_seq = 0;
      } // if
      break;

    case '1':
      p_seq->SetPunchIn((long)(p_seq->GetCurrentSongTime()));
      if(p_seq->GetPunchIn() > p_seq->GetPunchOut())
	p_seq->SetPunchOut((long)(p_seq->GetCurrentSongTime()));
      break;
      
    case '2':
      p_seq->SetPunchOut((long)(p_seq->GetCurrentSongTime()));
      if(p_seq->GetPunchOut() < p_seq->GetPunchIn())
	p_seq->SetPunchIn((long)(p_seq->GetCurrentSongTime()));
      break;
      
    case 73: // shift i
      p_entry = newCDKEntry (p_cdk_screen, 10, 3,
			     "", " time signature (2-12)/4: ", A_NORMAL, '.', vMIXED,
			     16, 0, 256, TRUE, FALSE);
      drawCDKEntry(p_entry, true);
      sprintf(temp_string, "%ld", p_seq->GetQuartersPerBeat());
      setCDKEntryValue(p_entry, temp_string);
      curs_set(1);
      popup_active = true;
      activateCDKEntry(p_entry, NULL);
      popup_active = false;
      if(sscanf(getCDKEntryValue(p_entry), "%ld", &temp_qpb) > 0) {
	if(temp_qpb < 2) temp_qpb = 2;
	if(temp_qpb > 12) temp_qpb = 12;
	p_seq->SetQuartersPerBeat(temp_qpb);
      } // if
      eraseCDKEntry(p_entry);
      destroyCDKEntry(p_entry);
      RedrawScreen();
      curs_set(0);
      break;
      
    case 'm':
      p_seq->GetActiveTrack()->SetMute(!p_seq->GetActiveTrack()->GetMute());
      break;

    case 'd':
	p_seq->GetActiveTrack()->CopyTrackTo(Track::GetFirstFreeTrack(p_seq));
	SetStatusText("Active track duplicated");
	RedrawTrackDisplay();
      break;

    case 'e': // erase beats
      if(PopupYesCancel(" Erase beats between Punch-In/-Out?", 3, 3, p_cdk_screen) == true) {
	p_seq->GetActiveTrack()->EraseBeats(p_seq->GetPunchIn(), p_seq->GetPunchOut());
	RedrawTrackDisplay();
	SetStatusText("Beats between Punch-In and Punch-Out erased");
      } // if
      break;

    case 'c': // copy beats
      if(PopupYesCancel(" Copy beats between Punch-In/-Out to locator?", 3, 3, p_cdk_screen) == true) {
	p_seq->GetActiveTrack()->CopyBeats(p_seq->GetSongOffset(), p_seq->GetPunchIn(), p_seq->GetPunchOut());
	RedrawTrackDisplay();
	SetStatusText("Beats between Punch-In and Punch-Out copied");
      } // if
      break;

    case 't': // transpose track
      p_entry = newCDKEntry (p_cdk_screen, 10, 3,
			     " transpose track between punch-in and punch-out ", "+/- notes : ", A_NORMAL, '.', vMIXED,
			     16, 0, 256, TRUE, FALSE);
      drawCDKEntry(p_entry, true);
      sprintf(temp_string, "%d", 0);
      setCDKEntryValue(p_entry, temp_string);
      curs_set(1);
      popup_active = true;
      activateCDKEntry(p_entry, NULL);
      popup_active = false;

      if(sscanf(getCDKEntryValue(p_entry), "%d", &delta_notes) > 0) {
	p_seq->GetActiveTrack()->Transpose(p_seq->GetPunchIn(), p_seq->GetPunchOut(), (char)delta_notes);
	RedrawTrackDisplay();
	SetStatusText("Notes between Punch-In and Punch-Out transposed");
      } // if

      eraseCDKEntry(p_entry);
      destroyCDKEntry(p_entry);
      RedrawScreen();
      curs_set(0);
      break;

    case 'i': // time shift
      p_entry = newCDKEntry (p_cdk_screen, 10, 3,
			     " shift notes between punch-in and punch-out ", "+/- no. quarter notes (fractions possible): ", A_NORMAL, '.', vMIXED,
			     16, 0, 256, TRUE, FALSE);
      drawCDKEntry(p_entry, true);
      sprintf(temp_string, "%1.1f", 0);
      setCDKEntryValue(p_entry, temp_string);
      curs_set(1);
      popup_active = true;
      activateCDKEntry(p_entry, NULL);
      popup_active = false;

      if(sscanf(getCDKEntryValue(p_entry), "%f", &delta_time) > 0) {
	p_seq->GetActiveTrack()->TimeShift(p_seq->GetPunchIn(), p_seq->GetPunchOut(), (long)(delta_time*10000*60/100));
	RedrawTrackDisplay();
	SetStatusText("Notes between punch-in and punch-out time-shifted");
      } // if

      eraseCDKEntry(p_entry);
      destroyCDKEntry(p_entry);
      RedrawScreen();
      curs_set(0);
      break;

    case 'h':
      // Select midi play channel

      if(p_seq->GetActiveTrack()) {
	popup_active = true;
	p_seq->GetActiveTrack()->SetMidiPlayChannel(ChannelPlaySelect(p_cdk_screen, p_seq->GetActiveTrack()->GetMidiPlayChannel()));
	popup_active = false;
      } // if
      break;
      
    case 'r':
      if(!p_seq->GetPlay()) {
	if(!p_seq->GetRec()) {
	  p_seq->StartRecord();
	  
	  SetStatusText("Sequencer recording...");
	} // if
      } // if
      else
	SetStatusText("No recording while playing...");
      break;
      
    case 'o':
      p_seq->SetRecOverwrite(!p_seq->GetRecOverwrite());
      break;

    case 's':
      SetStatusText("Sequencer stopped");
      p_seq->StopPlay();
      Track::MarkAllTracksDirty();
      break;

    case 'S':
      p_entry = newCDKEntry (p_cdk_screen, 10, 3,
			     "", " speed: ", A_NORMAL, '.', vMIXED,
			     16, 0, 256, TRUE, FALSE);
      drawCDKEntry(p_entry, true);
      sprintf(temp_string, "%.1f", p_seq->GetQuartersPerMinute());
      setCDKEntryValue(p_entry, temp_string);
      curs_set(1);
      popup_active = true;
      activateCDKEntry(p_entry, NULL);
      popup_active = false;
      if(sscanf(getCDKEntryValue(p_entry), "%f", &temp_qpm) > 0) {
	if(temp_qpm < 1) temp_qpm = 1;
	if(temp_qpm > 500) temp_qpm = 500;
	p_seq->SetQuartersPerMinute((double)temp_qpm);
      } // if
      eraseCDKEntry(p_entry);
      destroyCDKEntry(p_entry);
      RedrawScreen();
      curs_set(0);
      break;

    case ' ':
      switch(active_status_line) {

      case 1:
	switch(cursor_column) {
	case 1:
	  p_seq->SetQuartersPerMinute(p_seq->GetQuartersPerMinute()+1);
	  break;

	case 2:
	  break;

	case 3:
	  ungetch('o');
	  break;
	}; // switch
	
      case 2:
	switch(cursor_column) {

	case 1:
	  p_seq->SetSongOffset((long)((double)p_seq->GetSongOffset()+10000.0*60.0/100.0)+1);
	  break;

	case 4:
	  p_seq->SetPunchIn((long)((double)p_seq->GetPunchIn()+10000.0*60.0/100.0)+1);
	  break;

	case 5:
	  p_seq->SetPunchOut((long)((double)p_seq->GetPunchOut()+10000.0*60.0/100.0)+1);
	  break;

	}; // switch
	break;

      default:
	break;

      }; // switch
      break;

    case '+':
      switch(active_status_line) {

      case 1:
	if(cursor_column == 1)
	  p_seq->SetQuartersPerMinute(p_seq->GetQuartersPerMinute()+5);
	else ungetch('o');
	break;
	
      case 2:
	switch(cursor_column) {

	case 1:
	  p_seq->SetSongOffset((long)((double)p_seq->GetSongOffset()+10000.0*60.0*(double)p_seq->GetQuartersPerBeat()/100.0));
	  break;

	case 4:
	  p_seq->SetPunchIn((long)((double)p_seq->GetPunchIn()+10000.0*60.0*(double)p_seq->GetQuartersPerBeat()/100.0));
	  break;

	case 5:
	  p_seq->SetPunchOut((long)((double)p_seq->GetPunchOut()+10000.0*60.0*(double)p_seq->GetQuartersPerBeat()/100.0));
	  break;

	}; // switch
	break;

      case 3:
	switch(cursor_column) {
	case 1:
	  p_seq->SetRecStepByStepNoteLength(p_seq->GetRecStepByStepNoteLength() * 2);
	  break;

	case 2:
	  p_seq->SetRecStepByStepAdvanceTime(p_seq->GetRecStepByStepAdvanceTime() * 2);
	  break;
	}; // switch
	break;

      default:
	p_seq->StepForward();
	break;

      }; // switch
      break;

    case '-':
      switch(active_status_line) {

      case 1:
	switch(cursor_column) {
	case 1:
	  p_seq->SetQuartersPerMinute(p_seq->GetQuartersPerMinute()-5);
	  break;

	case 2:
	  break;

	case 3:
	  ungetch('o');
	  break;
	}; // switch
	break;
	
      case 2:
	switch(cursor_column) {

	case 1:
	  p_seq->SetSongOffset((long)((double)p_seq->GetSongOffset()-10000.0*60.0*(double)p_seq->GetQuartersPerBeat()/100.0));
	  break;

	case 4:
	  p_seq->SetPunchIn((long)((double)p_seq->GetPunchIn()-10000.0*60.0*(double)p_seq->GetQuartersPerBeat()/100.0));
	  break;

	case 5:
	  p_seq->SetPunchOut((long)((double)p_seq->GetPunchOut()-10000.0*60.0*(double)p_seq->GetQuartersPerBeat()/100.0));
	  break;

	}; // switch
	break;

      case 3:
	switch(cursor_column) {
	case 1:
	  p_seq->SetRecStepByStepNoteLength(p_seq->GetRecStepByStepNoteLength() / 2);
	  break;

	case 2:
	  p_seq->SetRecStepByStepAdvanceTime(p_seq->GetRecStepByStepAdvanceTime() / 2);
	  break;
	}; // switch
	break;
	
      default:
	p_seq->StepBackward();
	break;

      }; // switch
      break;

    case 'p':
      if(!p_seq->GetPlay()) {

	// start or continue playing song

	p_seq->StartPlay();
	SetStatusText("Sequencer playing...");
      } // if
      else {

	// pause playing song

	SetStatusText("Sequencer paused");
	p_seq->Pause();
      } // else
      break;
      
    case 'n':
      x_pos = 10;
      CalcOptBoxPosition(p_seq->GetActiveTrack(), p_first_displayed_track, x_pos, y_pos);
      p_entry = newCDKEntry (p_cdk_screen, x_pos, y_pos,
			     "", " track name: ", A_NORMAL, '.', vMIXED,
			     30, 0, 256, TRUE, FALSE);
      drawCDKEntry(p_entry, true);
      setCDKEntryValue(p_entry, p_seq->GetActiveTrack()->GetName());
      curs_set(1);
      popup_active = true;
      activateCDKEntry(p_entry, NULL);
      popup_active = false;
      p_seq->GetActiveTrack()->SetName(getCDKEntryValue(p_entry));
      eraseCDKEntry(p_entry);
      destroyCDKEntry(p_entry);
      RedrawScreen();
      curs_set(0);
      break;

    case 'b': // play beep
      p_seq->SetPlayBeep(!p_seq->GetPlayBeep());
      p_seq->beeper.Beep(Sequencer::Beeper::BEEP_OFF);
      break;

    case 'Q':
      QuantizeTrack(p_cdk_screen);
      break;

    case 'q':
      if(PopupYesCancel(" Want to exit?", 3, 3, p_cdk_screen) == true) goto terminate_program;
      break;

    case '4':
      p_seq->SetRecStepByStepNoteLength(4);
      p_seq->SetRecStepByStepAdvanceTime(4);
      break;

    case '8':
      p_seq->SetRecStepByStepNoteLength(8);
      p_seq->SetRecStepByStepAdvanceTime(8);
      break;

    case '6':
      p_seq->SetRecStepByStepNoteLength(16);
      p_seq->SetRecStepByStepAdvanceTime(16);
      break;

    case 127:
      x_pos = 10;
      CalcOptBoxPosition(p_seq->GetActiveTrack(), p_first_displayed_track, x_pos, y_pos);
      if(PopupYesCancel(" Are you sure you want to delete this track?", 
			x_pos, y_pos, p_cdk_screen) == true) p_seq->GetActiveTrack()->DeleteAllEvents();
      break;
      
    case KEY_RESIZE:
      exit(0);
      clear();
      RedrawScreen();
      break;
    }; // switch
  } // while

terminate_program:

  delete p_seq;

  destroyCDKScreen (p_cdk_screen);
  endCDK();
  exit(0);
} // main
