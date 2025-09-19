/////////////////////////////////////////////////////////////////////////////
// Name:       ArchiveStream.h
// Purpose:    Virtual manipulation of archives
// Part of:    4Pane
// Author:     David Hart
// Copyright:  (c) 2016 David Hart
// Licence:    GPL v3
/////////////////////////////////////////////////////////////////////////////
#ifndef ARCHIVESTREAMH
#define ARCHIVESTREAMH

#include "wx/wx.h"
#include "wx/txtstrm.h"
#include "wx/dir.h"
#include "wx/tarstrm.h"

#include <wx/mstream.h>

enum DB_filetype{ REGTYPE, HDLNKTYPE, SYMTYPE, CHRTYPE, BLKTYPE, DIRTYPE, FIFOTYPE, SOCTYPE };

enum ffscomp { ffsParent, ffsEqual, ffsChild, ffsCousin, ffsRubbish, ffsDealtwith  };    // Used to return result of comparison within 'dir' tree

class DataBase  // Base class for FileData and FakeFiledata
{
public:
virtual ~DataBase(){}
virtual wxString GetFilepath(){ return wxEmptyString; }
virtual wxString GetFilename(){ return wxEmptyString; }
virtual wxString ReallyGetName(){ return wxEmptyString; }
virtual wxString GetOwner(){ return wxEmptyString; }
virtual wxString GetGroup(){ return wxEmptyString; }
virtual wxString GetSymlinkDestination(bool AllowRelative=false){ return wxEmptyString; }
virtual wxULongLong Size(){ return (wxULongLong)0; }
virtual uid_t OwnerID(){ return (uid_t)0;}
virtual gid_t GroupID(){ return (gid_t)0;}
virtual wxString GetParsedSize(){ return wxEmptyString; }
virtual time_t ModificationTime(){ return (time_t)0; }
virtual size_t GetPermissions(){ return (size_t)0;}
virtual wxString PermissionsToText(){ return wxEmptyString; }
virtual bool IsSymlinktargetADir(bool recursing = false){return false; }
virtual bool IsSymlink(){ return Type==SYMTYPE; }
virtual bool IsBrokenSymlink(){ return false; }
virtual bool IsSocket(){ return Type==SOCTYPE; }
virtual bool IsFIFO(){ return Type==FIFOTYPE; }
virtual bool IsBlkDev(){ return Type==BLKTYPE; }
virtual bool IsCharDev(){ return Type==CHRTYPE; }
virtual bool IsRegularFile(){ return Type==REGTYPE; }
virtual DataBase* GetSymlinkData(){return NULL;}
virtual bool IsValid();
virtual bool IsDir(){ return false; }
wxString TypeString();                        // Returns string version of type eg "Directory"

wxString sortstring;                          // For most sort modalities, the data is stored here to be sorted
int sortint;                                  // For int sorts e.g. modtime, the data is stored here to be sorted
wxULongLong sortULL;                          // For wxULongLong sorts i.e. Size
DB_filetype Type; // e.g. FIFO
bool IsFake;                                  // To distinguish between genuine FileDatas and Fake ones
};


