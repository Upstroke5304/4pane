/////////////////////////////////////////////////////////////////////////////
// Name:       MyFiles.cpp
// Purpose:    File-view
// Part of:    4Pane
// Author:     David Hart
// Copyright:  (c) 2020 David Hart
// Licence:    GPL v3
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/log.h"

#include "wx/app.h"
#include "wx/frame.h" 
#include "wx/tokenzr.h"  
#include "wx/scrolwin.h"
#include "wx/menu.h"
#include "wx/dirctrl.h"
#include "wx/splitter.h"
#include "wx/file.h"
#include "wx/dragimag.h"
#include "wx/config.h"

#include "MyTreeCtrl.h"
#include "MyDirs.h"
#include "MyGenericDirCtrl.h"
#include "Accelerators.h"
#include "MyFrame.h"
#include "Redo.h"
#include "Dup.h"
#include "Archive.h"
#include "MyFiles.h"
#include "Filetypes.h"
#include "Misc.h"              // For FileDirDilg needed by OnProperties

//---------------------------------------------------------------------------------------------------------------------------
#include <pwd.h>
#include <grp.h>

void GetAllUsers(wxArrayString& names) // Global function, to acquire list of all usernames
{
struct passwd pw, *results;
static const size_t BUFLEN = 4096; char buf[BUFLEN];

setpwent();                                     // This call initialises what we're doing
while (true)
  { getpwent_r(&pw, buf, BUFLEN, &results);     // For every entry, get the user struct. The current item is pointed to by 'results'
                            
    if (results == NULL) 
      { endpwent();                             // If NULL we've run out.  Call endpwent() to sign off
        names.Sort(); return;                   // Sort the array & return
      }
    wxString str(results->pw_name, wxConvUTF8); // Extract this username & add it to the ArrayString
    names.Add(str);
  }
}


void GetProcessesGroupNames(wxArrayString& names)  // Global function, to acquire list of user's groups
{
struct group grp, *results;
static const size_t BUFLEN = 4096; char buf[BUFLEN];

if (getuid()==0)    // If we're root, we want a list of ALL groups, not just ones root happens to be a member of
  { setgrent();                                    // This call initialises what we're doing
    while (true)
      { getgrent_r(&grp, buf, BUFLEN, &results);   // For every entry, get the group struct
        
        if (results == NULL) 
          { endgrent();                            // If g==NULL we've run out.  Call endgrent() to sign off
            names.Sort(); return;                  // Sort the array & return
          }
        wxString str(results->gr_name, wxConvUTF8);// Extract the group's name & add it to the ArrayString
        names.Add(str);
      }
  }
                        // Otherwise do it the standard way, looking only at this user's groups
int ngroups = getgroups(0, NULL);                         // This first call to getgroups() returns their number
if (!ngroups) return;

gid_t* idarray  = new gid_t[ngroups * sizeof (gid_t)];    // Now we know the no of groups, make an int array to store them

int ans = getgroups(ngroups, idarray);                    // This 2nd call to getgroups() fills this array
if (ans != -1)
  for (int n=0; n < ngroups; ++n)
    { getgrgid_r(idarray[n], &grp, buf, BUFLEN, &results);// For every entry, fill the group struct
      if (results != NULL) 
        { wxString str(results->gr_name, wxConvUTF8);     // Extract the group's name & add it to the ArrayString
          names.Add(str);
        }
    }

names.Sort();                                             // Sort the array
delete[] idarray;
}

//---------------------------------------------------------------------------------------------------------------------------
static int wxCMPFUNC_CONV SortULLCompareFunction(DataBase**, DataBase**);
static int wxCMPFUNC_CONV SortULLReverseCompareFunction(DataBase**, DataBase**);

static int wxCMPFUNC_CONV FilenameCompareFunctionLC_COLLATE(DataBase **first, DataBase **second)  // LC_COLLATE aware
{
return wxStrcoll((*first)->ReallyGetName().c_str(), (*second)->ReallyGetName().c_str());
}

static int wxCMPFUNC_CONV FilenameReverseCompareFunctionLC_COLLATE(DataBase **second, DataBase **first)  // Same as above but reverse the sort. LC_COLLATE aware
{
return wxStrcoll((*first)->ReallyGetName().c_str(), (*second)->ReallyGetName().c_str());
}

static int wxCMPFUNC_CONV FilenameCompareFunction(DataBase **first, DataBase **second)  // Ditto, _not_ LC_COLLATE aware
{
return (*first)->ReallyGetName().CmpNoCase((*second)->ReallyGetName());
}

static int wxCMPFUNC_CONV FilenameReverseCompareFunction(DataBase **second, DataBase **first)
{
return (*first)->ReallyGetName().CmpNoCase((*second)->ReallyGetName());
}

static int wxCMPFUNC_CONV SortFilenameULLLC_COLLATECompareFunction(DataBase **first, DataBase **second)  // LC_COLLATE-aware alpha + decimal sort. This sorts foo1.txt, foo2.txt above foo10.txt, foo11.txt
{
if (((*first)->sortULL == (wxULongLong_t)-1) || ((*second)->sortULL == (wxULongLong_t)-1)
        || (*first)->sortstring.empty() || (*second)->sortstring.empty())
  return FilenameCompareFunctionLC_COLLATE(first, second); // We aren't comparing foo1, foo2 so revert to the standard way

int ans;
if ((ans = wxStrcoll((*first)->sortstring.c_str(), (*second)->sortstring.c_str())) == 0) // Sortstring holds foo, sortULL the terminal 1, 2, 10 or...
  return SortULLCompareFunction(first, second);  // If they match, re-sort by ULL
  
return ans;
}

static int wxCMPFUNC_CONV SortFilenameULLReverseLC_COLLATECompareFunction(DataBase **second, DataBase **first)  // Reverse LC_COLLATE-aware alpha + decimal sort
{
if (((*first)->sortULL == (wxULongLong_t)-1) || ((*second)->sortULL == (wxULongLong_t)-1)
        || (*first)->sortstring.empty() || (*second)->sortstring.empty())
  return FilenameReverseCompareFunctionLC_COLLATE(second, first);

int ans;
if ((ans = wxStrcoll((*first)->sortstring.c_str(), (*second)->sortstring.c_str())) == 0) 
  return SortULLReverseCompareFunction(second, first);  // If they match, re-sort by ULL
  
return ans;
}

static int wxCMPFUNC_CONV SortFilenameULLCompareFunction(DataBase **first, DataBase **second)  // Standard alpha + decimal sort
{
if (((*first)->sortULL == (wxULongLong_t)-1) || ((*second)->sortULL == (wxULongLong_t)-1)
        || (*first)->sortstring.empty() || (*second)->sortstring.empty())
  return FilenameCompareFunction(first, second); // We aren't comparing foo1, foo2 so revert to the standard way

int ans;
if ((ans = (*first)->sortstring.CmpNoCase((*second)->sortstring)) == 0)
  return SortULLCompareFunction(first, second);
  
return ans;
}

static int wxCMPFUNC_CONV SortFilenameULLReverseCompareFunction(DataBase **second, DataBase **first)  // Standard alpha + decimal reverse sort
{
if (((*first)->sortULL == (wxULongLong_t)-1) || ((*second)->sortULL == (wxULongLong_t)-1)
        || (*first)->sortstring.empty() || (*second)->sortstring.empty())
  return FilenameReverseCompareFunction(second, first);

int ans;
if ((ans = (*first)->sortstring.CmpNoCase((*second)->sortstring)) == 0)
  return SortULLReverseCompareFunction(second, first);  // If they match, re-sort by ULL
  
return ans;
}

static int wxCMPFUNC_CONV CompareFunctionLC_COLLATE(DataBase **first, DataBase **second)  // // Sort based on the string inserted into sortstring. LC_COLLATE aware
{
int ans;
if ((ans = wxStrcoll((*first)->sortstring.c_str(), (*second)->sortstring.c_str())) == 0)
  return wxStrcoll((*first)->ReallyGetName().c_str(), (*second)->ReallyGetName().c_str());  // If they match, re-sort by filename
  
return ans;
}

static int wxCMPFUNC_CONV ReverseCompareFunctionLC_COLLATE(DataBase **second, DataBase **first) // // LC_COLLATE aware
{
int ans;
if ((ans = wxStrcoll((*first)->sortstring.c_str(), (*second)->sortstring.c_str())) == 0)
  return wxStrcoll((*first)->ReallyGetName().c_str(), (*second)->ReallyGetName().c_str());
  
return ans;
}

static int wxCMPFUNC_CONV CompareFunction(DataBase **first, DataBase **second)  // // Sort based on the string inserted into sortstring
{
int ans;
if ((ans = (*first)->sortstring.CmpNoCase((*second)->sortstring)) == 0)  
  return (*first)->ReallyGetName().CmpNoCase((*second)->ReallyGetName());  // If they match, re-sort by filename
  
return ans;
}

static int wxCMPFUNC_CONV ReverseCompareFunction(DataBase **second, DataBase **first)
{
int ans;
if ((ans = (*first)->sortstring.CmpNoCase((*second)->sortstring)) == 0)  
  return (*first)->GetFilename().CmpNoCase((*second)->GetFilename());
  
return ans;
}

static int wxCMPFUNC_CONV (*FilenameCompareFunc)(DataBase**, DataBase**) = &FilenameCompareFunctionLC_COLLATE;
static int wxCMPFUNC_CONV (*FilenameReverseCompareFunc)(DataBase**, DataBase**) = &FilenameReverseCompareFunctionLC_COLLATE;
static int wxCMPFUNC_CONV (*FilenameULLCompareFunc)(DataBase**, DataBase**) = &SortFilenameULLLC_COLLATECompareFunction;
static int wxCMPFUNC_CONV (*FilenameULLReverseCompareFunc)(DataBase**, DataBase**) = &SortFilenameULLReverseLC_COLLATECompareFunction;

static int wxCMPFUNC_CONV SortintCompareFunction(DataBase **first, DataBase **second)  // // Sort based on contents of sortint
{
if ((*first)->sortint == (*second)->sortint)  
  return (*FilenameCompareFunc)(first, second);
  
return (*first)->sortint - (*second)->sortint;
}

static int wxCMPFUNC_CONV SortULLCompareFunction(DataBase **first, DataBase **second)	// // Sort based on contents of sortULL
{
if ((*first)->sortULL == (*second)->sortULL)	
	return (*FilenameCompareFunc)(first, second);
	
return ((*first)->sortULL > (*second)->sortULL) ? 1: -1;
}

static int wxCMPFUNC_CONV SortintReverseCompareFunction(DataBase **second, DataBase **first)
{
if ((*first)->sortint == (*second)->sortint)  
  return (*FilenameReverseCompareFunc)(second, first);
  
return (*first)->sortint - (*second)->sortint;
}

static int wxCMPFUNC_CONV SortULLReverseCompareFunction(DataBase **second, DataBase **first)
{
if ((*first)->sortULL == (*second)->sortULL)	
	return (*FilenameReverseCompareFunc)(second, first);
	
return ((*first)->sortULL > (*second)->sortULL) ? 1: -1;
}

static int wxCMPFUNC_CONV (*CompareFunc)(DataBase**, DataBase**) = &CompareFunctionLC_COLLATE;
static int wxCMPFUNC_CONV (*ReverseCompareFunc)(DataBase**, DataBase**) = &ReverseCompareFunctionLC_COLLATE;

void SetSortMethod(bool LC_COLLATE_aware)  // If true, make the dirpane and treelistctrl sorting take account of LC_COLLATE
{
if (LC_COLLATE_aware)
  { FilenameCompareFunc = &FilenameCompareFunctionLC_COLLATE;
    FilenameReverseCompareFunc = &FilenameReverseCompareFunctionLC_COLLATE;
    FilenameULLCompareFunc = &SortFilenameULLLC_COLLATECompareFunction;
    FilenameULLReverseCompareFunc = &SortFilenameULLReverseLC_COLLATECompareFunction;
    CompareFunc = &CompareFunctionLC_COLLATE;
    ReverseCompareFunc = &ReverseCompareFunctionLC_COLLATE;
  }
 else
  { FilenameCompareFunc = &FilenameCompareFunction;
    FilenameReverseCompareFunc = &FilenameReverseCompareFunction;
    FilenameULLCompareFunc = &SortFilenameULLCompareFunction;
    FilenameULLReverseCompareFunc = &SortFilenameULLReverseCompareFunction;
    CompareFunc = &CompareFunction;
    ReverseCompareFunc = &ReverseCompareFunction;
  }
}

const int HEADER_HEIGHT = 12;              // // The ht of the header column.  Originally 23

FileGenericDirCtrl::FileGenericDirCtrl(wxWindow* parent, const wxWindowID id, const wxString& START_DIR , const wxPoint& pos, const wxSize& size,
                                                                                                 long style, bool full_tree, const wxString& name)
    : MyGenericDirCtrl(parent, (MyGenericDirCtrl*)this, id, START_DIR ,  pos,  size ,  style , wxEmptyString, 0, name, ISRIGHT, full_tree), reverseorder(false), m_decimalsort(false)
{
m_StatusbarInfoValid = false; SelectedCumSize = 0;

headerwindow = new TreeListHeaderWindow(this, NextID++, (MyTreeCtrl*)GetTreeCtrl());
((MyTreeCtrl*)GetTreeCtrl())->headerwindow = headerwindow;

CreateAcceleratorTable();

  // For the define-an-ext subsubmenu of the context menu
Connect(SHCUT_EXT_FIRSTDOT, SHCUT_EXT_LASTDOT, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TreeListHeaderWindow::OnExtSortMethodChanged), NULL, headerwindow);
}

