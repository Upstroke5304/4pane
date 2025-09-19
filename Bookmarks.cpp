/////////////////////////////////////////////////////////////////////////////
// Name:       Bookmarks.cpp
// Purpose:    Bookmarks
// Part of:    4Pane
// Author:     David Hart
// Copyright:  (c) 2019 David Hart
// Licence:    GPL v3
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/log.h"
#include "wx/app.h"
#include "wx/dirctrl.h"
#include "wx/treectrl.h"
#include "wx/xrc/xmlres.h"
#include "wx/config.h"

#include "Externs.h"
#include "MyGenericDirCtrl.h"
#include "Bookmarks.h"
#include "MyDirs.h"
#include "MyFrame.h"
#include "Accelerators.h"
#include "Tools.h"

                            // Load the icons for the treectrl
#include "bitmaps/include/ClosedFolder.xpm"
#include "bitmaps/include/bookmark.xpm"
#include "bitmaps/include/separator.xpm"


void Submenu::ClearData()  // Deletes everything
{
thismenu = NULL; ID = 0; 
pathname.Clear(); Label.Clear(); FullLabel.Clear(); displayorder.Clear();         // I hope that's Clear

for (int n = (int)BMstruct.GetCount(); n > 0; --n)  { BookmarkStruct* item = BMstruct.Item(n-1); delete item; BMstruct.RemoveAt(n-1); }

for (size_t n = children.GetCount(); n > 0; --n)  children[n-1]->ClearData();     // Recurse thru children
WX_CLEAR_ARRAY(children);                                                         //  then delete them
}

void MyBookmarkDialog::OnButtonPressed(wxCommandEvent& event)
{
int id = event.GetId();

if (id == wxID_OK || id == wxID_CANCEL)   EndModal(id);

if (id == XRCID("Delete"))          parent->OnDeleteBookmark((wxTreeItemId)0l);
if (id == XRCID("EditBookmark"))    parent->OnEditBookmark();
if (id == XRCID("NewSeparator"))    parent->OnNewSeparator();
if (id == XRCID("NewFolder"))       parent->OnNewFolder();
if (id == XRCID("ChangeFolderBtn")) parent->OnChangeFolderButton();  // From the AddBookmark dlg
}

  
BEGIN_EVENT_TABLE(MyBookmarkDialog,wxDialog)
  EVT_BUTTON(-1, MyBookmarkDialog::OnButtonPressed)
END_EVENT_TABLE()

IMPLEMENT_DYNAMIC_CLASS(MyBmTree, wxTreeCtrl)

void MyBmTree::Init(Bookmarks* dad)
{
parent = dad;

wxImageList *images = new wxImageList(16, 16, true);   // Make an image list containing small icons

images->Add(wxIcon(ClosedFolder_xpm));
images->Add(wxIcon(bookmark_xpm));
images->Add(wxIcon(separator_xpm));

AssignImageList(images);


if (SHOW_TREE_LINES) SetWindowStyle(GetWindowStyle() & ~wxTR_NO_LINES);
  else SetWindowStyle(GetWindowStyle() | wxTR_NO_LINES);

if (!USE_DEFAULT_TREE_FONT && CHOSEN_TREE_FONT.Ok())  SetFont(CHOSEN_TREE_FONT);

SetIndent(TREE_INDENT);                           // Make the layout of the tree match the GenericDirCtrl sizes
SetSpacing(TREE_SPACING);
CreateAcceleratorTable();                         // Accelerator tables don't work with earlier versions
}

void MyBmTree::CreateAcceleratorTable()
{
int AccelEntries[] = { SHCUT_CUT, SHCUT_COPY, SHCUT_DUP, SHCUT_PASTE, SHCUT_TRASH,
                       SHCUT_DELETE, SHCUT_EDIT_BOOKMARK, SHCUT_NEW, SHCUT_NEW_SEPARATOR };
const size_t shortcutNo = sizeof(AccelEntries)/sizeof(int);
MyFrame::mainframe->AccelList->CreateAcceleratorTable(this, AccelEntries,  shortcutNo);
}

int MyBmTree::FindItemIndex(wxTreeItemId itemID)  // Returns where within its folder the item is to be found
{
int index = 0; wxTreeItemIdValue cookie;          // The point is that a folder will contain a mixture of bookmarks & subfolders.  This treats both the same, as does displayorder

wxTreeItemId folderID = GetItemParent(itemID); if (!folderID.IsOk()) return -1; // Find the folder id.  If invalid, forget the whole idea

wxTreeItemId id = GetFirstChild(folderID, cookie);
do
  { if (!id.IsOk())     return -1;               // If invalid, there are no matching children. Flag this by returning -1
    if (id == itemID)    return index;            // Got it.  Return index of location within folder
    ++index;                                      // If not, inc index & try again
    id = GetNextChild(folderID, cookie);
  }
 while (1);
}

void MyBmTree::LoadTree(struct Submenu& MenuStruct, bool LoadFoldersOnly/*=false*/)  // Load tree
{                                          // LoadFoldersOnly is for the folder-only version called from AddBookmark
wxString name(_("Bookmark Folders"));                                     // Give the tree a root. It'll be hidden, but who cares
MyTreeItemData* rootdata = new MyTreeItemData(wxT("/"), name, NextID++);  // Make a suitable data struct
wxTreeItemId rootId = AddRoot(name, TreeCtrlIcon_Folder, TreeCtrlIcon_Folder, (wxTreeItemData*)rootdata); 
SetItemHasChildren(rootId);

AddFolder(MenuStruct, rootId, LoadFoldersOnly);                           // Recursively add all the data

wxTreeItemIdValue cookie;  wxTreeItemId child = GetFirstChild(rootId, cookie); // Expand the first branch, which is the main Bookmarks folder
if (child.IsOk())    Expand(child);
}

bool MyBmTree::AddFolder(struct Submenu& SubMenuStruct, wxTreeItemId parent, bool LoadFoldersOnly/*=false*/, int pos /*=-1*/)
{
wxTreeItemId thisfolder;  bool haschildren = false;
                                        // First thing to do is to create the new folder
wxString foldername = SubMenuStruct.Label;                                         // What to call this folder
MyTreeItemData* data = new MyTreeItemData(wxT(""), foldername, SubMenuStruct.ID);  // Make a suitable data struct                                                  
if (pos == -1)
      thisfolder = AppendItem(parent, foldername, TreeCtrlIcon_Folder, -1, data);       // Append the folder to the tree. This is the standard way
 else   thisfolder = InsertItem(parent, pos, foldername, TreeCtrlIcon_Folder, -1, data);// Insert within parent folder.  Used when method is called by Paste
if (!thisfolder.IsOk()) return false;

    // OK, now fill it with its children.  The order info is stored in a wxString as a binary sequence: '0' means a bookmark, '1' a submenu. Let's hope it isn't corrupted!
for (size_t n=0, bm=0, gp=0; n < SubMenuStruct.displayorder.Len(); n++) // Note that the 2 entry-types each have their own index
  { if (SubMenuStruct.displayorder[n] == '0')                           // This one's an entry, so add it to this folder
      { if (bm >= SubMenuStruct.BMstruct.GetCount()) continue;          // (after a quick sanity check)      
        if (LoadFoldersOnly) continue;                                  // (& assuming we're interested)
                    // Bookmarks & separators both use the same struct
        MyTreeItemData* bmark = new MyTreeItemData(SubMenuStruct.BMstruct[bm]->Path,                 // Make a suitable data struct
                                                          SubMenuStruct.BMstruct[bm]->Label, SubMenuStruct.BMstruct[bm]->ID);
        if (bmark->GetPath() ==_("Separator"))                                                          
              AppendItem(thisfolder,SubMenuStruct.BMstruct[bm]->Label, TreeCtrlIcon_Sep, -1, bmark); // Add a separator to the tree
         else AppendItem(thisfolder,SubMenuStruct.BMstruct[bm]->Label, TreeCtrlIcon_BM, -1, bmark);  // or the bookmark

        haschildren = true;
        ++bm;
      }
     else                                                               // Otherwise it must be a subfolder
      { if (gp >= SubMenuStruct.children.GetCount()) continue;          // (Another sanity check)  
        if (AddFolder(*SubMenuStruct.children[gp], thisfolder, LoadFoldersOnly))  // Use recursion to do the work
          haschildren = true;
        ++gp;
      }
  }

if (haschildren)  SetItemHasChildren(thisfolder);                       // If any children loaded, set button
return true;
}