class FakeFiledata  :  public DataBase        // Holds the data for a single file in a FakeFilesystem.  Base class is to allow DataBase* also to reference a real FileData 
{
public:
FakeFiledata(){ IsFake = true; }
FakeFiledata(const FakeFiledata& fd){ *this = fd; }
FakeFiledata(const wxString& name, wxULongLong sze=0, time_t Time=(time_t)0, DB_filetype type=REGTYPE, const wxString& Target=wxT(""), size_t Perms=0664, const wxString& Owner=wxT(""), const wxString& Group=wxT(""))
                    :   Filepath(name), size(sze), ModTime(Time), Permissions(Perms), Ownername(Owner), Groupname(Group) { Type = type; symdest = Target; IsFake = true; }
virtual ~FakeFiledata(){}                    

FakeFiledata& operator=(const FakeFiledata& fd){ IsFake = true; Filepath = fd.Filepath; size = fd.size; ModTime = fd.ModTime; Type = fd.Type; symdest = fd.symdest;
                                           Permissions = fd.Permissions; Ownername = fd.Ownername; Groupname = fd.Groupname; return *this; }

wxString GetFilepath(){ return Filepath; }
virtual bool IsValid(){ return ! Filepath.IsEmpty(); }

virtual wxString GetFilename(){ return Filepath.AfterLast(wxFILE_SEP_PATH); }  // Note how these methods cleverly mimic those of FileData, so we can use either from a base pointer
virtual wxString ReallyGetName(){ return GetFilename(); }
virtual wxString GetOwner(){ return Ownername; }
virtual wxString GetGroup(){ return Groupname; }
virtual wxULongLong Size(){ return size; }
virtual wxString GetParsedSize(){ return ParseSize(size, false); }
virtual time_t ModificationTime(){ return ModTime; }
virtual size_t GetPermissions(){ return Permissions; }
virtual wxString PermissionsToText();

virtual wxString GetSymlinkDestination(bool AllowRelative=false){ return Type==SYMTYPE ? symdest : wxT(""); }
void SetSymlinkDestination(wxString& dest){ symdest = dest; }
virtual bool IsSymlinktargetADir(bool recursing = false){ return false; }

protected:
enum ffscomp Compare(FakeFiledata* comparator, FakeFiledata* candidate);  // Finds how the candidate file/dir is related to the comparator one

wxString Filepath;
wxULongLong size;
time_t ModTime;
size_t Permissions;
wxString Ownername;
wxString Groupname;
wxString symdest;
};

class FakeDir;
WX_DEFINE_ARRAY(DataBase*, FakeFiledataArray);
WX_DEFINE_ARRAY(FakeDir*, FakeDirdataArray);

class FakeDir  : public FakeFiledata    // Manages the data for a single (sub)dir in a FakeFilesystem
{
public:
FakeDir(wxString name, wxULongLong size=0, time_t Time=(time_t)0, size_t Perms=0664, const wxString& Owner=wxT(""), const wxString& Group=wxT(""), FakeDir* parent=NULL)
                                                              :  FakeFiledata(name, size, Time, DIRTYPE, wxT(""), Perms, Owner, Group), parentdir(parent)
          { filedataarray = new FakeFiledataArray; dirdataarray = new FakeDirdataArray;
            if (name.IsEmpty()) name = wxFILE_SEP_PATH;
            if (name.GetChar(0) != wxFILE_SEP_PATH) name = wxFILE_SEP_PATH + name;    // We're probably going to be fed obvious dirs, but just in case...
            if (name.GetChar(name.Len()-1) != wxFILE_SEP_PATH) name += wxFILE_SEP_PATH;
          }
FakeDir(const FakeDir& fd){ *this = fd; filedataarray = new FakeFiledataArray; dirdataarray = new FakeDirdataArray; }
virtual ~FakeDir(){ Clear(); delete dirdataarray; delete filedataarray; }
void Clear();

FakeDir& operator=(const FakeDir& fd){ Filepath = fd.Filepath; size = ((FakeDir&)fd).GetSize(); ModTime = fd.ModTime; Type = fd.Type;
                                           Permissions = fd.Permissions; Ownername = fd.Ownername; Groupname = fd.Groupname; 
                                           parentdir = fd.parentdir; /*filedataarray = fd.filedataarray; dirdataarray = fd.dirdataarray;*/ return *this; }

enum ffscomp  AddFile(FakeFiledata* file);                    // Adds a file to us or to our correct child, creating subdirs if need be; or not if it doesn't belong
enum ffscomp  AddDir(FakeDir* dir);                           // Ditto dir
void AddChildFile(FakeFiledata* file){ filedataarray->Add(file); }  // Adds file to our filedataarray, no questions asked
void AddChildDir(FakeDir* dir){ dirdataarray->Add(dir); }   // Adds dir to our dirdataarray, no questions asked

DataBase* GetFakeFiledata(size_t index ){ if (index > FileCount()) return NULL; return filedataarray->Item(index); }
FakeDir* FindSubdirByFilepath(const wxString& targetfilepath);
FakeFiledata* FindFileByName(wxString filepath);
bool HasSubDirs(wxString& dirname);                   // Find a FakeDir with this name, and see if it has subdirs
bool HasFiles(wxString& dirname);                     // Find a FakeDir with this name, and see if it has files
FakeDir* GetFakeDir(size_t index ){ if (index > SubDirCount()) return NULL; return dirdataarray->Item(index); }
size_t FileCount(){ return filedataarray->GetCount(); }
size_t SubDirCount(){ return dirdataarray->GetCount(); }
bool HasFileCalled(wxString filename){ return GetFakeFile(filename) != NULL; }
FakeFiledata* GetFakeFile(wxString filename){ if (filename.IsEmpty()) return NULL; 
                                  for (size_t n=0; n<FileCount(); ++n) if (filedataarray->Item(n)->GetFilename() == filename) return (FakeFiledata*)filedataarray->Item(n); return NULL; }
bool HasDirCalled(wxString dirname){ if (dirname.IsEmpty()) return false; 
                                  for (size_t n=0; n<SubDirCount(); ++n) if (dirdataarray->Item(n)->GetFilename() == dirname) return true; return false; }
void GetDescendants(wxArrayString& array, bool dirs_only = false); // Fill the array with all its files & (recursively) its subdirs
wxULongLong GetSize();                                  // Returns the size of its files
wxULongLong GetTotalSize();                             // Returns the size of all its files & those of its subdirs
FakeFiledataArray* GetFiles(){ return filedataarray; }
bool GetFilepaths(wxArrayString& array);
FakeDirdataArray* GetDirs(){ return dirdataarray; }

virtual wxString GetFilename(){ wxString fp(Filepath.BeforeLast(wxFILE_SEP_PATH)); return fp.AfterLast(wxFILE_SEP_PATH); }  // For fake dirs, return the last chunk of the filepath
bool IsRegularFile(){ return false; }
bool IsDir(){ return true; }

protected:

FakeFiledataArray* filedataarray;
FakeDirdataArray* dirdataarray;                // Holds child dirs
FakeDir* parentdir;
};


