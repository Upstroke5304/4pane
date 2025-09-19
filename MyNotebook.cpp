/////////////////////////////////////////////////////////////////////////////
// Name:       MyNotebook.cpp
// Purpose:    Notebook stuff
// Part of:    4Pane
// Author:     David Hart
// Copyright:  (c) 2019 David Hart
// Licence:    GPL v3
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if defined (__WXGTK__)
  #include <gtk/gtk.h>
#endif
  
#include "wx/notebook.h"

#include "Configure.h"
#include "Misc.h"
#include "Redo.h"
#include "Devices.h"
#include "Bookmarks.h"
#include "Accelerators.h"
#include "MyFrame.h"
#include "MyGenericDirCtrl.h"
#include "MyDirs.h"
#include "MyFiles.h"
#include "MyNotebook.h"


MyNotebook::MyNotebook(wxSplitterWindow *main, wxWindowID id,  const wxPoint& pos, const wxSize& size, long style)
                                                                          :  wxNotebook((wxWindow*)main, id, pos, size, style)
{
nextpageno = 0;
TemplateNoThatsLoaded = -1;

#if defined (__WXGTK__)
  #if !defined (__WXGTK3__)                    // I find the default tab-size too clunky. Removing the border improves them imho
    g_object_set (m_widget, "tab_vborder", (guint)false, NULL);      // In >=2.7.0 gtk_notebook_set_tab_vborder is deprecated and so not available :(
  #endif
#endif
BookmarkMan = new Bookmarks;                   // Set up bookmarks
    
DeleteLocation = new DirectoryForDeletions;    // Set up the Deleted and Trash subdirs

UnRedoMan = new UnRedoManager;                 // The sole instance of UnRedoManager
UnRedoManager::frame = MyFrame::mainframe;     // Tell UnRedoManager's (static) pointer where it is

DeviceMan = new DeviceAndMountManager;         // Organises things to do with mounting partitions & devices
LaunchFromMenu = new LaunchMiscTools;          // Launches user-defined external programs & scripts from the Tools menu
}

MyNotebook::~MyNotebook()
{
MyFrame::mainframe->SelectionAvailable = false; // We don't want any change-of-focus events causing a new write to the command line!

delete LaunchFromMenu;
delete DeviceMan;
delete UnRedoMan;
delete DeleteLocation;
delete BookmarkMan;
}