void FileGenericDirCtrl::CreateAcceleratorTable()
{
int AccelEntries[] = 
        { SHCUT_CUT, SHCUT_COPY, SHCUT_PASTE, SHCUT_HARDLINK, SHCUT_SOFTLINK, SHCUT_TRASH,
          SHCUT_DELETE, SHCUT_OPEN, SHCUT_OPENWITH, SHCUT_RENAME, SHCUT_NEW, SHCUT_UNDO, SHCUT_REDO,
          SHCUT_REFRESH, SHCUT_FILTER, SHCUT_TOGGLEHIDDEN, SHCUT_PROPERTIES,
          SHCUT_REPLICATE, SHCUT_SWAPPANES,
          SHCUT_SPLITPANE_VERTICAL, SHCUT_SPLITPANE_HORIZONTAL, SHCUT_SPLITPANE_UNSPLIT,
          SHCUT_GOTO_SYMLINK_TARGET, SHCUT_GOTO_ULTIMATE_SYMLINK_TARGET,
          SHCUT_SWITCH_FOCUS_OPPOSITE, SHCUT_SWITCH_FOCUS_ADJACENT, SHCUT_SWITCH_FOCUS_PANES, SHCUT_SWITCH_FOCUS_TERMINALEM,
          SHCUT_SWITCH_FOCUS_COMMANDLINE, SHCUT_SWITCH_FOCUS_TOOLBARTEXT, SHCUT_SWITCH_TO_PREVIOUS_WINDOW,
          SHCUT_PREVIOUS_TAB,SHCUT_NEXT_TAB,
          SHCUT_EXT_FIRSTDOT, SHCUT_EXT_MIDDOT, SHCUT_EXT_LASTDOT,
          SHCUT_DECIMALAWARE_SORT, SHCUT_NAVIGATE_DIR_UP, SHCUT_NAVIGATE_DIR_PREVIOUS, SHCUT_NAVIGATE_DIR_NEXT
        };
const size_t shortcutNo = sizeof(AccelEntries)/sizeof(int);
MyFrame::mainframe->AccelList->CreateAcceleratorTable(this, AccelEntries,  shortcutNo);
}

void FileGenericDirCtrl::RecreateAcceleratorTable()
{
        // It would be nice to be able to empty the old accelerator table before replacing it, but trying caused segfaulting!
CreateAcceleratorTable();                                   // Make a new one, with up-to-date data
((MyTreeCtrl*)GetTreeCtrl())->RecreateAcceleratorTable();   // Tell the tree to do likewise
}

void FileGenericDirCtrl::DoSize()
{
int w, h; GetClientSize(&w, &h);

GetTreeCtrl()->SetSize(0, HEADER_HEIGHT + 1, w, h - HEADER_HEIGHT - 1);  // Set the treectrl 'body' size
headerwindow->SetSize(0, 0, w, HEADER_HEIGHT);              // Set the header-window size

headerwindow->SizeColumns();                                //   and adjust the columns intelligently. Also does the scrollbars
}

void FileGenericDirCtrl::ShowContextMenu(wxContextMenuEvent& event)
{
if (((MyTreeCtrl*)GetTreeCtrl())->QueryIgnoreRtUp()) return; // If we've been Rt-dragging & now stopped, we don't want a context menu

int flags;                          // First, make sure there is a selection, unless the user Rt-clicked in mid-air
if (GetPath().IsEmpty())                                    // If there is no selection, select one if that's what the user probably wants
  { wxTreeItemId item = GetTreeCtrl()->HitTest(GetTreeCtrl()->ScreenToClient(event.GetPosition()), flags);
    if (item.IsOk())                                        // Find out if we're on the level of a tree item
      GetTreeCtrl()->SelectItem(item);                      // If so, select it
  }
 else
  if (GetPath() == startdir)  // If the selection is startdir, yet the click is directly on an item, select it.  That's probably what the user wants
    { wxTreeItemId item = GetTreeCtrl()->HitTest(GetTreeCtrl()->ScreenToClient(event.GetPosition()), flags);
      if (item.IsOk()  && !(flags & wxTREE_HITTEST_ONITEMRIGHT))  // Find out if we're on a tree item (& not in the space to the right of it)
        GetTreeCtrl()->SelectItem(item);
    }

bool show_Open = false;
bool show_OpenWith = false;
bool show_OpenWithKdesu = false;
bool ItsaDir;
bool WithinArchive = arcman->IsArchive();                   // Are we rummaging around inside a virtual archive?

    // See if the user has cleverly filtered out *all* of the pane's content. If so, use startdir (otherwise he can't get a contextmenu to reset the filter!)
wxString filepath(GetPath());
NoVisibleItems = filepath.IsEmpty();
if (NoVisibleItems)  filepath = startdir;                   // The NoVisibleItems flag is used by updateUI to disable menuitems like Del, Cut

FileData stat(filepath);
if (WithinArchive) ItsaDir = arcman->GetArc()->Getffs()->GetRootDir()->FindSubdirByFilepath(filepath) != NULL;  // We can't test for dir-ness except by finding one with this name
 else 
  { ItsaDir = stat.IsDir();                                 // We need to know if this file is in fact a Dir.  If so, we'll call it such in the menu
    if (stat.IsSymlink()) filepath = stat.GetUltimateDestination(); // If it's a symlink, make filepath the (ultimate) destination.  This makes FiletypeManager behave more sensibly
  }
  
FiletypeManager manager;
manager.Init(filepath);                                     // Use specialist class to get file info. NB this won't work inside archives; we'll do those in a minute
char buf[3];                                                // A char array will hold the answers
buf[0] = buf[1] = buf[2] = 0;
if (manager.QueryCanOpen(buf))                              // See if we can open the file by double-clicking, ie executable or ext with default command
  { show_OpenWith = buf[0];                                 // No, but the file exists
    show_Open = buf[1];                                     // Yes, we can open
    show_OpenWithKdesu = buf[2];                            // Yes, but we need root privilege
  }

wxMenu menu(wxT(""));

int Firstsection[] = { SHCUT_CUT, SHCUT_COPY, SHCUT_PASTE, SHCUT_SOFTLINK, SHCUT_HARDLINK, SHCUT_SELECTALL, wxID_SEPARATOR };
const wxString Firstsectionstrings[] = { wxT(""), wxT(""), wxT(""), _("Make a S&ymlink"), _("Make a Hard-Lin&k"), wxT(""), wxT("") }; // Don't use some defaults, to avoid mnemonic clashes
if (WithinArchive)
  { for (size_t n=0; n < 3; ++n)                            // We start with a section where it's easier to insert with a loop. Just the 1st 3 for archives
      MyFrame::mainframe->AccelList->AddToMenu(menu, Firstsection[n]);
    if (!ItsaDir)
      { show_Open = manager.QueryCanOpenArchiveFile(filepath); show_OpenWith = true; }
    
    MyFrame::mainframe->AccelList->AddToMenu(menu, wxID_SEPARATOR);
    MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_ARCHIVE_EXTRACT, _("Extract from archive"));
    MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_DELETE, _("De&lete from archive"));
    if (ItsaDir)  
      { MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_RENAME,  _("Rena&me Dir within archive"));
        MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_DUP,  _("Duplicate Dir within archive"));
      }
     else
      { MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_RENAME,  _("Rena&me File within archive"));
        MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_DUP,  _("Duplicate File within archive"));
      }
  }
 else
  { for (size_t n=0; n < 7; ++n)                            // We start with a section where it's easier to insert with a loop
      MyFrame::mainframe->AccelList->AddToMenu(menu, Firstsection[n], Firstsectionstrings[n]);
    
    wxString delmsg, trashmsg;
    if (ItsaDir)            { delmsg =  _("De&lete Dir"); trashmsg = _("Send Dir to &Trashcan"); }
     else  if (stat.IsSymlink())  { delmsg =  _("De&lete Symlink"); trashmsg = _("Send Symlink to &Trashcan"); }
          else            { delmsg =  _("De&lete File"); trashmsg = _("Send File to &Trashcan"); }
    
    MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_TRASH, trashmsg);
    MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_DELETE, delmsg);
  }

menu.AppendSeparator();

if (!WithinArchive && stat.IsSymlink())                     // If we're dealing with a symlink, add the relevant GOTO commands
  { MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_GOTO_SYMLINK_TARGET);
    if (stat.IsSymlinktargetASymlink()) MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_GOTO_ULTIMATE_SYMLINK_TARGET);
    menu.AppendSeparator();
  }

if (show_Open)   MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_OPEN);
if (show_OpenWithKdesu)
  { wxArrayString Commands;                                 // Invent an array to pass in OfferChoiceOfApplics.  Using CommandArray directly segfaulted
    if (manager.OfferChoiceOfApplics(Commands) && !show_OpenWith)  // If there are known applics to offer for this filetype, & we're not about to do this below
      { wxMenu* submenu = new wxMenu;                       //  make a submenu & populate it with the applics
        for (size_t n=0; n < manager.Array.GetCount(); ++n)
          submenu->Append(SHCUT_RANGESTART + n, manager.Array[n]);
        submenu->AppendSeparator();                         // Finish the submenu with a separator followed by an 'Other' option -> OpenWith
        submenu->Append(SHCUT_OPENWITH, _("Other . . ."));
        menu.Append(NextID++, _("Open using root privile&ges with . . . . "),submenu);
        CommandArray = Commands;                            // Store any commands
      }
     else  menu.Append(SHCUT_OPENWITH_KDESU, _("Open using root privile&ges"));  // Give kdesu option if appropriate
  }

if (show_OpenWith)   
  { wxArrayString Commands;                                 // Invent an array to pass in OfferChoiceOfApplics.  Using CommandArray directly segfaulted
    if (manager.OfferChoiceOfApplics(Commands))             // If there are known applics to offer for this filetype
      { wxMenu* submenu = new wxMenu;                       //  make a submenu & populate it with the applics
        for (size_t n=0; n < manager.Array.GetCount(); ++n)
          submenu->Append(SHCUT_RANGESTART + n, manager.Array[n]);
        submenu->AppendSeparator();                         // Finish the submenu with a separator followed by an 'Other' option -> OpenWith
        submenu->Append(SHCUT_OPENWITH, _("Other . . ."));
        menu.Append(NextID++, _("Open &with . . . . "), submenu);
        CommandArray = Commands;                            // Store any commands
      }
     else  MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_OPENWITH);
  }
if (show_Open || show_OpenWith || show_OpenWithKdesu)       // If there's an OpenSomething, add another separator
   menu.AppendSeparator();

MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_REFRESH);
if (!WithinArchive) 
  { if (ItsaDir)
      { MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_RENAME,  _("Rena&me Dir"));
        MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_DUP, _("&Duplicate Dir"));
      }
     else
      { MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_RENAME,  _("Rena&me File"));
        MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_DUP, _("&Duplicate File"));
      }
    MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_NEW);
    menu.AppendSeparator();

                  // Now for Archive/Compression entries.  Don't add any if there's no selection, otherwise choose depending on selected filetypes
    wxArrayString paths; bool arc=false, comp=false, ordinaryfile=false;
    size_t count = GetMultiplePaths(paths);
    for (size_t n=0; n < count; ++n)                          // Look thru every selection, seeing if there're any archives, compressed files, ordinary ones
      switch (Archive::Categorise(paths[n]))
        { case zt_invalid:          ordinaryfile = true; break;
          case zt_taronly:               arc = true; break;
          case zt_gzip: case zt_bzip:        comp = true; break;
            default:                comp = true; arc = true;  // Default catches targz, tarbz, zip
        }
    if (arc || comp)          MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_ARCHIVE_EXTRACT);
    if (ordinaryfile || comp)  MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_ARCHIVE_CREATE,_("Create a New Archive"));  // Explicit labels to avoid mnemonic clashes
    if (arc)                  MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_ARCHIVE_APPEND, _("Add to an Existing Archive"));
    if (arc || comp)          MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_ARCHIVE_TEST, _("Test integrity of Archive or Compressed Files"));
    if (ordinaryfile)          MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_ARCHIVE_COMPRESS, _("Compress Files"));
    if (count)                menu.AppendSeparator();
  }

MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_FILTER);
if (GetShowHidden())
  MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_TOGGLEHIDDEN, _("&Hide hidden dirs and files"));
 else
   MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_TOGGLEHIDDEN, _("&Show hidden dirs and files"));

if (GetIsDecimalSort())
   MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_DECIMALAWARE_SORT, _("&Sort filenames ending in digits normally"));
 else
  MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_DECIMALAWARE_SORT, _("&Sort filenames ending in digits in Decimal order"));

menu.AppendSeparator();