void MyBmTree::RecreateTree(struct Submenu& MenuStruct)
{
Delete(GetRootItem());
LoadTree(MenuStruct);
}

void MyBmTree::DeleteBookmark(wxTreeItemId id)
{
wxTreeItemId parent = GetItemParent(id);                // Store the id of the parent folder

Delete(id);                                             // Delete the thing

if (!parent.IsOk()) return;
wxTreeItemIdValue cookie;
wxTreeItemId child = GetFirstChild(parent,  cookie);    // Having deleted the bookmark, see if there're any other progeny
SetItemHasChildren(parent, child.IsOk());               // Set the HasChildren button according to result
}

void MyBmTree::OnBeginDrag(wxTreeEvent& event)
{
if (GetItemParent(event.GetItem()) == GetRootItem()) return;  // Don't want to drag Bookmarks folder

m_draggedItem = event.GetItem();
event.Allow();                                          // need explicitly to allow drag
}

void MyBmTree::OnEndDrag(wxTreeEvent& event)
{
if (!m_draggedItem.IsOk()) return;                      // In case we pressed ESC, or got here accidentally(?)

itemSrc = m_draggedItem;  itemDst = event.GetItem();    // Find what & where
if (!itemDst.IsOk())   return;
itemParent = GetItemParent(itemDst);                    // Find destination folder
m_draggedItem = (wxTreeItemId)0l;                       // Reset m_draggedItem

if (wxGetKeyState(WXK_CONTROL))                         // Tell Bookmarks class what to do: either copy/paste if the ctrl key was down
  { parent->Copy(itemSrc);
    parent->Paste(itemDst);
    BriefLogStatus bls(_("Duplicated")); 
  }
 else
  { parent->Move(itemSrc, itemDst);                     // or move if it wasn't
    BriefLogStatus bls(_("Moved")); 
  }
}

void MyBmTree::OnCut()
{
parent->Cut((wxTreeItemId)0l); 
}

void MyBmTree::OnCopy()
{
if (parent->Copy((wxTreeItemId)0l)) BriefLogStatus bls(_("Copied")); 
}

void MyBmTree::OnPaste()
{
parent->Paste((wxTreeItemId)0l);
}

void MyBmTree::ShowContextMenu(wxContextMenuEvent& event)
{
wxMenu menu(wxT(""));

int Firstsection[] = { SHCUT_CUT, SHCUT_COPY, SHCUT_PASTE, SHCUT_DUP, SHCUT_DELETE, wxID_SEPARATOR };
for (size_t n=0; n < 6; ++n)    MyFrame::mainframe->AccelList->AddToMenu(menu, Firstsection[n]);

MyTreeItemData* data = (MyTreeItemData*)GetItemData(GetSelection());
if (data->GetPath() !=_("Separator"))                // Can't edit a separator
  { if (data->GetPath().IsEmpty())                   // It's a folder
          MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_EDIT_BOOKMARK, _("Rename Folder"));
     else MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_EDIT_BOOKMARK, _("Edit Bookmark"));
  }
MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_NEW_SEPARATOR, _("NewSeparator"));
MyFrame::mainframe->AccelList->AddToMenu(menu, SHCUT_NEW, _("NewFolder"));

wxPoint pt = ScreenToClient(wxGetMousePosition());
PopupMenu(&menu, pt.x, pt.y);
}

void MyBmTree::OnMenuEvent(wxCommandEvent& event)
{
switch(event.GetId())
  { 
    case SHCUT_CUT:             parent->Cut((wxTreeItemId)0l); return;
    case SHCUT_COPY:            parent->Copy((wxTreeItemId)0l); return;
    case SHCUT_PASTE:           parent->Paste((wxTreeItemId)0l); return;
    case SHCUT_DUP:             parent->Paste((wxTreeItemId)0l, false, true); return;
    case SHCUT_TRASH:
    case SHCUT_DELETE:          parent->OnDeleteBookmark((wxTreeItemId)0l); return;
    case SHCUT_EDIT_BOOKMARK:   parent->OnEditBookmark(); return;
    case SHCUT_NEW:             parent->OnNewFolder(); return;
    case SHCUT_NEW_SEPARATOR:   parent->OnNewSeparator(); return;
  }
  
event.Skip();
}

void MyBmTree::DoMenuUI(wxUpdateUIEvent& event)
{
MyTreeItemData* data;
bool separator = false;

wxTreeItemId item = GetSelection();
if (item.IsOk())  
  { data = (MyTreeItemData*)GetItemData(item);
    separator = (data->GetPath() ==_("Separator"));
  }

bool selection = (item.IsOk() && ! (item==GetRootItem()));  // If there 'isn't' a selection, item seems to default to root!

switch(event.GetId())
  { case SHCUT_CUT:
    case SHCUT_DELETE: case SHCUT_TRASH:
    case SHCUT_COPY:
    case SHCUT_DUP:              event.Enable(selection); break;
    case SHCUT_PASTE:            event.Enable(selection && parent->bmclip.HasData); break;
    case SHCUT_EDIT_BOOKMARK:    event.Enable(selection && !separator); break;
  }
}

#if !defined(__WXGTK3__)
void MyBmTree::OnEraseBackground(wxEraseEvent& event)  // This is only needed in gtk2 under kde and brain-dead theme-manager, but can cause a blackout in some gtk3(themes?)
{
wxClientDC* clientDC = NULL;  // We may or may not get a valid dc from the event, so be prepared
if (!event.GetDC()) clientDC = new wxClientDC(this);
wxDC* dc = clientDC ? clientDC : event.GetDC();

wxColour colBg = GetBackgroundColour();        // Use the chosen background if there is one
if (!colBg.Ok()) colBg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);

#if wxVERSION_NUMBER < 2900
  dc->SetBackground(wxBrush(colBg, wxSOLID));
#else
  dc->SetBackground(wxBrush(colBg, wxBRUSHSTYLE_SOLID));
#endif
dc->Clear();

if (clientDC) delete clientDC;
}
#endif

BEGIN_EVENT_TABLE(MyBmTree, wxTreeCtrl)
  EVT_TREE_BEGIN_DRAG(XRCID("TREE"), MyBmTree::OnBeginDrag)
  EVT_TREE_END_DRAG(XRCID("TREE"), MyBmTree::OnEndDrag)
  EVT_MENU(wxID_ANY, MyBmTree::OnMenuEvent)    // To process the ContextMenu choices
  EVT_UPDATE_UI(wxID_ANY, MyBmTree::DoMenuUI)
  EVT_CONTEXT_MENU(MyBmTree::ShowContextMenu)
#ifdef __WXX11__
  EVT_RIGHT_UP(MyBmTree::OnRightUp)            // EVT_CONTEXT_MENU doesn't seem to work in X11
#endif
#if !defined(__WXGTK3__)
  EVT_ERASE_BACKGROUND(MyBmTree::OnEraseBackground)
#endif
END_EVENT_TABLE()

//--------------------------------------------------------------------------------------------------------------------

int Bookmarks::m_menuindex = -1;

Bookmarks::Bookmarks()
{
LoadBookmarks();
}

void Bookmarks::LoadBookmarks()  // Loads bookmarks from config, & puts them in the array & the bookmarks menu
{
config = wxConfigBase::Get(); if (config==NULL)   return;

if (!config->HasGroup(wxT("Bookmarks"))) SaveDefaultBookmarkDefault();  // If there's nothing in ini file, we need to save a stub

wxMenuBar* MenuBar = MyFrame::mainframe->GetMenuBar();
if (MenuBar==NULL)  { wxLogError(_("Couldn't load Menubar!?")); return; }

MenuStruct.ClearData();                           // Make sure the arrays are empty:  they won't be for a re-load!
bookmarkID = ID_FIRST_BOOKMARK;                   // Similarly reset bookmarkID
MenuStruct.ID = GetNextID();                      //   & use it indirectly to id MenuStruct

wxCHECK_RET(m_menuindex > wxNOT_FOUND, wxT("Bookmarks menu index not set")); // Should have been set when the bar was loaded
wxMenu* menu = MenuBar->GetMenu(m_menuindex);     // Find the menu for the group
if (!menu)  { wxLogError(_("Couldn't find menu!?")); return; }

MenuStruct.pathname =  wxT("/Bookmarks");
MenuStruct.FullLabel.Empty();                     // Needed for re-entry during Save/Load
MenuStruct.thismenu = menu;
LoadSubgroup(MenuStruct);                         // Recursively load the lot

config->Read(wxT("/Misc/DefaultBookmarkFolder"), &LastSubmenuAddedto);  // Load the default folder to which to addbookmarks

m_altered = false;                                // Reset the modified flag.  Done here as we'll be dealing with changes by saving/reloading

config->SetPath(wxT("/"));
}

