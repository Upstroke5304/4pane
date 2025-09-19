/////////////////////////////////////////////////////////////////////////////
// Name:       Tools.cpp
// Purpose:    Find/Grep/Locate, Terminal Emulator
// Part of:    4Pane
// Author:     David Hart
// Copyright:  (c) 2020 David Hart
// Licence:    wxWindows licence
/////////////////////////////////////////////////////////////////////////////
// For compilers that support precompilation, includes "wx/wx.h"
#include "wx/wxprec.h" 

#include "wx/log.h"
#include "wx/app.h"
#include "wx/frame.h" 
#include "wx/xrc/xmlres.h"
#include "wx/spinctrl.h"
#include "wx/splitter.h"
#include <wx/file.h>
#include "wx/dragimag.h"
#include "wx/config.h"
#include <wx/dirctrl.h>
#include <wx/textfile.h>
#include <wx/clipbrd.h>

#include "Externs.h"
#include "Tools.h"
#include "MyDirs.h"
#include "MyFiles.h"
#include "MyGenericDirCtrl.h"
#include "MyFrame.h"
#include "Filetypes.h"
#include "Accelerators.h"
#include "Misc.h"

#include <sys/types.h>
#include <unistd.h>
#if defined __WXGTK__
  #include <gtk/gtk.h>
#endif

bool CreateLink(const wxString& oldname, const wxString& newname, bool MakeHardlink)  //  Encapsulates creating a link. Used also in rename, move etc
{
wxCHECK_MSG(!oldname.empty() && !newname.empty(), false, wxT("An empty name passed"));

FileData stat(newname);
if (stat.IsValid())                                              // First check there isn't already a filepath of this name
  { wxMessageBox(_("There is already a file with this name in the destination path.  Sorry"), wxT("4Pane"), wxOK | wxCENTRE, MyFrame::mainframe->GetActivePane()); return false; }

wxString dpath(newname.BeforeLast(wxFILE_SEP_PATH));             // Now check for permission to write to the destination dir
if (dpath.empty()) dpath = wxT("/");                             // This would happen if trying to create a link in '/'
FileData destpath(dpath);
if (!destpath.CanTHISUserWriteExec())
  { wxMessageBox(_("I'm afraid you lack permission to write to the destination directory.  Sorry"), wxT("4Pane"), wxOK | wxCENTRE, MyFrame::mainframe->GetActivePane()); return false; }

if (MakeHardlink)                                                // There are a couple of constraints on hardlink creation
  { bool failed = false; wxString msg;
    FileData originfd(oldname);
    if (originfd.IsDir()) { failed = true; msg = _("You cannot make a hardlink to a directory."); }
    if (originfd.GetDeviceID() != destpath.GetDeviceID()) { failed = true; msg = _("You cannot make a hardlink to a different device or partition."); }
    if (failed)
      { wxMessageDialog ask(MyFrame::mainframe->GetActivePane(), msg+_("\nWould you like to make a symlink instead?"), wxString(_("No can do")), wxYES_NO | wxICON_QUESTION );
        if (ask.ShowModal() != wxID_YES) return false;
          else MakeHardlink = false;
      }
  }

int answer;
if (!MakeHardlink) answer = symlink(oldname.mb_str(wxConvUTF8), newname.mb_str(wxConvUTF8));  // Makes the link.  If successful returns false, otherwise -1
  else             answer = link(oldname.mb_str(wxConvUTF8), newname.mb_str(wxConvUTF8));
return (answer==0);
}

bool CreateSymlinkWithParameter(wxString oldname, wxString newname, enum changerelative rel)  //  CreateSymlink() with the option of changing from abs<->relative
{
if (rel == nochange) return CreateSymlink(oldname, newname);  // No change, so just relay to CreateSymlink()

wxString path = newname.BeforeLast(wxFILE_SEP_PATH);
wxFileName fn(oldname);

if (rel == makerelative && fn.IsAbsolute())                   // Otherwise make any necessary alterations, then do so
  { if (fn.MakeRelativeTo(path)==false) return false;
    // I had symlink-containing filepaths which had >1 way of being expressed as a relative path.
    // The one wxFileName::MakeRelativeTo returned then wasn't recognised by stat(), so the link was marked as broken.
    // A workaround is to use realpath() to change the path to something that stat() can cope with.
    // However that's likely to be an ugly filepath containing multiple ../../../
    // so try the standard way first; only if there's a problem use GetUltimateDestination (which calls realpath())
    if (!CreateSymlink(fn.GetFullPath(), newname)) return false;
    FileData stat(newname);
    if (!stat.IsBrokenSymlink()) return true; // Worked, nothing seems broken, so return

    // We have the above problem so do it the ugly way
    wxFileName bad(stat.GetFilepath());
    MyGenericDirCtrl::ReallyDelete(&bad); // First delete the newly-created broken link
    fn = wxFileName(FileData::GetUltimateDestination(oldname));
    if (fn.MakeRelativeTo(path)==false) return false;
  }


if (rel == makeabsolute && fn.IsRelative())
  if (fn.MakeAbsolute(path)==false) return false;
  
return CreateSymlink(fn.GetFullPath(), newname);
}

bool CreateSymlink(wxString oldname, wxString newname)  //  Encapsulates creating a symlink
{
return CreateLink(oldname, newname, false);
}

bool CreateHardlink(wxString oldname, wxString newname)  //  Encapsulates creating a Hardlink
{
return CreateLink(oldname, newname, true);
}

int _CheckForSu(const wxString& command)  // Used by TerminalEm::CheckForSu and LaunchMiscTools::Run
{
if (command.IsEmpty()) return 0;
  // Returns 0 for 'su', so the caller should abort; 1 for a standard command; 2 for su -c or sudo, which requires ExecuteInPty()
wxString cmd(command); cmd.Trim().Trim(false);
if ((cmd == wxT("su")) || (cmd == wxT("sudo su")) || (cmd == wxT("sudo bash")))
  return 0;

if ((cmd.Left(3) == wxT("su ") && (cmd.Mid(3).Trim(false).Left(3) == wxT("-c ")) && (!cmd.Mid(3).Trim(false).Left(3).Trim(false).empty()))
             || cmd.Left(5) == (wxT("sudo ")))
  { return 2; }                                     // It's a su -c or a sudo command

return 1;                                           // If we're here, it's a standard command
}

//-----------------------------------------------------------------------------------------------------------------------

void MyPipedProcess::SendOutput()    // Sends user input from the terminal to a running process
{
if (InputString.IsEmpty()) return;

InputString += '\n';                                  // The \n was swallowed by the textctrl, so add it back here (otherwise the stream blocks)

wxTextOutputStream os(*GetOutputStream());            // Make a text-stream & send it the string
os.WriteString(InputString);

InputString.Clear();                                  // Clear the string ready for more input
}

bool MyPipedProcess::HasInput()    // The following methods are adapted from the exec sample.  This one manages the stream redirection
{
char c;

bool hasInput = false;
    // The original used wxTextInputStream to read a line at a time.  Fine, except when there was no \n, whereupon the thing would hang
    // Instead, read the stream (which may occasionally contain non-ascii bytes e.g. from g++) into a memorybuffer, then to a wxString
while (IsInputAvailable())                            // If there's std input
  { wxMemoryBuffer buf;
    do
      { c = GetInputStream()->GetC();                 // Get a char from the input
        if (GetInputStream()->Eof()) break;           // Check we've not just overrun
        
        buf.AppendByte(c);
        if (c==wxT('\n')) break;                      // If \n, break to print the line
      }
      while (IsInputAvailable());                     // Unless \n, loop to get another char
    wxString line((const char*)buf.GetData(), wxConvUTF8, buf.GetDataLen()); // Convert the line to utf8

    text->AddInput(line);                             // Either there's a full line in 'line', or we've run out of input. Either way, print it
    matches = true;

    hasInput = true;
  }

while (IsErrorAvailable())                            // Ditto with std error
  { wxMemoryBuffer buf;
    do
      { c = GetErrorStream()->GetC();
        if (GetErrorStream()->Eof()) break;
        
        buf.AppendByte(c);
        if (c==wxT('\n')) break;
      }
      while (IsErrorAvailable());
    wxString line((const char*)buf.GetData(), wxConvUTF8, buf.GetDataLen());

    text->AddInput(line);
    matches = true;

    hasInput = true;
  }

return hasInput;
}


void MyPipedProcess::OnTerminate(int pid, int status)  // When the subprocess has finished, show the rest of the output
{
while (HasInput()) ;

if (!matches && DisplayMessage)                     // If there was no other output, emit a polite message if this is appropriate ie from find, not terminal
  { (*text) << _("Sorry, no match found.");
    if (!(rand() % 10))
          (*text) << _("  Have a nice day!\n");
     else  (*text) << wxT("\n");
  }

text->OnProcessTerminated(this);
}

void MyPipedProcess::OnKillProcess()
{
if (!m_pid) return;

  // Take the pid we stored earlier & kill it.  SIGTERM seems to work now we use wxEXEC_MAKE_GROUP_LEADER and wxKILL_CHILDREN
wxKillError result = Kill(m_pid, wxSIGTERM, wxKILL_CHILDREN);
if (result == wxKILL_OK)
  { while (HasInput());                               // Flush out any outstanding results before we boast
    (*text) << _("Process successfully aborted\n");
  }
 else
  { (*text) << _("SIGTERM failed\nTrying SIGKILL\n"); // If we could be bothered, result holds the index in an enum that'd tell us why it failed
    result = Kill(m_pid, wxSIGKILL, wxKILL_CHILDREN); // Fight dirty
    if (result == wxKILL_OK)
      { while (HasInput());
        (*text) << _("Process successfully killed\n");
      }
     else
      { (*text) << _("Sorry, Cancel failed\n"); }
  }
}



void MyBottomPanel::OnButtonPressed(wxCommandEvent& event)
{
int id = event.GetId();

if (id == XRCID("DoItBtn"))
  { switch(tool->WhichTool)
      { case locate:  tool->Locate(); return;
        case find:    tool->DoFind(); return;//tool->FindOrGrep(wxT("find")); return; 
        case grep:    tool->DoGrep(); return;
         default:     return;
      }
  }

if (id == XRCID("Clear"))       { tool->Clear(); tool->text->SetFocus(); return; }
if (id == XRCID("Close"))       { tool->Close(); return; }
if (id == XRCID("Cancel"))      { tool->text->OnCancel(); tool->text->SetFocus(); return; }
}

IMPLEMENT_DYNAMIC_CLASS(MyBottomPanel,wxPanel)

BEGIN_EVENT_TABLE(MyBottomPanel,wxPanel)
  EVT_BUTTON(wxID_ANY, MyBottomPanel::OnButtonPressed)
END_EVENT_TABLE()

void ReplaceMenuitemLabel(int menu_id, const wxString& oldstring, const wxString& newstring)
{
wxCHECK_RET(!(oldstring.IsEmpty() || newstring.IsEmpty()), wxT("Invalid string parameter"));

// I'd originally tried to use wxMenu::GetLabel to get the current label
// However in <wx2.9 this doesn't return the shortcut
AccelEntry* entry= MyFrame::mainframe->AccelList->GetEntry(menu_id);
wxCHECK_RET(entry, wxT("The menuitem id wasn't found"));

wxString label = entry->GetLabel();
wxCHECK_RET(!label.IsEmpty(), wxT("No label??"));

label.Replace(oldstring, newstring, false);
// Don't check that a replacement occurred; it won't if we're returning to the original situation

label = AcceleratorList::ConstructLabel(entry, label);  // Add the shortcut
MyFrame::mainframe->GetMenuBar()->SetLabel(menu_id, label);
}

const wxChar* Tools::FindSubpathArray[] = { wxT("PathCombo"), wxT("name"), wxT("NewerFile"), wxT("ExecCmd"),
                                 wxT("ExcludePattern"), wxT("IncludePattern"), wxT("SearchPattern"), wxT("PathGrep") };

Tools::Tools(LayoutWindows* layout) : Layout(layout), bottom(NULL)
{
WhichTool = null;                                     // Reset WhichTool here, so we can use its nullness as a test for current tool existence

ToolNamesArray.Add(wxT("locate"));
ToolTipsArray.Add(_("Run the 'locate' dialog"));
ToolNamesArray.Add(wxT("find"));
ToolTipsArray.Add(_("Run the 'find' dialog"));
ToolNamesArray.Add(wxT("grep"));
ToolTipsArray.Add(_("Run the 'grep' dialog"));

historylist[0] = &Paths;                              // Load the historylist array, to facilitate use in loops
historylist[1] = &Name;
historylist[2] = &NewerFile;
historylist[3] = &ExecCmd;
historylist[4] = &ExcludePattern;
historylist[5] = &IncludePattern;
historylist[6] = &Pattern;
historylist[7] = &PathGrep;
}

Tools::~Tools()
{
SaveHistory();
}

void Tools::Init(enum toolchoice toolindex)
{
if (WhichTool != toolindex)                           // First check to ensure we're not trying to load the already-loaded panel!
  { if (WhichTool != null && WhichTool != cmdline)                            // If !null, there's a different one currently loaded.  So remove it
      { SaveHistory(); History.Clear(); }             // Save then clear the current history to avoid cross-contamination

    if (bottom == NULL)  // Load the control-panel to go onto the bottompanel pane
      { bottom = (MyBottomPanel*)wxXmlResource::Get()->LoadPanel(Layout->bottompanel, wxT("Locate"));
        bottom->Init(this);
        Layout->bottompanel->GetSizer()->Add(bottom, 1, wxEXPAND);
        Layout->bottompanel->GetSizer()->Layout();
        text = (TerminalEm*)(bottom->FindWindow(wxT("text")));
        text->multiline = true;  text->Init();        // We want this to be the multiline version. Xrc doesn't call the real ctor, so do it here.  Ditto Init
      }

    wxWindow* Cancel = bottom->FindWindow(XRCID("Cancel"));
    if (Cancel) Cancel->Disable();                    // Start with the Cancel button disabled

    WhichTool = toolindex;                            // Save toolindex in case of re-entry
    LoadHistory();                                    // Load the relevant command history into the array
    Layout->EmulatorShowing = true;                   // Flag to parent that there's an emulator here to receive cd.s
    ReplaceMenuitemLabel(SHCUT_TERMINAL_EMULATOR, _("Show"), _("Hide"));  // Change the menuitem from Show to Hide
  }

wxButton* DoIt = (wxButton*)bottom->FindWindow(wxT("DoItBtn"));
if (DoIt)
  { switch(toolindex)
      { case locate:      DoIt->SetLabel(ToolNamesArray[toolindex]);  // Rename the button to 'locate'
                          DoIt->SetToolTip(ToolTipsArray[toolindex]);
                          DoIt->Show(); bottom->Layout();
                          Locate(); return;
        case find:        DoIt->SetLabel(ToolNamesArray[toolindex]);  // Rename the button to 'find'
                          DoIt->SetToolTip(ToolTipsArray[toolindex]);
                          DoIt->Show(); bottom->Layout();
                          DoFind(); return;
        case grep:        DoIt->SetLabel(ToolNamesArray[toolindex]);  // Rename the button to 'grep'
                          DoIt->SetToolTip(ToolTipsArray[toolindex]);
                          DoIt->Show(); bottom->Layout();
                          DoGrep(); return;                           // This does either QuickGrep or ordinary Grep
        case terminalem:  DoIt->Show(false); // Hide the button
                          text->SetFocus();
                          return;
        case cmdline:     DoIt->Show(false); // Hide the button
                          CommandLine();                              // Set up the command-line
                          Layout->commandline->SetOutput(text);       // Tell it where its output should go
                          Layout->commandline->SetFocus();
                          return;
          default:        return;
      }
  }
}

void Tools::LoadHistory()    // Load the relevant history into the stringarray & combobox
{
if (WhichTool > grep) return;                       // as the terminal/commandline history is dealt with separately

config = wxConfigBase::Get();                       // Find the config data
if (config==NULL)  { wxLogError(_("Couldn't load configuration!?")); return; }

wxString subpath = ToolNamesArray[WhichTool];       // Use this name to create the config path eg /History/Locate
config->SetPath(wxT("/History/") + subpath + wxT("/"));

size_t count = config->GetNumberOfEntries();
if (!count) { config->SetPath(wxT("/")); return; }

wxString item, key, histprefix(wxT("hist_"));  
for (size_t n=0; n < count; ++n)                    // Create the key for every item, from hist_a to hist_('a'+count-1)
  { wxLogNull PreventWarningsAboutFailedEnvironmentExpansion; // which can happen from wxExpandEnvVars() with unusual grep strings
    key.Printf(wxT("%c"), wxT('a') + (wxChar)n);    // Make key hold a, b, c etc
    config->Read(histprefix+key, &item);            // Load the entry
    if (item.IsEmpty()) continue;
    History.Add(item);                              // Store it in the array
  }
  
LoadSubHistories();

config->SetPath(wxT("/"));  
}

void Tools::LoadSubHistories()    // Load histories for Find & Grep comboboxes
{
for (size_t i=0; i < 4; ++i)
  { wxString name = FindSubpathArray[i];            // The 'XRCID' string of the combobox to load eg PathCombo
    config->SetPath(wxT("/History/Find/") + name);  // Use this name to create the config path eg /History/Find/PathCombo
    
    historylist[i]->Clear();                        // This is where the data will be stored. Clear in case this is a reload
    
    size_t count = config->GetNumberOfEntries();
    if (!count) continue;
    
    wxString item, key, histprefix(wxT("hist_"));  
    for (size_t n=0; n < count; ++n)                // Create the key for every item, from hist_a to hist_('a'+count-1)
      { wxLogNull PreventWarningsAboutFailedEnvironmentExpansion; // which can happen from wxExpandEnvVars() with unusual grep strings
        key.Printf(wxT("%c"), wxT('a') + (wxChar)n); // Make key hold a, b, c etc
        config->Read(histprefix+key, &item);
        if (item.IsEmpty()) continue;
        historylist[i]->Add(item);                  // Store it in correct array
      }
  }

for (size_t i=4; i < 8; ++i)
  { wxString name = FindSubpathArray[i];            // The 'XRCID' string of the combobox to load eg PathCombo
    config->SetPath(wxT("/History/Grep/") + name);  // Use this name to create the config path eg /History/Find/PathCombo
    
    historylist[i]->Clear();                        // This is where the data will be stored. Clear in case this is a reload
    
    size_t count = config->GetNumberOfEntries();
    if (!count) continue;
    
    wxString item, key, histprefix(wxT("hist_"));  
    for (size_t n=0; n < count; ++n)
      { wxLogNull PreventWarningsAboutFailedEnvironmentExpansion; // which can happen from wxExpandEnvVars() with unusual grep strings
        key.Printf(wxT("%c"), wxT('a') + (wxChar)n);
        config->Read(histprefix+key, &item);
        if (item.IsEmpty())   continue;
        historylist[i]->Add(item);
      }
  }
}

void Tools::SaveHistory()        // Save the relevant command history
{
if (WhichTool > grep) return;                       // as their histories are dealt with separately

config = wxConfigBase::Get();
if (config==NULL)  { wxLogError(_("Couldn't load configuration!?")); return; }

wxString subpath = ToolNamesArray[ WhichTool ];     // Use this name to create the config path eg /History/Locate
config->DeleteGroup(wxT("/History/") + subpath);    // Delete current info, otherwise we'll end up with duplicates or worse
subpath += wxT("/");                                // Now we can add a '/'. DeleteGroup would have failed if it was there
config->SetPath(wxT("/History/") + subpath);

size_t count = wxMin(History.GetCount(), MAX_COMMAND_HISTORY);  // Count the entries to be saved: not TOO many
if (!count) { config->SetPath(wxT("/")); return; }

wxString item, key, histprefix(wxT("hist_"));       // Create the key for every item, from hist_a to hist_('a'+count-1)
for (size_t n=0;  n < count; ++n)                   // The newest command is stored at the beginning of the array
  { key.Printf(wxT("%c"), wxT('a') + (wxChar)n);    // Make key hold a, b, c etc
    config->Write(histprefix+key, History[n]);      // Save the entry
  }

SaveSubHistories();                                 // Save histories for Find & Grep comboboxes

config->Flush();
config->SetPath(wxT("/"));  
}

void Tools::SaveSubHistories()  // Save histories for Find & Grep comboboxes
{
for (size_t i=0; i < 4; ++i)                // Find
  { wxString name = FindSubpathArray[i];             // The 'XRCID' string of the combobox to load eg PathCombo
    config->SetPath(wxT("/History/Find/") + name);   // Use this name to create the config path eg /History/Find/PathCombo

    size_t count = wxMin(historylist[i]->GetCount(), MAX_COMMAND_HISTORY);  // Count the entries to be saved: not TOO many
    if (!count) continue;
    
    wxString item, key, histprefix(wxT("hist_"));  
    for (size_t n=0; n < count; ++n)                 // Create the key for every item, from hist_a to hist_('a'+count-1)
      { key.Printf(wxT("%c"), wxT('a') + (wxChar)n); // Make key hold a, b, c etc
        config->Write(histprefix+key,(*historylist[i])[n]);  // Save the entry
      }
  }  
  
for (size_t i=4; i < 8; ++i)              // Grep
  { wxString name = FindSubpathArray[i];  
    config->SetPath(wxT("/History/Grep/") + name);

    size_t count = wxMin(historylist[i]->GetCount(), MAX_COMMAND_HISTORY);  // Count the entries to be saved: not TOO many
    if (!count) continue;
    
    wxString item, key, histprefix(wxT("hist_"));  
    for (size_t n=0; n < count; ++n)
      { key.Printf(wxT("%c"), wxT('a') + (wxChar)n);  
        config->Write(histprefix+key,(*historylist[i])[n]);
      }
  }  
}

void Tools::Close()
{
text->OnCancel();                                     // In case we were doing a lengthy Locate etc, this cancels it, avoiding leaving it chugging away in the background

Layout->EmulatorShowing = false;                      // Tell parent that there's no point sending more cd.s
ReplaceMenuitemLabel(SHCUT_TERMINAL_EMULATOR, _("Hide"), _("Show"));  // Change the menuitem to Hide from Show

if (Layout->CommandlineShowing)                       // If there's a command-line, this needs to be removed, otherwise repeated entry will create duplicates
  { Layout->ShowHideCommandLine();
    if (Layout->commandlinepane->GetSizer()->Detach(Layout->commandline))
        { Layout->commandline->Destroy(); Layout->commandline = NULL; }
    ReplaceMenuitemLabel(SHCUT_COMMANDLINE, _("Hide"), _("Show"));  // Change the menuitem to Hide from Show
  }

wxGetApp().GetFocusController().Invalidate(text);
Layout->bottompanel->GetSizer()->Detach(bottom);      // Remove the tool panel
Layout->CloseBottom();                                // Tell parent to shut up shop.  This also saves the command history

      // If the terminalem had keyboard focus, it was retaining it even when shut, -> rare segfaults. So return it to the active pane, or somewhere else that still exists
MyFrame::mainframe->SetSensibleFocus();
}

void Tools::Clear()
{
text->OnClear();
}

void Tools::Locate()
{
wxDialog dlg;
wxXmlResource::Get()->LoadDialog(&dlg, MyFrame::mainframe, wxT("LocateDlg"));
LoadPreviousSize(&dlg, wxT("LocateDlg"));

combo = ((wxComboBox*)dlg.FindWindow(wxT("searchstring")));

for (size_t n=0; n < History.GetCount(); ++n)       // Fill combobox from the command history array
    combo->Append(History[n]);
combo->SetSelection(History.GetCount()-1);          // Needed to make the Down key call up the 1st history item
combo->SetValue(wxT(""));                           // We don't want at history entry showing, so clear the textctrl
HelpContext = HCsearch;

int result = dlg.ShowModal();
SaveCurrentSize(&dlg, wxT("LocateDlg"));
if (result != wxID_OK)  { HelpContext = HCunknown; return; }
HelpContext = HCunknown;

wxString selection = combo->GetValue(); 
if (selection.IsEmpty()) return;

int index = History.Index(selection, false);        // See if this command already exists in the history array
if (index != wxNOT_FOUND)
  History.RemoveAt(index);                          // If so, remove it.  We'll add it back into position zero  
History.Insert(selection, 0);                       // Either way, insert into position zero

combo->Clear();          // We want to add it to the front of the combobox too.  Since there isn't an Insert() method, we have to redo from scratch
for (size_t n=0; n < History.GetCount(); ++n)  combo->Append(History[n]);
combo->SetValue(wxT(""));                           // We don't want the 1st history entry showing, so clear the textctrl

wxString options;                                   // See if any options were checked
if (((wxCheckBox*)dlg.FindWindow(wxT("Existing")))->IsChecked())  options << wxT("-e ");
if (((wxCheckBox*)dlg.FindWindow(wxT("Ignore")))->IsChecked())  options << wxT("-i ");
if (((wxCheckBox*)dlg.FindWindow(wxT("Basename")))->IsChecked())  options << wxT("-b ");
if (((wxCheckBox*)dlg.FindWindow(wxT("Regex")))->IsChecked())  options << wxT("-r ");

wxString command(wxT("locate ")); command << options ;  // Construct the command
command << wxT('"') << selection << wxT('"');
text->HistoryAdd(command);
text->RunCommand(command);
text->SetFocus();
}

