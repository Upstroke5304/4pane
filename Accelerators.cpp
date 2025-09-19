/////////////////////////////////////////////////////////////////////////////
// Name:       Accelerators.cpp
// Purpose:    Accelerators and shortcuts
// Part of:    4Pane
// Author:     David Hart
// Copyright:  (c) 2020 David Hart
// Licence:    wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h" 

#include "wx/log.h"
#include "wx/wx.h"
#include "wx/config.h"
#include "wx/xrc/xmlres.h"

#include "Externs.h"
#include "Configure.h"
#include "Bookmarks.h"
#include "Accelerators.h"
#include "MyFrame.h"


wxString TranslateKeycode(int code)  // Make a string from the keycode  eg "F1" from WXK_F1.  Adapted from GetGtkHotKey() in src/gtk/menu.cpp, & the Text sample
{
wxString str;

switch (code)
        {
            case WXK_F1:  case WXK_F2:   case WXK_F3:  case WXK_F4:
            case WXK_F5:  case WXK_F6:   case WXK_F7:  case WXK_F8:
            case WXK_F9:  case WXK_F10: case WXK_F11: case WXK_F12:  str << wxT('F') << code - WXK_F1 + 1;  break;

            case WXK_INSERT:         str << wxT("Insert");    break;
            case WXK_DELETE:         str << wxT("Delete");    break;
            case WXK_UP:             str << wxT("Up");        break;
            case WXK_DOWN:           str << wxT("Down");      break;
            case WXK_LEFT:           str << wxT("Left");      break;
            case WXK_RIGHT:          str << wxT("Right");     break;
            case WXK_HOME:           str << wxT("Home");      break;
            case WXK_END:            str << wxT("End");       break;
            case WXK_RETURN:         str << wxT("Return");    break;
            case WXK_BACK:           str << wxT("Back"); break;
            case WXK_TAB:            str << wxT("Tab"); break;
            case WXK_ESCAPE:         str << wxT("Escape"); break;
            case WXK_SPACE:          str << wxT("Space"); break;
            case WXK_START:          str << wxT("Start"); break;
            case WXK_LBUTTON:        str << wxT("LButton"); break;
            case WXK_RBUTTON:        str << wxT("RButton"); break;
            case WXK_CANCEL:         str << wxT("Cancel"); break;
            case WXK_MBUTTON:        str << wxT("MButton"); break;
            case WXK_CLEAR:          str << wxT("Clear"); break;
            case WXK_SHIFT:          str << wxT("Shift"); break;
            case WXK_ALT:            str << wxT("Alt"); break;
            case WXK_CONTROL:        str << wxT("Control"); break;
            case WXK_MENU:           str << wxT("Menu"); break;
            case WXK_PAUSE:          str << wxT("Pause"); break;
            case WXK_CAPITAL:        str << wxT("Capital"); break;
            case WXK_PAGEUP:         str << wxT("PgUp");  break;
            case WXK_PAGEDOWN:       str << wxT("PgDn");  break;
            case WXK_SELECT:         str << wxT("Select"); break;
            case WXK_PRINT:          str << wxT("Print"); break;
            case WXK_EXECUTE:        str << wxT("Execute"); break;
            case WXK_SNAPSHOT:       str << wxT("Snapshot"); break;
            case WXK_HELP:           str << wxT("Help"); break;
            case WXK_WINDOWS_LEFT:   str << wxT("Super_L"); break;
            case WXK_WINDOWS_RIGHT:  str << wxT("Super_R"); break;
            case WXK_WINDOWS_MENU:   str << wxT("Menu"); break;
            case WXK_NUMPAD0:        str << wxT("KP_0"); break;
            case WXK_NUMPAD1:        str << wxT("KP_1"); break;
            case WXK_NUMPAD2:        str << wxT("KP_2"); break;
            case WXK_NUMPAD3:        str << wxT("KP_3"); break;
            case WXK_NUMPAD4:        str << wxT("KP_4"); break;
            case WXK_NUMPAD5:        str << wxT("KP_5"); break;
            case WXK_NUMPAD6:        str << wxT("KP_6"); break;
            case WXK_NUMPAD7:        str << wxT("KP_7"); break;
            case WXK_NUMPAD8:        str << wxT("KP_8"); break;
            case WXK_NUMPAD9:        str << wxT("KP_9"); break;
            case WXK_MULTIPLY:       str << wxT("Multiply"); break;
            case WXK_ADD:            str << wxT("Add"); break;
            case WXK_SEPARATOR:      str << wxT("Separator"); break;
            case WXK_SUBTRACT:       str << wxT("Subtract"); break;
            case WXK_DECIMAL:        str << wxT("Decimal"); break;
            case WXK_DIVIDE:         str << wxT("Divide"); break;
            case WXK_NUMLOCK:        str << wxT("Num_Lock"); break;
            case WXK_SCROLL:         str << wxT("Scroll_Lock"); break;
            case WXK_NUMPAD_SPACE:   str << wxT("KP_Space"); break;
            case WXK_NUMPAD_TAB:     str << wxT("KP_Tab"); break;
            case WXK_NUMPAD_ENTER:   str << wxT("KP_Enter"); break;
            case WXK_NUMPAD_F1:      str << wxT("KP_F1"); break;
            case WXK_NUMPAD_F2:      str << wxT("KP_F2"); break;
            case WXK_NUMPAD_F3:      str << wxT("KP_F3"); break;
            case WXK_NUMPAD_F4:      str << wxT("KP_F4"); break;
            case WXK_NUMPAD_HOME:    str << wxT("KP_Home"); break;
            case WXK_NUMPAD_LEFT:    str << wxT("KP_Left"); break;
            case WXK_NUMPAD_UP:      str << wxT("KP_Up"); break;
            case WXK_NUMPAD_RIGHT:   str << wxT("KP_Right"); break;
            case WXK_NUMPAD_DOWN:    str << wxT("KP_Down"); break;
            case WXK_NUMPAD_PAGEUP:  str << wxT("KP_PageUp"); break;
            case WXK_NUMPAD_END:      str << wxT("KP_End"); break;
            case WXK_NUMPAD_BEGIN:    str << wxT("KP_Begin"); break;
            case WXK_NUMPAD_EQUAL:    str << wxT("KP_Equal"); break;
            case WXK_NUMPAD_DIVIDE:   str << wxT("KP_Divide"); break;
            case WXK_NUMPAD_MULTIPLY: str << wxT("KP_Multiply"); break;
            case WXK_NUMPAD_ADD:      str << wxT("KP_Add"); break;
            case WXK_NUMPAD_SEPARATOR: str << wxT("KP_Separator"); break;
            case WXK_NUMPAD_SUBTRACT: str << wxT("KP_Subtract"); break;
            case WXK_NUMPAD_DECIMAL:  str << wxT("KP_Decimal"); break;
            case WXK_NUMPAD_INSERT:   str << wxT("KP_Insert");    break;
            case WXK_NUMPAD_DELETE:   str << wxT("KP_Delete");   break;

             default:
                if (wxIsgraph(code))   { str << (wxChar)code;  break; }
    }

return str;
}

wxString MakeAcceleratorLabel(int metakeys, int keycode)
{
wxString accel;

if (metakeys & wxACCEL_CTRL)   accel = wxString(_("Ctrl")) + wxT("+");
if (metakeys & wxACCEL_ALT)    accel += wxString(_("Alt")) + wxT("+");
if (metakeys & wxACCEL_SHIFT)  accel += wxString(_("Shift")) + wxT("+");

accel += TranslateKeycode(keycode);                  // Add the keycode

return accel;
}

bool AccelEntry::Load(wxConfigBase* config, wxString& key)  // Given info about where it's stored, load data for this entry
{
if (config==NULL || key.IsEmpty())  return false;

cmd = (int)config->Read(key+wxT("/Cmd"), -1l);
if (cmd == wxNOT_FOUND) return false;                    
cmd += SHCUT_START;                                      // Shortcut was saved as 0,1,2 etc, so add the offset
flags = (int)config->Read(key+wxT("/Flags"), 0l);
keyCode = (int)config->Read(key+wxT("/KeyCode"), 0l);  
config->Read(key+wxT("/Label"), &label);
config->Read(key+wxT("/Help"), &help);
config->Read(key+wxT("/UD"), &UserDefined);
AE_type type = (AE_type)config->Read(key+wxT("/Type"), (long)AE_unknown);
if (type == wxNOT_FOUND)  // Otherwise any necessary compensation for the 0.8.0 changes has already occurred
  AdjustShortcutIfNeeded();
return true;
}

