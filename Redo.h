/////////////////////////////////////////////////////////////////////////////
// Name:       Redo.h
// Purpose:    Undo-redo and manager
// Part of:    4Pane
// Author:     David Hart
// Copyright:  (c) 2020 David Hart
// Licence:    wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef REDOH
#define REDOH

#include "wx/filename.h"
#include "wx/dir.h"
#include "wx/toolbar.h"

class MyGenericDirCtrl;
class DirGenericDirCtrl;

                    // This is the pop-up for dir-toolbar directory back/forward-button sidebars
class MyPopupMenu : public  wxWindow    // Need to derive from wxWindow for the PopupMenu method
{
public:
MyPopupMenu(DirGenericDirCtrl* dad, wxPoint& startpt, bool backwards, bool right);

protected:
void ShowMenu();                         // Prepares & displays the menu
bool LoadArray();                        // Used by ShowMenu
void GetAnswer(wxCommandEvent& event);   // Gets the selection from the selection event and returns it to calling class

wxPoint location;                        // Where to position the menu
bool previous;                           // Are we going forwards or backwards? previous==true means back
bool isright;                            // Is this the righthand dirview or the left?
DirGenericDirCtrl* parent;
size_t count;                            // These 2 ints get filled with info from parent's array
size_t arrayno;
wxArrayString entries;                   // We put here the data 2b displayed
wxArrayInt idarray;                      //  & its location within PreviousDirs
const unsigned int FIRST_ID;

DECLARE_EVENT_TABLE()
};

                    // This is the pop-up for Undo/Redo-button sidebars
class SidebarPopupMenu      :    public  wxWindow    // Need to derive from wxWindow for the PopupMenu method
{
public:
SidebarPopupMenu(wxWindow* dad, wxPoint& startpt, wxArrayString& names, int& ans);

protected:
void ShowMenu();                         // Prepares & displays the menu
void GetAnswer(wxCommandEvent& event);   // Gets the selection from the selection event and returns it to calling class
wxPoint location;                        // Where to position the menu
int& answer;                             // This is how to return the answer, in a ref passed by caller
wxArrayString unredonames;               // Array containing the strings to display

wxWindow* parent;
size_t count;
size_t arrayno;
wxArrayInt idarray;                      //  This array stores the answer's location within caller's array
const unsigned int FIRST_ID;

DECLARE_EVENT_TABLE()
};


  //This is my take on Undo/Redo.  A manager class which stores classes for each type of undo, all of which are derived from abstract class UnRedo
class MyFrame;
  
class UnRedo      // Very basic Base class
{
public:
UnRedo() : UndoPossible(false), RedoPossible(false), m_UsedThread(false) {}
UnRedo(const wxString& name = wxT("")) : clustername(name), UndoPossible(false), RedoPossible(false), m_UsedThread(false) {}
virtual ~UnRedo(){}

size_t clusterno;                  // Some unredos should only happen as a cluster eg Overwrite means Delete + Move.  Also used when multiple selections.
wxString clustername;              // What to call this cluster on the sidebar pop-up menu eg Move, Paste

bool UndoPossible;
bool RedoPossible;
void ToggleFlags(){ UndoPossible = !UndoPossible; RedoPossible = !RedoPossible; }
bool IsUndoing() const { return UndoPossible; }
bool GetUsedThread() const { return m_UsedThread; }

virtual bool Undo()=0;            // Abstract, as we certainly don't want any objects,
virtual bool Redo()=0;

protected:
bool m_UsedThread;                // Used for unredo move/paste. Was this a simple process or did we need to use a thread?
};


class UnRedoFile    :    public UnRedo      // Base class for files
{
protected:
wxFileName original;              // Holds the path/filename we started with
wxFileName final;                 //  & that we end up with, if applicable
wxString m_OverwrittenFile;       // Holds any filepath overwritten by the original move/paste. Its path will be inside the trashcan
wxArrayInt IDs;                   // Hold the IDs of the corresponding windows, for appropriate refreshing
bool ItsADir;
bool m_NeedsYield;
public:
UnRedoFile() : UnRedo(wxT("")) { m_NeedsYield = false; }
UnRedoFile(const wxString& firstFn, const wxArrayInt& IDlist, const wxString& secondFn = wxT(""), const wxString& name = wxT(""))
                                        : UnRedo(name), IDs(IDlist), m_NeedsYield(false)
  { if (!firstFn.IsEmpty())    original.Assign(firstFn);    // Assuming there IS data, reconvert it to a wxFileName
    if (!secondFn.IsEmpty())  final.Assign(secondFn);
  }
~UnRedoFile(){}

const wxArrayInt GetIDs() const { return IDs; }
wxString GetOverwrittenFile() const { return m_OverwrittenFile; }
void SetOverwrittenFile(const wxString& overwritten) { m_OverwrittenFile = overwritten; }
bool GetNeedsYield() const { return m_NeedsYield; }
void SetNeedsYield(bool yield) { m_NeedsYield = yield; }
};
  