void Tools::DoFind()
{
bool quicktype = (WHICHFIND == PREFER_QUICKFIND);
while (1)   // There are now 2 find alternatives: Quickfind for everyday use, and the original full find for special occasions
  if (quicktype) 
    { HelpContext = HCsearch;
      QuickFindDlg dlg;
      {
      wxLogNull NoLogErrorWithStaleXrcFile;
      if (!wxXmlResource::Get()->LoadDialog(&dlg, MyFrame::mainframe, wxT("QuickFindDlg")))
        { quicktype = false; continue; }  // If the dialog didn't load we may be using a pre-5.0 xrc file. Revert to the standard FullFind
      }
      LoadPreviousSize(&dlg, wxT("QuickFindDlg"));
      dlg.Init(this);// dlg.Centre();
      int result = dlg.ShowModal();
      SaveCurrentSize(&dlg, wxT("QuickFindDlg"));

      if (result == XRCID("FullFind"))
        { quicktype = false; continue; }  // If the FullFind button was pressed, try again with this
      HelpContext = HCunknown; return;    // Otherwise something must have gone right, so return
    }
   else
    { if (FindOrGrep(wxT("find")) == XRCID("QuickFind"))
          { quicktype = true; continue; } // If the QuickFind button was pressed, try again with this
      return;
    }
}

void Tools::DoGrep()
{
bool quicktype = (WHICHGREP == PREFER_QUICKGREP);
while (1)   // There are now 2 grep alternatives: Quickgrep for everyday use, and the original full grep for special occasions
  if (quicktype) 
    { HelpContext = HCsearch;
      QuickGrepDlg dlg; wxXmlResource::Get()->LoadDialog(&dlg, MyFrame::mainframe, wxT("QuickGrepDlg"));
      LoadPreviousSize(&dlg, wxT("QuickGrepDlg"));
      dlg.Init(this);// dlg.Centre();
      int result = dlg.ShowModal();
      SaveCurrentSize(&dlg, wxT("QuickGrepDlg"));

      if (result == XRCID("FullGrep"))
        { quicktype = false; continue; }  // If the FullGrep button was pressed, try again with this
      HelpContext = HCunknown; return;    // Otherwise something must have gone right, so return
    }
   else
    { if (FindOrGrep(wxT("grep")) == XRCID("QuickGrep"))
          { quicktype = true; continue; } // If the QuickGrep button was pressed, try again with this
      return;
    }
}

int Tools::FindOrGrep(const wxString commandstart)
{
MyFindDialog dlg;
if (commandstart == wxT("find"))
  { wxXmlResource::Get()->LoadDialog(&dlg, MyFrame::mainframe, wxT("FindDialog"));  // Use the frame as parent, otherwise in 2.8.0 the dialog position is wrong
    LoadPreviousSize(&dlg, wxT("FindDialog"));
  }
 else
  { wxXmlResource::Get()->LoadDialog(&dlg, MyFrame::mainframe,wxT("GrepDialog"));  // Use the frame as parent, also as otherwise after the RegEx help dialog the focus window is wrong. ?Why
    LoadPreviousSize(&dlg, wxT("GrepDialog"));
  }
dlg.Init(this, commandstart);
combo = (wxComboBox*)dlg.FindWindow(wxT("searchstring"));

dlg.Show(); // After 2.5.3, the new method of wxWindow sizing means that a long history string in a combobox will expand the box outside the notebook!  So Show() it before adding the data

if (commandstart == wxT("find"))
  ((MyFindNotebook*)dlg.FindWindow(wxT("Nbk")))->Init(this, combo);
else
  ((MyGrepNotebook*)dlg.FindWindow(wxT("Nbk")))->Init(this, combo);

#if ! defined(__WXGTK__)
  combo->Append(commandstart);  // We don't want the 1st history entry showing, so (inelegantly) insert a 'blank' entry 1st, to be followed by user input (this stopped happening after 2.7.0)
#endif
for (size_t n=0; n < History.GetCount(); ++n)        // Fill combobox from the command history array
  combo->Append(History[n]);
#if (defined(__WXGTK__) || defined(__WXX11__))
  combo->SetValue(commandstart);                     // >2.7.0 needs "find" (or "grep") to be specifically entered
#endif

HelpContext = HCsearch;
int ans = dlg.ShowModal();
if (commandstart == wxT("find")) SaveCurrentSize(&dlg, wxT("FindDialog"));
 else SaveCurrentSize(&dlg, wxT("GrepDialog"));
HelpContext = HCunknown;
if (ans != wxID_OK)  { return ans; }

wxString selection = combo->GetValue();
if (selection == commandstart) return -1;

int index = History.Index(selection, false);        // See if this command already exists in the history array
if (index != wxNOT_FOUND)
  History.RemoveAt(index);                          // If so, remove it.  We'll add it back into position zero  
History.Insert(selection, 0);                       // Either way, insert into position zero

combo->Clear();          // We want to add it to the front of the combobox too.  Since there isn't an Insert() method, we have to redo from scratch
combo->Append(commandstart);                        // See above
for (size_t n=0; n < History.GetCount(); ++n)  combo->Append(History[n]);

if (commandstart == wxT("grep"))    // Grep can't expand its own filepaths with wildcards, so run it with a shell
  { selection = wxT("sh -c \"") + selection; selection += wxT('\"'); }

text->HistoryAdd(selection);
text->RunCommand(selection);
text->SetFocus();
return -1;              // We don't care about the return value here: it's only read for the Quick-Grep button
}

void Tools::CommandLine()
{
Layout->commandline = new TerminalEm(Layout->commandlinepane,-1,wxT(""), false);
Layout->commandlinepane->GetSizer()->Add(Layout->commandline, 1, wxEXPAND);
Layout->commandlinepane->GetSizer()->Layout();
ReplaceMenuitemLabel(SHCUT_COMMANDLINE, _("Show"), _("Hide"));  // Change the menuitem from Show to Hide
}
//-----------------------------------------------------------------------------------------------------------------------
void MyFindDialog::OnClearButtonPressed(wxCommandEvent& event)
{
parent->combo->SetValue(commandstart);
}

BEGIN_EVENT_TABLE(MyFindDialog, wxDialog)
  EVT_BUTTON(XRCID("Clear"), MyFindDialog::OnClearButtonPressed)
  EVT_BUTTON(XRCID("QuickFind"), MyFindDialog::OnQuickGrep) // sic
  EVT_BUTTON(XRCID("QuickGrep"), MyFindDialog::OnQuickGrep)
END_EVENT_TABLE()
//-----------------------------------------------------------------------------------------------------------------------
#if wxVERSION_NUMBER < 2900
  DEFINE_EVENT_TYPE(MyFindGrepEvent)      // An eventtype to post when a delayed SetFocus is needed
#else
  wxDEFINE_EVENT(MyFindGrepEvent, wxCommandEvent);
#endif

IMPLEMENT_DYNAMIC_CLASS(MyFindNotebook, wxNotebook)
IMPLEMENT_DYNAMIC_CLASS(MyGrepNotebook, wxNotebook)

const wxChar* MyFindNotebook::OptionsStrings[] = { wxT("-daystart"), wxT("-depth"), wxT("-follow"), wxT("-noleaf"), wxT("-xdev"), wxT("-help") };
const wxChar* MyFindNotebook::OperatorStrings[] = { wxT("-a"), wxT("-o"), wxT("-not"), wxT("\\("), wxT("\\)"), wxT(",") };
const wxChar* MyFindNotebook::NameStrings4[] = { wxT("-name "), wxT("-iname "), wxT("-lname "), wxT("-ilname "), wxT("-path "), /* The pre-5.0 order */
                                       wxT("-ipath "), wxT("-regex "), wxT("-iregex ") };
const wxChar* MyFindNotebook::NameStrings5[] = { wxT("-name "), wxT("-iname "), wxT("-path "), wxT("-ipath "),
                                       wxT("-regex "), wxT("-iregex "), wxT("-lname "), wxT("-ilname ") };
const wxChar* MyFindNotebook::SizeStrings[] = { wxT("-empty") };
const wxChar* MyFindNotebook::OwnerStrings[] = { wxT("-nouser"), wxT("-nogroup") };
const wxChar* MyFindNotebook::OwnerStaticStrings[] = { wxT("PermStatic1"), wxT("PermStatic2"), wxT("PermStatic3"), wxT("PermStatic4"), wxT("PermStatic5"), wxT("PermStatic6"), wxT("PermStatic7"), wxT("PermStatic8") };
const wxChar* MyFindNotebook::OwnerPermissionStrings[] = { wxT("UserRead"), wxT("GroupRead"), wxT("OtherRead"), wxT("UserWrite"), wxT("GroupWrite"), wxT("OtherWrite"), 
                                          wxT("UserExec"), wxT("GroupExec"), wxT("OtherExec"), wxT("Suid"), wxT("Sguid"), wxT("Sticky") };
unsigned int MyFindNotebook::OwnerPermissionOctal[] = { 0400,040,04, 0200,020,02, 0100,010,01, 04000,02000,01000 };
const wxChar* MyFindNotebook::ActionStrings[] = { wxT("-print"), wxT("-print0"), wxT("-ls"), wxT("-exec %s \\{\\} \\;"), wxT("-ok %s \\{\\} \\;"),
                                       wxT("-printf"), wxT("-fls"), wxT("-fprint"), wxT("-fprint0"), wxT("-fprintf") };

void MyFindNotebook::Init(Tools* dad, wxComboBox* searchcombo)
{
parent = dad;
combo = searchcombo;

GetXRCIDsIntoArrays();

int n = (int)Path;
do ClearPage((enum pagename)n++);
 while ((n+1) <= (int)Actions);

LoadCombosHistory();
currentpage = Path;
DataToAdd = false;
Connect(wxID_ANY, wxEVT_COMMAND_NOTEBOOK_PAGE_CHANGING, (wxObjectEventFunction)&MyFindNotebook::OnPageChanging);
}

void MyFindNotebook::GetXRCIDsIntoArrays()    // Life becomes easier if we can access the XRCID values by looping thru an array
{
PathArray.Add(XRCID("PathCurrentDir"));
PathArray.Add(XRCID("PathHome"));
PathArray.Add(XRCID("PathRoot"));

OptionsArray.Add(XRCID("Daystart"));
OptionsArray.Add(XRCID("Depth"));
OptionsArray.Add(XRCID("Follow"));
OptionsArray.Add(XRCID("Noleaf"));
OptionsArray.Add(XRCID("Xdev"));
OptionsArray.Add(XRCID("Help"));
OptionsArray.Add(XRCID("Maxdepth"));
OptionsArray.Add(XRCID("Mindepth"));

OperatorArray.Add(XRCID("And"));
OperatorArray.Add(XRCID("Or"));
OperatorArray.Add(XRCID("Not"));
OperatorArray.Add(XRCID("OpenBracket"));
OperatorArray.Add(XRCID("CloseBracket"));
OperatorArray.Add(XRCID("List"));

TimeArray.Add(XRCID("TimePlain"));
TimeArray.Add(XRCID("Newer"));
TimeArray.Add(XRCID("LastAccess"));

SizeArray.Add(XRCID("SizeEmpty"));
SizeArray.Add(XRCID("Size"));
SizeArray.Add(XRCID("Filetype"));
  
OwnerArray.Add(XRCID("NoUser"));
OwnerArray.Add(XRCID("NoGroup"));
OwnerArray.Add(XRCID("Owner"));
OwnerArray.Add(XRCID("Permissions"));

ActionArray.Add(XRCID("Print"));
ActionArray.Add(XRCID("Print0"));
ActionArray.Add(XRCID("ls"));
ActionArray.Add(XRCID("Exec"));
ActionArray.Add(XRCID("ok"));
ActionArray.Add(XRCID("Printf"));
ActionArray.Add(XRCID("Fls"));
ActionArray.Add(XRCID("Fprint"));
ActionArray.Add(XRCID("Fprint0"));
ActionArray.Add(XRCID("Fprintf"));
}

void MyFindNotebook::LoadCombosHistory()    // Load the relevant history-arrays into comboboxes
{
wxComboBox* cbox;

for (size_t c=0; c < 4; ++c)                        // For every combobox in the array-list
  { cbox = (wxComboBox*)FindWindow(parent->FindSubpathArray[c]);
    wxArrayString* History = parent->historylist[c];// The history-array that goes with this combobox
    
    #if ! defined(__WXGTK__)
      cbox->Append(wxT(""));                        // This used to be needed to give a blank entry to display in the textbox
    #endif
    for (size_t n=0; n < History->GetCount(); ++n)  // Fill combobox from the corresponding history array
        cbox->Append((*History)[n]);
  }
}

void MyFindNotebook::SaveCombosHistory(int which)   // Save the relevant command history into their arrays
{
wxComboBox* cbox = (wxComboBox*)FindWindow(parent->FindSubpathArray[which]);
wxArrayString* History = parent->historylist[which];// The history-array that goes with this combobox

wxString string = cbox->GetValue();                 // Get the visible string
if (string.IsEmpty()) return;

int index = History->Index(string, false);          // See if this entry already exists in the history array
if (index != wxNOT_FOUND)
  History->RemoveAt(index);                         // If so, remove it.  We'll add it back into position zero  
History->Insert(string, 0);                         // Either way, insert into position zero

                  // Lastly, reload cbox.  This adds the new item in the correct position
cbox->Clear();
#if ! defined(__WXGTK__)
  cbox->Append(wxT(""));                            // This used to be needed to give a blank entry to display in the textbox
#endif
for (size_t n=0; n < History->GetCount(); ++n)      cbox->Append((*History)[n]);                
}

void MyFindNotebook::ClearPage(enum pagename page, int exceptthisitem /*=-1*/)
{
switch(page)
  { case Path: for (int n=0; n < (int)PathArray.GetCount(); ++n)                         // For every checkbox
                  if (PathArray[n] != exceptthisitem)                                    //   (if it's not one we want to ignore)
                      ((wxCheckBox*)FindWindow(PathArray[n]))->SetValue(false);          //     disable.      
                ((wxComboBox*)FindWindow(wxT("PathCombo")))->SetValue(wxT(""));          // Clear the combo textctrl
                return;
                
    case Options:for (int n=0; n < (int)OptionsArray.GetCount(); ++n)                    // For every checkbox
                        ((wxCheckBox*)FindWindow(OptionsArray[n]))->SetValue(false);     //     disable.
                        
                ((wxSpinCtrl*)FindWindow(wxT("maxdepthspin")))->Enable(false);           // Disable the spinctrls
                ((wxSpinCtrl*)FindWindow(wxT("mindepthspin")))->Enable(false);
                return;        

    case Operators: for (int n=0; n < (int)OperatorArray.GetCount(); ++n)
                      if (OperatorArray[n] != exceptthisitem)  
                        ((wxCheckBox*)FindWindow(OperatorArray[n]))->SetValue(false);
                    return;
                        
    case Name:  ((wxComboBox*)FindWindow(wxT("name")))->SetValue(wxT(""));
                ((wxRadioBox*)FindWindow(wxT("Nametype")))->SetSelection(0);
                ((wxRadioButton*)FindWindow(wxT("PruneReturn")))->SetValue(true);
                ((wxCheckBox*)FindWindow(wxT("IgnoreCase")))->SetValue(false);
                return;
    case Time:  for (int n=0; n < (int)TimeArray.GetCount(); ++n)
                  if (TimeArray[n] != exceptthisitem)   ((wxCheckBox*)FindWindow(TimeArray[n]))->SetValue(false);
                  
                ((wxRadioButton*)FindWindow(wxT("TimeAccessMod")))->SetValue(true);
                ((wxRadioButton*)FindWindow(wxT("TimeMore")))->SetValue(true);
                ((wxSpinCtrl*)FindWindow(wxT("TimeSpin")))->SetValue(0);
                ((wxRadioButton*)FindWindow(wxT("TimeDays")))->SetValue(true);

                ((wxRadioButton*)FindWindow(wxT("NewerMod")))->SetValue(true);
                ((wxComboBox*)FindWindow(wxT("NewerFile")))->SetValue(wxT(""));

                ((wxRadioButton*)FindWindow(wxT("LastMore")))->SetValue(true);
                ((wxSpinCtrl*)FindWindow(wxT("LastSpin")))->SetValue(0);
                return;

    case Size:  for (int n=0; n < (int)SizeArray.GetCount(); ++n)
                  if (SizeArray[n] != exceptthisitem)
                    ((wxCheckBox*)FindWindow(SizeArray[n]))->SetValue(false);

                ((wxRadioButton*)FindWindow(wxT("SizeMore")))->SetValue(true);
                ((wxSpinCtrl*)FindWindow(wxT("SizeSpin")))->SetValue(0);
                ((wxRadioButton*)FindWindow(wxT("DimensionKB")))->SetValue(true);
                ((wxRadioBox*)FindWindow(wxT("FiletypeType")))->SetSelection(0);
                return;
    case Owner: for (int n=0; n < (int)OwnerArray.GetCount(); ++n)
                  if (OwnerArray[n] != exceptthisitem)
                    ((wxCheckBox*)FindWindow(OwnerArray[n]))->SetValue(false);

                ((wxRadioButton*)FindWindow(wxT("WhoUser")))->SetValue(true);
                ((wxRadioButton*)FindWindow(wxT("SpecifyName")))->SetValue(0);
                ((wxTextCtrl*)FindWindow(wxT("OwnerName")))->SetValue(wxT(""));
                ((wxRadioButton*)FindWindow(wxT("IDMore")))->SetValue(true);
                ((wxRadioButton*)FindWindow(wxT("SpecifyID")))->SetValue(0);
                ((wxSpinCtrl*)FindWindow(wxT("IDSpin")))->SetValue(0);

                for (int n=0; n < 12; ++n)    // There are oodles of checkboxes for Permissions.  Do in a loop
                  ((wxCheckBox*)FindWindow(OwnerPermissionStrings[n]))->SetValue(false);
                
                ((wxTextCtrl*)FindWindow(wxT("Octal")))->SetValue(wxT(""));
                ((wxRadioButton*)FindWindow(wxT("PermAny")))->SetValue(true);
                return;
    case Actions: for (int n=0; n < 10; ++n)
                    if (ActionArray[n] != exceptthisitem)
                      ((wxCheckBox*)FindWindow(ActionArray[n]))->SetValue(false);  
                return;
    default:    return;    
  }      
}

void MyFindNotebook::OnCheckBox(wxCommandEvent& event)
{
switch(currentpage)
  { case Path:       OnCheckBoxPath(event); return;
    case Time:  
    case Actions:
    case Size:
    case Operators:  if (!event.IsChecked()) return;                     // We're not interested in UNchecking events
                     ClearPage(currentpage, event.GetId());  return;      // Reset all but this one                  
    case Owner:      if (event.IsChecked())
                      for (int n=0; n < (int)OwnerArray.GetCount(); ++n)
                        { if (OwnerArray[n] == event.GetId())             // We don't want to do this for the Permissions checkboxes!
                            { ClearPage(currentpage, event.GetId());  return; }  // Reset all but this one
                        }
                     CalculatePermissions(); return;                      // If we're here, it must've been a Permissions checkbox, so recalculate
    default:         return;
  }
}

void MyFindNotebook::OnCheckBoxPath(wxCommandEvent& event)
{
if (!event.IsChecked()) return;                    // We're not interested in UNchecking events

wxString entry;
int id = event.GetId();
              // Depending on which checkbox was checked, store an appropriate string in entry
if ((id == XRCID("PathCurrentDir")) && MyFrame::mainframe->GetActivePane())
    entry = MyFrame::mainframe->GetActivePane()->GetActiveDirPath();
  else if (id == XRCID("PathHome"))    entry = wxGetApp().GetHOME();
    else if (id == XRCID("PathRoot"))  entry = wxT("/");
      else return;
      
ClearPage(currentpage, event.GetId());                         // Reset the other checkboxes
((wxComboBox*)FindWindow(wxT("PathCombo")))->SetValue(entry);  // Place the result in the combobox
}

void MyFindNotebook::OnAddCommand(wxCommandEvent& event)
{
wxString answer;

switch(currentpage)
  { case Path:     answer = ((wxComboBox*)FindWindow(wxT("PathCombo")))->GetValue();  // Get the data from the combobox
                    if (answer.IsEmpty())  return;
                    answer = wxT('\"') + answer; answer += wxT('\"');
                    SaveCombosHistory(0); break;
    case Options:   answer = OnAddCommandOptions(); break;       // Too complicated to fit here in comfort
    case Operators: answer = OnAddCommandOperators(); break;
    case Name:      answer = OnAddCommandName(); break;
    case Time:      answer = OnAddCommandTime(); break;
    case Size:      answer = OnAddCommandSize(); break;
    case Owner:     answer = OnAddCommandOwner(); break;
    case Actions:   answer = OnAddCommandAction(); break;
    case NotSelected:  return;
  }

AddString(answer);                                  // Add the new data to the dlg command string
ClearPage(currentpage);                             //  & reset the current page
DataToAdd = false;                                  //   & the bool
}

wxString MyFindNotebook::OnAddCommandOptions()  // Depending on which checkboxes were checked, return an appropriate string
{
wxString entry;
                  // Check each checkbox.  If one is checked, add the appropriate string to the end of entry.
for (int n=0; n < 6; ++n)                           // For the first 6 checkboxes (the ones without spinctrls)
  if (((wxCheckBox*)FindWindow(OptionsArray[n]))->IsChecked())
    entry += OptionsStrings[n];                     // If it's checked, add the corresponding command to the string

                  // It's easier to do the other 2 by hand
if (((wxCheckBox*)FindWindow(wxT("Maxdepth")))->IsChecked())  
   { wxString str; str.Printf(wxT("-maxdepth %u"),  ((wxSpinCtrl*)FindWindow(wxT("maxdepthspin")))->GetValue()); entry += str; }
if (((wxCheckBox*)FindWindow(wxT("Mindepth")))->IsChecked())
   { wxString str; str.Printf(wxT("-mindepth %u"),  ((wxSpinCtrl*)FindWindow(wxT("mindepthspin")))->GetValue()); entry += str; }

return entry;        // Return the accumulated data
}

wxString MyFindNotebook::OnAddCommandOperators()  // Depending on which checkboxes were checked, return an appropriate string
{
                  // Check each checkbox.  If one is checked, add the appropriate string to the end of entry.
for (int n=0; n < (int)OperatorArray.GetCount(); ++n)             // For every checkbox
  if (((wxCheckBox*)FindWindow(OperatorArray[n]))->IsChecked())   // If it's checked, this is the one
    return OperatorStrings[n];                                    // so return the associated string

return wxEmptyString;        // Shouldn't happen
}

wxString MyFindNotebook::OnAddCommandTime()  // Depending on which checkboxes were checked, return an appropriate string
{
wxString entry;

if (((wxCheckBox*)FindWindow(wxT("TimePlain")))->IsChecked())
  { unsigned int spin = ((wxSpinCtrl*)FindWindow(wxT("TimeSpin")))->GetValue();
    if (!spin) return wxEmptyString;                             // Time 0 makes no sense
    wxString len; len.Printf(wxT("%u"), spin);                    // Get the figure into string form
    
    if  (((wxRadioButton*)FindWindow(wxT("TimeAccessAcc")))->GetValue())  entry = wxT("-a");
      else if  (((wxRadioButton*)FindWindow(wxT("TimeAccessMod")))->GetValue())  entry = wxT("-m");
        else entry = wxT("-c");
                                                                  // Add min or time, for minutes or days
    if  (((wxRadioButton*)FindWindow(wxT("TimeMins")))->GetValue())  entry += wxT("min ");
      else  entry += wxT("time ");

    if  (((wxRadioButton*)FindWindow(wxT("TimeMore")))->GetValue())  entry += wxT("+");  // Time may be + (more than), - (less than), or exact (no prefix)
      else if  (((wxRadioButton*)FindWindow(wxT("TimeLess")))->GetValue())  entry += wxT("-");

    entry += len;  return entry;                                  // Finally add the time
  }
  
if (((wxCheckBox*)FindWindow(wxT("Newer")))->IsChecked())
  { wxString name = ((wxComboBox*)FindWindow(wxT("NewerFile")))->GetValue();
    if (name.IsEmpty()) return name;
    
    SaveCombosHistory(2);                                         // Save the new command history into array

    if (((wxRadioButton*)FindWindow(wxT("NewerAcc")))->GetValue())  entry = wxT("-anewer \'");
      else if  (((wxRadioButton*)FindWindow(wxT("NewerMod")))->GetValue())  entry = wxT("-mnewer \'");
        else entry = wxT("-cnewer \'");

    entry += name; entry += wxT('\'');  return entry;  
  }
  
if (((wxCheckBox*)FindWindow(wxT("LastAccess")))->IsChecked())
  { unsigned int spin = ((wxSpinCtrl*)FindWindow(wxT("LastSpin")))->GetValue();
    if (!spin) return wxEmptyString;                            // Time 0 makes no sense
    wxString len; len.Printf(wxT("%u"), spin);                   // Get the figure into string form
    
    entry = wxT("-used ");

    if  (((wxRadioButton*)FindWindow(wxT("LastMore")))->GetValue())  entry += wxT("+");  // Time may be + (more than), - (less than), or exact (no prefix)
      else if  (((wxRadioButton*)FindWindow(wxT("LastLess")))->GetValue())  entry += wxT("-");
    
    entry += len;  return entry;                                 // Finally add the time
  }
return wxEmptyString;        // Shouldn't happen
}

