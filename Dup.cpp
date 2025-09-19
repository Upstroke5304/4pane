/////////////////////////////////////////////////////////////////////////////
// Name:       Dup.cpp
// Purpose:    Checks/manages Duplication & Renaming
// Part of:    4Pane
// Author:     David Hart
// Copyright:  (c) 2019 David Hart
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
#include "wx/spinbutt.h"
#include "wx/filename.h"
#include "wx/dir.h"
#include "wx/regex.h"

#include "Externs.h"
#include "MyDirs.h"
#include "MyGenericDirCtrl.h"
#include "MyFrame.h"
#include "Filetypes.h"
#include "Redo.h"
#include "Accelerators.h"
#include "Configure.h"
#include "Misc.h"
#include "Dup.h"


bool CheckDupRen::CheckForAccess(bool recurse /*=true*/) // Check for read permission when in a Moving/Pasting loop, or Dup/Rename
{
FileData stat(incomingpath);
if (!stat.IsValid()) return false;

if (!stat.CanTHISUserRead())
  { if (WhattodoifCantRead == DR_SkipAll) return false;  // See if we've a stored choice for when there's a clash ie SkipAll

    if (!IsMultiple)                              // Single file so simple dialog
      { wxString fp(incomingpath); if (fp.Len() > 100) fp = wxT("...") + fp.Right(100);
        wxString msg; msg.Printf(wxT("%s\n%s"), _("I'm afraid you don't have Read permission for the file"), fp.c_str());
        wxMessageBox(msg); return false; 
      }
    
    MyButtonDialog dlg;                           // If multiple & no stored choice, ask
    wxXmlResource::Get()->LoadDialog(&dlg, parent, wxT("CantReadMultipleDlg"));
    wxStaticText* stt = (wxStaticText*)dlg.FindWindow(wxT("Filename"));
    stt->SetLabel(TruncatePathtoFitBox(incomingpath, stt));
                    
    int answer = dlg.ShowModal();                 // Call the dialog to announce read-permission failure, & if multiple allows the user to skip all such
    
    if (answer == XRCID("SkipAll"))  WhattodoifCantRead = DR_SkipAll;  // If the result is SkipAll (must have been a multiple dlg), store for the future
    return false;
  }

if (!stat.IsDir() || !recurse)  return true;      // If we're not dealing with a dir, or if we're renaming & so don't care about contents, we're done

                  // Since this IS a dir, we need to check its contents, by recursion
wxDir dir(incomingpath);
if (!dir.IsOpened())        return false;

wxString filename; bool AtLeastOneSuccess = false;

bool cont = dir.GetFirst(&filename); if (!cont) return true;        // If the dir has no contents, we're OK
while  (cont)
  { wxString filepath = incomingpath + wxFILE_SEP_PATH + filename;  // Make filename into filepath
  
    CheckDupRen RecursingCheckIt(parent, filepath, wxT(""));        // Make another instance of this class
    RecursingCheckIt.IsMultiple = IsMultiple;     // Duplicate the flags
    RecursingCheckIt.WhattodoifCantRead = WhattodoifCantRead;
    
    bool ans = RecursingCheckIt.CheckForAccess(); // Recursively investigate
    
    WhattodoifCantRead = RecursingCheckIt.WhattodoifCantRead;  // Save any new SkipAll.  This may have happened even if ans==true, since there may have been some failures too
    if (ans)  AtLeastOneSuccess = true;           // Record the lack of total failure, so that we don't return false
    cont = dir.GetNext(&filename);                // and try for another file
  }

return AtLeastOneSuccess;  // which will be true if there has been >0 successes.  For the failures, the calling function will have to rely on wxCopyFile (or whatever) silently failing
}

bool CheckDupRen::CheckForIncest()  // Ensure we don't try to move a dir onto a descendant --- which would cause an indef loop
{
FileData fdin(incomingpath);
if (!fdin.IsDir())    return false;                // Pasting a FILE etc onto a descendant dir is not a problem

                // Make into wxFileNames to allow relative paths 2b made absolute
wxFileName dest;
wxFileName in(incomingpath + wxFILE_SEP_PATH);     // The '/' will do no harm, & may do good
FileData fddest(destpath);
if (!fddest.IsDir())
       dest.Assign(destpath);                      // If it's a file, the filename will be separated from the path, so we can use GetPath correctly below
 else  dest.Assign(destpath + wxFILE_SEP_PATH);
in.MakeAbsolute(); dest.MakeAbsolute();

if (dest.GetPath().Find(in.GetPath()) == wxNOT_FOUND) return false;  // See if there's any overlap.  Note use of GetPath rather than GetFULLPath

        // Hmm, it's not looking good. But check that we're not trying to paste /path/to/foo/ onto /path/to/foobar/ (which is OK but would have failed above)
wxArrayString indirs(in.GetDirs()); wxArrayString destdirs(dest.GetDirs());
wxString inlastseg = indirs.Item(indirs.GetCount()-1); wxString destlastseg = destdirs.Item(indirs.GetCount()-1);
if (destlastseg.Len() > inlastseg.Len()) return false;  // If the last segment of in is shorter than the equivalent segment of dest, we're OK

if (dest.GetPath() == in.GetPath()) return false;  // The dirs are identical.  This will be sorted by CheckForPreExistence(), so let it go here

                // If we're here, there's a problem
wxDialog dlg;                                      // Tell the user he's immoral
wxXmlResource::Get()->LoadDialog(&dlg, parent, _T("IncestDlg"));
((wxStaticText*)dlg.FindWindow(wxT("frompath")))->SetLabel(incomingpath);
((wxStaticText*)dlg.FindWindow(wxT("topath")))->SetLabel(dest.GetPath());
dlg.GetSizer()->Fit(&dlg);
dlg.ShowModal();
return true;                                       // True tells the calling method to abort
}