bool AccelEntry::Save(wxConfigBase* config, wxString& key)  // Given info about where it's stored, save data for this entry
{
if (config==NULL || key.IsEmpty())  return false;

config->Write(key+wxT("/Flags"), (long)flags);
config->Write(key+wxT("/KeyCode"), (long)keyCode);
config->Write(key+wxT("/Cmd"), (long)(cmd - SHCUT_START));
config->Write(key+wxT("/Label"), label);
config->Write(key+wxT("/Help"), help);
config->Write(key+wxT("/UD"), (long)UserDefined);
config->Write(key+wxT("/Type"), (long)type);

return true;
}

void AccelEntry::AdjustShortcutIfNeeded()
{
  // 0.8.0 inserted a new shortcut, SHCUT_REALLYDELETE, so this and subsequent old values need adjusting to compensate
if (cmd >= SHCUT_REALLYDELETE) ++cmd;

  // Pre-0.8.0 user-defined tools shortcuts followed immediately after the normal ones; so appending extra normal shortcuts affected the UDTools values
  // Since 0.8.0 the UDTools ones are in a higher range. This function tries to cope with the transition
if (cmd > SHCUT_TOOLS_REPEAT) cmd += GAP_FOR_NEW_SHORTCUTS;
SetType((cmd >= SHCUT_TOOLS_LAUNCH) ? AE_udtool : AE_normal);
}

#if wxVERSION_NUMBER < 2900
  DEFINE_EVENT_TYPE(MyReloadAcceleratorsEvent)    // Sent by AcceleratorList to tell everyone to reload their menus/acceltables
  DEFINE_EVENT_TYPE(MyListCtrlEvent)              // Used by AcceleratorList::ConfigureShortcuts() to size listctrl columns
#else
  wxDEFINE_EVENT(MyReloadAcceleratorsEvent, wxNotifyEvent);
  wxDEFINE_EVENT(MyListCtrlEvent, wxNotifyEvent);
#endif

#define EVT_MYLISTCTRL(fn) \
        DECLARE_EVENT_TABLE_ENTRY(\
            MyListCtrlEvent, wxID_ANY, wxID_ANY, \
            (wxObjectEventFunction)(wxEventFunction)(wxNotifyEventFunction)&fn, \
            (wxObject*) NULL ),
            
AcceleratorList::~AcceleratorList()
{
WX_CLEAR_ARRAY(Accelarray);
}

void AcceleratorList::Init()
{
WX_CLEAR_ARRAY(Accelarray);                       // Make sure the array is empty
ImplementDefaultShortcuts();                      // Set the array to defaults
LoadUserDefinedShortcuts();                       // Add any user-defined entries, and change any default ones to suit the user

AccelEntry* entry = GetEntry(SHCUT_ESCAPE);
if (entry)
  { wxGetApp().SetEscapeKeycode(entry->GetKeyCode()); // Set the Escape code/flags checked for in MyApp::FilterEvent
    wxGetApp().SetEscapeKeyFlags(entry->GetFlags());
  }
}