class UnRedoMove    :    public UnRedoFile    // Used by both Move & Delete
{
public:
UnRedoMove(const wxString& first, const wxArrayInt& IDlist, const wxString& second=wxT(""), const wxString& endbit=wxT(""), const wxString& Originalend=wxT(""),
              bool QueryDir=false, bool FromDel=false, ThreadSuperBlock* tsb = NULL) 
                  : UnRedoFile(first, IDlist, second), finalbit(endbit), originalfinalbit(Originalend), m_tsb(tsb), FromDelete(FromDel)
                      { ItsADir = QueryDir; clustername = (FromDelete ? _("Delete") : _("Move")); }
UnRedoMove(const UnRedoMove& urm) : UnRedoFile() { *this = urm; }
~UnRedoMove(){}
UnRedoMove& operator=(const UnRedoMove& urm) 
  { original=urm.original; IDs=urm.IDs; final=urm.final; finalbit=urm.finalbit; originalfinalbit= urm.originalfinalbit; 
    ItsADir=urm.ItsADir; m_tsb=urm.m_tsb; FromDelete=urm.FromDelete; m_NeedsYield=urm.m_NeedsYield; m_UsedThread=urm.m_UsedThread; return *this; 
  }

void SetThreadSuperblock(ThreadSuperBlock* tsb) { m_tsb = tsb; }

wxString finalbit;          // This is the pasted filename, or the terminal segment of the pasted dir  ie it's the name to Redo to
wxString originalfinalbit;  // This is the pre-Move filename/terminal segment, before any possible rename.  ie it's the name to Undo to

bool Undo();
bool Redo();

protected:
ThreadSuperBlock* m_tsb;
bool FromDelete;            // Flags whether the calling method was Delete or not.  It makes a difference as to which paths get refreshed
};
  
class UnRedoPaste    :    public UnRedoFile
{
public:
UnRedoPaste(const wxString& first, const wxArrayInt& IDlist, const wxString& second = wxT(""), const wxString& endbit = wxT(""),
                                    bool QueryDir=false, bool dirs_only = false, ThreadSuperBlock* tsb = NULL)
                        :   UnRedoFile(first, IDlist, second, dirs_only ? _("Paste dirs") : _("Paste")),
                            finalbit(endbit), m_dirs_only(dirs_only), m_tsb(tsb)  { ItsADir = QueryDir; }
UnRedoPaste(const UnRedoPaste& urp) : UnRedoFile() { *this = urp; }
~UnRedoPaste(){}
UnRedoPaste& operator=(const UnRedoPaste& urp) 
  { original=urp.original; IDs=urp.IDs; final=urp.final; finalbit=urp.finalbit; ItsADir=urp.ItsADir; m_dirs_only=urp.m_dirs_only; m_tsb=urp.m_tsb; m_NeedsYield=urp.m_NeedsYield; m_UsedThread=urp.m_UsedThread; return *this; }

void SetThreadSuperblock(ThreadSuperBlock* tsb) { m_tsb = tsb; }

wxString finalbit;          // This is the pasted filename, or the terminal segment of the pasted dir
bool m_dirs_only;

bool Undo();                // Undoing a Paste means deleting the copy
bool Redo();

protected:
ThreadSuperBlock* m_tsb;
};

class UnRedoLink    :    public UnRedoFile
{
public:
UnRedoLink(const wxString& from,  const wxArrayInt& IDlist, const wxString& to, bool linktype, enum changerelative rel = nochange)
                    :   UnRedoFile(wxT(""), IDlist, wxT(""), _("Link")), originfilepath(from), linkfilepath(to), linkage(linktype), relativity(rel)  {}
~UnRedoLink(){}

bool Undo();               // Undoing a Link means deleting the link
bool Redo();

protected:
wxString originfilepath;   // The link target, ready for redo
wxString linkfilepath;     // The link, ready for undo
bool linkage;              // Hard or sym
enum changerelative relativity; // Relative or absolute path
};