wxString MyFindNotebook::OnAddCommandSize()  // Depending on which checkboxes were checked, return an appropriate string
{
wxString entry;

for (int n=0; n < 1; ++n)                                       // For the first checkbox in the array (the one without extras)
  if (((wxCheckBox*)FindWindow(SizeArray[n]))->IsChecked())
      { entry = SizeStrings[n]; return entry; }                 // If it's checked, add the corresponding command to the string

if (((wxCheckBox*)FindWindow(wxT("Size")))->IsChecked())
  { unsigned int spin = ((wxSpinCtrl*)FindWindow(wxT("SizeSpin")))->GetValue();
    wxString len; len.Printf(wxT("%u"), spin);                  // Get the figure into string form  
  
    entry = wxT("-size ");
    
    if (((wxRadioButton*)FindWindow(wxT("SizeMore")))->GetValue())  entry += wxT("+"); // Size may be + (more than), - (less than), or exact (no prefix)
      else if  (((wxRadioButton*)FindWindow(wxT("SizeLess")))->GetValue())  entry += wxT("-");

    entry += len;

    if (((wxRadioButton*)FindWindow(wxT("DimensionBytes")))->GetValue())  entry += wxT("c");  // Bytes, 512-byte blocks, or KB
      else if  (((wxRadioButton*)FindWindow(wxT("DimensionBlocks")))->GetValue())  entry += wxT("b");
        else if  (((wxRadioButton*)FindWindow(wxT("DimensionKB")))->GetValue())  entry += wxT("k");

    return entry;
  }
  
if (((wxCheckBox*)FindWindow(wxT("Filetype")))->IsChecked())
  { entry = wxT("-type ");
    
    int type = ((wxRadioBox*)FindWindow(wxT("FiletypeType")))->GetSelection();
    switch(type)  
      { case 0: entry += wxT("d"); break; case 1: entry += wxT("f"); break; case 2: entry += wxT("l"); break; case 3: entry += wxT("p"); break;
        case 4: entry += wxT("s"); break; case 5: entry += wxT("b"); break; case 6: entry += wxT("c"); 
      }
    return entry;
  }

return wxEmptyString;        // Shouldn't happen
}

wxString MyFindNotebook::OnAddCommandOwner()  // Depending on which checkboxes were checked, return an appropriate string
{
wxString entry;

for (int n=0; n < 2; ++n)                                       // For the first 2 checkboxes in the array (the ones without extras)
  if (((wxCheckBox*)FindWindow(OwnerArray[n]))->IsChecked())
      { entry = OwnerStrings[n]; return entry; }                // If it's checked, add the corresponding command to the string

if (((wxCheckBox*)FindWindow(wxT("Owner")))->IsChecked())
  { bool group = ((wxRadioButton*)FindWindow(wxT("WhoGroup")))->GetValue();  // Find if we're dealing with user or group
                          // There are 2 sections:  for name and for value
    if (((wxRadioButton*)FindWindow(wxT("SpecifyName")))->GetValue())
      { wxString name = ((wxTextCtrl*)FindWindow(wxT("OwnerName")))->GetValue();
        if (name.IsEmpty()) return name;
        
        entry = group ? wxT("-group ") : wxT("-user ");
        entry += name; return entry;
      }
     else
       { entry = group ? wxT("-gid ") : wxT("-uid ");
        
        if (((wxRadioButton*)FindWindow(wxT("IDMore")))->GetValue())  entry += wxT("+");    // ID may be + (more than), - (less than), or exact (no prefix)
          else if  (((wxRadioButton*)FindWindow(wxT("IDLess")))->GetValue())  entry += wxT("-");
        
        unsigned int spin = ((wxSpinCtrl*)FindWindow(wxT("IDSpin")))->GetValue();
        wxString len; len.Printf(wxT("%u"), spin);                          // Get the figure into string form
        entry += len; return entry;
      }
  }  
  
if (((wxCheckBox*)FindWindow(wxT("Permissions")))->IsChecked())
  { entry = wxT("-perm ");
    // May be / (Match Any (i.e. if any matches are found), - (Match Each Specified (ignoring unspecified ones)), or no prefix (Exact Match only)
    if  (((wxRadioButton*)FindWindow(wxT("PermAny")))->GetValue())  entry << wxT("/");  // NB it used to be '+' instead of '/' but that's now deprecated
     else 
       { if (((wxRadioButton*)FindWindow(wxT("PermEach")))->GetValue())  entry << wxT("-"); }

    entry << ((wxTextCtrl*)FindWindow(wxT("Octal")))->GetValue();
    return entry; 
  }

return wxEmptyString;        // Shouldn't happen
}

wxString MyFindNotebook::OnAddCommandName()
{
wxString name = ((wxComboBox*)FindWindow(wxT("name")))->GetValue();
if (name.IsEmpty()) return name;

int ignorecase = ((wxCheckBox*)FindWindow(wxT("IgnoreCase")))->IsChecked();  // This will be 0 or 1

wxString entry(wxT("-name ")); // Least-bad default
wxRadioBox* rb = ((wxRadioBox*)FindWindow(wxT("Nametype")));
if (rb) // I changed the radiobox order after 4.0. Protect against using a stale XRC file
  { int radio = rb->GetSelection();
    if (rb->FindString(wxT("Filename")) != wxNOT_FOUND)
      entry = NameStrings4[ (2*radio) + ignorecase ]; // *2 as there are 2 entries for each type, case-sens & ignorecase
     else
      entry = NameStrings5[ (2*radio) + ignorecase ];
  }

entry += wxT('\"'); entry += (name + wxT('\"'));                             // Add the name, quoted

if (((wxRadioButton*)FindWindow(wxT("PruneIgnore")))->GetValue())            // If the prune control is set, add prune
  entry += wxT(" -prune -or");                                               // (The -or says 'Only do the following statements if the prune statement was false'

SaveCombosHistory(1);
return entry;
}

wxString MyFindNotebook::OnAddCommandAction()  // Depending on which checkboxes were checked, return an appropriate string
{
wxString entry;
                  // Check each checkbox.  If one is checked, add the appropriate string to entry.
for (int n=0; n < 3; ++n)                                      // For the first 3 checkboxes (the ones without extras)
  if (((wxCheckBox*)FindWindow(ActionArray[n]))->IsChecked())
      { entry = ActionStrings[n]; return entry; }              // If it's checked, add the corresponding command to the string
      
for (int n=3; n < 5; ++n)                                      // For the next 2, read the associated combobox
  if (((wxCheckBox*)FindWindow(ActionArray[n]))->IsChecked())
    { wxString execstr = ActionStrings[n]; 
      wxString prog = ((wxComboBox*)FindWindow(wxT("ExecCmd")))->GetValue();  // Get the data from the combobox
      if (prog.IsEmpty()) return prog;
      
      SaveCombosHistory(3);
      
      entry.Printf(execstr.c_str(), prog.c_str());             // execstr holds the command including a %s.  Printf replaces this with prog
      return entry;
    }
  // The rest is complex. There may be 1 active textctrl or 2, out of 4.  I'm using the content of the active ones, without checking which they are.
wxString format, file;
if (FindWindow(wxT("PrintfFormat"))->IsEnabled())
  format = ((wxTextCtrl*)FindWindow(wxT("PrintfFormat")))->GetValue();  // Get the data from the printf format textctrl
if (FindWindow(wxT("FprintfFormat"))->IsEnabled())
  format = ((wxTextCtrl*)FindWindow(wxT("FprintfFormat")))->GetValue(); //    or the fprintf one
if (FindWindow(wxT("FilePlain"))->IsEnabled())
   file = ((wxTextCtrl*)FindWindow(wxT("FilePlain")))->GetValue();      // Similarly with the file textctrls
if (FindWindow(wxT("FprintfFile"))->IsEnabled())
  file =  ((wxTextCtrl*)FindWindow(wxT("FprintfFile")))->GetValue();
  
if (!format.IsEmpty()) { format = wxT(" \'") + format; format += wxT('\''); }  // Doesn't work all in one assignation
if (!file.IsEmpty())      file = wxT(' ') + file;

for (int n=5; n < 10; ++n)                      
  if (((wxCheckBox*)FindWindow(ActionArray[n]))->IsChecked())
    { if (n==5 || n==9)    if (format.IsEmpty())  return format;        // If no format info when it's required, abort
      if (n > 5)            if (file.IsEmpty())  return file;           // Similarly if no file info
      
      entry = ActionStrings[n] + file + format;  return entry;          // This should return the relevant strings, with the empty ones doing no harm
    }

return wxEmptyString;
}

void MyFindNotebook::AddString(wxString& addition)
{
wxString command = combo->GetValue();                        // Extract the current textctrl contents
combo->SetValue(command +  wxT(" ") + addition);             //  & replace with that plus the new addition
}

void MyFindNotebook::CalculatePermissions()    // Parses the arrays of checkboxes, putting the result as octal string in textctrl
{
int octal = 0;

for (int n=0; n < 12; ++n)                                   // For each permission checkbox
  if (((wxCheckBox*)FindWindow(OwnerPermissionStrings[n]))->IsChecked())
    octal += OwnerPermissionOctal[n];                        // If the nth one is checked, add the corresponding value to total

wxString string; string.Printf(wxT("%o"), octal);            // Format the result as a string
((wxTextCtrl*)FindWindow(wxT("Octal")))->SetValue(string);   //  & insert into textctrl
}

void MyFindNotebook::OnPageChange(wxNotebookEvent& event)
{
enum pagename page = (pagename)event.GetSelection();
if (page != NotSelected)  currentpage = page;

if (page == Path)
  { wxCommandEvent myevent(MyFindGrepEvent, MyFindGrepEvent_PathCombo); wxPostEvent(this,myevent); }  // We want to SetFocus to PathCombo, but doing it here gets overridden soon after

if (page == Name)
  { wxCommandEvent myevent(MyFindGrepEvent, MyFindGrepEvent_name); wxPostEvent(this,myevent); }
}

void MyFindNotebook::OnSetFocusEvent(wxCommandEvent& event)  // We get here via wxPostEvent, by which time SetFocus will stick
{
if (event.GetId() == MyFindGrepEvent_PathCombo)
  FindWindow(wxT("PathCombo"))->SetFocus();
if (event.GetId() == MyFindGrepEvent_name)
  FindWindow(wxT("name"))->SetFocus();
}

void MyFindNotebook::OnPageChanging(wxNotebookEvent& event)
{
if (!DataToAdd) return;                              // There's no outstanding data to add to the Command, so allow the change

wxMessageDialog dialog(this, _("You haven't added this page's choices to the 'find' command. If the page changes, they'll be ignored.\nIgnore the choices?"),
                                                                                    _("Are you sure?"), wxYES_NO | wxICON_QUESTION);
if (dialog.ShowModal() != wxID_YES) event.Veto();
}

void MyFindNotebook::OnUpdateUI(wxUpdateUIEvent& event)
{
if (!ActionArray.GetCount()) return;                // Prevent premature entry

int id = event.GetId();

if (id == XRCID("AddToCommand"))
  { AddCommandUpdateUI(); event.Enable(DataToAdd); return; }

switch(currentpage)
  { case Options:   if (id == XRCID("Maxdepth"))  
                      FindWindow(wxT("maxdepthspin"))->Enable(((wxCheckBox*)(event.GetEventObject()))->IsChecked()); 
                      else if (id == XRCID("Mindepth"))  
                      FindWindow(wxT("mindepthspin"))->Enable(((wxCheckBox*)(event.GetEventObject()))->IsChecked());
                    return;
    case Path:      if (id == XRCID("QuickFindDefault") 
                      && ((wxCheckBox*)event.GetEventObject())->IsChecked()) WHICHFIND = PREFER_QUICKFIND;
                    return;
    case Name:      return;
    case Time:      if (id == XRCID("TimePlain"))
                      { bool enabled = ((wxCheckBox*)(event.GetEventObject()))->IsChecked();
                        FindWindow(wxT("TimeAccessAcc"))->Enable(enabled);
                        FindWindow(wxT("TimeAccessMod"))->Enable(enabled);
                        FindWindow(wxT("TimeAccessCh"))->Enable(enabled);
                        FindWindow(wxT("TimeMore"))->Enable(enabled);
                        FindWindow(wxT("TimeEqual"))->Enable(enabled);
                        FindWindow(wxT("TimeLess"))->Enable(enabled);
                        FindWindow(wxT("TimeSpin"))->Enable(enabled);
                        FindWindow(wxT("TimeMins"))->Enable(enabled);
                        FindWindow(wxT("TimeDays"))->Enable(enabled);
                      }
                    if (id == XRCID("Newer"))
                      { bool enabled = ((wxCheckBox*)(event.GetEventObject()))->IsChecked();
                        FindWindow(wxT("NewerAcc"))->Enable(enabled);
                        FindWindow(wxT("NewerMod"))->Enable(enabled);
                        FindWindow(wxT("NewerCh"))->Enable(enabled);
                        FindWindow(wxT("NewerStatic"))->Enable(enabled);
                        FindWindow(wxT("NewerFile"))->Enable(enabled);
                      }
                    if (id == XRCID("LastAccess")) 
                      { bool enabled = ((wxCheckBox*)(event.GetEventObject()))->IsChecked();
                        FindWindow(wxT("LastMore"))->Enable(enabled);
                        FindWindow(wxT("LastEqual"))->Enable(enabled);
                        FindWindow(wxT("LastLess"))->Enable(enabled);
                        FindWindow(wxT("LastSpin"))->Enable(enabled);
                        FindWindow(wxT("LastStatic"))->Enable(enabled);
                      }

                    return;
                    
    case Size:      if (id == XRCID("Size"))
                      { bool enabled = ((wxCheckBox*)(event.GetEventObject()))->IsChecked();
                        FindWindow(wxT("SizeMore"))->Enable(enabled);
                        FindWindow(wxT("SizeEqual"))->Enable(enabled);
                        FindWindow(wxT("SizeLess"))->Enable(enabled);
                        FindWindow(wxT("SizeSpin"))->Enable(enabled);
                        FindWindow(wxT("DimensionBytes"))->Enable(enabled);
                        FindWindow(wxT("DimensionBlocks"))->Enable(enabled);
                        FindWindow(wxT("DimensionKB"))->Enable(enabled);
                      }
                    if (id == XRCID("Filetype"))
                        ((wxRadioBox*)FindWindow(wxT("FiletypeType")))->Enable(((wxCheckBox*)(event.GetEventObject()))->IsChecked());
                    return;

    case Owner:     if (id == XRCID("Owner"))
                      { bool enabled = ((wxCheckBox*)(event.GetEventObject()))->IsChecked();
                        FindWindow(wxT("WhoUser"))->Enable(enabled);
                        FindWindow(wxT("WhoGroup"))->Enable(enabled);
                        FindWindow(wxT("SpecifyName"))->Enable(enabled);
                        FindWindow(wxT("OwnerName"))->Enable(enabled &&      // We only want enabled if we're choosing by name
                                                                      ((wxRadioButton*)FindWindow(wxT("SpecifyName")))->GetValue());
                        FindWindow(wxT("SpecifyID"))->Enable(enabled);
                        bool ID = ((wxRadioButton*)FindWindow(wxT("SpecifyID")))->GetValue();  // We only want these enabled if we're choosing by ID
                        FindWindow(wxT("IDMore"))->Enable(enabled && ID);
                        FindWindow(wxT("IDEqual"))->Enable(enabled && ID);
                        FindWindow(wxT("IDLess"))->Enable(enabled && ID);
                        FindWindow(wxT("IDSpin"))->Enable(enabled && ID);
                      }
                    if (id == XRCID("Permissions"))
                      { bool enabled = ((wxCheckBox*)(event.GetEventObject()))->IsChecked();
                        for (int n=0; n < 12; ++n)                                 // There are oodles of checkboxes for Permissions.  Do in a loop
                          FindWindow(OwnerPermissionStrings[n])->Enable(enabled);
                        for (int n=0; n < 8; ++n)                                  // Similarly statics
                          FindWindow(OwnerStaticStrings[n])->Enable(enabled);

                        FindWindow(wxT("Octal"))->Enable(enabled);
                        FindWindow(wxT("PermAny"))->Enable(enabled);
                        FindWindow(wxT("PermExact"))->Enable(enabled);
                        FindWindow(wxT("PermEach"))->Enable(enabled);
                      }
                    return;
    case Actions:   if (id == XRCID("ExecCmd"))
                      { bool enabled = ((wxCheckBox*)FindWindow(wxT("Exec")))->IsChecked()
                                                                                || ((wxCheckBox*)FindWindow(wxT("ok")))->IsChecked();
                        FindWindow(wxT("ExecCmd"))->Enable(enabled);
                        FindWindow(wxT("ExecStatic"))->Enable(enabled);
                      }
                      
                    if (id == XRCID("PrintfFormat"))
                      { FindWindow(wxT("PrintfFormat"))->Enable(((wxCheckBox*)FindWindow(wxT("Printf")))->IsChecked());
                        FindWindow(wxT("FormatStatic"))->Enable(((wxCheckBox*)FindWindow(wxT("Printf")))->IsChecked());
                      }
                      
                    if (id == XRCID("FilePlain"))           // This one's shared by 3 checkboxes
                      { bool enabled = ((wxCheckBox*)FindWindow(wxT("Fls")))->IsChecked()
                                        || ((wxCheckBox*)FindWindow(wxT("Fprint")))->IsChecked()
                                          || ((wxCheckBox*)FindWindow(wxT("Fprint0")))->IsChecked();
                        FindWindow(wxT("FilePlain"))->Enable(enabled);
                        FindWindow(wxT("FilePlainStatic"))->Enable(enabled);
                      }
                      
                    if (id == XRCID("Fprintf"))            // This checkbox controls 2 textboxes
                      { bool enabled = ((wxCheckBox*)(event.GetEventObject()))->IsChecked();
                        FindWindow(wxT("FprintfFormat"))->Enable(enabled);
                        FindWindow(wxT("FprintfFile"))->Enable(enabled);
                        FindWindow(wxT("FprintfFormatStatic"))->Enable(enabled);
                        FindWindow(wxT("FprintfFileStatic"))->Enable(enabled);
                      }

                    return;
    default:        return;
  }
}

void MyFindNotebook::AddCommandUpdateUI()
{
DataToAdd = false;                                    // It's easier to disprove the null hypothesis

switch(currentpage)
  { case Path:      DataToAdd = ! ((wxComboBox*)FindWindow(wxT("PathCombo")))->GetValue().IsEmpty();  return;  // The only data is in the combobox

    case Options:   for (int n=0; n < 6; ++n)                                        // For the first 6 checkboxes (the ones without spinctrls)
                      if (((wxCheckBox*)FindWindow(OptionsArray[n]))->IsChecked())
                        { DataToAdd = true; return; }
                    DataToAdd = (((wxCheckBox*)FindWindow(wxT("Maxdepth")))->IsChecked() || ((wxCheckBox*)FindWindow(wxT("Mindepth")))->IsChecked());
                    return;
                    
    case Operators: for (int n=0; n < (int)OperatorArray.GetCount(); ++n)            // For every checkbox
                      if (((wxCheckBox*)FindWindow(OperatorArray[n]))->IsChecked())   // If any are checked, there's data
                        { DataToAdd = true; return; }
                    return;
                    
    case Name:      DataToAdd = ! ((wxComboBox*)FindWindow(wxT("name")))->GetValue().IsEmpty();  return;
    case Time:      if (((wxCheckBox*)FindWindow(wxT("TimePlain")))->IsChecked() && ((wxSpinCtrl*)FindWindow(wxT("TimeSpin")))->GetValue() != 0)
                      { DataToAdd = true; return; }
                    if (((wxCheckBox*)FindWindow(wxT("Newer")))->IsChecked() && ! ((wxComboBox*)FindWindow(wxT("NewerFile")))->GetValue().IsEmpty())
                      { DataToAdd = true; return; }
                    if (((wxCheckBox*)FindWindow(wxT("LastAccess")))->IsChecked() && ((wxSpinCtrl*)FindWindow(wxT("LastSpin")))->GetValue() != 0)
                      { DataToAdd = true; return; }
                    return;
    case Size:      for (int n=0; n < 1; ++n)                                       // For the first checkboxes in the array (the one without extras)
                      if (((wxCheckBox*)FindWindow(SizeArray[n]))->IsChecked())
                        { DataToAdd = true; return; }
                    if ((((wxCheckBox*)FindWindow(wxT("Size")))->IsChecked() && ((wxSpinCtrl*)FindWindow(wxT("SizeSpin")))->GetValue() != 0)
                          || (((wxCheckBox*)FindWindow(wxT("Filetype")))->IsChecked()))
                        { DataToAdd = true; return; }
                    return;
    case Owner:     for (int n=0; n < 2; ++n)                                       // For the first 2 checkboxes in the array (the ones without extras)
                      if (((wxCheckBox*)FindWindow(OwnerArray[n]))->IsChecked())
                        { DataToAdd = true; return; }
                    if (((wxCheckBox*)FindWindow(wxT("Owner")))->IsChecked()
                          && (((wxRadioButton*)FindWindow(wxT("SpecifyID")))->GetValue()   // If SpecifyId, zero is a valid answer
                              || ! ((wxTextCtrl*)FindWindow(wxT("OwnerName")))->GetValue().IsEmpty()))  // otherwise a name must be provided
                        { DataToAdd = true; return; }

                    if (((wxCheckBox*)FindWindow(wxT("Permissions")))->IsChecked() && ! ((wxTextCtrl*)FindWindow(wxT("Octal")))->GetValue().IsEmpty())
                         DataToAdd = true; 
                    return;
    case Actions:   for (int n=0; n < 3; ++n)                                       // For the first 3 checkboxes (the ones without extras)
                      if (((wxCheckBox*)FindWindow(ActionArray[n]))->IsChecked())
                        { DataToAdd = true; return; }
                    for (int n=3; n < 5; ++n)                                          // For the next 2, read the associated combobox
                      if (((wxCheckBox*)FindWindow(ActionArray[n]))->IsChecked() && ! ((wxComboBox*)FindWindow(wxT("ExecCmd")))->GetValue().IsEmpty())
                        { DataToAdd = true; return; }  
                    if ((FindWindow(wxT("PrintfFormat"))->IsEnabled() && ! ((wxTextCtrl*)FindWindow(wxT("PrintfFormat")))->GetValue().IsEmpty()) 
                      || (FindWindow(wxT("FprintfFormat"))->IsEnabled() && ! ((wxTextCtrl*)FindWindow(wxT("FprintfFormat")))->GetValue().IsEmpty())
                      || (FindWindow(wxT("FilePlain"))->IsEnabled() && ! ((wxTextCtrl*)FindWindow(wxT("FilePlain")))->GetValue().IsEmpty())
                      || (FindWindow(wxT("FprintfFile"))->IsEnabled() && ! ((wxTextCtrl*)FindWindow(wxT("FprintfFile")))->GetValue().IsEmpty()))
                                 DataToAdd = true; 
                    return;
    case NotSelected:  return;
  }
}

BEGIN_EVENT_TABLE(MyFindNotebook, wxNotebook)
  EVT_CHECKBOX(-1, MyFindNotebook::OnCheckBox)
  EVT_NOTEBOOK_PAGE_CHANGED(-1, MyFindNotebook::OnPageChange)
  EVT_BUTTON(XRCID("AddToCommand"), MyFindNotebook::OnAddCommand)
  EVT_COMMAND(wxID_ANY, MyFindGrepEvent, MyFindNotebook::OnSetFocusEvent)
  EVT_UPDATE_UI(-1, MyFindNotebook::OnUpdateUI)
END_EVENT_TABLE()

