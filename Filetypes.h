/////////////////////////////////////////////////////////////////////////////
// Name:       Filetypes.h
// Purpose:    FileData class. Open/OpenWith.
// Part of:    4Pane
// Author:     David Hart
// Copyright:  (c) 2016 David Hart
// Licence:    GPL v3.  FileData class is wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef FILETYPESH
#define FILETYPESH

#include "wx/dir.h"
#include "wx/toolbar.h"
#include "wx/wx.h"
#include "wx/config.h"
#include "wx/longlong.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include "Externs.h"  
#include "ArchiveStream.h"

class FileData  :  public DataBase // Does lstat, stores the data, has accessor methods etc etc.  Base class is to allow DataBase* also to reference archivestream data
{
public:
FileData(wxString Filename, bool defererence = false);                  // If dereference, use stat instead of lstat
~FileData(){ if (symlinkdestination != NULL) delete symlinkdestination; delete statstruct; }

FileData& operator=(const FileData& fd)
  { result = fd.result; statstruct = fd.statstruct; Filepath = fd.Filepath; BrokenlinkName = fd.BrokenlinkName;
    symlinkdestination = fd.symlinkdestination; Type = fd.Type; IsFake = fd.IsFake;
    return *this;
  }

bool IsValid(){ return result != -1; }
wxString GetFilepath(){ return IsValid() ? Filepath : wxString(wxT("")); }
wxString GetFilename(){ return IsValid() ? Filepath.AfterLast(wxFILE_SEP_PATH) : wxString(wxT("")); }
wxString ReallyGetName(){ return !Filepath.AfterLast(wxFILE_SEP_PATH).empty() ? Filepath.AfterLast(wxFILE_SEP_PATH) : wxString(wxT("????????")); }
wxString GetPath();

bool IsFileExecutable();          // See if the file is executable (by SOMEONE, not necessarily us)
bool CanTHISUserRead();
bool CanTHISUserWrite();
bool CanTHISUserExecute();
bool CanTHISUserWriteExec();
bool CanTHISUserRename();         // See if the file's PARENT DIR has Write/Exec permissions for THIS USER (plus sticky-bit).  NB assumes that the name change doesn't effectively move the file to a different device
bool CanTHISUserRenameThusly(wxString& newname);                // See above.  Will this particular name work?
bool CanTHISUserMove_By_RenamingThusly(wxString& newname);      // Can we "Move" like this, by Renaming
int  CanTHISUserMove();           // Same as CanTHISUserRename(), plus do we have read permission for the file (so that it can be copied without a rename)
bool CanTHISUserPotentiallyCopy();            // See if the file's PARENT DIR has Exec permissions for THIS USER
bool CanTHISUserChmod();                      // See if the file's permissions are changable by US
bool CanTHISUserChown(uid_t newowner);        // See if the file's owner is changeable by US
bool CanTHISUserChangeGroup(gid_t newgroup);  // See if the file's group is changeable by US

uid_t OwnerID(){ return statstruct->st_uid; }                     // Owner ID
gid_t GroupID(){ return statstruct->st_gid; }                     // Group ID
wxString GetOwner();                                              // Returns owner's name as string
wxString GetGroup();                                              // Returns group name as string


time_t AccessTime(){ return statstruct->st_atime; }               // Time last accessed
time_t ModificationTime(){ return statstruct->st_mtime; }         // Time last modified
time_t ChangedTime(){ return statstruct->st_ctime; }              // Time last Admin-changed

static bool ModifyFileTimes(const wxString& fpath, const wxString& ComparisonFpath); // Sets the fpath's atime/mtime to that of ComparisonFpath
bool ModifyFileTimes(const wxString& ComparisonFpath);            // Ditto to set this particular FileData from another filepath
bool ModifyFileTimes(time_t mt);                                  // Ditto to set this particular FileData from a time_t

wxULongLong Size(){ return statstruct->st_size; }                 // Size in bytes
wxString GetParsedSize();                                         // Returns the size, but in bytes, KB or MB according to magnitude. (Uses global function)
blksize_t GetBlocksize(){ return statstruct->st_blksize; }        // Returns filesystem's blocksize
blkcnt_t GetBlockNo(){ return statstruct->st_blocks; }            // Returns no of allocated blocks for the file

ino_t GetInodeNo(){ return statstruct->st_ino; }                  // Returns inode no
dev_t GetDeviceID() { return  statstruct->st_dev; }               // Returns the device ie which disk/partition the file is on, as major*256 + minor format
nlink_t GetHardLinkNo(){ return statstruct->st_nlink; }           // Returns no of hard links

bool IsRegularFile(){ if (! IsValid()) return false; return S_ISREG(statstruct->st_mode); }  // Is Filepath a Regular File?
bool IsDir(){ if (! IsValid()) return false; return S_ISDIR(statstruct->st_mode); }          // Is Filepath a Dir?
bool IsSymlink(){ if (! IsValid()) return false; return S_ISLNK(statstruct->st_mode); }      // Is Filepath a Symlink?
bool IsBrokenSymlink();                                 																		 // Is Filepath a broken Symlink?
bool IsCharDev(){ if (! IsValid()) return false; return S_ISCHR(statstruct->st_mode); }      // Is Filepath a character device?
bool IsBlkDev(){ if (! IsValid()) return false; return S_ISBLK(statstruct->st_mode); }       // Is Filepath a block device?
bool IsSocket(){ if (! IsValid()) return false; return S_ISSOCK(statstruct->st_mode); }      // Is Filepath a Socket?
bool IsFIFO(){ if (! IsValid()) return false; return S_ISFIFO(statstruct->st_mode); }        // Is Filepath a FIFO?

bool IsUserReadable(){ return !!(statstruct->st_mode & S_IRUSR); }      // Permissions
bool IsUserWriteable(){ return !!(statstruct->st_mode & S_IWUSR); }
bool IsUserExecutable(){ return !!(statstruct->st_mode & S_IXUSR); }

bool IsGroupReadable(){ return !!(statstruct->st_mode & S_IRGRP); }
bool IsGroupWriteable(){ return !!(statstruct->st_mode & S_IWGRP); }
bool IsGroupExecutable(){ return !!(statstruct->st_mode & S_IXGRP); }

bool IsOtherReadable(){ return !!(statstruct->st_mode & S_IROTH); }
bool IsOtherWriteable(){ return !!(statstruct->st_mode & S_IWOTH); }
bool IsOtherExecutable(){ return !!(statstruct->st_mode & S_IXOTH); }

bool IsSetuid(){ return !!(statstruct->st_mode & S_ISUID); }
bool IsSetgid(){ return !!(statstruct->st_mode & S_ISGID); }
bool IsSticky(){ return !!(statstruct->st_mode & S_ISVTX); }

wxString PermissionsToText();                                       // Returns a string describing the filetype & permissions eg -rwxr--r--
size_t GetPermissions(){ return statstruct->st_mode & 07777; }      // Returns Permissions in numerical form

struct stat* GetStatstruct(){ return statstruct; }
class FileData* GetSymlinkData(){ return symlinkdestination; }
wxString GetSymlinkDestination(bool AllowRelative=false)
  { if (!GetSymlinkData()) return (BrokenlinkName.IsEmpty() ? GetFilepath() : BrokenlinkName);
    if (GetSymlinkData()->BrokenlinkName.IsEmpty() && ! GetSymlinkData()->GetFilepath().IsEmpty())
        return  (AllowRelative ? GetSymlinkData()->GetFilepath() : MakeAbsolute(GetSymlinkData()->GetFilepath(), GetPath()));
      else return GetSymlinkData()->BrokenlinkName;  // If the symlink's broken, this is where the original target name is stored
   }

wxString GetUltimateDestination();                                  // Returns the file at the end of a series of symlinks (or original filepath if not a link)
static wxString GetUltimateDestination(const wxString& filepath);   // Ditto for the passed filepath
wxString MakeAbsolute(wxString fpath=wxEmptyString, wxString cwd=wxEmptyString);  // Returns the absolute filepath version of the, presumably relative, given string, relative to cwd or the passed parameter
bool IsSymlinktargetASymlink();
bool IsSymlinktargetADir(bool recursing = false);

int DoChmod(mode_t newmode);                                        // If appropriate, change the file's permissions to newmode
int DoChangeOwner(uid_t newowner);                                  // If appropriate, change the file's owner
int DoChangeGroup(gid_t newgroup);                                  // If appropriate, change the file's group

wxString Filepath;
wxString BrokenlinkName;                                            // If the file is a symlink, & it's broken, the original target name is stored here

protected:
void SetType();                                                     // Called in ctor to set DataBase::Type

struct stat* statstruct;
class FileData* symlinkdestination;                                 // Another FileData instance if this one is a symlink
int result;
};
//--------------------------------------------------------------------------