class UnRedoChangeLinkTarget    :    public UnRedoFile
{
public:
UnRedoChangeLinkTarget(const wxString& link, const wxArrayInt& IDlist, const wxString& dest, const wxString& target)
                    :  UnRedoFile(wxT(""), IDlist, wxT(""), _("Change Link Target")), linkname(link), newtarget(dest), oldtarget(target) {}

~UnRedoChangeLinkTarget(){}

bool Undo();              // Undoing a ChangeLinkTarget means changing in reverse
bool Redo();

protected:
wxString linkname;        // The name of the link
wxString newtarget;       // The new target
wxString oldtarget;       // The original target
};

class UnRedoChangeAttributes    :    public UnRedoFile      // Change ownership, group or permissions
{
public:
UnRedoChangeAttributes(const wxString& fpath, const wxArrayInt& IDlist, size_t originalattr, size_t newattr, enum AttributeChange which )
    :  UnRedoFile(wxT(""), IDlist, wxT(""), _("Change Attributes")), filepath(fpath), OriginalAttribute(originalattr), NewAttribute(newattr), whichattribute(which) {}

~UnRedoChangeAttributes(){}

bool Undo();              // Undoing an attribute change means changing in reverse
bool Redo();

protected:
wxString filepath;
size_t OriginalAttribute;
size_t NewAttribute;
enum AttributeChange whichattribute;  // Are we changing permissions, owner or group
};

class UnRedoNewDirFile      :    public UnRedoFile    // UnRedo a "New" Dir or File
{
public:
UnRedoNewDirFile(const wxString& newname, const wxString& pth, bool QueryDir, const wxArrayInt& IDlist)
                                    :   UnRedoFile(wxT(""), IDlist),  filename(newname), path(pth)
                      { ItsADir = QueryDir; clustername = (ItsADir ? _("New Directory") : _("New File")); }
~UnRedoNewDirFile(){}

bool Undo();              // Undoing a New means ReallyDeleting it 
bool Redo();

protected:
wxString filename;
wxString path;
};

class UnRedoDup      :    public UnRedoFile    // Duplicates a Dir or File
{
public:
UnRedoDup(const wxString& destpth, const wxString& newpth,const  wxString& origname, const wxString& new_name,  bool QueryDir, wxArrayInt& IDlist)
            :   UnRedoFile(wxT(""), IDlist), originalname(origname), newname(new_name), destpath(destpth), newpath(newpth)
                  { ItsADir = QueryDir; clustername = (ItsADir ? _("Duplicate Directory") : _("Duplicate File")); }
~UnRedoDup(){}

bool Undo();             // Undoing a Dup means ReallyDeleting it 
bool Redo();

protected:
wxString originalname;   // The filename (or NULL)
wxString newname;        // See above
wxString destpath;       // The original dirname
wxString newpath;
};


class UnRedoRen      :    public UnRedoFile    // Renames a Dir or File
{
public:
UnRedoRen(const wxString& destpth, const wxString& newpth, const wxString& origname, const wxString& new_name, bool QueryDir, const wxArrayInt& IDlist)
          :   UnRedoFile(wxT(""), IDlist), destpath(destpth), originalname(origname), newpath(newpth), newname(new_name) 
                      {  ItsADir = QueryDir; clustername = (ItsADir ? _("Rename Directory") : _("Rename File"));
                        while (destpath.Last() == wxFILE_SEP_PATH)  destpath.RemoveLast();  // We don't want a terminal '/' 
                        while (newpath.Last() == wxFILE_SEP_PATH)   newpath.RemoveLast();
                      }
~UnRedoRen(){}

bool Undo();              // Undoing a Rename means ReRenaming it 
bool Redo();

protected:
wxString destpath;        // The path before the filename or last segment of dirname
wxString originalname;    // The filename (or NULL)
wxString newpath;
wxString newname;
};

class UnRedoArchiveRen      :    public UnRedoFile    // Renames within an archive
{
public:
UnRedoArchiveRen(const wxArrayString& orig, const wxArrayString& current, const wxArrayInt& IDlist, MyGenericDirCtrl* actv, bool duplicating, MyGenericDirCtrl* opp = NULL,bool dirs_only = false)
    :   UnRedoFile(wxT(""), IDlist, wxT(""),  duplicating ?_("Duplicate") : _("Rename")), originalfilepaths(orig),
                          newfilepaths(current), active(actv), dup(duplicating), other(opp), m_dirs_only(dirs_only) {}
~UnRedoArchiveRen(){}

bool Undo();              // Undoing a Rename means ReRenaming it 
bool Redo();

protected:
wxArrayString originalfilepaths; // The filepaths before the original rename, or after Redo
wxArrayString newfilepaths;
MyGenericDirCtrl* active;        // Ptr to (what we hope is still) the view with the still-open archive
bool dup;
MyGenericDirCtrl* other;         // Used if we're Moving between the same archive open in different panes. We only want to undo the second of these if it's still visible
bool m_dirs_only;
};