CDR_Result CheckDupRen::CheckForPreExistence()  // Ensure we don't accidentally overwrite
{
wxString inpath, destination;

FileData deststat(destpath);                       // We need to use FileData, as wxFileExists() can't (currently) cope with symlinks, esp broken ones
if (!deststat.IsDir())                             // If destination is a path/file, split it.
  { destination = destpath.Left(destpath.Find(wxFILE_SEP_PATH, true)+1);  
    destname = finalbit = destpath.Right(destpath.Len() - (destpath.Find(wxFILE_SEP_PATH, true)+1));
  }
 else 
   { if (destpath.Last() != wxFILE_SEP_PATH) destpath += wxFILE_SEP_PATH;  // To avoid problems comparing '/home/' with '/home'
    destination = destpath;
    if (!wxDirExists(destination))                  // Otherwise check it IS a valid dir
        { wxLogError(_("There seems to be a problem:  the destination directory doesn't exist!  Sorry.")); return CDR_skip; }
  }

if (!FromArchive)                                   // If origin is an archive, ItsADir is already set
  { FileData incomingstat(incomingpath); ItsADir = incomingstat.IsDir(); }
          
if (!ItsADir)                                       // Ditto incomingpath                  
  { inpath = incomingpath.Left(incomingpath.Find(wxFILE_SEP_PATH, true)+1);
    inname = incomingpath.Right(incomingpath.Len() - (incomingpath.Find(wxFILE_SEP_PATH, true)+1));
  }
  else 
   { if (incomingpath.Last() != wxFILE_SEP_PATH) incomingpath += wxFILE_SEP_PATH;
    inpath = incomingpath;
    if (!FromArchive && !wxDirExists(inpath))    { wxLogError(_("There seems to be a problem:  the incoming directory doesn't exist!  Sorry.")); return CDR_skip; }
  }

dup = (destination==inpath);                        // Flags whether the user is being stupid/trying to duplicate

bool clash = false;
if (ItsADir)
  {     // First fill finalbit & finalpath ready for Paste.  If there's a clash they may be amended later
    stubpath = inpath.BeforeLast(wxFILE_SEP_PATH);  // Borrow the wxString stubpath to chop off the terminal /
    finalbit = stubpath.AfterLast(wxFILE_SEP_PATH); // Then find the terminal bit
    finalpath = destpath;                           // This is the path to paste to
          // A clash occurs if a) We try to paste onto ourself, or b) onto our parent dir, or c) If we try to paste /x/y/foo/ into /a/b/foo/
    if (destination==inpath)                        // If it's a direct dup, ie /a/b/foo -> a/b/foo
      { stubpath = destpath.BeforeLast(wxFILE_SEP_PATH);                   // Take the opportunity to remove the last segment from the destpath:  /a/b/foo/ -> /a/b/
        lastseg = stubpath.AfterLast(wxFILE_SEP_PATH);                      // Save the foo bit
        stubpath = stubpath.BeforeLast(wxFILE_SEP_PATH) + wxFILE_SEP_PATH;  // We'll need both of these later
        finalpath = stubpath;                       // As this is a dup (if it isn't cancelled), we need to paste onto the stub, not the full destpath
        clash = true;
      }
     else if (wxDirExists(destination + (inpath.BeforeLast(wxFILE_SEP_PATH)).AfterLast(wxFILE_SEP_PATH)))
      { stubpath = destination;                     // This is already the bit before the last segment from the inpath:  /a/b/
        lastseg  = (inpath.BeforeLast(wxFILE_SEP_PATH)).AfterLast(wxFILE_SEP_PATH); // Save the foo bit
        if (destpath==destination)
          { destpath = destpath + lastseg+ wxFILE_SEP_PATH;  //  & also add it the destpath if necessary ie if trying to paste /a/b/foo to /a/b
            if (inpath==destpath)    dup = true;             // If so, we ARE dup.ing after all
          }
        clash = true;
      }
                // We've checked for the clash, now do something if there is one.
    if (clash)
      return DirAdjustForPreExistence();
     else 
      return CDR_ok;                                // This is for when there wasn't a clash anyway
   }    
 else                                 // It's a file
  {
  destpath = finalpath = destination;               // This changes destpath from path/file to path/    Grab provisional finalpath at the same time
  finalbit = inname;                                // Similarly, grab provisional finalbit.  If there isn't a nameclash, these will be the new dest filepath
  FileData destfilepath(destpath + inname);
  if (destfilepath.IsValid())                       // We need to use FileData here as wxFileExists can't cope with broken symlinks
    return FileAdjustForPreExistence();  

  return CDR_ok;                                    // This is for when there wasn't a clash anyway
  }
}


int CheckDupRen::CheckForPreExistenceInArchive()  // If there's a clash, do we duplicate, overwrite or skip?
{
int result;
wxString inpath, destination(destpath);
FakeFilesystem* destffs = parent->arcman->GetArc()->Getffs();
bool ItsADir = false;
                          // First, find if destination is a dir. If not, make it so
FakeFiledata* fd = destffs->GetRootDir()->FindSubdirByFilepath(destpath);    // Dirs aren't '/'-terminated, so just try it and see
if (!fd)                                          // It's not a dir, let's hope it's a file
  { fd = destffs->GetRootDir()->FindFileByName(destpath);
    if (fd==NULL) { result=false; return false; } // If we're in a rubbish situation, bail out

    destination = destpath.Left(destpath.Find(wxFILE_SEP_PATH, true)+1);
  }
if (destination.Right(1) != wxFILE_SEP_PATH) { destination << wxFILE_SEP_PATH; }

if (!FromArchive)                                // If origin is an archive, ItsADir is already set
  { FileData incomingstat(incomingpath); ItsADir = incomingstat.IsDir(); }
          
if (!ItsADir)                                    // Ditto incomingpath                  
  { inpath = incomingpath.Left(incomingpath.Find(wxFILE_SEP_PATH, true)+1);
    inname = incomingpath.Right(incomingpath.Len() - (incomingpath.Find(wxFILE_SEP_PATH, true)+1));
  }
  else 
   { if (incomingpath.Last() != wxFILE_SEP_PATH) incomingpath += wxFILE_SEP_PATH;
    inpath = incomingpath;
    if (!FromArchive && !wxDirExists(inpath))    { wxLogError(_("There seems to be a problem:  the incoming directory doesn't exist!  Sorry.")); return false; }
  }

dup = (destination==inpath);                      // Flags whether the user is being stupid/trying to duplicate

bool clash = false;
if (ItsADir)
  {     // First fill finalbit & finalpath ready for Paste.  If there's a clash they may be amended later
    stubpath = inpath.BeforeLast(wxFILE_SEP_PATH);  // Borrow the wxString stubpath to chop off the terminal /
    finalbit = stubpath.AfterLast(wxFILE_SEP_PATH); // Then find the terminal bit
    finalpath = destpath;                           // This is the path to paste to
          // A clash occurs if a) We try to paste onto ourself, or b) onto our parent dir, or c) If we try to paste /x/y/foo/ into /a/b/foo/
    if (destination==inpath)                      // If it's a direct dup, ie /a/b/foo -> a/b/foo
      { stubpath = destpath.BeforeLast(wxFILE_SEP_PATH);                    // Take the opportunity to remove the last segment from the destpath:  /a/b/foo/ -> /a/b/
        lastseg = stubpath.AfterLast(wxFILE_SEP_PATH);                      // Save the foo bit
        stubpath = stubpath.BeforeLast(wxFILE_SEP_PATH) + wxFILE_SEP_PATH;  // We'll need both of these later
        finalpath = stubpath;                     // As this is a dup (if it isn't cancelled), we need to paste onto the stub, not the full destpath
        clash = true;
      }
     else if (destffs->GetRootDir()->FindSubdirByFilepath(destination + (inpath.BeforeLast(wxFILE_SEP_PATH)).AfterLast(wxFILE_SEP_PATH)))
      { stubpath = destination;                   // This is already the bit before the last segment from the inpath:  /a/b/
        lastseg   = (inpath.BeforeLast(wxFILE_SEP_PATH)).AfterLast(wxFILE_SEP_PATH);    // Save the foo bit
        if (destpath==destination)
          { destpath = destpath + lastseg+ wxFILE_SEP_PATH;  //  & also add it the destpath if necessary ie if trying to paste /a/b/foo to /a/b
            if (inpath==destpath)    dup = true;             // If so, we ARE dup.ing after all
          }
        clash = true;
      }
        // We've checked for the clash, now do something if there is one.
    if (clash)
      { // We can't store 2 dirs in an archive (it's too complex) or rename the incoming one (it's far too complex) so abort
        wxString msg1 = _("There's already a directory in the archive with the name ");
        wxString msg2 = _("\nI suggest you rename either it, or the incoming directory");
        wxMessageBox(msg1 + lastseg + msg2, wxT(""), wxICON_INFORMATION | wxOK);
        return false;
      }
     else 
        return 1;                             // This is for when there wasn't a clash anyway
   }    
 else                                 // It's a file
  {
  destpath = finalpath = destination;         // This changes destpath from path/file to path/    Grab finalpath at the same time
  finalbit = inname;                          // Similarly, grab provisional finalbit: going into an archive it won't change, so together these will be the new dest filepath
  fd = destffs->GetRootDir()->FindFileByName(destpath + inname);
  if (fd)
    { result = FileAdjustForPreExistence(true, fd->ModificationTime());  
      return result;                          // !result means skip, result==2 means "OK, go ahead with the paste (or whatever)"
    }
  return true;                                // This is for when there wasn't a clash anyway
  }
}

