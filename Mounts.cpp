/////////////////////////////////////////////////////////////////////////////
// Name:       Mounts.cpp
// Purpose:    Mounting partitions
// Part of:    4Pane
// Author:     David Hart
// Copyright:  (c) 2020 David Hart
// Licence:    GPL v3
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/log.h"
#include "wx/wx.h"
#include "wx/app.h"
#include "wx/frame.h" 
#include "wx/menu.h"
#include "wx/dirctrl.h"
#include "wx/tokenzr.h"
#include "wx/busyinfo.h"

#include "Externs.h"
#include "Devices.h"
#include "Mounts.h"
#include "MyDirs.h"
#include "MyGenericDirCtrl.h"
#include "MyFrame.h"
#include "Filetypes.h"
#include "Configure.h"
#include "Misc.h"

void DeviceAndMountManager::OnMountPartition()  // Acquire & try to mount a hard-disk partition, either via an fstab entry or the hard way
{
bool OtherPartitions = false; wxString partition, mount, options, checkboxes;

  { // Start by protecting against a stale rc/ file; it's easier to do here than later
    MountPartitionDialog dlg;
    wxXmlResource::Get()->LoadDialog(&dlg,MyFrame::mainframe, wxT("MountPartitionDlg"));
    if (dlg.FindWindow(wxT("MountptTxt"))) // This was present until 4.0
      return;
  }

FillPartitionArray();                                         // Acquire partition & mount data

MountPartitionDialog dlg;
wxXmlResource::Get()->LoadDialog(&dlg,MyFrame::mainframe, wxT("MountFromFstabDlg"));
LoadPreviousSize(&dlg, wxT("MountFromFstabDlg"));
if (!dlg.Init(this)) return;
#ifdef __GNU_HURD__
  bool passive(false);
#endif

int ans = dlg.ShowModal();
SaveCurrentSize(&dlg, wxT("MountFromFstabDlg"));
if (ans==XRCID("OtherPartitions"))                            // If we want to mount a non-fstab partition, use a different dialog
  { OtherPartitions = true;
    MountPartitionDialog dlg;
    wxXmlResource::Get()->LoadDialog(&dlg,MyFrame::mainframe, wxT("MountPartitionDlg"));
#ifdef __GNU_HURD__
    wxCheckBox* passivechk = XRCCTRL(dlg, "MS_PASSIVE", wxCheckBox);
    if (passivechk) passivechk->Show();                             // gnu-hurd has an extra option: to use a passive translator
    
    wxCheckBox* temp = XRCCTRL(dlg, "MS_NOATIME", wxCheckBox); if (temp) temp->Disable(); // Not available in gnu-hurd
                temp = XRCCTRL(dlg, "MS_NODEV", wxCheckBox);   if (temp) temp->Disable();
#endif
    LoadPreviousSize(&dlg, wxT("MountPartitionDlg"));
    if (!dlg.Init(this)) return;
    
    do
      { ans = dlg.ShowModal();
        SaveCurrentSize(&dlg, wxT("MountPartitionDlg"));
        if (ans != wxID_OK) return;
        mount = dlg.MountptCombo->GetValue();
        if (mount.IsEmpty())  
          { wxMessageDialog dialog(&dlg, _("You haven't entered a mount-point.\nTry again?"), wxT(""),wxYES_NO | wxICON_QUESTION);
            if (dialog.ShowModal() == wxID_YES) continue;     // Try again
              else return;
          }
        break;
      }  
     while (true);
     
    partition = dlg.PartitionsCombo->GetValue();              // Read the data from the dialog
#ifdef __LINUX__
    if (((wxCheckBox*)(dlg.FindWindow(wxT("MS_SYNCHRONOUS"))))->IsChecked()) checkboxes += wxT("sync,");
    if (((wxCheckBox*)(dlg.FindWindow(wxT("MS_NOATIME"))))->IsChecked())    checkboxes += wxT("noatime,");
    if (((wxCheckBox*)(dlg.FindWindow(wxT("MS_RDONLY"))))->IsChecked())     checkboxes += wxT("ro,");
    
    if (((wxCheckBox*)(dlg.FindWindow(wxT("MS_NOEXEC"))))->IsChecked())     checkboxes += wxT("noexec,"); 
    if (((wxCheckBox*)(dlg.FindWindow(wxT("MS_NODEV"))))->IsChecked())      checkboxes += wxT("nodev,");
    if (((wxCheckBox*)(dlg.FindWindow(wxT("MS_NOSUID"))))->IsChecked())     checkboxes += wxT("nosuid,");
    
    if (!checkboxes.IsEmpty()) options = wxT("-o ") + checkboxes.BeforeLast(wxT(','));
#endif
#ifdef __GNU_HURD__
    if (((wxCheckBox*)(dlg.FindWindow(wxT("MS_SYNCHRONOUS"))))->IsChecked()) checkboxes += wxT("-s ");
    if (((wxCheckBox*)(dlg.FindWindow(wxT("MS_RDONLY"))))->IsChecked())     checkboxes += wxT("-r ");
    if (((wxCheckBox*)(dlg.FindWindow(wxT("MS_NOEXEC"))))->IsChecked())     checkboxes += wxT("-E "); 
    if (((wxCheckBox*)(dlg.FindWindow(wxT("MS_NOSUID"))))->IsChecked())     checkboxes += wxT("--no-suid ");
    passive = (passivechk && passivechk->IsChecked());

#endif
  }
 else
   partition = dlg.PartitionsCombo->GetValue(); // We need this even for an fstab mount, for the ReadMtab(partition) later
  
if (ans != wxID_OK) return;

if (!OtherPartitions)  mount = dlg.FstabMountptTxt->GetValue();  
 
if (mount.IsEmpty()) return;

#ifndef __GNU_HURD__   // In hurd we add -c, that autocreates the mountpt
  FileData mountdir(mount);
  if (!mountdir.IsValid())
    { if (wxMessageBox(_("The mount-point for this partition doesn't exist.  Create it?"), wxEmptyString, wxYES_NO) != wxYES) return;
      if (!MkDir(mount))  return;
    }
#endif

EscapeQuote(mount);                                           // Avoid any problems due to single-quotes in the path

wxArrayString output, errors;
long result;

#ifdef __LINUX__
  if (!OtherPartitions)
    result = wxExecute(wxT("mount \"") + mount + wxT("\""), output, errors);  // Call mount sync (ie wait for answer) lest we update the tree display before the data arrives
   else
     { if (getuid() != 0)                                       // If user isn't super
        { if (WHICH_SU==mysu)  
            { if (USE_SUDO)	                                    // sudo chokes on singlequotes :/
                result = ExecuteInPty(wxT("sudo sh -c \"mount ") + options + wxT(" ") + partition + wxT(' ') + EscapeSpaceStr(mount) + wxT('\"'), output, errors);
               else
                result = ExecuteInPty(wxT("su -c \"mount ") + options + wxT(" ") + partition + wxT(" \'") + mount + wxT("\'\""), output, errors);
              if (result == CANCEL_RETURN_CODE) return;
            }
           else if (WHICH_SU==kdesu) result = wxExecute(wxT("kdesu \"mount ") + options + wxT(" ") + partition + wxT(" \'") + mount + wxT("\'\""), output, errors);
           else if (WHICH_SU==gksu)  result = wxExecute(wxT("gksu \"mount ") + options + wxT(" ") + partition + wxT(" \'") + mount + wxT("\'\""), output, errors);
           else if (WHICH_SU==gnomesu)  result = wxExecute(wxT("gnomesu --title "" -c \"mount ") + options + wxT(" ") + partition + wxT(" \'") + mount + wxT("\'\" -d"), output, errors);
           else if (WHICH_SU==othersu && !OTHER_SU_COMMAND.IsEmpty())  
                      result = wxExecute(OTHER_SU_COMMAND + wxT("\"mount ") + options + wxT(" ") + partition + wxT(" \'") + mount + wxT("\'\" -d"), output, errors);
            else { wxMessageBox(_("Sorry, you need to be root to mount non-fstab partitions"), wxT(":-(")); return; }
        }
       else
         result = wxExecute(wxT("mount ") + options + wxT(" ") + partition + wxT(" \"") + mount + wxT("\""), output, errors);  // If we happen to be root
    }
#else // __GNU_HURD__
  // Hurd doesn't seem to be able to mount an fstab entry just using the mountpt, so always use both. NB we ignore any fstab options: afaict there's no API to get them
  wxString ttype = (passive ? wxT(" -apc"): wxT(" -ac"));
  
  if (ExecuteInPty(wxT("settrans") + ttype + wxT(" \"")+ mount + wxT("\" /hurd/ext2fs ") + checkboxes + wxT('\"') + partition + wxT('\"'), output, errors) != 0)
    { result = ExecuteInPty(wxT("su -c \"settrans") + ttype + wxT(" \'")+ mount + wxT("\' /hurd/ext2fs ") + checkboxes + wxT("\'") + partition + wxT("\'\""), output, errors);
      if (result == CANCEL_RETURN_CODE) return;
    }

  ans = DeviceAndMountManager::GnuCheckmtab(mount, partition, TT_active); // Check for an active one; if both were requested, the passive one is unlikely to have failed alone
  if (ans == CANCEL_RETURN_CODE) return;
  result = (ans != 0);
#endif //!#ifdef __LINUX__


if (!result)
#ifdef __LINUX__
  { if (ReadMtab(partition) == NULL)  return; }   // Success, provided ReadMtab can find the partition. (kdesu returns 0 if the user cancels!)
     else
#endif
	{ wxString msg(_("Oops, failed to mount the partition."));
		size_t ecount = errors.GetCount();        		// If it failed, show any (reasonable number of) error messages
		if (ecount)
			{ msg << _(" The error message was:"); 
				for (size_t n=0; n < wxMin(ecount, 9); ++n) msg << wxT('\n') << errors[n];
				if (ecount > 9) msg << wxT("\n...");
			}

		wxMessageBox(msg); return;
	}

if (MyFrame::mainframe->GetActivePane())
  MyFrame::mainframe->GetActivePane()->OnOutsideSelect(mount, false);
wxString msg(_("Mounted successfully on ")); BriefLogStatus bls(msg + mount);
}

bool DeviceAndMountManager::MkDir(wxString& mountpt)  // Make a dir onto which to mount
{
if (mountpt.IsEmpty() || mountpt.GetChar(0) != wxFILE_SEP_PATH  // If we're trying to create a dir out of total rubbish, abort
                          || mountpt == wxT("/"))               //   or if we're being asked to create root!
    { wxMessageBox(_("Impossible to create directory ") + mountpt);  return false; }
    
wxString parentdir = mountpt.BeforeLast(wxFILE_SEP_PATH);       // Try to work out what is the longest valid parent dir of the requested one
while (!wxDirExists(parentdir)) 
  { if (parentdir.IsEmpty()) parentdir = wxT("/");              // In case mountpt is eg /mnt
     else parentdir = parentdir.BeforeLast(wxFILE_SEP_PATH);    // In case we're being asked recursively to add several subdirs at a time
  }
                // Finally a valid parent dir! Check if we have write permission to it, as otherwise we'll need to su
FileData parent(parentdir);
if (parent.CanTHISUserWriteExec())
  { if (!wxFileName::Mkdir(mountpt, 0777, wxPATH_MKDIR_FULL))   // Do it the normal way. wxPATH_MKDIR_FULL means intermediate dirs are created too
      { wxMessageBox(_("Oops, failed to create the directory"));  return false; }
  }
 else // NB Most of the following won't work if mountpt contains a space. I've tried all the escape clauses, but none work with mkdir/su combo
  { wxArrayString output, errors;
    long result;
    if (WHICH_SU==mysu)  
      { if (USE_SUDO) result = ExecuteInPty(wxT("sudo sh -c \"mkdir -p ") + EscapeSpaceStr(mountpt) + wxT('\"'), output, errors);  // -p flags make intermediate dirs too
         else         result = ExecuteInPty(wxT("su -c \"mkdir -p ") + mountpt + wxT('\"'), output, errors);
        if (result == CANCEL_RETURN_CODE) return false;
      }
     else if (WHICH_SU==kdesu) result = wxExecute(wxT("kdesu \"sh -c \'mkdir -p ") + mountpt + wxT("\'\""), output, errors);
     else if (WHICH_SU==gksu)  result = wxExecute(wxT("gksu \"sh -c \'mkdir -p ") + mountpt + wxT("\'\""), output, errors);
     else if (WHICH_SU==gnomesu) result = wxExecute(wxT("gnomesu --title "" -c \"sh -c \'mkdir -p ") + mountpt + wxT("\'\" -d"), output, errors);
     else if (WHICH_SU==othersu && !OTHER_SU_COMMAND.IsEmpty())  result = wxExecute(OTHER_SU_COMMAND + wxT("\"sh -c \'mkdir -p ") + mountpt + wxT("\'\""), output, errors);
     else { wxMessageBox(_("Oops, I don't have enough permission to create the directory"));  return false; }

    if (!!result)
			{ wxString msg(_("Oops, failed to create the directory."));
				size_t ecount = errors.GetCount();        							// If it failed, show any error messages
				if (ecount)
					{ msg << _(" The error message was:"); 
						for (size_t n=0; n < wxMin(ecount, 9); ++n) msg << wxT('\n') << errors[n];
						if (ecount > 9) msg << wxT("\n...");
					}

				wxMessageBox(msg); return false;
			}
  }

return true;
}