class UnRedoExtract    :    public UnRedoFile
{
public:
UnRedoExtract(const wxArrayString& first, const wxString& second, const wxArrayInt& IDlist)
                        :   UnRedoFile(second, IDlist, wxT(""), _("Extract")), filepaths(first), path(second)  {}
~UnRedoExtract(){}

wxArrayString filepaths;   // These are the Extracted filenames, or the terminal segment of the Extracted dir  ie it's the name to Redo to
wxString path;             // Guess what this is
wxString trashpath;        // Where we dump the unwanted data ;)

bool Undo();               // Undoing an Extract means deleting it
bool Redo();               // Redoing means undeleting it
};

class UnRedoArchivePaste    :    public UnRedoFile
{
public:
UnRedoArchivePaste(const wxArrayString& first, const wxArrayString& second, const wxString& origpath, const wxString& path,
                                              const wxArrayInt& IDlist, MyGenericDirCtrl* pane, const wxString displayname=_("Paste"), bool dirs_only = false)
                        :   UnRedoFile(wxT(""), IDlist, wxT(""), displayname + (dirs_only ? _(" dirs") : wxT(""))), originalfilepaths(first), newfilepaths(second),
                            originpath(origpath), destpath(path), destctrl(pane), m_dirs_only(dirs_only) {}
~UnRedoArchivePaste(){}

wxArrayString originalfilepaths;
wxArrayString newfilepaths;
wxString originpath, destpath;
MyGenericDirCtrl* destctrl;  // Ptr to (what we hope is still) the view with the still-open archive
bool m_dirs_only;

bool Undo();              // Undoing a Paste means Removing
bool Redo();              // Re-doing a Paste means redoing the paste
};

class UnRedoArchiveDelete    :    public UnRedoFile
{
public:
UnRedoArchiveDelete(const wxArrayString& first, const wxArrayString& second, const wxString& origpth, const wxString& destpth, const wxArrayInt& IDlist, MyGenericDirCtrl* pane, const wxString displayname=_("Delete"))
                        :   UnRedoFile(wxT(""), IDlist, wxT(""), displayname), originalfilepaths(first), newfilepaths(second), originpath(origpth), destpath(destpth), originctrl(pane) {}
~UnRedoArchiveDelete(){}

wxArrayString originalfilepaths;
wxArrayString newfilepaths;
wxString originpath, destpath;
MyGenericDirCtrl* originctrl;  // Ptr to (what we hope is still) the view with the still-open archive

bool Undo();              // Undoing an Delete means undeleting it
bool Redo();              // Redoing means removing it
};

class UnRedoCutPasteToFromArchive  :    public UnRedoFile
{
public:
UnRedoCutPasteToFromArchive(const wxArrayString& orig, const wxArrayString& dests, const wxString origin, const wxString destination, const wxArrayInt& IDlist, 
                                                    MyGenericDirCtrl* first, MyGenericDirCtrl* second, enum archivemovetype wich )
    : UnRedoFile(wxT(""), IDlist, wxT(""), _("Move")), originalfilepaths(orig), newfilepaths(dests), originpath(origin), destpath(destination), originctrl(first), destctrl(second), which(wich){}
~UnRedoCutPasteToFromArchive(){}

bool Undo();                     // Undoing a Move means moving backwards; a Paste means deleting the copy
bool Redo();

protected:
wxArrayString originalfilepaths;
wxArrayString newfilepaths;
wxString originpath;
wxString destpath;
MyGenericDirCtrl* originctrl;    // Ptr to (what we hope is still) the view with the still-open archive for Moves out of archive, or the 'real' origin for Moves into
MyGenericDirCtrl* destctrl;      // Ditto but vice versa for destination
enum archivemovetype which;      // Are we coming, going or both?
};

