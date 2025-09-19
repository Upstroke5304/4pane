/////////////////////////////////////////////////////////////////////////////
// (Original) Name:        dirctrlg.h
// Purpose:     wxGenericDirCtrl class
//              Builds on wxDirCtrl class written by Robert Roebling for the
//              wxFile application, modified by Harm van der Heijden.
//              Further modified for Windows. Further modified for 4Pane
// Author:      Robert Roebling, Harm van der Heijden, Julian Smart et al
// Modified by: David Hart 2003-20
// Created:     21/3/2000
// Copyright:   (c) Robert Roebling, Harm van der Heijden, Julian Smart. Alterations (c) David Hart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
/*
#ifndef _WX_DIRCTRL_H_
#define _WX_DIRCTRL_H_

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "dirctrlg.h"
#endif

#if wxUSE_DIRDLG
*/

#ifndef MYGENERICDIRCTRL
#define MYGENERICDIRCTRL


#include "wx/treectrl.h"
#include "wx/dialog.h"
#include "wx/dirctrl.h"
#include "wx/choice.h"

#include "wx/hashmap.h"           // //
#include "wx/hashset.h"           // //

#include "Externs.h"              // //
#include "ArchiveStream.h"        // //

#include "wx/version.h"

// We need to use the cutdown sdk version of wxFileSystemWatcher pre 3.0.0 as it didn't exist earlier
// and we need to use it for 3.1.0 and <3.0.3 because of occasional crashes; see http://trac.wxwidgets.org/ticket/17122
#if	wxVERSION_NUMBER < 3003 || wxVERSION_NUMBER == 3100
  #define USE_MYFSWATCHER true
#else
  #define USE_MYFSWATCHER false
#endif

#if defined(__LINUX__)
  #if USE_MYFSWATCHER
    #include "sdk/fswatcher/MyFSWatcher.h"    // //
  #else
    #include "wx/fswatcher.h"       // //
  #endif

  #if wxVERSION_NUMBER < 2900
    DECLARE_EVENT_TYPE(FSWatcherProcessEvents, wxID_ANY)
    DECLARE_EVENT_TYPE(FSWatcherRefreshWatchEvent, wxID_ANY)
  #else
    wxDECLARE_EVENT(FSWatcherProcessEvents, wxNotifyEvent);
    wxDECLARE_EVENT(FSWatcherRefreshWatchEvent, wxNotifyEvent);
  #endif
#endif // defined(__LINUX__)



//-----------------------------------------------------------------------------
// classes
//-----------------------------------------------------------------------------

class wxTextCtrl;
class DirGenericDirCtrl;
class MyGenericDirCtrl;
class ThreadSuperBlock;


class MyFSEventManager
{
#if defined(__LINUX__) && defined(__WXGTK__)
  WX_DECLARE_STRING_HASH_MAP(wxFileSystemWatcherEvent*, FilepathEventMap);

  public:
  MyFSEventManager() : m_owner(NULL), m_EventSent(false) {}
  ~MyFSEventManager(){ ClearStoredEvents(); }

  void SetOwner(MyGenericDirCtrl* owner) { m_owner = owner; }

  void AddEvent(wxFileSystemWatcherEvent& event);

  void ProcessStoredEvents();
  void ClearStoredEvents();   // Clear the data e.g. if we're entering an archive, otherwise we'll block on exit

  protected:
  void DoProcessStoredEvent(const wxString& filepath, const wxFileSystemWatcherEvent* event);

  FilepathEventMap m_Eventmap;
#endif // defined(__LINUX__) && defined(__WXGTK__)

MyGenericDirCtrl* m_owner;
bool m_EventSent;
};

//-----------------------------------------------------------------------------
// Extra styles for wxGenericDirCtrl
//-----------------------------------------------------------------------------

