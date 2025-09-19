/////////////////////////////////////////////////////////////////////////////
// (Original) Name:        dirctrlg.cpp
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

#include "wx/wxprec.h"

#include "wx/log.h"
#include "wx/app.h"
#include "wx/frame.h"
#include "wx/scrolwin.h"
#include "wx/menu.h"
#include "wx/dirctrl.h"
#include "wx/config.h"
#include "wx/splitter.h"
#include "wx/filename.h" 
#include "wx/sizer.h"
#include "wx/dnd.h"
#include "wx/dcmemory.h"
#include "wx/dragimag.h"
#include "wx/clipbrd.h"
#include "wx/arrimpl.cpp"
#include "wx/dir.h"

#include "Devices.h"
#include "Archive.h"
#include "MyGenericDirCtrl.h"
#include "Filetypes.h"
#include "MyTreeCtrl.h"
#include "MyDirs.h"
#include "MyFiles.h"
#include "MyFrame.h"
#include "Misc.h"

#if defined __WXGTK__
  #include <gtk/gtk.h>
#endif

#if USE_MYFSWATCHER
  #include "sdk/fswatcher/MyFSWatcher.h"
#endif

WX_DEFINE_OBJARRAY(FileDataObjArray);                    // Define the array of (Fake)FileData objects

#if wxVERSION_NUMBER < 2900
  DEFINE_EVENT_TYPE(FSWatcherProcessEvents)
  DEFINE_EVENT_TYPE(FSWatcherRefreshWatchEvent)
#else
  wxDEFINE_EVENT(FSWatcherProcessEvents, wxNotifyEvent);
  wxDEFINE_EVENT(FSWatcherRefreshWatchEvent, wxNotifyEvent);
#endif


/* Closed folder */ 
static const char * icon1_xpm[] = {
/* width height ncolors chars_per_pixel */
"16 16 6 1",
/* colors */
"   s None  c None",
".  c #000000",
"+  c #c0c0c0",
"@  c #808080",
"#  c #ffff00",
"$  c #ffffff",
/* pixels */
"                ",
"                ",
"   @@@@@        ",
"  @#+#+#@       ",
" @#+#+#+#@@@@@@ ",
" @$$$$$$$$$$$$@.",
" @$#+#+#+#+#+#@.",
" @$+#+#+#+#+#+@.",
" @$#+#+#+#+#+#@.",
" @$+#+#+#+#+#+@.",
" @$#+#+#+#+#+#@.",
" @$+#+#+#+#+#+@.",
" @$#+#+#+#+#+#@.",
" @@@@@@@@@@@@@@.",
"  ..............",
"                "};

/* Open folder */
static const char * icon2_xpm[] = {
/* width height ncolors chars_per_pixel */
"16 16 6 1",
/* colors */
"   s None  c None",
".  c #000000",
"+  c #c0c0c0",
"@  c #808080",
"#  c #ffff00",
"$  c #ffffff",
/* pixels */
"                ",
"                ",
"   @@@@@        ",
"  @$$$$$@       ",
" @$#+#+#$@@@@@@ ",
" @$+#+#+$$$$$$@.",
" @$#+#+#+#+#+#@.",
"@@@@@@@@@@@@@#@.",
"@$$$$$$$$$$@@+@.",
"@$#+#+#+#+##.@@.",
" @$#+#+#+#+#+.@.",
" @$+#+#+#+#+#.@.",
"  @$+#+#+#+##@..",
"  @@@@@@@@@@@@@.",
"   .............",
"                "};

/* File */
static const char * icon3_xpm[] = {
/* width height ncolors chars_per_pixel */
"16 16 3 1",
/* colors */
"     s None    c None",
".    c #000000",
"+    c #ffffff",
/* pixels */
"                ",
"                ",
"  ........      ",
"  .++++++..     ",
"  .+.+.++.+.    ",
"  .++++++....   ",
"  .+.+.+++++.   ",
"  .+++++++++.   ",
"  .+.+.+.+.+.   ",
"  .+++++++++.   ",
"  .+.+.+.+.+.   ",
"  .+++++++++.   ",
"  .+.+.+.+.+.   ",
"  .+++++++++.   ",
"  ...........   ",
"                "};

/* Computer */
static const char * icon4_xpm[] = {
"16 16 7 1",
"     s None    c None",
".    c #808080",
"X    c #c0c0c0",
"o    c Black",
"O    c Gray100",
"+    c #008080",
"@    c Blue",
"    ........... ",
"   .XXXXXXXXXX.o",
"   .OOOOOOOOO..o",
"   .OoooooooX..o",
"   .Oo+...@+X..o",
"   .Oo+XXX.+X..o",
"   .Oo+....+X..o",
"   .Oo++++++X..o",
"   .OXXXXXXXX.oo",
"   ..........o.o",
"   ...........Xo",
"   .XXXXXXXXXX.o",
"  .o.o.o.o.o...o",
" .oXoXoXoXoXo.o ",
".XOXXXXXXXXX.o  ",
"............o   "};

/* Drive */
static const char * icon5_xpm[] = {
"16 16 7 1",
"     s None    c None",
".    c #808080",
"X    c #c0c0c0",
"o    c Black",
"O    c Gray100",
"+    c Green",
"@    c #008000",
"                ",
"                ",
"                ",
"                ",
"  ............. ",
" .XXXXXXXXXXXX.o",
".OOOOOOOOOOOO..o",
".XXXXXXXXX+@X..o",
".XXXXXXXXXXXX..o",
".X..........X..o",
".XOOOOOOOOOOX..o",
"..............o ",
" ooooooooooooo  ",
"                ",
"                ",
"                "};

/* CD-ROM */
static const char *icon6_xpm[] = {
"16 16 10 1",
"     s None    c None",
".    c #808080",
"X    c #c0c0c0",
"o    c Yellow",
"O    c Blue",
"+    c Black",
"@    c Gray100",
"#    c #008080",
"$    c Green",
"%    c #008000",
"        ...     ",
"      ..XoX..   ",
"     .O.XoXXX+  ",
"    ...O.oXXXX+ ",
"    .O..X.XXXX+ ",
"   ....X.+..XXX+",
"   .XXX.+@+.XXX+",
"   .X@XX.+.X@@X+",
" .....X...#XX@+ ",
".@@@...XXo.O@X+ ",
".@XXX..XXoXOO+  ",
".@++++..XoX+++  ",
".@$%@@XX+++X.+  ",
".............+  ",
" ++++++++++++   ",
"                "};

/* Floppy */                                              // // I changed this to a floppy disk
static const char * icon7_xpm[] = {
"16 16 11 1",
"  s None c None",
". c #a0a0a4",
"# c #008686",
"a c #0000ff",
"b c #0000c0",
"c c #dddddd",
"d c #ffffff",
"e c #c0c0c0",
"f c #006bc9",
"g c #000080",
"h c #969696",
"bbbbbbbbbbbbbbbh",
"baageeeeeeeeefab",
"baagdddddddddfab",
"baageeeeeeeedfab",
"baagdddddddddfab",
"baageeeeeeeddfab",
"baagddddddddefab",
"baaaaaaaaaaaafab",
"baaaaaaaaaaaafab",
"baaggggggggggfab",
"baag.eeeeegaafab",
"baag.aa.eegaafab",
"baag.aa..egaafab",
"baag.aa..egaafab",
"hb#gccccccg##fab",
"  bbbbbbbbbbbbbh"};

/* Removeable */
static const char * icon8_xpm[] = {
"16 16 7 1",
"     s None    c None",
".    c #808080",
"X    c #c0c0c0",
"o    c Black",
"O    c Gray100",
"+    c Red",
"@    c #800000",
"                ",
"                ",
"                ",
"  ............. ",
" .XXXXXXXXXXXX.o",
".OOOOOOOOOOOO..o",
".OXXXXXXXXXXX..o",
".O+@.oooooo.X..o",
".OXXOooooooOX..o",
".OXXXOOOOOOXX..o",
".OXXXXXXXXXXX..o",
".O............o ",
" ooooooooooooo  ",
"                ",
"                ",
"                "};

#include "bitmaps/include/symlink.xpm"                                // // Icon for Symlink
#include "bitmaps/include/symlinkbroken.xpm"                          // // Icon for broken Symlinks
#include "bitmaps/include/SymlinkToFolder.xpm"                        // // Icon for Symlink-to-folder
#include "bitmaps/include/LockedFolder.xpm"                           // // Icon for Locked Folder
#include "bitmaps/include/usb.xpm"                                    // // Icon for usb drive
#include "bitmaps/include/pipe.xpm"                                   // // Icon for FIFO
#include "bitmaps/include/blockdevice.xpm"                            // // Icon for Block device
#include "bitmaps/include/chardevice.xpm"                             // // Icon for Char device
#include "bitmaps/connect_no.xpm"                                     // // Icon for Socket
#include "bitmaps/include/compressedfile.xpm"                         // // Icon for compressed files
#include "bitmaps/include/tarball.xpm"                                // // Icon for tarballs et al e.g. .a 
#include "bitmaps/include/compressedtar.xpm"                          // // Icon for compressed tarballs, .cpio, packages e.g. rpms
#include "bitmaps/include/GhostClosedFolder.xpm"                      // // Icon for closed folder inside a virtual archive
#include "bitmaps/include/GhostOpenFolder.xpm"                        // // Icon for open folder inside a virtual archive
#include "bitmaps/include/GhostFile.xpm"                              // // Icon for file inside a virtual archive
#include "bitmaps/include/GhostCompressedFile.xpm"                    // // Icon for compressed file inside a virtual archive
#include "bitmaps/include/GhostTarball.xpm"                           // // Icon for archive inside a virtual archive
#include "bitmaps/include/GhostCompressedTar.xpm"                     // // Icon for compressed archive inside a virtual archive
#include "bitmaps/include/UnknownFolder.xpm"                          // // Icon for corrupted folder
#include "bitmaps/include/UnknownFile.xpm"                            // // Icon for corrupted file

// Function which is called by quick sort. We want to override the default wxArrayString behaviour,
// by using wxStrcoll(), which makes the sort locale-aware (see bug 2863704)
static int LINKAGEMODE wxDirCtrlStringCompareFunctionLC_COLLATE(const void *first, const void *second)
{
    wxString strFirst(*((wxString*)(first)));
    wxString strSecond(*((wxString*)(second)));

    return wxStrcoll((strFirst.c_str()), (strSecond.c_str()));
}

// The original, non-local-aware method
static int LINKAGEMODE wxDirCtrlStringCompareFunction(const void *first, const void *second)
{
    wxString *strFirst = (wxString *)first;
    wxString *strSecond = (wxString *)second;

    return strFirst->CmpNoCase(*strSecond);
}

static int LINKAGEMODE (*wxDirCtrlStringCompareFunc)(const void *first, const void *second) = &wxDirCtrlStringCompareFunctionLC_COLLATE;

void SetDirpaneSortMethod(const bool LC_COLLATE_aware)  // If true, make the dirpane sorting take account of LC_COLLATE
{
if (LC_COLLATE_aware)
  wxDirCtrlStringCompareFunc = &wxDirCtrlStringCompareFunctionLC_COLLATE;
 else
  wxDirCtrlStringCompareFunc = &wxDirCtrlStringCompareFunction;
}

//----------------------------------------------------------------------------- 
// wxGenericDirCtrl
//-----------------------------------------------------------------------------
#if wxVERSION_NUMBER < 2900
  DEFINE_EVENT_TYPE(MyUpdateToolbartextEvent)      // A custom event-type, used by DisplaySelectionInTBTextCtrl()
#else
  wxDEFINE_EVENT(MyUpdateToolbartextEvent, wxNotifyEvent);
#endif

#define EVT_MYUPDATE_TBTEXT(fn) \
        DECLARE_EVENT_TABLE_ENTRY( \
            MyUpdateToolbartextEvent, wxID_ANY, wxID_ANY, \
            (wxObjectEventFunction)(wxEventFunction)(wxNotifyEventFunction)&fn, \
            (wxObject*) NULL),


IMPLEMENT_DYNAMIC_CLASS(MyGenericDirCtrl, wxControl)

BEGIN_EVENT_TABLE(MyGenericDirCtrl, wxGenericDirCtrl)
    EVT_TREE_ITEM_EXPANDED(-1, MyGenericDirCtrl::OnExpandItem)
    EVT_TREE_ITEM_COLLAPSED(-1, MyGenericDirCtrl::OnCollapseItem)
    EVT_MENU(SHCUT_CUT, MyGenericDirCtrl::OnShortcutCut)
    EVT_MENU(SHCUT_COPY, MyGenericDirCtrl::OnShortcutCopy)
    EVT_MENU(SHCUT_PASTE, MyGenericDirCtrl::OnShortcutPaste)
    EVT_MENU(SHCUT_HARDLINK, MyGenericDirCtrl::OnShortcutHardLink)
    EVT_MENU(SHCUT_SOFTLINK, MyGenericDirCtrl::OnShortcutSoftLink)
    EVT_MENU(SHCUT_TRASH, MyGenericDirCtrl::OnShortcutTrash)
    EVT_MENU(SHCUT_DELETE, MyGenericDirCtrl::OnShortcutDel)
    EVT_MENU(SHCUT_RENAME, MyGenericDirCtrl::OnRename)
    EVT_MENU(SHCUT_DUP, MyGenericDirCtrl::OnDup)
    EVT_MENU(SHCUT_REPLICATE, MyGenericDirCtrl::OnReplicate)
    EVT_MENU(SHCUT_SWAPPANES, MyGenericDirCtrl::OnSwapPanes)
    EVT_MENU(SHCUT_FILTER, MyGenericDirCtrl::OnFilter)
    EVT_MENU(SHCUT_PROPERTIES, MyGenericDirCtrl::OnProperties)
    EVT_MENU(SHCUT_SPLITPANE_VERTICAL, MyGenericDirCtrl::OnSplitpaneVertical)
    EVT_MENU(SHCUT_SPLITPANE_HORIZONTAL, MyGenericDirCtrl::OnSplitpaneHorizontal)
    EVT_MENU(SHCUT_SPLITPANE_UNSPLIT, MyGenericDirCtrl::OnSplitpaneUnsplit)
    EVT_MENU(SHCUT_UNDO, MyGenericDirCtrl::OnShortcutUndo)  
    EVT_MENU(SHCUT_REDO, MyGenericDirCtrl::OnShortcutRedo)
    EVT_MENU(SHCUT_REFRESH, MyGenericDirCtrl::OnRefreshTree)
    EVT_MENU(SHCUT_GOTO_SYMLINK_TARGET, MyGenericDirCtrl::OnShortcutGoToSymlinkTarget)
    EVT_MENU(SHCUT_GOTO_ULTIMATE_SYMLINK_TARGET, MyGenericDirCtrl::OnShortcutGoToSymlinkTarget)
     
    EVT_SIZE(MyGenericDirCtrl::OnSize)
    EVT_TREE_SEL_CHANGED     (-1,  MyGenericDirCtrl::OnTreeSelected)    // //
    EVT_TREE_ITEM_ACTIVATED(-1,  MyGenericDirCtrl::OnTreeItemDClicked)  // //
    EVT_MYUPDATE_TBTEXT(MyGenericDirCtrl::DoDisplaySelectionInTBTextCtrl)  // //
    EVT_IDLE(MyGenericDirCtrl::OnIdle)  