class UnRedoMoveToFromArchive  :    public UnRedoFile    // Moves back into the archive
{
public:
UnRedoMoveToFromArchive(const wxArrayString& orig, const wxArrayString& dests, const wxString origin, const wxString destination, const wxArrayInt& IDlist, 
                                                    MyGenericDirCtrl* first, MyGenericDirCtrl* second, enum archivemovetype wich )
    : UnRedoFile(wxT(""), IDlist, wxT(""), _("Move")), originalfilepaths(orig), newfilepaths(dests), originpath(origin), destpath(destination), originctrl(first), destctrl(second), which(wich){}
~UnRedoMoveToFromArchive(){}

bool Undo();                     // Undoing a Move means moving backwards; a Paste means deleting the copy
bool Redo();

protected:
wxArrayString originalfilepaths;
wxArrayString newfilepaths;
wxString originpath;
wxString destpath;
MyGenericDirCtrl* originctrl;    // Ptr to (what we hope is still) the view with the still-open archive for Moves out of archive, or the 'real' origin for Moves into
MyGenericDirCtrl* destctrl;      // Ditto but vice versa for destination
enum archivemovetype which;      // Are we coming, going or both?
};

WX_DEFINE_ARRAY(UnRedo*, ArrayOfUnRedos);      // Define the array we'll be using


class UnRedoImplementer
{
public:
UnRedoImplementer() : m_UnRedoArray(NULL), m_CountarrayIndex(0), m_Successcount(0), m_NextItem(0), m_IsActive(false) {}

bool DoUnredos(ArrayOfUnRedos& urarr, size_t first, const wxArrayInt& countarr, bool undoing);

void StartItems();
void OnItemCompleted(size_t item);
void OnAllItemsCompleted(bool successfully = true);

bool IsActive() const { return m_IsActive; }

protected:
bool IsCompleted() const;
size_t GetNextItem();

ArrayOfUnRedos* m_UnRedoArray;
wxArrayInt m_CountArray;
size_t m_CountarrayIndex;
size_t m_Successcount;
size_t m_NextItem;
bool m_IsUndoing;
bool m_IsActive;
};

class UnRedoManager
{
public:
UnRedoManager()  { UnRedoArray.Alloc(MAX_NUMBER_OF_UNDOS); }
~UnRedoManager(){ ClearUnRedoArray(); }
static void ClearUnRedoArray();
static MyFrame *frame;
static bool AddEntry(UnRedo*);              // Adds a new undoable action to the array

static bool ClusterIsOpen;                  // Is a cluster currently open?
static bool Supercluster;                   // Similarly, but re superclusters

static bool StartClusterIfNeeded();         // Start a new cluster if none was open. Returns whether it was needed
static void EndCluster();                   // Closes the currently-open cluster
static int StartSuperCluster();             // Start a Supercluster.  Used eg for Cut/Paste combo, where we want undo to do both at once
static void ContinueSuperCluster();         // Continue a Supercluster.  eg 1 or subsequent Pastes following a cut
static void CloseSuperCluster();            // Close a Supercluster.  Called eg by Copy, so that a Cut, then an Copy, then a Paste, won't be aggregated

static void UndoEntry(int clusters = 1);    // Undoes 'clusters' of clusters of stored actions
static void RedoEntry(int clusters = 1);    // Redoes 'clusters' of clusters of stored actions
static void OnUndoSidebar();                // Called by the Undo-button sidebar
static void OnRedoSidebar();                // Called by the Redo-button sidebar
static bool UndoAvailable(){ return (m_count > 0); }  // Used by UpdateUI
static bool RedoAvailable(){ return (m_count < UnRedoArray.GetCount()); }
static UnRedoImplementer& GetImplementer() { return m_implementer; }

protected:
static void StartCluster();                  // Start a new UnRedoManager cluster
static void LoadArray(wxArrayString& namearray, int& index, bool redo);  // used by sidebars, to load a clutch of clusters from the main array into the temp one

static ArrayOfUnRedos UnRedoArray;          // Array of UnRedo pointers
static size_t m_count;                      // Indexes the next spare slot in the Undo array
static size_t cluster;                      // Holds the arbitrary codeno that will be used to identify the next cluster of unredos
static size_t currentcluster;               // Holds the codeno for the currently-open cluster
static size_t currentSupercluster;
static UnRedoImplementer m_implementer;
};


class DirectoryForDeletions        // Sets up and subsequently deletes if appropriate, temp dirs to hold deleted and trashed files/dirs
{
public:
DirectoryForDeletions();
~DirectoryForDeletions();
void EmptyTrash(bool trash);                // Empties trash-can or 'deleted'-can, depending on the bool
bool ReallyDelete(wxFileName *PathName);
static bool GetUptothemomentDirname(wxFileName& trashdir, enum whichcan trash);    // Uses DeletedName or whatever to create unique subdir, using current time
static wxString GetDeletedName(){ return DeletedName; }

protected:
static void CreateCan(enum whichcan);       // Create a trash-can or whatever
static wxString DeletedName;                // Names of the relevant subdirs
static wxString TrashedName;
static wxString TempfileDir;
};

#endif
    // REDOH
