/////////////////////////////////////////////////////////////////////////////
// Name:        MyFrame.cpp
// Purpose:     App, Frame and Layout stuff
// Part of:     4Pane
// Author:      David Hart
// Copyright:   (c) 2020 David Hart
// Licence:     GPL v3
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/log.h"
#include "wx/app.h"
#include "wx/frame.h"
#include "wx/scrolwin.h"
#include "wx/menu.h"
#include "wx/dirctrl.h"
#include "wx/cmdline.h"
#include "wx/dragimag.h"
#include "wx/config.h"
#include "wx/stdpaths.h"

#include <signal.h>

#include "Externs.h"
#include "Tools.h"
#include "MyTreeCtrl.h"
#include "MyDirs.h"
#include "MyFiles.h"
#include "MyGenericDirCtrl.h"
#include "Archive.h"
#include "Accelerators.h"
#include "MyFrame.h"
#include "Filetypes.h"
#include "config.h"
#include "Misc.h"
#include "Configure.h"
#include "Redo.h"
#include "Devices.h"
#include "Mounts.h"
#include "Bookmarks.h"

#include "bitmaps/include/4PaneIcon32.xpm"
#include "bitmaps/include/cut.xpm"
#include "bitmaps/include/copy.xpm"
#include "bitmaps/include/paste.xpm"
#include "bitmaps/include/undo.xpm"
#include "bitmaps/include/redo.xpm"

#if wxVERSION_NUMBER < 2900
  DEFINE_EVENT_TYPE(myEVT_BriefMessageBox)
#else
  wxDEFINE_EVENT(myEVT_BriefMessageBox, wxCommandEvent);
#endif

static void WaitForDebugger(int signo) 
{
wxString msg;
msg << wxT("4Pane crashed :(  You may try to find out why using gdb\n")
  << wxT("or let it crash silently..\n")
  << wxT("Investigate using gdb?\n");
  
int rc = wxMessageBox(msg, wxT("4Pane Crash Handler"), wxYES_NO|wxCENTER|wxICON_ERROR);
if (rc == wxYES)
  { // Launch a shell command with the command: gdb -p <PID>
    char command[256]; memset (command, 0, sizeof(command));
    sprintf(command, "xterm -T 'gdb' -e 'gdb -p %d'", getpid());
    if(system (command) == 0)
      { signal(signo, SIG_DFL); raise(signo); }
     else 
      { // Go down without launching the debugger, ask the user to do it manually
        wxMessageBox(wxString::Format(wxT("Failed to launch the debugger\nIf gdb is installed, you may still run it manually by typing this command in a terminal:\ngdb -p %d"), getpid()), wxT("CodeLite Crash Handler"), wxOK|wxCENTER|wxICON_ERROR);
        pause();
      }
  }

signal(signo, SIG_DFL); raise(signo);
}

// ----------------------------------------------------------------------------
// MyApp 
// ----------------------------------------------------------------------------
wxWindowID NextID = wxID_HIGHEST+5000;             // An int to generate "unique" window IDs.  Inc at each use.  Starts +5k to leave room for xrc ids

enum helpcontext HelpContext = HCunknown;          // What Help to show on F1

IMPLEMENT_APP(MyApp)

bool MyApp::OnInit()
{
  // Install signal handlers
signal(SIGSEGV, WaitForDebugger);
signal(SIGABRT, WaitForDebugger);

m_EscapeCode = m_EscapeFlags = 0;

wxString rsvglib = LocateLibrary("librsvg"); // dlopen librsvg if it's available
if (!rsvglib.empty())
  SetRsvgHandle(dlopen(rsvglib.ToUTF8(), RTLD_LAZY));

m_locale.Init();                                   // Initialize the catalog we'll be using
wxLocale::AddCatalogLookupPathPrefix(wxT("./locale"));  // Just in case we weren't 'make install'ed
wxLocale::AddCatalogLookupPathPrefix(wxT("/usr/share/locale"));  // Just in case we were installed into /usr/local/

if (!m_locale.AddCatalog(wxT("4Pane")))
  m_locale.AddCatalog(wxT("4pane"));              // Hedge bets re our spelling

m_locale.AddCatalog(wxT("wxstd"));                // Load the standard wxWidgets catalogue too: it'll help if there isn't a 4Pane one for a language

int ret = ParseCmdline();
if (ret == -2) // A test (for a test-suite?) so exit the eventloop, that being afaict the only way to return true without first starting the gui
  { CallAfter([this] { ExitMainLoop(); }); 
    return true; 
  }
 else if (ret < 0)  return false;

wxArrayString arr = Configure::GetXDGValues();
if (arr.GetCount() == 3)
  { m_XDGconfigdir = arr.Item(0);
    m_XDGcachedir = arr.Item(1);
    m_XDGdatadir = arr.Item(2);
  }

  // Try to detect if this is a Wayland session; we have some Wayland-workaround code
wxString sesstype(wxT("XDG_SESSION_TYPE")), session_type;
wxGetEnv(sesstype, &session_type); 
SetWaylandSession(session_type.Lower().Contains(wxT("wayland")));

if (!Configure::FindIni()) return false;          // Make sure there's a config file, resources etc. If not, show wizard and create one, find them etc, or abort

wxInitAllImageHandlers();

wxXmlResource::Get()->InitAllHandlers();

wxXmlResource::Get()->Load(RCDIR + wxT("dialogs.xrc"));
wxXmlResource::Get()->Load(RCDIR + wxT("moredialogs.xrc"));
wxXmlResource::Get()->Load(RCDIR + wxT("configuredialogs.xrc"));

              // 0.7.0 changed the contents of configuredialogs.xrc, so try to detect any stale configs
  { wxLogNull shh;
    wxDialog dlg; bool loaded = wxXmlResource::Get()->LoadDialog(&dlg, NULL, wxT("FakeDlg")); // This doesn't exist in the old version
    if (!loaded) 
      { wxXmlResource::Get()->Unload(RCDIR + wxT("configuredialogs.xrc"));
        RCDIR.Clear(); wxConfigBase::Get()->Write(wxT("Misc/ResourceDir"), RCDIR);
        wxString unused; Configure::FindRC(unused);                // Look harder, in standard places, then try again
        wxXmlResource::Get()->Load(RCDIR + wxT("configuredialogs.xrc"));
        loaded = wxXmlResource::Get()->LoadDialog(&dlg, NULL, wxT("FakeDlg"));
        if (loaded)
          { wxConfigBase::Get()->Write(wxT("Misc/ResourceDir"), RCDIR);
            wxString errormsg(wxT("I just had to search for an up-to-date resource file.\nThat is probably because your configuration pointed to an old version.\nThis one *should* work but, if it doesn't, I suggest you delete your ~/.4Pane file"));
            wxMessageBox(errormsg, _("Warning!"),wxICON_INFORMATION|wxOK);
          }
         else
          { wxString errormsg(wxT("An error occured when trying to load a resource.\nThat is probably because there are old configuration files on your computer.\
              \nI suggest you check your installation, and perhaps delete the file ~/.4Pane\nDo you want to try to continue anyway?"));
            if (wxMessageBox(errormsg, _("Warning!"),wxICON_ERROR|wxYES_NO) != wxYES) return false;
          }
      }
  }

              // 1.0 added some extra bitmaps; again try to detect any stale configs
  { wxLogNull shh;
    if (!wxFileExists(BITMAPSDIR + wxT("/NewTab.png")))
      { wxString errormsg(wxT("An error occured when trying to load a bitmap.\nThat is probably because there are old configuration files on your computer.\
          \nI suggest you check your installation, and perhaps delete the file ~/.4Pane\nDo you want to try to continue anyway?"));
        if (wxMessageBox(errormsg, _("Warning!"),wxICON_ERROR|wxYES_NO) != wxYES) return false;
      }
  }

              // 4.0 changed the NFS and similar dialogs; again try to detect any stale configs
  { wxLogNull shh;
    MountNFSDialog dlg;
    wxXmlResource::Get()->LoadDialog(&dlg,MyFrame::mainframe, wxT("MountNFSDlg"));
    if (dlg.FindWindow(wxT("MountptTxt")))
      { wxString errormsg(wxT("Old configuration files detected. This means that the 'Mount' dialogs will not work.\
                              \nI suggest you check your installation, and perhaps delete the file ~/.4Pane\nDo you want to try to continue anyway?"));
        if (wxMessageBox(errormsg, _("Warning!"),wxICON_ERROR|wxYES_NO) != wxYES) return false;
      }
  }

#ifndef NO_LZMA_ARCHIVE_STREAMS
  // See if there's the xz lib available for archive streams
  #if wxVERSION_NUMBER >= 2900  
    m_liblzma = new wxDynamicLibrary(wxT("liblzma"), wxDL_LAZY | wxDL_QUIET);
  #else
    {
    wxLogNull noerrormessage;
    m_liblzma = new wxDynamicLibrary(wxT("liblzma"), wxDL_LAZY);
    }
  #endif
  LIBLZMA_LOADED = (m_liblzma && m_liblzma->IsLoaded());
#endif // NO_LZMA_ARCHIVE_STREAMS

PreviewManager::Init();
long height = wxConfigBase::Get()->Read(wxT("/Misc/AppHeight"), 400l);
long width = wxConfigBase::Get()->Read(wxT("/Misc/AppWidth"), 600l);

frame = new MyFrame(wxSize(width, height));
wxString title = GetAppName();                      // In case this isn't the normal one
if (getuid() == 0) title += wxT(" (superuser)");    // It's nice to know if we're root
frame->SetTitle(title);
frame->GetMenuBar()->Check(SHCUT_SHOW_RECURSIVE_SIZE, SHOW_RECURSIVE_FILEVIEW_SIZE);
frame->GetMenuBar()->Check(SHCUT_RETAIN_REL_TARGET, RETAIN_REL_TARGET);
frame->GetMenuBar()->Check(SHCUT_RETAIN_MTIME_ON_PASTE, RETAIN_MTIME_ON_PASTE);
return true;
}

const wxColour* MyApp::GetBackgroundColourSelected(bool fileview /*=false*/) const
{
if (HIGHLIGHT_PANES)
  return (fileview ? &m_FvBackgroundColourSelected : &m_DvBackgroundColourSelected);
 else
  return GetBackgroundColourUnSelected();
}

void MyApp::SetPaneHighlightColours()  // Set colours to denote selected/unselectness in the panes
{
m_BackgroundColourUnSelected = GetSaneColour(NULL, true, wxSYS_COLOUR_BTNFACE);
m_DvBackgroundColourSelected = AdjustColourFromOffset(m_BackgroundColourUnSelected, DIRVIEW_HIGHLIGHT_OFFSET);
m_FvBackgroundColourSelected = AdjustColourFromOffset(m_BackgroundColourUnSelected, FILEVIEW_HIGHLIGHT_OFFSET); 
}

wxColour MyApp::AdjustColourFromOffset(const wxColour& col, int offset) const  // Utility function. Intelligently applies the given offset to col
{
wxColour colour;
 // We don't want overflow of rgb colour values. Truncating at 0 or 255 isn't perfect (as the proportions may change) but it's the lesser evil
if (offset >= 0)
  colour.Set(wxMin(col.Red()+offset, 255), wxMin(col.Green()+offset, 255), wxMin(col.Blue()+offset, 255));
 else
  colour.Set(wxMax(col.Red()+offset, 0), wxMax(col.Green()+offset, 0), wxMax(col.Blue()+offset, 0));

return colour;
}

wxImage MyApp::CorrectForDarkTheme(const wxString& filepath) const  // Utility function. Loads an black-on-transparent image and whitens if using a dark theme
{
static bool isdark = (wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE).Red() + wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE).Green()
                      + wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE).Blue()) < 382;
wxImage img = wxImage(filepath);
if (isdark && img.IsOk())
  { unsigned char* data = img.GetData();
    for (int y = 0; y < img.GetHeight(); ++y)
      for (int x = 0; x < img.GetWidth(); ++x)
        { static const char white = 255;
          unsigned char* px = data;
          if ((*px + *(px+1) + *(px+2)) == 0)
            { *px = white; *(px+1) = white; *(px+2) = white; }
          data += 3;
        }
  }

return img;
}

static const wxCmdLineEntryDesc cmdLineDesc[] =
{     
  { wxCMD_LINE_PARAM,  NULL, NULL, "Display from this filepath (optional)", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL | wxCMD_LINE_PARAM_MULTIPLE },
  { wxCMD_LINE_OPTION, "d", "home-dir",  "Use this as the home directory (optional)" },
  { wxCMD_LINE_OPTION, "c", "config-fpath",   "Use this filepath as 4Pane's configuration file (optional; if the filepath doesn't exist, it will be created)" },
  { wxCMD_LINE_SWITCH, "h", "help", "Show this help message", wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },
#if wxVERSION_NUMBER > 3101
  { wxCMD_LINE_SWITCH, "t", "test", "Testing testing 1 2 3 4", wxCMD_LINE_VAL_NONE, wxCMD_LINE_HIDDEN },
#endif
  { wxCMD_LINE_NONE }
};

int MyApp::ParseCmdline()
{
wxCmdLineParser parser(cmdLineDesc, argc, argv);
switch (parser.Parse())
  { case -1:  return -1;  // The parameter -h was passed, help was given, so abort the app
    case 0:   break;      // OK, so break to deal with any parameters etc
     default: return -1;  // Some syntax error occurred. Abort
  }
#if wxVERSION_NUMBER > 3101
if (parser.Found("t"))
  return -2;   // Just testing (for a test suite?) so return the appropriate value
#endif
          // If the app was launched via the 4pane->4Pane symlink, change the appname; otherwise resources aren't located
wxString appname(argv[0]); appname = appname.AfterLast(wxFILE_SEP_PATH);
if (appname==wxT("4pane")) SetAppName(wxT("4Pane"));

if (parser.GetParamCount() > 0)    startdir0 = parser.GetParam(0);    // We can cope with up to 2 startdirs
if (parser.GetParamCount() > 1)    startdir1 = parser.GetParam(1);

wxString HOME(wxGetHomeDir());
if (parser.Found(wxT("d"), &HOME))                 // See if there's an alternative home directory (useful perhaps if we're on a LiveCD)
  {  if (!wxDirExists(HOME))                       // It would normally be a dir, but check
      { HOME = HOME.BeforeLast(wxFILE_SEP_PATH);   // See if it was /foo/bar/.Not4Pane or something
        if (!wxDirExists(HOME))  HOME = wxGetHomeDir();  // If the parameter is rubbish, this effectively ignores it
      }
  }
m_home = HOME;

if (parser.Found(wxT("c"), &m_ConfigFilepath))       // See if there's an alternative config file, or a desire for one (again, LiveCD)
  { if (wxDirExists(m_ConfigFilepath)) m_ConfigFilepath << wxT("/.4Pane");  // It would normally be a file, but check and, if need be, add the name
    if (!wxIsAbsolutePath(m_ConfigFilepath))         // Check that we weren't passed a filename, rather than a filepath
      { wxString hm = m_home;
        if (hm.Right(1) != wxFILE_SEP_PATH) hm << wxFILE_SEP_PATH;
        m_ConfigFilepath = hm + m_ConfigFilepath;
      }
    wxString path = m_ConfigFilepath.BeforeLast(wxFILE_SEP_PATH); // Now ensure existence, & check for permission/ro filesystem
    if (!wxFileName::Mkdir(path, 0755, wxPATH_MKDIR_FULL))
      { wxLogError(_("Sorry, failed to create the directory for your chosen configuration-file; aborting.")); return false; }
    FileData temp(path);
    if (!temp.CanTHISUserWrite())
      { wxLogError(_("Sorry, you don't have permission to write to the directory containing your chosen configuration-file; aborting.")); return false; }
  }

StdDataDir = wxStandardPaths::Get().GetDataDir();    // This will probably be /usr/local/share/4Pane
size_t pre = StdDataDir.find(wxT("4Pane"));
if (pre != wxString::npos)  StdDocsDir = StdDataDir.Left(pre) + wxT("doc/") + wxT("4Pane");
  else StdDocsDir = StdDataDir + wxT("/doc/");       // Shouldn't happen, but this is a reasonable default

return true;
}

int MyApp::FilterEvent(wxEvent& event)
{
if (frame && event.GetEventType() == wxEVT_KEY_DOWN)
  { if (((wxKeyEvent&)event).GetKeyCode() == WXK_F1)
      { frame->OnHelpF1((wxKeyEvent&)event); return true; }

    if (((wxKeyEvent&)event).GetKeyCode() == GetEscapeKeycode())
      { int flags = 0;
        if (wxGetKeyState(WXK_CONTROL)) flags |= wxACCEL_CTRL; if (wxGetKeyState(WXK_ALT)) flags |= wxACCEL_ALT; if (wxGetKeyState(WXK_SHIFT)) flags |= wxACCEL_SHIFT;
        if (flags == GetEscapeKeyFlags())
          { frame->DoOnProcessCancelled(); return -1; } // This time return -1 (== Skip()) as other things may use this keypress e.g. DnD
      }
  }

return -1;
}


#if defined(__WXX11__)  && wxVERSION_NUMBER < 2800
  #include "wx/x11/private.h"
bool MyApp::ProcessXEvent(WXEvent* _event)  // Grab selection XEvents
{
if (wxApp::ProcessXEvent(_event)) return true;      // First let the standard method happen
 
XEvent* event = (XEvent*) _event;                   // The next bit is duplicated from wxApp::ProcessXEvent
wxWindow* win = NULL;
Window window = XEventGetWindow(event);
win = wxGetWindowFromTable(window);
if (!win)
  {
#if wxUSE_TWO_WINDOWS
    win = wxGetClientWindowFromTable(window);
    if (!win)
#endif
    return false;
  }
#if wxVERSION_NUMBER < 2900    // Makes the mousewheel work
if (event->type == ButtonPress)
  if (event->xbutton.button == Button4 || event->xbutton.button == Button5)
    { wxMouseEvent wxevent;
      wxevent.SetTimestamp(XButtonEventGetTime(event));
      wxevent.SetEventType(wxEVT_MOUSEWHEEL);
      wxevent.m_x = XButtonEventGetX(event);
      wxevent.m_y = XButtonEventGetY(event);
      wxevent.m_leftDown =  XButtonEventLIsDown(event);
      wxevent.m_middleDown = XButtonEventMIsDown(event);
      wxevent.m_rightDown = XButtonEventRIsDown(event);
      wxevent.m_shiftDown = XButtonEventShiftIsDown(event);
      wxevent.m_controlDown = XButtonEventCtrlIsDown(event);
      wxevent.m_altDown = XButtonEventAltIsDown((event));
      wxevent.m_metaDown = XButtonEventMetaIsDown(event);
      wxevent.m_linesPerAction = LINES_PER_MOUSEWHEEL_SCROLL;
      wxevent.m_wheelDelta = 120;
      wxevent.m_wheelRotation = (event->xbutton.button == Button4 ? 120 : -120);

      wxevent.SetId(win->GetId());
      wxevent.SetEventObject(win);

      return win->GetEventHandler()->ProcessEvent(wxevent);
    }
#endif //wxVERSION_NUMBER < 2900

if (win->GetName() == wxT("TerminalEm")                       // Kludge, but I can't get RTTI to work here. Too many levels of subclassing?
                    || (frame && win == frame->m_tbText))
  { ((TextCtrlBase*)win)->OnXevent(*event); return true; }    // This looks for Selection events, to implement PRIMARY and CLIPBOARD for  textctrls

return false;
}
#endif