/*enum
{
    // Only allow directory viewing/selection, no files
    wxDIRCTRL_DIR_ONLY       = 0x0010,
    // When setting the default path, select the first file in the directory
    wxDIRCTRL_SELECT_FIRST   = 0x0020,
    // Show the filter list
    wxDIRCTRL_SHOW_FILTERS   = 0x0040,
    // Use 3D borders on internal controls
    wxDIRCTRL_3D_INTERNAL    = 0x0080,
    // Editable labels
    wxDIRCTRL_EDIT_LABELS    = 0x0100
};
*/
//-----------------------------------------------------------------------------
// wxDirItemData
//-----------------------------------------------------------------------------
/*
class WXDLLEXPORT wxDirItemData : public wxTreeItemData
{
public:
    wxDirItemData(const wxString& path, const wxString& name, bool isDir);
    ~wxDirItemData();
    void SetNewDirName(const wxString& path);

    bool HasSubDirs() const;
    bool HasFiles(const wxString& spec = wxEmptyString) const;

    wxString m_path, m_name;
    bool m_isHidden;
    bool m_isExpanded;
    bool m_isDir;
};
*/
//-----------------------------------------------------------------------------
// wxDirCtrl
//-----------------------------------------------------------------------------

class wxDirFilterListCtrl;

class DirSplitterWindow;  // //
class MyTab;              // //
class MyTreeCtrl;         // //
class MyFSWatcher;        // //
class MyFSEventManager;   // //

class MyGenericDirCtrl : public wxGenericDirCtrl
{
friend class MyFSWatcher;
friend class MyFSEventManager;
public:
    MyGenericDirCtrl();
    MyGenericDirCtrl(wxWindow *parent, MyGenericDirCtrl* parentcontrol, const wxWindowID id = -1,
              const wxString &dir = wxDirDialogDefaultFolderStr,
              const wxPoint& posit = wxDefaultPosition,
              const wxSize& sze = wxDefaultSize,
              long style = wxDIRCTRL_3D_INTERNAL|wxSUNKEN_BORDER,
              const wxString& filter = wxEmptyString,
              int defaultFilter = 0,
              const wxString& name = wxT("MyGenericDirCtrl"),
        bool file_view = ISLEFT, bool full_tree=false)  : arcman(NULL), pos(posit), size(sze)
        
    {
    ParentControl = parentcontrol;  // //
        Init();
     startdir = dir;  // //
     fileview = file_view;  // //
     fulltree = full_tree;    // //
        Create(parent, id, dir, style, filter, defaultFilter, name);
    }
    
    bool Create(wxWindow *parent, const wxWindowID id = -1,
                    const wxString &dir = wxDirDialogDefaultFolderStr,
                    long style = wxDIRCTRL_3D_INTERNAL|wxSUNKEN_BORDER,
                    const wxString& filter = wxEmptyString,
                    int defaultFilter = 0,
                    const wxString& name = wxTreeCtrlNameStr
           );
void FinishCreation();    // // Separated from Create() so that MyFileDirCtrl can be initialised first      
void CreateTree();        // //  I extracted this from Create, 2b reused in ReCreate
  
    virtual void Init();

    virtual ~MyGenericDirCtrl();

ArchiveStreamMan* arcman;                         // //  Manages any archive stream
    
wxString  startdir;                              // //  Holds either the (real) root or the selected dir, depending on fulltree
MyGenericDirCtrl* partner;                       // //  Pointer to the twin pane, for coordination of dir selection
bool isright;                                    // //  Are we part of the left/top twinpane, or the right/bottom?
void  TakeYourPartners(MyGenericDirCtrl *otherside)  // //  Set the pointer to this ctrl's partner
    { partner = otherside; }
    void OnExpandItem(wxTreeEvent &event);
    void OnCollapseItem(wxTreeEvent &event);
  void OnBeginEditItem(wxTreeEvent &event);
  void OnEndEditItem(wxTreeEvent &event);
    void OnSize(wxSizeEvent &event);
void OnTreeSelected(wxTreeEvent &event);      // //
void OnTreeItemDClicked(wxTreeEvent &event);  // //
  void OnOutsideSelect(wxString& path, bool CheckDevices=true); // // Used to redo Ctrl from outside, eg the floppy-button clicked (& if called from Mount, don't remount again!)
  wxString GetPathIfRelevant(); // // Helper function used to get selection to pass to NavigationManager
  void RefreshTree(const wxString& path, bool CheckDevices=true, bool RetainSelection=true); // // Refreshes the display to show any outside changes (or following a change of rootitem via OnOutsideSelect)
  void OnRefreshTree(wxCommandEvent& event);                    // // From Context menu or F5. Needed to provide the parameter for RefreshTree
void DisplaySelectionInTBTextCtrl(const wxString& selection);   // // 
void CopyToClipboard(const wxArrayString& filepaths, bool primary = false) const; // // Add the selection to the (real) clipboard
virtual void NewFile(wxCommandEvent& event){};  // // Create a new File or Dir

wxString GetActiveDirectory();                  // // Returns the root dir if in branch-mode, or the selected dir in Fulltree-mode
wxString GetActiveDirPath();                    // // Returns selected dir in  either mode  NB GetPath returns either dir OR file