void Bookmarks::LoadSubgroup(struct Submenu& SubMenuStruct)
{
wxString OldPath = config->GetPath();
if (SubMenuStruct.pathname.IsEmpty())  { wxLogError(_("This Section doesn't exist in ini file!?")); return; }  // Check there is a valid path to be set
if (!config->Exists(SubMenuStruct.pathname))   return;      // Check that there IS a config-file group of this name.  There won't be the 1st time the program loads
config->SetPath(SubMenuStruct.pathname);                    // Set new Path from the struct

    // The plan is:  Load everything into the appropriate storage.  Then use the order info to add entries & submenus to the parent menu as required
wxString key, bmprefix(wxT("bm_")), labelprefix(wxT("lb_")), item; unsigned int count;

config->Read(wxT("folderlabel"), &item, _("Bookmarks"));    // Load folder name:  the real label, not /a, /b etc
SubMenuStruct.Label = item;
SubMenuStruct.FullLabel = SubMenuStruct.FullLabel + wxCONFIG_PATH_SEPARATOR + item; // Parent will have loaded its own FullLabel, so we just append ours

config->Read(wxT("displayorder"), &item, wxT(""));          // Now load & store the displayorder
SubMenuStruct.displayorder = item;
if (item.IsEmpty()) { config->SetPath(OldPath); return; }

count = config->GetNumberOfEntries();     // First do the entries.  These are stored as bookmark/label pairs.  (Let's hope they STAY paired)
if (count) { count /= 2; --count; }     // Check there ARE entries before adjusting!
                      // count equals n * bmark, n * label, + 1 * foldername + 1 * displayorder; ie 2n+2.  /2 means we have n+1
      
for (uint n=0; n < count; n++)          // We can therefore create the key for every item, from bm_0 to bm_(count-1)
  { struct BookmarkStruct* ItemStruct = new BookmarkStruct; // Make a new struct
    key.Printf(wxT("%u"), n);                               // Make key hold 0, 1, 2 etc
    config->Read(bmprefix+key, &item);                      // Load the bookmark
    if (item.IsEmpty()) continue;                           // Check it exists
    ItemStruct->Path = item;                                //   & add it to the struct
    config->Read(labelprefix+key, &item);                   // Ditto label
    if (item.IsEmpty())  item = ItemStruct->Path;           // If it doesn't exist, use the Path as its own label
    ItemStruct->Label = item;
    ItemStruct->ID = GetNextID();                           // Create an id for the bookmark
    SubMenuStruct.BMstruct.Add(ItemStruct);                 // Add this struct-let to the main struct
  }

long dummy;
bool bCont = config->GetFirstGroup(item, dummy);   // Now load the subgroups

while (bCont) 
  { struct Submenu* child = new struct Submenu;    // Create a new child struct
    wxMenu* menu = new wxMenu();                   // Create the submenu for this subgroup
    child->thismenu = menu;                        // Put relevant data into the child struct
    child->pathname = SubMenuStruct.pathname + wxCONFIG_PATH_SEPARATOR + item; // Append the itemname to the path.  If item is 'a',  child->pathname will be Bookmarks/b/a
    child->FullLabel = SubMenuStruct.FullLabel;    // Put our FullLabel into the child's one.  The child will append its own label as it's loading
    child->ID = GetNextID();                       // Give it an id
    SubMenuStruct.children.Add(child);             // Add child to the parent struct

    bCont = config->GetNextGroup(item, dummy); 
  }

    // OK, now fill the menu.  The order info is stored in a wxString as a binary sequence: '0' means a bookmark, '1' a submenu. Let's hope it isn't corrupted!
for (size_t n=0, bm=0, gp=0; n < SubMenuStruct.displayorder.Len(); n++)  // Note that the 2 entry-types each have their own index
  { if (SubMenuStruct.displayorder[n] == '0')                            // This one's an entry, so add it to this menu
      { if (bm >= SubMenuStruct.BMstruct.GetCount()) continue;           // (after a quick sanity check)                              
        if (SubMenuStruct.BMstruct[bm]->Path ==wxT("Separator"))  SubMenuStruct.thismenu->AppendSeparator(); // as a separator
          else SubMenuStruct.thismenu->Append(SubMenuStruct.BMstruct[bm]->ID, // or a bookmark's label & help (its target)
                                              SubMenuStruct.BMstruct[bm]->Label, SubMenuStruct.BMstruct[bm]->Path);
        ++bm;
      }
     else                                                                // Otherwise it must be a submenu  
      { if (gp >= SubMenuStruct.children.GetCount()) continue;           // (Another sanity check)  
        LoadSubgroup(*SubMenuStruct.children[gp]);                       // Use recursion to do the work
                                    // Add the completed submenu to the parent menu
        SubMenuStruct.thismenu->Append(SubMenuStruct.children[gp]->ID, 
                                            SubMenuStruct.children[gp]->Label, SubMenuStruct.children[gp]->thismenu);
        ++gp;
      }
  }
  
config->SetPath(OldPath);                          // Finally restore the original path
}

void Bookmarks::SaveBookmarks()  // Saves bookmarks into config
{
config = wxConfigBase::Get();                     // Find the config data (in case it's changed location recently)
if (config==NULL)  { wxLogError(_("Couldn't load configuration!?")); return; }

config->DeleteGroup(wxT("/Bookmarks"));           // Delete current info (otherwise renamed/deleted data would remain, & be reloaded in future)
SaveSubgroup(MenuStruct);                         // Do the Save by recursion
config->Write(wxT("/Misc/DefaultBookmarkFolder"), LastSubmenuAddedto);  // Take the opportunity to save default folder
config->Flush();

wxMenuBar* MenuBar = MyFrame::mainframe->GetMenuBar();                  // We now have to destroy the menu structure, so it can be born again
if (MenuBar==NULL)  { wxLogError(_("Couldn't load Menubar!?")); return; }

wxCHECK_RET(m_menuindex > wxNOT_FOUND, wxT("Bookmarks menu index not set")); // Should have been set when the bar was loaded
wxMenu* mainmenu = MenuBar->GetMenu(m_menuindex); // Find the menu for the group
if (!mainmenu)  { wxLogError(_("Couldn't find menu!?")); return; }

for (size_t count=mainmenu->GetMenuItemCount(); count > 3 ; --count)    // Skip the 1st 3 items, they're Add, Manage & a Separator
  mainmenu->Destroy(mainmenu->GetMenuItems().Item(count-1)->GetData()); // Destroy all the others, including submenus

config->SetPath(wxT("/"));
}

void Bookmarks::SaveSubgroup(struct Submenu& SubMenuStruct)  // Does the actual saving for this (sub)group
{
wxString OldPath = config->GetPath();
config->SetPath(SubMenuStruct.pathname);  // Set new Path from the struct, and creates it too

wxString key, bmprefix(wxT("bm_")), labelprefix(wxT("lb_"));
                        // Start by saving the bookmarks themselves
for (uint n=0; n < SubMenuStruct.BMstruct.GetCount(); n++)
  { key.Printf(wxT("%u"), n);                                             // Make key hold 0, 1, 2 etc
    config->Write(bmprefix+key, SubMenuStruct.BMstruct[n]->Path);         // Save each bookmark
    config->Write(labelprefix+key, SubMenuStruct.BMstruct[n]->Label);     //  & its label
  }
key =wxT("displayorder"); config->Write(key, SubMenuStruct.displayorder); // Now store the display-order
key = wxT("folderlabel"); config->Write(key, SubMenuStruct.Label);        //    & the folder label

            // Next the subgroups, using recursion.  We recreate the pathnames here.  That's because the old ones 
            // may have become redundant due to deletions or new groups being inserted.
size_t count = SubMenuStruct.children.GetCount();
for (size_t n=0; n < count; n++)
  { wxString name = CreateSubgroupName(n, count);                         // Get the next available configpath name for the child group
    SubMenuStruct.children[n]->pathname = SubMenuStruct.pathname + wxCONFIG_PATH_SEPARATOR + name;
    SaveSubgroup(*SubMenuStruct.children[n]);
  }
  
config->SetPath(OldPath);                      // Finally restore the original path
}