class FakeFilesystem    // Holds the data eg filenames, sizes, dir structure, of the (perhaps partial) contents of an archive
{
public:
FakeFilesystem(wxString filepath, wxULongLong size, time_t Time, size_t Perms, wxString Owner=wxEmptyString, wxString Group=wxEmptyString);
~FakeFilesystem(){ delete rootdir; }

void Clear(){ currentdir = rootdir; rootdir->Clear(); }
void AddItem(wxString filepath,  wxULongLong size, time_t Time, size_t Perms, DB_filetype Type, const wxString& Target=wxT(""), const wxString& Owner=wxT(""), const wxString& Group=wxT(""));
FakeFiledataArray* GetFiles(){ return currentdir->GetFiles(); }  // Returns the files held by currentdir
FakeDir* GetRootDir(){ return rootdir; }
FakeDir* GetCurrentDir(){ return currentdir; }
void SetCurrentDir(FakeDir* newdir){ currentdir = newdir; }

protected:

FakeDir* rootdir;
FakeDir* currentdir;
};


class MyGenericDirCtrl;

#include <wx/ptr_scpd.h>

wxDECLARE_SCOPED_PTR(wxInputStream, wxInputStreamPtr)
wxDECLARE_SCOPED_PTR(wxOutputStream, wxOutputStreamPtr)