int MyApp::OnExit()
{
delete m_liblzma;
ThreadsManager::Get().Release();
PasswordManager::Get().Release();
BriefLogStatus::DeleteTimer();
delete wxConfigBase::Set((wxConfigBase*) NULL);
#if defined __WXGTK__
  if (m_dlRsvgHandle) 
    { dlclose(m_dlRsvgHandle); m_dlRsvgHandle = NULL; }
#endif

return true;
}


// ----------------------------------------------------------------------------
// MyFrame 
// ----------------------------------------------------------------------------

BEGIN_EVENT_TABLE(MyFrame, wxFrame)
  EVT_ACTIVATE(MyFrame::OnActivate)
  EVT_MENU(SHCUT_UNDO, MyFrame::OnUndo)
  EVT_BUTTON(XRCID("IDM_TOOLBAR_LargeDropdown1"), MyFrame::OnUndoSidebar)
  EVT_MENU(SHCUT_REDO, MyFrame::OnRedo)
  EVT_BUTTON(XRCID("IDM_TOOLBAR_LargeDropdown2"), MyFrame::OnRedoSidebar)
  EVT_MENU(SHCUT_NEWTAB, MyFrame::OnAppendTab)
  EVT_MENU(SHCUT_INSERTTAB, MyFrame::OnInsTab)
  EVT_MENU(SHCUT_DELTAB, MyFrame::OnDelTab)
  EVT_MENU(SHCUT_PREVIEW, MyFrame::OnPreview)
  EVT_MENU(SHCUT_RENAMETAB, MyFrame::OnRenTab)
  EVT_MENU(SHCUT_DUPLICATETAB, MyFrame::OnDuplicateTab)
#if defined __WXGTK__
  EVT_MENU(SHCUT_ALWAYS_SHOW_TAB, MyFrame::OnAlwaysShowTab)
  EVT_MENU(SHCUT_SAME_TEXTSIZE_TAB, MyFrame::OnSameTabSize)
#endif
  EVT_MENU(SHCUT_REPLICATE, MyFrame::OnReplicate)
  EVT_MENU(SHCUT_SWAPPANES, MyFrame::OnSwapPanes)
  
  EVT_MENU(SHCUT_CUT, MyFrame::OnCut)
  EVT_MENU(SHCUT_COPY, MyFrame::OnCopy)
  EVT_MENU(SHCUT_PASTE, MyFrame::OnPaste)
  EVT_MENU(SHCUT_PASTE_DIR_SKELETON, MyFrame::OnPaste)
  EVT_MENU(SHCUT_ESCAPE, MyFrame::OnProcessCancelled)
  EVT_MENU(SHCUT_NEW, MyFrame::OnNew)
  EVT_MENU(SHCUT_HARDLINK, MyFrame::OnHardLink)
  EVT_MENU(SHCUT_SOFTLINK, MyFrame::OnSoftLink)
  EVT_MENU(SHCUT_TRASH, MyFrame::OnTrash)
  EVT_MENU(SHCUT_DELETE, MyFrame::OnDelete)
  EVT_MENU(SHCUT_REALLYDELETE, MyFrame::OnReallyDelete)
  EVT_MENU(SHCUT_RENAME, MyFrame::OnRename)
  EVT_MENU(SHCUT_REFRESH, MyFrame::OnRefresh)
  EVT_MENU(SHCUT_DUP, MyFrame::OnDup)
  EVT_MENU(SHCUT_PROPERTIES, MyFrame::OnProperties)  

  EVT_MENU(SHCUT_SPLITPANE_VERTICAL, MyFrame::SplitVertical)  
  EVT_MENU(SHCUT_SPLITPANE_HORIZONTAL, MyFrame::SplitHorizontal)  
  EVT_MENU(SHCUT_SPLITPANE_UNSPLIT, MyFrame::Unsplit)  
  EVT_MENU_RANGE(SHCUT_TABTEMPLATERANGE_START, SHCUT_TABTEMPLATERANGE_FINISH, MyFrame::OnTabTemplateLoadMenu)
  EVT_MENU(SHCUT_TAB_SAVEAS_TEMPLATE, MyFrame::OnSaveAsTemplate)
  EVT_MENU(SHCUT_TAB_DELETE_TEMPLATE, MyFrame::OnDeleteTemplate)
  
  EVT_MENU(SHCUT_LAUNCH_TERMINAL, MyFrame::OnLaunchTerminal)
  
  EVT_MENU(SHCUT_ADD_TO_BOOKMARKS, MyFrame::OnAddToBookmarks)
  EVT_MENU(SHCUT_MANAGE_BOOKMARKS, MyFrame::OnManageBookmarks)
  EVT_MENU_RANGE(ID_FIRST_BOOKMARK, ID__LAST_BOOKMARK, MyFrame::OnBookmark)
  
  EVT_MENU(SHCUT_ARCHIVE_EXTRACT, MyFrame::OnArchiveExtract)
  EVT_MENU(SHCUT_ARCHIVE_CREATE, MyFrame::OnArchiveCreate)
  EVT_MENU(SHCUT_ARCHIVE_APPEND, MyFrame::OnArchiveAppend)  
  EVT_MENU(SHCUT_ARCHIVE_TEST, MyFrame::OnArchiveTest)  
  EVT_MENU(SHCUT_ARCHIVE_COMPRESS, MyFrame::OnArchiveCompress)

  EVT_MENU(SHCUT_MOUNT_MOUNTPARTITION, MyFrame::OnMountPartition)
  EVT_MENU(SHCUT_MOUNT_UNMOUNTPARTITION, MyFrame::OnUnMountPartition)
  EVT_MENU(SHCUT_MOUNT_MOUNTISO, MyFrame::OnMountIso)
  EVT_MENU(SHCUT_MOUNT_MOUNTNFS, MyFrame::OnMountNfs)
  EVT_MENU(SHCUT_MOUNT_MOUNTSSHFS, MyFrame::OnMountSshfs)
  EVT_MENU(SHCUT_MOUNT_MOUNTSAMBA, MyFrame::OnMountSamba)
  EVT_MENU(SHCUT_MOUNT_UNMOUNTNETWORK, MyFrame::OnUnMountNetwork)
  EVT_MENU(SHCUT_COMMANDLINE, MyFrame::OnCommandLine)
  EVT_MENU(SHCUT_TERMINAL_EMULATOR, MyFrame::OnTermEm)
  EVT_MENU(SHCUT_TOOL_LOCATE, MyFrame::OnLocate)
  EVT_MENU(SHCUT_TOOL_FIND, MyFrame::OnFind)
  EVT_MENU(SHCUT_TOOL_GREP, MyFrame::OnGrep)
  
  EVT_MENU_RANGE(SHCUT_SWITCH_FOCUS_PANES,SHCUT_SWITCH_TO_PREVIOUS_WINDOW, MyFrame::SetFocusViaKeyboard)
  
  EVT_MENU_RANGE(SHCUT_TOOLS_LAUNCH,SHCUT_TOOLS_LAUNCH_LAST, MyFrame::OnToolsLaunch)
  EVT_MENU(SHCUT_TOOLS_REPEAT, MyFrame::OnToolsRepeat)
  
  EVT_MENU(SHCUT_EMPTYTRASH, MyFrame::OnEmptyTrash)
  EVT_MENU(SHCUT_EMPTYDELETED, MyFrame::OnEmptyTrash)
  
  EVT_MENU(SHCUT_SHOW_RECURSIVE_SIZE, MyFrame::ToggleFileviewSizetype)
  EVT_MENU(SHCUT_RETAIN_REL_TARGET, MyFrame::ToggleRetainRelSymlinkTarget)
  EVT_MENU(SHCUT_RETAIN_MTIME_ON_PASTE, MyFrame::ToggleRetainMtimeOnPaste)
  EVT_MENU(SHCUT_CONFIGURE, MyFrame::OnConfigure)
  
  EVT_MENU(SHCUT_SAVETABS, MyFrame::OnSaveTabs)
  
  EVT_MENU(SHCUT_HELP, MyFrame::OnHelpContents)
  EVT_MENU(SHCUT_FAQ, MyFrame::OnHelpFAQs)
  EVT_MENU(SHCUT_ABOUT, MyFrame::OnHelpAbout)
  
  EVT_MENU(SHCUT_CONFIG_SHORTCUTS, MyFrame::OnConfigureShortcuts)
  
  EVT_TEXT_ENTER(ID_FILEPATH, MyFrame::OnTextFilepathEnter)
  
  EVT_MOTION(MyFrame::DoMutex)    
  EVT_LEFT_UP(MyFrame::DoMutex)
  EVT_RIGHT_DOWN(MyFrame::OnDnDRightclick)
  EVT_MOUSEWHEEL(MyFrame::OnMouseWheel)

  EVT_COMMAND(wxID_ANY, myEVT_BriefMessageBox, MyFrame::OnShowBriefMessageBox)

#if wxVERSION_NUMBER >= 2800
  EVT_MOUSE_CAPTURE_LOST(MyFrame::OnMouseCaptureLost)
#endif
    EVT_MENU(SPLIT_VERTICAL, MyFrame::SplitVertical)
    EVT_MENU(SPLIT_HORIZONTAL, MyFrame::SplitHorizontal)
    EVT_MENU(SPLIT_UNSPLIT, MyFrame::Unsplit)
    
    EVT_MENU(SHCUT_EXIT, MyFrame::OnQuit)
  
  EVT_MENU(SHCUT_FILTER, MyFrame::OnFilter)
  EVT_MENU(SHCUT_TOGGLEHIDDEN, MyFrame::OnToggleHidden)
  EVT_MENU_RANGE(SHCUT_SHOW_COL_EXT,SHCUT_SHOW_COL_CLEAR, MyFrame::OnViewColMenu)
  EVT_MENU_RANGE(SHCUT_EXT_FIRSTDOT,SHCUT_EXT_LASTDOT, MyFrame::OnViewColMenu)
  EVT_RELOADSHORTCUTS(MyFrame::ReconfigureShortcuts)

  EVT_UPDATE_UI_RANGE(SHCUT_PASTE,SHCUT_NEW, MyFrame::DoMiscUI)
  EVT_UPDATE_UI_RANGE(SHCUT_CUT,SHCUT_OPENWITH_KDESU, MyFrame::DoQueryEmptyUI)  // Context menu items that depend on a selected item
  EVT_UPDATE_UI_RANGE(SHCUT_ALWAYS_SHOW_TAB,  SHCUT_SAME_TEXTSIZE_TAB, MyFrame::DoMiscTabUI)
  EVT_UPDATE_UI_RANGE(SHCUT_SPLITPANE_VERTICAL, SHCUT_SPLITPANE_UNSPLIT, MyFrame::DoSplitPaneUI)
  EVT_UPDATE_UI_RANGE(SHCUT_ARCHIVE_EXTRACT,SHCUT_ARCHIVE_COMPRESS, MyFrame::DoNoPanesUI)
  EVT_UPDATE_UI_RANGE(SHCUT_REPLICATE,SHCUT_SWAPPANES, MyFrame::DoNoPanesUI)
  EVT_UPDATE_UI_RANGE(SHCUT_FILTER,SHCUT_TOGGLEHIDDEN, MyFrame::DoNoPanesUI)
  EVT_UPDATE_UI_RANGE(SHCUT_SHOW_COL_ALL,SHCUT_SHOW_COL_CLEAR, MyFrame::DoNoPanesUI)
  EVT_UPDATE_UI_RANGE(SHCUT_SHOW_COL_EXT, SHCUT_SHOW_COL_LINK, MyFrame::DoColViewUI)
  EVT_UPDATE_UI(SHCUT_REFRESH, MyFrame::DoNoPanesUI)
  EVT_UPDATE_UI(SHCUT_PASTE_DIR_SKELETON, MyFrame::OnDirSkeletonUI)
  EVT_UPDATE_UI(SHCUT_DUPLICATETAB, MyFrame::DoNoPanesUI)
  EVT_UPDATE_UI(SHCUT_TAB_DELETE_TEMPLATE, MyFrame::DoTabTemplateUI)
  EVT_UPDATE_UI(SHCUT_TOOLS_REPEAT, MyFrame::DoRepeatCommandUI)
  EVT_UPDATE_UI(SHCUT_PREVIEW, MyFrame::OnPreviewUI)
  EVT_UPDATE_UI(SHCUT_ESCAPE, MyFrame::OnProcessCancelledUI)
END_EVENT_TABLE()

wxString CreateSubgroupName(size_t no, size_t total)    // Creates a unique subgroup name in ascending alphabetical order
{                                                       // So if total is between 26^2 & 26^3, names will start aaa, then aab, aac etc
wxString name;
if (!total) return name;                                // GIGO

size_t magnitude = 1;                                   // Work out how many letters we'll need for the whole subgroup ie 1st name may be a, aa, aaa etc
size_t tot = total;
while (tot /= 26)     magnitude *= 26;

for (size_t divisor = magnitude;  divisor > 0; divisor /= 26)  // Now work out which each of these letters should be for this no
  { name << wxChar(wxT('a') + (no / divisor));
    no %= divisor;
  }

return name;
}

MyFrame::MyFrame(wxSize size)  : wxFrame(NULL, -1, wxT("4Pane"),  wxDefaultPosition, size,
                                                             wxDEFAULT_FRAME_STYLE | wxNO_FULL_REPAINT_ON_RESIZE, wxT("MyFrame"))
{
SelectionAvailable = false;
Layout = NULL;

m_tbText = NULL;                                        // In case this isn't going to be used, start with it null for ease of testing

configure =  new class Configure;
configure->LoadConfiguration();

wxGetApp().SetPaneHighlightColours();  // Set colours to denote selected/unselectness in the panes

int widths[4];
ConfigureMisc::LoadStatusbarWidths(widths); // Load (or set from defaults) the widths of the 4 statusbar sections.  Proportional and, yes, the numbers are *supposed* to be negative
SetStatusBar(CreateStatusBar(4));
SetStatusWidths(4, widths);

AccelList = new class AcceleratorList(this); AccelList->Init();
CreateAcceleratorTable();

mainframe = this;
 
SetIcon(wxICON(FourPaneIcon32));

if (!HELPDIR.IsEmpty())                               // If we couldn't find a valid HELPDIR, limp along helplessly
  {
#if wxVERSION_NUMBER >= 3000
    // We can't use the default frame in wx3: when called from a dialog it won't exit its event-loop properly and breaks menu-clicks and the 4Pane close button
    Help = new HtmlHelpController(wxHF_TOOLBAR | wxHF_CONTENTS | wxHF_MERGE_BOOKS | wxHF_DIALOG | wxHF_MODAL);
#else
    Help = new wxHtmlHelpController(wxHF_TOOLBAR | wxHF_CONTENTS | wxHF_MERGE_BOOKS);
#endif
    if (Help->Initialize(HELPDIR))
       { wxLogError(_("Cannot initialize the help system; aborting.")); return; }

    Help->AddBook(wxFileName(HELPDIR + wxT("/Chapt.hhp")));
  }

MyBitmapButton::SetUnknownButtonNo();  // Locate the iconno of unknown.xpm, to use if an icon is missing

wxMenuBar* MenuBar = new wxMenuBar();
CreateMenuBar(MenuBar); SetMenuBar(MenuBar);

toolbar = NULL; LoadToolbarButtons();  // Toolbar pt1

panelette = new wxPanel(this, -1, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL, wxT("TBPanel"));  // & pt2, for MyBitmapButtons
sizerTB = new wxBoxSizer(wxHORIZONTAL);
sizerTB->Add(toolbar, 0, wxEXPAND);
sizerTB->Add(panelette, 1, wxEXPAND);

sizerMain = new wxBoxSizer(wxVERTICAL);
sizerMain->Add(sizerTB, 0, wxEXPAND);

AddControls();
Layout = new LayoutWindows(this);                    // This does the rest of the layout things
Layout->Setup();

#if defined(__WXGTK__)
  // gtk2 steals keydown events for any shortcuts! So if you use DEL as ->Trash, textctrls never get DEL keypresses.  Ditto Copy/Paste.  This is a workaround (see Tools.cpp). 
PushEventHandler(new EventDistributor);
#endif

Layout->m_notebook->LaunchFromMenu->LoadMenu();        //This adds user-defined Launch commands to the menubar

if (GetActivePane())                                   // (Check there *is* a tab to setfocus to)
  GetActivePane()->GetTreeCtrl()->SetFocus();          // If we don't do this, we start with NO focused window, which means some shortcuts fail

configure->ApplyTooltipPreferences();                  // This doesn't work if it's done earlier!?
#if defined (__WXGTK__)
DnDStdCursor = wxCursor( wxGetApp().CorrectForDarkTheme(BITMAPSDIR+wxT("DnDStdCursor.png")) );  // Load the 2 cursors for DnD
DnDSelectedCursor = wxCursor( wxGetApp().CorrectForDarkTheme(BITMAPSDIR+wxT("DnDSelectedCursor.png")) );
#else
DnDStdCursor = wxCursor(wxCURSOR_NO_ENTRY);
DnDSelectedCursor = wxCursor(wxCURSOR_BULLSEYE);
#endif
m_StandardCursor = GetCursor();                        // The original cursor, to reapply after DragImage leaves standard controls
m_dragMode = false;                                    // Not DnDing
m_DragMutex.Lock();                                    // Start with it locked, so that OnBeginDrag can unlock it without asserting

Connect(wxID_ANY, PasteProgressEventType, wxCommandEventHandler(MyFrame::OnPasteThreadProgress), NULL, this);
Connect(wxID_ANY, PasteThreadType, PasteThreadEventHandler(MyFrame::OnPasteThreadFinished), NULL, this);

#if defined __WXX11__
Connect(wxEVT_IDLE, (wxObjectEventFunction)&MyFrame::OnIdle);
#endif
}

#ifdef __WXX11__
void MyFrame::OnIdle(wxIdleEvent& WXUNUSED(event))    // One-off to make the toolbar textctrl show the initial selection properly. Otherwise we only see the last 2 letters
{
if (!SelectionAvailable || GetActivePane() == NULL) return;
wxString sel = GetActivePane()->GetPath(); GetActivePane()->DisplaySelectionInTBTextCtrl(sel);  // Show the selection again now the textctrl is big enough to cope!

Disconnect(wxID_ANY);
}
#endif

MyFrame::~MyFrame()
{
m_DragMutex.Unlock();

delete Layout;
delete Help;
delete AccelList;

configure->SaveConfiguration();
delete configure;
#if defined(__WXGTK__)
PopEventHandler(true);                               // In gtk2 we pushed a new event handler. Pop it to avoid a memory leak
#endif
}

void MyFrame::CreateMenuBar(wxMenuBar* MenuBar)
{
AccelList->FillMenuBar(MenuBar);
}

void MyFrame::RecreateMenuBar()
{
wxMenuBar* MenuBar = GetMenuBar();
AccelList->FillMenuBar(MenuBar, true);
}

