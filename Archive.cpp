/////////////////////////////////////////////////////////////////////////////
// Name:       Archive.cpp
// Purpose:    Non-virtual archive stuff
// Part of:    4Pane
// Author:     David Hart
// Copyright:  (c) 2020 David Hart
// Licence:    GPL v3
/////////////////////////////////////////////////////////////////////////////
#include "wx/wxprec.h" 

#include "wx/wx.h"
#include "wx/xrc/xmlres.h"
#include "wx/config.h"
#include "wx/tarstrm.h"

#include "MyGenericDirCtrl.h"
#include "Externs.h"
#include "ExecuteInDialog.h"
#include "Misc.h"
#include "Tools.h"
#include "Filetypes.h"
#include "MyDirs.h"
#include "MyFrame.h"
#include "Configure.h"
#include "Archive.h"


void MyListBox::Add(wxString newfile)  // Appends to the list only if not already present
{
if (newfile.IsEmpty()) return;

if (FindString(newfile) == wxNOT_FOUND)
  Append(newfile);
}

void MyListBox::OnKey(wxKeyEvent& event)  // If Delete key pressed while file(s) highlit, delete them
{
if (event.GetKeyCode() == WXK_DELETE)
  { wxArrayInt selections;
    int count = GetSelections(selections);
    if (!count) return;
    
    for (int n=count; n > 0; --n)
      Delete(selections[n-1]);
  }
 else 
  event.Skip();
}

IMPLEMENT_DYNAMIC_CLASS(MyListBox,wxListBox)

BEGIN_EVENT_TABLE(MyListBox,wxListBox)
  EVT_CHAR(MyListBox::OnKey)
END_EVENT_TABLE()
//-----------------------------------------------------------------------------------------------------------------------
BEGIN_EVENT_TABLE(DecompressOrExtractDlg,wxDialog)
  EVT_BUTTON(XRCID("Compressed"),DecompressOrExtractDlg::OnDecompressButton)
  EVT_BUTTON(XRCID("Archive"),DecompressOrExtractDlg::OnExtractButton)
END_EVENT_TABLE()
//-----------------------------------------------------------------------------------------------------------------------

void ArchiveDialogBase::Init()
{
combo = (wxComboBox*)FindWindow(wxT("Combo"));
list = (MyListBox*)FindWindow(wxT("CurrentFiles"));

m_parent->FilepathArray.Clear();                              // Empty the array first: some paths to this method will have called GetMultiplePaths() already
m_parent->active->GetMultiplePaths(m_parent->FilepathArray);  // Load the selected files into the stringarray

      // The next line means that a <CR> in combo's textctrl is equivalent to pressing the AddFile button
Connect(XRCID("Combo"), wxEVT_COMMAND_TEXT_ENTER, wxTextEventHandler(ArchiveDialogBase::OnAddFile), NULL, this);
}

//static
wxString ArchiveDialogBase::FindUncompressedName(const wxString& archivename, enum ziptype ztype)  // What'd a compressed archive be called if uncompressed?
{
wxString UncompressedName; int c;

if (archivename.IsEmpty() || (ztype >= zt_firstarchive && ztype < zt_firstcompressedarchive)) return wxEmptyString;

if ((c = archivename.Find(wxT(".tar."))) != -1)     // If this is a tar.?z(2) archive
  UncompressedName = archivename.Left(c + 4);       //   remove the .?z(2), keeping the .tar
 else
   { if (archivename.Right(3) == wxT(".7z")) return wxT(""); // A standard (non-tar) 7z is an archive, but not as we know it
      else
       { if (archivename.Right(4) == wxT(".rar")) return wxT(""); // Ditto for rar
          else                                          // Otherwise it must have a .t?z ext
            UncompressedName = archivename.BeforeLast(wxT('.')) + wxT(".tar");  // Replace this with tar
       }
   }

return UncompressedName;
}

//static
bool ArchiveDialogBase::IsAnArchive(const wxString& filename, bool append/*=false*/)    // Decide if the file is an archive suitable for extracting +/- appending to
{
wxString ext = filename.AfterLast('.'); ext.MakeLower();    // See if the last .ext is relevant
if (ext==wxT("tar") || ext==wxT("tgz") || ext==wxT("tbz2") || ext==wxT("tbz") || ext==wxT("lzma") || ext==wxT("tlz")
  || ext==wxT("a") || ext==wxT("ar") || ext==wxT("cpio") || ext==wxT("rpm") || ext==wxT("deb")
  || (ext=="rar" && !append) // We can extract a .rar using  'unar' or ( the free-ish) 'unrar' but can't append to one
  || ext==wxT("7z") // All 7z's are archives
  || ext==wxT("zip") || ext=="xpi" || ext==wxT("htb")) // xpi == zip
  return true;
  
return (filename.Contains(wxT(".tar.")));                   // Otherwise see if there's a less-terminal tar ext
}

void ArchiveDialogBase::OnAddFile(wxCommandEvent& WXUNUSED(event))  // Add the contents of combo to the listbox
{
for (size_t n=0; n < combo->GetCount(); ++n)            // First go thru the combobox proper
  list->Add(combo->GetString(n));                       // 'Add' checks for duplicates
  
wxString newfile = combo->GetValue();                   // Now deal with the textctrl bit, in case an entry was written in
if (!newfile.IsEmpty())
  { wxString tempfile = newfile;
    if (newfile.Left(2) == wxT("./"))                   // If asking for the 'current' dir, get it from active pane
      tempfile = m_parent->active->GetActiveDirPath() + newfile.Mid(1);
     else
       if (newfile.GetChar(0) != wxT('/'))              // If relative address for this file, similarly
        { tempfile = m_parent->active->GetActiveDirPath() + wxT('/'); tempfile += newfile; }
    if (!(wxFileExists(tempfile) || wxDirExists(tempfile)))  // Since this entry was user-inserted, not from Browsing, check for rubbish
      { wxMessageDialog dialog(this, _("I can't seem to find this file or directory.\nTry using the Browse button."),  _("Oops!"), wxOK |wxICON_ERROR);
        dialog.ShowModal();
        combo->SetValue(wxT(""));
      }
     else  
        list->Add(newfile);
  }

combo->Clear(); combo->SetValue(wxT(""));
}

void ArchiveDialogBase::OnFileBrowse(wxCommandEvent& WXUNUSED(event))  // Browse for files to add to listbox
{
FileDirDlg fdlg(this, FD_MULTIPLE | FD_SHOWHIDDEN, m_parent->active->GetActiveDirPath(), _("Choose file(s) and/or directories"));
if (fdlg.ShowModal() != wxID_OK) return;

wxArrayString filenames;
fdlg.GetPaths(filenames);                                  // Get the selection(s) into an array

for (size_t n=0; n < filenames.GetCount(); ++n)            // Store the answers in the combobox.  The AddFile button will pass them to the listbox
  if (filenames[n].AfterLast(wxFILE_SEP_PATH) != wxT("..") && filenames[n].AfterLast(wxFILE_SEP_PATH) != wxT("."))
      combo->Append(filenames[n]);
#if (defined(__WXGTK__) || defined(__WXX11__))  // In gtk2 we need explicitly to set the value
  #if wxVERSION_NUMBER > 2902
      if (!combo->IsListEmpty())  combo->SetSelection(0);
  #else
      if (!combo->IsEmpty())  combo->SetSelection(0);
  #endif
#endif
}

BEGIN_EVENT_TABLE(ArchiveDialogBase,wxDialog)
  EVT_BUTTON(XRCID("AddFile"), ArchiveDialogBase::OnAddFile)
  EVT_BUTTON(XRCID("Browse"), ArchiveDialogBase::OnFileBrowse)
END_EVENT_TABLE()
//-----------------------------------------------------------------------------------------------------------------------

MakeArchiveDialog::MakeArchiveDialog(Archive* parent, bool newarch) : ArchiveDialogBase(parent)
{
NewArchive = newarch;
bool foundarchive = false, loaded = false;
if (NewArchive)
      loaded = wxXmlResource::Get()->LoadDialog(this, MyFrame::mainframe, wxT("MakeArchiveDlg"));
 else loaded = wxXmlResource::Get()->LoadDialog(this, MyFrame::mainframe, wxT("AppendArchiveDlg"));
wxCHECK_RET(loaded, wxT("Failed to load dialog from xrc"));
Init();

createincombo = (wxComboBox*)FindWindow(wxT("CreateInCombo"));

                    // Set checkboxes to last-used values
((wxCheckBox*)FindWindow(wxT("DeReference")))->SetValue(m_parent->DerefSymlinks);
((wxCheckBox*)FindWindow(wxT("Verify")))->SetValue(m_parent->m_Verify);
((wxCheckBox*)FindWindow(wxT("DeleteSources")))->SetValue(m_parent->DeleteSource);
if (NewArchive)
  { m_compressorradio = (wxRadioBox*)FindWindow(wxT("CompressRadio"));
    int compressors[] = { zt_bzip, zt_gzip, zt_xz, zt_lzma, zt_7z, zt_lzop };
    for (size_t n=0; n < sizeof(compressors)/sizeof(int); ++n)
      m_compressorradio->Enable(n, m_parent->IsThisCompressorAvailable((ziptype)compressors[n]));  // Disable any unavailable compression types
    // If the last-used compressor is still available, make it the default. Otherwise, set no compression
    m_compressorradio->SetSelection(m_compressorradio->IsItemEnabled(m_parent->ArchiveCompress) ? m_parent->ArchiveCompress : 6);
  }

for (size_t n=0; n < m_parent->ArchiveHistory.GetCount(); ++n) // Load the archive path history into the combo
  createincombo->Append(m_parent->ArchiveHistory[n]);
if (NewArchive) createincombo->SetValue(wxT("./"));            // If creating, give archive-path a default value of ./
  else          createincombo->SetValue(wxT(""));

size_t count = m_parent->FilepathArray.GetCount();
for (size_t n=0; n < count; ++n)                               // & the listbox
  if (!(m_parent->FilepathArray[n]==m_parent->active->GetActiveDirectory() && count==1))  // We don't want rootdir in the list just because it's automatically highlit
    { if (!(NewArchive || foundarchive)) // If we're appending, see if we can spot an archive name to append to (if we haven't found one already)
        { if (IsAnArchive(m_parent->FilepathArray[n], true))        // See if the file is an archive suitable for appending to
            { createincombo->SetValue(m_parent->FilepathArray[n]);  // If so, put it in the archive filepath combobox
              foundarchive = true;
            }
           else list->Append(m_parent->FilepathArray[n]);      // Otherwise put in the files-to-be-added listbox as usual
        }
       else list->Append(m_parent->FilepathArray[n]);
    }
}