void DeviceAndMountManager::OnUnMountPartition()
{
FillPartitionArray(false);                                  // Acquire partition & mount data.  False signifies to look in mtab for mounted iso-images too

UnMountPartitionDialog dlg;
wxXmlResource::Get()->LoadDialog(&dlg,MyFrame::mainframe, wxT("UnMountDlg"));
LoadPreviousSize(&dlg, wxT("UnMountDlg"));
dlg.Init(this);
int ans = dlg.ShowModal();
SaveCurrentSize(&dlg, wxT("UnMountDlg"));
if (ans != wxID_OK) return;

wxString partition = dlg.PartitionsCombo->GetValue().BeforeFirst(' '); // BeforeFirst in case of "/dev/sda1  $UUID/$LABEL";
wxString mount = dlg.FstabMountptTxt->GetValue(); 
if (mount.IsEmpty()) return;          
if (mount==wxT("/")) { wxMessageBox(_("Unmounting root is a SERIOUSLY bad idea!"), wxT("Tsk! Tsk!")); return; }

EscapeQuote(mount);                                         // Avoid any problems due to single-quotes in the path

wxArrayString output, errors;

#ifdef __LINUX__
  long result = wxExecute(wxT("umount -l \"") + mount + wxT('\"'),  output, errors);  // We *could* test for kernel >2.4.11, when lazy umounts were invented ;)
  if (result != 0)                                            // If it fails:
    { if (errors.GetCount())
        { if (getuid() != 0 && (WHICH_SU != dunno))           // If there's an error message & we're not already root, try becoming it
            { errors.Clear();
              if (WHICH_SU==mysu)  
                { if (USE_SUDO) result = ExecuteInPty(wxT("sudo sh -c \"umount -l ") + EscapeSpaceStr(mount) + wxT('\"'), output, errors);	// sudo chokes on singlequotes :/
                   else         result = ExecuteInPty(wxT("su -c \"umount -l \'") + mount + wxT("\'\""), output, errors);
                  if (result == CANCEL_RETURN_CODE) return;
                }
               else
                if (WHICH_SU==kdesu)  
                { result = wxExecute(wxT("kdesu \"umount -l \'") + mount + wxT("\'\""), output, errors); }
               else
                if (WHICH_SU==gksu)
                { result = wxExecute(wxT("gksu \"umount -l \'") + mount + wxT("\'\""), output, errors); }
               else
                if (WHICH_SU==gnomesu)
                 { result = wxExecute(wxT("gnomesu --title "" -c \"umount -l \'") + mount + wxT("\'\" -d"), output, errors); }
               else
                if (WHICH_SU==othersu && !OTHER_SU_COMMAND.IsEmpty())
                { result = wxExecute(OTHER_SU_COMMAND + wxT("\"umount -l \'") + mount + wxT("\'\""), output, errors); }

              if (!!result)
                { wxString msg(_("Oops, failed to unmount the partition."));
                  size_t ecount = errors.GetCount();        	// If it failed, show any error messages
                  if (ecount)
                    { msg << _(" The error message was:"); 
                      for (size_t n=0; n < wxMin(ecount, 9); ++n) msg << wxT('\n') << errors[n];
                      if (ecount > 9) msg << wxT("\n...");
                    }
                  wxMessageBox(msg); return;
                }
              
                    // Unfortunately kdesu returns 0 if Cancel pressed, so check the partition really was umounted
              if (ReadMtab(partition) != NULL) { wxString str(_("Failed to unmount ")); BriefLogStatus bls(str + mount); return; }
              
              wxArrayInt IDs;  MyFrame::mainframe->OnUpdateTrees(mount, IDs);      // Update in case this partition was visible in a pane
              BriefLogStatus bls(mount + _(" unmounted successfully"));
            }
           else                                               // If we're already root or we're unable to su, print the error & abort
             { wxString msg; msg.Printf(wxT("%s%lu\n"), _("Oops, failed to unmount successfully due to error "), result); wxMessageBox(msg + errors[0]); }
        }
          else wxMessageBox(_("Oops, failed to unmount"));    // Generic error message (shouldn't happen)
      return;
    }
            // Unfortunately kdesu returns 0 if Cancel pressed, so check the partition really was umounted
  if (ReadMtab(partition) != NULL) { wxString str(_("Failed to unmount ")); BriefLogStatus bls(str + mount); return; }
       
#else // __GNU_HURD__
  // It doesn't seem to be possible to remove just the active translator when there's a passive one too 
  // (perhaps the passive immediately starts another active). So always remove both
  int result = ExecuteInPty(wxT("settrans -fg \"") + mount + wxT('\"'), output, errors);
  if (result != 0) // It currently returns 5 if "Operation not permitted", but let's not rely on that
    result = ExecuteInPty(wxT("su -c \"settrans -fg \'") + mount + wxT("\'\""), output, errors);
  if (result == CANCEL_RETURN_CODE) return;

  if (DeviceAndMountManager::GnuCheckmtab(mount, partition, TT_either))
    { wxString msg; msg.Printf(wxT("%s%lu\n"), _("Oops, failed to unmount successfully due to error "), result); wxMessageBox(msg + errors[0]); return; };
#endif //!#ifdef __LINUX__       

wxArrayInt IDs;  MyFrame::mainframe->OnUpdateTrees(mount, IDs);  // Update in case this partition was visible in a pane
BriefLogStatus bls(mount + _(" unmounted successfully"));
}

void DeviceAndMountManager::OnMountIso()
{
wxString image, mount, options, checkboxes; long result;

MountLoopDialog dlg;
wxXmlResource::Get()->LoadDialog(&dlg,MyFrame::mainframe, wxT("MountLoopDlg"));
LoadPreviousSize(&dlg, wxT("MountLoopDlg"));
if (!dlg.Init()) return;

int ans = dlg.ShowModal();
SaveCurrentSize(&dlg, wxT("MountLoopDlg"));
if (ans != wxID_OK) return;

image = dlg.ImageTxt->GetValue();
mount = dlg.MountptCombo->GetValue();
if (mount.IsEmpty()) return;

FileData mountdir(mount);
if (!mountdir.IsValid())
  { if (wxMessageBox(_("The requested mount-point doesn't exist.  Create it?"), wxEmptyString, wxYES_NO) != wxYES) return;
    if (!MkDir(mount))  return;
  }

options = wxT("-o loop");

EscapeQuote(image); EscapeQuote(mount);                     // Avoid any problems due to single-quotes in a path

wxArrayString output, errors;

#ifdef __LINUX__
  if (dlg.InFstab && (mount == dlg.FstabMt))                // If it has a fstab entry, and we're trying to use it
    { FileData mountdir(mount);
      wxBusyCursor wait;    
      result = wxExecute(wxT("mount \"") + image + wxT("\""),  output, errors);  // Do the mount the easy way
    }

    else
     { if (getuid() != 0)                                   // If user isn't super
        { if (WHICH_SU==mysu)  
            { if (USE_SUDO) result = ExecuteInPty(wxT("sudo sh -c \"mount ") + options + wxT(" \'") + image + wxT("\' \'") + mount + wxT("\'\""), output, errors);
               else         result = ExecuteInPty(wxT("su -c \"mount ") + options + wxT(" \'") + image + wxT("\' \'") + mount + wxT("\'\""), output, errors);
              if (result == CANCEL_RETURN_CODE) return;
            }
           else if (WHICH_SU==kdesu) result = wxExecute(wxT("kdesu \"mount ") + options + wxT(" \'") + image + wxT("\' \'") + mount + wxT("\'\""), output, errors);
           else if (WHICH_SU==gksu)  result = wxExecute(wxT("gksu \"mount ") + options + wxT(" \'") + image + wxT("\' \'") + mount + wxT("\'\""), output, errors);
           else if (WHICH_SU==gnomesu)  result = wxExecute(wxT("gnomesu --title "" -c \"mount ") + options + wxT(" \'") + image + wxT("\' \'") + mount + wxT("\'\" -d"), output, errors);
           else if (WHICH_SU==othersu && !OTHER_SU_COMMAND.IsEmpty())  result = wxExecute(OTHER_SU_COMMAND + wxT("\"mount ") + options + wxT(" \'") + image + wxT("\' \'") + mount + wxT("\'\""), output, errors);
            else { wxMessageBox(_("Sorry, you need to be root to mount non-fstab partitions"), wxT(":-(")); return; }
        }
       else
         result = wxExecute(wxT("mount ") + options + wxT(" \"") + image + wxT("\" \"") + mount + wxT("\""), output, errors);  // If we happen to be root
    }

  switch(result)
    { case 0:  break;                                       // Success
      case 1:  wxMessageBox(_("Oops, failed to mount successfully\nDo you have permission to do this?"));  return; 
      case 16: wxMessageBox(_("Oops, failed to mount successfully\nThere was a problem with /etc/mtab"));  return; 
      case 96: wxMessageBox(_("Oops, failed to mount successfully\nAre you sure the file is a valid Image?"));  return; 
       default:  wxString msg; msg.Printf(wxT("%s%lu"), _("Oops, failed to mount successfully due to error "), result); wxMessageBox(msg);  return; 
    }
#endif

#ifdef __GNU_HURD__
  result = ExecuteInPty(wxT("settrans -a \"") + mount + wxT("\" /hurd/iso9660fs ") + image + wxT('\"'), output, errors);
  if (result != 0) // It currently returns 5 if "Operation not permitted", but let's not rely on that
    result = ExecuteInPty(wxT("su -c \"settrans -a \'") + mount + wxT("\' /hurd/iso9660fs ") + wxT('\'') + image + wxT("\'\""), output, errors); 
  if (result == CANCEL_RETURN_CODE) return;

  if (!DeviceAndMountManager::GnuCheckmtab(mount, image, TT_active))
    { wxString msg;
      if (errors.GetCount() > 0) msg.Printf(wxT("%s%lu\n%s"), _("Failed to mount successfully due to error "), result, errors[0].c_str());
       else  msg.Printf(wxT("%s%lu"), _("Failed to mount successfully"), result);
      wxMessageBox(msg); return; 
    }
#endif

if (MyFrame::mainframe->GetActivePane())
  MyFrame::mainframe->GetActivePane()->OnOutsideSelect(mount, false);
wxString msg(_("Mounted successfully on ")); BriefLogStatus bls(msg + mount);
}