void MyNotebook::LoadTabs(int TemplateNo /*=-1*/, const wxString& startdir0 /*=""*/, const wxString& startdir1 /*=""*/)  // Load the default tabs, or if TemplateNo > -1, load that tab template
{
wxConfigBase* config = wxConfigBase::Get(); if (config==NULL)  return;

wxString Path;
if (TemplateNo >= 0) 
  { Path.Printf(wxT("/Tabs/Template_%c/"), wxT('a') + TemplateNo); // If we're loading a template, adjust path approprately
    TemplateNoThatsLoaded = TemplateNo;                            //   and optimistically store the template's no
  }
 else Path = wxT("/Tabs/");

long selectedpage = 0;

if (TemplateNo == -1) // If we're not loading a tab template, load these user prefs. If it is, don't or it'll override
 { config->Read(Path + wxT("alwaysshowtabs"), &AlwaysShowTabs, 0);
    config->Read(Path + wxT("equalsizedtabs"), &EqualSizedTabs, 1);

    config->Read(Path + wxT("showcommandline"), &showcommandline, 0);
    config->Read(Path + wxT("showterminal"), &showterminal, 0);
    config->Read(Path + wxT("saveonexit"), &saveonexit, 0);
    config->Read(Path + wxT("selectedpage"), &selectedpage, 0);
    config->Read(wxT("/Tabs/NoOfExistingTemplates"), &NoOfExistingTemplates, 0);
 }

long numberoftabs, tabdatano;    // Now find the no of tab pages preferred, & load each with its correct tabdata.  Default is 1, using tabdata 'a'
config->Read(Path + wxT("numberoftabs"), &numberoftabs, 1);
#if defined (__WXGTK__)
  ShowTabs(numberoftabs > 1 || AlwaysShowTabs);     // Show the tab of each page if >1, or if user always wants to
  EqualWidthTabs(EqualSizedTabs);                   // Similarly equalise the tab-head width if requested
#endif
            // We MUST Show the frame before creating the tabs
            //  If not, & there's >1 tab loaded, the window construction gets confused & wxFindWindowAtPointer fails  
MyFrame::mainframe->Show();

wxString key, prefix(wxT("TabdataForPage_"));  
for (int n=0; n < numberoftabs; ++n)                // Create the key for every page, from page_a to page_('a'+numberoftabs-1)
  { key.Printf(wxT("%c"), wxT('a') + n);            // Make key hold a, b, c etc
    config->Read(Path+prefix+key, &tabdatano, 0);   // Load the flavour of tabdata to use for this page

    if (!n) CreateTab(tabdatano, -1, TemplateNo, wxEmptyString, startdir0, startdir1);    // Create this tab. Any user-provided startdir override should only be for page 0
      else CreateTab(tabdatano, -1, TemplateNo);
  }

size_t tab;
if ((size_t)selectedpage < GetPageCount())    SetSelection(selectedpage);  // Set the tab selection

if (GetPageCount())
  MyFrame::mainframe->SelectionAvailable = true;    // Flags, eg to updateUI, that it's safe to use GetActivePane, because there's at least one pane  

for (tab=0; tab < GetPageCount(); ++tab)            // We can now safely unhide the tabs
  GetPage(tab)->Show(); 

#if defined (__WXX11__)
  size_t count = GetPageCount();    // X11 has a strange bug, that means that the initially-selected page doesn't 'take'
  if (count > 1)                    // Instead you get the last never-selected tab's contents visible in each of the other tabs, until you change downwards
    { for (size_t n=0; n < count; ++n) SetSelection(n);  // So Select every one first, then Select the correct one. This has to be done *after* the tabs are Show()n
      SetSelection(selectedpage);
    }
#endif //WXX11

MyFrame::mainframe->GetMenuBar()->Check(SHCUT_SAVETABS_ONEXIT, saveonexit);  // Set the menu item check according to bool
DoLoadTemplateMenu();               // Get the correct no of loadable templates listed on the menu
}

void MyNotebook::DoLoadTemplateMenu()    // Get the correct no of loadable templates listed on the menu
{
                              // Find the LoadTemplate menu
wxMenuItem* p_menuitem = MyFrame::mainframe->GetMenuBar()->FindItem(LoadTabTemplateMenu);

wxMenu* submenu = p_menuitem->GetSubMenu();                                 // Use this menuitem ptr to get ptr to submenu itself
wxASSERT(submenu);

for (size_t count=submenu->GetMenuItemCount(); count > 0; --count)  // Get rid of all the entries in this menu, so that we don't duplicate them
  submenu->Destroy(submenu->GetMenuItems().Item(count-1)->GetData());

wxConfigBase* config = wxConfigBase::Get();  
if (config==NULL)  { wxLogError(_("Couldn't load configuration!?")); return; }

NoOfExistingTemplates = wxMin(NoOfExistingTemplates, MAX_NO_TABS);  // Sanity check, in case config has become corrupted
for (int n=0; n < NoOfExistingTemplates; ++n)
  { wxString Path; Path.Printf(wxT("/Tabs/Template_%c/title"), wxT('a') + n); // Get path to this template
  
    wxString title; title.Printf(wxT("Template %c"), wxT('a') +  n);          // Make a default title to display
    config->Read(Path, &title);                                     // Overwrite with the title that was previously chosen, if it exists
    submenu->Append(SHCUT_TABTEMPLATERANGE_START + n, title);       // Append the title to the menu
  }
}