class ArchiveStream
{
public:
ArchiveStream(DataBase* archive);
ArchiveStream(wxString archivename);
virtual ~ArchiveStream(){ delete ffs; delete m_membuf; m_membuf=NULL; }

bool LoadToBuffer(wxString filepath);     // Used by the subclassed ctors to load the archive into m_membuf
bool LoadToBuffer(wxMemoryBuffer* mbuf);  // Reload m_membuf with data from a different pane's buffer

FakeFiledataArray* GetFiles();            // Returns the files for the current 'path'
bool GetDirs(wxArrayString& dirs);        // Returns in the arraystring the subdirs for the current 'path'
bool SetPath(const wxString& subdir);     // Makes subdir the new 'startdir' within the archive. Returns false if it's not a valid subdir
bool GetDescendants(wxString dirName, wxArrayString& filepaths); // Returns in the array all files, dirs & subdirs files for the path

bool OnExtract();                         // On Extract from context menu. Finds what to extract, then calls OnExtract
bool DoExtract(wxArrayString& filepaths, wxString& destpath, wxArrayString& destinations, bool DontUndo=false, bool dirs_only = false);  // From OnExtract or D'n'D/Paste
bool Alter(wxArrayString& filepaths, enum alterarc DoWhich, wxArrayString& newnames, bool dirs_only = false);  // This might be add remove del or rename, depending on the enum

bool MovePaste(MyGenericDirCtrl* originctrl, MyGenericDirCtrl* destctrl, wxArrayString& filepaths, wxString& destpath, bool Moving=true, bool dirs_only = false);
bool DoThingsWithinSameArchive(MyGenericDirCtrl* originctrl, MyGenericDirCtrl* destctrl, wxArrayString& filearray, wxString& destinationpath);

virtual bool Extract(wxArrayString& filepaths, wxString destpath, wxArrayString& destinations, bool dirs_only = false, bool files_only = false)=0;  // Create and extract filename from the archive
virtual wxMemoryBuffer* ExtractToBuffer(wxString filepath)=0; // Extract filepath from this archive into a new wxMemoryBuffer
virtual void ListContentsFromBuffer(wxString archivename)=0;  // Extract m_membuf and list the contents

virtual void RefreshFromDirtyChild(wxString archivename, ArchiveStream* child)=0;  // Used when a nested child archive has been altered; reload the altered version

FakeFilesystem* Getffs() { return ffs; }
wxMemoryBuffer* GetBuffer(){ return m_membuf; } // Used in RefreshFromDirtyChild()

static bool IsStreamable(enum ziptype ztype);
bool IsWithin(wxString filepath);
bool IsDirty(){ return Dirty; }
bool IsNested(){ return Nested; }
bool IsValid(){ return Valid; }
wxString GetArchiveName(){ if (ffs != NULL) return ffs->GetRootDir()->GetFilepath().BeforeLast(wxFILE_SEP_PATH); return wxEmptyString; }

protected:
bool SortOutNames(const wxString& filepath);  // Subroutine used in Extract etc, so made into a function in the name of code reuse
bool AdjustFilepaths(const wxArrayString& filepaths, const wxString& destpath, wxArrayString& newnames, adjFPs whichtype=afp_neither, bool dirs_only = false, bool files_only = false);  // Similar for multiple strings, but recursive for dirs
bool SaveBuffer();                            // Saves the compressed archive in buffer back to the filesystem

virtual void GetFromBuffer(bool dup = false)=0;     // Get the stored archive from membuf into the stream, through the uncompressing filter
virtual bool DoAlteration(wxArrayString& filepaths, enum alterarc DoWhich, const wxString& originroot=wxT(""), bool dirs_only = false)=0;  // Implement add remove del or rename, depending on the enum
virtual void ListContents(wxString archivename)=0;  // Used by ListContentsFromBuffer

wxString archivename;
wxString WithinArchiveName;
wxArrayString WithinArchiveNames;
wxArrayString WithinArchiveNewNames;      // Used if Rename()ing
bool IsDir;

wxInputStreamPtr MemInStreamPtr;          // Using this avoids contorsions to prevent memory leaks
wxInputStreamPtr zlibInStreamPtr;         // Ditto
wxInputStreamPtr DupMemInStreamPtr;       // SImilarly, when duplicating (so we need separate pointers)
wxInputStreamPtr DupzlibInStreamPtr;      // Ditto
wxInputStreamPtr InStreamPtr;             // Used to hold instreams to pass between functions
wxOutputStreamPtr OutStreamPtr;           // Ditto outstreams

wxMemoryBuffer* m_membuf;
FakeFilesystem* ffs;                      // The root dir, or subdir, within the archive that is currently the startdir of the dirview
bool Valid;
bool Nested;
bool Dirty;                               // This (nested) archive has been altered, and so needs saving to its parent during Pop
};

class ZipArchiveStream  :  public ArchiveStream
{
public:
ZipArchiveStream(DataBase* archive);
ZipArchiveStream(wxMemoryBuffer* membuf, wxString archivename);
~ZipArchiveStream(){}

bool Extract(wxArrayString& filepaths, wxString destpath, wxArrayString& destinations, bool dirs_only = false, bool files_only = false);  // Create and extract filename from the archive
virtual void ListContentsFromBuffer(wxString archivename);  // Extract m_membuf and list the contents
wxMemoryBuffer* ExtractToBuffer(wxString filepath);         // Extract filepath from this archive into a new wxMemoryBuffer

virtual void RefreshFromDirtyChild(wxString archivename, ArchiveStream* child);  // Used when a nested child archive has been altered; reload the altered version

protected:
bool DoAlteration(wxArrayString& filepaths, enum alterarc DoWhich, const wxString& originroot=wxT(""), bool dirs_only = false);  // Implement add remove del or rename, depending on the enum
virtual void GetFromBuffer(bool dup = false);               // Get the stored archive from membuf into the stream, through the uncompressing filter
virtual void ListContents(wxString archivename);            // Used by ListContentsFromBuffer
};

