/////////////////////////////////////////////////////////////////////////////
// Name:       ArchiveStream.cpp
// Purpose:    Virtual manipulation of archives
// Part of:    4Pane
// Author:     David Hart
// Copyright:  (c) 2019 David Hart
// Licence:    GPL v3
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h" 

#include "wx/tokenzr.h"
#include "wx/stream.h"
#include "wx/zipstrm.h"
#include "wx/zstream.h"
#include "wx/mstream.h"
#include "wx/wfstream.h"

#include "Devices.h"
#include "MyDirs.h"
#include "MyGenericDirCtrl.h"
#include "MyFrame.h"
#include "Externs.h"
#include "Redo.h"
#include "Archive.h"
#include "ArchiveStream.h"
#include "Filetypes.h"
#include "Misc.h"

#include "bzipstream.h"
#include "Otherstreams.h"
#include <grp.h>

wxString FakeFiledata::PermissionsToText() // Returns a string describing the filetype & permissions eg -rwxr--r--. Adapted from real FileData, but uses the archivestream::GetMode() info
{
wxString text;
text.Printf(wxT("%c%c%c%c%c%c%c%c%c%c"), IsDir() ? wxT('d') : wxT('-'),  
  !!(Permissions & S_IRUSR) ? wxT('r') : wxT('-'), !!(Permissions & S_IWUSR) ? wxT('w') : wxT('-'), !!(Permissions & S_IXUSR) ? wxT('x') : wxT('-'),
  !!(Permissions & S_IRGRP) ? wxT('r') : wxT('-'), !!(Permissions & S_IWGRP) ? wxT('w') : wxT('-'), !!(Permissions & S_IXGRP) ? wxT('x') : wxT('-'),
  !!(Permissions & S_IROTH) ? wxT('r') : wxT('-'), !!(Permissions & S_IWOTH) ? wxT('w') : wxT('-'), !!(Permissions & S_IXOTH) ? wxT('x') : wxT('-')
      );

if (text.GetChar(0) == wxT('-'))                                // If it wasn't a dir, 
  { if (IsSymlink()) text.SetChar(0, wxT('l'));                 //   see if it's one of the miscellany, rather than a regular file
    else if (IsCharDev())text.SetChar(0, wxT('c'));    
    else if (IsBlkDev()) text.SetChar(0, wxT('b'));    
    else if (IsSocket()) text.SetChar(0, wxT('s'));    
    else if (IsFIFO())   text.SetChar(0, wxT('p'));  
  }
                                // Now amend string if the unusual permissions are set.
if (!!(Permissions & S_ISUID)) text.GetChar(3) == wxT('x') ?  text.SetChar(3, wxT('s')) : text.SetChar(3, wxT('S'));  //  If originally 'x', use lower case, otherwise upper
if (!!(Permissions & S_ISGID)) text.GetChar(6) == wxT('x') ?  text.SetChar(6, wxT('s')) : text.SetChar(6, wxT('S'));
if (!!(Permissions & S_ISVTX)) text.GetChar(9) == wxT('x') ?  text.SetChar(9, wxT('t')) : text.SetChar(9, wxT('T'));
      
return text;
}

enum ffscomp FakeFiledata::Compare(FakeFiledata* comparator, FakeFiledata* candidate)  // Finds how the candidate file/dir is related to the comparator one
{
if (comparator==NULL || candidate==NULL) return ffsRubbish;
if (!comparator->IsDir())  return ffsRubbish;              // I can't see atm why we'd be comparing a file with another file

wxString candid = candidate->GetFilepath();
if (!candidate->IsDir()) candid = candid.BeforeLast(wxFILE_SEP_PATH); candid += wxFILE_SEP_PATH;  // In case this is a file, amputate the filename

      // Go thru the dirs segment by segment until there's a mismatch, or 1 runs out.  We can rely on each starting/ending in '/' as we did it in the ctor
wxStringTokenizer candtok(candid.Mid(1), wxFILE_SEP_PATH);
wxStringTokenizer comptok(comparator->GetFilepath().Mid(1), wxFILE_SEP_PATH);
while (comptok.HasMoreTokens())
  { if (!candtok.HasMoreTokens())
      { if (candidate->IsDir()) return ffsParent;           // The candidate is parent to the comparator dir
          else return ffsCousin;                            // Otherwise its a file belonging to a (g)parent dir. ffsCousin is the best description
      }
    wxString tmp = comptok.GetNextToken(); int ans = tmp.Cmp(candtok.GetNextToken());  // Compare next token of each
    if (ans != 0) return ffsCousin;                         // Different so return that they're not (very) related
  }                                                         // Otherwise identical so loop
    
if (candtok.HasMoreTokens()) return ffsChild;               // The candidate matches but is longer, so it's a (g)child of the comparator
if (!candidate->IsDir()) return ffsChild;                   // The candidate matches in terms of Path, but it's a file of the comparator
return ffsEqual;                                            // Otherwise they must be identical dirs
}

//-----------------------------------------------------------------------------------------------------------------------

void FakeDir::Clear()
{
for (int n = (int)dirdataarray->GetCount();  n > 0; --n)  { FakeDir* item = dirdataarray->Item(n-1); delete item; }
for (int n = (int)filedataarray->GetCount();  n > 0; --n) { DataBase* item = filedataarray->Item(n-1); delete item; }
dirdataarray->Clear(); filedataarray->Clear();
}

enum ffscomp FakeDir::AddDir(FakeDir* dir)
{
if (dir == NULL) return ffsRubbish;

enum ffscomp ans = Compare(this, dir);                      // First compare the dir with this one
if (ans != ffsChild) return ans;                            // If it's above us or in another lineage or rubbish, say so

for (size_t n=0; n < SubDirCount(); ++n)
  { enum ffscomp ans = Compare(GetFakeDir(n), dir);
    if (ans == ffsChild)  return GetFakeDir(n)->AddDir(dir);// It's a (g)child of this dir, add it there
    if (ans != ffsCousin)  { delete dir; return ffsDealtwith; } // If parent (which shouldn't be possible) or a duplicate or rubbish, abort
  }
        // Still here? Then it must be a previously unacknowledged child. Add it bit by bit, in case it's a grandchild & we haven't yet met its parent
FakeDir *AddHere = this, *lastdir = parentdir;              // The first segment of new dir will be added to us, of course
wxString rest = dir->GetFilepath().Mid(GetFilepath().Len());// Get the bit of string subsequent to this dir
wxStringTokenizer extra(rest, wxFILE_SEP_PATH, wxTOKEN_STRTOK); // NB use wxTOKEN_STRTOK here so as not to return an empty token from /foo/bar//baz
while (extra.HasMoreTokens())
  { wxString fp = AddHere->GetFilepath() + extra.GetNextToken() + wxFILE_SEP_PATH;  // Add this segment to the previous path, and create a new child FakeDir with that name
    if (extra.HasMoreTokens())                              // If there's more to come after this segment, create a new fakedir for this bit
      { FakeDir* newdir = new FakeDir(fp, 0, (time_t)0, 0,wxT(""),wxT(""), lastdir);
        AddHere->AddChildDir(newdir); parentdir = AddHere; AddHere = newdir;
      }
     else AddHere->AddChildDir(dir);                        // Otherwise add the actual new dir!
  }

return ffsDealtwith;
}

enum ffscomp FakeDir::AddFile(FakeFiledata* file)
{
if (file == NULL) return ffsRubbish;

enum ffscomp ans = Compare(this, file);                           // First compare the file with this one
if (ans !=  ffsChild) return ans;                                 // If it's above us or in another lineage or rubbish, say so

for (size_t n=0; n < SubDirCount(); ++n)
  { enum ffscomp ans = Compare(GetFakeDir(n), file);
    if (ans == ffsChild)  return GetFakeDir(n)->AddFile(file);    // It belongs in a (g)child of this dir, add it there
    if (ans != ffsCousin)  { delete file; return ffsDealtwith; }  // If it belongs in a parent (which shouldn't be possible) or is a duplicate or rubbish, abort
  }
        // Still here? Then it must be a previously unacknowledged child. Add it bit by bit, in case it's a grandchild & we haven't yet met its parent
wxString rest = file->GetFilepath().Mid(GetFilepath().Len());     // Get the bit of string subsequent to this dir
wxString filename = rest.AfterLast(wxFILE_SEP_PATH); rest = rest.BeforeLast(wxFILE_SEP_PATH);  // Separate into path & name
  
FakeDir *AddHere = this, *lastdir = parentdir;                    // The first segment of any new dir will be added to us, of course
wxStringTokenizer extra(rest, wxFILE_SEP_PATH, wxTOKEN_STRTOK);   // Go thru any new Path, creating dirs as we go
while (extra.HasMoreTokens())
  { wxString fp = AddHere->GetFilepath() + extra.GetNextToken() + wxFILE_SEP_PATH;  // Add this segment to the previous path, and create a new child FakeDir with that name
    FakeDir* newdir = new FakeDir(fp, 0, (time_t)0, 0,wxT(""),wxT(""), lastdir);
    AddHere->AddChildDir(newdir); parentdir = AddHere; AddHere = newdir;
  }

AddHere->AddChildFile(file);      // We've created any necessary dir structure. Now we can finally add the file
return ffsDealtwith;
}

FakeDir* FakeDir::FindSubdirByFilepath(const wxString& targetfilepath)
{
if (GetFilepath() == targetfilepath || GetFilepath() == targetfilepath+wxFILE_SEP_PATH) return this;  // If we're it, return us
for (size_t n=0; n < SubDirCount(); ++n)
  { FakeDir* child = dirdataarray->Item(n)->FindSubdirByFilepath(targetfilepath);  if (child != NULL) return child; }  // Otherwise recurse thru children

return NULL; 
}

FakeFiledata* FakeDir::FindFileByName(wxString filepath)
{
if (filepath.IsEmpty())  return NULL;
if (filepath.Last() == wxFILE_SEP_PATH) filepath = filepath.BeforeLast(wxFILE_SEP_PATH);  // In case there's a spurious terminal '/'

wxString path = filepath.BeforeLast(wxFILE_SEP_PATH), filename = filepath.AfterLast(wxFILE_SEP_PATH);
FakeDir* dir = FindSubdirByFilepath(path);
if (dir == NULL) return NULL;

return dir->GetFakeFile(filename);
}

bool FakeDir::HasSubDirs(wxString& dirname) // Find a FakeDir with this name, and see if it has subdirs
{
FakeDir* dir = FindSubdirByFilepath(dirname);       // Find if a dir with this name exists. NULL if not
if (dir != NULL) return (dir->SubDirCount() > 0);   // Then just count the subdirs
return false;
}

bool FakeDir::HasFiles(wxString& dirname)  // Find a FakeDir with this name, and see if it has files
{
FakeDir* dir = FindSubdirByFilepath(dirname);
if (dir != NULL) return (dir->FileCount() > 0);
return false;
}

bool FakeDir::GetFilepaths(wxArrayString& array)
{
if (!FileCount()) return false;

for (size_t n=0; n < FileCount(); ++n)
  { DataBase* temp = GetFakeFiledata(n);
    array.Add(temp->GetFilepath());
  }
return true;
}

void FakeDir::GetDescendants(wxArrayString& array, bool dirs_only/*=false*/)  // Fill the array with all its files & (recursively) its subdirs
{
if (!dirs_only)
  GetFilepaths(array);                              // First add this dir's files to the array (unless we're pasting a dir skeleton)

for (size_t n=0; n < SubDirCount(); ++n)            // Now for every subdir
  { FakeDir* temp = GetFakeDir(n); if (temp==NULL) continue;

    array.Add(temp->GetFilepath());                 // Add the subdir
    temp->GetDescendants(array, dirs_only);         // and recurse thru it
  }
}

wxULongLong FakeDir::GetSize() // Returns the size of its files
{
wxULongLong totalsize = 0;

for (size_t n=0; n < FileCount(); ++n)              // First add the size of each file
  totalsize += GetFakeFiledata(n)->Size();

// Since we've just worked it out anyway, why not update our 'm_size' too
size = totalsize;

return totalsize;
}

wxULongLong FakeDir::GetTotalSize() // Returns the size of all its files & those of its subdirs
{
wxULongLong totalsize = GetSize();                  // First the files
for (size_t n=0; n < SubDirCount(); ++n)            // Now recursively for every subdir
  totalsize += GetFakeDir(n)->GetTotalSize();

return totalsize;
}
//-----------------------------------------------------------------------------------------------------------------------

FakeFilesystem::FakeFilesystem(wxString filepath, wxULongLong size, time_t Time, size_t Perms, wxString Owner/*=wxT("")*/, wxString Group/*=wxT("")*/)
{
rootdir = new FakeDir(filepath, size, Time, Perms, Owner, Group); // Create the 'root' dir, which will be the archive itself
currentdir = rootdir;                                             // We start with the root
}

void FakeFilesystem::AddItem(wxString filepath, wxULongLong size, time_t Time, size_t Perms, DB_filetype Type, const wxString& Target/*=wxT("")*/, const wxString& Owner/*=wxT("")*/, const wxString& Group/*=wxT("")*/)
{
if (filepath.IsEmpty()) return;

if (Type == DIRTYPE)    rootdir->AddDir(new FakeDir(filepath, size, Time, Perms, Owner, Group, rootdir));
  else       rootdir->AddFile(new FakeFiledata(filepath, size, Time, Type, Target, Perms, Owner, Group));
}

//-----------------------------------------------------------------------------------------------------------------------

bool ArcDir::Open(wxString dirName)
{
if (arc == NULL || arc->Getffs() == NULL || dirName.IsEmpty()) return false;
if (arc->Getffs()->GetRootDir()->FindSubdirByFilepath(dirName) == NULL)  return false;
filepaths.Clear();
startdir = dirName;            // This is the dir to investigate. We need to save it to use in ArcDir::GetFirst
isopen = true; return true;
}

bool ArcDir::GetFirst(wxString* filename, const wxString& filespec, int flags)
{
SetFileSpec(filespec); SetFlags(flags);

wxString currentcurrentdir = arc->Getffs()->GetCurrentDir()->GetFilepath();      // Save the current currentdir
if (!arc->SetPath(startdir)) { arc->SetPath(currentcurrentdir); return false; }  // Change it to point to the dir we're interested in

if (m_flags & wxDIR_DIRS)
  { wxArrayString temp; arc->GetDirs(temp);
    filepaths = temp;
  }

if (m_flags & wxDIR_FILES)  arc->Getffs()->GetCurrentDir()->GetFilepaths(filepaths);

arc->SetPath(currentcurrentdir);                      // Revert to the original currentdir

if (filepaths.GetCount() > 0) { nextfile=0; return GetNext(filename); }
 else return false;
}

