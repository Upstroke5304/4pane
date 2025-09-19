/////////////////////////////////////////////////////////////////////////////
// Name:        MyDirs.h
// Purpose:     Dir-view
// Part of:     4Pane
// Author:      David Hart
// Copyright:   (c) 2020 David Hart
// Licence:     GPL v3
/////////////////////////////////////////////////////////////////////////////
#ifndef MYDIRSH
#define MYDIRSH

#include "wx/filename.h"
#include "wx/dir.h"
#include "wx/toolbar.h"
#include "wx/bmpbuttn.h"

#include <vector>

#include "Externs.h"  
#include "MyGenericDirCtrl.h"
#include "Dup.h"


#if wxVERSION_NUMBER < 2900
  DECLARE_EVENT_TYPE(PasteThreadType, wxID_ANY)
  DECLARE_EVENT_TYPE(PasteProgressEventType, wxID_ANY)
#else
  class PasteThreadEvent;
  wxDECLARE_EVENT(PasteThreadType, PasteThreadEvent);
  wxDECLARE_EVENT(PasteProgressEventType, wxCommandEvent);
#endif

class PasteThreadEvent: public wxCommandEvent
{
public:
	PasteThreadEvent(wxEventType commandType = PasteThreadType, int id = 0)
        		:  wxCommandEvent(commandType, id) { }
 
	PasteThreadEvent(const PasteThreadEvent& event)
        		:  wxCommandEvent(event)
      { this->SetArrayString(event.GetArrayString()); this->SetSuccesses(event.GetSuccesses()); this->SetRefreshesArrayString(event.GetRefreshesArrayString()); }
 
	wxEvent* Clone() const { return new PasteThreadEvent(*this); }
 
	wxArrayString GetArrayString() const { return m_ArrayString; }
	void SetArrayString(const wxArrayString& arr) { m_ArrayString = arr; }
	wxArrayString GetSuccesses() const { return m_SuccessesArrayString; }
	void SetSuccesses(const wxArrayString& arr) { m_SuccessesArrayString = arr; }
	wxArrayString GetRefreshesArrayString() const { return m_RefreshesArrayString; }
	void SetRefreshesArrayString(const wxArrayString& refreshes) { m_RefreshesArrayString = refreshes; }
 
protected:
	wxArrayString m_ArrayString;
	wxArrayString m_RefreshesArrayString;
	wxArrayString m_SuccessesArrayString;
};

typedef void (wxEvtHandler::*PasteThreadEventFunction)(PasteThreadEvent &);
#if wxVERSION_NUMBER < 2900
  #define PasteThreadEventHandler(func) (wxObjectEventFunction)(wxEventFunction)(wxCommandEventFunction)wxStaticCastEvent(PasteThreadEventFunction, &func)
#else
  #define PasteThreadEventHandler(func) wxEVENT_HANDLER_CAST(PasteThreadEventFunction, func)    
#endif


class MyDropdownButton  :  public wxBitmapButton
{
public:
MyDropdownButton(){}                            // Needed for RTTI
MyDropdownButton(wxWindow* parent, const wxWindowID id, const wxBitmap& bitmap)
                    : wxBitmapButton(parent, id, bitmap, wxDefaultPosition, wxDefaultSize, wxBU_AUTODRAW | wxNO_BORDER) {}

int partnerwidth;                               // The button to which this is the dropdown arrow stores its width here
int left, right;                                // This button's coords relative to the toolbar, measured & stored at each click

protected:
void OnButtonClick(wxCommandEvent& event);

DECLARE_EVENT_TABLE()
DECLARE_DYNAMIC_CLASS(MyDropdownButton)
};

struct NavigationData
{
NavigationData(const wxString& dir, const wxString& path) : m_dir(dir), m_path(path) {}
wxString GetDir() const { return m_dir; }
wxString GetPath() const { return m_path; }
void SetPath(const wxString& path) { m_path = path; }

protected:
wxString m_dir;
wxString m_path;
};