CDR_Result CheckDupRen::DirAdjustForPreExistence(bool intoArchive/*=false*/ , time_t modtime /*= 0*/)
{
enum DupRen Whattodo = WhattodoifClash;
if (Whattodo == DR_Unknown)                                     // See if we've a stored choice for when there's a clash eg SkipAll
  Whattodo = OfferToRename(2 + dup, intoArchive, modtime);      // If origin==dest, use the QueryIdiot dialog, otherwise be more polite.  +2 flags Dir

switch(Whattodo)                 // NB For Dirs, 'Overwrite' isn't a valid answer: it's only for files
  { case DR_Cancel:    WhattodoifClash = DR_Cancel; return CDR_skip;// To abort the rest of the items
  
    case DR_SkipAll:   WhattodoifClash = DR_SkipAll;            // Store this in variable, then fall thru into:
    case DR_Skip:      return CDR_skip;
    case DR_RenameAll: WhattodoifClash = DR_RenameAll;          // Store this in variable, then fall thru into:
    case DR_Rename:    if (!GetNewName(stubpath, lastseg)) return CDR_skip; // Ask for a new name for the terminal segment of path
                       if (dup)                                      // If we're duping, we don't actually want to rename the original dir, just dup it
                        { wxString origfilepath = stubpath + lastseg, newfilepath = stubpath + newname;
                          int answer = RenameDir(origfilepath, newfilepath, true);
                          if (answer)                               // If the dup worked, make a new UnRedoDup
                            { wxArrayInt IDs; IDs.Add(parent->GetId());
                              UnRedoDup* UnRedoptr = new UnRedoDup(stubpath, stubpath, lastseg, newname, true, IDs);
                              UnRedoManager::AddEntry(UnRedoptr);
                              MyFrame::mainframe->OnUpdateTrees(stubpath, IDs);
                              return CDR_renamed; // which signifies nothing else needs to be done
                            }
                           else return CDR_skip;
                        }  
                              // Otherwise we really are renaming
                      finalbit = newname;                       // Replace the original subdir-name in finalbit with the new version
                      return CDR_ok;  // return OK here; it's a dir not a file, so we won't save-into-deletecan in a thread
    default:        ;              
  }
return CDR_skip;                                                // This will catch any invalid values of 'Whattodo'
}

CDR_Result CheckDupRen::FileAdjustForPreExistence(bool intoArchive/*=false*/, time_t modtime /*= 0*/)
{
if (destname.IsEmpty())  destname = inname;                     // Lest we're copying from a fileview to a dirview

enum DupRen Whattodo = WhattodoifClash;
if (Whattodo == DR_Unknown)                                     // See if we've a stored choice for when there's a clash eg OverwriteAll
  Whattodo = OfferToRename(dup, intoArchive, modtime);          // If not, ask.  If origin==dest, use the QueryIdiot dialog, otherwise be more polite
  
switch(Whattodo)   
  { case DR_Cancel:       WhattodoifClash = DR_Cancel; return CDR_skip;// To abort the rest of the items
  
    case DR_SkipAll:      WhattodoifClash = DR_SkipAll;             // Store this in variable, then fall thru into:
    case DR_Skip:         return CDR_skip;
    case DR_StoreAll:     WhattodoifClash = DR_StoreAll;            // (Store is only for archives)
    case DR_Store:        return CDR_ok;
    case DR_OverwriteAll: WhattodoifClash = DR_OverwriteAll;        // Store this in variable, then fall thru into:
    case DR_Overwrite:    if (intoArchive) // For archives, the per-item what2do is cached, then all operations done at once, elsewhere
                            { WhattodoifClash = Whattodo; return CDR_ok; } // (Use Whattodo again here,as it might be either Overwrite or OverwriteAll
                  {  // We need to cache the current dest file in a can, for a future undo. Do it here if it's easy
                  wxString trashbindir;
                  if (!m_tsb || m_tsb->GetTrashdir().empty())       // m_tsb may be empty in an archive situation
                    { wxFileName trashdir;                          // Start by creating a unique subdir in trashdir, using current date/time
                      if (!DirectoryForDeletions::GetUptothemomentDirname(trashdir, delcan))
                        { wxMessageBox(_("For some reason, trying to create a dir to temporarily-save the overwritten file failed.  Sorry!")); return CDR_skip; }
                      trashbindir = StrWithSep(trashdir.GetFullPath());
                      if (m_tsb) m_tsb->SetTrashdir(trashbindir);
                    }
                   else trashbindir = m_tsb->GetTrashdir(); // Reuse m_tsb->GetTrashdir() if poss as we want the same trashdir for each item, not hundreds of different ones

                  wxString clashingfile(StrWithSep(destpath) + destname);
                  FileData fd(clashingfile);
                  if (fd.IsRegularFile())
                    m_overwrittenfilepath = StrWithSep(trashbindir) + destname; // For regular files this will be needed for undoing

                  if (fd.IsRegularFile() && !IsThreadlessMovePossible(clashingfile, DirectoryForDeletions::GetDeletedName()))
                    return CDR_overwrite; // It's a file & we can't do it the easy way, so flag we need to use threads
                    

                  // Otherwise it's a symlink or a misc (we won't get here if it's a dir) so we should be able to do a simple non-thread Move
                  wxFileName fn(destpath, destname);
                  wxFileName trashdir(trashbindir, wxT(""));
                  if (!parent->Move(&fn, &trashdir, destname, NULL))
                    return CDR_overwrite; // Despite checking, the simple method failed. So fall back to the thread way
                   else wxSafeYield();    // We must yield now, otherwise FSWatcher delete events from the Move() will clobber subsequent create ones
                  
                  wxArrayInt IDs; IDs.Add(parent->GetId());
                  if (!fd.IsRegularFile()) // For files, unredoing will be dealt with differently
                    { UnRedoMove* UnRedoptr = new UnRedoMove(destpath, IDs, trashdir.GetFullPath(), destname, destname, false, true);  // Make a new unredoMove. NB we don't need to worry about refreshing trashdir
                      UnRedoptr->SetNeedsYield(true); // We need a wxSafeYield() here, otherwise the display updates wrongly
                      UnRedoManager::AddEntry(UnRedoptr);               // and store it for Undoing (The 'true' in the line above signifies "From Delete", so UnRedo knows not to UpdateTrees for the Destination)
                    }
                                                
                  MyFrame::mainframe->OnUpdateTrees(destpath, IDs); // Finally we have to sort out the treectrl
                  return CDR_ok;                      // Return CDR_ok to signify that the paste (or whatever) can go ahead
                  }
                          
    case DR_RenameAll: WhattodoifClash = DR_RenameAll;              // Store this in variable, then fall thru into:
    case DR_Rename:  if (!GetNewName(destpath, destname)) return CDR_skip;
                  if (dup)                                          // If we're duping, we don't actually want to rename the original file, just dup it
                    { int answer = RenameFile(destpath, destname, newname, true);
                      if (answer)                                   // If the dup worked, make a new UnRedoDup
                        { wxArrayInt IDs; IDs.Add(parent->GetId());
                          UnRedoDup* UnRedoptr = new UnRedoDup(destpath, destpath, destname, newname, false, IDs);
                          UnRedoManager::AddEntry(UnRedoptr);
                          MyFrame::mainframe->OnUpdateTrees(destpath, IDs, wxT(""), intoArchive);
                          return CDR_ok;
                        }
                      else return CDR_skip;
                    }  
                                  // Otherwise we really are renaming
                  finalbit = newname;                               // Replace the original name in finalbit with the new version
                  return CDR_ok;                
      default:    ;
  }
return CDR_skip;                                                    // This will catch any invalid values of Whattodo
}

