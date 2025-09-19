/////////////////////////////////////////////////////////////////////////////
// Name:       MyFiles.h
// Purpose:    File-view
// Part of:    4Pane
// Author:     David Hart
// Copyright:  (c) 2016 David Hart
// Licence:    GPL v3
/////////////////////////////////////////////////////////////////////////////
#ifndef MYFILESH
#define MYFILESH

#include "wx/filename.h"
#include "wx/dir.h"
#include "wx/toolbar.h"
#include "wx/wx.h"
#include "wx/notebook.h"
#include "wx/listctrl.h"

#include "Externs.h"    
#include "MyGenericDirCtrl.h"

//--------------------------------------------------------------------------
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

WX_DECLARE_OBJARRAY(class DataBase, FileDataObjArray);  // Declare the array of FileData objects (or the FakeFiledata alternative for archives), to hold the result of each file's wxStat


class TreeListHeaderWindow;

class FileGenericDirCtrl  :  public MyGenericDirCtrl    // The class that displays & manipulates files
{
public:
FileGenericDirCtrl(wxWindow* parent, const wxWindowID id, const wxString& START_DIR , const wxPoint& pos, const wxSize& size,
                         long style = wxSUNKEN_BORDER, bool full_tree=false, const wxString& name = wxT("FileGenericDirCtrl"));
~FileGenericDirCtrl(){ CombinedFileDataArray.Clear(); FileDataArray.Clear(); }

void CreateColumns(bool left);                        // Called after ctor to create the columns.  left says use tabdata's Lwidthcol

void OnColumnSelectMenu(wxCommandEvent& event);
void OnSize(wxSizeEvent& event){ DoSize(); event.Skip(); }
void DoSize();

virtual void UpdateStatusbarInfo(const wxString& selected); // Feeds the other overload with a wxArrayString if called with a single selection
void UpdateStatusbarInfo(const wxArrayString& selections);  // Writes selections' name & size in the statusbar

void ReloadTree(wxString path, wxArrayInt& IDs);
void OnOpen(wxCommandEvent& event);                   // From DClick, Context menu or OpenWithKdesu, passes on to DoOpen
void DoOpen(wxString& filepath);                      // From Context menu or DClick in pane or TerminalEm
virtual void NewFile(wxCommandEvent& event);          // Create a new File or Dir

TreeListHeaderWindow* GetHeaderWindow(){ return headerwindow; }
enum columntype GetSelectedColumn();
void SetSelectedColumn(enum columntype col);
bool GetSortorder(){ return reverseorder; }
void SetSortorder(bool Sortorder){ reverseorder = Sortorder; }
bool GetIsDecimalSort(){ return m_decimalsort; }
void SetIsDecimalSort(bool decimalsort){ m_decimalsort = decimalsort; }
void SortStats(FileDataObjArray& array);                // Sorts the FileData array according to column selection
void UpdateFileDataArray(const wxString& filepath);     // Update an CombinedFileDataArray entry from filepath
void UpdateFileDataArray(DataBase* fd);                 // Update an CombinedFileDataArray entry from fd
void DeleteFromFileDataArray(const wxString& filepath); // Remove filepath from CombinedFileDataArray and update CumFilesize etc
void ShowContextMenu(wxContextMenuEvent& event);        // Right-Click menu
void RecreateAcceleratorTable();                        // When shortcuts are reconfigured, calls CreateAcceleratorTable()
void OnToggleHidden(wxCommandEvent& event);             // Toggles whether hidden dirs & files are visible
void OnToggleDecimalAwareSort(wxCommandEvent& event);   // Toggles decimal-aware sort order
void SelectFirstItem();                                 // Selects the first item in the tree. Used during navigation via keyboard
bool UpdateTreeFileModify(const wxString& filepath);    // Called when a wxFSW_EVENT_MODIFY arrives
bool UpdateTreeFileAttrib(const wxString& filepath);    // Called when a wxFSW_EVENT_ATTRIB arrives

size_t NoOfDirs;                                        // These 3 vars are filled during MyGenericDirCtrl::ExpandDir.  Data is displayed in statusbar 
size_t NoOfFiles;
wxULongLong CumFilesize;
wxULongLong SelectedCumSize;

FileDataObjArray CombinedFileDataArray;                 // Array of FileData objects, which store etc the wxStat data. Stores dirs & files
FileDataObjArray FileDataArray;                         // Temporary version just for files
wxArrayString CommandArray;                             // Used to store an applic's launch-command, found by FiletypeManager which then loses scope

protected:
void CreateAcceleratorTable();
void OpenWithSubmenu(wxCommandEvent& event);            // Events from the submenu of the context menu end up here

void OnSelectAll(wxCommandEvent& event) { GetTreeCtrl()->GetEventHandler()->ProcessEvent(event); }
void OnArchiveAppend(wxCommandEvent& event);
void OnArchiveTest(wxCommandEvent& event);

void OnOpenWith(wxCommandEvent& event);
void OpenWithKdesu(wxCommandEvent& event);

void HeaderWindowClicked(wxListEvent& event);           // A L or R click occurred on the header window
bool reverseorder;                                      // Is the selected column reverse-sorted?
bool m_decimalsort;                                     // Should we sort filenames in a decimal-aware manner i.e. foo1, foo2 above foo11?
TreeListHeaderWindow* headerwindow;

private:
DECLARE_EVENT_TABLE()
};



#endif
    // MYFILESH 