//-----------------------------------------------------------------------------------------------------------------------
const wxChar* MyGrepNotebook::MainStrings[] = { wxT("w"), wxT("i"), wxT("x"), wxT("v") };
const wxChar* MyGrepNotebook::OutputStrings[] = { wxT("c"), wxT("l"), wxT("H"), wxT("n"), wxT("s"), wxT("o"), wxT("L"), wxT("h"), wxT("b"), wxT("q") };
const wxChar* MyGrepNotebook::PatternStrings[] = { wxT("F"), wxT("P"), wxT("e") };

void MyGrepNotebook::Init(Tools* dad, wxComboBox* searchcombo)
{
parent = dad;
combo = searchcombo;

GetXRCIDsIntoArrays();

int n = (int)Main;
do ClearPage((enum pagename)n++);
 while ((n+1) <= (int)Path);

LoadCombosHistory();
currentpage = Main;
DataToAdd = false;
Connect(wxID_ANY, wxEVT_COMMAND_NOTEBOOK_PAGE_CHANGING, (wxObjectEventFunction)&MyGrepNotebook::OnPageChanging);
}

void MyGrepNotebook::GetXRCIDsIntoArrays()    // Life becomes easier if we can access the XRCID values by looping thru an array
{
MainArray.Add(XRCID("Whole"));
MainArray.Add(XRCID("IgnoreCase"));
MainArray.Add(XRCID("WholeLines"));
MainArray.Add(XRCID("InvertMatch"));

FileArray.Add(XRCID("MaxMatches"));
FileArray.Add(XRCID("Binaries"));
FileArray.Add(XRCID("Devices"));
FileArray.Add(XRCID("ExcludeFiles"));
FileArray.Add(XRCID("IncludeFiles"));

OutputArray.Add(XRCID("OnlyCount"));
OutputArray.Add(XRCID("PrintFilename"));
OutputArray.Add(XRCID("EachFilename"));
OutputArray.Add(XRCID("PrefixLineno"));
OutputArray.Add(XRCID("Silent"));
OutputArray.Add(XRCID("PrintPatternMatch"));
OutputArray.Add(XRCID("FilenameNoMatch"));
OutputArray.Add(XRCID("NoFilenameMultiple"));
OutputArray.Add(XRCID("PrefixOffset"));
OutputArray.Add(XRCID("Quiet"));
OutputArray.Add(XRCID("ContextLeading"));
OutputArray.Add(XRCID("ContextTrailing"));
OutputArray.Add(XRCID("ContextBoth"));
OutputArray.Add(XRCID("LabelDisplay"));


PatternArray.Add(XRCID("Fgrep"));
PatternArray.Add(XRCID("Perl"));
PatternArray.Add(XRCID("ProtectPattern"));
PatternArray.Add(XRCID("FromFile"));

PathArray.Add(XRCID("PathCurrentDir"));
PathArray.Add(XRCID("PathHome"));
PathArray.Add(XRCID("PathRoot"));
}

void MyGrepNotebook::LoadCombosHistory()  // Load the relevant history-arrays into comboboxes
{
wxComboBox* cbox;

for (size_t c=4; c < 8; ++c)                          // For every combobox in the array-list 4-7 as Find came first
  { cbox = (wxComboBox*)FindWindow(parent->FindSubpathArray[c]);
    wxArrayString* History = parent->historylist[c];  // The history-array that goes with this combobox
    
    #if ! defined(__WXGTK__)
      cbox->Append(wxT(""));                          // This used to be needed to give a blank entry to display in the textbox
    #endif
    for (size_t n=0; n < History->GetCount(); ++n)    // Fill combobox from the corresponding history array
        cbox->Append((*History)[n]);
  }
}

void MyGrepNotebook::SaveCombosHistory(int which)  // Save the relevant command history into their arrays
{
wxComboBox* cbox = (wxComboBox*)FindWindow(parent->FindSubpathArray[which]);
wxArrayString* History = parent->historylist[which];  // The history-array that goes with this combobox

wxString string = cbox->GetValue();                   // Get the visible string
if (string.IsEmpty()) return;

int index = History->Index(string, false);            // See if this entry already exists in the history array
if (index != wxNOT_FOUND)
  History->RemoveAt(index);                           // If so, remove it.  We'll add it back into position zero  
History->Insert(string, 0);                           // Either way, insert into position zero

                  // Lastly, reload cbox.  This adds the new item in the correct position
cbox->Clear();
#if ! defined(__WXGTK__)
  cbox->Append(wxT(""));                              // This used to be needed to give a blank entry to display in the textbox
#endif
for (size_t n=0; n < History->GetCount(); ++n)      cbox->Append((*History)[n]);                
}

void MyGrepNotebook::ClearPage(enum pagename page, int exceptthisitem /*=-1*/)
{
switch(page)
  { case Main:    for (int n=0; n < (int)MainArray.GetCount(); ++n)                 // For every checkbox
                      if (MainArray[n] != exceptthisitem)                           //  (if it's not one we want to ignore)
                        ((wxCheckBox*)FindWindow(MainArray[n]))->SetValue(false);   //   disable.
                
    case Dir:     ((wxCheckBox*)FindWindow( XRCID("Directories")))->SetValue(false);
    case File:    for (int n=0; n < (int)FileArray.GetCount(); ++n)
                        ((wxCheckBox*)FindWindow(FileArray[n]))->SetValue(false);

    case Output:  for (int n=0; n < (int)OutputArray.GetCount(); ++n)
                  if (OutputArray[n] != exceptthisitem)
                    ((wxCheckBox*)FindWindow(OutputArray[n]))->SetValue(false);
                  return;
    case Pattern: for (int n=0; n < (int)PatternArray.GetCount(); ++n)
                  if (PatternArray[n] != exceptthisitem)
                    ((wxCheckBox*)FindWindow(PatternArray[n]))->SetValue(false);
                  ((wxComboBox*)FindWindow(wxT("SearchPattern")))->SetValue(wxT(""));
                  return;
    case Path:    for (int n=0; n < (int)PathArray.GetCount(); ++n)  
                      if (PathArray[n] != exceptthisitem)
                        ((wxCheckBox*)FindWindow(PathArray[n]))->SetValue(false);
                ((wxComboBox*)FindWindow(wxT("PathGrep")))->SetValue(wxT(""));  return;
    default:      return;    
  }      
}



void MyGrepNotebook::OnCheckBox(wxCommandEvent& event)
{
switch(currentpage)
  { case Path:        OnCheckBoxPath(event); return;
    case Main:                                          // Nothing needed for all the rest, as UpdateUI will cope
    case Dir:
    case File:
    case Output:
    case Pattern:
    default:          return;
  }
}

void MyGrepNotebook::OnCheckBoxPath(wxCommandEvent& event)
{
if (!event.IsChecked()) return;  // We're not interested in UNchecking events

wxString entry;
int id = event.GetId();
              // Depending on which checkbox was checked, store an appropriate string in entry
if ((id == XRCID("PathCurrentDir")) && MyFrame::mainframe->GetActivePane())
      entry = MyFrame::mainframe->GetActivePane()->GetActiveDirPath();
  else if (id == XRCID("PathHome"))    entry = wxGetApp().GetHOME();
    else if (id == XRCID("PathRoot"))    entry = wxT("/");
      else return;
      
ClearPage(currentpage, event.GetId());                        // Reset the other checkboxes
((wxComboBox*)FindWindow(wxT("PathGrep")))->SetValue(entry);  // Place the result in the combobox
}

void MyGrepNotebook::OnAddCommand(wxCommandEvent& event)
{
wxString answer;

switch(currentpage)
  { case Path:      answer = ((wxComboBox*)FindWindow(wxT("PathGrep")))->GetValue();  // Get the data from the combobox
                    if (answer.IsEmpty())  return;
                    QuoteExcludingWildcards(answer);            // Try to cope with spaces etc in the filepath, without switching off globbing
                    SaveCombosHistory(7); break;
    case Dir:       if (((wxCheckBox*)FindWindow(wxT("Directories")))->IsChecked())
                      { if  (((wxRadioButton*)FindWindow(wxT("DirMatchName")))->GetValue())  answer = wxT("-d read");
                          else if  (((wxRadioButton*)FindWindow(wxT("DirRecurse")))->GetValue())  answer = wxT("-r");
                            else answer = wxT("-d skip");
                      }
                    break;
    case Main:      answer = OnAddCommandMain(); break;         // Too complicated to fit here in comfort
    case File:      answer = OnAddCommandFile(); break;
    case Output:    answer = OnAddCommandOutput(); break;
    case Pattern:   answer = OnAddCommandPattern(); break;
      default:       return;
  }

AddString(answer);                                              // Add the new data to the dlg command string
ClearPage(currentpage);                                         //  & reset the current page
DataToAdd = false;                                              //   & the bool
}

wxString MyGrepNotebook::OnAddCommandMain()  // Depending on which checkboxes were checked, return an appropriate string
{
wxString entry;
                  // Check each checkbox.  If one is checked, add the appropriate string to the end of entry.
for (int n=0; n < 4; ++n)                                       // For each checkbox
  if (((wxCheckBox*)FindWindow(MainArray[n]))->IsChecked())
      entry += MainStrings[n];                                  // If it's checked, add the corresponding command to the string

if (!entry.IsEmpty())  entry = wxT("-") + entry;                // Because grep can't cope with -n-w, but wants either -nw or -n -w, prefix just 1 - here
return entry;
}

wxString MyGrepNotebook::OnAddCommandFile()  // Depending on which checkboxes were checked, return an appropriate string
{
wxString entry;
  
if (((wxCheckBox*)FindWindow(wxT("MaxMatches")))->IsChecked())
  { wxString thisentry; thisentry.Printf(wxT(" -m %u"),  ((wxSpinCtrl*)FindWindow(wxT("MatchNo")))->GetValue());
    entry += thisentry;
  }
  
if (((wxCheckBox*)FindWindow(wxT("Binaries")))->IsChecked())
  { if  (((wxRadioButton*)FindWindow(wxT("BinaryName")))->GetValue())  entry += wxT(" --binary-files=binary");
      else if  (((wxRadioButton*)FindWindow(wxT("BinaryRead")))->GetValue())  entry += wxT(" --binary-files=text");
        else entry += wxT("  -I");
  }

if (((wxCheckBox*)FindWindow(wxT("Devices")))->IsChecked())
    entry += wxT(" -D skip");

if (((wxCheckBox*)FindWindow(wxT("ExcludeFiles")))->IsChecked())
  { wxString name = ((wxComboBox*)FindWindow(wxT("ExcludePattern")))->GetValue();
    if (!name.IsEmpty())
      { entry += wxT(" --exclude="); entry += name;
      
        SaveCombosHistory(4);                                     // Save the new command history into array
      }
  }

if (((wxCheckBox*)FindWindow(wxT("IncludeFiles")))->IsChecked())
  { wxString name = ((wxComboBox*)FindWindow(wxT("IncludePattern")))->GetValue();
    if (!name.IsEmpty())
      { entry += wxT(" --include="); entry += name;
      
        SaveCombosHistory(5);                                     // Save the new command history into array
      }
  }
  
entry.Trim(false);                                                // Remove initial space as it will be added later
return entry;  
}

wxString MyGrepNotebook::OnAddCommandOutput()  // Depending on which checkboxes were checked, return an appropriate string
{
wxString entry;

for (int n=0; n < 10; ++n)                                      // 1st 10 checkboxes are easy
  if (((wxCheckBox*)FindWindow(OutputArray[n]))->IsChecked())
      entry += OutputStrings[n];                                // If it's checked, add the corresponding command to the string
      
if (!entry.IsEmpty())  entry = wxT("-") + entry;                // Because grep can't cope with -n-w, but wants either -nw or -n -w, prefix just 1 - here
                  
                  // It's easier to do the rest by hand
if (((wxCheckBox*)FindWindow(wxT("ContextLeading")))->IsChecked())  
  { wxString thisentry; thisentry.Printf(wxT(" -B %u"),  ((wxSpinCtrl*)FindWindow(wxT("LeadingNo")))->GetValue());
    entry += thisentry;
  }

if (((wxCheckBox*)FindWindow(wxT("ContextTrailing")))->IsChecked())  
  { wxString thisentry; thisentry.Printf(wxT(" -A %u"),  ((wxSpinCtrl*)FindWindow(wxT("TrailingNo")))->GetValue());
    entry += thisentry;
  }

if (((wxCheckBox*)FindWindow(wxT("ContextBoth")))->IsChecked())  
  { wxString thisentry; thisentry.Printf(wxT(" -C %u"),  ((wxSpinCtrl*)FindWindow(XRCID("BothNo")))->GetValue());
    entry += thisentry;
  }
  
if (((wxCheckBox*)FindWindow(wxT("LabelDisplay")))->IsChecked())  
  { wxString name = ((wxTextCtrl*)FindWindow(wxT("FileLabel")))->GetValue();
    if (!name.IsEmpty())
      { entry += wxT(" --label="); entry += name; }
  }

entry.Trim(false);
return entry;  
}

wxString MyGrepNotebook::OnAddCommandPattern()  // Depending on which checkboxes were checked, return an appropriate string
{
wxString entry;

for (int n=0; n < 3; ++n)
  if (((wxCheckBox*)FindWindow(PatternArray[n]))->IsChecked())
      entry += PatternStrings[n];
      
if (!entry.IsEmpty())  entry = wxT("-") + entry;                // Because grep can't cope with -n-w, but wants either -nw or -n -w, prefix just 1 - here  

              // Either there's a file to take the pattern from, or a pattern was entered direct (or neither?)
if (((wxCheckBox*)FindWindow(PatternArray[3]))->IsChecked())    // If so, there's a file
  { wxString name = ((wxTextCtrl*)FindWindow(wxT("FileFrom")))->GetValue();
    if (!name.IsEmpty())
      { entry += wxT(" -f "); entry += name;                    // Add the filename and return
        entry.Trim(false);
        return entry;  
      }
  }
              // If we're still here, either that checkbox wasn't, or the filename was empty.  Either way, see if there's a direct-entry  
wxString name = ((wxComboBox*)FindWindow(wxT("SearchPattern")))->GetValue();
if (!name.IsEmpty())
  { entry += wxT("\\\""); entry += name; entry += wxT("\\\"");  // Grep runs in a shell, so we have to escape the quotes
    SaveCombosHistory(6);
  }
  
entry.Trim(false);
return entry;
}

void MyGrepNotebook::AddString(wxString& addition)
{
wxString command = combo->GetValue();                            // Extract the current textctrl contents
combo->SetValue(command + wxT(" ") + addition);                  //  & replace with that plus the new addition
}

void MyGrepNotebook::OnPageChanging(wxNotebookEvent& event)
{
if (!DataToAdd) return;                                          // There's no outstanding data to add to the Command, so allow the change

wxMessageDialog dialog(this, _("You haven't added this page's choices to the 'grep' command. If the page changes, they'll be ignored.\nIgnore the choices?"),
                                                                                    _("Are you sure?"), wxYES_NO | wxICON_QUESTION);
if (dialog.ShowModal() != wxID_YES) event.Veto();
}
            
void MyGrepNotebook::OnPageChange(wxNotebookEvent& event)
{
enum pagename page = (pagename)event.GetSelection();
if (page != NotSelected)  currentpage = page;

if (page == Pattern)
  { wxCommandEvent myevent(MyFindGrepEvent); wxPostEvent(this,myevent); }  // We want to SetFocus to SearchPattern, but doing it here gets overridden soon after
}

void MyGrepNotebook::OnPatternPage(wxCommandEvent& WXUNUSED(event))  // We get here via wxPostEvent, by which time SetFocus will stick
{
FindWindow(wxT("SearchPattern"))->SetFocus();
}

void MyGrepNotebook::OnUpdateUI(wxUpdateUIEvent& event)
{
if (!PathArray.GetCount()) return;                              // Prevent premature entry

int id = event.GetId();

if (id == XRCID("AddToCommand"))
  { AddCommandUpdateUI(); event.Enable(DataToAdd); return; }

switch(currentpage)
  { case Path:    return;
    case Main:    if (id == XRCID("QuickGrepDefault")) 
                      if (((wxCheckBox*)event.GetEventObject())->IsChecked()) WHICHGREP = PREFER_QUICKGREP;
                  return;
    case Dir:     if (id == XRCID("Directories"))
                    { bool ifchecked = ((wxCheckBox*)(event.GetEventObject()))->IsChecked();
                      FindWindow(wxT("DirMatchName"))->Enable(ifchecked);
                      FindWindow(wxT("DirRecurse"))->Enable(ifchecked);
                      FindWindow(wxT("DirIgnore"))->Enable(ifchecked);
                    }
                  return;
    case File:    if (id == XRCID("MaxMatches"))
                    ((wxSpinCtrl*)FindWindow(wxT("MatchNo")))->Enable(((wxCheckBox*)(event.GetEventObject()))->IsChecked());
                  if (id == XRCID("Binaries"))
                    { bool ifchecked = ((wxCheckBox*)(event.GetEventObject()))->IsChecked();
                      FindWindow(wxT("BinaryName"))->Enable(ifchecked);
                      FindWindow(wxT("BinaryRead"))->Enable(ifchecked);
                      FindWindow(wxT("BinaryIgnore"))->Enable(ifchecked);
                    }
                  if (id == XRCID("ExcludeFiles"))
                    ((wxComboBox*)FindWindow(wxT("ExcludePattern")))->Enable(((wxCheckBox*)(event.GetEventObject()))->IsChecked());
                  if (id == XRCID("IncludeFiles"))
                    ((wxComboBox*)FindWindow(wxT("IncludePattern")))->Enable(((wxCheckBox*)(event.GetEventObject()))->IsChecked());
                  return;                  
    case Output:  if (id == XRCID("ContextLeading"))
                    ((wxSpinCtrl*)FindWindow(wxT("LeadingNo")))->Enable(((wxCheckBox*)(event.GetEventObject()))->IsChecked());  
                  if (id == XRCID("ContextTrailing"))
                    ((wxSpinCtrl*)FindWindow(wxT("TrailingNo")))->Enable(((wxCheckBox*)(event.GetEventObject()))->IsChecked());
                  if (id == XRCID("ContextBoth"))
                    ((wxSpinCtrl*)FindWindow(wxT("BothNo")))->Enable(((wxCheckBox*)(event.GetEventObject()))->IsChecked());

                  if (id == XRCID("LabelDisplay"))
                    ((wxTextCtrl*)FindWindow(wxT("FileLabel")))->Enable(((wxCheckBox*)(event.GetEventObject()))->IsChecked());
                  return;
    case Pattern: if (id == XRCID("FromFile"))  
                    ((wxTextCtrl*)FindWindow(wxT("FileFrom")))->Enable(((wxCheckBox*)(event.GetEventObject()))->IsChecked());
                  return;
    default:      return;
  }
}

void MyGrepNotebook::AddCommandUpdateUI()
{
DataToAdd = false;                                    // It's easier to disprove the null hypothesis

switch(currentpage)
  { case Path:      DataToAdd = ! ((wxComboBox*)FindWindow(wxT("PathGrep")))->GetValue().IsEmpty();  return;  // The only data is in the combobox

    case Main:      for (int n=0; n < 4; ++n)
                      if (((wxCheckBox*)FindWindow(MainArray[n]))->IsChecked())
                          { DataToAdd = true; return; }

    case Dir:       DataToAdd = ((wxCheckBox*)FindWindow(wxT("Directories")))->IsChecked(); return;

    case File:      if (((wxCheckBox*)FindWindow(wxT("MaxMatches")))->IsChecked()
                          || ((wxCheckBox*)FindWindow(wxT("Binaries")))->IsChecked()
                            || ((wxCheckBox*)FindWindow(wxT("Devices")))->IsChecked())
                                { DataToAdd = true; return; }

                    if ((((wxCheckBox*)FindWindow(wxT("ExcludeFiles")))->IsChecked() && ! ((wxComboBox*)FindWindow(wxT("ExcludePattern")))->GetValue().IsEmpty())
                        || (((wxCheckBox*)FindWindow(wxT("IncludeFiles")))->IsChecked() && ! ((wxComboBox*)FindWindow(wxT("IncludePattern")))->GetValue().IsEmpty()))
                                DataToAdd = true;
                    return;

    case Output:    for (int n=0; n < 10; ++n)
                      if (((wxCheckBox*)FindWindow(OutputArray[n]))->IsChecked())
                        { DataToAdd = true; return; }
                    if (((wxCheckBox*)FindWindow(wxT("ContextLeading")))->IsChecked()
                        || ((wxCheckBox*)FindWindow(wxT("ContextTrailing")))->IsChecked()
                          || ((wxCheckBox*)FindWindow(wxT("ContextBoth")))->IsChecked())
                              { DataToAdd = true; return; }
                    if (((wxCheckBox*)FindWindow(wxT("LabelDisplay")))->IsChecked() && ! ((wxTextCtrl*)FindWindow(wxT("FileLabel")))->GetValue().IsEmpty())
                                DataToAdd = true;
                    return;

    case Pattern:   for (int n=0; n < 3; ++n)
                      if (((wxCheckBox*)FindWindow(PatternArray[n]))->IsChecked())
                        { DataToAdd = true; return; }
                    if (!((wxComboBox*)FindWindow(wxT("SearchPattern")))->GetValue().IsEmpty()
                      || (((wxCheckBox*)FindWindow(PatternArray[3]))->IsChecked() && ! ((wxTextCtrl*)FindWindow(wxT("FileFrom")))->GetValue().IsEmpty()))
                                DataToAdd = true;
                    return;

      default:      return;
  }
}

void MyGrepNotebook::OnRegexCrib(wxCommandEvent& event)
{
if (HELPDIR.IsEmpty()) return;
HtmlDialog(_("RegEx Help"), HELPDIR + wxT("/RegExpHelp.htm"), wxT("RegExpHelp"), GetPage(GetSelection()));
}

BEGIN_EVENT_TABLE(MyGrepNotebook, wxNotebook)
  EVT_COMMAND(wxID_ANY, MyFindGrepEvent, MyGrepNotebook::OnPatternPage)
  EVT_CHECKBOX(-1, MyGrepNotebook::OnCheckBox)
  EVT_NOTEBOOK_PAGE_CHANGED(-1, MyGrepNotebook::OnPageChange)
  EVT_BUTTON(XRCID("AddToCommand"), MyGrepNotebook::OnAddCommand)
  EVT_BUTTON(XRCID("RegexCrib"), MyGrepNotebook::OnRegexCrib)
  EVT_UPDATE_UI(-1, MyGrepNotebook::OnUpdateUI)
END_EVENT_TABLE()


//-----------------------------------------------------------------------------------------------------------------------
IMPLEMENT_DYNAMIC_CLASS(QuickFindDlg, wxDialog)

void QuickFindDlg::Init(Tools* dad)
{
parent = dad;

SearchPattern = ((wxComboBox*)FindWindow(wxT("name")));
PathFind = ((wxComboBox*)FindWindow(wxT("PathCombo")));

PathCurrentDir = ((wxCheckBox*)FindWindow(wxT("PathCurrentDir")));
PathHome = ((wxCheckBox*)FindWindow(wxT("PathHome")));
PathRoot = ((wxCheckBox*)FindWindow(wxT("PathRoot")));

RBName = ((wxRadioButton*)FindWindow(wxT("RBName")));
RBPath = ((wxRadioButton*)FindWindow(wxT("RBPath")));
RBRegex = ((wxRadioButton*)FindWindow(wxT("RBRegex")));

IgnoreCase = ((wxCheckBox*)FindWindow(wxT("IgnoreCase")));
PrependAsterix = ((wxCheckBox*)FindWindow("PrependAstx"));
AppendAsterix = ((wxCheckBox*)FindWindow("AppendAstx"));

LoadCheckboxState();

  // If one of the path checkboxes is now ticked, sync PathFind
if (PathCurrentDir->IsChecked() && MyFrame::mainframe->GetActivePane()) PathFind->SetValue(MyFrame::mainframe->GetActivePane()->GetActiveDirPath());
if (PathHome->IsChecked()) PathFind->SetValue(wxGetApp().GetHOME());
if (PathRoot->IsChecked()) PathFind->SetValue(wxT("/"));

if (PrependAsterix)
  PrependAsterix->Bind(wxEVT_UPDATE_UI, &QuickFindDlg::OnAsterixUpdateUI, this);
if (AppendAsterix)
  AppendAsterix->Bind(wxEVT_UPDATE_UI, &QuickFindDlg::OnAsterixUpdateUI, this);


Show(); // After 2.5.3, the new method of wxWindow sizing means that a long history string in a combobox will expand the box outside the dialog!  So Show() it before adding the data
LoadCombosHistory();
}

