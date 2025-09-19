/////////////////////////////////////////////////////////////////////////////
// Name:        MyFrame.h
// Purpose:     App, Frame and Layout stuff
// Part of:     4Pane
// Author:      David Hart
// Copyright:   (c) 2020 David Hart
// Licence:     GPL v3
/////////////////////////////////////////////////////////////////////////////
#ifndef MYFRAMEH
#define MYFRAMEH

#include "wx/wx.h"
#include "wx/config.h"
  

#include "wx/splitter.h"
#include "wx/sizer.h"
#include "wx/xrc/xmlres.h"
#include "wx/toolbar.h"
#include "wx/html/helpctrl.h"
#include "wx/dynlib.h"

#include "MyNotebook.h"
#include "Externs.h"
#include "Tools.h"
#include "MyDragImage.h"
#include "MyDirs.h"

#if defined __WXGTK__
  #include <gtk/gtk.h>
  #include <dlfcn.h>
#endif

#if wxVERSION_NUMBER < 2900
  DECLARE_EVENT_TYPE(myEVT_BriefMessageBox, wxID_ANY)
#else
  wxDECLARE_EVENT(myEVT_BriefMessageBox, wxCommandEvent);
#endif


class MyFrame;
class wxGenericDirCtrl;
class MyGenericDirCtrl;
class DirGenericDirCtrl;
class FileGenericDirCtrl;
class Tools;
class MyTab;
class AcceleratorList;

class FocusController  // Holds the current and last-focused windows, and most recently focused pane
{
public:
  FocusController() { m_CurrentFocus = NULL; m_PreviousFocus = NULL; }

  wxWindow* GetCurrentFocus()             const { return m_CurrentFocus; }
  wxWindow* GetPreviousFocus()            const { return m_PreviousFocus; }
  void SetCurrentFocus(wxWindow* win);
  void SetPreviousFocus(wxWindow* win)              { m_PreviousFocus = win; }
  
  void Invalidate(wxWindow* win);         // This wxWindow is no longer shown, so don't try to focus it

protected:
  wxWindow* m_CurrentFocus;
  wxWindow* m_PreviousFocus;
};


class MyApp : public wxApp
{
public:
MyApp() : wxApp(), frame(NULL), m_WaylandSession(false) {}
void RestartApplic();

const wxColour* GetBackgroundColourUnSelected() const { return &m_BackgroundColourUnSelected; }
const wxColour* GetBackgroundColourSelected(bool fileview = false) const;
void SetPaneHighlightColours();                                          // Set colours to denote selected/unselectness in the panes
wxColour AdjustColourFromOffset(const wxColour& col, int offset) const;  // Utility function. Intelligently applies the given offset to col
wxImage CorrectForDarkTheme(const wxString& filepath) const;             // Utility function. Loads an b/w image and inverts the cols if using a dark theme

wxDynamicLibrary* GetLiblzma() { return m_liblzma; }
FocusController& GetFocusController() { return m_FocusCtrlr; }

const wxString GetConfigFilepath() const { return m_ConfigFilepath; }
const wxString GetHOME() const { return m_home; }
const wxString GetXDGconfigdir() const { return m_XDGconfigdir; }
const wxString GetXDGcachedir() const { return m_XDGcachedir; }
const wxString GetXDGdatadir() const { return m_XDGdatadir; }
bool GetIsWaylandSession() const { return m_WaylandSession; }
wxString StdDataDir;
wxString StdDocsDir;
wxString startdir0, startdir1;

void SetEscapeKeycode(int code) { m_EscapeCode = code; }
void SetEscapeKeyFlags(int flags) { m_EscapeFlags = flags; }
int GetEscapeKeycode() const { return m_EscapeCode; }
int GetEscapeKeyFlags() const { return m_EscapeFlags; }
void* GetRsvgHandle() const { return m_dlRsvgHandle; }
void SetRsvgHandle(void* handle) { m_dlRsvgHandle = handle; }

private:
virtual bool OnInit();
int OnExit();
int ParseCmdline();
int FilterEvent(wxEvent& event);
void SetWaylandSession(bool session_type) { m_WaylandSession = session_type; }
#if defined(__WXX11__)  && wxVERSION_NUMBER < 2800
bool ProcessXEvent(WXEvent* _event);    // Grab selection XEvents
#endif
#if wxVERSION_NUMBER < 2600
  wxString AbsoluteFilepath;
#endif

wxColour m_BackgroundColourUnSelected;  // Holds the appropriate colour for when a pane has focus
wxColour m_DvBackgroundColourSelected;  // and unfocused, dirview
wxColour m_FvBackgroundColourSelected;  // and unfocused, fileview

MyFrame* frame;
wxLocale m_locale;
wxDynamicLibrary* m_liblzma;
FocusController m_FocusCtrlr;
wxString m_XDGconfigdir;
wxString m_XDGcachedir;
wxString m_XDGdatadir;
wxString m_ConfigFilepath;
wxString m_home; // In case we want to pass as a parameter an alternative $HOME, e.g. in a liveCD situation

int m_EscapeCode;
int m_EscapeFlags;
bool m_WaylandSession;
void* m_dlRsvgHandle;
};