wxMenu* colmenu = new wxMenu;
colmenu->SetClientData((wxClientData*)this); // In >wx2.8 we must store ourself in the menu, otherwise MyFrame::DoColViewUI can't work out who we are
int columns[] = { SHCUT_SHOW_COL_EXT, SHCUT_SHOW_COL_SIZE, SHCUT_SHOW_COL_TIME, SHCUT_SHOW_COL_PERMS, SHCUT_SHOW_COL_OWNER,
                SHCUT_SHOW_COL_GROUP, SHCUT_SHOW_COL_LINK, wxID_SEPARATOR, SHCUT_SHOW_COL_ALL, SHCUT_SHOW_COL_CLEAR };
for (size_t n=0; n < 10; ++n)                                  // This submenu is easy to insert with a loop
  { MyFrame::mainframe->AccelList->AddToMenu(*colmenu, columns[n], wxEmptyString,  (n < 7 ? wxITEM_CHECK : wxITEM_NORMAL));
  #ifdef __WXX11__
    if (n < 7) colmenu->Check(columns[n], !GetHeaderWindow()->IsHidden(n+1));  // The usual updateui way of doing this doesn't work in X11
  #endif
  }

int extsubmenu[] = { SHCUT_EXT_FIRSTDOT, SHCUT_EXT_MIDDOT, SHCUT_EXT_LASTDOT };
wxMenu* submenu = new wxMenu(_("An extension starts at the filename's..."));
for (size_t n=0; n < 3; ++n)
  MyFrame::mainframe->AccelList->AddToMenu(*submenu, extsubmenu[n], wxEmptyString, wxITEM_RADIO);
submenu->FindItem(extsubmenu[EXTENSION_START])->Check();

MyFrame::mainframe->AccelList->AddToMenu(*colmenu, wxID_SEPARATOR);
colmenu->AppendSubMenu(submenu, _("Extension definition..."));

menu.Append(NextID++, _("Columns to Display"), colmenu);

int Secondsection[] = { wxID_SEPARATOR, SHCUT_SPLITPANE_VERTICAL, SHCUT_SPLITPANE_HORIZONTAL, SHCUT_SPLITPANE_UNSPLIT, wxID_SEPARATOR,
                      SHCUT_REPLICATE, SHCUT_SWAPPANES, wxID_SEPARATOR };
const wxString Secondsectionstrings[] = { wxT(""), wxT(""), wxT(""), _("Unsplit Panes"), wxT(""), _("Repl&icate in Opposite Pane"), _("Swap the Panes"), wxT("") }; // Avoid some mnemonic clashes
size_t ptrsize = sizeof(Secondsection)/sizeof(int);
for (size_t n=0; n < ptrsize; ++n)                        // And another section where it's easier to insert with a loop
  MyFrame::mainframe->AccelList->AddToMenu(menu, Secondsection[n], Secondsectionstrings[n]);

MyFrame::mainframe->Layout->m_notebook->LaunchFromMenu->LoadMenu(&menu);  // Now add the User-defined Tools submenu

if (!WithinArchive) { menu.AppendSeparator(); MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_PROPERTIES); }

wxPoint pt = event.GetPosition();
ScreenToClient(&pt.x, &pt.y);

PopupMenu(&menu, pt.x, pt.y);
}

void FileGenericDirCtrl::OpenWithSubmenu(wxCommandEvent& event) // Events from the submenu of the context menu end up here 
{
if (!CommandArray.GetCount()) return;
if (event.GetId() >= SHCUT_RANGEFINISH)  return;

wxString open = CommandArray[event.GetId() - SHCUT_RANGESTART];
if (open.IsEmpty()) return;

wxString path = GetPath();
if (path.empty()) path = GetCwd();                                // This may happen if we're trying to launch a grep-result from the terminalem

FileData stat(path);                                              // We need to stat again to check if read-access: all the old info was lost with Context menu
bool usingsu = !arcman->IsArchive() && !stat.CanTHISUserRead();   // stat will be invalid if this is inside an archive: I don't believe we can fail to have permissions though  
FiletypeManager::Openfunction(open, usingsu);                     // Do the rest in FiletypeManager submethod, shared by 'Open with' etc
}

void FileGenericDirCtrl::NewFile(wxCommandEvent& event)           // Create a new File or Dir
{
wxString path, name;
int ItsADir, flag = 0;
                                    // Where to put the new item
path = GetPath();                                                 // Get path of selected item, in case it's a subdir
if (path.IsEmpty())  path = startdir;                             // If no selection, use "root"
  else
    { FileData fd(path);
      if (!fd.IsDir())  path = path.BeforeLast(wxFILE_SEP_PATH);  // If it's not a dir, it's a filename; remove this to leave the path
    }
if (path.Last() != wxFILE_SEP_PATH)  path += wxFILE_SEP_PATH;     // Whichever, we almost certainly need to add a terminal '/'        

FileData DestinationFD(path);
if (!DestinationFD.CanTHISUserWriteExec())                        // Make sure we have permission to write to the dir
  { wxString msg;
    if  (arcman != NULL && arcman->IsArchive())  msg = _("I'm afraid you can't Create inside an archive.\nHowever you can create the new element outside, then Move it in.");
      else msg = _("I'm afraid you don't have permission to Create in this directory");
    wxMessageDialog dialog(this, msg, _("No Entry!"), wxOK | wxICON_ERROR);
    dialog.ShowModal(); return;
    }

wxDialog dlg;
wxXmlResource::Get()->LoadDialog(&dlg, MyFrame::mainframe, wxT("NewFileOrDir"));

do                                  // Do the rest in a loop, to give a 2nd chance if there's a name clash etc
  { 
    if (dlg.ShowModal() != wxID_OK)    return;                    // If user cancelled, abort
    
    name = ((wxTextCtrl*)dlg.FindWindow(wxT("FileDir_text")))->GetValue();  // Get the desired name
    if (name.IsEmpty()) return;
    
    ItsADir = ((wxRadioBox*)dlg.FindWindow(wxT("FileDir_radiobox")))->GetSelection();  // Dir or File requested. File is 0, Dir 1
        
    flag = MyGenericDirCtrl::OnNewItem(name, path, (ItsADir==1)); // Do the rest in MyGenericDirCtrl, as shared with MyDirs version
  }
 while (flag==wxID_YES);              // Loop if requested: means there was an error & we want another go
 
if (!flag) return;
 
wxArrayInt IDs; IDs.Add(GetId());                                 // Update the panes
MyFrame::mainframe->OnUpdateTrees(path, IDs);
bool ClusterWasNeeded = UnRedoManager::StartClusterIfNeeded();
UnRedoNewDirFile *UnRedoptr = new UnRedoNewDirFile(name, path, (ItsADir==1), IDs);  // Make a new UnRedoNewDirFile
UnRedoManager::AddEntry(UnRedoptr);                                                 // and store it for Undoing 
if (ClusterWasNeeded) UnRedoManager::EndCluster();

BriefLogStatus bls(ItsADir==1 ? _("New directory created") : _("New file created"));
}

void FileGenericDirCtrl::OnArchiveAppend(wxCommandEvent& event)
{
MyFrame::mainframe->OnArchiveAppend(event);
}

void FileGenericDirCtrl::OnArchiveTest(wxCommandEvent& event)
{
MyFrame::mainframe->OnArchiveTest(event);
}

void MyGenericDirCtrl::OnDup(wxCommandEvent& WXUNUSED(event))
{
wxArrayString paths; size_t count = GetMultiplePaths(paths);
if (!count) return;
if (count > 1) DoMultipleRename(paths, true);
 else DoRename(true);                                             // Because this method can also rename, "true" flags that this is duplication
}

void MyGenericDirCtrl::OnRename(wxCommandEvent& WXUNUSED(event))  // Rename a File or Dir
{
wxArrayString paths; size_t count = GetMultiplePaths(paths);
if (!count) return;
if (count > 1) DoMultipleRename(paths, false);
 else DoRename(false);                                            // Because this method can also duplicate, "false" flags that this is a rename
}

void MyGenericDirCtrl::DoRename(bool duplicate, const wxString& pathtodup /*=wxT("")*/)  // Renames (or duplicates) a File or Dir
{
wxString path, origname, newname, oldname, displayname;
int ItsADir, flag = 0;
bool renamingstartdir = false;

if (pathtodup.IsEmpty())
  origname = GetPath();                                           // Get name of selected item
 else
  origname = pathtodup; // This happens if we arrived here from the within-archive section of MyGenericDirCtrl::OnDnDMove
if (origname.IsEmpty())  return;                                  // If no selection, disappear

while (origname.Len() > 1 && origname.Right(1) == wxFILE_SEP_PATH) origname.RemoveLast();  // Avoid '/' problems

if (origname == wxT("/"))
  { wxString msg;    
    if (duplicate)  msg = _("I don't think you really want to duplicate root, do you");
      else          msg = _("I don't think you really want to rename root, do you");
    wxMessageDialog dialog(this, msg, _("Oops!"), wxICON_ERROR); dialog.ShowModal(); return;
    }

FileData fd(origname); ItsADir = fd.IsDir();                      // Store whether it's a dir; needed later

path = origname.BeforeLast(wxFILE_SEP_PATH);                      // Get the path for this dir or file:  SHOULD work for both
path += wxFILE_SEP_PATH;                                          // Add back the terminal '/'        

              // We need tocheck whether we're renaming within an archive: if so things are different
MyGenericDirCtrl* active = MyFrame::mainframe->GetActivePane();   // Yes, I know we're inside MyGenericDirCtrl, but we want the correct pane
bool WithinArc = (active && active->arcman && active->arcman->IsArchive());

FileData DestinationFD(path);
if (!WithinArc && !DestinationFD.CanTHISUserWriteExec())          // Make sure we have permission to write to the dir
  { wxString msg;    
    if (duplicate)  msg = _("I'm afraid you don't have permission to Duplicate in this directory");
      else        msg = _("I'm afraid you don't have permission to Rename in this directory");
    wxMessageDialog dialog(this, msg, _("No Entry!"), wxOK | wxICON_ERROR);
    dialog.ShowModal(); return;
    }
    
wxDialog dlg;
wxXmlResource::Get()->LoadDialog(&dlg, MyFrame::mainframe,  wxT("RenameDlg"));

                // Use the filename in the dialog title, prefixed by .../
oldname = origname.AfterLast(wxFILE_SEP_PATH);
displayname = wxFILE_SEP_PATH + oldname;
if (displayname.Len() < origname.Len())  displayname =  wxT("...") + displayname; // Don't want a prefix for eg /home

if (duplicate) dlg.SetTitle(_("Duplicate   ") + displayname);     // Use the title as a way to confirm we're trying to dup the right item
 else          dlg.SetTitle(_("Rename   ") + displayname);        //   or rename it
 
do              // Do the rest in a loop, to give a 2nd chance if there's a name clash etc
  { wxTextCtrl* text = (wxTextCtrl*)dlg.FindWindow(wxT("newfilename"));
    text->Clear();                                                // In case this isn't the first iteration of the loop
    text->ChangeValue(oldname); text->SetSelection(-1, -1); text->SetFocus(); text->SetInsertionPoint(0);  // Insert the original name & highlight it. Then move to start of text, so that END will  unselect: this being a likely thing to want to do
    
    if (dlg.ShowModal() != wxID_OK)    return;                    // If user cancelled, abort
    
    newname = text->GetValue();                                   // Get the desired name
    if (newname.IsEmpty()) return;
    if (newname == oldname)
      { if (duplicate)   wxMessageBox(_("No, the idea is that you supply a NEW name"));
          else           wxMessageBox(_("No, the idea is that you CHANGE the name")); 
        flag = wxID_YES; continue; 
      }
      
    if (ItsADir)
      { oldname += wxFILE_SEP_PATH; newname += wxFILE_SEP_PATH; } // Make sure they're recognised as dirs 
    
    if (!WithinArc)    flag = Rename(path, oldname, newname, ItsADir, duplicate);  // Do the actual rename/dup in a submethod
     else                              // Things are very different within an archive
       { wxArrayString filepaths, newpaths;   // Make arrays so as to cope with renaming a dir: all its descendants need to be listed too
        filepaths.Add(path+oldname); newpaths.Add(path+newname); 
        if (active->arcman->GetArc()->Alter(filepaths, duplicate ? arc_dup : arc_rename, newpaths))  // and do the rename
          { bool ClusterWasNeeded = UnRedoManager::StartClusterIfNeeded();
            wxArrayInt IDs; IDs.Add(GetId());

            UnRedoArchiveRen* UnRedoptr = new UnRedoArchiveRen(filepaths, newpaths, IDs, active, duplicate);
            UnRedoManager::AddEntry(UnRedoptr);
            if (ClusterWasNeeded) UnRedoManager::EndCluster();;
            MyFrame::mainframe->OnUpdateTrees(path, IDs, wxT(""), true);
            DoBriefLogStatus(1, wxEmptyString, duplicate ?_("duplicated") :  _("renamed"));
            return;
          }
        if (wxMessageBox(_("Sorry, that didn't work. Try again?"), _("Oops"), wxYES_NO) != wxYES)  return;
        flag = wxID_YES;                                          // This flags to retry  
      }
  }
 while (flag==wxID_YES);          // Loop if requested: means there was an error & we want another go
 
if (!flag) return;

if (ItsADir && !duplicate && origname == GetCwd())                // If we've just renamed the cwd, change the cwd.  Otherwise Ctrl-T will fail
            SetWorkingDirectory(path + newname);

wxArrayInt IDs; IDs.Add(GetId());                                 // Update the panes
if (origname == startdir)                                         // If we've just renamed startdir, need to do things differently
  { wxString newstartdir = path + newname.BeforeLast(wxFILE_SEP_PATH);  // We know that newname ends in /, we just put it there!
    MyFrame::mainframe->OnUpdateTrees(origname, IDs, newstartdir);// Newstartdir will be substituted for the old one
    renamingstartdir = true;
  }
 else MyFrame::mainframe->OnUpdateTrees(path, IDs);
 
bool ClusterWasNeeded = UnRedoManager::StartClusterIfNeeded();
if (duplicate)
  { UnRedoDup* UnRedoptr = new UnRedoDup(path, path, oldname, newname, ItsADir, IDs);  
    UnRedoManager::AddEntry(UnRedoptr);
    DoBriefLogStatus(1, wxEmptyString, _("duplicated")); 
  }
 else
  { UnRedoRen* UnRedoptr;
    if (ItsADir)                        // To get the UnRedoRen right, if it's a dir, we have to merge path & names
      { newname = path + newname; 
        if (renamingstartdir)
          UnRedoptr = new UnRedoRen(origname, newname, wxT(""), newname, ItsADir, IDs); // If renaming startdir, use 4th string as notification
         else
          UnRedoptr = new UnRedoRen(origname, newname, wxT(""), wxT(""), ItsADir, IDs); // Otherwise make a new UnRedoRen the standard way
    
        wxString filepath = newname;      // While we're in a ItsADir area, we need to select the new dir, else nothing is selected & the fileview is empty
        while (filepath.Right(1) == wxFILE_SEP_PATH && filepath.Len() > 1)  filepath.RemoveLast();  // We don't want a terminal '/' in FindIdForPath
        wxTreeItemId item = FindIdForPath(filepath);              // Find the new dir's id, & use this to select it
        if (item.IsOk())   GetTreeCtrl()->SelectItem(item);
      }
      else UnRedoptr = new UnRedoRen(path, path, oldname, newname, ItsADir, IDs);

    DoBriefLogStatus(1, wxEmptyString, _("renamed"));
    UnRedoManager::AddEntry(UnRedoptr);
  }
if (ClusterWasNeeded) UnRedoManager::EndCluster();
}

