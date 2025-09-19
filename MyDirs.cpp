/////////////////////////////////////////////////////////////////////////////
// Name:       MyDirs.cpp
// Purpose:    Dir-view
// Part of:    4Pane
// Author:     David Hart
// Copyright:  (c) 2020 David Hart
// Licence:    GPL v3
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h" 

#include "wx/log.h"
#include "wx/app.h"
#include "wx/frame.h" 
#include "wx/scrolwin.h"
#include "wx/menu.h"
#include "wx/dirctrl.h"
#include "wx/dcmemory.h"
#include "wx/dragimag.h"
#include "wx/config.h"
#include "wx/splitter.h"
#include "wx/file.h"
#include <wx/ffile.h>


#include "MyTreeCtrl.h"
#include "MyFiles.h"
#include "Filetypes.h"
#include "Accelerators.h"
#include "Redo.h"
#include "MyDirs.h"
#include "MyFrame.h"
#include "Configure.h"
#include "Misc.h"

#include "bitmaps/include/fulltree.xpm"
#include "bitmaps/include/up.xpm"      

#if defined __WXGTK__
  #include <gtk/gtk.h>
#endif

class MakeLinkDlg  :  public wxDialog
{
public:
~MakeLinkDlg();
void init(bool multiple, bool dirsISQ);

wxStaticText* frompath;
wxStaticText* topath;
wxTextCtrl* LinkExt;
wxTextCtrl* LinkFilename;
wxCheckBox* MakeRelative;
wxCheckBox* ApplyToAll;
bool CanApply2All;
bool samedir;
long lastradiostation;

protected:
void OnLinkRadioButton(wxCommandEvent& event);     // If choose-name radio clicked
void OnRelativeCheckUI(wxUpdateUIEvent& event);
void OnSkip(wxCommandEvent& WXUNUSED(event)){ EndModal(XRCID("Skip")); }

wxRadioBox* Linktype;
wxString lastext;
DECLARE_EVENT_TABLE()
};


void MakeLinkDlg::init(bool multiple, bool dirsISQ)
{
CanApply2All = multiple; samedir = dirsISQ;
frompath = (wxStaticText*)InsertEllipsizingStaticText("placeholder", FindWindow("link_frompath"), 1);
topath = (wxStaticText*)InsertEllipsizingStaticText("placeholder", FindWindow("link_path"), 1);
LinkExt = (wxTextCtrl*)FindWindow(wxT("link_ext"));
LinkFilename = (wxTextCtrl*)FindWindow(wxT("link_filename"));
Linktype = (wxRadioBox*)FindWindow(wxT("link_radiobox"));
MakeRelative = (wxCheckBox*)FindWindow(wxT("MakeRelCheck"));
ApplyToAll = (wxCheckBox*)FindWindow(wxT("ApplyToAllCheck")); ApplyToAll->Enable(CanApply2All);

wxConfigBase::Get()->Read(wxT("History/MakeLinkDlg/Ext"), &lastext, wxString(wxT(""))); LinkExt->SetValue(lastext);
if (CanApply2All)
  { bool b(true); wxConfigBase::Get()->Read(wxT("History/MakeLinkDlg/ApplyToAll"), &b);
    ApplyToAll->SetValue(b);
  }

lastradiostation = wxConfigBase::Get()->Read(wxT("History/MakeLinkDlg/Filename"), 0l);
if (samedir && lastradiostation==0) lastradiostation = 1;       // If source & dest dirs are the same, we can't retain the same name

bool rel; wxConfigBase::Get()->Read(wxT("History/MakeLinkDlg/MakeRelative"), &rel, false); MakeRelative->SetValue(rel);

wxButton* skip = (wxButton*)FindWindow(wxT("Skip"));
if (skip) skip->Show(multiple);                                 // Show the Skip button only if there are multiple targets

int ID;
switch(lastradiostation)    // How did we last make a name for the link:  reuse the original, add an ext, or bespoke?
  { case 0: ((wxRadioButton*)FindWindow(wxT("NameSame")))->SetValue(true); ID=XRCID("NameSame"); break;
    case 1: ((wxRadioButton*)FindWindow(wxT("NameExt")))->SetValue(true); ID=XRCID("NameExt");  break;
    default: ((wxRadioButton*)FindWindow(wxT("NameName")))->SetValue(true); ID=XRCID("NameName"); 
  }

wxCommandEvent event; event.SetId(ID); OnLinkRadioButton(event);  // Fake an event to get the UpdateUI right
}

MakeLinkDlg::~MakeLinkDlg()
{
wxConfigBase::Get()->Write(wxT("History/MakeLinkDlg/Filename"), lastradiostation);
if (!LinkExt->GetValue().IsEmpty())  wxConfigBase::Get()->Write(wxT("History/MakeLinkDlg/Ext"), LinkExt->GetValue());
if (MakeRelative->IsEnabled()) wxConfigBase::Get()->Write(wxT("History/MakeLinkDlg/MakeRelative"), MakeRelative->IsChecked());
if (ApplyToAll->IsEnabled())   wxConfigBase::Get()->Write(wxT("History/MakeLinkDlg/ApplyToAll"), ApplyToAll->IsChecked());
}

void MakeLinkDlg::OnLinkRadioButton(wxCommandEvent& event)  // UI if name-type radiobuttons clicked in Make Link dialog
{
if (event.GetId() != XRCID("NameName"))                             // Do buttons 1 & 2 first
  { bool Ext = (event.GetId() == XRCID("NameExt"));
    if (samedir && !Ext)
      { BriefMessageBox msg(_("You can't link within a directory unless you alter the link's name."), AUTOCLOSE_FAILTIMEOUT / 1000, _("Not possible"));
        ((wxRadioButton*)FindWindow(wxT("NameName")))->SetValue(true); return;
      }
    LinkFilename->Disable(); LinkExt->Enable(Ext); lastradiostation = Ext;
    ApplyToAll->Enable(CanApply2All);  return;                      // Can ApplyToAll, providing there are >1 items
  }

LinkFilename->Enable(); LinkExt->Disable(); lastradiostation = 2;   // OK, button 3 was selected
if (LinkFilename->GetValue().IsEmpty()) LinkFilename->SetValue(frompath->GetLabel().AfterLast(wxFILE_SEP_PATH)); // Use the original name as template for the new one
ApplyToAll->Disable();                                              // Can't ApplyToAll, even if there are >1 items
}

void MakeLinkDlg::OnRelativeCheckUI(wxUpdateUIEvent& event)
{
if (Linktype) MakeRelative->Enable(Linktype->GetSelection());   // This enables the button only if it's a symlink that is going to be made
}

BEGIN_EVENT_TABLE(MakeLinkDlg,wxDialog)
  EVT_RADIOBUTTON(wxID_ANY, MakeLinkDlg::OnLinkRadioButton)
  EVT_BUTTON(XRCID("Skip"), MakeLinkDlg::OnSkip)
  EVT_UPDATE_UI(XRCID("MakeRelCheck"), MakeLinkDlg::OnRelativeCheckUI)
END_EVENT_TABLE()

//-----------------------------------------------------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(MyDropdownButton, wxBitmapButton)

void MyDropdownButton::OnButtonClick(wxCommandEvent& event)
{
int width, height;
GetClientSize(&width, &height);                             // How big are we?
wxPoint clientpt, screenpt = wxGetMousePosition();          // Get screen position
clientpt = ScreenToClient(screenpt);                        // & from that get the position within the button
screenpt = ((wxToolBar*)GetParent())->ScreenToClient(screenpt);   // & from that get the position within the button

left = screenpt.x - clientpt.x;                             // The left of the button is screen_pos - position_within_button
right = left + width;                                       // The right is left + width

left -= partnerwidth;                                       // For left dirviews, we actually want the left edge of the PREVIOUS button

event.Skip();
}

BEGIN_EVENT_TABLE(MyDropdownButton, wxBitmapButton)
  EVT_BUTTON(-1, MyDropdownButton::OnButtonClick)
END_EVENT_TABLE()


#if wxVERSION_NUMBER < 2900
  DEFINE_EVENT_TYPE(PasteThreadType)
  DEFINE_EVENT_TYPE(PasteProgressEventType)
#else
  wxDEFINE_EVENT(PasteThreadType, PasteThreadEvent);
  wxDEFINE_EVENT(PasteProgressEventType, wxCommandEvent);
#endif

void PastesCollector::Add(const wxString& origin, const wxString& dest, const wxString& originaldest, bool precreated/*=false*/)
{
wxString fordeletion;
if (GetIsMoves())
  fordeletion = origin; // For successful moves, we need to store this separately; it'll be the filepath to delete when the thead completes

wxString trash;
PasteThreadSuperBlock* tsb = dynamic_cast<PasteThreadSuperBlock*>(m_tsb); wxCHECK_RET(tsb, wxT("Wrong sort of threadblock"));
if (!tsb->GetTrashdir().empty())
  { if (tsb->GetUnRedoType() == URT_notunredo) // For an original Paste, a non-empty trashdir flags an overwriting situation & dest must be moved to trash before it's clobbered
      { wxString trashdir = tsb->GetTrashdir();
        trash = trashdir + dest.AfterLast(wxFILE_SEP_PATH);
      }
  }

if (tsb->GetUnRedoType() == URT_undo)
  trash = originaldest; // Or, if we're undoing, originaldest will hold any filepath that was originally moved to trash. It needs 2b retrieved after the undo

m_PasteData.push_back( PasteData(origin, dest, fordeletion, trash, precreated) );
}

void PastesCollector::StartThreads()
{
wxCHECK_RET(m_tsb, wxT("Passed a NULL superblock"));
wxCHECK_RET(!ThreadsManager::Get().PasteIsActive(), wxT("Trying to start a paste thread when there's already one active"));

if (!GetCount()) // We must be here just to cause the 'success' message to display in a non-thread situation
  { m_tsb->OnCompleted(); return; }

size_t ThreadsToUse = wxMin(ThreadsManager::GetCPUCount(), GetCount()); // How widely can/should we spread the load?
size_t count = GetCount() / ThreadsToUse;
bool overflow = (GetCount() % ThreadsToUse) > 0;
size_t fullcount = ThreadsToUse - overflow; // The no of threads that will have 'count' items passed to them

wxCriticalSectionLocker locker(ThreadsManager::Get().GetPasteCriticalSection());

int threadID(wxNOT_FOUND);
ThreadBlock* block = ThreadsManager::Get().AddBlock(ThreadsToUse, wxT("paste"), threadID, m_tsb);
wxCHECK_RET(block, wxT("Failed to create a ThreadBlock"));
wxCHECK_RET(threadID != wxNOT_FOUND, wxT("Invalid threadID"));

 // Now's a good time to start the PasteThreadStatuswriter timer. Much earlier and any hold-up e.g. ?overwrite, results in the throbber showing '..*... 0 bytes'
static_cast<PasteThreadSuperBlock*>(m_tsb)->StartThreadTimer();

 // If the itemcount is exactly divisible by the cpu count, that's fine. Otherwise the last thread gets its normal quota plus the extras
size_t p=0; wxULongLong totalsize=0;
for (size_t n=0; n < fullcount; ++n)
  { std::vector<PasteData> pastedata;
    for (size_t c=0; c < count; ++c, ++p)
      { pastedata.push_back(m_PasteData.at(p));
        FileData fd(m_PasteData.at(p).origin);
        if (fd.IsValid()) totalsize += fd.Size();
      }
    DoStartThread(pastedata, block, threadID++);
  }
if (overflow)    
  { std::vector<PasteData> pastedata;
    for (; p < m_PasteData.size(); ++p)
      { pastedata.push_back(m_PasteData.at(p));
        FileData fd(m_PasteData.at(p).origin);
        if (fd.IsValid()) totalsize += fd.Size();
      }
    DoStartThread(pastedata, block, threadID++);
  }
static_cast<PasteThreadSuperBlock*>(m_tsb)->SetTotalExpectedSize(totalsize);
}

void PastesCollector::DoStartThread(const std::vector<PasteData>& pastedata, ThreadBlock* block, int threadID)
{
enum wxThreadError error(wxTHREAD_NO_ERROR);

PasteThread* thread = new PasteThread(MyFrame::mainframe, threadID, pastedata, GetIsMoves());
thread->SetUnRedoType(m_tsb->GetUnRedoType());

#if wxVERSION_NUMBER < 2905
  error = thread->Create();
  if (error != wxTHREAD_NO_ERROR) wxLogDebug(wxT("Can't create thread!"));
   else
     error = thread->Run();
#else
  error = thread->Run(); // >2.9.5 Run() calls Create() itself
#endif //wxVERSION_NUMBER < 2905

if (error == wxTHREAD_NO_ERROR)
  block->SetThreadPointer(threadID, thread); // All is well, so store the thread, in case it needs to be interrupted
 else
  { wxLogDebug(wxT("Can't create thread!")); // So do it the original, non-thread way
    size_t successes(0), failures(0);
    for (size_t n=0; n < pastedata.size(); ++n)
      if (wxCopyFile(pastedata.at(n).origin, pastedata.at(n).dest, false))
        { if (!pastedata.at(n).del.empty())
            { wxFileName fn(StripSep(pastedata.at(n).del)); // If we're here this is a Move, so delete the original
              MyGenericDirCtrl::ReallyDelete(&fn);
            }
          
          UnexecuteImages(pastedata.at(n).dest); ++successes;
        }
       else ++failures;

    m_tsb->AddSuccesses(successes); m_tsb->AddFailures(failures);
  }
}

void* PasteThread::Entry()
{
wxCHECK_MSG(m_caller && m_PasteData.size(), NULL, wxT("Passed dud parameters"));

for (size_t n=0; n < m_PasteData.size(); ++n)
  { if (ProcessEntry(m_PasteData.at(n)))
      m_successfulpastes.Add(m_PasteData.at(n).origin);
  }
return NULL;
}

bool PasteThread::ProcessEntry(const PasteData& data)
{
if (data.precreated)
  return true;
  
wxString origin = data.origin, dest = data.dest, trash = data.overwrite;
FileData orig(origin);  // Check the origin filepath still exists i.e. nothing deleted it before we got here
if (!orig.IsValid()) return false;

  // Start by doing anything needed before the real Paste
if (m_fromunredo == URT_notunredo)
  { if (!trash.empty()) // If we're overwriting a file, save it to the trashcan first, then delete it before the paste happens
      { // First check if we _need_ to do this; it will usually have happened outside the thread, in CheckDupRen::FileAdjustForPreExistence
        FileData exists(dest);
        if (exists.IsValid())
          { if (CopyFile(dest, trash)) 
              { wxRemoveFile(dest);
                m_needsrefreshes.Add(dest); // Mark for refresh, as the FSWatcher Delete event often arrives after the Create event so the file doesn't display
              }
             else
              { wxLogDebug(wxT("Moving %s to %s either failed or was cancelled"), dest.c_str(), trash.c_str()); return false; }
          }
      }
  }
 else if (m_fromunredo == URT_redo)
  { FileData overwritten(dest); // See if there's already a destination file. If so it was an overwritten file moved to trash, then back again in Undo
    if (overwritten.IsValid())
      { wxRemoveFile(dest);     // So we need to kill it this time; no need to trashcan it as a copy still exists there
        m_needsrefreshes.Add(dest); // Mark for refresh, as the FSWatcher Delete event often arrives after the Create event so the file doesn't display
      }
  }

  // Now the main event: the Paste
bool result = CopyFile(origin, dest);
if (result) 
  { UnexecuteImages(dest);
    KeepShellscriptsExecutable(dest, orig.GetPermissions());

if (RETAIN_MTIME_ON_PASTE)
  FileData::ModifyFileTimes(dest, origin);

      // The aftermath: for a Move() we need to delete the origin
    if (m_ismoving)
      wxRemoveFile(origin); 

    if ((m_fromunredo == URT_undo) && !trash.empty()) // For an Undo we need to retrieve any overwritten file from the trashcan & put it back where it used to be
      { if (wxFileExists(origin))                     // First delete the pasted version, if it still exists
          wxRemoveFile(origin);
        CopyFile(trash, origin);                      // then replace it with the original
        m_needsrefreshes.Add(origin); // Mark for refresh, as the FSWatcher Delete event often arrives after the Create event so the file doesn't display
      }
  }

return result;
}

bool PasteThread::CopyFile(const wxString& origin, const wxString& destination)
{
static const size_t ALIQUOT(1000000);
if (TestDestroy())
  return false;

wxCHANGE_UMASK(0); // This turns off umask while in scope

FileData orig(origin);
wxULongLong filesize = orig.Size();
if (filesize.ToULong() == 0)
  return wxCopyFile(origin, destination, false); // If it's a zero-sized file, just copy it. We won't need to interrupt ;)

wxFile in(origin, wxFile::read);
if (!in.IsOpened()) return false;

wxFile out;
if (!out.Create(destination, true, orig.GetPermissions()) ) return false;
if (!out.IsOpened())
  { return false; }

char buffer[ALIQUOT + 1];
wxULongLong total(0);
while (true)
  { if (TestDestroy())
      { wxLogNull shh;  // We've been aborted, so remove any partial file and exit
        wxRemoveFile(destination); 
        return false; 
      }
    size_t read = in.Read(buffer, ALIQUOT);
    if (!read)
      return (total == filesize);

    size_t written = out.Write(buffer, read);
    if (written < read)
      return false;

    wxCommandEvent event(PasteProgressEventType, m_ID); // Report progress to date; for files < ALIQUOT that will be just once
    event.SetInt(read);
    wxPostEvent(MyFrame::mainframe, event);

    total += read;
    if (total >= filesize)
      break;
  }

return true;
}

void PasteThread::OnExit()
{
wxCriticalSectionLocker locker(ThreadsManager::Get().GetPasteCriticalSection());
ThreadsManager::Get().SetThreadPointer(m_ID, NULL);

wxArrayString deletes;
for (size_t n=0; n < m_PasteData.size(); ++n)
   deletes.Add( m_PasteData.at(n).del );

if (m_caller)
  { PasteThreadEvent event(PasteThreadType, m_ID);
    event.SetSuccesses(m_successfulpastes);
    event.SetArrayString(deletes);
    event.SetRefreshesArrayString(m_needsrefreshes);
    wxPostEvent(m_caller, event);
  }
}


//-----------------------------------------------------------------------------------------------------------------------

DirGenericDirCtrl::DirGenericDirCtrl(wxWindow* parent, const wxWindowID id, const wxString& START_DIR , const wxPoint& pos, const wxSize& size,
                                     long style, bool full_tree, const wxString& name)
      :  MyGenericDirCtrl(parent, (MyGenericDirCtrl*)this, id, START_DIR ,  pos,  size ,  style | wxDIRCTRL_DIR_ONLY, wxT(""), 0, name, ISLEFT, full_tree)
{
toolBar = NULL;
SelectedCumSize = 0;
CreateAcceleratorTable();

if (full_tree)
  Bind(wxEVT_IDLE, &DirGenericDirCtrl::OnIdle, this); // In full-tree mode we can't calculate how far to scroll to centre the selected dir until the tree (& 4Pane) is sensibly sized
}

/*
enum
{
    wxACCEL_NORMAL  = 0x0000,   // no modifiers
    wxACCEL_ALT     = 0x0001,   // hold Alt key down
    wxACCEL_CTRL    = 0x0002,   // hold Ctrl key down
    wxACCEL_SHIFT   = 0x0004    // hold Shift key down
};
*/

DirGenericDirCtrl::~DirGenericDirCtrl()
{ 
}

void DirGenericDirCtrl::OnIdle(wxIdleEvent& WXUNUSED(event))
{
// This is needed only in full-tree mode, where we can't calculate how far to scroll to centre the selected dir until the tree (& 4Pane) is sensibly sized
if (GetTreeCtrl()->GetSize().y >50)
  { 
    DoScrolling();
    Unbind(wxEVT_IDLE, &DirGenericDirCtrl::OnIdle, this);

  }
}