void MakeArchiveDialog::OnCheckBox(wxCommandEvent& event)  // Make sure there's only one type of archiving checked at a time
{
int id = event.GetId();

if (id == XRCID("UseTar"))
  ((wxCheckBox*)FindWindow(wxT("UseZip")))->SetValue(!((wxCheckBox*)(event.GetEventObject()))->IsChecked());

if (id == XRCID("UseZip"))
  ((wxCheckBox*)FindWindow(wxT("UseTar")))->SetValue(!((wxCheckBox*)(event.GetEventObject()))->IsChecked());
}

void MakeArchiveDialog::OnUpdateUI(wxUpdateUIEvent& event)
{
if (event.GetId() == XRCID("UseTar"))          // If Tar button, do UI for its subordinates
  { bool enabled = ((wxCheckBox*)event.GetEventObject())->GetValue();
    bool tar7z = (m_compressorradio && m_compressorradio->GetSelection() == 4);  // We won't be here if it's a real foo.7z
    ((wxCheckBox*)FindWindow(wxT("DeleteSources")))->Enable(enabled && !tar7z);
    ((wxCheckBox*)FindWindow(wxT("DeReference")))->Enable(enabled);
    ((wxCheckBox*)FindWindow(wxT("Verify")))->Enable(enabled && !tar7z);
    m_compressorradio->Enable(enabled);
    ((wxStaticText*)FindWindow(wxT("CreateInFolder")))->Enable(enabled);
    ((wxBitmapButton*)FindWindow(wxT("CreateInBrowse")))->Enable(enabled);
    if (createincombo) createincombo->Enable(enabled);
  }
    
if (event.GetId() == XRCID("wxID_OK"))        // Enable the OK button if archive-name present &&  >0 files
  { if (NewArchive) OKEnabled = !((wxTextCtrl*)(FindWindow(wxT("ArchiveName"))))->GetValue().empty();
     else           OKEnabled = !createincombo->GetValue().empty();
    event.Enable(list->GetCount() && OKEnabled);
  }
}

void MakeArchiveDialog::OnOK(wxCommandEvent &event)
{
if (!OKEnabled) return;                                       // If insufficient data to allow wxID_OK, ignore the keypress
                        // If there is a new entry in the create-in comb, add it to the history if it's unique
wxString string = createincombo->GetValue();                  // Get the visible string
if (!(string.IsEmpty() || string==wxT("./"))) 
  { int index = m_parent->ArchiveHistory.Index(string, false);// See if this entry already exists in the history array
    if (index != wxNOT_FOUND)
      m_parent->ArchiveHistory.RemoveAt(index);               // If so, remove it.  We'll add it back into position zero  
    m_parent->ArchiveHistory.Insert(string, 0);               // Either way, insert into position zero
  }

EndModal(wxID_OK);
}

void MakeArchiveDialog::OnCreateInBrowse(wxCommandEvent& WXUNUSED(event))  // Browse for the dir to store the new archive, or the filepath of existing one
{
wxString filepath;

if (NewArchive) 
  { 
    #if defined(__WXGTK__)
      int style = wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST;     // the gtk2 dirdlg uses different flags :/
    #else
     int style = wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER;
    #endif
    wxDirDialog ddlg(this, _("Choose the Directory in which to store the Archive"), m_parent->active->GetActiveDirPath(), style);
    if (ddlg.ShowModal() != wxID_OK) return;    
    filepath = ddlg.GetPath();
  }  
 else
  { wxFileDialog fdlg(this, _("Browse for the Archive to which to Append"), m_parent->active->GetActiveDirPath(), wxT(""), wxT("*"), wxFD_OPEN);
    if (fdlg.ShowModal()  != wxID_OK) return;    
    filepath = fdlg.GetPath();
  }  

if (!filepath.IsEmpty())   createincombo->SetValue(filepath);
}

enum returntype MakeArchiveDialog::GetCommand()
{
if (!list->GetCount()) return cancel;                         // Shouldn't happen, as Return would be disabled

m_parent->FilepathArray.Clear();                              // In case files were added/deleted during the dialog, clear the old data
for (size_t n=0; n < list->GetCount(); ++n)
  m_parent->FilepathArray.Insert(list->GetString(n), 0);

if (NewArchive)  return GetNewCommand();                      // Are we creating or appending?
 else return GetAppendCommand();
}

enum returntype MakeArchiveDialog::GetNewCommand()  // Parse entered options into command for a new archive
{
wxString command, archivefilename;

archivefilename = ((wxTextCtrl*)FindWindow(wxT("ArchiveName")))->GetValue();  // Now get the archive file name

                                  // See which checkboxes are set.
if (((wxCheckBox*)FindWindow(wxT("UseZip")))->GetValue())     // If zip is, that'll be the only thing to look up
  { wxString oldcwd = GetCwd();                               // Save the current cwd
    wxString cwd = m_parent->active->GetActiveDirPath();
    SetWorkingDirectory(cwd);                                 // & then set it to the currently-selected path (this solves various zip problems)
    m_parent->Updatedir.Add(cwd);                             // We'll want to update this dir once we're finished
    
    command = wxT("echo Creating an archive using zip; ");
    wxString changedir; changedir.Printf(wxT("cd \\\"%s\\\" && "), cwd.c_str());
    command << changedir <<wxT("zip -rTv \\\"");
    command << archivefilename << wxT("\\\"");                // Add the name of the archive

  // Add the files.  First amputate the cwd path, as unzip assumes we stored paths relative to this, & reconstitutes /home/me/foo as /home/me/home/me/foo
    for (size_t c=0; c < m_parent->FilepathArray.GetCount(); ++c)
      { wxString relativefilepath, filepath = m_parent->FilepathArray[c];
        if (filepath.StartsWith(cwd+wxT('/'), &relativefilepath))  // If path found, wString::StartsWith puts the rest of the string in relativefilepath
           filepath = relativefilepath;                       // If successful, this means filepath contains the (file)path relative to cwd
         
        command << wxT(" \\\"") << filepath << wxT("\\\"");   // Add the filepath, cropped or otherwise, to the command
      }
    
    SetWorkingDirectory(oldcwd);
  }
 else                              // If not zip, it must be tar. Start by finding the archive filepath
  { wxString archivename = createincombo->GetValue();         // Get first the selected path for the archive
    if (archivename == wxT("./"))                             // If asking for the 'current' dir, get it from active pane
        archivename = m_parent->active->GetActiveDirPath();
    if (archivename.Last() != wxT('/'))  archivename += wxT('/');
    if (!wxDirExists(archivename))
      { wxMessageDialog dialog(this, _("The directory in which you want to create the archive doesn't seem to exist.\nUse the current directory?"), wxT(""), wxYES_NO | wxICON_QUESTION);
        if (dialog.ShowModal() != wxID_YES)  return retry;
        archivename = m_parent->active->GetActiveDirPath();
      }
    
    m_parent->Updatedir.Add(archivename);                     // We'll want to update this dir once we're finished.  Grab the chance to save it
    archivename << archivefilename;
    
    archivename << wxT(".tar");                               // Add the ext to archive name
    command = wxT("echo Creating an archive using tar; tar");
    command << MakeTarCommand(archivename);
    
                                  // Now sort out the compression command, if any
    switch(m_parent->ArchiveCompress = m_compressorradio->GetSelection())
     { case 0:  command << wxT("&& echo Compressing with bzip2 && bzip2 -z \\\"") << archivename << wxT("\\\""); break;
       case 1:  command << wxT("&& echo Compressing with gzip && gzip \\\"") << archivename << wxT("\\\"");      break;
       case 2:  command << wxT("&& echo Compressing with xz && xz -z \\\"") << archivename << wxT("\\\"");       break;
       case 3:  command << wxT("&& echo Compressing with lzma && xz -z --format=lzma \\\"") << archivename << wxT("\\\""); break;
       case 4:  command << wxT("&& echo Compressing with 7z && cat \\\"") << archivename                      // 7z is pathetic re the commandline, so use 'cat'
                        << wxT("\\\" | \\\"") << m_parent->GetSevenZString() << wxT("\\\" a -si \\\"")        //  a -si means create from stdin
                        << archivename << wxT(".7z\\\" && rm -f \\\"") << archivename << wxT("\\\""); break;  // 7z doesn't delete the tarball, so do it ourselves
       case 5:  command << wxT("&& echo Compressing with lzop && lzop -U \\\"") << archivename << wxT("\\\"");   break;
     }
  }

m_parent->command = command;                                  // Pass the command string to parent's version
return valid;
}

enum returntype MakeArchiveDialog::GetAppendCommand()  // Parse entered options into command to append to an archive
{
wxString command;
                                  // Start by finding the archive filepath
wxString archivename = createincombo->GetValue();
if (!wxFileExists(archivename))
  { wxMessageDialog dialog(this, _("The archive to which you want to append doesn't seem to exist.\nTry again?"), wxT(""), wxYES_NO | wxICON_QUESTION);
    return (dialog.ShowModal()==wxID_YES ? retry : cancel);
  }

switch(m_parent->Categorise(archivename))
  { case zt_zip: case zt_htb:
      { wxString oldcwd = GetCwd();
        wxString cwd = m_parent->active->GetActiveDirPath();
        SetWorkingDirectory(cwd);                                // & then set it to the currently-selected path (this solves various zip problems)

        command = wxT("echo Appending to a zip archive; ");
        wxString changedir; changedir.Printf(wxT("cd \\\"%s\\\" && "), cwd.c_str());
        command << changedir << wxT("zip -urv");                 // Update, recursive, verbose
        
        if ((m_parent->DeleteSource = ((wxCheckBox*)FindWindow(wxT("DeleteSources")))->GetValue()))  // Do we want to delete the source files?
          command << wxT('m');
        if ((m_parent->m_Verify = ((wxCheckBox*)FindWindow(wxT("Verify")))->GetValue()))              // Do we want to verify the archive?
          command << wxT('T');  
        if (!(m_parent->DerefSymlinks = ((wxCheckBox*)FindWindow(wxT("DeReference")))->GetValue()))   // Do we want to save symlink itself, not the target?
          command << wxT('y');
          
        wxString relativefilepath;          // Zip wants its archive to be relative to the cwd. archivename is probably an absolute filepath
        if (archivename.StartsWith(cwd+wxT('/'), &relativefilepath))
           archivename = relativefilepath;                              // If successful, this means filepath contains the (file)path relative to cwd
        command << wxT(" \\\"") << archivename << wxT("\\\"");

        // Add the files. First amputate the cwd path, as unzip assumes we stored paths relative to this, & reconstitutes /home/me/foo as /home/me/home/me/foo
        for (size_t c=0; c < m_parent->FilepathArray.GetCount(); ++c)
          { wxString relativefilepath,  filepath = m_parent->FilepathArray[c];
            if (filepath.StartsWith(cwd+wxT('/'), &relativefilepath))
               filepath = relativefilepath;                      // If successful, this means filepath contains the (file)path relative to cwd

            command << wxT(" \\\"") << filepath << wxT("\\\"");  // Add the filepath, cropped or otherwise, to the command
          }
          
        SetWorkingDirectory(oldcwd);
        break;
      }

    case zt_7z:
        { wxString decompresscmd = AppendCompressedTar(archivename, zt_7z); 
        if (decompresscmd.empty()) { wxMessageBox(_("Can't find a 7z binary on your system"), _("Oops"), wxOK, this); return cancel; }
        command << wxT("echo Appending to a standard 7z archive; ") << decompresscmd; break; }

    case zt_taronly:  command = wxT("echo Appending to a tarball; tar");  command << MakeTarCommand(archivename); break;
    case zt_targz:    command = wxT("echo Appending to a tarball compressed with gzip; "); command << AppendCompressedTar(archivename, zt_gzip); break;
    case zt_tarbz:    command = wxT("echo Appending to a tarball compressed with xz; "); command << AppendCompressedTar(archivename, zt_bzip); break;
    case zt_tarxz:    command = wxT("echo Appending to a tarball compressed with xz; "); command << AppendCompressedTar(archivename, zt_xz); break;
    case zt_tarlzma:  command = wxT("echo Appending to a tarball compressed with lzma; "); command << AppendCompressedTar(archivename, zt_lzma); break;
    case zt_tar7z:   {command = wxT("echo Appending to a tarball compressed with 7z; "); 
                      wxString decompresscmd = AppendCompressedTar(archivename, zt_tar7z); 
                      if (decompresscmd.empty()) { wxMessageBox(_("Can't find a 7z binary on your system"), _("Oops"), wxOK, this); return cancel; }
                      command << decompresscmd; break;}
    case zt_tarlzop:  command = wxT("echo Appending to a tarball compressed with lzop; "); command << AppendCompressedTar(archivename, zt_lzop); break;
     default:         wxMessageBox(_("Can't find a valid archive to which to append"), _("Oops"), wxOK, this);
                      return cancel;    // if archive doesn't seem to be an archive, or one that can't be handled
  }

m_parent->command = command;                                // Pass the command string to parent's version
return valid;
}