void MyGenericDirCtrl::DoMultipleRename(wxArrayString& OriginalFilepaths, bool duplicate)  // Renames (or duplicates) multiple files/dirs
{
if (!OriginalFilepaths.GetCount()) return;

              // We need tocheck whether we're renaming within an archive: if so things are different
MyGenericDirCtrl* active = MyFrame::mainframe->GetActivePane();   // Yes, I know we're inside MyGenericDirCtrl, but it doesn't get IsArchive() right otherwise
bool WithinArc = (active && active->arcman && active->arcman->IsArchive());

FileData DestinationFD(OriginalFilepaths[0].BeforeLast(wxFILE_SEP_PATH));
if (!WithinArc && !DestinationFD.CanTHISUserWriteExec())          // Make sure we have permission to write to the dir
  { wxString msg;    
    if (duplicate)  msg = _("I'm afraid you don't have permission to Duplicate in this directory");
      else          msg = _("I'm afraid you don't have permission to Rename in this directory");
    wxMessageDialog dialog(this, msg, _("No Entry!"), wxOK | wxICON_ERROR);
    dialog.ShowModal(); return;
    }

for (size_t count = OriginalFilepaths.GetCount(); count > 0; --count)
  { wxString name = OriginalFilepaths[count-1];
    while (name.Len() > 1 && name.Right(1) == wxFILE_SEP_PATH) name.RemoveLast();  // Avoid '/' problems
    
    if (name == wxT("/"))
      { wxString msg;    
        if (duplicate)  msg = _("I don't think you really want to duplicate root, do you");
          else        msg = _("I don't think you really want to rename root, do you");
        wxMessageDialog dialog(this, msg, _("Oops!"), wxICON_ERROR); dialog.ShowModal();
        OriginalFilepaths.RemoveAt(count-1);
        if (OriginalFilepaths.GetCount()==1)  // If as a result of removing /, we no longer have a multiple situation, revert to the single method
          { SetPath(OriginalFilepaths[0]); return DoRename(duplicate); }
      }
  }

MultipleRename Mult(MyFrame::mainframe, OriginalFilepaths, duplicate);
wxArrayString NewFilepaths = Mult.DoRename();                     //This fills NewFilepaths with the altered filenames
if (NewFilepaths.IsEmpty())  return;
if (WithinArc)                                                    // If in archive, do things elsewhere
  { if (active->arcman->GetArc()->Alter(OriginalFilepaths, duplicate ? arc_dup : arc_rename, NewFilepaths))
      { bool ClusterWasNeeded = UnRedoManager::StartClusterIfNeeded();
        wxArrayInt IDs; IDs.Add(GetId());
        wxString path = OriginalFilepaths[0].BeforeLast(wxFILE_SEP_PATH) + wxFILE_SEP_PATH;
        
        UnRedoArchiveRen* UnRedoptr = new UnRedoArchiveRen(OriginalFilepaths, NewFilepaths, IDs, active, duplicate);
        UnRedoManager::AddEntry(UnRedoptr);
        if (ClusterWasNeeded) UnRedoManager::EndCluster();
        MyFrame::mainframe->OnUpdateTrees(path, IDs, wxT(""), true);
        DoBriefLogStatus(OriginalFilepaths.GetCount(), wxEmptyString, duplicate ? _("duplicated") :_("renamed"));
      }
    return; 
  } 

int successes = 0;
bool ClusterWasNeeded = UnRedoManager::StartClusterIfNeeded();
for (size_t n = NewFilepaths.GetCount(); n > 0 ; --n) // Loop backwards, in case we're renaming both a dir & its children: the dir should be before children in the array
  { bool renamingstartdir = false;
    wxString origfilepath = OriginalFilepaths[n-1], newfilepath = NewFilepaths[n-1];
    if (newfilepath.Right(1) == wxFILE_SEP_PATH) newfilepath.RemoveLast();  // Make sure we can do AfterLast in a minute to get the filename: can't use FileData before the file exists
    FileData Oldfd(origfilepath); bool ItsADir = Oldfd.IsDir();  
    wxString oldname = Oldfd.GetFilename(), newname = newfilepath.AfterLast(wxFILE_SEP_PATH), path = Oldfd.GetPath();
    if (path.Right(1) != wxFILE_SEP_PATH) path += wxFILE_SEP_PATH;
    if (ItsADir)
      { oldname += wxFILE_SEP_PATH; newname += wxFILE_SEP_PATH; } // Make sure they'll be recognised as dirs 

    Rename(path, oldname, newname, ItsADir, duplicate);           // Do the actual rename/dup in a submethod
    ++successes;

    if (ItsADir && !duplicate && origfilepath == GetCwd())        // If we've just renamed the cwd, change the cwd.  Otherwise Ctrl-T will fail
            SetWorkingDirectory(newfilepath);

    wxArrayInt IDs; IDs.Add(GetId());                             // Update the panes
    if (origfilepath == startdir)                                 // If we've just renamed startdir, need to do things differently
      { wxString newstartdir = path + newname.BeforeLast(wxFILE_SEP_PATH);  // We know that newname ends in /, we just put it there!
        MyFrame::mainframe->OnUpdateTrees(origfilepath, IDs, newstartdir);  // Newstartdir will be substituted for the old one
        renamingstartdir = true;
      }
     else MyFrame::mainframe->OnUpdateTrees(path, IDs);

    if (duplicate)
      { UnRedoDup* UnRedoptr = new UnRedoDup(path, path, oldname, newname, ItsADir, IDs);  
        UnRedoManager::AddEntry(UnRedoptr);
      }
     else
      { UnRedoRen* UnRedoptr;
        if (ItsADir)                      // To get the UnRedoRen right, if it's a dir, we have to merge path & names
          { newname = path + newname;     // Reconstitute the full filepath. Could have used newfilepath, but this way we know there's a terminal /
            if (renamingstartdir)
              UnRedoptr = new UnRedoRen(origfilepath, newname, wxT(""), newname, ItsADir, IDs);      // If renaming startdir, use 4th string as notification
             else
              UnRedoptr = new UnRedoRen(origfilepath, newname, wxT(""), wxT(""), ItsADir, IDs); // Otherwise make a new UnRedoRen the standard way
          }
         else UnRedoptr = new UnRedoRen(path, path, oldname, newname, ItsADir, IDs);
          
        UnRedoManager::AddEntry(UnRedoptr);
      }      
  }

if (fileview==ISLEFT)
  { SetPath(startdir);                    // We need to select the (possibly new) startdir, else nothing is selected & the fileview is empty
    ReCreateTreeFromSelection();
  }

if (ClusterWasNeeded) UnRedoManager::EndCluster();
DoBriefLogStatus(successes, wxEmptyString, duplicate ? _("duplicated") :_("renamed"));
}

int MyGenericDirCtrl::OnNewItem(wxString& newname, wxString& path, bool ItsADir)
{
if ((newname == wxT(".")) || (newname == wxT("..")) ||  (newname == wxT("/")))
  { wxMessageDialog dialog(this, _("Sorry, this name is Illegal\n        Try again?"), _("Oops!"), wxYES_NO |wxICON_ERROR);
    int ans = dialog.ShowModal();
    if (ans==wxID_YES)        return wxID_YES;
    return false;
    }

wxString filepath = path + newname;       // Now we can combine path & name
    
wxLogNull log;

FileData fd(filepath);
if (fd.IsValid())
  { wxMessageDialog dialog(this, _("Sorry, One of these already exists\n            Try again?"), _("Oops!"), wxYES_NO |wxICON_ERROR);
    int ans = dialog.ShowModal();
    if (ans==wxID_YES)        return wxID_YES;
    return false;
    }

bool success;
if (ItsADir)  
    success = wxFileName::Mkdir(filepath, 0777, wxPATH_MKDIR_FULL); // If a dir is requested. wxPATH_MKDIR_FULL means intermediate dirs are created too
 else
   { wxFile file; success = file.Create(filepath); }                // else ditto a file

if (!success)
  { wxMessageDialog dialog(this, _("Sorry, for some reason this operation failed\nTry again?"),  _("Oops!"), wxYES_NO |wxICON_ERROR);
    int ans = dialog.ShowModal();
    if (ans==wxID_YES)        return wxID_YES;
    return false;
    }

return true;
}

int MyGenericDirCtrl::Rename(wxString& path, wxString& origname, wxString& newname, bool ItsADir, bool dup /*=false*/)
{
int answer;

if ((newname == wxT(".")) || (newname == wxT("..")) ||  (newname == wxT("/")))
  { wxMessageDialog dialog(this, _("Sorry, this name is Illegal\n        Try again?"), _("Oops!"), wxYES_NO |wxICON_ERROR);
    int ans = dialog.ShowModal();
    if (ans==wxID_YES)        return wxID_YES;
    return false;
    }

wxLogNull log;

wxString filepath = path + newname;                         // Combine path & name, & check for pre-existence

FileData newfilepath(filepath);
if (newfilepath.IsValid())
  { wxMessageDialog dialog(this, _("Sorry, that name is already taken.\n            Try again?"), _("Oops!"), wxYES_NO | wxICON_ERROR);
    int ans = dialog.ShowModal();
    if (ans==wxID_YES)        return wxID_YES;
    return false;
    }

wxString OriginalFilepath = path + origname;
CheckDupRen CheckIt(this, OriginalFilepath, wxT(""));
CheckIt.WhattodoifCantRead = DR_Unknown;
FileData stat(OriginalFilepath);
if (stat.IsDir())                                           // If OriginalFilepath is a dir, see if it has offspring
  { wxDir d(OriginalFilepath);
    CheckIt.IsMultiple = (d.HasSubDirs() || d.HasFiles());  // If so, there are >1 items to test so use different dialogs in case of failure
  }

if (!CheckIt.CheckForAccess(dup))  return false; // This checks for Read permission failure.  Pass dup, as if false (renaming) we don't want to check dir contents, only the dir itself

                    // OK, we can go ahead with the Rename or Dup, subcontracting as CheckDupRen is already set up to do so
if (ItsADir)
  { wxString origpath = path + origname, newpath = path + newname;
    answer = CheckDupRen::RenameDir(origpath, newpath, dup);
  }
else
  answer = CheckDupRen::RenameFile(path, origname, newname, dup);

if (!answer)
  { wxMessageDialog dialog(this, _("Sorry, for some reason this operation failed\nTry again?"),  _("Oops!"), wxYES_NO |wxICON_ERROR);
    int ans = dialog.ShowModal();
    if (ans==wxID_YES)        return wxID_YES;
    return false;
    }

return answer;
}