void DirGenericDirCtrl::CreateAcceleratorTable()
{
int AccelEntries[] = {  SHCUT_CUT, SHCUT_COPY, SHCUT_PASTE, SHCUT_PASTE_DIR_SKELETON, SHCUT_HARDLINK, SHCUT_SOFTLINK,
                        SHCUT_TRASH,SHCUT_DELETE, SHCUT_RENAME, SHCUT_NEW, SHCUT_UNDO, SHCUT_REDO, IDM_TOOLBAR_fulltree,
                        SHCUT_REFRESH, SHCUT_FILTER, SHCUT_TOGGLEHIDDEN, SHCUT_PROPERTIES,
                        SHCUT_REPLICATE, SHCUT_SWAPPANES,
                        SHCUT_SPLITPANE_VERTICAL, SHCUT_SPLITPANE_HORIZONTAL, SHCUT_SPLITPANE_UNSPLIT,
                        SHCUT_SWITCH_FOCUS_OPPOSITE, SHCUT_SWITCH_FOCUS_ADJACENT, SHCUT_SWITCH_FOCUS_PANES, SHCUT_SWITCH_FOCUS_TERMINALEM,
                        SHCUT_SWITCH_FOCUS_COMMANDLINE, SHCUT_SWITCH_FOCUS_TOOLBARTEXT, SHCUT_SWITCH_TO_PREVIOUS_WINDOW,
                        SHCUT_PREVIOUS_TAB, SHCUT_NEXT_TAB,
                        SHCUT_EXT_FIRSTDOT, SHCUT_EXT_MIDDOT, SHCUT_EXT_LASTDOT,
                        SHCUT_NAVIGATE_DIR_UP, SHCUT_NAVIGATE_DIR_PREVIOUS, SHCUT_NAVIGATE_DIR_NEXT
                     };
const size_t shortcutNo = sizeof(AccelEntries)/sizeof(int);
MyFrame::mainframe->AccelList->CreateAcceleratorTable(this, AccelEntries,  shortcutNo);
}

void DirGenericDirCtrl::RecreateAcceleratorTable()
{
        // It would be nice to be able to empty the old accelerator table before replacing it, but trying caused segfaulting!
CreateAcceleratorTable();                                   // Make a new one, with up-to-date data
((MyTreeCtrl*)GetTreeCtrl())->RecreateAcceleratorTable();   // Tell the tree to do likewise
}
   
void DirGenericDirCtrl::CreateToolbar()
{
long style =  wxTB_HORIZONTAL;

DirSplitterWindow* DSW;                                     // Get ptr to the parent of the toolbar
if (isright)  DSW = ParentTab->m_splitterRightBottom;
 else  DSW = ParentTab->m_splitterLeftTop;
 
if (toolBar != NULL) toolBar->Destroy();                    // In case we're recreating

toolBar = new wxToolBar(DSW->GetDirToolbarPanel(), ID_TOOLBAR,wxDefaultPosition, wxDefaultSize, style);

wxBitmap toolBarBitmaps[2];
toolBarBitmaps[0] = wxBitmap(fulltree_xpm);                 // First the "standard" buttons
toolBarBitmaps[1] = GetBestBitmap(wxART_GO_UP, wxART_MENU, wxBitmap(up_xpm));
wxBitmapButton* backbtn = new wxBitmapButton(toolBar, XRCID("IDM_TOOLBAR_back"), GetBestBitmap(wxART_GO_BACK, wxART_MENU, wxBitmap(BITMAPSDIR + wxT("/back.xpm"))),
                                                  wxDefaultPosition, wxDefaultSize, wxBU_AUTODRAW | wxNO_BORDER);
backbtn->SetToolTip(_("Back to previous directory"));
wxBitmapButton* forwardbtn = new wxBitmapButton(toolBar, XRCID("IDM_TOOLBAR_forward"), GetBestBitmap(wxART_GO_FORWARD, wxART_MENU, wxBitmap(BITMAPSDIR + wxT("/forward.xpm"))),
                                                  wxDefaultPosition, wxDefaultSize, wxBU_AUTODRAW | wxNO_BORDER);
forwardbtn->SetToolTip(_("Re-enter directory"));

MenuOpenButton1 = new MyDropdownButton(toolBar, XRCID("IDM_TOOLBAR_smalldropdown1"), wxBitmap(BITMAPSDIR + wxT("/smalldropdown.png"), wxBITMAP_TYPE_PNG));
MenuOpenButton2 = new MyDropdownButton(toolBar, XRCID("IDM_TOOLBAR_smalldropdown2"), wxBitmap(BITMAPSDIR + wxT("/smalldropdown.png"), wxBITMAP_TYPE_PNG));
MenuOpenButton1->SetToolTip(_("Select previously-visited directory"));
MenuOpenButton2->SetToolTip(_("Select previously-visited directory"));
int width, height; backbtn->GetClientSize(&width, &height);
MenuOpenButton1->partnerwidth = width;                      // Tell the dropdown buttons how wide is their neighbour: needed correctly to position the dropdown menus
MenuOpenButton2->partnerwidth = width;

toolBar->AddTool(IDM_TOOLBAR_fulltree, wxT("Full Tree"), toolBarBitmaps[0], _("Show Full Tree"), wxITEM_CHECK);
toolBar->AddTool(IDM_TOOLBAR_up, wxT("Up"), toolBarBitmaps[1], _("Up to Higher Directory"), wxITEM_NORMAL);

toolBar->AddControl(backbtn);
toolBar->AddControl(MenuOpenButton1);
toolBar->AddControl(forwardbtn);
toolBar->AddControl(MenuOpenButton2);
toolBar->AddSeparator();

                                              // Now load any user-prefs buttons eg Home, BM1
ArrayofDirCtrlTBStructs* array = MyFrame::mainframe->configure->DirCtrlTBinfoarray;  // Readability
size_t count = wxMin(array->GetCount(), (size_t)IDM_TOOLBAR_Range);  // Check there aren't too many user-defined buttons requested
for (size_t n=0; n < count; ++n)
  { wxString label = array->Item(n)->label;
    if ((n==0) && array->Item(n)->bitmaplocation.AfterLast(('/')) == wxT("gohome.xpm")) // The first is go-home, so try to find a theme icon (unless the user has altered it already)
      toolBar->AddTool(IDM_TOOLBAR_bmfirst+n, label.empty() ? wxString(_("Home")) : label, GetBestBitmap(wxART_GO_HOME, wxART_MENU, array->Item(n)->bitmaplocation),
                                                                                                        array->Item(n)->tooltip, wxITEM_NORMAL);
     else
      { if (array->Item(n)->bitmaplocation.AfterLast(('/')) == wxT("MyDocuments.xpm")) // If Documents and the label is empty, supply a default
#if wxVERSION_NUMBER > 3105
          toolBar->AddTool(IDM_TOOLBAR_bmfirst+n, label.empty() ? wxString(_("Documents")) : label,wxBitmapBundle::FromBitmap(array->Item(n)->bitmaplocation),
                                                                                                        array->Item(n)->tooltip, wxITEM_NORMAL);
         else                                                                                                    
          toolBar->AddTool(IDM_TOOLBAR_bmfirst + n, label, wxBitmapBundle::FromBitmap(array->Item(n)->bitmaplocation), array->Item(n)->tooltip, wxITEM_NORMAL);

#else
          toolBar->AddTool(IDM_TOOLBAR_bmfirst+n, label.empty() ? wxString(_("Documents")) : label, array->Item(n)->bitmaplocation,

                                                                                                        array->Item(n)->tooltip, wxITEM_NORMAL);
         else                                                                                                    
          toolBar->AddTool(IDM_TOOLBAR_bmfirst + n, label, array->Item(n)->bitmaplocation, array->Item(n)->tooltip, wxITEM_NORMAL);
#endif //wxVERSION_NUMBER > 3105
      }
  }

#if wxVERSION_NUMBER >= 3000 && defined(__WXGTK3__) && !GTK_CHECK_VERSION(3,10,0)
  wxColour bg = toolBar->GetBackgroundColour(); // See the comment in GetSaneColour() for the explanation
  backbtn->SetBackgroundColour(bg);
  forwardbtn->SetBackgroundColour(bg);
  MenuOpenButton1->SetBackgroundColour(bg);
  MenuOpenButton2->SetBackgroundColour(bg);
#endif

toolBar->Realize();                                         // Essential to make the tools display

DSW->GetDirToolbarPanel()->GetSizer()->Add(toolBar, 1, wxEXPAND | wxTOP, 3);
DSW->GetDirToolbarPanel()->GetSizer()->Layout();            // Needed for early wx versions
}

void DirGenericDirCtrl::DoScrolling() // Called by the OnIdle handler to ScrollToMidScreen() if necessary
{
wxTreeItemId item = FindIdForPath(startdir); 
if (item.IsOk() && !GetTreeCtrl()->IsVisible(item)) 
  { GetMyTreeCtrl()->EnsureVisible(item);
    GetMyTreeCtrl()->ScrollToMidScreen(item, startdir);
  }
}

void DirGenericDirCtrl::OnDirCtrlTBButton(wxCommandEvent& event)  // When a user-defined dirctrlTB button is clicked
{
size_t buttonnumber = event.GetId() - IDM_TOOLBAR_bmfirst;  // Find which button was clicked
if (buttonnumber >= MyFrame::mainframe->configure->DirCtrlTBinfoarray->GetCount()) return;   // Sanity check

wxString path = MyFrame::mainframe->configure->DirCtrlTBinfoarray->Item(buttonnumber)->destination;
FileData fd(path); if (!fd.IsValid()) return;

if (((MyGenericDirCtrl*)this)->arcman->NavigateArchives(path) == Snafu)  return;  // This copes with both archives and ordinary filepaths

if (fulltree) // I don't grok why, but MyGenericDirCtrl::ExpandPath (inside the SetPath() call below) can't cope with paths containing symlinks in fulltree mode
  path = FileData::GetUltimateDestination(path);

wxString OriginalSelection = GetPathIfRelevant();
startdir = path; RefreshTree(path);

if (fulltree) GetTreeCtrl()->UnselectAll(); // Remove any current selection, otherwise we'll end up with the ambiguity of multiple ones
SetPath(path);
UpdateStatusbarInfo(path); AddPreviouslyVisitedDir(path, OriginalSelection);
}

void DirGenericDirCtrl::DoToolbarUI(wxUpdateUIEvent& event)  //  Greys out appropriate buttons 
{
switch(event.GetId())
  { case IDM_TOOLBAR_fulltree:  event.Check(fulltree);    // Set the display according to the bool
                              toolBar->EnableTool(IDM_TOOLBAR_fulltree,  !arcman->IsArchive()); return;  // Disable if within an archive
    case IDM_TOOLBAR_up:   event.Enable(!(fulltree || startdir.IsSameAs(wxFILE_SEP_PATH))); return; // Enable Up button if not fulltree && startdir!=root 
  }
#if !defined(__WXX11__)           // For some reason, this crashes on X11 with an invalid bitmap error message !?
  size_t count = GetNavigationManager().GetCount();
  int index = GetNavigationManager().GetCurrentIndex();
  if  (event.GetId()==XRCID("IDM_TOOLBAR_back")) 
    { bool enable = index > 0; event.Enable(enable); MenuOpenButton1->Enable(enable); return; } // Enable dir buttons together with their sidebars
  if (event.GetId()==XRCID("IDM_TOOLBAR_forward")) 
    { bool enable = (index+1) < (int)count; event.Enable(enable); MenuOpenButton2->Enable(enable); }
#endif
}

void DirGenericDirCtrl::OnFullTree(wxCommandEvent& WXUNUSED(event))  //  When FullTree button clicked
{
if (arcman->IsArchive()) return;                          // We don't want to allow fulltree-mode if inside an archive

fulltree = !fulltree;                                     // Toggle it

startdir = GetPath();                                     // Rebase startdir to current selection
if (!fulltree)    AddPreviouslyVisitedDir(startdir);      // If we're changing from fulltree to branch mode, store the new rootpath for posterity 
ReCreateTreeFromSelection();                              // Rebase the tree, from selection or from root
}

void DirGenericDirCtrl::OnDirUp(wxCommandEvent& WXUNUSED(event))  // From toolbar Up button.  Go up a directory (if not in Fulltree mode)
{
if (fulltree) return;                                     // It doesn't make sense if we're already displaying the whole tree

wxString path = startdir;

while (path.Last()==wxFILE_SEP_PATH && path.Len() > 1)    // Unlikely, but remove multiple terminal '/'s
    path.RemoveLast();                            

if (path.IsSameAs(wxFILE_SEP_PATH))   return;             // Can't get higher than root!

if (path.Last()==wxFILE_SEP_PATH)   path.RemoveLast();    // If there's a terminal '/', remove it first
startdir = path.BeforeLast(wxFILE_SEP_PATH);              // Now truncate path by one segment & put in startdir
startdir += wxFILE_SEP_PATH;                              // Add back the terminal '/', partly because otherwise going up from "/home" would give ""!

AddPreviouslyVisitedDir(startdir, GetPathIfRelevant());   // Store the new startdir for posterity
RefreshTree(startdir, true, false);                       // Finally use RefreshTree to redo the tree
}

void DirGenericDirCtrl::OnDirBack(wxCommandEvent& WXUNUSED(event))  // From toolbar Back button.  Go back to a previously-visited directory
{
RetrieveEntry(-1);                                        // Go back one
}

void DirGenericDirCtrl::OnDirForward(wxCommandEvent& WXUNUSED(event))  // From toolbar Forward button.  Go forward to a previously-undone directory
{
RetrieveEntry(1);                                         // Go forward one
}

void DirGenericDirCtrl::OnDirDropdown(wxCommandEvent& event)  // When one of the Previous/Next Directory dropdown buttons is clicked
{
int x; bool rightside, back;

MyDropdownButton* button =  (MyDropdownButton*)event.GetEventObject();
back = (event.GetId() == XRCID("IDM_TOOLBAR_smalldropdown1"));  // See if the click was on the back or forwards dropdown button

rightside = isright && (ParentTab->GetSplitStatus() == vertical);  // Do things differently for right-side dirviews, but only if split vertically!

if (rightside)
  x = button->right;                                      // The edge of the popdown menu will be on the Rt edge of the dropdown button
  else
  x = button->left;                                       //   or on the left edge of its left neighbour

wxPoint location(x, 0);
MyPopupMenu(this, location, back, rightside);             // This is a class that does the pop-up menu holding previously visited dirs to select.  If 'back' we're going backward
}

void DirGenericDirCtrl::AddPreviouslyVisitedDir(const wxString& newentry, const wxString& path/*=wxT("")*/)  // Adds a new revisitable dir to the array
{
GetNavigationManager().PushEntry(newentry, path);
}


void DirGenericDirCtrl::RetrieveEntry(int displacement)   // Retrieves a dir from the array.  'count + displacement' determines which. 
{                                                         // 'displacement' may be + or -, depending on whether we're going backwards or forwards
int OriginalIndex =  GetNavigationManager().GetCurrentIndex();

wxArrayString arr = GetNavigationManager().RetrieveEntry(displacement);
if (!arr.GetCount() || arr.Item(0).empty()) return;

  // Grab any selected subdir and store it, overwriting any previously-stored value; the user may subsequently have changed it
if (OriginalIndex >= 0) GetNavigationManager().SetPath(OriginalIndex, GetPathIfRelevant());

if (arcman && arcman->NavigateArchives(arr.Item(0)) != Snafu) // This copes with both archives and ordinary filepaths
  { startdir = arr.Item(0); RefreshTree(arr.Item(0), true, false);
    if (!arr.Item(1).empty())
      { GetTreeCtrl()->UnselectAll();                     // Unselect to avoid ambiguity due to multiple selections
        SetPath(arr.Item(1));
      }
    return; 
  }

wxMessageBox(_("Hmm.  It seems that directory no longer exists!"));
GetNavigationManager().DeleteInvalidEntry();  // Delete the dud entry
if (GetNavigationManager().GetCount() > 0)    // If there's at least 1 more dir stored, re-enter to try to retrieve the next one
  { if (displacement && (GetNavigationManager().GetCurrentIndex()+displacement < GetNavigationManager().GetCount())) // if it's safe to do so
      RetrieveEntry(displacement);
  }
 else
  { startdir = wxT("/"); RefreshTree(startdir, true, false); } // If there was only 1 path in array, this dud one, display '/' (which SURELY still exists)
}

void DirGenericDirCtrl::GetBestValidHistoryItem(const wxString& deadfilepath) // Retrieves the first valid dir from the history. Used when an IN_UNMOUNT event is caught
{
wxArrayString arr = GetNavigationManager().GetBestValidHistoryItem(deadfilepath);
wxCHECK_RET(arr.GetCount(), wxT("NavigationManager::GetBestValidHistoryItem returned an empty array")); // which shouldn't be possible

startdir = arr.Item(0);
RefreshTree(startdir);
if (arr.GetCount() == 2 && !arr.Item(1).empty()) SetPath(arr.Item(1)); // There often won't have been a separate path stored, but reapply it if it was
}

void DirGenericDirCtrl::UpdateStatusbarInfo(const wxString& selected)  // Feeds the other overload with a wxArrayString if called with a single selection
{
if (!selected.empty())
  { wxArrayString arr; arr.Add(selected);
    UpdateStatusbarInfo(arr);
  }
}

void DirGenericDirCtrl::UpdateStatusbarInfo(const wxArrayString& selections)  // Writes filter info to statusbar(3), & the selection's name & data to statusbar(2)
{
if (!MyFrame::mainframe || !MyFrame::mainframe->SelectionAvailable) return;
                            // First the filter info into the 4th statusbar pane
wxString filterinfo, start, filter = GetFilterString(); if (filter.IsEmpty())  filter = wxT("*");  // Translate "" into "*"
start = (GetShowHidden() ? _(" D H     ") : _(" D     "));      // Create a message string, D for dirs if applicable, H for Hidden ones
filterinfo.Printf(wxT("%s%s"), start.c_str(), filter.c_str());  // Create a message string, adding the current filter
MyFrame::mainframe->SetStatusText(filterinfo, 3); 

size_t count = selections.GetCount();
if (!count || !partner)
  { MyFrame::mainframe->SetStatusText(wxEmptyString, 2); return; }  // If no data, clear statusbar(2)


wxString selected = selections.Item(0);
wxString text;
DataBase* stat; bool IsArchive = arcman && arcman->IsArchive();
if (count == 1)
  text.Printf(wxT("%s%s   %u %s %s   %u %s"), _("Dir: "), selected.AfterLast(wxFILE_SEP_PATH).c_str(),
                       (uint)((FileGenericDirCtrl*)partner)->NoOfFiles, _("Files, total size"), ParseSize(((FileGenericDirCtrl*)partner)->CumFilesize, true).c_str(),
                                                                             (uint)((FileGenericDirCtrl*)partner)->NoOfDirs, _("Subdirectories"));
 else
  { if (IsArchive)                                                // Archives have to be treated differently
      { SelectedCumSize = 0;
        for (size_t n=0; n < count; ++n)
          { wxString selected(selections.Item(n));
            FakeDir* fd = arcman->GetArc()->Getffs()->GetRootDir()->FindSubdirByFilepath(selected);
            if (fd)
              SelectedCumSize += fd->GetTotalSize();
          }
      }

    text = wxString::Format(_("%s in %zu directories"), ParseSize(SelectedCumSize, true), count);
  }
    
MyFrame::mainframe->SetStatusText(text, 2);
}