wxString MakeArchiveDialog::AppendCompressedTar(const wxString& archivefpath, enum ziptype ztype)  // Handles the uncompress, append, recompress contruction
{
wxString command, UncompressedName, Recompress;

if (ztype != zt_7z)
  { UncompressedName = FindUncompressedName(archivefpath, ztype);  // eg  .tbz2 or .tar.gz -> .tar   
    if (UncompressedName.IsEmpty()) return UncompressedName;
  }

switch(ztype)
  { case zt_gzip:   command << wxT("gunzip -v \\\"") << archivefpath << wxT("\\\"");
                    Recompress <<  wxT(" ; echo Recompressing with gzip && gzip \\\"") << UncompressedName << wxT("\\\""); break;
    case zt_bzip :  command = wxT("bunzip2 -v \\\"") + archivefpath; command += wxT("\\\"");
                    Recompress << wxT(" ; echo Recompressing with bzip2 && bzip2 -z \\\"") << UncompressedName << wxT("\\\""); break;
    case zt_xz :    command = wxT("xz -dv \\\"") + archivefpath; command += wxT("\\\"");
                    Recompress << wxT(" ; echo Recompressing with xz && xz -z \\\"") << UncompressedName << wxT("\\\""); break;
    case zt_lzma :  command = wxT("xz -dv \\\"") + archivefpath; command += wxT("\\\"");
                    Recompress << wxT(" ; echo Recompressing with xz && xz -z --format=lzma \\\"") << UncompressedName << wxT("\\\""); break;
    case zt_7z :    {wxString sevenzbinary(m_parent->GetSevenZString()); if (sevenzbinary.empty()) return wxT("");  // NB a 'standard' 7z archive
                    // 7z is sufficiently different (!) that we deal with it here
                    wxString oldcwd = GetCwd(); wxString archivename(archivefpath.AfterLast(wxFILE_SEP_PATH)); // 7z really hates dealing with filepaths, so keep things relative
                    command = wxT("cd \\\""); command << archivefpath.BeforeLast(wxFILE_SEP_PATH) << wxT("\\\" && ");
                    command << sevenzbinary << wxT(" a \\\"") << archivename;
                    command << wxT("\\\" ") << AddSortedFiles(false) << wxT(" && cd \\\"") << oldcwd << wxT("\\\"");
                    return command;}
    case zt_tar7z : {wxString sevenzbinary(m_parent->GetSevenZString()); if (sevenzbinary.empty()) return wxT("");  // NB a tarball compressed with 7z
                    // 7z is sufficiently different (!) that we deal with it here
                    wxString oldcwd = GetCwd(); wxString archivename(archivefpath.AfterLast(wxFILE_SEP_PATH)); // 7z really hates dealing with filepaths, so keep things relative
                    command = wxT("cd \\\""); command << archivefpath.BeforeLast(wxFILE_SEP_PATH) << wxT("\\\" && ");
                    command << sevenzbinary << wxT(" e \\\"") << archivename;
                    command << wxT("\\\" && tar") << MakeTarCommand(UncompressedName) << wxT(" && rm -f \\\"") << archivefpath;
                    command << wxT("\\\" && echo Recompressing with 7z && cat \\\"") << UncompressedName << wxT("\\\" | ") << sevenzbinary 
                            << wxT(" a -si \\\"") << archivename << wxT("\\\"")  // a -si means create from stdin
                            << wxT(" && rm -f \\\"") << UncompressedName << wxT("\\\" && cd \\\"") << oldcwd << wxT("\\\"");
                    return command;}
    case zt_lzop :  command = wxT("lzop -dUv \\\"") + archivefpath; command += wxT("\\\"");
                    Recompress << wxT(" ; echo Recompressing with lzop && lzop -Uv \\\"") << UncompressedName << wxT("\\\""); break;
     default:       return command;
  }

command << wxT(" && tar");

command << MakeTarCommand(UncompressedName);
command << Recompress;

return command;
}

wxString MakeArchiveDialog::MakeTarCommand(const wxString& archivename)
{
wxString command;

if (NewArchive)  command << wxT(" -cv");                    // -c designates new archive, -v is verbose
 else command << wxT(" -rv");                               // -r is append

if ((m_parent->DerefSymlinks = ((wxCheckBox*)FindWindow(wxT("DeReference")))->GetValue()))  // Do we want to save symlink target, not the link itself?
  command << wxT('h');

command << wxT("f \\\"") << archivename  << wxT("\\\"");

if ((m_parent->DeleteSource = ((wxCheckBox*)FindWindow(wxT("DeleteSources")))->GetValue())) // Do we want to delete the source files?
  command << wxT(" --remove-files ");

command << AddSortedFiles();                                   // Add the files, sorted by dir, each dir preceded by a -C

if ((m_parent->m_Verify = ((wxCheckBox*)FindWindow(wxT("Verify")))->GetValue())) // Verify the archive? NB we do this here, not using the 'W' flag.
  {                                                                              // That's because it would fail if the source files had just been deleted
    wxString str; str.Printf(wxT("&& echo Verifying %s && "), archivename.AfterLast(wxT('/')).c_str());  
    command << str << wxT("tar -tvf \\\"") << archivename + wxT("\\\"");
  }

return command;
}

wxString MakeArchiveDialog::AddSortedFiles(bool prependingpaths /*= true*/)  // Returns files sorted by dir, each dir preceded by a -C
{
wxString command, lastpath;
wxArrayString filepaths, files;

if (!m_parent->FilepathArray.GetCount()) return command;

m_parent->FilepathArray.Sort();                                     // This should get them into dir order

for (size_t c=0; c < m_parent->FilepathArray.GetCount(); ++c)       // It's easier if we separate the 'local' files from the filepaths
  if (m_parent->FilepathArray[c].Left(2) == wxT("./"))              // Initial './', so this should be a file in the cwd. Remove its ./
    files.Add(m_parent->FilepathArray[c].Mid(2));
   else 
     if (m_parent->FilepathArray[c].GetChar(0) != wxT('/')) files.Add(m_parent->FilepathArray[c]);  // No initial '/', so this should be a file in the cwd
   else filepaths.Add(m_parent->FilepathArray[c]);                  // Otherwise put it in the filepaths array 
  
if (files.GetCount() && prependingpaths)          // Because the cwd probably isn't the active dir, we need to use the latter explicitly
  { command = wxT(" -C \\\""); command << m_parent->active->GetActiveDirPath() << wxT("\\\""); }
for (size_t c=0; c < files.GetCount(); ++c)
  { command << wxT(" \\\"") << files[c] << wxT("\\\""); }           // Add each 'local' file to the command

size_t n=0;
if (filepaths.GetCount())
 do                                                                 // Now do the absolute filepaths, path by path
  { lastpath = filepaths[n].BeforeLast('/');                        // Start with the path, making a new -C entry
    if (prependingpaths) command << wxT(" -C \\\"") << lastpath << wxT("/\\\"");
    do
      { command << wxT(" \\\"") << filepaths[n++].AfterLast('/') << wxT("\\\"");    // Add each file for this path
        if (n >= filepaths.GetCount()) return command;              // Return if we're finished
      }
     while (filepaths[n].BeforeLast('/') == lastpath);              // Continue while the path is unchanged
  }  
 while (1);                                                         // The path changed, so loop to cd

return command;
}

BEGIN_EVENT_TABLE(MakeArchiveDialog,ArchiveDialogBase)
  EVT_CHECKBOX(wxID_ANY, MakeArchiveDialog::OnCheckBox)
  EVT_BUTTON(XRCID("CreateInBrowse"), MakeArchiveDialog::OnCreateInBrowse)
  EVT_BUTTON(wxID_OK, MakeArchiveDialog::OnOK)
  EVT_UPDATE_UI(wxID_ANY, MakeArchiveDialog::OnUpdateUI)
END_EVENT_TABLE()

//-----------------------------------------------------------------------------------------------------------------------

CompressDialog::CompressDialog(Archive* parent) : ArchiveDialogBase(parent)
{
bool loaded = wxXmlResource::Get()->LoadDialog(this, MyFrame::mainframe, wxT("CompressDlg"));
wxCHECK_RET(loaded, wxT("Failed to load ExtractCompressedDlg from xrc"));
Init();

size_t count = m_parent->FilepathArray.GetCount();
for (size_t n=0; n < count; ++n)                                    // Put the highlit files into the listbox
  if (!((m_parent->FilepathArray[n]==m_parent->active->GetActiveDirectory()) && (count==1)))  // We don't want rootdir in the list just because it's automatically highlit
    list->Append(m_parent->FilepathArray[n]);

wxRadioBox* radio = (wxRadioBox*)FindWindow(wxT("CompressRadio"));
int compressors[] = { zt_bzip, zt_gzip, zt_xz, zt_lzma, zt_lzop, zt_compress };
for (size_t n=0; n < sizeof(compressors)/sizeof(int); ++n)
  radio->Enable(n, m_parent->IsThisCompressorAvailable((ziptype)compressors[n]));  // Disable any unavailable compression types

                                // Set checkboxes etc to last-used values
((wxSlider*)FindWindow(wxT("Slider")))->SetValue(m_parent->Slidervalue);

((wxCheckBox*)FindWindow(wxT("Recurse")))->SetValue(m_parent->Recurse);
((wxCheckBox*)FindWindow(wxT("Force")))->SetValue(m_parent->Force);

if (radio->IsItemEnabled(m_parent->m_Compressionchoice))
  radio->SetSelection(m_parent->m_Compressionchoice); // If the last-used compressor is still available, make it the default
}