    // Try to expand as much of the given path as possible.
    virtual bool ExpandPath(const wxString& path);

    // Accessors

    virtual inline wxString GetDefaultPath() const { return m_defaultPath; }
    virtual void SetDefaultPath(const wxString& path) { m_defaultPath = path; }

    // Get dir or filename
    virtual wxString GetPath() const;
virtual size_t GetMultiplePaths(wxArrayString& paths) const; // // Get an array of selected paths, used as multiple selection is enabled    

static bool Clustering;              // // Flags that we're in the middle of a cluster of activity, so don't update trees yet

static wxArrayString filearray;      // // These form the "clipboard" for copy etc.  Static as we want only one for whole application,
static size_t filecount;
static wxWindowID FromID;
static MyGenericDirCtrl* Originpane;
static wxArrayString DnDfilearray;   // // Similarly for DragnDrop
static wxString DnDDestfilepath;     // // Where we're pasting to
static wxWindowID DnDFromID;         // // ID of the originating pane
static wxWindowID DnDToID;           // // ID of the destination pane
static size_t DnDfilecount;
  
    // Get selected filename path only (else empty string).
    // I.e. don't count a directory as a selection
    virtual wxString GetFilePath() const;
    virtual void SetPath(const wxString& path);
    
    virtual void ShowHidden(bool show);
    virtual bool GetShowHidden() { return m_showHidden; }
void SetShowHidden(bool show){ m_showHidden = show; }
// //   virtual wxString GetFilter() const { return m_filter; }
// //   virtual void SetFilter(const wxString& filter);
void SetFilterArray(const wxArrayString& array, bool FilesOnly = false){ FilterArray = array; DisplayFilesOnly = FilesOnly; }  // //
void SetFilterArray(const wxString& filterstring, bool FilesOnly = false);  // // Parse the string into individual filters & store in array
wxString GetFilterString();                                   // //
wxArrayString& GetFilterArray(){ return FilterArray; }        // //
bool DisplayFilesOnly;                                        // // Flags a fileview to filter out dirs if required
bool NoVisibleItems;                                          // // Flags whether the user has filtered out all the items. Used for menuitem updateui
// //  virtual int GetFilterIndex() const { return m_currentFilter; }
// //   virtual void SetFilterIndex(int n);

    MyTreeCtrl* GetMyTreeCtrl() const { return m_treeCtrl; }
    virtual wxTreeCtrl* GetTreeCtrl() const { return (wxTreeCtrl*)m_treeCtrl; }        // // Altered as m_treeCtrl is now a MyTreeCtrl
// //    virtual wxDirFilterListCtrl* GetFilterListCtrl() const { return m_filterListCtrl; }

    // Helper
    virtual void SetupSections();
    
    // Parse the filter into an array of filters and an array of descriptions
// //    virtual int ParseFilter(const wxString& filterStr, wxArrayString& filters, wxArrayString& descriptions);
    
    // Find the child that matches the first part of 'path'.
    // E.g. if a child path is "/usr" and 'path' is "/usr/include"
    // then the child for /usr is returned.
    // If the path string has been used (we're at the leaf), done is set to true
    virtual wxTreeItemId FindChild(wxTreeItemId parentId, const wxString& path, bool& done);
    
    // Resize the components of the control
    virtual void DoResize();
    
    // Collapse & expand the tree, thus re-creating it from scratch:
    virtual void ReCreateTree();
    virtual void ReCreateTreeBranch(const wxTreeItemId* ItemToRefresh = NULL); // // Implements wxGDC::ReCreateTree, but with parameter

bool fulltree;                 // //  Flags whether we're allowed to show truncated trees, or if always full
bool SelFlag;                  // //  Flags if an apparent selection event should be disabled as its coming from an internal process
bool fileview;                 // //  Is this a left-side, directory, view or a right-side, file view?
  
void ReCreateTreeFromSelection(); // // On selection/DClick event, this redoes the tree & reselects
MyTab* ParentTab;              // // Pointer to the control's real parent, a tab panel.  Managed by derived class  

protected:
    void ExpandDir(wxTreeItemId parentId);
    void CollapseDir(wxTreeItemId parentId);
    const wxTreeItemId AddSection(const wxString& path, const wxString& name, int imageId = 0);