void MyFrame::CreateAcceleratorTable()
{
int AccelEntries[] = { SHCUT_LAUNCH_TERMINAL, SHCUT_TOOL_LOCATE, SHCUT_TOOL_FIND, SHCUT_TOOL_GREP, SHCUT_COMMANDLINE, SHCUT_TERMINAL_EMULATOR,
                       SHCUT_ARCHIVE_EXTRACT, SHCUT_ARCHIVE_CREATE, SHCUT_ARCHIVE_COMPRESS, SHCUT_F1, SHCUT_CONFIG_SHORTCUTS, SHCUT_TOOLS_REPEAT
                     };
const size_t shortcutNo = sizeof(AccelEntries)/sizeof(int);
AccelList->CreateAcceleratorTable(this, AccelEntries,  shortcutNo);
}

void MyFrame::ReconfigureShortcuts(wxNotifyEvent& WXUNUSED(event))
{
Layout->m_notebook->LaunchFromMenu->LoadMenu();                         // Needed to implement any shortcut change of a user-defined tool
RecreateMenuBar();                                                      // Delete & recreate the menus
CreateAcceleratorTable();                                               // Make a new one, with up-to-date data
                              // Now recreate each pane's table
for (size_t tab=0; tab < Layout->m_notebook->GetPageCount(); ++tab)
  { MyTab* page = (MyTab*)Layout->m_notebook->GetPage(tab);
    page->m_splitterLeftTop->m_left->RecreateAcceleratorTable();        // Tell each pane to recreate its accel table
    page->m_splitterLeftTop->m_right->RecreateAcceleratorTable();  
    page->m_splitterRightBottom->m_left->RecreateAcceleratorTable();
    page->m_splitterRightBottom->m_right->RecreateAcceleratorTable();
  }
                              // and finally the terminal emulator and relatives
m_tbText->CreateAcceleratorTable();
TextCtrlBase* tc = Layout->GetTerminalEm();
if (tc) tc->CreateAcceleratorTable();
tc = Layout->commandline;
if (tc) tc->CreateAcceleratorTable();
}

void MyFrame::LoadToolbarButtons()  // Just to encapsulate loading tools onto the toolbar
{
bool recreating = (toolbar != NULL);
if (recreating) 
  { sizerTB && sizerTB->Detach(toolbar);
    toolbar->Destroy();
  }

toolbar = new wxToolBar(this, ID_FRAMETOOLBAR);

wxBitmap toolBarBitmaps[8];

toolBarBitmaps[0] = GetBestBitmap(wxART_CUT,   wxART_TOOLBAR, wxBitmap(cut));
toolBarBitmaps[1] = GetBestBitmap(wxART_COPY,  wxART_TOOLBAR, wxBitmap(copy));
toolBarBitmaps[2] = GetBestBitmap(wxART_PASTE, wxART_TOOLBAR, wxBitmap(paste));
toolBarBitmaps[3] = GetBestBitmap(wxART_UNDO,  wxART_TOOLBAR, wxBitmap(undo));
toolBarBitmaps[4] = GetBestBitmap(wxART_REDO,  wxART_TOOLBAR, wxBitmap(redo));
toolBarBitmaps[5] = wxBitmap(BITMAPSDIR + wxT("/NewTab.png"), wxBITMAP_TYPE_PNG); // There aren't any official stock alternatives to these
toolBarBitmaps[6] = wxBitmap(BITMAPSDIR + wxT("/DelTab.png"), wxBITMAP_TYPE_PNG);
toolBarBitmaps[7] = wxBitmap(BITMAPSDIR + wxT("/Preview.png"), wxBITMAP_TYPE_PNG);

int count=0, separator=0;                                              // Use these to count the no of buttons etc loaded before the ones with sidebars
    
largesidebar1 = new wxBitmapButton(toolbar, XRCID("IDM_TOOLBAR_LargeDropdown1"), wxBitmap(BITMAPSDIR + wxT("/largedropdown.png"), wxBITMAP_TYPE_PNG),
                                                  wxDefaultPosition, wxDefaultSize, wxBU_AUTODRAW | wxNO_BORDER);
largesidebar2 = new wxBitmapButton(toolbar, XRCID("IDM_TOOLBAR_LargeDropdown2"), wxBitmap(BITMAPSDIR + wxT("/largedropdown.png"), wxBITMAP_TYPE_PNG),
                                                  wxDefaultPosition, wxDefaultSize, wxBU_AUTODRAW | wxNO_BORDER);
largesidebar1->SetToolTip(_("Undo several actions at once"));
largesidebar2->SetToolTip(_("Redo several actions at once"));

toolbar->AddTool(SHCUT_CUT, _("Cut"),     toolBarBitmaps[0], _("Click to Cut Selection"));    ++count;
toolbar->AddTool(SHCUT_COPY, _("Copy"),   toolBarBitmaps[1], _("Click to Copy Selection"));   ++count;
toolbar->AddTool(SHCUT_PASTE, _("Paste"), toolBarBitmaps[2], _("Click to Paste Selection"));  ++count;
toolbar->AddSeparator(); ++separator;

buttonsbeforesidebar = count; separatorsbeforesidebar = separator;    // Store the counts, as here come the ones with sidebars
toolbar->AddTool(SHCUT_UNDO, _("UnDo"),   toolBarBitmaps[3], _("Undo an action"));
toolbar->AddControl(largesidebar1);
toolbar->AddTool(SHCUT_REDO, _("ReDo"),   toolBarBitmaps[4], _("Redo a previously-Undone action"));
toolbar->AddControl(largesidebar2);

toolbar->AddSeparator();
toolbar->AddTool(SHCUT_NEWTAB, _("New Tab"),    toolBarBitmaps[5], _("Create a new Tab"));
toolbar->AddTool(SHCUT_DELTAB, _("Delete Tab"), toolBarBitmaps[6], _("Deletes the currently-selected Tab"));
toolbar->AddTool(SHCUT_PREVIEW, _("Preview tooltips"), toolBarBitmaps[7], _("Shows previews of images and text files in a 'tooltip'"), wxITEM_CHECK);

#if wxVERSION_NUMBER >= 3000 && defined(__WXGTK3__) && !GTK_CHECK_VERSION(3,10,0)
  wxColour bg = toolbar->GetBackgroundColour(); // See the comment in GetSaneColour() for the explanation
  largesidebar1->SetBackgroundColour(bg);
  largesidebar2->SetBackgroundColour(bg);
#endif

toolbar->Realize();

if (recreating)
  { wxCHECK_RET(sizerTB, wxT("NULL sizerTB during recreation :/"));
    sizerTB->Insert(0, toolbar, 0, wxEXPAND);
    sizerTB->Layout();
  }
}

void MyFrame::AddControls()        // Just to encapsulate adding textctrl & an editor button or 2 to the "toolbar", now actually a panel
{
wxBoxSizer* panelettesizer = new wxBoxSizer(wxHORIZONTAL);          // First set up a sizer for panelette
panelette->SetSizer(panelettesizer);

sizerControls = new wxBoxSizer(wxHORIZONTAL);                       // This sizer will actually hold the device buttons
wxBoxSizer* sizerSpacer = new wxBoxSizer(wxHORIZONTAL);             // This one is just for a spacer, to prevent the buttons clustering at the extreme right
sizerSpacer->Add(5, 25, 0, wxRIGHT, 20);
EditorsSizer = new wxBoxSizer(wxHORIZONTAL);                        // Put Editors etc here, so we can delete/reload them separately
FixedDevicesSizer = new wxBoxSizer(wxHORIZONTAL);                   // Put the cdroms etc here, ditto

panelettesizer->Add(sizerControls, 1, wxEXPAND);
panelettesizer->Add(sizerSpacer, 0, wxEXPAND);

                      // The first control to grab is (probably) the textctrl that holds the filepath of the selection
sizerControls->Add(20, 25);
m_tbText = new TextCtrlBase(panelette, ID_FILEPATH, wxT(""), false, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER, wxDefaultValidator, wxT("ID_FILEPATH"));
sizerControls->Add(m_tbText, 3,  wxALIGN_CENTER_VERTICAL);
if (!USE_DEFAULT_TERMINALEM_FONT)  m_tbText->SetFont(TERMINAL_FONT);  // It makes sense to use the same font as the terminal emulator
if (m_tbText && !SHOW_TB_TEXTCTRL)  m_tbText->Hide();             // I find it difficult to imagine anyone NOT wanting this, so I'll just hide it rather than doing anything cleverer
sizerControls->Add(20, 25);                                       // Put a cosmetic spacer between this & the first editor
m_TextCrlCursor = m_tbText->GetCursor();                          // Grab the opportunity to store the type of cursor that textctrl's use. Needed for DnD.

                      // Now load the editor button(s)
DisplayEditorsInToolbar();

sizerControls->Add(EditorsSizer, 0, wxEXPAND);                    // This is where the editor etc buttons will go
sizerControls->Add(10, 25, 0, wxEXPAND);                          // Put a cosmetic spacer between the last editor and the drive buttons
sizerControls->Add(FixedDevicesSizer, 0, wxEXPAND);               // This is where the drive buttons will go
}

void MyFrame::DisplayEditorsInToolbar()    // These are stored in a separate sizer within the panelette sizer, for ease of recreation
{
  // We need to start by deleting any existing buttons, in case we're here from config & we're about to recreate
wxWindowList::compatibility_iterator node = panelette->GetChildren().GetLast();
while (node)                                                      // Iterate through the list of the sizer's children
  { wxWindow* win = node->GetData();
    if (EditorsSizer->GetItem(win) != NULL) win->Destroy();       // If this window is found in the fixed-devices sizer, destroy it. If not, let it live
    node = node->GetPrevious();
  }

size_t NoOfEditors = (size_t)wxConfigBase::Get()->Read(wxT("/Editors/NoOfEditors"), 0l);
for (size_t n=0; n < NoOfEditors; ++n)
  { EditorBitmapButton* button = new EditorBitmapButton(panelette, n);
    if (!button || !button->IsOk()) 
      { BriefMessageBox msg(wxString::Format(wxT("Couldn't load Editor %zu"),n+1), 1.5, wxT("Oops"));
        if (button) button->Destroy(); continue;
      }
    if (button->Ignore) button->Destroy();                        // If we don't want to know about this button, kill it
     else EditorsSizer->Add(button, 0, wxEXPAND | wxRIGHT, 5);
  }
}

void MyFrame::DoMiscUI(wxUpdateUIEvent& event)  // UI for odds & ends
{
MyGenericDirCtrl* active = GetActivePane(); 
if (!active)
  { event.Enable(false); return; }

wxWindow* win = wxWindow::FindFocus();
if (win && (win->GetName() == wxString(wxT("TerminalEm")) || win->GetName() == wxString(wxT("ID_FILEPATH"))))  // If the focus window is the TerminalEm etc
   { event.Enable(event.GetId() == SHCUT_PASTE); return; }  // All the other items are irrelevant here, and there's no easy way to know if there *is* anything relevant in the unix clipboard

bool IsArchive = (active && active->arcman && active->arcman->IsArchive());
switch(event.GetId())                // Otherwise it's (presumably) a pane
  { case SHCUT_PASTE: { bool clip = (MyGenericDirCtrl::filecount > 0); event.Enable(clip); return; }                        
    case SHCUT_RENAMETAB:
    case SHCUT_DELTAB:{ bool tab = (Layout->m_notebook->GetPageCount()); event.Enable(tab); return; }
    case SHCUT_UNDO:  { bool any = UnRedoManager::UndoAvailable() && !ThreadsManager::Get().PasteIsActive(); event.Enable(any); largesidebar1->Enable(any); return; } // Enable Undo button & sidebar
    case SHCUT_REDO:  { bool any = UnRedoManager::RedoAvailable() && !ThreadsManager::Get().PasteIsActive(); event.Enable(any); largesidebar2->Enable(any); return; } // Enable Redo button & sidebar
    case SHCUT_NEW:     event.Enable(!IsArchive); return;
    case SHCUT_HARDLINK:
    case SHCUT_SOFTLINK: event.Enable(!IsArchive && (MyGenericDirCtrl::filecount > 0)); return;  // We can't make new files or links within an archive
  }
}

void MyFrame::DoQueryEmptyUI(wxUpdateUIEvent& event)  //  This is for the items which are UIed depending on whether there's a selection
{
MyGenericDirCtrl* active = GetActivePane(); 
if (!active)
  { event.Enable(false); return; }

bool empty=false, DisableInArchive=false;

wxWindow* win = wxWindow::FindFocus(); if (win == NULL) return;  // First deal with when the focus window is the TerminalEm etc
if (win->GetName() == wxString(wxT("TerminalEm")) || win->GetName() == wxString(wxT("ID_FILEPATH")))
  { empty = (((wxTextCtrl*)win)->GetStringSelection().IsEmpty() || event.GetId() > SHCUT_DELETE);
#if wxVERSION_NUMBER >= 2810 
// TerminalEm Delete stopped working in 2.8.10. It's partly because the item with that shortcut, Trash, is always disabled when a pane loses focus, so gets no updateui events
    GetMenuBar()->Enable(SHCUT_TRASH, true);  // So override that here
    GetMenuBar()->Enable(SHCUT_DELETE, true); // and Delete to be on the safe side
    GetMenuBar()->Enable(SHCUT_REALLYDELETE, true); // Ditto
#endif
  }
 else
  { if (!SelectionAvailable) empty = true;                      // For if there isn't a tab (or we're pretending so while we add/delete) 
     else                                                       // There IS a valid tab.  Find the active pane, then ? anything selected
      {                                                         // Otherwise it's (presumably) a pane
        DisableInArchive = (event.GetId()==SHCUT_PROPERTIES     // We don't want to be able to ask about Properties inside an archive
                              && active && active->arcman && active->arcman->IsArchive());
        if (active && active->fileview == ISLEFT)  empty = active->GetPath().IsEmpty();    // If a dirview this is straightforward
         else 
          if ((active && active->GetPath().IsEmpty()) || (active->GetPath() == active->startdir)  // If fileview, path prob won't be empty even if no sel, so check if path==root
                                                      || (active->NoVisibleItems == true)) // or if the user has filtered out all the pane's content!
            empty = true;
      }
  }
event.Enable(!(empty || DisableInArchive));
}

void MyFrame::DoTabTemplateUI(wxUpdateUIEvent& event)  //  Grey when there are no Templates to load or delete
{
if (!GetMenuBar() || !Layout || !Layout->m_notebook) return;

GetMenuBar()->Enable(event.GetId(), Layout->m_notebook->NoOfExistingTemplates);   // Do the Delete item, which is standard
  // Normally I'd have done the next item, LoadTemplate, in the same way, using EVT_UPDATE_UI_RANGE.
  // However, perhaps because it is a menu rather than an item, this won't work (the macro doesn't forward ANY events!?).  So piggyback on Delete
GetMenuBar()->Enable(LoadTabTemplateMenu, Layout->m_notebook->NoOfExistingTemplates);
}

void MyFrame::DoMiscTabUI(wxUpdateUIEvent& event)      //  UI for the tab-head items
{
if (!GetMenuBar() || !Layout || !Layout->m_notebook) return;

if (event.GetId()==SHCUT_ALWAYS_SHOW_TAB)    
  GetMenuBar()->Check(SHCUT_ALWAYS_SHOW_TAB, Layout->m_notebook->AlwaysShowTabs);
 else
  GetMenuBar()->Check(SHCUT_SAME_TEXTSIZE_TAB, Layout->m_notebook->EqualSizedTabs);
}

void MyFrame::OnDirSkeletonUI(wxUpdateUIEvent& event)
{
MyGenericDirCtrl* origin = MyGenericDirCtrl::Originpane; // NB If Originpane is NULL, the clipboard must be empty so there's definitely no DirSkeleton to paste
bool has_dir(false); MyTab* active = GetActiveTab();
MyGenericDirCtrl* pane = NULL; if (active) pane = active->GetActivePane();

if (origin && pane && (pane->fileview == ISLEFT)) // It makes it much easier to avoid illegality if we disallow pasting to fileviews!
  { for (size_t n = 0; n < MyGenericDirCtrl::filecount; ++n) // Disable unless there's >0 dir in the clipboard
      { wxString itemfilename;
        if (origin->arcman && origin->arcman->IsArchive())
          { FakeDir* fd = origin->arcman->GetArc()->Getffs()->GetRootDir()->FindSubdirByFilepath(MyGenericDirCtrl::filearray.Item(n));
            if (!fd) continue;                    // We're not interested in files
            itemfilename = fd->GetFilename();
          }
         else
          { FileData fd(MyGenericDirCtrl::filearray.Item(n));
            if (fd.IsDir())                       // We're not interested in files
              itemfilename = fd.GetFilename();
          }
        if (itemfilename.empty()) continue;

      // OK, we have a valid item 'foo/'. Check there isn't already a foo or a foo/ in the destination dir
      // Doing it this way checks both for dest being the origin dir, and for the dest foo being a different filepath with the same filename
      if (pane && pane->arcman && pane->arcman->IsArchive())
        { FakeDir* dest = pane->arcman->GetArc()->Getffs()->GetRootDir()->FindSubdirByFilepath(pane->GetPath());
          if (dest && (dest->HasDirCalled(itemfilename) || dest->HasFileCalled(itemfilename))) continue;
        }
       else
        { FileData dest(pane->GetActiveDirPath() + wxFILE_SEP_PATH + itemfilename);
          if (dest.IsValid()) continue;
        }

       has_dir = true; break; 
      }
  }

event.Enable(pane && has_dir);
}

void MyFrame::DoNoPanesUI(wxUpdateUIEvent& event)
{
event.Enable(GetActiveTab());  // Disable item if no tab
}

void MyFrame::DoSplitPaneUI(wxUpdateUIEvent& event)
{
MyTab* active = GetActiveTab(); 
if (!active)
  { event.Enable(false); return; }                           // Disable menu item if no tab!
                                                             //  or if the orientation matches the menu-item command
if (event.GetId()==SHCUT_SPLITPANE_VERTICAL)    event.Enable(active->GetSplitStatus() != vertical);
if (event.GetId()==SHCUT_SPLITPANE_HORIZONTAL)  event.Enable(active->GetSplitStatus() != horizontal);
if (event.GetId()==SHCUT_SPLITPANE_UNSPLIT)     event.Enable(active->GetSplitStatus() != unsplit);
}

void MyFrame::DoColViewUI(wxUpdateUIEvent& event)
{
// We need a ptr to the TreeListHeaderWindow where the right-click occurred; or, if from the menubar, of the active pane
FileGenericDirCtrl* filectrl;

#if wxVERSION_NUMBER < 3000 
  TreeListHeaderWindow* header = wxDynamicCast(event.GetEventObject(), TreeListHeaderWindow); // In wx2.8 the event-object _is_ the header
#else
  wxMenu* menu = wxDynamicCast(event.GetEventObject(), wxMenu); wxCHECK_RET(menu, wxT("A menu event's object that isn't a menu"));
  TreeListHeaderWindow* header = wxDynamicCast(menu->GetClientData(), TreeListHeaderWindow);
#endif
if (!header)
  { // Not from the headerwindow contextmenu. Try the fileview one
    #if wxVERSION_NUMBER < 3000 
      filectrl = wxDynamicCast(event.GetEventObject(), FileGenericDirCtrl);
    #else
      filectrl = wxDynamicCast(menu->GetClientData(), FileGenericDirCtrl);
    #endif
    if (!filectrl)
      { MyGenericDirCtrl* view = GetActivePane();                         // Nope. Must be the menubar
        if (!view)
          { event.Enable(false); return; }
        if (view->fileview == ISLEFT)                                     // If the left-panelet has focus, get its partner
          filectrl = (FileGenericDirCtrl*)view->partner;
         else filectrl = (FileGenericDirCtrl*)view;                       // Otherwise it's the correct panelet already
      }
    wxCHECK_RET(filectrl, wxT("Can't find the FileGenericDirCtrl"));
    header = filectrl->GetHeaderWindow();
  }
wxCHECK_RET(header, wxT("Can't find the TreeListHeaderWindow"));
 
bool hidden = header->IsHidden(event.GetId() - SHCUT_SHOW_COL_EXT + 1);   // Is this col showing or hidden?
event.Check(!hidden);
}