class NavigationManager
{
public:
NavigationManager() : m_NavDataIndex(-1) {}
~NavigationManager();

void PushEntry(const wxString& dir, const wxString& path = wxT(""));
wxArrayString RetrieveEntry(int displacement);
wxString GetCurrentDir() const;
wxString GetDir(size_t index) const;
wxString GetCurrentPath() const;
wxString GetPath(size_t index) const;
void SetPath(size_t index, const wxString& path);
void DeleteInvalidEntry();  // Deletes the current entry, which will have just been found no longer to exist
size_t GetCurrentIndex() const { return m_NavDataIndex; }
size_t GetCount() const { return m_NavigationData.size(); }
wxArrayString GetBestValidHistoryItem(const wxString& deadfilepath); // Retrieves the best valid dir from the vector.  Used when an IN_UNMOUNT event is caught

std::vector<NavigationData*>& GetNavigationData() { return m_NavigationData; }
protected:

std::vector<NavigationData*> m_NavigationData;     // Holds info for Back/Forward toolbar buttons
int m_NavDataIndex;                                // Indexes the currently-displayed dir in the vector
};

class DirGenericDirCtrl  :  public MyGenericDirCtrl        // The class that shows the directories, not the files
{
public:
DirGenericDirCtrl(wxWindow* parent, const wxWindowID id, const wxString& START_DIR , const wxPoint& pos, const wxSize& size ,
                                   long style = wxSUNKEN_BORDER, bool full_tree=false, const wxString& name = wxT("DirGenericDirCtrl"));
              
~DirGenericDirCtrl();

void CreateToolbar();
void OnFullTree(wxCommandEvent& event);         // Toggles Fulltree button
void OnDirUp(wxCommandEvent& event);            // From toolbar Up button
void OnDirBack(wxCommandEvent& event);          // From toolbar Back button.  Go back to a previously-visited directory
void OnDirForward(wxCommandEvent& event);       // From toolbar Forward button.  Go forward to a previously-undone directory
void OnDirDropdown(wxCommandEvent& event);      // For multiple forward/back
void OnDirCtrlTBButton(wxCommandEvent& event);  // When a user-defined dirctrlTB button is clicked
void DoToolbarUI(wxUpdateUIEvent& event);       // Greys out appropriate buttons
void OnToggleHidden(wxCommandEvent& event);     // Toggles whether hidden dirs (& partner's files) are visible
void ShowContextMenu(wxContextMenuEvent& event);
                    // These methods implement backward/forward navigation of dirs, especially in !Fulltree mode
void AddPreviouslyVisitedDir(const wxString& newentry, const wxString& path = wxT("")); // Adds a new revisitable dir to the array
void RetrieveEntry(int displacement);             // Retrieves a dir from the PreviousDirs array.  'displacement' determines which
void GetBestValidHistoryItem(const wxString& deadfilepath); // Retrieves the best valid dir from the vector.  Used when an IN_UNMOUNT event is caught

void NewFile(wxCommandEvent& event);              // Create a new File or Dir

virtual void UpdateStatusbarInfo(const wxString& selected); // Writes the selection's name & some relevant data in the statusbar
void UpdateStatusbarInfo(const wxArrayString& selections);  // Writes selections' name & size in the statusbar
void ReloadTree(wxString path, wxArrayInt& IDs);  // Does a branch or tree refresh
void ReloadTree(){ wxString path(wxFILE_SEP_PATH); wxArrayInt IDs; ReloadTree(path, IDs); }    // If no parameter, do the whole tree
void RecreateAcceleratorTable();                  // When shortcuts are reconfigured, deletes old table, then calls CreateAcceleratorTable()
void GetVisibleDirs(wxTreeItemId rootitem, wxArrayString& dirs) const; // Find which dirs can be seen i.e. children + expanded children's children
bool IsDirVisible(const wxString& fpath) const;   // Is fpath 'visible' i.e. expanded in the tree. Accounts for fulltree mode too
void CheckChildDirCount(const wxString& dir);     // See if the treeitem corresponding to dir has child dirs. Set the triangle accordingly

wxULongLong SelectedCumSize;
wxToolBar* toolBar;
NavigationManager& GetNavigationManager() { return m_NavigationManager; }

protected: 
void CreateAcceleratorTable();
void OnIdle(wxIdleEvent& event);   
void DoScrolling();

NavigationManager m_NavigationManager;
MyDropdownButton* MenuOpenButton1;                // The dir-navigation buttons sidebars (those things that allow you to choose any of the previous dirs) 
MyDropdownButton* MenuOpenButton2;

private:
DECLARE_EVENT_TABLE()
};

#endif
    // MYDIRSH 