void Bookmarks::SaveDefaultBookmarkDefault()  // Saves a stub of Bookmarks. Without, loading isn't right
{
wxConfigBase::Get()->Write(wxT("Bookmarks/folderlabel"), _("Bookmarks"));
wxConfigBase::Get()->Write(wxT("Bookmarks/displayorder"), wxT(""));
}

void Bookmarks::AddBookmark(wxString& newpath)  // Called from main menu.  Newpath is the currently-selected dir- or file-pane path
{
if (newpath.IsEmpty()) return;

MyBookmarkDialog dlg(this);  adddlg = &dlg;
wxXmlResource::Get()->LoadDialog(&dlg, MyFrame::mainframe->GetActivePane(), wxT("AddBookmarkDlg"));
LoadPreviousSize(&dlg, wxT("AddBookmarkDlg"));

struct Submenu* subfolder = FindSubmenuStruct(MenuStruct, LastSubmenuAddedto);       // Check the last-used folder still exists
if (subfolder==NULL) LastSubmenuAddedto =  wxString(wxT("/")) + _("Bookmarks");      // If not, use 'root'
wxStaticText* folderStatic = (wxStaticText*)dlg.FindWindow(wxT("AddBookmark_Path")); // Put the default folder into static
folderStatic->SetLabel(LastSubmenuAddedto);
#if wxVERSION_NUMBER > 2904
                  // Previously this was unnecessary; the statictexts knew their own size. However since wx3.1 they don't; perhaps because of the ellipsize feature
                  folderStatic->SetMinSize(folderStatic->GetSizeFromTextSize(folderStatic->GetTextExtent(LastSubmenuAddedto)));
#endif

wxTextCtrl* bmkname = (wxTextCtrl*)dlg.FindWindow(wxT("AddBookmark_Bookmark"));      // Similarly load the bookmark & label names
bmkname->ChangeValue(newpath);

wxTextCtrl* labelname = (wxTextCtrl*)dlg.FindWindow(wxT("AddBookmark_Label"));  
labelname->ChangeValue(newpath);

int result = dlg.ShowModal();
SaveCurrentSize(&dlg, wxT("AddBookmarkDlg"));
if (result != wxID_OK) return;

wxString newmark = bmkname->GetValue();           // Reload the data in case the user has altered it
wxString label = labelname->GetValue();
if (newmark.IsEmpty()) return;                    // We can't cope with a blank bookmark,
if (label.IsEmpty())       label=newmark;         //  but we can with a blank label:  just use the bookmark name

subfolder = FindSubmenuStruct(MenuStruct, LastSubmenuAddedto);  // Get the requested folder
if (subfolder==NULL) { wxLogError(_("Sorry, couldn't locate that folder"));  return; }

struct BookmarkStruct* ItemStruct = new BookmarkStruct; // Make a new struct
ItemStruct->Path = newmark;                             // & add the bookmark itself
ItemStruct->Label = label;                              // & the label
subfolder->BMstruct.Add(ItemStruct);                    // Add this struct-let to the main struct
subfolder->displayorder += '0';                         // and add a new book-marker to the order string

SaveBookmarks();                                        //  and make it happen by saving/reloading everything
LoadBookmarks();

BriefLogStatus bls(_("Bookmark added"));
}

void Bookmarks::OnChangeFolderButton()
{
size_t index;

MyBookmarkDialog dlg(this); mydlg = &dlg;
wxXmlResource::Get()->LoadDialog(&dlg, adddlg, wxT("ManageBookmarks")); // Reuse the ManageBookmarks dialog

wxButton* NewSeparatorBtn  = (wxButton*)dlg.FindWindow(wxT("NewSeparator"));
NewSeparatorBtn->Show(false);       // This time, we don't want to add separators, so hide and remove from sizer
wxSizer* sizer = NewSeparatorBtn->GetContainingSizer();
if (sizer->Detach(NewSeparatorBtn)) NewSeparatorBtn->Destroy();

if (!LoadPreviousSize(&dlg, wxT("ManageBookmarks")))
  { wxSize size = wxGetDisplaySize();                            // Get a reasonable dialog size.  WE have to, because the treectrl has no idea how big it is
    dlg.SetSize(2*size.x/3, 2*size.y/3);
  }
dlg.CentreOnScreen();

tree = (MyBmTree*)dlg.FindWindow(wxT("TREE"));
tree->Init(this);                                            // Do the bits that weren't done in XML, eg load images
tree->LoadTree(MenuStruct, true);                            // Recursively load the folder & data into the tree. True means don't load bookmarks/separators
  
int result = dlg.ShowModal();
SaveCurrentSize(&dlg, wxT("ManageBookmarks"));
if (result != wxID_OK) return;


wxTreeItemId sel = tree->GetSelection();
MyTreeItemData* data = (MyTreeItemData*)tree->GetItemData(sel);

if (MenuStruct.ID == data->GetID())                          // FindSubmenu doesn't test 'root' folder, so do it here
  LastSubmenuAddedto = MenuStruct.FullLabel;                 // If matches, change the default folder to new selection
 else 
  { struct Submenu* menu = FindSubmenu(MenuStruct, data->GetID(), index);  //  otherwise recurse thru the folders to find selection
    if (menu==NULL) return;
    LastSubmenuAddedto = menu->children[index]->FullLabel;   // Change the default folder to new selection
  }

wxStaticText* folderStatic = (wxStaticText*)adddlg->FindWindow(wxT("AddBookmark_Path"));  // Put the default folder into static
folderStatic->SetLabel(LastSubmenuAddedto);                  // Load the new default into AddBookmarks dialog
#if wxVERSION_NUMBER >= 3100
  folderStatic->SetMinSize(folderStatic->GetSizeFromTextSize(folderStatic->GetTextExtent(LastSubmenuAddedto)));
#endif

adddlg->GetSizer()->Layout();                                // For some reason, if we don't do this, the new folderStatic is LeftJustified, not centred
}

struct Submenu* Bookmarks::FindSubmenuStruct(Submenu& SubMenu, wxString& name)    // Uses recursion to match a submenu to the passed name
{ // NB We compare the FullLabels, eg Bookmarks/Programming/Projects,  not just the 'Projects' bit
if (name == SubMenu.FullLabel)  return &SubMenu;             // If this is the one, return it

for (size_t n=0; n < SubMenu.children.GetCount(); n++)       // If not, recurse through the submenus
  { Submenu* answer = FindSubmenuStruct(*SubMenu.children[n], name);
    if (answer != NULL) return answer;
  }
  
return (struct Submenu*)NULL;                                // If we're here, it failed
}

struct Submenu* Bookmarks::FindBookmark(Submenu& SubMenu, unsigned int id, size_t& index)  // Locates bmark id within SubMenu (or a child submenu)
{
if (id < ID_FIRST_BOOKMARK || id >= bookmarkID) return (struct Submenu*)NULL;

for (size_t n=0; n < SubMenu.BMstruct.GetCount(); n++)      // For every bookmarkstruct held by this 'menu'
  if (SubMenu.BMstruct[n]->ID == id)                        //  see if the id's match.  Will work for bmarks & separators, but not for folders (which are in a different struct anyway)
    { index = n;                                            // 'Return' the location within folder, by inserting it in reference
      return &SubMenu;                                      //  Return the bookmark's folder
    }

for (size_t n=0; n < SubMenu.children.GetCount(); n++)      // If not, recurse through the child submenus
  { Submenu* answer = FindBookmark(*SubMenu.children[n], id, index);
    if (answer != NULL) return answer;
  }
  
return (struct Submenu*)NULL;                               // If we're here, it failed
}