void DeviceAndMountManager::OnMountNfs()
{
MountNFSDialog dlg;
wxXmlResource::Get()->LoadDialog(&dlg,MyFrame::mainframe, wxT("MountNFSDlg"));
if (!dlg.Init()) return;
LoadPreviousSize(&dlg, wxT("MountNFSDlg"));

int ans = dlg.ShowModal();
SaveCurrentSize(&dlg, wxT("MountNFSDlg"));
if (ans != wxID_OK) return;

long result; wxString name, options, mountpt = dlg.MountptCombo->GetValue(); if (mountpt.IsEmpty()) return;
wxArrayString output, errors;

#ifdef __LINUX__
  if (dlg.IsMounted)                                        // If this export is already mounted, abort or mount elsewhere
    { if (mountpt == dlg.AtMountPt)
        { wxMessageBox(_("This export is already mounted at ") + dlg.AtMountPt); return; }
        
      wxString msg; msg.Printf(wxT("%s%s\nDo you want to mount it at %s too?"), _("This export is already mounted at "), dlg.AtMountPt.c_str(), mountpt.c_str());
      wxMessageDialog dialog(MyFrame::mainframe->GetActivePane(), msg, wxT(""), wxYES_NO | wxICON_QUESTION);
      if (dialog.ShowModal() != wxID_YES) return;
    }
#endif

name = dlg.IPCombo->GetValue();                             // Make a 'device' name. Use GetValue() in case there's been a write-in
name += wxT(':');
name += dlg.SharesCombo->GetValue();
 
 #ifdef __LINUX__
  if (dlg.RwRadio->GetSelection())  // First do the options.  Set the rw/ro according to the radiobox
        options =  wxT(" -o ro");
   else options =  wxT(" -o rw");
  if (dlg.MounttypeRadio->GetSelection()) // Now the texture of the mount. We used to look for 'intr' too, but that's now a deprecated no-op
        options << wxT(",soft");
   else options << wxT(",hard");

    if (dlg.InFstab && (mountpt == dlg.FstabMt))                // If it has a fstab entry, and we're trying to use it
      { FileData mountdir(mountpt);
        if (!mountdir.IsValid())
          { if (wxMessageBox(_("The mount-point for this Export doesn't exist.  Create it?"), wxEmptyString, wxYES_NO) != wxYES) return;
            if (!MkDir(mountpt))  return;
          }
        
        wxBusyCursor wait; // Do the mount the easy way. Use mountpt as mount is less picky about presence/absence of a terminal '/' than it is for the export
        result = wxExecute(wxT("mount \"") + mountpt + wxT("\""),  output, errors);
      }
      
     else                                                       // Not in fstab, so we have to work harder
      { wxBusyCursor wait;     
        FileData mountdir(mountpt);
        if (!mountdir.IsValid())
          { if (wxMessageBox(_("The mount-point for this share doesn't exist.  Create it?"), wxEmptyString, wxYES_NO) != wxYES) return;
            if (!MkDir(mountpt))  return;
          }
        
        EscapeQuote(name); EscapeQuote(mountpt);                // Avoid any problems due to single-quotes in a path

        if (getuid() != 0)                                      // If user isn't super
          { if (WHICH_SU==mysu)  
              { if (USE_SUDO) result = ExecuteInPty(wxT("sudo \"mount \'") + name + wxT("\' \'") + mountpt + wxT("\'") + options + wxT("\""), output, errors);
                 else         result = ExecuteInPty(wxT("su -c \"mount \'") + name + wxT("\' \'") + mountpt + wxT("\'") + options + wxT("\""), output, errors);
                if (result == CANCEL_RETURN_CODE) return;
              }
             else if (WHICH_SU==kdesu)  result = wxExecute(wxT("kdesu \"mount \'") + name + wxT("\' \'") + mountpt + wxT("\'") + options + wxT("\""), output, errors);
             else if (WHICH_SU==gksu)  result = wxExecute(wxT("gksu \"mount \'") + name + wxT("\' \'") + mountpt + wxT("\'") + options + wxT("\""), output, errors);
             else if (WHICH_SU==gnomesu)  result = wxExecute(wxT("gnomesu --title "" -c \"mount \'") + name + wxT("\' \'") + mountpt + wxT("\'") + options + wxT("\" -d"), output, errors);
             else if (WHICH_SU==othersu && !OTHER_SU_COMMAND.IsEmpty())  result = wxExecute(OTHER_SU_COMMAND + wxT("\"mount \'") + name + wxT("\' \'") + mountpt + wxT("\'") + options + wxT("\""), output, errors);
                else { wxMessageBox(_("Sorry, you need to be root to mount non-fstab exports"), wxT(":-(")); return; }
          }
         else
          result = wxExecute(wxT("mount \"") + name + wxT("\" \"") + mountpt + wxT("\"") + options, output, errors);  // If we happen to be root
      }

  switch(result)
    { case 0:  break;                                          // Success
      case 1:  wxMessageBox(_("Oops, failed to mount successfully\nDo you have permission to do this?"));  return; 
      case 4:  wxMessageBox(_("Oops, failed to mount successfully\nIs NFS running?"));  return; 
       default:  wxString msg; msg.Printf(wxT("%s%lu") , _("Oops, failed to mount successfully due to error "), result); wxMessageBox(msg);  return; 
    }
#endif

#ifdef __GNU_HURD__
  if (dlg.MounttypeRadio->GetSelection())
        options << wxT(" --soft ");
   else options << wxT(" --hard ");

  // At present, wxExecute will always hang here (and sometimes elsewhere too) so use ExecuteInPty for even for non-su
  result = ExecuteInPty(wxT("settrans -ac \"") + mountpt + wxT("\" /hurd/nfs ") + options + wxT('\"') + name + wxT('\"'), output, errors);
  //if (result != 0) 
  if (!DeviceAndMountManager::GnuCheckmtab(mountpt, name, TT_active))
    { result = ExecuteInPty(wxT("su -c \"settrans -ac \'") + mountpt + wxT("\' /hurd/nfs ") + options + wxT('\'') + name + wxT("\'\""), output, errors);
      if (result == CANCEL_RETURN_CODE) return;
    }

  if (!DeviceAndMountManager::GnuCheckmtab(mountpt, name, TT_active))
    { wxString msg;
      if (errors.GetCount() > 0) msg.Printf(wxT("%s%lu\n%s"), _("Failed to mount successfully due to error "), result, errors[0].c_str());
       else  msg.Printf(wxT("%s%lu"), _("Failed to mount successfully due to error "), result);
      wxMessageBox(msg); return; 
    }
#endif


if(MyFrame::mainframe->GetActivePane())
  MyFrame::mainframe->GetActivePane()->OnOutsideSelect(mountpt, false);
wxString msg(_("Mounted successfully on ")); BriefLogStatus bls(msg + mountpt);
}

void DeviceAndMountManager::OnMountSshfs()
{
MountSshfsDlg dlg;
wxXmlResource::Get()->LoadDialog(&dlg, MyFrame::mainframe, wxT("MountSshfsDlg")); 
if (!dlg.Init()) return;
LoadPreviousSize(&dlg, wxT("MountSshfsDlg"));

int ans = dlg.ShowModal();
SaveCurrentSize(&dlg, wxT("MountSshfsDlg"));
if (ans != wxID_OK) return;


wxString command(wxT("sshfs ")), mountpt = dlg.MountptCombo->GetValue(); if (mountpt.IsEmpty()) return;
mountpt.Trim().Trim(false);

  { FileData mountdir(mountpt); // Check mountdir in this limited scope, in case it's invalid. If so, we'll have to recreate the FileData after
    if (!mountdir.IsValid())
      { if (wxMessageBox(_("The selected mount-point doesn't exist.  Create it?"), wxT("4Pane"), wxYES_NO) != wxYES) return;
        if (!MkDir(mountpt))  return;
      }
  }

wxString options;
if (dlg.Idmap->IsChecked())     options << wxT("-o idmap=user ");
if (dlg.Readonly->IsChecked())  options << wxT("-o ro ");
wxString otheroptions = dlg.OtherOptions->GetValue();

FileData mountdir(mountpt);
if (!mountdir.IsDir()) { wxMessageBox(_("The selected mount-point isn't a directory"), wxT("4Pane")); return; }
wxDir dir(mountpt); if (!dir.IsOpened()) { wxMessageBox(_("The selected mount-point can't be accessed"), wxT("4Pane")); return; }
if ((dir.HasFiles() || dir.HasSubDirs()) && !otheroptions.Contains(wxT("nonempty")))
  { if (wxMessageBox(_("The selected mount-point is not empty.  Try to mount there anyway?"), wxT("4Pane"), wxYES_NO) != wxYES) return;
    options << wxT("-o nonempty ");
  }
if (!otheroptions.empty()) options << otheroptions << wxT(' ');

wxString user = dlg.UserCombo->GetValue(); user.Trim().Trim(false);
if (!user.empty()) user << wxT('@');                        // Supplying a remote user isn't compulsary, but if we do, @ is the separator

wxString host = dlg.HostnameCombo->GetValue(); host.Trim().Trim(false); host << wxT(':');

wxString remotemt = dlg.RemotedirCombo->GetValue(); remotemt.Trim().Trim(false);
remotemt = StrWithSep(remotemt);  // Any remote dir must be '/' terminated, otherwise mounting fails :/
remotemt << wxT(' ');

command << options << user << host << remotemt << wxT("\"") << mountpt << wxT("\"");
#if wxVERSION_NUMBER < 3000
  // For some reason, in wx2.8 using the usual sync execution with output,errors works, but hangs; and async doesn't give a useful exit code;
  // and we can't check for success by looking in mtab as it won't have mounted yet (sshfs takes a few seconds).
  // Trying use a wxProcess to output success/failure also fails, as the process hangs until the unmount occurs!
  // We also have the insuperable problem of what will ssh do: a password-less public key login; a password login with a valid gui setting for SSH_ASKPASS;
  // or will it ask for a password using stdin/out. If the latter, not providing one in a wxProcess will hang 4Pane!
  // and (for some reason) though using ExecuteInPty() 'works', the login itself fails silently (even with LogLevel=DEBUG3).
  // So, for now, just run the command and hope ssh can cope. Do the display update by polling (not with findmnt --poll, which is too recent).
wxExecute(command, wxEXEC_ASYNC | wxEXEC_MAKE_GROUP_LEADER);
m_MtabpollTimer->Begin(mountpt, 250, 120);                   // Poll every 1/4sec for up to 30sec

#else
wxArrayString output, errors;
long result = wxExecute(command, output, errors);

if (!result)                                                // Success
  { if (MyFrame::mainframe->GetActivePane())
    MyFrame::mainframe->GetActivePane()->OnOutsideSelect(mountpt, false);
    wxString msg(_("Mounted successfully on ")); BriefLogStatus bls(msg + mountpt);
    return;
  }

wxString msg; size_t ecount = errors.GetCount();            // If it failed, show any error messages or, failing that, the exit code
if (ecount)
  { msg = _("Failed to mount over ssh. The error message was:"); 
    for (size_t n=0; n < ecount; ++n) msg << wxT('\n') << errors[n];
  }
 else msg.Printf(wxT("%s%i"), _("Failed to mount over ssh due to error "), (int)result); 
wxMessageBox(msg);
#endif
}

#if wxVERSION_NUMBER < 3000
void MtabPollTimer::Notify()
{
static int hitcount = 0;
if (m_devman && (m_devman->Checkmtab(m_Mountpt1) || m_devman->Checkmtab(m_Mountpt2))) // One of these has a terminal '/'
  { // When, in a non-gui situation, there's no available password so the mount is destined to fail, somehow the mountpoint gets briefly added to mtab
    // It's then immediately removed, but by then we've hit it, resulting in an error message when we try to stat it. To be safe, ignore the first 2 hits
    if (hitcount++ == 2)
      { Stop(); hitcount = 0;
        if (MyFrame::mainframe->GetActivePane())  MyFrame::mainframe->GetActivePane()->OnOutsideSelect(m_Mountpt1, false);
        BriefLogStatus bls(_("Mounted successfully on ") + m_Mountpt1);
        return;
      }
  }

if (--remaining <= 0)
  { Stop(); hitcount = 0;
    BriefLogStatus bls(wxString::Format(_("Failed to mount over ssh on %s"), m_Mountpt1.c_str()));
  }
}
#endif