void QuickFindDlg::LoadCheckboxState()  // Load the last-used which-boxes-were-checked state, + directories radio
{
long state;
wxConfigBase::Get()->Read(wxT("/Misc/QuickFindCheckState"), &state, 0x0);

PathCurrentDir->SetValue(state & 0x01);
PathHome->SetValue(state & 0x02);
PathRoot->SetValue(state & 0x04);
IgnoreCase->SetValue(state & 0x10);

if (PrependAsterix)
  PrependAsterix->SetValue(state & 0x20);
if (AppendAsterix)
  AppendAsterix->SetValue(state & 0x40);
  
wxConfigBase::Get()->Read("/Misc/QuickFindRadioState", &state, 0x0);
RBName->SetValue(state == 0);
RBPath->SetValue(state == 1);
RBRegex->SetValue(state == 2);
}

void QuickFindDlg::SaveCheckboxState()  // Save the current which-boxes-are-checked state, + directories radio
{
long state = 0;
if (PathCurrentDir->IsChecked()) state |= 0x01;
if (PathHome->IsChecked()) state |= 0x02;
if (PathRoot->IsChecked()) state |= 0x04;
if (IgnoreCase->IsChecked()) state |= 0x10;
if (PrependAsterix && PrependAsterix->IsChecked()) state |= 0x20;
if (AppendAsterix && AppendAsterix->IsChecked()) state |= 0x40;
wxConfigBase::Get()->Write("/Misc/QuickFindCheckState", state);

RBName->SetValue(state == 0);
if (RBPath->GetValue()) wxConfigBase::Get()->Write("/Misc/QuickFindRadioState", 1l);
 else if (RBRegex->GetValue()) wxConfigBase::Get()->Write("/Misc/QuickFindRadioState", 2l);
 else  wxConfigBase::Get()->Write("/Misc/QuickFindRadioState", 0l);
}

void QuickFindDlg::LoadCombosHistory()  // Load the relevant history-arrays into comboboxes
{
wxComboBox* cbox;

for (size_t c=0; c < 2; ++c)                          // For the 1st 2 comboboxes (used also by FullFind)
  { cbox = (wxComboBox*)FindWindow(parent->FindSubpathArray[c]);
    wxArrayString* History = parent->historylist[c];  // The history-array that goes with this combobox
    
#if ! defined(__WXGTK__)
    cbox->Append(wxT(""));                            // This used to be needed to give a blank entry to display in the textbox
#endif
    if (History)
      for (size_t n=0; n < History->GetCount(); ++n)    // Fill combobox from the corresponding history array
        cbox->Append((*History)[n]);
  }
}

void QuickFindDlg::SaveCombosHistory(int which)  // Save the relevant command history into their arrays
{
wxComboBox* cbox = (wxComboBox*)FindWindow(parent->FindSubpathArray[which]);
wxArrayString* History = parent->historylist[which];  // The history-array that goes with this combobox

wxString string = cbox->GetValue();                   // Get the visible string
if (string.IsEmpty()) return;

int index = History->Index(string, false);            // See if this entry already exists in the history array
if (index != wxNOT_FOUND)
  History->RemoveAt(index);                           // If so, remove it.  We'll add it back into position zero  
History->Insert(string, 0);                           // Either way, insert into position zero

                  // Lastly, reload cbox.  This adds the new item in the correct position
cbox->Clear();
#if ! defined(__WXGTK__)
  cbox->Append(wxT(""));                              // This used to be needed to give a blank entry to display in the textbox
#endif
for (size_t n=0; n < History->GetCount(); ++n)  cbox->Append((*History)[n]);                
}

void QuickFindDlg::OnCheck(wxCommandEvent& event) 
{
int id = event.GetId();
if (id == XRCID("PathCurrentDir"))
  { PathFind->SetValue((event.IsChecked() && MyFrame::mainframe->GetActivePane()) ? MyFrame::mainframe->GetActivePane()->GetActiveDirPath() : wxT(""));
    PathHome->SetValue(false); PathRoot->SetValue(false);  // Clear the other path boxes
  }
if (id == XRCID("PathHome"))
  { PathFind->SetValue(event.IsChecked() ? wxGetApp().GetHOME() : wxT(""));
    PathCurrentDir->SetValue(false); PathRoot->SetValue(false);
  }
if (id == XRCID("PathRoot"))
  { PathFind->SetValue(event.IsChecked() ? wxT("/") : wxT(""));
    PathHome->SetValue(false); PathCurrentDir->SetValue(false);
  }
}

void QuickFindDlg::OnButton(wxCommandEvent& event)
{
int id = event.GetId();

if (id == XRCID("wxID_OK"))
  { wxString next, cmd(wxT("find "));
    next = PathFind->GetValue();
    if ( next.IsEmpty()) { BriefMessageBox bmb(wxT("You need to provide a Path to Search in"), -1, wxT("O_o")); return; }
    cmd << next << wxT(' '); next.Empty();

    int searchtype = (RBPath->GetValue() ? 2:0) + (RBRegex->GetValue() ? 4:0) + IgnoreCase->IsChecked();
    next = MyFindNotebook::NameStrings5[searchtype]; // We can piggyback on the fullfind strings; we just don't use the last pair (symlinks)
    cmd << next;

    next = SearchPattern->GetValue();
    if ( next.IsEmpty()) { BriefMessageBox bmb(wxT("You need to provide a pattern to Search for"), -1, wxT("O_o")); return; }
    wxString preAstx = PrependAsterix && PrependAsterix->IsEnabled() && PrependAsterix->IsChecked() ? "*":""; // If one/both of the '*' boxes is checked...
    wxString appAstx = AppendAsterix && AppendAsterix->IsEnabled() && AppendAsterix->IsChecked() ? "*":"";
    cmd << wxT("\\\"") << preAstx << next << appAstx << wxT("\\\" ");  SaveCombosHistory(1); // Find runs in a shell, so we have to escape the quotes

    SaveCombosHistory(0); SaveCheckboxState();

        // Even though quickfind has no history combo, we must add the command to Tools::History, otherwise the combobox subhistories aren't saved
    int index = parent->History.Index(cmd, false);                  // See if this command already exists in the history array
    if (index != wxNOT_FOUND)
      parent->History.RemoveAt(index);                              // If so, remove it.  We'll add it back into position zero  
    parent->History.Insert(cmd, 0);                                 // Either way, insert into position zero

    
    cmd = wxT("sh -c \"") + cmd; cmd << wxT('\"');
    parent->text->HistoryAdd(cmd); parent->text->RunCommand(cmd); parent->text->SetFocus(); // Run the command in the terminalem
  }

EndModal(id);                 // Return the id, because we must anyway, and it might be XRCID("FullFind")
}

void QuickFindDlg::OnUpdateUI(wxUpdateUIEvent& event)
{
if (RBName == NULL) return; // Protection against too-early updateui events

int id = event.GetId();
if (id == XRCID("FullFindDefault")) 
  if (((wxCheckBox*)event.GetEventObject())->IsChecked()) WHICHFIND = PREFER_FULLFIND;
}

void QuickFindDlg::OnAsterixUpdateUI(wxUpdateUIEvent& event)
{
  // We dont want auto-asterixes around regexes
event.Enable(PrependAsterix && !RBRegex->GetValue()); // The Append checkbox UI will be the same as the Prepend, so this check works for both
}

BEGIN_EVENT_TABLE(QuickFindDlg, wxDialog)
  EVT_BUTTON(wxID_ANY, QuickFindDlg::OnButton)
  EVT_CHECKBOX(wxID_ANY, QuickFindDlg::OnCheck)
  EVT_KEY_DOWN(QuickFindDlg::OnKeyDown)
  EVT_UPDATE_UI(wxID_ANY, QuickFindDlg::OnUpdateUI)
END_EVENT_TABLE()

//-----------------------------------------------------------------------------------------------------------------------
IMPLEMENT_DYNAMIC_CLASS(QuickGrepDlg, wxDialog)

void QuickGrepDlg::Init(Tools* dad)
{
parent = dad;

SearchPattern = ((wxComboBox*)FindWindow(wxT("SearchPattern")));
PathGrep = ((wxComboBox*)FindWindow(wxT("PathGrep")));

PathCurrentDir = ((wxCheckBox*)FindWindow(wxT("PathCurrentDir")));
PathHome = ((wxCheckBox*)FindWindow(wxT("PathHome")));
PathRoot = ((wxCheckBox*)FindWindow(wxT("PathRoot")));

WholeWord = ((wxCheckBox*)FindWindow(wxT("WholeWord")));
IgnoreCase = ((wxCheckBox*)FindWindow(wxT("IgnoreCase")));
PrefixLineno = ((wxCheckBox*)FindWindow(wxT("PrefixLineno")));
Binaries = ((wxCheckBox*)FindWindow(wxT("Binaries")));
Devices = ((wxCheckBox*)FindWindow(wxT("Devices")));

DirectoriesRadio = ((wxRadioBox*)FindWindow(wxT("DirectoriesRadio")));
LoadCheckboxState();

  // If one of the path checkboxes is now ticked, sync PathGrep
if (PathCurrentDir->IsChecked() && MyFrame::mainframe->GetActivePane()) PathGrep->SetValue(MyFrame::mainframe->GetActivePane()->GetActiveDirPath());
if (PathHome->IsChecked()) PathGrep->SetValue(wxGetApp().GetHOME());
if (PathRoot->IsChecked()) PathGrep->SetValue(wxT("/"));

Show(); // After 2.5.3, the new method of wxWindow sizing means that a long history string in a combobox will expand the box outside the dialog!  So Show() it before adding the data
LoadCombosHistory();
}

void QuickGrepDlg::LoadCheckboxState()  // Load the last-used which-boxes-were-checked state, + directories radio
{
long state;
wxConfigBase::Get()->Read(wxT("/Misc/QuickGrepCheckState"), &state, 0x60); // 0x60 means have the ignore binaries/devices boxes checked by default 

PathCurrentDir->SetValue(state & 0x01);
PathHome->SetValue(state & 0x02);
PathRoot->SetValue(state & 0x04);
WholeWord->SetValue(state & 0x08);
IgnoreCase->SetValue(state & 0x10);
PrefixLineno->SetValue(state & 0x80);
Binaries->SetValue(state & 0x20);
Devices->SetValue(state & 0x40);

DirectoriesRadio->SetSelection((int)wxConfigBase::Get()->Read(wxT("/Misc/QuickGrepRadioState"), 1l) & 3);
}

void QuickGrepDlg::SaveCheckboxState()  // Save the current which-boxes-are-checked state, + directories radio
{
long state = 0;
if (PathCurrentDir->IsChecked()) state |= 0x01;
if (PathHome->IsChecked()) state |= 0x02;
if (PathRoot->IsChecked()) state |= 0x04;
if (WholeWord->IsChecked()) state |= 0x08;
if (IgnoreCase->IsChecked()) state |= 0x10;
if (PrefixLineno->IsChecked()) state |= 0x80;
if (Binaries->IsChecked()) state |= 0x20;
if (Devices->IsChecked()) state |= 0x40;
wxConfigBase::Get()->Write(wxT("/Misc/QuickGrepCheckState"), state);

wxConfigBase::Get()->Write(wxT("/Misc/QuickGrepRadioState"), (long)DirectoriesRadio->GetSelection());
}

void QuickGrepDlg::LoadCombosHistory()  // Load the relevant history-arrays into comboboxes
{
wxComboBox* cbox;

for (size_t c=6; c < 8; ++c)                          // For every combobox in the array-list 6-7 as Find came first, then irrelevant full-grep ones
  { cbox = (wxComboBox*)FindWindow(parent->FindSubpathArray[c]);
    wxArrayString* History = parent->historylist[c];  // The history-array that goes with this combobox
    
#if ! defined(__WXGTK__)
    cbox->Append(wxT(""));                            // This used to be needed to give a blank entry to display in the textbox
#endif
    for (size_t n=0; n < History->GetCount(); ++n)    // Fill combobox from the corresponding history array
        cbox->Append((*History)[n]);
  }
}

void QuickGrepDlg::SaveCombosHistory(int which)  // Save the relevant command history into their arrays
{
wxComboBox* cbox = (wxComboBox*)FindWindow(parent->FindSubpathArray[which]);
wxArrayString* History = parent->historylist[which];  // The history-array that goes with this combobox

wxString string = cbox->GetValue();                   // Get the visible string
if (string.IsEmpty()) return;

int index = History->Index(string, false);            // See if this entry already exists in the history array
if (index != wxNOT_FOUND)
  History->RemoveAt(index);                           // If so, remove it.  We'll add it back into position zero  
History->Insert(string, 0);                           // Either way, insert into position zero

                  // Lastly, reload cbox.  This adds the new item in the correct position
cbox->Clear();
#if ! defined(__WXGTK__)
  cbox->Append(wxT(""));                              // This used to be needed to give a blank entry to display in the textbox
#endif
for (size_t n=0; n < History->GetCount(); ++n)  cbox->Append((*History)[n]);                
}

void QuickGrepDlg::OnCheck(wxCommandEvent& event) 
{
int id = event.GetId();
if (id == XRCID("PathCurrentDir"))
  { PathGrep->SetValue((event.IsChecked() && MyFrame::mainframe->GetActivePane()) ? MyFrame::mainframe->GetActivePane()->GetActiveDirPath() : wxT(""));
    PathHome->SetValue(false); PathRoot->SetValue(false);  // Clear the other path boxes
  }
if (id == XRCID("PathHome"))
  { PathGrep->SetValue(event.IsChecked() ? wxGetApp().GetHOME() : wxT(""));
    PathCurrentDir->SetValue(false); PathRoot->SetValue(false);
  }
if (id == XRCID("PathRoot"))
  { PathGrep->SetValue(event.IsChecked() ? wxT("/") : wxT(""));
    PathHome->SetValue(false); PathCurrentDir->SetValue(false);
  }
}

void QuickGrepDlg::OnButton(wxCommandEvent& event)
{
int id = event.GetId();

if (id == XRCID("RegexCrib"))
  { if (!HELPDIR.IsEmpty())  HtmlDialog(_("RegEx Help"), HELPDIR + wxT("/RegExpHelp.htm"), wxT("RegExpHelp"), this); return; }

bool dir_recurse = false;
if (id == XRCID("wxID_OK"))
  { wxString next, cmd(wxT("grep "));
    switch(DirectoriesRadio->GetSelection())
      { case 0: next = wxT("-d read "); break;  // Try to match dirnames
        case 1: next = wxT("-r "); dir_recurse = true; break;  // Recurse into dirs. Flag this, we use it below
        case 2: next = wxT("-d skip "); break;  // Ignore
      }
    cmd << next; next.Empty();

    if (WholeWord->IsChecked()) next << wxT("-w ");
    if (IgnoreCase->IsChecked()) next << wxT("-i ");
    if (PrefixLineno->IsChecked()) next << wxT("-n ");
    if (Binaries->IsChecked()) next << wxT("-I ");
    if (Devices->IsChecked()) next << wxT("-D skip ");
    cmd << next;

    next = SearchPattern->GetValue();
    if ( next.IsEmpty()) { BriefMessageBox bmb(wxT("You need to provide a pattern to Search for"), -1, wxT("O_o")); return; }
    cmd << wxT("\\\"") << next << wxT("\\\" ");  SaveCombosHistory(6); // Grep runs in a shell, so we have to escape the quotes

    next = PathGrep->GetValue();
    if ( next.IsEmpty()) { BriefMessageBox bmb(wxT("You need to provide a Path to Search in"), -1, wxT("O_o")); return; }

    // If the -r option isn't 2b used, we need to ensure that any dir Path is wildcarded: otherwise nothing will be searched. So add a * if it's a dir
    if (wxDirExists(next)) AddWildcardIfNeeded(dir_recurse, next); // '~/filefoo' is OK anyway. '~/dirbar/ ~/dirbaz/' will fail: too bad
    QuoteExcludingWildcards(next);
    cmd << next;
    SaveCombosHistory(7); SaveCheckboxState();

        // Even though quickgrep has no history combo, we must add the command to Tools::History, otherwise the combobox subhistories aren't saved
    int index = parent->History.Index(cmd, false);                  // See if this command already exists in the history array
    if (index != wxNOT_FOUND)
      parent->History.RemoveAt(index);                              // If so, remove it.  We'll add it back into position zero  
    parent->History.Insert(cmd, 0);                                 // Either way, insert into position zero

    
    cmd = wxT("sh -c \"") + cmd; cmd << wxT('\"');
    parent->text->HistoryAdd(cmd); parent->text->RunCommand(cmd); parent->text->SetFocus(); // Run the command in the terminalem
  }

EndModal(id);                 // Return the id, because we must anyway, and it might be XRCID("FullGrep")
}

void QuickGrepDlg::AddWildcardIfNeeded(const bool dir_recurse, wxString& path) const // Adds a * to the path if appropriate
{
if (dir_recurse || path.IsEmpty()) return;    // If -r, we don't need an asterix anyway
if (path.Right(1) != wxFILE_SEP_PATH) path << wxFILE_SEP_PATH;
path << wxT('*');
}


void QuickGrepDlg::OnUpdateUI(wxUpdateUIEvent& event)
{
if (DirectoriesRadio == NULL) return; // Protection against too-early updateui events

int id = event.GetId();

if (id == XRCID("FullGrepDefault")) 
  if (((wxCheckBox*)event.GetEventObject())->IsChecked()) WHICHGREP = PREFER_FULLGREP;
}

BEGIN_EVENT_TABLE(QuickGrepDlg, wxDialog)
  EVT_BUTTON(wxID_ANY, QuickGrepDlg::OnButton)
  EVT_CHECKBOX(wxID_ANY, QuickGrepDlg::OnCheck)
  EVT_KEY_DOWN(QuickGrepDlg::OnKeyDown)
  EVT_UPDATE_UI(wxID_ANY, QuickGrepDlg::OnUpdateUI)
END_EVENT_TABLE()
//-----------------------------------------------------------------------------------------------------------------------
wxArrayString TerminalEm::History;
int TerminalEm::HistoryCount = -1;                  // But will be overwritten in LoadHistory() if there's any previous
bool TerminalEm::historyloaded = false;
int TerminalEm::QuerySuperuser = -1;

TerminalEm::~TerminalEm()
{
SaveHistory(); 
if (multiline)   SetWorkingDirectory(originaldir);  // If we originally changed to cwd, revert it
}

void TerminalEm::Init()    // Do the ctor work here, as otherwise wouldn't be done under xrc     
{
int fontsize;

m_running = NULL; m_ExecInPty = NULL;                // Null to show there isn't currently a running process or pty
m_timerIdleWakeUp.SetOwner(this);                    // Initialise the timer

SetName(wxT("TerminalEm"));                          // Distinctive name for DnD

if (TERMINAL_FONT.Ok())  
  {
      SetFont(TERMINAL_FONT);
      SetDefaultStyle(wxTextAttr(wxNullColour, wxNullColour, TERMINAL_FONT));
  }  
 else
   {
#ifdef __WXGTK__
    wxFont font = GetFont(); fontsize = font.GetPointSize() - 2;  // GTK2 gives a font-size that is c.2 points larger than GTK1.2
    font.SetPointSize(fontsize); SetFont(font);        // This changes it only for the INPUT of the textctrl.  See a few lines down for output
#else
    wxFont font = GetFont(); fontsize = font.GetPointSize(); 
#endif

SetDefaultStyle(wxTextAttr(wxNullColour, wxNullColour, wxFont(fontsize, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL)));
  }

m_CommandStart = 0;
busy = false;
PromptFormat = TERMEM_PROMPTFORMAT;
display = this;                                    // The default display destination is ourselves.  This won't work for commandlines, which will reset it later

if (multiline && MyFrame::mainframe->GetActivePane())
  { originaldir = GetCwd();                       // If this is the terminal emulator, not the commandline, save the original cwd
    SetWorkingDirectory(MyFrame::mainframe->GetActivePane()->GetActiveDirPath());  // & then set it to the currently-selected path
  }

LoadHistory();
WritePrompt();                                    // Writes the chosen prompt
}

void TerminalEm::LoadHistory()
{
if (historyloaded)                                   // It's already been loaded by another instance
  { HistoryCount = History.GetCount() - 1; return; } // So just set the index to the correct place & return

wxConfigBase *config = wxConfigBase::Get();
if (config==NULL)  { wxLogError(_("Couldn't load configuration!?")); return; }

config->SetPath(wxT("/History/Terminal/"));

size_t count = config->GetNumberOfEntries();
if (count)
  { wxString item, key, histprefix(wxT("hist_"));  
    for (size_t n=0; n < count; ++n)                 // Create the key for every item, from hist_a to hist_('a'+count-1)
      { key.Printf(wxT("%c"), wxT('a') + (wxChar)n); // Make key hold a, b, c etc
        config->Read(histprefix+key, &item);         // Load the entry
        if (item.IsEmpty())   continue;
        History.Add(item);                           // Store it in the array
      }
  }
HistoryCount = History.GetCount() - 1;               // Set the index to the correct place

historyloaded = true;
config->SetPath(wxT("/"));  
}

void TerminalEm::SaveHistory()
{
wxConfigBase *config = wxConfigBase::Get();
if (config==NULL)  { wxLogError(_("Couldn't load configuration!?")); return; }

config->DeleteGroup(wxT("/History/Terminal"));       // Delete current info, otherwise we'll end up with duplicates or worse
config->SetPath(wxT("/History/Terminal/"));

size_t count = wxMin(History.GetCount(), MAX_COMMAND_HISTORY);  // Count the entries to be saved: not TOO many
size_t first = History.GetCount() - count;           // The newest commands are stored at the end of the array, so save the last 'count' of them
if (!count) { config->SetPath(wxT("/")); return; }

wxString item, key, histprefix(wxT("hist_"));        // Create the key for every item, from hist_a to hist_('a'+count-1)
for (size_t n=0;  n < count; ++n)      
  { key.Printf(wxT("%c"), wxT('a') + (wxChar)n);     // Make key hold a, b, c etc
    config->Write(histprefix+key, History[first+n]); // Save the entry
  }

config->Flush();
config->SetPath(wxT("/"));  
}

inline void TerminalEm::FixInsertionPoint(void)
{
m_CommandStart = GetInsertionPoint();
}

void TerminalEm::WritePrompt()
{
wxString prompt;
promptdir = false;                                     // This turns off changing the prompt with changes of dirview selection
  // %h=hostname %H=hostname_up_to_dot %u=username %w=cwd %W=cwd basename only %$= either $, or # if root      

if (PromptFormat.IsEmpty())  PromptFormat = wxT("%u@%h: %W %$>");

for (size_t n=0; n < PromptFormat.Len(); ++n)
  { wxChar c = PromptFormat[n];
    if (c != wxT('%'))  prompt += c;                                   // If this is a standard character, add it to the string
     else
       { c = PromptFormat[++n];                                        // If it was '%', the next char determines what to do
        if (c==wxT('%')) { prompt += c; continue; }                    // %% really means it
        if (c==wxT('H')) { prompt += wxGetFullHostName();  continue; } // Full host-name
        if (c==wxT('h')) { prompt += wxGetHostName();  continue; }     // Less-full host-name
        if (c==wxT('u')) { prompt += wxGetUserId();  continue; }       // Add user-name to prompt
        if (c==wxT('w')) { prompt += GetCwd(); promptdir = true; continue; }  // Add Current Working Dir
        if (c==wxT('W')) { prompt += GetCwd().AfterLast(wxFILE_SEP_PATH); promptdir = true; continue; }  // Filename-bit only of cwd
        if (c==wxT('$'))                                               // If user is root, add '#'.  Otherwise add '$'
          { // The next bit works out if the user is root or otherwise, & sets a bit of the command-prompt accordingly
            // It's static so done once only as su doesn't work anyway, so the answer shouldn't change!
            if (QuerySuperuser == -1)                                  // If we haven't done this already
              QuerySuperuser = (getuid()==0);    
            if (QuerySuperuser == 1)  prompt += wxT('#');
              else   prompt += wxT('$');
            continue;
          }
      }
  }        
        
if (multiline)   // We have to be a bit careful here.  If there's any text Selected, we'll overwrite it, however many times we SetInsertionPointEnd()
  { long from,  to; GetSelection(&from, &to);                // Save any selection
    SetSelection(GetLastPosition(), GetLastPosition());      // Cancel it, whether there's a valid one or not:  even an old selection interferes
    AppendText(prompt);                                      //  write the prompt,
    if (from < to)  SetSelection(from, to);                  //   then reinstate any valid selection
  }
 else SetValue(prompt);                                      // Set the command-line to the desired prompt

SetInsertionPointEnd();
FixInsertionPoint();
m_CurrentPrompt = prompt;
}