struct Submenu* Bookmarks::FindSubmenu(Submenu& SubMenu, unsigned int id, size_t& index)  // Locates submenu id within SubMenu (or a child submenu)
{
if (id < ID_FIRST_BOOKMARK || id >= bookmarkID) return (struct Submenu*)NULL;

          // Note that we don't check if the passed menu is itself the target (doing so would prevent using recursion)
for (size_t n=0; n < SubMenu.children.GetCount(); n++)      // For every child of this 'menu'
  { if (SubMenu.children[ n ]->ID == id)                    //    see if the id's match
      { index = n;                                          // If so, 'Return' the location within menu, by inserting it in reference
        return &SubMenu;                                    // Return this menu, the located one's parent
      }
    Submenu* answer = FindSubmenu(*SubMenu.children[n], id, index);  // Failing that, recurse through each child submenu
    if (answer != NULL) return answer;                      // Non-null answer means found    
  }
  
return (struct Submenu*)NULL;                               // If we're here, it failed
}

void Bookmarks::ManageBookmarks()
{
MyBookmarkDialog dlg(this);   mydlg = &dlg;
wxXmlResource::Get()->LoadDialog(&dlg, MyFrame::mainframe->GetActivePane(), wxT("ManageBookmarks")); // Load the appropriate dialog

if (!LoadPreviousSize(&dlg, wxT("ManageBookmarks")))
  { wxSize size = wxGetDisplaySize();                       // Get a reasonable dialog size.  WE have to, because the treectrl has no idea how big it is
    dlg.SetSize(2*size.x/3, 2*size.y/3);
  }
dlg.CentreOnScreen();

tree  = (MyBmTree*)dlg.FindWindow(wxT("TREE"));
tree->Init(this);                                           // Do the bits that weren't done in XML, eg load images
tree->LoadTree(MenuStruct);                                 // Recursively load the folder & bookmark data into the tree
    
int result = dlg.ShowModal();
SaveCurrentSize(&dlg, wxT("ManageBookmarks"));
if (result != wxID_OK)
  { if (m_altered)
      { wxMessageDialog dialog(MyFrame::mainframe, _("Lose all changes?"), _("Are you sure?"), wxYES_NO | wxICON_QUESTION);
        if (dialog.ShowModal() != wxID_YES)  { SaveBookmarks(); LoadBookmarks(); }  // Make changes permanent by saving/reloading everything
        return;
      }
  }
  
if (m_altered)
  { SaveBookmarks(); LoadBookmarks(); }                     // Make changes permanent by saving/reloading everything
}

void Bookmarks::OnNewFolder()
{
wxTreeItemIdValue cookie; int loc = -1, pos = -1, treepos = -1; 
wxString newlabel;

wxTreeItemId folderID = tree->GetFirstChild(tree->GetRootItem(), cookie);  // Get the tree Bookmarks folder, to use as default location
struct Submenu* SubMenu = &MenuStruct;                          // Similarly, use MenuStruct as the default menu

  // Having done that, see if there's somewhere to put the thing that's more user-defined!  If current-selection's a bmark, add after it.  If a folder, make it its 1st child
wxTreeItemId id = tree->GetSelection();
if (id.IsOk())  WhereToInsert(id, folderID, &SubMenu, loc, pos, treepos, true);  // If selection's valid, use subfunction to locate where it is in terms of tree/menu

                  // OK, one way or another we know where we're going to do the insertion.  So get the new folder's name
wxTextEntryDialog getname(tree->GetParent(), _("What Label would you like for the new Folder?"));
do                                                              // Get the name in a loop, in case of duplication
  { if (getname.ShowModal() != wxID_OK)  return;
    
    newlabel = getname.GetValue();                              // Get the desired label from the dlg
    if (newlabel.IsEmpty())  return;
    bool flag = false;
    for (size_t n=0; n < SubMenu->children.GetCount(); n++)     // Ensure it's not a duplicate with the same path
      if (SubMenu->children[n]->Label == newlabel)
          { wxString msg; msg.Printf(_("Sorry, there is already a folder called %s\n                     Try again?"), newlabel.c_str());
            wxMessageDialog dialog(tree->GetParent(), msg, _("Oops!"), wxYES_NO);
            if (dialog.ShowModal() != wxID_YES) return;
              else { flag = true; continue; }
          }
    if (flag) continue;
    break;                                                      // If we're here, no match so all's well
  }
 while (1);
  
                                      // Now do the construction
struct Submenu* child = new struct Submenu;         // Create a new child struct
    // *Don't* create a new menu: it won't be used (as we're about to Save/LoadBookmarks), and it'll cause a memory leak
child->Label = newlabel;
child->FullLabel = SubMenu->FullLabel + wxCONFIG_PATH_SEPARATOR + newlabel;
child->ID = GetNextID();                            // Give it an id

MyTreeItemData* newdata = new MyTreeItemData(wxT(""), newlabel, child->ID);  // Make a new TreeItemData

        // There are now 3 possibilities.  We might be appending throughout, flagged by everything == -1
        // Otherwise, we might be inserting throughout; or we might be appending to BMstruct, yet inserting into the tree (after last bm but before a subfolder)
if (treepos == -1)                                  // If treepos is -1, so must loc etc be.  So do a global append
  { 
    SubMenu->children.Add(child);                   // Append the struct to the main struct
    SubMenu->displayorder += '1';                   // Append to the order string
    tree->AppendItem(folderID, newlabel, TreeCtrlIcon_Folder, -1, newdata);  // Append to the tree
  }  
 else  
   { tree->InsertItem(folderID, treepos, newlabel, TreeCtrlIcon_Folder,  -1, newdata);  // Otherwise Insert into tree. InsertBefore treepos position
    SubMenu->displayorder.insert(pos+1, 1, '1');    // and insert a new marker into the order string.  This is InsertAfter!
    if (loc == -1)                                  // If treepos was OK, but loc -1, this means putting item at end of bmarks, but before a subfolder
          SubMenu->children.Add(child);             // So append the struct, but use the inserted displayorder from 2 lines above
     else  SubMenu->children.Insert(child, loc);    // If loc was OK too,  do a standard InsertBefore
  }

m_altered = true;                                   // Set the modified flag
BriefLogStatus bls(_("Folder added"));
}

void Bookmarks::OnNewSeparator()
{
wxTreeItemIdValue cookie; int loc = -1, pos = -1, treepos = -1;

wxTreeItemId folderID = tree->GetFirstChild(tree->GetRootItem(), cookie); // Get the tree Bookmarks folder, to use as default location

struct Submenu* SubMenu = &MenuStruct;                  // Similarly, use MenuStruct as the default menu

  // Having done that, see if there's somewhere to put the thing that's more user-defined!  If current-selection's a bmark, add after it.  If a folder, make it its 1st child
wxTreeItemId id = tree->GetSelection();
if (id.IsOk())  WhereToInsert(id, folderID, &SubMenu, loc, pos, treepos, false);  // If selection is valid, use subfunction to locate where it is in terms of tree/menu

                  // OK, one way or another we know where we're going to do the insertion.  So do the construction
struct BookmarkStruct* ItemStruct = new BookmarkStruct; // Make a new struct
ItemStruct->Path = ItemStruct->Label = _("Separator");
ItemStruct->ID = GetNextID();
MyTreeItemData* newdata = new MyTreeItemData(ItemStruct->Path, ItemStruct->Label, ItemStruct->ID);

        // There are now 3 possibilities.  We might be appending throughout, flagged by everything == -1
        // Otherwise, we might be inserting throughout; or we might be appending to BMstruct, yet inserting into the tree (after last bm but before a subfolder)
if (treepos == -1)                                      // If treepos is -1, so must loc etc be.  So do a global append
  { 
    SubMenu->BMstruct.Add(ItemStruct);                  // Append the struct-let to the main struct
    SubMenu->displayorder += wxT('0');                  // Append to the order string
    tree->AppendItem(folderID, ItemStruct->Label, TreeCtrlIcon_Sep, -1, newdata);  // Append to the tree
  }  
 else  
   { tree->InsertItem(folderID, treepos, ItemStruct->Label, TreeCtrlIcon_Sep,  -1, newdata);  // Otherwise Insert into tree. InsertBefore treepos position
    SubMenu->displayorder.insert(pos, 1, wxT('0'));     // and insert a new marker into the order string.  This is InsertAfter!
    if (loc == -1)                                      // If treepos was OK, but loc -1, this means putting item at end of bmarks, but before a subfolder
          SubMenu->BMstruct.Add(ItemStruct);            // So append the struct-let, but use the inserted displayorder from 2 lines above
     else  SubMenu->BMstruct.Insert(ItemStruct, loc);   // If loc was OK too,  do a standard InsertBefore
  }

m_altered = true;                                       // Set the modified flag
BriefLogStatus bls(_("Separator added"));
}