DECLARE_APP(MyApp)

class MyPanel  :  public wxPanel    // This is mostly because it's good practice to put a panel into a splitter window; and we may need an event table there
{
public:
MyPanel( wxWindow* parent, wxWindowID id = -1, const wxPoint& pos = wxDefaultPosition,
                     const wxSize& size = wxDefaultSize, long style = wxTAB_TRAVERSAL, const wxString& name = wxT(""))
            :  wxPanel(parent, -1, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL, wxT(""))  {}
};

class DirSplitterWindow  :  public wxSplitterWindow          //Splitter-Window type specifically for the directories.  Is inserted into a higher-level split
{
public:
DirSplitterWindow(wxSplitterWindow* parent, bool right);

MyPanel *DirPanel, *FilePanel;
wxBoxSizer *DirSizer, *FileSizer, *DirToolbarSizer;
DirGenericDirCtrl *m_left;
FileGenericDirCtrl *m_right;

bool isright;                          // Is this the left hand conjoint twins or the right

void Setup(MyTab* parent_tab, bool hasfocus, const wxString& start_dir = wxT(""), bool full_tree = false, 
                  bool showfiles = false, bool showhiddendirs = true, bool showhiddenfiles = true, const wxString& path = wxT(""));
void SetDirviewFocus(wxFocusEvent& event);  // (Un)Highlights the m_highlight_panel colour to indicate whether the dirview has focus
wxWindow* GetDirToolbarPanel() { return m_highlight_panel; } // Returns the panel to which to add the dirview's toolbar

protected:
void OnFullTree(wxCommandEvent& event);     // Various toolbar methods, all passed to dirview
void OnDirUp(wxCommandEvent& event);
void OnDirBack(wxCommandEvent& event);
void OnDirForward(wxCommandEvent& event);
void OnDirDropdown(wxCommandEvent& event);
void OnDirCtrlTBButton(wxCommandEvent& event);

void DoToolbarUI(wxUpdateUIEvent& event);

void OnSashPosChanging(wxSplitterEvent& event);
void OnDClick(wxSplitterEvent& event){ event.Veto(); }    // On sash DClick, to prevent the default unsplitting

void SetFocusOpposite(wxCommandEvent& event);   // Key-navigate to opposite pane

wxSplitterWindow* m_parent;
wxPanel* m_highlight_panel;

DECLARE_EVENT_TABLE()
};

class MyTab;

class Tabdata            // Class to manage the appearance of a tab
{
public:
wxString startdirleft;                   // Left(Top) pane starting dir and path
wxString pathleft;

wxString startdirright;                  // Ditto Right(Bottom)
wxString pathright;

bool fulltreeleft;                       //  & their fulltree status
bool fulltreeright;
bool LeftTopIsActive;                    // Which twin-pane was selected at the time of a SaveDefaults() of an unsplit tab. Needed so that reloading gives the same unhidden pane

split_type splittype;                    // How is the pane split
int verticalsplitpos;                    // If it's vertical, this is the main sash pos
int horizontalsplitpos;                  // Horizontal ditto

bool TLFileonly;                         // Does this fileview display dirs & files, or just files?
bool BRFileonly;

bool showhiddenTLdir;                    // Does each pane show Hidden?
bool showhiddenTLfile;
bool showhiddenBRdir;
bool showhiddenBRfile;

int leftverticalsashpos;                // If it's vertical, this is where to position the dirview/fileview sash
int tophorizontalsashpos;               // etc
int rightverticalsashpos;
int bottomhorizontalsashpos;
int unsplitsashpos;

int Lwidthcol[8];                       // How wide is each Fileview column?
bool LColShown[8];                      //   and is it to be initially visible?
int Rwidthcol[8];                       // Ditto for RightBottom fileview
bool RColShown[8];

int Lselectedcol;
int Rselectedcol;
bool Lreversesort;
bool Rreversesort;
bool Ldecimalsort;
bool Rdecimalsort;

wxString tabname;                      // Label on the tab

Tabdata(MyTab* parent) : m_parent(parent) {}
Tabdata(const Tabdata& oldtabdata);
int Load(MyNotebook* NB, int tabno = 0, int TemplateNo = -1);     // Load the current defaults for this tab, or those of the requested template
void Save(int tabno = 0, int TemplateNo = -1);    // Save the data for this tab.  If TemplateNo==-1, it's a normal save );

protected:
MyTab* m_parent;
};