    // Extract description and actual filter from overall filter string
    bool ExtractWildcard(const wxString& filterStr, int n, wxString& filter, wxString& description);

void SetTreePrefs();      // //  Adjusts the Tree width parameters according to prefs
void DoDisplaySelectionInTBTextCtrl(wxNotifyEvent& event);       // // Does so on receiving a MyUpdateToolbartextEvent from DisplaySelectionInTBTextCtrl

MyGenericDirCtrl* ParentControl;    // //  Pointer to the derived CONTROL, eg DirGenericDirCtrl, not the splitter window or tab panel

#if defined(__LINUX__) && defined(__WXGTK__)
	void OnFileWatcherEvent(wxFileSystemWatcherEvent& event);
	void OnProcessStoredFSWatcherEvents(wxNotifyEvent& event);
	MyFSWatcher* m_watcher;  // //
	MyFSEventManager m_eventmanager;  // //
#endif

public:
    void UpdateTree(InotifyEventType type, const wxString& alteredfilepath, const wxString& newfilepath = wxT("")); // // Update the data and labels of (part of) the tree, without altering its visible structure
protected:
    void DoUpdateTree(wxTreeItemId id, InotifyEventType type, const wxString& oldfilepath, const wxString& newfilepath); // //
void DoUpdateTreeRename(wxTreeItemId id, const wxString& oldfilepath, const wxString& newfilepath); // //

void SetRootItemDataPath(const wxString& fpath);
      // ----------------------------------------------The following were originally in MyDirs/Files  --------------------------------------------------------------------------------
void OnShortcutUndo(wxCommandEvent&);                  // // Ctrl-Z
void OnShortcutRedo(wxCommandEvent&);                  // // Ctrl-R
bool Delete(bool trash, bool fromCut=false);           // // Used by both OnShortcutTrash and OnShortcutDel, & by Cut

public:
wxTreeItemId FindIdForPath(const wxString& path) const;
void OnSplitpaneVertical(wxCommandEvent& event);       // // Vertically splits the panes of the current tab
void OnSplitpaneHorizontal(wxCommandEvent& event);     // // Horizontally splits the panes of the current tab
void OnSplitpaneUnsplit(wxCommandEvent& event);        // // Unsplits the panes of the current tab
void OnFilter(wxCommandEvent& event);                  // // Calls Filter dialog
void OnProperties(wxCommandEvent& event);              // // Calls Properties dialog
void OnBrowseButton(wxCommandEvent& event);            // // If symlink-target Browse button clicked in Properties dialog
void OnLinkTargetButton(wxCommandEvent& event);        // // If symlink-target button clicked in Properties dialog
void LinkTargetButton(wxCommandEvent& event);          // // Recreate tree to goto selected link's target 
void BrowseButton();                                   // // If symlink-target Browse button clicked in Properties dialog 
wxDialog* PropertyDlg;                                 // // To store the above dlg

void OnShortcutGoToSymlinkTarget(wxCommandEvent& event){ GoToSymlinkTarget(event.GetId() == SHCUT_GOTO_ULTIMATE_SYMLINK_TARGET); }
void GoToSymlinkTarget(bool ultimate);                 // // Goto selected link's target (or ultimate target).  Called both by the above context-menu entry & by LinkTargetButton()

bool OnExitingArchive();                               // // We were displaying the innards of an archive, and now we're not

void OnShortcutTrash(wxCommandEvent& event);           // // Del
void OnShortcutDel(wxCommandEvent& event);             // // Sh-Del
void OnShortcutReallyDelete();    // //
void OnShortcutCopy(wxCommandEvent& event);            // // Ctrl-C
void OnShortcutCut(wxCommandEvent& event);             // // Ctrl-X
void OnShortcutHardLink(wxCommandEvent& event);        // // Ctr-Sh-L
void OnShortcutSoftLink(wxCommandEvent& event);        // // Alt-Sh-L
void OnShortcutPaste(wxCommandEvent& event);           // // Ctrl-V.  Calls the following:
void OnPaste(bool FromDnD, const wxString& destinationpath = wxT(""), bool dir_skeleton = false);  // // Ctrl-V or DnD
void OnReplicate(wxCommandEvent& event);               // // Duplicates this pane's filepath in its opposite pane
void OnSwapPanes(wxCommandEvent& event);               // // Swap this pane's filepath with that of its opposite pane
void OnLink(bool FromDnD, int linktype);               // // Create hard or soft link
void OnDnDMove();                                      // //
MyGenericDirCtrl* GetOppositeDirview(bool OnlyIfUnsplit = true); // // Return a pointer to the dirview of the twin-pane opposite this one
MyGenericDirCtrl* GetOppositeEquivalentPane();         // // Return a pointer to the equivalent view of the twin-pane opposite this one
void SetFocusViaKeyboard(wxCommandEvent& event);       // // Key-navigate to opposite or adjacent pane
void RefreshVisiblePanes(wxFont* font=(wxFont*)NULL);  // // Called e.g. after a change of font
void ReloadDirviewToolbars();                          // // Called after a change of user-defined tools
static enum MovePasteResult Paste(wxFileName* From, wxFileName* To, const wxString& ToName, bool ItsADir, bool dir_skeleton=false,
                                    bool recursing=false, ThreadSuperBlock* tsb = NULL, const wxString& overwrittenfile = wxT("")); // // Copies files, dirs+contents
static enum MovePasteResult Move(wxFileName* From, wxFileName* To, wxString ToName, ThreadSuperBlock* tsb, const wxString& OverwrittenFile = wxT("")); // // Moves files, dirs+contents.  Static as used by UnRedo too
static bool ReallyDelete(wxFileName* PathName);                             // // Not just trashing, the real thing.  eg for undoing pastes
int OnNewItem(wxString& newname, wxString& path, bool ItsADir = true);      // // Used to create both new dirs & files
void OnDup(wxCommandEvent& event);                                          // // Duplicate.  Actually calls DoRename
void OnRename(wxCommandEvent& event);                                       // // F2 for rename. Calls DoRename
void DoRename(bool RenameOrDuplicate, const wxString& pathtodup = wxT("")); // // Does either rename or duplicate, depending on the bool
void DoMultipleRename(wxArrayString& paths, bool duplicate);                // // Renames (or duplicates) multiple files/dirs
int Rename(wxString& path, wxString& origname, wxString& newname, bool ItsADir, bool dup=false);  // // Renames both dirs & files (or dups if the bool is set)


void OnIdle(wxIdleEvent& event);                       // // Checks whether we need to update the statusbar with e.g. size of selection?
virtual void UpdateStatusbarInfo(const wxString& selected){} // In derived classes, calls the following overload
virtual void UpdateStatusbarInfo(const wxArrayString& selections){} // In derived classes, writes the selection's name & some relevant data in the statusbar
bool m_StatusbarInfoValid;                             // // If true, the statusbar size info etc is up-to-date
      // -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
private:
    bool            m_showHidden;
    wxTreeItemId    m_rootId;
    wxImageList*    m_imageList;
    wxString        m_defaultPath; // Starting path
    long            m_styleEx; // Extended style
    wxString        m_filter;  // Wildcards in same format as per wxFileDialog   // // No longer used
    int             m_currentFilter; // The current filter index                 // // No longer used
    wxString m_currentFilterStr;     // Current filter string                    // // No longer used, except as a convenient wxString
    wxArrayString FilterArray;       // // Flter strings now they can be multiple
    MyTreeCtrl*     m_treeCtrl;
    wxDirFilterListCtrl* m_filterListCtrl;  // // No longer used

long treeStyle;    // //  I moved these 4 vars to here from locals in Create, so their data is still available in ReCreate 
long filterStyle;
const wxPoint& pos;
const wxSize& size;                                                
  
//  wxTreeItemId LocateItem(wxDirItemData* data);        // //  MINE. Helper function for OnTreeSelected()
//  int CompareItems(wxTreeItemId candidateId, wxDirItemData* data);    // Subfunction for LocateItem()

private:
    DECLARE_EVENT_TABLE()
    DECLARE_DYNAMIC_CLASS(MyGenericDirCtrl)
};