enum DupRen CheckDupRen::OfferToRename(int dialogversion, bool intoArchive/*=false*/ , time_t intomodtime /*= 0*/)
{
MyButtonDialog mydlg;

switch(dialogversion)                  // If dialogversion==0 or 2, use polite dialog as copying same-name file/dir, but different paths
  { 
    case 0:  if (IsMultiple)           // Load polite FILE dlg from xrc:  Rename, overwrite or cancel; multiple or otherwise
              { if (intoArchive)
                  wxXmlResource::Get()->LoadDialog(&mydlg, parent, wxT("QueryArchiveAddMultipleDlg"));
                 else
                  wxXmlResource::Get()->LoadDialog(&mydlg, parent, wxT("QueryRenameMultipleDlg"));
              }
             else
              { if (intoArchive)
                  wxXmlResource::Get()->LoadDialog(&mydlg, parent, wxT("QueryArchiveAddDlg"));
                 else
                  wxXmlResource::Get()->LoadDialog(&mydlg, parent, wxT("QueryRenameDlg"));
              }
              
              { wxStaticText* NameStatic = (wxStaticText*)mydlg.FindWindow(wxT("NameStatic"));
                wxStaticText* CurrentStatic = (wxStaticText*)mydlg.FindWindow(wxT("Current"));
                wxStaticText* IncomingStatic = (wxStaticText*)mydlg.FindWindow(wxT("Incoming"));
                wxStaticText* fn = (wxStaticText*)mydlg.FindWindow(wxT("Filename"));
                if (fn) fn->Hide();

                wxString namestr = NameStatic->GetLabel() + wxT(" ") + destname; 
                NameStatic->SetLabel(namestr);

                time_t fromtime;  // If we're Pasting from an archive, we can't use a FileData to find the mod-time, so it was passed to the ctor
                if (FromArchive) fromtime = FromModTime;
                  else 
                  { FileData in(incomingpath); fromtime = in.ModificationTime(); }
                wxDateTime intime(fromtime);

                time_t totime;    // Ditto to an archive
                if (intoArchive) totime = intomodtime;
                  else 
                  { FileData dest(destpath+destname); totime = dest.ModificationTime(); }
                wxDateTime desttime(totime);

                if (intime.IsSameTime(desttime))
                    CurrentStatic->SetLabel(_("Both files have the same Modification time"));
                 else 
                  { wxString desttimestr, intimestr;
                    if (intime.IsEqualUpTo(desttime, wxTimeSpan::Day()))
                          { intimestr = intime.Format(wxT("%x %R")); desttimestr = desttime.Format(wxT("%x %R")); }
                      else  { intimestr = intime.Format(wxT("%x")); desttimestr = desttime.Format(wxT("%x")); }
                    wxString str(_("The current file was modified on ")); CurrentStatic->SetLabel(str + desttimestr);
                    str = _("The incoming file was modified on "); IncomingStatic->SetLabel(str + intimestr);
                  }
#if wxVERSION_NUMBER >= 3100
                  // Previously this was unnecessary; the statictexts knew their own size. However since wx3.1 they don't; perhaps because of the ellipsize feature
                  NameStatic->SetMinSize(NameStatic->GetSizeFromTextSize(NameStatic->GetTextExtent(namestr)));
                  CurrentStatic->SetMinSize(CurrentStatic->GetSizeFromTextSize(CurrentStatic->GetTextExtent(CurrentStatic->GetLabel())));
                  IncomingStatic->SetMinSize(IncomingStatic->GetSizeFromTextSize(IncomingStatic->GetTextExtent(IncomingStatic->GetLabel())));
#endif

                mydlg.GetSizer()->Fit(&mydlg);
                mydlg.GetSizer()->Layout();     // Needed from 2.9, otherwise the statics don't recentre
              }  
            break;
    case 1: if (!intoArchive) { wxXmlResource::Get()->LoadDialog(&mydlg, parent, wxT("QueryIdiotDlg")); }  // Load ?Idiot dlg:  Dup or cancel FILE
              else { wxXmlResource::Get()->LoadDialog(&mydlg, parent, wxT("QueryArchiveDupDlg")); }
            break;
    case 2:  if (IsMultiple)                    // Load the polite DIR version:  Rename or cancel; multiple or otherwise
              { wxXmlResource::Get()->LoadDialog(&mydlg, parent, wxT("QueryRenameMultipleDirDlg"));
                wxStaticText* NameStatic = (wxStaticText*)mydlg.FindWindow(wxT("Filename"));
                NameStatic->SetLabel(lastseg);

                mydlg.GetSizer()->Layout();     // Needed from 2.9, otherwise the statics don't recentre
              }
             else
               wxXmlResource::Get()->LoadDialog(&mydlg, parent, wxT("QueryRenameDirDlg"));
            break;
    case 3:   { wxXmlResource::Get()->LoadDialog(&mydlg, parent, wxT("QueryIdiotDlg"));       // Load ?Idiot dlg:  Dup or cancel DIR
                wxString msg(_("You seem to want to overwrite this directory with itself"));  // Message for the dir version.  The original says 'file'
                ((wxStaticText*)mydlg.FindWindow(wxT("QueryIdiot_message")))->SetLabel(msg);
              }
             break; 
     default: return DR_Unknown;
  }

if (FromArchive)             // If we're Pasting from an archive, disable rename: we can't conveniently rename if there's a clash
  { wxWindow* rename = mydlg.FindWindow(wxT("Rename")); if (rename) rename->Disable();
    rename = mydlg.FindWindow(wxT("RenameAll")); if (rename) rename->Disable();
  }

int answer = mydlg.ShowModal();                 // Call the dialog that allows the user to (overwrite) rename or cancel

if (answer == XRCID("Rename"))        return DR_Rename;
if (answer == XRCID("RenameAll"))     return DR_RenameAll;
if (answer == XRCID("Store"))         return DR_Store;
if (answer == XRCID("StoreAll"))      return DR_StoreAll;
if (answer == XRCID("Overwrite"))     return DR_Overwrite;
if (answer == XRCID("OverwriteAll"))  return DR_OverwriteAll;
if (answer == XRCID("Skip"))          return DR_Skip;
if (answer == XRCID("SkipAll"))       return DR_SkipAll;
if (answer == XRCID("Cancel") || answer == wxID_CANCEL) return DR_Cancel;

return DR_Unknown;
}