void MyFrame::OnViewColMenu(wxCommandEvent& event)
{
MyGenericDirCtrl* view = GetActivePane(); if (view==NULL) return;
if (view->fileview == ISLEFT)
  view->partner->GetEventHandler()->ProcessEvent(event);
 else
  view->GetEventHandler()->ProcessEvent(event);
}

void MyFrame::OnHelpF1(wxKeyEvent& WXUNUSED(event))
{
if (HELPDIR.IsEmpty()) return;                            // In case we couldn't find our helpfiles
switch(HelpContext)
  { case HCconfigUDC:         Help->DisplaySection(wxT("ConfigureUserDefTools.htm")); return;
    case HCconfigShortcuts:   Help->DisplaySection(wxT("ConfiguringShortcuts.htm")); return;
    case HCconfigMisc:        Help->DisplaySection(wxT("ConfiguringMisc.htm")); return;
    case HCconfigMiscExport:  Help->DisplaySection(wxT("Export.htm")); return;
    case HCconfigMiscStatbar: Help->DisplaySection(wxT("Statusbar.htm")); return;
    case HCmultiplerename:    Help->DisplaySection(wxT("MultipleRenDup.htm")); return;
    case HCconfigDevices:     Help->DisplaySection(wxT("ConfiguringDevices.htm")); return;
    case HCconfigDisplay:     Help->DisplaySection(wxT("ConfiguringDisplay.htm")); return;
    case HCconfigNetworks:    Help->DisplaySection(wxT("ConfiguringNetworks.htm")); return;
    case HCconfigterminal:    Help->DisplaySection(wxT("ConfiguringTerminals.htm")); return;
    case HCconfigMount:       Help->DisplaySection(wxT("Mount.htm")); return;
    case HCarchive:           Help->DisplaySection(wxT("Archive.htm")); return;
    case HCterminalEm:        Help->DisplaySection(wxT("TerminalEm.htm")); return;
    case HCsearch:            Help->DisplaySection(wxT("Tools.htm")); return;
    case HCproperties:        Help->DisplaySection(wxT("Properties.htm")); return;
    case HCdnd:               Help->DisplaySection(wxT("DnD.htm")); return;
    case HCbookmarks:         Help->DisplaySection(wxT("Bookmarks.htm")); return;
    case HCfilter:            Help->DisplaySection(wxT("Filter.htm")); return;
    case HCopenwith:          Help->DisplaySection(wxT("OpenWith.htm")); return;
    case HCunknown:    
     default:                 Help->DisplaySection(wxT("Chapt.htm")); return;
  }
}

void MyFrame::OnHelpContents(wxCommandEvent& WXUNUSED(event))
{
if (HELPDIR.IsEmpty()) return;                             // In case we couldn't find our helpfiles
Help->DisplaySection(wxT("Chapt.htm"));
}

void MyFrame::OnHelpFAQs(wxCommandEvent& WXUNUSED(event))
{
if (HELPDIR.IsEmpty()) return;                             // In case we couldn't find our helpfiles
Help->DisplaySection(wxT("FAQ.htm"));
}

void MyFrame::OnHelpAbout(wxCommandEvent& WXUNUSED(event))
{
if (HELPDIR.IsEmpty()) return;
HtmlDialog(_("About"), HELPDIR + wxT("/About.htm"), wxT("AboutDlg"), this, wxSize(300, 300));
}

void MyFrame::OnSaveTabs(wxCommandEvent& WXUNUSED(event))
{
Layout->m_notebook->saveonexit = GetMenuBar()->IsChecked(SHCUT_SAVETABS_ONEXIT);  // Update the saveonexit bool, as this gets saved too
Layout->m_notebook->SaveDefaults(); 
}

void MyFrame::OnQuit(wxCommandEvent& WXUNUSED(event))
{
Close(true);
}


void MyFrame::OnUpdateTrees(const wxString& path, const wxArrayInt& PanesToUpdate, const wxString& newstartdir /*=wxT("")*/, bool force /*=false*/)
{ // Tells all treectrls that branch 'path' needs updating. PanesToUpdate hold which panes MUST be done
  // Force means either it's from ArchiveStream so ignore USE_FSWATCHER, or else it's the DoItNow! message from UpdateTrees()
if (USE_FSWATCHER && !force)
  return;                                               // as this is done elsewhere

static wxArrayInt IDs;
static wxArrayString PathsToUpdate;

int count = Layout->m_notebook->GetPageCount();         // How many tabs are there?
if (!count) return;

              // This method comes in 2 parts.  First we store the data, so that a cluster of actions can be updated all at once, not piecemeal
WX_APPEND_ARRAY(IDs, PanesToUpdate);                    // Store the path and IDs in the appropriate static arrays
if (path != wxT("**FLAG**"))                            // If this isn't a phantom path passed by twin OnUpdateTrees()
  {  if (newstartdir.IsEmpty())                         // Check that we're not changing startdir 
      { if (PathsToUpdate.Index(path) == wxNOT_FOUND)   // Providing we don't have this path already
            PathsToUpdate.Add(path);                    //    store it
      }
     else
      { PathsToUpdate.Add(wxT("**RECREATE**"));         // If we ARE changing startdir, flag this
        PathsToUpdate.Add(path);                        //   & then store consecutively both path & newstartdir
        PathsToUpdate.Add(newstartdir);
      }
  }     
  
if (MyGenericDirCtrl::Clustering) return;               // If the flag signals that we're in mid-cluster, do nothing else yet
  
              // Now for part 2.  If we're here, we've finished storing a cluster, so actually do the Updating
for (size_t n=0; n < PathsToUpdate.Count(); ++n)
  { wxString path, newstartdir, parent;
    bool changeroot = false;
    
    path = PathsToUpdate[n];
    if (path == wxT("**RECREATE**"))                    // See if we've flagged to uproot a tree because of a change to 'root' item
      { path = PathsToUpdate[++n]; newstartdir = PathsToUpdate[++n];  // If so, set it up
        if (path != wxFILE_SEP_PATH) parent = path.BeforeLast(wxFILE_SEP_PATH);  // Check path isn't root dir, as this would cause an error below
            else parent = path;
        changeroot = true;
      }
    
    if (!path.IsEmpty())                                // Prune path of any terminal /'s to aid comparisons
      while (path.Right(1) == wxFILE_SEP_PATH && path.Len() > 1)  path.RemoveLast();
      
    for (int tab=0; tab < count; ++tab)
      { MyTab* page = (MyTab*)Layout->m_notebook->GetPage(tab);
        DirGenericDirCtrl *dirL = page->m_splitterLeftTop->m_left;   // For sanity's sake, get ptrs to GenericDirCtrls
        FileGenericDirCtrl *dirLF = page->m_splitterLeftTop->m_right;  
        DirGenericDirCtrl *dirR = page->m_splitterRightBottom->m_left;
        FileGenericDirCtrl *dirRF = page->m_splitterRightBottom->m_right;
        if (path==wxEmptyString)                           // No path, because of error or need multiple alterations so easier just to RELOAD treectrls
          { dirL->ReloadTree(); dirR->ReloadTree(); continue; }
        
        if (changeroot)                                    // We've renamed/destroyed startdir, so need to regrow at least one tree on different rootstock
          { if (dirL->fulltree || dirL->startdir==path || dirL->startdir.BeforeLast(wxFILE_SEP_PATH)==path)    // If L dir-pane has path as its 'root'
              { dirL->startdir = newstartdir; dirL->ReCreateTreeFromSelection(); dirLF->ReCreateTreeFromSelection(); }  //   redo it from new startdir
             else                                          // Otherwise, refresh but with parent as path. This catches panes that contain path but not as 'root'
              { dirL->ReloadTree(parent, IDs); dirLF->ReloadTree(parent, IDs); }
              
            if (dirR->fulltree || dirR->startdir==path || dirR->startdir.BeforeLast(wxFILE_SEP_PATH)==path)  // Ditto for R pane
              { dirR->startdir = newstartdir; dirR->ReCreateTreeFromSelection();  dirRF->ReCreateTreeFromSelection(); }
             else
              { dirR->ReloadTree(parent, IDs); dirRF->ReloadTree(parent, IDs); }
          }
         else
          { if (dirL->fulltree || dirL->startdir==path || dirL->startdir.BeforeLast(wxFILE_SEP_PATH)==path)  // Again check if we're refreshing startdir, even if it hasn't changed
              { dirL->ReCreateTreeFromSelection(); dirLF->ReCreateTreeFromSelection(); }
             else { dirL->ReloadTree(path, IDs); dirLF->ReloadTree(path, IDs); }           // Otherwise, do standard Refreshes
            if (dirR->fulltree || dirR->startdir==path || dirR->startdir.BeforeLast(wxFILE_SEP_PATH)==path)
              { dirR->ReCreateTreeFromSelection(); dirRF->ReCreateTreeFromSelection(); }
             else { dirR->ReloadTree(path, IDs); dirRF->ReloadTree(path, IDs); }
          }
      }
  }
  
PathsToUpdate.Clear(); IDs.Clear();                          // Clear the arrays, ready for next time
}

void MyFrame::UpdateTrees()  // At the end of a cluster of actions, tells all treectrls actually to do the updating they've stored
{
wxString path(wxT("**FLAG**")); wxArrayInt IDs;              // Create 'empty' parameters to pass (wxEmptyString is already in use)

MyGenericDirCtrl::Clustering = false;                        // Reset the flag that was preventing the updating

OnUpdateTrees(path, IDs, wxT(""), true);                     // Now call its twin, using blank parameters.  The reset Clustering flag means things actually happen
}

void MyFrame::OnUnmountDevice(wxString& path, const wxString& replacement/*=""*/)  // When we've successfully umounted eg a floppy, reset treectrl(s) displaying it to HOME
{
if (path==wxEmptyString)   return;

int count = Layout->m_notebook->GetPageCount();              // How many tabs are there?
if (!count) return;

wxString newdir(replacement), chosen;
FileData CheckValid(replacement);
if (!CheckValid.IsDir())
  { newdir = wxGetApp().GetHOME();                                               // Use this as default
    wxConfigBase* config = wxConfigBase::Get();
    if (config==NULL)  { wxLogError(_("Couldn't load configuration!?")); return; }

    long choice; config->Read(wxT("/DefaultPathOnUmount/Type"), &choice);  // Find out which case to use below

    switch(choice)
      { case current:   newdir = GetCwd(); break;                // One alternative is to use the current working dir
        case selected:  config->Read(wxT("/DefaultPathOnUmount/Chosen"), &chosen);  // Another is to use choice stored in config. 
                        wxFileName fn(chosen);                                      // If so, first check it's a valid dir!
                        if (wxDirExists(fn.GetFullPath()))  newdir = chosen;        // If it is, use it.  If not, stick to default
      }
  }

for (int tab=0; tab < count; tab++)
  { MyTab* page = (MyTab*)Layout->m_notebook->GetPage(tab);
    DirGenericDirCtrl* dirL = page->m_splitterLeftTop->m_left;      
    DirGenericDirCtrl* dirR = page->m_splitterRightBottom->m_left;

    if (dirL->startdir == path)  dirL->OnOutsideSelect(newdir);
    if (dirR->startdir == path)  dirR->OnOutsideSelect(newdir);
  }
}

void MyFrame::OnActivate(wxActivateEvent& event)
{
if (!event.GetActive())
  PreviewManager::Clear(); // Needed to prevent any preview popup persisting (though that would only have happened if the mouse was inside the preview at the time)
}

void MyFrame::OnUndo(wxCommandEvent& WXUNUSED(event))
{
if (ThreadsManager::Get().PasteIsActive())  // We shouldn't try to do 2 pastes at a time, and this probably involves pasting
 { BriefMessageBox(_("Please try again in a moment"), 2,_("I'm busy right now")); return; }

UnRedoManager::UndoEntry();
}

void MyFrame::OnUndoSidebar(wxCommandEvent& WXUNUSED(event))
{
if (ThreadsManager::Get().PasteIsActive())  // We shouldn't try to do 2 pastes at a time, and this probably involves pasting
 { BriefMessageBox(_("Please try again in a moment"), 2,_("I'm busy right now")); return; }

UnRedoManager::OnUndoSidebar();
}

void MyFrame::OnRedo(wxCommandEvent& WXUNUSED(event))
{
if (ThreadsManager::Get().PasteIsActive())  // We shouldn't try to do 2 pastes at a time, and this probably involves pasting
 { BriefMessageBox(_("Please try again in a moment"), 2,_("I'm busy right now")); return; }

UnRedoManager::RedoEntry();
}

void MyFrame::OnRedoSidebar(wxCommandEvent& WXUNUSED(event))
{
if (ThreadsManager::Get().PasteIsActive())  // We shouldn't try to do 2 pastes at a time, and this probably involves pasting
 { BriefMessageBox(_("Please try again in a moment"), 2,_("I'm busy right now")); return; }

UnRedoManager::OnRedoSidebar();
}

void MyFrame::OnCut(wxCommandEvent& event)
{
if (ThreadsManager::Get().PasteIsActive())  // We shouldn't try to do 2 pastes at a time, and Cut involves a paste-to-can
 { BriefMessageBox(_("Please try again in a moment"), 2,_("I'm busy right now")); return; }

MyGenericDirCtrl* pane = GetActivePane();
if (pane) pane->OnShortcutCut(event);   // Find the active MyGenericDirCtrl & pass request to it
}

void MyFrame::OnCopy(wxCommandEvent& event)
{
MyGenericDirCtrl* pane = GetActivePane();
if (pane) pane->OnShortcutCopy(event);
}

void MyFrame::OnPaste(wxCommandEvent& event)
{
if (ThreadsManager::Get().PasteIsActive())  // We shouldn't try to do 2 pastes at a time, and it was probably a mistake anyway
 { BriefMessageBox(_("Please try again in a moment"), 2,_("I'm busy right now")); return; }

MyGenericDirCtrl* pane = GetActivePane();
if (pane) pane->OnShortcutPaste(event);
}

void MyFrame::OnProcessCancelled(wxCommandEvent& WXUNUSED(event))
{
DoOnProcessCancelled();
}

void MyFrame::OnProcessCancelledUI(wxUpdateUIEvent& event)
{
event.Enable(ThreadsManager::Get().PasteIsActive());
}

void MyFrame::DoOnProcessCancelled()
{
wxWindow* focus = wxGetApp().GetFocusController().GetCurrentFocus();
bool fromtextctrl = dynamic_cast<TextCtrlBase*>(focus);
bool frompane = dynamic_cast<wxGenericDirCtrl*>(focus);
int flags(0);
if (fromtextctrl) flags |= ThT_text;
if (frompane)     flags |= ThT_paste;
if (!flags)       flags = ThT_all;

wxCriticalSectionLocker locker(ThreadsManager::Get().GetPasteCriticalSection()); // When we have other thread-types, use the correct type here
ThreadsManager::Get().CancelThread(flags);
}

void MyFrame::OnNew(wxCommandEvent& event)
{
MyGenericDirCtrl* pane = GetActivePane();
if (pane) pane->NewFile(event);
}

void MyFrame::OnHardLink(wxCommandEvent& event)
{
MyGenericDirCtrl* pane = GetActivePane();
if (pane) pane->OnShortcutSoftLink(event);
}

void MyFrame::OnSoftLink(wxCommandEvent& event)
{
MyGenericDirCtrl* pane = GetActivePane();
if (pane) pane->OnShortcutSoftLink(event);
}

void MyFrame::OnTrash(wxCommandEvent& event)
{
MyGenericDirCtrl* pane = GetActivePane();
if (pane) pane->OnShortcutTrash(event);
}

void MyFrame::OnDelete(wxCommandEvent& event)
{
if (ThreadsManager::Get().PasteIsActive())  // We shouldn't try to do 2 pastes at a time, and Delete involves a paste-to-can
 { BriefMessageBox(_("Please try again in a moment"), 2,_("I'm busy right now")); return; }

MyGenericDirCtrl* pane = GetActivePane();
if (pane) pane->OnShortcutDel(event);
}

void MyFrame::OnReallyDelete(wxCommandEvent& WXUNUSED(event))
{
MyGenericDirCtrl* pane = GetActivePane();
if (pane) pane->OnShortcutReallyDelete();
}

void MyFrame::OnRename(wxCommandEvent& event)
{
MyGenericDirCtrl* pane = GetActivePane();
if (pane) pane->OnRename(event);
}

void MyFrame::OnRefresh(wxCommandEvent& event)
{
MyGenericDirCtrl* pane = GetActivePane();
if (pane) pane->OnRefreshTree(event);
}

void MyFrame::OnDup(wxCommandEvent& event)
{
MyGenericDirCtrl* pane = GetActivePane();
if (pane) pane->OnDup(event);
}

void MyFrame::OnProperties(wxCommandEvent& event)
{
MyGenericDirCtrl* pane = GetActivePane();
if (pane) pane->OnProperties(event);
}

void MyFrame::OnFilter(wxCommandEvent& event)
{
MyGenericDirCtrl* pane = GetActivePane();
if (pane) pane->OnFilter(event);
}

void MyFrame::OnToggleHidden(wxCommandEvent& event)
{
MyGenericDirCtrl* active = GetActivePane();
if (active==NULL)    return;
if (active->fileview == ISRIGHT) ((FileGenericDirCtrl*)active)->OnToggleHidden(event);
  else ((DirGenericDirCtrl*)active)->OnToggleHidden(event);
}

void MyFrame::OnArchiveExtract(wxCommandEvent& WXUNUSED(event))
{
MyGenericDirCtrl* active = GetActivePane();
if (active==NULL)    return;

if (active->arcman->IsArchive())         // If we're peering into an archive stream, tell this what to do
  { active->arcman->GetArc()->OnExtract(); return; }

Archive* archive = new Archive(active);  // Otherwise do the standard-type extract
HelpContext = HCarchive;
archive->ExtractOrVerify();
HelpContext = HCunknown;
}

void MyFrame::OnArchiveCreate(wxCommandEvent& WXUNUSED(event))
{
if (!GetActivePane()) return;

Archive* archive = new Archive(GetActivePane());
HelpContext = HCarchive;
archive->CreateOrAppend(true);
HelpContext = HCunknown;
}

void MyFrame::OnArchiveAppend(wxCommandEvent& WXUNUSED(event))
{
if (!GetActivePane()) return;

Archive* archive = new Archive(GetActivePane());
HelpContext = HCarchive;
archive->CreateOrAppend(false);
HelpContext = HCunknown;
}

void MyFrame::OnArchiveTest(wxCommandEvent& WXUNUSED(event))
{
if (!GetActivePane()) return;

Archive* archive = new Archive(GetActivePane());
HelpContext = HCarchive;
archive->ExtractOrVerify(false);                       // False signifies Verify
HelpContext = HCunknown;
}