void DeviceAndMountManager::OnMountSamba()
{
MountSambaDialog dlg;
wxXmlResource::Get()->LoadDialog(&dlg,MyFrame::mainframe, wxT("MountSambaDlg"));
if (!dlg.Init()) return;
LoadPreviousSize(&dlg, wxT("MountSambaDlg"));

int ans = dlg.ShowModal();
SaveCurrentSize(&dlg, wxT("MountSambaDlg"));
if (ans != wxID_OK) return;


long result = -1; wxString name, login, mountpt = dlg.MountptCombo->GetValue(); if (mountpt.IsEmpty()) return;
wxArrayString output, errors;

if (dlg.IsMounted)                                          // If this share is already mounted, abort or mount elsewhere
  { if (mountpt == dlg.AtMountPt)
      { wxMessageBox(_("This share is already mounted at ") + dlg.AtMountPt); return; }
      
    wxString msg; msg.Printf(_("This share is already mounted at %s\nDo you want to mount it at %s too?"), dlg.AtMountPt.c_str(), mountpt.c_str());
    wxMessageDialog dialog(MyFrame::mainframe->GetActivePane(), msg, wxT("4Pane"), wxYES_NO | wxICON_QUESTION);
    if (dialog.ShowModal() != wxID_YES) return;
  }

if (dlg.RwRadio->GetSelection())                            // First do the options.  Set the rw/ro according to the radiobox
       login =  wxT("-o ro");
  else login =  wxT("-o rw");
if (!((wxRadioBox*)dlg.FindWindow(wxT("PwRadio")))->GetSelection())  // Now the username/password
  { wxString user = dlg.Username->GetValue(); if (!user.IsEmpty()) user = wxT(",user=") + user;  // No username means smb defaults to the linux user
    wxString pw = dlg.Password->GetValue(); if (!pw.IsEmpty()) pw = wxT(",pass=") + pw;
    if (pw.IsEmpty()) login += wxT(",guest");               // We can't cope without a password, as smb would prompt for one.  So try for guest
      else { login += user; login += pw; }                  // Add the username (if any) & pw
  }
 else login += wxT(",guest");                               // If the radiobox selection was for anon
 login += wxT(" ");

  //  Make a 'device' name. I used to use the hostname in preference to the ip address, but the latter seems more reliable
name = dlg.IPCombo->GetValue();
if (name.IsEmpty()) name = dlg.HostnameCombo->GetValue();   // If there isn't a valid ip address, use the hostname
name = wxT("//") + name; name += wxT('/');
name += dlg.SharesCombo->GetValue();
 
if (dlg.InFstab && (mountpt == dlg.FstabMt))                // If it has a fstab entry, and we're trying to use it
  { FileData mountdir(mountpt);
    if (!mountdir.IsValid())
      { if (wxMessageBox(_("The mount-point for this share doesn't exist.  Create it?"), wxEmptyString, wxYES_NO) != wxYES) return;
        if (!MkDir(mountpt))  return;
      }
    
    wxBusyCursor wait;    
    result = wxExecute(wxT("mount \"") + name + wxT('\"'), output, errors);  // Do the mount the easy way
    if (!result) result = errors.GetCount();    // Unfortunately, wxExecute returns 0 even if submnt fails.  Success gives no error msg, so use this instead
  }
  
if (!!result)  // Not in fstab, or it is but it failed (probably as recent smbmounts aren't setuid); so we have to work harder
  { wxBusyCursor wait;     
    FileData mountdir(mountpt);
    if (!mountdir.IsValid())
      { if (wxMessageBox(_("The mount-point for this share doesn't exist.  Create it?"), wxEmptyString, wxYES_NO) != wxYES) return;
        if (!MkDir(mountpt))  return;
      }
    
    EscapeQuote(name); EscapeQuote(mountpt);                // Avoid any problems due to single-quotes in a path
    
    if (WeCanUseSmbmnt(mountpt))                            // We can only use smbmnt if we're root, or if it's setuid root & we own mountpt
      { wxString smbmount_filepath;
        if (Configure::TestExistence(wxT("smbmount"), true))  smbmount_filepath = wxT("smbmount");  // See if it's in PATH
          else smbmount_filepath = SMBPATH+wxT("smbmount \""); // Otherwise use the presumed samba dir
        result = wxExecute(smbmount_filepath + name + wxT("\" \"") + mountpt + wxT("\" ") + login, output, errors);
      }
     else  
      { if (getuid() != 0)                                  // If user isn't super
          { if (WHICH_SU==mysu)  
              { if (USE_SUDO) result = ExecuteInPty(wxT("sudo sh -c \"mount -t cifs ") + login + wxT("\'") + name + wxT("\' \'") + mountpt + wxT("\'\""), output, errors);
                 else         result = ExecuteInPty(wxT("su -c \"mount -t cifs ") + login + wxT("\'") + name + wxT("\' \'") + mountpt + wxT("\'\""), output, errors);
                if (result == CANCEL_RETURN_CODE) return;
              }
             else if (WHICH_SU==kdesu)  result = wxExecute(wxT("kdesu \"mount -t cifs ") + login + wxT("\'") + name + wxT("\' \'") + mountpt + wxT("\'\""), output, errors);
             else if (WHICH_SU==gksu)  result = wxExecute(wxT("gksu \"mount -t cifs ") + login + wxT("\'") + name + wxT("\' \'") + mountpt + wxT("\'\""), output, errors);
             else if (WHICH_SU==gnomesu)  result = wxExecute(wxT("gnomesu --title "" -c \"mount -t cifs ") + login + wxT("\'") + name + wxT("\' \'") + mountpt + wxT("\'\" -d"), output, errors);
             else if (WHICH_SU==othersu && !OTHER_SU_COMMAND.IsEmpty())  result = wxExecute(OTHER_SU_COMMAND + wxT("\"mount -t cifs ") + login + wxT("\'") + name + wxT("\' \'") + mountpt + wxT("\'\""), output, errors);
                else { wxMessageBox(_("Sorry, you need to be root to mount non-fstab shares"), wxT(":-(")); return; }
          }
         else
          result = wxExecute(wxT("mount -t cifs ") + login + wxT("\"") + name + wxT("\" \"") + mountpt + wxT("\""), output, errors);  // If we happen to be root
      }
  }

if (!result)                                                // Success
  { if (MyFrame::mainframe->GetActivePane())
    MyFrame::mainframe->GetActivePane()->OnOutsideSelect(mountpt, false);
    wxString msg(_("Mounted successfully on ")); BriefLogStatus bls(msg + mountpt);
    return;
  }

wxString msg; size_t ecount = errors.GetCount();            // If it failed, show any error messages or, failing that, the exit code
if (ecount)
  { msg = _("Oops, failed to mount successfully. The error message was:"); 
    for (size_t n=0; n < ecount; ++n) msg << wxT('\n') << errors[n];
  }
 else msg.Printf(wxT("%s%i"), _("Oops, failed to mount successfully due to error "), (int)result); 
wxMessageBox(msg);
}

bool DeviceAndMountManager::WeCanUseSmbmnt(wxString& mountpt, bool TestUmount /*=false*/)  // Do we have enough kudos to (u)mount with smb
{
wxString smbfilepath = SMBPATH + (TestUmount ? wxT("smbumount") : wxT("smbmnt"));  // Make a filepath to whichever file we're interested in
FileData smb(smbfilepath);
if (!smb.IsValid()) return false;                           // If it's not in SMBPATH (or doesn't exist anyway e.g. ubuntu), forget it

size_t ourID = getuid();
if (ourID == 0)  return true;                               // If we're root anyway we certainly can use it

FileData mnt(mountpt); if (!mnt.IsValid()) return false;    // This is to test the mountpt dir's details.  If it doesn't exist, return

if (smb.IsSetuid())                                         // If the file is setuid, we can use it
  if (TestUmount ||                                         //    if we're UNmounting
      (mnt.OwnerID()==ourID && mnt.IsUserWriteable()))      //       or provided we own and have write permission for the target dir
                return true;
return false;
}

void DeviceAndMountManager::OnUnMountNetwork()
{
UnMountSambaDialog dlg;
wxXmlResource::Get()->LoadDialog(&dlg,MyFrame::mainframe, wxT("UnMountDlg"));
if (!dlg.Init())
  { wxMessageBox(_("No Mounts of this type found")); return; }  // Init returns the no located.  Bug out if none


LoadPreviousSize(&dlg, wxT("UnMountDlg"));
dlg.SetTitle(_("Unmount a Network mount"));
((wxStaticText*)dlg.FindWindow(wxT("FirstStatic")))->SetLabel(_("Share or Export to Unmount"));
((wxStaticText*)dlg.FindWindow(wxT("SecondStatic")))->SetLabel(_("Mount-Point"));

int ans = dlg.ShowModal();
SaveCurrentSize(&dlg, wxT("UnMountDlg"));
if (ans != wxID_OK) return;

wxString mount = dlg.FstabMountptTxt->GetValue(); 
if (mount.IsEmpty()) return;          
if (mount==wxT("/")) { wxMessageBox(_("Unmounting root is a SERIOUSLY bad idea!"), wxT("Tsk! Tsk!")); return; }

wxArrayString output, errors;  long result = -1;

if (dlg.IsSshfs())   // sshfs does things in userspace, so try its unmounter first
  { if (!FUSERMOUNT.empty() && (wxExecute(FUSERMOUNT + wxT(" -u ") + mount,  output, errors) == 0))
      { wxArrayInt IDs;  MyFrame::mainframe->OnUpdateTrees(mount, IDs);
        BriefLogStatus bls(mount + _(" unmounted successfully"));
        return;
      }
  } // Otherwise fall through to use the standard umount

if (dlg.IsSamba() && WeCanUseSmbmnt(mount, true))           // If this is a samba share, & we're set up to do so, use smbumount
  { wxString smbmount_filepath;
    if (Configure::TestExistence(wxT("smbumount"), true))  smbmount_filepath = wxT("smbumount");  // See if it's in PATH
      else smbmount_filepath = SMBPATH+wxT("smbumount ");
    result = wxExecute(smbmount_filepath + mount, output, errors);
    if (!result)
      { wxArrayInt IDs;  MyFrame::mainframe->OnUpdateTrees(mount, IDs);
        BriefLogStatus bls(mount + _(" Unmounted successfully")); return;
      }
     else errors.Clear();
  }  
                                                            // Otherwise we have to do it the harder way
if ((result = wxExecute(wxT("umount -l ") + mount,  output, errors)) != 0)
  { wxString mount1;                                        // If it failed, try again with(out) a terminal '/'
    if (mount.Right(1) == wxFILE_SEP_PATH) mount1 = mount.BeforeLast(wxFILE_SEP_PATH);
     else mount1 << mount << wxFILE_SEP_PATH;
    result = wxExecute(wxT("umount -l ") + mount1,  output, errors);
  }
if (result != 0)
  { if (WHICH_SU != dunno)
      { errors.Clear();                                     // Didn't work using fstab. Try again as root
        if (WHICH_SU==mysu)  
            { if (USE_SUDO) result = ExecuteInPty(wxT("sudo sh -c \"umount -l ") + mount + wxT('\"'), output, errors);
               else         result = ExecuteInPty(wxT("su -c \'umount -l ") + mount + wxT("\'"), output, errors);
              if (result == CANCEL_RETURN_CODE) return;
            }
         else if (WHICH_SU==kdesu)  result = wxExecute(wxT("kdesu \'umount -l ") + mount + wxT("\'"), output, errors);
         else if (WHICH_SU==gksu) result = wxExecute(wxT("gksu \'umount -l ") + mount + wxT("\'"), output, errors);
         else if (WHICH_SU==gnomesu) result = wxExecute(wxT("gnomesu --title "" -c \'umount -l ") + mount + wxT("\' -d"), output, errors);
         else if (WHICH_SU==othersu && !OTHER_SU_COMMAND.IsEmpty()) result = wxExecute(OTHER_SU_COMMAND + wxT("\'umount -l ") + mount + wxT("\'"), output, errors);
      }

    if (!!result)                                           // If nothing's worked, abort after displaying what we did get
      { wxString msg; size_t ecount = errors.GetCount();
        if (ecount)
          { msg = _("Unmount failed with the message:"); 
            for (size_t n=0; n < ecount; ++n) msg << wxT('\n') << errors[n];
          }
          else msg = (_("Oops, failed to unmount")); // Generic error message (shouldn't happen)
        wxMessageBox(msg); return;
      }
  }

if (!!result) return;                                       // If we missed any failures above, catch them here

wxArrayInt IDs;  MyFrame::mainframe->OnUpdateTrees(mount, IDs);  // Update in case this partition was visible in a pane
BriefLogStatus bls(mount + _(" unmounted successfully"));
}

bool MountPartitionDialog::Init(DeviceAndMountManager* dad, wxString device /*=wxEmptyString*/)
{
parent = dad;
PartitionsCombo = (wxComboBox*)FindWindow(wxT("PartitionsCombo"));
FstabMountptTxt = NULL;                                     // Start with this null, as we use this to test which dialog version we're in
FstabMountptTxt = (wxTextCtrl*)FindWindow(wxT("FstabMountptTxt"));// Find the textctrl if this is the fstab version of the dialog
MountptCombo = (wxComboBox*)FindWindow(wxT("MountptCombo"));      // Ditto for the 'Other' version
if (!FstabMountptTxt && !MountptCombo) return false; // These are new in 4.0, so protect against a stale rc/

historypath = wxT("/History/MountPartition/");
LoadMountptHistory();

if (device != wxEmptyString)                                // This is if we're trying to mount a DVD-RAM disc
  { wxArrayString answerarray;
    PartitionsCombo->Append(device.c_str());
    if (MountptCombo && parent->FindUnmountedFstabEntry(device, answerarray))  // Try to find a suggested mountpt from ftab
      { if (answerarray.GetCount())   // Array holds any unused mountpoints. Just show the first (there'll probably be 0 or 1 anyway)
          MountptCombo->SetValue(answerarray.Item(0));
      }
    return true;
  }                    

for (size_t n=0; n < parent->PartitionArray->GetCount(); ++n)
  { PartitionStruct* pstruct = parent->PartitionArray->Item(n);
    if (FstabMountptTxt == NULL)                            // If this is the 'Other' version of the dialog, load every known unmounted partition
      { if (!pstruct->IsMounted)
          { wxString devicestr(pstruct->device);
            if (!pstruct->label.empty()) devicestr << " " << pstruct->label;
            PartitionsCombo->Append(devicestr);
          }
      }
     else
      { if (pstruct->IsMounted || pstruct->mountpt.IsEmpty()) continue;  // Ignore already-mounted partitions, or ones not in fstab (so no known mountpt)
        wxString devicestr(pstruct->device);
        if (!pstruct->label.empty()) devicestr << " " << pstruct->label;
         else if (!pstruct->uuid.empty())
          { wxString UUID(pstruct->uuid);
            if (UUID.Len() > 16)
              UUID = pstruct->uuid.BeforeFirst('-') + "..." + pstruct->uuid.AfterLast('-'); // Truncate display of standard UUIDs
            devicestr << " " << UUID;
          }
        PartitionsCombo->Append(devicestr);   // This one qualifies, so add it
      }
  }
#if (defined(__WXGTK__) || defined(__WXX11__))
if (PartitionsCombo->GetCount())     PartitionsCombo->SetSelection(0);
#endif
DisplayMountptForPartition();                               // Show ftab's suggested mountpt, if we're in the correct dialog
return true;
}