bool ArcDir::GetNext(wxString* filename)
{
if (filepaths.IsEmpty() || nextfile < 0) return false;

while ((size_t)nextfile < filepaths.GetCount())
  { wxString name = filepaths[ nextfile++ ];
    if (m_filespec.IsEmpty()                         // An empty filespec is considered to match everything
          ||  wxMatchWild(m_filespec, name))         // Pass this name thru the filter
      { *filename = name; return true; }
  }

return false;                                        // No (more) names that match filespec
}

//-----------------------------------------------------------------------------------------------------------------------


ArchiveStream::ArchiveStream(DataBase* archive)  // It's a real archive
{
m_membuf = NULL;
wxString name = archive->GetFilepath(); if (name.IsEmpty()) return;
if (name.GetChar(name.Len()-1) != wxFILE_SEP_PATH) name << wxFILE_SEP_PATH;  // Although it's an archive, not a real dir, for our purposes we need it to pretend
ffs = new FakeFilesystem(name, archive->Size(), archive->ModificationTime(), archive->GetPermissions(), archive->GetOwner(), archive->GetGroup());
Dirty = Nested = false; archivename = name;
}

ArchiveStream::ArchiveStream(wxString name)  // Used for nested archives
{
m_membuf = NULL;
if (name.IsEmpty()) return;
if (name.GetChar(name.Len()-1) != wxFILE_SEP_PATH) name << wxFILE_SEP_PATH;  // Although it's an archive, not a real dir, for our purposes we need it to pretend
ffs = new FakeFilesystem(name, 0, 0, 0);
Dirty = false; Nested = true; archivename = name;
}

bool ArchiveStream::LoadToBuffer(wxString filepath)  // Used by the subclassed ctors to load the archive into m_membuf
{
wxFFileInputStream in(filepath);                  // Load the archive thru a wxFFileInputStream
wxStreamBuffer streambuf((wxInputStream&)in, wxStreamBuffer::read);  //  and store it first in a wxStreamBuffer (since this can cope with a stream feed)
size_t size = in.GetSize();                       // This is how much was loaded i.e. the file size
if (!size) return false;

wxChar* buf = new wxChar[ size ];                 // It's ugly, but the easiest way I've found to get the data from the streambuffer to a memorybuffer
streambuf.Read(buf, size);                        //   is by using an intermediate char[]

delete m_membuf;                                  // Get rid of any old data, otherwise we'll leak when reloading
m_membuf = new wxMemoryBuffer(size);              // Now (at last) we can store the data in a memory buffer
m_membuf->AppendData(buf, size);
delete[] buf;

return true;
}

bool ArchiveStream::LoadToBuffer(wxMemoryBuffer* mbuf)  // Reload m_membuf with data from a different pane's buffer
{
if (mbuf == NULL) return false;

delete m_membuf;                              // Get rid of any old data, otherwise we'll leak
m_membuf = new wxMemoryBuffer(mbuf->GetDataLen());
m_membuf->AppendData(mbuf->GetData(), mbuf->GetDataLen());
return true;
}

bool ArchiveStream::SaveBuffer()  // Saves the compressed archive in buffer back to the filesystem
{
if (IsNested()) { Dirty = true; return true; }  // Except we don't if this is a nested archive: it gets saved on Pop

wxFFileOutputStream fileoutstream(archivename); if (!fileoutstream.Ok()) return false;
fileoutstream.Write(m_membuf->GetData(), m_membuf->GetDataLen());

return fileoutstream.IsOk();                    // Returns true if the Write was successful
}

void ArchiveStream::ListContentsFromBuffer(wxString archivename)
{
wxBusyCursor busy;
GetFromBuffer();
ListContents(archivename);
}

bool ArchiveStream::IsWithin(wxString filepath)  // Find if the passed filepath is within this archive
{
bool isdir = Getffs()->GetRootDir()->FindSubdirByFilepath(filepath) != NULL;  // Start by seeing if there's a dir with this name
if (isdir) return true;
return (Getffs()->GetRootDir()->FindFileByName(filepath) != NULL);            // Failing that, try for a file
}


FakeFiledataArray* ArchiveStream::GetFiles()  // Returns the files for the current 'path'
{
return ffs->GetFiles();
}

bool ArchiveStream::GetDirs(wxArrayString& dirs)  // Returns in the arraystring the files for the current 'path'
{
dirs.Clear();

for (size_t n=0; n < ffs->GetCurrentDir()->SubDirCount(); ++n)
  { FakeDir* dir =  ffs->GetCurrentDir()->GetFakeDir(n);
    dirs.Add(dir->GetFilepath());  // NB a FakeDir GetFilepath() returns the full filepath, whereas a FakeFiledata GetFilepath() currently just gives the filename
  }
return dirs.GetCount() > 0;
}

bool ArchiveStream::GetDescendants(wxString dirName, wxArrayString& filepaths)  // Returns in the array all files, dirs & subdirs files for the path
{
filepaths.Clear();

FakeDir* dir = Getffs()->GetRootDir()->FindSubdirByFilepath(dirName);
if (dir == NULL) return false;        // Path must be a dir, not a file or a null

dir->GetDescendants(filepaths);       // This does the real work

return filepaths.GetCount() > 0;
}

bool ArchiveStream::SetPath(const wxString& dir)
{
if (dir.IsEmpty()) return false;

if ((dir+wxFILE_SEP_PATH)  == ffs->GetRootDir()->GetFilepath())  { ffs->SetCurrentDir(ffs->GetRootDir()); return true; }  // First check that we're not reverting to root

wxString path(dir);
if (path.GetChar(path.Len()-1) != wxFILE_SEP_PATH) path << wxFILE_SEP_PATH;

FakeDir* newdir =  ffs->GetRootDir()->FindSubdirByFilepath(path);
if (newdir != NULL) { ffs->SetCurrentDir(newdir); return true; }   // Bingo! There's a subdir with this name

wxString prepath = dir.BeforeLast(wxFILE_SEP_PATH) + wxFILE_SEP_PATH; // It wasn't a subdir. Let's check & see if it's a file
newdir =  ffs->GetRootDir()->FindSubdirByFilepath(prepath);
if (newdir == NULL) return false;                                  // No valid parent dir either
if (newdir->HasFileCalled(dir.AfterLast(wxFILE_SEP_PATH)))         // See if this parent dir contains the correct file
  { ffs->SetCurrentDir(newdir); return true;  }                    // If so, set path to the parent dir

return false;
}


bool ArchiveStream::OnExtract()   // On Extract from context menu. Finds what to extract
{
MyGenericDirCtrl* active = MyFrame::mainframe->GetActivePane(); if (active==NULL) return false;
wxArrayString filepaths; size_t count = active->GetMultiplePaths(filepaths); if (!count) return false;

bool ClusterWasNeeded = UnRedoManager::StartClusterIfNeeded(); wxString empty;
wxArrayString resultingfilepaths; bool result = DoExtract(filepaths, empty, resultingfilepaths);
if (ClusterWasNeeded) UnRedoManager::EndCluster();
return result;
}

bool ArchiveStream::DoExtract(wxArrayString& filepaths, wxString& destination, wxArrayString& resultingfilepaths, bool DontUndo/*=false*/, bool dirs_only/*=false*/)  // From OnExtract or D'n'D/Paste, or Move
{
MyGenericDirCtrl* active = MyFrame::mainframe->GetActivePane(); if (active==NULL) return false;

size_t count = filepaths.GetCount(); if (!count) return false;
wxArrayString WithinArcNames;
for (size_t n=0; n < count; ++n)
  { wxString filepath = filepaths[n];             // For each filepath, find and store the bit distal to the archive name
    if (!SortOutNames(filepath)) return false;    // Some implausible breakage
    
    wxString WithinArcName(WithinArchiveName); WithinArcNames.Add(WithinArcName);
  }

wxString suggestedfpath = active->arcman->GetPathOutsideArchive();

while (destination.IsEmpty())      // Get the desired destination from the user, if it's not already been provided
  { wxString msg; if (count >1) msg = _("To which directory would you like these files extracted?");
      else msg = _("To which directory would you like this extracted?");
    destination = wxGetTextFromUser(msg, _("Extracting from archive"), suggestedfpath);
    if (destination.IsEmpty()) return false;                                  // A blank answer must be a hint ;)
  
    wxString destinationpath(destination); if (destinationpath.Last() == wxFILE_SEP_PATH) destinationpath = destinationpath.BeforeLast(wxFILE_SEP_PATH);
    FileData DestinationFD(destinationpath);
    if (DestinationFD.IsValid() && ! DestinationFD.CanTHISUserWriteExec())    // Make sure we have permission to write to the dir
      { wxMessageDialog dialog(MyFrame::mainframe->GetActivePane(),
                    _("I'm afraid you don't have permission to Create in this directory\n                       Try again?"), _("No Entry!"), wxYES_NO | wxICON_ERROR);
        if (dialog.ShowModal() == wxID_YES) { destination.Empty(); continue; }
          else return false;
      }
    bool flag=false;
    for (size_t n=0; n < count; ++n)
      { FileData fd(destinationpath + wxFILE_SEP_PATH + WithinArcNames[n]);  // Check for pre-existence of each file: at least those in the base dir
        if (fd.IsValid())
          { wxString msg; msg.Printf(_("Sorry, %s already exists\n            Try again?"), fd.GetFilepath().c_str());
            wxMessageDialog dialog(active, msg, _("Oops!"), wxYES_NO | wxICON_ERROR);
            if (dialog.ShowModal() == wxID_YES) { destination.Empty(); flag=true; break; }
              else return false;
          }
      }
    if (flag == true) continue;
  }

if (destination.Last() != wxFILE_SEP_PATH)  destination << wxFILE_SEP_PATH;   // Nasty things would happen otherwise

if (!Extract(filepaths, destination, resultingfilepaths, dirs_only))   return false;

wxArrayInt IDs; IDs.Add(active->GetId());
if (!DontUndo)
  { UnRedoExtract* UnRedoptr = new UnRedoExtract(resultingfilepaths, destination, IDs);
    UnRedoManager::AddEntry(UnRedoptr);  
  }
  
MyFrame::mainframe->OnUpdateTrees(destination, IDs, wxT(""), true);
return true;
}

bool ArchiveStream::SortOutNames(const wxString& filepath)  // Subroutine used in Extract etc, so made into a function in the name of code reuse
{
WithinArchiveName.Empty(); WithinArchiveNames.Clear(); WithinArchiveNewNames.Clear(); IsDir = false;
archivename = ffs->GetRootDir()->GetFilepath(); if (archivename.IsEmpty()) return false;  // Now get the true archivename: the archive expects it ;)

wxString FakeFilesystemPath(ffs->GetCurrentDir()->GetFilepath()); if (FakeFilesystemPath.IsEmpty()) return false;  // First use current path, in case we're recursing in a subdir
wxString filename; 
if (!(filepath == archivename.BeforeLast(wxFILE_SEP_PATH) 
        || filepath.StartsWith(FakeFilesystemPath, &filename)))            // This effectively removes the current within-archive path from filepath, leaving the filename
  { if (!filename.IsEmpty())                                               // just in case filepath IS the archive
      { if (ffs->GetCurrentDir()->HasDirCalled(filename)) IsDir = true;    // We'll need to know, as dirs get a terminal '/'
          else if (!ffs->GetCurrentDir()->HasFileCalled(filename))   return false;  // If it's not a file either, abort!
      }
     else
      { if (FakeFilesystemPath == (filepath+wxFILE_SEP_PATH))  IsDir = true; }
  }
                               // WithinArchiveName is the filename to extract: may be subpath/filename within the archive
if (!filepath.StartsWith(archivename, &WithinArchiveName))  return false;  // Get the "filename" bit: may have a bit of path-within-archive too
if (IsDir)  WithinArchiveName << wxFILE_SEP_PATH;                          // Dirs in the archive will have a terminal '/'

archivename = archivename.BeforeLast(wxFILE_SEP_PATH);                     // Remove the archive's spurious terminal '/'

return true;
}
    // Get the within-archive names, any descendants, and (if renaming) the new names