void MyGenericDirCtrl::OnShortcutCut(wxCommandEvent& WXUNUSED(event))                // Ctrl-X
{
if (ThreadsManager::Get().PasteIsActive())  // We shouldn't try to do 2 pastes at a time, and Cut involves a paste-to-can
 { BriefMessageBox(_("Please try again in a moment"), 2,_("I'm busy right now")); return; }

MyGenericDirCtrl::filearray.Clear();            // We'll be adding to the clipboard, so start with a clean sheet
MyGenericDirCtrl::filecount = 0;
MyGenericDirCtrl::Originpane = this;

UnRedoManager::StartSuperCluster();             // Start a new UnRedoManager StartSuperCluster, to be continued by Paste
bool AlreadyEnded = Delete(false, true);        // The 1st bool means Delete not Trash. The 2nd flags it's from Cut, so deleted files are stored in clipboard
if (!AlreadyEnded)                              // Otherwise this will be done later when the thread completes
  UnRedoManager::EndCluster();                  // Close the cluster (but Paste can reopen)
}


void DirGenericDirCtrl::NewFile(wxCommandEvent& WXUNUSED(event))  // Create a new Dir
{
wxString path, name;
                                      // Where to put the new item
path = GetPath();                                               // Get path of selected item
if (path.IsEmpty())  path = startdir;                           // If no selection, use "root"
if (path.Last() != wxFILE_SEP_PATH)  path += wxFILE_SEP_PATH;   // We almost certainly need to add a terminal '/'

FileData DestinationFD(path);
if (!DestinationFD.CanTHISUserWriteExec())                      // Make sure we have permission to write to the dir
  { wxString msg;
    if (arcman && arcman->IsArchive())  msg = _("I'm afraid you can't Create inside an archive.\nHowever you could create the new element outside, then Move it in.");
      else msg = _("I'm afraid you don't have permission to Create in this directory");
    wxMessageDialog dialog(this, msg, _("No Entry!"), wxOK | wxICON_ERROR);
    dialog.ShowModal(); return;
    }


wxDialog dlg;
wxXmlResource::Get()->LoadDialog(&dlg, MyFrame::mainframe, wxT("NewDir"));

int flag = 0;
do              // Do the rest in a loop, to give a 2nd chance if there's a name clash etc
  { if (dlg.ShowModal() != wxID_OK)    return;                  // If user cancelled, abort
    
    name = ((wxTextCtrl*)dlg.FindWindow(wxT("FileDir_text")))->GetValue();  // Get the desired name
    if (name.IsEmpty()) return;
    
    // Suppose path is $HOME/foo, and someone types in $HOME/foo/bar. They almost certainly want to add a bar/, not create $HOME/foo/$HOME/foo/bar. OTOH $HOME/baz is probably intentional
    wxString residue;
    if (name.Left(1) == wxFILE_SEP_PATH && name.StartsWith(path, &residue))
      { residue = StripSep(residue);
        if (residue.Len())
          name = residue;
      }
    
    flag = MyGenericDirCtrl::OnNewItem(name, path, true);       // Do the rest in MyGenericDirCtrl, as shared with MyFiles version
  }
 while (flag==wxID_YES);
 
if (!flag) return;
 

wxArrayInt IDs; IDs.Add(GetId());                               // Update the panes
MyFrame::mainframe->OnUpdateTrees(path, IDs);

bool ClusterWasNeeded = UnRedoManager::StartClusterIfNeeded();
UnRedoNewDirFile *UnRedoptr = new UnRedoNewDirFile(name, path, true, IDs);  // Make a new UnRedoNewDirFile
UnRedoManager::AddEntry(UnRedoptr);                             // and store it for Undoing
if (ClusterWasNeeded) UnRedoManager::EndCluster();
}

void DirGenericDirCtrl::ShowContextMenu(wxContextMenuEvent& event)
{
if (((MyTreeCtrl*)GetTreeCtrl())->QueryIgnoreRtUp()) return;    // If we've been Rt-dragging & now stopped, we don't want a context menu

wxMenu menu;
int Firstsection[] = { SHCUT_CUT, SHCUT_COPY, SHCUT_PASTE, SHCUT_PASTE_DIR_SKELETON, SHCUT_SOFTLINK, SHCUT_HARDLINK, wxID_SEPARATOR, 
                        SHCUT_TRASH, SHCUT_DELETE, wxID_SEPARATOR, SHCUT_REFRESH };
for (size_t n=0; n < sizeof(Firstsection)/sizeof(int); ++n)
  MyFrame::mainframe->AccelList->AddToMenu(menu, Firstsection[n]);

  // For these we need to override the standard label, appending 'Directory'
MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_RENAME, wxT("Rena&me Directory"));
MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_DUP, wxT("&Duplicate Directory"));
MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_NEW, wxT("&New Directory"));

menu.AppendSeparator();
MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_FILTER);

wxString label = GetShowHidden() ?  _("&Hide hidden dirs and files\tCtrl+H") : _("&Show hidden dirs and files\tCtrl+H");
MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_TOGGLEHIDDEN, label);


int Secondsection[] = { wxID_SEPARATOR, SHCUT_SPLITPANE_VERTICAL, SHCUT_SPLITPANE_HORIZONTAL, SHCUT_SPLITPANE_UNSPLIT, 
                        wxID_SEPARATOR, SHCUT_REPLICATE, SHCUT_SWAPPANES, wxID_SEPARATOR, SHCUT_PROPERTIES };
for (size_t n=0; n < sizeof(Secondsection)/sizeof(int); ++n)
  MyFrame::mainframe->AccelList->AddToMenu(menu, Secondsection[n]);

wxPoint pt = event.GetPosition();
ScreenToClient(&pt.x, &pt.y);

PopupMenu(&menu, pt.x, pt.y);
}

void DirGenericDirCtrl::CheckChildDirCount(const wxString& dir)
{
wxTreeItemId id = FindIdForPath(dir);
if (id.IsOk())
  { wxDirItemData* data = (wxDirItemData*)GetTreeCtrl()->GetItemData(id);
    if (data && !data->m_isExpanded)
      GetTreeCtrl()->SetItemHasChildren(id, data->HasSubDirs()); // (Un)Set the 'haschildren' marker
  }
}

void MyGenericDirCtrl::OnSplitpaneVertical(wxCommandEvent& WXUNUSED(event))  // Vertically splits the panes of the current tab
{
ParentTab->PerformSplit(vertical);
}

void MyGenericDirCtrl::OnSplitpaneHorizontal(wxCommandEvent& WXUNUSED(event))  // Horizontally splits the panes of the current tab
{
ParentTab->PerformSplit(horizontal);
}

void MyGenericDirCtrl::OnSplitpaneUnsplit(wxCommandEvent& WXUNUSED(event))  // Unsplits the panes of the current tab
{
ParentTab->PerformSplit(unsplit);
}


void MyGenericDirCtrl::OnShortcutTrash(wxCommandEvent& WXUNUSED(event))  // Del
{
if (ThreadsManager::Get().PasteIsActive())  // We shouldn't try to do 2 pastes at a time, and Delete involves a paste-to-can
 { BriefMessageBox(_("Please try again in a moment"), 2,_("I'm busy right now")); return; }

bool ClusterWasNeeded = UnRedoManager::StartClusterIfNeeded();
bool AlreadyEnded = Delete(true);
if (ClusterWasNeeded && !AlreadyEnded) UnRedoManager::EndCluster();
}


void MyGenericDirCtrl::OnShortcutDel(wxCommandEvent& WXUNUSED(event))  // Sh-Del
{
if (ThreadsManager::Get().PasteIsActive())  // We shouldn't try to do 2 pastes at a time, and Delete involves a paste-to-can
 { BriefMessageBox(_("Please try again in a moment"), 2,_("I'm busy right now")); return; }

bool ClusterWasNeeded = UnRedoManager::StartClusterIfNeeded();
bool AlreadyEnded = Delete(false);
if (ClusterWasNeeded && !AlreadyEnded) UnRedoManager::EndCluster();
}


void MyGenericDirCtrl::OnDnDMove()  // Move by DnD
{
wxString DestPath;
enum DupRen WhattodoifClash = DR_Unknown, WhattodoifCantRead = DR_Unknown; // These get passed to CheckDupRen.  May become SkipAll, OverwriteAll etc, but start with Unknown

                    // The item(s) 2b moved are in DnD clipboard
if (!MyGenericDirCtrl::DnDfilecount                               // If no files selected
    || MyGenericDirCtrl::DnDDestfilepath == wxEmptyString)        //   or no destination selected, abort
  { if (UnRedoManager::ClusterIsOpen) UnRedoManager::EndCluster(); return; }

wxString path = MyGenericDirCtrl::DnDDestfilepath;

MyGenericDirCtrl* active = MyFrame::mainframe->GetActivePane();   // Active is the origin MyGenericDirCtrl, 'this' is the destination one

                    // We need to check whether we're moving into or from within an archive: if so things are different
    // This copes with the standard scenario of the user dragging from one archive to the same part of the same one :/
if (arcman->GetArc()->DoThingsWithinSameArchive(active, this, MyGenericDirCtrl::DnDfilearray, path)) 
  { if (UnRedoManager::ClusterIsOpen) UnRedoManager::EndCluster(); return; } // It returned true, so the situation was sufficiently dealt with

if (active != NULL && active->arcman != NULL && active->arcman->IsArchive()) // The origin pane is an archive
  { if (active->arcman->GetArc()->MovePaste(active, this, MyGenericDirCtrl::DnDfilearray, path))
      DoBriefLogStatus(MyGenericDirCtrl::DnDfilearray.GetCount(), wxEmptyString,  _("moved")); // Use GetCount() as some items may have been skipped & removed from the array
    if (UnRedoManager::ClusterIsOpen) UnRedoManager::EndCluster();
    return;
  }
if (arcman != NULL && arcman->IsArchive())                        // The destination pane is an archive
  { if (arcman->GetArc()->MovePaste(active, this, MyGenericDirCtrl::DnDfilearray, path))
      DoBriefLogStatus(DnDfilecount, wxEmptyString,  _("moved"));
    if (UnRedoManager::ClusterIsOpen) UnRedoManager::EndCluster();
    return;
  }
// OK, end of archive stuff. It's safe to come out from behind the furniture

                    // Make sure the destination path is a dir with a terminal /  It's probably missing it
wxFileName Destination(path);
FileData fd(path);

if (fd.IsDir() || fd.IsSymlinktargetADir(true))                   // If the overall path is indeed a dir, plonk a / on the end of path, for the future
  { if (!Destination.GetFullName().IsEmpty())  path += Destination.GetPathSeparator(); }
 else                                                             // If it's a FILE, remove the filename, leaving the path for pasting to
  { Destination.SetFullName(wxEmptyString); path = Destination.GetFullPath(); }

FileData DestinationFD(path);
if (!DestinationFD.CanTHISUserWriteExec())                        // Make sure we have permission to write to the destination dir
  { wxMessageDialog dialog(this, _("I'm afraid you don't have Permission to write to this Directory"), _("No Entry!"), wxOK | wxICON_ERROR);
    dialog.ShowModal();
    if (UnRedoManager::ClusterIsOpen) UnRedoManager::EndCluster(); return;
  }

wxArrayInt IDs; IDs.Add(MyGenericDirCtrl::DnDFromID);             // Make an int array, & store the ID of origin pane
IDs.Add(GetId());                                                 // Add the destination ID

FileData OriginFileFD(MyGenericDirCtrl::DnDfilearray[0]);
if (!OriginFileFD.CanTHISUserMove_By_RenamingThusly(path))        // Make sure we have permissions for the origin dir to Rename its files i.e. the new path is still on the same device/partition
  if (!(OriginFileFD.CanTHISUserMove()  && CanFoldertreeBeEmptied(MyGenericDirCtrl::DnDfilearray[0])==CanBeEmptied))  //   or alternatively, that we will be allowed to do a true Move ie copy + delete the original
    { if (OriginFileFD.CanTHISUserPotentiallyCopy())              // See if we have parent dir exec (even though not write) permission
        { wxMessageDialog dialog(this, _("I'm afraid you don't have the right permissions to make this Move.\nDo you want to try to Copy instead?"),
                                                                                    _("No Exit!"), wxYES_NO | wxICON_QUESTION);
          if (dialog.ShowModal() != wxID_YES)
            { if (UnRedoManager::ClusterIsOpen) UnRedoManager::EndCluster(); return; }
           else return OnPaste(true, path);                       // Use Paste instead
        }
       else 
        { wxMessageDialog dialog(this, _("I'm afraid you don't have the right permissions to make this Move"),   _("No Exit!"));
          if (UnRedoManager::ClusterIsOpen) UnRedoManager::EndCluster(); return;
        }
    }

PasteThreadSuperBlock* tsb = dynamic_cast<PasteThreadSuperBlock*>(ThreadsManager::Get().StartSuperblock(wxT("move")));

int successes = 0;
for (size_t n=0; n < DnDfilecount; ++n)                           // For every entry in the clipboard
  { wxString DestFilename, DestOriginalname;
    bool ItsADir;
    
    FileData FromFD(MyGenericDirCtrl::DnDfilearray[n]);

                            // We need to check we're not about to do something illegal or immoral
    CheckDupRen CheckIt(this, MyGenericDirCtrl::DnDfilearray[n], path);  // Make an instance of the class that tests for this
    CheckIt.SetTSB(tsb);
    CheckIt.IsMultiple = (DnDfilecount > 1);        // We use different dialogs for multiple items
    CheckIt.WhattodoifClash = WhattodoifClash; CheckIt.WhattodoifCantRead = WhattodoifCantRead;  // Set CheckIt's variables to match any previous choice
    if (!CheckIt.CheckForAccess())                  // This checks for Read permission failure, and sets a SkipAll flag if appropriate
      { WhattodoifCantRead = CheckIt.WhattodoifCantRead; continue; }
    CDR_Result QueryPreExists = CheckIt.CheckForPreExistence();// Check we're not pasting onto ourselves or a namesake
    if (CheckIt.CheckForIncest()                    // or onto a descendant. False means OK
         || QueryPreExists == CDR_skip)             // There's a clash, and the user want to skip/cancel
      { WhattodoifClash = CheckIt.WhattodoifClash;  // Store any new choice in local variable.  This catches SkipAll & Cancel, which return false
        if (WhattodoifClash==DR_Cancel) 
          { n = DnDfilecount; break; }              // Cancel this item & the rest of the loop by increasing n
         else continue;                             // Skip this item but continue with loop
      }
      // If there was a clash & subsequent Rename, CheckDupRen will have changed the filename or the last segment of the path
    DestPath = CheckIt.finalpath;                   // This will be either the original path without the filename/last dir-segment, or the altered version
    DestFilename = CheckIt.finalbit;                // Ditto, but the filename/last-segment
    WhattodoifClash = CheckIt.WhattodoifClash;      // Store any new choice in local variable
    ItsADir = CheckIt.ItsADir;                      // CheckIt already tested whether it's a file or a dir. Piggyback onto this, it saves constant rechecking

    if (ItsADir && MyGenericDirCtrl::DnDfilearray[n].Last() != wxFILE_SEP_PATH) // If we're moving a dir, make sure it ends in a /
      { MyGenericDirCtrl::DnDfilearray[n] += wxFILE_SEP_PATH;                   // Also, store the last segment of the path, before any rename
        DestOriginalname = (MyGenericDirCtrl::DnDfilearray[n].BeforeLast(wxFILE_SEP_PATH)).AfterLast(wxFILE_SEP_PATH);  
      }
     else  DestOriginalname = MyGenericDirCtrl::DnDfilearray[n].AfterLast(wxFILE_SEP_PATH);  // If it's a file, store the original filename, before any rename
      
    wxFileName From(MyGenericDirCtrl::DnDfilearray[n]); // Make this File/Dir being dragged into a wxFileName
    wxFileName To(DestPath);                            // Ditto the To path  Must be done in each loop, as Move alters its contents

    if (!Move(&From, &To, DestFilename, tsb))           // Subcontract the actual Move
      { tsb->AddOverallFailures(1); continue; }

                                          // If we're here, it worked
    tsb->AddOverallSuccesses(1); tsb->SetMessageType(_("moved"));
    wxString origFromFP(From.GetFullPath());
    if (FromFD.IsDir())
      From.RemoveDir(From.GetDirCount() - 1);           // If we just dragged a dir, readjust wxFileName to take into account the removal
     else From.SetFullName(wxEmptyString);              // If not a dir, remove its name
          // NB The Move already altered Destination, appending to it the pasted subdir. Fortunately, this is just what we want for Undoing
    UnRedoMove* urm = new UnRedoMove(From.GetFullPath(), IDs, To.GetFullPath(), DestFilename, DestOriginalname, ItsADir);
    urm->SetOverwrittenFile( CheckIt.GetOverwrittenFilepath() ); // Store any saved overwritten filepath ready for undoing. Only needed for regular files using threads
    tsb->StoreUnRedoPaste(urm, origFromFP);             // We need to use the original filepath  here for matching, not the readjusted one
   
    // If we're not using FSWatcher, we need to sort out the treectrl. UpdateTrees does just that, and is a NOP if FSWatcher is active
    MyFrame::mainframe->OnUpdateTrees(From.GetPath(), IDs);
    MyFrame::mainframe->OnUpdateTrees(DestPath, IDs);
    ++successes;
  }
if (successes)
  tsb->StartThreads(); 
 else
  { ThreadsManager::Get().AbortThreadSuperblock(tsb); // Otherwise the thread will be orphaned and the progress throbber stuck on 0
    if (UnRedoManager::ClusterIsOpen) UnRedoManager::EndCluster();
  }
}

void MyGenericDirCtrl::OnShortcutHardLink(wxCommandEvent& WXUNUSED(event))
{
bool ClusterWasNeeded = UnRedoManager::StartClusterIfNeeded();
OnLink(false, myDragHardLink);            // I'm recycling myDragHardLink, even though this isn't a Drag, as flagged by the 'false'
if (ClusterWasNeeded) UnRedoManager::EndCluster();
}

void MyGenericDirCtrl::OnShortcutSoftLink(wxCommandEvent& WXUNUSED(event))
{
bool ClusterWasNeeded = UnRedoManager::StartClusterIfNeeded();
OnLink(false, myDragSoftLink);            // I'm recycling myDragSoftLink, even though this isn't a Drag, as flagged by the 'false'
if (ClusterWasNeeded) UnRedoManager::EndCluster();
}