#if defined(__LINUX__) && defined(__WXGTK__) && !USE_MYFSWATCHER
    EVT_FSWATCHER(wxID_ANY, MyGenericDirCtrl::OnFileWatcherEvent)  // //
#endif
END_EVENT_TABLE()


MyGenericDirCtrl::MyGenericDirCtrl(void)    :   pos(wxDefaultPosition), size(wxDefaultSize)
{
    Init();
}

bool MyGenericDirCtrl::Create(wxWindow *parent,
                              const wxWindowID id,
                              const wxString& dir,
                              long style,
                              const wxString& filter,
                              int defaultFilter,
                              const wxString& name
                            )
{
  if (!wxControl::Create(parent, id, pos, size, style, wxDefaultValidator, name))
        return false;
        
   SelFlag = false;                                           // // Flags if a selection event is real or from internals
   DisplayFilesOnly = false;                                  // //
   NoVisibleItems = false;                                    // // Flags whether the user has filtered out all the items
   
  wxColour backgroundcol = wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE);
  SetBackgroundColour(backgroundcol);

if (!USE_DEFAULT_TREE_FONT && CHOSEN_TREE_FONT.Ok())  SetFont(CHOSEN_TREE_FONT);    // //
  else
#ifdef __WXGTK__
  { wxFont font = GetFont(); font.SetPointSize(font.GetPointSize() - 2); SetFont(font); }  // // GTK2 gives a font-size that is c.2 points larger than GTK1.2
#endif
  
    Init();
 
  if  (fileview==ISLEFT)                                     // //  Directory view
          treeStyle = wxTR_HAS_BUTTONS | wxTR_HIDE_ROOT;
   else
           treeStyle = wxTR_HIDE_ROOT;                       // //  File view so no "+-boxes"
          
  treeStyle |= wxTR_MULTIPLE;      // //  Implement multiple selection
          
    if (style & wxDIRCTRL_EDIT_LABELS)
        treeStyle |= wxTR_EDIT_LABELS;

    if ((style & wxDIRCTRL_3D_INTERNAL) == 0)
        treeStyle |= wxNO_BORDER;
    else
        treeStyle |= wxBORDER_SUNKEN;

    filterStyle = 0;
    if ((style & wxDIRCTRL_3D_INTERNAL) == 0)
        filterStyle |= wxNO_BORDER;
    else
        filterStyle |= wxBORDER_SUNKEN;

   m_filter = filter;

  //  SetFilterIndex(defaultFilter);

    if (m_filterListCtrl)
        m_filterListCtrl->FillFilterList(filter, defaultFilter);

  m_treeCtrl = new MyTreeCtrl(this, NextID++/*wxID_TREECTRL*/, pos, size, treeStyle);        // // Originally wxTreeCtrl
  
  if (COLOUR_PANE_BACKGROUND)                                 // //
    { if (fileview == ISLEFT) backgroundcol =  BACKGROUND_COLOUR_DV;
        else backgroundcol =  SINGLE_BACKGROUND_COLOUR ? BACKGROUND_COLOUR_DV:BACKGROUND_COLOUR_FV;
      m_treeCtrl->SetBackgroundColour(backgroundcol);
    }
#if wxVERSION_NUMBER >= 3000 && defined(__WXGTK3__) && !GTK_CHECK_VERSION(3,10,0)
   else // An early gtk+3, which by default will give the tree a grey background
    { m_treeCtrl->SetBackgroundColour( GetSaneColour(m_treeCtrl, true, wxSYS_COLOUR_LISTBOX) ); }
#endif

    m_imageList = new wxImageList(16, 16, true);
    m_imageList->Add(wxIcon(icon1_xpm));
    m_imageList->Add(wxIcon(icon2_xpm));
    m_imageList->Add(wxIcon(icon3_xpm));
    m_imageList->Add(wxIcon(icon4_xpm));
    m_imageList->Add(wxIcon(icon5_xpm));
    m_imageList->Add(wxIcon(icon6_xpm));
    m_imageList->Add(wxIcon(icon7_xpm));
    m_imageList->Add(wxIcon(icon8_xpm));
    m_imageList->Add(wxIcon(symlink_xpm));                        // //
    m_imageList->Add(wxIcon(symlinkbroken_xpm));                  // //
    m_imageList->Add(wxIcon(SymlinkToFolder_xpm));                // //
    m_imageList->Add(wxIcon(LockedFolder_xpm));                   // //
    m_imageList->Add(wxIcon(usb_xpm));                            // //
    m_imageList->Add(wxIcon(pipe_xpm));                           // //
    m_imageList->Add(wxIcon(blockdevice_xpm));                    // //
    m_imageList->Add(wxIcon(chardevice_xpm));                     // //
    m_imageList->Add(wxIcon(connect_no_xpm));                     // //
    m_imageList->Add(wxIcon(compressedfile_xpm));                 // //
    m_imageList->Add(wxIcon(tarball_xpm));                        // //
    m_imageList->Add(wxIcon(compressedtar_xpm));                  // //
    m_imageList->Add(wxIcon(ghostclosedfolder_xpm));              // //
    m_imageList->Add(wxIcon(ghostopenfolder_xpm));                // //
    m_imageList->Add(wxIcon(ghostfile_xpm));                      // //
    m_imageList->Add(wxIcon(ghostcompressedfile_xpm));            // //
    m_imageList->Add(wxIcon(ghosttarball_xpm));                   // //
    m_imageList->Add(wxIcon(ghostcompressedtar_xpm));             // //
    m_imageList->Add(wxIcon(UnknownFolder_xpm));                  // //
    m_imageList->Add(wxIcon(UnknownFile_xpm));                    // //

    m_treeCtrl->AssignImageList(m_imageList);                     // //

    m_showHidden = SHOWHIDDEN;
          
   return true;
}

void MyGenericDirCtrl::FinishCreation()    // // Separated from Create() so that MyFileDirCtrl can be initialised first
{
#if defined(__LINUX__) && defined(__WXGTK__)
  m_eventmanager.SetOwner(this);
  m_watcher = NULL;                                              // // In case we can't instantiate it here as the eventloop isn't yet available
#endif

if (fileview == ISLEFT) arcman = new ArchiveStreamMan;           // // The twin views share an archive manager
 else if (partner) arcman = partner->arcman;                     // // So create in the dirview (which is made first), copy when in fileview

CreateTree();
if (fileview == ISLEFT)
  static_cast<DirGenericDirCtrl*>(this)->GetNavigationManager().PushEntry(startdir);        // // startdir will probably have been empty until the CreateTree() call
  
SelFlag = true;                                                  // //  Permits future selection events

if (USE_FSWATCHER)
  {
    #if defined(__LINUX__) && defined(__WXGTK__)
    if (wxApp::IsMainLoopRunning())  // If the loop's not running yet, this will happen from OnIdle()
      { m_watcher = new MyFSWatcher(this);
        if (USE_FSWATCHER && !(arcman && arcman->IsArchive()))
          { if (fileview == ISLEFT)
              m_watcher->SetDirviewWatch(GetActiveDirPath());
             else
              m_watcher->SetFileviewWatch(GetActiveDirPath());
          }
      }

    Connect(wxID_ANY, FSWatcherProcessEvents, wxNotifyEventHandler(MyGenericDirCtrl::OnProcessStoredFSWatcherEvents), NULL, this);

    #if USE_MYFSWATCHER
      Connect(wxID_ANY, wxEVT_FSWATCHER, wxFileSystemWatcherEventHandler(MyGenericDirCtrl::OnFileWatcherEvent), NULL, this);
    #endif
    #endif
  }
}


void MyGenericDirCtrl::OnIdle(wxIdleEvent& WXUNUSED(event))
{
#if defined(__LINUX__) && defined(__WXGTK__)
if (!m_watcher && !GetActiveDirPath().empty())      // If not already done, and the event-loop is now running
  { m_watcher = new MyFSWatcher(this);
    
    if (USE_FSWATCHER && !(arcman && arcman->IsArchive()))
      { if (fileview == ISLEFT)
          m_watcher->SetDirviewWatch(GetActiveDirPath());
         else
          m_watcher->SetFileviewWatch(GetActiveDirPath());
      }
  }

if (USE_FSWATCHER && arcman && arcman->IsArchive())
  m_eventmanager.ClearStoredEvents();
#endif

if (m_StatusbarInfoValid) return;                   // Call UpdateStatusbarInfo() if needed

if (GetPath().IsEmpty()) return;  //We must check for emptiness, as this method is called before creation is absolutely finished, whereupon arcman->IsArchive() segs

wxArrayString selections;
size_t count = GetMultiplePaths(selections);

if (arcman && !arcman->IsArchive() && (fileview == ISRIGHT)) // Archives are done in FileGenericDirCtrl::UpdateStatusbarInfo
  { wxStaticCast(this, FileGenericDirCtrl)->SelectedCumSize = 0;
    for (size_t n=0; n < count; ++n)
      { if (SHOW_RECURSIVE_FILEVIEW_SIZE)  // Do we use the slow & complete recursive method
          wxStaticCast(this, FileGenericDirCtrl)->SelectedCumSize += GetDirSizeRecursively(selections.Item(n));
         else
          wxStaticCast(this, FileGenericDirCtrl)->SelectedCumSize += GetDirSize(selections.Item(n));  // or the quick and incomplete one-shot?
      }
  }
 else if (arcman && !arcman->IsArchive() && fileview == ISLEFT)
  { wxStaticCast(this, DirGenericDirCtrl)->SelectedCumSize = 0;
    for (size_t n=0; n < count; ++n)
      wxStaticCast(this, DirGenericDirCtrl)->SelectedCumSize += GetDirSize(selections.Item(n));
  }

m_StatusbarInfoValid = true;
UpdateStatusbarInfo(selections);
}

#if defined(__LINUX__) && defined(__WXGTK__)
void MyGenericDirCtrl::OnProcessStoredFSWatcherEvents(wxNotifyEvent& event)
{
if (USE_FSWATCHER && !(arcman && arcman->IsArchive()))
  { int count = event.GetInt();
    if (count > 0) // If the count > 0 resend it, decremented. Otherwise do the actual processing
      { event.SetInt(count - 1);
        wxPostEvent(this, event);
      }
     else
      m_eventmanager.ProcessStoredEvents();
  }
}
#endif

void MyGenericDirCtrl::CreateTree()    // //  Extracted from Create, reused in ReCreateTreeFromSelection()
{
  if (startdir.IsEmpty())  startdir = wxGetApp().GetHOME();     // // If startdir = null, go straight HOME
  bool InsideArchive = false;                                   // // Used below as we can't use FileData within an archive
  ziptype ArchiveType = zt_invalid;                             // //
                                      // // This is all mine
  if (fileview == ISLEFT)  // // If dirview and inside an archive, make sure we're still inside it. If not, come out layer by layer until startdir is found
    while (arcman->IsArchive() &&                               // // If within an archive, see if we just dclicked inside the current archive
              !(arcman->IsWithinThisArchive(startdir) || arcman->IsWithinThisArchive(startdir + wxFILE_SEP_PATH)))
                     OnExitingArchive();                        // // If not, remove a layer of nested archives & try again

  InsideArchive = arcman && arcman->IsWithinArchive(startdir);

  bool success = true;                                         // //
  ArchiveType = Archive::Categorise(startdir);                 // // Check if startdir is likely to be an archive
  if (fileview == ISLEFT && ArchiveType != zt_invalid)
    { if (arcman && (!arcman->GetArc() ||  (startdir+wxFILE_SEP_PATH) != arcman->GetArc()->Getffs()->GetRootDir()->GetFilepath()))  // // and not this one
        success = arcman->NewArc(startdir, ArchiveType);       // // If so, this Pushes any old arc and creates a new one
    }
    else if (InsideArchive)  ArchiveType = arcman->GetArchiveType();  // // Otherwise, if we're still inside an archive, revert to its type

    
  if (fileview==ISLEFT)
    { if (!success || (ArchiveType == zt_invalid && !wxDirExists(startdir)))   // // If it's not a dir or an archive, or if NewArc failed above
        { while (startdir.Right(1) == wxFILE_SEP_PATH && startdir.Len() > 1) startdir.RemoveLast();
          startdir = startdir.BeforeLast(wxFILE_SEP_PATH);    // // If the requested startdir doesn't exist, try its parent
          if (!wxDirExists(startdir))
            { startdir = wxGetApp().GetHOME();                // // If this still doesn't exist, use any passed HOME
              if (!wxDirExists(startdir))
                  startdir = wxGetHomeDir();                  // // If this doesn't exist either, use $HOME (which surely . . . )
            }
        }

      
      MyGenericDirCtrl* active = MyFrame::mainframe->GetActivePane();
      if (active)  
        if (active->GetId() == GetId())
          DisplaySelectionInTBTextCtrl(startdir);             // //  If we're the active pane, update the textctrl
        
        
      if (!startdir.IsSameAs(wxFILE_SEP_PATH))                // // Don't want to root-prune
        while (startdir.Right(1) == wxFILE_SEP_PATH           // // Avoid '/' problems
                  && startdir.Len() > 1)     startdir.RemoveLast(); // //  but don't remove too many --- lest someone enters "///" ?
    }

  m_defaultPath = startdir;                                  // //
  SetTreePrefs();                                            // // Sets the Tree widths according to prefs
  
  wxDirItemData* rootData;
  
  if (fulltree &&  fileview==ISLEFT) 
     rootData = new wxDirItemData(wxT("/"), wxT(""), true); // // The original way
   else
    if (fileview==ISLEFT)   rootData = new wxDirItemData(/*startdir*/wxT("foo"), wxT(""), true);
     else                 rootData = new wxDirItemData(startdir, wxT(""), true);
    
    wxString rootName;

#if defined(__WINDOWS__) || defined(__WXPM__) || defined(__DOS__)
    rootName = _("Computer");
#else
    rootName = _("Sections");
#endif

    m_rootId = m_treeCtrl->AddRoot(rootName, 3, -1, rootData);
    m_treeCtrl->SetItemHasChildren(m_rootId);
    // I've added this unnecessary call (to a protected wxGenericTreeCtrl function) to protect against pathological filenames
    // e.g. if a file is called "Foo\rBar\rBaz", or if disc corruption results in linefeed-containing garbage, each line of the tree would have 3*correct line-height
    // and this would persist even after navigating away. So reset the height when the tree is recreated 
    m_treeCtrl->CallCalculateLineHeight();

  if (!InsideArchive)                                       // //
    { FileData startdirFD(startdir);                        // //
      if (fileview == ISLEFT || (fileview == ISRIGHT && startdirFD.CanTHISUserExecute()))  // // If this is a fileview, check we have exec permission for parent dir.  Otherwise it looks horrible
          ExpandDir(m_rootId); // automatically expand first level
    }
   else
           ExpandDir(m_rootId); // automatically expand first level
          
    //Expand and select the default path
    if (!m_defaultPath.IsEmpty())                          // This was set above to the passed param "dir"
        ExpandPath(m_defaultPath);

    DoResize();
}