extern bool SafelyCloneSymlink(const wxString& oldFP, const wxString& newFP, FileData& stat);   // Encapsulates cloning a symlink even if broken

//--------------------------------------------------------------------------

struct Fileype_Struct; struct FiletypeGroup; class FiletypeManager; // Forward declarations

WX_DEFINE_ARRAY(struct Filetype_Struct*, ArrayOfFiletypeStructs);   // Define the array of Filetype structs
WX_DEFINE_ARRAY(struct FiletypeGroup*, ArrayOfFiletypeGroupStructs);// Define array of FiletypeGroup structs

struct Filetype_Struct                  // Struct to hold the filetype data within OpenWith dialog
  { wxString AppName;                   // eg "kedit"
    wxString Ext;                       // eg "txt", the extension(s) to associate with an application
    wxString Filepath;                  // eg "/usr/local/bin/gs", the filepath as opposed to just the label
    wxString Command;                   // eg "kedit %s", the command-string to launch the application
    wxString WorkingDir;                // Optional working dir to which to cd before launching the application
  
    Filetype_Struct& operator=(const Filetype_Struct& fs){ AppName = fs.AppName; Ext = fs.Ext; Filepath = fs.Filepath;
                                                      Command = fs.Command; WorkingDir = fs.WorkingDir; return *this; }                                                   
    void DeDupExt();
    void Clear(){ AppName.Clear(); Ext.Clear(); Filepath.Clear(); Command.Clear(); WorkingDir.Clear(); }
  };