void TerminalEm::OnMouseDClick(wxMouseEvent& event)
{
static bool flag = false;                                    // This flags whether we've already dealt with the original event

if (!flag)                                                   // False means it's the 1st time thru
  { event.Skip();                                            // Let the wxTextCtrl do its thing, to highlight the nearest word

    wxMouseEvent clonedevent(wxEVT_LEFT_DCLICK);             // Now make a duplicate event so that we can re-enter & do the things below
    clonedevent.m_x = event.m_x; clonedevent.m_y = event.m_y;// We can't just continue downwards: it takes so long for the selection to occur
    clonedevent.m_controlDown = event.m_controlDown;
       wxPostEvent(this, clonedevent);                       // Action the new event, & set the flag so that when it appears, we'll know
    flag = true; return;
  }

flag = false;                                                // Reset the flag, as we're about to deal with the clone event

wxString sel = ExtendSelection();                            // Select the whole (presumed) filepath; the dclick itself stops at . and /
DoOpenOrGoTo(event.m_controlDown, sel);                      //   & use another method to deal with it
}

wxString TerminalEm::ExtendSelection(long f, long t)    // Extend selected text to encompass the whole contiguous word
{
if (!multiline) return "";                        // Not relevant for the Commandline
long from, to;
if (f != -1 && t != -1)
  { from = f; to = t; }
 else GetSelection(&from, &to);                   // Get the coords of the word selected

if (GetCurrentPrompt().Len() > from) return "";   // Abort if we're within the prompt

wxString OriginalSel = GetStringSelection();
if (OriginalSel.IsEmpty())
  OriginalSel = GetRange(from, to);

                                  // Now extend the selection backwards & forwards until we hit white-space:  the dclick itself stops at . and /
long x, y; PositionToXY(from, &x, &y);                      // Find what line we're on
wxString wholeline = GetLineText(y);    
int index2 = (int)x; int index1 = index2;                   // x should index the start of the highlit string

if (!wxIsspace(wholeline.at(index1)))                       // Just in case the dclick was at the end of the line
  { --index1;                                               // index1 now indexes the char before the selection
    while (index1 >=0)                                      // Stop if we overreach the beginning of the line
      { if (wholeline.at(index1) == wxT(':') // Stop at the ':' that separates the matching text from the grep filepath. ofc this will break filepaths that contain ':'...
           || (wholeline.at(index1) == wxT('>') && (index1 > 0) && (wholeline.at(index1 - 1) == wxT('$') || wholeline.at(index1 - 1) == wxT('#'))))
         { ++index1; break; }                               // Stop if we reach $>, as this is the prompt
        --index1; --from;                                   // It's a valid char, so incorporate it into the selection and loop
      }
  }
                                  // Now do the same thing to the end of the selection
index2 += OriginalSel.length() - 1;                         // index2 should now index the end of the highlit string
if (!wxIsspace(wholeline.at(index2)))                       // Just in case the dclick was at the end of the line
  { ++index2;
    while (index2 < (int)wholeline.length())
      { if (wholeline.at(index2) == wxT(':'))
          break;  // Stop at the ':' that separates the grep filepath from the matching text. ofc this will break filepaths that contain ':'...
        ++index2; ++to;
      }
  }
      
SetSelection(from, to);                                     // Select the whole contiguous word
if (index1 < 0) index1 = 0; // Unless we stop early for the prompt, index1 will be -1
return wholeline.Mid(index1, index2 - index1);
}

#ifdef __WXX11__
  void TerminalEm::OnRightUp(wxMouseEvent& event)  // EVT_CONTEXT_MENU doesn't seem to work in X11
  {
  wxContextMenuEvent evt(wxEVT_CONTEXT_MENU, 0, ClientToScreen(event.GetPosition()));
  ShowContextMenu(evt); 
  }
#endif

void TerminalEm::ShowContextMenu(wxContextMenuEvent& event)
{
long from, to;
wxTextCoord x, y;
wxTextCtrlHitTestResult HTresult = HitTest(ScreenToClient(wxGetMousePosition()), &x, &y);
if (HTresult != wxTE_HT_UNKNOWN)
  from = to = XYToPosition(x, y);
bool result = (GetLineText(y).StartsWith( GetCurrentPrompt() ) == false); // See if there's a prompt; if so, it's not a result so we won't extend the selection

wxString Sel = GetStringSelection();
if (multiline && result)  // Not relevant for the Commandline, or if we're not in a result line
  { if (Sel.IsEmpty())
      { if (HTresult != wxTE_HT_UNKNOWN)
            from = to = XYToPosition(x, y);
          else GetSelection(&from, &to);                        // Failing that, default to the cursor position

        int len = GetLineLength(y);
        if (x < len) ++to;                                      // If we're not at the end of a line, extend the 'selection' by 1
          else if (x) --from;                                   // Otherwise, assuming there's any text at all, dec by 1
            else return;                                        // No text so abort.  Shouldn't happen as there'll always be a prompt
      }
     else
       { if (HTresult != wxTE_HT_UNKNOWN)
          { from = to = XYToPosition(x, y);
            int len = GetLineLength(y);
            if (x < len) ++to;
              else if (x) --from;
                else return;                                    // No text so abort.  Really shouldn't happen here as there was a selection!
          }
       else return;
      }
    SetSelection(from, to);
  }

bool SelectionIsFilepath(false);
if (multiline && result)  // Not relevant for the Commandline, or if we're not in a result line
  { Sel = ExtendSelection(from, to);
    if (Sel.Left(1) != wxFILE_SEP_PATH) Sel = GetCwd() + wxFILE_SEP_PATH + Sel;   // If the selection can't be a valid absolute path, see if it's a relative one
    FileData fd(Sel);
    if (fd.IsValid())  { SelectionIsFilepath = (fd.IsRegularFile() || fd.IsSymlink()); }   // If valid, see if it's an openable filepath
     else
      { Sel = Sel.BeforeFirst( wxT(':'));                                         // If not, see if it's a filepath followed by ':', as Grep results start with the filepath + ':'
        FileData fd1(Sel);         
        SelectionIsFilepath = (fd1.IsValid() ||  (fd.IsRegularFile() || fd.IsSymlink())); // If it still isn't an openable filepath, don't put GOTO in contextmenu
      }
  }

wxMenu menu(wxT(""));
if (SelectionIsFilepath)
  { menu.Append(SHCUT_TERMINALEM_GOTO, _("GoTo selected filepath"));
    wxMenuItem* open = menu.Append(wxID_ANY, _("Open selected file")); // Trying to use SHCUT_OPEN here fails as the frame insists on disabling it in UpdateUI
    Connect(open->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TerminalEm::OnOpenOrGoTo));
    menu.Append(wxID_SEPARATOR);
  }
menu.Append(wxID_COPY, _("Copy"));
wxMenuItem* paste = menu.Append(wxID_PASTE, _("Paste")); Connect(paste->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TerminalEm::OnOpenOrGoTo));

wxPoint pt = event.GetPosition();
ScreenToClient(&pt.x, &pt.y);

PopupMenu(&menu, pt.x, pt.y);
}

void TerminalEm::OnOpenOrGoTo(wxCommandEvent& event)
{
if (event.GetId() == wxID_COPY)
  { wxString sel = event.GetString();
    wxArrayString filepath; filepath.Add(sel);
    MyGenericDirCtrl* active = MyFrame::mainframe->GetActivePane();
    if (active)
      active->CopyToClipboard(filepath);
  }
  else if (event.GetId() == wxID_PASTE)
  { if (wxTheClipboard->Open())
     { if (wxTheClipboard->IsSupported(wxDF_TEXT) || wxTheClipboard->IsSupported(wxDF_UNICODETEXT))
        { wxTextDataObject data;
          wxTheClipboard->GetData(data);
          WriteText(data.GetText());
        }
      wxTheClipboard->Close();
     }
  }
  else
    DoOpenOrGoTo(event.GetId() == SHCUT_TERMINALEM_GOTO);   // The event could be Open or GoTo
}

void TerminalEm::DoOpenOrGoTo(bool GoTo, const wxString& selection /*=wxT("")*/)    // Used by OnMouseDClick() & Context Menu
{
wxString Sel = selection;
if (Sel.empty())
  Sel = GetStringSelection();
if (Sel.Left(1) != wxFILE_SEP_PATH)  Sel = GetCwd() + wxFILE_SEP_PATH + Sel;    // If the selection can't be a valid absolute path, see if it's a relative one
FileData* fd = new FileData(Sel); 
if (!fd->IsValid())
  { delete fd;
    Sel = Sel.BeforeFirst( wxT(':'));                                           // If it isn't valid, see if it's a filepath followed by ':', as Grep results start with the filepath + ':'
    fd = new FileData(Sel);                          
    if (!fd->IsValid())  
      { delete fd; return; }                                                    // If it still isn't a filepath, abort
  }
MyGenericDirCtrl* active = MyFrame::mainframe->GetActivePane();
if (!active) { delete fd; return; }

if (GoTo)                                                                       // DClick uses the control key as signifying GoTo
  { active->OnOutsideSelect(Sel);                                               // Select the path
    if (fd->IsValid() && !fd->IsDir())
      { if (active->fileview == ISLEFT) active->partner->SetPath(fd->GetFilepath());  // & any filename
          else active->SetPath(fd->GetFilepath());
      }
    delete fd; return; 
  }
delete fd;

if (active->fileview == ISLEFT)  active = active->partner;                      // Otherwise find the active fileview & DoOpen()
((FileGenericDirCtrl*)active)->DoOpen(Sel);
}

void TerminalEm::OnKey(wxKeyEvent &event)
{
wxString command;

switch(event.GetKeyCode())
  { 
    case WXK_PAGEUP:
                      if (!event.ControlDown())  { event.Skip(); return; } // If this is plain PageUp, we don't want to know
                                              // If Ctrl-PageUp, go to the first history entry
                      if (HistoryCount < 0) return;                         // Already there
                      
                      if (HistoryCount == (int)(History.GetCount() - 1))    // If this is the 1st step into History
                        UnenteredCommand = GetRange(GetCommandStart(), GetLastPosition());  //  save any partially-entered command, lest it's wanted later
                      HistoryCount = 0;                                     // Go to the first history entry
                      Remove(GetCommandStart(), GetLastPosition());
                      SetSelection(GetLastPosition(), GetLastPosition());   // Cancel any selection, which otherwise would grab the AppendText:  even an old selection interferes
                      AppendText(GetPreviousCommand());  
                      SetInsertionPointEnd();
                      return;
    case WXK_UP:      if (HistoryCount == (int)(History.GetCount() - 1))    // If this is the 1st step into History
                        UnenteredCommand = GetRange(GetCommandStart(), GetLastPosition());  //    save any partially-entered command, lest it's wanted later
                      
                      command = GetPreviousCommand();                       // Get the previous command
                      if (command.IsEmpty())     return;                    // If empty, we're out of them so abort

                      Remove(GetCommandStart(), GetLastPosition());         // Clear the textctrl (except for the cursor)
                      SetSelection(GetLastPosition(), GetLastPosition());   // Cancel any selection, which otherwise would grab the AppendText:  even an old selection interferes
                      AppendText(command);                                  // Insert the previous command
                      SetInsertionPointEnd();
                      return;

    case WXK_PAGEDOWN:
                      if (!event.ControlDown())  { event.Skip(); return; }  // If this is plain PageDown, we don't want to know
                                  // If Ctrl-PageDown, go to the UnenteredCommand if it exists, else blank
                      HistoryCount = (int)(History.GetCount() - 2);         // This sets up for GetNextCommand(), into which we fall
    case WXK_DOWN:    Remove(GetCommandStart(), GetLastPosition());
                      SetSelection(GetLastPosition(), GetLastPosition());   // Cancel any selection, which otherwise would grab the AppendText:  even an old selection interferes
                      AppendText(GetNextCommand());                         // Don't check for empty this time, as a blank is useful
                      SetInsertionPointEnd();
                      return;
    case WXK_NUMPAD_ENTER:
    case WXK_RETURN:  command = GetRange(GetCommandStart(), GetLastPosition());                         
                      command.Trim(false);
                      command.Trim(true);
                                        //  Using 'display->' below ensures that commandline commands are passed to terminal
                      display->AppendText(wxT("\n"));                       // Run the command on a new line
                      if (!multiline)  OnClear();                           // If not multiline, clear the text

                      if (CheckForCD(command))                              // If this is just a request to change dir, do it & abort.
                           { HistoryAdd(command); return; }
                                        // If there's already a process running, the string is user input into it
                      if (m_running != NULL)  m_running->InputString = command;
                       else if (m_ExecInPty != NULL)  m_ExecInPty->AddInputString(command);
                       else               // Otherwise it must be a command to be run
                          { HistoryAdd(command);                            // Store the command first, as it may be altered in Process()
                            QueryWrapWithShell(command);                    // Searches command for e.g. '>'. If found, wraps command with sh -c
                            busy = true;
                            display->RunCommand(command, false);            // False says this is internally generated, not from eg Find
                            busy = false;
                          }
                      return;
    case WXK_DELETE:
    case WXK_BACK:    long from, to; GetSelection(&from,  &to);
                      if (to > from)                                        // If there's a valid selection, delete it
                          { if (to > GetCommandStart())   Remove(wxMax(from,GetCommandStart()), to); }
                        else                                                // Otherwise do a normal Del/Backspace
                          { if (event.GetKeyCode() == WXK_BACK)
                              { long IP; IP = GetInsertionPoint();  if (IP  > GetCommandStart())    Remove(IP-1, IP); }
                              else
                              { long IP; IP = GetInsertionPoint();  if (IP >= GetCommandStart() && IP < GetLastPosition())  Remove(wxMax(IP,GetCommandStart()), IP+1); }
                          }
                      return;
    case WXK_LEFT:    if (GetInsertionPoint() > GetCommandStart())  event.Skip();  
                      return;  
    case WXK_HOME:    SetInsertionPoint(GetCommandStart());
                      return;
   }

            // We need to do this here, as otherwise wxTextCtrl swallows Ctrl-F.  NB in gtk2, we can't use the event to get the flags, so:
int flags = 0; if (wxGetKeyState(WXK_CONTROL)) flags |= wxACCEL_CTRL; if (wxGetKeyState(WXK_ALT)) flags |= wxACCEL_ALT; if (wxGetKeyState(WXK_SHIFT)) flags |= wxACCEL_SHIFT;
AccelEntry* accel = MyFrame::mainframe->AccelList->GetEntry(SHCUT_TOOL_FIND);
if (event.GetKeyCode() == 'F' && accel->GetKeyCode() == 'F' && accel->GetFlags() == flags)  // Of course we have to check that the shortcut hasn't been changed
  { MyFrame::mainframe->Layout->DoTool(find); return; }
#ifdef __WXX11__
  accel = MyFrame::mainframe->AccelList->GetEntry(SHCUT_SELECTALL);
  if (event.GetKeyCode() == accel->GetKeyCode()  && accel->GetFlags() == flags)  { OnCtrlA(); return; }  // X11 doesn't seem to do Ctrl-A, & I couldn't get an accel table to work
#endif
            // If it wasn't any of these exciting keys, just ensure a sane cursor position, then pass the buck to base-class
long from, to; GetSelection(&from,  &to);  // but we do need to prevent the prompt being deleted accidentally if part of it is selected, then a key pressed
if (to > from)                                                            // If there is a selection,
  { if (to < GetCommandStart()) return;                                   // Abort if it's only the prompt that's selected
    if (from < GetCommandStart()) SetSelection(GetCommandStart(), to);    // Otherwise move the selection start to the deletable bit
  }
if (GetInsertionPoint() < GetCommandStart()) SetInsertionPointEnd();
event.Skip();
}

void TerminalEm::HistoryAdd(wxString& command)
{
if (command.IsEmpty()) return;
                                // Append the command to History, but only if it doesn't duplicate the previous entry
size_t count = History.GetCount();
if (count && (History[count-1] == command)) ;  // If identical commands, don't do anything
  else History.Add(command);                

UnenteredCommand.Empty();                      // If we've been hoarding a partially-entered command, we can stop now
        // HistoryCount might have been indexing anywhere in History, due to previous retrievals.  Reset it to the top
HistoryCount = History.GetCount() - 1;
}

wxString TerminalEm::GetPreviousCommand()  // Retrieve the 'Previous' command from History
{
if (!History.GetCount())  { wxBell(); return wxEmptyString; }  // Not a lot to retrieve!
if (HistoryCount < 0)   { wxBell(); return wxEmptyString; }    // We've already retrieved all the available history

wxString command = History[HistoryCount--];  // HistoryCount points to the item to be returned by a Previous request.  Dec it after, for the future
return command;
}

wxString TerminalEm::GetNextCommand()  // Retrieve the 'Next' command from History ie move towards the current 'entry'
{
if (!History.GetCount()) return wxEmptyString;          // Not a lot to unretrieve!
if (HistoryCount == (int)(History.GetCount() - 2))      // Already unretrieved all the genuine history
  { ++HistoryCount; return UnenteredCommand; }          //  so inc HistoryCount ready for a 'Previous', & return the carefully preserved UnenteredCommand

if (HistoryCount >= (int)(History.GetCount() - 1))      // Already unretrieved all the genuine history AND the UnenteredCommand
     return wxEmptyString;                              //   so return blank, to give a clean sheet if desired

wxString command = History[++HistoryCount + 1];         // HistoryCount points to the last item returned by a Next request, so inc it first
return command;
}

void TerminalEm::OnClear()
{
Clear();
WritePrompt();  
}

void TerminalEm::OnChangeDir(const wxString& newdir, bool ReusePrompt)  // Change the cwd, this being the only way to cd in the terminal/commandline
{
wxLogNull NoCantFindDirMessagesThanks;

  // The 'if' part of this is to protect against being inside an archive: you can't SetWorkingDirectory there!
MyGenericDirCtrl* active = MyFrame::mainframe->GetActivePane();
if (active && active->arcman && active->arcman->IsWithinArchive(newdir)) 
  { SetWorkingDirectory(active->arcman->GetPathOutsideArchive()); }
 else
  { FileData fd(newdir);  // SetWorkingDirectory() can't cope with filepaths, only paths, so amputate any filename bit
    if (!fd.IsValid()) { return; }
    if (fd.IsSymlinktargetADir())
      SetWorkingDirectory(fd.GetUltimateDestination());
     else
      SetWorkingDirectory(fd.IsDir() ? newdir : fd.GetPath());
  }

if (!ReusePrompt)  { WritePrompt(); return; }              // If this is a from a terminal cd, we need to rewrite the prompt properly

                            // Otherwise this must be from a change of dirview selection
if (!promptdir) return;                                    // If the prompt doesn't contain the dir, no point changing it!

if (busy) return;                                          // If we're in the middle of a command, don't redo the prompt

                      // Redraw the prompt rather than redo it on a new line.  This allows any user input to be conserved
wxString temp = GetRange(GetCommandStart(), GetLastPosition()); // Save any recently-entered text
int len = GetLineLength(GetNumberOfLines() - 1);           // We need to find the start-of-line position.  The only way is to find the line & its length, then start = end-len
Remove(GetLastPosition() - len, GetLastPosition());        // Delete the current line
SetInsertionPointEnd();                                    // Make sure we're actually on the last line, & haven't been scrolling up the screen!
WritePrompt();
SetSelection(GetLastPosition(), GetLastPosition());        // Cancel any selection, which otherwise would grab the AppendText; even an old selection interferes
AppendText(temp);
}

int TerminalEm::CheckForSu(const wxString& command)  // su fails and probably hangs, so check and abort here. su -c and sudo need different treatment
{
if (command.IsEmpty()) return 0;
  // Returns 0 for 'su', so the caller should abort; 1 for a standard command; 2 for su -c or sudo, which requires 4Pane's builtin su
int ret = _CheckForSu(command); // Use a function, as the code is shared with LaunchMiscTools::Run

if (ret == 0)
  { wxString msg(_("Sorry, becoming superuser like this isn't possible here.\n"));
    if (USE_SUDO) msg << _("You need to start each command with 'sudo'");
     else msg << _("You can instead do: su -c \"<command>\"");
    wxMessageBox(msg, wxT("4Pane"), wxICON_ERROR | wxOK, this);

    Remove(GetCommandStart(), GetLastPosition());
  }

return ret;
}

bool TerminalEm::CheckForCD(wxString command)  // cd refuses to work, so intercept attempts & do it here
{
if (command.IsEmpty()) return true;                     // Returning true tells caller there's nothing else to do
if (command.Left(2) != wxT("cd"))  return false;        // Not a cd attempt so return false so that RunCommand takes over
if (wxIsalpha(command.GetChar(2))) return false;        // This must be a real command beginning with cd???

if (command == wxT("cd.") || command == wxT("cd ."))  return true;  // Not a lot to do

if (command == wxT("cd") || command == wxT("cd~") || command == wxT("cd ~"))  // Go home
  command = wxGetHomeDir();
 else                                                   // Otherwise it should be a real dir. Remove the initial cd, plus any white-space  
  { command = command.Mid(2); command.Trim(false); }
  
MyFrame::mainframe->Layout->OnChangeDir(command, false); // The residue should be a valid dir to change to
return true;
}

void TerminalEm::ClearHistory()
{
History.Clear();
}

void TerminalEm::RunCommand(wxString& command, bool external /*= true*/)  // Do the Process/Execute things to run the command
{
if (command.IsEmpty()) return;

if (external)                                       // If this is an externally generated command eg locate, grep
  { AppendText(command); AppendText(wxT("\n")); }   //  write the command into the terminal, then CR

int chk_for_su = CheckForSu(command);
if (chk_for_su == 0) return;                        // A 'su' attempt

if (chk_for_su == 2)                                // 'su -c ' or 'sudo ', so ExecuteInPty()
  { m_ExecInPty = new ExecInPty(this);
    /*long ret =*/ m_ExecInPty->ExecuteInPty(command);
/*    if (ret > 0)
      { if (!errors.GetCount()) { wxLogError(wxT("Execution of '%s' failed."), command.c_str()); } // If there's no message, fire a dialog
        for (size_t n=0; n < errors.GetCount(); ++n) *this << errors.Item(n); 
      }
     else
      { for (size_t n=0; n < output.GetCount(); ++n) *this << output.Item(n); }

    WritePrompt(); return;*/
  }
 else                                               // The standard, non-super way
  { MyPipedProcess* process = new MyPipedProcess(this, external); // Make a new Process, the sort with built-in redirection
    long pid = wxExecute(command, wxEXEC_ASYNC | wxEXEC_MAKE_GROUP_LEADER, process); // Try & run the command.  pid of 0 means failed

    if (!pid)
      { wxLogError(wxT("Execution of '%s' failed."), command.c_str());
        delete process; return;
      }

    process->m_pid = pid;                               // Store the pid in case Cancel is pressed
    AddAsyncProcess(process);                           // Start the timer to poll for output
  }

wxWindow* Cancel = MyFrame::mainframe->Layout->bottompanel->FindWindow(XRCID("Cancel"));
if (Cancel) { Cancel->Enable(); Cancel->Update(); }
}

void TerminalEm::AddInput(wxString input) // Displays input received from the running Process
{
if (input.IsEmpty()) return;

display->AppendText(input);               // Add string to textctrl.  See above for explanation of display 

SetInsertionPointEnd();                   // If we don't, the above text is added to any future user input and passed to the command!
FixInsertionPoint();
}

void TerminalEm::OnEndDrag()    // Drops filenames into the prompt-line, either of the terminal em or commandline
{
wxString command;

enum myDragResult dragtype = MyFrame::mainframe->ParseMetakeys();  // First find out if any metakeys were pressed

for (size_t count=0; count < MyGenericDirCtrl::DnDfilecount; ++count)
  switch (dragtype)        // I'm reusing the ParseMetakeys answers, even though they're nothing to do with TerminalEm
     {
      case myDragHardLink:                                  // Ctrl+Sh pressed, so add the whole filepath to the string
                          command += MyGenericDirCtrl::DnDfilearray[count] + wxT(' '); continue;
      case myDragCopy:    command += wxT("./");             // Just Ctrl pressed, so add "./"  to the string, then fall thru to add the filename
        default:          wxString filename = MyGenericDirCtrl::DnDfilearray[count].AfterLast(wxFILE_SEP_PATH);    // Treat anything else as none pressed
                          if (filename.IsEmpty()) filename = (MyGenericDirCtrl::DnDfilearray[count].BeforeLast(wxFILE_SEP_PATH)).AfterLast(wxFILE_SEP_PATH);
                          command += filename + wxT(' ');
     }

AppendText(command);                                        // Insert at the command prompt
SetInsertionPointEnd();
SetFocus();                                                 // Without this, adding a filepath by drag to e.g. rm -f leaves the focus on the treectrl, so pressing Enter tries to launch the file

MyFrame::mainframe->m_TreeDragMutex.Unlock();               // Finally, release the tree-mutex lock
}

void TerminalEm::OnCancel()
{
if (m_running) m_running->OnKillProcess();                  // If there's a running process, murder it
wxWindow* Cancel = MyFrame::mainframe->Layout->bottompanel->FindWindow(XRCID("Cancel"));
if (Cancel) Cancel->Disable();                            // We don't need the Cancel button for a while
}