void MyGenericDirCtrl::OnLink(bool FromDnD, int linktype)  // Create hard or soft link
{
size_t count;
wxArrayString filearray;
wxArrayInt IDs;
wxString path, ext, linkname; bool samedir; int nametype=0; wxRadioBox* LinktypeRadio;
                        // We may be using Ctrl-V or Dnd, so get data into local vars accordingly
if (FromDnD)
  { count = MyGenericDirCtrl::DnDfilecount;
    filearray = MyGenericDirCtrl::DnDfilearray;
    IDs.Add(GetId());                           // Store the ID of the destination pane
    path = MyGenericDirCtrl::DnDDestfilepath;
  }
 else
   { count = MyGenericDirCtrl::filecount;
    filearray = MyGenericDirCtrl::filearray;
    IDs.Add(GetId());
    path = GetPath();
    if (path.IsEmpty() && fileview==ISRIGHT)    // If we're in a fileview pane, but not hovering over something, 
      path = startdir;                          //    get the root path for the pane
        
    FileData fd(path);
    if (fd.IsSymlink())                         // If we're being asked to Paste onto a symlink, see if its target is a dir
      { wxString newpath = fd.GetUltimateDestination();
        FileData nfd(newpath); 
        if (nfd.IsDir())  path = newpath;       // Yes, it's a symlink-to-dir, so we want to replace path with the symlink target
      }                      // NB If the target isn't a dir, we don't want to deref the symlink at all; just paste into its parent dir as usual
  
  }

if (!count)  return;                            // Nothing in the "Clipboard"
if (path == wxEmptyString)  return;             // Nowhere to put it (not very likely, but nevertheless)

wxFileName Destination(path);                   // I'm making sure the destination path is a dir with a terminal /  It's probably missing it
FileData fd(path);
if ((fd.IsDir() || fd.IsSymlinktargetADir()) && !Destination.GetFullName().IsEmpty())
  { if (path.Last() != wxFILE_SEP_PATH)
      path += wxFILE_SEP_PATH;                  // If the overall path is indeed a dir, plonk a / on the end of path, for the future
  }
 else 
  { Destination.SetFullName(wxEmptyString);   // If it IS a file, remove the filename, leaving the path for pasting to
    path = Destination.GetFullPath();
  }

FileData fd1(path);                             // Make a new FileData as path may have been altered above
if (!fd1.CanTHISUserWriteExec())                // Ensure we have permission to write to the destination dir
  { wxString msg;
    if  (arcman && arcman->IsArchive())  msg = _("I'm afraid you can't create links inside an archive.\nHowever you could create the new link outside, then Move it in.");
      else msg = _("I'm afraid you don't have permission to create links in this directory");
    wxMessageDialog dialog(this, msg, _("No Entry!"), wxOK | wxICON_ERROR);
    dialog.ShowModal(); return;
    }
          // We need to be able to compare the source path with the dest path, to ensure we don't copy onto ourselves without changing name
wxString sourcepath = filearray[0].BeforeLast(wxFILE_SEP_PATH);
while (filearray[0].Right(1) == wxFILE_SEP_PATH  && filearray[0].Len() > 1) sourcepath.RemoveLast();
samedir = (sourcepath == path.BeforeLast(wxFILE_SEP_PATH));    // See if they're the same (we just added a terminal '/' to path, so remove if first)

MakeLinkDlg dlg; wxXmlResource::Get()->LoadDialog(&dlg, this, wxT("MakeLinkDlg"));
dlg.init(count > 1, samedir);                                  // If count<2 it doesn't make sense to offer to apply choices to other files.  If samedir, compel a name-change
LinktypeRadio = (wxRadioBox*)dlg.FindWindow(wxT("link_radiobox"));

int successes = 0;
for (size_t c=0; c < count; ++c)                // For every path in the clipboard to be linked
  { if (c==0 || !(dlg.ApplyToAll->IsEnabled() && dlg.ApplyToAll->GetValue())) // If this isn't the 1st time thru the loop, see if we've been told to ApplyToAll.  If so, skip to the 'Do it' section
      { wxString msg; msg.Printf(_("Create a new %s Link from:"), (linktype==myDragHardLink ? _("Hard") : _("Soft")));
        ((wxStaticText*)dlg.FindWindow(wxT("link_message")))->SetLabel(msg);    // Adjust the label to state link type
        LinktypeRadio->SetSelection((linktype==myDragSoftLink));
        dlg.frompath->SetLabel(filearray[c]);
        dlg.topath->SetLabel(path);

        if (dlg.lastradiostation == 2)                                // If we're likely to be making the link from the filename, insert the filename
          dlg.LinkFilename->SetValue(filearray[c].AfterLast(wxFILE_SEP_PATH));
        dlg.GetSizer()->Layout();
        
        int result;
        do
          { result = false;
            int ans = dlg.ShowModal();
            if (ans == wxID_CANCEL) return;     // Cancel everything else
            if (ans != wxID_OK)     continue;   // If user pressed Skip, abort this iteration of the main loop
            result = true;
            
            if  (((wxRadioButton*)dlg.FindWindow(wxT("NameSame")))->GetValue()) nametype = 0;  // How do we want to make a name for the link:  reuse the original, add an ext, or bespoke?
              else if  (((wxRadioButton*)dlg.FindWindow(wxT("NameExt")))->GetValue()) nametype = 1;
              else nametype = 2;

            if (nametype == 1)                  // Add an ext  
              { ext = dlg.LinkExt->GetValue();
                if (ext.IsEmpty())
                  { wxMessageBox(_("Please provide an extension to append"));
                    result = 2; continue;
                  }
                if (ext.at(0) != wxT('.')) ext = wxT('.') +ext;  // Ensure the ext is an ext!
              }
            if (nametype == 2 &&  dlg.LinkFilename->GetValue().IsEmpty())
                { wxMessageBox(_("Please provide a name to call the link"));
                  result = 2; continue;
                }
            if (nametype == 2 &&  dlg.LinkFilename->GetValue() == filearray[c].AfterLast(wxFILE_SEP_PATH))
                { wxMessageBox(_("Please provide a different name for the link"));
                  result = 2; continue;
                }    
          } while (result == 2);                // Redo the dialog if there was insufficient data
        if (!result) continue;                  // Cancel this iteration
      }
      
    if (nametype == 0)  linkname = filearray[c].AfterLast(wxFILE_SEP_PATH);                 // Reuse the filename
     else if (nametype == 1)    linkname = filearray[c].AfterLast(wxFILE_SEP_PATH) + ext;   //  or add an ext
     else if (nametype == 2)    linkname = dlg.LinkFilename->GetValue();                    //    or use a fresh name
     
    wxString linkfilepath = path + linkname;    // Create the new filepath

    bool success; bool linkage = LinktypeRadio->GetSelection(); // Reread the radiobox to see if we had 2nd thoughts about the type of link
    if (linkage)  
      { int rel = dlg.MakeRelative->IsChecked();                // Do we want to make a relative- or absolute-path symlink?
        success = CreateSymlinkWithParameter(filearray[c], linkfilepath, (enum changerelative)rel);  // Create a new symlink
        if (!success)  continue;
        
        UnRedoLink *UnRedoptr = new UnRedoLink(filearray[c], IDs, linkfilepath, linkage, (enum changerelative)rel);
        UnRedoManager::AddEntry(UnRedoptr);
      }
      else  
      { success = CreateHardlink(filearray[c], linkfilepath);                                      //   or hardlink
        if (!success)  continue;
        
        UnRedoLink *UnRedoptr = new UnRedoLink(filearray[c], IDs, linkfilepath, linkage);
        UnRedoManager::AddEntry(UnRedoptr);
      }
      
    ++successes;

    
    MyFrame::mainframe->OnUpdateTrees(linkfilepath, IDs);
    if (FromDnD && fileview==ISLEFT)  partner->ReCreateTreeFromSelection();  // Symlinks made into a dirview will otherwise not update the fileview
  }

DoBriefLogStatus(successes, wxEmptyString,  _("linked"));
}

bool MyGenericDirCtrl::Delete(bool trash, bool fromCut /*=false*/)  // Does either Delete or Trash, depending on 1st bool
{
wxString msg, path, DestFilename;
wxArrayString paths;
bool ItsADir;
wxString action;
if (fromCut)  action = _("cut");
 else action = (trash ?  _("trashed") : _("deleted"));
    
      // We need to get the highlit item(s) to be deleted.  If wxTR_MULTIPLE there may be >1
if (GetTreeCtrl()->HasFlag(wxTR_MULTIPLE))  GetMultiplePaths(paths);  // Fill array with multiple selections, if style permits
 else paths.Add(GetPath());                                   // If not multiple selection style, get the one&only selection

size_t count = paths.GetCount(); if (!count) return false;    // If no selection, abort

bool IsArchive = (arcman != NULL && arcman->IsArchive());     // Archives are very different

if (!IsArchive)
  { FileData fd0(paths[0]);                                   // Check for delete permission for the parent dir
    if (fd0.CanTHISUserMove() == 0)
      { wxMessageDialog dialog(this, _("I'm afraid you don't have permission to Delete from this directory"), wxT("Permission problem"), wxOK | wxICON_ERROR);
        dialog.ShowModal(); return false;
      }

    if (fileview == ISLEFT) // If, in a dirview, a parent dir and one of its children were both selected, skip the child to prevent an 'Oops' dialog
      for (size_t n = count; n > 0; --n)
        for (size_t i = count; i > 0; --i)
          if ((n-1 != i-1) && IsDescendentOf(paths[n-1], paths[i-1]))
            { paths.RemoveAt(i-1); --count; }

    wxArrayString badpermissions;
    for (size_t n=0; n < count; ++n)                          // Check that (all) the item(s) still exist (e.g. an nfs mount, and another machine has deleted one)
      { FileData fd(paths[n]);
        if (!fd.IsValid())
          { msg = (count > 1) ? _("At least one of the items to be deleted seems not to exist") : _("The item to be deleted seems not to exist");
            wxMessageDialog dialog(this, msg, _("Item not found"), wxOK | wxICON_ERROR);
            dialog.ShowModal();
            if (fileview == ISLEFT) RefreshTree(startdir);    // Since there's obviously a problem, let's refresh the tree
              else partner->RefreshTree(partner->startdir);
            return false;
          }
        if (!fd.CanTHISUserRead()) badpermissions.Add(paths[n]);
      }

    size_t badpermscount = badpermissions.GetCount();
    if (badpermscount > 0)
      { if (badpermscount == count) 
          { if (count > 1) msg << _("I'm afraid you don't have permission to move these items to a 'can'.") << _("\nHowever you could 'Permanently delete' them.");
             else msg << wxString::Format(_("I'm afraid you don't have permission to move %s to a 'can'."), badpermissions.Item(0).c_str())
                      << _("\nHowever you could 'Permanently delete' it.");
            wxMessageDialog dialog(this, msg, wxT("Permission problem"), wxOK | wxICON_ERROR);
            dialog.ShowModal(); return false;
          }
         else
          { if (badpermscount > 1) msg << wxString::Format(_("I'm afraid you don't have permission to move %u of these %u items to a 'can'."), (unsigned int)badpermscount, (unsigned int)count)
                                       << _("\nHowever you could 'Permanently delete' them.");
             else msg << wxString::Format(_("I'm afraid you don't have permission to move %s to a 'can'."), badpermissions.Item(0).c_str())
                      << _("\nHowever you could 'Permanently delete' it.");
            msg << _("\n\nDo you wish to delete the other item(s)?");
            wxMessageDialog dialog(this, msg, wxT("Permission problem"), wxYES_NO | wxICON_QUESTION);
            if (dialog.ShowModal() != wxID_YES)  return false;

            for (size_t n=0; n < badpermscount; ++n)
              { int index = paths.Index(badpermissions.Item(n));
                if (index != wxNOT_FOUND) paths.RemoveAt(index);
              }
            count = paths.GetCount();
          }
      }
  }

if (trash)  msg = _("Discard to Trash: "); 
 else msg = _("Delete: ");

if (((ASK_ON_DELETE && !trash) || (ASK_ON_TRASH && trash))    // If it's preferred not to destroy the filesystem accidentally
                                      && !fromCut)            //   & we're deleting, not Cutting
  { if (count < 10)
      for (size_t n=0; n < count; n++)                        // Get the path(s) into path, separated by newlines
        path +=  wxT("\n") + paths[n];      
     else                                                     // If there are too many items, the MessageDialog expands off the page! So prune
      { path.Printf(_("%zu items, from: "), count);
        for (size_t n=0; n < 3; n++)                          // Get first 3 paths into path, separated by newlines
          path +=  wxT("\n") + paths[n];
        path +=  _("\n\n          To:\n");
        for (size_t n = count-3; n < count; n++)              // & finish with the last 3 paths
          path +=  wxT("\n") + paths[n];
      }
    wxMessageDialog ask(this,msg+path,wxString(_("Are you SURE?")), wxYES_NO | wxICON_QUESTION);
    if (ask.ShowModal() != wxID_YES) return false;
  }

wxFileName trashdirbase;                                      // Create a unique subdir in trashdir, using current date/time
if (!DirectoryForDeletions::GetUptothemomentDirname(trashdirbase, trash ? trashcan : delcan))
  { wxMessageBox(_("For some reason, trying to create a dir to receive the deletion failed.  Sorry!")); return false; }

if (IsArchive)            
  { wxArrayString destinations, unused; wxString trashpath(trashdirbase.GetPath());
    if (!arcman->GetArc()->DoExtract(paths, trashpath, destinations, true))  // First extract the files to the bin. The true means don't unredo: it'll happen elsewhere
      { wxMessageDialog dialog(this, _("Sorry, Deletion failed"), _("Oops!"), wxOK | wxICON_ERROR); dialog.ShowModal(); return false; }
    arcman->GetArc()->Alter(paths, arc_remove, unused);                      // Then remove from the archive
    
    wxString pathtoupdate = paths[0].BeforeLast(wxFILE_SEP_PATH); // We can't use path[0] here (we've just deleted it!) so use its parent
    wxArrayInt IDs; IDs.Add(GetId());
    MyFrame::mainframe->OnUpdateTrees(pathtoupdate, IDs, wxT(""), true);
    UnRedoCutPasteToFromArchive* UnRedoptr = new UnRedoCutPasteToFromArchive(paths, destinations, paths[0], trashpath, IDs, this, this, amt_from);
    UnRedoManager::AddEntry(UnRedoptr);
    if (fromCut)                                              // If we're Cutting, we now need to fill the 'clipboard' ready for any Paste
      { wxDir d(trashpath); if (!d.IsOpened()) return false;
        wxString filename; bool cont = d.GetFirst(&filename);
             while (cont)  { MyGenericDirCtrl::filearray.Add(trashpath+wxFILE_SEP_PATH+filename); cont = d.GetNext(&filename); }
        MyGenericDirCtrl::filecount = MyGenericDirCtrl::filearray.GetCount(); 
        MyGenericDirCtrl::Originpane = this;
      }
    DoBriefLogStatus(count, wxEmptyString, action);
    return false; // Still return false here: it signals that we're not in a thread situation, so the cluster does need closing
  }

PasteThreadSuperBlock* tsb = dynamic_cast<PasteThreadSuperBlock*>(ThreadsManager::Get().StartSuperblock(wxT("move")));
size_t successes=0, cantdel=0, cantdelsub=0, invalid=0;       // so that for multiple deletions, we can if necessary report on partial success
for (size_t n=0; n < count; n++)                              // For every path in the array
  { wxString original;
    wxFileName trashdir;
    
    if (count > 1)                                            // If we're doing a multiple del, create a unique subdir for each item (in case of del.ing both ./foo & ./bar/foo)
      { wxString subdirname = trashdirbase.GetFullPath() + CreateSubgroupName(n, count);      // Create a unique subgroup name
        trashdir.Mkdir(subdirname);                           // Create the subdir to which to delete
        trashdir.AssignDir(subdirname);
      }  
     else trashdir = trashdirbase;                            // If only one item to delete, use the base dir

    FileData stat(paths[n]); ItsADir = stat.IsDir();
    if (ItsADir)
      { enum emptyable ans = CanFoldertreeBeEmptied(paths[n]);// Check this dir and any downstream can be deleted from
        if (ans != CanBeEmptied)
          { switch (ans)
              { case CannotBeEmptied:        ++cantdel;   break;
                case SubdirCannotBeEmptied:  ++cantdelsub;break;
                 default:                    ++invalid;   break;
              }
            continue;                   // Try another filepath
          }
      
        if (paths[n].Last() != wxFILE_SEP_PATH) paths[n] << wxFILE_SEP_PATH;            // If we're deleting a dir, make sure it ends in a /
        DestFilename = (paths[n].BeforeLast(wxFILE_SEP_PATH)).AfterLast(wxFILE_SEP_PATH); // We need the last segment of the path
      }
     else  DestFilename = paths[n].AfterLast(wxFILE_SEP_PATH);// If it's a file, store the filename
       
    wxFileName fn(paths[n]);
    
    if (!Move(&fn, &trashdir, DestFilename, tsb)) break;      // Do the actual "deletion"

    ++successes;            // If we're here, it worked.  Readjust wxFileName to take into account the deletion
    if (ItsADir)                                              // If it's a dir,
      { original = fn.GetPath();                              // Save current path
        fn.RemoveDir(fn.GetDirCount() - 1);                   // then readjust wxFileName to take into account the deletion.
      }
     else
        fn.SetFullName(wxEmptyString);                        // If it's a file, just kill the filename, leaving the path unchanged
      
    path = fn.GetPath();                                      //  Either way, load path with truncated version
  
                      // NB The Move already altered To, appending to it the pasted subdir. Fortunately, this is just what we want for Undoing
    wxArrayInt IDs;
    IDs.Add(GetId());
    tsb->StoreUnRedoPaste( new UnRedoMove(fn.GetFullPath(), IDs, trashdir.GetFullPath(), DestFilename, DestFilename, ItsADir, true), paths[n] );

    if (fromCut)
      { MyGenericDirCtrl::filearray.Add(trashdir.GetFullPath()); // Store the address of the deleted item in the "clipboard"
        ++MyGenericDirCtrl::filecount;
      }
                                              // Finally we have to sort out the treectrl
    if (fileview==ISLEFT && GetPath()==stat.GetFilepath())    // If we've just deleted the selected dir
              SetPath(path);                                  //   we need to SetPath to elsewhere, otherwise the fileview continues to show the contents of the deleted dir!

    // If !USE_FSWATCHER and the 'Move' was done by renaming, this is necessary. If not, it'll do no harm
    if ((ItsADir && startdir==original) || (startdir==original.BeforeLast(wxFILE_SEP_PATH)))  // Make sure we're not deleting 'root'
          { while (path.Right(1) == wxFILE_SEP_PATH && path.Len() > 1) path.RemoveLast();    // Cope with "/foo///" and also "///"
            MyFrame::mainframe->OnUpdateTrees(original, IDs, path);// If we are, use the alt version of updatetrees to replace startdir with its parent (path)
          }
      else MyFrame::mainframe->OnUpdateTrees(path, IDs);      // Otherwise the standard version
  }
tsb->AddOverallSuccesses(successes); tsb->AddOverallFailures(count - successes); tsb->SetMessageType(fromCut ? _("cut") : _("deleted"));
if (successes)
  tsb->StartThreads(); 
 else
  ThreadsManager::Get().AbortThreadSuperblock(tsb); // Otherwise the thread will be orphaned and the progress throbber stuck on 0

if (cantdel || cantdelsub || invalid)                         // Were there any failures above?  If so, do an apologetic dialog
  { wxString msg, msg2, msg3, msg4;
    if (count == 1)
      { if (cantdel) msg2 = _(" You don't have permission to Delete from this directory.\nYou could try deleting piecemeal.");
          else if (cantdelsub) msg2 = _(" You don't have permission to Delete from a subdirectory of this directory.\nYou could try deleting piecemeal.");
            else if (invalid)  msg2 = _(" The filepath was invalid.");
      }
    else                                                      // More complex message if  >1 item
      { if (cantdel) msg2.Printf(wxT("\n%s%u%s"),_("For "), (uint)cantdel, _(" of the items, you don't have permission to Delete from this directory."));  
        if (cantdelsub) msg3.Printf(wxT("\n%s%u%s"),_("For "), (uint)cantdelsub, _(" of the items, you don't have permission to Delete from a subdirectory of this directory."));
        if (invalid)  msg4.Printf(wxT("\n%s%u%s"),_("For "), (uint)invalid, _(" of the items,  the filepath was invalid."));
        msg2 =  msg2 + msg3 + msg4;                           // Concatanate to cater for multiple failure modalities
        if (!msg2.IsEmpty())  msg2 += _(" \nYou could try deleting piecemeal.");
      }
    if (!successes)                                           // Abject failure
      { if (count == 1)  msg = _("I'm afraid the item couldn't be deleted."); // Only 1 item so different grammar
          else          msg.Printf(wxT("%s%u%s"),_("I'm afraid "), (uint)count, _(" items could not be deleted."));
      }
     else                                                     // If there was at least one success
        msg.Printf(wxT("%s%u%s%u%s"),_("I'm afraid only "), (uint)successes, _(" of the "), (uint)count, _(" items could be deleted."));

    wxMessageDialog dialog(this, msg + msg2, _("Oops!"), wxOK | wxICON_ERROR); dialog.ShowModal();
  }

if (!successes) BriefLogStatus bls(fromCut ?  _("Cut failed") : _("Deletion failed"));
return successes > 0;
}