//-----------------------------------------------------------------------------
// wxDirFilterListCtrl
//-----------------------------------------------------------------------------
/*
class WXDLLEXPORT wxDirFilterListCtrl: public wxChoice
{
public:
    wxDirFilterListCtrl() { Init(); }
    wxDirFilterListCtrl(wxGenericDirCtrl* parent, const wxWindowID id = -1,
              const wxPoint& pos = wxDefaultPosition,
              const wxSize& size = wxDefaultSize,
              long style = 0)
    {
        Init();
        Create(parent, id, pos, size, style);
    }
    
    bool Create(wxGenericDirCtrl* parent, const wxWindowID id = -1,
              const wxPoint& pos = wxDefaultPosition,
              const wxSize& size = wxDefaultSize,
              long style = 0);

    void Init();

    ~wxDirFilterListCtrl() {};

    //// Operations
    void FillFilterList(const wxString& filter, int defaultFilter);

    //// Events
    void OnSelFilter(wxCommandEvent& event);

protected:
    wxGenericDirCtrl*    m_dirCtrl;

    DECLARE_EVENT_TABLE()
    DECLARE_CLASS(wxDirFilterListCtrl)
};
*/
#if !defined(__WXMSW__) && !defined(__WXMAC__) && !defined(__WXPM__)
    #define wxDirCtrl wxGenericDirCtrl