bool ArchiveStream::AdjustFilepaths(const wxArrayString& filepaths, const wxString& destpath, wxArrayString& newnames, adjFPs whichtype/*=afp_neither*/, bool dirs_only/*=false*/, bool files_only/*=false*/)
{
WithinArchiveNames.Clear(); WithinArchiveNewNames.Clear();
archivename = ffs->GetRootDir()->GetFilepath(); if (archivename.IsEmpty()) return false; // Get the archive name, which we need to remove
                      // If we're from Rename or Extract, newnames will hold the new names/destination path
if (whichtype==afp_rename && newnames.GetCount() > filepaths.GetCount()) return false;  // If there are too many of them, abort

for (size_t n=0; n < filepaths.GetCount(); ++n)                               // For every file/dir we were passed
  { if (!filepaths[n].StartsWith(archivename, &WithinArchiveName))  continue; // Amputate the /path/to/archive bit
    wxString WithinArchiveNewname;
    if (whichtype==afp_rename) 
      if (!newnames[n].StartsWith(archivename, &WithinArchiveNewname))  continue;  // We need to get rid of the path from newname too

    FakeDir* fd = ffs->GetRootDir()->FindSubdirByFilepath(filepaths[n]);      // See if it's a dir
    if (fd == NULL) 
      { if (dirs_only) continue; // Don't look for files if it's not a dir and we only want those

        FakeFiledata* ffd = ffs->GetRootDir()->FindFileByName(filepaths[n]); if (ffd == NULL) continue;  // If it's not a dir, or a file, abort this one
        WithinArchiveNames.Add(WithinArchiveName);
        if (whichtype==afp_rename)   WithinArchiveNewNames.Add(WithinArchiveNewname);  // If we're renaming, add the corresponding new name
          else { newnames.Add(destpath + filepaths[n].AfterLast(wxFILE_SEP_PATH)); WithinArchiveNewNames.Add(newnames[n]); } // Extract to here
      }
     else                // It's a dir. We need to find & process all descendants too
      { if (!files_only) // but only if we _want_ dirs
          { WithinArchiveNames.Add(WithinArchiveName + wxFILE_SEP_PATH);          // but first do the dir itself
            if (whichtype==afp_rename)   WithinArchiveNewNames.Add(WithinArchiveNewname);  // If we're renaming, add the corresponding new dir name
              else 
              { wxString dirname;
                if (filepaths[n].Last() == wxFILE_SEP_PATH) dirname = (filepaths[n].BeforeLast(wxFILE_SEP_PATH)).AfterLast(wxFILE_SEP_PATH); 
                  else dirname = filepaths[n].AfterLast(wxFILE_SEP_PATH);
                newnames.Add(destpath + dirname); WithinArchiveNewNames.Add(destpath + dirname);  // Extract to here
              }
              
            wxArrayString fpaths; fd->GetDescendants(fpaths, dirs_only);          // This recursively gets all (files and) subdirs into fpaths
            size_t distalbit = 0;                                                 // We need this to extract bar/ from archive/foo/bar. Assume 0 to start with, which would be true for foo
            if (WithinArchiveName.find(wxFILE_SEP_PATH) != wxString::npos)
              distalbit = WithinArchiveName.BeforeLast(wxFILE_SEP_PATH).Len() + (archivename.Last()==wxFILE_SEP_PATH);  // This gets bar/ from archive/foo/bar
            for (size_t i=0; i < fpaths.GetCount(); ++i)
              { wxString descendantpart;
                if (!fpaths[i].StartsWith(archivename, &descendantpart)) continue;// For every descendant, get & add the descendant part
                WithinArchiveNames.Add(descendantpart);
                if (whichtype==afp_rename)                                        // If we're renaming, construct the corresponding new name for each
                    WithinArchiveNewNames.Add(WithinArchiveNewname + descendantpart.Mid(WithinArchiveName.Len()));
                 else { newnames.Add(destpath + descendantpart.Mid(distalbit)); WithinArchiveNewNames.Add(destpath + descendantpart.Mid(distalbit));  } // distalbit is calculated to use just the desired distal segments 
              }
        }
      }
  }

archivename = archivename.BeforeLast(wxFILE_SEP_PATH);                        // Remove the archive's spurious terminal '/'
return WithinArchiveNames.GetCount() > 0;
}

bool ArchiveStream::Alter(wxArrayString& filepaths, enum alterarc DoWhich, wxArrayString& newnames, bool dirs_only/*=false*/)  // This might be add remove del or rename, depending on the enum
{
if (filepaths.IsEmpty()) return false;

if (DoWhich != arc_add)                         // If not arc_add, remove the /pathtoarchive/archive bit of each filepath
  { wxString Unused; if (!AdjustFilepaths(filepaths, Unused, newnames, DoWhich==arc_rename || DoWhich==arc_dup ? afp_rename : afp_neither, dirs_only)) return false; 
    wxBusyCursor busy;
    if (!DoAlteration(filepaths, DoWhich)) return false;  // Do the exciting bit in a tar or zip-specific method
  }
 else                                           // If arc_add, get the target dir for insertion into WithinArchivename.  I've overloaded newnames to contain its full filepath
  { if (newnames.IsEmpty())   return false; 
    wxString dest(newnames[0]);
    if (SortOutNames(dest))                     // Sortoutnames also fills archivename. Don't test for success: if we're pasting directly onto the archive, it'll return false
      if (!IsDir) WithinArchiveName = WithinArchiveName.BeforeLast(wxFILE_SEP_PATH) + wxFILE_SEP_PATH;  // If we're pasting onto a file, remove the filename
    wxBusyCursor busy;
    if (!DoAlteration(filepaths, DoWhich, newnames[1], dirs_only)) return false;  // Do the exciting bit in a tar or zip-specific method
  }

size_t size = OutStreamPtr->GetSize();          // This is how much was loaded i.e. the file size
wxChar* buf = new wxChar[ size ];               // It's ugly, but the easiest way I've found to get the data from the stream to a memorybuffer
((wxMemoryOutputStream*)OutStreamPtr.get())->CopyTo(buf, size);  // is by using an intermediate char[]
OutStreamPtr.reset();                           // Delete memoutstream

delete m_membuf; m_membuf = new wxMemoryBuffer(size); // I've no idea why, but trying to delete the old data by SetDataLen(0) failed :(
m_membuf->AppendData(buf, size);                // Store the data
delete[] buf;

SaveBuffer();

ffs->Clear();                                   // Throw out the old dir structure, and reload
ListContentsFromBuffer(archivename);

return true;
}