void MountPartitionDialog::DisplayMountptForPartition(bool GetDataFromMtab /*=false*/)  // This enters the correct mountpt for the selected partition, in the fstab version of the dialog
{
if (FstabMountptTxt == NULL) return;                        // because in the 'Other' version of the dialog, the user enters his own mountpt

wxString dev = PartitionsCombo->GetValue();                 // We need to use GetValue(), not GetStringSelection(), as in >2.6.3 the latter fails
for (size_t n=0; n < parent->PartitionArray->GetCount(); ++n)
  if (parent->PartitionArray->Item(n)->device == dev.BeforeFirst(' ')) // BeforeFirst in case of "/dev/sda1  $UUID/$LABEL"
    { if (GetDataFromMtab)    // If we're unmounting, we can't rely on the PartitionArray info:  the partition may not have been mounted where fstab intended
        { FstabMountptTxt->Clear();
#ifdef __linux__
          struct mntent* mnt = parent->ReadMtab(dev.BeforeFirst(' ')); // So see where it really is
          if (mnt != NULL)   FstabMountptTxt->ChangeValue(wxString(mnt->mnt_dir, wxConvUTF8));
#else
          struct statfs* mnt = parent->ReadMtab(dev.BeforeFirst(' '));
          if (mnt != NULL) FstabMountptTxt->ChangeValue(wxString(mnt->f_mntonname, wxConvUTF8));
#endif
          return;
        }
       else
        { FstabMountptTxt->Clear(); FstabMountptTxt->ChangeValue(parent->PartitionArray->Item(n)->mountpt); return; }
    }  
}

void MountPartitionDialog::OnBrowseButton(wxCommandEvent& WXUNUSED(event))  //  The Browse button was clicked, so let user locate a mount-pt dir
{
wxString startdir; if (wxDirExists(wxT("/mnt"))) startdir = wxT("/mnt");
wxDirDialog ddlg(this, _("Choose a Directory to use as a Mount-point"), startdir, wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
if (ddlg.ShowModal() != wxID_OK) return;    

wxString filepath = ddlg.GetPath();
if (!filepath.IsEmpty()) MountptCombo->SetValue(filepath);
}

void MountPartitionDialog::LoadMountptHistory() const
{
if (!MountptCombo || historypath.empty()) return;

size_t count = (size_t)wxConfigBase::Get()->Read(historypath + wxT("itemcount"), 0l);
for (size_t n=0; n < count; ++n)
  { wxString Mountpt = wxConfigBase::Get()->Read(historypath + wxString::Format(wxT("%c/"), wxT('a') + (wxChar)n) + wxT("Mountpt"), wxString());
    if (!Mountpt.empty())
      MountptCombo->Append(Mountpt);
  }
}

void MountPartitionDialog::SaveMountptHistory() const
{
if (!MountptCombo || historypath.empty()) return;
wxString mp = MountptCombo->GetValue(); if (mp.empty()) return;

wxArrayString History = MountptCombo->GetStrings();
int index = History.Index(mp);
if (index != wxNOT_FOUND) History.RemoveAt(index); // Avoid duplication
History.Insert(mp, 0); // Prepend the current entry

wxConfigBase::Get()->DeleteGroup(historypath);
size_t count = wxMin(History.GetCount(), MAX_COMMAND_HISTORY);
wxConfigBase::Get()->Write(historypath + wxT("itemcount"), (long)count);
for (size_t n=0;  n < count; ++n)
  wxConfigBase::Get()->Write(historypath + wxString::Format(wxT("%c/"), wxT('a') + (wxChar)n) + wxT("Mountpt"), History[n]);
}

BEGIN_EVENT_TABLE(MountPartitionDialog,wxDialog)
  EVT_COMBOBOX(XRCID("PartitionsCombo"), MountPartitionDialog::OnSelectionChanged)
  EVT_BUTTON(XRCID("Browse"), MountPartitionDialog::OnBrowseButton)
  EVT_BUTTON(-1, MountPartitionDialog::OnButtonPressed)
END_EVENT_TABLE()

bool MountLoopDialog::Init()
{
ImageTxt = (wxTextCtrl*)FindWindow(wxT("ImageTxt"));
MountptCombo = (wxComboBox*)FindWindow(wxT("MountptCombo"));
if (!MountptCombo) return false; // This is new in 4.0, so protect against a stale rc/
AlreadyMounted = (wxStaticText*)FindWindow(wxT("AlreadyMounted")); AlreadyMounted->Hide();

historypath = wxT("/History/MountLoop/");
LoadMountptHistory();

wxString possibleIso;
if (MyFrame::mainframe->GetActivePane()) 
  possibleIso = MyFrame::mainframe->GetActivePane()->GetPath();
FileData poss(possibleIso); if (!poss.IsValid()) return true;
FileData tgt(poss.GetUltimateDestination());
if (tgt.IsRegularFile())
  { ImageTxt->ChangeValue(possibleIso);
    MountptCombo->SetFocus();
  }
 else ImageTxt->SetFocus();

return true;
}

void MountLoopDialog::OnIsoBrowseButton(wxCommandEvent& WXUNUSED(event))
{
MyGenericDirCtrl* active = MyFrame::mainframe->GetActivePane();   // Find the active dirview's path to use as startdir for dialog
if (active->fileview==ISRIGHT) active = active->partner;

wxFileDialog fdlg(this,_("Choose an Image to Mount"), active->GetPath(), wxEmptyString, wxT("*"), wxFD_OPEN);
if (fdlg.ShowModal()  != wxID_OK) return;    

wxString filepath = fdlg.GetPath();                               // Get the selection
if (!filepath.IsEmpty())   ImageTxt->SetValue(filepath);          // Paste into the textctrl
}

void MountLoopDialog::DisplayMountptForImage()  // This enters the correct mountpt for the selected Image if it's in fstab
{
wxString Image = ImageTxt->GetValue();
if (Image.IsEmpty())                                          // If no currently selected share,  abort
  { InFstab = IsMounted = false; FstabMt.Empty(); AtMountPt.Empty();
    AlreadyMounted->Hide(); return;
  }

struct fstab* fs = DeviceAndMountManager::ReadFstab(Image);
InFstab = (fs != NULL);                                       // Store or null the data according to the result
FstabMt = (InFstab ? wxString(fs->fs_file, wxConvUTF8) : wxT(""));

#ifdef __linux__
struct mntent* mnt = DeviceAndMountManager::ReadMtab(Image);  // Now read mtab to see if the share's already mounted
#else
struct statfs* mnt = DeviceAndMountManager::ReadMtab(Image);
#endif
IsMounted = (mnt != NULL);
AlreadyMounted->Show(IsMounted); GetSizer()->Layout();        // If it is mounted, expose the wxStaticTxt that says so (and Layout, else 2.8.0 displays it in top left corner!)
#ifdef __linux__
AtMountPt = (IsMounted ? wxString(mnt->mnt_dir, wxConvUTF8) : wxT(""));  // Store any mountpt, or delete any previous entry
#else
AtMountPt = (IsMounted ? wxString(mnt->f_mntonname, wxConvUTF8) : wxT(""));
#endif
if (IsMounted)
    MountptCombo->SetValue(AtMountPt);                        // Put any mountpt in the combobox
  else if (InFstab)
    MountptCombo->SetValue(FstabMt);                          // If not, but there was an fstab entry, insert that instead

}

BEGIN_EVENT_TABLE(MountLoopDialog,MountPartitionDialog)
  EVT_BUTTON(XRCID("Browse"), MountLoopDialog::OnBrowseButton)
  EVT_BUTTON(XRCID("IsoBrowse"), MountLoopDialog::OnIsoBrowseButton)
  EVT_TEXT(XRCID("ImageTxt"), MountLoopDialog::OnSelectionChanged)
END_EVENT_TABLE()

bool MountSshfsDlg::Init()
{
UserCombo = (wxComboBox*)FindWindow(wxT("UserCombo"));
HostnameCombo = (wxComboBox*)FindWindow(wxT("HostnameCombo"));
RemotedirCombo = (wxComboBox*)FindWindow(wxT("RemotedirCombo"));
MountptCombo = (wxComboBox*)FindWindow(wxT("MountptCombo"));
if (!MountptCombo) return false; // This is new in 4.0, so protect against a stale rc/
Idmap = (wxCheckBox*)FindWindow(wxT("idmap"));
Readonly = (wxCheckBox*)FindWindow(wxT("readonly"));
OtherOptions = (wxTextCtrl*)FindWindow(wxT("OtherOptions"));
if (!(UserCombo && HostnameCombo && RemotedirCombo && MountptCombo && OtherOptions)) return false;

historypath = wxT("/History/MountSshfs/");
LoadMountptHistory();

wxConfigBase* config = wxConfigBase::Get();
config->SetPath(wxT("/History/SshfsHistory/"));
size_t count = (size_t)config->Read(wxT("itemcount"), 0l);
SshfsHistory.Clear();
wxString key; const wxString dfault;
for (size_t n=0; n < count; ++n)
  { key.Printf(wxT("%c/"), wxT('a') + (wxChar)n);
    wxString Hostname = config->Read(key + wxT("Hostname"), dfault); if (Hostname.IsEmpty()) continue;
    wxString User = config->Read(key + wxT("User"), dfault);
    wxString Remotedir = config->Read(key + wxT("Remotedir"), dfault);
    wxString Mountpt = config->Read(key + wxT("Mountpt"), dfault);
    bool map; config->Read(key + wxT("idmap"), &map, true);
    bool ro;  config->Read(key + wxT("readonly"), &ro, true);
    wxString OtherOpts = config->Read(key + wxT("OtherOptions"), dfault);

    SshfsHistory.Add( new SshfsNetworkStruct(User, Hostname, Remotedir, Mountpt, map, ro, OtherOpts) );
  }
config->SetPath(wxT("/"));

  // Now see if there's anything extra in /etc/fstab
ArrayofPartitionStructs PartitionArray; wxString types(wxT("fuse,fuse.sshfs"));
size_t fcount = MyFrame::mainframe->Layout->m_notebook->DeviceMan->DeviceAndMountManager::ReadFstab(PartitionArray, types);
for (size_t n=0; n < fcount; ++n)
  { bool idmap(false), ro(false);
    wxString source(PartitionArray.Item(n)->device.AfterLast(wxT('#')));  // There are 2 fstab entry methods. One prepends 'sshfs#'
    wxString user =      source.BeforeLast(wxT('@'));     // Use BeforeLast, not First, as it returns empty if not found
    wxString hostname =  source.AfterLast(wxT('@')).BeforeFirst(wxT(':')); // We _want_ the whole string if there's no ':'
    wxString remotedir = source.AfterFirst(wxT(':'));     // Use AfterFirst as we want "" if not found
    wxString options;                                     // Now parse any options
    wxArrayString optsarray = wxStringTokenize(PartitionArray.Item(n)->uuid, // They were put here, for want of a better field
                                                    wxT(","),wxTOKEN_STRTOK);
    for (size_t op=0; op < optsarray.GetCount(); ++op)
      { wxString opt = optsarray.Item(op).Trim().Trim(false);
        size_t pos = opt.find(wxT("idmap="));             // Filter out idmap=user and ro, as we have separate boxes for them
        if ((pos != wxString::npos) && (opt.find(wxT("user"), pos+6, 4) != wxString::npos))
          { idmap = true; continue; }
        if ((pos = opt.find(wxT("ro"))) != wxString::npos)
          { ro = true; continue; }
        if (!options.empty() && !opt.empty()) options << wxT(' ');
        options << wxT("-o ") << opt;
      }

    SshfsNetworkStruct* sstruct = new SshfsNetworkStruct(user, hostname, remotedir, PartitionArray.Item(n)->mountpt, idmap, ro, options);

    bool found(false);
    for (size_t i=0; i < SshfsHistory.GetCount(); ++i)
      if (SshfsHistory.Item(i)->IsEqualTo(sstruct))
        { found = true; break; }
      
    if (!found) SshfsHistory.Add(sstruct);
     else delete sstruct;
  }

  // Load all the hostnames, uniqued
for (size_t n=0; n < SshfsHistory.GetCount() && n < MAX_COMMAND_HISTORY; ++n)
  { SshfsNetworkStruct* sstruct = SshfsHistory.Item(n);
    if (HostnameCombo->FindString(sstruct->hostname) == wxNOT_FOUND)
      HostnameCombo->Append(sstruct->hostname); 
  }

Connect(wxEVT_COMMAND_COMBOBOX_SELECTED, wxCommandEventHandler(MountSshfsDlg::OnSelectionChanged), NULL, this); // We must connect them all, otherwise the baseclass catches some and crashes
Connect(wxID_OK, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MountSshfsDlg::OnOK), NULL, this);
Connect(wxID_OK, wxEVT_UPDATE_UI, wxUpdateUIEventHandler(MountSshfsDlg::OnOKUpdateUI), NULL, this);

if (SshfsHistory.GetCount() > 0)
  { HostnameCombo->SetSelection(0);
    // Now fake a selection event to get the index==0 host's user/remotedir data loaded
    wxCommandEvent event(wxEVT_COMMAND_COMBOBOX_SELECTED, XRCID("HostnameCombo"));
    GetEventHandler()->ProcessEvent(event);
  }

return true;
}