MyGenericDirCtrl::~MyGenericDirCtrl()
{
if (fileview == ISLEFT) { delete arcman; arcman = NULL; }   // //
 else arcman = NULL;                                        // // Needed as Redo sometimes stores subsequently-deleted MyGenericDirCtrls*

#if defined(__LINUX__) && defined(__WXGTK__)
	delete m_watcher;
#endif
}

void MyGenericDirCtrl::Init()
{ 
  partner = NULL;
  arcman = NULL;
m_showHidden = SHOWHIDDEN;                                  // //
    m_imageList = NULL;
    m_currentFilter = 0;
    SetFilterArray(wxEmptyString, false);                   // // Default: any file
    m_treeCtrl = NULL;
    m_filterListCtrl = NULL;
}

void MyGenericDirCtrl::ShowHidden(bool show)
{
    m_showHidden = show;

    wxString path = GetPath();
    ReCreateTree();
    if (!path.empty()) SetPath(path);
}

const wxTreeItemId MyGenericDirCtrl::AddSection(const wxString& path, const wxString& name, int imageId)
{
    wxDirItemData *dir_item = new wxDirItemData(path,name,true);

    wxTreeItemId id = m_treeCtrl->AppendItem(m_rootId, name, imageId, -1, dir_item);

    m_treeCtrl->SetItemHasChildren(id);
  return id;
}

void MyGenericDirCtrl::SetupSections()
{
#if defined(__WINDOWS__) || defined(__DOS__) || defined(__WXPM__) 

#ifdef __WIN32__
    wxChar driveBuffer[256];
    size_t n = (size_t) GetLogicalDriveStrings(255, driveBuffer);
    size_t i = 0;
    while (i < n)
    {
        wxString path, name;
        path.Printf(wxT("%c:\\"), driveBuffer[i]);
        name.Printf(wxT("(%c:)"), driveBuffer[i]);

        int imageId = GDC_drive;
        int driveType = ::GetDriveType(path);
        switch (driveType)
        {
            case DRIVE_REMOVABLE:
                if (path == wxT("a:\\") || path == wxT("b:\\"))
                    imageId = GDC_floppy;
                else
                    imageId = GDC_removable;
                break;
            case DRIVE_FIXED:
                imageId = GDC_drive;
                break;
            case DRIVE_REMOTE:
                imageId = GDC_drive;
                break;
            case DRIVE_CDROM:
                imageId = GDC_cdrom;
                break;
            default:
                imageId = GDC_drive;
                break;
        }

        AddSection(path, name, imageId);

        while (driveBuffer[i] != wxT('\0'))
            i ++;
        i ++;
        if (driveBuffer[i] == wxT('\0'))
            break;
    }
#else // !__WIN32__
    int drive;

    /* If we can switch to the drive, it exists. */
    for(drive = 1; drive <= 26; drive++)
    {
        wxString path, name;
        path.Printf(wxT("%c:\\"), (wxChar) (drive + 'a' - 1));
        name.Printf(wxT("(%c:)"), (wxChar) (drive + 'A' - 1));

        if (wxIsDriveAvailable(path))
        {
            AddSection(path, name, (drive <= 2) ? GDC_floppy : GDC_drive);
        }
    }
#endif // __WIN32__/!__WIN32__

#elif defined(__WXMAC__)
    FSSpec volume ;
    short index = 1 ;
    while(1) {
      short actualCount = 0 ;
      if (OnLine(&volume , 1 , &actualCount , &index) != noErr || actualCount == 0)
        break ;

      wxString name = wxMacFSSpec2MacFilename(&volume) ;
      AddSection(name + wxFILE_SEP_PATH, name, GDC_closedfolder);
    }
#elif defined(__UNIX__)

  if (fulltree)                         // //If we're displaying the whole directory tree, need to provide a "My Computer" equivalent
      AddSection(wxT("/"), wxT("/"), GDC_computer);
   else                                 // // Otherwise, if just displaying part of the disk, this plonks in the start dir & selects the appropriate icon
    { DeviceAndMountManager* pDM = MyFrame::mainframe->Layout->m_notebook->DeviceMan;
      DevicesStruct* ds = pDM->QueryAMountpoint(startdir);
      treerooticon roottype = ordinary;                       // Provide a bland default 
      if (ds != NULL) roottype = pDM->deviceman->DevInfo->GetIcon(ds->devicetype);
      switch(roottype)
        { case floppytype:    AddSection(startdir, startdir, GDC_floppy); break;
          case cdtype:        AddSection(startdir, startdir, GDC_cdrom); break;
          case usbtype:       AddSection(startdir, startdir, GDC_usb); break;
          case ordinary:      if (arcman && arcman->IsArchive())
                                { AddSection(startdir, startdir, Archive::GetIconForArchiveType(arcman->GetArchiveType(), false)); break; } // Archive. Otherwise fall thru to default
           default:           AddSection(startdir, startdir, GDC_openfolder); break;
        }
             
    }
#else
    #error "Unsupported platform in wxGenericDirCtrl!"
#endif
}

void MyGenericDirCtrl::OnBeginEditItem(wxTreeEvent &event)
{
    // don't rename the main entry "Sections"
    if (event.GetItem() == m_rootId)
    {
        event.Veto();
        return;
    }

    // don't rename the individual sections
    if (m_treeCtrl->GetItemParent(event.GetItem()) == m_rootId)
    {
        event.Veto();
        return;
    }
}

void MyGenericDirCtrl::OnEndEditItem(wxTreeEvent &event)
{
    if ((event.GetLabel().IsEmpty()) ||
        (event.GetLabel() == wxT(".")) ||
        (event.GetLabel() == wxT("..")) ||
        (event.GetLabel().First(wxT("/")) != wxNOT_FOUND))
    {
        wxMessageDialog dialog(this, _("Illegal directory name."), _("Error"), wxOK | wxICON_ERROR);
        dialog.ShowModal();
        event.Veto();
        return;
    }

    wxTreeItemId id = event.GetItem();
    wxDirItemData *data = (wxDirItemData*)m_treeCtrl->GetItemData(id);
    wxASSERT(data);

    wxString new_name(wxPathOnly(data->m_path));
    new_name += wxString(wxFILE_SEP_PATH);
    new_name += event.GetLabel();

    wxLogNull log;

    if (wxFileExists(new_name))
    {
        wxMessageDialog dialog(this, _("File name exists already."), _("Error"), wxOK | wxICON_ERROR);
        dialog.ShowModal();
        event.Veto();
    }

    if (wxRenameFile(data->m_path,new_name))
    {
        data->SetNewDirName(new_name);
    }
    else
    {
        wxMessageDialog dialog(this, _("Operation not permitted."), _("Error"), wxOK | wxICON_ERROR);
        dialog.ShowModal();
        event.Veto();
    }
}

void MyGenericDirCtrl::OnExpandItem(wxTreeEvent &event)
{
    wxTreeItemId parentId = event.GetItem();

    // VS: this is needed because the event handler is called from wxTreeCtrl
    //     ctor when wxTR_HIDE_ROOT was specified
    if (!m_rootId.IsOk())
        m_rootId = m_treeCtrl->GetRootItem();

    ExpandDir(parentId);
#if defined(__LINUX__) && defined(__WXGTK__)
    if (USE_FSWATCHER && m_watcher)
      // // We've changed the visible dirs, so need to update what's watched
      // // But not immediately in case it's recursive i.e. '*' on e.g. /usr/lib, which would take a significant time if repeated for each dir
      // // Also, don't try to watch an archive stream that's just being opened, or a subdir inside one. That'll give wxLogErrors in FileSystemWatcher::Add
      { // wxLogDebug(wxT("About to SetDirviewWatch from OnExpandItem()"));
        FileData fd(startdir);
        if (!fd.IsValid() || (!fd.IsDir() && !fd.IsSymlinktargetADir())) 
          return;
        
        if (fulltree)
              m_watcher->DoDelayedSetDirviewWatch(wxT("/"));
         else m_watcher->DoDelayedSetDirviewWatch(startdir);
      }
#endif
}

void MyGenericDirCtrl::OnCollapseItem(wxTreeEvent &event)
{
    CollapseDir(event.GetItem());
    //wxLogDebug(wxT("About to SetDirviewWatch from OnCollapseItem()"));
#if defined(__LINUX__) && defined(__WXGTK__)
    if (m_watcher)
      m_watcher->SetDirviewWatch(startdir); // // We've changed the visible dirs, so need to update what's watched
#endif
}

void MyGenericDirCtrl::CollapseDir(wxTreeItemId parentId)
{
    wxTreeItemId child;

    wxDirItemData *data = (wxDirItemData *) m_treeCtrl->GetItemData(parentId);
    if (!data || !data->m_isExpanded)
        return;

    data->m_isExpanded = false;
    wxTreeItemIdValue cookie;
    /* Workaround because DeleteChildren has disapeared (why?) and
     * CollapseAndReset doesn't work as advertised (deletes parent too) */
    child = m_treeCtrl->GetFirstChild(parentId, cookie);
    while (child.IsOk())
    {
        m_treeCtrl->Delete(child);
        /* Not GetNextChild below, because the cookie mechanism can't
         * handle disappearing children! */
        child = m_treeCtrl->GetFirstChild(parentId, cookie);
    }
}

