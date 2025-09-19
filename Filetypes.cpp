/////////////////////////////////////////////////////////////////////////////
// Name:       Filetypes.cpp
// Purpose:    FileData class. Open/OpenWith.
// Part of:    4Pane
// Author:     David Hart
// Copyright:  (c) 2020 David Hart
// Licence:    GPL v3
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/log.h"
#include "wx/app.h"
#include "wx/frame.h" 
#include "wx/scrolwin.h"
#include "wx/menu.h"
#include "wx/dirctrl.h"
#include "wx/dragimag.h"
#include "wx/imaglist.h"
#include "wx/config.h"
#include "wx/xrc/xmlres.h"
#include "wx/tokenzr.h"
#include "wx/dynarray.h"
#include "wx/arrimpl.cpp"
#include "wx/mimetype.h"

#include <pwd.h>                                  // For finding usernames from ids
#include <grp.h>                                  // Ditto groupnames
#include <sys/statvfs.h>                          // For statvfs(), used in FileData::CanTHISUserWrite()
#include <sys/time.h>                             // For lutime()

#if defined __WXGTK__
  #include <gio/gio.h>                            // To get the default Open command
#endif

#include "Externs.h"  
#include "MyGenericDirCtrl.h"
#include "MyDirs.h"
#include "MyFrame.h"
#include "MyFiles.h"
#include "Redo.h"
#include "Configure.h"
#include "Misc.h"

#include "Filetypes.h"

//This file deals with 3 things. First (& least), a FileData class which does stat & similar on a filepath.
//The 2 main things deal with launching  & opening.  There are classes that do the group-based OpenWith dialog & its child tree & dialogs.  
//These are called from a method of the FiletypeManager class.  That also deals with Opening by a default applic on double-clicking, and by 
//previous choices: the applics offered by reference to the file's extension, prior to the full OpenWith dialog.  
//You might feel that too much has ended up in this class, & the methods use a confusing mixture of the group-related structs and 
//the ext-related ones.  I might agree with you.


/* File */
static const char * iconfile_xpm[] = {
/* width height ncolors chars_per_pixel */
"16 16 3 1",
/* colors */
"     s None    c None",
".    c #000000",
"+    c #ffffff",
/* pixels */
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
"                ",
"                "};

//---------------------------------------------------------------------------------------------------------------------------

FileData::FileData(wxString filepath, bool defererence /*=false*/)  :  Filepath(filepath)
{
IsFake = false;
statstruct = new struct stat;
symlinkdestination = NULL;

if (!defererence)
  { result = lstat(Filepath.mb_str(wxConvUTF8), statstruct);      // lstat rather than stat as it doesn't dereference symlinks
    if (!result && IsSymlink())                                   // If we have a valid result & it's a symlink
      symlinkdestination = new FileData(filepath, true);          //  load another FileData with its target
  }
 else
  { errno=0;
    result = stat(Filepath.mb_str(wxConvUTF8), statstruct);       // Unless we WANT to dereference, in which case use stat
    int stat_error = errno;
              // Strangely, stat/lstat don't return any filepath data.  Here we need it, so we have to use readlink
    char buf[500];                                                // We have to specify a char buffer.  500 bytes should be long enough: it gets truncated anyway
    int len = readlink(Filepath.mb_str(wxConvUTF8), buf, 500);    // Readlink inserts into buf the destination filepath of the symlink
    if (len != -1) 
      { Filepath = wxString(buf, wxConvUTF8, len);                // If it worked, len == length of the destination filepath. Put it into Filepath
        if (result==-1 && stat_error==ENOENT)                     // However if stat failed because the target ain't there no longer...
          BrokenlinkName = Filepath;
      }
     else Filepath = wxEmptyString;                               // If readlink failed, there's nothing sensible to be done
  }

SetType();
}

void FileData::SetType()  // Called in ctor to set DataBase::Type
{
if (IsDir())    { Type = DIRTYPE; return; }
if (IsSymlink()){ Type = SYMTYPE; return; }
if (IsCharDev()){ Type = CHRTYPE; return; }
if (IsBlkDev()) { Type = BLKTYPE; return; }
if (IsSocket()) { Type = SOCTYPE; return; }
if (IsFIFO())   { Type = FIFOTYPE; return; }
Type = REGTYPE;  // The least-stupid default
}

wxString FileData::GetPath()
{
if (!IsValid()) return wxEmptyString;

wxString Path = GetFilepath();
if (Path.Right(1) == wxFILE_SEP_PATH)  Path = Path.BeforeLast(wxFILE_SEP_PATH);  // Avoid problems with any terminal '/'

return wxString(Path.BeforeLast(wxFILE_SEP_PATH) + wxFILE_SEP_PATH); // Chop off any filename & add a '/' for luck.  This also means we return '/' if the original filepath was itself '/' !
}

bool FileData::IsFileExecutable()  // See if the file is executable (by SOMEONE, not necessarily us)
{
if (!IsValid()) return false;                                 // If lstat() returned an error, the answer must be 'No'

unsigned int st_mode = ((unsigned int)statstruct->st_mode);   // For readability

return (!!(st_mode & S_IXUSR) || !!(st_mode & S_IXGRP) || !!(st_mode & S_IXOTH)); // If user, group or other has execute permission, return true
}

bool FileData::IsBrokenSymlink()																		 // Is Filepath a broken Symlink?
{ 
FileData* sfd = GetSymlinkData();
if (!sfd || !sfd->IsValid())
  return true;
if (sfd->GetFilepath() == GetFilepath())
  return true; // Self-referential symlink
  
return !sfd->BrokenlinkName.empty();
}

bool FileData::CanTHISUserRead()  // Do WE have permission to read this file
{
if (!IsValid()) return false;
                  // We can't use access() if this is a symlink, as it reports on the target instead.  This is a particular problem if the symlink is broken!
if (IsSymlink()) return true;                                 // All symlinks seem to have global rwx permissions

int result = access(Filepath.mb_str(wxConvUTF8), R_OK);       // Use 'access' to see if we have read permission
return (result==0 ?  true : false);
}

bool FileData::CanTHISUserWrite()  // Do WE have permission to write to this file
{
if (!IsValid()) return false;
                  // We can't use access() if this is a symlink, as it reports on the target instead.  This is a particular problem if the symlink is broken!
if (IsSymlink())                                              // All symlinks seem to have global rwx permissions, so no need to check for these
  { struct statvfs buf;                                       // However we do need to check for a ro filesystem
    int ans = statvfs(GetPath().mb_str(wxConvUTF8), &buf);    // Use the path here, rather than the filepath, as statvfs can't cope with broken symlinks
    return (ans==0 && (buf.f_flag & ST_RDONLY)==0);
  }

int result = access(Filepath.mb_str(wxConvUTF8), W_OK);       // Use 'access' to see if we have write permission
return (result==0 ?  true : false);
}

bool FileData::CanTHISUserExecute()  // Do WE have permission to execute this file
{
if (!IsValid()) return false;
                  // I _am_ using access() here, even if this is a symlink.  Otherwise, as symlinks all have rwx permissions, they will all appear to be executable, even if the target isn't!
int result = access(Filepath.mb_str(wxConvUTF8), X_OK);       // Use 'access' to see if we have execute permission
return (result==0 ?  true : false);
}

bool FileData::CanTHISUserWriteExec()  // Do WE have write & exec permissions for this file
{
if (!IsValid()) return false;
                  // We can't use access() if this is a symlink, as it reports on the target instead.  This is a particular problem if the symlink is broken!
if (IsSymlink()) return CanTHISUserWrite();                   // All symlinks seem to have global rwx permissions, but use CanTHISUserWrite() to check for a ro filesystem

int result = access(Filepath.mb_str(wxConvUTF8), W_OK | X_OK); // Use 'access' to see if we have permissions
return (result==0 ?  true : false);
}

bool FileData::CanTHISUserRename()  // See if the file can be Renamed by US
{
if (!IsValid()) return false;
                      // To Rename a file or dir, we need to have 'write' permission for its parent dir (& execute or we can't stat it!)
FileData dir(GetPath());                                      // Make a FileData for the parent dir. NB GetPath() still returns '/' if the original file was root
if (!dir.IsValid()) return false;                             // If stat() returned an error, the answer must be 'No'

if (!dir.CanTHISUserWriteExec()) return false;                // Return false if parent dir hasn't write & exec permission for us

                      // If we HAVE got these permissions, we still have to check that the Sticky bit isn't set
if (!dir.IsSticky()) return true;                             // If it isn't, we're OK

uid_t id = getuid();                                          // Get user's numerical id
return (id == OwnerID()  ||  id == dir.OwnerID());            // We're still OK if we own either the file or the parent dir.  Otherwise we're not
}

bool FileData::CanTHISUserRenameThusly(wxString& newname)  // See above.  Will this particular name work?
{
if (!IsValid()) return false;
if (newname.IsEmpty()) return false;
if (!CanTHISUserRename()) return false;                       // Check for 'write' permission for its parent dir (& execute or we can't stat it!)
          // To rename a file or dir, we also need to check that the new location will be on the same device/partition as the old.  Otherwise rename won't work, we'd have to copy/delete
FileData newlocation(newname);
if (!newlocation.IsValid()) return false;                     // If stat() returned an error, the answer must be 'No'

return (GetDeviceID() == newlocation.GetDeviceID());          // We're OK if st_dev is unchanged
}

int HowManyChildrenInThisDir(wxString& dirname);

bool FileData::CanTHISUserMove_By_RenamingThusly(wxString& newname)  // Can we "Move" like this, by Renaming
{
if (!IsValid()) return false;
if (newname.IsEmpty()) return false;
if (!CanTHISUserRenameThusly(newname)) return false;          // Check for 'write' permission for its parent dir (& execute or we can't stat it!);  + that From & To are on the same device
if (!IsDir()) return true;                                    // If it's a file, that's all we need to know

if (CanTHISUserWriteExec())    return true;                   // To rename a dir, we also need write/exec permission to "delete" any progeny from this filepath when we rename it to another
return (HowManyChildrenInThisDir(newname) == 0);              // The dir can't be emptied, unless it's obliging enough to be empty    
}

int FileData::CanTHISUserMove()  // See if the file can be moved by US.  NB this means Move, rather than just renaming the file
{
// Return '0' for no because of the parent dir, '-1' because of read permissions. (This allows for a more-intelligent explanation in the subsequent dialog)
if (!CanTHISUserRename()) return false;                       // To move a file, we first need to have 'write' permission for its parent dir (& execute or we can't stat it!).  This is the same as Rename(), so use this
 
return CanTHISUserRead() ? 1:-1;                              // We also need to have 'read' permission for the file, otherwise we can't copy it
}

bool FileData::CanTHISUserPotentiallyCopy()  // See if files in the parent dir can in theory be Copied by US NB to copy the individual files in the dir will also require Read access to them
{
if (!IsValid()) return false;
                      // To copy a file or dir, we need to have 'execute' permission for its parent dir 
                      // (BTW We also need 'read' to be able to list the dir in the first place, and 'read' permission for each file we want to copy)
FileData dir(GetPath());                                      // Make a FileData for the parent dir. NB GetPath() still returns '/' if the original file was root
if (!dir.IsValid()) return false;                             // If stat() returned an error, the answer must be 'No'

return dir.CanTHISUserExecute();                              // Return true if parent dir has exec permission for user
}

bool FileData::CanTHISUserChmod()  // See if the file's permissions are changeable by US
{
if (!IsValid()) return false;
                      // To change a file or dir's permissions, we need to own it or be root
uid_t id = getuid();                                          // Get user's numerical id
return (id == OwnerID()  ||  id == 0);                        // We're OK if we own either the file or are root.  Otherwise we're not
}

bool FileData::CanTHISUserChown(uid_t newowner)  // See if the file's owner is changeable by US
{
unsigned int invalid = (unsigned int)-1;                      // This is to prevent compiler warnings about -1 in unsigned ints
if (!IsValid()) return false;
if (newowner == invalid || newowner == OwnerID()              // because there's nothing to do
                    || getuid() != 0)  return false;          // Only root can change ownership

return true;
}

bool FileData::CanTHISUserChangeGroup(gid_t newgroup)  // See if the file's group is changeable by US
{
unsigned int invalid = (unsigned int)-1;                      // This is to prevent compiler warnings about -1 in unsigned ints
if (!IsValid()) return false;
if (newgroup == invalid || newgroup == GroupID())    return false;  // because there's nothing to do
if (getuid()!=0 && getuid()!=OwnerID())  return false;        // Only the file-owner or root can change a group

return true;
}

int FileData::DoChmod(mode_t newmode)  // If appropriate, change the file's permissions to newmode
{
if (!IsValid()) return false;
if (!CanTHISUserChmod()) return false;                        // Check we have the requisite authority (we need to own it or be root)

if (newmode == (statstruct->st_mode & 07777)) return 2;       // Check the new mode isn't identical to the current one! If so, return a flag

return (chmod(Filepath.mb_str(wxConvUTF8), newmode) == 0);    // All's well, so do the chmod. Success is flagged by zero return
}

int FileData::DoChangeOwner(uid_t newowner)  // If appropriate, change the file's owner
{
if (newowner == OwnerID())  return 2;                         // Check the new owner isn't identical to the current one! If so, return a flag
if (!CanTHISUserChown(newowner)) return false;                // Check it can be done

unsigned int invalid = (unsigned int)-1;                      // This is to prevent compiler warnings about -1 in unsigned ints
return (lchown(Filepath.mb_str(wxConvUTF8), newowner, invalid) == 0);  // Try to do the chown (actually lchown as this works on links too).  Zero return means success
}

int FileData::DoChangeGroup(gid_t newgroup)  // If appropriate, change the file's group
{
if (newgroup == GroupID())  return 2;                         // Check the new group isn't identical to the current one! If so, return a flag
if (!CanTHISUserChangeGroup(newgroup)) return false;          // First check it can be done

unsigned int invalid = (unsigned int)-1;                      // This is to prevent compiler warnings about -1 in unsigned ints
return (lchown(Filepath.mb_str(wxConvUTF8), invalid, newgroup) == 0);  // Try to do the chown (actually lchown as this works on links too).  Zero return means success
}

wxString FileData::GetParsedSize()  // Returns the size, but in bytes, KB, MB or GB according to magnitude
{
return ParseSize(Size(), false);
}

wxString FileData::PermissionsToText()  // Returns a string describing the filetype & permissions eg -rwxr--r--, just as does ls -l
{
wxString text;
text.Printf(wxT("%c%c%c%c%c%c%c%c%c%c"), IsRegularFile() ? wxT('-') : wxT('d'),    // To start with, assume all non-regular files are dirs
  IsUserReadable() ? wxT('r') : wxT('-'),  IsUserWriteable()  ? wxT('w') : wxT('-'), IsUserExecutable()  ? wxT('x') : wxT('-'),
  IsGroupReadable() ? wxT('r') : wxT('-'), IsGroupWriteable() ? wxT('w') : wxT('-'), IsGroupExecutable() ? wxT('x') : wxT('-'),
  IsOtherReadable() ? wxT('r') : wxT('-'), IsOtherWriteable() ? wxT('w') : wxT('-'), IsOtherExecutable() ? wxT('x') : wxT('-')
   );

if (text.GetChar(0) != wxT('-'))                              // If it wasn't a file, 
  { if (IsDir()) ;                                            //   check it really IS a dir (& if so, do nothing).  If it's NOT a dir, it'll be one of the miscellany
    else if (IsSymlink()) text.SetChar(0, wxT('l'));    
    else if (IsCharDev()) text.SetChar(0, wxT('c'));    
    else if (IsBlkDev()) text.SetChar(0, wxT('b'));    
    else if (IsSocket()) text.SetChar(0, wxT('s'));    
    else if (IsFIFO()) text.SetChar(0, wxT('p'));
  }
                                    // Now amend string if the unusual permissions are set.
if (IsSetuid()) text.GetChar(3) == wxT('x') ?  text.SetChar(3, wxT('s')) : text.SetChar(3, wxT('S'));  //  If originally 'x', use lower case, otherwise upper
if (IsSetgid()) text.GetChar(6) == wxT('x') ?  text.SetChar(6, wxT('s')) : text.SetChar(6, wxT('S'));
if (IsSticky()) text.GetChar(9) == wxT('x') ?  text.SetChar(9, wxT('t')) : text.SetChar(9, wxT('T'));
      
return text;
}