void MyGenericDirCtrl::OnShortcutReallyDelete()  // Deletes permanently, can't unredo
{
wxString msg, path, DestFilename;
wxArrayString paths;
bool ItsADir;

      // We need to get the highlit item(s) to be deleted.  If wxTR_MULTIPLE there may be >1
if (GetTreeCtrl()->HasFlag(wxTR_MULTIPLE))  GetMultiplePaths(paths);  // Fill array with multiple selections, if style permits
  else    paths.Add(GetPath());                               // If not multiple selection style, get the one&only selection

size_t count = paths.GetCount(); if (!count) return;          // If no selection, abort

bool IsArchive = (arcman && arcman->IsArchive());             // Archives are very different

if (!IsArchive)
  { bool missing = false;                                         // Check that (all) the item(s) still exist (e.g. an nfs mount, and another machine has deleted one)
    for (size_t n=0; n < count; n++)
      { FileData fd(paths[n]);
        if (!fd.IsValid())
          { missing = true; break; }
      }
    if (missing)
      { wxString msg = (count > 1) ? _("At least one of the items to be deleted seems not to exist") : _("The item to be deleted seems not to exist");
        wxMessageDialog dialog(this, msg, _("Item not found"), wxOK | wxICON_ERROR);
        dialog.ShowModal();
        if (fileview == ISLEFT) RefreshTree(startdir);            // Since there's obviously a problem, let's refresh the tree
          else partner->RefreshTree(partner->startdir);
        return;
      }

    FileData fd0(paths[0]);                                   // Make a FileData to check for delete permission for the parent dir
    if (!fd0.CanTHISUserMove())
      { wxMessageDialog dialog(this, _("I'm afraid you don't have permission to Delete from this directory"), _("No Entry!"), wxOK | wxICON_ERROR);
        dialog.ShowModal(); return;
      }
  }

if (fileview == ISLEFT && !IsArchive) // If, in a dirview, a parent dir and one of its children were both selected, skip the child to prevent an 'Oops' dialog
  for (size_t n = count; n > 0; --n)
    for (size_t i = count; i > 0; --i)
      if ((n-1 != i-1) && IsDescendentOf(paths[n-1], paths[i-1]))
        { paths.RemoveAt(i-1); --count; }

if (count < 10)
for (size_t n=0; n < count; n++)                              // Get the path(s) into path, separated by newlines
    path +=  wxT("\n") + paths[n];      
 else                                                         // If there are too many items, the MessageDialog expands off the page! So prune
  { path.Printf(_("%zu items, from: "), count);
    for (size_t n=0; n < 3; n++)                              // Get first 3 paths into path, separated by newlines
      path +=  wxT("\n") + paths[n];
    path +=  _("\n\n          To:\n");
    for (size_t n = count-3; n < count; n++)                  // & finish with the last 3 paths
      path +=  wxT("\n") + paths[n];
  }
msg = _("Permanently delete (you can't undo this!): ");
wxMessageDialog ask(this,msg+path,wxString(_("Are you ABSOLUTELY sure?")), wxYES_NO | wxICON_QUESTION);
if (ask.ShowModal() != wxID_YES) return;


if (IsArchive)            
  { wxArrayString unused;
    if (!arcman->GetArc()->Alter(paths, arc_remove, unused))  // Remove from the archive
      { wxMessageDialog dialog(this, _("Sorry, Deletion failed"), _("Oops!"), wxOK | wxICON_ERROR); dialog.ShowModal(); return; }
    
    wxString pathtoupdate = paths[0].BeforeLast(wxFILE_SEP_PATH); // We can't use path[0] here (we've just deleted it!) so use its parent
    wxArrayInt IDs; IDs.Add(GetId());
    MyFrame::mainframe->OnUpdateTrees(pathtoupdate, IDs, wxT(""), true);

    DoBriefLogStatus(count, wxEmptyString, _("irrevocably deleted"));
    return;
  }

size_t successes=0, cantdel=0, cantdelsub=0, invalid=0;       // so that for multiple deletions, we can if necessary report on partial success

for (size_t n=0; n < count; n++)                              // For every path in the array
  { wxString original;
    FileData stat(paths[n]); ItsADir = stat.IsDir();
    if (ItsADir)
      { enum emptyable ans = CanFoldertreeBeEmptied(paths[n]);// Check this dir and any downstream can be deleted from
        if (ans != CanBeEmptied)
          { switch (ans)
              { case CannotBeEmptied:       ++cantdel;    break;
                case SubdirCannotBeEmptied: ++cantdelsub; break;
                 default:                   ++invalid;    break;
              }
            continue;                   // Try another filepath
          }
      
        if (paths[n].Last() != wxFILE_SEP_PATH) paths[n] << wxFILE_SEP_PATH;            // If we're deleting a dir, make sure it ends in a /
        DestFilename = (paths[n].BeforeLast(wxFILE_SEP_PATH)).AfterLast(wxFILE_SEP_PATH); // We need the last segment of the path
      }
     else  DestFilename = paths[n].AfterLast(wxFILE_SEP_PATH);// If it's a file, store the filename
       
    wxFileName fn(paths[n]);                                  // Turn filepath into the class that does clever things to the file system
    
    if (!ReallyDelete(&fn))  break;                           // Do the actual deletion

    ++successes;            // If we're here, it worked.  Readjust wxFileName to take into account the deletion. 
    if (ItsADir)                                              // If it's a dir,
      { original = fn.GetPath();                              // Save current path
        fn.RemoveDir(fn.GetDirCount() - 1);                   // then readjust wxFileName to take into account the deletion.
      }
     else
        fn.SetFullName(wxEmptyString);                        // If it's a file, just kill the filename, leaving the path unchanged
      
    path = fn.GetPath();                                      //  Either way, load path with truncated version

                                              // Finally we have to sort out the treectrl
    if (fileview==ISLEFT && GetPath()==stat.GetFilepath())    // If we've just deleted the selected dir
       SetPath(path); //   we need to SetPath to elsewhere, otherwise the fileview continues to show the contents of the deleted dir!

    wxArrayInt IDs; IDs.Add(GetId());                         // Make an int array to store the ID of the origin pane
    if ((ItsADir && startdir==original) || (startdir==original.BeforeLast(wxFILE_SEP_PATH)))  // Make sure we're not deleting 'root'
      { while (path.Right(1) == wxFILE_SEP_PATH && path.Len() > 1) path.RemoveLast();    // Cope with "/foo///" and also "///"
        MyFrame::mainframe->OnUpdateTrees(original, IDs, path);// If we are, use the alt version of updatetrees to replace startdir with its parent (path)
      }
     else MyFrame::mainframe->OnUpdateTrees(path, IDs);       // Otherwise the standard version
  }

if (cantdel || cantdelsub || invalid)                         // Were there any failures above?  If so, do an apologetic dialog
  { wxString msg, msg2, msg3, msg4;
    if (count == 1)
      { if (cantdel) msg2 = _(" You don't have permission to Delete from this directory.\nYou could try deleting piecemeal.");
          else if (cantdelsub) msg2 = _(" You don't have permission to Delete from a subdirectory of this directory.\nYou could try deleting piecemeal.");
            else if (invalid)  msg2 = _(" The filepath was invalid.");
      }
    else                                                      // More complex message if  >1 item
      { if (cantdel) msg2.Printf(wxT("\n%s%u%s"),_("For "), (uint)cantdel, _(" of the items, you don't have permission to Delete from this directory."));  
        if (cantdelsub) msg3.Printf(wxT("\n%s%u%s"),_("For "), (uint)cantdelsub, _(" of the items, you don't have permission to Delete from a subdirectory of this directory."));
        if (invalid)  msg4.Printf(wxT("\n%s%u%s"),_("For "), (uint)invalid, _(" of the items,  the filepath was invalid."));
        msg2 =  msg2 + msg3 + msg4;                           // Concatanate to cater for multiple failure modalities
        if (!msg2.IsEmpty())  msg2 += _(" \nYou could try deleting piecemeal.");
      }
    if (!successes)                                           // Abject failure
      { if (count == 1)  msg = _("I'm afraid the item couldn't be deleted.");    // Only 1 item so different grammar
          else            msg.Printf(wxT("%s%u%s"),_("I'm afraid "), (uint)count, _(" items could not be deleted."));
      }
     else                                                     // If there was at least one success
        msg.Printf(wxT("%s%u%s%u%s"),_("I'm afraid only "), (uint)successes, _(" of the "), (uint)count, _(" items could be deleted."));

    wxMessageDialog dialog(this, msg + msg2, _("Oops!"), wxOK | wxICON_ERROR); dialog.ShowModal();
  }

if (!successes) { BriefLogStatus bls(_("Deletion failed")); return; }

DoBriefLogStatus(count, wxEmptyString, _("irrevocably deleted"));
}

//static
bool MyGenericDirCtrl::ReallyDelete(wxFileName* PathName)  // Deletes files, dirs+contents.  Static as used by UnRedoManager too
{
wxString msg;
        // Getting paths from the wxTreeCtrl  has a disadvantage:  if a dir is selected, it has no terminal /   The same's true of wxDir.
        // As a result wxFileName assumed that the name is path+filename, and this screws up the subsequent procedures.
        // So check whether it's actually a dir, and if so to append the "filename" to the path.
        // There's also the minor problem of symlinks, which may target a dir!  So use FileData
class FileData stat(PathName->GetFullPath());
if (stat.IsDir())                                             // If it's really a dir
  { if (!PathName->GetFullName().IsEmpty())                   // and providing there IS a "filename"
      PathName->AppendDir(PathName->GetFullName()); PathName->SetFullName(wxEmptyString); // make it into the real path
  }
 else
  { if (stat.IsSymlink())
      { if (!wxRemoveFile(stat.GetFilepath()))
          { wxMessageBox(_("Deletion Failed!?!")); return false; }  // Shouldn't fail, but exit ungracefully if somehow it does eg due to permissions
      }      
     else                           // It must be a file
      { if (!stat.IsValid())                                  // Check it exists.  If not, exit ungracefully
          { msg = _("? Never heard of it!"); wxMessageBox(PathName->GetFullName()+msg); return false; }
        if (!wxRemoveFile(PathName->GetFullPath()))           // It's alive, so kill it
          { wxMessageBox(_("Deletion Failed!?!")); return false; }  // Shouldn't fail, but exit gracelessly if somehow it does eg due to permissions
      }
    return true;
  }

                                  // If we're here, it's a dir
wxDir dir(PathName->GetPath());                               // Use helper class to deal sequentially with each child file & subdir
if (!dir.IsOpened()) return false;

wxString filename;
while (dir.GetFirst(&filename))                               // Go thru the dir, committing infanticide 1 child @ a time
  { wxFileName child(PathName->GetPath(), filename);          // Make a new wxFileName for child
    if (!ReallyDelete(&child))                                // Slaughter it by recursion, whether it's a dir or file
      return false;                                           // If this fails, bug out
  }
  
if (!PathName->Rmdir())                                       // Finally, kill the dir itself
  { wxMessageBox(_("Directory deletion Failed!?!")); return false; }

return true; 
}

//static 
enum MovePasteResult MyGenericDirCtrl::Move(wxFileName* From, wxFileName* To, wxString ToName, ThreadSuperBlock* tsb, const wxString& OverwrittenFile/*=wxT("")*/)  // Moves files, dirs+contents.  Static as used by UnRedo too
{
wxLogNull log;
wxFileName OldTo(*To);                                  // Store the unamended destination, in case we need it later

FileData from(From->GetFullPath());
if (!from.IsValid())
  { wxMessageBox(_("The File seems not to exist!?!")); return MPR_fail; }  

if (from.IsDir())                          // If we're Moving a dir, we need to add it to the path
  { wxString fullpath(To->GetPath()); if (fullpath.Last() != wxFILE_SEP_PATH) fullpath << wxFILE_SEP_PATH;
    if (!ToName.IsEmpty() && ToName.Last() != wxFILE_SEP_PATH) ToName << wxFILE_SEP_PATH;
    
    if (!(ToName.Left(ToName.Len()-1)).BeforeLast(wxFILE_SEP_PATH).IsEmpty()) // See if there's an intermediate dir that we have to make ie foo/bar/ instead of just bar/
      { wxFileName temp(fullpath + ToName);             // If so, create a wxFileName, truncate it and mkdir
        temp.RemoveDir(temp.GetDirCount() - 1);
        temp.Mkdir(0777, wxPATH_MKDIR_FULL);
      }
    
    To->Assign(fullpath + ToName);                      // Use Assign here, rather than AppendDir, in case ToName is foo/bar/
  }
 else                                  // If it's not a dir, make it the filename
  { wxString ToNamePath = ToName.BeforeLast(wxFILE_SEP_PATH), tooname(ToName);
    if (!ToNamePath.IsEmpty()) tooname = tooname.AfterLast(wxFILE_SEP_PATH);
    wxString fullpath(To->GetPath()); if (fullpath.Last() != wxFILE_SEP_PATH) fullpath << wxFILE_SEP_PATH;
    if (!ToNamePath.IsEmpty() && ToNamePath.Last() != wxFILE_SEP_PATH) ToNamePath << wxFILE_SEP_PATH;
    ToNamePath = fullpath + ToNamePath;
    To->Assign(ToNamePath, tooname);                    // Use Assign here, rather than SetFullName(), in case ToName is foo/bar
  }

  // There are 2 ways to Move a file or dir:  the easy way is just to rename it.  However this fails if the destination filepath is on a different partition.
  // It also fails if we're moving a symlink with a relative target (which we don't want it to retain) so use the second method for that (I've altered Paste() to cope)
  // Oh, and we can't use this if we're overwriting the destination and it wasn't possible to do a thread-free save to the trashcan, or if it's a misc e.g. a socket
bool overwriting = tsb && !dynamic_cast<PasteThreadSuperBlock*>(tsb)->GetTrashdir().empty();
bool oddity = !(from.IsDir() || from.IsRegularFile() || from.IsSymlink());
if (!overwriting && !oddity && !(RETAIN_REL_TARGET && ContainsRelativeSymlinks(from.GetFilepath())))
  { if (wxRename(From->GetFullPath(), To->GetFullPath()) == 0)
      { UnexecuteImages(To->GetFullPath());
        KeepShellscriptsExecutable(To->GetFullPath(), from.GetPermissions());
        if (tsb)
          { tsb->AddPreCreatedItem(From->GetFullPath(), To->GetFullPath()); tsb->AddSuccesses(1); }
        return MPR_completed;
      }
     else if (!tsb && !from.IsSymlink() && !oddity)
      return MPR_fail; // This might happen from CheckDupRen::FileAdjustForPreExistence if that function's IsThreadlessMovePossible() call got it wrong
  }

  // If we're here, it was either a relative symlink, an oddity or renaming failed. Try the alternative, Copy/Delete, method
if (from.IsSymlink()) 
  { MovePasteResult result = Paste(From, &OldTo, ToName, from.IsDir(), false, false, NULL, OverwrittenFile);
    if (result != MPR_fail) // For symlinks, no need to use threads (and CreateLink() uses wxMessageBox error reporting...)
      { if (RETAIN_MTIME_ON_PASTE)
          FileData::ModifyFileTimes(To->GetFullPath(), From->GetFullPath());
        ReallyDelete(From);
        if (tsb) 
          { tsb->AddSuccesses(1);
            if (result == MPR_completed) tsb->AddPreCreatedItem(From->GetFullPath(), To->GetFullPath());
          }
      }
    return result;
  }

wxCHECK_MSG(tsb || oddity, MPR_fail, wxT("Trying to Move() a file with an invalid tsb")); // Check we have a tsb for a regular file
MovePasteResult result = Paste(From, &OldTo, ToName, from.IsDir(), false, false, tsb, OverwrittenFile); // For non-symlinks use the thread method
if (result && tsb)
  { if (result == MPR_completed) tsb->AddPreCreatedItem(From->GetFullPath(), To->GetFullPath());
    if (!from.IsRegularFile()) // Files self-delete inside the thread
      static_cast<PasteThreadSuperBlock*>(tsb)->DeleteAfter(from.GetFilepath());
  }

return result;
}