void FileGenericDirCtrl::OnToggleHidden(wxCommandEvent& event)  // Ctrl-H
{
bool hidden = !GetShowHidden();  // Do the toggle
ShowHidden(hidden);              //  & make it happen.  This time, I'm not telling partner, as it's slightly more likely that we'll want hidden dirs but not files

wxString path = GetPath();
UpdateStatusbarInfo(path);       // Make it appear in the statusbar
}

void FileGenericDirCtrl::OnToggleDecimalAwareSort(wxCommandEvent& event)  // ?Sort foo1, foo2, foo11 in decimal order
{
SetIsDecimalSort(!GetIsDecimalSort()); // Do the toggle

RefreshTree(GetPath(), false);
}

void FileGenericDirCtrl::OpenWithKdesu(wxCommandEvent& event)  // Execute, or Launch appropriate application for this file-type
{
wxString filepath = GetPath();                            // Get the selected item.
if (filepath.IsEmpty())  return;                          // Check there IS a valid selection 

FileData stat(filepath);                                  // See if it's a symlink.  If so, try to Open the target, as opening a symlink is unlikely to be sensible
if (stat.IsSymlink())  filepath = stat.GetUltimateDestination();

if (!wxDirExists(filepath))                               // If it's not a dir, try to process it
  { wxString WorkingDir = GetCwd();                       // Change the working dir to the file's path.  Some badly-written programs malfunction otherwise
    wxString thisdir = filepath.BeforeLast(wxFILE_SEP_PATH); 
    SetWorkingDirectory(thisdir);                    
    
    FiletypeManager manager(filepath);  
    manager.SetKdesu();
    CommandArray.Clear();
    manager.Open(CommandArray);
    
    SetWorkingDirectory(WorkingDir);                      // Revert to the original working directory
  }
}

void FileGenericDirCtrl::OnOpen(wxCommandEvent& WXUNUSED(event))  // From Context menu, passes on to Open
{
wxString filepath = GetPath();                            // Get the selected item
DoOpen(filepath);
}

void FileGenericDirCtrl::DoOpen(wxString& filepath) // From Context menu or DClick in pane or TerminalEm
{
FiletypeManager manager;

if (filepath.IsEmpty())  return;                          // Check there IS a valid selection 

DataBase* stat;
bool IsArchive = false;
if (arcman && arcman->IsArchive()) IsArchive = true;
if (IsArchive) stat = new FakeFiledata(filepath);
  else stat = new FileData(filepath);

if (!IsArchive && stat->IsSymlink())
  { filepath = ((FileData*)stat)->GetUltimateDestination();  // See if it's a symlink.  If so, try to Open the target, as opening a symlink is unlikely to be sensible
    if (filepath.IsEmpty()) { delete stat; return; }      // unless of course it's a broken symlink!
  }

bool IsDir =  stat->IsDir();
delete stat; if (IsDir) return;
                                  // If it's not a dir, try to open directly with FiletypeManager::Open()
wxString WorkingDir = GetCwd();                           // Change the working dir to the file's path.  Some badly-written programs malfunction otherwise
wxString thisdir = filepath.BeforeLast(wxFILE_SEP_PATH); 
SetWorkingDirectory(thisdir);                    

manager.Init(filepath);
CommandArray.Clear();
bool result = manager.Open(CommandArray);

SetWorkingDirectory(WorkingDir);                          // Revert to the original working directory
if (!result) return;                                      // If returns false, we're done

                // If we're here, Open returned true, signifying there's applic-choice data in Array/CommandArray
                //  So do a pop-up menu with them
if (!CommandArray.GetCount()) return;                     // after a quick check
wxMenu menu(wxT("                                  "));   // Make a menu & populate it with the known applics
for (size_t n=0; n < manager.Array.GetCount(); ++n)
  menu.Append(SHCUT_RANGESTART + n, _("Open with ") + manager.Array[n]);
menu.AppendSeparator();                                   // Finish with a separator followed by an 'Other' option -> OpenWith
MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_OPENWITH);

wxPoint pt = ScreenToClient(wxGetMousePosition());
PopupMenu(&menu, pt.x, pt.y);                             // Now do the pop-up.  This relays to OpenWithSubmenu() or OpenWith
}


void FileGenericDirCtrl::OnOpenWith(wxCommandEvent& event)
{
wxString filename = GetPath();
if (filename.IsEmpty())  return;

FiletypeManager manager(filename);
manager.OpenWith();

}

void FileGenericDirCtrl::UpdateStatusbarInfo(const wxString& selected)  // Feeds the other overload with a wxArrayString if called with a single selection
{
if (!selected.empty())
  { wxArrayString arr; arr.Add(selected);
    UpdateStatusbarInfo(arr);
  }
}

void FileGenericDirCtrl::UpdateStatusbarInfo(const wxArrayString& selections)  // Writes filter info to statusbar(3), &  selections' names/size (or whatever) to statusbar(2)
{
                            // First the filter info into the 4th statusbar pane
wxString filterinfo, start, filter = GetFilterString(); if (filter.IsEmpty())  filter = wxT("*");  // Translate "" into "*"
start = (DisplayFilesOnly ? wxT(" F") : _(" D F"));           // Create a message string, D for dirs if applicable, F for files, R for recursive subdirs
if (SHOW_RECURSIVE_FILEVIEW_SIZE) start += (_(" R"));         // R for recursive subdirs
start += (GetShowHidden() ? _(" H     ") : wxT("     "));     // Add whether we're displaying Hidden ones
filterinfo.Printf(wxT("%s%s"),start.c_str(), filter.c_str()); // Merge into message string, adding the current filter
MyFrame::mainframe->SetStatusText(filterinfo, 3); 

if (selections.IsEmpty()) 
  { MyFrame::mainframe->SetStatusText(wxT(""), 2); return; }  // Nothing to display if the first-and-only focus click was on a blank line

wxString text;
DataBase* stat; bool IsArchive = arcman && arcman->IsArchive();
size_t count = selections.GetCount();
if (!count) return;

if (count == 1)
  { wxString selected(selections.Item(0));
    if (IsArchive)                                                // Archives have to be treated differently
      { FakeFiledata* fd = arcman->GetArc()->Getffs()->GetRootDir()->FindFileByName(selected);
        if (fd)
          { stat = new FakeFiledata(*fd);                         // We can't just make a new FakeFiledata: the size etc would be unknown; so copy the real one
            SelectedCumSize = stat->Size();
          }
         else    
          { FakeDir* fd = arcman->GetArc()->Getffs()->GetRootDir()->FindSubdirByFilepath(selected);  // If it wasn't a file, try for a dir
            if (!fd) return;
            stat = new FakeDir(*fd);
            if (SHOW_RECURSIVE_FILEVIEW_SIZE)   SelectedCumSize = fd->GetTotalSize();
              else SelectedCumSize = fd->GetSize();
          }
      }
     else  stat = new FileData(selected);

    if (!stat || !stat->IsValid()) 
      { MyFrame::mainframe->SetStatusText(wxT(""), 2); return; } 

    if (!stat->IsSymlink())
      { text.Printf(wxT("%s:  %s"), stat->TypeString().c_str(), selected.AfterLast(wxFILE_SEP_PATH).c_str());
        if (m_StatusbarInfoValid) 
          { text += wxString::Format(wxT("   %s"), ParseSize(SelectedCumSize, true).c_str());
            if (stat->IsDir()) text += wxString::Format(wxT("%s"), SHOW_RECURSIVE_FILEVIEW_SIZE ? _("  of files and subdirectories") : _(" of files"));
          }
      }
     else
      { text.Printf(wxT("%s:  %s"), stat->TypeString().c_str(), selected.AfterLast(wxFILE_SEP_PATH).c_str()); // If possible, add the symlink target
        if (stat->IsFake)
          { if (!stat->GetSymlinkDestination().IsEmpty()) 
              { wxString target; target.Printf(wxT("  --> %s"), stat->GetSymlinkDestination(true).c_str()); text << target; }
          }
          else
           if (stat->GetSymlinkData())
            { wxString target(wxT("  --> ")); size_t len = target.Len(); // Store length without target
              if (stat->GetSymlinkData()->IsValid()) 
                target << stat->GetSymlinkDestination(true);      // 'true' says to display relative symlinks as relative
               else  target << ((FileData*)stat)->GetSymlinkData()->BrokenlinkName;  // If !IsValid, display any known broken-link name
              if (target.Len() > len)                             // See if we've successfully appended a target i.e. added something to "  --> "
                text << target;                                   // If so, use it
            }
      }
  }
 else
  { size_t failures(0), dirs(0), files(0);
    if (IsArchive) SelectedCumSize = 0;
    for (size_t n=0; n < count; ++n)
    { wxString selected(selections.Item(n));
      if (IsArchive)                                                // Archives have to be treated differently
        { FakeFiledata* fd = arcman->GetArc()->Getffs()->GetRootDir()->FindFileByName(selected);
          if (fd)
            { stat = new FakeFiledata(*fd);                         // We can't just make a new FakeFiledata: the size etc would be unknown; so copy the real one
              SelectedCumSize += stat->Size();
            }
           else    
            { FakeDir* fd = arcman->GetArc()->Getffs()->GetRootDir()->FindSubdirByFilepath(selected);  // If it wasn't a file, try for a dir
              if (!fd) return;
              stat = new FakeDir(*fd);
              if (SHOW_RECURSIVE_FILEVIEW_SIZE)   SelectedCumSize += fd->GetTotalSize();
                else SelectedCumSize += fd->GetSize();
            }
        }
       else  stat = new FileData(selected);

      if (!stat || !stat->IsValid()) { ++failures; continue; }

      if (stat->IsDir()) ++dirs;
        else ++files;
    }

    if (failures < count)
      { 
        if (SHOW_RECURSIVE_FILEVIEW_SIZE && dirs)
          text = wxString::Format(_("%s in %zu %s"), ParseSize(SelectedCumSize, true), count, _("files and directories"));
         else
          { text = wxString::Format("%s", ParseSize(SelectedCumSize, true));
            wxString DirOrDirs = (dirs>1 ? _("directories"):_("directory"));
            wxString FileOrFiles = (files>1 ? _("files"):_("file"));
            if (dirs && files) text << wxString::Format(_(" in %zu %s and %zu %s"), dirs, DirOrDirs, files, FileOrFiles);
             else
               { if (dirs) text << wxString::Format(_(" in %zu %s"), dirs, DirOrDirs);
                  else text << wxString::Format(_(" in %zu %s"), files, FileOrFiles);
               }
          }
      }
  }

MyFrame::mainframe->SetStatusText(text, 2); 
delete stat;
}

void FileGenericDirCtrl::UpdateFileDataArray(const wxString& filepath)
{
if (filepath.empty()) return;

FileData* fd = new FileData(filepath);
UpdateFileDataArray(fd);
}

void FileGenericDirCtrl::UpdateFileDataArray(DataBase* fd) // Updates for a change in size etc after a modification
{
if (!fd->IsValid()) { delete fd; return; }

for (size_t n=0; n < CombinedFileDataArray.Count(); ++n)
  { DataBase* item = &CombinedFileDataArray.Item(n);
    wxCHECK_RET(item->IsFake == fd->IsFake, wxT("Trying to replace a DataBase with one of a different type"));
    if (item->IsValid() && item->GetFilepath() == fd->GetFilepath())
      { CumFilesize += fd->Size() - item->Size();               // Note any change in size

        CombinedFileDataArray.Insert(fd, n);
        CombinedFileDataArray.RemoveAt(n+1);
        return;
      } 
  }

delete fd;                                                      // We shouldn't get here, but if so we need to delete the FileData or it'll leak
}

void FileGenericDirCtrl::DeleteFromFileDataArray(const wxString& filepath) // Updates for a deletion
{
if (filepath.empty() || CombinedFileDataArray.IsEmpty()) return;

for (int n = (int)CombinedFileDataArray.Count() - 1; n >= 0 ; --n)
  { DataBase* item = &CombinedFileDataArray.Item(n);
    if (item && item->GetFilepath() == filepath)
      { CumFilesize -= item->Size();                            // Adjust for the change in size
        if (item->IsDir()) --NoOfDirs;
          else --NoOfFiles;

        CombinedFileDataArray.RemoveAt(n);
        return;
      } 
  }
}

void FileGenericDirCtrl::ReloadTree(wxString path, wxArrayInt& IDs)  // Either F5 or after eg a Cut/Paste/Del/UnRedo, to keep treectrl congruent with reality
{
  // First decide whether it's important for this pane to be refreshed ie if it is/was the active pane, or otherwise if a file/dir that's just been acted upon is visible here
wxWindowID pane = GetId();                                      // This is us
for (unsigned int c=0; c < IDs.GetCount(); c++)                 // For every ID passed here,
  if (IDs[c] == pane)  { ReCreateTreeFromSelection(); return; } //  see if it's us. If so, recreate the tree unconditionally

    // There are 2 possibilities.  Either the file/subdir that's just been acted upon is visible, or its not.  Remember, invisible means no TreeItemId exists
wxTreeItemId item = FindIdForPath(path);                        // Translate path into wxTreeItemId
if (!item.IsOk())                                               // If it's not valid,
  { if (path  != wxFILE_SEP_PATH)                               //    & it's not root!
      { while (path.Last() == wxFILE_SEP_PATH && path.Len() > 1)//   and if it's got a terminal '/' or two 
          path.RemoveLast();                                    //         remove it and try again without.
        item = FindIdForPath(path);                             // This is cos the last dir of a branch doesn't have them, yet path DOES, so would never match
      }
  }

if (item.IsOk())                                                // If it's valid, the item is visible, so definitely refresh it
    ReCreateTreeFromSelection();                                // Since this is a fileview, there's no percentage in trying to be clever.  Just redo the tree
}