struct FiletypeGroup    // Struct to manage the data, including Filetype_Struct, during OpenWith dialog
  {  ArrayOfFiletypeStructs FiletypeArray;  // To hold structs, each with one application's data, belonging to this subgroup
    wxString Category;                      // eg Editors, or Graphics.  Subgroup in config file
    
    ~FiletypeGroup(){ Clear(); }
    void Clear(){  for (int n = (int)FiletypeArray.GetCount();  n > 0; --n )  { Filetype_Struct* item = FiletypeArray.Item(n-1); delete item; FiletypeArray.RemoveAt(n-1); }
                Category.Clear(); 
              }
  };

class MyMimeTreeItemData : public wxTreeItemData
{
public:
MyMimeTreeItemData(wxString Label,  unsigned int id, bool isapp, Filetype_Struct* ftstruct=NULL)  { Name = Label; ID = id; IsApplication = isapp; FT = ftstruct;}
  
//MyMimeTreeItemData& operator=(const MyMimeTreeItemData& td) { ID = td.ID; Name = td.Name; IsApplication = td.IsApplication; return *this; }
void Clear(){ Name.Clear(); ID=0; IsApplication=false;}

struct Filetype_Struct *FT;
wxString Name;
unsigned int ID;  
bool IsApplication;
};

class MyFiletypeDialog    :    public wxDialog
{
public:
MyFiletypeDialog(){}
void Init(FiletypeManager* dad);

wxButton* AddApplicBtn;
wxButton* EditApplicBtn;
wxButton* RemoveApplicBtn;
wxButton* AddFolderBtn;
wxButton* RemoveFolderBtn;
wxButton* OKBtn;
wxTextCtrl* text;
wxCheckBox* isdefault;
wxCheckBox* interminal;

protected:
FiletypeManager* parent;

void TerminalBtnClicked(wxCommandEvent& event);
void OnIsDefaultBtn(wxCommandEvent& event);
void OnButtonPressed(wxCommandEvent& event);
void DoUpdateUIOK(wxUpdateUIEvent &event);       // Enable the OK button when the textctrl has text

DECLARE_EVENT_TABLE()
};