//static 
enum MovePasteResult MyGenericDirCtrl::Paste(wxFileName* From, wxFileName* To, const wxString& ToName, bool ItsADir,
                                               bool dir_skeleton/*=false*/, bool recursing/*=false*/, ThreadSuperBlock* tsb/*=NULL*/, const wxString& overwrittenfile/*=""*/)
{
wxLogNull log;

FileData stat(From->GetFullPath());

if (!stat.IsDir())                    // Check first to see if From isn't a dir.  If so, it's easy: just it, no progeny
  { enum MovePasteResult result = MPR_fail;
    if (dir_skeleton) return result;  // Easier still if we only *want* dirs
    
    wxString ToNamePath = ToName.BeforeLast(wxFILE_SEP_PATH), tooname(ToName);  // However these lines are in case ToName is foo/bar, and foo/ doesn't exist
    if (!ToNamePath.IsEmpty()) 
      { tooname = tooname.AfterLast(wxFILE_SEP_PATH);
        wxString fullpath( StrWithSep(To->GetPath()) );
        ToNamePath = fullpath + ToNamePath; if (ToNamePath.Last() != wxFILE_SEP_PATH) ToNamePath << wxFILE_SEP_PATH;
        To->Assign(ToNamePath);
        if (!To->DirExists())
          if (!To->Mkdir(0777, wxPATH_MKDIR_FULL))  return MPR_fail;            // wxPATH_MKDIR_FULL means intermediate dirs are created too
      }
    
    To->SetFullName(tooname);                                                   // Add the filename to create the desired dest. path/name

    if (stat.IsSymlink())                                                       // If it's a symlink, make a new one in destination dir
      { wxString symlinktarget = stat.GetSymlinkDestination(true);
        if (RETAIN_REL_TARGET && symlinktarget.GetChar(0) != wxFILE_SEP_PATH)   // If it's a relative symlink (& we want to) we need to be clever to keep the same target
          { wxFileName fn(symlinktarget); fn.MakeAbsolute(From->GetPath());     // So first make the target absolute, relative to the original symlink,
            fn.MakeRelativeTo(To->GetPath());  symlinktarget = fn.GetFullPath();//  then relative to the new one
          }
        if (!SafelyCloneSymlink(symlinktarget, To->GetFullPath(), stat))  return MPR_fail;
        result = MPR_completed;

        if (RETAIN_MTIME_ON_PASTE)
            FileData::ModifyFileTimes(To->GetFullPath(), From->GetFullPath());

        if (tsb)
          { tsb->AddPreCreatedItem(From->GetFullPath(), To->GetFullPath()); tsb->AddSuccesses(1); }
      }
     else
      if (stat.IsRegularFile())                                                 // If it's a normal file, use built-in wxCopyFile from a thread
        { wxCHECK_MSG(tsb, MPR_fail, wxT("Trying to Paste() a file with an invalid tsb"));
          tsb->AddToCollector(From->GetFullPath(), To->GetFullPath(), overwrittenfile);
          result = MPR_thread;
        }
     else
        { wxBusyCursor busy;                                                    // Otherwise it's an oddity like a fifo, which makes wxCopyFile hang, so do it the linux way
          mode_t oldUmask = umask(0);                                           // Temporarily change umask so that permissions are copied exactly
          bool ans = mknod(To->GetFullPath().mb_str(wxConvUTF8), stat.GetStatstruct()->st_mode, stat.GetStatstruct()->st_rdev);  // Technically this works.  Goodness knows if it achieves anything useful!
          umask(oldUmask);
          result = MPR_completed;
          
          if (tsb)
            { if (ans==0) { tsb->AddPreCreatedItem(From->GetFullPath(), To->GetFullPath()); tsb->AddSuccesses(1); }
                else tsb->AddFailures(1);
            }
        }

    return result;
  }
  
        // If From is a dir, we need to recreate a new subdir on .../.../To/, usually with the same name as the last segment of the 'From' one, but not if there was a name clash
        // Notice we actually change To, since we want to put everything into the new subdir anyway, and this way that happens automatically
bool AtLeastOneSuccess = false; MovePasteResult mpresult = MPR_completed;

if (!wxDirExists(From->GetFullPath()))  return MPR_fail;                        // Shouldn't happen
                              // Append the new pathname to the TO path. Use Assign not AppendDir, in case ToName is foo/bar/
wxString fullpath( StrWithSep(To->GetPath()) );
To->Assign(fullpath + StrWithSep(ToName));
if (!To->Mkdir(To->GetFullPath(), 0777, wxPATH_MKDIR_FULL)) return MPR_fail;    // Then use this to make a new dir

if (tsb)
  { tsb->AddPreCreatedItem(From->GetFullPath(), To->GetFullPath());
    PasteThreadSuperBlock* ptsb = dynamic_cast<PasteThreadSuperBlock*>(tsb);
    if (ptsb && RETAIN_MTIME_ON_PASTE) ptsb->StoreChangeMtimes(To->GetFullPath(), stat.ModificationTime());
  }
if (dir_skeleton) AtLeastOneSuccess = true; // For a skeleton, this dir may be the only success

wxDir dir(From->GetPath());                                                     // Use wxDir to deal sequentially with each child file & subdir
if (!dir.IsOpened())  return MPR_fail;
if (!dir.HasSubDirs() && !dir.HasFiles()) // If the dir has no progeny, we're OK
  { if (RETAIN_MTIME_ON_PASTE)
      FileData::ModifyFileTimes(To->GetFullPath(), From->GetFullPath());
       
    return MPR_completed;
  }

wxString filename;
                          // Go thru the dir, dealing first with files (unless we're pasting a dir skeleton, in which case don't!)
if (!dir_skeleton)
  { bool cont = dir.GetFirst(&filename, wxEmptyString, wxDIR_FILES | wxDIR_HIDDEN);
    while  (cont)
      { wxString file = wxFILE_SEP_PATH + filename;                             // Make filename into /filename
      
        class FileData stat(From->GetPath() + file);                            // Stat the file, to make sure it's not something else eg a symlink
        if (stat.IsSymlink())                                                   // If it's a symlink, make a new one in destination dir
          { wxString symlinktarget = stat.GetSymlinkDestination(true);        
            if (RETAIN_REL_TARGET && symlinktarget.GetChar(0) != wxFILE_SEP_PATH)  // If it's a relative symlink (& we want to) we need to be clever to keep the same target
              { wxFileName fn(symlinktarget); fn.MakeAbsolute(From->GetPath()); // So first make the target absolute, relative to the original symlink,
                fn.MakeRelativeTo(To->GetPath()); symlinktarget = fn.GetFullPath(); //  then relative to the new one
              }        

            if (SafelyCloneSymlink(symlinktarget, To->GetPath() + file, stat)) 
              { if (RETAIN_MTIME_ON_PASTE)
                  FileData::ModifyFileTimes(To->GetPath() + file, From->GetPath() + file);
                if (tsb)
                  { tsb->AddPreCreatedItem(From->GetFullPath(), To->GetFullPath()); tsb->AddSuccesses(1); }
                AtLeastOneSuccess = true; 
              }
          }
         else
           if (stat.IsRegularFile())                                            // If it's a normal file, use built-in wxCopyFile via a thread
            { wxCHECK_MSG(tsb, MPR_fail, wxT("Trying to Paste() a file with an invalid tsb"));
              tsb->AddToCollector(From->GetFullPath() + file, To->GetFullPath() + file, overwrittenfile);
              AtLeastOneSuccess = true; mpresult = MPR_thread;
            }
         else
            { mode_t oldUmask = umask(0);                                       // Otherwise it's an oddity like a fifo, which makes wxCopyFile hang, so do it the linux way
              if (!mknod(wxString(To->GetPath() + file).mb_str(wxConvUTF8), stat.GetStatstruct()->st_mode, stat.GetStatstruct()->st_rdev))  // Success returns 0
                { if (tsb)
                    { tsb->AddPreCreatedItem(From->GetFullPath(), To->GetFullPath()); tsb->AddSuccesses(1); }
                  AtLeastOneSuccess = true;
                }
              umask(oldUmask);
            }
        cont = dir.GetNext(&filename);
    }
  }
                          // and now with any subdirs
static wxString first_illegal_filepath;
if (!recursing) // The first time thru, construct an infinite-loop guard if needed
  { if (To->GetFullPath().StartsWith(From->GetFullPath()))
      first_illegal_filepath = To->GetFullPath();
  }

bool cont = dir.GetFirst(&filename, wxEmptyString, wxDIR_DIRS | wxDIR_HIDDEN);                          
while (cont)
  { wxString file = wxFILE_SEP_PATH + filename;
    FileData stat(From->GetPath() + file);                                      // Stat the 'dir', to make sure it's not a symlink pointing to a dir!
    if (stat.IsSymlink())
      { if (!dir_skeleton)                                                      // If so, only paste it if we're not doing a dir skeleton
         { if (SafelyCloneSymlink(From->GetPath() + file, To->GetPath() + file, stat))  
             { if (RETAIN_MTIME_ON_PASTE)
                  FileData::ModifyFileTimes(To->GetPath() + file, From->GetPath() + file);
             }
           AtLeastOneSuccess = true;   // If it's a symlink, make a new one in destination dir
         }
      }
     else
      { wxFileName subdir(From->GetPath() + wxFILE_SEP_PATH + filename + wxFILE_SEP_PATH);  // Make the subdir into a wxFileName
        // Check that we're not pasting a parent onto a child: legal (Move isn't) but need to avoid infinite looping
        // If we are, and we're already recursing, check the cached first_illegal_filepath
        if ((!first_illegal_filepath.empty()) && subdir.GetFullPath() == first_illegal_filepath)
              { cont = dir.GetNext(&filename); continue; }
        
        wxFileName copyofTo(To->GetPath() + wxFILE_SEP_PATH);                   // Why? Because the wxFileName we pass will be altered in the subroutine, and To mustn't be
        
        MovePasteResult result = Paste(&subdir, &copyofTo, filename, true, dir_skeleton, true, tsb); // Do the subdir Paste by recursion
        if (result > MPR_fail) AtLeastOneSuccess = true;
        if (result == MPR_thread) mpresult = MPR_thread; // Record whether it needed a thread
      }
    cont = dir.GetNext(&filename);
  }

return AtLeastOneSuccess ? mpresult : MPR_fail;
}

void MyGenericDirCtrl::OnShortcutCopy(wxCommandEvent& WXUNUSED(event))  // Ctrl-C
{
size_t count;                                           // No of selections (may be multiple, after all) 
wxArrayString paths;                                    // Array to store them

UnRedoManager::CloseSuperCluster();                     // This switches off any extant supercluster, eg if last entry was a Cut.  Prevents inappropriate aggregation of next Paste

count = GetMultiplePaths(paths);                        // Make the tree fill the array of selected pathname/filenames
if (!count) return;

MyGenericDirCtrl::filearray = paths;                    // Fill the "clipboard" with new data
MyGenericDirCtrl::FromID = GetId();                     //  and the window ID of this pane
MyGenericDirCtrl::filecount = count;                    // Store the count
MyGenericDirCtrl::Originpane = this;                    // and which pane we were in: need if this is an archive

CopyToClipboard(paths);                                 // FWIW, copy the filepath(s) to the real clipboard too

DoBriefLogStatus(count, wxEmptyString, _("copied"));
}

void MyGenericDirCtrl::OnShortcutPaste(wxCommandEvent& event)  // Ctrl-V
{
if (ThreadsManager::Get().PasteIsActive())  // We shouldn't try to do 2 pastes at a time, and it was probably a mistake anyway
 { BriefMessageBox(_("Please try again in a moment"), 2,_("I'm busy right now")); return; }

wxString destinationpath = GetPath();

                  // First, check that we're not hovering over an empty area of a file-pane.  If so, we need to use its root path as the destination
if (destinationpath.IsEmpty() && (fileview == ISRIGHT)) // If we're in a fileview pane, but not hovering over something, 
  { wxString rootpath = startdir;                       //  get the root path for the pane
    if (!rootpath.IsEmpty())
      destinationpath = rootpath;                       // If it's valid, use it as the path
     else return;                                       // Otherwise, stop wasting time
  }

  // We may have got here from a 'Paste dir skeleton' item. If so, remove non-dirs from the clipboard
  // Doing so isn't a complete solution, but it will avoid false alarms from the CheckDupRen tests
bool paste_dir_skeleton = (event.GetId() == SHCUT_PASTE_DIR_SKELETON);
MyGenericDirCtrl* origin = MyGenericDirCtrl::Originpane;
if (event.GetId() == SHCUT_PASTE_DIR_SKELETON)
   { for (size_t n = MyGenericDirCtrl::filecount; n > 0; --n)
      { if (origin && origin->arcman && origin->arcman->IsArchive())
          { FakeDir* fd = origin->arcman->GetArc()->Getffs()->GetRootDir()->FindSubdirByFilepath(MyGenericDirCtrl::filearray.Item(n-1));
            if (!fd)
              MyGenericDirCtrl::filearray.RemoveAt(n-1);
          }
         else
          { FileData fd(MyGenericDirCtrl::filearray.Item(n-1));
            if (!fd.IsDir() || (fd.GetFilepath() == destinationpath)) // Also check for pasting onto self
              MyGenericDirCtrl::filearray.RemoveAt(n-1);
          }
      }
    MyGenericDirCtrl::filecount = MyGenericDirCtrl::filearray.GetCount(); // in case there've been any removals
    }
if (!MyGenericDirCtrl::filecount) return;

UnRedoManager::ContinueSuperCluster();                  // Start a new cluster if from Copy, or continue the supercluster started by Cut
OnPaste(false, destinationpath, paste_dir_skeleton);
}

void MyGenericDirCtrl::OnPaste(bool FromDnD, const wxString& destinationpath/*=wxT("")*/, bool dir_skeleton/*=false*/)  // Ctrl-V or DnD
{
enum DupRen WhattodoifClash = DR_Unknown, WhattodoifCantRead = DR_Unknown; // These get passed to CheckDupRen.  May become SkipAll, OverwriteAll etc, but start with Unknown
size_t count;
wxArrayString filearray;
wxArrayInt IDs;
wxString path, DestPath;
MyGenericDirCtrl *originctrl, *destctrl;
                        // We may have got here from Ctrl-V or Dnd, so get data into local vars as appropriate
if (FromDnD)
  { count = MyGenericDirCtrl::DnDfilecount;
    filearray = MyGenericDirCtrl::DnDfilearray;
    IDs.Add(MyGenericDirCtrl::DnDFromID); IDs.Add(GetId()); // Store the IDs of the panes that have changed
    path = MyGenericDirCtrl::DnDDestfilepath;
    originctrl = MyFrame::mainframe->GetActivePane(); destctrl = this;  // Active is the origin MyGenericDirCtrl, 'this' is the destination one
  }
 else
  { count = MyGenericDirCtrl::filecount;
    filearray = MyGenericDirCtrl::filearray;
    IDs.Add(MyGenericDirCtrl::FromID); IDs.Add(GetId());  // Store the IDs of the panes that have changed
    
    FileData fd(destinationpath);
    if (fd.IsSymlink() && fd.IsSymlinktargetADir(true))   // If we're being asked to Paste onto a symlink, see if its target is a dir
      path = fd.GetUltimateDestination(); // If so, replace path with the symlink target.  NB If the target isn't a dir, we don't want to deref the symlink at all; just paste into its parent dir as usual
     else
      path = destinationpath;                   // OnShortcutPaste() processed the result of GetPath()
    originctrl = MyGenericDirCtrl::Originpane; destctrl = this;  // For Paste, the origin MyGenericDirCtrl was stored by Copy or Cut; 'this' is the destination one
  }

if (!count) return;                             // Nothing in the "Clipboard"
if (path.IsEmpty()) return;                     // Nowhere to put it (not very likely, but nevertheless)

                    // We need to check whether we're moving into or from within an archive: if so things are different

    // This copes with the standard scenario of the user dragging from one archive to the same part of the same one :/
if (arcman && arcman->GetArc() && arcman->GetArc()->DoThingsWithinSameArchive(originctrl, destctrl, filearray, path)) 
  { if (UnRedoManager::ClusterIsOpen) UnRedoManager::EndCluster();
    return; // It returned true, so the situation was sufficiently dealt with
  }

bool PastingFromTrash = filearray[0].StartsWith(DirectoryForDeletions::GetDeletedName()); // We'll need this if we'd Cut from an archive ->trash; & now we're Pasting it
if  (destctrl && destctrl->arcman != NULL && destctrl->arcman->IsArchive())
  { if (arcman->GetArc()->MovePaste(originctrl, destctrl, filearray, path, false, dir_skeleton)) // If we're pasting into an archive (either from one or not) sort things out elsewhere
      { if (dir_skeleton)  BriefLogStatus(_("Directory skeleton pasted"));
          else DoBriefLogStatus(count, wxT(""), _("pasted"));
      }
    if (UnRedoManager::ClusterIsOpen) UnRedoManager::EndCluster();
    return;
  }
if (!PastingFromTrash && originctrl && originctrl->arcman != NULL && originctrl->arcman->IsArchive()) // Check we're not coming from trash, and originctrl is no longer relevant
  { if (originctrl->arcman->GetArc()->MovePaste(originctrl, destctrl, filearray, path, false, dir_skeleton)) // If we're pasting FROM an archive into the outside world
      { if (dir_skeleton)  BriefLogStatus(_("Directory skeleton pasted"));
          else DoBriefLogStatus(filearray.GetCount(), wxT(""), _("pasted"));
      }
    if (UnRedoManager::ClusterIsOpen) UnRedoManager::EndCluster();
    return;
  }

FileData fd(path);                        // OK, back to non-archives
wxFileName Destination(path);                           // Make sure the destination path is a dir with a terminal /  It's probably missing it
if ((fd.IsDir() || fd.IsSymlinktargetADir(true))        // If the overall path is indeed a dir
             &&  !Destination.GetFullName().IsEmpty())  
    path += Destination.GetPathSeparator();             //   plonk a / on the end of path, for the future
  else 
    { Destination.SetFullName(wxEmptyString);           // If it IS a file, remove the filename, leaving the path for pasting to
      path = Destination.GetFullPath();
    }

FileData DestinationFD(path);
if (!DestinationFD.CanTHISUserWriteExec())              // Make sure we have permission to write to the destination dir
  { wxMessageDialog dialog(this, _("I'm afraid you don't have Permission to write to this Directory"), _("No Entry!"), wxOK | wxICON_ERROR);
    dialog.ShowModal(); 
    if (UnRedoManager::ClusterIsOpen) UnRedoManager::EndCluster();
    return;
  }

FileData OriginFileFD(filearray[0]);
if (!OriginFileFD.CanTHISUserPotentiallyCopy())         // Make sure we have exec permissions for the origin dir
  { wxMessageDialog dialog(this, _("I'm afraid you don't have permission to access files from this Directory"),  _("No Exit!")); 
    if (UnRedoManager::ClusterIsOpen) UnRedoManager::EndCluster();
    return; 
  }
    
PasteThreadSuperBlock* tsb = dynamic_cast<PasteThreadSuperBlock*>(ThreadsManager::Get().StartSuperblock(wxT("paste")));

int successes = 0;
for (size_t c=0; c < count; c++)                        // For every path in the clipboard to be pasted
  { wxString DestFilename;
    bool ItsADir;
                              // We need to check we're not about to do something illegal or immoral
    CheckDupRen CheckIt(this, filearray[c], path);      // Make an instance of the class that tests for this
    CheckIt.SetTSB(tsb);
    CheckIt.IsMultiple = FromDnD ? (DnDfilecount > 1) : (MyGenericDirCtrl::filecount > 1); // We use different dialogs for multiple items
    CheckIt.WhattodoifClash = WhattodoifClash; CheckIt.WhattodoifCantRead = WhattodoifCantRead;  // Set CheckIt's variables to match any previous choice
    
    if (!CheckIt.CheckForAccess())                      // This checks for Read permission failure, and sets a SkipAll flag if appropriate
      { WhattodoifCantRead = CheckIt.WhattodoifCantRead; continue; }
      // NB. No need for CheckIt.CheckForIncest() here: *pasting* onto a descendant is OK; it's moving that isn't
    CDR_Result QueryPreExists = CheckIt.CheckForPreExistence(); // Check we're not pasting onto ourselves or a namesake (& if so offer to resolve). True means OK
    if (QueryPreExists == CDR_skip)               
      { WhattodoifClash = CheckIt.WhattodoifClash;      // Store any new choice in local variable.  This catches SkipAll & Cancel, which return false
        if (WhattodoifClash==DR_Cancel) { c = count; break; }  // Cancel this item & the rest of the loop by increasing c
           else continue;                               // Skip this item but continue with loop
      }
        // If there was a clash & subsequent Rename, CheckDupRen will have changed the filename or the name of the subdir to be made
    DestPath = CheckIt.finalpath;                       // This will be either the original path without the filename/last dir-segment, or the altered version
    DestFilename = CheckIt.finalbit;                    // Ditto, but the filename/last-segment
    WhattodoifClash = CheckIt.WhattodoifClash;          // Store any new choice in local variable
    ItsADir = CheckIt.ItsADir;                          // CheckIt already tested whether it's a file or a dir.  Piggyback onto this, it saves constant rechecking

    if (ItsADir && filearray[c].Last() != wxFILE_SEP_PATH)
      filearray[c] += wxFILE_SEP_PATH;                  // If we're pasting a dir, make sure it ends in a /
      
    wxFileName From(filearray[c]);                      // Make the From path into a wxFileName
    wxFileName To(DestPath);                            // and the To path
  
    if (Paste(&From, &To, DestFilename, ItsADir, dir_skeleton, false, tsb))
      { // NB The Paste has altered To, appending to it the pasted file or subdir. Fortunately, this is just what we want for Undoing
        wxString frompath = From.GetFullPath(), topath = To.GetFullPath();
        UnRedoPaste* urp = new UnRedoPaste(frompath, IDs, topath, DestFilename, ItsADir, dir_skeleton, tsb);
        urp->SetOverwrittenFile( CheckIt.GetOverwrittenFilepath() ); // Store any saved overwritten filepath ready for undoing. Only needed for regular files using threads
        tsb->StoreUnRedoPaste(urp, frompath);
        ++successes;
      }
  }

tsb->AddOverallSuccesses(successes); tsb->AddOverallFailures(count - successes); tsb->SetMessageType(_("pasted"));
if (successes)
  tsb->StartThreads(); 
 else
  { ThreadsManager::Get().AbortThreadSuperblock(tsb); // Otherwise the thread will be orphaned and the progress throbber stuck on 0
    if (UnRedoManager::ClusterIsOpen) UnRedoManager::EndCluster();
    return;
  }

if (dir_skeleton)  BriefLogStatus(_("Directory skeleton pasted"));

                              // Finally we have to sort out the treectrl
if (fileview == ISLEFT)
  { destctrl->GetTreeCtrl()->UnselectAll();             // Unselect to avoid ambiguity due to multiple selections
    if (USE_FSWATCHER)                                  // If so use SelectPath(). We don't need the extra overhead of SetPath()
      { wxTreeItemId id = destctrl->FindIdForPath(DestPath);
        if (id.IsOk()) destctrl->GetTreeCtrl()->SelectItem(id);
      }
     else
        SetPath(DestPath);  // Needed when moving onto the 'root' dir, otherwise the selection won't alter & so the fileview won't be updated
  }

MyFrame::mainframe->OnUpdateTrees(DestPath, IDs);       // Ask for the Refresh. DestPath is correct, whether a plain paste or a dup. (This wouldn't be true of path)
}