void CompressDialog::OnUpdateUI(wxUpdateUIEvent& event)
{
if (event.GetId() == XRCID("Slider"))                                // I'm using the slider UI event, as the radiobox didn't seem to generate one!?  
  { int selection = ((wxRadioBox*)FindWindow(wxT("CompressRadio")))->GetSelection();
    bool enabled = (selection > 0) && (selection < 6) && (selection != 4);     // Disable the slider if the selected compressor is 0 (bzip2), 4 (7z) or 6 (compress)
    ((wxStaticText*)FindWindow(wxT("FasterStatic")))->Enable(enabled);
    ((wxSlider*)FindWindow(wxT("Slider")))->Enable(enabled);
    ((wxStaticText*)FindWindow(wxT("SmallerStatic")))->Enable(enabled);
  }
    
if (event.GetId() == XRCID("wxID_OK"))                               // Enable the OK button if >0 files in listbox
  ((wxButton*)event.GetEventObject())->Enable(list->GetCount());
}

enum returntype CompressDialog::GetCommand()    // Parse entered options into a command
{
enum { cdgc_bzip, cdgc_gzip, cdgc_xz, cdgc_lzma, cdgc_lzop, cdgc_compress };
wxString command, files, verifystring;
bool data = 0;                                              // Flags that we've got >0 files to compress

if (!list->GetCount()) return cancel;

m_parent->FilepathArray.Clear();                            // In case files were added/deleted during the dialog, clear the old data
for (size_t n=0; n < list->GetCount(); ++n)
  m_parent->FilepathArray.Insert(list->GetString(n), 0);
if (m_parent->FilepathArray.GetCount() == 0) return cancel;

wxString squash = wxString::Format(wxT("%u"), m_parent->Slidervalue = ((wxSlider*)FindWindow(wxT("Slider")))->GetValue());
unsigned int whichcompressor = ((wxRadioBox*)FindWindow(wxT("CompressRadio")))->GetSelection();  // Which compressor are we using?
m_parent->m_Compressionchoice = whichcompressor;

command = wxT("echo Compressing ");
switch(whichcompressor)
  { case cdgc_bzip: command << wxT("using bzip2; bzip2 -zv"); break;    // Compress, verbose. No point adding squash as bzip2 always does 9
    case cdgc_gzip: command << wxT("using gzip; gzip -v") << squash; break;
    case cdgc_xz:   command << wxT("using xz; xz -v") << squash; break;
    case cdgc_lzma: command << wxT("using lzma; xz --format=lzma -v") << squash; break;  // lzma, but still use xz to do the work
    case cdgc_lzop: command << wxT("using lzop; lzop -Uv") << squash; break;      // lzop retains original files unless the -U option is given
    case cdgc_compress: command << wxT("using \'compress\'; compress -v"); break; // For historical interest, 'compress'
  }

wxCheckBox* fcb = (wxCheckBox*)FindWindow(wxT("Force"));
m_parent->Force = fcb && fcb->IsEnabled() && fcb->IsChecked();
if (m_parent->Force)
  command << wxT('f');                                     // Force overwrite if requested

wxCheckBox* rcb = (wxCheckBox*)FindWindow(wxT("Recurse"));
bool recurse = rcb && rcb->IsEnabled() && rcb->IsChecked();
m_parent->Recurse = recurse;

for (size_t n=0; n < m_parent->FilepathArray.GetCount(); ++n) // Now add the requested files
  { FileData fd(m_parent->FilepathArray[n]);
    if (fd.IsDir())                                           // If this 'file' is a directory, we have to be clever
      { wxArrayString filearray;
        // Grab files from this dir, & optionally all its subdirs. NB We had to do it like this for bz2 as bzip2 has no recurse flag, but my gzip's recursion is buggy anyway                          
        AddRecursively(m_parent->FilepathArray[n], filearray, recurse);        
        for (size_t c=0; c < filearray.GetCount(); ++c)       // Then add them 1 by 1 to the files string
          { files << wxT(" \\\"") << filearray[c] << wxT("\\\""); data = 1; }
      }
                                      
    else // If the file IS a file, or if a dir using gzip -r, add it to the string.  NB we quote using escaped "", in case a filepath contains a '
       { files << wxT(" \\\"") << m_parent->FilepathArray[n] << wxT("\\\""); data = 1; }
  }

if (!data) return cancel;                                    // Lest there were no actual files to compress

m_parent->command = command + files;                         // Join the command strings in parent's version
return valid;
}

void CompressDialog::AddRecursively(const wxString& dir, wxArrayString& filearray, bool recurse) // Put files from this dir, & optionally all its subdirs, into filearray
{
wxArrayString localarray;    
  // First get all the files (optionally also in subdirs) from the passed dir into a local array
int flags = wxDIR_FILES | wxDIR_HIDDEN;
if (recurse) 
  { flags |= wxDIR_DIRS;
#if wxVERSION_NUMBER > 2904
    flags |= wxDIR_NO_FOLLOW;
#endif
  }
wxDir::GetAllFiles(dir, &localarray, wxEmptyString, flags);
for (size_t c=0; c < localarray.GetCount(); ++c)
  { FileData fd(localarray[c]);
    if (fd.IsDir())
      { if (recurse) AddRecursively(localarray[c], localarray, recurse); }  // recursing into dirs (which will append their contents)
     else  filearray.Add(localarray[c]);
  }
}

BEGIN_EVENT_TABLE(CompressDialog,ArchiveDialogBase)
  EVT_UPDATE_UI(wxID_ANY, CompressDialog::OnUpdateUI)
END_EVENT_TABLE()

//-----------------------------------------------------------------------------------------------------------------------

DecompressDialog::DecompressDialog(Archive* parent) : ArchiveDialogBase(parent)
{
bool loaded = wxXmlResource::Get()->LoadDialog(this, MyFrame::mainframe, wxT("ExtractCompressedDlg"));
wxCHECK_RET(loaded, wxT("Failed to load ExtractCompressedDlg from xrc"));
Init();

size_t count = m_parent->FilepathArray.GetCount();
for (size_t n=0; n < count; ++n)
  { FileData fd(m_parent->FilepathArray[n]);
    if (fd.IsDir()) list->Append(m_parent->FilepathArray[n]); // If someone selected a dir(s), presumably he wants its files decompressed
     else
      { enum ziptype ztype = m_parent->Categorise(m_parent->FilepathArray[n]);
            if ((ztype <= zt_compress)                     // If this is a compressed file,
                      || (ztype >= zt_firstcompressedarchive && ztype != zt_invalid))  //or a compressed archive,
              list->Append(m_parent->FilepathArray[n]);    // put it in the to-be-decompressed listbox
      }
  }
                                // Set checkboxes etc to last-used values
((wxCheckBox*)FindWindow(wxT("Recurse")))->SetValue(m_parent->DecompressRecurse);
((wxCheckBox*)FindWindow(wxT("Force")))->SetValue(m_parent->DecompressForce);
((wxCheckBox*)FindWindow(wxT("ArchivesToo")))->SetValue(m_parent->DecompressArchivesToo);
}

void DecompressDialog::OnUpdateUI(wxUpdateUIEvent& event)
{
if (event.GetId() == XRCID("wxID_OK")) // Enable the OK button if >0 files in listbox
  ((wxButton*)event.GetEventObject())->Enable(list->GetCount());
}

enum returntype DecompressDialog::GetCommand()  // Parse entered options into a command
{
wxString command; wxArrayString gz, bz, xz, sevenz, rar, lzo;

if (!list->GetCount()) return cancel;                   // Shouldn't happen, as Return would be disabled

m_parent->FilepathArray.Clear();
for (size_t n=0; n < list->GetCount(); ++n)
  m_parent->FilepathArray.Insert(list->GetString(n), 0);

bool recurse = ((wxCheckBox*)FindWindow(wxT("Recurse")))->IsChecked();
m_parent->DecompressRecurse = recurse;
bool force = ((wxCheckBox*)FindWindow(wxT("Force")))->IsChecked();
m_parent->DecompressForce = force;
m_parent->DecompressArchivesToo = ((wxCheckBox*)FindWindow(wxT("ArchivesToo")))->IsChecked();


    // We may have 5 sorts of file in the array:  gz, bz2 etc, and ordinary files added in error.  Oh, and dirs. Because of possible recursion, do in a submethod
    // NB we don't use sevenz (as foo.7z is an archive, not a 'compressed file'; but it's needed as a SortFiles() parameter
if (!SortFiles(m_parent->FilepathArray, gz, bz, xz, sevenz, lzo, recurse, m_parent->DecompressArchivesToo))        
  { wxMessageDialog dialog(this, _("No relevant compressed files were selected.\nTry again?"),    // If there weren't any compressed files selected...
                                                                        wxT(""), wxYES_NO | wxICON_QUESTION);
    return (dialog.ShowModal() == wxID_YES ? retry : cancel);
  }
wxCHECK_MSG(sevenz.IsEmpty(), cancel, wxT("Impossible result: 7z files shouldn't be found here"));

if (!gz.IsEmpty())                                      // Create a gunzip command, adding relevant files
  { command << wxT("gunzip -v");                        // Verbose
    if (force) command << wxT('f');
    for (size_t c=0; c < gz.GetCount(); ++c)
      { command << wxT(" \\\"") << gz[c] << wxT("\\\""); }
  }

if (!bz.IsEmpty())
  { if (!command.empty()) command << wxT(" && ");
    command << wxT("bunzip2 -v"); 
    if (force) command << wxT('f');
    for (size_t c=0; c < bz.GetCount(); ++c)
      { command << wxT(" \\\"") << bz[c] << wxT("\\\""); }
  }

if (!xz.IsEmpty())
  { if (!command.empty()) command << wxT(" && ");
    command << wxT("xz -dv"); 
    if (force) command << wxT('f');
    for (size_t c=0; c < xz.GetCount(); ++c)
      { command << wxT(" \\\"") << xz[c] << wxT("\\\""); }
  }

if (!lzo.IsEmpty())
  { if (!command.empty()) command << wxT(" && ");
    command << wxT("lzop -dUv"); // -U says _not_ to keep the original compressed files
    if (force) command << wxT('f');
    for (size_t c=0; c < lzo.GetCount(); ++c)
      { command << wxT(" \\\"") << lzo[c] << wxT("\\\""); }
  }

if (!command.empty())
  { m_parent->command = wxT("echo Decompressing; ") + command;  return valid; }
 else return cancel;
}