void MyNotebook::DeleteTemplate()    // Delete an existing template
{
if (!NoOfExistingTemplates) return;

wxConfigBase* config = wxConfigBase::Get();  
if (config==NULL)  { wxLogError(_("Couldn't load configuration!?")); return; }

wxArrayString Choices;
for (int n=0; n < NoOfExistingTemplates; ++n)
  { wxString Path; Path.Printf(wxT("/Tabs/Template_%c/title"), wxT('a') + n); // Get path to this template
  
    wxString title; title.Printf(wxT("Template %c"), wxT('a') +  n);          // Make a default title to display
    config->Read(Path, &title);                                  // Overwrite with the title that was previously chosen, if it exists
    Choices.Add(title);                                          // Add the title to the stringarray
  }

int ans = wxGetSingleChoiceIndex(_("Which Template do you wish to Delete?"),_("Delete Template"), Choices);
if (ans == wxID_CANCEL) return;

if (!config->Exists( wxString::Format(wxT("/Tabs/Template_%c"), wxT('a') + ans) ) ) return;
config->DeleteGroup( wxString::Format(wxT("/Tabs/Template_%c"), wxT('a') + ans) );

 // if necessary, renumber to compensate for the deleted template
config->SetPath(wxT("/Tabs"));
for (int n=ans+1; n < NoOfExistingTemplates; ++n)
  config->RenameGroup( wxString::Format(wxT("Template_%c"), wxT('a') + n), wxString::Format(wxT("Template_%c"), wxT('a') + n-1) );

if (TemplateNoThatsLoaded >= 0)
  { if (TemplateNoThatsLoaded > ans) TemplateNoThatsLoaded--; // We've deleted a lower one, so we need to dec the currently-loaded marker to compensate
        else if (TemplateNoThatsLoaded == ans) 
          TemplateNoThatsLoaded = -1; // If we've deleted the current-loaded template that's equivalent to saying there's none loaded
  }

config->SetPath(wxT("/"));
config->Write(wxT("/Tabs/NoOfExistingTemplates"), --NoOfExistingTemplates); // Dec & save template index
config->Flush();

DoLoadTemplateMenu();                                           // Update loadable templates listed on the menu
BriefMessageBox msg(wxT("Template Deleted"), AUTOCLOSE_TIMEOUT / 1000, _("Success"));
}

void MyNotebook::LoadTemplate(int TemplateNo)   // Triggered by a menu event, loads the selected tab template
{
if (TemplateNo < 0 || TemplateNo >= MAX_NO_TABS) return;

MyFrame::mainframe->SelectionAvailable = false; // Turn off UpdateUI for a while so we can safely . . .
for (size_t n = GetPageCount(); n>0; --n)       // Kill any existing tabs
  DeletePage(n-1);
nextpageno = 0;                                 // Reset the pageno count

LoadTabs(TemplateNo);                           // Load the requested template
}