void MyGenericDirCtrl::ExpandDir(wxTreeItemId parentId)
{
    wxDirItemData *data = (wxDirItemData *) m_treeCtrl->GetItemData(parentId);

    if (!data || data->m_isExpanded)
        return;

    data->m_isExpanded = true;
    
if (fileview == ISLEFT)                      // //  if we're supposed to be showing directories, rather than files
    if (parentId == m_treeCtrl->GetRootItem())
    {
        SetupSections();
        return;
    }

    wxASSERT(data);

    wxString search,path,filename;

    wxString dirName(data->m_path);

#if defined(__WINDOWS__) || defined(__DOS__) || defined(__WXPM__)
    // Check if this is a root directory and if so,
    // whether the drive is avaiable.
    if (!wxIsDriveAvailable(dirName))
    {
        data->m_isExpanded = false;
        //wxMessageBox(wxT("Sorry, this drive is not available."));
        return;
    }
#endif

    // This may take a longish time. Go to busy cursor
    wxBusyCursor busy;

#if defined(__WINDOWS__) || defined(__DOS__) || defined(__WXPM__)
    if (dirName.Last() == ':')
        dirName += wxString(wxFILE_SEP_PATH);
#endif

if (dirName.Last() != wxFILE_SEP_PATH)                    // // I'm speeding things up by taking this out of the loops below
        dirName += wxString(wxFILE_SEP_PATH);

    wxArrayString dirs;
    wxArrayString filenames;

 //   wxDir d;
    DirBase* d;                                          // // This is the base-class both for mywxDir and for ArcDir (for archives)

bool IsArchv = arcman && arcman->IsArchive();            // //
if (IsArchv)  d = new ArcDir(arcman->GetArc());          // //
  else  d = new mywxDir;  // //

    wxString eachFilename;

    wxLogNull log;
    d->Open(dirName);

FileGenericDirCtrl* FileCtrl;                                  // //  

if (fileview==ISRIGHT)       // // If this is a fileview, do things differently from normal:  use array of FileData* to store & sort the data
  { FileCtrl = (FileGenericDirCtrl*)this;
    FileCtrl->CombinedFileDataArray.Clear();                  // // Clear the array of FileData* (the place where the stat data will be stored)
    FileCtrl->FileDataArray.Clear();                          // //   & the temp one for files
    FileCtrl->CumFilesize = 0;                                // //     & this dir's cum filesize, to display in statusbar

    if (d->IsOpened())
    {
        int style = wxDIR_FILES;                              // //
        if (!DisplayFilesOnly)  style |= wxDIR_DIRS;          // //
        if (m_showHidden) style |= wxDIR_HIDDEN;              // //

     if (GetFilterArray().GetCount() < 2)                     // // If we're not using multiple filter strings, do things the standard way    
      { m_currentFilterStr = GetFilterArray().Item(0);        // // Find the sole filter-string (which may well be "")
        if (d->GetFirst(&eachFilename, m_currentFilterStr, style))  // // Since wxDir can't (currently) distinguish a dir from a symlink-to-dir, get every filetype at once
          { do
             { if ((eachFilename != wxT(".")) && (eachFilename != wxT("..")))
                { DataBase* stat=NULL;
                  if (IsArchv)  
                    { wxString path(dirName + eachFilename);
                      if (path.Right(1)==wxFILE_SEP_PATH)
                        { FakeDir* fd = arcman->GetArc()->Getffs()->GetRootDir()->FindSubdirByFilepath(eachFilename);
                          if (fd) stat = new FakeDir(*fd);
                        }
                        else 
                        { FakeFiledata* fd = arcman->GetArc()->Getffs()->GetRootDir()->FindFileByName(eachFilename);
                          if (fd) stat = new FakeFiledata(*fd);
                        }
                    }
                   else stat = new FileData(dirName + eachFilename);      // // Create a new FileData for the entry & add it to the appropriate array
                  if (stat->IsValid())
                    { if (stat->IsDir()                                   // // Is it a dir?
                                  || (TREAT_SYMLINKTODIR_AS_DIR && stat->IsSymlinktargetADir()))  // // or a symlink to one, & user wants to display it with dirs
                          FileCtrl->CombinedFileDataArray.Add(stat);      // // Yes, so put it in combined array
                        else FileCtrl->FileDataArray.Add(stat);           // // No, so put it in file array
                    }
                   else // Invalid, so presumably a corrupt item
                    { if (ReallyIsDir(dirName, eachFilename))
                        FileCtrl->CombinedFileDataArray.Add(stat); // // It's probably a corrupt dir
                       else
                        FileCtrl->FileDataArray.Add(stat);         // // It's not likely to be a dir
                    }
                }
             }
              while (d->GetNext(&eachFilename));
          }
      }

    else
      { if (d->GetFirst(&eachFilename, wxEmptyString, style))       // // Otherwise, get ALL the entries that match style, and do the filtering separately
          { do
             { if ((eachFilename != wxT(".")) && (eachFilename != wxT("..")))
                { bool matches=false;
                  for (size_t n=0; n < GetFilterArray().GetCount(); ++n)// // Go thru the list of filters, applying each in turn until one matches
                    if (wxMatchWild(GetFilterArray().Item(n), eachFilename))
                        { matches = true; break; }                  // // Got a match so flag & skip the rest of the for-loop
                    
                  if (matches)                                      // // If there was a match, add to the correct array.  Otherwise ignore this entry
                    { DataBase* stat=NULL;
                      if (IsArchv)  
                        { wxString path(dirName + eachFilename);
                          if (path.Right(1)==wxFILE_SEP_PATH)
                            { FakeDir* fd = arcman->GetArc()->Getffs()->GetRootDir()->FindSubdirByFilepath(eachFilename);
                              if (fd) stat = new FakeDir(*fd);
                            }
                            else 
                            { FakeFiledata* fd = arcman->GetArc()->Getffs()->GetRootDir()->FindFileByName(eachFilename);
                              if (fd) stat = new FakeFiledata(*fd);
                            }
                        }
                       else stat = new FileData(dirName + eachFilename);     // // Create a new FileData for the entry & add it to the appropriate array
                      if (stat->IsValid())
                        { if (stat->IsDir()                                  // // Is it a dir?
                                || (TREAT_SYMLINKTODIR_AS_DIR && stat->IsSymlinktargetADir()))  // // or a symlink to one, & user wants to display it with dirs
                            FileCtrl->CombinedFileDataArray.Add(stat);       // // Yes, so put it in combined array
                           else FileCtrl->FileDataArray.Add(stat);           // // No, so put it in file array
                        }
                       else // Invalid, so presumably a corrupt item
                        { if (ReallyIsDir(dirName, eachFilename))
                            FileCtrl->CombinedFileDataArray.Add(stat); // // It's probably a corrupt dir
                           else
                            FileCtrl->FileDataArray.Add(stat);         // // It's not likely to be a dir
                        }
                    }
                }
             }
              while (d->GetNext(&eachFilename));
          }
        }
  
  FileCtrl->SortStats(FileCtrl->CombinedFileDataArray);             // // Sort the dirs
  FileCtrl->NoOfDirs = FileCtrl->CombinedFileDataArray.GetCount();  // // Store the no of dirs
    
  FileCtrl->SortStats(FileCtrl->FileDataArray);                     // // Sort the files
  FileCtrl->NoOfFiles = FileCtrl->FileDataArray.GetCount();         // // Store the no of files
    
    // Add the sorted dirs
    size_t i;
    for (i = 0; i < FileCtrl->CombinedFileDataArray.GetCount(); ++i)
    {
      if (FileCtrl->CombinedFileDataArray[i].IsValid())  
        { wxString eachFilename(FileCtrl->CombinedFileDataArray[i].GetFilename());  // // Note that we take the filenames from the sorted FileDataArray
          path = dirName + eachFilename;
   // //       if (path.Last() != wxFILE_SEP_PATH)
   // //           path += wxString(wxFILE_SEP_PATH);
   // //       path += eachFilename;

          wxDirItemData *dir_item = new wxDirItemData(path,eachFilename,true);
          wxTreeItemId id;
          if (TREAT_SYMLINKTODIR_AS_DIR && FileCtrl->CombinedFileDataArray[i].IsSymlinktargetADir())    // //
            id =  m_treeCtrl->AppendItem(parentId, eachFilename, GDC_symlinktofolder, -1, dir_item);    // // Display as symlink-to-dir
           else
            { int imageindex = 0 + (GDC_ghostclosedfolder*IsArchv) + ((!IsArchv && access(path.mb_str(wxConvUTF8), R_OK))*GDC_lockedfolder);  // // access() checks if the dir is locked
              id = m_treeCtrl->AppendItem(parentId, eachFilename, imageindex, -1, dir_item);
            }
          m_treeCtrl->SetItemImage(id, IsArchv ? GDC_ghostopenfolder : GDC_openfolder, wxTreeItemIcon_Expanded);
        }
       else
        { wxString name(FileCtrl->CombinedFileDataArray[i].ReallyGetName());
          wxDirItemData *dir_item = new wxDirItemData(dirName + name,name,false);
          m_treeCtrl->AppendItem(parentId, name, GDC_unknownfolder, -1, dir_item); // // An invalid filedata, so presumably a corrupt dir
        }

        // Has this got any children? If so, make it expandable.
        // (There are two situations when a dir has children: either it
        // has subdirectories or it contains files that weren't filtered
        // out. The latter only applies to dirctrl with files.)
// //        if (dir_item->HasSubDirs() ||                          // // This isn't needed for a fileview
// //             (((GetWindowStyle() & wxDIRCTRL_DIR_ONLY) == 0) &&
// //               dir_item->HasFiles(m_currentFilterStr)))
// //        {
// //            m_treeCtrl->SetItemHasChildren(id);
// //        }
    }

    // Add the sorted filenames
   for (i = 0; i < FileCtrl->FileDataArray.GetCount(); i++)
    {
      if (FileCtrl->FileDataArray[i].IsValid())
        { FileCtrl->CumFilesize += FileCtrl->FileDataArray[i].Size();       // //  Grab the opportunity to update the cumulative files-size
        
            wxString eachFilename(FileCtrl->FileDataArray[i].GetFilename());// // Note that we take the filenames from the sorted FileDataArray
            path = dirName + eachFilename;

            wxDirItemData *dir_item = new wxDirItemData(path,eachFilename,false);
          if (FileCtrl->FileDataArray[i].IsRegularFile())                   // // If the item is a file
            { ziptype zt =  Archive::Categorise(path);
              if (zt != zt_invalid)                                         // // There are special icons for archives etc
                (void)m_treeCtrl->AppendItem(parentId, eachFilename, Archive::GetIconForArchiveType(zt, IsArchv), -1, dir_item);
               else (void)m_treeCtrl->AppendItem(parentId, eachFilename, IsArchv? GDC_ghostfile : GDC_file, -1, dir_item);  // // otherwise use the standard icon
             }
           else if (FileCtrl->FileDataArray[i].IsSymlink())                 // // If the item is a symlink
            { 
              if (FileCtrl->FileDataArray[i].IsSymlinktargetADir())         // //  If it's pointing to a dir, use the appropriate icon
                (void)m_treeCtrl->AppendItem(parentId, eachFilename, GDC_symlinktofolder, -1, dir_item);  // //
               else
                 { if (FileCtrl->FileDataArray[i].IsBrokenSymlink())
                     (void)m_treeCtrl->AppendItem(parentId, eachFilename, GDC_brokensymlink, -1, dir_item);
                    else
                     (void)m_treeCtrl->AppendItem(parentId, eachFilename, GDC_symlink, -1, dir_item);  // // If it's pointing to a file, use that icon
                 }
            }
          else if (FileCtrl->FileDataArray[i].IsFIFO())               // // FIFO
            (void)m_treeCtrl->AppendItem(parentId, eachFilename, GDC_pipe, -1, dir_item);
          else if (FileCtrl->FileDataArray[i].IsBlkDev())             // // Block device
            (void)m_treeCtrl->AppendItem(parentId, eachFilename, GDC_blockdevice, -1, dir_item);
          else if (FileCtrl->FileDataArray[i].IsCharDev())            // // Char device
            (void)m_treeCtrl->AppendItem(parentId, eachFilename, GDC_chardevice, -1, dir_item);
          else if (FileCtrl->FileDataArray[i].IsSocket())             // // Socket
            (void)m_treeCtrl->AppendItem(parentId, eachFilename, GDC_socket, -1, dir_item);

          else (void)m_treeCtrl->AppendItem(parentId, eachFilename, GDC_unknownfile, -1, dir_item); // // wtf is this?
        }
      else      // // An invalid filedata, so presumably a corrupt file (corrupt dirs were dealt with earlier)
        { wxString name(FileCtrl->FileDataArray[i].ReallyGetName());
          if (!ReallyIsDir(dirName, name))
            { wxDirItemData *dir_item = new wxDirItemData(path,eachFilename,false);
              (void)m_treeCtrl->AppendItem(parentId, name, GDC_unknownfile, -1, dir_item);
            }
        }
    }

    size_t count = FileCtrl->FileDataArray.Count();             // // Merge the 2 arrays, Dir <-- File
    for (size_t n = 0;  n < count; ++n)                         // // For count iterations,
        FileCtrl->CombinedFileDataArray.Add(FileCtrl->FileDataArray.Detach(0));  // // transfer array[0], as detaching shifts everything down
    }
  }
  
 else                              // // dirview, so do it the standard way
  { 
  if (d->IsOpened())
    {
        int style = wxDIR_DIRS;
        if (m_showHidden) style |= wxDIR_HIDDEN;
        
     if (GetFilterArray().GetCount() < 2)                       // // If we're not using multiple filter strings, do things the standard way    
      { m_currentFilterStr = GetFilterArray().Item(0);          // // Find the sole filter-string (which may well be "")
        if (d->GetFirst(&eachFilename, m_currentFilterStr, style))  // //
          { do
              { if ((eachFilename != wxT(".")) && (eachFilename != wxT("..")))
                  { DataBase* stat;
                    if (IsArchv)  
                      stat = new FakeDir(eachFilename);     // // 
                     else stat = new FileData(dirName + eachFilename);    // //
                    if (stat->IsValid())
                      { if (stat->IsDir())                    // // Ensure it IS a dir, & not a symlink-to-dir
                          dirs.Add(eachFilename);
                      }
                     else // Invalid, so presumably a corrupt item
                      { if (ReallyIsDir(dirName, eachFilename))
                          dirs.Add(eachFilename);             // // It's probably a corrupt dir
                      }
                    delete stat;
                  }
              }
              while (d->GetNext(&eachFilename));
          }
       }
    
     else
      { if (d->GetFirst(&eachFilename, wxEmptyString, style))     // // Otherwise, get ALL the entries that match style, and do the filtering separately
          { do
             { if ((eachFilename != wxT(".")) && (eachFilename != wxT("..")))
                { bool matches=false;
                  for (size_t n=0; n < GetFilterArray().GetCount(); ++n)  // // Go thru the list of filters, applying each in turn until one matches
                    if (wxMatchWild(GetFilterArray().Item(n), eachFilename))
                        { matches = true; break; }               // // Got a match so flag & skip the rest of the for-loop
                    
                  if (matches)                                   // // If there was a match, add to the correct array.  Otherwise ignore this entry
                    { DataBase* stat;
                      if (IsArchv)  
                        stat = new FakeDir(eachFilename);        // // 
                       else   stat = new FileData(dirName + eachFilename);  // //
                      if (stat->IsDir())                         // // Ensure it IS a dir, & not a symlink-to-dir
                            dirs.Add(eachFilename);
                      delete stat;
                    }
                }
             }
              while (d->GetNext(&eachFilename));
          }
        }         
     }
    
  dirs.Sort((wxArrayString::CompareFunction) (*wxDirCtrlStringCompareFunc));

   
    // Now do the filenames -- but only if we're allowed to
    if ((GetWindowStyle() & wxDIRCTRL_DIR_ONLY) == 0)
    {
        int style = wxDIR_FILES;                              // //
        if (m_showHidden) style |= wxDIR_HIDDEN;              // //

    wxLogNull log;

        d->Open(dirName);

        if (d->IsOpened())
        {
            if (d->GetFirst(& eachFilename, m_currentFilterStr, style))  // //
            {
                do
                {
                    if ((eachFilename != wxT(".")) && (eachFilename != wxT("..")))
                    {
                        filenames.Add(eachFilename);
                    }
                }
                while (d->GetNext(& eachFilename));
            }
        }
    dirs.Sort((wxArrayString::CompareFunction) (*wxDirCtrlStringCompareFunc));
    }

    // Add the sorted dirs
    size_t i;
    for (i = 0; i < dirs.Count(); i++)
    {
        wxString eachFilename(dirs[i]);
        if (IsArchv)
          { if (eachFilename.Last() == wxFILE_SEP_PATH)  eachFilename = eachFilename.BeforeLast(wxFILE_SEP_PATH);  // //
            path = eachFilename;                                      // // Archives are already Path.ed
             eachFilename = eachFilename.AfterLast(wxFILE_SEP_PATH);  // // and here we *don't* want the path
          }
         else 
          path = dirName + eachFilename;


        wxDirItemData *dir_item = new wxDirItemData(path,eachFilename,true);
        int imageindex = 0 + (GDC_ghostclosedfolder * IsArchv) + (!IsArchv && access(path.mb_str(wxConvUTF8), R_OK) * GDC_lockedfolder);
        wxTreeItemId id = m_treeCtrl->AppendItem(parentId, eachFilename, imageindex, -1, dir_item); // // access() checks if the dir is locked
        m_treeCtrl->SetItemImage(id, IsArchv? GDC_ghostopenfolder : GDC_openfolder, wxTreeItemIcon_Expanded);

        // Has this got any children? If so, make it expandable.
        // (There are two situations when a dir has children: either it
        // has subdirectories or it contains files that weren't filtered
        // out. The latter only applies to dirctrl with files.)
        if (dir_item->HasSubDirs() ||
        (IsArchv && arcman->GetArc()->Getffs()->GetRootDir()->HasSubDirs(path)) ||  // // If it's an archive, we have to use our own method
             (((GetWindowStyle() & wxDIRCTRL_DIR_ONLY) == 0) &&    // // I've left this in, but since this is a dir-view, it always is false
               dir_item->HasFiles(m_currentFilterStr)))                  // //   in which case this is never reached.  I've therefore not updated it for multiple filters
        {
            m_treeCtrl->SetItemHasChildren(id);
        }
    }

    // Add the sorted filenames                                  /// // It's a dirview so won't happen
    if ((GetWindowStyle() & wxDIRCTRL_DIR_ONLY) == 0)
    {
        for (i = 0; i < filenames.Count(); i++)
        {
            wxString eachFilename(filenames[i]);
            path = dirName;
 // //           if (path.Last() != wxFILE_SEP_PATH)
 // //               path += wxString(wxFILE_SEP_PATH);
            path += eachFilename;
            //path = dirName + wxString(wxT("/")) + eachFilename;
            wxDirItemData *dir_item = new wxDirItemData(path,eachFilename,false);
            (void)m_treeCtrl->AppendItem(parentId, eachFilename, GDC_file, -1, dir_item);
        }
    }
 }
 
delete d;
}