bool DecompressDialog::SortFiles(const wxArrayString& selected, 
    wxArrayString& gz, wxArrayString& bz, wxArrayString& xz, wxArrayString& sevenz, wxArrayString& lzo, bool recurse, bool archivestoo /*=false */)  //  Sort selected files into .bz, .gz & rubbish
{
for (size_t n=0; n < selected.GetCount(); ++n)
  { FileData fd(selected[n]);
    if (fd.IsDir())                   // If this is a directory, include its files. If 'recurse' do so recursively
      { int flags = wxDIR_FILES | wxDIR_HIDDEN;
        if (recurse) 
          { flags |= wxDIR_DIRS;
        #if wxVERSION_NUMBER > 2904
            flags |= wxDIR_NO_FOLLOW;
        #endif
          }
        wxArrayString filearray;      // Put files from this dir, & optionally all its subdirs, into filearray
        wxDir::GetAllFiles(selected[n], &filearray, wxEmptyString, flags);
        SortFiles(filearray, gz, bz, xz, sevenz, lzo, true, archivestoo); // Now sort by recursion
        continue;                                        // We've processed this item. Loop here to avoid falling thru to the next bit
      }    
    
    switch(m_parent->Categorise(selected[n]))                 // This method returns different answers depending on whether .gz, .tar.bz2 etc
      { case zt_targz: case zt_taZ: if (!archivestoo) break;  // If we don't want to decompress any compressed archives, break. Otherwise fall into gzip
        case zt_compress:                                     // gzip does this
        case zt_gzip:  gz.Add(selected[n]); break;
        case zt_tarbz: if (!archivestoo)  break;
        case zt_bzip:  bz.Add(selected[n]); break;
        case zt_tarxz: case zt_tarlzma: if (!archivestoo)  break;
        case zt_xz: case zt_lzma:  xz.Add(selected[n]); break;
        case zt_tar7z: if (!archivestoo)  break;
        case zt_7z:  sevenz.Add(selected[n]); break;
        case zt_tarlzop: if (!archivestoo)  break;
        case zt_lzop:  lzo.Add(selected[n]); break;
          default: break;                                // because the file is not compressed
      }
  }
return !(gz.IsEmpty() && bz.IsEmpty() && xz.IsEmpty() && sevenz.IsEmpty() && lzo.IsEmpty());
}



BEGIN_EVENT_TABLE(DecompressDialog,ArchiveDialogBase)
  EVT_UPDATE_UI(wxID_ANY, DecompressDialog::OnUpdateUI)
END_EVENT_TABLE()

//-----------------------------------------------------------------------------------------------------------------------

ExtractArchiveDlg::ExtractArchiveDlg(Archive* parent) : m_parent(parent)
{
wxXmlResource::Get()->LoadDialog(this, MyFrame::mainframe, wxT("ExtractArchiveDlg"));
Init();
}

void ExtractArchiveDlg::Init()
{
text = (wxTextCtrl*)FindWindow(wxT("text"));
CreateInCombo = (wxComboBox*)FindWindow(wxT("CreateInCombo"));

for (size_t n=0; n < m_parent->ArchiveHistory.GetCount(); ++n)    // Load the archive path history into the combo
  CreateInCombo->Append(m_parent->ArchiveHistory[n]);
CreateInCombo->SetValue(wxT("./"));                               // Give archive-path a default value of ./

m_parent->active->GetMultiplePaths(m_parent->FilepathArray);      // Load the selected files into the stringarray

size_t count = m_parent->FilepathArray.GetCount();
for (size_t n=0; n < count; ++n)
  if (ArchiveDialogBase::IsAnArchive(m_parent->FilepathArray[n]))
      { text->ChangeValue(m_parent->FilepathArray[n]); break; }   // Put 1st archive we find into the textctrl, then stop looking

                                // Set checkbox to last-used value
((wxCheckBox*)FindWindow(wxT("Force")))->SetValue(m_parent->ArchiveExtractForce);
}

void ExtractArchiveDlg::OnFileBrowse(wxCommandEvent& WXUNUSED(event))  // Browse for archive
{
wxFileDialog fdlg(this,_("Browse for the Archive to Verify"), m_parent->active->GetActiveDirPath(), wxT(""), wxT("*"), wxFD_OPEN);
if (fdlg.ShowModal() != wxID_OK) return;    
  
wxString filepath = fdlg.GetPath();
if (!filepath.IsEmpty())
  text->ChangeValue(filepath);
}

void ExtractArchiveDlg::OnCreateInBrowse(wxCommandEvent& WXUNUSED(event))  // Browse for the dir into which to extract the archive
{
#if defined(__WXGTK__)
  int style = wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST;      // the gtk2 dirdlg uses different flags :/
#else
  int style = wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER;
#endif

wxDirDialog ddlg(this,_("Choose the Directory in which to Extract the archive"), m_parent->active->GetActiveDirPath(), style);
if (ddlg.ShowModal() != wxID_OK) return;    

wxString filepath = ddlg.GetPath();
if (!filepath.IsEmpty())   CreateInCombo->SetValue(filepath);
}

void ExtractArchiveDlg::OnUpdateUI(wxUpdateUIEvent& event)
{
event.Enable(!text->GetValue().IsEmpty());  // Enable the OK button if there's an archive name in textctrl
  // While we're here, set and disable the 'Overwrite' checkbox if it's a 7z or ar archive (as the Linux version probably can't be told not to)
bool mustoverwrite = text->GetValue().Right(3) == wxT(".7z") || text->GetValue().Right(2) == wxT(".a") || text->GetValue().Right(3) == wxT(".ar");
wxCheckBox* overwrite = (wxCheckBox*)FindWindow(wxT("Force"));
if (overwrite) // It'll be NULL if this is the Verify dialog
  { overwrite->Enable(!mustoverwrite);
    if (mustoverwrite) overwrite->SetValue(true);
  }
}

void ExtractArchiveDlg::OnOK(wxCommandEvent &event)
{
if (!((wxButton*)event.GetEventObject())->IsEnabled()) return;  // If insufficient data to allow wxID_OK, ignore the keypress
                        // If there is a new entry in the create-in comb, add it to the history if it's unique
wxString string = CreateInCombo->GetValue();
if (!(string.IsEmpty() || string==wxT("./")) ) 
  { int index = m_parent->ArchiveHistory.Index(string, false); // See if this entry already exists in the history array
    if (index != wxNOT_FOUND)
      m_parent->ArchiveHistory.RemoveAt(index);                // If so, remove it.  We'll add it back into position zero  
    m_parent->ArchiveHistory.Insert(string, 0);                // Either way, insert into position zero
  }
  
EndModal(wxID_OK);
}

class ExtractDeb // Helper class for the deb section of ExtractArchiveDlg::GetCommand. Nowadays the .deb constituents may be either tar.gz or tar.xz, and we can't rely on tar -x to cope
{
public:
ExtractDeb(const wxString& deb, const wxString& cdir, bool force) : archive(deb), createindir(cdir)
 {  wxCHECK_RET(!archive.empty() && !createindir.empty(), wxT("You must pass a filename and dest dir"));
    archiveprefix = wxT("ar -p \\\""); archivepostfix = wxT("\\\" ");
    tarprefix = wxT(" | tar -"); tarpostfix = wxT("xv -C \\\"");
    overwrite = force ? wxT("\\\" --overwrite") : wxT("\\\" --skip-old-files");

    wxArrayString output, errors;
    long ans = wxExecute(wxString(wxT("ar -t ")) + deb, output,errors);
    wxCHECK_RET(!ans, wxT("Trying to run 'ar' unaccountably failed"));

    for (size_t n=0; n < output.GetCount(); ++n)
      { if (output[n].StartsWith(wxT("control")))
          { control = output[n]; 
            continue;
          }
        if (output[n].StartsWith(wxT("data")))
          { data = output[n]; continue; }
        binary = output[n];
      }
 }
const wxString GetControl() { return archiveprefix + archive + archivepostfix + control + tarprefix + GetCompression(control) + tarpostfix + createindir + overwrite; }
const wxString GetData() { return archiveprefix + archive + archivepostfix + data + tarprefix + GetCompression(data) + tarpostfix + createindir + overwrite; }
const wxString GetBinary() { return wxString(wxT("ar -p \\\"")) + archive + archivepostfix + binary + wxT(" > \\\"") + createindir +  wxT('/') + binary +  wxT("\\\" "); }

protected:
wxString GetCompression(const wxString& file)
  { wxString comp;
    if (file.Right(2) == wxT("gz")) comp = wxT("z");
      else { if (file.Right(2) == wxT("xz")) comp = wxT("J"); }
    return comp; // If !gz && !xz we'll just have to hope that this version of tar can autodetect
  }

wxString archiveprefix, archivepostfix, tarprefix, tarpostfix;

wxString archive;
wxString createindir;
wxString control;
wxString data;
wxString binary; // This is debian-binary which, despite the name, is a text file that currrently says '2.0'
wxString overwrite;
};

 wxString LocateRARextractor() // Helper to find either unar or unrar
{
if (Configure::TestExistence("unar")) return "unar ";
if (Configure::TestExistence("unrar")) return "unrar x ";

return ""; 
}

wxString LocateRARverifier() // Helper to find either lsarr or unrar
{
if (Configure::TestExistence("lsar")) return "lsar -t ";
if (Configure::TestExistence("unrar")) return "unrar t ";

return ""; 
}
 