bool ArchiveStream::MovePaste(MyGenericDirCtrl* originctrl, MyGenericDirCtrl* destctrl, wxArrayString& filepaths, wxString& destpath, bool Moving/*=true*/, bool dirs_only/*=false*/)  
{
if (originctrl == NULL || destctrl == NULL || filepaths.IsEmpty() || destpath.IsEmpty()) return false;

bool PastingFromTrash = filepaths[0].StartsWith(DirectoryForDeletions::GetDeletedName());  // We'll need this if we'd Cut from an archive ->trash; & now we're Pasting it
bool FromArchive = ! PastingFromTrash && (originctrl->arcman != NULL && originctrl->arcman->IsArchive()); // If we're PastingFromTrash, originctrl->->IsArchive() would be irrelevantly true 
bool ToArchive = (destctrl->arcman != NULL && destctrl->arcman->IsArchive());

wxString OriginPath(filepaths[0]);                                  // We'll need this later
OriginPath = OriginPath.BeforeLast(wxFILE_SEP_PATH) + wxFILE_SEP_PATH;
  
wxArrayInt IDs; IDs.Add(destctrl->GetId());                         // Make an int array, & store the ID of destination pane
if (Moving) IDs.Add(originctrl->GetId());                         // If Moving, add the origin ID

if (FromArchive && ! ToArchive)
  { FileData dp(destpath); if (!dp.IsValid()) return false;     // Avoid pasting onto a file
    if (!dp.IsDir() && ! (dp.IsSymlink() && dp.IsSymlinktargetADir()))
      destpath = dp.GetPath();

    FileData DestinationFD(destpath);
    if (!DestinationFD.CanTHISUserWriteExec())                      // Make sure we have permission to write to the destination dir
      { wxMessageDialog dialog(destctrl, _("I'm afraid you don't have Permission to write to this Directory"), _("No Entry!"), wxOK | wxICON_ERROR);
        dialog.ShowModal(); return false;
      }

    enum DupRen WhattodoifClash = DR_Unknown, WhattodoifCantRead = DR_Unknown;  // These get passed to CheckDupRen.  May become SkipAll, OverwriteAll etc, but start with Unknown
    size_t filecount = filepaths.GetCount(); wxArrayString fpaths;  // fpaths will hold the filepaths once they've been checked/amended etc
    for (size_t n=0; n < filecount; ++n)                            // For every entry in the clipboard
      { bool ItsADir = false;                                       // We need to check we're not about to do something illegal
        if (WhattodoifClash==DR_Rename || WhattodoifClash==DR_Overwrite || WhattodoifClash==DR_Skip || WhattodoifClash==DR_Store)
            WhattodoifClash = DR_Unknown;                           // Remove any stale single-shot choice
        FakeFiledata* fd = Getffs()->GetRootDir()->FindFileByName(filepaths[n]);  // Dirs aren't '/'-terminated, so just try it and see
        if (fd==NULL)  
          { fd = Getffs()->GetRootDir()->FindSubdirByFilepath(filepaths[n]); ItsADir = true; }  // Not a file, so try for a dir
        if (fd==NULL) continue;
        CheckDupRen CheckIt(destctrl, filepaths[n], destpath, true, fd->ModificationTime());  // Make an instance of the class that tests for this. The 'true' means "From archive"
        CheckIt.IsMultiple = (filecount > 1);                       // We use different dialogs for multiple items
        CheckIt.WhattodoifClash = WhattodoifClash; CheckIt.WhattodoifCantRead = WhattodoifCantRead; CheckIt.ItsADir = ItsADir;  // Set CheckIt's variables to match any previous choice
        if (!CheckIt.CheckForPreExistence())                        // Check we're not pasting onto ourselves or a namesake. True means OK
          { WhattodoifClash = CheckIt.WhattodoifClash;              // Store any new choice in local variable.  This catches SkipAll & Cancel, which return false
            if (WhattodoifClash==DR_Cancel) { n = filecount; break; }  // Cancel this item & the rest of the loop by increasing n
             else continue;                                         // Skip this item but continue with loop
          }
         else 
          { WhattodoifClash = CheckIt.WhattodoifClash;              // Needed to cache any DR_OverwriteAll
            fpaths.Add(filepaths[n]);  // If there was an unskipped clash, this will effect an overwrite. Rename isn't possible ex archive (ie it isn't worth the bother of altering everything to implement it)
          }
      }
    filepaths = fpaths;                                   // Revert to using filepaths. This is a reference, so the caller can use it to count the items actually done
    if (!filepaths.GetCount()) return false;              // If everything was skipped/cancelled, go home

    bool ClusterWasNeeded = UnRedoManager::StartClusterIfNeeded();
    wxArrayString destinations;
    originctrl->arcman->GetArc()->DoExtract(filepaths, destpath, destinations, Moving, dirs_only); // The 'Moving' says don't create an Undo for this; but for a paste we need one 
    
    if (Moving)                                           // If we're Moving (cf Pasting), delete those files from the archive
      { wxArrayString unused; originctrl->arcman->GetArc()->Alter(filepaths, arc_remove, unused);
        UnRedoMoveToFromArchive* UnRedoptr = new UnRedoMoveToFromArchive(filepaths, destinations, OriginPath, destpath, IDs, originctrl, destctrl, amt_from);
        UnRedoManager::AddEntry(UnRedoptr);
        MyFrame::mainframe->OnUpdateTrees(OriginPath, IDs, wxT(""), true);
      }

    if (ClusterWasNeeded) UnRedoManager::EndCluster();
    return true;
  }

if (ToArchive && !FromArchive)
  { // First check that we have exec permissions for the origin dir
    FileData OriginFileFD(filepaths[0]);
    if (!OriginFileFD.CanTHISUserPotentiallyCopy())
      { wxMessageDialog dialog(destctrl, _("I'm afraid you don't have permission to access files from this Directory"),  _("No Exit!")); return false; }
      
    enum DupRen WhattodoifClash = DR_Unknown, WhattodoifCantRead = DR_Unknown;  // These get passed to CheckDupRen.  May become SkipAll, OverwriteAll etc, but start with Unknown
    size_t filecount = filepaths.GetCount(); wxArrayString Unskippedfpaths;     // Some filepaths may clash and be skipped; store the used ones here
    wxArrayString fpaths;  // fpaths will hold the filepaths once they've been checked/amended etc
    wxArrayInt DupRens;    // This holds the 'WhattodoifClash' responses, which might be 'dup' for one item, 'overwrite' for another
    for (size_t n=0; n < filecount; ++n)                        // For every entry in the clipboard
      { bool ItsADir = false;                                   // We need to check we're not about to do something illegal
        if (WhattodoifClash==DR_Rename || WhattodoifClash==DR_Overwrite || WhattodoifClash==DR_Skip || WhattodoifClash==DR_Store)
            WhattodoifClash = DR_Unknown;                       // Remove any stale single-shot choice
        FileData fd(filepaths[n]); ItsADir = fd.IsDir();
        CheckDupRen CheckIt(destctrl, filepaths[n], destpath);
        CheckIt.IsMultiple = (filecount > 1);                   // We use different dialogs for multiple items
        CheckIt.WhattodoifClash = WhattodoifClash; CheckIt.WhattodoifCantRead = WhattodoifCantRead; CheckIt.ItsADir = ItsADir;  // Set CheckIt's variables to match any previous choice
        int result = CheckIt.CheckForPreExistenceInArchive();   // Check we're not pasting onto ourselves or a namesake. True means OK
        if (!result)                                            // false means skip or cancel
          { WhattodoifClash = CheckIt.WhattodoifClash;          // Store any new choice in local variable.  This catches SkipAll & Cancel, which return false
            if (WhattodoifClash==DR_Cancel)   { n = filecount; break; }  // Cancel this item & the rest of the loop by increasing n
             else continue;                                     // Skip this item but continue with loop
          }
         else 
          { WhattodoifClash = CheckIt.WhattodoifClash;          // Store any new choice in local variable; it'll be fed back in the next iteration of the loop
            // 'result' may be 1, in which case there wasn't a clash; or 2, in which case use WhattodoifClash to flag what to do to this item
            enum DupRen Whattodothistime = (result==1) ? DR_Unknown : WhattodoifClash;
            if (!ItsADir)
              { Unskippedfpaths.Add(filepaths[n]);
                fpaths.Add(filepaths[n]);       // If there was an unskipped clash, this will effect a duplication or overwrite, depending on the DupRen returned
                DupRens.Add(Whattodothistime);  // So store that in the corresponding position in this array
              }
             else // For a dir, we need to go thru its filepaths to add its children and any subdirs
              { Unskippedfpaths.Add(filepaths[n]);
                fpaths.Add(filepaths[n]); DupRens.Add(Whattodothistime); // First do the dir itself
                wxArrayString tempfpaths; RecursivelyGetFilepaths(filepaths[n], tempfpaths); // The contents of any dirs are now in tempfpaths
                for (size_t c=0; c < tempfpaths.GetCount(); ++c)         // Go through these, storing the overwrite/add info too
                  { fpaths.Add(tempfpaths.Item(c)); DupRens.Add(Whattodothistime); }
              }
          }
      }
    filepaths = Unskippedfpaths;
    if (!filepaths.GetCount()) return false;                 // If everything was skipped/cancelled, go home


    FakeFiledata* ffd = destctrl->arcman->GetArc()->ffs->GetRootDir()->FindFileByName(destpath);  // Destpath doesn't have a '/', so we need to see if it's a file and amputate
    if (ffd != NULL) destpath = destpath.BeforeLast(wxFILE_SEP_PATH);
    if (destpath.Last() != wxFILE_SEP_PATH) destpath << wxFILE_SEP_PATH;

    // Some of the new items may clash with existing ones; and the user may (should!) have decided to overwrite these
    // If so, we need to extract-to-trash the existing items so that they can be unredone, then remove them from the archive
    wxArrayString deletions;
    for (size_t n=0; n < DupRens.GetCount(); ++n)            // Start by seeing if the problem exists
      if ((DupRens[n] == DR_Overwrite) || (DupRens[n] == DR_OverwriteAll))
        { wxString distal; if (!fpaths[n].StartsWith(OriginPath, &distal))  continue;
          deletions.Add(destpath + distal);
        }

    bool ClusterWasNeeded = UnRedoManager::StartClusterIfNeeded();

    if (!deletions.IsEmpty())
      { wxFileName trashdirbase;                        // Create a unique subdir in trashdir, using current date/time
        if (!DirectoryForDeletions::GetUptothemomentDirname(trashdirbase, delcan))
          { wxMessageBox(_("For some reason, trying to create a dir to receive the backup failed.  Sorry!")); if (ClusterWasNeeded) UnRedoManager::EndCluster(); return false; }
        wxArrayString destinations, unused;
        wxString trashpath(trashdirbase.GetPath());
        if (!destctrl->arcman->GetArc()->DoExtract(deletions, trashpath, destinations, true, dirs_only))  // Extract the files to the bin. The true means don't unredo: we'll do it here
          { wxMessageDialog dialog(destctrl, _("Sorry, backing up failed"), _("Oops!"), wxOK | wxICON_ERROR); if (ClusterWasNeeded) UnRedoManager::EndCluster(); return false; }
        UnRedoCutPasteToFromArchive* UnRedoptr = new UnRedoCutPasteToFromArchive(deletions, destinations, destpath, trashpath, IDs, destctrl, originctrl, amt_from);
        UnRedoManager::AddEntry(UnRedoptr);

        if (!destctrl->arcman->GetArc()->Alter(deletions, arc_remove, unused))                  // Then remove from the archive
          { wxMessageDialog dialog(destctrl, _("Sorry, removing items failed"), _("Oops!"), wxOK | wxICON_ERROR); if (ClusterWasNeeded) UnRedoManager::EndCluster(); return false; }
      }

    wxArrayString Path;                                 // We have to provide a 2nd arraystring anyway due to the Alter API, so use it to transport the path
    Path.Add(destpath);
    wxString originpath = OriginFileFD.GetPath();       // We also need to know the 'root' of the origin path
    if (originpath.Last() != wxFILE_SEP_PATH) originpath << wxFILE_SEP_PATH;
    Path.Add(originpath);                               // This too will kludgily pass

    if (!destctrl->arcman->GetArc()->Alter(fpaths, arc_add, Path, dirs_only))  // Do the Paste bit of the Move
          { if (ClusterWasNeeded) UnRedoManager::EndCluster(); return false; }

    wxArrayString destinations;
	bool needsyield(false);
    for (size_t n=0; n < filepaths.GetCount(); ++n)     // Do the delete bit
      { if (Moving)
          { wxFileName From(filepaths[n]);
            originctrl->ReallyDelete(&From);
#if wxVERSION_NUMBER > 2812 && (wxVERSION_NUMBER < 3003 || wxVERSION_NUMBER == 3100)
			// Earlier wx3 version have a bug (http://trac.wxwidgets.org/ticket/17122) that can cause segfaults if we SetPath() on returning from here
			if (From.IsDir()) needsyield = true; // Yielding works around the problem by altering the timing of incoming fswatcher events
#endif
          }  
        wxString distal; if (!filepaths[n].StartsWith(OriginPath, &distal))  continue;
        destinations.Add(destpath + distal);            // Fill destinations array ready for UnRedoing
      }
    if (needsyield) wxSafeYield();

    if (Moving)
      { UnRedoMoveToFromArchive* UnRedoptr = new UnRedoMoveToFromArchive(filepaths, destinations, OriginPath, destpath, IDs, originctrl, destctrl, amt_to);
        UnRedoManager::AddEntry(UnRedoptr);  
        MyFrame::mainframe->OnUpdateTrees(OriginPath, IDs, wxT(""), true);
      }
     else                      // Move would have done the Paste Unredo bit too. Since we're not Moving, do it here
      { UnRedoArchivePaste* UnRedoptr = new UnRedoArchivePaste(fpaths, destinations, OriginPath, destpath, IDs, destctrl, _("Paste"), dirs_only);
        UnRedoManager::AddEntry(UnRedoptr);
      }
    if (ClusterWasNeeded) UnRedoManager::EndCluster();

    MyFrame::mainframe->OnUpdateTrees(destpath, IDs, wxT(""), true);
    return true;
  }

  // If we're here, we are moving/pasting from one archive to another, which might be:
  // Within the same archive in the same pane                --- Anything that's really a Rename/Dup happens in DoThingsWithinSameArchive(). This is for moving/pasting between (sub)dirs
  // Within the same archive displayed in different panes    --- Ditto, then reload the other one (as otherwise the 2 will be out of sync)
  // Between different archives displayed in different panes  --- Extract to a tempfile, then add to the 2nd archive. If moving, delete from the 1st
if (originctrl->arcman->GetArc()->Getffs()->GetRootDir()->GetFilepath()       //  Within the same archive in the same or different panes, so Rename
                                      == destctrl->arcman->GetArc()->Getffs()->GetRootDir()->GetFilepath())
  { wxArrayString newpaths;    
    FakeFiledata* ffd = ffs->GetRootDir()->FindFileByName(destpath);  // Destpath doesn't have a '/', so we need to see if it's a file and amputate. The '/' gets added later
    if (ffd) destpath = destpath.BeforeLast(wxFILE_SEP_PATH);
    
    if (destpath.Last() != wxFILE_SEP_PATH) destpath << wxFILE_SEP_PATH;
    for (size_t n=0; n < filepaths.GetCount(); ++n)                   // Go thru the filepaths, changing the paths
      { wxString filename;
        if (filepaths.Item(n).Last() != wxFILE_SEP_PATH) filename = filepaths.Item(n).AfterLast(wxFILE_SEP_PATH);
          else  filename = (filepaths.Item(n).BeforeLast(wxFILE_SEP_PATH)).AfterLast(wxFILE_SEP_PATH) + wxFILE_SEP_PATH; // If it's a dir, manoevre round the terminal '/'
        newpaths.Add(destpath + filename);
      }

    int whattodoifclash = XRCID("DR_Unknown");
    for (size_t n=newpaths.GetCount(); n > 0; --n)                    // Do a poor-man's CheckIt.WhattodoifClash
      { if (ffs->GetRootDir()->FindFileByName(newpaths.Item(n-1))
            || ffs->GetRootDir()->FindSubdirByFilepath(newpaths.Item(n-1)))
          { // It's difficult/impossible to cope with a clash when moving between archives, so don't try
            if (whattodoifclash != XRCID("SkipAll"))
              { MyButtonDialog dlg; wxXmlResource::Get()->LoadDialog(&dlg, destctrl, wxT("PoorMansClashDlg"));
                wxString distal;
                if (!newpaths.Item(n-1).StartsWith(destpath+wxFILE_SEP_PATH, &distal))  distal = newpaths.Item(n-1); // Try to show only the in-archive bit of the fpath
                ((wxStaticText*)dlg.FindWindow(wxT("Filename")))->SetLabel(distal);
                dlg.GetSizer()->Fit(&dlg);
                whattodoifclash = dlg.ShowModal();  // whattodoifclash might now hold Skip, SkipAll or Cancel
                if (whattodoifclash == XRCID("Cancel")) return false;
              }
            newpaths.RemoveAt(n-1); filepaths.RemoveAt(n-1);
          }
      }
    if (filepaths.IsEmpty()) return false;

    if (!destctrl->arcman->GetArc()->Alter(filepaths, Moving ? arc_rename : arc_dup, newpaths, dirs_only))  // Do the rename. NB we use the destination pane here, and (maybe) reload the origin
        return false;
    
    if (originctrl != destctrl && originctrl != destctrl->partner)    // If we have 2 editions of the  same archive open, reload the origin one
      { archivename = ffs->GetRootDir()->GetFilepath(); archivename = archivename.BeforeLast(wxFILE_SEP_PATH);
        originctrl->arcman->GetArc()->Getffs()->Clear();              // Throw out the old dir structure, and reload
        if (!IsNested())
          { if (!archivename.IsEmpty())  originctrl->arcman->GetArc()->LoadToBuffer(archivename); }
         else
          originctrl->arcman->GetArc()->LoadToBuffer(destctrl->arcman->GetArc()->GetBuffer());  // If we're in a nested buffer situation, copy the updated buffer into the out-of-date one
        originctrl->arcman->GetArc()->ListContentsFromBuffer(archivename);
      }

    bool ClusterWasNeeded(false);
    if (Moving)  ClusterWasNeeded = UnRedoManager::StartClusterIfNeeded();
    wxArrayInt IDs; IDs.Add(originctrl->GetId());
    bool IdenticalArc = (originctrl->arcman->GetArc()->Getffs() == destctrl->arcman->GetArc()->Getffs()); // Are we moving/pasting to a subdir, within the same pane?
    UnRedoArchiveRen* UnRedoptr  = new UnRedoArchiveRen(filepaths, newpaths, IDs, destctrl, 
                                          Moving==false, IdenticalArc ? NULL : originctrl, dirs_only); // Moving==false == not duplicating. Passing destctrl says 'Only if still visible'
    UnRedoManager::AddEntry(UnRedoptr);

    if (Moving) 
      { if (ClusterWasNeeded) UnRedoManager::EndCluster();
        MyFrame::mainframe->OnUpdateTrees(OriginPath, IDs, wxT(""), true);
      }
    MyFrame::mainframe->OnUpdateTrees(destpath, IDs, wxT(""), true);
    return true;  
  }

 else                              //  Otherwise we're going between different archives
  { wxArrayString destinations, newpaths;    wxString dest(destpath);
    FakeFiledata* ffd = destctrl->arcman->GetArc()->ffs->GetRootDir()->FindFileByName(dest); // Destpath doesn't have a '/', so we need to see if it's a file and amputate
    if (ffd != NULL) dest = dest.BeforeLast(wxFILE_SEP_PATH);
    if (dest.Last() != wxFILE_SEP_PATH) dest << wxFILE_SEP_PATH;

    
    for (size_t n=0; n < filepaths.GetCount(); ++n)                   // Go thru the filepaths, changing the paths
      { wxString filename;
        if (filepaths.Item(n).Last() != wxFILE_SEP_PATH) filename = filepaths.Item(n).AfterLast(wxFILE_SEP_PATH);
          else  filename = (filepaths.Item(n).BeforeLast(wxFILE_SEP_PATH)).AfterLast(wxFILE_SEP_PATH) + wxFILE_SEP_PATH; // If it's a dir, manoevre round the terminal '/'
        newpaths.Add(dest + filename);
      }

    FakeDir* droot = destctrl->arcman->GetArc()->Getffs()->GetRootDir();
    int whattodoifclash = XRCID("DR_Unknown");
    for (size_t n=newpaths.GetCount(); n > 0; --n)                    // Do a poor-man's CheckIt.WhattodoifClash
      { if (droot->FindFileByName(newpaths.Item(n-1))
            || droot->FindSubdirByFilepath(newpaths.Item(n-1)))
          { // It's difficult/impossible to cope with a clash when moving between archives, so don't try
            if (whattodoifclash != XRCID("SkipAll"))
              { MyButtonDialog dlg; wxXmlResource::Get()->LoadDialog(&dlg, destctrl, wxT("PoorMansClashDlg"));
                wxString distal;
                if (!newpaths.Item(n-1).StartsWith(dest, &distal))  distal = newpaths.Item(n-1); // Try to show only the in-archive bit of the fpath
                ((wxStaticText*)dlg.FindWindow(wxT("Filename")))->SetLabel(distal);
                dlg.GetSizer()->Fit(&dlg);
                whattodoifclash = dlg.ShowModal();  // whattodoifclash might now hold Skip, SkipAll or Cancel
                if (whattodoifclash == XRCID("Cancel")) return false;
              }
            newpaths.RemoveAt(n-1); filepaths.RemoveAt(n-1);
          }
      }
    if (!filepaths.GetCount()) return false;            // If everything was skipped/cancelled, go home

    wxArrayString tempfiles = originctrl->arcman->ExtractToTempfile(filepaths, dirs_only);  // This extracts the selected filepaths to temporary files, which it returns
    if (tempfiles.IsEmpty()) return false;
  
    wxArrayString Path; Path.Add(dest);                 // We have to provide a 2nd arraystring anyway due to the Alter API, so use it to transport the path
    FileData tmppath(tempfiles[0]);
    wxString tfileoriginpath = tmppath.GetPath();       // We also need to know the 'root' of the tempfile path
    if (tfileoriginpath.Last() != wxFILE_SEP_PATH) tfileoriginpath << wxFILE_SEP_PATH;
    Path.Add(tfileoriginpath);                          // This too will kludgily pass

    if (!destctrl->arcman->GetArc()->Alter(tempfiles, arc_add, Path, dirs_only)) return false;  // Do the Paste bit of the Move

    for (size_t n=0; n < tempfiles.GetCount(); ++n)     // Fill destinations array ready for UnRedoing
      { wxString distal; if (!tempfiles[n].StartsWith(tfileoriginpath, &distal))  continue;
        destinations.Add(dest + distal);
      }

    bool ClusterWasNeeded = UnRedoManager::StartClusterIfNeeded();

    if (Moving)                                         // If we're Moving (cf Pasting), delete those files from the archive
      { wxArrayString unused; originctrl->arcman->GetArc()->Alter(filepaths, arc_remove, unused);
        
        UnRedoArchiveDelete* UnRedoptr = new UnRedoArchiveDelete(filepaths, tempfiles, OriginPath, tfileoriginpath, IDs, originctrl, _("Move"));
        UnRedoManager::AddEntry(UnRedoptr);

        MyFrame::mainframe->OnUpdateTrees(OriginPath, IDs, wxT(""), true);
      }

    UnRedoArchivePaste* UnRedoptr = new UnRedoArchivePaste(tempfiles, destinations, tfileoriginpath, dest, IDs, destctrl, Moving ? _("Move") : _("Paste"), dirs_only);
    UnRedoManager::AddEntry(UnRedoptr);
    if (ClusterWasNeeded) UnRedoManager::EndCluster();
    MyFrame::mainframe->OnUpdateTrees(destpath, IDs, wxT(""), true);
    return true;
  }
}

    // Abstracted from MyGenericDirCtrl::OnDnDMove and OnPaste to cope with renaming-by-Paste or D'n'D within an archive