void MyGenericDirCtrl::ReCreateTree()
{
  const wxTreeItemId root = m_treeCtrl->GetRootItem();
  if (root.IsOk())
    ReCreateTreeBranch(&root);
}

void MyGenericDirCtrl::ReCreateTreeBranch(const wxTreeItemId* ItemToRefresh) // // Implements wxGDC::ReCreateTree, but with parameter
{
wxCHECK_RET(ItemToRefresh->IsOk(), wxT("Trying to refresh an invalid item"));

wxString topitem;
wxTreeItemId fv = GetTreeCtrl()->GetFirstVisibleItem(); // // Find the first visible item and cache its path
if (fv.IsOk())
  { wxDirItemData* data = (wxDirItemData*)GetTreeCtrl()->GetItemData(fv);
    if (data) topitem = data->m_path;
  }

Freeze();
    CollapseDir(*ItemToRefresh);
    ExpandDir(*ItemToRefresh);
Thaw();

if (!topitem.empty())                                   // // Now make a valiant effort to replicate the previous tree's appearance
  { fv = FindIdForPath(topitem);
    if (fv.IsOk())
      wxStaticCast(GetTreeCtrl(), MyTreeCtrl)->ScrollToOldPosition(fv);
  }
}


// Find the child that matches the first part of 'path'.
// E.g. if a child path is "/usr" and 'path' is "/usr/include"
// then the child for /usr is returned.
wxTreeItemId MyGenericDirCtrl::FindChild(wxTreeItemId parentId, const wxString& path, bool& done)
{
    wxString path2(path);

    // Make sure all separators are as per the current platform
    path2.Replace(wxT("\\"), wxString(wxFILE_SEP_PATH));
    path2.Replace(wxT("/"), wxString(wxFILE_SEP_PATH));

    // Append a separator to foil bogus substring matching
    path2 += wxString(wxFILE_SEP_PATH);

    // In MSW or PM, case is not significant
#if defined(__WINDOWS__) || defined(__DOS__) || defined(__WXPM__)
    path2.MakeLower();
#endif

    wxTreeItemIdValue cookie;
    wxTreeItemId childId = m_treeCtrl->GetFirstChild(parentId, cookie);
    while (childId.IsOk())
    {
        wxDirItemData* data = (wxDirItemData*) m_treeCtrl->GetItemData(childId);

        if (data && !data->m_path.IsEmpty())
        {
            wxString childPath(data->m_path);
            if (childPath.Last() != wxFILE_SEP_PATH)
                childPath += wxString(wxFILE_SEP_PATH);

            // In MSW and PM, case is not significant
#if defined(__WINDOWS__) || defined(__DOS__) || defined(__WXPM__)
            childPath.MakeLower();
#endif

            if (childPath.Len() <= path2.Len())
            {
                wxString path3 = path2.Mid(0, childPath.Len());
                if (childPath == path3)
                {
                    if (path3.Len() == path2.Len())
                        done = true;
                    else
                        done = false;
                    return childId;
                }
            }
        }

        childId = m_treeCtrl->GetNextChild(parentId, cookie);
    }
    wxTreeItemId invalid;
    return invalid;
}

// Try to expand as much of the given path as possible,
// and select the given tree item.
bool MyGenericDirCtrl::ExpandPath(const wxString& path)
{
 bool done = false;
    wxTreeItemId id = FindChild(m_rootId, path, done);
    wxTreeItemId lastId = id; // The last non-zero id
    while (id.IsOk() && !done)
    {
        ExpandDir(id);

        id = FindChild(id, path, done);
        if (id.IsOk())
            lastId = id;
    }
    if (lastId.IsOk())
    {
        wxDirItemData *data = (wxDirItemData *) m_treeCtrl->GetItemData(lastId);
        if (data->m_isDir)
        {
            m_treeCtrl->Expand(lastId);
        }
        if ((GetWindowStyle() & wxDIRCTRL_SELECT_FIRST) && data->m_isDir)
        {
            // Find the first file in this directory
            wxTreeItemIdValue cookie;
            wxTreeItemId childId = m_treeCtrl->GetFirstChild(lastId, cookie);
            bool selectedChild = false;
            while (childId.IsOk())
            {
                wxDirItemData* data = (wxDirItemData*) m_treeCtrl->GetItemData(childId);

                if (data && data->m_path != wxT("") && !data->m_isDir)
                {
                    m_treeCtrl->SelectItem(childId);
                    m_treeCtrl->EnsureVisible(childId);
                    selectedChild = true;
                    break;
                }
                childId = m_treeCtrl->GetNextChild(lastId, cookie);
            }
            if (!selectedChild)
            {
                m_treeCtrl->SelectItem(lastId);
                m_treeCtrl->EnsureVisible(lastId);
            }
        }
        else
        {
            m_treeCtrl->SelectItem(lastId);
            m_treeCtrl->EnsureVisible(lastId);
        }

        return true;
    }
    else
        return false;
}

wxString MyGenericDirCtrl::GetPath() const
{
#if wxVERSION_NUMBER < 2900
  wxTreeItemId id = m_treeCtrl->GetSelection();
#else  // From 2.9, an assert prevents the use of GetSelection in a multiple-selection treectrl
  wxTreeItemId id = m_treeCtrl->GetFocusedItem ();
#endif

if (id)
  { wxDirItemData* data = (wxDirItemData*) m_treeCtrl->GetItemData(id);
    return data->m_path;
  }
else
    return wxEmptyString;
}


size_t MyGenericDirCtrl::GetMultiplePaths(wxArrayString& paths) const // // Get an array of selected paths, used as multiple selection is enabled
{
size_t count;                          // No of selections 
wxArrayTreeItemIds selectedIds;        // Array in which to store them

count = m_treeCtrl->GetSelections(selectedIds);  // Make the tree fill the array
paths.Alloc(count);                              // and allocate that much space in the path array

for (size_t c=0; c < count; c++)
  {
    wxDirItemData* data = (wxDirItemData*) m_treeCtrl->GetItemData(selectedIds[c]);
    paths.Add(data->m_path);
  }

return count;
}



wxString MyGenericDirCtrl::GetFilePath() const
{
#if wxVERSION_NUMBER < 2900
  wxTreeItemId id = m_treeCtrl->GetSelection();
#else  // From 2.9, an assert prevents the use of GetSelection in a multiple-selection treectrl
  wxTreeItemId id = m_treeCtrl->GetFocusedItem ();
#endif
  if (id)
    {
        wxDirItemData* data = (wxDirItemData*) m_treeCtrl->GetItemData(id);
        if (data->m_isDir)
            return wxEmptyString;
        else
            return data->m_path;
    }
   else
        return wxEmptyString;
}

void MyGenericDirCtrl::SetPath(const wxString& path)
{
  wxCHECK_RET(!path.empty(), wxT("Trying to set an empty path"));
  m_defaultPath = path;
  if (!m_rootId) return;
  
  if (arcman && arcman->IsArchive())                       // // If we're in an archive situation
    if (!arcman->GetArc()->SetPath(path))                  // // Tell the archive about the new path. If that takes us out of the archive, clean up
      { do OnExitingArchive();
          while (arcman->IsArchive() && !arcman->GetArc()->SetPath(path));  // If we were inside a nested, child archive, retry
      }
  ExpandPath(path);

#if defined(__LINUX__) && defined(__WXGTK__)
  if (USE_FSWATCHER && m_watcher && (fileview == ISRIGHT) && !(arcman && arcman->IsArchive()))
    { FileData fd(path);
      if (fd.IsValid())
        { if (fd.IsDir() || fd.IsSymlinktargetADir())
            m_watcher->SetFileviewWatch(path);
           else
            m_watcher->SetFileviewWatch(fd.GetPath()); // This can happen when a file is selected, then F5 pressed
        }
    }
#endif
}


void MyGenericDirCtrl::DoResize()
{
    wxSize sz = GetClientSize();
    int verticalSpacing = 3;
    if (m_treeCtrl)
    {
        wxSize filterSz ;
        if (m_filterListCtrl)
        {
#ifdef __WXMSW__
            // For some reason, this is required in order for the
            // correct control height to always be returned, rather
            // than the drop-down list height which is sometimes returned.
            wxSize oldSize = m_filterListCtrl->GetSize();
            m_filterListCtrl->SetSize(-1, -1, oldSize.x+10, -1, wxSIZE_USE_EXISTING);
            m_filterListCtrl->SetSize(-1, -1, oldSize.x, -1, wxSIZE_USE_EXISTING);
#endif
            filterSz = m_filterListCtrl->GetSize();
            sz.y -= (filterSz.y + verticalSpacing);
        }
       if (fileview==ISLEFT)     m_treeCtrl->SetSize(0, 0, sz.x, sz.y);  // // A fileview will sort itself out
        if (m_filterListCtrl)
        {
            m_filterListCtrl->SetSize(0, sz.y + verticalSpacing, sz.x, filterSz.y);
            // Don't know why, but this needs refreshing after a resize (wxMSW)
            m_filterListCtrl->Refresh();
        }
    }
}


void MyGenericDirCtrl::OnSize(wxSizeEvent& WXUNUSED(event))
{
    DoResize();
}

void MyGenericDirCtrl::SetTreePrefs()      // // MINE.  Adjusts the Tree width parameters according to prefs
{
if (fileview==ISLEFT)                    // If this is a left-side, directory view, set the indent between tree branches, horiz twig spacing
  { m_treeCtrl->SetIndent(TREE_INDENT);
    m_treeCtrl->SetSpacing(TREE_SPACING);  
  }
 else
  { m_treeCtrl->SetIndent(0);             // If this is a right-side, file view, turn off left margin and the prepended horiz lines
    m_treeCtrl->SetSpacing(0);
  }
}

wxString MyGenericDirCtrl::GetActiveDirectory()      // // MINE.  Returns the root dir if in branch-mode, or the selected dir in Fulltree-mode
{
if (fileview==ISRIGHT) return partner->GetActiveDirectory();  // This is a dir-view thing

if (fulltree) return GetPath();
return startdir;
}

wxString MyGenericDirCtrl::GetActiveDirPath()      // // MINE.  Returns selected dir in  either mode  NB GetPath returns either dir OR file
{
wxString invalid;
if(!this) return invalid; // in case we arrived here when there aren't any tabs

if (fileview==ISRIGHT) return partner->GetActiveDirPath();  // This is a dir-view thing

return GetPath();
}