enum returntype ExtractArchiveDlg::GetCommand()
{
wxString command, decompresscommand;
wxString rpm2cpio(wxT("rpm2cpio "));

wxString createindir = CreateInCombo->GetValue();            // Get first the selected path for the archive
if (createindir == wxT("./"))  createindir = m_parent->active->GetActiveDirPath();

if (!wxDirExists(createindir))
  if (!wxFileName::Mkdir(createindir, 0777, wxPATH_MKDIR_FULL))
      { wxMessageDialog dialog(this, _("Failed to create the desired destination directory.\nTry again?"), wxT(""), wxYES_NO | wxICON_QUESTION);
        if (dialog.ShowModal() == wxID_YES)  return retry; else return cancel;
      }

m_parent->Updatedir.Add(createindir);                       // We'll want to update this dir once we're finished.  Grab the chance to save it

bool force = ((wxCheckBox*)FindWindow(wxT("Force")))->GetValue();
m_parent->ArchiveExtractForce = force;

wxString archive = text->GetValue();                        // Get the archive filepath
wxString archivename(archive.AfterLast(wxFILE_SEP_PATH)), archivepath(archive.BeforeLast(wxFILE_SEP_PATH));
if (createindir.IsEmpty() || (createindir == wxT("./"))) createindir = archivepath; // Needed for cpio etc

enum ziptype ztype = m_parent->Categorise(archive);         // Is it a bird, is it a plane . . .?
switch(ztype)
  { case zt_htb:                                            // htb's are effectively zips  
    case zt_zip:command.Printf(wxT("echo Unzipping %s;"), archivename.c_str());
                command << wxT("unzip ");
                command << (force ? wxT("-o") : wxT("-n"));
                command << wxT(" \\\"") << archive << wxT("\\\""); // Add the archive filepath
                if (!createindir.IsEmpty())                 // Extract to the destination path, if one was requested
                  { wxString destdir; destdir.Printf(wxT(" -d \\\"%s\\\" "), createindir.c_str());
                    command << destdir;
                  }  
                break;
    case zt_7z: // All 7z's are archives. This is for any 'standard', non-tar ones
               {wxString sevenzbinary(m_parent->GetSevenZString()); if (sevenzbinary.empty()) return cancel;
                wxString oldcwd = GetCwd();           // 7z really hates dealing with filepaths, so keep things relative
                command.Printf(wxT("echo Extracting %s;"), archivename.c_str());
                command << wxT("cd \\\"") << archivepath << wxT("\\\" && ")
                        << sevenzbinary << wxT(" x -y \\\"") << archivename << wxT("\\\"");  // 'x' is extract retaining filepaths, -y == yes-to-everything i.e. Overwrite
                if (!createindir.IsEmpty()) command << wxT(" -o\\\"") << createindir << wxT("\\\"");
                command << wxT(" && cd \\\"") << oldcwd << wxT("\\\"");
                break;}
    case zt_tar7z: // 7z is very different!
               {wxString sevenzbinary(m_parent->GetSevenZString()); if (sevenzbinary.empty()) return cancel;
                wxString oldcwd = GetCwd();           // 7z really hates dealing with filepaths, so keep things relative
                decompresscommand << wxT("cd \\\"") << archivepath << wxT("\\\" && ") << sevenzbinary << wxT(" e -y \\\"") 
                                  << archivename << wxT("\\\" && cd \\\"") << oldcwd << wxT("\\\" && ");
               }// Fall through towards zt_taronly for the proper extraction

    case zt_targz: case zt_tarbz: case zt_tarlzma: case zt_tarxz: case zt_tarlzop: case zt_taZ:
                // Variously-commpressed tarballs. We used to specify the correct option e.g. -j. Since v1.15 in 12/04 tar can work it out for itself
    case zt_taronly:
                command.Printf(wxT("echo Extracting %s;"), archivename.c_str());
                command << decompresscommand;               // Only relevant for tar.7z
                if (!createindir.IsEmpty())                 // cd to the destination path, if one was requested
                  { wxString changedir; changedir.Printf(wxT("(cd \\\"%s\\\" && "), createindir.c_str());
                    command << changedir;
                  }  
                command << wxT("tar ");
                if (force)  command << wxT("--overwrite ");
                command << wxT(" -xv");
                if (!force)  command << wxT('k');
                if (ztype == zt_tar7z)
                  { wxString uncompressedfilepath(ArchiveDialogBase::FindUncompressedName(archive, ztype));
                    command << wxT("f \\\"") << uncompressedfilepath    // Add the extracted tarball filepath
                            << wxT("\\\" && rm -f \\\"") << uncompressedfilepath << wxT("\\\"");  // and delete it after
                  }
                 else
                  command << wxT("f \\\"") << archive << wxT("\\\"");  // Add the archive filepath
                if (!createindir.IsEmpty()) command << wxT(')');       // Revert any cd
                break;
    case zt_cpio:
                command.Printf(wxT("echo Extracting %s; "), archivename.c_str());
                command << wxT("(cd \\\"") << createindir << wxT("\\\" && cpio -idv ");
                if (force)  command << wxT("--unconditional ");
                command << wxT("-I \\\"") << archive << wxT("\\\")");
                break;
    case zt_rpm:  // Unavoidable code duplication :(
                if (!Configure::TestExistence(wxT("rpm2cpio")))  { wxMessageBox(_("Can't find rpm2cpio on your system..."), _("Oops"), wxOK, this); return cancel; }
                if (createindir.IsEmpty() || (createindir == wxT("./"))) createindir = archivepath; // cpio defaults to the 4Pane filepath :/
                command.Printf(wxT("echo Extracting %s; "), archivename.c_str());
                command << wxT("(cd \\\"") << createindir << wxT("\\\" && rpm2cpio \\\"") << archive << wxT("\\\" | cpio -idv ");
                if (force)  command << wxT("--unconditional ");
                command << wxT(')');  break;
    case zt_ar: if (!Configure::TestExistence(wxT("ar"))) { wxMessageBox(_("Can't find ar on your system..."), _("Oops"), wxOK, this); return cancel; }
                command << wxString::Format(wxT("echo Extracting %s && "), archivename.c_str()) << wxT("(cd \\\"") << createindir
                        << wxT("\\\" && ar xv \\\"") << archive <<  wxT("\\\")"); break;
    case zt_deb:{if (!Configure::TestExistence(wxT("ar"))) { wxMessageBox(_("Can't find ar on your system..."), _("Oops"), wxOK, this); return cancel; }
                ExtractDeb helper(archive, createindir, force);
                command << wxString::Format(wxT("echo Extracting %s && "), archivename.c_str())
                        << helper.GetBinary();
                command << wxT(" && ") << helper.GetControl();
                command << wxT(" && ") << helper.GetData();
                break;
                }
    case zt_rar:{ wxString derar = LocateRARextractor(); 
                      if (derar.IsEmpty())  { wxMessageBox(_("Can't find unar either or unrar on your system..."), _("Oops"), wxOK, this); return cancel; }
                      command << wxString::Format("echo Extracting %s && (cd \\\"%s\\\" && %s  \\\"%s\\\" )", archivename, createindir, derar, archive); break;}

    default:    wxMessageDialog dialog(this, _("Can't find a valid archive to extract.\nTry again?"), wxT(""), wxYES_NO | wxICON_QUESTION);
                if (dialog.ShowModal() == wxID_YES)  return retry; else return cancel;
  }

m_parent->command = command;
return valid;
}

BEGIN_EVENT_TABLE(ExtractArchiveDlg,wxDialog)
  EVT_BUTTON(XRCID("Browse"), ExtractArchiveDlg::OnFileBrowse)
  EVT_BUTTON(XRCID("CreateInBrowse"), ExtractArchiveDlg::OnCreateInBrowse)
  EVT_BUTTON(wxID_OK, ExtractArchiveDlg::OnOK)
  EVT_TEXT_ENTER(wxID_ANY, ExtractArchiveDlg::OnOK)
  EVT_UPDATE_UI(wxID_OK, ExtractArchiveDlg::OnUpdateUI)
END_EVENT_TABLE()
//-----------------------------------------------------------------------------------------------------------------------

VerifyCompressedDialog::VerifyCompressedDialog(Archive* parent) : DecompressDialog()
{
m_parent = parent;
bool loaded = wxXmlResource::Get()->LoadDialog(this, MyFrame::mainframe, wxT("VerifyCompressedDlg"));
wxCHECK_RET(loaded, wxT("Failed to load VerifyCompressedDlg from xrc"));
Init();

combo = (wxComboBox*)FindWindow(wxT("Combo"));
list = (MyListBox*)FindWindow(wxT("CurrentFiles"));

size_t count = m_parent->FilepathArray.GetCount();
for (size_t n=0; n < count; ++n)
  { enum ziptype ztype = m_parent->Categorise(m_parent->FilepathArray[n]);
    if (ztype <= zt_lastcompressed)
      list->Append(m_parent->FilepathArray[n]);                // If this is a compressed file put it in the files-to-be-verified listbox
  }
}

enum returntype VerifyCompressedDialog::GetCommand()  // Parse entered options into a command
{
wxString command; wxArrayString gz, bz, xz, sevenz, lzo;

if (!list->GetCount()) return cancel;                         // Shouldn't happen, as Return would be disabled

m_parent->FilepathArray.Clear();                              // In case files were added/deleted during the dialog, clear the old data
for (size_t n=0; n < list->GetCount(); ++n)
  m_parent->FilepathArray.Insert(list->GetString(n), 0);

        // We may have 5 sorts of file in the array: bz2, gz (and Z), xz (and lzma), lzo, and ordinary files added in error
if (!SortFiles(m_parent->FilepathArray, gz, bz, xz, sevenz, lzo, false))       // If there weren't any compressed files selected...
  { wxMessageDialog dialog(this, _("No relevant compressed files were selected.\nTry again?"), wxT(""), wxYES_NO | wxICON_QUESTION);
    if (dialog.ShowModal() == wxID_YES)  return retry; else return cancel;
  }
  
if (!gz.IsEmpty())                                            // Create a gunzip command, adding relevant files
  { command << wxT("gunzip -tv");                             // Test, Verbose
    for (size_t c=0; c < gz.GetCount(); ++c)
      command << wxT(" \\\"") << gz[c] << wxT("\\\"");
  }

if (!bz.IsEmpty())
  { if (!command.empty()) command << wxT(" && ");
    command << wxT("bunzip2 -tv"); 
    for (size_t c=0; c < bz.GetCount(); ++c)
      command << wxT(" \\\"") << bz[c] << wxT("\\\"");
  }

if (!xz.IsEmpty())
  { if (!command.empty()) command << wxT(" && ");
    command << wxT("xz -tv"); 
    for (size_t c=0; c < xz.GetCount(); ++c)
      command << wxT(" \\\"") << xz[c] << wxT("\\\"");
  }

if (!lzo.IsEmpty())
  { if (!command.empty()) command << wxT(" && ");
    command << wxT("lzop -tv"); 
    for (size_t c=0; c < lzo.GetCount(); ++c)
      command << wxT(" \\\"") << lzo[c] << wxT("\\\"");
  }

if (!command.empty())
  { m_parent->command = wxT("echo Verifying; ") + command; return valid; }
 else return cancel;
}

//-----------------------------------------------------------------------------------------------------------------------
VerifyArchiveDlg::VerifyArchiveDlg(Archive* parent) : ExtractArchiveDlg()
{
m_parent = parent;
bool loaded = wxXmlResource::Get()->LoadDialog(this, MyFrame::mainframe,wxT("VerifyArchiveDlg"));
wxCHECK_RET(loaded, wxT("Failed to load VerifyCompressedDlg from xrc"));

text = (wxTextCtrl*)FindWindow(wxT("text"));

m_parent->active->GetMultiplePaths(m_parent->FilepathArray);     // Load the selected files into the stringarray

size_t count = m_parent->FilepathArray.GetCount();
for (size_t n=0; n < count; ++n)
  if (ArchiveDialogBase::IsAnArchive(m_parent->FilepathArray[n]))
      { text->AppendText(m_parent->FilepathArray[n]); break; }   // Put 1st archive we find into the textctrl, then stop looking
}