wxString DataBase::TypeString()  // Returns string version of type eg "Directory" NB a baseclass DataBase method, not FileData
{
wxString type;

switch(Type)
  { case HDLNKTYPE:
    case REGTYPE:      type = _("Regular File"); break;
    case SYMTYPE:      if (IsFake || (GetSymlinkData() && GetSymlinkData()->IsValid()))    type = _("Symbolic Link");  // A FakeFiledata symlink can't cope with target/broken
                        else   type = _("Broken Symbolic Link");
                       break;
    case CHRTYPE:      type = _("Character Device"); break;
    case BLKTYPE:      type = _("Block Device"); break;
    case DIRTYPE:      type = _("Directory"); break;
    case FIFOTYPE:     type = _("FIFO"); break;
    case SOCTYPE:      type = _("Socket"); break;
      default:         type = _("Unknown Type?!");
  }
 
return type;
}

bool DataBase::IsValid()
{
if (IsFake)
  return (FakeFiledata*)IsValid();
 else
  return (FileData*)IsValid();
}

wxString FileData::GetOwner()  // Returns owner's name as string
{
struct passwd p, *result;
static const size_t bufsize = 4096; char buf[bufsize];

getpwuid_r(OwnerID(), &p, buf, bufsize, &result);    // Use getpwuid_r to fill password struct
if (result == NULL) return wxT("");
wxString name(result->pw_name, wxConvUTF8); 
return name;
}

wxString FileData::GetGroup()  // Returns group name as string
{
struct group g, *result;
static const size_t bufsize = 4096; char buf[bufsize];
getgrgid_r(GroupID(), &g, buf, bufsize, &result);     // Use getgrgid_r to fill group struct
if (result == NULL) return wxT("");
wxString name(result->gr_name, wxConvUTF8);
return name;
}

bool FileData::IsSymlinktargetASymlink()
{
if (!IsValid() || !GetSymlinkData()->IsValid() || GetSymlinkData()->GetFilepath().empty()) return false;

FileData next(GetSymlinkDestination());
return (next.IsValid() && next.IsSymlink());
}

bool FileData::IsSymlinktargetADir(bool recursing /*=false*/) // (Recursively) search to see if the ultimate target's a dir
{
if (!IsValid())   return false;
if (recursing && IsDir())   return true;                      // Success, providing this isn't the 1st iteration & the file's a *real* dir

if (!IsSymlink() || GetSymlinkData()->GetFilepath().empty()) return false;  // If it's not a symlink (or it's a broken one), then the target wasn't a dir

FileData recurse(GetSymlinkDestination());
return recurse.IsSymlinktargetADir(true);                     // Return recursion result
}

wxString FileData::GetUltimateDestination()  // Returns the filepath at the end of a series of symlinks (or the original filepath if not a symlink)
{
if (!IsValid()) return wxT("");

return GetUltimateDestination(GetFilepath());
}

#ifndef PATH_MAX
  #define PATH_MAX 10000 // gnu/hurd doesn't have a max path length, so it doesn't pre-define this
#endif

//static 
wxString FileData::GetUltimateDestination(const wxString& filepath)  // Ditto, but a static that tests the passed filepath
{
if (filepath.empty()) return filepath;

char buf[PATH_MAX];
if (realpath(filepath.mb_str(wxConvUTF8), buf) == NULL) return filepath;  // This function absolutes a relative path, and undoes any symlinks within the path eg /foo/subdir where foo is a symlink for /bar

return wxString(buf, wxConvUTF8);
}

wxString FileData::MakeAbsolute(wxString fpath/*=wxEmptyString*/, wxString cwd/*=wxEmptyString*/)  // Returns the absolute filepath version of the, presumably relative, given string, relative to cwd or the passed parameter
{
if (fpath.IsEmpty())                                          // If fpath is empty & the FileData is valid, assume it's the original filepath we're to absolute
  { if (!IsValid()) return fpath;
     else { fpath = GetFilepath(); if (fpath.IsEmpty())  return fpath; }  // If fpath is still empty, abort
  }

wxFileName fn(fpath);                                         // Now use wxFileName's Normalize method to do the hard bit
fn.Normalize(wxPATH_NORM_ENV_VARS | wxPATH_NORM_DOTS | wxPATH_NORM_TILDE | wxPATH_NORM_ABSOLUTE, cwd);
return fn.GetFullPath();
}

//static
bool FileData::ModifyFileTimes(const wxString& fpath, const wxString& ComparisonFpath)  // Sets the fpath's atime/mtime to that of ComparisonFpath
{
if (fpath.empty() || ComparisonFpath.empty()) return false;
FileData fd(fpath);
if (fd.IsValid())
  return fd.ModifyFileTimes(ComparisonFpath);
return false;
}

bool FileData::ModifyFileTimes(const wxString& ComparisonFpath)  // Sets the FileData's atime/mtime to that of ComparisonFpath
{
if (ComparisonFpath.empty() || !IsValid()) return false;
FileData fd(ComparisonFpath);
if (!fd.IsValid()) return false;

struct timeval tvp[2];
tvp[0].tv_sec = fd.AccessTime(); tvp[0].tv_usec = 0;
tvp[1].tv_sec = fd.ModificationTime(); tvp[1].tv_usec = 0;
 // Use lutimes instead of utimes as, like lstat, it doesn't 'look through' symlinks. For non-symlinks there's no difference afaict
return (lutimes(GetFilepath().ToUTF8(), tvp) == 0);
}

bool FileData::ModifyFileTimes(time_t mt)  // Sets the FileData's mtime to mt
{
if (!IsValid()) return false;

struct timeval tvp[2];
tvp[0].tv_sec = 0; tvp[0].tv_usec = 0; // For our usecase, atime is 'now' anyway
tvp[1].tv_sec = mt; tvp[1].tv_usec = 0;

return (lutimes(GetFilepath().ToUTF8(), tvp) == 0);
}
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------

bool SafelyCloneSymlink(const wxString& oldFP, const wxString& newFP, FileData& stat)  //  Clones a symlink even if badly broken
{
wxCHECK_MSG(stat.IsSymlink(), false, wxT("Passed something other than a symlink"));

if (stat.IsBrokenSymlink() && stat.GetSymlinkDestination().empty())
  { wxString sdfilepath( stat.GetSymlinkData()->Filepath ); // Don't use GetFilepath() here, as !IsValid() so will return ""
    if (!sdfilepath.empty())
      CreateSymlink(sdfilepath, newFP);
     else
      return wxCopyFile(oldFP, newFP, false); // ...for want of something better
  }
 else 
  { enum changerelative symlinksetting;
    if (RETAIN_REL_TARGET && oldFP.GetChar(0) != wxFILE_SEP_PATH)
      symlinksetting = makerelative; // This is a relative symlink, & we want to keep it pointing to the original dest
     else 
      symlinksetting = nochange;     // It's absolute and pointing correctly to the original dest, or relative & we DON'T want to keep it pointing to the original dest

    CreateSymlinkWithParameter(oldFP, newFP, symlinksetting);
  }
 

FileData fd(newFP);
return fd.IsSymlink();
}


wxString QuoteSpaces(wxString& str)  // Intelligently cover with "". Do each filepath separately, in case we're launching a script with a parameter which takes parameters . . .
{
if (str.IsEmpty()) return str;

wxString word, result;
wxStringTokenizer tkz(str);
while (tkz.HasMoreTokens())
  { wxString token = tkz.GetNextToken();
    word += token; 
    FileData fd(word);
    if (fd.IsValid())                                         // If the token is a valid filepath, followed by whitespace, add it to the result
      { result += wxT('\"') + word; result += wxT('\"');      // Add string-to-date, surrounded by ""
        word.Empty();
      }
      
    if (tkz.HasMoreTokens())  // If there's more to come, add the intervening white space. This might be the space-between-filepaths from above, or a space within a filepath
      { size_t spacestart = str.find(token);
        if (spacestart != wxString::npos)
          { wxString residue = str.Mid(spacestart + token.Len());
            for (size_t n=0; n < residue.Len(); ++n)
              if (wxIsspace(residue.GetChar(n))) word += residue.GetChar(n);
                else break;
          }
      }
  }

if (!word.IsEmpty()) { result += wxT('\"') + word; result += wxT('\"'); }  // If there's a residue that isn't a filepath, quote it too
return result;
}


//---------------------------------------------------------------------------------------------------------------------------------------------------------------------

#include <stdio.h>
#include <dirent.h>

        // The following 2 helper functions are taken/adapted from "Simple Program to List a Directory, Mark II" in the GNU C lib manual

static int one(const struct dirent* unused)  // Just returns true
{
  return 1;
}

int HowManyChildrenInThisDir(wxString& dirname)  // Returns the no of dirs/files in the passed dir.  Used in CanFoldertreeBeEmptied() below
{
struct dirent **eps;
int n;

n = scandir(dirname.mb_str(wxConvUTF8), &eps, one, alphasort);
if (n >= 0)  return n - 2;                                    // Return n-2, as .. and . are counted too so empty is 2
  else return -1;
}

enum emptyable CanFoldertreeBeEmptied(wxString path)  // Global to check that this folder & any daughters can be deleted from
{
bool PathWasOriginallyAFile = false;                          // Flags whether we were originally passed a file, not a dir
while (path.Right(1) == wxFILE_SEP_PATH && path.Len() > 1)   path.RemoveLast();

FileData* stat = new FileData(path);  // We have to use FileData throughout, not eg wxDirExists which (currently) falsely accepts symlinks-to-dirs

if (!stat->IsDir())
  { if (!stat->IsValid()) { delete stat; return RubbishPassed; }  // Flagging rubbish    
    if (stat->IsSymlink() && stat->IsSymlinktargetADir(true))     // We want to assess a symlink-to-dir as the target dir
      { path = stat->GetUltimateDestination();
        delete stat; stat = new FileData(path);
      }
     else                                                     // If it's a valid filedata, but not a dir or a symlink-to-dir, it must be a 'file'. Use the parent dir
      { path = path.BeforeLast(wxFILE_SEP_PATH);              // This should amputate any filename
        delete stat; stat = new FileData(path);               // Now try again
        if ( !(stat->IsDir() || (stat->IsSymlink() && stat->IsSymlinktargetADir(true))) )
          { delete stat; return RubbishPassed; }              // Must have been disguised rubbish
        PathWasOriginallyAFile = true;                        // Remember we did this
      }
  }

wxDir dir(path); if (!dir.IsOpened()) { delete stat; return RubbishPassed; }  // Heavily-disguised rubbish!

if (!stat->CanTHISUserWriteExec())
  { delete stat;                                              // The passed folder fails, unless it's obliging enough to be empty
    return (HowManyChildrenInThisDir(path) == 0) ? CanBeEmptied : CannotBeEmptied;
  }

delete stat;                // If we're here, the passed folder was OK.  Recursively check any subfolders (unless we were actually passed a file...)
if (PathWasOriginallyAFile) return CanBeEmptied;

wxString subdirname; bool cont = dir.GetFirst(&subdirname, wxEmptyString, wxDIR_DIRS);
while (cont)
  { wxString filepath = path + wxFILE_SEP_PATH + subdirname;  // Make 'subdir' into a filepath
    FileData stat(filepath);
    if (stat.IsDir())                                         // Protect against symlinks-to-dirs
      { enum emptyable ans = CanFoldertreeBeEmptied(filepath);
        if (ans != CanBeEmptied) 
          return (ans==CannotBeEmptied ? SubdirCannotBeEmptied : ans); // If failure, return the fail code, but translate Cannot into SubdirCannot
      }
    cont = dir.GetNext(&subdirname);
  }

return CanBeEmptied;
}

bool HasThisDirSubdirs(wxString& dirname)  // Does the passed dir have child dirs?  Used in DirGenericDirCtrl::ReloadTree()
{
DIR* dp;
struct dirent* ep;

dp = opendir (dirname.mb_str(wxConvUTF8));
if (dp != NULL)
  { while  ((ep = readdir (dp)) != NULL)
      { wxString fname(ep->d_name, wxConvUTF8);
        if (fname == wxT(".") || fname == wxT("..")) continue;                  // Check we're not trying to count . or ..
        FileData fd(dirname + wxFILE_SEP_PATH + fname);                         // Make a filedata with the filepath in question
        if (fd.IsValid() && fd.IsDir()) { (void) closedir (dp); return true; }  // If it's a dir, we know there's a subdir
      }
    (void) closedir (dp);
  }

return false;
}

bool ReallyIsDir(const wxString& path, const wxString& item)  // Is the passed, presumably corrupt, item likely to be a dir?
{
// This is used to (hopefully) distinguish between a corrupt, or otherwise non-'stat'able, file and dir
// It works correctly at least for .gvfs/ (see http://forums.opensuse.org/showthread.php/387162-permission-denied-on-gvfs)
struct dirent *next;
DIR* dirp = opendir(path.mb_str(wxConvUTF8));
if (!dirp)
  { wxLogDebug(wxT("The passed parent dir, %s, was (also?) invalid"), path.c_str());
    return false;
  }

while(1)
  { errno = 0;
    next = readdir(dirp);
    if (!next)
      { wxString msg;
        switch (errno)
          { case EACCES:    msg = wxT("of a permissions issue"); break;
            case ENOENT:    msg = wxT("of an invalid name"); break;
            case 0:         msg = wxT("it simply wasn't there"); break;
            default:        msg = wxString::Format(wxT("error %i"), errno);
          }
        wxLogDebug(wxT("Couldn't find or access item %s inside %s because "), item.c_str(), path.c_str(), msg.c_str());
        closedir(dirp); return false;
      }

    if (wxString(next->d_name, wxConvUTF8) != item) continue;

#if defined(_DIRENT_HAVE_D_TYPE)
  bool ans = (next->d_type == DT_DIR);
  closedir(dirp); return ans;
#else
  wxLogDebug(wxT("_DIRENT_HAVE_D_TYPE not available"));
#endif
  }

closedir(dirp); return false;
}


wxULongLong globalcumsize;                // Threads? What are threads...? :p

#include <ftw.h>
int AddToCumSize(const char *filename, const struct stat *statptr, int flags, struct FTW *unused)
{
if ((flags & FTW_F) == FTW_F)                                 // If this is a regular file or similar, add its size to the total
  globalcumsize += statptr->st_size;
return 0;
}