void MyGenericDirCtrl::OnTreeSelected(wxTreeEvent &event)    // //  MINE. When change of selected item, tells Fileview to mimic change
{
if (SelFlag == false) return;                        // To avoid responding to internal selection events, causing infinite looping
wxTreeItemId sel = event.GetItem();                  // There's been a selection event. Get the selected item
#if wxVERSION_NUMBER > 2603 && !(defined(__WXGTK__) || defined(__WXX11__))
GetTreeCtrl()->Refresh();                            // Recent wxGTK1.2's have a failure-to-highlight problem otherwise
#endif
if (sel.IsOk() && (partner != NULL))                 // Check there IS a valid selection, and a valid partner
  { wxDirItemData* data = (wxDirItemData*)m_treeCtrl->GetItemData(sel);      // Find the referenced data
    DisplaySelectionInTBTextCtrl(data->m_path);      // Take the opportunity to display selection in textctrl

    if (arcman && arcman->IsArchive() && fileview == ISLEFT)  // If we're in an archive situation, & it's a dirview: can't escape from an archive if fileview!
      { wxString filepath(data->m_path);
        if (!filepath.IsEmpty() && filepath.GetChar(filepath.Len()-1) != wxFILE_SEP_PATH) filepath << wxFILE_SEP_PATH;
        if (!arcman->GetArc()->SetPath(filepath)) // Tell the archive about the new path. If that takes us out of the archive, clean up
          { do OnExitingArchive();
              while (arcman->IsArchive() && !arcman->GetArc()->SetPath(filepath));  // If we were inside a nested, child archive, retry
          }
      }

    m_StatusbarInfoValid = false;                  // Flag to show the new selection's data in the statusbar
    if (fileview == ISRIGHT)                       // If this is a fileview
      { MyFrame::mainframe->Layout->OnChangeDir(data->m_path);  // Tell Layout to tell any extant terminal-emulator or commandline of the change
        wxArrayString filepaths; GetMultiplePaths(filepaths);
        CopyToClipboard(filepaths, true);          // FWIW, copy the filepath(s) to the primary selection too
        return;                                    // & bug out.  We set fileview from dirview, not vice versa.  (That comes from DClick)
      }
    partner->startdir = data->m_path;
    partner->ReCreateTreeFromSelection();          //  Tell Right partner to Redo itself with new selection 
    
    wxArrayString filepaths; GetMultiplePaths(filepaths);
    ((DirGenericDirCtrl*)this)->UpdateStatusbarInfo(filepaths);// Show the new selection's data in the statusbar
    MyFrame::mainframe->Layout->OnChangeDir(data->m_path);  // Tell Layout to tell any extant terminal-emulator or commandline of the change
  }
}

void MyGenericDirCtrl::ReCreateTreeFromSelection()  // //  MINE.  On selection/DClick event, this redoes the tree & reselects
{
SelFlag = false;                              // Turn off to avoid loops 

wxASSERT(GetTreeCtrl());

wxString topitem;
wxTreeItemId fv = GetTreeCtrl()->GetFirstVisibleItem(); // Find the first visible item and cache its path
if (fv.IsOk())
  { wxDirItemData* data = (wxDirItemData*)GetTreeCtrl()->GetItemData(fv);
    if (data) topitem = data->m_path;
  }

Freeze();
GetTreeCtrl()->DeleteAllItems();              // Uproot the old tree
CreateTree();                                 // and recreate from new root
Thaw();                                       // We must thaw before calling ScrollToOldPosition()

if (!topitem.empty())                         // Now make a valiant effort to replicate the previous tree's appearance
  { fv = FindIdForPath(topitem);
    if (fv.IsOk())
      wxStaticCast(GetTreeCtrl(), MyTreeCtrl)->ScrollToOldPosition(fv);
  }

#if defined(__LINUX__) && defined(__WXGTK__)
if (USE_FSWATCHER && m_watcher && !(arcman && arcman->IsArchive()))
  { if (fileview == ISRIGHT)
      { FileData fd(startdir);
        if (fd.IsDir() || fd.IsSymlinktargetADir())
          m_watcher->SetFileviewWatch(startdir);
      }
  }
#endif

SelFlag = true;                               // Reset flag
}

void MyGenericDirCtrl::OnTreeItemDClicked(wxTreeEvent &event)  // // What to do when a tree-item is double-clicked
{
wxTreeItemId sel = event.GetItem();           // There's been a dclick event. Get the selected item.
if (!sel.IsOk())  return;                     // Check there IS a valid selection 

wxDirItemData* data = (wxDirItemData*)m_treeCtrl->GetItemData(sel);      // Find the referenced data

ziptype zt = Archive::Categorise(data->m_path);    // Check if the item is (probably) an archive

if (arcman && arcman->IsArchive())                 // Check that we didn't just dclick inside an open archive
  { if (arcman->GetArc()->Getffs()->GetRootDir()->FindSubdirByFilepath(data->m_path) != NULL  // If it's a subdir,
                        ||  zt != zt_invalid)      //  or a child archive
      { OnOutsideSelect(data->m_path, false);      // If on a subdir just move within it; open a child archive
        return; 
      }
     else
      { ((FileGenericDirCtrl*)this)->OnOpen(event); return; }  // Otherwise it's a file, so pass to FileGenericDirCtrl::OnOpen
  }

FileData fd(data->m_path);
if (fd.IsRegularFile() || (fd.IsSymlink() && !fd.IsSymlinktargetADir()))  // Check if the DClick was on a file
  { if (zt == zt_invalid)                        // (We deal with archives differently)
      { ((FileGenericDirCtrl*)this)->OnOpen(event); return; }  // It's a file, so pass to FileGenericDirCtrl::OnOpen
  }

if (fileview==ISRIGHT)                           // If the dclick is from a Right pane,
  { if (fd.IsSymlinktargetADir())                // Do things differently if it's a symlink-to-dir
      { wxString ultdest(fd.GetUltimateDestination()); return OnOutsideSelect(ultdest); }

    if (zt != zt_invalid                        // Do things very differently if it's an archive
      && (fd.IsRegularFile() || (fd.IsSymlink() && !fd.IsSymlinktargetADir())))  // Just in case someone labelled a dir or symlink-to-dir with an archive-y ext!
          { OnOutsideSelect(data->m_path, false); return; }            // If so, peer into it

    if (partner != NULL)                        // Otherwise just get Left partner to do the work
      { partner->GetTreeCtrl()->UnselectAll();  // Remove the current selection, otherwise both become selected
        partner->SetPath(data->m_path);
      }

    return;
  }
                                // OK, it was a Left-pane dclick
if (fulltree)                                   // Are we in full-tree mode
  m_treeCtrl->Toggle(sel);                      // If so, we don't want to redo tree, just toggle expansion/collapse of this branch
 else                                           // We're NOT in full-tree mode
  { startdir = data->m_path;                    // Reset Startdir accordingly to new selection
    ((DirGenericDirCtrl*)ParentControl)->AddPreviouslyVisitedDir(startdir, GetPathIfRelevant());  // Store this root-dir for posterity  
    ReCreateTreeFromSelection();                //  Redo the tree with new selection as root
  }
if (partner != NULL)
    partner->ReCreateTreeFromSelection();       //  Tell right-pane partner what to do
} 


void MyGenericDirCtrl::OnOutsideSelect(wxString& path, bool CheckDevices/*=true*/)  // // Used to redo the control from outside, eg the floppy-button clicked, or internally after e.g. a DClick
{
wxString oldpath(path);
if (path.IsEmpty()) return;
if (fileview == ISRIGHT)                        // If this is a fileview pane, switch to partner to deal with it
  { partner->OnOutsideSelect(path, CheckDevices);
    SetPath(oldpath); return;                   // Select the originally passed filepath, using the carefully preserved string in case it was an archive (when 'path' will be altered)
  }

               // Assuming path isn't root, remove terminal '/'s. It doesn't matter otherwise, but if we're jumping into an archive, the '/' causes problems
while (path != wxT("/") && path.Last() == wxFILE_SEP_PATH) path = path.BeforeLast(wxFILE_SEP_PATH);
       
if (arcman->NavigateArchives(path) == Snafu)      // This should ensure that any archives are opened/closed as necessary
  { wxLogError(wxT("Sorry, that selection doesn't seem to exist")); return; }

ziptype zt = Archive::Categorise(path);
if (!arcman->IsArchive() && zt == zt_invalid)     // If we're not already in an archive, and not just about to 
   { if (!wxDirExists(path))
      { if (!wxFileExists(path))                  // If path is no longer valid, abort
          { wxLogError(wxT("Sorry, that selection doesn't seem to exist")); return; }
         else  path = path.BeforeLast(wxFILE_SEP_PATH) + wxFILE_SEP_PATH;    // If it's a file (& there's nothing to stop us bookmarking a file), revert to its path
      }
  }
  else if (arcman->IsArchive() && zt == zt_invalid && arcman->GetArc()->Getffs()->GetRootDir()->FindFileByName(path) != NULL)
        path = path.BeforeLast(wxFILE_SEP_PATH) + wxFILE_SEP_PATH;        // Again, if this is a bookmarked fakefile, amputate

 // Check that any attempt at archive peeking is likely to work, as failure messes up the dirview display
if (zt != zt_invalid && !ArchiveStream::IsStreamable(zt))
  { switch(zt)
      { case zt_gzip: case zt_bzip: case zt_lzma: case zt_xz: case zt_lzop: case zt_compress:
                    wxMessageBox(_("This file is compressed, but it's not an archive so you can't peek inside."), _("Sorry")); return;
         default:   wxMessageBox(_("I'm afraid I can't peek inside that sort of archive."), _("Sorry")); return;
      }
  }

wxString OriginalSelection = GetPathIfRelevant();
startdir = path;                                 // Reset Startdir accordingly to new selection
RefreshTree(path, CheckDevices);                 //  so that Refresh() actually changes tree
((DirGenericDirCtrl*)ParentControl)->AddPreviouslyVisitedDir(startdir,OriginalSelection);  // Store this root-dir for posterity
if (fulltree)
  { GetTreeCtrl()->UnselectAll();                // Unselect any current items first, else we'll have an ambiguous multiple selection
    SetPath(oldpath);                            // Then select the real one; it's not selected otherwise
  }
}

void MyGenericDirCtrl::OnRefreshTree(wxCommandEvent& event)      // // From Context menu or F5
{
RefreshTree(startdir);                                // Needed to provide the parameter for RefreshTree
}

void MyGenericDirCtrl::RefreshTree(const wxString& path, bool CheckDevices, bool RetainSelection)  // // Refreshes the display to show any outside changes (or following a change of rootitem via OnOutsideSelect)
{
const wxString selected = GetPath();                    // Retrieve the current selection for use below
  // We need to see if startdir was also selected (as part of a multiple selection)
  // If so, we don't want to deselect it later
bool StartdirWasSelected = false;
if (startdir != selected)
  { wxTreeItemId item = FindIdForPath(startdir);
    if (item.IsOk())   StartdirWasSelected = GetTreeCtrl()->IsSelected(item);
  }

if (fileview == ISRIGHT)                                // If this is a fileview pane, switch to partner to deal with it
  { partner->RefreshTree(path, CheckDevices);
    if (!selected.empty())  SetPath(selected);          // Reinstate original selection. This also ensures it's visible
    return;
  }
if (CheckDevices)                                       // No point doing this if we've only just done it in calling method --- thus the bool
  { DevicesStruct* ds = MyFrame::mainframe->Layout->m_notebook->DeviceMan->QueryAMountpoint(path);  // Look up whether this is a relevant device
    if (ds) MyFrame::mainframe->Layout->m_notebook->DeviceMan->Mount(ds->buttonnumber);             // If so, (re)Mount it
  }

wxString CopyOfPath(path);                              // When within an archive, valgrind complains about invalid reads below if I use 'path' directly
ReCreateTreeFromSelection();                            // Redo the tree (with any new selection as root)
if (!selected.empty() && selected != startdir // If we're supposed to, and selection isn't rootitem, 
    &&  (selected.StartsWith(startdir) || fulltree))    // and is still a child of startdir (which may have just changed), or we're in fulltree mode
  { SetPath(selected);                                  //  reinstate original selection, to preserve (as much as poss) the original pattern of dir-expansion
    if (RetainSelection && !StartdirWasSelected)        // Unless startdir was originally selected...
      { wxTreeItemId item = FindIdForPath(startdir);    // recreating the tree automatically selects startdir too
        if (item.IsOk()) GetTreeCtrl()->SelectItem(item, false); // This should deselect it
      }
    if (!RetainSelection)
      { wxTreeItemId item = FindIdForPath(selected);    // Similarly, if we're not retaining the old path, deselect it
        if (item.IsOk()) GetTreeCtrl()->SelectItem(item, false);
      }
  }

if (MyFrame::mainframe->GetActivePane())
  MyFrame::mainframe->GetActivePane()->DisplaySelectionInTBTextCtrl(CopyOfPath);  //  Otherwise put in path
MyFrame::mainframe->Layout->OnChangeDir(CopyOfPath);    // Tell Layout to tell any extant terminal-emulator or commandline of the change
((DirGenericDirCtrl*)ParentControl)->UpdateStatusbarInfo(CopyOfPath);

if (partner)                                            // Keep right-pane partner in synch
 { if (RetainSelection)
    { partner->startdir = GetPath();                    // This caters both for new dirview startdirs, and startdir != selected above
      partner->ReCreateTreeFromSelection();
    }
   else
    { partner->startdir = startdir;                     // If we're using the navigation tools, the fileview should follow the dirview's startdir
      partner->ReCreateTreeFromSelection();
    }
 }
}

MyGenericDirCtrl* MyGenericDirCtrl::GetOppositeDirview(bool OnlyIfUnsplit /*=true*/)  // Return a pointer to the dirview of the twin-pane opposite this one
{
if (OnlyIfUnsplit && (ParentTab->GetSplitStatus() == unsplit)) return NULL;  // OTOH if we're not split, don't unless we really want to

if (isright)                                              // A MyGenericDirCtrl knows its own parity, so use this to locate the opposite one
  return ParentTab->LeftorTopDirCtrl;
 else
  return  ParentTab->RightorBottomDirCtrl;
}


MyGenericDirCtrl* MyGenericDirCtrl::GetOppositeEquivalentPane()  // Return a pointer to the equivalent view of the twin-pane opposite this one
{
MyGenericDirCtrl* oppositedv = GetOppositeDirview();
if (!oppositedv) return NULL;

return (fileview == ISLEFT) ? oppositedv : oppositedv->partner;
}

void MyGenericDirCtrl::SetFocusViaKeyboard(wxCommandEvent& event)
{
MyGenericDirCtrl* fv;
if (event.GetId() == SHCUT_SWITCH_FOCUS_OPPOSITE)
  { MyGenericDirCtrl* dv = GetOppositeDirview();
    if (!dv) return;
    if (fileview == ISLEFT)
      { dv->GetTreeCtrl()->SetFocus(); return; }                  // Do the easy one first; dirviews don't have selection issues

    fv = dv->partner;
  }
 else //SHCUT_SWITCH_FOCUS_ADJACENT
  { if (fileview == ISRIGHT)
      { partner->GetTreeCtrl()->SetFocus(); return; }             // As above
    
    fv = partner;
  }

                    // If we're here, the focus destination is a fileview
if (fv->GetPath().empty())                                        // If there was no highlit item, we need to set one. SetFocus() alone doesn't do this
  { FileGenericDirCtrl* fgdc = wxStaticCast(fv, FileGenericDirCtrl);
    if (fgdc) fgdc->SelectFirstItem();
  }
    
fv->GetTreeCtrl()->SetFocus();
}