void MyNotebook::SaveAsTemplate()  // Save the current tab setup as a template
{
if (TemplateNoThatsLoaded >=0 && TemplateNoThatsLoaded < MAX_NO_TABS)  // Is there a template loaded?
  { 
    wxString title, titlepath; titlepath.Printf(wxT("/Tabs/Template_%c/title"), wxT('a') + TemplateNoThatsLoaded);  // If so, find its title
    wxConfigBase::Get()->Read(titlepath, &title);
    TabTemplateOverwriteDlg dlg;
    wxXmlResource::Get()->LoadDialog(&dlg, this, wxT("TabTemplateOverwriteDlg"));  // Ask if we want to overwrite this template, or SaveAs a different one
    
    if (!title.IsEmpty())                          // If there is a title for this template, use it in the dlg
      { wxString msg; msg.Printf(wxT("There is a Template loaded, called %s"), title.c_str());  // Use title to correct the static message
        ((wxStaticText*)dlg.FindWindow(wxT("Static")))->SetLabel(msg);          // Adjust the label to add the template name
        dlg.GetSizer()->Fit(&dlg);
      }
     else title.Printf(wxT("Template %c"), wxT('a') + TemplateNoThatsLoaded);   // Otherwise create a default title
     
    int ans = dlg.ShowModal();
    if (ans==wxID_CANCEL) { BriefMessageBox msg(_("Template Save cancelled"), AUTOCLOSE_TIMEOUT / 1000, _("Cancelled")); return; }
    if (ans==XRCID("Overwrite"))
      {  SaveDefaults(TemplateNoThatsLoaded, title); // This should overwrite the current template
          BriefMessageBox msg(wxT("Template Overwritten"), AUTOCLOSE_TIMEOUT / 1000, _("Success")); return;
      }    
  }                                              // Otherwise SaveAs a new template, as though it wasn't one already

              // If there isn't a template loaded, or if we're falling through from above, save as new template
wxString workingtitle; workingtitle.Printf(wxT("Template %c"), wxT('a') + NoOfExistingTemplates); 
wxString title = wxGetTextFromUser(_("What would you like to call this template?"), _("Choose a template label"), workingtitle);
if (title.IsEmpty())  { BriefMessageBox msg(_("Save Template cancelled"), AUTOCLOSE_TIMEOUT / 1000, _("Cancelled")); return; }

TemplateNoThatsLoaded = NoOfExistingTemplates++;
wxConfigBase::Get()->Write(wxT("/Tabs/NoOfExistingTemplates"), NoOfExistingTemplates); // Inc & save template index
wxConfigBase::Get()->Flush();
SaveDefaults(NoOfExistingTemplates-1, title);
DoLoadTemplateMenu();                           // Update loadable templates listed on the menu
BriefMessageBox msg(wxT("Template Saved"), AUTOCLOSE_TIMEOUT / 1000, _("Success"));
}

void MyNotebook::SaveDefaults(int TemplateNo /*=-1*/, const wxString& title /*=wxT("")*/)
{
wxConfigBase* config = wxConfigBase::Get(); if (!config)  return;

int width, height; MyFrame::mainframe->GetSize(&width, &height);
config->Write(wxT("/Misc/AppHeight"), (long)height);                  // Save 4Pane's initial size
config->Write(wxT("/Misc/AppWidth"), (long)width);

wxString Path;
if (TemplateNo >= 0) 
  { Path.Printf(wxT("/Tabs/Template_%c/"), wxT('a') + TemplateNo);    // If we're saving a template, adjust path approprately
    config->Write(Path + wxT("title"), title);                        // Save the title by which this template will be known (default is 'Template a' etc)
  }
 else Path = wxT("/Tabs/");
 
config->Write(Path + wxT("showcommandline"), MyFrame::mainframe->Layout->CommandlineShowing);
config->Write(Path + wxT("showterminal"), MyFrame::mainframe->Layout->EmulatorShowing);
if ( MyFrame::mainframe->Layout->EmulatorShowing)
  config->Write(Path + "terminalheight", MyFrame::mainframe->Layout->bottompanel->GetSize().GetHeight());

config->Write(Path + wxT("saveonexit"), saveonexit);
config->Write(Path + wxT("selectedpage"), GetSelection());

if (TemplateNo < 0) // Don't save these settings for templates, otherwise loading a tab template would (unexpectedly) override the user's current choices
 { config->Write(Path + wxT("alwaysshowtabs"), AlwaysShowTabs);
    config->Write(Path + wxT("equalsizedtabs"), EqualSizedTabs);
 }

long numberoftabs = GetPageCount();
config->Write(Path + wxT("numberoftabs"), numberoftabs);        // Save the no of tabs displayed

wxString  key, prefix(wxT("TabdataForPage_"));  
for (int n=0; n < numberoftabs; ++n)                            // Create the key for every page, from page_a to page_('a'+numberoftabs-1)
  { key.Printf(wxT("%c"), wxT('a') + n);                        // Make key hold a, b, c etc
    MyTab* tab = (MyTab*)GetPage(n);
    config->Write(Path+prefix+key, n);                          // Save the type of tabdata to use for this page
    tab->StoreData(n);                                          // Tell the tab to update its tabdata with the current settings
    tab->tabdata->Save(n, TemplateNo);                          // Now tell tabdata to save itself
  }
  
config->Flush();
}