void MyFrame::OnArchiveCompress(wxCommandEvent& WXUNUSED(event))
{
if (!GetActivePane()) return;

Archive* archive = new Archive(GetActivePane());
HelpContext = HCarchive;
archive->Compress();
HelpContext = HCunknown;
}

void MyFrame::OnLaunchTerminal(wxCommandEvent& WXUNUSED(event))    // Ctrl-T by default.  Launch a stand-alone terminal session, using the selected dir 
{
    // Originally, for any terminal type, it was possible to cd to the cwd, open the terminal, then cwd back
    // However recent konsole versions e.g. 2.2.2 don't notice the cwd; instead they insist on konsole --workdir foo
    // Fortunately this seems to be backwards-compatible to at least 1.2.1 (in SuSE 8.2)
wxString path;
MyGenericDirCtrl* active = GetActivePane();
if (active)
  { path = active->GetPath();             //  Get the dir currently highlit. Path, not filepath
    if (path.IsEmpty())  // This might happen e.g. if we just d-clicked a dir in a fileview, so opening it in the dirview and therefore rebasing the fileview
       path = active->partner->GetPath(); // If so, try the other side of the twin-pane
  }
if (path.IsEmpty())  path = GetCwd();
FileData fd(path); 
if (fd.IsSymlink() && fd.IsSymlinktargetADir(true))             // If it's a symlink-to-dir, open in the destination dir
  path = fd.GetUltimateDestination();
 else if (!fd.IsDir()) path = fd.GetPath();                     // because SetWorkingDirectory() borks if it's passed a FILEpath

if (TERMINAL.StartsWith(wxT("konsole")))
  { EscapeQuote(path);  EscapeSpace(path);                      // Cope with weird filepaths, as wxExecute doesn't
    wxExecute(wxT("konsole --workdir ") + path);
  }
 else
  { wxString oldwd = GetCwd();                                  // Save the Current working directory, to restore later.  ?necessary, but it sounds like good practice
    SetWorkingDirectory(path);                                  // If it's valid, make it the current working dir, so that the terminal will cd into this
      
    wxExecute(TERMINAL);
    SetWorkingDirectory(oldwd);
  }
}

void MyFrame::OnCommandLine(wxCommandEvent& WXUNUSED(event))
{
Layout->OnCommandLine();
}

void MyFrame::OnMountPartition(wxCommandEvent& WXUNUSED(event))
{
HelpContext = HCconfigMount;
Layout->m_notebook->DeviceMan->OnMountPartition();
HelpContext = HCunknown;
}

void MyFrame::OnUnMountPartition(wxCommandEvent& WXUNUSED(event))
{
HelpContext = HCconfigMount;
Layout->m_notebook->DeviceMan->OnUnMountPartition();
HelpContext = HCunknown;
}

void MyFrame::OnMountIso(wxCommandEvent& WXUNUSED(event))
{
HelpContext = HCconfigMount;
Layout->m_notebook->DeviceMan->OnMountIso();
HelpContext = HCunknown;
}

void MyFrame::OnMountNfs(wxCommandEvent& WXUNUSED(event))
{
HelpContext = HCconfigMount;
Layout->m_notebook->DeviceMan->OnMountNfs();
HelpContext = HCunknown;
}

void MyFrame::OnMountSshfs(wxCommandEvent& WXUNUSED(event))
{
HelpContext = HCconfigMount;
Layout->m_notebook->DeviceMan->OnMountSshfs();
HelpContext = HCunknown;
}

void MyFrame::OnMountSamba(wxCommandEvent& WXUNUSED(event))
{
HelpContext = HCconfigMount;
Layout->m_notebook->DeviceMan->OnMountSamba();
HelpContext = HCunknown;
}

void MyFrame::OnUnMountNetwork(wxCommandEvent& WXUNUSED(event))
{
HelpContext = HCconfigMount;
Layout->m_notebook->DeviceMan->OnUnMountNetwork();
HelpContext = HCunknown;
}

void MyFrame::OnTermEm(wxCommandEvent& WXUNUSED(event))
{
Layout->DoTool(terminalem);
}

void MyFrame::OnLocate(wxCommandEvent& WXUNUSED(event))
{
Layout->DoTool(locate);
}

void MyFrame::OnFind(wxCommandEvent& WXUNUSED(event))
{
Layout->DoTool(find);
}

void MyFrame::OnGrep(wxCommandEvent& WXUNUSED(event))
{
Layout->DoTool(grep);
}

void MyFrame::OnToolsLaunch(wxCommandEvent& event)
{
int id = event.GetId();
if (id < SHCUT_TOOLS_LAUNCH || id >= SHCUT_TOOLS_LAUNCH_LAST)  return;
  
Layout->m_notebook->LaunchFromMenu->Run(id);
}

void MyFrame::OnToolsRepeat(wxCommandEvent& WXUNUSED(event))  // Repeat the last command
{
Layout->m_notebook->LaunchFromMenu->RepeatLastCommand();
}

void MyFrame::DoRepeatCommandUI(wxUpdateUIEvent& event)  //  Disable when there hasn't been a previous command to repeat
{
GetMenuBar()->Enable(event.GetId(), (int)Layout->m_notebook->LaunchFromMenu->GetLastCommand() != -1); 
}

void MyFrame::OnEmptyTrash(wxCommandEvent& event)
{
Layout->m_notebook->DeleteLocation->EmptyTrash(event.GetId() == SHCUT_EMPTYTRASH);  // Either empty the trash-can or the 'deleted'can
}

void MyFrame::ToggleFileviewSizetype(wxCommandEvent& WXUNUSED(event))
{
SHOW_RECURSIVE_FILEVIEW_SIZE = !SHOW_RECURSIVE_FILEVIEW_SIZE;
}

void MyFrame::ToggleRetainRelSymlinkTarget(wxCommandEvent& WXUNUSED(event))
{
RETAIN_REL_TARGET = !RETAIN_REL_TARGET;
}

void MyFrame::ToggleRetainMtimeOnPaste(wxCommandEvent& WXUNUSED(event))
{
RETAIN_MTIME_ON_PASTE = !RETAIN_MTIME_ON_PASTE;
}

void MyFrame::OnConfigure(wxCommandEvent& WXUNUSED(event))
{
configure->Configure4Pane();
}

void MyFrame::OnBeginDrag(wxWindow* dragorigin)
{
if (!dragorigin) return;
if (ThreadsManager::Get().IsActive())
  return; // Dragging doesn't work properly while threads are eating cpu cycles, and attempts can leave artifacts on the screen

if (m_DragMutex.TryLock() == wxMUTEX_NO_ERROR) { wxLogTrace(wxT("%s"),wxT("Entering OnBeginDrag:  Mutex was UNLOCKED!?")); }
if (m_dragMode)
  { // This happened to me once: during a drag, 4Pane froze for minutes due to a stale nfs mount
    // So reset everything to factory defaults
    oldCursor = m_StandardCursor; SetCursor(m_StandardCursor);
    m_dragMode = false; m_DragMutex.Unlock(); m_TreeDragMutex.Unlock();
    wxLogTrace(wxT("%s"),wxT("m_dragMode == true when it shouldn't have been"));
    return;
  }                      

#if defined(__WXGTK3__)
  wxPoint pt = wxGetMousePosition();
#else
  wxPoint pt = dragorigin->ClientToScreen(wxGetMousePosition()); // In gtk2 we use fullscreen mode: see below
#endif

oldCursor = GetCursor();
SetCursor(DnDStdCursor);

m_dragImage = new MyDragImage();
if (!m_dragImage) { wxLogTrace(wxT("%s"),wxT("m_dragImage creation failed")); SetCursor(oldCursor); return; }

m_dragImage->pt = pt;
m_dragImage->SetImageType(ParseMetakeys());               // This sets the initial image type according to the metakey state - - - drag, softlink etc

// Use fullscreen mode in gtk2; otherwise the image overwrites the statusbar in wx2.8, and they fail to draw in wx3.0 :/
bool fullscreen(true);
#if defined(__WXGTK3__)
  fullscreen = false;
#endif
m_dragImage->BeginDrag(wxPoint(0, 0), this, fullscreen);  
m_dragImage->Move(pt);
m_dragImage->Show();

lastwin = dragorigin;                                     // Store which treectrl we're dealing with

m_dragMode = true;                                        // Flags that we're in DnD mode
m_LeftUp = false;                              
m_finished = false;
ESC_Pressed = false;
m_DragMutex.Unlock();                                     // Finally, do the big unlock that makes everything work
}

void MyFrame::DoMutex(wxMouseEvent& event)
{
#if wxVERSION_NUMBER >= 2901 
  bool leftup = !wxGetMouseState().LeftIsDown();
#else
  bool leftup = !wxGetMouseState().LeftDown();
#endif
if (m_DragMutex.TryLock() == wxMUTEX_NO_ERROR)             // If the mutex is free, lock it & pass the event on to the correct method
  { if (event.LeftUp() || m_LeftUp || leftup)    OnEndDrag(event);   //  ie end if LeftUp now or recently
     else
      { if (event.Dragging())  OnMouseMove(event);         //   otherwise move
          else  { m_DragMutex.Unlock(); return; }          // If all else fails,  abort
      }
  }
 else
  { if (event.LeftUp() && !m_finished)  m_LeftUp = true;   // If we just caught LeftUp, & it's the real one, flag it for next time
    return;                                                // Either way, abort
  }
}

void MyFrame::OnESC_Pressed()  // If we're in DnD, this will cancel it.  If not, nothing
{
if (m_dragMode)
  { ESC_Pressed = true; m_LeftUp = true; }                // This means that the next time thru, DoMutex will call OnEndDrag, which will abort
}


void MyFrame::OnMouseMove(wxMouseEvent& event)
{
if (m_dragMode == false) { wxLogTrace(wxT("%s"),wxT("m_dragMode == false")); return; }

wxPoint pt;
wxWindow* win = wxFindWindowAtPointer(pt);                  // Find window under mouse, & pointer screen coords

if (win == NULL) { m_DragMutex.Unlock(); return; }          // Otherwise it crashes, when outside this frame

if (win != lastwin)                                         // If we've left the previous window
  { m_dragImage->SetCursor(dndUnselect);                    // In case cursor is the Selected type, revert to standard
    if (lastwin->GetName() == wxT("MyTreeCtrl"))            // If we just left a treectrl
        m_dragImage->OnLeavingPane(lastwin);                //      tell MyDragImage, which may start the scroll-timer

if (wxGetApp().GetIsWaylandSession())
  {  // Without this on Wayland, at least in fedora 25/6, a stale image is often left when leaving a pane. Update()/Refresh() etc don't help
    wxSafeYield(); // This seems to work 90+% of the time, though at the cost of making the toolbar flicker
  } 

    lastwin = win;                                          // Store current window  
  }

if (win->GetName() == wxT("MyTreeCtrl"))                    // If we're within a treectrl
  { MyTreeCtrl* tree = (MyTreeCtrl*)win;
    if (tree==NULL) { wxLogTrace(wxT("%s"),wxT("*************** tree was dead wood! *******************")); }
    pt = win->ScreenToClient(pt);

    int flags; wxTreeItemId item = tree->HitTest(pt, flags);  // Find out if we're near a tree item
    if (item.IsOk() && !(flags & wxTREE_HITTEST_ONITEMRIGHT))
      m_dragImage->SetCursor(dndSelectedCursor);
     else  m_dragImage->SetCursor(dndStdCursor);
  }
 else if (win->GetName() == wxT("Editor") || win->GetName() == wxT("Device") || win->GetName() == wxT("TerminalEm"))
        m_dragImage->SetCursor(dndSelectedCursor);

#if defined(__WXGTK3__)
  m_dragImage->pt = event.GetPosition();
#else
  m_dragImage->pt = ClientToScreen(event.GetPosition());   // Store the mouse position, so that the animation timer can redraw the image correctly
#endif

m_CtrlPressed = event.m_controlDown;                       // Store the pattern of metakeys, so we know if we're copying, moving etc 
m_ShPressed = event.ShiftDown();
m_AltPressed = event.AltDown();
m_dragImage->SetImageType(ParseMetakeys());                // Change image type to fit the metakey state - - - drag, softlink etc

        // Move and show the image again    ************ I killed the Show, not needed
if (m_dragMode) m_dragImage->Move(event.GetPosition());    // Now do the Image Move
  else  return;

#if wxVERSION_NUMBER <= 3000  // 3.0.1 and above do different things with cursors, so we yield elsewhere
    // We need to Yield() from time to time, otherwise other events accumulate which have to run their course before the Drop, resulting in a second or 2 of freeze
    // However Yielding every mouse-movement makes the Drag very jerky, so do it 1:10
static size_t counter = 0; 
if (!(++counter % 10))   wxYieldIfNeeded();
#endif

if (m_LeftUp && !m_finished) { OnEndDrag(event); return; } // If LeftUp happened while we were here, jump to EndDrag

m_DragMutex.Unlock();
}

void MyFrame::OnEndDrag(wxMouseEvent& event) 
{
if (m_dragMode == false)                                // Check if we're in DnD or not
  { m_TreeDragMutex.Unlock(); return; }
m_finished = true;                                      // Set the flag that prevents reentry

m_dragMode = false;                                     // Unflag DnD

// End the drag and delete the drag image
if (m_dragImage)
  { m_dragImage->Hide();                               // Shouldn't be necessary, but it is.
    m_dragImage->EndDrag();
#if wxVERSION_NUMBER > 3000 
    m_dragImage->SetCursor(dndOriginalCursor);
#endif
    delete m_dragImage;
    m_dragImage = NULL;

    if (lastwin && wxGetApp().GetIsWaylandSession())
      lastwin->Refresh(); // Without this on Wayland, at least in fedora 25/6, a stale image remains
  }

SetCursor(oldCursor);

if (ESC_Pressed) { m_TreeDragMutex.Unlock(); return; } // If we're here because of an ESC, abort

                // Finally, we need to hand over to the destination window to do the actual file-work 
wxPoint pt;
wxWindow* win = wxFindWindowAtPointer(pt);                  // Find window under mouse, & pointer screen coords
if (win)                                            // (Otherwise it crashes when LButtonUp occurs outside this frame) 
  {  if (win->GetName() == wxT("MyTreeCtrl"))               // Is it a tree . . . . ?            
      { ((MyTreeCtrl*)win)->OnEndDrag((wxTreeEvent&)event); return; }
    if (win->GetName() == wxT("Editor"))                    // Is it an editor . . . . ?
      { ((EditorBitmapButton*)win)->OnEndDrag(); return; }  
    if (win->GetName() == wxT("Device"))                    // Is it a device . . . . ?
      { ((DeviceBitmapButton*)win)->OnEndDrag(); return; }
    if (win->GetName() == wxT("TerminalEm"))                // Are we dropping onto the Terminal Emulator or Command Line ?
      { ((TerminalEm*)win)->OnEndDrag(); return; }
  }

m_TreeDragMutex.Unlock();
}

void MyFrame::OnDnDRightclick(wxMouseEvent& event)
{
if (m_dragMode == false) { event.Skip(); return; }            // We only want it to happen during D'n'D

m_dragImage->DoPageScroll(event);
}

void MyFrame::OnMouseWheel(wxMouseEvent& event)
{
if (m_dragMode == false) { event.Skip(); return; }            // We only want it to happen during D'n'D

m_dragImage->DoLineScroll(event);
}

enum myDragResult MyFrame::ParseMetakeys()  // Use the metakey store to find if we're trying to copy, move, link . . .  
{
int metakeys = wxACCEL_NORMAL;
if (wxGetKeyState(WXK_CONTROL))   metakeys |= wxACCEL_CTRL;
if (wxGetKeyState(WXK_SHIFT))     metakeys |= wxACCEL_SHIFT;
if (wxGetKeyState(WXK_ALT))       metakeys |= wxACCEL_ALT;

if (metakeys == METAKEYS_COPY)     return myDragCopy;
if (metakeys == METAKEYS_HARDLINK) return myDragHardLink;
if (metakeys == METAKEYS_SOFTLINK) return myDragSoftLink;
return myDragMove;                                            // Seems a sensible default
}


MyTab* MyFrame::GetActiveTab()  // Returns the currently-active tab, if there is one
{
if (!Layout || !Layout->m_notebook)   return NULL;
if (Layout->m_notebook->GetPageCount() == 0)   return NULL;

int current = Layout->m_notebook->GetSelection();
if (current == wxNOT_FOUND) return NULL;

return (MyTab*)(Layout->m_notebook->GetPage(current));
}

void MyFrame::RefreshAllTabs(wxFont* font, bool ReloadDirviewTBs /*=false*/)  // Refreshes all panes on all tabs, optionally with a new font, or mini-toolbars
{
if (Layout->m_notebook==NULL)   return;
size_t count = Layout->m_notebook->GetPageCount(); if (!count)   return;

for (size_t n=0; n < count; ++n)
  { MyTab* tab = (MyTab*)Layout->m_notebook->GetPage(n); if (tab == NULL) continue;
    MyGenericDirCtrl* active = tab->GetActiveDirview(); if (active == NULL)  continue;
    if (ReloadDirviewTBs)  active->ReloadDirviewToolbars();
     else active->RefreshVisiblePanes(font);
  }
}

void MyFrame::SetSensibleFocus()    // Ensures that keyboard focus is held by a valid window, usually the active pane
{
HelpContext = HCunknown;
MyGenericDirCtrl* active = GetActivePane();
if (active) { active->GetTreeCtrl()->SetFocus(); return; } // The standard scenario  

if (m_tbText && m_tbText->IsShown())
  { m_tbText->SetFocus(); return; }                        // If all the tabs have been deleted, try for the toolbar textctrl

SetFocus();                                                // Failing all else, use the frame itself
}

MyGenericDirCtrl* MyFrame::GetActivePane()    // Returns the most-recently-active pane in the visible tab
{
MyTab* tab = GetActiveTab();
if (tab == NULL) return NULL;

return tab->GetActivePane();
}

void MyFrame::OnReplicate(wxCommandEvent& event){ GetActivePane()->OnReplicate(event); }  // Find the active MyGenericDirCtrl & pass request to it
void MyFrame::OnSwapPanes(wxCommandEvent& event){ GetActivePane()->OnSwapPanes(event); }  // Ditto

void MyFrame::OnAddToBookmarks(wxCommandEvent& WXUNUSED(event))
{
wxString path = GetActivePane()->GetPath();                  // Find the current selection, which is presumably to be bookmarked
HelpContext = HCbookmarks;
Layout->m_notebook->BookmarkMan->AddBookmark(path);          //  & pass it to the Bookmark manager
HelpContext = HCunknown;
}

void MyFrame::OnManageBookmarks(wxCommandEvent& WXUNUSED(event))
{
HelpContext = HCbookmarks;
Layout->m_notebook->BookmarkMan->ManageBookmarks();
HelpContext = HCunknown;
}

