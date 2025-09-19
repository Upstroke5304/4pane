/////////////////////////////////////////////////////////////////////////////
// Name:       Bookmarks.h
// Purpose:    Bookmarks
// Part of:    4Pane
// Author:     David Hart
// Copyright:  (c) 2016 David Hart
// Licence:    GPL v3
/////////////////////////////////////////////////////////////////////////////
#ifndef BOOKMARKSH
#define BOOKMARKSH

#include "MyTreeCtrl.h"

enum { TreeCtrlIcon_Folder, TreeCtrlIcon_BM, TreeCtrlIcon_Sep };  // Used by MyBmTree

struct Submenu; struct BookmarkStruct;                            // Advance declaration for:
WX_DEFINE_ARRAY(struct Submenu*, ArrayOfSubmenus);                // Define the array of structs
WX_DEFINE_ARRAY(struct BookmarkStruct*, ArrayOfBookmarkStructs);  // Define array of BookmarkStructs

struct BookmarkStruct
  { wxString Path;                        // Exactly
    wxString Label;                       // Each bookmark's label  eg /home/david/documents could be called Documents. These are what the menu displays
    unsigned int ID;                      // This bookmark's event.Id
    BookmarkStruct(){ Clear(); }
    BookmarkStruct(const BookmarkStruct& bm){ *this = bm; }
    BookmarkStruct& operator=(const BookmarkStruct& bs){ Path = bs.Path; Label = bs.Label; ID = bs.ID; return *this; }
    void Clear(){ Path.Clear(); Label.Clear(); ID = 0; }
  };
  
struct Submenu
  { wxString pathname;                    // Holds the (sub)menu "path" eg /Bookmarks/b/a.  Used to store in config-file, which loads/stores alphabetically, so won't keep the same order if we use Label
    wxString Label;                       // The name that the outside world will label the menu/folder eg Projects.  See above comment
    wxString FullLabel;                   // The "Path" of the menu/folder eg /Bookmarks/Programming/Projects.  Needed for AddBookmark
    unsigned int ID;                      // Makes it easier to identify a submenu in a BMTree
    wxString displayorder;                // Allows the menu to hold the user's choice of bookmark/subgroup order
    wxMenu* thismenu;                     // The menu that this struct's bookmarks will be displayed in.  This struct's children will append their menus to this
    ArrayOfBookmarkStructs BMstruct;      // Holds the path, label & id of each of this menu's bookmarks
    ArrayOfSubmenus children;             // Child subgroups go here
    Submenu(){ ClearData(); }
    ~Submenu(){ ClearData(); }
    Submenu(const Submenu& sm){ *this = sm; }
    Submenu& operator=(const Submenu& sm){ pathname = sm.pathname; Label = sm.Label; FullLabel = sm.FullLabel; ID = sm.ID; displayorder = sm.displayorder; // Don't copy the menu: it'll leak
                                           for (size_t n=0; n < sm.BMstruct.GetCount(); ++n) BMstruct.Add(new BookmarkStruct(*sm.BMstruct.Item(n)));
                                           for (size_t n=0; n < sm.children.GetCount(); ++n)   children.Add(new Submenu(*sm.children.Item(n)));
                                           return *this;
                                         }

    void ClearData();                     // Deletes everything
  };

class MyTreeItemData : public wxTreeItemData
{
public:
MyTreeItemData(){}
MyTreeItemData(wxString Path,  wxString Label,  unsigned int ID)  { data.Path = Path; data.Label = Label; data.ID = ID; }
MyTreeItemData(const struct BookmarkStruct& param) { data = param; }
  
MyTreeItemData& operator=(const MyTreeItemData& td) { data = td.data; return *this; }
void Clear(){ data.Clear(); }

wxString GetPath() { return data.Path; }
wxString GetLabel() { return data.Label; }
unsigned int GetID() { return data.ID; }

void SetPath(wxString Path) { data.Path = Path; }
void SetLabel(wxString Label) { data.Label = Label; }

protected:
struct BookmarkStruct data;
};

class BMClipboard              // Stores the TreeItemData & structs for pasting
{  
public:
BMClipboard(){}
~BMClipboard(){ Clear(); Menu.ClearData(); }

void Clear()
      { data.Clear(); HasData = false; FolderHasChildren = false; 
        for (int n = (int)BMarray.GetCount();  n > 0; --n )  { BookmarkStruct* item = BMarray.Item(n-1); delete item; BMarray.RemoveAt(n-1); } 
      }
MyTreeItemData data;
struct Submenu Menu;
ArrayOfBookmarkStructs BMarray;
bool IsFolder;
bool HasData;
bool FolderHasChildren;
};

class Bookmarks;

class MyBookmarkDialog    :    public wxDialog
{
public:
MyBookmarkDialog(Bookmarks* dad)   :  parent(dad){}

protected:
Bookmarks* parent;

void OnButtonPressed(wxCommandEvent& event);
void OnCut(wxCommandEvent& event);
void OnPaste(wxCommandEvent& event);

DECLARE_EVENT_TABLE()
};