void VerifyArchiveDlg::OnOK(wxCommandEvent& event)
{
if (!((wxButton*)event.GetEventObject())->IsEnabled()) return;   // If insufficient data to allow wxID_OK, ignore the keypress

EndModal(wxID_OK);
}

enum returntype VerifyArchiveDlg::GetCommand()
{
wxString command;

wxString archivefilepath = text->GetValue();          // Get the archive filepath
wxString archivename = archivefilepath.AfterLast(wxT('/')), archivepath(archivefilepath.BeforeLast(wxFILE_SEP_PATH)); 
switch(m_parent->Categorise(archivefilepath))
  { case zt_htb:
    case zt_zip:
            command.Printf(wxT("echo Verifying %s;"), archivename.c_str());
            command << wxT("unzip -t");
            command << wxT(" \\\"") << archivefilepath << wxT("\\\"");  // Add the archive filepath
            break;
    case zt_7z: // All 7z's are archives. This is for any 'standard', non-tar ones
           {wxString sevenzbinary(m_parent->GetSevenZString());
            if (sevenzbinary.empty())  { wxMessageBox(_("Can't find a 7z binary on your system"), _("Oops"), wxOK, this); return cancel; }
            // 7z can actually cope with filepaths for verification!
            command.Printf(wxT("echo Verifying %s; "), archivename.c_str());
            command << sevenzbinary << wxT(" t \\\"") << archivefilepath << wxT("\\\""); break;}// 't' is 'verify'
    case zt_tar7z: // Here we need to decompress first to get to the tarball
           {wxString uncompressedfilepath(ArchiveDialogBase::FindUncompressedName(archivefilepath, zt_tar7z)); wxString sevenzbinary(m_parent->GetSevenZString());
            if (sevenzbinary.empty())  { wxMessageBox(_("Can't find a 7z binary on your system"), _("Oops"), wxOK, this); return cancel; }
            wxString oldcwd = GetCwd();           // 7z really hates dealing with filepaths, so keep things relative
            command << wxString::Format(wxT("echo Verifying %s && cd \\\""), archivename.c_str()) << archivepath << wxT("\\\" && ") << sevenzbinary << wxT(" e \\\"")
                    << archivename << wxT("\\\" && tar -tvf \\\"") << uncompressedfilepath <<  wxT("\\\" && rm -f \\\"") << uncompressedfilepath 
                    << wxT("\\\" && cd \\\"") << oldcwd << wxT("\\\""); break;}
    case zt_targz: case zt_tarbz: case zt_tarlzma: case zt_tarxz: case zt_tarlzop: case zt_taZ: // tar can now list compressed archives on the fly :)
    case zt_taronly:
            command << wxString::Format(wxT("echo Verifying %s && "), archivename.c_str()) << wxT("tar -tvf \\\"") << archivefilepath <<  wxT("\\\""); break;
    case zt_rpm:
            if (!Configure::TestExistence(wxT("rpm2cpio")))
              { wxMessageBox(_("Can't find rpm2cpio on your system..."), _("Oops"), wxOK, this); return cancel; }
            command.Printf(wxT("echo Verifying %s; "), archivename.c_str());
            command << wxT("rpm2cpio \\\"") << archivefilepath << wxT("\\\" | cpio -t"); break;
    case zt_cpio:
            command.Printf(wxT("echo Verifying %s; "), archivename.c_str());
            command << wxT("cpio -t ") << wxT("-I \\\"") << archivefilepath << wxT("\\\""); break;
    case zt_ar:
            command << wxString::Format(wxT("echo Verifying %s && "), archivename.c_str()) << wxT("ar tv \\\"") << archivefilepath <<  wxT("\\\""); break;
    case zt_rar:
                      { wxString verifyrar = LocateRARverifier(); 
                      if (verifyrar.IsEmpty()) { wxMessageBox(_("Can't find either lsar or unrar on your system..."), _("Oops"), wxOK, this); return cancel; }
                      command << wxString::Format("echo Verifying %s && %s ", archivename, verifyrar) << "\\\"" << archivefilepath <<  "\\\""; break;
                      }
    case zt_deb:
            command << wxString::Format(wxT("echo Verifying %s && "), archivename.c_str())
                    << wxT("ar p \\\"") << archivefilepath <<  wxT("\\\" control.tar.gz | tar -ztv && ")
                    << wxT("ar p \\\"") << archivefilepath <<  wxT("\\\" data.tar.gz | tar -ztv"); break;
     default: wxMessageDialog dialog(this, _("Can't find a valid archive to verify.\nTry again?"), wxT(""), wxYES_NO | wxICON_QUESTION);
              return (dialog.ShowModal() == wxID_YES) ? retry : cancel;
  }

m_parent->command = command;                                 // Pass the command string to parent's version
return valid;
}

//-----------------------------------------------------------------------------------------------------------------------

wxString Archive::m_7zString;

void Archive::ExtractOrVerify(bool extract /*=true*/)  // Extracts or verifies either archives or compressed files, depending on which are selected
{
    // Are we decompressing or extracting?  If there is even 1 compressed non-archive file selected, Decompress
    // If there are only archives (+/- rubbish), then select the 1st archive (there should only be one anyway) to extract

active->GetMultiplePaths(FilepathArray);

bool archivefound = false;
if (!FilepathArray.IsEmpty())
  { for (size_t n=0; n < FilepathArray.GetCount(); ++n)
      { enum ziptype ztype = Categorise(FilepathArray[n]);
        if (ztype <= zt_lastcompressed)                  // We've found a compressed non-archive so Decompress/Verify this/these
          { if (extract) return DoExtractCompressVerify(ecv_decompress); 
             else        return DoExtractCompressVerify(ecv_verifycompressed);
          }
         else if (ztype != zt_invalid) archivefound = true;
      }
    if (archivefound)                                    // We found no compressed non-archives, & >0 archives, so extract/verify this
      { if (extract) return DoExtractCompressVerify(ecv_extractarchive); 
         else        return DoExtractCompressVerify(ecv_verifyarchive);
      }
  }
  
          // If we're here, either there were no files selected, or they were all rubbish.  Ask for directions
DecompressOrExtractDlg dlg;
if (extract)
  wxXmlResource::Get()->LoadDialog(&dlg, MyFrame::mainframe, wxT("DecompressOrExtractDlg"));
 else
  wxXmlResource::Get()->LoadDialog(&dlg, MyFrame::mainframe, wxT("VerifyWhichDlg"));  // VerifyWhichDlg is derived from the above; only labels are different
int ans = dlg.ShowModal();
 
if (ans==XRCID("Archive"))
  { if (extract) return DoExtractCompressVerify(ecv_extractarchive); 
     else return DoExtractCompressVerify(ecv_verifyarchive);
  }

if (ans==XRCID("Compressed"))
  { if (extract) return DoExtractCompressVerify(ecv_decompress); 
     else return DoExtractCompressVerify(ecv_verifycompressed);
  }

delete this;                                               // If neither choice was taken, abort
}

void Archive::DoExtractCompressVerify(enum ExtractCompressVerify_type type)
{
enum returntype ans; wxString successmsg, failuremsg;

wxDialog* dlg = NULL;
switch(type)
  { case ecv_compress:
            {dlg = new CompressDialog(this);
             successmsg = _("File(s) compressed"); failuremsg = _("Compression failed");
             break;}
    case ecv_decompress:
            {dlg = new DecompressDialog(this);
             successmsg = _("File(s) decompressed"); failuremsg = _("Decompression failed");
             break;}
    case ecv_verifycompressed:
            {dlg = new VerifyCompressedDialog(this);
             successmsg = _("File(s) verified"); failuremsg = _("Verification failed");
             break;}

    case ecv_makearchive:
            {dlg = new MakeArchiveDialog(this, true);
             LoadHistory();
             successmsg = _("Archive created"); failuremsg = _("Archive creation failed");
             break;}
    case ecv_addtoarchive:
            {dlg = new MakeArchiveDialog(this, false);
             LoadHistory();
             successmsg = _("File(s) added to Archive"); failuremsg = _("Archive addition failed");
             break;}
    case ecv_extractarchive:
            {dlg = new ExtractArchiveDlg(this);
             LoadHistory();
             successmsg = _("Archive extracted"); failuremsg = _("Extraction failed");
             break;}
    case ecv_verifyarchive:
            {dlg = new VerifyArchiveDlg(this);
             successmsg = _("Archive verified"); failuremsg = _("Verification failed");
             break;}
     default: return;
  }

do
  { command.Clear();                       // Make sure there's no stale data
    if (dlg->ShowModal() != wxID_OK)
      { ans = cancel; break; }
    
    switch(type)
      { case ecv_compress:          ans = static_cast<CompressDialog*>(dlg)->GetCommand(); break;
        case ecv_decompress:        ans = static_cast<DecompressDialog*>(dlg)->GetCommand(); break;
        case ecv_verifycompressed:  ans = static_cast<VerifyCompressedDialog*>(dlg)->GetCommand(); break;
        case ecv_makearchive:
        case ecv_addtoarchive:      ans = static_cast<MakeArchiveDialog*>(dlg)->GetCommand(); break;
        case ecv_extractarchive:    ans = static_cast<ExtractArchiveDlg*>(dlg)->GetCommand(); break;
        case ecv_verifyarchive:     ans = static_cast<VerifyArchiveDlg*>(dlg)->GetCommand(); break;
         default: return;
      }
 
    if (ans == retry)  continue;
    break;
  }
 while (1);

if (ans == cancel)   return;

command = wxT("sh -c \"") + command; command += wxT('\"'); // Since command is multiple (if only with echos) we need to execute it thru a shell

            // We now have the command.  Do the dialog that runs it and displays the results
wxBusyCursor busy;

CommandDisplay OutputDlg;
wxXmlResource::Get()->LoadDialog(&OutputDlg, MyFrame::mainframe, wxT("OutputDlg"));
LoadPreviousSize(&OutputDlg, wxT("OutputDlg"));

OutputDlg.Init(command);
OutputDlg.ShowModal();
SaveCurrentSize(&OutputDlg, wxT("OutputDlg"));

if (!OutputDlg.exitstatus) BriefLogStatus bls(successmsg);
 else  BriefLogStatus bls(_("Verification failed"));

if ((type != ecv_verifycompressed) && (type != ecv_verifyarchive))
  UpdatePanes();

dlg->Destroy();
delete this;
}