void MyFrame::OnBookmark(wxCommandEvent& event)
{
wxString filepath, path = Layout->m_notebook->BookmarkMan->RetrieveBookmark(event.GetId());  // Use Bookmark manager to retrieve the path of the requested bookmark
if (path.IsEmpty())  return;
filepath = path;                                            // Store it, as it gets altered in OnOutsideSelect()

MyGenericDirCtrl* active = GetActivePane();
if (active == NULL)    return;
if (active->fileview == ISRIGHT)  active = active->partner; // It's much easier if we use the dirview to set both panes

if (active->fulltree)
  path = FileData::GetUltimateDestination(path);            // I don't grok why, but MyGenericDirCtrl::ExpandPath can't cope with paths containing symlinks in fulltree mode


active->OnOutsideSelect(filepath);                          // If everything's valid, select it into the currently-active dirview

if (active->fulltree)                                       //  If in fulltree mode, make fileview-pane match too.  If not, OnOutsideSelect() does it anyway
  { active->partner->startdir = path;  
    active->partner->ReCreateTreeFromSelection();
  }

  // Is it a dir or a file?  If it's a file, it's nice to set the filepath to it in the fileview
active->GetTreeCtrl()->UnselectAll();                      // but first remove any current selection: important in fulltree mode
FileData fd(path);
if (fd.IsValid() && fd.IsDir()) active->SetPath(path);
 else if (fd.IsValid() || active->arcman->IsArchive()) active->partner->SetPath(path);

active->DisplaySelectionInTBTextCtrl(path);                // Put it in the textctrl too
active->GetTreeCtrl()->SetFocus();                         // Clicking the menubar took the focus from the pane.  This returns it
}

void MyFrame::OnPreview(wxCommandEvent& event)    // Turn on/off fileview previews
{
SHOWPREVIEW = event.IsChecked();
if (!SHOWPREVIEW)
  PreviewManager::Clear();
}

void MyFrame::OnPreviewUI(wxUpdateUIEvent& event)
{
event.Check(SHOWPREVIEW);
}

void MyFrame::TextFilepathEnter()    // The Filepath textctrl has the focus and Enter has been added, so GoTo its contents
{
if (m_tbText == NULL) return;
wxString filepath, path = m_tbText->GetValue();            // Get the path requested
if (path.IsEmpty())   return;
filepath = path;                                           // Store it, as it gets altered in OnOutsideSelect()

MyGenericDirCtrl* active = GetActivePane();
if (active == NULL)    return;
if (active->fileview == ISRIGHT)  active = active->partner;

active->OnOutsideSelect(filepath);                         // If everything's valid, select it into the currently-active dirview

FileData fd(path);                                         // We're going to need this in a minute, so use it to get the path cf filepath
if (active->fulltree)                                      //  If in fulltree mode, make fileview-pane match too.  If not, OnOutsideSelect() does it anyway
  { active->partner->startdir = fd.GetPath();  
    active->partner->ReCreateTreeFromSelection();
  }

if (fd.IsValid())
  { if (fd.IsDir()) active->SetPath(path);                 // Is it a dir or a file?  If it's a file, it's nice to set the filepath to it in the fileview
      else active->partner->SetPath(path);
  }
}

void MyFrame::SplitHorizontal(wxCommandEvent& WXUNUSED(event))
{
MyTab* tab = GetActiveTab();
if (tab == NULL)   return;

tab->PerformSplit(horizontal);
}

void MyFrame::SplitVertical(wxCommandEvent& WXUNUSED(event))
{
MyTab* tab = GetActiveTab();
if (tab == NULL)   return;

tab->PerformSplit(vertical);
}

void MyFrame::Unsplit(wxCommandEvent& WXUNUSED(event))
{
MyTab* tab = GetActiveTab();
if (tab == NULL)   return;

tab->PerformSplit(unsplit);
}

void MyFrame::OnTabTemplateLoadMenu(wxCommandEvent& event)
{
Layout->m_notebook->LoadTemplate(event.GetId() - SHCUT_TABTEMPLATERANGE_START);
}

void MyFrame::OnConfigureShortcuts(wxCommandEvent& WXUNUSED(event))
{
AccelList->ConfigureShortcuts();
}

void MyFrame::OnShowBriefMessageBox(wxCommandEvent& event)
{
  // In wx2.9 showing a messagebox in MyProcess::OnTerminate hangs. So it posts an event to here
int time = AUTOCLOSE_FAILTIMEOUT / 1000;
wxString caption(_("Error"));

if (event.GetId() == 1) // Use 1 for success, anything else for failure
  { time = AUTOCLOSE_TIMEOUT / 1000;
    wxString caption(_("Success"));
  }

BriefMessageBox bmb(event.GetString(), time, caption);
}


void MyFrame::SetFocusViaKeyboard(wxCommandEvent& event)
{
MyGenericDirCtrl* gdc;
switch(event.GetId())
  { case SHCUT_SWITCH_FOCUS_TERMINALEM:
            if (Layout->EmulatorShowing)
              { wxWindow* tm = Layout->GetTerminalEm();
                if (tm) tm->SetFocus();
              }
            return;
    case SHCUT_SWITCH_FOCUS_COMMANDLINE:
            if (Layout->CommandlineShowing) Layout->commandline->SetFocus(); return;
    case SHCUT_SWITCH_FOCUS_TOOLBARTEXT:
            if (m_tbText->IsShown()) m_tbText->SetFocus(); return;
    case SHCUT_SWITCH_FOCUS_PANES:        // Switch to the panes from e.g. the terminalem
            gdc = GetActivePane(); if (!gdc) return;
            gdc->GetTreeCtrl()->SetFocus();
            if (gdc->fileview == ISLEFT) return;
            break;  // Otherwise break to the fileview code
    case SHCUT_SWITCH_TO_PREVIOUS_WINDOW:
            {wxWindow* win = wxGetApp().GetFocusController().GetPreviousFocus();
            if (!win || win == wxWindow::FindFocus()) return;

            gdc = wxDynamicCast(win, MyGenericDirCtrl);
            if (!gdc) { win->SetFocus(); return; }  // If it's not a pane, just set focus
            gdc->GetTreeCtrl()->SetFocus();
            if (gdc->fileview == ISLEFT) return;
            break;  // Otherwise break to the fileview code
            }
    default: return;
  }

if (!gdc) return;
                    // If we're here, the focus destination is a fileview
if (gdc->GetPath().empty())               // If there was no highlit item, we need to set one. SetFocus() alone doesn't do this
  { FileGenericDirCtrl* fgdc = wxStaticCast(gdc, FileGenericDirCtrl);
    if (fgdc) fgdc->SelectFirstItem();
  }
    
gdc->GetTreeCtrl()->SetFocus();
}

void MyFrame::OnPasteThreadProgress(wxCommandEvent& event)
{
ThreadsManager::Get().OnThreadProgress(event.GetId(), event.GetInt());
}

void MyFrame::OnPasteThreadFinished(PasteThreadEvent& event)
{
if (event.GetId() != -1) // -1 flags that this is a fake threadevent; we're only here to force a refresh
  ThreadsManager::Get().OnThreadCompleted(event.GetId(), event.GetArrayString(), event.GetSuccesses());

size_t count = event.GetRefreshesArrayString().GetCount(); // Now refresh the display for any overwrites; FSWatcher fails as the create events may precede the delete ones!
if (USE_FSWATCHER && count && GetActivePane()) // No need if !USE_FSWATCHER, or no overwrites, or no panes
  { wxSafeYield(); // Give any FSWatcher events a chance to be processed (this is why we're doing it here, not inside the thread)
    wxArrayInt IDs; IDs.Add(GetActivePane()->GetId()); // We need at least 1 ID to pass, otherwise OnUpdateTrees() aborts
    for (size_t n=0; n < count; ++n)
      OnUpdateTrees(event.GetRefreshesArrayString().Item(n), IDs, wxT(""), true);
  }
}

//--------------------------------------------------------------------------------------------------------------------------------------
LayoutWindows::LayoutWindows(MyFrame *parent) : m_splitterMain(NULL), m_notebook(NULL), bottompanel(NULL), commandline(NULL), tool(NULL), m_frame(parent)
{
CommandlineShowing = EmulatorShowing = false;
}

LayoutWindows::~LayoutWindows()
{
wxConfigBase::Get()->Write(wxT("/Misc/PropertiesPage"), m_frame->PropertiesPage);  // Save the last-used page of the Properties notebook

m_notebook->saveonexit = m_frame->GetMenuBar()->IsChecked(SHCUT_SAVETABS_ONEXIT);
if (m_notebook->saveonexit) m_notebook->SaveDefaults();        // If we're supposed to save tabdata etc, do so

if (tool) delete tool; 
}

void LayoutWindows::Setup()
{
  // Start with the first division: a textctrl with a standard rest-of-screen wxSplitterWindow below, using a sizer not a splitter to position
commandlinepane = wxXmlResource::Get()->LoadPanel(m_frame, wxT("CommandPane"));  // Load the pane for displaying the command-line
m_frame->sizerMain->Add(commandlinepane, 0, wxEXPAND);
m_frame->sizerMain->Show(commandlinepane, false);
m_frame->sizerMain->Layout();

                                      // Here comes the rest-of-screen splitter-window
m_splitterMain = new wxSplitterWindow(m_frame, -1, wxDefaultPosition, wxDefaultSize,  0);
m_frame->sizerMain->Add(m_splitterMain, 1, wxEXPAND, 0);
m_frame->SetSizer(m_frame->sizerMain);

    // Right, we want something to put in the main splitterwindow: a Notebook to hold the secondary splitter, and something below to hold terminal sessions etc
m_notebook = new MyNotebook(m_splitterMain, NextID++);
m_splitterMain->Initialize(m_notebook);                      // Initialise the notebook part of m_splitterMain

    // Load the preferred series of tabs, or else a default one
m_notebook->LoadTabs(-1, ((MyApp&)wxGetApp()).startdir0, ((MyApp&)wxGetApp()).startdir1);  // The 2nd/3rd parameters are in case the user typed 4Pane /path/to/foo, /path/to/bar

                            // While we're here, load which page of the Properties notebook to start with
wxConfigBase::Get()->Read(wxT("/Misc/PropertiesPage"), &m_frame->PropertiesPage, 0);

  // Now show the commandline (which calls the terminal emulator too) or the terminal emulator, if requested
if (m_notebook->showcommandline)    OnCommandLine();
  else if (m_notebook->showterminal)  wxGetApp().CallAfter([this] { LayoutWindows::DoTool(terminalem, true); } ); // Do this async: the program won't yet be a sensible size so saved terminal-ht >> ht
}

void LayoutWindows::OnChangeDir(const wxString& newdir, bool ReusePrompt)  // Called when a terminalem does a cd, to tell commandline or vice versa
{                                // Also called when a dirview changes selection, so that this can be reflected in any terminal/commandline prompt
if (newdir.empty()) return;

if (EmulatorShowing) 
  { GetTerminalEm()->OnChangeDir(newdir, ReusePrompt);
    if (CommandlineShowing) commandline->WritePrompt(); // If the emulator's there, see if the commandline is too 
  }

if (SHOW_DIR_IN_TITLEBAR)
  { wxString prefix = m_frame->GetTitle().BeforeFirst(wxT('[')).Trim(); // Get the text proximal to any existing dirname
    MyGenericDirCtrl* gdc = m_frame->GetActivePane();
    if (!gdc) return;

    wxString dir = gdc->GetActiveDirPath(); // This gives the selection in the dirview, which is probably the most sensible thing to display. It also copes with fulltree
    m_frame->SetTitle(prefix + wxT(" [") + dir.AfterLast(wxFILE_SEP_PATH) + wxT("/]"));
  }
}

void LayoutWindows::OnCommandLine()
{
if (!CommandlineShowing)                                // If it's not showing, load & show the command-line
  { DoTool(cmdline);
    ShowHideCommandLine();                              // If it's showing, hide it; & vice versa
  }
 else    // NB tool->Close() will itself call ShowHideCommandLine()
   tool->Close();                                       // If it IS showing, shut it down, closing the bottom panel in the process
}

void LayoutWindows::ShowHideCommandLine() // Toggle whether it's showing
{
CommandlineShowing = !CommandlineShowing;
m_frame->sizerMain->Show(commandlinepane, CommandlineShowing);
m_frame->sizerMain->Layout();

if (!CommandlineShowing)
  wxGetApp().GetFocusController().Invalidate(commandline);

if (wxWindow::FindFocus() == commandline)
  { wxWindow* win = m_frame->GetActivePane();
    if (win) win->SetFocus();
  }
}

bool LayoutWindows::ShowBottom(bool FromSetup/* = false*/)    // If bottompanel isn't visible, show it
{
if (m_splitterMain->IsSplit()) return false;            // Can't split it into four

bottompanel = wxXmlResource::Get()->LoadPanel(m_splitterMain, wxT("TerminalPane"));

static int no = 1;
int w, h; m_notebook->GetClientSize(&w, &h);
int PrevHt = (int)wxConfigBase::Get()->Read("Tabs/terminalheight", (long)0);
if ((PrevHt + m_splitterMain->GetSashSize()) > h)
  { if (no > 30)
      { no = 1; PrevHt = 0; } // We're clearly not going to win here, so forget the old figure
     else
      { ++no;
        wxGetApp().CallAfter([this] { LayoutWindows::DoTool(terminalem, true); } );
        return false;
      }
  }
if (PrevHt)  h -= (PrevHt + m_splitterMain->GetSashSize());
 else h = h *2/3;
m_splitterMain->SplitHorizontally(m_notebook, bottompanel, h);

no = 1;
return true;
}

void LayoutWindows::UnShowBottom()    // If bottompanel is visible, unsplit it 
{
if (!m_splitterMain->IsSplit()) return;               // Can't unsplit it if it's not split

wxConfigBase::Get()->Write("Tabs/terminalheight", bottompanel->GetSize().GetHeight());
m_splitterMain->Unsplit(bottompanel);
}

void LayoutWindows::DoTool(enum toolchoice whichtool, bool FromSetup/* = false*/) // Invoke the requested tool in the bottom panel
{
if (ShowBottom(FromSetup))                                // If there's already a split, there's already a Tool instance
  { tool = new Tools(this);                               // If not, make one
    tool->Init(whichtool);                                // whichtool informs Tools what it's supposed to be running eg grep, locate
  }
 else if (tool)
   { if (whichtool != terminalem)  tool->Init(whichtool); // It's a change of tool, eg from grep to locate, or a do-it-again
      else tool->Close();                                 // Otherwise it's a second Ctrl-M, so toggle closed
  }
}

void LayoutWindows::CloseBottom()
{
delete tool; tool = NULL;
UnShowBottom();
}

//--------------------------------------------------------------------------------------------------------------------------------------

BEGIN_EVENT_TABLE(TabTemplateOverwriteDlg,wxDialog)  // This is for a dialog used in MyNotebook::SaveAsTemplate()
EVT_BUTTON(XRCID("Overwrite"),TabTemplateOverwriteDlg::OnOverwriteButton)
EVT_BUTTON(XRCID("SaveAs"),TabTemplateOverwriteDlg::OnSaveAsButton)
END_EVENT_TABLE()
//-----------------------------------------------------------------------------------------------------------------------

MyTab::MyTab(MyNotebook* dad, wxWindowID id /*=-1*/, int tabdatano /*=0*/, int TemplateNo /*=-1*/, const wxString& tabname/*=""*/, const wxString& startdir0 /*=""*/, const wxString& startdir1 /*=""*/)
            :  wxPanel(dad, id)
{
tabdata = new class Tabdata(this);
        // Load it with data for the requested page.  If this doesn't exist, sensible defaults will be substituted
tabdatanumber = tabdata->Load(dad, tabdatano, TemplateNo); // tabdatanumber is set to the ACTUAL tabdata loaded ie 0 if the requested one didn't exist

Create(startdir0, startdir1);                         // Do the actual construction
}
                          // This is the Copy Ctor version; used by DuplicateTab()
MyTab::MyTab(Tabdata* tabdatatocopy, MyNotebook* dad, wxWindowID id /*=-1*/, int TemplateNo /*=-1*/, const wxString& tabname /*=""*/)
            :  wxPanel(dad, id, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL, wxT(""))
{
tabdata = new class Tabdata(*tabdatatocopy);

Create();
}

void MyTab::Create(const wxString& startdir0 /*=""*/, const wxString& startdir1 /*=""*/)
{
wxBoxSizer* panelsizer = new wxBoxSizer(wxVERTICAL);

m_splitterQuad = new wxSplitterWindow(this, -1, wxDefaultPosition, wxDefaultSize,  0);  // Splitter that holds quad panes splitters
m_splitterQuad->SetMinimumPaneSize(20);

panelsizer->Add(m_splitterQuad, 1, wxEXPAND);
SetSizer(panelsizer);
                                    // Now create the  other splitters
m_splitterLeftTop = new DirSplitterWindow(m_splitterQuad, false);    // One for the left or top (flagged by false)
m_splitterRightBottom = new DirSplitterWindow(m_splitterQuad, true); // & one for the right or bottom
m_splitterLeftTop->SetMinimumPaneSize(20);
m_splitterRightBottom->SetMinimumPaneSize(20);

                                    // and do the splitting
#if !defined(__WXX11__)  // For some reason, this isn't necessary/makes things worse in wxX11
  // Since 2.6.2 the sash positions are set when the window is still of zero size, so when it finally expands the Rt pane gets most of the area
  PerformSplit(initialsplit, true);            // So do an initial split, then redo the user-defined dimensions from an idle event later
  Connect(wxEVT_IDLE, (wxObjectEventFunction)&MyTab::OnIdle);
  #if ! (defined(__WXGTK__) || defined(__WXX11__))
    NeedsLayout = false;
  #endif
#else
PerformSplit(tabdata->splittype, true);
#endif

wxString startdir_0 = startdir0, startdir_1 = startdir1;
if (startdir_0.IsEmpty()) startdir_0 = tabdata->startdirleft;    // If the user passed a "Start here" parameter or 2, override the tab's previous startdir
if (startdir_1.IsEmpty()) startdir_1 = tabdata->startdirright;
                                    // Do the actual directory creation. True means starts with focus
                                    // NB do RightBottom first as LeftTop calls MyTab::SwitchOffHighlights which accesses RightBottom trees!
m_splitterRightBottom->Setup(this, false, startdir_1, tabdata->fulltreeright,
                                       tabdata->BRFileonly, tabdata->showhiddenBRdir, tabdata->showhiddenBRfile, tabdata->pathright);
m_splitterLeftTop->Setup(this, true, startdir_0, tabdata->fulltreeleft,
                                       tabdata->TLFileonly, tabdata->showhiddenTLdir, tabdata->showhiddenTLfile, tabdata->pathleft);
SetActivePaneFromConfig();

m_splitterQuad->SetSashGravity(0.5);
m_splitterLeftTop->SetSashGravity(0.3);
m_splitterRightBottom->SetSashGravity(0.3);
}

void MyTab::SetActivePaneFromConfig() // Should only be called immediately following a tab load; it's orthogonal to setting focus by clicking
{
SetActivePane( tabdata->LeftTopIsActive ? LeftorTopDirCtrl : RightorBottomDirCtrl );
}

void MyTab::SetActivePane(MyGenericDirCtrl* active)
{
LastKnownActivePane = active;
wxGetApp().GetFocusController().SetCurrentFocus(active);

if (active->fileview == ISLEFT) LeftRightDirview = active;
 else LeftRightDirview = active->partner;
}