void MyGenericDirCtrl::RefreshVisiblePanes(wxFont* font/*=NULL*/)  // Called e.g. after a change of font
{
if (font) { GetTreeCtrl()->SetFont(*font); partner->GetTreeCtrl()->SetFont(*font); }

wxColour backgroundcolDV = COLOUR_PANE_BACKGROUND ? BACKGROUND_COLOUR_DV : GetSaneColour(GetTreeCtrl(), true, wxSYS_COLOUR_WINDOW);
wxColour backgroundcolFV = COLOUR_PANE_BACKGROUND ? BACKGROUND_COLOUR_FV : GetSaneColour(GetTreeCtrl(), true, wxSYS_COLOUR_WINDOW);
if (SINGLE_BACKGROUND_COLOUR) backgroundcolFV = backgroundcolDV;

if (fileview==ISLEFT) 
  { GetTreeCtrl()->SetBackgroundColour(backgroundcolDV);
    partner->GetTreeCtrl()->SetBackgroundColour(backgroundcolFV);
  }
 else
  { GetTreeCtrl()->SetBackgroundColour(backgroundcolFV);
    partner->GetTreeCtrl()->SetBackgroundColour(backgroundcolDV);
  }

if (fileview==ISLEFT) 
   { if (SHOW_TREE_LINES) GetTreeCtrl()->SetWindowStyle(GetTreeCtrl()->GetWindowStyle() & ~wxTR_NO_LINES);
      else  GetTreeCtrl()->SetWindowStyle(GetTreeCtrl()->GetWindowStyle() | wxTR_NO_LINES);
  }
 else 
  { if (SHOW_TREE_LINES) partner->GetTreeCtrl()->SetWindowStyle(partner->GetTreeCtrl()->GetWindowStyle() & ~wxTR_NO_LINES);
      else  partner->GetTreeCtrl()->SetWindowStyle(partner->GetTreeCtrl()->GetWindowStyle() | wxTR_NO_LINES);
  }
RefreshTree(startdir, false); partner->RefreshTree(startdir, false);  // Refresh each of these 2 panes
if (ParentTab->GetSplitStatus() == unsplit) return;         // Only 1 visible twin-pane, so we're done

MyGenericDirCtrl* oppositepane = GetOppositeDirview();      // Otherwise switch to the opposite split and repeat
if (oppositepane ==  NULL) return;
if (font) { oppositepane->GetTreeCtrl()->SetFont(*font); oppositepane->partner->GetTreeCtrl()->SetFont(*font); }
oppositepane->GetTreeCtrl()->SetBackgroundColour(backgroundcolDV);
oppositepane->partner->GetTreeCtrl()->SetBackgroundColour(backgroundcolFV);
if (SHOW_TREE_LINES) oppositepane->GetTreeCtrl()->SetWindowStyle(oppositepane->GetTreeCtrl()->GetWindowStyle() & ~wxTR_NO_LINES);
  else  oppositepane->GetTreeCtrl()->SetWindowStyle(oppositepane->GetTreeCtrl()->GetWindowStyle() | wxTR_NO_LINES);

oppositepane->RefreshTree(oppositepane->startdir, false); oppositepane->partner->RefreshTree(oppositepane->startdir, false);
}

void MyGenericDirCtrl::ReloadDirviewToolbars()  // Called after a change of user-defined tools
{
if (fileview == ISLEFT) ((DirGenericDirCtrl*)this)->CreateToolbar();  // Reload the dirview TB for  this pane
 else ((DirGenericDirCtrl*)partner)->CreateToolbar();
if (ParentTab->GetSplitStatus() == unsplit) return;       // Only 1 visible twin-pane, so we're done

MyGenericDirCtrl* oppositepane = GetOppositeDirview();    // Otherwise switch to the opposite split and repeat
if (oppositepane) ((DirGenericDirCtrl*)oppositepane)->CreateToolbar();  // Reload the dirview TB for  this pane
}

void MyGenericDirCtrl::OnReplicate(wxCommandEvent& event) // Duplicates this pane's filepath in the opposite pane
{
if (fileview == ISRIGHT)                                  // If this is a fileview pane, switch to partner to deal with it
  return partner->OnReplicate(event);

MyGenericDirCtrl* oppositepane = GetOppositeDirview();
if (oppositepane == NULL) return;
FileGenericDirCtrl* Fileview = dynamic_cast<FileGenericDirCtrl*>(partner);
wxCHECK_RET(Fileview, wxT("A partner that isn't a FileGenericDirCtrl"));
FileGenericDirCtrl* oppFileview = dynamic_cast<FileGenericDirCtrl*>(oppositepane->partner);
wxCHECK_RET(oppFileview, wxT("A partner that isn't a FileGenericDirCtrl"));

wxString OriginalSelection = oppositepane->GetPathIfRelevant();

oppositepane->fulltree = fulltree;                        // Now we can duplicate ourselves, fulltree-status and path
oppositepane->startdir = startdir;
oppositepane->SetFilterArray(GetFilterArray());
oppositepane->ReCreateTreeFromSelection();
oppositepane->SetPath(GetPath());
if (!oppositepane->fulltree)                              // If opposite pane is not in fulltree mode store its startdir for possible returning
  ((DirGenericDirCtrl*)oppositepane)->AddPreviouslyVisitedDir(oppositepane->startdir, OriginalSelection);

if (oppFileview)                                          //  Keep right-pane partner in synch
  { oppFileview->startdir = GetPath();
    oppFileview->GetHeaderWindow()->SetSelectedColumn(Fileview->GetHeaderWindow()->GetSelectedColumn());
    oppFileview->GetHeaderWindow()->SetSortOrder(Fileview->GetHeaderWindow()->GetSortOrder());
    oppFileview->GetHeaderWindow()->SetSelectedColumnImage(Fileview->GetHeaderWindow()->GetSortOrder());
    oppFileview->SetIsDecimalSort(Fileview->GetIsDecimalSort());
    oppFileview->SetFilterArray(partner->GetFilterArray(), partner->DisplayFilesOnly);
    oppFileview->ReCreateTreeFromSelection();
  }
}

void MyGenericDirCtrl::OnSwapPanes(wxCommandEvent& event) // Swaps this pane's filepath etc with the opposite pane
{
if (fileview == ISRIGHT)                                  // If this is a fileview pane, switch to partner to deal with it
  return partner->OnSwapPanes(event);

MyGenericDirCtrl* oppositepane = GetOppositeDirview(false); // Passing false says we really want the answer, even if the tab is unsplit. This lets swapping switch to the hidden twin-pane
if (oppositepane == NULL) return;
FileGenericDirCtrl* Fileview = dynamic_cast<FileGenericDirCtrl*>(partner);
wxCHECK_RET(Fileview, wxT("A partner that isn't a FileGenericDirCtrl"));
FileGenericDirCtrl* oppFileview = dynamic_cast<FileGenericDirCtrl*>(oppositepane->partner);
wxCHECK_RET(oppFileview, wxT("A partner that isn't a FileGenericDirCtrl"));

bool oppfulltree = oppositepane->fulltree;                // Store the opposite pane's current settings
bool oppFilesOnly=false;
wxArrayString opppartnerfilter;
wxArrayString oppfilter = oppositepane->GetFilterArray();
wxString oppstartdir = oppositepane->startdir;
wxString opppath = oppositepane->GetPath();
int opppartnerSelectedCol(0);
bool opppartnerReversesort(false);
bool opppartnerDecimalsort(false);

wxString OriginalSelection = GetPathIfRelevant();
wxString OppOriginalSelection = oppositepane->GetPathIfRelevant();

oppositepane->fulltree = fulltree;                       // Now we duplicate this pane into opposite, fulltree-status and path
oppositepane->SetFilterArray(GetFilterArray());
oppositepane->startdir = startdir;
oppositepane->ReCreateTreeFromSelection();
oppositepane->SetPath(GetPath());
if (!oppositepane->fulltree)                             // If opposite pane is not in fulltree mode store its startdir for possible returning
  ((DirGenericDirCtrl*)oppositepane)->AddPreviouslyVisitedDir(oppositepane->startdir, OppOriginalSelection);

if (oppFileview)                                         // Keep right-pane partner in synch
  { oppFilesOnly = oppFileview->DisplayFilesOnly;
    opppartnerfilter = oppFileview->GetFilterArray();
    opppartnerSelectedCol = oppFileview->GetHeaderWindow()->GetSelectedColumn();
    opppartnerReversesort = oppFileview->GetHeaderWindow()->GetSortOrder();
    opppartnerDecimalsort = oppFileview->GetIsDecimalSort();

    oppFileview->startdir = GetPath();
    oppFileview->GetHeaderWindow()->SetSelectedColumn(Fileview->GetHeaderWindow()->GetSelectedColumn());
    oppFileview->GetHeaderWindow()->SetSortOrder(Fileview->GetHeaderWindow()->GetSortOrder());
    oppFileview->GetHeaderWindow()->SetSelectedColumnImage(Fileview->GetHeaderWindow()->GetSortOrder());
    oppFileview->SetIsDecimalSort(Fileview->GetIsDecimalSort());
    oppFileview->SetFilterArray(partner->GetFilterArray(), partner->DisplayFilesOnly);
    oppFileview->ReCreateTreeFromSelection();
  }
  
fulltree = oppfulltree;                                  // Use stores to replicate opposite's settings here
startdir = oppstartdir;
SetFilterArray(oppfilter);
ReCreateTreeFromSelection();
SetPath(opppath);
if (!fulltree)                                           // If not in fulltree mode store startdir for possible returning
  ((DirGenericDirCtrl*)ParentControl)->AddPreviouslyVisitedDir(startdir, OriginalSelection);

if (partner)                                             // Keep right-pane partner in synch
  { partner->startdir = opppath;
    partner->SetFilterArray(opppartnerfilter, oppFilesOnly);
    Fileview->GetHeaderWindow()->SetSelectedColumn(opppartnerSelectedCol);
    Fileview->GetHeaderWindow()->SetSortOrder(opppartnerReversesort);
    Fileview->GetHeaderWindow()->SetSelectedColumnImage(opppartnerReversesort);
    Fileview->SetIsDecimalSort(opppartnerDecimalsort);
    partner->ReCreateTreeFromSelection();
  }

wxYieldIfNeeded();                                       // This is needed to give time for an UpdateUI event, to ensure the toolbar appearance is correct
}

void MyGenericDirCtrl::DisplaySelectionInTBTextCtrl(const wxString& selection)
{
  // In >=wx2.9 we need to post an event to create a delay; otherwise the old textctrl value is sometimes not cleared
  // and it does no harm in 2.8...
if (MyFrame::mainframe && MyFrame::mainframe->SelectionAvailable && MyFrame::mainframe->m_tbText != NULL)
  { wxNotifyEvent event(MyUpdateToolbartextEvent);  
    event.SetString(selection);
    wxPostEvent(this, event);
  }
}

void MyGenericDirCtrl::DoDisplaySelectionInTBTextCtrl(wxNotifyEvent& event) // Change the value in the toolbar textctrl to match the tree selection
{
if (MyFrame::mainframe && MyFrame::mainframe->SelectionAvailable && MyFrame::mainframe->m_tbText != NULL)
    MyFrame::mainframe->m_tbText->ChangeValue(event.GetString());
}

bool MyGenericDirCtrl::OnExitingArchive()  // // We were displaying the innards of an archive, and now we're not
{
if (arcman)  return arcman->RemoveArc();             // This should remove 1 layer of archive, or the sole archive, or do nothing. False means no longer within any archive
  else return false;
}

void MyGenericDirCtrl::CopyToClipboard(const wxArrayString& filepaths, bool primary/*=false*/) const
{
if (filepaths.empty()) return;
wxString selection = filepaths.Item(0);             // We just checked that this exists
for (size_t n=1; n < filepaths.GetCount(); ++n)     // Add any further items, separated by \n
  selection << wxT('\n') << filepaths.Item(n);

if (primary)
  { // In >=wx2.9 it's possible to add the path(s) to the primary selection
#if defined(__WXGTK__) && wxCHECK_VERSION(2,9,0)
		wxTheClipboard->UsePrimarySelection(true);
		if (wxTheClipboard->Open())
      { wxTheClipboard->SetData(new wxTextDataObject(selection));
        wxTheClipboard->Close();
      }
		wxTheClipboard->UsePrimarySelection(false);
#endif
  }
 else
  { if (wxTheClipboard->Open())
      { wxTheClipboard->SetData(new wxTextDataObject(selection));
        wxTheClipboard->Close();
      }
  }
}

wxString MyGenericDirCtrl::GetPathIfRelevant() // Helper function used to get selection to pass to NavigationManager
{
wxString selection, path = GetPath();
if (!path.empty())
  { FileData stat(path);
    if (stat.IsValid())
      { if (!(stat.IsDir() || stat.IsSymlinktargetADir())) selection = stat.GetPath();
          else selection = stat.GetFilepath();
      }
  }

return selection;
}