class MyApplicDialog    :    public wxDialog      // This is the AddApplic & EditApplic dialog
{
public:
MyApplicDialog(FiletypeManager* dad)  :  parent(dad)  {}
void Init();
void UpdateInterminalBtn();                       // Called by parent when it alters textctrl, to see if checkbox should be set
void WriteBackExtData(Filetype_Struct* ftype);    // Writes the contents of the Ext textctrl back to ftype

wxButton* AddFolderBtn;
wxButton* browse;
wxTextCtrl* label;
wxTextCtrl* filepath;
wxTextCtrl* ext;
wxTextCtrl* command;
wxTextCtrl* WorkingDir;
wxCheckBox* isdefault;
wxCheckBox* interminal;

struct Filetype_Struct* ftype;                    // FiletypeManager::OnEditApplication() copies the applic data here
protected:
void OnBrowse(wxCommandEvent& event);
void OnNewFolderButton(wxCommandEvent& event);
void OnIsDefaultBtn(wxCommandEvent& event);
FiletypeManager* parent;

DECLARE_EVENT_TABLE()
};


class MyFiletypeTree : public wxTreeCtrl        // Treectrl used in ManageFiletypes dialog.  A much mutilated version of the treectrl sample
{
public:
MyFiletypeTree() {}
MyFiletypeTree(wxWindow *parent, const wxWindowID id, const wxPoint& pos, const wxSize& size, long style)  // NB: isn't called as using XRC
                                                        : wxTreeCtrl(parent, id, pos, size, style) {}
virtual ~MyFiletypeTree(){};

void Init(FiletypeManager* dad);

void LoadTree(ArrayOfFiletypeGroupStructs& FiletypeGrp);
wxString GetSelectedFolder();               // Returns either the selected subfolder, the subfolder containing the selected applic, or "" if no selection
bool IsSelectionApplic();                   // If there is a valid selection, which is an application rather than a folder, returns true

Filetype_Struct* FTSelected;                // Stores the applic data for the currently selected item

protected:
void OnSelection(wxTreeEvent &event);

FiletypeManager* parent;
private:
DECLARE_DYNAMIC_CLASS(MyFiletypeTree)
DECLARE_EVENT_TABLE()
};

struct Applicstruct; struct FiletypeExts;
WX_DEFINE_ARRAY(struct Applicstruct*, ArrayOfApplics);          // Define the array of Applicstruct structs
WX_DEFINE_ARRAY(struct FiletypeExts*, ArrayOfFiletypeExts);     // Define the array of FiletypeExts structs

struct Applicstruct      // Struct to hold applics name & Command, to use for pre-OpenWith menu
  {  wxString Label;                    // The name of the application eg kedit
    wxString Command;                   // eg "kedit %s", the command to launch the applic
    wxString WorkingDir;                // Any working dir into which to cd first
    void Clear(){ Label.Clear(); Command.Clear(); WorkingDir.Clear(); }
  };

struct FiletypeExts      // Struct to match Exts with default applics to use for Open()
  {  wxString Ext;                      // eg "txt"
    ArrayOfApplics Applics;             // The applications to offer to run this sort of ext
    struct Applicstruct Default;        // The applicstruct containing info to launch a file with extension Ext
    
    ~FiletypeExts(){ Clear(); }
    void Clear(){  for (int n = (int)Applics.GetCount();  n > 0; --n )  { Applicstruct* item = Applics.Item(n-1); delete item; Applics.RemoveAt(n-1); }
                Ext.Clear(); Default.Clear(); 
              }
  };
  