void MyTab::PerformSplit(split_type splittype, bool newtab /*=false*/)    // Splits a tab into left/right or top/bottom or unsplit, as desired
{
int mode = m_splitterQuad->IsSplit();                  // Are we currently split
if (mode) { mode = m_splitterQuad->GetSplitMode(); }   // If so which way?  (We have to do it like this as an unsplit SplitterWin reports it's already vertical)

switch(splittype)
  { case vertical:  if (mode == wxSPLIT_VERTICAL) return;        // Nothing to do

                    if (mode == wxSPLIT_HORIZONTAL)              // Changing from horiz to vertical
                      { tabdata->horizontalsplitpos = m_splitterQuad->GetSashPosition();  // Save current sash positions, in case they'd been altered
                        tabdata->tophorizontalsashpos = m_splitterLeftTop->GetSashPosition();
                        tabdata->bottomhorizontalsashpos = m_splitterRightBottom->GetSashPosition();  
                        m_splitterQuad->Unsplit(m_splitterRightBottom); // Unsplit first.
                        m_splitterQuad->SplitVertically(m_splitterLeftTop, m_splitterRightBottom, tabdata->verticalsplitpos);  
                        m_splitterRightBottom->Show(true);              // Unsplitting hid this pane, so need to Show it again
                                          // Set the desired sash positions, first checking there are valid nos
                        if (tabdata->leftverticalsashpos == -1)         // If there is no valid sash position, set it to 1/3rd of the window
                          { wxSize size = m_splitterLeftTop->GetClientSize();
                            tabdata->leftverticalsashpos = size.GetWidth() / 3;
                          }
                        if (tabdata->rightverticalsashpos == -1)
                          { wxSize size = m_splitterRightBottom->GetClientSize();
                            tabdata->rightverticalsashpos = size.GetWidth() / 3;
                          }

                        m_splitterLeftTop->SetSashPosition(tabdata->leftverticalsashpos);
                        m_splitterRightBottom->SetSashPosition(tabdata->rightverticalsashpos);
                        splitstatus = vertical; return;
                      }
                                          // Otherwise we must be changing from unsplit to vertical, either by choice or because the tab is being created
                    if (tabdata->verticalsplitpos == 0 && newtab)          // First get a sensible default position if needed ie if new tab and no positive value
                      { wxSize size = MyFrame::mainframe->GetClientSize(); // (if not a newtab, a zero default halves the screen, which is OK)
                        tabdata->verticalsplitpos = size.GetWidth();       // I'd've thought this should be halved, but this works best
                      }

                    m_splitterQuad->SplitVertically(m_splitterLeftTop, m_splitterRightBottom, tabdata->verticalsplitpos);  // Split the page into 2, vertically
                    m_splitterLeftTop->Show(true); m_splitterRightBottom->Show(true); splitstatus = vertical;
                    m_splitterLeftTop->SetSashPosition(tabdata->leftverticalsashpos);  // Make sure the sash positions haven't changed
                    m_splitterRightBottom->SetSashPosition(tabdata->rightverticalsashpos);
                    if (!newtab) return;                                  // If this ISN'T a new tab, we're done  
                                                                          // Otherwise do the dirview/fileview splits
                    if (tabdata->leftverticalsashpos == -1)               // Again check for valid defaults
                      { wxSize size = MyFrame::mainframe->GetClientSize(); // This time, use main window size, as the local clientsize isn't yet established
                        tabdata->leftverticalsashpos = size.GetWidth() / 3;
                      }
                    if (tabdata->rightverticalsashpos == -1)
                      { wxSize size = MyFrame::mainframe->GetClientSize();
                        tabdata->rightverticalsashpos = size.GetWidth() / 3;
                      }
                    m_splitterLeftTop->SplitVertically(m_splitterLeftTop->DirPanel, m_splitterLeftTop->FilePanel, tabdata->leftverticalsashpos);
                    m_splitterRightBottom->SplitVertically(m_splitterRightBottom->DirPanel, m_splitterRightBottom->FilePanel, tabdata->rightverticalsashpos);
                                                
                    splitstatus = vertical; return;

                    
    case horizontal:  if (mode == wxSPLIT_HORIZONTAL)  return;
    
                    if (mode == wxSPLIT_VERTICAL)                  // Changing from vertical to horiz.
                      { tabdata->verticalsplitpos = m_splitterQuad->GetSashPosition();  // Save current sash positions, in case they'd been altered
                        tabdata->leftverticalsashpos = m_splitterLeftTop->GetSashPosition();
                        tabdata->rightverticalsashpos = m_splitterRightBottom->GetSashPosition();

                        m_splitterQuad->Unsplit(m_splitterRightBottom);  // Unsplit first
                        m_splitterQuad->SplitHorizontally(m_splitterLeftTop, m_splitterRightBottom, tabdata->horizontalsplitpos);
                        m_splitterRightBottom->Show(true);               // Unsplitting hid this pane, so need to Show it again
                                  // Set the desired sash positions, first checking there are valid nos
                        if (tabdata->tophorizontalsashpos == -1)         // If there is no valid sash position, set it to 1/3rd of the window
                          { wxSize size = m_splitterLeftTop->GetClientSize();
                            tabdata->tophorizontalsashpos = size.GetWidth() / 3;
                          }
                        if (tabdata->bottomhorizontalsashpos == -1)
                          { wxSize size = m_splitterRightBottom->GetClientSize();
                            tabdata->bottomhorizontalsashpos = size.GetWidth() / 3;
                          }

                        m_splitterLeftTop->SetSashPosition(tabdata->tophorizontalsashpos);
                        m_splitterRightBottom->SetSashPosition(tabdata->bottomhorizontalsashpos);
                        splitstatus = horizontal; return;
                      }
                        // Otherwise we must be changing from unsplit to horiz, either by choice or because the tab is being created
                    if (tabdata->horizontalsplitpos == 0  && newtab)      // First get a sensible default position if needed.  See 'vertical' comments
                      { wxSize size = MyFrame::mainframe->GetClientSize();
                        tabdata->horizontalsplitpos = size.GetHeight();
                      }

                    m_splitterQuad->SplitHorizontally(m_splitterLeftTop, m_splitterRightBottom, tabdata->horizontalsplitpos);  // Split the page into 2, horiz.ly
                    m_splitterRightBottom->Show(true); m_splitterLeftTop->Show(true); splitstatus = horizontal; 
                    m_splitterLeftTop->SetSashPosition(tabdata->tophorizontalsashpos);
                    m_splitterRightBottom->SetSashPosition(tabdata->bottomhorizontalsashpos);
                    if (!newtab) return;                        // If this ISN'T a new tab, we're done  
                                                                // Otherwise do the dirview/fileview splits                                  
                    if (tabdata->tophorizontalsashpos == -1)
                      { wxSize size = MyFrame::mainframe->GetClientSize();
                        tabdata->tophorizontalsashpos = size.GetWidth() / 3;
                      }
                    if (tabdata->bottomhorizontalsashpos == -1)
                      { wxSize size = MyFrame::mainframe->GetClientSize();
                        tabdata->bottomhorizontalsashpos = size.GetWidth() / 3;
                      }
                    m_splitterLeftTop->SplitVertically(m_splitterLeftTop->DirPanel, m_splitterLeftTop->FilePanel, tabdata->tophorizontalsashpos);                                                                                                        
                    m_splitterRightBottom->SplitVertically(m_splitterRightBottom->DirPanel,
                                                                  m_splitterRightBottom->FilePanel, tabdata->bottomhorizontalsashpos);
                    splitstatus = horizontal; return;

    case unsplit:   if (mode == wxSPLIT_VERTICAL                  // Changing from vertical to Unsplit
                                || mode == wxSPLIT_HORIZONTAL     //   or horizontal to Unsplit
                                || splitstatus == initialsplit)   //   or previously just initialised
                      {  if (mode == wxSPLIT_VERTICAL)            // If vertical, save the vertical sash positions in case they'd changed
                            { tabdata->verticalsplitpos = m_splitterQuad->GetSashPosition();
                              tabdata->leftverticalsashpos = m_splitterLeftTop->GetSashPosition();
                              tabdata->rightverticalsashpos = m_splitterRightBottom->GetSashPosition();
                            }
                          else { if (mode == wxSPLIT_HORIZONTAL)  // Similarly if horizontal
                                  { tabdata->horizontalsplitpos = m_splitterQuad->GetSashPosition();
                                    tabdata->tophorizontalsashpos = m_splitterLeftTop->GetSashPosition();
                                    tabdata->bottomhorizontalsashpos = m_splitterRightBottom->GetSashPosition();                          
                                  }
                               }
                        if (LeftTopIsActive())  
                          { m_splitterQuad->Unsplit(m_splitterRightBottom);  // Unsplit the unwanted half
                            m_splitterLeftTop->SetSashPosition(tabdata->unsplitsashpos);  // Set the sash to unsplit position
                          }
                         else      
                          { m_splitterQuad->Unsplit(m_splitterLeftTop);
                            m_splitterRightBottom->SetSashPosition(tabdata->unsplitsashpos);
                          }
                        splitstatus = unsplit; return;
                      }

                    if (!newtab) return;                          // If this ISN'T a new tab, nothing to do as it was already unsplit
                    splitstatus = unsplit;                        // Otherwise set the about-to-be status and fall thru into initialsplit
                                                    
    case initialsplit:  if (tabdata->unsplitsashpos == -1)                // Otherwise do the dirview/fileview splits
                          { wxSize size = MyFrame::mainframe->GetClientSize();
                            tabdata->unsplitsashpos = size.GetWidth() / 2;
                          }
                    DirSplitterWindow *active, *hidden;
                    bool WhichIsActive(true);
                    if (splittype == initialsplit) // i.e. we're not falling through from unsplit
                      WhichIsActive = tabdata->LeftTopIsActive;
                     else
                      WhichIsActive = LeftTopIsActive();
                    
                    if (WhichIsActive)
                      { active = m_splitterLeftTop; hidden = m_splitterRightBottom; }
                     else 
                      { active = m_splitterRightBottom; hidden = m_splitterLeftTop; }
                    m_splitterQuad->Initialize(active);
                    active->Show(true);
                    hidden->Hide();  // Hide the off-side pane, as otherwise it shows as a tiny topleft-corner square if loaded unsplit
                    
                    active->SplitVertically(active->DirPanel, active->FilePanel, tabdata->unsplitsashpos);
                                    // We have to split (probably) m_splitterRightBottom here, even though it's invisible: it might be wanted later
                    hidden->SplitVertically(hidden->DirPanel, hidden->FilePanel, tabdata->unsplitsashpos);
                    
                    if (splittype == initialsplit) splitstatus = initialsplit;  // If we weren't falling thru from unsplit, set status.  It'll be needed when the idle event fires
  }
}

void MyTab::OnIdle(wxIdleEvent& WXUNUSED(event))
{  // See the comment in MyTab::Create about delayed application of user-defined sizes
#if ! (defined(__WXGTK__) || defined(__WXX11__))
  if (NeedsLayout)             // Recent versions don't do the initial layout properly in gtk1.2: only a few files are visible
    { GetSizer()->Layout(); Disconnect(wxID_ANY, wxEVT_IDLE); return; } // If this is the second idle event, Layout, then Disconnect
#endif

if (m_splitterQuad->GetClientSize().GetWidth() > 20 && splitstatus == initialsplit)  // Don't do it until the window system is up and running; but only once
  { PerformSplit(tabdata->splittype);                                   // OK, we're safe to do it properly
#if defined(__WXGTK__) || defined(__WXX11__)
    Disconnect(wxID_ANY, wxEVT_IDLE);                                   // If this *isn't* a recent version/gtk1, Disconnect now: we don't want to keep being called
#else
    NeedsLayout = true;                                                 // Otherwise flag that we need to do Layout in the next idle event
#endif
  }
}

void MyTab::SwitchOffHighlights()
{
  // The new highlighting-of-the-currently-selected-pane has a problem with new tabs
  // Until the user clicks on a pane, kill-focus doesn't happen (as nothing properly *has* focus)
  // so sometimes 2 different panes end up highlit. Therefore unhighlight them all before highlighting the correct one
wxFocusEvent kfe(wxEVT_KILL_FOCUS);
LeftorTopDirCtrl->GetTreeCtrl()->GetEventHandler()->ProcessEvent(kfe);
RightorBottomDirCtrl->GetTreeCtrl()->GetEventHandler()->ProcessEvent(kfe);
LeftorTopFileCtrl->GetTreeCtrl()->GetEventHandler()->ProcessEvent(kfe);
RightorBottomFileCtrl->GetTreeCtrl()->GetEventHandler()->ProcessEvent(kfe);
}

void MyTab::StoreData(int tabno)    // Refreshes the tabdata ready for saving
{
if (tabdata == NULL)  return;

tabdata->startdirleft = LeftorTopDirCtrl->startdir;                    // First store the various data in tabdata
tabdata->startdirright = RightorBottomDirCtrl->startdir;
tabdata->pathleft = LeftorTopDirCtrl->GetPath();
tabdata->pathright = RightorBottomDirCtrl->GetPath();
tabdata->fulltreeleft = LeftorTopDirCtrl->fulltree;
tabdata->fulltreeright = RightorBottomDirCtrl->fulltree;
//tabdata->tabname = MyFrame::mainframe->Layout->m_notebook->GetPageText(tabno);  // No, don't do this, otherwise we store Page1, Page2 etc. which should be auto-generated on loading
tabdata->showhiddenTLdir = LeftorTopDirCtrl->GetShowHidden();
tabdata->showhiddenTLfile = LeftorTopFileCtrl->GetShowHidden();
tabdata->showhiddenBRdir = RightorBottomDirCtrl->GetShowHidden();
tabdata->showhiddenBRfile = RightorBottomFileCtrl->GetShowHidden();

tabdata->TLFileonly = LeftorTopFileCtrl->DisplayFilesOnly;
tabdata->BRFileonly = RightorBottomFileCtrl->DisplayFilesOnly;

size_t Llastvisible = ((FileGenericDirCtrl*)LeftorTopFileCtrl)->GetHeaderWindow()->GetLastVisibleCol();
size_t Rlastvisible = ((FileGenericDirCtrl*)RightorBottomFileCtrl)->GetHeaderWindow()->GetLastVisibleCol();

for (size_t n=0; n < TOTAL_NO_OF_FILEVIEWCOLUMNS; ++n)
  { tabdata->LColShown[n] = !((FileGenericDirCtrl*)LeftorTopFileCtrl)->GetHeaderWindow()->IsHidden(n);
    if (tabdata->LColShown[n] && (n != Llastvisible))
          tabdata->Lwidthcol[n] = ((FileGenericDirCtrl*)LeftorTopFileCtrl)->GetHeaderWindow()->GetColumnWidth(n);
     else tabdata->Lwidthcol[n] = ((FileGenericDirCtrl*)LeftorTopFileCtrl)->GetHeaderWindow()->GetColumnDesiredWidth(n); // If not currently shown, store the size it would have been

    tabdata->RColShown[n] = !((FileGenericDirCtrl*)RightorBottomFileCtrl)->GetHeaderWindow()->IsHidden(n);
    if (tabdata->RColShown[n] && (n != Rlastvisible))
          tabdata->Rwidthcol[n] = ((FileGenericDirCtrl*)RightorBottomFileCtrl)->GetHeaderWindow()->GetColumnWidth(n);
     else tabdata->Rwidthcol[n] = ((FileGenericDirCtrl*)RightorBottomFileCtrl)->GetHeaderWindow()->GetColumnDesiredWidth(n);
  }

tabdata->Lselectedcol = ((FileGenericDirCtrl*)LeftorTopFileCtrl)->GetHeaderWindow()->GetSelectedColumn();
tabdata->Rselectedcol = ((FileGenericDirCtrl*)RightorBottomFileCtrl)->GetHeaderWindow()->GetSelectedColumn();
tabdata->Lreversesort = ((FileGenericDirCtrl*)LeftorTopFileCtrl)->GetHeaderWindow()->GetSortOrder();
tabdata->Rreversesort = ((FileGenericDirCtrl*)RightorBottomFileCtrl)->GetHeaderWindow()->GetSortOrder();
tabdata->Ldecimalsort = ((FileGenericDirCtrl*)LeftorTopFileCtrl)->GetIsDecimalSort();
tabdata->Rdecimalsort = ((FileGenericDirCtrl*)RightorBottomFileCtrl)->GetIsDecimalSort();
  
int mode = m_splitterQuad->IsSplit();                 // Are we currently split
if (mode) { mode = m_splitterQuad->GetSplitMode(); }  // If so which way?  (We have to do it like this as an unsplit SplitterWin reports it's already vertical)
switch(mode)
  { case wxSPLIT_VERTICAL:    tabdata->splittype = vertical;
                              tabdata->verticalsplitpos = m_splitterQuad->GetSashPosition();
                              tabdata->leftverticalsashpos = m_splitterLeftTop->GetSashPosition();
                              tabdata->rightverticalsashpos = m_splitterRightBottom->GetSashPosition();
                              break;
    case wxSPLIT_HORIZONTAL:  tabdata->splittype = horizontal; 
                              tabdata->horizontalsplitpos = m_splitterQuad->GetSashPosition();
                              tabdata->tophorizontalsashpos = m_splitterLeftTop->GetSashPosition();
                              tabdata->bottomhorizontalsashpos = m_splitterRightBottom->GetSashPosition();                          
                              break;
     default:                 tabdata->splittype = unsplit;
                              tabdata->unsplitsashpos = ((wxSplitterWindow*)m_splitterQuad->GetWindow1())->GetSashPosition();
  }
}

//-----------------------------------------------------------------------------------------------------------------------
Tabdata::Tabdata(const Tabdata& oldtabdata)
{
m_parent = oldtabdata.m_parent;
startdirleft = oldtabdata.startdirleft;                  // Left(Top) pane starting dir and path
pathleft = oldtabdata.pathleft;
startdirright = oldtabdata.startdirright;                // Ditto Right(Bottom)
pathright = oldtabdata.pathright;

fulltreeleft = oldtabdata.fulltreeleft;                  //  & their fulltree status
fulltreeright = oldtabdata.fulltreeright;

TLFileonly = oldtabdata.TLFileonly;                      // & file-only status
BRFileonly = oldtabdata.BRFileonly;

showhiddenTLdir = oldtabdata.showhiddenTLdir;            // & showhidden status
showhiddenTLfile = oldtabdata.showhiddenTLfile;
showhiddenBRdir = oldtabdata.showhiddenBRdir;
showhiddenBRfile = oldtabdata.showhiddenBRfile;

splittype = oldtabdata.splittype;                       // How is the pane split
verticalsplitpos = oldtabdata.verticalsplitpos;         // If it's vertical, this is the main sash pos
horizontalsplitpos = oldtabdata.horizontalsplitpos;     // Horizontal ditto

leftverticalsashpos = oldtabdata.leftverticalsashpos;   // If it's vertical, this is where to position the dirview/fileview sash
tophorizontalsashpos = oldtabdata.tophorizontalsashpos; // etc
rightverticalsashpos = oldtabdata.rightverticalsashpos;
bottomhorizontalsashpos = oldtabdata.bottomhorizontalsashpos;
unsplitsashpos = oldtabdata.unsplitsashpos;
LeftTopIsActive = oldtabdata.LeftTopIsActive;

for (size_t n=0; n < TOTAL_NO_OF_FILEVIEWCOLUMNS; ++n)  // Widths of the 2 fileviews' columns
  { Lwidthcol[n] = oldtabdata.Lwidthcol[n]; Rwidthcol[n] = oldtabdata.Rwidthcol[n];
    LColShown[n] = oldtabdata.LColShown[n]; RColShown[n] = oldtabdata.RColShown[n];
  }

Lselectedcol = oldtabdata.Lselectedcol;
Rselectedcol = oldtabdata.Rselectedcol;
Lreversesort = oldtabdata.Lreversesort;
Rreversesort = oldtabdata.Rreversesort;
Ldecimalsort = oldtabdata.Ldecimalsort;
Rdecimalsort = oldtabdata.Rdecimalsort;

tabname = oldtabdata.tabname;                           // Label on the tab
}