#if defined(__LINUX__) && defined(__WXGTK__)
void MyGenericDirCtrl::OnFileWatcherEvent(wxFileSystemWatcherEvent& event)
{
try 
  { 
    wxString filepath = event.GetPath().GetFullPath();
    wxString newfilepath = event.GetNewPath().GetFullPath();  // For renames

    switch(event.GetChangeType())
      { case wxFSW_EVENT_CREATE:
              m_eventmanager.AddEvent(event);
              break;
        case wxFSW_EVENT_MODIFY:
              m_eventmanager.AddEvent(event);
              break;
        case wxFSW_EVENT_ATTRIB:
              m_eventmanager.AddEvent(event);
              break;
        case wxFSW_EVENT_RENAME:
              m_eventmanager.AddEvent(event);
              break;
        case wxFSW_EVENT_DELETE:  /*IN_DELETE, IN_DELETE_SELF or IN_MOVE_SELF (both in & out)*/
              m_eventmanager.AddEvent(event);
              break;
        case wxFSW_EVENT_UNMOUNT:
              m_watcher->RemoveWatchLineage(filepath);
              m_eventmanager.AddEvent(event);
              break;
        case wxFSW_EVENT_WARNING: 
              if (event.GetWarningType() == wxFSW_WARNING_OVERFLOW)
                { WatchOverflowTimer* otimer = m_watcher->GetOverflowTimer();
                  if (!otimer->IsRunning()) // We'll probably get more warnings after we've already done this
                    { wxString basepath = m_watcher->GetWatchedBasepath(); // filepath will be "" for overflow warnings
                      m_watcher->GetWatcher()->RemoveAll(); // Turn off the current watch, to give inotify a chance to catch up with itself
                      m_eventmanager.ClearStoredEvents();
                      otimer->SetFilepath(basepath); otimer->SetType(fileview == ISLEFT);
                      //wxLogDebug("Starting the %s overflow timer", basepath);
                      otimer->Start(500, wxTIMER_ONE_SHOT);
                    }
                }
               else // Report warnings, but not if they're wxFSW_WARNING_OVERFLOW, which is 1) not interesting, and 2) very, very multiple
                { wxLogDebug(wxT("FSW WARNING: winID:%i:  Type:%i %s"), GetId(), event.GetWarningType(), event.GetErrorDescription().c_str()); } 
      }
  }
catch(...)
  { wxLogWarning(wxT("Caught an unknown exception in MyGenericDirCtrl::OnFileWatcherEvent")); }
}


MyFSWatcher::MyFSWatcher(wxWindow* owner/* = NULL*/) : m_owner(owner)
{
m_fswatcher = NULL;
m_DelayedDirwatchTimer = new DirviewWatchTimer(this);
m_OverflowTimer = new WatchOverflowTimer(this);
}

MyFSWatcher::~MyFSWatcher()
{
delete m_fswatcher;
delete m_DelayedDirwatchTimer; 
delete m_OverflowTimer; 
}

void MyFSWatcher::SetDirviewWatch(const wxString& path)
{
try 
  { 
    if (!CreateWatcherIfNecessary()) return;

    m_fswatcher->RemoveAll();

    if (path.empty())
      return;  // as things have probably gone pear-shaped

    DirGenericDirCtrl* owner = wxDynamicCast(m_owner, DirGenericDirCtrl);
    wxCHECK_RET(owner, wxT("m_owner is not a DirGenericDirCtrl"));

    wxTreeItemId rootId = owner->GetTreeCtrl()->GetRootItem();
    if (!rootId.IsOk()) return;

    wxArrayString dirs;                           // Find the visible dirs and watch them
    owner->GetVisibleDirs(rootId, dirs);

    wxLogNull NoLogWarningsAboutWrongPaths;

    bool need_startdir = true;
    for (size_t n=0; n < dirs.GetCount(); ++n)
      { m_fswatcher->Add(StrWithSep(dirs.Item(n)));
        // Usually 'path' is found by GetVisibleDirs(); but not always...
        if (StripSep(dirs.Item(n)) == StripSep(path)) need_startdir = false;
      }
    if (need_startdir) m_fswatcher->Add(StrWithSep(path));

    if (!owner->fulltree)                         // If fulltree, parents are already visible and so are auto-added
      { wxString parent(path);                    // Otherwise watch the hidden rootdir too, and all its ancestors; they can't be seen, but what if one were renamed...
        if (parent != wxT("/"))                   // Unless 'path' is actually '/', in which case 1) it doesn't have a parent, and 2) it's already added
         while (true)
          { wxFileName fn(parent); parent = fn.GetPath(); // GetVisibleDirs() included the original 'path', so start the loop with a GetPath()
            if (parent == wxT("/")) break;
            m_fswatcher->Add(StrWithSep(parent), wxFSW_EVENT_RENAME);
          }
      }

    m_WatchedBasepath = path; // Needed if we need to reset due to an inotify stack overflow
  }

catch(...)
  { wxLogWarning(wxT("Caught an unknown exception in MyFSWatcher::SetDirviewWatch")); }
}

void MyFSWatcher::SetFileviewWatch(const wxString& path)
{
wxCHECK_RET(!path.empty(), wxT("Trying to watch an empty path"));
try 
  { 
    if (!CreateWatcherIfNecessary()) return;
    if (StripSep(path) == m_WatchedBasepath)  // This can happen with e.g. SetPath(). It's not only pointless, but can result in lost events during the change
      { if (m_fswatcher->GetWatchedPathsCount()) // There are ways (involving UnDo) that result in a blank, unwatched fileview, even though startdir is set (idk how to make it not-set)
          return;
      }

    FileData* fd = new FileData(path);
    if (!fd->IsValid())
      { delete fd; // This happened on some boxes when a usbpen was unmounted, as a race condition which I hope I've solved elsewhere in this commit. But just in case:
        fd = new FileData(wxGetHomeDir());
      }
    
    wxASSERT(fd->IsDir() || fd->IsSymlinktargetADir()); // Keep this code for a while, in case there're more ways to arrive here with 'path' holding a file

    m_fswatcher->RemoveAll();

    wxLogNull NoLogWarningsAboutWrongPaths;
    m_fswatcher->Add(StrWithSep(fd->GetFilepath()));
    m_WatchedBasepath = StripSep(fd->GetFilepath());
    //PrintWatches(wxString(wxT("SetFileviewWatch() end")));
    delete fd;
  }
catch(...)
  { wxLogWarning(wxT("Caught an unknown exception in MyFSWatcher::SetFileviewWatch")); }
}

void MyFSWatcher::RefreshWatch(const wxString& filepath1 /*=wxT("")*/, const wxString& filepath2 /*=wxT("")*/) // Updates after an fsw rename event
{
try 
  { 
    if (!CreateWatcherIfNecessary()) return;
    if (!filepath1.empty() && !filepath2.empty())     // A rename
      { RemoveWatchLineage(StripSep(filepath1));      // Remove the now-defunct lineage of watches
        AddWatchLineage(StripSep(filepath2));         // and add the new dir and any visible descendant dirs
      }
  }
catch(...)
  { wxLogWarning(wxT("Caught an unknown exception in the 'rename' MyFSWatcher::RefreshWatch")); }
}

void MyFSWatcher::RefreshWatchDirCreate(const wxString& filepath) // Updates after an fsw create (dir) event
{
wxCHECK_RET(!filepath.empty(), wxT("Passed an empty filepath"));

try 
  { 
    if (!CreateWatcherIfNecessary()) return;

    wxArrayString CurrentWatches;
    m_fswatcher->GetWatchedPaths(&CurrentWatches);

    if ((CurrentWatches.Index(filepath) != wxNOT_FOUND) || (CurrentWatches.Index(StrWithSep(filepath)) != wxNOT_FOUND))
      { wxLogTrace(wxT("myfswatcher"), wxT("Filepath %s, allegedly newly created, was already watched in RefreshWatch()"), filepath.c_str()); return; }

    AddWatchLineage(StripSep(filepath)); // Add the new dir and any visible descendant dirs
  }
catch(...)
  { wxLogWarning(wxT("Caught an unknown exception in MyFSWatcher::RefreshWatchDirCreate")); }
}

void MyFSWatcher::AddWatchLineage(const wxString& filepath)  // Add a watch for filepath and all visible descendant dirs
{
DirGenericDirCtrl* owner = wxDynamicCast(m_owner, DirGenericDirCtrl);
wxCHECK_RET(owner && owner->fileview == ISLEFT, wxT("m_owner is not a DirGenericDirCtrl"));

try 
  { 
    wxArrayString dirs;                           // If called from a dirview, find the visible dirs to be watched
    wxTreeItemId treeId;
    if (owner)
      { treeId = owner->FindIdForPath(filepath);
        if (treeId.IsOk())
          owner->GetVisibleDirs(treeId, dirs);
      }

    //PrintWatches(wxString(wxT("AddWatchLineage() start. filepath=")) + filepath);
    wxLogNull NoLogWarningsAboutWrongPaths;
    bool need_startdir = true;
    for (size_t n=0; n < dirs.GetCount(); ++n)
      { m_fswatcher->Add(StrWithSep(dirs.Item(n)));
        // Usually 'filepath' is found by GetVisibleDirs(); but not always...
        if (StripSep(dirs.Item(n)) == StripSep(filepath)) need_startdir = false;
      }
    if (need_startdir) 
      m_fswatcher->Add(StrWithSep(filepath));

    if (!owner->fulltree)               // If fulltree, parents are already visible and so are auto-added
      { wxString parent(filepath);      // Otherwise add a wxFSW_EVENT_RENAME watch the hidden rootdir too, and all its ancestors; they can't be seen, but what if one were renamed...
        if (parent != wxT("/"))         // Unless 'filepath' is actually '/', in which case 1) it doesn't have a parent, and 2) it's already added
         while (true)
          { wxFileName fn(parent); parent = fn.GetPath(); // GetVisibleDirs() included the original 'filepath', so start the loop with a GetPath()
            if (parent == wxT("/")) break;
            m_fswatcher->Add(StrWithSep(parent), wxFSW_EVENT_RENAME);
          }
      }
    //PrintWatches(wxT("AddWatchLineage() end:"));
  }
catch(...)
  { wxLogWarning(wxT("Caught an unknown exception in MyFSWatcher::AddWatchLineage")); }
}

void MyFSWatcher::RemoveWatchLineage(const wxString& filepath) // Remove all watches starting with this filepath
{
wxCHECK_RET(!filepath.empty(), wxT("Passed an empty filepath"));

try 
  { 
    if (!CreateWatcherIfNecessary()) return;

    wxArrayString CurrentWatches;
    int currentcount = m_fswatcher->GetWatchedPaths(&CurrentWatches);
    wxLogTrace(wxT("myfswatcher"), wxT("RemoveWatchLineage() start: %i watches"), currentcount);

    if ((CurrentWatches.Index(StripSep(filepath)) == wxNOT_FOUND) && (CurrentWatches.Index(StrWithSep(filepath)) == wxNOT_FOUND))
      { wxLogTrace(wxT("myfswatcher"), wxT("Filepath %s wasn't watched in RemoveWatchLineage()"), filepath.c_str()); return; }
    //PrintWatches(wxT("RemoveWatchLineage() start loop:"));
    for (int n = currentcount-1; n >= 0; --n)
      { wxString item = CurrentWatches.Item(n);
        if ((StripSep(CurrentWatches.Item(n)) == StripSep(filepath)) || (CurrentWatches.Item(n).StartsWith(StrWithSep(filepath))))
          { wxLogNull NoLogWarningsAboutWrongPaths;
            do
              { m_fswatcher->Remove(item); }
             while (IsWatched(item));
            CurrentWatches.RemoveAt(n);
          }
      }
    wxLogTrace(wxT("myfswatcher"), wxT("RemoveWatchLineage() end: %i watches after removal starting with %s"), m_fswatcher->GetWatchedPathsCount(), filepath.c_str());
    //PrintWatches(wxT("RemoveWatchLineage() end:"));
  }
catch(...)
  { wxLogWarning(wxT("Caught an unknown exception in MyFSWatcher::RemoveWatchLineage")); }
}

bool MyFSWatcher::IsWatched(const wxString& filepath) const
{
wxArrayString CurrentWatches;
m_fswatcher->GetWatchedPaths(&CurrentWatches);

return (CurrentWatches.Index(StrWithSep(filepath)) != wxNOT_FOUND) || (CurrentWatches.Index(StripSep(filepath)) != wxNOT_FOUND);
}

bool MyFSWatcher::CreateWatcherIfNecessary()
{
if (m_fswatcher) return true;

#if wxVERSION_NUMBER > 2809
  if (!wxApp::IsMainLoopRunning()) return false;  // Until 2.8.10 wxApp::IsMainLoopRunning always returned false
#endif

#if USE_MYFSWATCHER
  m_fswatcher = new MyFileSystemWatcher;
#else
  m_fswatcher = new wxFileSystemWatcher;
#endif
if (m_owner) m_fswatcher->SetOwner(m_owner);
return true;
}

void MyFSWatcher::DoDelayedSetDirviewWatch(const wxString& path)
{
 // Check we're fully made: I got a segfault once on starting: m_fswatcher was valid, but not m_DelayedDirwatchTimer
 // If it isn't, ignore: we must be doing an initial expand of startdir, and the timer is irrelevant
if (m_DelayedDirwatchTimer)
  { m_DelayedDirwatchPath = path;
    m_DelayedDirwatchTimer->Start(200, wxTIMER_ONE_SHOT);
  }
}

void MyFSWatcher::OnDelayedDirwatchTimer()
{
if (!m_DelayedDirwatchPath.empty())
  SetDirviewWatch(m_DelayedDirwatchPath); // The timer fired, meaning that there was a, possibly recursive, expansion of a dir

m_DelayedDirwatchPath.Clear();
}

void MyFSWatcher::PrintWatches(const wxString& msg /*=wxT("")*/) const
{
wxArrayString CurrentWatches;
int currentcount = m_fswatcher->GetWatchedPaths(&CurrentWatches);
wxLogTrace(wxT("myfswatcher"), wxT("PrintWatches(): %s Count = %i"), msg.c_str(), currentcount);

for (int n = 0; n < currentcount; ++n)
  wxLogTrace(wxT("myfswatcher"), CurrentWatches.Item(n));
}

void WatchOverflowTimer::Notify()
{ 
if (m_isdirview) m_watch->SetDirviewWatch(m_filepath);
 else  m_watch->SetFileviewWatch(m_filepath);
// We must also force an update here. If not, any changes from the last 500ms won't display if the thread completed during that time
MyGenericDirCtrl* gdc = dynamic_cast<MyGenericDirCtrl*>(m_watch->GetOwner());
if (gdc && !m_isdirview) // Don't update a dirview: it doesn't seem 2b needed and it'll collapse the dir structure
  gdc->ReCreateTree();
}
#endif // defined(__LINUX__) && defined(__WXGTK__)