enum columntype FileGenericDirCtrl::GetSelectedColumn()  // Get the 'selected' col ie that has the up/down icon to control sort-orientation
{
return headerwindow->GetSelectedColumn(); 
}

void FileGenericDirCtrl::SetSelectedColumn(enum columntype col)  // Set the 'selected' col ie that has the up/down icon to control sort-orientation
{
if (col==headerwindow->GetSelectedColumn())                     // See if the clicked column is was already selected
  { headerwindow->ReverseSelectedColumn(); return; }            // If so, reverse its sort order
     
headerwindow->SelectColumn(col);                                // Otherwise this must be a new selection
}

void FileGenericDirCtrl::CreateColumns(bool left)    // Sets the column parameters.  The bool distinguishes a LeftTop from a RtBottom pane
{
int* pArray = (left ? ParentTab->tabdata->Lwidthcol : ParentTab->tabdata->Rwidthcol);  // Tabdata widths were loaded here
bool* pBool = (left ? ParentTab->tabdata->LColShown : ParentTab->tabdata->RColShown);  // Tabdata Do-we-show-this-column?

for (size_t n=0; n < TOTAL_NO_OF_FILEVIEWCOLUMNS; ++n)
  { wxTreeListColumnInfo col(wxT(""));
    col.SetAlignment(wxTL_ALIGN_LEFT);
    int width = pArray[n];
    if (width == 0)                                 // If there's no user-defined width for this column, use the default
        width = GetTreeCtrl()->GetCharWidth() * DEFAULT_COLUMN_WIDTHS[n];
    col.SetDesiredWidth(width);                     // This is the width to use when/if the column is visible

    width = pBool[n]==true ? width : 0;             // If it's not supposed to be visible atm, zero it
    col.SetWidth(width);
    headerwindow->AddColumn(col);
  }
headerwindow->SizeColumns();

enum columntype selectedcol = (columntype)(left ? ParentTab->tabdata->Lselectedcol : ParentTab->tabdata->Rselectedcol);  // The tabdata's selected col
headerwindow->SelectColumn(selectedcol);

bool reversesort = (left ? ParentTab->tabdata->Lreversesort : ParentTab->tabdata->Rreversesort);  // Was the sort-order reversed
if (reversesort)    headerwindow->ReverseSelectedColumn();
bool decimalsort = (left ? ParentTab->tabdata->Ldecimalsort : ParentTab->tabdata->Rdecimalsort);  // Similarly should we decimal-sort
SetIsDecimalSort(decimalsort);

ReCreateTree();                                     // Now we have to redo the tree, or the col selection/reverse doesn't happen
}

void FileGenericDirCtrl::OnColumnSelectMenu(wxCommandEvent& event)
{
headerwindow->OnColumnSelectMenu(event);
}

void FileGenericDirCtrl::HeaderWindowClicked(wxListEvent& event)  // A L click occurred on a header window column to select or reverse-sort
{
int col = event.GetColumn();
if (col >= 0 && col < (int)headerwindow->GetColumnCount()) // If the column is valid, change the master column (or reverse-sort if it's the same one) 
  SetSelectedColumn((enum columntype)col);          // If invalid it'll be from a column "hide & change selected" situation, so we've already done this
ReCreateTree();                                     // This makes the change visible
#if !defined(__WXGTK__)
  headerwindow->Refresh();                          // A gtk1.2 bug for a change: without this, the header doesn't scroll back to 0, yet the fileview does
#endif
}


void FileGenericDirCtrl::SortStats(FileDataObjArray& array)  // Sorts the FileData array according to column selection
{
static const wxString NUMBERS = wxT("0123456789");
bool reverse = headerwindow->GetSortOrder();
switch(headerwindow->GetSelectedColumn())                               // Since the col selection defines the sort method required
  { case filename:    if (reverse)                                      // Here we're just sorting on the filepath, so use it direct, reverse or otherwise
                        { if (GetIsDecimalSort())
                           { for (size_t n = 0; n < array.GetCount(); ++n)     // For each array entry,
                              { wxString filename = array[n].GetFilename().BeforeFirst(wxT('.'));
                                size_t pos = filename.find_last_not_of(NUMBERS);
                                if (pos != wxString::npos)
                                  { array[n].sortstring = filename.Mid(0, pos+1);
                                    wxULongLong_t ull((wxULongLong_t)-1);
                                    filename.Mid(pos+1).ToULongLong(&ull);
                                    array[n].sortULL = ull;
                                  }
                              }
                             array.Sort((*FilenameULLReverseCompareFunc));
                            }
                          else
                           array.Sort((*FilenameReverseCompareFunc));
                        }
                       else 
                        { if (GetIsDecimalSort())
                           { for (size_t n = 0; n < array.GetCount(); ++n)     // For each array entry,
                              { wxString filename = array[n].GetFilename().BeforeFirst(wxT('.'));
                                size_t pos = filename.find_last_not_of(NUMBERS);
                                if (pos != wxString::npos)
                                  { array[n].sortstring = filename.Mid(0, pos+1);
                                    wxULongLong_t ull((wxULongLong_t)-1);
                                    filename.Mid(pos+1).ToULongLong(&ull);
                                    array[n].sortULL = ull;
                                  }
                              }
                            array.Sort(FilenameULLCompareFunc);
                           }
                          else 
                            array.Sort((*FilenameCompareFunc));
                        }
                        break;

                       
    case ext:         for (size_t n = 0; n < array.GetCount(); ++n)
                        { wxString ext = (array[n].GetFilename().Mid(1)).AfterFirst(wxT('.'));  // Note the Mid(1) to avoid hidden files being called ext.s!
                          array[n].sortstring = ext;                    // If we define an 'ext' as 'after the first dot', that's it
                          if (EXTENSION_START > 0)                      // but for others:
                            { size_t pos = ext.rfind(wxT('.'));
                              if (pos != wxString::npos)
                                { array[n].sortstring = ext.Mid(pos+1); // We've found a last dot. Store the remaining string
                                  if (EXTENSION_START == 1)             // and then, if so configured, look for a penultimate one
                                      { pos = ext.rfind(wxT('.'), pos-1);
                                        if (pos != wxString::npos)
                                          array[n].sortstring = ext.Mid(pos+1);
                                      }
                                }
                            }
                        }
                      if (reverse)
                            array.Sort((*ReverseCompareFunc));
                       else  array.Sort((*CompareFunc)); break;

    case filesize:    for (size_t n = 0; n < array.GetCount(); ++n)     // For each array entry,
                        array[n].sortULL = array[n].Size();             // We're sorting on the size, so put it in sortULL (wxULongLong)
                      if (reverse)
                            array.Sort(SortULLReverseCompareFunction);
                       else  array.Sort(SortULLCompareFunction); break;
  
    case modtime:      for (size_t n = 0; n < array.GetCount(); ++n)    // For each array entry,
                        array[n].sortint = array[n].ModificationTime(); // We're sorting on an 'int' variable, so put it in sortint
                      if (reverse)
                            array.Sort(SortintReverseCompareFunction);
                       else  array.Sort(SortintCompareFunction); break;
  
    case permissions:    for (size_t n = 0; n < array.GetCount(); ++n)
                        array[n].sortstring = array[n].PermissionsToText();
                      if (reverse)
                            array.Sort((*ReverseCompareFunc));
                       else  array.Sort((*CompareFunction)); break;
                       
    case owner:       for (size_t n = 0; n < array.GetCount(); ++n)
                        { array[n].sortstring = array[n].GetOwner(); 
                          if (array[n].sortstring.IsEmpty())  array[n].sortstring = wxT("zzz");
                        }
                      if (reverse)
                            array.Sort((*ReverseCompareFunc));
                       else  array.Sort((*CompareFunction)); break;
                      
    case group:       for (size_t n = 0; n < array.GetCount(); ++n)
                        { array[n].sortstring = array[n].GetGroup(); 
                          if (array[n].sortstring.IsEmpty())  array[n].sortstring = wxT("zzz");
                        }
                      if (reverse)
                            array.Sort((*ReverseCompareFunc));
                       else  array.Sort((*CompareFunction)); break;
                       
    case linkage:     for (size_t n = 0; n < array.GetCount(); ++n)
                        if (array[n].IsSymlink())
                          array[n].sortstring = array[n].GetSymlinkDestination(); else array[n].sortstring = wxT("zzz");
                      if (reverse)
                            array.Sort((*ReverseCompareFunc));
                       else  array.Sort((*CompareFunc));
  }
}

void FileGenericDirCtrl::SelectFirstItem()
{
wxTreeItemId rootId = GetTreeCtrl()->GetRootItem();
if (rootId.IsOk())
  { wxTreeItemIdValue cookie;
    wxTreeItemId firstId = GetTreeCtrl()->GetFirstChild(GetTreeCtrl()->GetRootItem(), cookie);
    if (firstId.IsOk())  GetTreeCtrl()->SelectItem(firstId);
  }
}

bool FileGenericDirCtrl::UpdateTreeFileModify(const wxString& filepath) // Called when a wxFSW_EVENT_MODIFY arrives
{
wxCHECK_MSG(!filepath.empty(), true, wxT("Called UpdateTreeFileModify() with empty fpath"));

if (GetSelectedColumn() != filesize) 
  return false; // We only need to recreate the tree to sort by size. Otherwise let the caller do a simpler update

if (StripSep(filepath).BeforeLast(wxFILE_SEP_PATH) == StripSep(startdir)) // We know it's a fileview, so no need to look at other situations
    ReCreateTreeFromSelection();

return true;
}

bool FileGenericDirCtrl::UpdateTreeFileAttrib(const wxString& filepath) // Called when a wxFSW_EVENT_ATTRIB arrives
{
wxCHECK_MSG(!filepath.empty(), true, wxT("Called UpdateTreeFileAttrib() with empty fpath"));

int sorttype = GetSelectedColumn();
if ((sorttype < modtime) || (sorttype > group)) 
  return false; // We only need to recreate the tree to sort by datetime, permissions, owner & group. Otherwise let the caller do a simpler update

if (StripSep(filepath).BeforeLast(wxFILE_SEP_PATH) == StripSep(startdir)) // We know it's a fileview, so no need to look at other situations
    ReCreateTreeFromSelection();

return true;
}

void MyGenericDirCtrl::OnFilter(wxCommandEvent& WXUNUSED(event))
{
wxDialog dlg;
wxXmlResource::Get()->LoadDialog(&dlg, this, wxT("FilterDlg"));

wxCheckBox* FilesChk = (wxCheckBox*)dlg.FindWindow(wxT("FilesOnly"));
if (fileview==ISLEFT)
  { wxSizer* sizer = FilesChk->GetContainingSizer();        // If we're a dirview, remove this checkbox, as we can't display files anyway
    FilesChk->Hide();                                       // For some reason we seem to have to hide it first
    if (sizer->Detach(FilesChk)) FilesChk->Destroy();
  }
 else FilesChk->SetValue(DisplayFilesOnly);                 // If it's a fileview, Set the checkbox according to the pane's current status
   
if (!FilterHistory.Count()) FilterHistory.Add(wxT("*"));    // If there's no history, provide '*'

wxComboBox* combo = (wxComboBox*)dlg.FindWindow(wxT("Combo"));
for (size_t n=0; n < FilterHistory.Count(); ++n) combo->Append(FilterHistory[n]); // Add history to combobox
if (FilterHistory.Count()) combo->SetValue(FilterHistory[0]);                     // Select the last-used value

HelpContext = HCfilter;
if (dlg.ShowModal() != wxID_OK)    { HelpContext = HCunknown; return; }
HelpContext = HCunknown;

bool FilesOnly = false;
if (fileview==ISRIGHT) FilesOnly = FilesChk->GetValue();    // If we're a fileview, see if we should be displaying files only

wxString selection = combo->GetValue();
if (((wxCheckBox*)dlg.FindWindow(wxT("Reset")))->GetValue() == true) // If the Reset checkbox is ticked, kill any selection from the combobox
  selection.Empty();
  
int index = FilterHistory.Index(selection, false);          // See if this command already exists in the history array
if (index != wxNOT_FOUND)
  FilterHistory.RemoveAt(index);                            // If so, remove it.  We'll add it back into position zero  
FilterHistory.Insert(selection, 0);                         // Either way, insert into position zero

if (selection ==  wxT("*")) selection.Empty();              // Since emptystring is equivalent

SetFilterArray(selection, FilesOnly);                       // Set this pane's filter
wxArrayInt IDs;  IDs.Add(GetId()); MyFrame::mainframe->OnUpdateTrees(startdir, IDs, wxT(""), true);  // & refresh

if (((wxCheckBox*)dlg.FindWindow(wxT("AllPanes")))->GetValue() == true)  // If the AllPanes checkbox is ticked,
  { wxArrayInt IDs;  IDs.Add(partner->GetId());
    if (partner != NULL)                                    // Do our twin
      { partner->SetFilterArray(selection, FilesOnly);
        wxArrayInt IDs;  IDs.Add(partner->GetId());  MyFrame::mainframe->OnUpdateTrees(partner->startdir, IDs, wxT(""), true);
      }
      
    if (ParentTab->GetSplitStatus() != unsplit)             // If the tab is split, do the other side too
      { MyGenericDirCtrl* oppositepane = GetOppositeDirview();
        if (oppositepane == NULL) return;
        
        oppositepane->SetFilterArray(selection, FilesOnly);
        wxArrayInt IDs;  IDs.Add(oppositepane->GetId()); MyFrame::mainframe->OnUpdateTrees(oppositepane->startdir, IDs, wxT(""), true);
        if (oppositepane->partner != NULL)  
          { oppositepane->partner->SetFilterArray(selection, FilesOnly); 
            wxArrayInt IDs;  IDs.Add(oppositepane->partner->GetId());  MyFrame::mainframe->OnUpdateTrees(oppositepane->partner->startdir, IDs, wxT(""), true);
          }
      }
  }
}