bool ArchiveStream::DoThingsWithinSameArchive(MyGenericDirCtrl* originctrl, MyGenericDirCtrl* destctrl, wxArrayString& filearray, wxString& destinationpath)
{
if (!filearray.GetCount()) return false; // Shouldn't happen

bool OrigIsArchive = originctrl && originctrl->arcman && originctrl->arcman->IsArchive();
bool DestIsArchive = destctrl && destctrl->arcman && destctrl->arcman->IsArchive();
bool WithinSameArchive = (OrigIsArchive && DestIsArchive &&
                      (originctrl->arcman->GetArc()->Getffs()->GetRootDir()->GetFilepath() == destctrl->arcman->GetArc()->Getffs()->GetRootDir()->GetFilepath()));
if (!WithinSameArchive) return false;

  // Try to distinguish between pasting within the same dir, and pasting to elsewhere in the archive
wxString dest(destinationpath);
if (destctrl->arcman->GetArc()->Getffs()->GetRootDir()->FindFileByName(dest))
  dest = dest.BeforeLast(wxFILE_SEP_PATH);  // If it's a file, amputate

wxString lastseg = wxFILE_SEP_PATH, originpath = filearray.Item(0);
if (originctrl->arcman->GetArc()->Getffs()->GetRootDir()->FindFileByName(originpath))
  { originpath = originpath.BeforeLast(wxFILE_SEP_PATH);  // If it's a file, amputate
    if (dest != originpath) return false;     // If we're moving/pasting from arc/foo to arc/foo/bar, do it the normal way
  }
 else 
  { if (!originctrl->arcman->GetArc()->Getffs()->GetRootDir()->FindSubdirByFilepath(originpath)) return false; // Not a file, not a dir...
    lastseg << originpath.AfterLast(wxFILE_SEP_PATH);
    if (dest+lastseg != originpath) return false;     // If we're not moving/pasting e.g. arc/foo to arc/, do it the normal way
  }

// If we're Moving/Pasting to the same dir within the same archive (perhaps open on different panes), ask if the user wants to rename
MyButtonDialog dlg; wxXmlResource::Get()->LoadDialog(&dlg, destctrl, wxT("QuerySameArchiveRenameDlg"));
int result = dlg.ShowModal();
if (result == XRCID("Rename"))
  {  if (filearray.GetCount() > 1)
        { destctrl->DoMultipleRename(filearray, true); }  // Piggyback on the Rename code
      else
        { wxString filepath(filearray.Item(0));           // Use the single-selection version
          destctrl->DoRename(true, filepath); 
        }

    wxString archivename = originctrl->arcman->GetArc()->Getffs()->GetRootDir()->GetFilepath(); archivename = archivename.BeforeLast(wxFILE_SEP_PATH);
    destctrl->arcman->GetArc()->Getffs()->Clear();        // Throw out the old dir structure, and reload
    if (!destctrl->arcman->GetArc()->IsNested())
      { if (!archivename.IsEmpty())    destctrl->arcman->GetArc()->LoadToBuffer(archivename); }
     else
        destctrl->arcman->GetArc()->LoadToBuffer(destctrl->arcman->GetArc()->GetBuffer());  // If we're in a nested buffer situation, copy the updated buffer into the out-of-date one
    destctrl->arcman->GetArc()->ListContentsFromBuffer(archivename);

    wxArrayInt IDs; IDs.Add(originctrl->GetId());
    MyFrame::mainframe->OnUpdateTrees(archivename, IDs, wxT(""), true);
    return true;
  }
return true; // Otherwise the user presumably pressed Cancel, but still return true to signify that nothing more needs be done
}

//---------------------------------------------------------------------------------
wxDEFINE_SCOPED_PTR(wxInputStream, wxInputStreamPtr)
wxDEFINE_SCOPED_PTR(wxOutputStream, wxOutputStreamPtr)
wxDEFINE_SCOPED_PTR_TYPE(wxTarEntry);
wxDEFINE_SCOPED_PTR_TYPE(wxZipEntry);

ZipArchiveStream::ZipArchiveStream(DataBase* archive)  :  ArchiveStream(archive)  // Opens a 'real' archive, not a nested one
{
Valid = false;
if (!archive->IsValid()) return;

wxBusyCursor busy;
if (!LoadToBuffer(archive->GetFilepath())) return;  // Load the wxMemoryBuffer with the archive

Valid = true;
}

ZipArchiveStream::ZipArchiveStream(wxMemoryBuffer* membuf, wxString archivename)  :  ArchiveStream(archivename)  // Opens a 'nested' archive
{
Valid = false;
if (archivename.IsEmpty()) return;

m_membuf = membuf;
Valid = true;
}

bool ZipArchiveStream::Extract(wxArrayString& filepaths, wxString destpath, wxArrayString& resultingfilepaths, bool dirs_only/*=false*/, bool files_only/*=false*/)   // Extract filepath from the archive
{
if (filepaths.IsEmpty() || destpath.IsEmpty()) return false;
  // AdjustFilepaths puts each filepath into WithinArchiveNames, less the /path-to-archive/archive; & creates its dest in WithinArchiveNewNames. If a dir, recursively adds into children too
if (!AdjustFilepaths(filepaths, destpath, resultingfilepaths, afp_extract, dirs_only, files_only)) return false;

wxZipEntryPtr entry;
wxMemoryInputStream meminstream(m_membuf->GetData(), m_membuf->GetDataLen());  // The archive is in the memory buffer m_membuf. 'Extract' it to a memory stream
wxZipInputStream zip(meminstream);

size_t NumberFound = 0;

while (entry.reset(zip.GetNextEntry()), entry.get() != NULL)
  { int item = WithinArchiveNames.Index(entry->GetName());  // See if this file is one of the ones we're interested in
    if (item == wxNOT_FOUND)  continue;
                // Found it. If it's a file, extract it. If a dir,  make a new dir with this name (*outside* the archive ;)
    wxLogNull log;
    if (!entry->IsDir())                                    // (For a dir there's no data to extract)
      { wxString Path(resultingfilepaths[ item ]); Path = Path.BeforeLast(wxFILE_SEP_PATH);
        if (!wxFileName::Mkdir(Path, 0777, wxPATH_MKDIR_FULL)) return false;  // Make any necessary dir

        wxFFileOutputStream out(resultingfilepaths[ item ]);
        if (!out || ! out.Write(zip) || ! zip.Eof())   return false;  // and extract into the new filepath
        FileData fd(resultingfilepaths[ item ]); if (fd.IsValid()) fd.DoChmod(entry->GetMode());    // Correct the extracted file's permissions
      }
     else if (!wxFileName::Mkdir(resultingfilepaths[ item ], 0777, wxPATH_MKDIR_FULL)) return false;  // Just create the dir
     
     if (++NumberFound >= WithinArchiveNames.GetCount()) return true; // See if we've used all the passed filepaths
  }

return false;
}

bool ZipArchiveStream::DoAlteration(wxArrayString& filepaths, enum alterarc DoWhich, const wxString& originroot/*=wxT("")*/, bool dirs_only/*=false*/)  // Implement add remove del or rename, depending on the enum
{
wxInputStreamPtr dupptr;
GetFromBuffer();                               // The archive is in the memory buffer m_membuf. 'Extract' it to InStreamPtr
if (DoWhich==arc_dup)                          // If we're duplicating, we need a 2nd instream, so store the first and re-extract
  { dupptr.reset(InStreamPtr.release()); GetFromBuffer(true); }

wxMemoryOutputStream* memoutstream = new wxMemoryOutputStream;
wxZipOutputStream outzip(*memoutstream);

outzip.CopyArchiveMetaData(*(wxZipInputStream*)InStreamPtr.get());

wxZipEntryPtr entry,  dupentry;
while (entry.reset(((wxZipInputStream*)InStreamPtr.get())->GetNextEntry()), entry.get() != NULL)      // Go thru the archive, looking for the selected files.
  { int item = WithinArchiveNames.Index(entry->GetName());     // See if this file is one of the ones we're interested in
    switch(DoWhich)
      { case arc_add:      break;                              // Add happens later
        case arc_remove:  if (item != wxNOT_FOUND)  continue;  // If this file is one of the 2b-removed ones, ignore it, don't add it to the new archive
                          else break;                          // Otherwise break to copy the entry
        case arc_dup:      dupentry.reset(((wxZipInputStream*)dupptr.get())->GetNextEntry());  // For dup, get the next entry, then fall thru to rename
        case arc_rename:  if (item != wxNOT_FOUND)             // For rename, change the name then break to copy the entry
                          { if (entry->IsDir() && WithinArchiveNewNames[ item ].Last() != wxFILE_SEP_PATH) WithinArchiveNewNames[ item ] << wxFILE_SEP_PATH;
                            entry->SetName(WithinArchiveNewNames[ item ]); 
                          } 
                        break;
                          
        case arc_del:      continue;
      }
                // Copy this entry to the outstream
    if (!entry->IsDir())
      { if (!outzip.CopyEntry(entry.release(), *(wxZipInputStream*)InStreamPtr.get())) return false; // CopyEntry is specific to archive/zip-outputsteams, which is why I'm using one here
        if (DoWhich==arc_dup && item != wxNOT_FOUND)           // If we're duplicating, we now need to output the original version to the stream
           if (!outzip.CopyEntry(dupentry.release(), *(wxZipInputStream*)dupptr.get())) return false;
      }
     else 
      { if (!outzip.PutNextDirEntry(entry->GetName(), entry->GetDateTime())) return false; // If it's a dir, this just writes the 'header' info; there isn't any data
        entry.release();
        if (DoWhich==arc_dup && item != wxNOT_FOUND)           // If we're duplicating, we now need to output the original version to the stream
          { if (!outzip.PutNextDirEntry(dupentry->GetName(), dupentry->GetDateTime())) return false; dupentry.release(); }
      }
  }  

if (DoWhich == arc_add)    // If we're trying to add, we've just copied all the old entries. Now add new the files to the within-archive dir WithinArchiveName
  { bool success = false;
    wxString path =  filepaths[0].BeforeLast(wxFILE_SEP_PATH) + wxFILE_SEP_PATH;  // The first item *must* exist. Use it to find the curent path of the entries: which will be amputated
    for (size_t n=0; n < filepaths.GetCount(); ++n)
      { FileData fd(filepaths[n]); if (!fd.IsValid()) continue;
        wxString Name; if (!filepaths[n].StartsWith(path, &Name))  continue; // Get what's after path into Name: if we've adding  dir foo, path==../path/, Name will be /foo or /foo/bar
        Name = WithinArchiveName + Name; wxDateTime DT(fd.ModificationTime());
        if (fd.IsDir())
          { success = outzip.PutNextDirEntry(Name, DT); continue; }          // Dirs don't have data, so just provide the filepath

        if (dirs_only) continue;  // If we're pasting a dir skeleton, skip the non-dirs code below

        wxFFileInputStream file(filepaths[n]); if (!file.Ok()) continue;     // Make a wxFFileInputStream to get the data to be added
        wxZipEntry* addentry = new wxZipEntry(Name, DT, file.GetSize()); addentry->SetSystemMadeBy(wxZIP_SYSTEM_UNIX); addentry->SetMode(fd.GetPermissions());
        if (!outzip.PutNextEntry(addentry)                     // and add it to the outputstream
                        || ! outzip.Write(file) || ! file.Eof())    continue;
        success = true;                                        // Flag that we've at least one successful addition
      }
    if (!success) return false;                                // No point proceeding if nothing worked
  }

if (!outzip.Close()) return false;
if (!memoutstream->Close()) return false;

OutStreamPtr.reset(memoutstream);  // This has a kludgey feel, but it returns the memoutstream to Alter() via the member scoped ptr
return true;
}

wxMemoryBuffer* ZipArchiveStream::ExtractToBuffer(wxString filepath)  // Extract filepath from this archive into a new wxMemoryBuffer
{
if (filepath.IsEmpty()) return NULL;

if (!SortOutNames(filepath)) return NULL;

wxZipEntryPtr entry;

wxBusyCursor busy;
wxMemoryInputStream meminstream(m_membuf->GetData(), m_membuf->GetDataLen());  // The archive is in the memory buffer m_membuf. 'Extract' it to a memory stream
wxZipInputStream zip(meminstream);

while (entry.reset(zip.GetNextEntry()), entry.get() != NULL)
  { if (entry->GetName() != WithinArchiveName)  continue;       // Go thru the archive, looking for the desired file
                // Found it. Extract it to a new buffer
    wxMemoryOutputStream memoutstream;
    if (!memoutstream || ! memoutstream.Write(zip))   return NULL;  

    size_t size = memoutstream.GetSize();                       // This is how much was loaded i.e. the file size
    wxChar* buf = new wxChar[ size ];                           // It's ugly, but the easiest way I've found to get the data from the stream to a memorybuffer
    memoutstream.CopyTo(buf, size);                             //   is by using an intermediate char[]

    wxMemoryBuffer* membuf = new wxMemoryBuffer(size);          // Now (at last) we can store the data in a memory buffer
    membuf->AppendData(buf, size);
    delete[] buf;
  
    return membuf;
  }

return NULL;
}

void ZipArchiveStream::ListContentsFromBuffer(wxString archivename)
{
wxBusyCursor busy;
GetFromBuffer();
ListContents(archivename);
}

void ZipArchiveStream::GetFromBuffer(bool dup)  // Get the stored archive from membuf into the stream
{
wxZipInputStream* zip;
wxMemoryInputStream* meminstream = new wxMemoryInputStream(m_membuf->GetData(), m_membuf->GetDataLen());  // The archive is in the memory buffer membuf. Stream it to a memory stream
if (!dup) 
  { MemInStreamPtr.reset(meminstream);
     zip = new wxZipInputStream(*MemInStreamPtr.get());         // Then pass it thru the filter
  }
 else
  { DupMemInStreamPtr.reset(meminstream);
     zip = new wxZipInputStream(*DupMemInStreamPtr.get());      // If we're duplicating, use DupMemInStreamPtr the second time around
  }

InStreamPtr.reset(zip);                                         // Use this member wxScopedPtr to 'return' the stream
}