void MyNotebook::CreateTab(int tabtype /*=0*/, int after /*= -1*/,  int TemplateNo /*=-1*/, const wxString& pgName /*=""*/, const wxString& startdir0 /*=""*/, const wxString& startdir1 /*=""*/)
{
if (GetPageCount() >= (size_t)MAX_NO_TABS)  { wxLogError(_("Sorry, the maximum number of tabs are already open.")); return; }

wxString pageName(pgName);
                                    // Add panel as notebook page
wxPanel* page = new MyTab(this, -1, tabtype, TemplateNo, pageName, startdir0, startdir1);    // Create the 'paper' of the new page

if (pageName.IsEmpty())  pageName = ((MyTab*)page)->tabdata->tabname;   // The tab may have come with a previously-saved name
if (pageName.IsEmpty())  pageName.Printf(wxT("Page %u"), nextpageno+1); // If not, & there isn't one offered by the caller, invent a sensible name

if (after == -1)                                      // -1 flags to append, not insert
  { if (AddPage(page, pageName, true))  ++nextpageno;
       else return;                                   // If page couldn't be created, bug out
  }
 else                                                 // else Insert it
   { if (InsertPage(after, page, pageName, true))  ++nextpageno;
       else return;                                   // If page couldn't be created, bug out
  }
}

void MyNotebook::AppendTab()
{
CreateTab(GetPageCount());                          // By passing GetPageCount(), the correct tabdata should be used
SetSelection(GetPageCount() - 1);                   // Select it
#if defined (__WXGTK__)
  ShowTabs(GetPageCount() > 1 || AlwaysShowTabs);   // Show the tab of each page if now >1, or if user always wants to
#endif
MyFrame::mainframe->SelectionAvailable = true;      // Flags, eg to updateUI, that it's safe to use GetActivePane, because there's at least one pane
}

void MyNotebook::InsertTab()
{
if (!GetPageCount())   return AppendTab();          // If there isn't a page yet, we can't very well insert

int page = GetSelection();                          // We're trying to insert after the selected tab, so find it
if (page == -1) return;                             // If it's invalid, abort

CreateTab(page+1, page+1);                          // Use CreateTab to do the creation
SetSelection(page+1);                               // Select the new tab
#if defined (__WXGTK__)
  ShowTabs(GetPageCount() > 1 || AlwaysShowTabs);   // Show the tab of each page if now >1, or if user always wants to
#endif
}

void MyNotebook::DelTab()
{
int page = GetSelection();                          // We're trying to delete the selected tab, so find it
if (page != -1)  DeletePage(page);                  // -1 would have meant no selection, so presumably the notebook is empty
#if defined (__WXGTK__)
  ShowTabs(GetPageCount() > 1 || AlwaysShowTabs);   // Show the tab of each page if now >1, or if user always wants to
#endif
if (!GetPageCount()) MyFrame::mainframe->SelectionAvailable = false;  // If we've just deleted the only tab, tell the frame
}

void MyNotebook::DuplicateTab()
{
int page = GetSelection();                          // We're trying to insert after the selected tab, so find it
if (page == -1)  return;                            // If it's not valid, take fright

                            // The rest is a cut-down, amended version of CreateTab()
if (GetPageCount() >= (size_t)MAX_NO_TABS)  { wxLogError(_("Sorry, the maximum number of tabs are already open.")); return; }
  
wxString pageName(GetPageText(page));               // Get the name of the tab we're duplicating
pageName += _(" again");                            // Derive from it an exciting new name

MyTab* currenttab = (MyTab*)GetPage(page);          // Find current tab
currenttab->StoreData(currenttab->tabdatanumber);   // Tell it to update its tabdata with the current settings
Tabdata* oldtabdata = currenttab->tabdata;          // Get the tabdata to be copied

                                    // Add panel as notebook page
wxPanel *duppage = new MyTab(oldtabdata, this, -1, -1, pageName);  // Create the 'paper' of the new page, using the copyctor version
((MyTab*)duppage)->tabdatanumber = currenttab->tabdatanumber;

if (InsertPage(page + 1, duppage, pageName, true))  
  { duppage->Show(); SetSelection(page+1); ++nextpageno; } // Insert, Select & Show it

#if defined (__WXGTK__)
  ShowTabs(true);                                   // Show the tab heads now, as by definition there must be >1 tab
#endif
}