void AcceleratorList::ImplementDefaultShortcuts()
{
        // The following 3 arrays contain matching label/flags/keycode for SHCUTno of shortcuts.  Hack them in to Accelarray

const wxString DefaultMenuLabels[] = { 
  _("C&ut"), _("&Copy"), _("Send to &Trash-can"), _("De&lete"), _("Permanently Delete"), _("Rena&me"), _("&Duplicate"),
  _("Prop&erties"),_("&Open"),_("Open &with..."),wxT(""),_("&Paste"),_("Make a &Hard-Link"),_("Make a &Symlink"),
  _("&Delete Tab"),_("&Rename Tab"),_("Und&o"), _("&Redo"), _("Select &All"), _("&New File or Dir"), _("&New Tab"), _("&Insert Tab"),
  _("D&uplicate this Tab"),_("Show even single Tab &Head"),_("Give all Tab Heads equal &Width"),_("&Replicate in Opposite Pane"),_("&Swap the Panes"),_("Split Panes &Vertically"),
  _("Split Panes &Horizontally"),_("&Unsplit Panes"),_("&Extension"),_("&Size"),_("&Time"),_("&Permissions"),
  _("&Owner"),_("&Group"),_("&Link"),_("Show &all columns"),_("&Unshow all columns"),_("Save &Pane Settings"),
  _("Save Pane Settings on &Exit"),_("&Save Layout as Template"),wxT(""),_("D&elete a Template"),
  _("&Refresh the Display"),_("Launch &Terminal"),_("&Filter Display"),_("Show/Hide Hidden dirs and files"),_("&Add To Bookmarks"),_("&Manage Bookmarks"),
#ifndef __GNU_HURD__ 
  _("&Mount a Partition"),_("&Unmount a Partition"),_("Mount an &ISO image"),_("Mount an &NFS export"),_("Mount a &Samba share"),_("U&nmount a Network mount"),
#else
  _("&Mount a Partition"),_("&Unmount"),_("Mount an &ISO image"),_("Mount an &NFS export"),_("Unused"),_("Unused"),
#endif
  wxT("&locate"),wxT("&find"),wxT("&grep"),_("Show &Terminal Emulator"),_("Show Command-&line"),_("&GoTo selected file"),_("Empty the &Trash-can"),_("Permanently &delete 'Deleted' files"),
  _("E&xtract Archive or Compressed File(s)"),_("Create a &New Archive"),_("&Add to an Existing Archive"),_("&Test integrity of Archive or Compressed Files"),_("&Compress Files"),
  _("&Show Recursive dir sizes"),_("&Retain relative Symlink Targets"),_("&Configure 4Pane"),_("E&xit"),_("Context-sensitive Help"),_("&Help Contents"),_("&FAQ"),_("&About 4Pane"),_("Configure Shortcuts"),
  _("Go to Symlink Target"),_("Go to Symlink Ultimate Target"),_("Edit"),_("New Separator"), _("Repeat Previous Program"), _("Navigate to opposite pane"), _("Navigate to adjacent pane"),
  _("Switch to the Panes"), _("Switch to the Terminal Emulator"), _("Switch to the Command-line"), _("Switch to the toolbar Textcontrol"), _("Switch to the previous window"), 
  _("Go to Previous Tab"), _("Go to Next Tab"), _("Paste as Director&y Template"), _("&First dot"), _("&Penultimate dot"), _("&Last dot"),
  _("Mount over Ssh using ssh&fs"), _("Show &Previews"), _("C&ancel Paste"), _("Decimal-aware filename sort"), _("&Keep Modification-time when pasting files"), 
  _("Navigate up to higher directory"), _("Navigate back to previously visited directory"), _("Navigate forward to next visited directory")};

int DefaultShortcutFlags[] = { wxACCEL_CTRL, wxACCEL_CTRL, wxACCEL_NORMAL, wxACCEL_SHIFT, wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_CTRL,
      wxACCEL_ALT, wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_CTRL, wxACCEL_CTRL+wxACCEL_SHIFT, wxACCEL_ALT+wxACCEL_SHIFT,
      wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_CTRL, wxACCEL_CTRL, wxACCEL_CTRL, wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_NORMAL,
      wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_NORMAL,
      wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_NORMAL,
      wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_NORMAL,
      wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_NORMAL,
      wxACCEL_NORMAL, wxACCEL_CTRL, wxACCEL_CTRL+wxACCEL_SHIFT, wxACCEL_CTRL, wxACCEL_NORMAL, wxACCEL_NORMAL,
      wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_NORMAL,
      wxACCEL_CTRL, wxACCEL_CTRL, wxACCEL_CTRL, wxACCEL_CTRL, wxACCEL_CTRL, wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_NORMAL,
      wxACCEL_CTRL, wxACCEL_CTRL+wxACCEL_SHIFT, wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_CTRL+wxACCEL_ALT,
      wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_ALT, wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_CTRL,
      wxACCEL_NORMAL, wxACCEL_NORMAL,wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_CTRL+wxACCEL_SHIFT, wxACCEL_CTRL+wxACCEL_SHIFT,
      wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_NORMAL,
      wxACCEL_CTRL+wxACCEL_SHIFT, wxACCEL_CTRL+wxACCEL_SHIFT, wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_NORMAL, wxACCEL_CTRL, wxACCEL_SHIFT, wxACCEL_NORMAL, wxACCEL_NORMAL,
      wxACCEL_CTRL+wxACCEL_SHIFT, wxACCEL_CTRL+wxACCEL_SHIFT, wxACCEL_CTRL+wxACCEL_SHIFT };

int DefaultShortcutKeycode[] = { 'X', 'C', WXK_DELETE,WXK_DELETE,0, WXK_F2, 'D', // 7 entries
                              'P', 0, 0, 0, 'V', 'L', 'L',                       // 7
                              0, 0, 'Z', 'R', 'A', WXK_F10, 0, 0,                // 8
                              0, 0, 0, 0,  0, 0,                                 // 6
                              0, 0, 0, 0, 0, 0,                                  // 6
                              0, 0, 0, 0, 0, 0,                                  // 6
                              0, 0, 0, 0,                                        // 4
                              WXK_F5, 'T', 'F', 'H', 0, 0,                       // 6
                              0, 0, 0, 0, 0, 0,                                  // 6
                              'L', 'F', 'G', 'M', WXK_F6, 0, 0, 0,               // 8
                              'E', 'Z', 0, 0, 'Z',                               // 5
                              0, 0, 0, WXK_F4, WXK_F1, 0, 0, 0, 'S',             // 9
                              0, 0, 0, 0, 0, 'O','A',                            // 7
                              0, 0, 0, 0, 0,                                     // 5
                              ',', '.', 0, 0, 0, 0 , 0, 'P', WXK_ESCAPE, 0, 0,   // 11
                              WXK_UP, WXK_LEFT, WXK_RIGHT };                     // 3

const wxString DefaultMenuHelp[] = { 
  _("Cuts the current selection"), _("Copies the current selection"), _("Send to the Trashcan"), _("Kill, but may be resuscitatable"),_("Delete with extreme prejudice"), wxT(""), wxT(""),
  wxT(""), wxT(""), wxT(""), wxT(""), _("Paste the contents of the Clipboard"), _("Hardlink the contents of the Clipboard to here"), _("Softlink the contents of the Clipboard to here"),
_("Delete the currently-selected Tab"), _("Rename the currently-selected Tab"), wxT(""), wxT(""), wxT(""), wxT(""), _("Append a new Tab"), _("Insert a new Tab after the currently selected one"),
  wxT(""), _("Hide the head of a solitary tab"), wxT(""), _("Copy this side's path to the opposite pane"), _("Swap one side's path with the other"), wxT(""),
  wxT(""), wxT(""), wxT(""), wxT(""), wxT(""), wxT(""),
  wxT(""), wxT(""), wxT(""), wxT(""), wxT(""),  _("Save the layout of panes within each tab"),
  _("Always save the layout of panes on exit"), _("Save these tabs as a reloadable template"), wxT(""), wxT(""),
  wxT(""), wxT(""), wxT(""), wxT(""), _("Add the currently-selected item to your bookmarks"), _("Rearrange, Rename or Delete bookmarks"),
  wxT(""), wxT(""), wxT(""), wxT(""), wxT(""), wxT(""),
  _("Locate matching files.  Much faster than 'find'"), _("Find matching file(s)"), _("Search Within Files"), _("Show/Hide the Terminal Emulator"), _("Show/Hide the Command-line"), wxT(""), _("Empty 4Pane's in-house trash-can"), _("Delete 4Pane's 'Deleted' folder"),
  wxT(""), wxT(""), wxT(""), wxT(""), wxT(""),
  _("Whether to calculate dir sizes recursively in fileviews"),_("On moving a relative symlink, keep its target the same"), wxT(""), _("Close this program"), wxT(""), wxT(""), wxT(""), wxT(""), wxT(""), wxT(""),
  wxT(""),wxT(""), wxT(""), wxT(""), wxT(""), wxT(""), 
  wxT(""), wxT(""), wxT(""), wxT(""), wxT(""),
  wxT(""), wxT(""), _("Paste only the directory structure from the clipboard"),_("An ext starts at first . in the filename"), _("An ext starts at last or last-but-one . in the filename"), _("An ext starts at last . in the filename"),
  wxT(""), _("Show previews of image and text files"), _("Cancel the current process"), _("Should files like foo1, foo2 be in Decimal order"), _("Should a Moved or Pasted file keep its original modification time (as in 'cp -a')"), 
  wxT(""), wxT(""), wxT("") };
                              
const size_t SHCUTno = sizeof(DefaultShortcutKeycode)/sizeof(int);

wxCHECK_RET(SHCUTno == sizeof(DefaultShortcutFlags)/sizeof(int) && SHCUTno == sizeof(DefaultMenuLabels)/sizeof(wxString)
                     && SHCUTno == sizeof(DefaultMenuHelp)/sizeof(wxString),   _("\nThe arrays in ImplementDefaultShortcuts() aren't of equal size!"));

for (size_t n=0; n < SHCUTno; ++n)                    // For every entry in the static arrays, make a new AccelEntry & add to array
  { AccelEntry* AccEntry = new class AccelEntry(AccelEntry::AE_normal);
    AccEntry->SetShortcut(SHCUT_START + n);
    AccEntry->SetFlags(DefaultShortcutFlags[n]);
    AccEntry->SetKeyCode(DefaultShortcutKeycode[n]);
    AccEntry->SetLabel(DefaultMenuLabels[n]);
    AccEntry->SetHelpstring(DefaultMenuHelp[n]);
    AccEntry->SetUserDefined(false);

    Accelarray.Add(AccEntry);
  }

AccelEntry* AccEntry = new class AccelEntry(AccelEntry::AE_normal);  // Now do Fulltree toggle:  it's in a different enum range, so isn't in the above arrays
AccEntry->SetShortcut(IDM_TOOLBAR_fulltree); AccEntry->SetFlags(wxACCEL_ALT);
AccEntry->SetKeyCode((int)'W'); AccEntry->SetLabel(_("Toggle Fulltree mode")); AccEntry->SetUserDefined(false);
Accelarray.Add(AccEntry);

        // Finally, make room for the User-Defined Tools.  We have to pretend to load them to find out how many there are, and the name of each
        // Unfortunately we can't piggyback on MyFrame::mainframe->Layout->m_notebook->LaunchFromMenu, as it doesn't yet exist
int index = SHCUT_TOOLS_LAUNCH;
LoadUDToolData(wxT("/Tools/Launch"), index);  
}

void AcceleratorList::LoadUDToolData(wxString name, int& index)  // Recursively add to array a (sub)menu 'name' of user-defined commands
{
if (name.IsEmpty()) return;
if (index >= SHCUT_TOOLS_LAUNCH_LAST) return;

wxConfigBase* config = wxConfigBase::Get();  if (config==NULL)  { wxLogError(_("Couldn't load configuration!?")); return; }
config->SetPath(name);                          // Use name to create the config path eg /Tools/Launch/a

wxString itemlabel, itemdata, key;
                                  // First do the entries
size_t count = (size_t)config->Read(wxT("Count"), 0l); // Load the no of tools in this (sub)menu
for (size_t n=0; n < count; ++n)                     // Create the key for every item, from label_a & data_a, to label_/data_('a'+count-1)
  { key.Printf(wxT("%c"), wxT('a') + (wxChar)n);     // Make key hold a, b, c etc
    config->Read(wxT("label_")+key, &itemlabel);     // Load the entry's label
    config->Read(wxT("data_")+key, &itemdata);       //  & data
    if (itemdata.IsEmpty())   continue;
    if (itemlabel.IsEmpty())   itemlabel = itemdata; // If need be, use the data as label
    
    AccelEntry* AccEntry = new class AccelEntry(AccelEntry::AE_udtool); // Make an accel entry for this tool
    AccEntry->SetShortcut(index);
    AccEntry->SetLabel(itemlabel);
    AccEntry->SetFlags(wxACCEL_NORMAL);              // Null the rest of the entry
    AccEntry->SetKeyCode(0);
    AccEntry->SetHelpstring(wxT(""));
    AccEntry->SetUserDefined(false);
    
    Accelarray.Add(AccEntry); ++index;
  }

                                  // Now deal with any subfolders, by recursion
count = config->GetNumberOfGroups();
for (size_t n=0; n < count; ++n)                     // Create the path for each. If name was "a", then subfolders will be "a/a", "a/b" etc
  { key.Printf(wxT("%c"), wxChar(wxT('a') + n));
    LoadUDToolData(name + wxCONFIG_PATH_SEPARATOR + key, index);
  }

config->SetPath(wxT("/"));  
}