//static
enum ziptype Archive::Categorise(const wxString& ftype)    // Returns the type of file in filetype ie .gz, .tar etc
{
wxCHECK_MSG(!ftype.empty(), zt_invalid, wxT("An empty type..."));
if (ftype.Last() == wxFILE_SEP_PATH) return zt_invalid;

wxString endExt, prevExt, filetype = ftype.AfterLast(wxFILE_SEP_PATH);  // We're only interested in the current filename, not a dir with a '.' halfway up the path
                                  
endExt = filetype.AfterLast(wxT('.'));
if (endExt == filetype) return zt_invalid;  // Check there IS an ext (AfterLast() returns whole string if the char isn't found)

prevExt = (filetype.BeforeLast(wxT('.'))).AfterLast(wxT('.')); // Get any previous ext, in case of foo.tar.gz

if (endExt==wxT("htb"))  return zt_htb;
if (endExt==wxT("zip") || endExt==wxT("xpi"))  return zt_zip; // 'xpi' is used for a Firefox browser addon, but it's really just a zip

if (endExt==wxT("tar"))                                                 return zt_taronly;
if (endExt==wxT("cpio"))                                                return zt_cpio;
if (endExt==wxT("rpm"))                                                 return zt_rpm;  // which we're treating as an archive
if (endExt==wxT("deb")  || endExt==wxT("ddeb"))                         return zt_deb;  // ditto (and debian now labels debug debs 'ddeb')
if (endExt==wxT("ar")   || endExt==wxT("a"))                            return zt_ar;
if (endExt=="rar")                                                      return zt_rar;

if (endExt==wxT("tgz")  || (prevExt==wxT("tar") && endExt==wxT("gz")))  return zt_targz;
if (endExt==wxT("tbz2") || endExt==wxT("tbz"))                          return zt_tarbz;
if (endExt==wxT("txz"))                                                 return zt_tarxz;
if (endExt==wxT("tlz"))                                                 return zt_tarlzma;
if (endExt==wxT("taZ"))                                                 return zt_taZ;

if (prevExt==wxT("tar") && (endExt==wxT("bz2") || endExt==wxT("bz")))   return zt_tarbz;
if (prevExt==wxT("tar") && endExt==wxT("lzma"))                         return zt_tarlzma;
if (prevExt==wxT("tar") && endExt==wxT("xz"))                           return zt_tarxz;
if (prevExt==wxT("tar") && endExt==wxT("7z"))                           return zt_tar7z;
if (prevExt==wxT("tar") && endExt==wxT("lzo"))                          return zt_tarlzop;
if (prevExt==wxT("tar") && endExt==wxT("Z"))                            return zt_taZ;

if (endExt==wxT("gz"))   return zt_gzip;       // Since we've already tested for e.g. foo.tar.gz, it's safe to test for foo.gz
if (endExt==wxT("bz2"))  return zt_bzip;
if (endExt==wxT("lzma")) return zt_lzma;
if (endExt==wxT("xz"))   return zt_xz;
if (endExt==wxT("7z"))   return zt_7z;
if (endExt==wxT("lzo"))  return zt_lzop;
if (endExt==wxT("Z"))    return zt_compress;

return zt_invalid;
}

//static
enum GDC_images Archive::GetIconForArchiveType(const wxString& filename, bool IsFakeFS)
{
ziptype zt = Categorise(filename);
wxCHECK_MSG(zt != zt_invalid, GDC_file, wxT("Wrongly trying to find a archive icon"));

return GetIconForArchiveType(zt, IsFakeFS);
}

//static
enum GDC_images Archive::GetIconForArchiveType(ziptype zt, bool IsFakeFS)
{
if (zt <= zt_lastcompressed) return IsFakeFS ? GDC_ghostcompressedfile : GDC_compressedfile;
if ((zt == zt_taronly) || (zt == zt_ar) || (zt == zt_cpio)) return IsFakeFS ? GDC_ghosttarball : GDC_tarball;  // These are uncompressed archives
if ((zt > zt_cpio /*OK as we already checked for zt_ar*/) && (zt <= zt_lastcompressedarchive))
  return IsFakeFS ? GDC_ghostcompressedtar : GDC_compressedtar;

wxCHECK_MSG(false, GDC_file, wxT("How did we get here?"));
}

//static
bool Archive::IsThisCompressorAvailable(enum ziptype type)
{
wxCHECK_MSG((type <= zt_lastcompressed) || (type ==zt_7z), false, wxT("'type' is not a compressor"));

if (type == zt_7z) // Do this first, as it's more complicated. There are 3 possible packages
  { m_7zString = wxT("7z"); if (Configure::TestExistence(m_7zString)) return true;
    m_7zString = wxT("7za"); if (Configure::TestExistence(m_7zString)) return true;
    m_7zString = wxT("7zr"); if (Configure::TestExistence(m_7zString)) return true;
    m_7zString.Clear(); return false;
  }
    // Now the rest:
const int values[] = { zt_gzip, zt_bzip, zt_xz, zt_lzma, zt_lzop, zt_compress }; // Protect against future changes in ziptype order
const wxString strings[] = 
  { wxString(wxT("gzip")), wxString(wxT("bzip2")), wxString(wxT("xz")), wxString(wxT("xz")) /*we use xz for lzma*/, wxString(wxT("lzop")), wxString(wxT("compress")) };
wxString compressor;

for (size_t n=0; n < sizeof(values)/sizeof(int); ++n)
  if (values[n] == type) compressor = strings[n];
wxCHECK_MSG(!compressor.empty(), false, wxT("'type' not found in string array"));

return Configure::TestExistence(compressor);
}

void Archive::LoadHistory()    // Load the combobox history into the stringarray
{
config = wxConfigBase::Get();
if (config==NULL)  { wxLogError(_("Couldn't load configuration!?")); return; }

config->SetPath(wxT("/History/Archive/ArchiveFilePaths/"));

size_t count = config->GetNumberOfEntries();
if (!count) { config->SetPath(wxT("/")); return; }

ArchiveHistory.Clear();

wxString item, key, histprefix(wxT("hist_"));  
for (size_t n=0; n < count; ++n)                    // Create the key for every item, from hist_a to hist_('a'+count-1)
  { key.Printf(wxT("%c"), wxT('a') + (wxChar)n);
    config->Read(histprefix+key, &item);
    if (item.IsEmpty()) continue;
    ArchiveHistory.Add(item);
  }

config->SetPath(wxT("/"));  
}

void Archive::LoadPrefs()      // Load the last-used values for checkboxes etc
{
config = wxConfigBase::Get();                   // Find the config data
if (config==NULL)  { wxLogError(_("Couldn't load configuration!?")); return; }

config->SetPath(wxT("/History/Archive/LastUsedValues/"));
wxString key, str; long lng;
                                                // Compress dialog
key = wxT("Slidervalue"); config->Read(key, &lng, (long)6); Slidervalue = (unsigned int)lng;
key = wxT("Compressionchoice"); config->Read(key, &lng, (long)1); m_Compressionchoice = (unsigned int)lng;
key = wxT("Recurse"); config->Read(key, &Recurse, 1);
key = wxT("Force"); config->Read(key, &Force, 0);
                                                // Decompress dialog
key = wxT("DecompressRecurse"); config->Read(key, &DecompressRecurse, 1);
key = wxT("DecompressForce"); config->Read(key, &DecompressForce, 0);
key = wxT("DecompressArchivesToo"); config->Read(key, &DecompressArchivesToo, 0);

key = wxT("ArchiveExtractForce"); config->Read(key, &ArchiveExtractForce, 0);  // Archive Extract dialog
                                                // CreateAppend dialogs
key = wxT("DerefSymlinks"); config->Read(key, &DerefSymlinks, 0);
key = wxT("Verify"); config->Read(key, &m_Verify, 0);
key = wxT("DeleteSource"); config->Read(key, &DeleteSource, 0);
key = wxT("ArchiveCompress"); config->Read(key, &lng, (long)1); ArchiveCompress = (unsigned int)lng;

config->SetPath(wxT("/"));
}

void Archive::SaveHistory()  // Save the Archive filepath history
{
config = wxConfigBase::Get();
if (config==NULL)  { wxLogError(_("Couldn't load configuration!?")); return; }

config->DeleteGroup(wxT("/History/Archive/ArchiveFilePaths"));  // Delete current info, otherwise we'll end up with duplicates or worse
config->SetPath(wxT("/History/Archive/ArchiveFilePaths/"));

size_t count = wxMin(ArchiveHistory.GetCount(), MAX_COMMAND_HISTORY);  // Count the entries to be saved: not TOO many
if (!count) { config->SetPath(wxT("/")); return; }

wxString item, key, histprefix(wxT("hist_")); // Create the key for every item, from hist_a to hist_('a'+count-1)
for (size_t n=0;  n < count; ++n)             // The newest command is stored at the beginning of the array
  { key.Printf(wxT("%c"), wxT('a') + (wxChar)n);
    config->Write(histprefix+key, ArchiveHistory[n]);
  }

config->Flush();
config->SetPath(wxT("/"));  
}

void Archive::SavePrefs()    // Save the last-used values for checkboxes etc
{
config = wxConfigBase::Get();
if (config==NULL)  { wxLogError(_("Couldn't load configuration!?")); return; }

wxString path(wxT("/History/Archive/LastUsedValues/"));

                                                // Compress dialog
config->Write(path+wxT("Slidervalue"), (long)Slidervalue);
config->Write(path+wxT("Compressionchoice"), (long)m_Compressionchoice);
config->Write(path+wxT("Recurse"), Recurse);
config->Write(path+wxT("Force"), Force);
                                                // Decompress dialog
config->Write(path+wxT("DecompressRecurse"), DecompressRecurse);
config->Write(path+wxT("DecompressForce"), DecompressForce);
config->Write(path+wxT("DecompressArchivesToo"), DecompressArchivesToo);
                                                // Archive Extract dialog
config->Write(path+wxT("ArchiveExtractForce"), ArchiveExtractForce);
                                                // CreateAppend dialogs
config->Write(path+wxT("DerefSymlinks"), DerefSymlinks);
config->Write(path+wxT("Verify"), m_Verify);
config->Write(path+wxT("DeleteSource"), DeleteSource);
config->Write(path+wxT("ArchiveCompress"), (long)ArchiveCompress);

config->Flush();
}


void Archive::UpdatePanes()  // Work out which dirs are most likely to have been altered, & update them
{
                // Updatedir may already have an entry, for the current dir
for (size_t n=0; n < FilepathArray.Count(); ++n)  // Go thru the items that were in the listbox, adding their dirs to the array
  { wxString path = FilepathArray[n];
    FileData fd(path);  
    if (fd.IsRegularFile() || fd.IsSymlink())  path = path.BeforeLast(wxFILE_SEP_PATH);  // We only want paths, not filepaths
    Updatedir.Add(path);                          // Store it
  }
if (Updatedir.IsEmpty()) return;

MyGenericDirCtrl::Clustering = true;
wxArrayInt IDs;
for (size_t n=0; n < Updatedir.Count(); ++n)      // Go thru Updatedir, adding each item to the OnUpdateTrees() store
  MyFrame::mainframe->OnUpdateTrees(Updatedir[n], IDs);

MyFrame::mainframe->UpdateTrees();                // Now flag actually to do the update
}