class MyTab  :  public wxPanel    // Represents each page of the notebook.  Derived so as to parent the GenericDirCtrls
{
public:
MyTab(MyNotebook* parent, wxWindowID id = -1, int pageno = 0, int TemplateNo = -1, const wxString& name = wxT(""), const wxString& startdir0=wxT(""), const wxString& startdir1=wxT(""));
MyTab(Tabdata* tabdatatocopy, MyNotebook* dad, wxWindowID id = -1, int TemplateNo = -1, const wxString& name = wxT(""));
~MyTab(){ delete tabdata; }

void Create(const wxString& startdir0 = wxT(""), const wxString& startdir1 = wxT(""));    
void PerformSplit(split_type splittype, bool newtab = false);  // Splits a tab into left/right or top/bottom or unsplit.  If newtab, initialises one pane
void StoreData(int tabno);                       // Refreshes the tabdata ready for saving
void SwitchOffHighlights();

bool LeftTopIsActive(){ return (LeftRightDirview == LeftorTopDirCtrl); }  // Returns true if the active twinpane is the left (or top) one
void SetActivePaneFromConfig();  // On loading, sets which should become the active twinpane

split_type GetSplitStatus() { return splitstatus; }
void SetActivePane(MyGenericDirCtrl* active);
MyGenericDirCtrl* GetActivePane() const { return LastKnownActivePane; }
MyGenericDirCtrl* GetActiveDirview() const { return LeftRightDirview; }

class Tabdata* tabdata;
int tabdatanumber;                       // Stores the type of tabdata that this tab gets loaded with, so that notebook can save it later
DirSplitterWindow* m_splitterLeftTop, *m_splitterRightBottom;
MyGenericDirCtrl* LeftorTopDirCtrl;      // These point to the actual Dirviews, for ease of extracting current rootdir, path etc
MyGenericDirCtrl* LeftorTopFileCtrl;     //   & Fileviews, for the column widths
MyGenericDirCtrl* RightorBottomDirCtrl;
MyGenericDirCtrl* RightorBottomFileCtrl;

protected:
wxSplitterWindow* m_splitterQuad;
MyGenericDirCtrl* LeftRightDirview;      // Holds the dirview of which of the 2 ctrls was last 'active', Left/Top or Right/Bottom
MyGenericDirCtrl* LastKnownActivePane;   // Holds which pane was last active
split_type splitstatus;                  // Vertical/Horiz/Unsplit status, just to simplify Viewmenu UpdateUI

void OnIdle(wxIdleEvent& event);
#if wxVERSION_NUMBER > 2603 && ! (defined(__WXGTK__) || defined(__WXX11__))
bool NeedsLayout;
#endif
};

class TabTemplateOverwriteDlg  :  public wxDialog  // Simple dialog to allow choice between Overwrite & SaveAs in MyNotebook::SaveAsTemplate()
{
protected:
void OnOverwriteButton(wxCommandEvent& WXUNUSED(event)){ EndModal(XRCID("Overwrite")); }
void OnSaveAsButton(wxCommandEvent& WXUNUSED(event)){ EndModal(XRCID("SaveAs")); }
DECLARE_EVENT_TABLE()
};