void MyGenericDirCtrl::OnShortcutUndo(wxCommandEvent& event)  // Ctrl-Z
{
UnRedoManager::UndoEntry();
}

void MyGenericDirCtrl::OnShortcutRedo(wxCommandEvent& event)  // Ctrl-R
{
UnRedoManager::RedoEntry();
}

void DirGenericDirCtrl::OnToggleHidden(wxCommandEvent& event)  // Ctrl-H
{
bool hidden = !GetShowHidden();                           // Do the toggle
ShowHidden(hidden);                                       //  & make it happen
partner->ShowHidden(hidden);                              // Do it for fileview too, as it's difficult to imagine not wanting this

wxString path = GetPath();
UpdateStatusbarInfo(path);                                // Make it appear in the statusbar
}

void MyGenericDirCtrl::SetRootItemDataPath(const wxString& fpath)
{
wxTreeItemId rootId = GetTreeCtrl()->GetRootItem();
if (rootId.IsOk())
  { wxDirItemData& data = (wxDirItemData&)*(GetTreeCtrl()->GetItemData(rootId));
    data.m_path = fpath;
  }
}

wxTreeItemId MyGenericDirCtrl::FindIdForPath(const wxString& path) const
{
wxString pth = StripSep(path);
wxTreeCtrl* tree = GetTreeCtrl();
wxTreeItemId root = m_rootId;
if (!root.IsOk()) return root; // because of a race condition?

wxTreeItemId item = root;                             // Since we don't know where to look in the tree, start from root
wxDirItemData* data = (wxDirItemData*)tree->GetItemData(item);
while (data && StripSep(data->m_path) != pth)
  { item = tree->GetNext(item);                       // Get the next leaf in the tree
    if (!item.IsOk()) return item;                    // If item is corrupt, return as invalid
    if (item==root) return wxTreeItemId();            // If we've traversed the whole tree without success, signal by returning invalid
    data = (wxDirItemData*)tree->GetItemData(item);   // OK, try this leaf
  }

return item;                                          // If we're here, must have matched  
}

#if defined(__LINUX__) && defined(__WXGTK__)
bool CopeWithCreateRename(InotifyEventType& type, wxString& alteredfilepath, wxString& newfilepath)
{
FileData fd(newfilepath);
if (fd.IsValid())
  { // If we're here, alteredfilepath was created, then instantly renamed to newfilepath. So pretend it was an IET_create all along
    type = IET_create;
    alteredfilepath = newfilepath;
    return true;
  }
return false;
}

void MyGenericDirCtrl::UpdateTree(InotifyEventType type, const wxString& alteredfilepath, const wxString& newfilepath /*= wxT("")*/)
{
wxCHECK_RET(!alteredfilepath.empty(), wxT("Called UpdateTree() with empty fpath"));
wxTreeItemId item;

try 
  {
    if (startdir != wxT("/") && ((fileview == ISRIGHT) || !fulltree))
      { // I started by using plain wxString::StartsWith here, but that was broken by a rename of /path/to/foo to /path/to/foo1
        wxFileName altfp(StripSep(alteredfilepath));
        wxFileName startdirfp(StripSep(startdir));

                // startdir /foo, alteredfilepath /foo/bar/baz
        if (altfp.GetPath().StartsWith(startdirfp.GetPath()) && (altfp.GetDirCount() > startdirfp.GetDirCount()))
          { item = FindIdForPath(alteredfilepath);            // If it's downstream of startdir, update it and all items below
            // We need to cope with the situation where a dir is created, then immediately renamed; dpkg does this when installing.
            // If the rename is immediate, the events will be sent in a single cluster, so the create will be overridden by the rename.
            // We'll therefore arrive here with an invalid item as the created dirname wasn't added to the tree
            if (!item.IsOk() && type == IET_rename)
              { wxString altfp(alteredfilepath); wxString newfp(newfilepath);
                if (CopeWithCreateRename(type, altfp, newfp)) // Pretend it was a newfp create
                  DoUpdateTree(item, type, altfp, newfp);
              }
             else
              DoUpdateTree(item, type, alteredfilepath, newfilepath);
          }
                // startdir /foo/bar/baz and alteredfilepath /foo/, or /foo/ and /foo/
         else if ((startdirfp.GetPath().StartsWith(altfp.GetPath()) && (altfp.GetDirCount() < startdirfp.GetDirCount()))
                  || (StripSep(startdir) == StripSep(alteredfilepath)))
          { item = FindIdForPath(startdir);                   // If it's upstream, or startdir itself, update everything
            DoUpdateTree(item, type, alteredfilepath, newfilepath);
            // The whole tree is affected, so we also need to update startdir etc
            startdir.replace(0, alteredfilepath.Len(), newfilepath);
            m_defaultPath = startdir;
            return;
          }
      }
     else // This copes with fulltree mode too, and also startdir == "/"
      { wxString path = alteredfilepath.BeforeLast(wxFILE_SEP_PATH); 
        if (path.empty()) path = wxT("/");
        if (wxStaticCast(this, DirGenericDirCtrl)->IsDirVisible(alteredfilepath)
              || ((type == IET_create || type == IET_delete || type == IET_rename) && wxStaticCast(this, DirGenericDirCtrl)->IsDirVisible(path)))
          { item = FindIdForPath(alteredfilepath);            // There's no difference between upstream/downstream in a fulltree dirview
            if (!item.IsOk()) // (See above for the reasoning)
              { wxString altfp(alteredfilepath); wxString newfp(newfilepath);
                if (CopeWithCreateRename(type, altfp, newfp))
                  DoUpdateTree(item, type, altfp, newfp);
              }
             else
              DoUpdateTree(item, type, alteredfilepath, newfilepath);
          }
      }

     // If we're here, alteredfilepath is orthogonal to startdir, so nothing to do
  }
catch(...)
  { wxLogWarning(wxT("Caught an unknown exception in MyGenericDirCtrl::UpdateTree")); }
}

void MyGenericDirCtrl::DoUpdateTree(wxTreeItemId id, InotifyEventType type, const wxString& oldfilepath, const wxString& newfilepath)
{
// There are 5 possibilities: 1) a rename. The 2 filenames will differ, though the paths will be the same
//                            2) a move between dirs within a watch e.g. foo/bar/baz -> foo/baz. The paths will differ, but not the names, and id will be valid
//                            3) a creation (or move into a dir from a different watch). They'll be identical, and id will be invalid
//                            4) a deletion (or move out of a dir to a different watch). Identical and id will be valid
//                            5) a 'touch' or similar. Identical and id will be valid
wxTreeItemId parentitem;
wxTreeCtrl* tree = GetTreeCtrl();

if (type == IET_rename)
  { try 
      {
        wxString sortdir = (id == GetRootId()) ? wxT("") : StripSep(oldfilepath).BeforeLast(wxFILE_SEP_PATH); // Deduce the filepath that may need its children sorted

        if (oldfilepath.AfterLast(wxFILE_SEP_PATH) != (newfilepath.AfterLast(wxFILE_SEP_PATH)))  // A genuine rename
          { if (fileview == ISRIGHT) return ReCreateTreeFromSelection(); // It's too hard to re-sort a fileview if we try to be clever

            // For a dirview reordering is easier

            DoUpdateTreeRename(id, oldfilepath, newfilepath);

            if (!sortdir.empty())
              { wxTreeItemId sortId = FindIdForPath(sortdir);
                if (sortId.IsOk())
                  tree->SortChildren(sortId);
              }
            if (StripSep(partner->startdir) == StripSep(oldfilepath))
              { wxWindow* currentfocus = wxGetApp().GetFocusController().GetCurrentFocus();

                partner->startdir = StrWithSep(newfilepath);  // Keep partner in sync if need be
                partner->SetRootItemDataPath(newfilepath);
                partner->ReCreateTreeFromSelection();
                
                if (this == currentfocus)                     // Update the toolbar textctrl if appropriate
                  DisplaySelectionInTBTextCtrl(newfilepath);  // If we had focus, that means the new name
                else if (partner == currentfocus)
                  DisplaySelectionInTBTextCtrl(wxT(""));      // If partner did, it's too difficult so clear it
              }

            return;
          }
         else
          { // A 'move-within-watch'. Sometimes the _FROM and _TO inotify events cluster and we get both the oldpath and newpath. Sometimes not...
            FileData fd(newfilepath);
            if (!fd.IsDir() && (fileview == ISLEFT)) // If the moved item isn't a dir and we're a dirview, then it won't be visible anyway
              return;

            if (oldfilepath != newfilepath)
              { // Ask for a 'delete' followed by a 'create'
                if (id.IsOk())        // It won't be OK if this pane has already been refreshed, or for a dir inside a collapsed parent dir
                  { DoUpdateTree(id, IET_delete, oldfilepath, oldfilepath);
                    id = FindIdForPath(newfilepath);  // it'll be stale
                  }
                 else                 // Deal with the case of a collapsed parent dir losing its last child; it needs to lose its triangle too
                  { MyGenericDirCtrl* gdg = (fileview == ISLEFT) ? this : partner;
                    wxStaticCast(gdg, DirGenericDirCtrl)->CheckChildDirCount(sortdir);
                  }

                if (!id.IsOk())       // It won't be !OK if this pane has already been refreshed
                    DoUpdateTree(id, IET_create, newfilepath, newfilepath);

                return;
              }
             else
              { // Ask the file system if the filepath exists. That should distinguish between a move from or to
                if (!fd.IsValid())    // If invalid it must be a move away
                  { if (id.IsOk())    // It won't be OK if this pane has already been refreshed
                      DoUpdateTree(id, IET_delete, oldfilepath, oldfilepath);
                  }
                 else                 // A move to
                  { if (!id.IsOk())   // It won't be !OK if this pane has already been refreshed
                      DoUpdateTree(id, IET_create, oldfilepath, oldfilepath);
                  }

                return;
              }
          }
      }
    catch(...)
      { wxLogWarning(wxT("Caught an unknown exception in MyGenericDirCtrl::DoUpdateTree IET_rename")); }
  }

// A 'creation' or 'move into'
if (type == IET_create)
  { try 
      { if (!id.IsOk()) // If id.IsOk the pane may have been already refreshed, probably due to multiple events; but see the 'else' branch
          { wxString path = StripSep(oldfilepath).BeforeLast(wxFILE_SEP_PATH);
            if (path.empty()) path = wxT("/");
            parentitem = FindIdForPath(path);
            if (!parentitem.IsOk())
              { wxLogTrace(wxTRACE_FSWATCHER, wxT("Failed to find a parentitem for filepath %s"), oldfilepath.c_str());
                return; // This can happen if the original filepath was startdir, and has actually been deleted
              }
            bool WasSelected = tree->IsSelected(parentitem);

            ReCreateTreeBranch(&parentitem);
            parentitem = FindIdForPath(StripSep(oldfilepath).BeforeLast(wxFILE_SEP_PATH)); // as parentitem is now (I think) invalid

            if ((fileview == ISLEFT) && (this == MyFrame::mainframe->GetActivePane()) && WasSelected)
              m_StatusbarInfoValid = false;  // If a dirview is active and parent was selected, it must update the statusbar
             else if ((fileview == ISRIGHT) && (partner == MyFrame::mainframe->GetActivePane()))
              partner->m_StatusbarInfoValid = false;  // ditto
          }
         else if (fileview == ISRIGHT)
          { // Even if id.IsOk() we should update the item in a fileview. Someone may have overwritten foo.txt with a different foo.txt, so size etc needs updating
            wxStaticCast(this, FileGenericDirCtrl)->UpdateFileDataArray(oldfilepath);
            GetTreeCtrl()->Refresh();
          }

        if ((fileview == ISLEFT) && parentitem.IsOk()) // We need to do things here for dir additions/removals to keep the parent dir's button sane
          { wxDirItemData* data = (wxDirItemData*)tree->GetItemData(parentitem);
            if (data->m_path != StripSep(startdir)) // We always want a 'haschildren' marker for startdir; it looks odd otherwise
              { tree->SetItemHasChildren(parentitem, true); // Check as a deletion may have been the last subdir, & a creation may have been a file
              }
          }
      }
    catch(...)
      { wxLogWarning(wxT("Caught an unknown exception in MyGenericDirCtrl::DoUpdateTree IET_create")); }
  }

// A 'deletion' or 'move away'
if (type == IET_delete)
  { try 
      { if (id.IsOk()) // If not OK, either the parent is collapsed or else the pane must have been already refreshed, probably due to multiple events
         {
          bool WasSelected = tree->IsSelected(id);
          wxDirItemData* data = (wxDirItemData*)tree->GetItemData(id);
          bool WasDir = data && data->m_isDir;
          tree->Delete(id);
          // There are some situations where we need explicitly to remove the watch, as there's no collapse/expand/SetPath
          // and otherwise we risk an "Already watched" assert if the path is recreated e.g. by an undo. Anyway, it's not expensive if the watch doesn't exist
          //wxLogDebug(wxT("IET_delete: *** About to call RemoveWatchLineage %s"), oldfilepath.c_str()); 
          m_watcher->RemoveWatchLineage(oldfilepath);

          if ((fileview == ISRIGHT) && WasDir)                                      // If the dirview isn't expanded, it won't be watching any child dirs
            wxStaticCast(partner, DirGenericDirCtrl)->CheckChildDirCount(startdir); // So tell it that one of them has died; if none are left it should lose its triangle

          if ((fileview == ISRIGHT) && GetTreeCtrl()->GetRootItem().IsOk()) // It might not be OK if the dirview has already been updated
            { ReCreateTree();
              wxStaticCast(this, FileGenericDirCtrl)->DeleteFromFileDataArray(StripSep(oldfilepath)); // Adjust CumFilesize etc
              id = wxTreeItemId(); // This is now invalid
            }

          if (WasSelected && this == MyFrame::mainframe->GetActivePane()) // If the deleted item was selected, we need to clear the statusbar
            MyFrame::mainframe->SetStatusText(wxEmptyString, 2);
           else
            { if (fileview == ISRIGHT)
                { if (partner == MyFrame::mainframe->GetActivePane())
                    partner->m_StatusbarInfoValid = false; // If the dirview is active, it must update the statusbar
                }
            }
         }

        if (fileview == ISLEFT) // Even if id != OK, we need to do things here for dir removals to keep the parent dir's button sane
          { parentitem = FindIdForPath(StripSep(oldfilepath).BeforeLast(wxFILE_SEP_PATH));
            if (parentitem.IsOk())
              { wxDirItemData* pdata = (wxDirItemData*)tree->GetItemData(parentitem);
                if (pdata->m_path != StripSep(startdir)) // We always want a 'haschildren' marker for startdir; it looks odd otherwise
                  { tree->SetItemHasChildren(parentitem, (pdata && pdata->HasSubDirs())); // Check as a deletion may have been the last subdir
                  }
              }
          }
      }
    catch(...)
      { wxLogWarning(wxT("Caught an unknown exception in MyGenericDirCtrl::DoUpdateTree IET_delete")); }
  }

    // A 'touch' or similar, just updating metadata
if ((type == IET_attribute) && (fileview == ISRIGHT)) // I don't think we care about a dir in a dirview being touched
  { try 
      { wxStaticCast(this, FileGenericDirCtrl)->UpdateFileDataArray(oldfilepath);
        GetTreeCtrl()->Refresh();
      }
    catch(...)
      { wxLogWarning(wxT("Caught an unknown exception in MyGenericDirCtrl::DoUpdateTree IET_attribute")); }
  }
}

void MyGenericDirCtrl::DoUpdateTreeRename(wxTreeItemId id, const wxString& oldfilepath, const wxString& newfilepath)
{
if (!id.IsOk()) return;

//m_watcher->PrintWatches(wxT("DoUpdateTreeRename:");
wxTreeCtrl* tree = GetTreeCtrl();
wxDirItemData* data = (wxDirItemData*)tree->GetItemData(id);
if (!data) return;
wxCHECK_RET((StripSep(data->m_path) == StripSep(oldfilepath)) || (data->m_path.StartsWith(StrWithSep(oldfilepath))), wxT("Trying to update a treeitem unnecessarily"));

data->m_path.replace(0, oldfilepath.Len(), newfilepath);
if (oldfilepath != startdir)
  tree->SetItemText(id, data->m_path.AfterLast(wxFILE_SEP_PATH));
 else                                                                   // Generally we use the filename as label, but not for startdir
  { tree->SetItemText(id, data->m_path);
    startdir = newfilepath;
  }

wxTreeItemIdValue cookie;
wxTreeItemId child = tree->GetFirstChild(id, cookie); // Now do all descendents by recursion
while (child.IsOk())
  { DoUpdateTreeRename(child, oldfilepath, newfilepath);
    child = tree->GetNextChild(id, cookie);
  }
}
#endif // defined(__LINUX__) && defined(__WXGTK__)

void DirGenericDirCtrl::ReloadTree(wxString path, wxArrayInt& IDs)  // Either F5 or after an Cut/Paste/Del/UnRedo, to keep treectrl congruent with reality
{
  // First decide whether it's important for this pane to be refreshed ie if it is/was the active pane, or otherwise if a file/dir that's just been acted upon is visible here
bool flag = false;
wxWindowID pane = GetId();                                        // This is us
for (unsigned int c=0; c < IDs.GetCount(); c++)                   // For every ID passed here,
  if (IDs[c] == pane)  { flag = true; break; }                    //  see if it's us

    // There are 2 possibilities.  Either the dir that's just been acted upon is visible, or its not.  Remember, invisible means no TreeItemId exists
wxTreeItemId item = FindIdForPath(path);                          // Translate path into wxTreeItemId
if (!item.IsOk())                                                 // If it's not valid,
  { if (path  != wxFILE_SEP_PATH)                                 //   & it's not root!
      { while (path.Right(1) == wxFILE_SEP_PATH && path.Len() > 1) //     and if it's got a terminal '/' or two 
                path.RemoveLast();                                //       remove it and try again without.
        item = FindIdForPath(path);                               // This is cos the last dir of a branch doesn't have them, yet path DOES, so would never match
      }
  }

if (item.IsOk())                                // If it's valid, the item is visible, so definitely refresh it
  { wxTreeCtrl* tree = GetTreeCtrl();
    bool subs = true;                           // If this is the root, we need a +/- box, otherwise there is only a dangling line coming from nowhere
    if (tree->GetItemParent(item) != GetRootId())
      { if (arcman != NULL && arcman->IsArchive())
          { FakeDir* fakedir = arcman->GetArc()->Getffs()->GetRootDir()->FindSubdirByFilepath(path);
              subs = (fakedir != NULL && fakedir->SubDirCount() > 0);
          }
         else subs = HasThisDirSubdirs(path);   // It saves a lot of faffing with the treectrl if we use this global function to do a real-life count of subdirs --- remember this is a dirview so children must be dirs
      }
    tree->SetItemHasChildren(item, subs);        

    if (((wxDirItemData *) tree->GetItemData(item))->m_isExpanded) tree->Collapse(item);  // Do the refresh by (collapsing if needed &) expanding  
    tree->Expand(item);
  }

 else                                           // Otherwise the path isn't visible
   if (flag)                                    // If the pane must be refreshed,
     ExpandPath(path);                          //  this should make it so

if (path == GetPath() || (path+wxFILE_SEP_PATH) == GetPath())  // (Note another '/' problem) Finally, if the altered item is one selected on this pane,
  partner->ReCreateTreeFromSelection();         // tell file-partner to refresh
}