bool CheckDupRen::GetNewName(wxString& destpath, wxString& oldname)
{
wxDialog dlg;
wxXmlResource::Get()->LoadDialog(&dlg, parent, wxT("RenameDlg"));

do
  { wxTextCtrl* text = (wxTextCtrl*)dlg.FindWindow(wxT("newfilename"));
    text->ChangeValue(oldname); text->SetSelection(-1, -1); text->SetFocus(); // Insert the original name & highlight it

    if (dlg.ShowModal() == wxID_CANCEL)    return false;
    newname = text->GetValue();                                    // Get the desired name
    if (newname.IsEmpty())  return false;
    if (newname != oldname)
      { FileData newfpath(destpath+newname);
        if (newfpath.IsValid())
          { wxMessageBox(_("Sorry, that name is already taken.  Please try again.")); continue; }
        else return true;                                          // Return success.  Newname is stored as class variable
      }
    wxMessageBox(_("No, the idea is that you CHANGE the name"));
  } while (1);
  
return false;   // Just to avoid compiler warnings
}

//static
int CheckDupRen::RenameFile(wxString& originpath, wxString& originname, wxString& newname, bool dup)  // Renames or Duplicates
{
wxString oldpath = originpath; if (oldpath.Last() != wxFILE_SEP_PATH) oldpath += wxFILE_SEP_PATH;  // We need the '/' to make the filepaths below

if (!dup) return (wxRenameFile(oldpath+originname, oldpath+newname) ? 2 : false);  // If success, return 2 to tell caller it's a rename, not a dup
                            // If we're here, we're duplicating
FileData stat(oldpath+originname);          // Stat the file, to make sure it's not something else eg a symlink
if (stat.IsSymlink())                       // If it's a symlink, make a new one
  { if (!stat.IsBrokenSymlink())
      { if (!CreateSymlink(stat.GetSymlinkDestination(true), oldpath+newname))  return false; }
     else
      return SafelyCloneSymlink(oldpath+originname, oldpath+newname, stat);
  }
 else       
   if (stat.IsRegularFile())                // If it's a normal file, use built-in wxCopyFile
    { wxBusyCursor busy;
      if (!wxCopyFile(oldpath+originname, oldpath+newname))    // Copy it, with the new instance having a different name
        { wxMessageBox(_("Sorry, I couldn't duplicate the file.  A permissions problem maybe?")); return false; }
    }
 else
    { wxBusyCursor busy;                    // Otherwise it's an oddity like a fifo, which makes wxCopyFile hang, so do it the linux way
      mode_t oldUmask = umask(0);           // Temporarily change umask so that permissions are copied exactly
      bool ans = mknod((oldpath+newname).mb_str(wxConvUTF8), stat.GetStatstruct()->st_mode, stat.GetStatstruct()->st_rdev);  // Technically this works.  Goodness knows if it achieves anything useful!
      umask(oldUmask); return ans==0;
    }
    
return 1;                                   // to flag a successful dup (as opposed to rename)
}

//static 
int CheckDupRen::RenameDir(wxString& destpath, wxString& newpth, bool dup)  // Renames or Duplicates
{
if (!dup)    // If we're renaming rather than duplicating, just do it
     return (wxRenameFile(destpath, newpth) ? 2 : false);  // If success, return 2 to tell caller it's a rename, not a dup
                                        
                                                // If we're here, we're duplicating
wxString origpath = destpath, newpath = newpth; // Copy strings: we' re using references, & they'll probably be altered in sub-method
if (origpath.Last() != wxFILE_SEP_PATH) origpath += wxFILE_SEP_PATH;  // Seem to be needed
if (newpath.Last() != wxFILE_SEP_PATH) newpath += wxFILE_SEP_PATH;

if (!CheckDupRen::DoDup(origpath, newpath))     // Now do the dup in sub-method
  return false;
  
return true;                                    // Return 1 to flag a successful Dup
}

bool CheckDupRen::DoDup(wxString& originalpath, wxString& newpath)  // Used by DoRename to duplicate dirs
{
if (!wxFileName::Mkdir(newpath, 0777, wxPATH_MKDIR_FULL))  // Make the new dir. wxPATH_MKDIR_FULL means intermediate dirs are created too
  { wxLogError(_("Sorry, I couldn't make room for the incoming directory.  A permissions problem maybe?")); return false; }

wxDir dir(originalpath);                          // Use helper class to deal sequentially with each child file & subdir
if (!dir.IsOpened()) { wxLogError(_("Sorry, an implausible filesystem error occurred :-(")); return false; }  // Oops

wxBusyCursor busy;

wxString filename; // Go thru the dir, dealing first with files.  NB we don't return false in the event of failure:  there's already been SOME success (the parent dir) & there may be others to come
bool cont = dir.GetFirst(&filename, wxEmptyString, wxDIR_FILES | wxDIR_HIDDEN);
while  (cont)
  { wxString file = wxFILE_SEP_PATH + filename;
    class FileData stat(originalpath + file);
    if (stat.IsSymlink())
      SafelyCloneSymlink(originalpath + file, newpath + file, stat);
     else  
      if (stat.IsRegularFile())                   // If it's a normal file, use built-in wxCopyFile
        { wxBusyCursor busy;
          wxCopyFile(originalpath + file, newpath + file, false);
        }
     else
        { wxBusyCursor busy;                      // Otherwise it's an oddity like a fifo, which makes wxCopyFile hang, so do it the linux way
          mode_t oldUmask = umask(0);             // Temporarily change umask so that permissions are copied exactly
          mknod((newpath + file).mb_str(wxConvUTF8), stat.GetStatstruct()->st_mode, stat.GetStatstruct()->st_rdev);  // Technically this works.  Goodness knows if it achieves anything useful!
          umask(oldUmask);
        }

    cont = dir.GetNext(&filename);                // and try for another file
  }
                    // and now with any subdirs
cont = dir.GetFirst(&filename, wxEmptyString, wxDIR_DIRS | wxDIR_HIDDEN);                          
while (cont)
  { wxString file = wxFILE_SEP_PATH + filename;   // Make filename into /filename
    class FileData stat(originalpath + file);     // Stat the filepath, to make sure it's not a symlink pointing to a dir!
    if (stat.IsSymlink())
      SafelyCloneSymlink(originalpath + file, newpath + file, stat);
     else
      { wxString from(originalpath + filename + wxFILE_SEP_PATH), to(newpath + filename + wxFILE_SEP_PATH);  // Make the new paths
        DoDup(from, to);                          // Do the subdir by recursion                  
      }
    
    cont = dir.GetNext(&filename);
  }
return true;
}