void Bookmarks::Move(wxTreeItemId itemSrc, wxTreeItemId itemDst)
{
if (!(itemSrc.IsOk() &&  itemDst.IsOk()))    return;
if (itemSrc == itemDst)                    return;      // Don't want to be pasting onto ourselves, especially followed by Delete!

bool samefolder = false;

if  (tree->GetItemParent(itemSrc) == tree->GetItemParent(itemDst)) // Are we moving within a folder?
    samefolder = true;
 else
  { if (tree->GetItemParent(itemSrc) ==  itemDst)
      { MyTreeItemData* data = (MyTreeItemData*)tree->GetItemData(itemDst); // Check whether itemDst is a folder
        if (data->GetPath().IsEmpty())    samefolder = true;  // If so, we ARE still moving within same folder:  moving to its beginning
      }
  }

    // Move by Copy->Paste->DeleteOriginal.  The reason for this is, were we to Cut->Paste and paste fails, we'd've lost the source data.
    // However if we Copy->Paste within a folder, we've got to turn off duplicate-testing (which will always be +ve).  No need anyway, there can't be a real duplicate.
if (Copy(itemSrc))                  // If copy works, paste.  Otherwise don't
  if (Paste(itemDst, samefolder))   // If paste worked, delete.  Otherwise don't
    OnDeleteBookmark(itemSrc);      // Doing it this way means that, if Paste is aborted because of duplication, the source isn't lost
}


bool Bookmarks::Cut(wxTreeItemId item/*=null*/)
{
if (Copy(item))                                 // If copy works, delete.  Otherwise don't
  if (OnDeleteBookmark(item))  return true;

return false;
}

bool Bookmarks::Copy(wxTreeItemId item/*=null*/)    // Fill local clipboard, with item if it exists, else with Selection
{
size_t index=0;

if (!item.IsOk())  item = tree->GetSelection();   // If we weren't passed a valid item, use selection
if (!item.IsOk()) return false;                   // Either way, check validity
bmclip.Clear();                                   // Clear the clipboard

                        // Load everything into the local clipboard, bmclip
MyTreeItemData* data = (MyTreeItemData*)tree->GetItemData(item);
bmclip.data = *data;                              // Store the TreeItemData

if (bmclip.data.GetPath().IsEmpty())              // Empty path, so it must be a folder
  { struct Submenu* parentmenu = FindSubmenu(MenuStruct, data->GetID(), index);  // Find the parent folder.  Index will be loaded with position within it
    if (parentmenu==NULL)   return false;
    bmclip.Menu = *parentmenu->children[ index ]; // Store the folder data
    bmclip.IsFolder = true;
    bmclip.FolderHasChildren = tree->ItemHasChildren(item); // Need to store if has children, since paste will need to set button accordingly
  }
 else                             // Otherwise it must be a bookmark or separator.  They're dealt with the same
   { struct Submenu* parentmenu = FindBookmark(MenuStruct, data->GetID(), index); // Find the parent folder.  Index will be loaded with position within it
    if (parentmenu==NULL)   return false;
    bmclip.BMarray.Add (new BookmarkStruct(*parentmenu->BMstruct[ index ]));      // Store the bookmark
    bmclip.IsFolder = false;
  }
  
bmclip.HasData = true; return true;
}

void Bookmarks::AdjustFolderID(struct Submenu& SubMenu)  // Used for folder within clipboard.  If we don't adjust IDs, they'll be duplication & crashes
{
SubMenu.ID = GetNextID();

for (size_t n=0; n < SubMenu.children.GetCount(); n++)      // Redo any child folders by recursion
    AdjustFolderID(*SubMenu.children[n]);
}