void ZipArchiveStream::ListContents(wxString archivename)
{
        // Zip files don't contain absolute paths, just any subdirs, so get the bit to be prepended
if (archivename.GetChar(archivename.Len()-1) != wxFILE_SEP_PATH) archivename << wxFILE_SEP_PATH;

wxZipEntryPtr entry;

while (entry.reset(((wxZipInputStream*)InStreamPtr.get())->GetNextEntry()), entry.get() != NULL)
  { entry->SetSystemMadeBy(wxZIP_SYSTEM_UNIX);
    wxString name = archivename + entry->GetName();
    wxDateTime time = entry->GetDateTime();
    // read 'zip' to access the entry's data
    ffs->AddItem(name, entry->GetSize(), time.GetTicks(), entry->GetMode(), entry->IsDir() ? DIRTYPE : REGTYPE); // wxZipEntry can't cope with symlinks etc 
  }
}

void ZipArchiveStream::RefreshFromDirtyChild(wxString archivename, ArchiveStream* child)  // Used when a nested child archive has been altered; reload the altered version
{
if (archivename.IsEmpty()  || child == NULL) return;
wxMemoryInputStream  childstream(child->GetBuffer()->GetData(), child->GetBuffer()->GetDataLen());

GetFromBuffer();                                  // The archive is in the memory buffer m_membuf. 'Extract' it to InStreamPtr
wxMemoryOutputStream memoutstream;
wxZipOutputStream outzip(memoutstream);

outzip.CopyArchiveMetaData(*(wxZipInputStream*)InStreamPtr.get());

wxZipEntryPtr entry;
while (entry.reset(((wxZipInputStream*)InStreamPtr.get())->GetNextEntry()), entry.get() != NULL)    // Go thru the archive, looking for the selected files.
  {                 // Copy this entry to the outstream
    if (!entry->IsDir())
      { if (archivename == entry->GetName())        // If this is the archive we want to refresh
          { if (!outzip.PutNextEntry(entry->GetName(), entry->GetDateTime(), childstream.GetSize()) // ignore the original archive; instead add the new data
                        || ! outzip.Write(childstream) || ! childstream.Eof())    return;
          }
         else if (!outzip.CopyEntry(entry.release(), *(wxZipInputStream*)InStreamPtr.get())) return; // CopyEntry is specific to archive/zip-outputsteams, which is why I'm using one here

      }
     else 
      { if (!outzip.PutNextDirEntry(entry->GetName(), entry->GetDateTime())) return;   // If it's a dir, this just writes the 'header' info; there isn't any data
        entry.release();
      }
  }  

if (!outzip.Close()) return;
if (!memoutstream.Close()) return;

size_t size = memoutstream.GetSize();             // This is how much was loaded i.e. the file size
wxChar* buf = new wxChar[ size ];                 // It's ugly, but the easiest way I've found to get the data from the stream to a memorybuffer
memoutstream.CopyTo(buf, size);                   // is by using an intermediate char[]

delete m_membuf; m_membuf = new wxMemoryBuffer(size); // I've no idea why, but trying to delete the old data by SetDataLen(0) failed :(
m_membuf->AppendData(buf, size);                  // Store the data
delete[] buf;

SaveBuffer();                                     // We now need to save (or declare dirty) the parent arc
}

//-----------------------------------------------------------------------------------------------------------------------

TarArchiveStream::TarArchiveStream(DataBase* archive)  :  ArchiveStream(archive)  // Opens a 'real' archive, not a nested one
{
Valid = false;
if (!archive->IsValid()) return;

wxBusyCursor busy;
if (!LoadToBuffer(archive->GetFilepath())) return;  // Common code, so do this in base class. Loads the wxMemoryBuffer with the archive
Valid = true;
}

TarArchiveStream::TarArchiveStream(wxMemoryBuffer* membuf, wxString archivename)  :  ArchiveStream(archivename)  // Opens a 'nested' archive
{
Valid = false;
if (archivename.IsEmpty()) return;

m_membuf = membuf;
Valid = true;
}

void TarArchiveStream::ListContentsFromBuffer(wxString archivename)
{
wxBusyCursor busy;
GetFromBuffer();
ListContents(archivename);
}

void TarArchiveStream::ListContents(wxString archivename)
{
        // Tar files don't contain absolute paths, just any subdirs, so get the bit to be prepended
if (archivename.GetChar(archivename.Len()-1) != wxFILE_SEP_PATH) archivename << wxFILE_SEP_PATH;

wxTarEntryPtr entry;
while (entry.reset(((wxTarInputStream*)InStreamPtr.get())->GetNextEntry()), entry.get() != NULL)
  { wxString target;
    wxString name = archivename + entry->GetName();
    wxDateTime time = entry->GetDateTime();
    DB_filetype Type;  // wxTarEntry (cf. zipentry) stores the type of 'file' e.g. symlink. So make use of that
    switch(entry->GetTypeFlag())
      { case wxTAR_DIRTYPE: Type = DIRTYPE; break; case wxTAR_FIFOTYPE: Type = FIFOTYPE; break;
        case wxTAR_BLKTYPE: Type = BLKTYPE; break; case wxTAR_CHRTYPE: Type = CHRTYPE; break;
        case wxTAR_SYMTYPE: Type = SYMTYPE; target = entry->GetLinkName(); break; 
         default: Type = REGTYPE;  // Not only reg files, but hardlinks, ex-sockets & "contiguous files" (see wxTAR_CONTTYPE and http://www.gnu.org/software/tar/manual/html_node/Standard.html)
      }

    ffs->AddItem(name, entry->GetSize(), time.GetTicks(),  entry->GetMode(), Type, target);
  }
}

void TarArchiveStream::GetFromBuffer(bool dup)    // Get the stored archive from m_membuf into the stream
{
wxMemoryInputStream* meminstream = new wxMemoryInputStream(m_membuf->GetData(), m_membuf->GetDataLen());  // The archive is in the memory buffer membuf. Stream it to a memory stream
wxTarInputStream* tar;
if (!dup) 
  { MemInStreamPtr.reset(meminstream);
    tar = new wxTarInputStream(*MemInStreamPtr.get());
  }
 else
   { DupMemInStreamPtr.reset(meminstream);                    // If we're duplicating, use DupMemInStreamPtr the second time around
    tar = new wxTarInputStream(*DupMemInStreamPtr.get());
  }

InStreamPtr.reset(tar);                                       // Use this member wxScopedPtr to 'return' the stream
}

bool TarArchiveStream::CompressStream(wxOutputStream* outstream) // Pretend to compress this stream, ready to be put into membuf
{
OutStreamPtr.reset(outstream);  // Since we can't compress a plain vanilla tarstream, use the member wxScopedPtr to 'return' the same stream
return true;
}

bool TarArchiveStream::Extract(wxArrayString& filepaths, wxString destpath, wxArrayString& resultingfilepaths, bool dirs_only/*=false*/, bool files_only/*=false*/)  // Create and extract filepath from the archive
{
if (filepaths.IsEmpty() || destpath.IsEmpty()) return false;

  // AdjustFilepaths puts each filepath into WithinArchiveNames, less the /path-to-archive/archive; & creates its dest in WithinArchiveNewNames. If a dir, recursively adds into children too
if (!AdjustFilepaths(filepaths, destpath, resultingfilepaths, afp_extract, dirs_only, files_only)) return false;

GetFromBuffer();

size_t NumberFound = 0;
wxTarEntryPtr entry;

while (entry.reset(((wxTarInputStream*)InStreamPtr.get())->GetNextEntry()), entry.get() != NULL)
  { int item = WithinArchiveNames.Index(entry->GetName());  // See if this file is one of the ones we're interested in
    if (item == wxNOT_FOUND)  continue;
                // Found it. If it's a file, extract it. If a dir,  make a new dir with this name (*outside* the archive ;)
    wxLogNull log;
    if ((entry->GetTypeFlag() == wxTAR_REGTYPE) // Start with the files, which are the only things that require extraction of data. wxTAR_REGTYPE is 48 ('0')
          || (entry->GetTypeFlag() == 0))       // However in ancient tar formats (as automake's make dist seems to use by default :/) a regular file has a type of NUL
      { wxString Path(resultingfilepaths[ item ]); Path = Path.BeforeLast(wxFILE_SEP_PATH);
        if (!wxFileName::Mkdir(Path, 0777, wxPATH_MKDIR_FULL)) return false;    // Make any necessary dir
        wxFFileOutputStream out(resultingfilepaths[ item ]);
        if (!out || ! out.Write(*(wxTarInputStream*)InStreamPtr.get()) || ! InStreamPtr->Eof())   return false;  // and extract into the new filepath
        FileData fd(resultingfilepaths[ item ]); if (fd.IsValid()) fd.DoChmod(entry->GetMode());  // Correct the extracted file's permissions
      }
     else if (entry->IsDir())  
           { if (!wxFileName::Mkdir(resultingfilepaths[ item ], 0777, wxPATH_MKDIR_FULL)) return false; }  // Just create the dir

    else if (entry->GetTypeFlag() == wxTAR_SYMTYPE || entry->GetTypeFlag() == wxTAR_LNKTYPE) // The 2nd is in case of confusion, but all links should be symlinks
           { if (!CreateSymlink(entry->GetLinkName(), resultingfilepaths[ item ])) return false; }
          
    else        // Otherwise it's a Misc:  a FIFO or similar. Just create a new one with the same name
      { wxString Path(resultingfilepaths[ item ]); Path = Path.BeforeLast(wxFILE_SEP_PATH);
        FileData fd(Path, true);
         mode_t mode; dev_t dev=0;
        switch(entry->GetTypeFlag())
          { case wxTAR_CHRTYPE:  mode = S_IFCHR; dev = fd.GetDeviceID(); break; case wxTAR_BLKTYPE: mode = S_IFBLK; dev = fd.GetDeviceID(); break;
            case wxTAR_FIFOTYPE: mode = S_IFIFO; break;  // FIFOs don't need the major/minor thing in dev
             default: continue; 
          }

        mode_t oldUmask = umask(0); 
        if (entry->GetTypeFlag()==wxTAR_FIFOTYPE || getuid()==0)    // If we're making a fifo (which doesn't require su), or we're already root
          mknod(resultingfilepaths[ item ].mb_str(wxConvUTF8), mode | 0777, dev);
         else
          { wxString secondbit = (mode==S_IFBLK ? wxT(" b ") :  wxT(" c ")); secondbit << (int)(dev / 256) << wxT(" ") << (int)(dev % 256);
            if (WHICH_SU==mysu)  
              { if (USE_SUDO) ExecuteInPty(wxT("sudo \"mknod -m 0777 ") + resultingfilepaths[ item ] + secondbit + wxT("\""));
                 else         ExecuteInPty(wxT("su -c \"mknod -m 0777 ") + resultingfilepaths[ item ] + secondbit + wxT("\""));
              }
             else if (WHICH_SU==kdesu) wxExecute(wxT("kdesu \"mknod -m 0777 ") + resultingfilepaths[ item ] + secondbit + wxT("\""));
             else if (WHICH_SU==gksu) wxExecute(wxT("gksu \"mknod -m 0777 ") + resultingfilepaths[ item ] + secondbit + wxT("\""));
             else if (WHICH_SU==gnomesu) wxExecute(wxT("gnomesu --title "" -c \"mknod -m 0777 ") + resultingfilepaths[ item ] + secondbit + wxT("\" -d"));
             else if (WHICH_SU==othersu && !OTHER_SU_COMMAND.IsEmpty()) wxExecute(OTHER_SU_COMMAND + wxT("\"mknod -m 0777 ") +resultingfilepaths[ item ] + secondbit + wxT("\" -d"));
                else { wxMessageBox(_("Sorry, you need to be root to extract character or block devices"), wxT(":-(")); umask(oldUmask); return false; }
          }
        umask(oldUmask);
      }
     
    ++NumberFound;
  }

  // This line used to be inside the loop. However it broke if someone added foo to an archive that already contained a foo; the bogof situation messed up the count
if (NumberFound >= WithinArchiveNames.GetCount()) return true;  // See if we've used all the passed filepaths
 else return false;
}

wxMemoryBuffer* TarArchiveStream::ExtractToBuffer(wxString filepath)  // Extract filepath from this archive into a new wxMemoryBuffer
{
if (filepath.IsEmpty()) return NULL;

if (!SortOutNames(filepath)) return NULL;

wxTarEntryPtr entry;

wxBusyCursor busy;
GetFromBuffer();          // The archive is in the memory buffer m_membuf. 'Extract' it to InStreamPtr

while (entry.reset(((wxTarInputStream*)InStreamPtr.get())->GetNextEntry()), entry.get() != NULL)
  { if (entry->GetName() != WithinArchiveName)  continue;   // Go thru the archive, looking for the desired file
                // Found it. Extract it to a new buffer
    wxMemoryOutputStream memoutstream;
    if (!memoutstream || ! memoutstream.Write(*(wxTarInputStream*)InStreamPtr.get()))   return NULL;  

    size_t size = memoutstream.GetSize();                   // This is how much was loaded i.e. the file size
    wxChar* buf = new wxChar[ size ];                       // It's ugly, but the easiest way I've found to get the data from the stream to a memorybuffer
    memoutstream.CopyTo(buf, size);                         //   is by using an intermediate char[]

    wxMemoryBuffer* membuf = new wxMemoryBuffer(size);      // Now (at last) we can store the data in a memory buffer
    membuf->AppendData(buf, size);
    delete[] buf;
  
    return membuf;
  }

return NULL;
}