//static
bool CheckDupRen::ChangeLinkTarget(wxString& linkfilepath, wxString& destfilepath)  // Here for convenience:  change the target of a symlink
{
if (linkfilepath.IsEmpty() || destfilepath.IsEmpty()) return false;

if (!wxRemoveFile(linkfilepath))                               // Delete the old link
  { wxMessageBox(_("Symlink Deletion Failed!?!")); return false; }  // Shouldn't fail, but exit ungracefully if somehow it does eg due to permissions

if (!CreateSymlink(destfilepath, linkfilepath))  return false; // Create a new symlink, to the new target

return true;
}

//--------------------------------------------------------------------------------------------------------------------------------------

wxArrayString MultipleRename::DoRename()
{
if (OldFilepaths.IsEmpty()) return OldFilepaths;

wxXmlResource::Get()->LoadDialog(&dlg, parent,  wxT("MultipleRenameDlg"));  // This is the dialog that gets the user input
dlg.Init();
if (dup)   dlg.SetTitle(_("Multiple Duplicate"));
LoadHistory();

HelpContext = HCmultiplerename;

int confirm;
wxCheckBox* inmChk = (wxCheckBox*)dlg.FindWindow(wxT("IgnoreNonMatchesChk"));
wxArrayString tempOldFilepaths;

do
  { NewFilepaths.Clear(); tempOldFilepaths.Clear();         // In case this is >1st time thru
    int ans = dlg.ShowModal();
    
    if (ans != wxID_OK) return NewFilepaths;
    
    bool IgnoreNonMatches = inmChk && inmChk->IsChecked();  // If checked, we'll skip any non-matching files in the non-regex sections below
    bool body = !((wxRadioBox*)dlg.FindWindow(wxT("BodyExtRadio")))->GetSelection();
    bool alwaysinc = !((wxCheckBox*)dlg.FindWindow(wxT("OnlyIfNeededChk")))->IsChecked();

    if (((wxCheckBox*)dlg.FindWindow(wxT("UseRegexChk")))->IsChecked())
      { size_t matches;
        wxString ReplaceThis = ((wxComboBox*)dlg.ReplaceCombo)->GetValue();
        wxString WithThis = ((wxComboBox*)dlg.WithCombo)->GetValue();
        if (dlg.ReplaceAllRadio->GetValue()) matches = 0;
          else matches = dlg.MatchSpin->GetValue();

        for (size_t n=0; n < OldFilepaths.GetCount(); ++n)
          { wxFileName fn(OldFilepaths[n]); wxString name = fn.GetFullName();
            if (DoRegEx(name, ReplaceThis, WithThis, matches))
                { fn.SetFullName(name); NewFilepaths.Add(fn.GetFullPath()); 
                  tempOldFilepaths.Add(OldFilepaths[n]); 
                }
              else 
                if (!IgnoreNonMatches)
                  { NewFilepaths.Add(OldFilepaths[n]);      // Only add the unchanged, non-matching files if the user wants to process them later
                    tempOldFilepaths.Add(OldFilepaths[n]);  // in which case, add OldFilepaths[n] to keep the old/new in sync
                  }
          }
      }
      else 
        { NewFilepaths = OldFilepaths; tempOldFilepaths = OldFilepaths; }  // If we're not going to copy in the above if statement, do it here
    
    wxString Prepend = ((wxComboBox*)dlg.PrependCombo)->GetValue();
    wxString Append = ((wxComboBox*)dlg.AppendCombo)->GetValue();
    DoPrependAppend(body, Prepend, Append);
    
    DoInc(body, dlg.Inc, dlg.IncWith, alwaysinc);
    
    MyButtonDialog confirmdlg;                                              // Give the user a chance to reconsider
    wxXmlResource::Get()->LoadDialog(&confirmdlg, MyFrame::mainframe->GetActivePane(),  wxT("ConfirmMultiRenameDlg"));
    if (dup) confirmdlg.SetTitle(_("Confirm Duplication"));
    LoadPreviousSize(&confirmdlg, wxT("ConfirmMultiRenameDlg"));
    
    wxListBox* BeforeList = (wxListBox*)confirmdlg.FindWindow(wxT("BeforeList")); 
    wxListBox* AfterList = (wxListBox*)confirmdlg.FindWindow(wxT("AfterList"));
    wxCHECK_MSG(tempOldFilepaths.GetCount() == NewFilepaths.GetCount(), wxArrayString(), wxT("Mismatch in numbers of old/new names"));
    for (size_t n=0; n < NewFilepaths.GetCount(); ++n)
      { BeforeList->Append(tempOldFilepaths[n].AfterLast(wxFILE_SEP_PATH));  AfterList->Append(NewFilepaths[n].AfterLast(wxFILE_SEP_PATH)); }

    confirm = confirmdlg.ShowModal();
    SaveCurrentSize(&confirmdlg, wxT("ConfirmMultiRenameDlg"));
    if (confirm == wxID_CANCEL)  { NewFilepaths.Clear(); return NewFilepaths; }
  }
  while (confirm != wxID_OK);                                             // Loop if user clicked "Try Again"
  
SaveHistory();
HelpContext = HCunknown;
OldFilepaths = tempOldFilepaths; // OldFilepaths is a reference, so this keeps the caller in register when ignoring non-matching files in the selection
return NewFilepaths;
}