void AcceleratorList::LoadUserDefinedShortcuts()  // Add any user-defined tools, & override default shortcuts with user prefs
{
wxConfigBase* config = wxConfigBase::Get();   if (config==NULL)  return;

long cookie; wxString group;

config->SetPath(wxT("/Configure/Shortcuts"));
bool bCont = config->GetFirstGroup(group, cookie);
while (bCont) 
  { AccelEntry* AccEntry = new class AccelEntry(AccelEntry::AE_unknown);
    if (!AccEntry->Load(config, group))   delete AccEntry;    // Must have been a mis-load
     else
      { int current = FindEntry(AccEntry->GetShortcut());     // There was a valid user entry, so see if there's already an entry for this shortcut
        if (current == -1)                                    // No, there isn't
            Accelarray.Add(AccEntry);                         //  so add to the array
         else                             // There IS already a default entry, so remove it and replace with the user's choices
          { AccelEntry* oldentry = Accelarray.Item(current);
            Accelarray.RemoveAt(current);
            AccEntry->SetDefaultFlags(oldentry->GetFlags());  // Insert into the default-store the old entry's accel data
            AccEntry->SetDefaultKeyCode(oldentry->GetKeyCode());
            AccEntry->SetType(oldentry->GetType());
            
            if (current < (int)Accelarray.GetCount())         //  Insert if the (new, reduced) count shows it won't be at the end
                  Accelarray.Insert(AccEntry, current);
              else  Accelarray.Add(AccEntry);                 // Otherwise append
            
            delete oldentry;
          }  
      }

    bCont = config->GetNextGroup(group, cookie);
  }

config->SetPath(wxT("/"));
}

void AcceleratorList::SaveUserDefinedShortcuts(ArrayOfAccels& Accelarray)
{
wxConfigBase* config = wxConfigBase::Get(); if (config==NULL)  return;

size_t count = Accelarray.GetCount(), UDtotal = 0;
for (size_t n=0; n < count; ++n)
  if (Accelarray.Item(n)->GetUserDefined())  ++UDtotal;   // Go thru the array, counting the UD entries, the ones that we should be saving

config->DeleteGroup(wxT("/Configure/Shortcuts"));         // Delete current info (otherwise deleted data would remain, & be reloaded in future)
config->SetPath(wxT("/Configure/Shortcuts"));             // Do this even if UDtotal is 0; otherwise the last UD entry doesn't get deleted!

if (UDtotal)
  { size_t UDcount = 0;
    for (size_t n=0; n < count; ++n)
      { if (Accelarray.Item(n)->GetUserDefined())         // If this is one of the user-defined entries, save it
          { wxString name = CreateSubgroupName(UDcount++, UDtotal); // Get the next available path name for this group eg aa, ab
            Accelarray.Item(n)->Save(config, name);
          }
      }
  }
  
config->Flush(); config->SetPath(wxT("/"));
}

int AcceleratorList::FindEntry(int shortcutId)  // Returns the index of the shortcut within Accelarray, or -1 for not found
{
for (size_t n=0; n < Accelarray.GetCount(); ++n)
  if (Accelarray.Item(n)->GetShortcut() == shortcutId)  
      return (int)n;

return -1;
}

AccelEntry* AcceleratorList::GetEntry(int shortcutId)  // Returns the entry that contains this shortcut ID
{
for (size_t n=0; n < Accelarray.GetCount(); ++n)
  if (Accelarray.Item(n)->GetShortcut() == shortcutId)  
    return Accelarray.Item(n);

AccelEntry* empty = NULL; return empty;                          // If we're here, the shortcut wasn't found
}

void AcceleratorList::FillEntry(wxAcceleratorEntry& entry, int shortcutId) // Fills entry with the data corresponding to this shortcut
{
AccelEntry* arrayentry = GetEntry(shortcutId);            // Find the correct entry within the array
wxCHECK_RET(arrayentry, wxT("Invalid shortcutId in FillEntry()"));

entry.Set(arrayentry->GetFlags(), arrayentry->GetKeyCode(), arrayentry->GetShortcut());  // Use its data to fill the referenced entry
}

void AcceleratorList::CreateAcceleratorTable(wxWindow* win, int AccelEntries[], size_t shortcutNo)  // Creates an accelerator table for the passed window
{
if (win==NULL || !shortcutNo) return;

wxAcceleratorEntry* entries = new wxAcceleratorEntry[shortcutNo];
for (size_t n=0; n < shortcutNo; ++n)                     // For every shortcut,
    FillEntry(*(entries+n), AccelEntries[n]);             // Get the keycode etc

wxAcceleratorTable accel(shortcutNo, entries);
win->SetAcceleratorTable(accel);
delete[] entries;
}