class FiletypeManager
{
public:
FiletypeManager() { m_altered=false; force_kdesu=false;  stat=NULL; from_config=false; ExtdataLoaded=false; }
FiletypeManager(wxString& pathname) { Init(pathname); from_config=false; }
~FiletypeManager();
void Init(wxString& pathname);
//~~~~~~~~~~~~~~~ The Which File-Ext bits ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

bool Open(wxArrayString& commands);
bool OpenUsingMimetype(const wxString& filepath);
bool OpenUsingBuiltinMethod(const wxString& filepath, wxArrayString& commands, bool JustOpen);
static void Openfunction(wxString& open, bool usingsu);         // Submethod actually to launch applic+file.  Shared by Open, Open-With, & from context submenu
void SetKdesu(){ force_kdesu=true; }
bool QueryCanOpen(char* buf);                                   // See if we can open the file by double-clicking, ie executable or ext with default command
bool QueryCanOpenArchiveFile(wxString& filepath);               // A cutdown QueryCanOpen() for use within archives
void LoadExtdata();                                             // Load/Save the Ext data
void SaveExtdata(wxConfigBase* conf = NULL);
bool OfferChoiceOfApplics(wxArrayString& CommandArray);         // Loads Array with applics for this filetype, & CommandArray with launch-commands
bool IsApplicDefaultForExt(const struct Filetype_Struct& ftype, const wxString& Ext);  // See if the applic is the default for extension Ext
bool AddDefaultApplication(struct Filetype_Struct& ftype, bool noconfirm=true);  // Make applic default for a particular ext (or ext.s)
void RemoveDefaultApplication(struct Filetype_Struct& ftype);   // Kill the default application for a particular extension (or ext.s)
void RemoveDefaultApplicationGivenExt(bool confirm=false);      //  Given an ext (in filepath), un-associate its default applic, if any
wxArrayString Array;                        // This holds things like choices of ext.s; used by different methods, including OfferChoiceOfApplics()
wxString filepath;                          // The one we're dealing with
protected:

wxString ParseCommand(const wxString& command = wxT(""));       // Goes thru command, looking for %s to replace with filename. command may be in Array[0]
bool DeduceExt();                         // Which bit of a filepath should we call 'ext' for GetLaunchCommandFromExt()
bool GetLaunchCommandFromExt();
void AddApplicationToExtdata(struct Filetype_Struct& ftype);    // Add submenu application for a particular extension, adding ext if needed
void RemoveApplicationFromExtdata(struct Filetype_Struct& ftype); // Remove application described by ftype from the Extdata structarray
int GetDefaultApplicForExt(const wxString& Ext);                // Given an ext, find its default applic, if any. Returns -1 if none
void UpdateDefaultApplics(const struct Filetype_Struct& oldftype, const struct Filetype_Struct& newftype);  // Update default data after Edit

FileData *stat;
bool force_kdesu;
bool ExtdataLoaded;
ArrayOfFiletypeExts Extdata;              // The array of structs containing Ext-orientated data

//~~~~~~~~~~~~~~~ The OpenWith Dialog bits ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
public:
void OpenWith();                          // Do 'Open with' dialog

void LoadFiletypes();                     // Load/Save the filetype dialogue data
void SaveFiletypes(wxConfigBase* conf = NULL);

bool OnNewFolder();
void OnRemoveFolder();
void OnAddApplication();
void OnDeleteApplication();
void OnEditApplication();
void OnNewFolderButton();
void OnBrowse();                          // Called from MyFiletypeDialog when browse button is pressed. Calls Browse();
wxString Browse();                        // This actually does the browsing. Also used from OnAddApplication()
bool GetApplicExtsDefaults(const struct Filetype_Struct& ftype, wxTextCtrl* text);  // Fill textctrl, & see if applic is default for any of its ext.s. Adds those in Bold
bool AddExtToApplicInGroups(const struct Filetype_Struct& ftype);  // Try to locate the applic matching ftype. If so, add ftype.Ext to it

MyFiletypeTree* tree;                     // The treectrl used in OpenWith()
MyFiletypeDialog* mydlg;                  // Ptr used to access dlg from treectrl

bool m_altered;
protected:
void LoadSubgroup(struct FiletypeGroup* fgroup);
void SaveSubgroup(struct FiletypeGroup* fgroup);

wxComboBox* combo;                        // Shared between OnAddApplication() & OnNewFolderButton()
wxConfigBase* config;
ArrayOfFiletypeGroupStructs GroupArray;   // The  dialogue-type data
bool from_config;                         // If we've come from config, only save amendments on final OK.  Otherwise save as we go
};

#endif
    // FILETYPESH 