bool MultipleRename::DoRegEx(wxString& String, wxString& ReplaceThis, wxString& WithThis, size_t matches)
{
if (String.IsEmpty() || (ReplaceThis.IsEmpty() && WithThis.IsEmpty())) return false;

wxString Repl(ReplaceThis);                                           // Switch to a local wxString because:
if (Repl.IsEmpty()) Repl = String;                                    // Assume that an empty Replace & non-empty With means replace whole String

  // I'm not sure if it's a bug, or a bug in my regex understanding, but
  // trying to replace ".*" or ".?" will cause an infinite loop if match==0
if ((matches==0) && (Repl==wxT(".*") || Repl==wxT(".?")))
  matches = 1;

wxLogNull WeDontWantwxRegExErrorMessages;
wxRegEx Expr(Repl);
if (Expr.IsValid() && Expr.Matches(String))  
  { Expr.Replace(&String, WithThis, matches);
    return true;
  }

return false; // Not a match
}

bool MultipleRename::DoInc(bool body, int InitialValue, int type, bool AlwaysInc)
{
for (size_t n=0; n < NewFilepaths.GetCount(); ++n)
  { wxString name; wxFileName fn; int start = InitialValue;
    if (AlwaysInc  || CheckForClash(NewFilepaths[n], n))            // If we're always to inc, or if there's a clash
      { do
            { fn.Assign(NewFilepaths[n]);
              if (body | fn.GetExt().IsEmpty())
                    { name = fn.GetName() + IncText(type, start++); fn.SetName(name); }
                else { name = fn.GetExt() + IncText(type, start++); fn.SetExt(name); }
            }
           while (CheckForClash(fn.GetFullPath(), n));              // If there's (still) a clash, re-Inc
          
        NewFilepaths[n] = fn.GetFullPath();                         // Write the altered string back to the array
      }
  }

return true;
}

bool MultipleRename::DoPrependAppend(bool body, wxString& Prepend, wxString& Append)
{
if (Prepend.IsEmpty() && Append.IsEmpty()) return false;

for (size_t n=0; n < NewFilepaths.GetCount(); ++n)
  { wxString name; wxFileName fn(NewFilepaths[n]);
    if (body | fn.GetExt().IsEmpty())                                       // If we've been told to use the body, or there isn't an ext anyway
            { name = Prepend + fn.GetName() + Append; fn.SetName(name); }   //   pre/append to the body.
      else  { name = Prepend + fn.GetExt()  + Append; fn.SetExt(name); }    // Otherwise to the ext
    
    NewFilepaths[n] = fn.GetFullPath();                                     // Write the altered string back to the array
  }

return true;
}

bool MultipleRename::CheckForClash(wxString Filepath, size_t index)       // See if there's already a 'file' with this filepath
{
FileData fd(Filepath);
if (fd.IsValid()) return true;                                            // Yes, there is

for (size_t n=0; n < index; ++n)                                          // Now check that a prior (potential) rename won't clash
  if (NewFilepaths[n] == Filepath) return true;

return false;
}

//static
wxString MultipleRename::IncText(int type, int start)  // Increments "start", returning the result as a string. Used also by MultipleRenameDlg
{
wxString value;
if (type == digit)  { value = (wxString::Format(wxT("%d"), start)); return value; }

if (start < 26) value = wxString::Format(wxT("%c"), start + wxT('a')); 
 else value = CreateSubgroupName(start-26, start);                        // Recycle CreateSubgroupName to make "a" or "aa" or "aaa" or...
if (type == upper) value.MakeUpper();
return value;
}

void MultipleRename::LoadHistory()    // Load the comboboxes history
{
wxConfigBase* config = wxConfigBase::Get();  if (config==NULL)   return; 

wxString item, key; size_t count;

config->SetPath(wxT("/History/MultRename/Replace/"));
count = config->GetNumberOfEntries();
dlg.ReplaceCombo->Clear(); //if (count) dlg.ReplaceCombo->Append(wxT(""));
for (size_t n=0; n < count; ++n)  
  { key.Printf(wxT("%c"), wxT('a') + (wxChar)n);        // Make key hold a, b, c etc
    config->Read(key, &item);
    if (!item.IsEmpty() && (dlg.ReplaceCombo->FindString(item) == wxNOT_FOUND))
      dlg.ReplaceCombo->Append(item);
  }
dlg.ReplaceCombo->SetValue(wxT(""));  // We don't want an immediate selection

config->SetPath(wxT("/History/MultRename/With/"));
count = config->GetNumberOfEntries();
dlg.WithCombo->Clear();
for (size_t n=0; n < count; ++n)  
  { key.Printf(wxT("%c"), wxT('a') + (wxChar)n);
    config->Read(key, &item);
    if (!item.IsEmpty() && (dlg.WithCombo->FindString(item) == wxNOT_FOUND))
      dlg.WithCombo->Append(item);
  }
dlg.WithCombo->SetValue(wxT(""));

config->SetPath(wxT("/History/MultRename/Prepend/"));
count = config->GetNumberOfEntries();
dlg.PrependCombo->Clear();
for (size_t n=0; n < count; ++n)  
  { key.Printf(wxT("%c"), wxT('a') + (wxChar)n);
    config->Read(key, &item);
    if (!item.IsEmpty() && (dlg.PrependCombo->FindString(item) == wxNOT_FOUND))
      dlg.PrependCombo->Append(item);
  }
dlg.PrependCombo->SetValue(wxT(""));

config->SetPath(wxT("/History/MultRename/Append/"));
count = config->GetNumberOfEntries();
dlg.AppendCombo->Clear();
for (size_t n=0; n < count; ++n)  
  { key.Printf(wxT("%c"), wxT('a') + (wxChar)n);
    config->Read(key, &item);
    if (!item.IsEmpty() && (dlg.AppendCombo->FindString(item) == wxNOT_FOUND))
      dlg.AppendCombo->Append(item);
  }
dlg.AppendCombo->SetValue(wxT(""));

config->SetPath(wxT("/"));  
}