wxULongLong GetDirSizeRecursively(wxString& dirname)  // Get the recursive size of the dir's contents  Used by FileGenericDirCtrl::UpdateStatusbarInfo
{
globalcumsize = 0;
int result = nftw(dirname.mb_str(wxConvUTF8), AddToCumSize, 5,  FTW_PHYS | FTW_MOUNT);
if (result==0) return globalcumsize;
 else return 0;
}

wxULongLong GetDirSize(wxString& dirname)  // Get the, non-recursive, size of the dir's contents (or the file's size if a file is passed)  Used by FileGenericDirCtrl::UpdateStatusbarInfo
{
DIR* dp;
struct dirent* ep;
wxULongLong cumsize = 0;

FileData filepath(dirname);
if (!filepath.IsValid()) return cumsize;  // which will be 0
if (!filepath.IsDir() && !filepath.IsSymlinktargetADir(true)) return (cumsize = filepath.Size());  // If it's not a dir, return the file's size

dp = opendir (dirname.mb_str(wxConvUTF8));
if (dp != NULL)
  { while  ((ep = readdir (dp)) != NULL)
      { wxString fname(ep->d_name, wxConvUTF8);
        if (fname == wxT(".") || fname == wxT("..")) continue;// Check we're not trying to count . or ..
        FileData fd(dirname + wxFILE_SEP_PATH + fname);       // Make a filedata with the filepath in question
        if (fd.IsValid())   cumsize += fd.Size();             //  and add its size to cumsize
      }
    (void) closedir (dp);
  }

return cumsize;
}

int RecursivelyChangeAttributes(wxString filepath, wxArrayInt& IDs, size_t newdata, size_t& successes, size_t& failures, enum AttributeChange whichattribute, int flag)  // Global recursively to chmod etc a dir +/- any progeny
{          // If flag==0, just do this filepath. If ==1, recurse.  If ==-1, we've come from UnRedo, so just do this filepath & don't set up another UnRedo
const int PartialSuccess = 2; size_t oldattr = 0;

FileData* stat = new FileData(filepath); if (!stat->IsValid()) { delete stat; return false; }

if (flag==1 && stat->IsDir())                                 // If we're supposed to, & this is a dir, first recurse thru all its files & subdirs
  { struct dirent **eps;
    int count = scandir(filepath.mb_str(wxConvUTF8), &eps, one, alphasort);
    if (count != -1)                                          // -1 means error
      for (int n=0; n < count; ++n)
        { wxString name(eps[n]->d_name, wxConvUTF8);
          if (name==wxT(".") || name==wxT("..")) continue;    // Ignore dots
          RecursivelyChangeAttributes(filepath + wxFILE_SEP_PATH + name, IDs, newdata, successes, failures, whichattribute, flag);
        }
  }

int ans = 0;                                                  // Now do the passed filepath itself, whatever type it is
switch (whichattribute)
  { case ChangeOwner:      oldattr = stat->OwnerID(); ans = stat->DoChangeOwner((uid_t)newdata); break;
    case ChangeGroup:      oldattr = stat->GroupID(); ans = stat->DoChangeGroup((gid_t)newdata); break;
    case ChangePermissions:  oldattr = stat->GetPermissions(); ans = stat->DoChmod((mode_t)newdata); break;  
  }

delete stat;

if (ans==1)                                                   // If nothing changed because newdata==old, ans will be 2
  { ++successes;                                              // If it worked, inc the int that holds successes;
    if (flag != -1)                                           // (flag==-1 signifies that this is an UnRedo already
      { UnRedoChangeAttributes* UnRedoptr = new UnRedoChangeAttributes(filepath, IDs, oldattr, newdata, whichattribute);
        UnRedoManager::AddEntry(UnRedoptr);                   // Store UnRedo data
      }
  }
 else if (!ans) ++failures;    // If false, inc the int that holds failures.  NB If nothing changed because newdata==old, ans will be 2

            //  If this is the original iteration, we need a return code to signify whether we've had total, partial or no success.  If we're currently recursing, it will be ignored
if (successes)  { if (!failures) return true; else return PartialSuccess; }
if (flag == -1)  return ans==PartialSuccess;  // This is a workaround for unredoing changes of group: as chmod happens at the same time, and if unchanged will return 2...
return false;
}

bool RecursivelyGetFilepaths(wxString path, wxArrayString& array)  // Global recursively to fill array with all descendants of this dir
{
FileData stat(path);
if (stat.IsValid() && stat.IsDir())                           // If we're supposed to, & this is a dir, first recurse thru all its files & subdirs
  { struct dirent **eps;
    int count = scandir(path.mb_str(wxConvUTF8), &eps, one, alphasort);
    if (count == -1)   return false;                          // -1 means error
    wxArrayString dirs;
    for (int n=0; n < count; ++n)
      { wxString filename(eps[n]->d_name, wxConvUTF8);
		free (eps[n]);
        if (filename==wxT(".") || filename==wxT("..")) continue;  // Ignore dots
        wxString name = path + wxFILE_SEP_PATH + filename;
        FileData fd(name);
        if (fd.IsDir()) dirs.Add(name);                       // If it's a dir, store it here for a while
         else array.Add(name);                                // Otherwise add it to the array. Doing it this way means all path's children keep together
      }
	free (eps);

    for (size_t n=0; n < dirs.GetCount(); ++n)                // Now add each dir, then recurse into it
      { array.Add(dirs[n]);
        RecursivelyGetFilepaths(dirs[n], array);
      }
  }
 else return false;

return true;
}