void MountSshfsDlg::OnSelectionChanged(wxCommandEvent& event)
{
if (event.GetId() != XRCID("HostnameCombo")) return; // We only care about this id, but have to catch them all here to prevent MountPartitionDialog catching and crashing
int index = HostnameCombo->GetSelection(); if (index == wxNOT_FOUND) return;

  // Fill the user/remotedir combos, but only with the data previously used for this hostname
UserCombo->Clear(); RemotedirCombo->Clear();
wxString hostname = HostnameCombo->GetStringSelection(); int firstfound = -1;
for (size_t n=0; n < SshfsHistory.GetCount() && n < MAX_COMMAND_HISTORY; ++n)
  { SshfsNetworkStruct* sstruct = SshfsHistory.Item(n);
    if (sstruct->hostname == hostname)
      { if (UserCombo->FindString(sstruct->user) == wxNOT_FOUND)
          UserCombo->Append(sstruct->user);
        if (RemotedirCombo->FindString(sstruct->remotedir) == wxNOT_FOUND)
          RemotedirCombo->Append(sstruct->remotedir);

        if (firstfound == -1) // We need to cache the first-found index-item so we can use it to get the correct data below
          firstfound = n;
      }
  }

if (UserCombo->GetCount() > 0) UserCombo->SetSelection(0);
if (RemotedirCombo->GetCount() > 0) RemotedirCombo->SetSelection(0);

if (!SshfsHistory[index]->localmtpt.empty())  // If there's data for this item, overwrite the current textctrl's contents. Otherwise don't clobber with ""
  MountptCombo->SetValue(SshfsHistory[firstfound]->localmtpt);
Idmap->SetValue(SshfsHistory[firstfound]->idmap); Readonly->SetValue(SshfsHistory[firstfound]->readonly); OtherOptions->ChangeValue(SshfsHistory[firstfound]->otheroptions);
}

void MountSshfsDlg::OnOKUpdateUI(wxUpdateUIEvent& event)
{
event.Enable(!HostnameCombo->GetValue().empty() && !MountptCombo->GetValue().empty());
}

void MountSshfsDlg::OnOK(wxCommandEvent& event)
{
SshfsNetworkStruct* newitem = new SshfsNetworkStruct(UserCombo->GetValue(), HostnameCombo->GetValue(), RemotedirCombo->GetValue(),
                                                    MountptCombo->GetValue(), Idmap->IsChecked(), Readonly->IsChecked(), OtherOptions->GetValue());
for (size_t n=0; n < SshfsHistory.GetCount(); ++n)
  if (SshfsHistory[n]->IsEqualTo(newitem))
    { SshfsHistory.RemoveAt(n); break; }  // Remove any identical item
SshfsHistory.Insert(newitem, 0);          // Either way, prepend the new item

wxConfigBase* config = wxConfigBase::Get();
config->DeleteGroup(wxT("/History/SshfsHistory")); 
config->SetPath(wxT("/History/SshfsHistory/"));
long count = wxMin(SshfsHistory.GetCount(), 26);
config->Write(wxT("itemcount"), count);

wxString key;
for (int n=0; n < count; ++n)
  { key.Printf(wxT("%c/"), wxT('a') + (wxChar)n);
    SshfsNetworkStruct* history = SshfsHistory[n];
    config->Write(key + wxT("User"), history->user);
    config->Write(key + wxT("Hostname"), history->hostname);
    config->Write(key + wxT("Remotedir"), history->remotedir);
    config->Write(key + wxT("Mountpt"), history->localmtpt);
    config->Write(key + wxT("idmap"), history->idmap);
    config->Write(key + wxT("readonly"), history->readonly);
    config->Write(key + wxT("OtherOptions"), history->otheroptions);
  }
config->SetPath(wxT("/"));

event.Skip(); // Needed to save the 'global' mountptcombo history (though we already saved the sshfs-specific stuff above)
}

bool MountSambaDialog::Init()
{
IPCombo = (wxComboBox*)FindWindow(wxT("IPCombo"));
HostnameCombo = (wxComboBox*)FindWindow(wxT("HostnameCombo"));
SharesCombo = (wxComboBox*)FindWindow(wxT("SharesCombo"));
MountptCombo = (wxComboBox*)FindWindow(wxT("MountptCombo"));
if (!MountptCombo) return false; // This is new in 4.0, so protect against a stale rc/
AlreadyMounted = (wxStaticText*)FindWindow(wxT("AlreadyMounted")); AlreadyMounted->Hide();
RwRadio  = (wxRadioBox*)FindWindow(wxT("RwRadio"));
Username = (wxTextCtrl*)FindWindow(wxT("Username"));
Password = (wxTextCtrl*)FindWindow(wxT("Password"));

historypath = wxT("/History/MountSamba/");
LoadMountptHistory();

GetFstabShares();

if (!FindData() && (NetworkArray.GetCount() == 0)) return false;  // Fill the array with data. If none, abort unless some came from /etc/fstab
for (size_t n=0; n < NetworkArray.GetCount(); ++n)
  { IPCombo->Append(NetworkArray.Item(n)->ip);
    HostnameCombo->Append(NetworkArray.Item(n)->hostname);
  }
#if (defined(__WXGTK__) || defined(__WXX11__))
  if (NetworkArray.GetCount())  IPCombo->SetSelection(0);     // Need actually to set the textbox in >2.7.0.1
  if (NetworkArray.GetCount())  HostnameCombo->SetSelection(0);
#endif
if (NetworkArray.GetCount())  LoadSharesForThisServer(0);     // If there were any servers found, load the shares for the 1st of them

return true;
}

void MountSambaDialog::LoadSharesForThisServer(int sel) // Fill the relevant combo with available shares for the displayed server
{
if (sel == -1) return;                                        // If no currently selected hostname,  abort
if (sel > (int)(NetworkArray.GetCount()-1)) return;           // Shouldn't happen, of course
struct NetworkStruct* ss = NetworkArray.Item(sel);

SharesCombo->Clear(); SharesCombo->SetValue(wxEmptyString);
for (size_t c=0; c < ss->shares.GetCount(); ++c)              // We have the server.  Load its shares into the combo
    SharesCombo->Append(ss->shares[c]);
#if (defined(__WXGTK__) || defined(__WXX11__))
  if (SharesCombo->GetCount()) SharesCombo->SetSelection(0);
#endif

if (!ss->user.empty())    Username->ChangeValue(ss->user);    // If we have user/pass/ro info for this item, set it (but don't clobber unnecessarily)
if (!ss->passwd.empty())  Password->ChangeValue(ss->passwd);
RwRadio->SetSelection(ss->readonly);

GetMountptForShare();                                         // Finally display any relevant mountpt
}

void MountSambaDialog::GetMountptForShare()  // Enter into MountptTxt any known mountpoint for the selection in SharesCombo
{
wxString device1, device2;

wxString share = SharesCombo->GetValue();
if (share.IsEmpty())                                          // If no currently selected share,  abort
  { InFstab = IsMounted = false; FstabMt.Empty(); AtMountPt.Empty();
    AlreadyMounted->Hide(); return;
  }

device1.Printf(wxT("//%s/%s"), HostnameCombo->GetValue().c_str(), share.c_str());    // Construct a 'device' name to search for.  \\hostname\share
device2.Printf(wxT("//%s/%s"), IPCombo->GetValue().c_str(), share.c_str());          //   & another with eg \\192.168.0.1\share

struct fstab* fs = DeviceAndMountManager::ReadFstab(device1);
if (fs == NULL)  fs = DeviceAndMountManager::ReadFstab(device2);          // Null means not found so try again with the IP version
InFstab = (fs != NULL);                                                   // Store or null the data according to the result
FstabMt = (InFstab ? wxString(fs->fs_file, wxConvUTF8) : wxT(""));

#ifdef __linux__
struct mntent* mnt = DeviceAndMountManager::ReadMtab(device1);            // Now read mtab to see if the share's already mounted
#else
struct statfs* mnt = DeviceAndMountManager::ReadMtab(device1);
#endif
if (mnt == NULL)  mnt = DeviceAndMountManager::ReadMtab(device2);         // Null means not found, so try again with the IP version
IsMounted = (mnt != NULL);
AlreadyMounted->Show(IsMounted); GetSizer()->Layout();                    // If it is mounted, expose the wxStaticTxt that says so (and Layout, else 2.8.0 displays it in top left corner!)
#ifdef __linux__
AtMountPt = (IsMounted ? wxString(mnt->mnt_dir, wxConvUTF8) : wxT(""));   // Store any mountpt, or delete any previous entry
#else
AtMountPt = (IsMounted ? wxString(mnt->f_mntonname, wxConvUTF8) : wxT(""));
#endif
if (IsMounted)
    MountptCombo->SetValue(AtMountPt);
  else if (InFstab)
    MountptCombo->SetValue(FstabMt);
}

size_t IsPlausibleIPaddress(wxString& str)
{
if (str.IsEmpty()) return false;                                          // Start with 2 quick checks.  If the string is empty,
if (!wxIsxdigit(str.GetChar(0))) return false;                            //    or if the 1st char isn't a hex number, abort

if (str.find(wxT(':')) == wxString::npos)                                 // If there isn't a ':', it can't be IPv6
  { wxStringTokenizer tkn(str, wxT("."), wxTOKEN_STRTOK);
    if (tkn.CountTokens() !=4) return false;                              // If it's IPv4 there should be 4 tokens
    for (int c=0; c < 4; ++c)
      { unsigned long val;
        if ((tkn.GetNextToken()).ToULong(&val) == false  ||  val > 255)   // If each token isn't a number < 256, abort
            return false;
      }
    return 4;                                                             // If we've here, it's a valid IPv4 address.  Return 4 to signify this
  }

wxStringTokenizer tkn(str, wxT(":"), wxTOKEN_STRTOK);
size_t count = tkn.CountTokens();
if (count == 0 || count > 8) return false;    // If it's IPv6 we can't be sure of the token no as :::::::1 is valid, but there must be 1, and < 9
for (size_t c=0; c < count; ++c)
  { unsigned long val;
    wxString next = tkn.GetNextToken();
    if (next.IsEmpty())
      { if (c == (count-1))  return false; }                              // If the token is empty, abort if it's the last one
     else
      if (next.ToULong(&val,16) == false  ||  val > 255)                  // If each token isn't a number < 256, abort
        return false;
  }

return 8;                                                                 // If we've here, it's a valid IPv6 address.  Return 8 to signify this
}

bool MountSambaDialog::GetSharesForServer(wxArrayString& array, wxString& hostname, bool findprinters/*=false*/)  // Return  hostname's available shares
{
wxString diskorprinter = (findprinters ? wxT("Printer") : wxT("Disk"));   // This determines which sort of line is recognised

wxString smbclient;                                                 // Check that we have a known smbclient file
if (Configure::TestExistence(wxT("smbclient"), true))  smbclient = wxT("smbclient");
 else
   { FileData fd(SMBPATH + wxT("smbclient"));                       // If it's not in PATH, see if it's in a known place
    if (fd.IsValid()) smbclient = SMBPATH + wxT("smbclient");
  }

if (smbclient.IsEmpty())
  { wxMessageBox(_("I can't find the utility \"smbclient\". This probably means that Samba isn't properly installed.\nAlternatively there may be a PATH or permissions problem"));
    return false; 
  }

wxArrayString output, errors;                                       // Call smbclient to list the available Shares on this host
if (wxExecute(smbclient + wxT(" -d0 -N -L ") + hostname,  output, errors) != 0)
  return true; // Don't shout and scream here: this may be a stale nmb entry, with others succeeding. And return true, not false, else FindData() will abort

for (size_t n=0; n < output.GetCount(); ++n)                        // The output is in 'output'
  { wxStringTokenizer tkn(output[n]);                               // Parse it to find lines that begin with an IP address
    wxString first = tkn.GetNextToken();
    if (first.Right(1) == wxT("$")) continue;                       // If it ends in $, it's an admin thing eg ADMIN$, so ignore
    if (tkn.GetNextToken() == diskorprinter)                        // If the next token is Disk (or Printer if we're looking for printers), it's a valid share
      array.Add(first);                                             //   so add the name to array
  }
return true;
}