class LayoutWindows
{
public:
LayoutWindows(MyFrame* parent);
~LayoutWindows();

void Setup();                        // Does the splitting work

void OnCommandLine();
void ShowHideCommandLine();          // If it's showing, hide it; & vice versa

void DoTool(enum toolchoice whichtool, bool FromSetup = false);  // Invoke the requested tool in the bottom panel
void CloseBottom();                  // Called to return from DoTool()
bool ShowBottom(bool FromSetup = false); // If bottompanel isn't visible, show it
void UnShowBottom();                 // If bottompanel is visible, unsplit it
void OnChangeDir(const wxString& newdir, bool ReusePrompt = true);  // Change terminal/commandline prompt, and perhaps titlebar, when cd entered or the dirview selection changes
void OnUnsplit(wxSplitterEvent& event);
TerminalEm* GetTerminalEm() { if (tool) return tool->text; return NULL; }

wxSplitterWindow* m_splitterMain;
MyNotebook* m_notebook;
wxPanel* bottompanel;                // The pane for the bottom section, for displaying grep etc
TerminalEm* commandline;
bool EmulatorShowing;
bool CommandlineShowing;
wxPanel* commandlinepane;

protected:
Tools* tool;
MyFrame* m_frame;
};


class MyFrame : public wxFrame
{
public:

MyFrame(wxSize size);
virtual ~MyFrame();

void OnActivate(wxActivateEvent& event);

// Menu commands
void OnUndo(wxCommandEvent& event);
void OnUndoSidebar(wxCommandEvent& event);
void OnRedo(wxCommandEvent& event);
void OnRedoSidebar(wxCommandEvent& event);
void OnArchiveAppend(wxCommandEvent& event);
void OnArchiveTest(wxCommandEvent& event);
void SplitHorizontal(wxCommandEvent& event);
void SplitVertical(wxCommandEvent& event);
void Unsplit(wxCommandEvent& event);
void OnTabTemplateLoadMenu(wxCommandEvent& event);

void OnQuit(wxCommandEvent& event);
void OnHelpF1(wxKeyEvent& event);
void OnHelpContents(wxCommandEvent& event);
void OnHelpFAQs(wxCommandEvent& event);
void OnHelpAbout(wxCommandEvent& event);

wxHtmlHelpController* Help;
AcceleratorList* AccelList;

wxBoxSizer* sizerMain;
wxBoxSizer* sizerControls;    // The sizer within the 'toolbar' panelette main sizer.  Holds the device bitmapbuttons, so DeviceAndMountManager needs to access it
wxBoxSizer* EditorsSizer;
wxBoxSizer* FixedDevicesSizer;

void OnUpdateTrees(const wxString& path, const wxArrayInt& IDs, const wxString& newstartdir = wxT(""), bool FromArcStream = false);  // Tells all panes that branch <path> needs updating.  IDs hold which panes MUST be done
void UpdateTrees();                                // At the end of a cluster of actions, tells all treectrls actually to do the updating they've stored
void OnUnmountDevice(wxString& path, const wxString& replacement="");  // When we've successfully umounted eg a floppy, reset treectrl(s) displaying it to 'replacement' or HOME or...

void SetSensibleFocus();                           // Ensures that keyboard focus is held by a valid window, usually the active pane
MyGenericDirCtrl* GetActivePane();                 // Returns the most-recently-active pane in the visible tab
MyTab* GetActiveTab();                             // Returns the currently-active tab, if there is one
void RefreshAllTabs(wxFont* font, bool ReloadDirviewTBs=false);  // Refreshes all panes on all tabs, optionally with a new font, or mini-toolbars
void LoadToolbarButtons();                         // Encapsulates loading tools onto the toolbar
void TextFilepathEnter();                          // The Filepath textctrl has the focus and Enter has been added, so GoTo its contents

void OnBeginDrag(wxWindow* dragorigin);     // DnD
void DoMutex(wxMouseEvent& event);
void OnESC_Pressed();                       // Cancel DnD if ESC pressed
void OnMouseMove(wxMouseEvent& event);
void OnEndDrag(wxMouseEvent& event);
void OnMouseWheel(wxMouseEvent& event);     // If we use the mousewheel to scroll during DnD
void OnDnDRightclick(wxMouseEvent& event);  // If we use the Rt mousebutton to scroll a page during DnD
bool m_dragMode;

bool SelectionAvailable;                    // Flags whether it's safe to ask m_notebook for active pane

bool m_CtrlPressed;                         // Stores the pattern of metakeys during DnD
bool m_ShPressed;
bool m_AltPressed;
enum myDragResult ParseMetakeys();          // & this deduces the intended result
bool ESC_Pressed;                           // Makes DnD abort
void DoOnProcessCancelled();                // Cancels any active thread

static MyFrame* mainframe;                  // To facilitate access to the frame from deep inside

wxToolBar *toolbar;
wxBitmapButton* largesidebar1;              // The UnRedo buttons sidebars (those things that allow you to choose any of the previous entries) 
wxBitmapButton* largesidebar2;
int buttonsbeforesidebar;                   // Used to help locate the position for the sidebar pop-up menus
int separatorsbeforesidebar;
wxPanel *panelette;                         // The 2nd half of the "toolbar"
TextCtrlBase *m_tbText;                     // The toolbar textctrl, for duplicating the selected filepath
void DisplayEditorsInToolbar();             // These are stored in a separate sizer within the panelette sizer, for ease of recreation

LayoutWindows* Layout;
int PropertiesPage;                         // When showing the Properties notebook, which page should we start with?

class Configure* configure;                 // Pointer to my configure class, not wxConfigBase

wxBoxSizer *sizerTB;
wxMutex m_TreeDragMutex;
wxCursor& GetStandardCursor() { return m_StandardCursor; } 
wxCursor& GetTextCrlCursor()  { return m_TextCrlCursor; } 
wxCursor DnDStdCursor;
wxCursor DnDSelectedCursor;

protected:
void CreateAcceleratorTable();
void ReconfigureShortcuts(wxNotifyEvent& event);  // Reconfigure all shortcuts after user has changed some
void CreateMenuBar( wxMenuBar* MenuBar );
void RecreateMenuBar();
void AddControls();               // Just to encapsulate adding textctrl & an editor button or 2 to the "toolbar", now actually a panel

void OnAppendTab(wxCommandEvent& event){ Layout->m_notebook->OnAppendTab(event); }    // These are just switchboards for Menu & Toolbar events
void OnInsTab(wxCommandEvent& event){ Layout->m_notebook->OnInsTab(event); }
void OnDelTab(wxCommandEvent& event){ Layout->m_notebook->OnDelTab(event); }
void OnPreview(wxCommandEvent& event);
void OnPreviewUI(wxUpdateUIEvent& event);
void OnDuplicateTab(wxCommandEvent& event){ Layout->m_notebook->OnDuplicateTab(event); }
void OnRenTab(wxCommandEvent& event){ Layout->m_notebook->OnRenTab(event); }
#if defined __WXGTK__
void OnAlwaysShowTab(wxCommandEvent& event){ Layout->m_notebook->OnAlwaysShowTab(event); }
void OnSameTabSize(wxCommandEvent& event){ Layout->m_notebook->OnSameTabSize(event); }
#endif
void OnReplicate(wxCommandEvent& event);      // Find the active MyGenericDirCtrl & pass request to it
void OnSwapPanes(wxCommandEvent& event);  // Ditto
void OnSaveTabs(wxCommandEvent& event);
void OnSaveAsTemplate(wxCommandEvent& WXUNUSED(event)){ Layout->m_notebook->SaveAsTemplate(); }
void OnDeleteTemplate(wxCommandEvent& WXUNUSED(event)){ Layout->m_notebook->DeleteTemplate(); }
void OnCut(wxCommandEvent& event);
void OnCopy(wxCommandEvent& event);
void OnPaste(wxCommandEvent& event);
void OnProcessCancelled(wxCommandEvent& event);
void OnProcessCancelledUI(wxUpdateUIEvent& event);
void OnNew(wxCommandEvent& event);
void OnHardLink(wxCommandEvent& event);
void OnSoftLink(wxCommandEvent& event);
void OnTrash(wxCommandEvent& event);
void OnDelete(wxCommandEvent& event);
void OnReallyDelete(wxCommandEvent& event);
void OnRename(wxCommandEvent& event);
void OnRefresh(wxCommandEvent& event);
void OnDup(wxCommandEvent& event);
void OnProperties(wxCommandEvent& event);
void OnFilter(wxCommandEvent& event);
void OnToggleHidden(wxCommandEvent& event);
void OnViewColMenu(wxCommandEvent& event);
void OnArchiveExtract(wxCommandEvent& event);
void OnArchiveCreate(wxCommandEvent& event);
void OnArchiveCompress(wxCommandEvent& event);
void OnTermEm(wxCommandEvent& event);
void OnToolsLaunch(wxCommandEvent& event);
void OnToolsRepeat(wxCommandEvent& event);
void DoRepeatCommandUI(wxUpdateUIEvent& event);  //  UI for SHCUT_TOOLS_REPEAT menu entry
void OnEmptyTrash(wxCommandEvent& event);

void OnMountPartition(wxCommandEvent& event);
void OnUnMountPartition(wxCommandEvent& event);
void OnMountIso(wxCommandEvent& event);
void OnMountNfs(wxCommandEvent& event);
void OnMountSshfs(wxCommandEvent& event);
void OnMountSamba(wxCommandEvent& event);
void OnUnMountNetwork(wxCommandEvent& event);

void OnCommandLine(wxCommandEvent& event);
void OnLaunchTerminal(wxCommandEvent& event);
void OnLocate(wxCommandEvent& event);
void OnFind(wxCommandEvent& event);
void OnGrep(wxCommandEvent& event);

void OnAddToBookmarks(wxCommandEvent& event);
void OnManageBookmarks(wxCommandEvent& event);
void OnBookmark(wxCommandEvent& event);

void ToggleFileviewSizetype(wxCommandEvent& event);
void ToggleRetainRelSymlinkTarget(wxCommandEvent& event);
void ToggleRetainMtimeOnPaste(wxCommandEvent& event);
void OnConfigure(wxCommandEvent& event);
void OnConfigureShortcuts(wxCommandEvent& event);
void OnShowBriefMessageBox(wxCommandEvent& event);
void SetFocusViaKeyboard(wxCommandEvent& event);

void OnPasteThreadProgress(wxCommandEvent& event);   // A PasteThread is reporting some bytes written
void OnPasteThreadFinished(PasteThreadEvent& event); // Called when a PasteThread completes

#ifdef __WXX11__
void OnIdle( wxIdleEvent& event );    // One-off to make the toolbar textctrl show the initial selection properly
#endif

#if wxVERSION_NUMBER >= 2800
void OnMouseCaptureLost(wxMouseCaptureLostEvent& event) {OnEndDrag((wxMouseEvent&)event); } // If the mouse escapes, try and do something sensible
#endif

void OnTextFilepathEnter(wxCommandEvent& WXUNUSED(event)){ TextFilepathEnter(); }

void DoMiscUI(wxUpdateUIEvent& event);         //  UpdateUI for misc items
void DoQueryEmptyUI(wxUpdateUIEvent& event);   // This is for the items which are UIed depending on whether there's a selection
void DoNoPanesUI(wxUpdateUIEvent& event);      //  UI for when there aren't any tabs
void DoMiscTabUI(wxUpdateUIEvent& event);      //  UI for the tab-head items
void OnDirSkeletonUI(wxUpdateUIEvent& event);  //  UI for SHCUT_PASTE_DIR_SKELETON
void DoSplitPaneUI(wxUpdateUIEvent& event);    //  UI for the Split/Unsplit menu
void DoTabTemplateUI(wxUpdateUIEvent& event);  //  UI for Tab menu
void DoColViewUI(wxUpdateUIEvent& event);
                    // DnD                            
wxWindow* lastwin;              // Stores the window we've just been dragging over (in case it's changed)
wxMutex m_DragMutex;
bool m_LeftUp;                  // If there's a LeftUp event but the mutex is locked
bool m_finished;                // Flags that EndDrag has been done
MyDragImage *m_dragImage;
wxCursor m_StandardCursor;      // Stores the cursor that 4Pane started with (in case of DnD accidents)
wxCursor m_TextCrlCursor;       // Ditto for textctrls
wxCursor oldCursor;             // Stores original version

private:
DECLARE_EVENT_TABLE()
};


// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// ID for the menu commands
enum
{
    SPLIT_QUIT,
    SPLIT_HORIZONTAL,
    SPLIT_VERTICAL,
    SPLIT_UNSPLIT,
    SPLIT_LIVE,
    SPLIT_SETPOSITION,
    SPLIT_SETMINSIZE
};

#endif
    // MYFRAMEH 
    