bool TarArchiveStream::DoAlteration(wxArrayString& filepaths, enum alterarc DoWhich, const wxString& originroot/*=wxT("")*/, bool dirs_only/*=false*/)  // Implement add remove del or rename, depending on the enum
{
wxInputStreamPtr dupptr;
GetFromBuffer();                                  // The archive is in the memory buffer m_membuf. 'Extract' it to InStreamPtr
if (DoWhich==arc_dup)                             // If we're duplicating, we need a 2nd instream, so store the first and re-extract
  { dupptr.reset(InStreamPtr.release()); GetFromBuffer(true); }

wxMemoryOutputStream* memoutstream = new wxMemoryOutputStream;
if (!CompressStream(memoutstream)) return false;  // This will compress the output (unless we're dealing with a plain tar). False means we're not gzip-capable

wxTarOutputStream taroutstream(*OutStreamPtr.get());  // OutStreamPtr is the 'output' of the CompressStream call in Alter()

wxTarEntryPtr entry,  dupentry;
while (entry.reset(((wxTarInputStream*)InStreamPtr.get())->GetNextEntry()), entry.get() != NULL)  // Go thru the archive, looking for the selected files.
  { int item = WithinArchiveNames.Index(entry->GetName());  // See if this file is one of the ones we're interested in
    switch(DoWhich)
      { case arc_add:      break;                 // Add happens later
        case arc_remove:  if (item != wxNOT_FOUND)  continue; // If this file is one of the 2b-removed ones, ignore it, don't add it to the new archive
                          else break;                         // Otherwise break to copy the entry
        case arc_dup:      dupentry.reset(((wxTarInputStream*)dupptr.get())->GetNextEntry());  // For dup, get the next entry, then fall thru to rename
        case arc_rename:  if (item != wxNOT_FOUND)            // For rename, change the name then break to copy the entry
                          { if (entry->IsDir() && WithinArchiveNewNames[ item ].Last() != wxFILE_SEP_PATH) WithinArchiveNewNames[ item ] << wxFILE_SEP_PATH;
                            entry->SetName(WithinArchiveNewNames[ item ]); 
                          } 
                        break;
                          
        case arc_del:      continue;
      }
                // Copy this entry to the outstream
    if (!entry->IsDir())
      { if (!taroutstream.CopyEntry(entry.release(), *(wxTarInputStream*)InStreamPtr.get())) return false;  // CopyEntry is specific to archive/zip-outputsteams, which is why I'm using one here
        if (DoWhich==arc_dup && item != wxNOT_FOUND)         // If we're duplicating, we now need to output the original version to the stream
            if (!taroutstream.CopyEntry(dupentry.release(), *(wxTarInputStream*)dupptr.get())) return false;
      }
     else 
      { if (!taroutstream.PutNextDirEntry(entry->GetName(), entry->GetDateTime())) return false; // If it's a dir, this just writes the 'header' info; there isn't any data
        entry.release();
        if (DoWhich==arc_dup && item != wxNOT_FOUND)         // If we're duplicating, we now need to output the original version to the stream
          { if (!taroutstream.PutNextDirEntry(dupentry->GetName(), dupentry->GetDateTime())) return false; dupentry.release(); }
      }
  }  

if (DoWhich == arc_add)    // If we're trying to add, we've just copied all the old entries. Now add the files to the within-archive dir WithinArchiveName
  { bool success = false;
    for (size_t n=0; n < filepaths.GetCount(); ++n)
      { if (DoAdd(filepaths[n], taroutstream, originroot, dirs_only))  success = true;  }
    if (!success) return false;                             // No point proceeding if nothing worked
  }

if (!taroutstream.Close()) return false;
if (!OutStreamPtr.get()->Close())  return false;
if (!memoutstream->Close()) return false;

OutStreamPtr.reset(memoutstream);  // This has a kludgey feel, but it returns the memoutstream to Alter() via the member scoped ptr
return true;
}

bool TarArchiveStream::DoAdd(const wxString& filepath, wxTarOutputStream& taroutstream, const wxString& originroot, bool dirs_only/*=false*/)
{ 
FileData fd(filepath); if (!fd.IsValid()) return false;
wxString Name; if (!filepath.StartsWith(originroot, &Name))  return false;  // Get what's after path into Name: if we've adding  dir foo, path==../path/, Name will be /foo or /foo/bar
Name = WithinArchiveName + Name; wxDateTime DT(fd.ModificationTime());
if (fd.IsDir())
    return taroutstream.PutNextDirEntry(Name, DT);           // Dirs don't have data, so just provide the filepath

if (dirs_only) return false;                                 // If we're pasting a dir skeleton, skip the non-dirs code below


wxTarEntry* addentry = new wxTarEntry(Name, DT); addentry->SetMode(fd.GetPermissions());
if (fd.IsRegularFile())
  { wxFFileInputStream file(filepath); if (!file.Ok()) return false;  // Make a wxFFileInputStream to get the data to be added
    addentry->SetSize(file.GetSize());
    // and add it to the outputstream
    return (taroutstream.PutNextEntry(addentry) && (!!taroutstream.Write(file)) && file.Eof());
  }
                      // If it's not a dir, or a file, it must be a misc
int type = wxTAR_REGTYPE; if (fd.IsSymlink()) type = wxTAR_SYMTYPE;
if (fd.IsCharDev()) type = wxTAR_CHRTYPE; if (fd.IsBlkDev()) type = wxTAR_BLKTYPE; if (fd.IsFIFO()) type = wxTAR_FIFOTYPE;
addentry->SetTypeFlag(type);
if (fd.IsSymlink()) addentry->SetLinkName(fd.GetSymlinkDestination(true));  // If a symlink, tell entry its target
return taroutstream.PutNextEntry(addentry);                  // Add it to the outputstream
}

void TarArchiveStream::RefreshFromDirtyChild(wxString archivename, ArchiveStream* child)  // Used when a nested child archive has been altered; reload the altered version
{
if (archivename.IsEmpty() || child == NULL) return;
wxMemoryInputStream  childstream(child->GetBuffer()->GetData(), child->GetBuffer()->GetDataLen());

GetFromBuffer();                                      // The archive is in the memory buffer m_membuf. 'Extract' it to InStreamPtr
wxMemoryOutputStream* memoutstream = new wxMemoryOutputStream;
if (!CompressStream(memoutstream)) return;            // This will compress the output (unless we're dealing with a plain tar). False means we're not gzip-capable
wxTarOutputStream taroutstream(*OutStreamPtr.get());  // OutStreamPtr is the 'output' of the CompressStream call in Alter()

wxTarEntryPtr entry;
while (entry.reset(((wxTarInputStream*)InStreamPtr.get())->GetNextEntry()), entry.get() != NULL)    // Go thru the archive, looking for the selected files.
  {                 // Copy this entry to the outstream
    if (!entry->IsDir())
      { if (archivename == entry->GetName())         // If this is the archive we want to refresh
          { if (!taroutstream.PutNextEntry(entry->GetName(), entry->GetDateTime(), childstream.GetSize())  // ignore the original archive; instead add the new data
                        || ! taroutstream.Write(childstream) || ! childstream.Eof())    return;
          }
         else if (!taroutstream.CopyEntry(entry.release(), *(wxTarInputStream*)InStreamPtr.get())) return; // CopyEntry is specific to archive/zip-outputsteams, which is why I'm using one here

      }
     else 
      { if (!taroutstream.PutNextDirEntry(entry->GetName(), entry->GetDateTime())) return; // If it's a dir, this just writes the 'header' info; there isn't any data
        entry.release();
      }
  }  

if (!taroutstream.Close()) return;
if (!OutStreamPtr.get()->Close()) return;
if (!memoutstream->Close())return;

OutStreamPtr.release();                       // Not doing this causes a double-deletion segfault when the archivestream is deleted

size_t size = memoutstream->GetSize();        // This is how much was loaded i.e. the file size
wxChar* buf = new wxChar[ size ];             // It's ugly, but the easiest way I've found to get the data from the stream to a memorybuffer
memoutstream->CopyTo(buf, size);              // is by using an intermediate char[]
delete memoutstream;

delete m_membuf; m_membuf = new wxMemoryBuffer(size); // I've no idea why, but trying to delete the old data by SetDataLen(0) failed :(
m_membuf->AppendData(buf, size);              // Store the data
delete[] buf;

SaveBuffer();                                 // We now need to save (or declare dirty) the parent arc
}

//-----------------------------------------------------------------------------------------------------------------------

void GZArchiveStream::GetFromBuffer(bool dup)  // Get the stored archive from membuf into the stream
{
wxZlibInputStream* zlib;  wxTarInputStream* tar;
wxMemoryInputStream* meminstream = new wxMemoryInputStream(m_membuf->GetData(), m_membuf->GetDataLen());  // The archive is in the memory buffer membuf. Stream it to a memory stream
if (!dup) 
  { MemInStreamPtr.reset(meminstream);
    zlib = new wxZlibInputStream(*MemInStreamPtr.get());    // Then pass it thru the filter
    zlibInStreamPtr.reset(zlib);
    tar = new wxTarInputStream(*zlibInStreamPtr.get());     // into a wxTarInputStream
  }
 else
   { DupMemInStreamPtr.reset(meminstream);
    zlib = new wxZlibInputStream(*DupMemInStreamPtr.get()); // If we're duplicating, use DupMemInStreamPtr/DupzlibInStreamPtr the second time around
    DupzlibInStreamPtr.reset(zlib);
    tar = new wxTarInputStream(*DupzlibInStreamPtr.get());
  }

InStreamPtr.reset(tar);                                     // Use this member wxScopedPtr to 'return' the stream
}

bool GZArchiveStream::CompressStream(wxOutputStream* outstream)  // Compress this stream, ready to be put into membuf
{
if (!wxZlibOutputStream::CanHandleGZip())
  { wxLogError(_("I'm afraid your zlib is too old to be able to do this :(")); return false; }

wxZlibOutputStream* zlib = new wxZlibOutputStream(*outstream, wxZ_BEST_COMPRESSION, wxZLIB_GZIP);  // Pass the outstream thru the filter
OutStreamPtr.reset(zlib);                                                                          // Use the member wxScopedPtr to 'return' the stream

return true;
}

void GZArchiveStream::ListContentsFromBuffer(wxString archivename)
{
wxBusyCursor busy;
GetFromBuffer();
ListContents(archivename);
}

//-----------------------------------------------------------------------------------------------------------------------
void BZArchiveStream::GetFromBuffer(bool dup)  // Get the stored archive from membuf into the stream
{
wxBZipInputStream* zlib; wxTarInputStream* tar;
wxMemoryInputStream* meminstream = new wxMemoryInputStream(m_membuf->GetData(), m_membuf->GetDataLen());  // The archive is in the memory buffer membuf. Stream it to a memory stream
if (!dup) 
  { MemInStreamPtr.reset(meminstream);
    zlib = new wxBZipInputStream(*MemInStreamPtr.get());          // Then pass it thru the filter
    zlibInStreamPtr.reset(zlib);
    tar = new wxTarInputStream(*zlibInStreamPtr.get());           // into a wxTarInputStream
  }
  else
  { DupMemInStreamPtr.reset(meminstream);
    zlib = new wxBZipInputStream(*DupMemInStreamPtr.get());       // If we're duplicating, use DupMemInStreamPtr/DupzlibInStreamPtr the second time around
    DupzlibInStreamPtr.reset(zlib);
    tar = new wxTarInputStream(*DupzlibInStreamPtr.get());
  }

InStreamPtr.reset(tar);                                           // Use this member wxScopedPtr to 'return' the stream
}

bool BZArchiveStream::CompressStream(wxOutputStream* outstream)   // Compress this stream, ready to be put into membuf
{
wxBZipOutputStream* bzip = new wxBZipOutputStream(*outstream, 9); // Pass the outstream thru the filter
OutStreamPtr.reset(bzip);                                         // Use the member wxScopedPtr to 'return' the stream

return true;
}

void BZArchiveStream::ListContentsFromBuffer(wxString archivename)
{
wxBusyCursor busy;
GetFromBuffer();
ListContents(archivename);
}

//-----------------------------------------------------------------------------------------------------------------------
#ifndef NO_LZMA_ARCHIVE_STREAMS
void XZArchiveStream::GetFromBuffer(bool dup)  // Get the stored archive from membuf into the stream
{
XzInputStream* zlib; wxTarInputStream* tar;
wxMemoryInputStream* meminstream = new wxMemoryInputStream(m_membuf->GetData(), m_membuf->GetDataLen());  // The archive is in the memory buffer membuf. Stream it to a memory stream
if (!dup) 
  { MemInStreamPtr.reset(meminstream);
    if (m_zt == zt_tarxz)
      zlib = new XzInputStream(*MemInStreamPtr.get());            // Then pass it thru the filter
     else
      zlib = new LzmaInputStream(*MemInStreamPtr.get());
    zlibInStreamPtr.reset(zlib);
    tar = new wxTarInputStream(*zlibInStreamPtr.get());           // into a wxTarInputStream
  }
  else
  { DupMemInStreamPtr.reset(meminstream);
    if (m_zt == zt_xz)
      zlib = new XzInputStream(*DupMemInStreamPtr.get());         // If we're duplicating, use DupMemInStreamPtr/DupzlibInStreamPtr the second time around
     else
      zlib = new LzmaInputStream(*DupMemInStreamPtr.get());
    DupzlibInStreamPtr.reset(zlib);
    tar = new wxTarInputStream(*DupzlibInStreamPtr.get());
  }

InStreamPtr.reset(tar);                                           // Use this member wxScopedPtr to 'return' the stream
}

bool XZArchiveStream::CompressStream(wxOutputStream* outstream)   // Compress this stream, ready to be put into membuf
{
XzOutputStream* xz;
if (m_zt == zt_tarxz)
  xz = new XzOutputStream(*outstream);      // Pass the outstream thru the filter
 else
  xz = new LzmaOutputStream(*outstream);
OutStreamPtr.reset(xz);                     // Use the member wxScopedPtr to 'return' the stream

return true;
}

void XZArchiveStream::ListContentsFromBuffer(wxString archivename)
{
wxBusyCursor busy;
GetFromBuffer();
ListContents(archivename);
}
#endif // ndef NO_LZMA_ARCHIVE_STREAMS
//-----------------------------------------------------------------------------------------------------------------------

void ArchiveStreamMan::Push()  // Make a new ArchiveStruct and save it on the array
{
if (arc==NULL) return;

ArchiveStruct* newarcst = new struct ArchiveStruct(arc, ArchiveType);
ArcArray->Add(newarcst);
}