bool MountSambaDialog::FindData()    // Query the network to find samba servers, & each server to find available Shares
{
wxString nmblookuperrormsg =
          _("I can't get the utility \"nmblookup\" to work. This probably means that Samba isn't properly installed.\nAlternatively there may be a PATH or permissions problem");
wxString nmblookup;                                                   // Check that we have a known nmblookup file
if (Configure::TestExistence(wxT("nmblookup"), true))  nmblookup = wxT("nmblookup");
 else
  { FileData fd(SMBPATH + wxT("nmblookup"));                          // If it's not in PATH, see if it's in a known place
    if (fd.IsValid()) nmblookup = fd.GetFilepath();
  }

if (nmblookup.IsEmpty())
  { wxMessageBox(_("I can't find the utility \"nmblookup\". This probably means that Samba isn't properly installed.\nAlternatively there may be a PATH or permissions problem"));
    return false; 
  }

wxBusyInfo wait(_("Searching for samba shares..."));

  // We don't know what the local IP address is, so start by not specifying one. That seems to give only one result, but from that we can deduce where to look
wxArrayString output, errors; wxString IPaddress, secondchance;
if (wxExecute(nmblookup + wxT(" -d0 '*'"), output, errors) != 0)      // -d0 means debug=0
  { wxMessageBox(nmblookuperrormsg);
    return false; 
  }
if (!output.GetCount()) return true;                                  // even though there's no data :/

for (size_t n=0; n < output.GetCount(); ++n)                          // The output from nmblookup is in 'output'
  { wxStringTokenizer tkn(output[n]);                                 // Parse it to find lines that begin with an IP address
    wxString addr = tkn.GetNextToken();
    if (IsPlausibleIPaddress(addr))
      { IPaddress = addr.BeforeLast(wxT('.')) + wxT(".0"); break; }   // Found a valid address. Deduce the base e.g. 192.168.0.3 -> 192.168.0.0
    // If that string isn't the right answer, it may contain '<cruft> 127.255.255.255' or '<cruft> 192.168.0.255'
    // This is all we'll get if there's not a local samba server, so grab the latter
    wxString retry = output[n].AfterLast(wxT(' '));
    if ((retry != wxT("127.255.255.255")) && IsPlausibleIPaddress(retry))
			 secondchance = retry.BeforeLast(wxT('.')) + wxT(".0");
  }
if (IPaddress.empty()) IPaddress = secondchance;

if (IPaddress.empty())
  { wxString msg(_("I can't find an active samba server.\nIf you know of one, please tell me its address e.g. 192.168.0.3"));
    IPaddress = wxGetTextFromUser(msg, _("No server found"));
    if (IPaddress.empty()) return false;
  }

output.Clear(); errors.Clear();                                       // Now try again, using the base IP address. That should query other boxes too
if (wxExecute(nmblookup + wxT(" -d0 -B ") + IPaddress + wxT(" '*'"), output, errors) != 0)
  { wxMessageBox(nmblookuperrormsg);
    return false; 
  }

wxArrayString IPList;
for (size_t n=0; n < output.GetCount(); ++n)                          // Parse the resulting output
  { wxStringTokenizer tkn(output[n]);
    wxString addr = tkn.GetNextToken();
    if (IsPlausibleIPaddress(addr)) IPList.Add(addr);
  }
  
  // We should now have a list of >0 IP addresses e.g. 192.168.0.2, 192.168.0.3. We need to query each to find its share(s)
for (size_t i=0; i < IPList.GetCount(); ++i)
  { wxString addr = IPList.Item(i);
    output.Clear(); errors.Clear(); 
    if (wxExecute(nmblookup + wxT(" -d0 -A ") + addr, output, errors) != 0)
      { wxMessageBox(nmblookuperrormsg);
        return false; 
      }

    for (size_t n=0; n < output.GetCount(); ++n)
      { wxStringTokenizer tkn(output[n]);
        wxString hostname = tkn.GetNextToken(); hostname.Trim(false);
        wxString second = tkn.GetNextToken(); second.Trim(false);
        if (second != wxT("<00>")) continue;  // We're looking for lines that (trimmed) look like "hostname<00> ..."
        
        struct NetworkStruct* ss = new struct NetworkStruct;  
        ss->ip = addr;
        ss->hostname = hostname;
        if (!GetSharesForServer(ss->shares, ss->hostname)) return false;  // False means smbclient doesn't work, not just that there are no available shares
        NetworkArray.Add(ss); break;
      }
  }

return true;
}

void MountSambaDialog::GetFstabShares()
{
ArrayofPartitionStructs PartitionArray; wxString types(wxT("smbfs,cifs"));
size_t count = MyFrame::mainframe->Layout->m_notebook->DeviceMan->DeviceAndMountManager::ReadFstab(PartitionArray, types);
for (size_t n=0; n < count; ++n)
  { // The 'partition' may be //netbiosname/share or //192.168.0.1/share
    if ((PartitionArray.Item(n)->device.Left(2) != wxT("//")) || (PartitionArray.Item(n)->device.Mid(2).Find(wxT('/')) == wxNOT_FOUND)) continue;
    wxString hostname = PartitionArray.Item(n)->device.Mid(2).BeforeLast(wxT('/'));
    wxString share = PartitionArray.Item(n)->device.Mid(2).AfterLast(wxT('/'));

    struct NetworkStruct* ss = new struct NetworkStruct;
    if (IsPlausibleIPaddress(hostname)) ss->ip = hostname;
      else ss->hostname = hostname;
    ss->shares.Add(share); ss->mountpt = PartitionArray.Item(n)->mountpt;

    wxArrayString optsarray = wxStringTokenize(PartitionArray.Item(n)->uuid, // Options were put here, for want of a better field
                                                    wxT(","),wxTOKEN_STRTOK);
    for (size_t op=0; op < optsarray.GetCount(); ++op)
      { wxString opt = optsarray.Item(op).Trim().Trim(false);
        
        if (opt == wxT("ro")) { ss->readonly = true; continue; }
        if (opt == wxT("rw")) { ss->readonly = false; continue; }  // This is the default anyway
        wxString rest;
        if (opt.StartsWith(wxT("user="), &rest) || opt.StartsWith(wxT("username="), &rest))
          { ss->user = rest; continue; }
        if (opt.StartsWith(wxT("pass="), &rest) || opt.StartsWith(wxT("password="), &rest))
          { ss->passwd = rest; continue; }
      }

    NetworkArray.Add(ss);
  }
}

void MountSambaDialog::OnSelectionChanged(wxCommandEvent& event)
{
int id = event.GetId();
if (id == XRCID("SharesCombo"))  { GetMountptForShare(); return; }    // If it's the Shares combo, enter any known mountpoint

                  // If it's one of the Server combos, set the other one to the same selection
if (id == XRCID("IPCombo"))  
  { HostnameCombo->SetSelection(IPCombo->GetSelection());
    LoadSharesForThisServer(IPCombo->GetSelection());                 // Then load that selection's shares
  }
  
if (id == XRCID("HostnameCombo"))  
  { IPCombo->SetSelection(HostnameCombo->GetSelection());
    LoadSharesForThisServer(HostnameCombo->GetSelection());
  }                                    
}

void MountSambaDialog::OnUpdateUI(wxUpdateUIEvent& event)
{ // NB The event here is from Username ctrl.  We have to use a textctrl UI event as, at least in wxGTK2.4.2, wxRadioBox doesn't generate them!
bool enabled = !((wxRadioBox*)FindWindow(wxT("PwRadio")))->GetSelection();

Username->Enable(enabled);
((wxStaticText*)FindWindow(wxT("UsernameStatic")))->Enable(enabled);
Password->Enable(enabled);
((wxStaticText*)FindWindow(wxT("PasswordStatic")))->Enable(enabled);
}

BEGIN_EVENT_TABLE(MountSambaDialog,MountPartitionDialog)
  EVT_COMBOBOX(-1, MountSambaDialog::OnSelectionChanged)
  EVT_UPDATE_UI(XRCID("Username"), MountSambaDialog::OnUpdateUI)
END_EVENT_TABLE()

bool MountNFSDialog::Init()
{
IPCombo = (wxComboBox*)FindWindow(wxT("IPCombo"));
SharesCombo = (wxComboBox*)FindWindow(wxT("SharesCombo"));
MountptCombo = (wxComboBox*)FindWindow(wxT("MountptCombo"));
if (!MountptCombo) return false; // This is new in 4.0, so protect against a stale rc/
RwRadio = (wxRadioBox*)FindWindow(wxT("RwRadio"));
MounttypeRadio = (wxRadioBox*)FindWindow(wxT("MounttypeRadio"));
AlreadyMounted = (wxStaticText*)FindWindow(wxT("AlreadyMounted")); AlreadyMounted->Hide();

historypath = wxT("/History/MountNFS/");
LoadMountptHistory();

#ifndef __GNU_HURD__  // There's no showmount or equivalent in hurd atm, so the user has to enter hosts by hand
  while (SHOWMOUNTFPATH.IsEmpty() || !Configure::TestExistence(SHOWMOUNTFPATH))
    { // Test again: the user might not yet have had nfs installed when 4Pane first looked
      if (Configure::TestExistence(wxT("/usr/sbin/showmount")))
        { SHOWMOUNTFPATH = wxT("/usr/sbin/showmount");  wxConfigBase::Get()->Write(wxT("/Network/ShowmountFilepath"), SHOWMOUNTFPATH); break; }
      if (Configure::TestExistence(wxT("/sbin/showmount")))
        { SHOWMOUNTFPATH = wxT("/sbin/showmount");  wxConfigBase::Get()->Write(wxT("/Network/ShowmountFilepath"), SHOWMOUNTFPATH); break; }

      wxMessageBox(_("I'm afraid I can't find the showmount utility. Please use Configure > Network to enter its filepath")); return false; 
    }
#endif
 
return FindData();    // Fill the array with data
}

void MountNFSDialog::LoadExportsForThisServer(int sel)  // Fill the relevant combo with available exports for the displayed server
{
if (sel == -1) return;                                      // If no currently selected server,  abort
if (sel > (int)(NetworkArray.GetCount()-1)) return;         // Shouldn't happen, of course
struct NetworkStruct* ns = NetworkArray.Item(sel);

SharesCombo->Clear(); SharesCombo->SetValue(wxEmptyString); // Clear data from previous servers
wxArrayString exportsarray;
GetExportsForServer(exportsarray, ns->ip);                  // Find the exports for this server

for (size_t c=0; c < exportsarray.GetCount(); ++c)          // Load them into the combo
    SharesCombo->Append(exportsarray[c]);
#if (defined(__WXGTK__) || defined(__WXX11__))
  if (SharesCombo->GetCount()) SharesCombo->SetSelection(0);
#endif

RwRadio->SetSelection(ns->readonly);                        // If this item was extracted from /etc/fstab, there'll be valid data here
if (ns->texture == wxT("hard")) MounttypeRadio->SetSelection(0);
 else if (ns->texture == wxT("soft"))  MounttypeRadio->SetSelection(1); // If neither is specified, don't make any change

GetMountptForShare();                                       // Finally display any relevant mountpt
}

void MountNFSDialog::GetMountptForShare()  // Enter into MountptCombo any known mountpoint for the selection in SharesCombo
{
wxString share;
if (SharesCombo->GetCount())                                // Check first, as GTK-2.4.2 Asserts if we try to find a non-existent selection
  share = SharesCombo->GetValue();
if (share.IsEmpty())                                        // If no currently selected share,  abort
  { InFstab = IsMounted = false; FstabMt.Empty(); AtMountPt.Empty();
    AlreadyMounted->Hide(); return;
  }

wxString device = IPCombo->GetValue() + wxT(':') + share;   // Construct a 'device' name to search for eg 192.168.0.1:/exportdir

struct fstab* fs = DeviceAndMountManager::ReadFstab(device);
if (!fs)                                                    // Work around the usual terminal '/' issue
  { if (device.Right(1) == wxFILE_SEP_PATH) device = device.BeforeLast(wxFILE_SEP_PATH);
     else device << wxFILE_SEP_PATH;
    fs = DeviceAndMountManager::ReadFstab(device);
  }
InFstab = (fs != NULL);                                     // Store or null the data according to the result
FstabMt = (InFstab ? wxString(fs->fs_file, wxConvUTF8) : wxT(""));

#ifdef __linux__
mntent* mnt = DeviceAndMountManager::ReadMtab(device);      // Now read mtab to see if the share's already mounted
#else
struct statfs* mnt = DeviceAndMountManager::ReadMtab(device);
#endif
IsMounted = (mnt != NULL);
AlreadyMounted->Show(IsMounted); GetSizer()->Layout();      // If it is mounted, expose the wxStaticTxt that says so (and Layout, else 2.8.0 displays it in top left corner!)
#ifdef __linux__
AtMountPt = (IsMounted ? wxString(mnt->mnt_dir, wxConvUTF8) : wxT(""));  // Store any mountpt, or delete any previous entry
#else
AtMountPt = (IsMounted ? wxString(mnt->f_mntonname, wxConvUTF8) : wxT(""));
#endif

if (IsMounted)
    MountptCombo->SetValue(AtMountPt);
  else if (InFstab)
    MountptCombo->SetValue(FstabMt);
}

bool MountNFSDialog::GetExportsForServer(wxArrayString& exportsarray, wxString& server)  // Return server's available exports
{
#ifdef __GNU_HURD__
  return false; // There's no showmount or equivalent in hurd atm
#endif

if (SHOWMOUNTFPATH.IsEmpty())
{ wxMessageBox(_("I can't find the utility \"showmount\".  This probably means that NFS isn't properly installed.\nAlternatively there may be a PATH or permissions problem"));
  return false; 
}

wxArrayString output, errors;                                 // Call showmount to list the available Shares on this host
wxExecute(SHOWMOUNTFPATH + wxT(" --no-headers -e ") + server,  output, errors);

for (size_t n=0; n < output.GetCount(); ++n)                  // The output is in 'output'
  { wxStringTokenizer tkn(output[n]);                         // Parse each entry to find the first token, which is the exportable dir
    wxString first = tkn.GetNextToken();                      // (the rest is a list of permitted clients, & as we don't know who WE are . . .
    if (first.IsEmpty()) continue;
    exportsarray.Add(first);
  }
  
return true;                                                  //  with the exports list in exportsarray
}