bool Bookmarks::Paste(wxTreeItemId item/*=null*/, bool NoDupCheck/*=false*/, bool duplicating/*=false*/) // Used by both Ctrl-V and DnD, and Dup.  DnD will provide item for drop location
{
int loc = -1, pos = -1, treepos = -1;
struct Submenu* SubMenu; wxTreeItemId folderID; bool IsFolder; 
MyTreeItemData *olddata=NULL, *newdata; struct Submenu* child; struct BookmarkStruct* ItemStruct; wxString newname;

if (!item.IsOk())                                         // If no item was passed, it must be a Dup, or a genuine Paste
  { if (!(duplicating || bmclip.HasData)) return false;   // Check there's something TO paste, if we're trying to
    item = tree->GetSelection();                          // If we weren't passed a valid item, use selection
  }
if (!item.IsOk()) return false;

if (duplicating)
  { olddata = (MyTreeItemData*)tree->GetItemData(item);
    IsFolder = olddata->GetPath().IsEmpty();
  }
 else IsFolder = bmclip.IsFolder;

WhereToInsert(item, folderID, &SubMenu, loc, pos, treepos, IsFolder, duplicating);  // Use subfunction to locate where to paste in terms of tree & menu
if (SubMenu==NULL) return false;                          // If SubMenu is null, something went wrong so abort.  There isn't any sensible default here

if (duplicating || (IsFolder && ! NoDupCheck))  // If a folder (& we're not just moving it within it parent), check for duplicate Label.  If BM/Sep, don't: they CAN be duplicates
  { bool clash = false;
    if (!duplicating)
      for (size_t n=0; n < SubMenu->children.GetCount(); n++)  // Ensure there's not a duplicate in the same path
        if (SubMenu->children[n]->Label == bmclip.Menu.Label)
            { wxString msg; msg.Printf(_("Sorry, there is already a folder called %s\n                  Change the name?"), bmclip.Menu.Label.c_str());
              wxMessageDialog dialog(tree->GetParent(), msg, _("Oops?"), wxYES_NO);
              if (dialog.ShowModal() != wxID_YES) return false;
                else { clash = true; break; }
            }
    if (clash || (duplicating && IsFolder))
      { wxString msg; msg = duplicating ? _("What would you like to call the Folder?") : _("What Label would you like for the Folder?");
        wxTextEntryDialog getname(tree->GetParent(), msg);
        do                                                // Get the name in a loop, in case of duplication
          { if (getname.ShowModal() != wxID_OK)  return false;
            
            newname = getname.GetValue();                 // Get the desired label from the dlg
            if (newname.IsEmpty()) return false;

            bool flag = false;
            for (size_t n=0; n < SubMenu->children.GetCount(); n++)  // Ensure it's not a duplicate with the same path
              if (SubMenu->children[n]->Label == newname)
                { wxString msg; msg.Printf(_("Sorry, there is already a folder called %s\n                  Change the name?"), newname.c_str());
                  wxMessageDialog dialog(tree->GetParent(), msg, _("Oops"), wxYES_NO);
                  if (dialog.ShowModal() != wxID_YES) return false;
                    else { flag = true; continue; }
                }
            if (flag) continue;
            
            if (!duplicating) bmclip.Menu.Label = newname;  // If we're here, no clash so all's well
            break;
          }
         while (1);
      }
  }
                                      // Now do the construction
if (duplicating)
  { size_t index=0;
    if (IsFolder)                                      
      { struct Submenu* parentmenu = FindSubmenu(MenuStruct, olddata->GetID(), index);  // Find the parent folder.  Index will be loaded with position within it
        if (parentmenu==NULL)   return false;
        child = new struct Submenu(*parentmenu->children[ index ]);  // Create a new child struct using copy ctor
            // *Don't* create a new menu: it won't be used (as we're about to Save/LoadBookmarks), and it'll cause a memory leak
        child->ID = GetNextID();                              // Give it a unique id, don't use the original one (lest multiple pastes?)
        child->Label = newname;
      }  
     else
      { struct Submenu* parentmenu = FindBookmark(MenuStruct, olddata->GetID(), index);  // Find the parent folder.  Index will be loaded with position within it
        if (parentmenu==NULL)   return false;
        ItemStruct = new BookmarkStruct(*parentmenu->BMstruct[ index ]);  // Make a new struct using copy ctor
        ItemStruct->ID = GetNextID();                         // Give it a unique id, don't use the original one (lest multiple pastes?)
        newdata = new MyTreeItemData(ItemStruct->Path, ItemStruct->Label, ItemStruct->ID);
      }
  }
 else  
  { if (IsFolder)                                      
      { child = new struct Submenu(bmclip.Menu);              // Create a new child struct using copy ctor
            // *Don't* create a new menu: it won't be used (as we're about to Save/LoadBookmarks), and it'll cause a memory leak
        child->ID = GetNextID();                              // Give it a unique id, don't use the original one (lest multiple pastes?)
      }  
     else
      { if (bmclip.BMarray.IsEmpty())  return false;
        ItemStruct = new BookmarkStruct(*bmclip.BMarray[0]);// Make a new struct using copy ctor
        ItemStruct->ID = GetNextID();                         // Give it a unique id, don't use the original one (lest multiple pastes?)
        newdata = new MyTreeItemData(ItemStruct->Path, ItemStruct->Label, ItemStruct->ID);
      }
  }
        // There are now 3 possibilities.  We might be appending throughout, flagged by everything == -1
        // Otherwise, we might be inserting throughout; or we might be appending to BMstruct, yet inserting into the tree (after last bm but before a subfolder)
if (IsFolder)  
  { child->FullLabel = SubMenu->FullLabel + wxCONFIG_PATH_SEPARATOR + child->Label;  // Create the altered FullLabel
    AdjustFolderID(*child);                                 // Redo the IDs of the menu+children, to avoid having 2 menus with identical IDs
                                                            // NB We do this here, not in Copy(), since this copes with multiple Pastes
    if (treepos == -1)                                      // If treepos is -1, so must loc etc be.  So do a global append
      { SubMenu->children.Add(child);                       // Append the struct to the main struct
        SubMenu->displayorder += '1';                       // Append to the order string
        if (!tree->AddFolder(*child, folderID))  return false; // Append to the tree, recursively if there are children
      }  
     else  
      { if (!tree->AddFolder(*child, folderID, false, treepos))  return false;  // Insert into the tree, recursively if there are children. InsertBefore treepos position
        SubMenu->displayorder.insert(pos+1, 1, '1');        // and insert a new marker into the order string.  This is InsertAfter!
        if (loc == -1)                                      // If treepos was OK, but loc -1, this means putting item at end of bmarks, but before a subfolder
              SubMenu->children.Add(child);                 // So append the struct, but use the inserted displayorder from 2 lines above
         else  SubMenu->children.Insert(child, loc);        // If loc was OK too,  do a standard InsertBefore
      }

  }
 else
  { if (treepos == -1)                                      // If treepos is -1, so must loc etc be.  So do a global append 
      { SubMenu->BMstruct.Add(ItemStruct);                  // Append the struct-let to the main struct
        SubMenu->displayorder += '0';                       // Append to the order string
        if (newdata->GetPath() ==_("Separator"))
              tree->AppendItem(folderID, ItemStruct->Label, TreeCtrlIcon_Sep, -1, newdata);  // Append Separator to the tree
         else   tree->AppendItem(folderID, ItemStruct->Label, TreeCtrlIcon_BM, -1, newdata); // or the bookmark
      }  
     else  
      { if (newdata->GetPath() ==_("Separator"))
              tree->InsertItem(folderID, treepos, ItemStruct->Label, TreeCtrlIcon_Sep,  -1, newdata);// Otherwise Insert into tree. InsertBefore treepos position
         else tree->InsertItem(folderID, treepos, ItemStruct->Label, TreeCtrlIcon_BM,  -1, newdata);  
        SubMenu->displayorder.insert(pos, 1, '0');          // and insert a new marker into the order string.  This is InsertAfter!
        if (loc == -1)                                      // If treepos was OK, but loc -1, this means putting item at end of bmarks, but before a subfolder
              SubMenu->BMstruct.Add(ItemStruct);            // So append the struct-let, but use the inserted displayorder from 2 lines above
         else SubMenu->BMstruct.Insert(ItemStruct, loc);    // If loc was OK too,  do a standard InsertBefore
      }
  }

if (duplicating) BriefLogStatus bls(_("Duplicated"));  
  else BriefLogStatus bls(_("Pasted"));  
m_altered = true; return true;                              // Set the modified flag
}

void Bookmarks::WhereToInsert(wxTreeItemId id, wxTreeItemId& folderID, struct Submenu** SubMenu, int& loc, int& pos, int& treepos, bool InsertingFolder, bool Duplicating/*=false*/)
{
size_t index;
MyTreeItemData* data = (MyTreeItemData*)tree->GetItemData(id);  // id is the treeitem to insert after.  It may be a folder or a BMark

if (data->GetPath().IsEmpty())                                  // Empty path, so id must be a folder
  { if (InsertingFolder && Duplicating)                         // If we're duping a folder, make it a sibling, not a child
      { wxTreeItemId parentfolder = tree->GetItemParent(id);    // Fnd its parent folder
        if (!parentfolder.IsOk()) return;                       // If not OK, we're presumably trying to duplicate root
        folderID = parentfolder;
        data = (MyTreeItemData*)tree->GetItemData(folderID);  
      }
      else folderID = id;                                       // Otherwise we presumably want to insert as the 1st item of the folder.  So use it as parent-folder

    struct Submenu* menu;                                       // Now try to locate the corresponding submenu
    if (MenuStruct.ID == data->GetID())                         // We have to check topmost menu here, FindSubmenu doesn't
      { menu = &MenuStruct; *SubMenu = &MenuStruct; }           // If we're inserting onto "Bookmarks", fill the structs accordingly
      else 
      { menu = FindSubmenu(MenuStruct, data->GetID(), index);   // If it isn't topmost menu, use FindSubmenu    NB returns parent
        if (menu != NULL)  *SubMenu = menu->children[index];    // If not null, use index to get location within its parent array
      }
    if (menu != NULL)                                           // If we're still on target
      { wxTreeItemIdValue cookie; wxTreeItemId child = tree->GetFirstChild(folderID, cookie);  // Check to see if the folder already has children
        if (child.IsOk())  { loc = 0; pos = 0; treepos = 0; }   // If so, use loc = treepos = 0 to insert at beginning
      }                                                         // Otherwise loc & treepos remain -1 to append
  }
 else                                                
  { wxTreeItemId parentfolder = tree->GetItemParent(id);        // If id isn't a folder, find its parent folder
    if (parentfolder.IsOk())
      { folderID = parentfolder;                                // If parentfolder is valid, use this as folder into which to insert
        struct Submenu* menu = FindBookmark(MenuStruct, data->GetID(), index);  // Now try to locate the submenu id's located in
        if (menu != NULL)                                       // If not null, use it
          { *SubMenu = menu;
            if (InsertingFolder)
              { // We now need to find the location within the SubMenu.children struct after which to insert
                      // We can infer it by subtracting index, which ignores folders, from pos, the bmark+folder counter
                pos = tree->FindItemIndex(id);                  // Get the pos within tree for the insertion
                loc = pos - index;
                if (loc == (int)menu->children.GetCount())  loc = -1;  // If loc == the final entry, we append instead, flagged by -1 (the default)
              }
            else                              // Index gives us the location within menu after which to insert
              { if ((index + 1) < menu->BMstruct.GetCount())    // If index is the final entry, we append instead, flagged by -1 (the default)
                    loc = index + 1;                            // We use index+1, as we'll be InsertingBefore
                pos = tree->FindItemIndex(id);                  // Finally, get the pos within tree for the insertion,  to use on displayorder
              }

            if ((pos != -1) && ((pos+1) < (int)tree->GetChildrenCount(folderID, false)))  // Similar reasoning as above.   -1 means it failed
                  treepos = pos + 1;                            // Position on tree, which needs to be +1 as it's InsertingBefore too.
          }
      }
  }
}