void MultipleRename::SaveHistory()  // Save the comboboxes history
{
wxConfigBase* config = wxConfigBase::Get();  if (config==NULL)   return; 

wxString item, key; wxArrayString array;

item = dlg.ReplaceCombo->GetValue();                // See if there's a new entry
if (!item.IsEmpty())                                // If not, there's nothing to change
  { array.Add(item);                                // Store the new item    
    size_t count = wxMin((size_t)dlg.ReplaceCombo->GetCount(), MAX_COMMAND_HISTORY);  // Count the entries to be saved: not TOO many
    for (size_t n=0; n < count; ++n)  { item = dlg.ReplaceCombo->GetString(n); if (!item.IsEmpty()) array.Add(item); }

    config->DeleteGroup(wxT("/History/MultRename/Replace"));
    config->SetPath(wxT("/History/MultRename/Replace"));
    for (size_t n=0; n < array.GetCount(); ++n)  
      { item = array[n]; if (item.IsEmpty()) continue;
        key.Printf(wxT("%c"), wxT('a') + (wxChar)n);        // Make key hold a, b, c etc
        config->Write(key, item);
      }
  }

item = dlg.WithCombo->GetValue();
if (!item.IsEmpty()) 
  { array.Clear(); array.Add(item);
    size_t count = wxMin((size_t)dlg.WithCombo->GetCount(), MAX_COMMAND_HISTORY);
    for (size_t n=0; n < count; ++n)  { item = dlg.WithCombo->GetString(n); if (!item.IsEmpty()) array.Add(item); }

    config->DeleteGroup(wxT("/History/MultRename/With"));  
    config->SetPath(wxT("/History/MultRename/With/"));
    for (size_t n=0; n < array.GetCount(); ++n)  
      { item = array[n]; if (item.IsEmpty()) continue;
        key.Printf(wxT("%c"), wxT('a') + (wxChar)n);
        config->Write(key, item);
      }
  }

item = dlg.PrependCombo->GetValue();
if (!item.IsEmpty()) 
  { array.Clear(); array.Add(item);
    size_t count = wxMin((size_t)dlg.PrependCombo->GetCount(), MAX_COMMAND_HISTORY);
    for (size_t n=0; n < count; ++n)  { item = dlg.PrependCombo->GetString(n); if (!item.IsEmpty()) array.Add(item); }

    config->DeleteGroup(wxT("/History/MultRename/Prepend"));
    config->SetPath(wxT("/History/MultRename/Prepend/"));
    for (size_t n=0; n < array.GetCount(); ++n)  
      { item = array[n]; if (item.IsEmpty()) continue;
        key.Printf(wxT("%c"), wxT('a') + (wxChar)n);
        config->Write(key, item);
      }
  }

item = dlg.AppendCombo->GetValue();
if (!item.IsEmpty()) 
  { array.Clear(); array.Add(item);
    size_t count = wxMin((size_t)dlg.AppendCombo->GetCount(), MAX_COMMAND_HISTORY);
    for (size_t n=0; n < count; ++n)  { item = dlg.AppendCombo->GetString(n); if (!item.IsEmpty()) array.Add(item); }

    config->DeleteGroup(wxT("/History/MultRename/Append"));
    config->SetPath(wxT("/History/MultRename/Append/"));
    for (size_t n=0; n < array.GetCount(); ++n)  
      { item = array[n]; if (item.IsEmpty()) continue;
        key.Printf(wxT("%c"), wxT('a') + (wxChar)n);
        config->Write(key, item);
      }
  }

config->Flush();
config->SetPath(wxT("/"));  
}


//--------------------------------------------------------------------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(MultipleRenameDlg,wxDialog)

void MultipleRenameDlg::Init()
{
IncText = (wxTextCtrl*)FindWindow(wxT("IncText"));
ReplaceAllRadio = (wxRadioButton*)FindWindow(wxT("ReplaceAllRadio")); ReplaceAllRadio->SetValue(true);
ReplaceOnlyRadio = (wxRadioButton*)FindWindow(wxT("ReplaceOnlyRadio"));
MatchSpin = (wxSpinCtrl*)FindWindow(wxT("MatchSpin")); MatchSpin->Enable(false);
DigitLetterRadio = (wxRadioBox*)FindWindow(wxT("DigitLetterRadio"));
CaseRadio = (wxRadioBox*)FindWindow(wxT("CaseRadio")); CaseRadio->Enable(false);
ReplaceCombo = (wxComboBox*)FindWindow(wxT("ReplaceCombo"));
WithCombo = (wxComboBox*)FindWindow(wxT("WithCombo"));
PrependCombo = (wxComboBox*)FindWindow(wxT("PrependCombo"));
AppendCombo = (wxComboBox*)FindWindow(wxT("AppendCombo"));

IncWith = digit; Inc = 0;
}

void MultipleRenameDlg::SetIncText()
{
wxString value = MultipleRename::IncText(IncWith, Inc);
IncText->SetValue(value);
}

void MultipleRenameDlg::OnRegexCrib(wxCommandEvent& event)
{
HtmlDialog(_("RegEx Help"), HELPDIR + wxT("/RegExpHelp.htm"), wxT("RegExpHelp"), this);
}

void MultipleRenameDlg::OnSpinUp(wxSpinEvent& WXUNUSED(event))
{
++Inc;
SetIncText();
}

void MultipleRenameDlg::OnSpinDown(wxSpinEvent& event)
{
if (Inc > 0) --Inc;                                                   // We don't want negative values
  else Inc = ((wxSpinButton*)event.GetEventObject())->GetMax();
SetIncText();
}

void MultipleRenameDlg::OnRadioChanged(wxCommandEvent& WXUNUSED(event)) // Change whether the IncText box holds a digit or UC/UC letter
{
if (DigitLetterRadio->GetSelection()==0) IncWith = digit;             // Work out what's now wanted
 else IncWith = (CaseRadio->GetSelection()==0 ? upper : lower);
  
SetIncText();

CaseRadio->Enable(IncWith > digit);                                   // A convenient place to do UpdateUI
}

void MultipleRenameDlg::PanelUpdateUI(wxUpdateUIEvent& event)
{
bool ischecked = ((wxCheckBox*)FindWindow(wxT("UseRegexChk")))->IsChecked();
((wxPanel*)event.GetEventObject())->Enable(ischecked);
// The (recently added) IgnoreNonMatches checkbox needs the same UpdateUI as the panel
wxWindow* inmChk = FindWindow(wxT("IgnoreNonMatchesChk"));
if (inmChk)
  inmChk->Enable(ischecked);
}

void MultipleRenameDlg::ReplaceAllRadioClicked(wxCommandEvent& WXUNUSED(event))
{
bool all = ReplaceAllRadio->GetValue();                               // Did the user just select this radio, or unselect?
ReplaceOnlyRadio->SetValue(!all);                                     // Negate the other radio
MatchSpin->Enable(!all);                                              //  and appropriately enable the spinctrl
}

void MultipleRenameDlg::ReplaceOnlyRadioClicked(wxCommandEvent& WXUNUSED(event))
{
bool all = ReplaceOnlyRadio->GetValue();
ReplaceAllRadio->SetValue(!all);
MatchSpin->Enable(all);
}

BEGIN_EVENT_TABLE(MultipleRenameDlg, wxDialog)
  EVT_BUTTON(XRCID("RegexCrib"), MultipleRenameDlg::OnRegexCrib)
  EVT_RADIOBOX(XRCID("DigitLetterRadio"), MultipleRenameDlg::OnRadioChanged)
  EVT_RADIOBOX(XRCID("CaseRadio"), MultipleRenameDlg::OnRadioChanged)
  EVT_SPIN_UP(wxID_ANY, MultipleRenameDlg::OnSpinUp)
  EVT_SPIN_DOWN(wxID_ANY, MultipleRenameDlg::OnSpinDown)
  EVT_RADIOBUTTON(XRCID("ReplaceAllRadio"), MultipleRenameDlg::ReplaceAllRadioClicked)
  EVT_RADIOBUTTON(XRCID("ReplaceOnlyRadio"), MultipleRenameDlg::ReplaceOnlyRadioClicked)
  EVT_UPDATE_UI(XRCID("Panel"), MultipleRenameDlg::PanelUpdateUI)
END_EVENT_TABLE()