int Tabdata::Load(MyNotebook* NB, int tabno /*= 0*/, int TemplateNo /*=-1*/)  // Load the current defaults for this tab. tabno allows different tabs in the notebook to have different defaults
{
int tabnumber = tabno; bool usingdefault = false;

wxConfigBase *config = wxConfigBase::Get();
if (config==NULL)  { wxLogError(_("Couldn't load configuration!?")); return false; }

wxString Path;
if (TemplateNo >= 0) Path.Printf(wxT("/Tabs/Template_%c/"), wxT('a') + TemplateNo);  // If we're loading a template, adjust path approprately
 else Path = wxT("/Tabs/");

wxString Subpath; Subpath.Printf(wxT("tab_%c/"), wxT('a') + tabno);    // Make the subpath read tab_a/, tab_b/ or whatever
if (!config->Exists(wxString(Path+Subpath).BeforeLast('/')))  
  { Subpath = wxT("tab_a/"); tabnumber = 0; usingdefault = true; }     // If the requested tabno has no defaults, default to 0.  NB this won't work for templates with no tabs, but surely . . .

Path += Subpath;

wxString key, pageno; 
pageno = NB->CreateUniqueTabname(); // Make a default tab title
if (usingdefault) tabname = pageno;                                   // If we're using (probably) page 0 because there isn't a page 21, we mustn't load the page's title.
  else                                                                // If we did, we'd end up with a whole series tabs called "Page 1"
  { key = wxT("tabname"); config->Read(Path+key, &tabname, pageno); }

key = wxT("startdirleft"); config->Read(Path+key, &startdirleft, wxT(""));
key = wxT("pathleft"); config->Read(Path+key, &pathleft, wxT(""));
key = wxT("fulltreeleft"); config->Read(Path+key, &fulltreeleft, 0);

key = wxT("startdirright"); config->Read(Path+key, &startdirright, wxT(""));
key = wxT("pathright"); config->Read(Path+key, &pathright, wxT(""));
key = wxT("fulltreeright"); config->Read(Path+key, &fulltreeright, 0);

key = wxT("TLFileonly"); config->Read(Path+key, &TLFileonly, 0);
key = wxT("BRFileonly"); config->Read(Path+key, &BRFileonly, 0);

key = wxT("showhiddenTLdir"); config->Read(Path+key, &showhiddenTLdir, 1);
key = wxT("showhiddenTLfile"); config->Read(Path+key, &showhiddenTLfile, 1);
key = wxT("showhiddenBRdir"); config->Read(Path+key, &showhiddenBRdir, 1);
key = wxT("showhiddenBRfile"); config->Read(Path+key, &showhiddenBRfile, 1);

key = wxT("splittype"); splittype = (split_type)config->Read(Path+key, (long)vertical);
key = wxT("verticalsplitpos"); verticalsplitpos = (int)config->Read(Path+key, (long)450);
key = wxT("horizontalsplitpos"); horizontalsplitpos = (int)config->Read(Path+key, (long)300);
key = wxT("leftverticalsashpos"); leftverticalsashpos = (int)config->Read(Path+key, (long)-1);  // -1 is a cue to measure the page-size
key = wxT("rightverticalsashpos"); rightverticalsashpos = (int)config->Read(Path+key, (long)-1);
key = wxT("tophorizontalsashpos"); tophorizontalsashpos = (int)config->Read(Path+key, (long)-1);
key = wxT("bottomhorizontalsashpos"); bottomhorizontalsashpos = (int)config->Read(Path+key, (long)-1);
key = wxT("unsplitsashpos"); unsplitsashpos = (int)config->Read(Path+key, (long)-1);
key = wxT("LeftTopIsActive"); LeftTopIsActive = config->Read(Path+key, (long)0);

wxString Lkey = wxT("Lwidthcol"), Rkey = wxT("Rwidthcol");
wxString LShownkey = wxT("LColShown"), RShownkey = wxT("RColShown");
for (size_t n=0; n < TOTAL_NO_OF_FILEVIEWCOLUMNS; ++n)
  { wxString suffix = wxChar('0'+n);
    Lwidthcol[n] = (int)config->Read(Path+Lkey+suffix, 0l); 
    Rwidthcol[n] = (int)config->Read(Path+Rkey+suffix, 0l);
    config->Read(Path+LShownkey+suffix, &LColShown[n], DEFAULT_COLUMN_TEMPLATE[n]); 
    config->Read(Path+RShownkey+suffix, &RColShown[n], DEFAULT_COLUMN_TEMPLATE[n]);
  }
key = wxT("Lselectedcol"); Lselectedcol = (int)config->Read(Path+key, (long)0);
key = wxT("Rselectedcol"); Rselectedcol = (int)config->Read(Path+key, (long)0);
key = wxT("Lreversesort"); config->Read(Path+key, &Lreversesort, 0);
key = wxT("Rreversesort"); config->Read(Path+key, &Rreversesort, 0);
key = wxT("Ldecimalsort"); config->Read(Path+key, &Ldecimalsort, 0);
key = wxT("Rdecimalsort"); config->Read(Path+key, &Rdecimalsort, 0);

  // If we're in fulltree mode, having a different startdir makes no sense
  // and also causes there to be 2 dirs highlit on RefreshTree()
if (fulltreeleft) startdirleft = pathleft;
if (fulltreeright) startdirright = pathright;

return tabnumber;                                      // Return the actually-used tab number (may be zero if requested one doesn't exist)
}

void Tabdata::Save(int tabno /*= 0*/, int TemplateNo /*=-1*/)  // Save the data for this tab.  If TemplateNo==-1, it's a normal save
{
wxConfigBase *config = wxConfigBase::Get();
if (config==NULL)  { wxLogError(_("Couldn't load configuration!?")); return; }

wxString Path;
if (TemplateNo >= 0) Path.Printf(wxT("/Tabs/Template_%c/"), wxT('a') + TemplateNo);  // If we're saving a template, adjust path approprately
 else Path = wxT("/Tabs/");
 
wxString Subpath; Subpath.Printf(wxT("tab_%c/"), wxT('a') + tabno);    // Make the subpath read tab_a/, tab_b/ or whatever
Path += Subpath;

config->Write(Path+wxT("tabname"), tabname);

config->Write(Path+wxT("startdirleft"), startdirleft);
config->Write(Path+wxT("pathleft"), pathleft);
config->Write(Path+wxT("fulltreeleft"), fulltreeleft);

config->Write(Path+wxT("startdirright"), startdirright);
config->Write(Path+wxT("pathright"), pathright);
config->Write(Path+wxT("fulltreeright"), fulltreeright);

config->Write(Path+wxT("TLFileonly"), TLFileonly);
config->Write(Path+wxT("BRFileonly"), BRFileonly);

config->Write(Path+wxT("showhiddenTLdir"), showhiddenTLdir);
config->Write(Path+wxT("showhiddenTLfile"), showhiddenTLfile);
config->Write(Path+wxT("showhiddenBRdir"), showhiddenBRdir);
config->Write(Path+wxT("showhiddenBRfile"), showhiddenBRfile);

config->Write(Path+wxT("splittype"), (long)splittype);
config->Write(Path+wxT("verticalsplitpos"), (long)verticalsplitpos);
config->Write(Path+wxT("horizontalsplitpos"), (long)horizontalsplitpos);
config->Write(Path+wxT("leftverticalsashpos"), (long)leftverticalsashpos);
config->Write(Path+wxT("rightverticalsashpos"), (long)rightverticalsashpos);
config->Write(Path+wxT("tophorizontalsashpos"), (long)tophorizontalsashpos);
config->Write(Path+wxT("bottomhorizontalsashpos"), (long)bottomhorizontalsashpos);
config->Write(Path+wxT("unsplitsashpos"), (long)unsplitsashpos);

bool LeftTopIsActive = (m_parent ? m_parent->LeftTopIsActive() : true);
config->Write(Path+wxT("LeftTopIsActive"), (long)LeftTopIsActive);

wxString Lkey = wxT("Lwidthcol"), Rkey = wxT("Rwidthcol");
wxString LShownkey = wxT("LColShown"), RShownkey = wxT("RColShown");
for (size_t n=0; n < TOTAL_NO_OF_FILEVIEWCOLUMNS; ++n)
  { wxString suffix = wxChar('0'+n);
    config->Write(Path+Lkey+suffix, (long)Lwidthcol[n]);
    config->Write(Path+Rkey+suffix, (long)Rwidthcol[n]);
    config->Write(Path+LShownkey+suffix, LColShown[n]);
    config->Write(Path+RShownkey+suffix, RColShown[n]);
  }

config->Write(Path+wxT("Lselectedcol"), (long)Lselectedcol);  
config->Write(Path+wxT("Rselectedcol"), (long)Rselectedcol);  
config->Write(Path+wxT("Lreversesort"), Lreversesort);
config->Write(Path+wxT("Rreversesort"), Rreversesort);
config->Write(Path+wxT("Ldecimalsort"), Ldecimalsort);
config->Write(Path+wxT("Rdecimalsort"), Rdecimalsort);
}

//--------------------------------------------------------------------------------------------------------------------------------------
DirSplitterWindow::DirSplitterWindow(wxSplitterWindow* parent, bool right)
         :  wxSplitterWindow(parent, -1, wxDefaultPosition, wxDefaultSize, 0), isright(right)
{
m_parent = parent;

DirPanel = new MyPanel(this, NextID++, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL, wxT("DirPanel"));
FilePanel = new MyPanel(this, NextID++, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL, wxT("FilePanel"));

DirSizer = new wxBoxSizer(wxVERTICAL);
FileSizer= new wxBoxSizer(wxVERTICAL);
DirToolbarSizer= new wxBoxSizer(wxVERTICAL);

m_highlight_panel = new wxPanel(DirPanel);
m_highlight_panel->SetBackgroundColour(*wxGetApp().GetBackgroundColourUnSelected());
wxBoxSizer* highlight_panelSizer= new wxBoxSizer(wxVERTICAL);
m_highlight_panel->SetSizer(highlight_panelSizer);

DirToolbarSizer->Add(m_highlight_panel, 1, wxEXPAND);
}

void DirSplitterWindow::Setup(MyTab* parent_tab, bool hasfocus, const wxString& startdir/*=""*/, bool fulltree/*=false*/, 
                                  bool showfiles/*=false*/, bool showhiddendirs/*=true*/, bool showhiddenfiles/*=true*/, const wxString& path /*=""*/)
{
DirSizer->Add(DirToolbarSizer, 0, wxEXPAND);
                                      // Create the twin controls
m_left = new DirGenericDirCtrl(DirPanel, NextID++, startdir, wxDefaultPosition, wxDefaultSize, wxSUNKEN_BORDER | wxDIRCTRL_SELECT_FIRST, fulltree);
m_right = new FileGenericDirCtrl(FilePanel, NextID++, m_left->startdir, wxDefaultPosition, wxDefaultSize, wxSUNKEN_BORDER | wxDIRCTRL_SELECT_FIRST);

m_left->ParentTab = parent_tab;                        // Tell the ctrls which page owns them
m_right->ParentTab = parent_tab;
m_left->TakeYourPartners(m_right);                     // Tell each ctrl where its partner lives
m_right->TakeYourPartners(m_left);
m_left->isright = isright;                             // Tell each ctrl whether it's part of the left/top or right/bottom twin panes
m_right->isright = isright;
                                      // Finish off the dirview
m_left->SetShowHidden(showhiddendirs);
m_left->FinishCreation();

  // Now do the fileview.  Note that we use m_left->startdir.  This is lest the original startdir didn't exist & a default was used, in which case the fileview would be blank
m_right->DisplayFilesOnly = showfiles;
m_right->SetShowHidden(showhiddenfiles);
m_right->FinishCreation();

m_left->CreateToolbar();                              // Now ParentTab is valid, it's safe to create the dirview toolbar

m_left->GetTreeCtrl()->Connect(wxEVT_SET_FOCUS, wxFocusEventHandler(DirSplitterWindow::SetDirviewFocus), NULL, this);
m_left->GetTreeCtrl()->Connect(wxEVT_KILL_FOCUS, wxFocusEventHandler(DirSplitterWindow::SetDirviewFocus), NULL, this);

                                                      // Connect the events that allow keyboard navigation
m_left-> Connect(SHCUT_SWITCH_FOCUS_OPPOSITE,SHCUT_SWITCH_FOCUS_ADJACENT, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(MyGenericDirCtrl::SetFocusViaKeyboard), NULL, m_left);
m_right->Connect(SHCUT_SWITCH_FOCUS_OPPOSITE,SHCUT_SWITCH_FOCUS_ADJACENT, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(MyGenericDirCtrl::SetFocusViaKeyboard), NULL, m_right);

if (hasfocus)                                         // If this is the LeftorTop pane-pair
  { parent_tab->LeftorTopDirCtrl = m_left;            // Store the dirview address
    parent_tab->LeftorTopFileCtrl = m_right;          //  & fileview
  }
 else
  { parent_tab->RightorBottomDirCtrl = m_left;
    parent_tab->RightorBottomFileCtrl = m_right;
  }
  
if (!path.empty()) m_right->SetPath(path);          // Set a supplied non-default path in the fileview (but not for "" as this now triggers an assert)
m_right->CreateColumns(hasfocus);                   // Set up columns in the fileview.  hasfocus tells fileview whether to use tabdata's L or R col-widths

if (hasfocus)                                       // If this is the LeftorTop pane-pair
  { wxFocusEvent fev(wxEVT_SET_FOCUS); SetDirviewFocus(fev);  // Show that this one is starting focused
    m_left->GetTreeCtrl()->SetFocus();
  }

DirSizer->Add(m_left, 1, wxEXPAND);                 // Put controls in the sizer
FileSizer->Add(m_right, 1, wxEXPAND);  

DirPanel->SetSizer(DirSizer); FilePanel->SetSizer(FileSizer);
}

void DirSplitterWindow::SetDirviewFocus(wxFocusEvent& event)  // (Un)Highlights the m_highlight_panel colour to indicate whether the dirview has focus
{
if (event.GetEventType() == wxEVT_SET_FOCUS)
  m_left->ParentTab->SwitchOffHighlights(); // Start with a blank canvas, so that we don't get multiple panes highlit

m_highlight_panel->SetBackgroundColour((event.GetEventType() == wxEVT_SET_FOCUS) ? *wxGetApp().GetBackgroundColourSelected() : *wxGetApp().GetBackgroundColourUnSelected());
m_highlight_panel->Refresh();
event.Skip();
}

void DirSplitterWindow::OnSashPosChanging(wxSplitterEvent& event)
{
size_t currentsashpos = GetSashPosition();
int diff = currentsashpos - event.GetSashPosition();  // Is the sash shrinking the dirview or the fileview, & by how much
if (!diff) return;                                    // Abort if it hasn't moved

if (diff > 0)                                         // If it's the dirview that's shrinking, impose an arbitrary min size of 10, so that it doesn't disappear
  { if ((currentsashpos - diff) < 30)  event.Veto(); return; }
                                                      // Otherwise it's the fileview
int w, h; GetClientSize(&w, &h);                      // Find the whole-splitterwin size (to subtract currentsashpos from)
if ((w - event.GetSashPosition()) < 50) event.Veto(); // Need a larger minimum, to allow for scrollbars
}

void  DirSplitterWindow::OnFullTree(wxCommandEvent& event){ m_left->OnFullTree(event); }      // Various toolbar methods, all passed to dirview
void  DirSplitterWindow::OnDirUp(wxCommandEvent& event){ m_left->OnDirUp(event); }
void  DirSplitterWindow::OnDirBack(wxCommandEvent& event){ m_left->OnDirBack(event); }
void  DirSplitterWindow::OnDirForward(wxCommandEvent& event){ m_left->OnDirForward(event); }
void  DirSplitterWindow::OnDirDropdown(wxCommandEvent& event){ m_left->OnDirDropdown(event); }
void  DirSplitterWindow::OnDirCtrlTBButton(wxCommandEvent& event){ m_left->OnDirCtrlTBButton(event); }

void DirSplitterWindow::DoToolbarUI(wxUpdateUIEvent& event){ m_left->DoToolbarUI(event); }

BEGIN_EVENT_TABLE(DirSplitterWindow,wxSplitterWindow)
  EVT_MENU(IDM_TOOLBAR_fulltree, DirSplitterWindow::OnFullTree)
  EVT_MENU(IDM_TOOLBAR_up, DirSplitterWindow::OnDirUp)
  EVT_BUTTON(XRCID("IDM_TOOLBAR_back"), DirSplitterWindow::OnDirBack)
  EVT_BUTTON(XRCID("IDM_TOOLBAR_smalldropdown1"),DirSplitterWindow::OnDirDropdown)
  EVT_BUTTON(XRCID("IDM_TOOLBAR_forward"), DirSplitterWindow::OnDirForward)
  EVT_BUTTON(XRCID("IDM_TOOLBAR_smalldropdown2"),DirSplitterWindow::OnDirDropdown)  
  EVT_MENU_RANGE(IDM_TOOLBAR_bmfirst, IDM_TOOLBAR_bmlast, DirSplitterWindow::OnDirCtrlTBButton)

  EVT_MENU(SHCUT_NAVIGATE_DIR_UP, DirSplitterWindow::OnDirUp)
  EVT_MENU(SHCUT_NAVIGATE_DIR_PREVIOUS, DirSplitterWindow::OnDirBack)
  EVT_MENU(SHCUT_NAVIGATE_DIR_NEXT, DirSplitterWindow::OnDirForward)

  EVT_UPDATE_UI_RANGE(IDM_TOOLBAR_fulltree,IDM_TOOLBAR_up, DirSplitterWindow::DoToolbarUI)
  EVT_UPDATE_UI(XRCID("IDM_TOOLBAR_back"), DirSplitterWindow::DoToolbarUI)    // These used to be a _RANGE, but that no longer works in wx2.9
  EVT_UPDATE_UI(XRCID("IDM_TOOLBAR_forward"), DirSplitterWindow::DoToolbarUI)
  EVT_SPLITTER_SASH_POS_CHANGING(-1, DirSplitterWindow::OnSashPosChanging)
  EVT_SPLITTER_DCLICK(-1, DirSplitterWindow::OnDClick)
END_EVENT_TABLE()

//--------------------------------------------------------------------------------------------------------------------------------------
void FocusController::SetCurrentFocus(wxWindow* win)
{
wxCHECK_RET(win, wxT("Null window focused :/"));
if (win == GetCurrentFocus()) return;                 // No change, so ignore

SetPreviousFocus(GetCurrentFocus());
m_CurrentFocus = win;
}

void FocusController::Invalidate(wxWindow* win) // This wxWindow is no longer shown, so don't try to focus it
{
wxCHECK_RET(win, wxT("Trying to invalidate a null window :/"));

if (win == GetPreviousFocus()) SetPreviousFocus(NULL);
if (win == GetCurrentFocus()) m_CurrentFocus = NULL; // Don't try to retrieve the previous focus; this is a 'for future reference' method, not a 'change focus' one
}