#endif

// Symbols for accessing individual controls
#define wxID_TREECTRL          7000
#define wxID_FILTERLISTCTRL    7001

//#endif // wxUSE_DIRDLG

#if defined(__LINUX__) && defined(__WXGTK__)
class DirviewWatchTimer;
class WatchOverflowTimer;

class MyFSWatcher
{
public:
MyFSWatcher(wxWindow* owner = NULL);
~MyFSWatcher();

void SetFileviewWatch(const wxString& path);
void SetDirviewWatch(const wxString& path);
void RefreshWatchDirCreate(const wxString& filepath); // Updates after an fsw create (dir) event
void RefreshWatch(const wxString& filepath1 = wxT(""), const wxString& filepath2 = wxT("")); // Updates after an fsw event
void DoDelayedSetDirviewWatch(const wxString& path);
void OnDelayedDirwatchTimer();
void PrintWatches(const wxString& msg = wxT("")) const;
void AddWatchLineage(const wxString& filepath);     // Add a watch for filepath and all visible descendant dirs
void RemoveWatchLineage(const wxString& filepath);  // Remove all watches starting with this filepath
WatchOverflowTimer* GetOverflowTimer() const { return m_OverflowTimer; }
wxString GetWatchedBasepath() const { return m_WatchedBasepath; }
#if USE_MYFSWATCHER
  MyFileSystemWatcher* GetWatcher() const { return m_fswatcher; }
#else
  wxFileSystemWatcher* GetWatcher() const { return m_fswatcher; }
#endif
wxWindow* GetOwner() const { return m_owner; }

protected:
bool CreateWatcherIfNecessary();
bool IsWatched(const wxString& filepath) const;     // Is filepath currently watched?

DirviewWatchTimer* m_DelayedDirwatchTimer;
WatchOverflowTimer* m_OverflowTimer;
wxString m_DelayedDirwatchPath;
#if USE_MYFSWATCHER
  MyFileSystemWatcher* m_fswatcher;
#else
  wxFileSystemWatcher* m_fswatcher;
#endif
wxWindow* m_owner;
wxString m_WatchedBasepath;         // For fileviews, helps prevent reapplying the current watches from SetPath()
};

class DirviewWatchTimer : public wxTimer  // Used by MyFSWatcher
{
public:
DirviewWatchTimer(MyFSWatcher* watch) : wxTimer(), m_watch(watch) {}

protected:
void Notify(){ m_watch->OnDelayedDirwatchTimer(); }

MyFSWatcher* m_watch;
};

class WatchOverflowTimer : public wxTimer  // Used when wxFSW_WARNING_OVERFLOW is received
{
public:
WatchOverflowTimer(MyFSWatcher* watch) : wxTimer(), m_watch(watch), m_isdirview(true) {}
void SetFilepath(const wxString& filepath) { m_filepath = filepath; }
void SetType(bool isdirview) { m_isdirview = isdirview; }

protected:
void Notify();

MyFSWatcher* m_watch;
bool m_isdirview;
wxString m_filepath;
};
#endif // defined(__LINUX__) && defined(__WXGTK__)

#endif
    // MYGENERICDIRCTRL  (Originally  _WX_DIRCTRLG_H_)