void AcceleratorList::FillMenuBar(wxMenuBar* MenuBar, bool refilling/*=false*/)  // Creates/refills the menubar menus, attaches them to menubar
{
const int ID_INSERT_SUBMENU = wxID_SEPARATOR - 1;
const int ID_CHECKNEXTITEM = wxID_SEPARATOR - 2;            // Flags to Check next item
const int ID_RADIONEXTITEM = wxID_SEPARATOR - 3;            // Flags to Radio next item
const int ID_EMPTYITEM = wxID_SEPARATOR - 4;                // Flags that the next item shouldn't be added to its menu
const size_t menuno = 10;                                   // The no of main menus
const size_t submenuno = 2;                                 // Ditto submenus
const size_t ptrsize = sizeof(int);

int* itemslistarray[ menuno ]; size_t itemslistcount[ menuno ];
int* subitemslistarray[ submenuno ]; size_t subitemslistcount[ submenuno ];
int fileitems[] = { SHCUT_EXIT };
int edititems[] = { SHCUT_CUT, SHCUT_COPY, SHCUT_PASTE, SHCUT_PASTE_DIR_SKELETON, SHCUT_ESCAPE, wxID_SEPARATOR, SHCUT_NEW, SHCUT_SOFTLINK, SHCUT_HARDLINK, wxID_SEPARATOR,
                  SHCUT_TRASH, SHCUT_DELETE, SHCUT_REALLYDELETE, wxID_SEPARATOR, SHCUT_REFRESH, SHCUT_RENAME, SHCUT_DUP, wxID_SEPARATOR, SHCUT_UNDO, SHCUT_REDO, wxID_SEPARATOR, SHCUT_PROPERTIES };
int viewitems[] = { SHCUT_TERMINAL_EMULATOR, SHCUT_COMMANDLINE, ID_CHECKNEXTITEM, SHCUT_PREVIEW, wxID_SEPARATOR, SHCUT_SPLITPANE_VERTICAL, SHCUT_SPLITPANE_HORIZONTAL,
                 SHCUT_SPLITPANE_UNSPLIT, wxID_SEPARATOR, SHCUT_REPLICATE, SHCUT_SWAPPANES, wxID_SEPARATOR, SHCUT_FILTER, SHCUT_TOGGLEHIDDEN,wxID_SEPARATOR, ID_INSERT_SUBMENU };
int tabitems[] = { SHCUT_NEWTAB, SHCUT_DELTAB, SHCUT_INSERTTAB, SHCUT_RENAMETAB, SHCUT_DUPLICATETAB, wxID_SEPARATOR, 
                ID_CHECKNEXTITEM, SHCUT_ALWAYS_SHOW_TAB, ID_CHECKNEXTITEM, SHCUT_SAME_TEXTSIZE_TAB, wxID_SEPARATOR,
                ID_INSERT_SUBMENU, SHCUT_TAB_SAVEAS_TEMPLATE, SHCUT_TAB_DELETE_TEMPLATE };
int bmitems[] = { SHCUT_ADD_TO_BOOKMARKS, SHCUT_MANAGE_BOOKMARKS, wxID_SEPARATOR };
int archiveitems[] = { SHCUT_ARCHIVE_EXTRACT, wxID_SEPARATOR, SHCUT_ARCHIVE_CREATE, SHCUT_ARCHIVE_APPEND,
                      SHCUT_ARCHIVE_TEST, wxID_SEPARATOR, SHCUT_ARCHIVE_COMPRESS };

#ifndef __GNU_HURD__ 
  bool sshfs = Configure::TestExistence(wxT("sshfs")); // Test if sshfs is available. Don't add its menuitem otherwise
  int mountitems[] = { SHCUT_MOUNT_MOUNTPARTITION, SHCUT_MOUNT_UNMOUNTPARTITION, wxID_SEPARATOR, SHCUT_MOUNT_MOUNTISO, wxID_SEPARATOR, SHCUT_MOUNT_MOUNTNFS,
                        sshfs ? SHCUT_MOUNT_MOUNTSSHFS : ID_EMPTYITEM, SHCUT_MOUNT_MOUNTSAMBA, SHCUT_MOUNT_UNMOUNTNETWORK };
#else
  int mountitems[] = { SHCUT_MOUNT_MOUNTPARTITION, ID_EMPTYITEM, SHCUT_MOUNT_MOUNTISO, SHCUT_MOUNT_MOUNTNFS,
                        ID_EMPTYITEM, ID_EMPTYITEM, wxID_SEPARATOR, SHCUT_MOUNT_UNMOUNTPARTITION };
#endif


int toolsitems[] = { SHCUT_TOOL_LOCATE, SHCUT_TOOL_FIND, SHCUT_TOOL_GREP, wxID_SEPARATOR, SHCUT_LAUNCH_TERMINAL };
int optionsitems[] = { ID_CHECKNEXTITEM, SHCUT_SHOW_RECURSIVE_SIZE, ID_CHECKNEXTITEM, SHCUT_RETAIN_REL_TARGET, ID_CHECKNEXTITEM, SHCUT_RETAIN_MTIME_ON_PASTE, wxID_SEPARATOR, SHCUT_SAVETABS, ID_CHECKNEXTITEM, SHCUT_SAVETABS_ONEXIT, wxID_SEPARATOR, SHCUT_EMPTYTRASH, SHCUT_EMPTYDELETED, wxID_SEPARATOR, SHCUT_CONFIGURE };
int helpitems[] = { SHCUT_HELP, SHCUT_FAQ, wxID_SEPARATOR, SHCUT_ABOUT };

int columnitems[] = { ID_CHECKNEXTITEM,SHCUT_SHOW_COL_EXT, ID_CHECKNEXTITEM,SHCUT_SHOW_COL_SIZE, ID_CHECKNEXTITEM,SHCUT_SHOW_COL_TIME,
            ID_CHECKNEXTITEM,SHCUT_SHOW_COL_PERMS, ID_CHECKNEXTITEM,SHCUT_SHOW_COL_OWNER, ID_CHECKNEXTITEM,SHCUT_SHOW_COL_GROUP,
            ID_CHECKNEXTITEM,SHCUT_SHOW_COL_LINK, wxID_SEPARATOR, SHCUT_SHOW_COL_ALL, SHCUT_SHOW_COL_CLEAR };
int loadtabitems[] = { };  // "This page intentionally left blank" --- the items are created on the fly if needed
                                  // Hack in the arrays to the array-ptrs
itemslistarray[0] = fileitems; itemslistcount[0] = sizeof(fileitems)/ptrsize; itemslistarray[1] = edititems; itemslistcount[1] = sizeof(edititems)/ptrsize;
itemslistarray[2] = viewitems; itemslistcount[2] = sizeof(viewitems)/ptrsize; itemslistarray[3] = tabitems; itemslistcount[3] = sizeof(tabitems)/ptrsize;
itemslistarray[4] = bmitems; itemslistcount[4] = sizeof(bmitems)/ptrsize; itemslistarray[5] = archiveitems; itemslistcount[5] = sizeof(archiveitems)/ptrsize;
itemslistarray[6] = mountitems; itemslistcount[6] = sizeof(mountitems)/ptrsize; itemslistarray[7] = toolsitems; itemslistcount[7] = sizeof(toolsitems)/ptrsize;
itemslistarray[8] = optionsitems; itemslistcount[8] = sizeof(optionsitems)/ptrsize; itemslistarray[9] = helpitems; itemslistcount[9] = sizeof(helpitems)/ptrsize;
subitemslistarray[0] = columnitems; subitemslistcount[0] = sizeof(columnitems)/ptrsize; subitemslistarray[1] = loadtabitems; subitemslistcount[1] = sizeof(loadtabitems)/ptrsize;

                                    // Make arrays for menus & for submenus
const wxString menutitles[] = { _("&File"), _("&Edit"), _("&View"), _("&Tabs"), _("&Bookmarks"), _("&Archive"), _("&Mount"), _("Too&ls"), _("&Options"), _("&Help") };
Bookmarks::SetMenuIndex(4); // *** remember to change these if their position changes! ***
LaunchMiscTools::SetMenuIndex(7);

const wxString submenutitles[] = { _("&Columns to Display"), _("&Load a Tab Template") };
int submenID[2];  submenID[0] = SHCUT_FINISH + 1; submenID[1] = LoadTabTemplateMenu;  // "Columns to Display"  hasn't a set id, so use SHCUT_FINISH+1
wxMenu* menuarray[ menuno ]; wxMenu* submenuarray[ submenuno ];
if (refilling)                          // If we're updating, we need to acquire ptrs to the current menus
  { for (size_t n=0; n < MenuBar->GetMenuCount(); ++n)  
      menuarray[n] = MenuBar->GetMenu(n);
    for (size_t n=0; n < sizeof(submenutitles)/sizeof(wxString); ++n)
      { wxMenuItem* item = MenuBar->FindItem(submenID[n]);       // Submenus are trickier. You have to have stored their id, then find its menuitem
        submenuarray[n] = item->GetSubMenu();                    // This will then locate the submenu
      }
  }

for (size_t n=0; n < submenuno; ++n)                             // Do the submenus first, so they're available to be attached to the menus
  { wxItemKind kind = wxITEM_NORMAL;                             // kind may be normal or check (& in theory radio)
    if (!refilling) submenuarray[n] = new wxMenu;                // First make a new menu, unless we're updating
    int* ip = subitemslistarray[n];
    for (size_t i=0; i < subitemslistcount[n]; ++i)              // For every item in the corresponding itemarray
      { if (ip[i] == ID_CHECKNEXTITEM)
          { kind = wxITEM_CHECK;  continue; }                    // If this item just says "check next item", flag & skip to the next entry
        if (ip[i] == ID_RADIONEXTITEM)
          { kind = wxITEM_RADIO;  continue; }                    // Ditto "radio next item"
        if (ip[i] == ID_EMPTYITEM) continue;                     // This item isn't to be entered. Currently only SHCUT_MOUNT_MOUNTSSHFS when sshfs isn't installed
          
        if (refilling)  UpdateMenuItem(submenuarray[n], ip[i]);  // Otherwise update/add to the menu
          else AddToMenu(*submenuarray[n], ip[i], wxEmptyString, kind);
        kind = wxITEM_NORMAL;                                    // Reset the flag in case we just used it
      }    
  }

size_t sub = 0;                                                  // 'sub' indexes which submenu to use next
for (size_t n=0; n < menuno; ++n)      // Now do the same sort of thing for the main menus, attaching submenus where appropriate
  { wxItemKind kind = wxITEM_NORMAL;
    if (!refilling) menuarray[n] = new wxMenu;
    int* ip = itemslistarray[n];
    for (size_t i=0; i < itemslistcount[n]; ++i)
      { if (ip[i] == ID_CHECKNEXTITEM)
          { kind = wxITEM_CHECK;  continue; }                    // If this item just says "check next item", flag & skip to the next entry
        if (ip[i] == ID_RADIONEXTITEM)
          { kind = wxITEM_RADIO;  continue; }                    // Ditto "radio next item"
        if (ip[i] == ID_EMPTYITEM) continue;                     // This item isn't to be entered. Currently only for hurd, & SHCUT_MOUNT_MOUNTSSHFS when sshfs isn't installed
        
        if (ip[i] == ID_INSERT_SUBMENU)                          // If it's a placeholder for a submenu, append the next one here if not updating
          { if (!refilling) { menuarray[n]->Append(submenID[sub], submenutitles[sub], submenuarray[sub]); sub++; }
            continue;
          }
        
        if (refilling)  UpdateMenuItem(menuarray[n], ip[i]);     // Otherwise update/add to the menu  
          else  AddToMenu(*menuarray[n], ip[i], wxEmptyString, kind);
        kind = wxITEM_NORMAL;
      }
    if (!refilling) MenuBar->Append(menuarray[n], menutitles[n]);  // Finally append the menu to the menubar
  }
}

void AcceleratorList::AddToMenu(wxMenu& menu, int shortcutId,   // Fills menu with the data corresponding to this shortcut
                               wxString overridelabel /*=wxEmptyString*/, wxItemKind kind /*=wxITEM_NORMAL*/,wxString overridehelp /*=wxEmptyString*/)
{
if (shortcutId == wxID_SEPARATOR) 
    { menu.AppendSeparator(); return; }                       // Nothing else to do!

AccelEntry* arrayentry = GetEntry(shortcutId);                // Find the correct entry within the array

wxString label = ConstructLabel(arrayentry, overridelabel);
wxString help = overridehelp.IsEmpty() ? arrayentry->GetHelpstring() : overridehelp;  // Unless there's a helpstring provided by the caller, use the default one
  
menu.Append(shortcutId, label, help, kind);
}