bool ContainsRelativeSymlinks(wxString filepath)  // Is filepath a relative symlink, or a dir containing at least one such?
{
if (filepath.IsEmpty()) return false;
FileData fd(filepath); if (!fd.IsValid()) return false;

if (fd.IsSymlink() && fd.GetSymlinkDestination(true).GetChar(0) != wxFILE_SEP_PATH) return true;  // If this *is* a relative symlink, don't think too hard
if (!fd.IsDir()) return false;
    // OK, it's a dir. Use RecursivelyGetFilepaths() to grab all its contents, then check each one
wxArrayString fpaths; if (!RecursivelyGetFilepaths(fd.GetFilepath(), fpaths)) return false;
for (size_t n=0; n < fpaths.GetCount(); ++n)
    if (ContainsRelativeSymlinks(fpaths[n])) return true;   // Did someone mention recursion?

return false;
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------
#include "bitmaps/include/ClosedFolder.xpm"
enum { FILETYPE_FOLDER, FILETYPE_FILE };

void MyFiletypeDialog::Init(FiletypeManager* dad)
{
parent = dad;

AddApplicBtn = (wxButton*)FindWindow(wxT("AddApplication")); AddApplicBtn->Enable(true);
EditApplicBtn = (wxButton*)FindWindow(wxT("EditApplication")); EditApplicBtn->Enable(false);
RemoveApplicBtn  = (wxButton*)FindWindow(wxT("RemoveApplication")); RemoveApplicBtn->Enable(false);
AddFolderBtn = (wxButton*)FindWindow(wxT("AddFolder"));
RemoveFolderBtn = (wxButton*)FindWindow(wxT("RemoveFolder")); RemoveFolderBtn->Enable(false);
OKBtn = (wxButton*)FindWindow(wxT("wxID_OK")); OKBtn->Enable(false);
isdefault = (wxCheckBox*)FindWindow(wxT("AlwaysUse"));
interminal = (wxCheckBox*)FindWindow(wxT("OpenInTerminal"));
text = (wxTextCtrl*)FindWindow(wxT("text"));
}

void MyFiletypeDialog::OnButtonPressed(wxCommandEvent& event)
{
int id = event.GetId();

if (id == wxID_OK)   EndModal(id);                            // Finished 
if (id == wxID_CANCEL)   EndModal(id);                        // Aborted 

if (id == XRCID("RemoveApplication"))  parent->OnDeleteApplication();
if (id == XRCID("EditApplication"))       parent->OnEditApplication();
if (id == XRCID("AddApplication"))      parent->OnAddApplication();
if (id == XRCID("AddFolder"))          parent->OnNewFolder();
if (id ==  XRCID("RemoveFolder"))      parent->OnRemoveFolder();  
if (id ==  XRCID("Browse"))            parent->OnBrowse();  
}

void MyFiletypeDialog::TerminalBtnClicked(wxCommandEvent& event)  // The InTerminal checkbox was clicked in the MAIN dialog
{
wxString launch = text->GetValue();                           // Get the launch command

if (interminal->IsChecked())          // If the change was from unchecked to checked
  { if (!launch.Contains(wxT("$OPEN_IN_TERMINAL ")))          // If command doesn't already contain it, prepend the InTerminal marker
      { text->Clear(); text->SetValue(wxT("$OPEN_IN_TERMINAL ") + launch); }
    return;
  }
                                    // Otherwise the change was from checked to unchecked
launch.Replace(wxT("$OPEN_IN_TERMINAL "), wxT(""));           // Remove any InTerminal marker
text->Clear(); text->SetValue(launch);                        //   & write back the command to textctrl
}

void MyFiletypeDialog::OnIsDefaultBtn(wxCommandEvent& event)  // The IsDefault checkbox was clicked in the MAIN dialog
{
if (isdefault->IsChecked())            // If the change was from unchecked to checked
  { struct Filetype_Struct* ftype;                            // We'll need ftype to use as a parameter
    wxString appname, launch;
    
    wxString ext = parent->filepath.AfterLast(wxFILE_SEP_PATH);   // First find which ext we are dealing with
    ext = ext.AfterLast(wxT('.'));                                //   use the LAST dot to get the ext  (otherwise what if myprog.3.0.1.tar ?)
                // If no dot, try using the whole filename, in case of eg makefile having an association
    if (ext.IsEmpty()) return;
    
    if (parent->tree->IsSelectionApplic())                    // If there's a selected applic, 
      { appname = parent->tree->FTSelected->AppName;          //   use data from the selected applic where appropriate ie NOT Ext
        launch = parent->tree->FTSelected->Command;
      }
     else                          // If there isn't a selected applic, use the contents of textctrl
       { launch = text->GetValue();                           // Get the launch command
        if (launch.IsEmpty()) return;
        
        appname = launch.BeforeFirst(wxT('\%'));              // We presume the launch command is /path/path/filename %s
        appname = appname.AfterLast(wxFILE_SEP_PATH);         // Now we have just the filename + terminal space
        appname.Trim();                                       // Remove the space
      }
      
    ftype = new Filetype_Struct;                              // Create ftype
    ftype->Ext = ext;                                         // Use the constructed ext
    ftype->AppName = appname;                                 //  & the other data from whichever source
    ftype->Command = launch;
        
    if (parent->AddDefaultApplication(*ftype, false))         // Do it. False==confirm replacements
       { parent->SaveExtdata();                               // Success, so need to save the amended extdata
        parent->LoadExtdata();                                // Reload too, to make sure it happens in this instance
        if (parent->AddExtToApplicInGroups(*ftype))           // Try to add the ext to the ftype in GroupArray
          parent->SaveFiletypes();                            // Save if worked      
      }

    delete ftype;                                             // We need to delete ftype, as it wasn't added to any array
    return;
  }
                      // If we're here, the change was from checked to unchecked
parent->RemoveDefaultApplicationGivenExt(true);               // Use one of parent's methods to sort it out
}

void MyFiletypeDialog::DoUpdateUIOK(wxUpdateUIEvent &event)   // This is to enable the OK button when the textctrl has text
{
if (text==NULL) return;
bool hasdata = !text->GetValue().IsEmpty();
OKBtn->Enable(hasdata || parent->m_altered);
}


BEGIN_EVENT_TABLE(MyFiletypeDialog, wxDialog)
  EVT_BUTTON(-1, MyFiletypeDialog::OnButtonPressed)
  EVT_CHECKBOX(XRCID("OpenInTerminal"), MyFiletypeDialog::TerminalBtnClicked)
  EVT_CHECKBOX(XRCID("AlwaysUse"), MyFiletypeDialog::OnIsDefaultBtn)
  EVT_UPDATE_UI(XRCID("text"), MyFiletypeDialog::DoUpdateUIOK)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(MyApplicDialog, wxDialog)
  EVT_BUTTON(XRCID("Browse"), MyApplicDialog::OnBrowse)
  EVT_BUTTON(XRCID("NewFolder"), MyApplicDialog::OnNewFolderButton)
  EVT_CHECKBOX(XRCID("AlwaysUse"), MyApplicDialog::OnIsDefaultBtn)
END_EVENT_TABLE()

void MyApplicDialog::Init()
{
AddFolderBtn = (wxButton*)FindWindow(wxT("NewFolder"));
browse = (wxButton*)FindWindow(wxT("Browse"));
label = (wxTextCtrl*)FindWindow(wxT("Label"));
filepath = (wxTextCtrl*)FindWindow(wxT("Filepath"));
ext = (wxTextCtrl*)FindWindow(wxT("Ext"));
command = (wxTextCtrl*)FindWindow(wxT("Command"));
WorkingDir = (wxTextCtrl*)FindWindow(wxT("WorkingDir"));
isdefault = (wxCheckBox*)FindWindow(wxT("AlwaysUse"));
interminal = (wxCheckBox*)FindWindow(wxT("OpenInTerminal"));
}

void MyApplicDialog::UpdateInterminalBtn()  // Called by parent when it alters textctrl, to see if checkbox should be set
{
bool term = command->GetValue().Contains(wxT("$OPEN_IN_TERMINAL "));
interminal->SetValue(term);
}

void MyApplicDialog::OnNewFolderButton(wxCommandEvent& event)
{
parent->OnNewFolderButton(); 
}

void MyApplicDialog::OnBrowse(wxCommandEvent& event)
{
wxString Filepath = parent->Browse();                         // Call FiletypeManager::Browse() to get the desired applic filepath
if (Filepath.IsEmpty())   return;

filepath->SetValue(Filepath);                                 // Insert filepath into the Name textctrl

if (label->GetValue().IsEmpty())                              // If the label textctrl is empty, invent a name
    label->SetValue(Filepath.AfterLast(wxFILE_SEP_PATH));

if (command->GetValue().IsEmpty())
  { Filepath += wxT(" %s"); command->SetValue(Filepath); }    // If command textctrl is empty,  insert Filepath with a guess at param
}

void MyApplicDialog::OnIsDefaultBtn(wxCommandEvent& event)
{
if (GetTitle().Left(4) != wxT("Edit"))     return;            // We only want this to happen from Edit, not Add

WriteBackExtData(ftype);                                      // The contents of the ext textctrl might well have changed, so reload ftype::Ext
if (isdefault->IsChecked())                                   // If the change was from unchecked to checked
  { if (parent->AddDefaultApplication(*ftype, false))         // Add the ext(s). False==confirm replacements
      { parent->GetApplicExtsDefaults(*ftype, ext);           // Update the Ext textctrl
        parent->SaveExtdata();                                // Success, so need to save the amended extdata
        parent->LoadExtdata();                                // Reload it too, to make sure it happens in this instance
                                    // The next bit is to update the main-dialog IsDefault checkbox
        wxString ext= parent->filepath.AfterLast(wxFILE_SEP_PATH);  // Next find which ext parent's filepath has. Find the filename, 
        ext = ext.AfterLast(wxT('.'));                        //  Use the LAST dot to get the ext  (otherwise what if myprog.3.0.1.tar ?)
                                    // If no dot, try using the whole filename, in case of eg 'makefile' or 'README' having an association  
        parent->mydlg->isdefault->SetValue(parent->IsApplicDefaultForExt(*ftype, ext));
        return;
      }
  }
  
parent->RemoveDefaultApplication(*ftype);                     // Otherwise it was an UN-check
parent->GetApplicExtsDefaults(*ftype, ext);                   // Update the Ext textctrl
parent->mydlg->isdefault->SetValue(false);                    // Since all are undefaulted, reset the main-dialog IsDefault checkbox
parent->SaveExtdata();                                        // Save changes:  there must have been a default previously for the box 2b checked
parent->LoadExtdata();                                        // Reload it too, to make sure it happens in this instance
}

void MyApplicDialog::WriteBackExtData(Filetype_Struct* ftype) // Writes the contents of the ext textctrl back to ftype
{
ftype->Ext.Empty();                                           // There may be several exts, comma separated
wxStringTokenizer tkz(ext->GetValue(), wxT(","));             // First tokenise
while (tkz.HasMoreTokens())
  { wxString ext = tkz.GetNextToken(); ext.Strip(wxString::both); // Strip white-space
    if (ext.IsEmpty())  continue;                             // In case someone is daft enough to enter "   "
    if (ext.GetChar(0) == wxT('.')) ext = ext.Mid(1);         // In case someone enters .txt instead of txt (but allow tar.gz)
    ftype->Ext +=ext.Lower() + wxT(",");                      // Then add the ext to ftype->Ext
  }

ftype->DeDupExt();
if (!ftype->Ext.IsEmpty() && ftype->Ext.Last() ==  wxT(','))   ftype->Ext.RemoveLast();  // Assuming >0 additions, get rid of that last comma
}

BEGIN_EVENT_TABLE(MyFiletypeTree, wxTreeCtrl)
  EVT_TREE_SEL_CHANGED(wxID_ANY,  MyFiletypeTree::OnSelection)
END_EVENT_TABLE()

void MyFiletypeTree::Init(FiletypeManager* dad)
{
parent = dad;

wxImageList *images = new wxImageList(16, 16, true);          // Make an image list containing small icons

images->Add(wxIcon(ClosedFolder_xpm));
images->Add(wxIcon(iconfile_xpm));
AssignImageList(images);

SetIndent(TREE_INDENT);                                       // Make the layout of the tree match the GenericDirCtrl sizes
SetSpacing(TREE_SPACING);
}

void MyFiletypeTree::LoadTree(ArrayOfFiletypeGroupStructs& GroupArray)
{          // The data loaded from config by FiletypeManager is held in the array of FiletypeGroup structs GroupArray
wxTreeItemId rootId = AddRoot(_("Applications"));             // A tree needs a root
SetItemHasChildren(rootId);

for (size_t n=0; n < GroupArray.GetCount(); ++n)
  { struct FiletypeGroup* fgroup = GroupArray[n];             // Grab an entry from GroupArray, representing one subgroup eg "Editors"
    wxTreeItemId thisfolder;  bool haschildren = false;
                                                // First thing to do is to create the new folder
    MyMimeTreeItemData* data = new MyMimeTreeItemData(fgroup->Category, NextID++, false); // Make a suitable data struct                                                  
    
    thisfolder = AppendItem(rootId, fgroup->Category, FILETYPE_FOLDER, -1, data);         // Append the folder to the tree
    if (!thisfolder.IsOk()) continue;
    
                                                // Now fill this category with any loaded application names
    for (size_t c=0; c < fgroup->FiletypeArray.GetCount(); ++c)
      { Filetype_Struct* ft = fgroup->FiletypeArray[c];
        MyMimeTreeItemData* entry = new MyMimeTreeItemData(ft->AppName, NextID++, true, ft); // Make a suitable data struct
        AppendItem(thisfolder, ft->AppName, FILETYPE_FILE, -1, entry);                       // Insert the application into the folder
        haschildren = true;
      }            
      
    if (haschildren)  SetItemHasChildren(thisfolder);         // If any children were loaded, set button
  }
  

Expand(rootId);
}

wxString MyFiletypeTree::GetSelectedFolder()  // Returns either the selected subfolder, the subfolder containing the selected applic, or "" if no selection
{
wxTreeItemId sel = GetSelection();
if (!sel.IsOk()) return wxT("");

MyMimeTreeItemData* data = (MyMimeTreeItemData*)GetItemData(sel);
if (data==NULL) return wxT("");

if (data->IsApplication)
  sel = GetItemParent(sel);                                   // If the selection is an applic, replace it with its parent subfolder
if (sel == GetRootItem()) return wxT("");                     // Shouldn't happen, but just in case

data = (MyMimeTreeItemData*)GetItemData(sel);                 // Need to re-get the data in case sel was just changed
return data->Name;
}

bool MyFiletypeTree::IsSelectionApplic()  // If there is a valid selection, which is an application rather than a folder, returns true
{
wxTreeItemId sel = GetSelection();
if (!sel.IsOk()) return false;

MyMimeTreeItemData* data = (MyMimeTreeItemData*)GetItemData(sel);
if (data==NULL) return false;

return data->IsApplication;
}

void MyFiletypeTree::OnSelection(wxTreeEvent &event)
{
bool isapplic;

wxTreeItemId sel = event.GetItem();
if (!sel.IsOk()) return;
MyMimeTreeItemData* data = (MyMimeTreeItemData*)GetItemData(sel);
if (data==NULL || !data->IsApplication) isapplic = false;
  else isapplic = true;

                        
parent->mydlg->EditApplicBtn->Enable(isapplic);               // Do UI for the buttons
parent->mydlg->RemoveApplicBtn->Enable(isapplic);
parent->mydlg->RemoveFolderBtn->Enable(!isapplic);

if (isapplic)  
  { FTSelected = data->FT;                                    // If an applic, store its data,

    parent->mydlg->interminal->SetValue(FTSelected->Command.Contains(wxT("$OPEN_IN_TERMINAL ")));  // Set interminal depending on whether command contains its marker
    parent->mydlg->text->SetValue(FTSelected->Command);       //  Insert the launch command
    
    wxString ext= parent->filepath.AfterLast(wxFILE_SEP_PATH);// Next find which ext parent's filepath has. Find the filename, 
    ext = ext.AfterLast(wxT('.'));                            //  Use the LAST dot to get the ext  (otherwise what if myprog.3.0.1.tar ?)
                    // If no dot, try using the whole filename, in case of eg makefile having an association  
    parent->mydlg->isdefault->SetValue(parent->IsApplicDefaultForExt(*FTSelected, ext));  // Use this to see if the IsDefault check should be set
  }
 else 
    FTSelected = NULL; 
}


IMPLEMENT_DYNAMIC_CLASS(MyFiletypeTree, wxTreeCtrl)

//-----------------------------------------------------------------------------------------------------------------------
void Filetype_Struct::DeDupExt()
{
wxCHECK_RET(!Ext.empty(), "Trying to dedup an empty Ext");
wxString DeDuped;

wxStringTokenizer tkz(Ext, wxT(",;"));
wxString tok = tkz.GetNextToken().Lower();
while (1) 
  { if (tok.empty())
      { Ext = DeDuped; return; }
    if (!DeDuped.Contains(tok))  // Check this ext is already there (presumably for historical 'txt'/'TXT' reasons)
      { if (!DeDuped.empty()) DeDuped << ','; DeDuped << tok; }
    tok = tkz.GetNextToken().Lower();
  }
}

void FiletypeManager::Init(wxString& pathname)
{
filepath = pathname;
LoadExtdata();                                                // Load the ext/command data
stat = new FileData(filepath);                                // Do statty-type things on filepath
m_altered=false; force_kdesu=false;
}

FiletypeManager::~FiletypeManager()
{
for (int n = (int)GroupArray.GetCount();  n > 0; --n)  { FiletypeGroup* item = GroupArray.Item(n-1); delete item; GroupArray.RemoveAt(n-1); }
for (int n = (int)Extdata.GetCount();  n > 0; --n)  { FiletypeExts* item = Extdata.Item(n-1); delete item; Extdata.RemoveAt(n-1); }
if (stat != NULL) delete stat;
}

void FiletypeManager::LoadFiletypes()  // Loads Filetype dialog data from config
{
wxString Path(wxT("/FileAssociations"));                      // The Top-level config path
config = wxConfigBase::Get();                                 // Find the config data
if (config==NULL)  { wxLogError(_("Couldn't load configuration!?")); return; }

GroupArray.Clear();                                           // Make sure the array is empty

config->SetPath(Path);                                        // This is where the categories are stored
wxString entry; long cookie;

bool bCont = config->GetFirstGroup(entry, cookie);            // Load first subgroup
while (bCont) 
  { wxString oldpath = config->GetPath();
    config->SetPath(Path + wxCONFIG_PATH_SEPARATOR + entry);  // Set the subfolder path

    struct FiletypeGroup* fgroup = new struct FiletypeGroup;
    LoadSubgroup(fgroup);                                     // Use submethod to load subgroup contents to FiletypeArray

    fgroup->Category = entry;                                 // Store the subgroup name in Category
    GroupArray.Add(fgroup);                                   //  & add the group struct to main array
    
    config->SetPath(oldpath);
    bCont = config->GetNextGroup(entry, cookie);              // Look for another subgroup
  }

config->SetPath(wxT("/"));
}

void FiletypeManager::LoadSubgroup(struct FiletypeGroup* fgroup)
{
wxString Path = config->GetPath();                              // We'll need the subgroup path

wxString appname; long cookie;
                // Each filetype is stored in a sub-subgroup with the application name as its label  eg 'kedit'
bool bCont=config->GetFirstGroup(appname, cookie);              // Load first application
while (bCont) 
  { wxString subpath = Path + wxCONFIG_PATH_SEPARATOR + appname + wxCONFIG_PATH_SEPARATOR; 
    struct Filetype_Struct* ftype = new Filetype_Struct;        // Make a new Filetype_Struct

    ftype->AppName = appname;
    config->Read(subpath + wxT("Ext"), &ftype->Ext);
    ftype->DeDupExt();
    config->Read(subpath + wxT("Filepath"), &ftype->Filepath);
    config->Read(subpath + wxT("Command"), &ftype->Command);
    config->Read(subpath + wxT("WorkingDir"), &ftype->WorkingDir);
    
    fgroup->FiletypeArray.Add(ftype);
    bCont = config->GetNextGroup(appname, cookie);
  }
}

void FiletypeManager::SaveFiletypes(wxConfigBase* conf /*= NULL*/)
{
if (conf != NULL) config = conf;                                // In case we're exporting the data in ConfigureMisc::OnExportBtn
  else config = wxConfigBase::Get();                            // Otherwise find the config data (in case it's changed location recently)
if (config == NULL)  return; 

          // Delete current info (otherwise deleted data would remain, & be reloaded in future). 
          // I'm doing this at subgroup level, to prevent [FileAssociations] moving around the 'ini' file all the time
config->SetPath(wxT("/FileAssociations"));
wxString grp; bool bCont;
do
  { long cookie;
    if ((bCont = config->GetFirstGroup(grp, cookie)))
          config->DeleteGroup(grp);
   }
 while (bCont);

for (size_t n=0; n < GroupArray.GetCount(); n++)                // Go thru the array, creating each subgroup & saving its data
  SaveSubgroup(GroupArray[n]);

config->Flush();
config->SetPath(wxT("/"));
}

void FiletypeManager::SaveSubgroup(struct FiletypeGroup* fgroup)
{
wxString Path = wxT("/FileAssociations/") + fgroup->Category;   // Make the path to this subgroup
config->SetPath(Path);
if (!fgroup->FiletypeArray.GetCount())
  config->Write(Path + wxCONFIG_PATH_SEPARATOR +  wxT("dummy"), wxT(""));  // Create it & insert a dummy entry, otherwise an empty subgroup wouldn't be created

for (size_t n=0; n < fgroup->FiletypeArray.GetCount(); n++)                              // For every applic
  { wxString subpath = Path + wxCONFIG_PATH_SEPARATOR
                          + fgroup->FiletypeArray[n]->AppName + wxCONFIG_PATH_SEPARATOR; // AppName becomes the subgroup subpath
    config->Write(subpath + wxT("Ext"), fgroup->FiletypeArray[n]->Ext);                  // Write the Extension
    config->Write(subpath + wxT("Filepath"), fgroup->FiletypeArray[n]->Filepath);        //  & the Filepath
    config->Write(subpath + wxT("Command"), fgroup->FiletypeArray[n]->Command);          //  & the Command
    config->Write(subpath + wxT("WorkingDir"), fgroup->FiletypeArray[n]->WorkingDir);    //  & any working dir
  }

config->SetPath(wxT("/"));
}

bool FiletypeManager::OnNewFolder()
{
wxString newlabel;

wxTextEntryDialog getname(mydlg, _("What Label would you like for the new Folder?"));
do                                                              // Get the name in a loop, in case of duplication
  { if (getname.ShowModal() != wxID_OK)  return false;
    
    newlabel = getname.GetValue();                              // Get the desired label from the dlg
    if (newlabel.IsEmpty())  return false;
    bool flag = false;
    for (size_t n=0; n < GroupArray.GetCount(); n++)            // Ensure it's not a duplicate
      if (GroupArray[n]->Category == newlabel)
        { wxString msg; msg.Printf(_("Sorry, there is already a folder called %s\n                     Try again?"), newlabel.c_str());
          wxMessageDialog dialog(mydlg, msg, _("Are you sure?"), wxYES_NO);
          if (dialog.ShowModal() != wxID_YES) return false;
            else { flag = true; continue; }
        }
    if (flag) continue;
    break;                                                      // If we're here, no match so all's well
  }
 while (1);

struct FiletypeGroup* fgroup = new struct FiletypeGroup;        // Make a new FiletypeGroup
fgroup->Category = newlabel;                                    // Store the subgroup name in Category
GroupArray.Add(fgroup);                                         //  & add the group struct to main array

if (!from_config) SaveFiletypes();                              // Now Save the database if appropriate
m_altered=true;                                                 // Either way, set m_altered so that OK button will be enabled

                              // Finally add new folder to the tree
MyMimeTreeItemData* data = new MyMimeTreeItemData(fgroup->Category, NextID++, false);
tree->AppendItem(tree->GetRootItem(), fgroup->Category, FILETYPE_FOLDER, -1, data);  // Append the folder to the tree
return true;
}

void FiletypeManager::OnRemoveFolder()
{
bool deleted=false;

wxTreeItemId id = tree->GetSelection();  
if (!id.IsOk()) return;  

MyMimeTreeItemData* data = (MyMimeTreeItemData*)tree->GetItemData(id);
if (data->IsApplication) return;                              // Shouldn't be possible, as the button would be disabled

if (id == tree->GetRootItem())                                // Ensure we're not trying to delete the lot
  { wxString msg(_("Sorry, you're not allowed to delete the root folder"));
    wxMessageDialog dialog(mydlg, msg, _("Sigh!"), wxICON_EXCLAMATION);
    dialog.ShowModal(); return;
    }

wxString msg;
if (tree->ItemHasChildren(id))                                // Check we really mean it
      msg.Printf(_("Delete folder %s and all its contents?"), data->Name.c_str());
 else msg.Printf(_("Delete folder %s?"), data->Name.c_str());
wxMessageDialog dialog(mydlg, msg, _("Are you sure?"), wxYES_NO | wxICON_QUESTION);
if (dialog.ShowModal() != wxID_YES) return;

for (size_t n=0; n < GroupArray.GetCount(); n++)              // Find the subgroup to kill
  if (GroupArray[n]->Category == data->Name)                  // Found it. Delete the folder & contents
    { struct FiletypeGroup* temp = GroupArray[n];             // (Store group while it's removed)
      GroupArray.RemoveAt(n);                                 // Remove it
      temp->Clear(); delete temp;                             // Clear() should deal with contents, then delete releases the memory      
      deleted = true;
    }

if (!deleted) { wxLogError(_("Sorry, I couldn't find that folder!?")); return; }

tree->Delete(id);                                             // Remove from the tree

if (!from_config) SaveFiletypes();                            // Now Save the database if appropriate
m_altered=true;                                               // Either way, set m_altered so that OK button will be enabled
}

void FiletypeManager::OnAddApplication()
{
wxString Label, Filepath, WorkingDir; bool flag = false;
struct FiletypeGroup* fgroup;
MyApplicDialog dlg(this);
wxXmlResource::Get()->LoadDialog(&dlg, mydlg, wxT("AddApplicationDlg"));
dlg.Init();
combo = (wxComboBox*)dlg.FindWindow(wxT("Combo"));    

wxString selected = tree->GetSelectedFolder();                // This is either the selected subfolder or the one containing the selected applic, or ""
int index = -1;
for (size_t n=0; n < GroupArray.GetCount(); n++)              // Load the subgroup names into combobox
  { combo->Append(GroupArray[n]->Category);
    if (GroupArray[n]->Category == selected) index = n;       // If this subfolder is the one to be selected, store its index
  }
if (index != -1)
  { combo->SetSelection(index);                               // If there was a valid folder selection, use it
    combo->SetValue(combo->GetString(index));                 // Need actually to set the textbox in >2.7.0.1
  }

do                                // Get the name in a loop, in case of duplication
  { 
    dlg.label->SetFocus();                                    // We want to start with the Name textctrl
    
    if (dlg.ShowModal() != wxID_OK)  return;
    
    Label = dlg.label->GetValue();                            // Get the applic name
    Filepath = dlg.filepath->GetValue();                      // Get the applic
    if (Filepath.IsEmpty())   return;                         // If it doesn't exist, abort  
    if (Label.IsEmpty())                                      // If a specific label wasn't entered, use the filename from filepath
      Label = Filepath.AfterLast(wxFILE_SEP_PATH);            // (Returns either the filename or the whole string if there wasn't a wxFILE_SEP_PATH)
    WorkingDir = dlg.WorkingDir->GetValue();

    selected = combo->GetStringSelection();                   // Get the folder selection
    if (selected.IsEmpty())       return;                     // If there isn't one, abort.  This is only possible if the user just added a null entry!
    index = -1;          // It is possible that the user added a new combobox entry during the dialog, so search by name, not by index
    for (size_t n=0; n < GroupArray.GetCount(); n++)
        if (GroupArray[n]->Category == selected) index = n;   // If this subfolder is the one to be selected, store its index
    if (index == -1) return;                                  // Theoretically impossible

    fgroup = GroupArray[index];
    
    wxString commandstart = Filepath.BeforeFirst(wxT(' '));   // Get the real command, without parameters etc
    if (!Configure::TestExistence(commandstart, true))        // Check there is such a program
          { wxString msg; 
            if (wxIsAbsolutePath(commandstart))  msg.Printf(_("I can't find an executable program \"%s\".\nContinue anyway?"), commandstart.c_str());
              else  msg.Printf(_("I can't find an executable program \"%s\" in your PATH.\nContinue anyway?"), commandstart.c_str());
            wxMessageDialog dialog(mydlg, msg, wxT(" "), wxYES_NO);
            if (dialog.ShowModal() != wxID_YES) return;
          }
    
    for (size_t n=0; n < fgroup->FiletypeArray.GetCount(); ++n)  // Check for duplication
      if (fgroup->FiletypeArray[n]->AppName == Label)
          { wxString msg; msg.Printf(_("Sorry, there is already an application called %s\n                     Try again?"), Label.c_str());
            wxMessageDialog dialog(mydlg, msg, wxT(" "), wxYES_NO);
            if (dialog.ShowModal() != wxID_YES) return;
              else { dlg.label->SetValue(Label); flag = true; continue; }
          }
    if (flag) continue;
    break;                                        // If we're here, no match so all's well
  }
 while (1);

struct Filetype_Struct* ftype = new Filetype_Struct;          // Make a new Filetype_Struct
ftype->AppName = Label;                                       // Store the applic data
ftype->Filepath = Filepath;
ftype->Command = dlg.command->GetValue();
ftype->WorkingDir = dlg.WorkingDir->GetValue();
dlg.WriteBackExtData(ftype);                                  // Do the ext in a sub-method

if (ftype->Command.IsEmpty())                                 // If no Command was entered, fake it
  ftype->Command = ftype->Filepath + wxT(" %s");

if (dlg.interminal->IsChecked())                              // If we want this applic to launch in a terminal
  if (!ftype->Command.Contains(wxT("$OPEN_IN_TERMINAL ")))    //   & if this hasn't been sorted
     ftype->Command =  wxT("$OPEN_IN_TERMINAL ") + ftype->Command;//     prepend the command with marker for (by default) xterm -e
  
if (dlg.isdefault->IsChecked())                               // If making this default for ext,
  { bool IsDefault=true;
    wxString Ext = ftype->Ext;                                // Check an ext was entered!
    if (Ext.IsEmpty())                                        // If not, see if filepath has one
      { wxString file_ext, title;
        if ((filepath.AfterLast(wxFILE_SEP_PATH)).Find(wxT('.')) != -1)  // If there's at least 1 dot in the filename
          { file_ext = (filepath.AfterLast(wxFILE_SEP_PATH)).AfterLast(wxT('.'));  //   use the LAST of these to get the suggestion for ext
            title =  _("Please confirm");
          }
         else title =  _("You didn't enter an Extension!");
        wxString msg(_("For which extension(s) do you wish this application to be the default?"));
        Ext = wxGetTextFromUser(msg, title, file_ext);
        if (Ext.IsEmpty()) 
          IsDefault = false;                                  // If STILL no ext, flag to ignore IsDefault checkbox
         else ftype->Ext = Ext.Lower();                       // Otherwise save in ftype  
       }
    if (IsDefault)                                            // Check bool in case we reset it above
      AddDefaultApplication(*ftype, false);                   // Make default, overwriting any current version. False==confirm replacements
  }

if (!ftype->Ext.IsEmpty())   AddApplicationToExtdata(*ftype); // If there is an ext, add the applic to the submenu-type structarray

                    // Right, that's finished the ext-based bits.  Now back to the group-dialog
fgroup->FiletypeArray.Add(ftype);                             // Add the ftype entry in the designated group

if (!from_config)
  { SaveFiletypes();  SaveExtdata(); }                        // Now Save the database if appropriate
m_altered=true;                                               // Either way, set m_altered so that OK button will be enabled

mydlg->text->Clear(); mydlg->text->SetValue(ftype->Command);  // Put the new launch command into the MAIN textctrl
                                // Finally add new applic to the tree.  We need to find the folder first
wxTreeItemIdValue cookie;
wxTreeItemId folderID = tree->GetFirstChild(tree->GetRootItem(), cookie);
do
  { if (!folderID.IsOk())     return;                         // Folder not found.  This shouldn't happen!
    if (tree->GetItemText(folderID) == fgroup->Category)  break;  // Folder matches label so break
    folderID = tree->GetNextChild(tree->GetRootItem(), cookie);
  }
 while (1);
 
MyMimeTreeItemData* entry = new MyMimeTreeItemData(Label, NextID++, true, ftype);  // Make a suitable data struct
tree->AppendItem(folderID, Label, FILETYPE_FILE, -1, entry);  // Insert the application into the folder
tree->SetItemHasChildren(folderID);     
}

bool FiletypeManager::AddDefaultApplication(struct Filetype_Struct& ftype, bool noconfirm/*=true*/)  // Make applic default for a particular ext (or ext.s)
{
wxArrayString choices; wxArrayInt selections;
int NoOfExts; bool flag;
wxStringTokenizer tkz(ftype.Ext, wxT(","));                   // First tokenise ftype.Ext, in case there's more than one extension
NoOfExts = tkz.CountTokens();
if (!NoOfExts) return false;
if (NoOfExts > 1)          // If there are >1 ext.s, allow the user to chose which he wants applic to be default for
  { while (tkz.HasMoreTokens())                               // First put the ext.s in a stringarray.  It's easier
      choices.Add(tkz.GetNextToken().Strip(wxString::both));  // Strip them of their white-spaces
                          // Now do a dialog to see if we want default for all the exts or only some    
    wxDialog dlg;
    wxXmlResource::Get()->LoadDialog(&dlg, mydlg, wxT("MultipleDefaultDlg"));    // Load the appropriate dialog
    wxString msg; msg.Printf(_("For which Extensions do you want %s to be default"), ftype.AppName.c_str());  // Personalise the message
    ((wxStaticText*)dlg.FindWindow(wxT("message")))->SetLabel(msg);
    
    wxListBox* list = (wxListBox*)dlg.FindWindow(wxT("Listbox"));  
    for (size_t n=0; n < choices.GetCount(); ++n)             // Enter the ext.s into the listbox
      list->Append(choices[n]);
    dlg.GetSizer()->SetSizeHints(&dlg);
    if (dlg.ShowModal() != wxID_OK)  return false;            // Do the dialog
    
    int rad = ((wxRadioBox*)dlg.FindWindow(wxT("radiobox")))->GetSelection();
    switch(rad)
      { case 2:  list->GetSelections(selections); break;      // If various selections, get into the int-array
        case 1:  selections.Add(0); break;                    // If just the first, put a 0 into the int-array
        case 0:                                               // Otherwise select them all, by putting all their indices into array
         default: for (int n=0; n < (int)choices.GetCount(); ++n)
                     selections.Add(n);
      }
  }
 else                                 // If there's only 1 ext, add it to arrays
   { choices.Add(tkz.GetNextToken().Strip(wxString::both)); selections.Add(0); }
 
for (size_t sel=0; sel < selections.GetCount(); ++sel)        // For every ext that found its way into the int-array
  { wxString Ext = choices[ selections[sel] ].Lower();        // Get the string from string-array
    flag=false;    
    for (size_t n=0; n < Extdata.GetCount(); ++n)             // Go thru Extdata array, trying to match 1 of its known ext.s to the given one
      { if (Extdata[n]->Ext.Lower() == Ext)                   // Found it, so see if we're really changing the default applic, or just editing the old one
          { wxString ftypecommand = ftype.Command;            // Comparison is much easier if we don't have to worry about InTerminal  prefixes in either comparator
            if (ftypecommand.Contains(wxT("$OPEN_IN_TERMINAL ")))     ftypecommand.Replace(wxT("$OPEN_IN_TERMINAL "), wxT(""));
            wxString defaultcommand = Extdata[n]->Default.Command;
            if (defaultcommand.Contains(wxT("$OPEN_IN_TERMINAL ")))   defaultcommand.Replace(wxT("$OPEN_IN_TERMINAL "), wxT(""));
            
            if (!defaultcommand.IsEmpty()  &&  defaultcommand != ftypecommand)  // See if the current DefaultCommand is the same as the new one.  If so, nothing to do
              { wxString msg; msg.Printf(_("Replace %s\nwith %s\nas the default command for files of type %s?"),
                  Extdata[n]->Default.Label.c_str(), ftype.AppName.c_str(), Ext.c_str());  // If different command, check it was intended

                int result(0);
                if (!noconfirm)
                  { MyButtonDialog ApplyToAllDlg;
                    wxLogNull nocantfindmessagesthanks;
                    bool ATAdlgFound = wxXmlResource::Get()->LoadDialog(&ApplyToAllDlg, mydlg, wxT("ApplyToAllDlg"));
                    if (ATAdlgFound)
                      { ((wxStaticText*)ApplyToAllDlg.FindWindow(wxT("m_MessageTxt")))->SetLabel(msg);
                        ApplyToAllDlg.Fit();
                        result = ApplyToAllDlg.ShowModal();
                        if (((wxCheckBox*)ApplyToAllDlg.FindWindow(wxT("m_ApplyToAllCheck")))->IsChecked()) // If the user doesn't want to be asked multiple times...
                          { if (result == wxID_YES)  noconfirm = true;  // this will accept all
                             else 
                               { sel=selections.GetCount(); flag=true;  break; } // If this one wasn't wanted, apply-to-all means 'no to all the rest'
                          }
                      }
                     else // Old xrc file, so use the original way
                      { wxMessageDialog dialog(mydlg, msg, _("Are you sure?"), wxYES_NO | wxICON_QUESTION);
                        result = dialog.ShowModal();
                      }
                  }

                if (noconfirm || (result == wxID_YES))
                  { Extdata[n]->Default.Label = ftype.AppName; // If intended, substitute the new command
                    Extdata[n]->Default.Command = ftype.Command;
                    Extdata[n]->Default.WorkingDir = ftype.WorkingDir;
                  }
              }
             else 
              { Extdata[n]->Default.Command = ftype.Command;  // If there wasn't a default command before, there is now!
                Extdata[n]->Default.WorkingDir = ftype.WorkingDir;
                Extdata[n]->Default.Label = ftype.AppName;
              }
            flag=true; break;                                 // Flag we've done it
          }
      }
    if (flag) continue;    
    
                              // If we're still here, we didn't find a pre-existing ext, so do an Add
    struct FiletypeExts *extstruct = new struct FiletypeExts; // Make a new struct for the ext
    extstruct->Ext = Ext;                                     // Store the data in it
    extstruct->Default.Label = ftype.AppName;
    extstruct->Default.Command = ftype.Command;
    extstruct->Default.WorkingDir = ftype.WorkingDir;
    struct Applicstruct* app = new struct Applicstruct;       // Make a new struct for the applic bits
    app->Label = ftype.AppName;                               // Store the label bit
    app->Command = ftype.Command;                             //  & Command
    app->WorkingDir = ftype.WorkingDir;                       //  & WorkingDir
    extstruct->Applics.Add(app);                              //  & add to extstruct
    Extdata.Add(extstruct);                                   // Add struct to main structarray
  }
  
return true;
}

void FiletypeManager::RemoveDefaultApplication(struct Filetype_Struct& ftype)  // Kill the default application for a particular extension (or extensions)
{
wxStringTokenizer tkz(ftype.Ext, wxT(","));                   // First tokenise ftype.Ext, in case there's more than one extension
while (tkz.HasMoreTokens())                                   // Do every ext in a loop
  { wxString Ext = tkz.GetNextToken().Strip(wxString::both);  // Get an ext, removing surrounding white space
    for (size_t n=0; n < Extdata.GetCount(); ++n)             // Go thru Extdata array, trying to match ext
      if (Extdata[n]->Ext.Lower() == Ext.Lower())
        { Extdata[n]->Default.Clear(); break; }               // Clear default data
  }
}

void FiletypeManager::AddApplicationToExtdata(struct Filetype_Struct& ftype)  // Add submenu application for particular extension(s), adding ext if needed
{
bool flag;
wxStringTokenizer tkz(ftype.Ext, wxT(","));                   // First tokenise ftype.Ext, in case there's more than one extension
while (tkz.HasMoreTokens())                                   // Do every ext in a loop
  { wxString Ext = tkz.GetNextToken().Strip(wxString::both);  // Get an ext, removing surrounding white space
    flag=false;
    for (size_t n=0; n < Extdata.GetCount(); ++n)             // Go thru Extdata array, trying to match 1 of its known ext.s to the given one
      { bool found = false;
        if (Extdata[n]->Ext.Lower() == Ext.Lower())           // Found ext
          { for (size_t a=0; a < Extdata[n]->Applics.GetCount(); ++a)// Now look thru its applics
              if (Extdata[n]->Applics[a]->Label == ftype.AppName)  // If labels match, don't add.  Even if the commands differ, we list by label
                { found = true; break; }
            if (!found)                                       // If the applic isn't already there, add it
              { struct Applicstruct* app = new struct Applicstruct;  // Make a new struct for the applic bits
                app->Label = ftype.AppName;                   // Store the label bit
                app->Command = ftype.Command;                 //  & Command
                app->WorkingDir = ftype.WorkingDir;           //  & WorkingDir
                Extdata[n]->Applics.Add(app);                 //  & add to extstruct
              }
            flag=true; break;                                 // Flag we've done it
          }
      }
    if (flag) continue;                        
                
                // If we're here, the ext wasn't in Extdata, so add it  
    struct FiletypeExts *extstruct = new struct FiletypeExts; // Make a new struct for the ext
    extstruct->Ext = Ext.Lower();                             // Store the data in it
    
    struct Applicstruct* app = new struct Applicstruct;       // Make a new struct for the applic bits
    app->Label = ftype.AppName;                               // Store the label bit
    app->Command = ftype.Command;                             //  & Command
    app->WorkingDir = ftype.WorkingDir;                       //  & WorkingDir
    extstruct->Applics.Add(app);                              //  & add to extstruct
    
    Extdata.Add(extstruct);                                   // Add extstruct to main structarray
  }
}


void FiletypeManager::RemoveApplicationFromExtdata(struct Filetype_Struct& ftype)  // Remove application described by ftype from the Extdata structarray
{
wxStringTokenizer tkz(ftype.Ext, wxT(","));                   // First tokenise ftype.Ext, in case there's more than one extension
while (tkz.HasMoreTokens())                                   // Do every ext in a loop
  { wxString Ext = tkz.GetNextToken().Strip(wxString::both);  // Get an ext, removing surrounding white space

    for (size_t n=0; n < Extdata.GetCount(); ++n)             // Go thru Extdata array, trying to match ext
      if (Extdata[n]->Ext.Lower() == Ext.Lower())             // Found ext
        { for (size_t a=0; a < Extdata[n]->Applics.GetCount(); ++a)// Now look thru its applics
            if (Extdata[n]->Applics[a]->Label == ftype.AppName)    // If labels match, delete.  Even if the commands differ, we list by label
              { struct Applicstruct* temp = Extdata[n]->Applics[a];  // Store applic while it's removed
                Extdata[n]->Applics.RemoveAt(a);              // Remove it from array
                delete temp;                                  //   & delete it
              }
          break;                         // If we're here, no such applic in the ext.  Break: there won't be another match for this ext
        }
  }
}

void FiletypeManager::OnNewFolderButton()  // This is called (indirectly) when the "New Folder" button is pressed in the Add Application method
{
wxString newgroup; int index;

if (!OnNewFolder())   return;                                 // Use pre-existing method to get the new group

for (size_t n=0; n < GroupArray.GetCount(); n++)              // Go thru the subgroups, comparing each with the combobox contents
  { if (combo->FindString(GroupArray[n]->Category) == -1)     // When we find the new entry, save its name & append it
      { newgroup = GroupArray[n]->Category;
        combo->Append(newgroup);
      }
  }
  
index = combo->FindString(newgroup);  
if (index != -1)
    combo->SetSelection(index);                               // If we found a new folder, select it.  It should be the last item
}

void FiletypeManager::OnDeleteApplication()
{
int index = -1;
wxTreeItemId id = tree->GetSelection();  
if (!id.IsOk()) return;  

MyMimeTreeItemData* data = (MyMimeTreeItemData*)tree->GetItemData(id);
if (!data->IsApplication) return;                             // Shouldn't be possible, as the button would be disabled
if (id == tree->GetRootItem()) return;                        // Similarly

wxTreeItemId parent = tree->GetItemParent(id);                // This should be the group
if (!parent.IsOk()) return;

for (size_t n=0; n < GroupArray.GetCount(); n++)              // Find the containing subgroup
  if (GroupArray[n]->Category ==  tree->GetItemText(parent))
    { index = n; break; }                                     // Found it
if (index == -1) { wxLogError(_("Sorry, I couldn't find the application!?")); return; }  // Just in case

wxString msg; msg.Printf(_("Delete %s?"), tree->GetItemText(id).c_str());
wxMessageDialog dialog(mydlg, msg, _("Are you sure?"), wxYES_NO | wxICON_QUESTION);
if (dialog.ShowModal() != wxID_YES) return;


struct FiletypeGroup* fgroup = GroupArray[index];             // So now we have the group
for (size_t n=0; n < fgroup->FiletypeArray.GetCount(); n++)   // Go thru every applic in it
  if (fgroup->FiletypeArray[n]->AppName == tree->GetItemText(id)) // This is the one
    { RemoveDefaultApplication(*fgroup->FiletypeArray[n]);        // Remove any defaults
      RemoveApplicationFromExtdata(*fgroup->FiletypeArray[n]);    // Remove it from Extdata
      struct Filetype_Struct* temp = fgroup->FiletypeArray[n];// (Store applic while it's removed)
      fgroup->FiletypeArray.RemoveAt(n);                      // Remove it from fgroup
      delete temp;
      break; 
    }

mydlg->text->Clear();                                         // Clear the textctrl
tree->Delete(id);                                             // Remove from the tree
wxTreeItemIdValue cookie;
wxTreeItemId child = tree->GetFirstChild(parent, cookie);     // If that was the folder's sole applic, remove the HasChildren button
if (!child.IsOk()) 
  tree->SetItemHasChildren(parent, false); 
  
if (!from_config)
  { SaveFiletypes();  SaveExtdata(); }                        // Now Save the database if appropriate
m_altered=true;                                               // Either way, set m_altered so that OK button will be enabled
}

void FiletypeManager::OnEditApplication()
{
struct FiletypeGroup* fgroup, *newfgroup;
struct Filetype_Struct* ftype = NULL;
int index = -1, OriginalFtypeIndex = -1; wxString Label, Filepath, WorkingDir, selected;
                              // First recycle some of OnDeleteApplication() to work out which applic we're editing
wxTreeItemId id = tree->GetSelection();  
if (!id.IsOk()) return;  

MyMimeTreeItemData* data = (MyMimeTreeItemData*)tree->GetItemData(id);
if (!data->IsApplication) return;                             // Shouldn't be possible, as the button would be disabled
if (id == tree->GetRootItem()) return;                        // Similarly

wxTreeItemId parent = tree->GetItemParent(id);                // This should be the group
if (!parent.IsOk()) return;

for (size_t n=0; n < GroupArray.GetCount(); ++n)              // Find the containing subgroup
  if (GroupArray[n]->Category ==  tree->GetItemText(parent))
    { index = n; break; }                                     // Found it
if (index == -1) { wxLogError(_("Sorry, I couldn't find the application!?")); return; }

fgroup = GroupArray[index];                                   // So now we have the group
for (size_t n=0; n < fgroup->FiletypeArray.GetCount(); ++n)   // Go thru every applic in it
  if (fgroup->FiletypeArray[n]->AppName == tree->GetItemText(id))  // This is the one
    { ftype = fgroup->FiletypeArray[n];                       // So store it
      OriginalFtypeIndex = n; break;                          //  & its index
    }
                      // Now recycle most of OnAddApplication(), first filling the dialog with current data
MyApplicDialog dlg(this);
wxXmlResource::Get()->LoadDialog(&dlg, mydlg, wxT("AddApplicationDlg"));  // Load the inappropriate dialog
dlg.SetTitle(_("Edit the Application data")); 
dlg.Init();
combo = (wxComboBox*)dlg.FindWindow(wxT("Combo"));    

for (size_t n=0; n < GroupArray.GetCount(); ++n)              // Load the subgroup names into combobox
   combo->Append(GroupArray[n]->Category);
combo->SetSelection(index);

                                  // Load old data into the textctrls
dlg.ftype = ftype;                                            // Store ftype here too, for ease of use
dlg.label->SetValue(ftype->AppName);
dlg.filepath->SetValue(ftype->Filepath);
dlg.ext->SetValue(ftype->Ext.Lower());
dlg.command->SetValue(ftype->Command);
dlg.WorkingDir->SetValue(ftype->WorkingDir);
dlg.UpdateInterminalBtn();                                    // See if the InTerminal checkbox should be set
if (!ftype->Ext.IsEmpty())                                    // If there's an Ext, set the Default Applic checkbox if applic's the default
  dlg.isdefault->SetValue(GetApplicExtsDefaults(*ftype, dlg.ext));  //   while at the same time filling the Ext textctrl, defaults in bold


if (ftype->Command.Contains(wxT("$OPEN_IN_TERMINAL ")))       // Amputate any InTerminal marker:  it looks ugly when displayed.  We add it back later
  { ftype->Command.Replace(wxT("$OPEN_IN_TERMINAL "), wxT(""));
    dlg.command->SetValue(ftype->Command); // If so, rewrite the textctrl.  The original version had to be loaded earlier, otherwise UpdateInterminalBtn() would have failed
  }

do                                // Get the name in a loop, in case of duplication
  { 
    dlg.label->SetFocus();                                    // We want to start with the Name textctrl
    
    if (dlg.ShowModal() != wxID_OK)  return;

    Filepath = dlg.filepath->GetValue();                      // Get the applic
    if (Filepath.IsEmpty())   return;                         // If it doesn't exist, abort

    Label = dlg.label->GetValue();                            // See if Label still exists. If it does, accept whatever's there: it may be intentionally different
    if (Label.IsEmpty())                                      // If it's empty, invent it as usual
        Label = Filepath.AfterLast(wxFILE_SEP_PATH);
    WorkingDir = dlg.WorkingDir->GetValue();

    selected = combo->GetStringSelection();                   // Get the folder selection
    if (selected.IsEmpty())       return;                     // If there isn't one, abort.  This is only possible if the user just added a null entry!
    index = -1;          // It is possible that the user added a new combobox entry during the dialog, so search by name, not by index
    for (size_t n=0; n < GroupArray.GetCount(); ++n)
        if (GroupArray[n]->Category == selected) index = n;   // If this subfolder is the one to be selected, store its index
    if (index == -1) return;                                  // Theoretically impossible
      
    newfgroup = GroupArray[index];
    bool flag = false;
    if (newfgroup != fgroup || Label != ftype->AppName)       // ie don't complain about dup name if it's the applic itself!
      for (size_t n=0; n < newfgroup->FiletypeArray.GetCount(); ++n)  // Otherwise check that the amended version doesn't clash with a pre-existing one
        if (newfgroup->FiletypeArray[n]->AppName == Label)
            { wxString msg; msg.Printf(_("Sorry, there is already an application called %s\n                     Try again?"), Label.c_str());
              wxMessageDialog dialog(mydlg, msg, wxT(" "), wxYES_NO);
              if (dialog.ShowModal() != wxID_YES) return;
                else { dlg.label->SetValue(Label); flag = true; continue; }
            }
    if (flag) continue;
    break;                                                    // If we're here, no match so all's well
  }
 while (1);

RemoveApplicationFromExtdata(*ftype);    // Before any updating is done, remove the applic from extdata.  We'll add it back when it's been amended
struct Filetype_Struct oldftype = *ftype;                     // Save orig data, for use in updating default-applics

ftype->AppName = Label;                                       // Store the amended data
ftype->Filepath = Filepath;
ftype->Command = dlg.command->GetValue();
ftype->WorkingDir = dlg.WorkingDir->GetValue();
dlg.WriteBackExtData(ftype);                                  // Do the ext in a sub-method

if (ftype->Command.IsEmpty())                                 // If no Command was entered, fake it
  ftype->Command = ftype->Filepath + wxT(" %s");

if (dlg.interminal->IsChecked())                              // If we want this applic to launch in a terminal
  { if (!ftype->Command.Contains(wxT("$OPEN_IN_TERMINAL ")))  //   & if this hasn't been sorted
       ftype->Command = wxT("$OPEN_IN_TERMINAL ") + ftype->Command; //  prepend the command with marker for (by default) xterm -e
  }
 else
  ftype->Command.Replace(wxT("$OPEN_IN_TERMINAL "), wxT("")); // Remove any InTerminal marker
     
mydlg->text->Clear(); mydlg->text->SetValue(ftype->Command);  // Put the (new?) launch command into the MAIN textctrl

AddApplicationToExtdata(*ftype);                              // Now we've updated ftype, it's safe to return the applic to extdata
UpdateDefaultApplics(oldftype, *ftype);                       // Make sure any label/command edits are copied into default data too

if (newfgroup != fgroup)  // If there hasn't been a change of group, that's all we need to do.  But if there has been:
  { newfgroup->FiletypeArray.Add(fgroup->FiletypeArray[OriginalFtypeIndex]); // Add the ftype entry its new group (doing it this way should avoid needing to delete)
    fgroup->FiletypeArray.RemoveAt(OriginalFtypeIndex);       //  & remove it from the original one

    tree->Delete(id);                                         // Remove from the tree
    wxTreeItemIdValue cookie;
    wxTreeItemId child = tree->GetFirstChild(parent, cookie); // If that was the folder's sole applic, remove the HasChildren button
    if (!child.IsOk()) 
      tree->SetItemHasChildren(parent, false); 

                                    // Finally add applic to its new folder.  We need to find the folder first
    cookie=0;
    wxTreeItemId folderID = tree->GetFirstChild(tree->GetRootItem(), cookie);
    do
      { if (!folderID.IsOk())     return;                     // Folder not found.  This shouldn't happen!
        if (tree->GetItemText(folderID) == newfgroup->Category)  break;  // Folder matches label so break
        folderID = tree->GetNextChild(tree->GetRootItem(), cookie);
      }
     while (1);

    MyMimeTreeItemData* entry = new MyMimeTreeItemData(Label, NextID++, true, ftype);  // Make a new data struct. Easier than amending the previous one
    tree->AppendItem(folderID, Label, FILETYPE_FILE, -1, entry);  // Insert the application into the folder
    tree->SetItemHasChildren(folderID);     
  }
  
if (!from_config)
  { SaveFiletypes();  SaveExtdata(); }                        // Now Save the database if appropriate
m_altered=true;                                               // Either way, set m_altered so that OK button will be enabled
}

void FiletypeManager::OnBrowse()
{
wxString filepath = Browse();                                 // Call Browse() to get the desired applic filepath
if (filepath.IsEmpty())   return;

mydlg->text->SetValue(filepath);                              // Insert it into the textctl
}

wxString FiletypeManager::Browse()
{
wxString message(_("Choose a file"));
wxFileDialog fdlg(mydlg,message, wxGetApp().GetHOME(), wxT(""), wxT("*"), wxFD_OPEN);
if (fdlg.ShowModal()  != wxID_OK) return wxT("");

return fdlg.GetPath();
}

void FiletypeManager::LoadExtdata()  // Load the file containing preferred Ext:Applic data
{
config = wxConfigBase::Get(); if (config==NULL)  return;
Extdata.Clear();                                              // In case this is a reload (after a save), need to clear the stucts first

wxString Rootname(wxT("LaunchExt/"));
size_t count = (size_t)config->Read(Rootname + wxT("count"), 0l);

for (size_t n=0; n < count; ++n)
  { wxString grp = CreateSubgroupName(n, count);              // Create a subgroup name: "a", "b" etc
    struct FiletypeExts* extstruct = new struct FiletypeExts; // Make a new struct for storage

    extstruct->Ext = config->Read(Rootname+grp+wxT("/Ext"));  // Load the ext we're dealing with
    extstruct->Ext.MakeLower();
    extstruct->Default.Label = config->Read(Rootname+grp+wxT("/DefaultLabel"), wxT(""));
    extstruct->Default.Command = config->Read(Rootname+grp+wxT("/DefaultCommand"), wxT(""));
    extstruct->Default.WorkingDir = config->Read(Rootname+grp+wxT("/DefaultWorkingDir"), wxT(""));
    
    size_t appcount = (size_t)config->Read(Rootname+grp+ wxT("/appcount"), 0l);
    for (size_t c=0; c < appcount; ++c)                       // Now go thru the Applic array, making a subgroup for each Label/Command pair
      { wxString subgrp = wxT("/") + CreateSubgroupName(c, appcount);
        struct Applicstruct* app = new struct Applicstruct;
        app->Label = config->Read(Rootname+grp+subgrp+wxT("/Label"), wxT(""));
        app->Command = config->Read(Rootname+grp+subgrp+wxT("/Command"), wxT(""));
        app->WorkingDir = config->Read(Rootname+grp+subgrp+wxT("/WorkingDir"), wxT(""));
        extstruct->Applics.Add(app);                          //  Add to extstruct
      }
      
    Extdata.Add(extstruct);                                   // Finished this ext.  Add struct to main structarray
  }

ExtdataLoaded = true;
}


void FiletypeManager::SaveExtdata(wxConfigBase* conf /*= NULL*/)  // Save the file containing preferred Ext:Applic data
{
if (conf != NULL) config = conf;                              // In case we're exporting the data in ConfigureMisc::OnExportBtn
  else config = wxConfigBase::Get();                          // Otherwise find the config data (in case it's changed location recently)

if (config==NULL)  return;

config->DeleteGroup(wxT("LaunchExt"));                        // Delete previous info

wxString Rootname(wxT("LaunchExt/"));
size_t count = Extdata.GetCount();
config->Write(wxT("LaunchExt/count"), (long)count);

for (size_t n=0; n < count; ++n)
  { wxString grp = CreateSubgroupName(n, count);              // Create a subgroup name: "a", "b" etc
    config->Write(Rootname+grp+wxT("/Ext"), Extdata[n]->Ext.Lower()); // Store the ext we're dealing with
    if (!Extdata[n]->Default.Command.IsEmpty())               // Assuming there is a default, save it
      { config->Write(Rootname+grp+wxT("/DefaultLabel"), Extdata[n]->Default.Label);
        config->Write(Rootname+grp+wxT("/DefaultCommand"), Extdata[n]->Default.Command);
        config->Write(Rootname+grp+wxT("/DefaultWorkingDir"), Extdata[n]->Default.WorkingDir);
      }
    
    size_t appcount = Extdata[n]->Applics.GetCount();
    config->Write(Rootname+grp+wxT("/appcount"), (long)appcount);
    for (size_t c=0; c < appcount; ++c)                       // Now go thru the Applic array, making a subgroup for each Label/Command pair
      { wxString subgrp = wxT("/") + CreateSubgroupName(c, appcount);
        config->Write(Rootname+grp+subgrp+wxT("/Label"), Extdata[n]->Applics[c]->Label);
        config->Write(Rootname+grp+subgrp+wxT("/Command"), Extdata[n]->Applics[c]->Command);
        config->Write(Rootname+grp+subgrp+wxT("/WorkingDir"), Extdata[n]->Applics[c]->WorkingDir);
      }
  }

config->Flush();
}

void FiletypeManager::OpenWith()  // Do 'Open with' dialog
{
LoadFiletypes();                                          // Load the dialog data
if (!ExtdataLoaded)   LoadExtdata();                      // & the Ext data

MyFiletypeDialog dlg;   mydlg = &dlg;
wxXmlResource::Get()->LoadDialog(&dlg, MyFrame::mainframe->GetActivePane(), wxT("ManageFiletypes"));  // Load the appropriate dialog
dlg.Init(this);

wxSize size = MyFrame::mainframe->GetClientSize();        // Get a reasonable dialog size.  WE have to, because the treectrl has no idea how big it is
dlg.SetSize(size.x/3, 2*size.y/3);
dlg.CentreOnScreen();

tree  = (MyFiletypeTree*)dlg.FindWindow(wxT("TREE"));
tree->Init(this);                                         // Do the bits that weren't done in XRC
tree->LoadTree(GroupArray);                               // Load the folder & filetype data into the tree

HelpContext = HCopenwith;
if (dlg.ShowModal() != wxID_OK)
  { if (m_altered && filepath.IsEmpty())                  // filepath will be empty if we've come from Configure
      { wxMessageDialog dialog(MyFrame::mainframe->GetActivePane(), _("Lose all changes?"), _("Are you sure?"), wxYES_NO | wxICON_QUESTION);
        if (dialog.ShowModal() != wxID_YES)   SaveFiletypes(); 
      }
    HelpContext = HCunknown; return;
  }
HelpContext = HCunknown;

wxString Command = dlg.text->GetValue();                  // Get the launch command from textctrl, in case user had amended it
if (!Command.IsEmpty())                                   // Check we have an application to use
  { wxString open =  ParseCommand(Command);               // Replace any %s with filepath (else just shove it on the end)
    if (open.IsEmpty())  return;
    bool usingsu = (force_kdesu || (!stat->CanTHISUserRead()));  // While we still have stat-access, check if we'll need kdesu
    Openfunction(open, usingsu);                          // Then use sub-method for the Open
  }
}

wxString FiletypeManager::ParseCommand(const wxString& cmd /*=wxT("")*/)  // Goes thru command, looking for %s to replace with filepath
{     // A cut-down, amended version of wxFileType::ExpandCommand()
wxString command(cmd);
if (command.IsEmpty())    command = Array[0];
                              // Later, we'll use single quotes around the filepath to avoid problems with spaces etc.              
EscapeQuote(filepath);        // However this CAUSES a problem with apostrophes!  This global function goes thru filepath, Escaping any it finds

wxString str; bool found = false;

for (const wxChar *pc = command.c_str(); *pc != wxT('\0'); pc++)    // Go thru command, looking for %
  { if (*pc == wxT('%'))
      { if (*++pc == wxT('s'))                                      // Found one, so see if next char is 's'.  If so:
          {              // '%s' expands into filepath, quoted because it might contain spaces unless there are already quotes
            found = true;
            if ((*(pc - 2) == wxT('"') && *(pc + 1) == wxT('"'))    // If the %s is already surrounded by double quotes,
                || (*(pc - 2) == wxT('\'') && *(pc + 1) == wxT('\'')))//  or by single ones,
                        str << filepath;                            // just replace it with filepath
              else
                        str << wxT('\'') << filepath << wxT('\'');  // Otherwise do so surrounded by single quotes: they seem less upseting to some progs
          }        
             else                                                   // If it wasn't 's'
          { if (*pc == wxT('%'))    str << *pc;                     // If it's another %, ie %% means escaped %, just copy it
             else { found = true; str << filepath; }                // I'm wickedly choosing to treat any other %char as a parameter but not using quotes
          }
          }
     else str << *pc;                                               // If it wasn't '%', copy it
    }

                    // Right, that's finished the for-loop.  If we found a %s, we're done.  But what if there wasn't one? 
if (found == false)
  str = command + wxT(" \'") + filepath + wxT("\'");                // Just plonk the filepath on the end, surrounded by single quotes
  
return str;    
}

bool FiletypeManager::Open(wxArrayString& commands)
{
bool CtrlDown = wxGetKeyState(WXK_CONTROL); // Some files eg scripts, may be both executable and openable.  If ctrl is pressed, open instead of exec
bool AltDown = wxGetKeyState(WXK_ALT);      // Similarly, if the 'ALT' key is pressed, show the OpenWith menu instead of exec or any default OpenUsingFoo()
wxMouseState state = wxGetMouseState();
// NB wxGetMouse/KeyboardState don't work for the META key, and ALT+DClick seems to be swallowed by the system, so we have to look for CTRL+ALT+DClick

if (AltDown) // If so, instead of Open just show the OpenWith menu
  { return OfferChoiceOfApplics(commands); }

                          // If it's an executable, execute it if we have permission
if (stat->IsFileExecutable() && !CtrlDown)                    // Is it executable?  Is the Ctrl-key pressed, which means use OpenWith instead
  { if (filepath.Contains(wxT("$OPEN_IN_TERMINAL ")))         // First check to see if we're to run in terminal.  If so, replace marker with eg. 'xterm -e'
      filepath.Replace(wxT("$OPEN_IN_TERMINAL "), TERMINAL_COMMAND, false); // As this section will be reached from e.g. a DClick, I doubt if this'll ever happen

    if ((!force_kdesu) && stat->CanTHISUserExecute())         // Assuming we weren't TOLD to open with su, & we have execute permission...
        { wxExecute(filepath); return false; }

    if (WHICH_SU==mysu)                                       // If it's executable, but we don't have permission, su if available
      { if (USE_SUDO) ExecuteInPty(wxT("sudo \"") + filepath + wxT('\"'));
         else         ExecuteInPty(wxT("su -c \"") + filepath + wxT('\"'));
        return false;
      }
    else
     if (WHICH_SU==kdesu)
      { wxString command; command.Printf(wxT("kdesu %s"), filepath.c_str());
        wxExecute(command); return false;
      }      
    else     
     if (WHICH_SU==gksu)                                      // or gksu
      { wxString command; command.Printf(wxT("gksu %s"), filepath.c_str());
        wxExecute(command); return false;
      }      
    else 
     if (WHICH_SU==gnomesu)
      { wxString command; command.Printf(wxT("gnomesu --title "" -c %s -d"), filepath.c_str());  // -d means don't show the "Which user?" box, just use root
        wxExecute(command); return false;
      }
    else     
     if (WHICH_SU==othersu && !OTHER_SU_COMMAND.IsEmpty())    // or user-provided
      { 
        wxExecute(OTHER_SU_COMMAND + filepath); return false;
      }  
                // Still here?  Then apologise & ask if user wishes to try Reading the file, assuming Read permission
    if (!stat->CanTHISUserRead()) 
      { wxMessageBox(_("Sorry, you don't have permission to execute this file.")); return false; }
     
    if (wxMessageBox(_("Sorry, you don't have permission to execute this file.\nWould you like to try to Read it?"),
                    wxT(" "), wxYES_NO | wxICON_QUESTION) != wxYES)    return false;
  }
                          
                // It's NOT executable, so try to Open it.  First use the file's ext to try to get a default launch command
if (PREFER_SYSTEM_OPEN)
  { if (USE_SYSTEM_OPEN && FiletypeManager::OpenUsingMimetype(filepath))
      return false; // false here signifies success
     else return (USE_BUILTIN_OPEN && FiletypeManager::OpenUsingBuiltinMethod(filepath, commands, false));
  }
 else
  { if (USE_BUILTIN_OPEN && FiletypeManager::OpenUsingBuiltinMethod(filepath, commands, true))
      return false; // false here signifies success
     else if (USE_SYSTEM_OPEN && FiletypeManager::OpenUsingMimetype(filepath))
      return false;
     else return (USE_BUILTIN_OPEN && FiletypeManager::OpenUsingBuiltinMethod(filepath, commands, false));
  }
wxCHECK_MSG(false, false, wxT("Unreachable code"));
}

bool FiletypeManager::OpenUsingBuiltinMethod(const wxString& filepath, wxArrayString& commands, bool JustOpen)
{
if (DeduceExt())
  if (GetLaunchCommandFromExt())                              // If true, Array[0] now contains the launch command eg kedit %s
    { wxString open = ParseCommand();                         //  Parse combines this with filepath eg  kedit '/home/david/my.txt'
      bool usingsu = (force_kdesu || (!stat->CanTHISUserRead())); // While we still have stat-access, check if we'll need kdesu
      Openfunction(open, usingsu);                            // Do the rest in submethod, shared by 'Open with'
      return true;
    }
if (JustOpen) return false;                                   // Stop here to let the system method have a go

                // Didn't work?  OK, look & see if there is a list of known applics for the ext
if (OfferChoiceOfApplics(commands))
  return false;   // There ARE known applics, so return true to make caller process them
 else
  { OpenWith(); return true; }                                // No known applics, so go the whole hog with OpenWith()
}

bool FiletypeManager::OpenUsingMimetype(const wxString& fpath)
{
wxString filepath(fpath);

// If we're trying to Open a file that's inside an archive, first extract it to a temp dir
MyGenericDirCtrl* active = MyFrame::mainframe->GetActivePane();
if (active && active->arcman && active->arcman->IsArchive())
  { FakeFiledata ffd(filepath); if (!ffd.IsValid()) return false;
    wxArrayString arr; arr.Add(filepath);
    wxArrayString tempfilepaths = active->arcman->ExtractToTempfile(arr, false, true);  // This extracts filepath to a temporary file, which it returns. The bool params mean not-dirsonly, files-only
    if (tempfilepaths.IsEmpty()) return false;          // Empty == failed
    
    filepath = tempfilepaths[0];
    FileData tfp(filepath); tfp.DoChmod(0444);  // Make the file readonly, as we won't be saving any alterations and exec.ing it is unlikely to be sensible
  }

// Start by trying to use GIO calls, provided we're using wxGTK
#if defined __WXGTK__
if (filepath.empty()) return false;

GAppInfo* appinfo = NULL;
GFile* file = g_file_new_for_path(filepath.mb_str(wxConvUTF8));
if (file)
  { appinfo = g_file_query_default_handler(file, NULL, NULL);
    g_object_unref(file);
  }

if (appinfo)
  { wxString command(g_app_info_get_executable(appinfo), wxConvUTF8);
    if (!command.empty())
      { command << wxT(' ') << EscapeQuoteStr( EscapeSpaceStr(filepath) );
        wxExecute(command);
        return true;
      }
  }
// If it failed, we might as well try the wxTheMimeTypesManager way so fall through
#endif

if (!DeduceExt()) return false;                       // false means filepath was empty
if (!Array.GetCount()) return false;                  // Shouldn't happen

wxString fp = EscapeQuoteStr( EscapeSpaceStr(filepath) );
for (size_t n=0; n < Array.GetCount(); ++n)           // There may be >1 offerings in Array
  { wxFileType* ft = wxTheMimeTypesManager->GetFileTypeFromExtension(Array[n]);
    if (ft)
      { wxString command = ft->GetOpenCommand(fp);
        delete ft;
        if (!command.empty())
          { wxExecute(command);
            return true;
          }
      }
  }

return false;
}

//static
void FiletypeManager::Openfunction(wxString& open, bool usingsu)  // Submethod to actually launch applic+file.  Used by Open and Open-With
{
if (open.Contains(wxT("$OPEN_IN_TERMINAL ")))           // First check to see if we're to run in terminal.  If so, replace marker with eg. 'xterm -e'
  open.Replace(wxT("$OPEN_IN_TERMINAL "), TERMINAL_COMMAND, false);

MyGenericDirCtrl* active = MyFrame::mainframe->GetActivePane();
bool InArchive =  (active != NULL && active->arcman != NULL && active->arcman->IsArchive());

if (InArchive)                                          // If we're opening a file from within an archive, we need the archive's cooperation!
  { wxString tempfilepath = active->arcman->ExtractToTempfile();  // This extracts the selected filepath to a temporary file, which it returns
    if (!tempfilepath) return;                          // Empty == failed
    FileData tfp(tempfilepath); tfp.DoChmod(0444);      // Take the opportunity to make this file readonly, as we won't be saving any alterations and exec.ing it is unlikely to be sensible
    
    wxStringTokenizer tkz(open);                        // We now need to replace the /withinarchive/path/to/foo bit of open with ~/temppath/to/foo
    wxString file = tempfilepath.AfterLast(wxFILE_SEP_PATH);
    bool flag = false;
    while (tkz.HasMoreTokens())            
      { wxString tok =  tkz.GetNextToken();
        if (tok.Contains(file))                         // This will break if open is e.g."kwrite kwrite"; but it probably won't be!
          { open.Replace(tok, tempfilepath, false); flag = true; break; }
      }
    if (!flag) return;                                  // Shouldn't happen, but if the substitution couldn't be made, abort
    usingsu = false;                                    // Because of earlier FileData failures, we might have been wrongly told to su
  }

if (!usingsu)  wxExecute(open);                         // If we can, execute the command to launch the file   
  else
  { if (WHICH_SU==mysu)                                 // If we don't have permission, su if available
      { if (USE_SUDO) ExecuteInPty(wxT("sudo \"") + open + wxT('\"'));
         else         ExecuteInPty(wxT("su -c \"") + open + wxT('\"'));
      }
    else
      if (WHICH_SU==kdesu)
      { wxString command; command.Printf(wxT("kdesu \"%s\""), open.c_str());
        wxExecute(command);
      }
    else
      if (WHICH_SU==gksu)                               // or gksu
        { wxString command; command.Printf(wxT("gksu \"%s\""), open.c_str());
          wxExecute(command);
        }      

    else 
     if (WHICH_SU==gnomesu)
      { // Try using xsu (== gnomesu == gsu) but it's less satisfactory.
        //  It can't cope with simple names like gedit, it needs the whole pathname, even for gnome applications.
        // It also throws an Error on exiting, complaining that kbuildsycoca is running (actually it only manages ildsycoca!). That's a kde applic that does the mimetypes.
        wxString command; command.Printf(wxT("gnomesu --title "" -c \"%s\" -d"), open.c_str());  // -d means don't show the "Which user?" box, just use root
        wxExecute(command);                             //  wxLogNull doesn't work
      }
    else
      if (WHICH_SU==othersu && !OTHER_SU_COMMAND.IsEmpty())  // or user-provided
      { wxString command; command.Printf(wxT("\"%s\""), open.c_str());
        wxExecute(OTHER_SU_COMMAND + command);
      } 
  }
}

bool FiletypeManager::DeduceExt()  // Which bit of a filepath should we call 'ext' for GetLaunchCommandFromExt()
{
int index;
if (filepath.IsEmpty())     return false;
Array.Clear();                                          // Array is where we'll store the answer(s), so get rid of any stale data

wxString ext = filepath.AfterLast(wxFILE_SEP_PATH);     // First remove the path

  // The plan is:  Look for a dot.  If there isn't one, put the whole filename into the array, in case we want to open eg makefile with an Editor.
  // If there is, put everything to the R of it in the array, & then look for another dot.  This way we return a hierarchy of possible exts eg tar.gz, then gz
  
index = ext.Find(wxT('.'));                             // Check there's at least one dot  (-1 means no)
if (index == -1)    
  { Array.Add(ext); return true; }                      // If not, put the whole filename into the array, in case we want to open eg makefile with an Editor
  
do                                // There IS a .ext
  { ext = ext.Mid(index+1);                             // ext now contains the substring distal to the 1st dot
    Array.Add(ext);                                     // Add the rest to Array
    index = ext.Find(wxT('.'));                         // See if there's another dot
  }
 while (index != -1);                                   // If there is, go round for another bite

return true;
}

bool FiletypeManager::GetLaunchCommandFromExt()  // FiletypeManager::Array contains a list of possible ext.s, created in DeduceExt()
{
if (!Array.GetCount()) return false;                    // Shouldn't happen

for (size_t e=0; e < Extdata.GetCount(); ++e)           // Go thru Extdata array, trying to match 1 of its known ext.s to one of the file's ones
  for (size_t n=0; n < Array.GetCount(); ++n)           // There may be >1 offerings in Array
    if (Array[n].Lower() == Extdata[e]->Ext.Lower())    // Got one
      if (!Extdata[e]->Default.Command.IsEmpty())       // See if there's a default command
        { Array.Clear();                                // If so, put it in the Array, flagging its presence by returning true
          wxString command;
            // If there's a working dir to cd into first, we need a shell + dquotes. The next global function does that
          PPath_returncodes ans = PrependPathToCommand(Extdata[e]->Default.WorkingDir, Extdata[e]->Default.Command, wxT('\"'), command, true);
          if (ans == Oops) return false;
          if (ans == needsquote) command << wxT('\"');  // The command now starts with sh -c, so we need to close the quote

          Array.Add(command);
          return true;
        }   // Otherwise carry on thru the Array, in case a later entry works eg prog.0.3.1.tar.gz might succeed with tar.gz, but not 0.3.1.tar.gz

return false;                                           // No match
}

void FiletypeManager::RemoveDefaultApplicationGivenExt(bool confirm /*=false*/)  //  Given an ext (in filepath), un-associate its default applic, if any
{
wxString Ext = filepath.AfterLast(wxFILE_SEP_PATH);       // Remove path
Ext = Ext.AfterLast(wxT('.'));                            // Use the bit after the last dot
                    // If no dot, try using the whole filename, in case of eg makefile having an association  
if (Ext.IsEmpty()) return;

int index = GetDefaultApplicForExt(Ext.Lower());          // Try to find an applic for it
if (index == -1)  return;                                 // -1 flags failure

if ((size_t)index > Extdata.GetCount())   return;

if (confirm)
  { wxString msg; 
    msg.Printf(_("No longer use %s as the default command for files of type %s?"), Extdata[index]->Default.Label.c_str(), Ext.c_str());  
    wxMessageDialog dialog(mydlg, msg, _("Are you sure?"), wxYES_NO | wxICON_QUESTION);
    if (dialog.ShowModal() != wxID_YES)  return;
  }

Extdata[index]->Default.Clear();                            // Clear the default
SaveExtdata();
LoadExtdata();                                              // Save & reload the data, to make it happen in this instance
}

int FiletypeManager::GetDefaultApplicForExt(const wxString& Ext)  // Given an ext, find its default applic, if any. Returns -1 if none
{
for (size_t e=0; e < Extdata.GetCount(); ++e)               // Go thru Extdata array, trying to match 1 of its known ext.s to Ext
  if (Extdata[e]->Ext.Lower() == Ext.Lower())                               // Got it
    { if (Extdata[e]->Default.Command.IsEmpty())            // See if there's a default command
        return -1;                                          // Nope
      return e;                                             // There is, so return its index
    }
    
return -1;
}

bool FiletypeManager::GetApplicExtsDefaults(const struct Filetype_Struct& ftype, wxTextCtrl* text)  // Fill textctrl, & see if applic is default for any of its ext.s
{
bool result=false, isdefault, notfirst=false;

wxTextAttr style; wxFont font;

text->Clear();                                              // Clear textctrl
style = text->GetDefaultStyle();                            // Save its original style
font = style.GetFont();                                     // Get default font, & enBolden it
font.SetWeight(wxFONTWEIGHT_BOLD);

wxStringTokenizer tkz(ftype.Ext, wxT(","));                 // First tokenise Exts
while (tkz.HasMoreTokens())            
  { wxString Ext =  tkz.GetNextToken().Trim().Lower();      // Remove any terminal space: see the workaround below for why there might be some
    isdefault = IsApplicDefaultForExt(ftype, Ext);          // For each ext, see if it's default
    
    if (notfirst) text->AppendText(wxT(','));               // If this isn't the 1st ext, separate with a comma
    
    if (isdefault)
      { text->SetDefaultStyle(wxTextAttr(wxNullColour, wxNullColour, font));  // Turn on Bold
        text->AppendText(Ext);                              // Add Ext to textctrl with Bold font
        text->SetDefaultStyle(style);                       // Return to original style
        result = true;                                      // Flag that there's at least one
      }
     else   
        text->AppendText(Ext);                              // If applic isn't default for this ext, add Ext with normal font
    
    notfirst = true;
  }
// The next line is a workaround.  If the last item entered was in Bold, the (re)SetDefaultStyle() doesn't work. Nor does anything else eg add something then remove it.
if (result)  text->AppendText(wxT(" "));                    // So add a space. This means if the user adds another ext, it isn't in Bold; yet it doesn't show too much.   
return result;                                              // True if >0 defaults
}

bool FiletypeManager::IsApplicDefaultForExt(const struct Filetype_Struct& ftype, const wxString& Ext)  // See if the applic is the default for extension Ext
{
if (Ext.IsEmpty() || ftype.Command.IsEmpty()) return false;

wxString ext = Ext.BeforeFirst(wxT(','));                   // The passed Ext may be multiple eg htm, html.  This will never match, so use just the 1st item

wxString command = ftype.Command;  // The stored command may start with the InTerminal flag, but this would have been temporarily stripped during OnEditApplication()
if (command.Contains(wxT("$OPEN_IN_TERMINAL "))) command.Replace(wxT("$OPEN_IN_TERMINAL "), wxT("")); // So provide the trucated version for the comparison too

for (size_t e=0; e < Extdata.GetCount(); ++e)               // Go thru Extdata array, trying to find ext
  if (Extdata[e]->Ext.Lower() == ext.Lower())
    { if (Extdata[e]->Default.Label == ftype.AppName && (Extdata[e]->Default.Command == ftype.Command || Extdata[e]->Default.Command == command))
            return true;                                    // If everything matches, return the Ext
       else   return false;
    }
    
return false;                                               // No match
}

void FiletypeManager::UpdateDefaultApplics(const struct Filetype_Struct& oldftype, const struct Filetype_Struct& newftype)  // Update default data after Edit
{
if (oldftype.AppName.IsEmpty())     return;                 // Don't want to match empty data

for (size_t e=0; e < Extdata.GetCount(); ++e)               // Go thru Extdata array, trying to find matching default data for oldftype
  if (Extdata[e]->Default.Label == oldftype.AppName && Extdata[e]->Default.Command == oldftype.Command)
    { Extdata[e]->Default.Label = newftype.AppName;         // Got a match, so replace olddata with new, in case of alterations
      Extdata[e]->Default.Command = newftype.Command;
      Extdata[e]->Default.WorkingDir = newftype.WorkingDir;
    }
}

bool FiletypeManager::OfferChoiceOfApplics(wxArrayString& CommandArray)  // Loads Array with applics for this filetype, & CommandArray with launch-commands
{
if (filepath.IsEmpty())     return false;

wxString ext = filepath.AfterLast(wxFILE_SEP_PATH);
ext = ext.AfterLast(wxT('.'));                            // Use the LAST dot to get the ext  (otherwise what if myprog.3.0.1.tar ?)
            // If no dot, try using the whole filename, in case of eg makefile having an association

for (size_t n=0; n < Extdata.GetCount(); ++n)             // Go thru Extdata array, trying to match 1 of its known ext.s to the file's one
  if (Extdata[n]->Ext.Lower() == ext.Lower())
    { Array.Clear();
      for (size_t c=0; c < Extdata[n]->Applics.GetCount(); ++c)  // Add all the associated applic names to an array
        { Array.Add(Extdata[n]->Applics[c]->Label);       // This application's name goes into FiletypeManager::Array
          wxString launchcommand =  ParseCommand(Extdata[n]->Applics[c]->Command);  // Parse its associated launch command
          CommandArray.Add(launchcommand);                // & put result (indirectly) in FileGenericDirCtrl's array, so that it can be retrieved after Context menu 
        }
      if (!Array.GetCount()) return false;
        else return true;
    }
return false;
}

bool FiletypeManager::QueryCanOpen(char* buf)  // See if we can open the file by double-clicking, ie executable or ext with default command
{
if (filepath.IsEmpty())     return false;
if (!wxFileExists(filepath)) return false;                // Check file actually exists

if (stat->IsFileExecutable())                             // See if the file is executable
  { if (stat->CanTHISUserExecute())  buf[1] = true;       // Yes, by us so flag Open
     else if (WHICH_SU != dunno)                          // Yes but WE can't
                                 buf[2] = true;           //  so flag only if we have su-itable alternative
  }

if (stat->CanTHISUserRead() || stat->CanTHISUserWrite())
                    buf[0] = true;                        // If it can be read or written by us, flag OpenWith
if ((!stat->CanTHISUserRead() || !stat->CanTHISUserWrite()) // If either are missing,
              &&  (WHICH_SU != dunno))                    //   & we have *su
                    buf[2] = true;                        //     flag OpenWithKdesu

if (!buf[1] && stat->CanTHISUserRead())    // Finally, if we haven't already flagged Open for executable reasons, & we can read, see if we have a default launcher
  { if (!ExtdataLoaded) LoadExtdata();                    // Load the ext/command data
    Array.Add(filepath);                                  // Put filepath in 1st slot of Array
    if (DeduceExt())                                      // if DeduceExts returns true, Array will now be filled with 1 or more ext.s to check
      if (GetLaunchCommandFromExt())                      // If this is true, there is a launch command available, so flag Open
        buf[1] = true;
  }
return true;
}

bool FiletypeManager::QueryCanOpenArchiveFile(wxString& filepath) // A cutdown QueryCanOpen() for use within archives
{
if (!ExtdataLoaded) LoadExtdata();                        // Load the ext/command data
Array.Add(filepath);                                      // Put filepath in 1st slot of Array
if (DeduceExt())                                          // if DeduceExts returns true, Array will now be filled with 1 or more ext.s to check
  if (GetLaunchCommandFromExt())  return true;            // If this is true, there is a launch command available, so flag Open

return false;      
}

bool FiletypeManager::AddExtToApplicInGroups(const struct Filetype_Struct& ftype)  // Try to locate applic matching ftype. If so, add ftype.Ext to it
{
for (size_t g=0; g < GroupArray.GetCount(); ++g)          // Go thru the GroupArray              
  { for (size_t n=0; n < GroupArray[g]->FiletypeArray.GetCount(); ++n)  // For each filetype in subgroup  
      if (GroupArray[g]->FiletypeArray[n]->Command == ftype.Command)    // If the Commands match, assume the rest does too
        { wxString Ext; Filetype_Struct* fgroup = GroupArray[g]->FiletypeArray[n];
          wxStringTokenizer tkz(fgroup->Ext, wxT(","));                 // Tokenise ftype.Ext, in case it has more than one extension
          while (tkz.HasMoreTokens())                                   // Do every ext in a loop
            { Ext = tkz.GetNextToken().Strip(wxString::both);           // Get an ext, removing surrounding white space
              if (Ext.Lower() == ftype.Ext.Lower()) return false;       // See if it matches the new one.  If so, abort
            }
          fgroup->Ext += wxT(',') + ftype.Ext.Lower();    // If we're here, the ext is a new one, so add it
          return true;                                    // No point in further searching
        }      
  }
return false;                                             // In case applic not found
}