void MyNotebook::OnSelChange(wxNotebookEvent& event)
{
int page = event.GetSelection(); if (page==-1) return;    // Find the new page index

MyTab* tab = (MyTab*)GetPage(page);
MyGenericDirCtrl* activepane = tab->GetActivePane();      //  & thence the active pane
if (!activepane) return;

wxString path = activepane->GetPath();
activepane->DisplaySelectionInTBTextCtrl(path);           // So we can now set the TBTextCtrl
MyFrame::mainframe->Layout->OnChangeDir(path);            //  & the Terminal etc if necessary

  // Now make sure there's a highlit pane, ideally the correct one. And just one, so kill all highlights first, then set the active one
tab->SwitchOffHighlights();

wxFocusEvent sfe(wxEVT_SET_FOCUS);
activepane->GetTreeCtrl()->GetEventHandler()->AddPendingEvent(sfe);
}

void MyNotebook::ShowContextMenu(wxContextMenuEvent& event)
{
wxPoint pt = ScreenToClient(wxGetMousePosition());

#if defined (__WXX11__)
  int page =  GetSelection();  if (page == wxNOT_FOUND) return;
#else
  int page = HitTest(pt, NULL); if (page == wxNOT_FOUND) return;
  if (page != GetSelection())  SetSelection(page);  // This wasn't needed in earlier versions, as the rt-click used to change the selection before the contextmenu event arrived
#endif

int shortcuts[] = { SHCUT_NEWTAB, SHCUT_DELTAB, SHCUT_INSERTTAB, wxID_SEPARATOR, SHCUT_RENAMETAB, SHCUT_DUPLICATETAB
#if defined (__WXGTK__)
                                                     ,wxID_SEPARATOR, SHCUT_ALWAYS_SHOW_TAB, SHCUT_SAME_TEXTSIZE_TAB
#endif
              };
wxMenu menu;
MyFrame::mainframe->AccelList->AddToMenu(menu, shortcuts[0], _("&Append Tab"));  // Do SHCUT_NEWTAB separately, as it makes more sense here to call it Append

for (size_t n=1; n < sizeof(shortcuts)/sizeof(int); ++n)
  MyFrame::mainframe->AccelList->AddToMenu(menu, shortcuts[n], wxEmptyString,  (n > 6 ? wxITEM_CHECK : wxITEM_NORMAL));
#if defined (__WXGTK__)
menu.Check(SHCUT_ALWAYS_SHOW_TAB, AlwaysShowTabs);
menu.Check(SHCUT_SAME_TEXTSIZE_TAB, EqualSizedTabs);
#endif
menu.SetTitle(wxT("Tab Menu"));                     // This doesn't seem to work with wxGTK-2.4.2, but no harm leaving it in!
PopupMenu(&menu, pt.x, pt.y);
}

void MyNotebook::RenameTab()
{
int page = GetSelection();                          // Find which tab we're supposed to be renaming
if (page == -1)   return;
wxString text = wxGetTextFromUser(_("What would you like to call this tab?"),_("Change tab title"));
if (text.IsEmpty()) return;                         // as it will be if the user Cancelled, or was just playing silly-buggers
SetPageText(page, text);

MyTab* tab = (MyTab*)GetPage(page);
tab->tabdata->tabname = text;                       // Since this is a user-chosen name, store it in case the settings are to be saved
}