void TerminalEm::OnSetFocus(wxFocusEvent& WXUNUSED(event))
{
HelpContext = HCterminalEm;
}

void TerminalEm::AddAsyncProcess(MyPipedProcess* process /*= NULL*/)  // Starts up the timer that generates wake-ups for OnIdle
{
if (!process)                                               // If it's an ExecInPty instance, just start the timer
  { m_timerIdleWakeUp.Start(100); return; }

if (m_running==NULL)                                        // If it's not null, there's already a running process
  { m_timerIdleWakeUp.Start(100);                           // Tell the timer to create idle events every 1/10th sec
    m_running = process;
  }
}

void TerminalEm::RemoveAsyncProcess(MyPipedProcess* process /*= NULL*/) // Stops everything, whether we were using a process or an ExecInPty
{ 
m_timerIdleWakeUp.Stop();

delete process;
delete m_ExecInPty;

m_running = NULL; m_ExecInPty = NULL;
}

void TerminalEm::OnProcessTerminated(MyPipedProcess* process /*= NULL*/)
{
RemoveAsyncProcess(process);                                // Shut down the process
wxWindow* Cancel = MyFrame::mainframe->Layout->bottompanel->FindWindow(XRCID("Cancel"));
if (Cancel) Cancel->Disable();                              // We don't need the Cancel button for a while

wxLogNull NoCantFindDirMessagesThanks;                      // Just in case the Process was "rm -rf ."
WritePrompt();
}

void TerminalEm::OnTimer(wxTimerEvent& WXUNUSED(event))  // When the timer fires, it wakes up the OnIdle method
{
wxWakeUpIdle();
}

void TerminalEm::OnIdle(wxIdleEvent& event)  // Made to happen by the timer firing, runs HasInput() while there is more to come
{
if (m_running)
  { m_running->SendOutput();                                    // Send any user-output to the process
    m_running->HasInput();                                      // Check for input from the process
  }
 else if (m_ExecInPty)
    m_ExecInPty->WriteOutputToTextctrl();                       // Get any input from the pty
}

IMPLEMENT_DYNAMIC_CLASS(TerminalEm, TextCtrlBase)

BEGIN_EVENT_TABLE(TerminalEm, TextCtrlBase)
  EVT_MENU(SHCUT_TERMINALEM_GOTO, TerminalEm::OnOpenOrGoTo)
  EVT_SET_FOCUS(TerminalEm::OnSetFocus)
  EVT_LEFT_DCLICK(TerminalEm::OnMouseDClick)
  EVT_CONTEXT_MENU(TerminalEm::ShowContextMenu)
#ifdef __WXX11__
  EVT_RIGHT_UP(TerminalEm::OnRightUp)          // EVT_CONTEXT_MENU doesn't seem to work in X11
#endif
  EVT_KEY_DOWN(TerminalEm::OnKey)
  EVT_IDLE(TerminalEm::OnIdle)
  EVT_TIMER(-1, TerminalEm::OnTimer)
  END_EVENT_TABLE()
//-----------------------------------------------------------------------------------------------------------------------
MyBriefLogStatusTimer* BriefLogStatus::BriefLogStatusTimer = NULL;

BriefLogStatus::BriefLogStatus(wxString Message, int secondsdisplayed /*= -1*/, int whichstatuspane /*= 1*/)
{
if (!BriefLogStatusTimer)
  BriefLogStatusTimer = new MyBriefLogStatusTimer;

//wxString olddisplay = MyFrame::mainframe->GetStatusBar()->GetStatusText(whichstatuspane);  // Save the current display for this statusbar pane
wxString olddisplay; // This was a good idea, but as I don't currently use pane 0 for any 'permanent' display, it's unnecessary, and causes confusion if the last display was itself meant 2b brief

BriefLogStatusTimer->Stop();                               // In case another message is already displayed
BriefLogStatusTimer->Init(MyFrame::mainframe, olddisplay, whichstatuspane);

MyFrame::mainframe->SetStatusText(Message, whichstatuspane);

int length;
if (secondsdisplayed == 0) return;                        // Zero signifies indefinite
if (secondsdisplayed < 0)                                 // Minus number means default
  length = DEFAULT_BRIEFLOGSTATUS_TIME;
 else length = secondsdisplayed;
 
BriefLogStatusTimer->Start(length * 1000, wxTIMER_ONE_SHOT);
}

void DoBriefLogStatus(int no, wxString msgA, wxString msgB)  // A convenience function, to call BriefLogStatus with the commonest requirements
{
wxString msg; msg << no;
if (msgA.IsEmpty())                // If nothing different was supplied, call the things item(s)
  { wxString item = (no > 1  ? _(" items ") : _(" item "));
    msgA << item;
  }
if (msgA.GetChar(0) != wxT(' ')) msg << wxT(' ');
BriefLogStatus bls(msg + msgA + msgB);
}
//-----------------------------------------------------------------------------------------------------------------------

int LaunchMiscTools::m_menuindex = -1;

void LaunchMiscTools::ClearData()
{
for (int n = (int)ToolArray.GetCount();   n > 0; --n)  { UserDefinedTool* item = ToolArray.Item(n-1); delete item; ToolArray.RemoveAt(n-1); }
for (int n = (int)FolderArray.GetCount(); n > 0; --n)  { UDToolFolder* item = FolderArray.Item(n-1); delete item; FolderArray.RemoveAt(n-1); }
}

void LaunchMiscTools::LoadMenu(wxMenu* tools /*=NULL*/)
{
bool FromContextmenu = (tools != NULL);
LoadArrays();
if (!FolderArray.GetCount()) return;                // No data, not event the "Run a Program" menu

if (tools == NULL)                                  // If so, we're loading the menubar menu, so find it
  { wxCHECK_RET(m_menuindex > wxNOT_FOUND, wxT("Tools menu index not set")); // Should have been set when the bar was loaded
    tools = MyFrame::mainframe->GetMenuBar()->GetMenu(m_menuindex);
    wxCHECK_RET(tools, wxT("Couldn't find the Tools menu"));
    static bool alreadyloaded=false; if (!alreadyloaded) { tools->AppendSeparator(); alreadyloaded=true; }  // We don't want multiple separators due to reloads
  }

FillMenu(tools);                                    // Do everything else in recursive submethod

if (FromContextmenu)                                // If we're filling the context menu
  { if (LastCommandNo != (size_t)-1)                //  and there *was* a last command
      { wxString lastlabel(_("Repeat:  "));         // Alter the Repeat Last Command menu entry to display the actual command
        lastlabel += ToolArray[ LastCommandNo-SHCUT_TOOLS_LAUNCH ]->Label;
        tools->Append(SHCUT_TOOLS_REPEAT, lastlabel);
      }
  }
 else
    tools->Append(SHCUT_TOOLS_REPEAT, _("Repeat Previous Program"));
}

void LaunchMiscTools::LoadArrays()    // from config
{
ClearData();                                // Make sure the arrays are empty:  they won't be for a re-load!
EventId = SHCUT_TOOLS_LAUNCH; FolderIndex = EntriesIndex = 0;  // Similarly reset EventId & indices

Load(wxT("/Tools/Launch"));                 // Recursively load data from config into structarrays
}

void LaunchMiscTools::Load(wxString name)  // Recursively load from config a (sub)menu 'name' of user-defined commands
{
if (name.IsEmpty()) return;
if (EventId >= SHCUT_TOOLS_LAUNCH_LAST) return; // Not very likely, since there's space for 500 tools!

wxConfigBase* config = wxConfigBase::Get();  
if (config==NULL)  { wxLogError(_("Couldn't load configuration!?")); return; }
config->SetPath(name);                          // Use name to create the config path eg /Tools/Launch/a

struct UDToolFolder* folder = new UDToolFolder;
config->Read(wxT("Label"), &folder->Label);

wxString itemlabel, itemdata, key, data(wxT("data_")), root(wxT("asroot_")), terminal(wxT("terminal_")), persists(wxT("persists_")),
                                      refreshpage(wxT("refreshpage_")), refreshopposite(wxT("refreshopposite_")), label(wxT("label_"));
bool asroot, interminal, terminalpersists, refresh, refreshopp;
                                  // First do the entries
size_t count = (size_t)config->Read(wxT("Count"), 0l);// Load the no of tools
for (size_t n=0; n < count; ++n)                      // Create the key for every item, from label_a & data_a, to label_/data_('a'+count-1)
  { key.Printf(wxT("%c"), wxT('a') + (wxChar)n);      // Make key hold a, b, c etc
    config->Read(label+key, &itemlabel);              // Load the entry's label
    config->Read(data+key, &itemdata);                //  & data
    config->Read(root+key, &asroot, false);
    config->Read(terminal+key, &interminal, false);
    config->Read(persists+key, &terminalpersists, false);
    config->Read(refreshpage+key, &refresh, false);
    config->Read(refreshopposite+key, &refreshopp, false);

    if (itemdata.IsEmpty())  continue;
    if (itemlabel.IsEmpty()) itemlabel = itemdata;    // If need be, use the data as label
    
    AddToolToArray(itemdata, asroot, interminal, terminalpersists, refresh, refreshopp, itemlabel);  // Make a new tool item & append it to toolarray
        
    folder->Entries++;                                // We've successfully loaded an entry, so inc the count for this folder
  }

count = config->GetNumberOfGroups();                  // Count how many subfolders this folder has.  Save the number
folder->Subs = count;
FolderArray.Add(folder);

                                  // Now deal with any subfolders, by recursion
for (size_t n=0; n < count; ++n)                      // Create the path for each.  If name was "a", then subfolders will be "a/a", "a/b" etc
  { key.Printf(wxT("%c"), wxT('a') + (wxChar)n);
    Load(name + wxCONFIG_PATH_SEPARATOR + key);
  }

config->SetPath(wxT("/"));  
}

void LaunchMiscTools::SaveMenu()  // Save data from the folder & tools arrays to config, delete all menu entries
{
if (!FolderArray.GetCount()) return;                 // No data, not even the "Run a Program" menu

FolderIndex = EntriesIndex = 0;                      // Reset indices.  The submethod uses these to index next entry/folder to save

wxConfigBase* config = wxConfigBase::Get();
config->DeleteGroup(wxT("/Tools/Launch"));           // Delete current info (otherwise deleted data could remain, & be reloaded in future)
Save(wxT("/Tools/Launch"), config);                  // Recursively save
config->Flush();

wxCHECK_RET(m_menuindex > wxNOT_FOUND, wxT("Tools menu index not set")); // Should have been set when the bar was loaded
wxMenu* tools = MyFrame::mainframe->GetMenuBar()->GetMenu(m_menuindex);
wxCHECK_RET(tools, wxT("Couldn't find the Tools menu"));

int sm = tools->FindItem(FolderArray[0]->Label);     // Find the id menu of the relevant submenu in the Tools menu
if (sm == wxNOT_FOUND) return;
tools->Destroy(sm);                                  // Use this to delete (recursively) the "Run a Program" menu

tools->Delete(SHCUT_TOOLS_REPEAT);                   // Finally remove the "Repeat Last Command" entry, otherwise we'll end up with several of them
}

void LaunchMiscTools::Save(wxString name, wxConfigBase* config)  // Recursively save to config a (sub)menu of user-defined commands
{
if (FolderIndex >= FolderArray.GetCount()) return;

struct UDToolFolder* folder = FolderArray[FolderIndex++];

wxString key, data(wxT("/data_")), root(wxT("/asroot_")), terminal(wxT("/terminal_")), persists(wxT("/persists_")), 
              refreshpage(wxT("/refreshpage_")), refreshopposite(wxT("/refreshopposite_")), label(wxT("/label_"));

config->Write(name + wxT("/Label"), folder->Label);  // Save the metadata & entries for this folder
config->Write(name + wxT("/Count"), (long)wxMin(folder->Entries, 26));
for (size_t n=0; n < folder->Entries; ++n)           // Create the key for every item, from label_a & data_a, to label_/data_('a'+count-1)
  { if (EntriesIndex >= ToolArray.GetCount()) break;
    if (n > 25) { EntriesIndex++; break; }           // We can only cope with 26 tools per folder (!).  Inc thru any excess
    
    key.Printf(wxT("%c"), wxT('a') + (wxChar)n);     // Make key hold a, b, c etc
    config->Write(name + label + key, ToolArray[EntriesIndex]->Label);
    config->Write(name + data + key, ToolArray[EntriesIndex]->Filepath);
    config->Write(name + root + key, ToolArray[EntriesIndex]->AsRoot);
    config->Write(name + terminal + key, ToolArray[EntriesIndex]->InTerminal);
    config->Write(name + persists + key, ToolArray[EntriesIndex]->TerminalPersists);
    config->Write(name + refreshpage + key, ToolArray[EntriesIndex]->RefreshPane);
    config->Write(name + refreshopposite + key, ToolArray[EntriesIndex++]->RefreshOpposite);
  }

                                    // Now deal with any subfolders, by recursion
for (size_t n=0; n < folder->Subs; ++n)  
  { if (n > 25) { FolderIndex++; break; }
    key.Printf(wxT("%c"), wxT('a') + (wxChar)n);     // Create the path for each.  If name was "/Tools/Launch/a", then subfolders will be "/Tools/Launch/a/a", "../a/b" etc
    Save(name + wxCONFIG_PATH_SEPARATOR + key, config);
  }
}

void LaunchMiscTools::Export(wxConfigBase* config)   // Exports the data to a conf file
{
if (!FolderArray.GetCount()) return;                 // No data, not even the "Run a Program" menu

size_t OldEntriesIndex = EntriesIndex; size_t OldFolderIndex = FolderIndex; // Save the real values
FolderIndex = EntriesIndex = 0;                      // Reset indices.  The submethod uses these to index next entry/folder to save

Save(wxT("/Tools/Launch"), config);                  // Recursively save
EntriesIndex = OldEntriesIndex;FolderIndex =  OldFolderIndex;
}

void LaunchMiscTools::AddToolToArray(wxString& command, bool asroot,  bool interminal, bool terminalpersists,
                                       bool refreshpane, bool refreshopp, wxString& label, int insertbeforeitem/* = -1*/)  // -1 signifies append
{
struct UserDefinedTool* toolstruct = new UserDefinedTool;
toolstruct->Filepath = command; toolstruct->AsRoot = asroot; toolstruct->InTerminal = interminal; toolstruct->TerminalPersists = terminalpersists;
toolstruct->RefreshPane = refreshpane; toolstruct->RefreshOpposite = refreshopp; toolstruct->Label = label;
toolstruct->ID = EventId++;
if (insertbeforeitem != -1 && ToolArray.GetCount() && insertbeforeitem < (int)(ToolArray.GetCount()))
      ToolArray.Insert(toolstruct, insertbeforeitem);
 else ToolArray.Add(toolstruct);
}

bool LaunchMiscTools::AddTool(int folder, wxString& command, bool asroot, bool interminal, bool terminalpersists, bool refreshpane, bool refreshopp, wxString& label)
{
if (FolderArray.Item(folder)->Entries > 25)
  { wxMessageBox(_("Sorry, there's no room for another command here\nI suggest you try putting it in a different submenu"),
                                                                      wxT("4Pane"), wxOK | wxCENTRE, MyFrame::mainframe->GetActivePane());
    return false;
  }
  
    // We need to find after which entry to insert, so for every folder <= the one 2b added to, add its entries to entrycount
size_t entrycount=0; for (int n=0; n <= folder; ++n) entrycount += FolderArray.Item(n)->Entries;
if (entrycount && (entrycount < ToolArray.GetCount()))                     // For any tool above the new entry, we must inc any existing accel shortcut-no
  { int nextID = ToolArray.Item(entrycount-1)->ID;
    ArrayOfAccels accelarray = MyFrame::mainframe->AccelList->GetAccelarray();
    for (size_t n=0; n < accelarray.GetCount(); ++n)
      if (accelarray.Item(n)->GetShortcut() > nextID)
          accelarray.Item(n)->SetShortcut(accelarray.Item(n)->GetShortcut() + 1);
    MyFrame::mainframe->AccelList->SaveUserDefinedShortcuts(accelarray);  // Save the amended shortcut data
  }
AddToolToArray(command, asroot, interminal, terminalpersists, refreshpane, refreshopp, label, entrycount); // Do the addition

FolderArray.Item(folder)->Entries++;
BriefMessageBox msg(_("Command added"), AUTOCLOSE_TIMEOUT / 1000, _("Success")); return true;
}

bool LaunchMiscTools::DeleteTool(int sel)
{
int entrycount = 0;      // We need to find which folder contains the entry to delete, so we can dec its count
for (size_t n=0; n < FolderArray.GetCount(); ++n)
  { entrycount += FolderArray.Item(n)->Entries;             // Get a cum total of entries in the folders to date
    if (entrycount < sel) continue;                         // If the cum entrycount is less than this selection, we're not there yet
    
    FolderArray.Item(n)->Entries--; break;                  // If entrycount >= the selection, the current folder must be the one to dec
  }

int nextID = ToolArray.Item(sel)->ID;                       // Now sort out any user-defined accelerators for tools
ArrayOfAccels accelarray = MyFrame::mainframe->AccelList->GetAccelarray();
for (size_t n=0; n < accelarray.GetCount(); ++n)
  { if (accelarray.Item(n)->GetShortcut() == nextID)        // If the tool 2b deleted has a shortcut, delete it
      { accelarray.RemoveAt(n);                             // (I expected to have to delete this entry, but trying->segfault)
        --n;                                                // We now have to dec n, else the next entry gets missed
      }
     else if (accelarray.Item(n)->GetShortcut() > nextID)   // For any tool above that 2b deleted, we must dec any existing accel shortcut-no
        accelarray.Item(n)->SetShortcut(accelarray.Item(n)->GetShortcut() - 1);
  }

MyFrame::mainframe->AccelList->SaveUserDefinedShortcuts(accelarray);  // Save the amended shortcut data

UserDefinedTool* tool = ToolArray.Item(sel); ToolArray.RemoveAt(sel); delete tool; // Remove & delete the tool
return true;
}

void LaunchMiscTools::FillMenu(wxMenu* parentmenu)  // Recursively append entries and submenus to parentmenu
{
if (parentmenu==NULL || FolderIndex >= FolderArray.GetCount()) return;

UDToolFolder* folder = FolderArray[ FolderIndex++ ];

wxMenu* menu = new wxMenu();
for (size_t n=0; n < folder->Entries; ++n)
  { if (EntriesIndex >= ToolArray.GetCount()) break;
    UserDefinedTool* tool = ToolArray[ EntriesIndex++ ]; if (tool->Label.IsEmpty()) continue;
    MyFrame::mainframe->AccelList->AddToMenu(*menu, tool->ID, "", wxITEM_NORMAL, tool->Filepath);  // Append this entry to the menu, using Filepath (the command) as help-string
  }

for (size_t n=0; n < folder->Subs; ++n)  FillMenu(menu);        // Do by recursion any submenus

parentmenu->Append(SHCUT_TOOLS_LAUNCH_LAST, folder->Label, menu); // Append the menu to parent. Needs an id, so (ab)use SHCUT_TOOLS_LAUNCH_LAST
}

void LaunchMiscTools::DeleteMenu(size_t DeleteIt, size_t &entrycount)  // Delete the folder DeleteIt & its progeny
{
if (DeleteIt >= FolderArray.GetCount()) return;

for (size_t n=0; n < FolderArray[DeleteIt]->Entries; ++n)  // First kill the entries
  { UserDefinedTool* tool = ToolArray[entrycount];
    ToolArray.RemoveAt(entrycount);                        // Don't inc entrycount:  removing from the array has the same effect!
    delete tool;
  }

for (size_t n=0; n < FolderArray[DeleteIt]->Subs; ++n)    
    DeleteMenu(DeleteIt+n+1, entrycount);                  // Recursively delete any subfolders

UDToolFolder* folder = FolderArray[DeleteIt];              // Now delete this folder
FolderArray.RemoveAt(DeleteIt); delete folder;
}

void LaunchMiscTools::Run(size_t index)
{
if ((int)index >= SHCUT_TOOLS_LAUNCH_LAST) return;          // Just in case something got corrupted

wxString command, originalfilepath;
bool UseTerminal = ToolArray[index-SHCUT_TOOLS_LAUNCH]->InTerminal;
if (UseTerminal)
  { if (ToolArray[index-SHCUT_TOOLS_LAUNCH]->TerminalPersists)
      command = TOOLS_TERMINAL_COMMAND;                     // If the command runs in a terminal & the user wants to be able to read the output, prefix with xterm -hold -e or whatever
     else command = TERMINAL_COMMAND;                       // If in a terminal but doesn't have to hold, prefix with xterm -e  or whatever
  }

originalfilepath = GetCwd();                                // Change the working dir to the parameter's path, or badly-written programs may malfunction
MyGenericDirCtrl* active = MyFrame::mainframe->GetActivePane();
if (active)
  { wxString newdir = active->GetPath();
    FileData stat(newdir); if (!stat.IsDir()) newdir = stat.GetPath();  // It should be a file, so chop off the filename
    SetWorkingDirectory(newdir);
  }

wxString ParsedCommand =  ParseCommand(ToolArray[index-SHCUT_TOOLS_LAUNCH]->Filepath);
if (ParsedCommand.empty()) { SetWorkingDirectory(originalfilepath); return; }  // Probably because of a malformed parameter
command += ParsedCommand;

MyProcess* proc = new MyProcess(command, ToolArray[index-SHCUT_TOOLS_LAUNCH]->Label, UseTerminal,
                                        ToolArray[index-SHCUT_TOOLS_LAUNCH]->RefreshPane, ToolArray[index-SHCUT_TOOLS_LAUNCH]->RefreshOpposite);

if (ToolArray[index-SHCUT_TOOLS_LAUNCH]->AsRoot)
  { int already = _CheckForSu(ParsedCommand);               // Check that the user didn't 'helpfully' provide su -c/sudo already
    if (already == 1)                                       // 2 means he did; 0 means 'su' or similar; assume he knows what he's doing: it's only a UDC after all
      { if (USE_SUDO) command = wxT("sudo ") + command;
         else command = wxT("su -c \"") + command + wxT('\"');
      }
    
    wxArrayString output, errors;
    long ret = ExecuteInPty(command, output, errors);
    if (ret != 0)                                           // If there was a problem, there's no terminal to show any error messages so do it here
      { wxString msg(wxString::Format(wxT("Execution of '%s' failed"), command.c_str()));
        if (errors.GetCount()) msg << wxT(":\n");
        for (size_t n=0; n < wxMin(errors.GetCount(), 7); ++n)
          { wxString line = errors.Item(n);
            if (line.empty() || (line == wxT('\n')) || (line == wxT('\r'))) continue;
            if (line.Left(1) != wxT('\n') && (line.Left(1) != wxT('\r')))
              msg << wxT('\n');
            msg << line;
          }
        wxMessageBox(msg, wxT("4Pane"), wxICON_ERROR | wxOK, active);
      }
     else proc->OnTerminate(0, 0);                          // ExecuteInPty doesn't use MyProcess, but hijack its pane-refreshing code :)
  }
 else
    wxExecute(command, wxEXEC_ASYNC, proc);

LastCommandNo = index;                                      // Store this command, in case we want to repeat it

wxString lastlabel(_("Repeat:  "));                         // Alter the Repeat Last Command menu entry to display the actual command
lastlabel += ToolArray[index-SHCUT_TOOLS_LAUNCH]->Label;
MyFrame::mainframe->GetMenuBar()->SetLabel(SHCUT_TOOLS_REPEAT, lastlabel);

SetWorkingDirectory(originalfilepath);
}

void LaunchMiscTools::RepeatLastCommand()
{
if ((int)GetLastCommand() != -1) Run(GetLastCommand());
}

enum LMT_itemtype { LMTi_file, LMTi_dir, LMTi_any };  // Only a file, only a dir, don't care

wxString GetFirstItem(const MyGenericDirCtrl* pane, LMT_itemtype type)  // Helper for LaunchMiscTools::ParseCommand
{
wxString item = pane->GetPath();
if (item.empty() || type == LMTi_any) return item;

if ((type == LMTi_file) && (wxFileExists(item))) return item;
if ((type == LMTi_dir) && (wxDirExists(item))) return item;

wxArrayString fileselections;                     // If the top selection doesn't match the required type, try any other selections
pane->GetMultiplePaths(fileselections);
for (size_t n=0; n < fileselections.GetCount(); ++n)
  { item = fileselections[n];
    if ((type == LMTi_file) && (wxFileExists(item))) return item;
    if ((type == LMTi_dir) && (wxDirExists(item))) return item;
  }

item = wxT(""); return item;
}