class TarArchiveStream  :  public ArchiveStream
{
public:
TarArchiveStream(DataBase* archive);
TarArchiveStream(wxMemoryBuffer* membuf, wxString archivename);
~TarArchiveStream(){ InStreamPtr.reset(); }

bool Extract(wxArrayString& filepaths, wxString destpath, wxArrayString& destinations, bool dirs_only = false, bool files_only = false);  // Create and extract filename from the archive
wxMemoryBuffer* ExtractToBuffer(wxString filepath);         // Extract filepath from this archive into a new wxMemoryBuffer
virtual void ListContentsFromBuffer(wxString archivename);  // Extract m_membuf and list the contents

virtual void RefreshFromDirtyChild(wxString archivename, ArchiveStream* child);  // Used when a nested child archive has been altered; reload the altered version

protected:
void ListContents(wxString archivename);                    // Used by ListContentsFromBuffer
virtual void GetFromBuffer(bool dup = false);               // Get the stored archive from membuf into the stream. No filter in baseclass
virtual bool CompressStream(wxOutputStream* outstream);     // Does nothing, but we need this in GZArchiveStream etc
bool DoAlteration(wxArrayString& filepaths, enum alterarc DoWhich, const wxString& originroot=wxT(""), bool dirs_only = false);  // Implement add remove del or rename, depending on the enum
bool DoAdd(const wxString& filepath, wxTarOutputStream& taroutstream, const wxString& originroot, bool dirs_only = false); // Adds an entry to a tarstream
};


class GZArchiveStream  :  public TarArchiveStream
{
public:
GZArchiveStream(DataBase* archive) : TarArchiveStream(archive) {}
GZArchiveStream(wxMemoryBuffer* membuf, wxString archivename) : TarArchiveStream(membuf, archivename) {}
~GZArchiveStream(){}
virtual void ListContentsFromBuffer(wxString archivename);  // Extract m_membuf and list the contents

protected:
virtual void GetFromBuffer(bool dup = false);               // Get the stored archive from membuf into the stream, through the uncompressing filter
bool CompressStream(wxOutputStream* outstream);             // Used to compress appropriately the output stream following Add, Rename etc
};

class BZArchiveStream  :  public TarArchiveStream
{
public:
BZArchiveStream(DataBase* archive) : TarArchiveStream(archive) {}
BZArchiveStream(wxMemoryBuffer* membuf, wxString archivename) : TarArchiveStream(membuf, archivename) {}
~BZArchiveStream(){}
virtual void ListContentsFromBuffer(wxString archivename);  // Extract m_membuf and list the contents

protected:
virtual void GetFromBuffer(bool dup = false);               // Get the stored archive from membuf into the stream, through the uncompressing filter
bool CompressStream(wxOutputStream* outstream);             // Used to compress appropriately the output stream following Add, Rename etc
};

#ifndef NO_LZMA_ARCHIVE_STREAMS
class XZArchiveStream  :  public TarArchiveStream
{
public:
XZArchiveStream(DataBase* archive, enum ziptype zt = zt_xz) : TarArchiveStream(archive), m_zt(zt) {}
XZArchiveStream(wxMemoryBuffer* membuf, wxString archivename, enum ziptype zt = zt_xz) :TarArchiveStream(membuf, archivename), m_zt(zt) {}
~XZArchiveStream(){}
virtual void ListContentsFromBuffer(wxString archivename);    // Extract m_membuf and list the contents

protected:
virtual void GetFromBuffer(bool dup = false);                 // Get the stored archive from membuf into the stream, through the uncompressing filter
bool CompressStream(wxOutputStream* outstream);               // Used to compress appropriately the output stream following Add, Rename etc
enum ziptype m_zt;
};
#endif // ndef NO_LZMA_ARCHIVE_STREAMS

class DirBase  // Base for mywxDir and ArcDir, so that I can use base pointers
{
public:
virtual ~DirBase(){}
virtual bool IsOpened()=0;
virtual bool Open(wxString dirName)=0;
virtual bool GetFirst(wxString* filename, const wxString& filespec = wxEmptyString, int flags = 0)=0;
virtual bool GetNext(wxString* filename)=0;
};