wxString MyNotebook::CreateUniqueTabname() const
{
const static int enormous = 1000;
int largest(0);

for (size_t n = 0; n < GetPageCount(); ++n)
  { wxString name = GetPageText(n);
    wxString num; long li;
    name.StartsWith(wxT("Page "), &num);
    if (num.ToLong(&li))
      largest = wxMax(largest, li);
  }

for (int n = GetPageCount(); n < enormous; ++n)
  { if (n > largest)
      { wxString name = wxString::Format(wxT("Page %i"), n);
        return name;
      }
  }

return wxString::Format(wxT("Page %zu"), GetPageCount()+1);  // We've exceeded the enormous int, presumably because someone renamed to 'Page 2345', so default to 'Page <count>'
}

#if defined (__WXGTK__)

void MyNotebook::OnLeftDown(wxMouseEvent& event)
{
  // Recent gtks have refused to change the selection on a mouse-click unless the notebook had focus
  // This is probably because of lots of other focus/child-focus events delaying things
  // So do it here too. It doesn't seem to do any harm
wxPoint pt = ScreenToClient(wxGetMousePosition());
int page = HitTest(pt, NULL); if (page == wxNOT_FOUND) return;
if (page != GetSelection())  SetSelection(page);
event.Skip();
}

void MyNotebook::OnAlwaysShowTab(wxCommandEvent& WXUNUSED(event))
{
AlwaysShowTabs = !AlwaysShowTabs; 
wxConfigBase::Get()->Write(wxT("Tabs/alwaysshowtabs"), AlwaysShowTabs);

ShowTabs(GetPageCount() > 1 || AlwaysShowTabs);
}

void MyNotebook::ShowTabs(bool show_tabs)
{
gtk_notebook_set_show_tabs(GTK_NOTEBOOK(m_widget), (gboolean) show_tabs);
}

void MyNotebook::EqualWidthTabs(bool equal_tabs)
{
#if !defined (__WXGTK3__) 
  g_object_set (m_widget, "homogeneous", (gboolean)equal_tabs, NULL); // In 2.7.0 gtk_notebook_set_homogeneous_tabs is deprecated and so not available :(
  wxConfigBase::Get()->Write(wxT("Tabs/equalsizedtabs"), equal_tabs);
#endif
}
#endif // defined __WXGTK__

BEGIN_EVENT_TABLE(MyNotebook,wxNotebook)
  EVT_MENU(SHCUT_NEWTAB, MyNotebook::OnAppendTab)
  EVT_MENU(SHCUT_INSERTTAB, MyNotebook::OnInsTab)
  EVT_MENU(SHCUT_DELTAB, MyNotebook::OnDelTab)
  EVT_MENU(SHCUT_RENAMETAB, MyNotebook::OnRenTab)
  EVT_MENU(SHCUT_DUPLICATETAB, MyNotebook::OnDuplicateTab)
  EVT_MENU_RANGE(SHCUT_PREVIOUS_TAB,SHCUT_NEXT_TAB, MyNotebook::OnAdvanceSelection)
#if defined (__WXGTK__)
  EVT_MENU(SHCUT_ALWAYS_SHOW_TAB, MyNotebook::OnAlwaysShowTab)
  EVT_MENU(SHCUT_SAME_TEXTSIZE_TAB, MyNotebook::OnSameTabSize)
  EVT_LEFT_DOWN(MyNotebook::OnLeftDown)
#endif
  EVT_NOTEBOOK_PAGE_CHANGED(-1, MyNotebook::OnSelChange)
  EVT_LEFT_DCLICK(MyNotebook::OnButtonDClick)
#if defined (__WXX11__)
  EVT_RIGHT_UP(MyNotebook::OnRightUp)          // EVT_CONTEXT_MENU doesn't seem to work in X11
#endif
  EVT_CONTEXT_MENU(MyNotebook::ShowContextMenu)
END_EVENT_TABLE()
//-----------------------------------------------------------------------------------------------------------------------

