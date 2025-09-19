/////////////////////////////////////////////////////////////////////////////
// Name:       MyNotebook.h
// Purpose:    Notebook stuff
// Part of:    4Pane
// Author:     David Hart
// Copyright:  (c) 2016 David Hart
// Licence:    GPL v3
/////////////////////////////////////////////////////////////////////////////
#ifndef MYNOTEBOOKH
#define MYNOTEBOOKH

#include "Externs.h"

class DirectoryForDeletions;
class UnRedoManager;
class DeviceAndMountManager;
class Bookmarks;
class LaunchMiscTools;

class MyNotebook : public wxNotebook
{
public:
MyNotebook(wxSplitterWindow *main, wxWindowID id = -1, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = 0);
~MyNotebook();

void LoadTabs(int TemplateNo = -1, const wxString& startdir0=wxT(""), const wxString& startdir1=wxT(""));  // Loads the default tabs, or if TemplateNo > -1, loads that tab template

void CreateTab(int tabtype = 0, int after = -1, int TemplateNo = -1, const wxString& pageName = wxT(""), const wxString& startdir0=wxT(""), const wxString& startdir1=wxT(""));

void OnAppendTab(wxCommandEvent& WXUNUSED(event)){ AppendTab(); }
void OnInsTab(wxCommandEvent& WXUNUSED(event)){ InsertTab(); }
void OnDelTab(wxCommandEvent& WXUNUSED(event)){ DelTab(); }
void OnRenTab(wxCommandEvent& WXUNUSED(event)){ RenameTab(); }
void OnDuplicateTab(wxCommandEvent& WXUNUSED(event)){ DuplicateTab(); }
void OnAdvanceSelection(wxCommandEvent& event){AdvanceSelection(event.GetId() == SHCUT_NEXT_TAB); }
wxString CreateUniqueTabname() const;
#if defined (__WXGTK__)
  void OnAlwaysShowTab(wxCommandEvent& event);
  void OnSameTabSize(wxCommandEvent& event){ EqualSizedTabs = ! EqualSizedTabs; EqualWidthTabs(EqualSizedTabs); }
#endif

void LoadTemplate(int TemplateNo);    // Triggered by a menu event, loads the selected tab template
void SaveDefaults(int TemplateNo = -1, const wxString& title = wxT(""));// Save current tab setup.  If TemplateNo > -1 save as a template; otherwise just do a 'normal' save
void SaveAsTemplate();                // Save the current tab setup as a template
void DeleteTemplate();                // Delete an existing template

DirectoryForDeletions* DeleteLocation; // Organises unique dirnames for deleted/trashed files
UnRedoManager* UnRedoMan;             // The sole instance of UnRedoManager
DeviceAndMountManager* DeviceMan;     // Find where floppy, cds are
Bookmarks* BookmarkMan;
LaunchMiscTools* LaunchFromMenu;      // Launches user-defined external programs & scripts from the Tools menu

bool showcommandline;
bool showterminal;
bool saveonexit;
int NoOfExistingTemplates;            // How many templates are there?
int TemplateNoThatsLoaded;            // Which of them is loaded, if any.  -1 means none
bool AlwaysShowTabs;
bool EqualSizedTabs;

protected:
void DoLoadTemplateMenu();            // Get the correct no of loadable templates listed on the menu
void OnSelChange(wxNotebookEvent& event);
void OnButtonDClick(wxMouseEvent& WXUNUSED(event)){ RenameTab(); }
#if defined (__WXX11__)
  void OnRightUp(wxMouseEvent& event){ ShowContextMenu((wxContextMenuEvent&)event); }  // EVT_CONTEXT_MENU doesn't seem to work in X11
#endif
void ShowContextMenu(wxContextMenuEvent& event);
void AppendTab();
void InsertTab();
void DelTab();
void DuplicateTab();
void RenameTab();
#if defined (__WXGTK__)
  void OnLeftDown(wxMouseEvent& event);
  void ShowTabs(bool show_tabs);
  void EqualWidthTabs(bool equal_tabs);
#endif
int nextpageno;
private:
DECLARE_EVENT_TABLE()
};



#endif
    // MYNOTEBOOKH 