void MyGenericDirCtrl::SetFilterArray(const wxString& filterstring, bool FilesOnly /*=false*/)  // Parse the string into individual filters & store in array
{
GetFilterArray().Clear();

if (!filterstring.IsEmpty())
  { wxStringTokenizer tkz(filterstring, wxT(",|"), wxTOKEN_STRTOK);  // The filters may be separated by ',' or '|' (but not spaces. What if "ext_with_a_space.ex t")
    while (tkz.HasMoreTokens())            
      { wxString fil  =  tkz.GetNextToken(); fil.Trim(true); fil.Trim(false); GetFilterArray().Add(fil); }
  }
 else GetFilterArray().Add(wxEmptyString);
 
DisplayFilesOnly = FilesOnly;
}

wxString MyGenericDirCtrl::GetFilterString()  // Turns the filterarray back into "*.jpg | *.png"
{
wxString CombinedFilters;

for (size_t n=0; n < GetFilterArray().GetCount(); ++n)                   // Go thru the list of filters, appending to the CombinedFilters string
  { wxString fil = GetFilterArray().Item(n);
    if (!fil.IsEmpty())
      { if (!CombinedFilters.IsEmpty())  CombinedFilters += wxT(" | ");  // If there's already data, add a separator
        CombinedFilters += fil;
      }
  }

return CombinedFilters;
}

void MyGenericDirCtrl::OnProperties(wxCommandEvent& WXUNUSED(event))
{
static const wxChar* MiscPermissionStrings[] = { wxT("OtherExec"), wxT("OtherWrite"), wxT("OtherRead"), wxT("GroupExec"), wxT("GroupWrite"), wxT("GroupRead"),
                                    wxT("UserExec"), wxT("UserWrite"), wxT("UserRead"), wxT("Sticky"), wxT("Sguid"), wxT("Suid"),
              wxT("PermStatic1"), wxT("PermStatic2"), wxT("PermStatic3"), wxT("PermStatic4"), wxT("PermStatic5"), wxT("PermStatic6"), wxT("PermStatic7") };

wxArrayString filepaths; GetMultiplePaths(filepaths); if (filepaths.IsEmpty()) return;
    // Though there might be multiple selections, focus on the 1st for convenience.  Multiple makes sense only chmod & chown.
FileData* stat = new FileData(filepaths[0]);  if (!stat->IsValid()) { delete stat; return; }


wxDialog dlg;
if (!stat->IsSymlink()) wxXmlResource::Get()->LoadDialog(&dlg, MyFrame::mainframe, wxT("PropertiesDlg"));
 else 
  { if (stat->IsSymlinktargetASymlink())
       { wxXmlResource::Get()->LoadDialog(&dlg, MyFrame::mainframe, wxT("UltSymlinkPropertiesDlg")); // The multiple-symlink version has 2 extra sections
        wxTextCtrl* UltTarget = (wxTextCtrl*)dlg.FindWindow(wxT("UltLinkTargetText"));
        UltTarget->SetValue(stat->GetUltimateDestination());
        dlg.Connect(XRCID("UltLinkTargetBtn"), wxEVT_COMMAND_BUTTON_CLICKED, (wxObjectEventFunction)&MyGenericDirCtrl::OnLinkTargetButton);
      }
     else    wxXmlResource::Get()->LoadDialog(&dlg, MyFrame::mainframe, wxT("SymlinkPropertiesDlg")); // The Symlink version has 1 extra section
  }
LoadPreviousSize(&dlg, wxT("PropertiesDlg"));

PropertyDlg = &dlg;  // Store the address of dlg, in case OnLinkTargetButton needs it

((wxNotebook*)dlg.FindWindow(wxT("PropertiesNB")))->SetSelection(MyFrame::mainframe->PropertiesPage); // Set the notebk page to that last used

wxTextCtrl* Name = (wxTextCtrl*)dlg.FindWindow(wxT("Name")); Name->SetValue(stat->GetFilename());

wxStaticText* Path = (wxStaticText*)dlg.FindWindow(wxT("Path"));
wxString path = filepaths[0].BeforeLast(wxFILE_SEP_PATH) + wxFILE_SEP_PATH;
Path = dynamic_cast<wxStaticText*>(InsertEllipsizingStaticText(path, Path)); // Most paths are too big for the Path static, so use an elipsizing statictext
wxCHECK_RET(Path, "InsertEllipsizingStaticText() failed");

wxTextCtrl* UserText = (wxTextCtrl*)dlg.FindWindow(wxT("UserText"));
wxComboBox* UserCombo = (wxComboBox*)dlg.FindWindow(wxT("UserCombo"));

bool isroot = (getuid()==0);                            // Are we root?
if (isroot)                                             // Only root can change ownership
  { wxSizer* sizer = UserText->GetContainingSizer();    // There is both a wxTextCtrl & a wxComboBox in this sizer.  If we're root, remove the textctrl
    UserText->Hide();                                   // For some reason we seem to have to hide it first,
    if (sizer->Detach(UserText)) UserText->Destroy();   //  & then remove it! And no, Layout() didn't work
    wxArrayString usernames; GetAllUsers(usernames);    // Use this global function to get list of usernames
    for (size_t n=0; n < usernames.Count(); ++n) UserCombo->Append(usernames[n]);  // Add them to combobox
    wxString owner = stat->GetOwner(); if (owner.IsEmpty()) owner.Printf(wxT("%u"), stat->OwnerID());
    UserCombo->SetValue(owner);                         // Select the current owner;  his name if available, else his id
  }
 else
  { wxSizer* sizer = UserCombo->GetContainingSizer();   // Similarly if we're not root:  remove the combobox
    UserCombo->Hide();
    if (sizer->Detach(UserCombo)) UserCombo->Destroy();
    wxString owner = stat->GetOwner(); if (owner.IsEmpty()) owner.Printf(wxT("%u"), stat->OwnerID());
    UserText->SetValue(owner);
  }

wxComboBox* Group = (wxComboBox*)dlg.FindWindow(wxT("Group"));
wxArrayString groupnames; GetProcessesGroupNames(groupnames); // Use this global function to get list of alternative group-names
for (size_t n=0; n < groupnames.Count(); ++n) Group->Append(groupnames[n]);  // Add them to combobox
Group->SetValue(stat->GetGroup());                            // Set combobox to current group


((wxStaticText*)dlg.FindWindow(wxT("Type")))->SetLabel(stat->TypeString());

((wxStaticText*)dlg.FindWindow(wxT("SizeStatic")))->SetLabel(stat->Size().ToString() + wxT(" B"));

wxDateTime time(stat->ModificationTime()); 
((wxStaticText*)dlg.FindWindow(wxT("ModStatic")))->SetLabel(time.Format(wxT("%x %X")));

time.Set(stat->AccessTime()); 
((wxStaticText*)dlg.FindWindow(wxT("AccessStatic")))->SetLabel(time.Format(wxT("%x %X")));

time.Set(stat->ChangedTime()); 
((wxStaticText*)dlg.FindWindow(wxT("ChangedStatic")))->SetLabel(time.Format(wxT("%x %X")));

size_t perms = stat->GetPermissions();                  // Get current permissions in numerical form
for (int n=0; n < 12; ++n)                              //  & set each checkbox appropriately.  Note the clever-clever use of << to select each bit
  ((wxCheckBox*)dlg.FindWindow(MiscPermissionStrings[n]))->SetValue(perms & (1 << n));

if (stat->IsSymlink())
  { if (stat->TypeString() == wxT("Broken Symbolic Link")) // If a broken symlink, disable the GoToTarget button
      { wxWindow* win = dlg.FindWindow(wxT("LinkTargetBtn")); if (win) win->Disable(); }
    ((wxTextCtrl*)dlg.FindWindow(wxT("LinkTargetText")))->SetValue(stat->GetSymlinkDestination(true));  // 'true' says to display relative symlinks as relative
    dlg.Connect(XRCID("LinkTargetBtn"), wxEVT_COMMAND_BUTTON_CLICKED, (wxObjectEventFunction)&MyGenericDirCtrl::OnLinkTargetButton);
    dlg.Connect(XRCID("Browse"), wxEVT_COMMAND_BUTTON_CLICKED, (wxObjectEventFunction)&MyGenericDirCtrl::OnBrowseButton);
  }
                                    // Now do the Permissions/Ownership page
bool writable = stat->CanTHISUserRename();
if (!writable)
  { Name->Enable(false); Path->Enable(false); }

bool chmodable = stat->CanTHISUserChmod();
if (!chmodable)
  { for (int n=0; n < 19; ++n)
      ((wxCheckBox*)dlg.FindWindow(MiscPermissionStrings[n]))->Enable(false);
    
    Group->Enable(false);                               // The requirements for changing group are the same as for chmod
  }
      
if (!isroot)    UserText->Enable(false);                // Only root can change ownership

wxCheckBox* Recursebox = (wxCheckBox*)dlg.FindWindow(wxT("RecurseCheck"));
bool queryrecurse=false;
for (size_t sel=0; sel < filepaths.GetCount(); ++sel)
  { FileData selstat(filepaths[sel]); if (selstat.IsDir()) queryrecurse = true; }
Recursebox->Show(queryrecurse);                         // Don't show the Apply to Descendants checkbox unless >0 selection's a dir!

                                    // Now do the Esoterica page
((wxStaticText*)dlg.FindWindow(wxT("DeviceID")))->SetLabel(wxString::Format(wxT("%d"), (int)stat->GetDeviceID()));
((wxStaticText*)dlg.FindWindow(wxT("Inode")))->SetLabel(wxString::Format(wxT("%u"), (uint)stat->GetInodeNo()));
((wxStaticText*)dlg.FindWindow(wxT("Hardlinks")))->SetLabel(wxString::Format(wxT("%u"), (uint)stat->GetHardLinkNo()));
((wxStaticText*)dlg.FindWindow(wxT("BlockNo")))->SetLabel(wxString::Format(wxT("%u"), (uint)stat->GetBlockNo()));
((wxStaticText*)dlg.FindWindow(wxT("Blocksize")))->SetLabel(wxString::Format(wxT("%lu"), stat->GetBlocksize()));
((wxStaticText*)dlg.FindWindow(wxT("OwnerID")))->SetLabel(wxString::Format(wxT("%u"), stat->OwnerID()));
((wxStaticText*)dlg.FindWindow(wxT("GroupID")))->SetLabel(wxString::Format(wxT("%u"), stat->GroupID()));

wxTextCtrl* filecommandoutput = dynamic_cast<wxTextCtrl*>(dlg.FindWindow("FileCommandOutput"));
if (filecommandoutput)
 { wxArrayString output, errors;
   long ans = wxExecute("sh -c \"(cd " + stat->GetPath() + " && file ./" + stat->GetFilename() + ')' + '\"', output,errors); // Do 'file ./filename' so the output isn't spammed by long paths
   if (ans == 0 && !output.IsEmpty())
      filecommandoutput->SetValue(output.Item(0));
 }

bool ClusterWasNeeded = UnRedoManager::StartClusterIfNeeded();  // Start a new UnRedoManager cluster. Doing it here catches both rename & changesymlinktarget
do
  { HelpContext = HCproperties;
    int ans = dlg.ShowModal();
    SaveCurrentSize(&dlg, wxT("PropertiesDlg"));
    if (ans != wxID_OK)    
      { MyFrame::mainframe->PropertiesPage = ((wxNotebook*)dlg.FindWindow(wxT("PropertiesNB")))->GetSelection();  // Save the notebook pageno
        HelpContext = HCunknown;
        delete stat; if (ClusterWasNeeded) UnRedoManager::EndCluster(); return; // If user cancelled, abort
      }
    HelpContext = HCunknown;
    if (writable)                                               // If user was allowed to change the filename
      { if (Name->GetValue() != stat->GetFilename())            //   & has,
          { bool ItsADir = stat->IsDir();
            wxString path = Path->GetLabel() + wxFILE_SEP_PATH, oldname = stat->GetFilename(), newname = Name->GetValue();
            int ans = Rename(path, oldname, newname, ItsADir);
            if (ans == wxID_YES) continue;                      // If there's a filename problem, redo the dialog 
        
            if (ans)
              { wxString newstartdir = path + newname;
                bool renamingstartdir = false;
                wxArrayInt IDs; IDs.Add(GetId());                     // Update the panes
                if (path.BeforeLast(wxFILE_SEP_PATH) == startdir)     // If we've just renamed startdir, need to do things differently
                  { MyFrame::mainframe->OnUpdateTrees(path, IDs, newstartdir);  // Newstartdir will be substituted for the old one
                    renamingstartdir = true;
                  }
                 else MyFrame::mainframe->OnUpdateTrees(path, IDs);

                UnRedoRen* UnRedoptr;
                if (ItsADir)
                  { path += oldname;
                    if (renamingstartdir)
                      UnRedoptr = new UnRedoRen(path, newstartdir, wxT(""), newstartdir, ItsADir, IDs);  // If renamingstartdir, use 4th string as notification
                     else
                      UnRedoptr = new UnRedoRen(path, newstartdir, wxT(""), wxT(""), ItsADir, IDs); // Otherwise make an UnRedoRen the standard way
                  }
                 else UnRedoptr = new UnRedoRen(path, path, oldname, newname, ItsADir, IDs);  
                UnRedoManager::AddEntry(UnRedoptr);
                                          // Since we've just changed the name, we need to recreate stat to match. If not, there'd be failures below
                delete stat; stat = new FileData(newstartdir);  if (!stat->IsValid()) { delete stat; if (ClusterWasNeeded) UnRedoManager::EndCluster(); return; }
              }
          }
      }

    if (stat->IsSymlink())
      { wxString dest = ((wxTextCtrl*)dlg.FindWindow(wxT("LinkTargetText")))->GetValue();
        if (!dest.IsEmpty() && dest != stat->GetSymlinkDestination(true))
          { wxString absolute(dest);
            if (!dest.StartsWith( wxString(wxFILE_SEP_PATH) ))       
              absolute = StrWithSep(GetActiveDirPath()) + dest; // FileDatas can't cope with relative names so temporarily make absolute
            
            FileData deststat(absolute);
            if (!deststat.IsValid())                          // If the desired new target doesn't exist
              { wxMessageDialog dialog(this, _("The new target for the link doesn't seem to exist.\nTry again?"), wxT(""),wxYES_NO | wxICON_QUESTION);
                if (dialog.ShowModal() == wxID_YES) continue;   // Try again
              }
             else
              { wxString currentfilepath = stat->GetFilepath(), oldtarget = stat->GetSymlinkDestination(true);
                bool result = CheckDupRen::ChangeLinkTarget(currentfilepath, dest);
                if (!result) wxMessageBox(_("For some reason, changing the symlink's target seems to have failed.  Sorry!"));
                  else
                    { wxArrayInt IDs; IDs.Add(GetId());
                      MyFrame::mainframe->OnUpdateTrees(currentfilepath, IDs, "", true); // 'true' forces a refresh in case the symlink icon needs to change to/from broken

                      UnRedoChangeLinkTarget *UnRedoptr = new UnRedoChangeLinkTarget(currentfilepath, IDs, dest, oldtarget);
                      UnRedoManager::AddEntry(UnRedoptr);
                      
                                          // Since we've just deleted/recreated the link, we need to recreate stat to match. If not, there'd be failures below                      
                      delete stat; stat = new FileData(currentfilepath);  if (!stat->IsValid()) { delete stat; if (ClusterWasNeeded) UnRedoManager::EndCluster(); return; }
                    }
              }
          }
  }
    break;
  }
    while(true);

bool Recurse = Recursebox->GetValue();    // (If >0 selection is a dir) are we supposed to implement changes recursively?
size_t totalpsuccesses=0, totalpfailures=0, totalosuccesses=0, totalofailures=0, totalgsuccesses=0, totalgfailures=0;
wxArrayInt IDs; IDs.Add(GetId());

for (size_t sel=0; sel < filepaths.GetCount(); ++sel)
  { size_t psuccesses=0, pfailures=0, osuccesses=0, ofailures=0, gsuccesses=0, gfailures=0;
    FileData* stat = new FileData(filepaths[sel]);  if (!stat->IsValid()) { delete stat; continue; }
    
    if (isroot)                               // Only root can change owner
      { long uid; wxString newuser = UserCombo->GetValue(); // This will usually be a name, but...
        if (newuser.ToLong(&uid))                           //   check if a number was entered e.g. 999 in a *buntu livecd
            RecursivelyChangeAttributes(stat->GetFilepath(), IDs, uid, osuccesses, ofailures, ChangeOwner, Recurse);  // If so, use it
          else
          { struct passwd p, *result;     // Otherwise find the pw-struct for the selected user-name
            static const size_t bufsize = 4096; char buf[bufsize];
            getpwnam_r(newuser.mb_str(wxConvUTF8), &p, buf, bufsize, &result);
            if (result != NULL)               //  Try to change to this user. Nothing will happen if no change was made
              RecursivelyChangeAttributes(stat->GetFilepath(), IDs, result->pw_uid, osuccesses, ofailures, ChangeOwner, Recurse);
          }
      }

    if (chmodable)                            // Now group & permissions, which have the same Access requirement
      { long gid; wxString newgroup = Group->GetValue();
        if (newgroup.ToLong(&gid))
            RecursivelyChangeAttributes(stat->GetFilepath(), IDs, gid, gsuccesses, gfailures, ChangeGroup, Recurse);
          else
          { struct group g, *result;
            static const size_t bufsize = 4096; char buf[bufsize];
            getgrnam_r(newgroup.mb_str(wxConvUTF8), &g, buf, bufsize, &result);
            if (result != NULL)
              RecursivelyChangeAttributes(stat->GetFilepath(), IDs, result->gr_gid, gsuccesses, gfailures, ChangeGroup, Recurse);
          }

        size_t newperms = 0;
        for (int n=0; n < 12; ++n)            // Create a new permissions int by <<ing each checkbox appropriately
          newperms |= (((wxCheckBox*)dlg.FindWindow(MiscPermissionStrings[n]))->GetValue() << n);
        RecursivelyChangeAttributes(stat->GetFilepath(), IDs, newperms, psuccesses, pfailures, ChangePermissions, Recurse);  // Try to change permissions, recursively if requested. Nothing will happen if no change was made        
      }
      
    delete stat;
    totalpsuccesses += psuccesses; totalosuccesses += osuccesses; totalgsuccesses += gsuccesses;
    totalpfailures += pfailures; totalofailures += ofailures; totalpfailures += gfailures; 
  }
                                    // Sum all possible successes/failures, in case eg both owner & group were changed
size_t successes = totalpsuccesses+totalosuccesses+totalgsuccesses; size_t failures = totalpfailures+totalofailures+totalgfailures;
if (successes + failures)                     // Only display if something was actually changed (or attempts failed)
  { wxString msg; msg.Printf(wxT("%u%s%u%s"), (uint)successes, _(" alterations successfully made, "), (uint)failures, _(" failures"));
    BriefLogStatus bls(msg);
    
    if (successes)  MyFrame::mainframe->OnUpdateTrees(stat->GetFilepath(), IDs); // Update the panes if appropriate
  }

if (ClusterWasNeeded) UnRedoManager::EndCluster();

int page = ((wxNotebook*)dlg.FindWindow(wxT("PropertiesNB")))->GetSelection();
MyFrame::mainframe->PropertiesPage = page<0 ? 0 : page; // Save the notebook pageno
delete stat;
}