class MyBmTree : public wxTreeCtrl  // Treectrl used in ManageBookmarks dialog.  A much mutilated version of the treectrl sample
{
public:
MyBmTree() {}

virtual ~MyBmTree(){};

void Init(Bookmarks* dad);
int FindItemIndex(wxTreeItemId itemID);   // Returns where within its folder the item is to be found
void LoadTree(struct Submenu& MenuStruct, bool LoadFoldersOnly = false);
bool AddFolder(struct Submenu& SubMenu, wxTreeItemId parent, bool LoadFoldersOnly=false, int pos = -1);  // Recursively loads tree with folders from SubMenu
void RecreateTree(struct Submenu& MenuStruct);
void DeleteBookmark(wxTreeItemId id);
void OnBeginDrag(wxTreeEvent& event);
void OnEndDrag(wxTreeEvent& event);

protected:
wxTreeItemId m_draggedItem;                 // item being dragged right now
wxTreeItemId itemSrc;
wxTreeItemId itemDst;
wxTreeItemId itemParent;

void OnCopy();
void OnCut();
void OnPaste();
void ShowContextMenu(wxContextMenuEvent& event);
#ifdef __WXX11__
  void OnRightUp(wxMouseEvent& event){ ShowContextMenu((wxContextMenuEvent&)event); }  // EVT_CONTEXT_MENU doesn't seem to work in X11
#endif
void OnMenuEvent(wxCommandEvent& event);
void DoMenuUI(wxUpdateUIEvent& event);
#if !defined(__WXGTK3__)
  void OnEraseBackground(wxEraseEvent& event);
#endif
void CreateAcceleratorTable();

Bookmarks* parent;
private:

DECLARE_DYNAMIC_CLASS(MyBmTree)
DECLARE_EVENT_TABLE()
};

  
class Bookmarks
{
public:
Bookmarks();
~Bookmarks(){ MenuStruct.ClearData(); }

void LoadBookmarks();
void SaveBookmarks();

void AddBookmark(wxString& newpath);
void OnChangeFolderButton();

void ManageBookmarks();
wxString RetrieveBookmark(unsigned int id);

bool OnDeleteBookmark(wxTreeItemId item);
void OnEditBookmark();
void OnNewSeparator();
void OnNewFolder();
void Move(wxTreeItemId itemSrc, wxTreeItemId itemDst);
bool Cut(wxTreeItemId item);
bool Paste(wxTreeItemId item, bool NoDupCheck=false, bool duplicating=false);
bool Copy(wxTreeItemId item);

static void SetMenuIndex(int index) { m_menuindex = index; }

wxDialog* adddlg;                      // These 2 ptrs are used to access their dialogs from other methods
MyBookmarkDialog* mydlg;
BMClipboard bmclip;                    // Stores the TreeItemData & structs for pasting

protected:
wxConfigBase* config;
MyBmTree* tree;                        // The treectrl used in ManageBookmarks
bool m_altered;                        // Do we have anything to save?

static int m_menuindex;                // Passed to wxMenuBar::GetMenu so we can ID the menu without using its label
unsigned int bookmarkID;               // Actually it ID.s folders & separators too
unsigned int GetNextID(){ if (bookmarkID == ID__LAST_BOOKMARK) bookmarkID=ID_FIRST_BOOKMARK;  // Recycle
                          return bookmarkID++; }                                              // Return next vacant bookmarkID

struct Submenu MenuStruct;             // The top-level struct
wxString LastSubmenuAddedto;           // This is where the last add-bookmark occurred, so this is the current default for next time

void SaveDefaultBookmarkDefault();     // Saves a stub of Bookmarks. Without, loading isn't right
void LoadSubgroup(struct Submenu& SubMenuStruct);                       // Does the recursive loading
void SaveSubgroup(struct Submenu& SubMenuStruct);                       // Does the recursive saving
struct Submenu* FindSubmenuStruct(Submenu& SubMenu, wxString& folder);  // This one uses recursion to match a submenu to the passed folder-name
wxString FindPathForID(Submenu& SubMenu, unsigned int id);              // & this to search each menu for a bookmark with the requested id
void AdjustFolderID(struct Submenu& SubMenu);                           // Used for folder within clipboard.  If we don't adjust IDs, they'll be duplication & crashes
struct Submenu* FindBookmark(Submenu& SubMenu, unsigned int id, size_t& index); // Locates bmark id within SubMenu (or a child submenu)
struct Submenu* FindSubmenu(Submenu& SubMenu, unsigned int id, size_t& index);  // Locates child submenu id within SubMenu (or a child of SubMenu)
void WhereToInsert(wxTreeItemId id, wxTreeItemId& folderID, Submenu** SubMenu, int& loc, int& pos, int& treepos, bool InsertingFolder, bool Duplicating=false); // Subfunction
};

#endif
    // BOOKMARKSH