void Bookmarks::OnEditBookmark()
{
wxString newlabel; size_t index;
struct Submenu* SubMenu = &MenuStruct;

wxTreeItemId id = tree->GetSelection();
if (!id.IsOk())   return;                                     // Check there IS a selection to edit

MyTreeItemData* data = (MyTreeItemData*)tree->GetItemData(id);

if (data->GetLabel().IsSameAs(_("Separator")))   return;      // Not a lot to edit in a separator!

if (data->GetPath().IsEmpty())                                // Empty path, so Selection must be a folder
  { struct Submenu* menu;                                     // So try to locate the corresponding submenu
    if (MenuStruct.ID == data->GetID()) menu = &MenuStruct;   // We have to check topmost menu here, FindSubmenu doesn't
     else 
      { menu = FindSubmenu(MenuStruct, data->GetID(), index); // If it isn't topmost menu, use FindSubmenu   NB returns parent
        if (menu != NULL)  SubMenu = menu->children[index];   // If not null, use index to get location within its parent array
      }
                              // Now we know who we are, get new label from user
    wxTextEntryDialog getname(tree->GetParent(), _("Alter the Folder Label below"));
    do                                                        // Get the name in a loop, in case of duplication
      { getname.SetValue(data->GetLabel());
        if (getname.ShowModal() != wxID_OK)  return;
        
        newlabel = getname.GetValue();                        // Get the desired label from the dlg
        if (newlabel.IsEmpty())  return;
        if (newlabel == data->GetLabel())  return;            // If no change, abort
        
        bool flag = false;
        for (size_t n=0; n < menu->children.GetCount(); n++)  // Ensure it's not a duplicate with the same path
          if (menu->children[n]->Label == newlabel)
            { wxString msg; msg.Printf(_("Sorry, there is already a folder called %s\n                     Try again?"), newlabel.c_str());
              wxMessageDialog dialog(tree->GetParent(), msg, _("Are you sure?"), wxYES_NO);
              if (dialog.ShowModal() != wxID_YES) return;
                else { flag = true; continue; }
            }
        if (flag) continue;
        data->SetLabel(newlabel); SubMenu->Label = newlabel;  // If we're here, no match so all's well
        SubMenu->FullLabel = SubMenu->FullLabel.BeforeLast(wxCONFIG_PATH_SEPARATOR);  // Chop off the end of the old FullLabel
        SubMenu->FullLabel = SubMenu->FullLabel + wxCONFIG_PATH_SEPARATOR + newlabel; // & replace
        tree->SetItemText(id, newlabel);
        m_altered = true; return;
      }
     while (1);  
  }
                                    // If we're here, it must be a bookmark
SubMenu = FindBookmark(MenuStruct, data->GetID(), index);     // Get the parent menu
if (SubMenu==NULL)  return;
  
wxDialog dlg;
wxXmlResource::Get()->LoadDialog(&dlg, tree->GetParent(), wxT("EditBookmarkDlg"));
LoadPreviousSize(&dlg, wxT("EditBookmarkDlg"));

wxTextCtrl* pathname = (wxTextCtrl*)dlg.FindWindow(wxT("EditPath"));  // Load the current path & label
pathname->ChangeValue(data->GetPath());

wxTextCtrl* labelname = (wxTextCtrl*)dlg.FindWindow(wxT("EditLabel"));  
labelname->ChangeValue(data->GetLabel());

int result = dlg.ShowModal();
SaveCurrentSize(&dlg, wxT("EditBookmarkDlg"));
if (result != wxID_OK) return;

if (pathname->GetValue().IsEmpty()) return;                   // Not allowed a blank path
 
if (pathname->GetValue() != data->GetPath())                  // If the path has been altered
  { data->SetPath(pathname->GetValue());                      // Update the data stores
    SubMenu->BMstruct[index]->Path = pathname->GetValue();
    m_altered = true;
  }

newlabel = labelname->GetValue();
if (newlabel != data->GetLabel())                             // If the label has been altered
  { if (newlabel.IsEmpty())
      newlabel = data->GetPath();                             // If for some reason the user has deleted the label, use the path as a label
    data->SetLabel(newlabel);
    SubMenu->BMstruct[index]->Label = newlabel;
    tree->SetItemText(id, newlabel);
    m_altered = true;
  }
}

bool Bookmarks::OnDeleteBookmark(wxTreeItemId id/*=null*/)
{
bool DnD=false; size_t index=0; int pos; struct Submenu* submen;

if (!id.IsOk())  id = tree->GetSelection();                   // If we weren't passed a valid item, use selection
 else DnD = true;                                             // If we were, flag whether it's a DragnDrop situation or from Cut
if (!id.IsOk()) return false;                                 // Either way, check validity

MyTreeItemData* data = (MyTreeItemData*)tree->GetItemData(id);

if (data->GetID() == MenuStruct.ID)                           // Ensure we're not trying to delete the lot
  { wxString msg;
    if (DnD)  msg = _("Sorry, you're not allowed to move the main folder");
     else       msg = _("Sorry, you're not allowed to delete the main folder");
    wxMessageDialog dialog(tree->GetParent(), msg, _("Tsk tsk!"), wxICON_EXCLAMATION);
    dialog.ShowModal(); return false;
    }

if (data->GetPath().IsEmpty())        // Empty path, so it must be a folder
  { if (!DnD)                                                 // Don't ask permission if from DnD or Cut
      { wxString msg; msg.Printf(_("Delete folder %s and all its contents?"), data->GetLabel().c_str());  // Check we really mean it
        wxMessageDialog dialog(tree->GetParent(), msg, _("Are you sure?"), wxYES_NO | wxICON_QUESTION);
        int ans = dialog.ShowModal(); if (ans != wxID_YES) return false;
      }
    submen = FindSubmenu(MenuStruct, data->GetID(), index);     // Find the parent folder.  Index is passed by reference, & will be loaded with position within it
    if (submen==NULL)   return false;
    
    pos = tree->FindItemIndex(id); if (pos == -1) return false; // Find the index of the item within folder. It'll be identical to that within displayorder.  -1 flags failure
    Submenu* item = submen->children.Item(index); delete item; 
    submen->children.RemoveAt(index);                           // Delete the folder & contents

    if (!DnD) BriefLogStatus bls(_("Folder deleted"));
  }
 else                    // Otherwise it must be a bookmark or separator.  They're dealt with the same
   { 
    submen = FindBookmark(MenuStruct, data->GetID(), index);    // Find the parent folder.  Index is passed by reference, & will be loaded with position within it
    if (submen==NULL)   return false;

    pos = tree->FindItemIndex(id); if (pos == -1) return false; // Find index of the item within folder. It'll be identical to that within displayorder.  -1 flags failure
    BookmarkStruct* item = submen->BMstruct.Item(index); delete item; 
    submen->BMstruct.RemoveAt(index);                           // Delete the bookmark

    if (!DnD) BriefLogStatus bls(_("Bookmark deleted"));  
  }
  
submen->displayorder.Remove(pos, 1);                            // Delete item's position
tree->DeleteBookmark(id);                                       // Remove from the tree
m_altered = true;                                               // Set the modified flag
return true;
}


wxString Bookmarks::RetrieveBookmark(unsigned int id)  // Returns the path of a previously-saved bookmark
{
if (id < ID_FIRST_BOOKMARK || id >= bookmarkID) return wxEmptyString;

return FindPathForID(MenuStruct, id);
}

wxString Bookmarks::FindPathForID(Submenu& SubMenu, unsigned int id)  // Recursively searches each menu for a bookmark with the requested id
{
wxString answer;

for (size_t n=0; n < SubMenu.BMstruct.GetCount(); n++)  // For every bookmarkstruct held by this 'menu'
  if (SubMenu.BMstruct[ n ]->ID == id)                  // If this is the one,
    return SubMenu.BMstruct[ n ]->Path;                 //     return the bookmark path

for (size_t n=0; n < SubMenu.children.GetCount(); n++)  // No success, so recurse thru the submenus
  if ((answer = FindPathForID(*SubMenu.children[n], id)) != wxT(""))
    return answer;                                      // A non-empty string means success
    
return wxEmptyString;                                   // Sorry, no one at home
}