void MyGenericDirCtrl::OnLinkTargetButton(wxCommandEvent& event)  // // If symlink-target button clicked in Properties dialog
{
MyGenericDirCtrl* active = MyFrame::mainframe->GetActivePane();   // This method behaves as though it were static:  we can't use 'this' as the active pane
if (active) active->LinkTargetButton(event);                      // So jump to where we can!
}

void MyGenericDirCtrl::LinkTargetButton(wxCommandEvent& event)    // // Recreate tree to goto selected link's target (or ultimate target)
{
if (PropertyDlg)  PropertyDlg->EndModal(wxID_OK);                 // First shutdown the originating dialog

GoToSymlinkTarget(event.GetId() == XRCID("UltLinkTargetBtn"));    // Use another method to do the GoTo, as it's also used by a context menu entry
}

void MyGenericDirCtrl::GoToSymlinkTarget(bool ultimate)    // // Goto selected link's target (or ultimate target)
{
FileData stat(GetPath()); if (!(stat.IsValid() && stat.GetSymlinkData()->IsValid())) return;  // Locate the selected link's target (or ultimate target)

wxString SymlinkTarget, storedSymlinkTarget;
if (ultimate)   SymlinkTarget = stat.GetUltimateDestination();
 else SymlinkTarget = stat.MakeAbsolute(stat.GetSymlinkDestination(), stat.GetPath());
 
storedSymlinkTarget = SymlinkTarget;                              // OnOutsideSelect() alters the string
OnOutsideSelect(SymlinkTarget); 
SetPath(storedSymlinkTarget);                                     // OnOutsideSelect() only does the dir, not the file selection
}

void MyGenericDirCtrl::OnBrowseButton(wxCommandEvent& WXUNUSED(event))    // // If symlink-target Browse button clicked in Properties dialog
{
MyGenericDirCtrl* active = MyFrame::mainframe->GetActivePane();   // This method behaves as though it were static:  we can't use 'this' as the active pane
if (active != NULL) active->BrowseButton();                       // So jump to where we can!
}

void MyGenericDirCtrl::BrowseButton()    // //  The Browse button was clicked in Properties dialog
{
if (PropertyDlg == NULL) return;

wxString currenttargetpath, currenttargetname;

FileData stat(GetPath());                                         // We need to decide with which filepath to load the dialog
if (stat.GetSymlinkData()->IsValid())                             // Providing it's valid, use the symlink's current target
  { if (stat.GetSymlinkData()->IsDir())
      { currenttargetpath = stat.GetSymlinkDestination(); currenttargetname = wxEmptyString; }
     else    
        { currenttargetpath = stat.GetSymlinkDestination().BeforeLast(wxFILE_SEP_PATH);
        currenttargetname = stat.GetSymlinkDestination().AfterLast(wxFILE_SEP_PATH);
      }
  }
 else                                                             // Otherwise use the symlink itself
   { if (stat.IsDir())
      { currenttargetpath = stat.GetFilepath(); currenttargetname = wxEmptyString; }
     else    
        { currenttargetpath = stat.GetFilepath().BeforeLast(wxFILE_SEP_PATH);
        currenttargetname = stat.GetFilepath().AfterLast(wxFILE_SEP_PATH);
      }
  }

FileDirDlg fdlg(PropertyDlg, FD_SHOWHIDDEN, currenttargetpath, _("Choose a different file or dir to be the new target"));
if (fdlg.ShowModal() != wxID_OK) return;

wxString newtarget = fdlg.GetFilepath();

((wxTextCtrl*)PropertyDlg->FindWindow(wxT("LinkTargetText")))->SetValue(newtarget);  // Paste into the textctrl
}


BEGIN_EVENT_TABLE(FileGenericDirCtrl,MyGenericDirCtrl)
  EVT_SIZE(FileGenericDirCtrl::OnSize)
  EVT_MENU(SHCUT_NEW, FileGenericDirCtrl::NewFile)
  EVT_MENU(SHCUT_SELECTALL, FileGenericDirCtrl::OnSelectAll)
  
  EVT_MENU(SHCUT_ARCHIVE_APPEND, FileGenericDirCtrl::OnArchiveAppend)
  EVT_MENU(SHCUT_ARCHIVE_TEST, FileGenericDirCtrl::OnArchiveTest)
  
  EVT_MENU(SHCUT_TOGGLEHIDDEN, FileGenericDirCtrl::OnToggleHidden)
  EVT_MENU(SHCUT_DECIMALAWARE_SORT, FileGenericDirCtrl::OnToggleDecimalAwareSort)
  EVT_MENU(SHCUT_OPENWITH, FileGenericDirCtrl::OnOpenWith)
  EVT_MENU(SHCUT_OPEN, FileGenericDirCtrl::OnOpen)
  EVT_MENU(SHCUT_OPENWITH_KDESU, FileGenericDirCtrl::OpenWithKdesu)
  EVT_MENU_RANGE(SHCUT_RANGESTART,SHCUT_RANGEFINISH, FileGenericDirCtrl::OpenWithSubmenu)
  EVT_MENU_RANGE(SHCUT_SHOW_COL_EXT, SHCUT_SHOW_COL_CLEAR, FileGenericDirCtrl::OnColumnSelectMenu)
  
  EVT_LIST_COL_CLICK(-1, FileGenericDirCtrl::HeaderWindowClicked)

  EVT_CONTEXT_MENU(FileGenericDirCtrl::ShowContextMenu)
END_EVENT_TABLE()