bool ArchiveStreamMan::Pop()    // Revert to any previous archive. Returns true if this happened
{
size_t count = ArcArray->GetCount();
if (!count)                                                     // If we're not within multiple archives, just delete and exit
  { if (!IsArchive()) return false;                             //  providing there's anything to delete
    delete arc; arc = NULL; ArchiveType = zt_invalid; OriginalArchive.Empty();   // NB We also kill any arc data, so Pop() will exit a 'single' archive too
            // NB we really need the following checks, as otherwise we'll segfault if we're within an archive when the program exits
    if (MyFrame::mainframe==NULL || ! MyFrame::mainframe->SelectionAvailable) return false;
    MyGenericDirCtrl* active = MyFrame::mainframe->GetActivePane(); if (active==NULL) return false; 
    if (active->fileview == ISRIGHT) active = active->partner; active->fulltree = WasOriginallyFulltree;  // Set the pane's fulltree-ness to its original value
    return false; 
  }

ArchiveStruct* PreviousArcst = ArcArray->Item(count-1);
if (arc->IsDirty())                                             // If we're exiting a nested archive, it might need 2b saved to its parent
  { // Remove the parent-archive/ bit of the filepath (we can't just use the last segment of arc->GetArchiveName(); we may be in a parentarc/subdir/childarc situation)
    wxString dirtychildname = arc->GetArchiveName().Mid(PreviousArcst->arc->GetArchiveName().Len() + 1);
    PreviousArcst->arc->RefreshFromDirtyChild(dirtychildname, arc);
  }
delete arc; arc = PreviousArcst->arc; ArchiveType = PreviousArcst->ArchiveType;
ArcArray->RemoveAt(count-1); delete PreviousArcst;                // NB deleting the struct doesn't delete the stored archive, as it is referenced by arc.

return true;
}

bool ArchiveStreamMan::NewArc(wxString filepath, enum ziptype NewArchiveType)
{
if (filepath.IsEmpty()) return false;

DataBase* stat;
bool OriginallyInArchive = IsArchive();
if (IsArchive()) stat = new FakeFiledata(filepath);  // If we're opening an archive within an archive, we need to use FakeFiledata here
 else stat = new FileData(filepath);
if (!stat->IsValid()) { delete stat; return false; }

ArchiveStream* newarc;

switch(NewArchiveType)
    { case zt_htb:                                                              // htb's are effectively zips
      case zt_zip: if (!IsArchive()) newarc = new ZipArchiveStream(stat);       // If we're not already inside an archive, do things the standard way
                   else
                    { wxMemoryBuffer* membuf; membuf = arc->ExtractToBuffer(filepath);
                      if (membuf == NULL) { delete stat; return false; }
                      newarc = new ZipArchiveStream(membuf, filepath);
                    }
                   if (!(newarc && newarc->IsValid()))
                    { wxLogError(_("For some reason, the archive failed to open  :(")); 
                      delete stat; delete newarc; return false;
                    }
                    ((ZipArchiveStream*)newarc)->ListContentsFromBuffer(filepath); break;
      case zt_targz: if (!IsArchive())  newarc = new GZArchiveStream(stat);     // If we're not already inside an archive, do things the standard way
                   else
                    { wxMemoryBuffer* membuf; membuf = arc->ExtractToBuffer(filepath);  // Extract filepath from the previous archive to a membuffer
                      if (membuf == NULL) { delete stat; return false; }
                      newarc = new GZArchiveStream(membuf, filepath);           // Make a new archive of it
                    }
                   if (!(newarc && newarc->IsValid()))
                    { wxLogError(_("For some reason, the archive failed to open  :(")); 
                      delete stat; delete newarc; return false;
                    }
                   ((GZArchiveStream*)newarc)->ListContentsFromBuffer(filepath); break;
      case zt_taronly: if (!IsArchive())  newarc = new TarArchiveStream(stat);
                   else
                    { wxMemoryBuffer* membuf; membuf = arc->ExtractToBuffer(filepath);
                      if (membuf == NULL) { delete stat; return false; }
                      newarc = new TarArchiveStream(membuf, filepath);
                    }
                   if (!(newarc && newarc->IsValid()))
                    { wxLogError(_("For some reason, the archive failed to open  :(")); 
                      delete stat; delete newarc; return false;
                    }
                   ((TarArchiveStream*)newarc)->ListContentsFromBuffer(filepath); break;
      case zt_tarbz: if (!IsArchive())  newarc = new BZArchiveStream(stat);
                   else
                    { wxMemoryBuffer* membuf; membuf = arc->ExtractToBuffer(filepath);
                      if (membuf == NULL) { delete stat; return false; }
                      newarc = new BZArchiveStream(membuf, filepath);
                    }
                   if (!(newarc && newarc->IsValid()))
                    { wxLogError(_("For some reason, the archive failed to open  :(")); 
                      delete stat; delete newarc; return false;
                    }
                   ((BZArchiveStream*)newarc)->ListContentsFromBuffer(filepath); break;
#ifndef NO_LZMA_ARCHIVE_STREAMS
      case zt_tarlzma:
      case zt_tarxz: if (!LIBLZMA_LOADED)
                      { wxMessageBox(_("I can't peek inside that sort of archive unless you install liblzma."), _("Missing library")); delete stat; return false; }
                   if (!IsArchive())  newarc = new XZArchiveStream(stat, NewArchiveType);
                    else
                      { wxMemoryBuffer* membuf; membuf = arc->ExtractToBuffer(filepath);
                        if (membuf == NULL) { delete stat; return false; }
                        newarc = new XZArchiveStream(membuf, filepath, NewArchiveType);
                      }
                   if (!(newarc && newarc->IsValid()))
                    { wxLogError(_("For some reason, the archive failed to open  :(")); 
                      delete stat; delete newarc; return false;
                    }
                   ((XZArchiveStream*)newarc)->ListContentsFromBuffer(filepath); break;
#endif // ndef NO_LZMA_ARCHIVE_STREAMS
      case zt_gzip: case zt_bzip: case zt_lzma: case zt_xz: case zt_lzop: case zt_compress:
                    wxMessageBox(_("This file is compressed, but it's not an archive so you can't peek inside."), _("Sorry")); delete stat; return false;
       default:     wxMessageBox(_("I'm afraid I can't peek inside that sort of archive."), _("Sorry")); delete stat; return false;
    }
if (newarc == NULL) { delete stat; return false; }

Push();                                         // Save any current archive data
arc = newarc; ArchiveType = NewArchiveType;     //  and install the new values
if (!OriginallyInArchive)                       // If this is the original archive (ie not nesting), store interesting info
    { OriginalArchive = filepath; OutOfArchivePath = filepath.BeforeLast(wxFILE_SEP_PATH) + wxFILE_SEP_PATH; }
  
delete stat; return true;
}

//static 
bool ArchiveStream::IsStreamable(enum ziptype ztype)
{
if (ztype < zt_firstarchive) return false; // We can't stream compresed files

switch(ztype)
  { case zt_taronly: case zt_targz: case zt_tarbz: case zt_zip: case zt_htb: return true;
    
#ifndef NO_LZMA_ARCHIVE_STREAMS
    case zt_tarlzma: case zt_tarxz: return LIBLZMA_LOADED;
#endif // ndef NO_LZMA_ARCHIVE_STREAMS

    default: return false; // zt_invalid, zt_tar7z, zt_tarlzop, zt_taZ, zt_7z, zt_cpio, zt_rpm, zt_ar, zt_deb
  }
}

bool ArchiveStreamMan::IsWithinArchive(wxString filepath) // Is filepath within the archive system i.e. the original archive itself, or 1 of its contents, or that of a nested archive
{
if (arc==NULL || ! IsArchive()) return false;

while (filepath.Right(1) == wxFILE_SEP_PATH && filepath.Len() > 1) filepath.RemoveLast();
return filepath.StartsWith(OriginalArchive);
}

bool ArchiveStreamMan::IsWithinThisArchive(wxString filepath)
{
if (!IsWithinArchive(filepath)) return false;          // It's faster first to check if it's possible

return arc->IsWithin(filepath); 
}

bool ArchiveStreamMan::MightFilepathBeWithinChildArchive(wxString filepath)  // See if filepath is longer than the current rootdir. Assume we've just checked it's not in *this* archive
{
if (!IsArchive() || filepath.IsEmpty()) return false;  // If there isn't an archive open, the question is meaningless

return filepath.StartsWith(GetArc()->Getffs()->GetRootDir()->GetFilepath());  // Shouldn't need to worry about the terminal '/' on rootdir
}

wxString ArchiveStreamMan::FindFirstArchiveinFilepath(wxString filepath)  // Filepath will be a fragment of a path. Find the first thing that looks like an archive
{
wxString filename, segment;
while (true)
  { if (filepath.GetChar(0) == wxFILE_SEP_PATH) filepath = filepath.Mid(1);    // Remove any prepended '/'
    if (filepath.IsEmpty()) return filepath;
    filename += wxFILE_SEP_PATH; segment = filepath.BeforeFirst(wxFILE_SEP_PATH); // Get the first segment of filepath
    filename += segment; if (Archive::Categorise(filename) != zt_invalid) return filename;  // See if it's an archive. If so return it
    filepath = filepath.Mid(segment.Len());                                    // Amputate the bit we just checked from filepath & try again
  }

return wxEmptyString;    // We shouldn't reach here
}


enum OCA ArchiveStreamMan::OpenCorrectArchive(wxString& filepath)  // See if filepath is (within) a, possibly nested, archive. If so, open it
{
if (filepath.IsEmpty()) return OCA_false;
if (IsWithinThisArchive(filepath)) return OCA_true;   // The trivial case!

wxString archivepath;
if (!IsArchive())
  { archivepath = FindFirstArchiveinFilepath(filepath);
    if (archivepath.IsEmpty()) return OCA_false;
    ziptype zt = Archive::Categorise(archivepath); 
    if (zt == zt_invalid)  return OCA_false;          // Shouldn't happen!
    if (!NewArc(archivepath, zt)) return OCA_false;   // Enter the 1st archive
  }

if (MightFilepathBeWithinChildArchive(filepath))      // ie we're going forwards, not backwards
  while (true)
    { if (IsWithinThisArchive(filepath))  return OCA_true; // Are we nearly there yet?
      archivepath = GetArc()->Getffs()->GetRootDir()->GetFilepath(); archivepath = archivepath.BeforeLast(wxFILE_SEP_PATH);
      wxString extra; if (!filepath.StartsWith(archivepath, &extra)) return OCA_false;  // Shouldn't happen!
                                        // Extra now contains the unused bit of filepath. 
      wxString nextbit = FindFirstArchiveinFilepath(extra);   // Get the next archive in the path
      if (nextbit.IsEmpty())  return OCA_false;
      archivepath += nextbit;
      ziptype zt = Archive::Categorise(archivepath); 
      if (zt == zt_invalid)  return OCA_false;        // Shouldn't happen!
      if (!NewArc(archivepath, zt)) return OCA_false; // Enter the next archive in the chain
    }

bool flag=true;          // It wasn't further into this archive, so try reversing
while (true)
  { flag = Pop();
    if (IsWithinThisArchive(filepath)) return OCA_true;
    if (!flag) return OCA_retry;                      // We're out of the old archive. Retry says do it again, to enter any new one
  }
 
return OCA_false;
}

enum NavigateArcResult ArchiveStreamMan::NavigateArchives(wxString filepath)  // If filepath is in an archive, goto the archive. Otherwise don't
{
if (filepath.IsEmpty()) return Snafu;

FileData stat(filepath);

if (!IsArchive())              // If we're not already inside an archive, we may need to sort out fulltree mode:
  { if ((stat.IsValid() && Archive::Categorise(filepath) != zt_invalid)           // if we're entering an archive,
      || (! stat.IsValid() && ! FindFirstArchiveinFilepath(filepath).IsEmpty()))  // or if we're jumping deep into an archive,
        { MyGenericDirCtrl* active = MyFrame::mainframe->GetActivePane(); if (active->fileview == ISRIGHT) active = active->partner;
          WasOriginallyFulltree = active->fulltree; active->fulltree = false;     // Turn off fulltree mode if it's on, after storing the current state
        }
  }

if (stat.IsValid() && filepath != OriginalArchive)  // If this is an ordinary filepath, escape from any archive(s) and return for normal processing
  { while (Pop())									// The only other reason for stat being valid is if it's the original archive; in which case no point exiting here
		; 
	return OutsideArc; 
  }            

enum OCA ans;
do ans = OpenCorrectArchive(filepath);              // Navigate either in, out or shake it all about
  while (ans == OCA_retry);                         // We'll need to retry if we *were* in an archive, and now have to come out to enter a different one
return (ans==OCA_true ? FoundInArchive : Snafu);
}

wxString ArchiveStreamMan::ExtractToTempfile()  // Called by FiletypeManager::Openfunction to provide a extracted tempfile
{
MyGenericDirCtrl* active = MyFrame::mainframe->GetActivePane(); if (active==NULL) return wxEmptyString; 
wxString filepath = active->GetFilePath(); if (filepath.IsEmpty()) return filepath;       // Find the filepath to open

wxArrayString filepaths; filepaths.Add(filepath);   // Although there's only 1 filepath, the real ExtractToTempfile() needs an array
wxArrayString resultingfilepaths = ExtractToTempfile(filepaths);

if (resultingfilepaths.IsEmpty()) return wxEmptyString;
return resultingfilepaths[0];                       // The resulting filepaths are returned in an array. We know there'll only be one of them
}

wxArrayString ArchiveStreamMan::ExtractToTempfile(wxArrayString& filepaths, bool dirs_only/*=false*/, bool files_only/*=false*/)  // Called by e.g. EditorBitmapButton::OnEndDrag etc, to provide extracted tempfile(s)
{
wxArrayString tempfilepaths;
if (!IsArchive() || arc==NULL || filepaths.IsEmpty()) return tempfilepaths;

wxFileName tempdirbase;
wxString temppath;
while (true)                                        // Do in a loop in case there's already one there ?!?
  { if (!DirectoryForDeletions::GetUptothemomentDirname(tempdirbase, tempfilecan)) return tempfilepaths;  // Make a temporary dir to hold the files
    temppath = tempdirbase.GetFullPath();
    FileData stat(temppath); if (stat.IsValid()) break;
  }

if (!arc->Extract(filepaths, temppath, tempfilepaths, dirs_only, files_only))   // Extract, deleting the temp dir if it fails
  { tempfilepaths.Clear(); tempdirbase.Rmdir(); }

return tempfilepaths;                               // Return the extracted tempfilepaths
}