void DirGenericDirCtrl::GetVisibleDirs(wxTreeItemId rootitem, wxArrayString& dirs) const // Find which dirs can be seen i.e. children + expanded children's children
{
wxTreeItemIdValue cookie;

wxTreeItemId childId = GetTreeCtrl()->GetFirstChild(rootitem, cookie);
while (childId.IsOk())
  { wxDirItemData* data = (wxDirItemData*)GetTreeCtrl()->GetItemData(childId);
    if (data && !data->m_path.IsEmpty())
      { dirs.Add(data->m_path);                 // Add the child path
//        wxLogDebug(wxT("Adding %s"), data->m_path.c_str());
        if (GetTreeCtrl()->IsExpanded(childId)) // If it's expanded, add its children too, recursively if need be
          GetVisibleDirs(childId, dirs);
      }

    childId = GetTreeCtrl()->GetNextChild(rootitem, cookie);
  }
}

bool DirGenericDirCtrl::IsDirVisible(const wxString& fpath) const
{
wxString fp = StripSep(fpath);
if (fp == StripSep(startdir))
  return true;  // We can reasonably assume that startdir is visible

wxArrayString dirs;
if (fulltree)
  GetVisibleDirs(GetTreeCtrl()->GetRootItem(), dirs);
 else
  { wxTreeItemId id = FindIdForPath(startdir);
    if (!id.IsOk()) return false;
    GetVisibleDirs(id, dirs);
  }

for (size_t n=0; n < dirs.GetCount(); ++n)
  if (fp == StripSep(dirs[n]))
    return true;

return false;
}

BEGIN_EVENT_TABLE(DirGenericDirCtrl,MyGenericDirCtrl)
  EVT_MENU(SHCUT_NEW, DirGenericDirCtrl::NewFile)
  EVT_MENU(SHCUT_TOGGLEHIDDEN, DirGenericDirCtrl::OnToggleHidden)
  EVT_CONTEXT_MENU(DirGenericDirCtrl::ShowContextMenu)
END_EVENT_TABLE()

//-------------------------------------------------------------------------------------------------------------------------------------------

NavigationManager::~NavigationManager()
{
for (size_t n=0; n < GetCount(); ++n)
  delete m_NavigationData.at(n);

m_NavigationData.clear();
}

void NavigationManager::PushEntry(const wxString& dir, const wxString& path/*= wxT("")*/)
{
wxCHECK_RET(!dir.empty(), wxT("Trying to add an empty filepath"));

  // Check that there haven't already been some 'undo's.  If so, adding another one invalidates any distal entries, so delete them
if (m_NavDataIndex+1 < (int)GetCount())
  { for (size_t n = m_NavDataIndex+1; n < GetCount(); ++n)
      delete m_NavigationData.at(n);
    m_NavigationData.erase(m_NavigationData.begin() + m_NavDataIndex+1, m_NavigationData.end());
  }

if (GetCount() == MAX_NUMBER_OF_PREVIOUSDIRS)
  { delete m_NavigationData.front();  // Oops: too many entries.  Do a FIFO by removing oldest entry
    m_NavigationData.erase(m_NavigationData.begin());
    m_NavDataIndex = wxMin(m_NavDataIndex, GetCount() - 1);
  }

  // In case we've been navigating in a circle; we don't want the back-button to make us march on the spot
if (StripSep(GetCurrentDir()) == StripSep(dir)) return; 

  // Hack path into the _current_ navdata, which should hold the _current_ dir. This sets its selection correctly if we return to it
if (GetCount() && !path.empty())
  { FileData fd(GetCurrentDir());
    if (fd.IsRegularFile()) // If so, it's probably an archive that was being browsed & we're now coming out of
      wxASSERT(StripSep(path).Contains(StripSep(fd.GetPath()))); // so test its containing dir instead
     else
      wxASSERT(StripSep(path).Contains(StripSep(GetCurrentDir())));
    SetPath(m_NavDataIndex, path);
  }

m_NavigationData.push_back( new NavigationData(dir, wxT("")) );
++m_NavDataIndex;
}

wxArrayString NavigationManager::RetrieveEntry(int displacement) // Returns the currently indexed entry and inc/decs the index
{
wxArrayString arr;
wxCHECK_MSG(GetCount() > 0, arr, wxT("Trying to retrieve from an empty array"));

bool invalid(false);
if (displacement > 0)                                     // If +ve, we're going forwards
  { if (m_NavDataIndex + displacement >= (int)GetCount()) invalid = true; }
 else                                    // Otherwise we're going backwards
  { if (m_NavDataIndex + displacement < 0) invalid = true; }

if (invalid) return arr; // This would previously have triggered an assert but, now that there are keyboard shortcuts with no UpdateUI protection, overruns are easy & legitimate

m_NavDataIndex += displacement; // Move the index to requested entry. Note the += works even if displacement is negative

arr.Add( GetCurrentDir() );
arr.Add( GetCurrentPath() );
return arr;
}

void NavigationManager::DeleteInvalidEntry()
{
wxCHECK_RET(GetCount() > 0, wxT("Trying to remove from an empty array"));
wxCHECK_RET(m_NavDataIndex > -1, "Trying to remove from before array start");

delete m_NavigationData.at(m_NavDataIndex);
m_NavigationData.erase(m_NavigationData.begin() + m_NavDataIndex);
--m_NavDataIndex; // Dec the index. If the first item was deleted the index -> -1, which IIUC is correct...
}

wxArrayString NavigationManager::GetBestValidHistoryItem(const wxString& deadfilepath) // Retrieves the first valid dir from the history. Used when an IN_UNMOUNT event is caught
{
wxArrayString arr;

bool valid(false); // First find the mountpt, which must be the first valid dir
wxString mountpt = StrWithSep(deadfilepath); // We need the StrWithSep(), otherwise we hang if mountpt was e.g. '/mntpt'
do
  { mountpt = mountpt.BeforeLast(wxFILE_SEP_PATH);
    FileData fd(mountpt); valid = fd.IsValid();
  }
 while (!valid);

for (size_t n = GetCount(); n > 0; --n)
  { wxString entry = m_NavigationData.back()->GetDir();
    FileData fd(entry);
    if (!fd.IsValid() // This filepath must have been within the now-unmounted filesystem. (Or maybe inside an archive, but who cares...)
          || StripSep(entry).StartsWith(StripSep(deadfilepath)) ) // We don't want to revert to a previous instance of the same/similar filepath!
      { delete m_NavigationData.back(); m_NavigationData.pop_back();
        if (m_NavDataIndex > 0 && (size_t)m_NavDataIndex < n) --m_NavDataIndex; // If we've just deleted its target, adjust the you-are-here marker too
      }
  }

if (!mountpt.empty() && GetCount() > 0 && (StripSep(mountpt) == StripSep(m_NavigationData.back()->GetDir()))) // We don't want to end up in the now-empty mountpoint
  { delete m_NavigationData.back(); m_NavigationData.pop_back();
    --m_NavDataIndex;
  }

if (GetCount() && (m_NavDataIndex < (int)GetCount())) // sanity
  { arr.Add( GetCurrentDir() ); arr.Add( GetCurrentPath() ); }
 else  arr.Add(wxGetHomeDir());

return arr;
}


wxString NavigationManager::GetCurrentDir() const
{
if (m_NavDataIndex < 0 || !GetCount()) return wxT("");
return GetDir(m_NavDataIndex);
}

wxString NavigationManager::GetDir(size_t index) const
{
if (m_NavDataIndex < 0 || !GetCount()) return wxT("");
wxCHECK_MSG(index < GetCount(), wxT(""), wxT("Trying to access an unindexed navdir"));

return m_NavigationData.at(index)->GetDir();
}

wxString NavigationManager::GetCurrentPath() const
{
if (m_NavDataIndex < 0 || !GetCount()) return wxT("");
return GetPath(m_NavDataIndex);
}

wxString NavigationManager::GetPath(size_t index) const
{
if (m_NavDataIndex < 0 || !GetCount()) return wxT("");
wxCHECK_MSG(index < GetCount(), wxT(""), wxT("Trying to access an unindexed navdir"));

return m_NavigationData.at(index)->GetPath();
}

void NavigationManager::SetPath(size_t index, const wxString& path)
{
if (m_NavDataIndex < 0 || !GetCount()) return;
wxCHECK_RET(index < GetCount(), wxT("Trying to access an unindexed navdir"));

m_NavigationData.at(index)->SetPath(path);
}


//-------------------------------------------------------------------------------------------------------------------------------------------
#if defined(__LINUX__) && defined(__WXGTK__)
void MyFSEventManager::AddEvent(wxFileSystemWatcherEvent& event)
{
wxCHECK_RET(m_owner, wxT("NULL owner"));

try 
  {
    int changetype = event.GetChangeType();
    wxString filepath = StripSep(event.GetPath().GetFullPath());  // First check that we need to notice this event
    wxString origfilepath(filepath); // Cache it in case we truncate it
    if (m_owner->fileview == ISRIGHT)
      { if (!filepath.StartsWith(m_owner->startdir)         // e.g. startdir is /foo,  filepath /foo/bar/baz
              && !m_owner->startdir.StartsWith(filepath))   // or startdir is /foo/bar/baz/, filepath /foo/
          return;
      }
     else // This copes with fulltree mode too
      if (!wxStaticCast(m_owner, DirGenericDirCtrl)->IsDirVisible(filepath) // Check the dir is visible, or for a creation/deletion/move, the parent dir
            && !((changetype == wxFSW_EVENT_CREATE || changetype == wxFSW_EVENT_DELETE || changetype == wxFSW_EVENT_RENAME) 
                                          && wxStaticCast(m_owner, DirGenericDirCtrl)->IsDirVisible(event.GetPath().GetPath())))
        return;

    if ((changetype == wxFSW_EVENT_CREATE) && !event.GetPath().IsDir())
      { 
#if wxVERSION_NUMBER < 3000 
     // For non-dir creates/moves it's almost good enough to refresh just the path, not the filepath, and that way if 2000 files are pasted we won't update 2000 times.
     // However that risks a race-condition where the path is refreshed before all the files are actually created; a race that was being occasionally lost.
     // So for >=wx3.0.0 actually use the filepath; even for 10000 files there was no perceptible slowdown.
     // But earlier wx versions, perhaps because of the wxString differences in wx3, took far, far longer (in FindIdForPath()) so for these versions use the path.
        filepath = filepath.BeforeLast(wxFILE_SEP_PATH);
#endif
        FileData fd(origfilepath);
        if (fd.IsSymlink()) // For symlink creates, check if there's an out-of-order Delete; we need to use the filepath for that, not just the path
          { FilepathEventMap::iterator delete_iter = m_Eventmap.find(origfilepath);
            if (delete_iter != m_Eventmap.end())
              { wxFileSystemWatcherEvent* oldevent = delete_iter->second;
                if (oldevent->GetChangeType() == wxFSW_EVENT_DELETE) // We found an unwanted Delete; this might happen when retargetting the symlink
                 { m_Eventmap.erase(delete_iter); // If we don't kill it, it removes the link and the Create event fails to redisplay it
                   delete oldevent;
                 }
              }
          }
      }

    FilepathEventMap::iterator iter = m_Eventmap.find(filepath);
    if (iter == m_Eventmap.end())
      { m_Eventmap.insert(FilepathEventMap::value_type(filepath, wxStaticCast(event.Clone(), wxFileSystemWatcherEvent)));
        iter = m_Eventmap.find(filepath);
        wxCHECK_RET(iter != m_Eventmap.end(), wxT("Failed to insert an element"));
      }
     else
      { // If there's already an event for this filepath, see if it's a dup, or if we need to overwrite the original, or...
        wxFileSystemWatcherEvent* oldevent = iter->second;
        if (oldevent->GetChangeType() == changetype)
          return;

        // If the new event is more important than the old, replace. 'More important' means (by chance) a lower value, except for renames and umounts
        // An umount trumps everything
        // A move-in rename is effectively a create, so trumps everything else. A move-out is a delete, so trumps everything bar a umount & create
        // As we can't tell the difference from the event (unless it's a genuine rename with 2 different filenames), ask the filesystem
        // (However for readability, don't replace a CREATE with a pseudo-create, or a DELETE with a pseudo-delete)
        if (oldevent->GetChangeType() == wxFSW_EVENT_UNMOUNT)
          return;
        bool ignore = (oldevent->GetChangeType() < changetype)
            || ((oldevent->GetChangeType() == wxFSW_EVENT_RENAME) && (oldevent->GetPath() != oldevent->GetNewPath())); // Don't clobber a stored genuine rename

        if (changetype == wxFSW_EVENT_RENAME)
          { if (StripSep(event.GetNewPath().GetFullPath()) == filepath)
              { FileData fd(filepath);
                if ((fd.IsValid() && oldevent->GetChangeType() != wxFSW_EVENT_CREATE) || (!fd.IsValid() && oldevent->GetChangeType() != wxFSW_EVENT_DELETE))
                  ignore = false;
              }
             else
              ignore = false; // A genuine rename, so definitely use it
          }
        if (ignore)
          return;

        m_Eventmap.erase(iter);
        delete oldevent;

        m_Eventmap.insert(FilepathEventMap::value_type(filepath, wxStaticCast(event.Clone(), wxFileSystemWatcherEvent)));
        iter = m_Eventmap.find(filepath);
        wxCHECK_RET(iter != m_Eventmap.end(), wxT("Failed to re-insert an element"));
       
      }

    if (!m_EventSent)
      { wxNotifyEvent evt(FSWatcherProcessEvents, wxID_ANY);
        evt.SetInt(10); // The no of times to catch the event before taking action. 10 sounds a lot, but doesn't noticeably slow things down
        wxPostEvent(m_owner, evt);
        m_EventSent = true;
      }
  }
catch(...)
  { wxLogWarning(wxT("Caught an unknown exception in MyFSEventManager::AddEvent")); }
}

void MyFSEventManager::ProcessStoredEvents()
{
wxCHECK_RET(m_owner, wxT("NULL owner"));

m_EventSent = false;

try 
  {
    for (FilepathEventMap::iterator iter = m_Eventmap.begin(); iter != m_Eventmap.end(); ++iter)
      { wxFileSystemWatcherEvent* event = iter->second;

        wxString filepath = iter->first;
        DoProcessStoredEvent(filepath.c_str(), event); // A deep copy here might avoid a race condition in wxFSW_EVENT_UNMOUNT 
      }
    
    while (m_Eventmap.size())
      { delete m_Eventmap.begin()->second; m_Eventmap.erase(m_Eventmap.begin()); }
    m_Eventmap.clear();
  }
catch(...)
  { wxLogWarning(wxT("Caught an unknown exception in MyFSEventManager::ProcessStoredEvents")); }
}

void MyFSEventManager::DoProcessStoredEvent(const wxString& filepath, const wxFileSystemWatcherEvent* event)
{
wxCHECK_RET(m_owner, wxT("NULL owner"));
bool isright = m_owner->fileview == ISRIGHT;
bool isleft = !isright; // readability


wxString newfilepath = event->GetNewPath().GetFullPath();  // For renames
try 
  {
    switch(event->GetChangeType())
      { case wxFSW_EVENT_CREATE:
            { wxString realfilepath = event->GetPath().GetFullPath(); // For Creates, we indexed by path, not filepath, so greatly reducing duplicate updates
              if (isright)
                m_owner->UpdateTree(IET_create, realfilepath, realfilepath);
               else // Otherwise refresh the path of the new dir
                { FileData fd(realfilepath);
                  if (fd.IsDir()) // dirviews don't care about non-dirs
                    { m_owner->UpdateTree(IET_create, realfilepath, realfilepath);
                      m_owner->m_watcher->RefreshWatchDirCreate(realfilepath);  // We need explicitly to watch the new dir, as nothing else makes this happen
                    }
                }
               break;
            }
        case wxFSW_EVENT_MODIFY:  
              // IN_MODIFY means that a file is being written to, and is often (very, very) multiple
              // It doesn't affect the name or anything visible in the panes apart from the size in the fileview
              if (isright)
                  { try 
                      {
                        if (wxStaticCast(m_owner, FileGenericDirCtrl)->UpdateTreeFileModify(filepath) == false) // If it doesn't recreate the tree,
                          { wxStaticCast(m_owner, FileGenericDirCtrl)->UpdateFileDataArray(filepath);
                            m_owner->GetTreeCtrl()->Refresh();
                          }
                        if ((m_owner->GetFilePath() == filepath) && (m_owner == MyFrame::mainframe->GetActivePane()))
                          m_owner->m_StatusbarInfoValid = false;           // Flag to update the statusbar value
                        if (m_owner->partner == MyFrame::mainframe->GetActivePane())
                          m_owner->partner->m_StatusbarInfoValid = false;  // If the dirview is active, it must update the statusbar
                      }
                  catch(...)
                    { wxLogWarning(wxT("Caught an unknown exception in MyFSEventManager::DoProcessStoredEvent wxFSW_EVENT_MODIFY")); }
                }
              break;
        case wxFSW_EVENT_ATTRIB:
              // IN_ATTRIB may mean that the access time has altered, and so affects a tree sorted by datetime. Similarly owner/group/permissions. Also that item's display ofc.
              // Otherwise I don't think we care. There is the issue described in http://linux-unix.itags.org/q_linux-unix_132584.html of someone watching a file that
              // gets a new hard link to it created, then the original file is deleted. In that case the link count is the only hint. But how often does that happen?
              if (isright)
                { if (wxStaticCast(m_owner, FileGenericDirCtrl)->UpdateTreeFileAttrib(filepath) == false) // If it doesn't recreate the tree,
                    m_owner->UpdateTree(IET_attribute, filepath, filepath);                               // just update the affected item
                }
              break;
        case wxFSW_EVENT_RENAME:
             {FileData fd(newfilepath); // NB!! This will only be valid if newpath exists i.e. if oldfp != newfp
              // We don't want file renames in a dirview
              if (isright || (fd.IsValid() && fd.IsDir()))
                m_owner->UpdateTree(IET_rename, filepath, newfilepath);
              
              if (isleft) // AFAICT we only get dir renames in a single 'view'. That leaves the other view with stale paths
                { if (fd.IsDir()) // We don't want to do this for symlinks-to-dir, as somehow that causes "Already watched" asserts
                    m_owner->m_watcher->RefreshWatch(filepath, newfilepath); // Update what paths are watched, but only for dir renames
                }
               else if (fd.IsValid() && fd.IsDir())
                m_owner->partner->m_watcher->RefreshWatch(filepath, newfilepath);
              break;}
        case wxFSW_EVENT_DELETE:  /*IN_DELETE, IN_DELETE_SELF or IN_MOVE_SELF (both in & out)*/

              if (isleft || (StripSep(filepath) != StripSep(m_owner->startdir))) // We can't cope with a deleted fileview startdir; we don't know our new startdir. Wait for partner
                  m_owner->UpdateTree(IET_delete, filepath, filepath);
              break;
        case wxFSW_EVENT_UNMOUNT:
              if (isleft)     
                { if (wxStaticCast(m_owner, DirGenericDirCtrl)->IsDirVisible(filepath)) // Don't refresh for every event; multiple child dirs result in a *very* noisy fileview
                    wxStaticCast(m_owner, DirGenericDirCtrl)->GetBestValidHistoryItem(filepath);
                }
              break;
      }
  }
catch(...)
  { wxLogWarning(wxT("Caught an unknown exception in MyFSEventManager::DoProcessStoredEvent")); }
}

void MyFSEventManager::ClearStoredEvents() // Clear the data e.g. if we're entering an archive, otherwise we'll block on exit
{
while (m_Eventmap.size())
  { delete m_Eventmap.begin()->second; m_Eventmap.erase(m_Eventmap.begin()); }
m_Eventmap.clear();
m_EventSent = false;
}
#endif // defined(__LINUX__) && defined(__WXGTK__)