void AcceleratorList::UpdateMenuItem(wxMenu* menu, int shortcutId)
{
if (shortcutId == wxID_SEPARATOR)    return;
AccelEntry* arrayentry = GetEntry(shortcutId);                // Find the correct entry within the array

wxString label = ConstructLabel(arrayentry, wxT(""));         // Construct & set the new label/accel
menu->SetLabel(shortcutId, label);
menu->SetHelpString(shortcutId, arrayentry->GetHelpstring()); // Set help too
}

//static
wxString AcceleratorList::ConstructLabel(AccelEntry* arrayentry, const wxString& overridelabel)  // Make a label/accelerator combination
{
wxString label =  overridelabel.IsEmpty() ? arrayentry->GetLabel() : overridelabel;  // Unless there's a label provided by the caller, use the default one
label = label.BeforeFirst(wxT('\t'));                          // Remove any pre-existing accelerator included by mistake

if (arrayentry->GetKeyCode())                                  // If there's a keycode, add it preceded by any flags
  { label += wxT('\t');                                        // Add a tab, to denote that what follows is accelerator
    label += MakeAcceleratorLabel(arrayentry->GetFlags(), arrayentry->GetKeyCode());
  }

return label;
}

void AcceleratorList::ConfigureShortcuts()
{
  // This dialog used to be loaded, empty, from XRC. However that caused a gtk size error (because it was empty...)
wxDialog dlg(parent, wxID_ANY, _("Configure Shortcuts"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
if (!LoadPreviousSize(&dlg, wxT("ConfigureShortcuts")))
  { wxSize size = wxGetDisplaySize();                            // Get a reasonable dialog size.  WE have to, because the treectrl has no idea how big it is
    dlg.SetSize(2*size.x/3, 2*size.y/3);
  }
dlg.Centre();
                              // Load and add the contents, which are used also by ConfigureNotebook
MyShortcutPanel* panel = (MyShortcutPanel*)wxXmlResource::Get()->LoadPanel(&dlg, wxT("ConfigureShortcuts"));
panel->Init(this, false);

wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
sizer->Add(panel, 1, wxEXPAND);
dlg.SetSizer(sizer);
sizer->Layout();

wxNotifyEvent event(MyListCtrlEvent);  // Send a custom event to panel to size listctrl columns.  Can't just tell it: list won't be properly made yet
wxPostEvent(panel, event);

dlg.ShowModal();
SaveCurrentSize(&dlg, wxT("ManageBookmarks"));
}

//----------------------------------------------------------------------------------------

MyShortcutPanel::~MyShortcutPanel()
{
for (int n = (int)Temparray.GetCount();  n > 0; --n )  
    { AccelEntry* item = Temparray.Item(n-1); delete item; Temparray.RemoveAt(n-1); } 

if (list) list->Disconnect(wxEVT_COMMAND_LIST_COL_END_DRAG, wxListEventHandler(MyShortcutPanel::OnColDrag), NULL, this);
list = NULL; 
}

void MyShortcutPanel::Init(AcceleratorList* parent, bool fromNB)
{
AL = parent; FromNotebook = fromNB;

if (!FromNotebook)
  { CallersHelpcontext = HelpContext; HelpContext = HCconfigShortcuts; // Save the global to local store & set to us (if from notebook, this has already been done)
    wxButton* btn = wxDynamicCast(FindWindow(XRCID("UDSCancel")), wxButton); wxCHECK_RET(btn, wxT("Can't find the Cancel button"));
    btn->SetId(wxID_CANCEL); // When used in the Configure dialog, the button has a bespoke ID. Revert to wxID_CANCEL, otherwise the 'X' button doesn't close the dialog in wx2.8
  }

AL->dirty = false;
list = (wxListCtrl*)FindWindow(wxT("List"));

for (int n = (int)Temparray.GetCount(); n > 0; --n )      // Ensure the local array is empty, then copy to it the contents of the main array
  { AccelEntry* item = Temparray.Item(n-1); delete item; Temparray.RemoveAt(n-1); }
ArrayOfAccels mainarray = AL->GetAccelarray();
for (size_t n=0; n < mainarray.GetCount(); ++n)
    Temparray.Add(new AccelEntry(*mainarray.Item(n)));

HideMnemonicsChk = (wxCheckBox*)FindWindow(wxT("HideMnemonicsChk"));
if (HideMnemonicsChk)
  { bool hidemn; wxConfigBase::Get()->Read(wxT("/Configure/Shortcuts/hide_mnemonics"), &hidemn, true);
    HideMnemonicsChk->SetValue(hidemn);
  }

LoadListCtrl();

list->Disconnect(wxEVT_COMMAND_LIST_COL_END_DRAG, wxListEventHandler(MyShortcutPanel::OnColDrag), NULL, this); // In case re-entering after a Cancel
list->Connect(wxEVT_COMMAND_LIST_COL_END_DRAG, wxListEventHandler(MyShortcutPanel::OnColDrag), NULL, this);

wxString DE; wxGetEnv(wxT("XDG_CURRENT_DESKTOP"), &DE); // xfce seems not to set focus anywhere, so compel it
if (DE == wxT("XFCE"))
  list->SetFocus();
}

void MyShortcutPanel::LoadListCtrl()
{
list->ClearAll();                                            // Make sure we start with a clean sheet
list->InsertColumn(0, _("Action"));
list->InsertColumn(1, _("Shortcut"));
list->InsertColumn(2, _("Default"));

for (size_t n=0; n < Temparray.GetCount(); ++n)    DisplayItem(n);

list->SetColumnWidth(0, wxLIST_AUTOSIZE);                 // Make sure the 1st 2 cols are big enough. The 3rd will get the dregs
list->SetColumnWidth(1, wxLIST_AUTOSIZE);
}

long MyShortcutPanel::GetListnoFromArrayNo(int arrayno)   // Translates from array index to list index
{
for (int n=0; n < (int)list->GetItemCount(); ++n)
  if ((int)list->GetItemData(n) == arrayno)  return (long)n;

return (long)wxNOT_FOUND;
}

void MyShortcutPanel::DisplayItem(size_t no, bool refresh /*= false*/)
{
static const wxString LabelsToRename[] = {                 //Some shortcuts aren't self-explanatory out of context, so provide an expanded label
              _("Extension"),_("(Un)Show fileview Extension column"), _("Size"),_("(Un)Show fileview Size column"), _("Time"),_("(Un)Show fileview Time column"),
              _("Permissions"),_("(Un)Show fileview Permissions column"), _("Owner"),_("(Un)Show fileview Owner column"), _("Group"),_("(Un)Show fileview Group column"),
              _("Link"),_("(Un)Show fileview Link column"), _("Show all columns"),_("Fileview: Show all columns"), _("Unshow all columns"),_("Fileview: Unshow all columns"),
              _("Edit"),_("Bookmarks: Edit"), _("New Separator"),_("Bookmarks: New Separator"), _("First dot"),_("Extensions start at the First dot"), 
              _("Penultimate dot"),_("Extensions start at the Penultimate dot"), _("Last dot"),_("Extensions start at the Last dot")
                                        };

AccelEntry* entry = Temparray.Item(no);
if (entry->GetLabel().IsEmpty())  return;                 // As some 'shortcuts' will be only for UpdateUI, or for some other reason aren't user-accessible

wxString accel = MakeAcceleratorLabel(entry->GetFlags(), entry->GetKeyCode());
                        // We can't use 'no' to index the list, as we may have earlier skipped some Temparray items
long listindex;
if (!refresh)                                             // If we're not refreshing, we have to Insert the item
  { wxString label = entry->GetLabel();                   // The 'What is it' label goes in 1st col

    if (HideMnemonicsChk && HideMnemonicsChk->IsChecked())
      label.Replace(wxT("&"), wxT(""));                   // If requested, display 'Cut' instead of 'C&ut'

    // If the label isn't self-explanatory out of context, replace with an expanded version
    for (size_t n=0; n < sizeof(LabelsToRename)/sizeof(wxString); n+=2)
      { wxString lbl(label); lbl.Replace(wxT("&"), wxT(""));
        if (lbl == LabelsToRename[n])
          label = LabelsToRename[n+1];
      }
    listindex = list->InsertItem(list->GetItemCount(), label, 0);
  }
 else 
  { listindex = GetListnoFromArrayNo(no);                 // If we are refreshing, don't.  Just find the item index
    if (listindex == wxNOT_FOUND) return;
  }

if (refresh)
  { wxString label = entry->GetLabel();                   // If refreshing, redo the label in case we just changed it
    if (HideMnemonicsChk && HideMnemonicsChk->IsChecked())
      label.Replace(wxT("&"), wxT(""));
    list->SetItem(listindex, 0, label);
  }

list->SetItem(listindex, 1, accel);                       // The accelerator is displayed in 2nd col

if (entry->IsKeyFlagsUserDefined())                       // Now see if the keycode/flags are user-defined.  If so, put the default accelerator in 3rd col
  { wxString accel = MakeAcceleratorLabel(entry->GetFlags(true), entry->GetKeyCode(true));
    list->SetItem(listindex, 2, accel);
  }
 else if (refresh) list->SetItem(listindex, 2, wxEmptyString);  // If refreshing, clear any previous default display lest we've reverted to default

wxListItem item; item.m_itemId = listindex;
if (!refresh) { item.SetData(no);  item.SetMask(wxLIST_MASK_DATA); }  // Store the temparray index as the list item data
item.SetTextColour((entry->IsKeyFlagsUserDefined() ? *wxBLUE : GetForegroundColour())); // Colour the item blue if keycode/flags non-default

list->SetItem(item);
}

void MyShortcutPanel::OnEdit(wxListEvent& event)
{
wxString current, dfault, action = list->GetItemText(event.GetIndex());
ArrayIndex = event.GetData();  // Remember, because of omitted items, list-index and array-index are not always the same

wxListItem item;
item.m_itemId = event.GetIndex(); item.m_col = 1; item.m_mask = wxLIST_MASK_TEXT;
if (list->GetItem(item))  current = item.m_text.c_str();
item.m_col = 2;
if (list->GetItem(item))  dfault = item.m_text.c_str();

ChangeShortcutDlg dlg; 
wxXmlResource::Get()->LoadDialog(&dlg, this, wxT("ChangeShortcutDlg"));
LoadPreviousSize(&dlg, wxT("ChangeShortcutDlg"));

dlg.Init(action, current, dfault, Temparray.Item(ArrayIndex));  // 'action' will usually be the label, but might have been changed from LabelsToRename[] in DisplayItem()
dlg.parent = this; dlg.label = Temparray.Item(ArrayIndex)->GetLabel(); dlg.help = Temparray.Item(ArrayIndex)->GetHelpstring();

int result = dlg.ShowModal();
SaveCurrentSize(&dlg, wxT("ChangeShortcutDlg"));
if (result == wxID_CANCEL || result==XRCID("Cancel"))  return;  // Cancel is from the DuplicateAccel dialog

      // The result must have been OK (from ChangeShortcutDlg) or Override (from the DuplicateAccel dialog).  Either way, save any new accel
AccelEntry* AccEntry = Temparray.Item(ArrayIndex);
if (AccEntry->IsKeyFlagsUserDefined() &&   // If we're reverting to default, take the data from AccEntry & reset the flag
    ((dlg.text->GetValue()==wxT("None")) || (dlg.text->GetValue()==dfault)))
  { AccEntry->SetKeyCode(AccEntry->GetKeyCode(true)); AccEntry->SetFlags(AccEntry->GetFlags(true));
    AccEntry->SetKeyFlagsUserDefined(false);
  }
 else
  { if (!AccEntry->IsKeyFlagsUserDefined())                    // If the entry wasn't previously key-flag UD, make it so & save default data
      { if (AccEntry->GetKeyCode() != dlg.text->keycode || AccEntry->GetFlags() != dlg.text->flags) // but 1st check 1 of these changed, not just Help or Label
          { AccEntry->SetDefaultKeyCode(AccEntry->GetKeyCode()); AccEntry->SetDefaultFlags(AccEntry->GetFlags()); 
            AccEntry->SetKeyFlagsUserDefined(true);            // Switch on the key/flags flag
          }
      }
    
    AccEntry->SetKeyCode(dlg.text->keycode);                   // Now we can insert the new data
    AccEntry->SetFlags(dlg.text->flags);
  }

if (dlg.label != AccEntry->GetLabel())                         // If the Label or Help-string changed, save & set appropriate flag
  { AccEntry->SetLabel(dlg.label); AccEntry->SetUserDefined(AccEntry->GetUserDefined() | UD_LABEL); }
if (dlg.help != AccEntry->GetHelpstring())
  { AccEntry->SetHelpstring(dlg.help); AccEntry->SetUserDefined(AccEntry->GetUserDefined() | UD_HELP); }

AL->dirty = true;

if (ArrayIndex == SHCUT_ESCAPE - SHCUT_START) // If it's the Escape shortcut that's been changed, cache the value as MyApp::FilterEvent needs it
  { wxGetApp().SetEscapeKeycode(AccEntry->GetKeyCode());
    wxGetApp().SetEscapeKeyFlags(AccEntry->GetFlags());
  }

DisplayItem(ArrayIndex, true);                                 // Update the display.  True makes it refresh, not append
if (result != XRCID("Override"))  return;                      // If we're not stealing another entry's accelerator, we're done

AccelEntry* DupedEntry = Temparray.Item(DuplicateAccel);       // Get the duplicated shortcut & zero its data
if (!DupedEntry->IsKeyFlagsUserDefined())                      // If the entry was previously not key-flag UD, save defaults
  { DupedEntry->SetDefaultKeyCode(DupedEntry->GetKeyCode()); DupedEntry->SetDefaultFlags(DupedEntry->GetFlags()); }
DupedEntry->SetKeyCode(0); DupedEntry->SetFlags(0);
                  // Whether or not it was user-defined before it will be now, unless the default is to have no accelerator!
DupedEntry->SetKeyFlagsUserDefined(DupedEntry->GetKeyCode(true) || DupedEntry->GetFlags(true));
DisplayItem(DuplicateAccel, true);                             // Update the display of the item
                  // Now we need to allow the user to provide a new accelerator if he wishes
wxListEvent newevent(wxEVT_COMMAND_LIST_ITEM_ACTIVATED);       // Make a fake activation event, in the name of the burgled shortcut
newevent.m_item.m_data = DuplicateAccel;                       // Store the only 2 bits of info we use, array index & listctrl index
newevent.m_itemIndex = GetListnoFromArrayNo(DuplicateAccel);
ProcessEvent(newevent);
}

int MyShortcutPanel::CheckForDuplication(int key, int flags, wxString& label, wxString& help)  // Returns the entry corresponding to this accel if there already is one, else wxNOT_FOUND
{
if (!key) return wxNOT_FOUND;

for (int n=0; n < (int)Temparray.GetCount(); ++n)
  if (Temparray.Item(n)->GetKeyCode() == key && Temparray.Item(n)->GetFlags() == flags)
    { if (n != ArrayIndex) return n;                          // Found a match. Return it if it's not the original item, which the user failed to amend
      if (Temparray.Item(n)->GetLabel() == label && Temparray.Item(n)->GetHelpstring() == help)
            return SH_UNCHANGED;                              // Nothing was changed, so signal the fact to caller
       else return wxNOT_FOUND;                               // The Help or Label strings were changed.  wxNOT_FOUND will deal with these
    }

return wxNOT_FOUND;                                           // If we're here, there wasn't a match
}

void MyShortcutPanel::OnHideMnemonicsChk(wxCommandEvent& event)
{
wxConfigBase::Get()->Write(wxT("/Configure/Shortcuts/hide_mnemonics"), HideMnemonicsChk->IsChecked());

LoadListCtrl();
DoSizeCols();
}

void MyShortcutPanel::OnSize(wxSizeEvent& event)
{
DoSizeCols();
event.Skip();
}

void MyShortcutPanel::OnColDrag(wxListEvent& event)  // If the user adjusts a col header position
{
wxUnusedVar(event);
DoSizeCols();
}

void MyShortcutPanel::SizeCols(wxNotifyEvent& event)  // Parent panel sends this event when everything should be properly created
{
wxUnusedVar(event);
DoSizeCols();
}

void MyShortcutPanel::DoSizeCols()  // Common to both SizeCols and OnSize. Sizes col 3 to fill the spare space
{
if (!list) return;  // Too soon
int width, height; list->GetClientSize(&width, &height);

int spare = width - list->GetColumnWidth(0) - list->GetColumnWidth(1);  // Cols 0 & 1 have self-sized to fit the text.  Give col 3 the spare
spare -= 15;                                                            // Unfortunately this somehow makes the cols a bit too big, so a scrollbar appears
list->SetColumnWidth(2, spare);
}

void MyShortcutPanel::OnFinished(wxCommandEvent& event)  // On Apply or Cancel.  Saves data if appropriate, does dtor things
{
if (event.GetId() == XRCID("UDSCancel"))
  { if (AL->dirty)
      { wxMessageDialog dialog(this, _("Lose changes?"), _("Are you sure?"), wxYES_NO | wxICON_QUESTION );
        if (dialog.ShowModal() != wxID_YES)  return;
      }
  }
  
if (event.GetId() == XRCID("UDSApply"))                          // If it's an OK
  { if (AL->dirty)                                               //   & there are changes, save the data to parent array & tell parent
      { AL->SaveUserDefinedShortcuts(Temparray);
        AL->Init();                                              // This deletes the main array & reloads it with the new data
        wxNotifyEvent event(MyReloadAcceleratorsEvent);          // Send a custom event, eventually to MyFrame, to tell it to update menus, accel tables
        wxPostEvent(wxTheApp->GetTopWindow(), event);
      }
  }
 else
  if (FromNotebook)
    { Init(AL, FromNotebook); }                                  // Cancel needs to redo the listctrl as the dialog will remain visible

WX_CLEAR_ARRAY(Temparray);
AL->dirty = false;

if (FromNotebook)                                               // If we came from ConfigureNotebook, reload data as we're not EndingModal
  { ArrayOfAccels mainarray = AL->GetAccelarray();
    for (size_t n=0; n < mainarray.GetCount(); ++n)
        Temparray.Add(new AccelEntry(*mainarray.Item(n)));
  }
  else                                                          // If we came from a dialog, close it
  { HelpContext = CallersHelpcontext;                           //  after restoring the caller's helpcontext
    return ((wxDialog*)GetParent())->EndModal(event.GetId() == XRCID("UDSApply") ? wxID_OK : wxID_CANCEL);
  }
}

IMPLEMENT_DYNAMIC_CLASS(MyShortcutPanel,wxPanel)

BEGIN_EVENT_TABLE(MyShortcutPanel,wxPanel)
  EVT_LIST_ITEM_ACTIVATED(-1,MyShortcutPanel::OnEdit)
  EVT_CHECKBOX(XRCID("HideMnemonicsChk"), MyShortcutPanel::OnHideMnemonicsChk)
  EVT_BUTTON(XRCID("UDSApply"), MyShortcutPanel::OnFinished)
  EVT_BUTTON(XRCID("UDSCancel"), MyShortcutPanel::OnFinished)
  EVT_MYLISTCTRL(MyShortcutPanel::SizeCols)
  EVT_SIZE(MyShortcutPanel::OnSize)
END_EVENT_TABLE()


void ChangeShortcutDlg::Init(wxString& action, wxString& current, wxString& dfault, AccelEntry* ntry)
{
entry = ntry;
text = (NewAccelText*)FindWindow(wxT("Text"));
text->Init(current);                  // Set the textctrl to the current key combination
text->keycode = entry->GetKeyCode();  // Store current keycode/flags here, lest user clicks OK without making a keypress, so leaving the ints empty
text->flags = entry->GetFlags();

((wxButton*)FindWindow(wxT("DefaultBtn")))->Enable((dfault != current));       // If no separate default, turn off its button
wxString statictext;
if (dfault.IsEmpty())
  { if (current.IsEmpty())    statictext = _("None");                        // If no default & no current, say there's no data
     else statictext = (entry->GetUserDefined() ?  _("None") : _("Same")); // If current IS the default, say 'same'
  }

defaultstatic = (wxStaticText*)FindWindow(wxT("DefaultStatic"));
defaultstatic->SetLabel(dfault.IsEmpty() ? statictext : dfault);               // Set the static to default value, or "None"/"Same"

((wxStaticText*)FindWindow(wxT("ActionStatic")))->SetLabel(action);            // Set this one to the name of the function  eg Paste

SetEscapeId(wxID_NONE); // If we don't, it's impossible to use the ESC key as a shortcut; it just closes the dialog
}

void ChangeShortcutDlg::OnButton(wxCommandEvent& event)
{
if (event.GetId() == XRCID("DefaultBtn"))
  { text->SetValue(defaultstatic->GetLabel());                 // Reset the textctrl to default value
    text->keycode = entry->GetKeyCode(true);                   // Store default keycode/flags here (the 'true' parameter says we want the default value)
    text->flags = entry->GetFlags(true);
  }
 else if (event.GetId() == XRCID("ChangeLabel"))
  { wxString newlabel = wxGetTextFromUser(_("Type in the new Label to show for this menu item"), _("Change label"), label);
    if (!newlabel.IsEmpty())                                   // We certainly don't want a blank label!
      { label = newlabel;
        ((wxStaticText*)FindWindow(wxT("ActionStatic")))->SetLabel(newlabel);
      }
  }  
 else if (event.GetId() == XRCID("ChangeHelp"))
       help = wxGetTextFromUser(_("Type in the Help string to show for this menu item\nCancel will Clear the string"), _("Change Help String"), help);
  
 else if (event.GetId() == wxID_CANCEL)
       EndModal(wxID_CANCEL);

 else
  { text->keycode = text->flags = 0;                           // 'Clear' button, so clear the keycode/flags store
    text->SetValue(wxEmptyString);                             // Clear the textctrl
  }

text->SetFocus();                                              // Need to return the focus to the textctrl after
}

void ChangeShortcutDlg::EndModal(int retCode)  // Check that the code that was input is not already in use
{
if (retCode == wxID_OK)                                        // We're only interested if the user WANTS the result
  { int item = parent->CheckForDuplication(text->keycode, text->flags, label, help);
    if (item == SH_UNCHANGED)   retCode = wxID_CANCEL;         // If the user clicked OK without altering the data, convert it into a Cancel
      else 
       if (item != wxNOT_FOUND)                                // If wxNOT_FOUND, there's no duplication so no problem
        { wxString dup = parent->Temparray.Item(item)->GetLabel();
          DupAccelDlg dlg; wxXmlResource::Get()->LoadDialog(&dlg, this, wxT("DupAccelDlg"));
          ((wxStaticText*)dlg.FindWindow(wxT("Duplicate")))->SetLabel(dup);
          retCode = dlg.ShowModal();                           // Ask whether to override the duplicated code, abort, or try again
          if (retCode == XRCID("TryAgain"))                    // Try again, so cancel the EndModal.  Otherwise fall thru to it
            { text->SetFocus(); return; }
          if (retCode == XRCID("Override")) parent->DuplicateAccel = item;  // If overriding, store the bereaved entry's index here
        }
  }

wxDialog::EndModal(retCode);
}

IMPLEMENT_DYNAMIC_CLASS(ChangeShortcutDlg,wxDialog)

BEGIN_EVENT_TABLE(ChangeShortcutDlg,wxDialog)
  EVT_BUTTON(XRCID("DefaultBtn"), ChangeShortcutDlg::OnButton)
  EVT_BUTTON(XRCID("ChangeLabel"), ChangeShortcutDlg::OnButton)
  EVT_BUTTON(XRCID("ChangeHelp"), ChangeShortcutDlg::OnButton)
  EVT_BUTTON(XRCID("ClearBtn"), ChangeShortcutDlg::OnButton)
  EVT_BUTTON(wxID_CANCEL, ChangeShortcutDlg::OnButton)
END_EVENT_TABLE()


void NewAccelText::OnKey(wxKeyEvent& event)
{
int key = event.GetKeyCode();
if (key == WXK_SHIFT || key == WXK_CONTROL || key == WXK_ALT) return;  // We don't want to see metakeys alone

int metakeys = 0;
if (event.ControlDown())  metakeys |= wxACCEL_CTRL;
if (event.ShiftDown())    metakeys |= wxACCEL_SHIFT;
if (event.AltDown())      metakeys |= wxACCEL_ALT;

if (key == keycode && metakeys == flags) return;                       // No change from previous iteration

keycode = key; flags = metakeys;                                       // OK, this is a real, new selection.  Store it

wxString accel = MakeAcceleratorLabel(metakeys, key);                  // Now translate all this into a displayable string
SetValue(accel);
}

IMPLEMENT_DYNAMIC_CLASS(NewAccelText,wxTextCtrl)

BEGIN_EVENT_TABLE(NewAccelText,wxTextCtrl)
  EVT_KEY_DOWN(NewAccelText::OnKey)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(DupAccelDlg,wxDialog)
  EVT_BUTTON(-1, DupAccelDlg::OnButtonPressed)
END_EVENT_TABLE()