bool LaunchMiscTools::SetCorrectPaneptrs(const wxChar* &pc, MyGenericDirCtrl* &first, MyGenericDirCtrl* &second)
{
MyGenericDirCtrl* inactive = active->GetOppositeEquivalentPane();
switch (*pc)
  { case wxT('p'):                  first = active; return true;  // Just a parameter request, but set 'first' as null flags failure

    case wxT('A'): first = active; ++pc; return true;
    case wxT('I'): first = active->GetOppositeEquivalentPane(); ++pc; return (first != NULL);
    case wxT('1'): first = leftdv; ++pc; return true;
    case wxT('2'): first = leftfv; ++pc; return true;
    case wxT('3'): first = rightdv; ++pc; return true;
    case wxT('4'): first = rightfv; ++pc; return true;
    case wxT('5'): first = (active->fileview == ISLEFT) ? active:active->partner; ++pc; return true; // The active dirview
    case wxT('6'): first = (inactive->fileview == ISLEFT) ? inactive:inactive->partner; ++pc; return true; // The inactive dirview
    case wxT('7'): first = (active->fileview == ISLEFT) ? active->partner:active; ++pc; return true; // The active fileview
    case wxT('8'): first = (inactive->fileview == ISLEFT) ? inactive->partner:inactive; ++pc; return true; // The inactive fileview

    case wxT('b'): second = active->GetOppositeEquivalentPane();      // Then fall thru to:
    case wxT('s'): case wxT('f'):
    case wxT('d'): case wxT('a'): first = active; return true;
     default:      wxCHECK_MSG(false, false, wxT("Invalid parameter in a user-defined tool"));
  }
}

wxString LaunchMiscTools::ParseCommand(wxString& command) // Goes thru command, looking for parameters to replace
{                                                         // A cut-down, much-amended version of wxFileType::ExpandCommand()
if (command.IsEmpty())    return command;
MyTab* tab = MyFrame::mainframe->GetActiveTab(); if (!tab)  return command; // for want of something more sensible
active = tab->GetActivePane(); if (!active)  return command;      // May be fileview or dirview

// Later, we'll use single quotes around each parameter to avoid problems with spaces etc. in filepaths
// However this CAUSES a problem with apostrophes!  The global function EscapeQuoteStr(filepath) goes thru filepath, Escaping any it finds

// Possible parameters are:
//    %s = active pane's selected file/dir, %a = multiple ditto, %f = file only, %d = dir only, %b = 1 selection from EACH twinpane,
//    %{1-4}{s,a,f,d} gets the whatever from left/top dirview, fileview, right/bottom ditto; %{A,I}{s,a,f,d} designate 'Active' or 'Inactive'
//    %{5-8}{s,a,f,d} gets the whatever from active dirview, inactive dirview, active fileview, inactive fileview
//    %p = ask user to enter string parameter

wxString str;
leftfv = tab->LeftorTopFileCtrl; rightfv = tab->RightorBottomFileCtrl; leftdv = tab->LeftorTopDirCtrl; rightdv = tab->RightorBottomDirCtrl;

int pcount = 0, countdown = 0;                                    // How many %p's does the command contain? So that we can later say "Enter the *second* parameter" etc
const wxString message[] = { wxT(""), _("first"), _("other"), _("next"), _("last") }; enum indx { empty, indx_first, other, next, last };
for (const wxChar *pc = command.c_str(); *pc != wxT('\0'); ++pc)  // Have a preliminary look thru command, counting the %p's.
  { if (*pc != wxT('%'))  continue;
    if (*++pc == wxT('p')) { ++pcount; continue; }
  }
countdown = pcount;

for (const wxChar *pc = command.c_str(); *pc != wxT('\0'); ++pc)  // Go thru command, looking for "%"s
  { if (*pc != wxT('%')) { str << *pc; continue; }                // If it wasn't '%', copy the char
      
    if (*++pc == wxT('%')) { str << *pc; continue; }              // If the next char is another %, ie %% means escaped %, just copy it
    if (!wxIsalnum(*pc)) { str << *pc; continue; }                // If it's not alnum, we're in an undefined situation; just copy it anyway
    
    const wxChar modifiers[] = wxT("12345678AI");                 // Sanity-check any modifiers
    if (wxStrchr(modifiers, *pc) != NULL)
      { if (*(pc+1) == wxT('b'))
          { wxLogWarning(_("Malformed user-defined command: using the modifier '%c' with 'b' doesn't make sense. Aborting"), *pc);
            wxString invalid; return invalid;
          }
        if (wxStrchr(modifiers, *(pc+1)) != NULL)
          { wxLogWarning(_("Malformed user-defined command: you can use only one modifier per parameter. Aborting"));
            wxString invalid; return invalid;
          }
        if (wxStrchr(wxT("sfda"), *(pc+1)) == NULL)
          { wxLogWarning(_("Malformed user-defined command: the modifier '%c' must be followed by 's','f','d' or'a'. Aborting"), *pc);
            wxString invalid; return invalid;
          }
      }

    MyGenericDirCtrl *first = NULL, *second = NULL;
    if (!SetCorrectPaneptrs(pc, first, second) || !first)         // Sort out which pane(s) to use, and cope with modifiers
        { wxString invalid; return invalid; }

    if (*pc == wxT('s'))                                          // See if next char is 's'.  If so, just replace it with correct pane's filepath
          str << EscapeSpaceStr(first->GetPath()); 
    
     else
      if (*pc == wxT('a'))                                        // 'a' means all the pane's selections
        { wxArrayString selections;
          first->GetMultiplePaths(selections);
          for (int n=0; n < (int)selections.GetCount() - 1; ++n)
            str << EscapeSpace(selections[n]) << wxT(' ');        // Append a space to all but the last
          if (selections.GetCount() > 0)                          // Then do the last (if there _was_ a last)
            str << EscapeSpace(selections[selections.GetCount() - 1]);
        }
      
      else  
       if (*pc == wxT('f') || *pc == wxT('b'))                    // 'f' means one of the active fileview's selections.  'b' means one from inactive too
        { wxString item = GetFirstItem(first, (*pc == wxT('f')) ? LMTi_file : LMTi_any);
          if (!item.IsEmpty()) str << EscapeSpace(item);          // Add the item, assuming we found an appropriate one
        }
        
      if ((*pc == wxT('b')) && second)                            // 'b' means one each from active & inactive pane.  We got the active one above
        { wxString item = GetFirstItem(second, LMTi_any);         //   so now repeat for the INactive one
          if (!item.IsEmpty()) str << wxT(' ') << EscapeSpace(item); // Add the 2nd item, with a preceding blank
        }
      
      else  
      if (*pc == wxT('d'))                                        // 'd' means one dir
        { wxString DefinitelyADir;
          if (first->fileview == ISLEFT)  DefinitelyADir = first->GetPath();  // If the active pane is a dirview, this should always work
           else                                                  // Otherwise see if there's a dir in the fileview selections
              DefinitelyADir = GetFirstItem(first, LMTi_dir);
              
          if (!DefinitelyADir.IsEmpty()) str << EscapeSpace(DefinitelyADir);  // Add the dir if it exists
        }

     else
      if (*pc == wxT('p'))                                       // 'p' means ask the user for an input string
        { wxASSERT(pcount && countdown);                         // We shouldn't get here if either is 0
          indx index; wxString msg(_("Please type in the"));     // First construct a message string, dependant on previous/subsequent %p's
          if (countdown == pcount ) index = (pcount>1 ? indx_first : empty);
            else if (countdown == 1 && pcount == 2)  index = other;
              else if (countdown == 1 && pcount > 2)  index = last;
                else  index = next;
          msg << wxT(" ") << message[index] << wxT(" ") << _("parameter"); --countdown;  // Having constructed the message, dec countdown
          wxString param = wxGetTextFromUser(msg);
          if (!param.IsEmpty())   str << param;
        }
  }

return str;    
}

void MyProcess::OnTerminate(int WXUNUSED(pid), int status)
{
if (!m_UseTerminal)   // There's no point doing this if the user can see the output anyway, and wxEXEC_ASYNC so the timing is wrong
  { if (!status)      // Should be zero for success
      BriefLogStatus bls(m_toolname + wxT(" ") + _("ran successfully"));
     else
      { wxCommandEvent event(myEVT_BriefMessageBox, wxID_ANY);
        event.SetString(_("User-defined tool") + wxString::Format(wxT(" '%s' %s %d"),
                                  m_toolname.c_str(), _("returned with exit code"), status));

        wxPostEvent(MyFrame::mainframe, event);
      }
  }

                      // If the user asked for the pane(s) to be refreshed when the app exited, do so
if (m_RefreshPane)
  { MyGenericDirCtrl* pane = MyFrame::mainframe->GetActivePane();
    if (pane)
      { wxCommandEvent event; 
        pane->OnRefreshTree(event);
        if (m_RefreshOpposite)     // If required, do the opposite pane too
          { MyGenericDirCtrl* oppositepane = pane->GetOppositeDirview();
            if (oppositepane) oppositepane->OnRefreshTree(event);
          }
      }
  }

delete this;
}


// ------------------------------------------------------------------------------------------------------------------------------

void HtmlDialog(const wxString& title, const wxString& file2display, const wxString& dialogname, wxWindow* parent /*=null*/ , wxSize size /*=wxSize(0,0)*/)    // A global function to set up a dialog with just a wxHtmlWindow & a 'Finished' button
{
if (file2display.IsEmpty()) return;

wxDialog dlg; wxXmlResource::Get()->LoadDialog(&dlg, parent, wxT("HtmlDlg"));
dlg.SetTitle(title);

wxHtmlWindow html(&dlg);
#if defined GIT_BUILD_VERSION && wxVERSION_NUMBER >= 3000
  #define STRINGIFY(x) #x
  #define TOSTRING(x) STRINGIFY(x)
  wxString gitversion( TOSTRING(GIT_BUILD_VERSION) );
  if (dialogname == wxT("AboutDlg") && !gitversion.empty())
    { wxString text, prefix(wxT(", git "));
      gitversion = prefix + gitversion + wxT("<br>");
      wxTextFile tfile(file2display);
      if (!tfile.Open())
        { wxLogDebug(wxT("Couldn't open the 'About' dialog page")); return; }
      for (size_t n=0; n < tfile.GetLineCount(); ++n)
        { wxString line = tfile.GetLine(n);
          if (line.Contains(wxT("4Pane Version")))
            line.Replace(wxT("<br>"), gitversion, false);
          text << line;
        }
      tfile.Close();
      html.SetPage(text);
    }
   else
    html.LoadPage(file2display);
#else
  html.LoadPage(file2display);
#endif

#ifdef __WXGTK__
  int fontsizes[] = { 6, 8, 10, 12, 20, 24, 32 }; 
  html.SetFonts(wxEmptyString, wxEmptyString, fontsizes);   // otherwise gtk2 displays much larger than 1.2
#endif
wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
sizer->Add(&html, 1, wxEXPAND);
dlg.GetSizer()->Prepend(sizer, 1, wxEXPAND);

if (!LoadPreviousSize(&dlg, dialogname))
  { if (size.GetWidth() == 0 && size.GetHeight() == 0)
      { int width, height; wxDisplaySize(&width, &height); size.Set(width*2/3, height*2/3); } // Set default size of 2/3 of screen
    dlg.SetSize(size); // wxHtmlWindows have no intrinsic size
  }

dlg.Centre();
dlg.GetSizer()->Layout();

dlg.ShowModal();
SaveCurrentSize(&dlg, dialogname);
}

//-----------------------------------------------------------------------------------------------------------------------
#if defined(__WXGTK__)
        // gtk2 steals keydown events for any shortcuts!  So if you use DEL as ->Trash, textctrls never get DEL keypresses.  Ditto Copy/Paste.  This is a workaround
  void EventDistributor::CheckEvent(wxCommandEvent& event)
  {
      // Start by seeing if we're actually interested in this event: we don't want to interfere with e.g. Tools > Find, or Mounting etc
  if (!((event.GetId() >= SHCUT_CUT && event.GetId() < SHCUT_RENAME)   // Cut Copy Trash Del
                     || event.GetId() == SHCUT_PASTE)) { event.Skip(); return; }

  wxWindow* win = wxWindow::FindFocus();
  if (win == NULL) { wxLogDebug(wxT("EventDistributor::ProcessEvent.FindFocus gave a null window")); event.Skip(); return; }

  if (win->GetName() == wxString(wxT("TerminalEm")))
    { ((TerminalEm*)win)->OnShortcut(event, true); return; }

  if (win->GetName() == wxString(wxT("ID_FILEPATH")))
    { ((TextCtrlBase*)win)->OnShortcut(event, false); return; }

  event.Skip();
  }

  BEGIN_EVENT_TABLE(EventDistributor, wxEvtHandler)
    EVT_MENU(wxID_ANY, EventDistributor::CheckEvent)
  END_EVENT_TABLE()

  //-----------------------------------------------------------------------------------------------------------------------

  void TextCtrlBase::OnShortcut(wxCommandEvent& event, bool ChildIsTerminalEm)  // Copes with menu shortcuts in gtk2
  {
  long IP;

  switch(event.GetId())    // First deal with the easy ones.  We likely want copy etc to use the same shortcut keys as the rest of the program
    { case SHCUT_COPY:       Copy(); return;
      case SHCUT_CUT:        Cut(); return;
      case SHCUT_PASTE:      Paste(); return;
    }

                            // Now see if there are any plain-key shortcuts which have been pre-empted for shortcuts.  eg DEL
  AccelEntry* accel = MyFrame::mainframe->AccelList->GetEntry(event.GetId());
  if (accel==NULL) { event.Skip(); return; }  // This can happen if e.g. the toolbar textctrl has focus, then a bookmark is called
  int kc = accel->GetKeyCode();

  long from, to; GetSelection(&from,  &to); bool IsSelection = to > from;  // This is used in Back and Del (twice), so do it first

  if (ChildIsTerminalEm)    // Terminal emulator or Commandline
    switch(kc)
      { case WXK_END:          SetInsertionPointEnd();  return;
        case WXK_BACK:            // Pass these to TerminalEm::OnKey()
        case WXK_DELETE:
        case WXK_HOME:        wxKeyEvent kevent(wxEVT_KEY_DOWN); 
                              kevent.m_keyCode = kc;
                              kevent.m_controlDown = kevent.m_shiftDown =  kevent.m_altDown = kevent.m_metaDown = false;
                              return ((TerminalEm*)this)->OnKey(kevent);
      }

   else
    switch(kc)            // Otherwise the toolbar filepath ctrl, so we have to do it here
      { case WXK_DELETE:      if (IsSelection)
                                { Remove(from, to); return; }
                                else 
                                { IP = GetInsertionPoint();
                                  if (IP < GetLastPosition())    Remove(IP, IP+1); return;
                                }
        case WXK_BACK:        if (IsSelection)
                                { Remove(from, to); return; }
                                else 
                                { IP = GetInsertionPoint();
                                  if (IP  > 0)    Remove(IP-1, IP); return;
                                }
        case WXK_HOME:        SetInsertionPoint(0); return;
        case WXK_END:         SetInsertionPointEnd();  return;
      }

  wxChar kcchar = (wxChar)kc;    // The other things to check for are plain keypresses like 's', in case some idiot has used one as a shortcut
  if (wxIsalnum(kcchar) && ! (wxGetKeyState(WXK_CONTROL) || wxGetKeyState(WXK_ALT)))   
    { if (wxIsalpha(kcchar))  kcchar = wxTolower(kcchar);
      *this << kcchar;  return; 
    }

  event.Skip();                                          // In case we don't deal with this one
  }
#endif // gtk2

IMPLEMENT_DYNAMIC_CLASS(TextCtrlBase,wxTextCtrl)

void TextCtrlBase::Init()
{
CreateAcceleratorTable();

Connect(GetId(), wxEVT_SET_FOCUS, wxFocusEventHandler(TextCtrlBase::OnFocus), NULL, this);

#if defined(__WXX11__)  
  SetupX();
#endif
}

void TextCtrlBase::CreateAcceleratorTable()
{
int AccelEntries[] = 
  { SHCUT_SWITCH_FOCUS_PANES, SHCUT_SWITCH_FOCUS_TERMINALEM, SHCUT_SWITCH_FOCUS_COMMANDLINE, SHCUT_SWITCH_FOCUS_TOOLBARTEXT,
#if wxVERSION_NUMBER > 2904 && defined(__WXGTK__)
    // In wx2.9.5 the issue of shortcuts like Del affecting the tree too is fixed. However the fix means we _must_ add such shortcuts here, else they aren't seen
    SHCUT_CUT, SHCUT_COPY, SHCUT_PASTE, SHCUT_DELETE,
#endif
 SHCUT_SWITCH_TO_PREVIOUS_WINDOW,
 };
const size_t shortcutNo = sizeof(AccelEntries)/sizeof(int);
MyFrame::mainframe->AccelList->CreateAcceleratorTable(this, AccelEntries, shortcutNo);
}

#if defined(__WXX11__)                        // Copy/Paste etc are not currently implemented in the wxX11 wxTextCtrl so do it the X11 way
#if wxVERSION_NUMBER >= 2900 
  #define GetMainWindow X11GetMainWindow
#endif

#include  <X11/Xatom.h>  // For XA_STRING etc

wxString TextCtrlBase::Selection;
bool TextCtrlBase::OwnCLIPBOARD = false;

void TextCtrlBase::SetupX()
{
OwnPRIMARY = false;

CLIPBOARD = XInternAtom((Display*)wxGetDisplay(), "CLIPBOARD", False);        // Get/create the atom ids
TARGETS = XInternAtom((Display*)wxGetDisplay(), "TARGETS", False);
SELECTION_PROPty = XInternAtom((Display*)wxGetDisplay(), "SELECTION_PROPty", False);
}

void TextCtrlBase::Copy()
{
Selection = GetStringSelection(); if (Selection.IsEmpty()) return;
XSetSelectionOwner((Display*)wxGetDisplay(), CLIPBOARD, (Window)GetMainWindow(),  CurrentTime);  // Get CLIPBOARD ownership

OwnCLIPBOARD = XGetSelectionOwner((Display*)wxGetDisplay(), CLIPBOARD) == (Window)GetMainWindow();  // Set the bool if it worked
}

void TextCtrlBase::Cut()
{
long from, to; GetSelection(&from, &to);
if (from == to) return;                         // Nothing selected

Copy();                                         // Copy the highlit text, & replace it with ""
Replace(from, to, wxT(""));
}


void TextCtrlBase::Paste()
{
if (OwnCLIPBOARD) DoThePaste(Selection);       // If the clipboard data's ours anyway, no point bothering the X server
  else
  XConvertSelection((Display*)wxGetDisplay(), CLIPBOARD, XA_STRING, SELECTION_PROPty, (Window)GetMainWindow(), CurrentTime);
}

void TextCtrlBase::DoThePaste(wxString& str)
{
if (str.IsEmpty()) return;

long from, to; GetSelection(&from, &to);
if (from != to)
  { Replace(from, to, str); return; }           // If there's highlit text, replace it with the pasted 'selection'

WriteText(str);                                 // Otherwise just write
}

void TextCtrlBase::DoPrimarySelection(bool select)
{
if (select)
  XSetSelectionOwner((Display*)wxGetDisplay(), XA_PRIMARY, (Window)GetMainWindow(),  CurrentTime);
 else if (OwnPRIMARY)  // If we no longer have a selection, relinquish ownership by passing the property of None
            XSetSelectionOwner((Display*)wxGetDisplay(), XA_PRIMARY, None,  CurrentTime);

OwnPRIMARY = XGetSelectionOwner((Display*)wxGetDisplay(), XA_PRIMARY) == (Window)GetMainWindow();  // Set the bool if it worked
}

void TextCtrlBase::OnMiddleClick(wxMouseEvent& event)
{
XConvertSelection((Display*)wxGetDisplay(), XA_PRIMARY, XA_STRING, SELECTION_PROPty, (Window)GetMainWindow(), CurrentTime);
}

void TextCtrlBase::OnLeftUp(wxMouseEvent& event)
{
event.Skip();

long from, to; GetSelection(&from, &to);        // If there's a selection, we must have been dragging to cause it. Otherwise a click will have unselected
DoPrimarySelection(from != to);                 // Either way, tell X
}

void TextCtrlBase::OnCtrlA()
{ 
SetSelection(0, GetLastPosition());
long from, to; GetSelection(&from, &to);        // It's just possible that someone will have done Ctrl-A when there was no text!
DoPrimarySelection(from != to);
}

bool TextCtrlBase::OnXevent(XEvent& xevent)
{
switch (xevent.type)
  { case SelectionClear: if (xevent.xselectionclear.selection==XA_PRIMARY)  // If it's a PRIMARY clear, unselect any text
                          { SetSelection(GetInsertionPoint(), GetInsertionPoint()); OwnPRIMARY = false; }
                          else if (xevent.xselectionclear.selection==CLIPBOARD)
                           { Selection.Empty(); OwnCLIPBOARD = false; }         // Similarly for a CLIPBOARD clear
                        return true;
  
    case SelectionRequest:{int len = 0;                                         // Someone wants our selection, 

                        if (xevent.xselectionrequest.target == XA_STRING)       //  in ISO Latin-1 character set form
                          { wxCharBuffer cbuf; wxString str;
                            if (xevent.xselectionrequest.selection==CLIPBOARD && ! Selection.IsEmpty())
                              str = Selection;
                             else if (xevent.xselectionrequest.selection==XA_PRIMARY)
                                str = GetStringSelection();
                            len = str.Len();
                            cbuf.extend(len); memcpy(cbuf.data(), str.mb_str(), len);
                                      // Send the requested info to the requestor's window
                            if (len) XChangeProperty((Display*)wxGetDisplay(), xevent.xselectionrequest.requestor, xevent.xselectionrequest.property,
                                         xevent.xselectionrequest.target,  8, PropModeReplace, (const unsigned char*)cbuf.data(), len);
                          }
                        else if (xevent.xselectionrequest.target == TARGETS)  // If we're just being asked what targets we support, return the info here
                          { Atom target = XA_STRING; len = 1;
                            XChangeProperty((Display*)wxGetDisplay(), xevent.xselectionrequest.requestor, xevent.xselectionrequest.property,
                                         xevent.xselectionrequest.target,  8, PropModeReplace, (unsigned char*)&target, len);
                          }  
                              // else we don't do anything, so len==0 and therefore below we'll set NotifyEvent.xselection.property to None
                        XEvent NotifyEvent;                            // Time to confess what we've done
                        NotifyEvent.type = SelectionNotify; NotifyEvent.xselection.requestor = xevent.xselectionrequest.requestor;
                        NotifyEvent.xselection.selection = xevent.xselectionrequest.selection; NotifyEvent.xselection.target = xevent.xselectionrequest.target;
                        NotifyEvent.xselection.time = xevent.xselectionrequest.time;
                        if (len) NotifyEvent.xselection.property = xevent.xselectionrequest.property;  // Success == len>0, so return the provided property
                         else NotifyEvent.xselection.property = None;                          // In the event of failure, don't

                        XSendEvent((Display*)wxGetDisplay(), NotifyEvent.xselection.requestor, False, 0l, &NotifyEvent);
                        return true;}
  

    case SelectionNotify: if (xevent.xselection.property == None) return true;      // Oops
                        Atom actual_type; int actual_format; unsigned long nitems, remaining;
                        unsigned char* data; static wxCharBuffer cbuf; static long bufPos = 0;
                        
                        if (XGetWindowProperty((Display*)wxGetDisplay(), xevent.xselection.requestor, xevent.xselection.property,
                          bufPos / 4, 65536, True, AnyPropertyType, &actual_type, &actual_format, &nitems, &remaining, &data) != Success) return true;
                        
                        size_t length = nitems * actual_format / 8;
                        if (bufPos == 0) cbuf.reset();                          // Kill any stale bytes, providing we're not 1/2-way thru a download
                        if (!cbuf.extend(bufPos + length - 1)) return true;  // and make room for the available amount
                        memcpy(cbuf.data() + bufPos, data, length);              //  then add the available data, appending to any previously loaded
                        bufPos += length; XFree(data);
                        if ( ! remaining)
                          { if (bufPos) 
                              { wxString str(cbuf, *wxConvCurrent, bufPos);     // Finished, so use the new data and reset
                                DoThePaste(str);
                              }
                            cbuf.reset(); bufPos = 0;
                          }                               // If not, leave the partial data waiting for further events
                        return true;
  }
return false;
}

BEGIN_EVENT_TABLE(TextCtrlBase, wxTextCtrl)
  EVT_MIDDLE_DOWN(TextCtrlBase::OnMiddleClick)
  EVT_LEFT_UP(TextCtrlBase::OnLeftUp)
END_EVENT_TABLE()

#endif // defined(__WXX11__)

void TextCtrlBase::OnTextFilepathEnter(wxCommandEvent& WXUNUSED(event))
{
MyFrame::mainframe->TextFilepathEnter();
}

void TextCtrlBase::OnFocus(wxFocusEvent& event)
{
wxGetApp().GetFocusController().SetCurrentFocus(this);
event.Skip();
}