class mywxDir  :  public DirBase, public wxDir  // A way of calling wxDir from a base pointer
{
public:
mywxDir(){}
~mywxDir(){}
bool IsOpened(){ return wxDir::IsOpened(); }
bool Open(wxString dirName){ return wxDir::Open(dirName); }
bool GetFirst(wxString* filename, const wxString& filespec = wxEmptyString, int flags = 0)  { return wxDir::GetFirst(filename, filespec, flags); }
bool GetNext(wxString* filename)  { return wxDir::GetNext(filename); }
};

class ArcDir  :  public DirBase  // A version of wxDir for FakeDirs
{
public:
ArcDir(ArchiveStream* arch) : arc(arch) { nextfile = -1; isopen = true;}
~ArcDir(){}

bool IsOpened(){ return isopen; }
bool Open(wxString dirName);
bool GetFirst(wxString* filename, const wxString& filespec = wxEmptyString, int flags = 0);
bool GetNext(wxString* filename);

protected:
void SetFileSpec(const wxString& filespec) { m_filespec = filespec.c_str(); }
void SetFlags(int flags) { m_flags = flags; }

wxString startdir;
ArchiveStream* arc;
bool isopen;
wxString m_filespec;
int m_flags;
int nextfile;
wxArrayString filepaths;
};

struct ArchiveStruct
{
ArchiveStruct(ArchiveStream* Arc, ziptype archivetype) : arc(Arc), ArchiveType(archivetype) {}
ArchiveStream* arc;                            // Pointer to this ArchiveStream
enum ziptype ArchiveType;                      //  and which type this is (or NotArchive)
};

WX_DEFINE_ARRAY(ArchiveStruct*, ArchiveStructArray);

class ArchiveStreamMan    // Manages which archive we're peering into
{
public:
ArchiveStreamMan(){ arc = NULL; ArchiveType = zt_invalid; ArcArray = new ArchiveStructArray; }
~ArchiveStreamMan(){ while (Pop()) // Pop() undoes any Pushed archives, deleting as it goes
                        ; 
                     delete ArcArray; 
                   }     
ArchiveStream* GetArc() { return arc; }
enum ziptype GetArchiveType(){ return ArchiveType; }
bool IsArchive(){ return GetArchiveType() != zt_invalid; }
bool IsWithinThisArchive(wxString filepath);
bool IsWithinArchive(wxString filepath);                    // Is filepath within the archive system i.e. the original archive itself, or 1 of its contents, or that of a nested archive
wxString GetPathOutsideArchive(){ return OutOfArchivePath; }// This is where, by default, to dump extracted files
bool NewArc(wxString filepath, enum ziptype NewArchiveType);
bool RemoveArc(){ return Pop(); }
enum NavigateArcResult NavigateArchives(wxString filepath); // If filepath is in an archive, goto the archive. Otherwise don't
wxString ExtractToTempfile();                               // Called by FiletypeManager::Openfunction. Uses ExtractToTempfile(wxArrayString& filepaths)
wxArrayString ExtractToTempfile(wxArrayString& filepaths, bool dirs_only = false, bool files_only = false);  // Called by e.g. EditorBitmapButton::OnEndDrag to provide a extracted tempfile

protected:
void Push();
bool Pop();
enum OCA OpenCorrectArchive(wxString& filepath);            // See if filepath is (within) a, possibly nested, archive. If so, open it
bool MightFilepathBeWithinChildArchive(wxString filepath);  // See if filepath is longer than the current rootdir. Assume we've just checked it's not in *this* archive
wxString FindFirstArchiveinFilepath(wxString filepath);     // FIlepath will be a fragment of a path. Find the first thing that looks like an archive

bool WasOriginallyFulltree;
wxString OriginalArchive;                      // The filepath of the archive (or original archive if nested)
wxString OutOfArchivePath;                     // The filepath holding the (original)archive, which is the default location to put any extracted files
ArchiveStructArray* ArcArray;                  // Stores Push()ed structs when we're inside an archive that's inside an archive thats...
ArchiveStream* arc;                            // Holds the currently active pointer to the relevant class, or null
enum ziptype ArchiveType;                      //  and which type this is (or NotArchive)
};

#endif
    //ARCHIVESTREAMH