bool MountNFSDialog::FindData()  // Query the network to find NFS servers, & each server to find available Exports
{
wxString ip;
#ifndef __GNU_HURD__  // which doesn't have showmount atm
  wxArrayString output, errors; 

  ClearNetworkArray();
  GetFstabShares();

  wxExecute(SHOWMOUNTFPATH + wxT(" --no-headers"), output, errors); // See if there're any other ip addresses already known to NFS
  for (size_t n=0; n < output.GetCount(); ++n)                      // The output from showmount is in 'output'
    { wxStringTokenizer tkn(output[n]);                             // Parse it to find lines that begin with an IP address
      wxString first = tkn.GetNextToken(); if (!IsPlausibleIPaddress(first)) continue;
      if (NFS_SERVERS.Index(first) == wxNOT_FOUND)  NFS_SERVERS.Add(first);
    }
    
  wxExecute(SHOWMOUNTFPATH + wxT(" --no-headers 127.0.0.1"),  output, errors);
  for (size_t n=0; n < output.GetCount(); ++n)
    { wxStringTokenizer tkn(output[n]);
      wxString first = tkn.GetNextToken(); if (!IsPlausibleIPaddress(first)) continue;
      if (NFS_SERVERS.Index(first) == wxNOT_FOUND)  NFS_SERVERS.Add(first);
    }

  if (!NFS_SERVERS.GetCount())                                  // If there weren't any answers, ask for advice
    if (wxMessageBox(_("I don't know of any NFS servers on your network.\nDo you want to continue, and write some in yourself?"),
                                                    _("No current mounts found"), wxYES_NO)  != wxYES) return false;
#endif

for (size_t n=0; n < NFS_SERVERS.GetCount(); ++n)             // Add each server name to the structarray
  { bool flag(false);
    for (size_t a=0; a < NetworkArray.GetCount(); ++a)        // but first check it wasn't already added in GetFstabShares()
      if (NetworkArray.Item(a)->ip == NFS_SERVERS[n])
        { flag = true; break; }
    if (!flag)
      { struct NetworkStruct* ss = new struct NetworkStruct;  
        ss->ip = NFS_SERVERS[n];
        NetworkArray.Add(ss);
      }
  }

IPCombo->Clear();
for (size_t n=0; n < NetworkArray.GetCount(); ++n)
  IPCombo->Append(NetworkArray.Item(n)->ip);

if (NetworkArray.GetCount())
  {
#if (defined(__WXGTK__) || defined(__WXX11__))
    IPCombo->SetSelection(0);           // Need actually to set the textbox in >2.7.0.1
#endif
    LoadExportsForThisServer(0);        // If there were any servers found, load the exports for the 1st of them
  }

return true;
}

void MountNFSDialog::GetFstabShares()
{
ArrayofPartitionStructs PartitionArray; wxString types(wxT("nfs,nfs4"));
size_t count = MyFrame::mainframe->Layout->m_notebook->DeviceMan->DeviceAndMountManager::ReadFstab(PartitionArray, types);
for (size_t n=0; n < count; ++n)
  { wxString ip = PartitionArray.Item(n)->device.BeforeFirst(wxT(':'));
    // if (!IsPlausibleIPaddress(ip)) continue;   Don't check for plausibility here, to cater for using hostnames instead
    wxString expt = PartitionArray.Item(n)->device.AfterFirst(wxT(':')); if (expt.empty()) continue;
    if (NFS_SERVERS.Index(ip) == wxNOT_FOUND)  NFS_SERVERS.Add(ip);           // If this address isn't already in the array, add it

    struct NetworkStruct* ss = new struct NetworkStruct;
    ss->ip = ip;
    ss->shares.Add(expt); ss->mountpt = PartitionArray.Item(n)->mountpt;      // Add these, but they'll probably be overwritten by LoadExportsForThisServer()

    wxArrayString optsarray = wxStringTokenize(PartitionArray.Item(n)->uuid,  // Options were put here, for want of a better field
                                                    wxT(","),wxTOKEN_STRTOK);
    for (size_t op=0; op < optsarray.GetCount(); ++op)                        // Process any we care about
      { wxString opt = optsarray.Item(op).Trim().Trim(false);
        
        if (opt == wxT("ro")) { ss->readonly = true; continue; }
        if (opt == wxT("rw")) { ss->readonly = false; continue; }  // This is the default anyway
        if ((opt == wxT("soft")) || (opt == wxT("hard"))) { ss->texture = opt; continue; }
      }

    NetworkArray.Add(ss);
  }

if (count) SaveIPAddresses();
}

void MountNFSDialog::OnAddServerButton(wxCommandEvent& WXUNUSED(event))  // Manually add another server to ipcombo
{
wxDialog dlg;
wxXmlResource::Get()->LoadDialog(&dlg, this, wxT("AddNFSServerDlg"));
if (dlg.ShowModal() != wxID_OK) return;

wxString newserver = ((wxTextCtrl*)dlg.FindWindow(wxT("NewServer")))->GetValue(); 
if (!IsPlausibleIPaddress(newserver)) return;

if (NFS_SERVERS.Index(newserver) != wxNOT_FOUND)  return;     // Check it's not a duplicate
NFS_SERVERS.Add(newserver);                                   // Add it to the servers array, struct array & combo.  Select it
struct NetworkStruct* ss = new struct NetworkStruct;
ss->ip = newserver; NetworkArray.Add(ss);

#ifdef __GNU_HURD__
  // There's no showmount or equivalent in hurd atm
#else
 FindData();  // We've got a new potential server, so call FindData() again to see if it's connected
#endif

if (((wxCheckBox*)dlg.FindWindow(wxT("Save")))->IsChecked())
  SaveIPAddresses();
}

void MountNFSDialog::SaveIPAddresses() const
{
wxConfigBase *config = wxConfigBase::Get();

wxString key(wxT("/Network/IPAddresses/"));
config->DeleteGroup(key);

size_t count = NFS_SERVERS.GetCount();                        // Find how many entries there now are
config->Write(key+wxT("Count"), (long)count);
for (size_t n=0; n < count; ++n)
  config->Write(key + CreateSubgroupName(count-1, count), NFS_SERVERS.Item(n));

config->Flush();
}

void MountNFSDialog::OnSelectionChanged(wxCommandEvent& WXUNUSED(event))
{
static bool flag=0; if (flag) return;                         // Avoid a 2.4.2 problem here with re-entrancy:
flag = 1;                                                     // Multiple selection events occur from the combobox if the L mouse button is held down while selecting
LoadExportsForThisServer(IPCombo->GetSelection());            // Load the new selection's exports
flag = 0;
}


BEGIN_EVENT_TABLE(MountNFSDialog,MountPartitionDialog)
  EVT_BUTTON(XRCID("AddServer"), MountNFSDialog::OnAddServerButton)
  EVT_COMBOBOX(XRCID("IPCombo"), MountNFSDialog::OnSelectionChanged)
  EVT_COMBOBOX(XRCID("SharesCombo"), MountNFSDialog::OnGetMountptForShare)
END_EVENT_TABLE()

void UnMountPartitionDialog::Init(DeviceAndMountManager* dad)
{
parent = dad;
PartitionsCombo = (wxComboBox*)FindWindow(wxT("PartitionsCombo"));
FstabMountptTxt = (wxTextCtrl*)FindWindow(wxT("FstabMountptTxt"));  // Find the textctrl

#ifdef __GNU_HURD__
  parent->FillGnuHurdMountsList(*parent->PartitionArray);
#endif

for (size_t n=0; n < parent->PartitionArray->GetCount(); ++n)
  { PartitionStruct* pstruct = parent->PartitionArray->Item(n);
    if (pstruct->IsMounted && !pstruct->mountpt.IsEmpty()           // Ignore unmounted partitions
                           && !(pstruct->mountpt == wxT("/")))      // Filter out root too; we don't want to be umounted ourselves
      { wxString devicestr(pstruct->device);
        if (!pstruct->label.empty()) devicestr << " " << pstruct->label;
         else if (!pstruct->uuid.empty())
          { wxString UUID(pstruct->uuid);
            if (UUID.Len() > 16)
              UUID = pstruct->uuid.BeforeFirst('-') + "..." + pstruct->uuid.AfterLast('-'); // Truncate display of standard UUIDs
            devicestr << " " << UUID;
          }
        PartitionsCombo->Append(devicestr);
      }
  }

#if (defined(__WXGTK__) || defined(__WXX11__))
if (PartitionsCombo->GetCount()) PartitionsCombo->SetSelection(0);
#endif

#if (defined(__LINUX__))
  DisplayMountptForPartition(true);                                 // Show mountpt. True means use the actual mtpt, not the fstab suggestion
#else
  DisplayMountptForPartition(false);                                // We just looked for the mount-point, so don't search for it again
#endif
}

size_t UnMountSambaDialog::Init()
{
SearchForNetworkMounts();

PartitionsCombo = (wxComboBox*)FindWindow(wxT("PartitionsCombo"));
FstabMountptTxt = (wxTextCtrl*)FindWindow(wxT("FstabMountptTxt"));  // Find the textctrl

for (size_t n=0; n < Mntarray.GetCount(); ++n)
  { PartitionStruct* mntstruct = Mntarray.Item(n);            // PartitionStruct is here a misnomer
    PartitionsCombo->Append(mntstruct->device);               // Append this share's name to the combobox      
  }
#if (defined(__WXGTK__) || defined(__WXX11__))
  if (PartitionsCombo->GetCount()) PartitionsCombo->SetSelection(0);
#endif

DisplayMountptForShare();                                     // Show corresponding mountpt

return Mntarray.GetCount();
}

void UnMountSambaDialog::SearchForNetworkMounts()  // Scans mtab for established NFS & samba mounts
{
#ifdef __linux__
FILE* fmp = setmntent (_PATH_MOUNTED, "r");                   // Get a file* to (probably) /etc/mtab
if (fmp==NULL) return;

while (1)                                                     // For every mtab entry
  { struct mntent* mnt = getmntent(fmp);                      //  get a struct representing a mounted partition
    if (mnt == NULL)  
      { endmntent(fmp);  return; }                            // If it's null, we've finished mtab

    wxString type(mnt->mnt_type, wxConvUTF8);
    if (ParseNetworkFstype(type) != MT_invalid)
      { struct PartitionStruct* newmnt = new struct PartitionStruct;
        newmnt->device = wxString(mnt->mnt_fsname, wxConvUTF8);
        newmnt->mountpt = wxString(mnt->mnt_dir, wxConvUTF8);
#else
struct statfs *fslist;

int numfs = getmntinfo(&fslist, MNT_NOWAIT);
if (numfs < 1) return;

for (int i = 0; i < numfs; ++i)
  { wxString type(fslist[i].f_fstypename, wxConvUTF8);
    if (ParseNetworkFstype(type) != MT_invalid)
      { struct PartitionStruct* newmnt = new struct PartitionStruct;
        newmnt->device = wxString(fslist[i].f_mntfromname, wxConvUTF8);
        newmnt->mountpt = wxString(fslist[i].f_mntonname, wxConvUTF8);
#endif
        newmnt->type = type;
        Mntarray.Add(newmnt);                      
      }
  }
}

UnMountSambaDialog::MtType UnMountSambaDialog::ParseNetworkFstype(const wxString& type) const // Is this a mount that we're interested in: nfs, sshfs or samba
{
if (type == wxT("nfs") || (type.Left(3) == wxT("nfs") && wxIsdigit(type.GetChar(3))))  // If its type is "nfs" or "nfs4" (note we don't want the mountpt for nfsd)
  return MT_nfs;
if (type.Contains(wxT("sshfs"))) return MT_sshfs;
if (type == wxT("smbfs") || type == wxT("cifs")) return MT_samba;

return MT_invalid;
}

void UnMountSambaDialog::DisplayMountptForShare()  // Show mountpt corresponding to a share or export or sshfs mount
{
int sel = PartitionsCombo->GetSelection(); if (sel == -1) return;  // Get the currently selected share/export.  Abort if none
if (sel > (int)(Mntarray.GetCount()-1)) return;               // Shouldn't happen, of course

FstabMountptTxt->Clear(); FstabMountptTxt->ChangeValue(Mntarray.Item(sel)->mountpt); // We have the correct struct, so shove its mountpt into the textctrl
m_Mounttype = ParseNetworkFstype(Mntarray.Item(sel)->type);   // Store which type of network mount for use when dismounting
}


BEGIN_EVENT_TABLE(UnMountSambaDialog,MountPartitionDialog)
  EVT_COMBOBOX(-1, UnMountSambaDialog::OnSelectionChanged)
END_EVENT_TABLE()


