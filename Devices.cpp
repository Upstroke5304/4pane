/////////////////////////////////////////////////////////////////////////////
// Name:       Devices.cpp
// Purpose:    Mounting etc of Devices
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
#include "wx/wfstream.h"
#include "wx/tokenzr.h"
#include "wx/regex.h"

#include "Devices.h"
#include "Externs.h"
#include "Mounts.h"
#include "MyDirs.h"
#include "MyGenericDirCtrl.h"
#include "MyFrame.h"
#include "Filetypes.h"
#include "Redo.h"
#include "Misc.h"
#include "Configure.h"

  // If Path, prepend it to Command, and wrap with sh -c quote
enum PPath_returncodes PrependPathToCommand(const wxString& Path, const wxString& Command, wxChar quote, wxString& FinalCommand, bool ThereWillBeParameters)
{
if (Command.IsEmpty()) return Oops;
if (Path.IsEmpty())                                 // and it normally will be. If so, no point in messing
  { FinalCommand = Command; return unaltered; }

  // There are 3 possibilities. 1) Just prepend the Path, and surround with "sh -c\"...\""
  // 2) Command already contains sh -c
  // 3) If ThereWillBeParameters, don't add the terminal \" here: it'll be added by the caller after the parameters
wxString localCommand(Command);
if (localCommand.Contains(wxT("sh -c")))            // We don't need two of them
  localCommand.Replace(wxT("sh -c"), wxT(""));      // Note that I'm not trying for any subsequent quotes. They'll likely be dquotes anyway

FinalCommand << wxT("cd ") << Path << wxT(" && ") << localCommand;
if (quote == wxT('\'')) EscapeQuote(FinalCommand);  // If we're going to wrap with sh -c '  '  to avoid clashes with double-quotes, escape any single-quotes first
wxString prepend(wxT("sh -c ")); prepend += quote;  // quote might be singlequote or dquote
FinalCommand = prepend + FinalCommand;

if (!ThereWillBeParameters) { FinalCommand += quote; return noneed4quote; }
return needsquote;
}

int GetUniqueDeviceButtonNo()      // Global to provide a unique index for each device button
{
static int no = ID_FLOPPY1;                           // Start with ID_FLOPPY1 (which is probably 0)

if (no >= MAXDEVICE)  { wxLogError(wxT("Sorry, too many devices already.")); }  // We're only allowed 99! NB the next no is still returned
return no++;
}

wxString FindUsbIdsFilepath() // Search for the filepath of usb.ids, which contains info about many usb devices
{
// This is usually one (or more) of: 
wxString choices[] = { wxT("/usr/share/hwdata/usb.ids"), wxT("/usr/share/usb.ids"), wxT("/usr/share/misc/usb.ids"), wxT("/var/lib/usbutils/usb.ids"), wxT("Not found") };

wxString fp(choices[0]);
for (size_t n = 0; n < 4;)
  { if (wxFileExists(fp)) return fp;
    fp = choices[++n];                // If it wasn't found, inc to the next choice. When the loop is about to exit, this will be "Not found"
  }

return fp;                            // If we're here, this will be "Not found"
}

int ParseUsbIdsFile(const wxString& vendorid, const wxString& productid) // See if there's useful type info about this usb device
{
static wxString filepath; if (filepath.empty()) filepath = FindUsbIdsFilepath(); // The first time in this 4Pane instance, look for the file
if (filepath == wxT("Not found")) return wxNOT_FOUND;

if (vendorid.empty() || productid.empty()) return wxNOT_FOUND;

wxFileInputStream file_input(filepath); if (!file_input.IsOk()) return wxNOT_FOUND;
wxBufferedInputStream input(file_input);
wxTextInputStream text(input);

/*The usb.ids format is:
Vendor1_hexID<tab>Vendorname1(optional)
<tab>hexProductID  Product1 Description
<tab>hexProductID  Product2 Description
Vendor2_hexID<tab>Vendorname2(optional)*/

wxString line;
while (!input.Eof())
  { line = text.ReadLine();
    if (!line.Len()) continue;                // Needed because of a 2.8.12 stl-build (openSUSE uses this) bug when the line is ""
    if (wxIsspace(line.GetChar(0))) continue; // The vendor lines don't start with white-space
    if (line.Left(vendorid.Len()) == vendorid)
      do
        { line = text.ReadLine();
          if (!wxIsspace(line.GetChar(0)))    // The product lines always start with a tab
            return wxNOT_FOUND;               // AFAICT a vendor ID will occur only once, so we can safely abort here

          line.Trim(false);
          if (line.Left(productid.Len()) == productid)
            { line.MakeLower();
              if (line.Contains(wxT("card reader"))) return 8; // These hard-coded numbers need to match the positions in DeviceInfo::LoadDefaults!
              if (line.Contains(wxT("flash drive")) || line.Contains(wxT("pen drive"))) return 6;
              return wxNOT_FOUND;
            }
        }
        while (1);
  }

return wxNOT_FOUND;
}

  // Two helper functions for DeviceAndMountManager::FillPartitionArrayUsingLsblk and ::FillPartitionArrayUsingBlkid
void ParseBlkidOutput(const wxString& line, wxString& device, wxString& type, wxString& uuid)
{
wxStringTokenizer tkz(line, wxT(": \t\r\n"));
while (tkz.HasMoreTokens())
  { wxString token = tkz.GetNextToken();
    if (token.Left(5) == wxT("/dev/")) device = token;
     else if (token.Left(4) == wxT("TYPE"))
        { type = token.AfterFirst(wxT('"')); type = type.BeforeFirst(wxT('"')); } // We want the insides of TYPE="ext4"
     else if (token.Left(4) == wxT("UUID"))
        { uuid = token.AfterFirst(wxT('"')); uuid = uuid.BeforeFirst(wxT('"')); } // We want the insides of UUID="1234-5678"
  }
}

wxString DoGetDataFromBlkid(const wxArrayString& output, const wxString& name, int field)
{
if (name.empty()) return wxString();

for (size_t n=0; n < output.GetCount(); ++n)
  { wxString line = output.Item(n);
    if (line.Contains(name))
      { wxString device, type, uuid;
        ParseBlkidOutput(line, device, type, uuid);
        return field==0 ? type:uuid;
      }
  }
return wxString();
}

wxString GetTypeFromBlkid(const wxArrayString& output, const wxString& name)
{
return DoGetDataFromBlkid(output, name, 0);
}

wxString GetUUIDFromBlkid(const wxArrayString& output, const wxString& name)
{
return DoGetDataFromBlkid(output, name, 1);
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------
BEGIN_EVENT_TABLE(MyButtonDialog,wxDialog)    // The rest of the MyButtonDialog class is in .h, but the event table needs to be here
  EVT_BUTTON(-1, MyButtonDialog::OnButtonPressed)
END_EVENT_TABLE()
//--------------------------------------------------------------------------------------------------------------------------------------------------------------

BEGIN_EVENT_TABLE(DeviceBitmapButton, MyBitmapButton)
  EVT_BUTTON(-1, DeviceBitmapButton::OnButtonClick)
  EVT_MENU(ID_MOUNT_DVDRAM, DeviceBitmapButton::OnMountDvdRam)
  EVT_MENU_RANGE(ID_DISPLAY_DVDRAM, ID_DISPLAY_DVDRAM+MAX_NO_OF_DVDRAM_MOUNTS, DeviceBitmapButton::OnDisplayDvdRam)
  EVT_MENU(ID_UNMOUNT_DVDRAM, DeviceBitmapButton::OnUnMountDvdRam)
  EVT_MENU_RANGE(ID_MOUNT, ID_MOUNT+MAX_NO_OF_PARTITONS, DeviceBitmapButton::OnMount)      // Same as above, but from context menu choice
  EVT_MENU_RANGE(ID_UNMOUNT, ID_UNMOUNT+MAX_NO_OF_PARTITONS, DeviceBitmapButton::OnUnMount)
  EVT_MENU(ID_EJECT, DeviceBitmapButton::OnEject)
  EVT_CONTEXT_MENU(DeviceBitmapButton::OnRightButtonClick)
#ifdef __WXX11__
  EVT_RIGHT_UP(DeviceBitmapButton::OnRightUp)         // EVT_CONTEXT_MENU doesn't seem to work in X11
#endif
END_EVENT_TABLE()

//static 
long MyBitmapButton::UnknownButtonNo = wxNOT_FOUND;   // In case a bitmap isn't found, use unknown.xpm

//static
void MyBitmapButton::SetUnknownButtonNo()   // Tries to locate the iconno of unknown.xpm
{
wxConfigBase* config = wxConfigBase::Get(); if (config == NULL)  return;

long NoOfKnownIcons = config->Read(wxT("/Editors/Icons/NoOfKnownIcons"), 0l);
for (long n=0; n < NoOfKnownIcons; ++n)
  { wxString Icon;
    config->Read(wxT("/Editors/Icons/")+CreateSubgroupName(n,NoOfKnownIcons)+wxT("/Icon"), &Icon);
    if (Icon == wxT("unknown.xpm"))
      { UnknownButtonNo = n; return; }
  }
}

DeviceBitmapButton::DeviceBitmapButton(enum devicecategory which)
{
wxConfigBase* config = wxConfigBase::Get(); if (config==NULL)  { wxLogDebug(_("Couldn't load configuration!?")); return; }
long count = config->Read(wxT("Devices/Misc/NoOfButtons"), 0l);   // How many buttons have info stored?
if (!count) 
  { SaveDefaults(); config->Read(wxT("Devices/Misc/NoOfButtons"), 0l); }  // If there's currently no button data, save (& then use) defaults

if (which >= (int)count) which = unknowncat;                      // If the requested button isn't, use the generic one


wxString Rootname(wxT("Devices/Buttons/"));
wxString subgrp = CreateSubgroupName((int)which, count);          // Create the correct subgroup name: "a", "b" etc
wxString bitmap; config->Read(Rootname+subgrp+wxT("/Bitmap"), &bitmap);
wxString tooltip; config->Read(Rootname+subgrp+wxT("/Tooltip"), &tooltip);

bitmap = BITMAPSDIR + bitmap;
#if wxVERSION_NUMBER > 3105
Create(MyFrame::mainframe->panelette, -1, wxBitmapBundle::FromBitmap(bitmap), wxDefaultPosition, wxDefaultSize, wxNO_BORDER);
#else
Create(MyFrame::mainframe->panelette, -1, bitmap, wxDefaultPosition, wxDefaultSize, wxNO_BORDER);
#endif
SetToolTip(tooltip);
}

//static
void DeviceBitmapButton::SaveDefaults(wxConfigBase* config/*=NULL*/)
{
const wxChar* DefaultBitmaps[] = { wxT("harddisk.xpm"), wxT("floppy.xpm"), wxT("cdrom.xpm"), wxT("cdr.xpm"), wxT("UsbPen.xpm"), wxT("UsbMem.xpm"), wxT("UsbMulticard.xpm"), wxT("harddisk-usb.xpm"), wxT("unknown.xpm") };
const wxString DefaultTooltips[] = {_("Hard drive"),_("Floppy disc"),_("CD or DVDRom"),_("CD or DVD writer"),_("USB Pen"),_("USB memory card"),_("USB card-reader"),_("USB hard drive"),_("Unknown device") };

if (config == NULL)  // It'll usually be null, the exception being if coming from ConfigureMisc::OnExportBtn
   config = wxConfigBase::Get();  if (config==NULL)  return; 
long count = sizeof(DefaultBitmaps)/sizeof(wxChar*);

config->Write(wxT("Devices/Misc/NoOfButtons"), count);

wxString Rootname(wxT("Devices/Buttons/"));
for (long n=0; n < count; ++n)
  { wxString subgrp = CreateSubgroupName(n, count);
    config->Write(Rootname+subgrp+wxT("/Bitmap"), wxString(DefaultBitmaps[n]));
    config->Write(Rootname+subgrp+wxT("/Tooltip"), DefaultTooltips[n]);
  }
}

void DeviceBitmapButton::OnMount(wxCommandEvent& event)  // If the Context Menu choice was to Mount one of the partitions
{
if (ButtonNumber < ID_FLOPPY1 || ButtonNumber >= MAXDEVICE)  return;// Check it's a device

DevicesStruct* tempstruct = MyFrame::mainframe->Layout->m_notebook->DeviceMan->deviceman->FindStructForButtonNumber(ButtonNumber);  // Find its struct
if (tempstruct == NULL) return;

tempstruct->defaultpartition = event.GetId() - ID_MOUNT;            // If the partition chosen wasn't the current default, it is now!
OnButtonClick(event);                                               // Now use OnButtonClick() to do the actual mount
}

void DeviceBitmapButton::OnButtonClick(wxCommandEvent& WXUNUSED(event))
{
if (ButtonNumber < ID_FLOPPY1 || ButtonNumber >= MAXDEVICE)  return;// Check it's a device
DeviceAndMountManager* pDM = MyFrame::mainframe->Layout->m_notebook->DeviceMan; if (pDM==NULL) return;
        // The button that was clicked knows its number, which was given it on creation.  It matches the enum, so:-
DevicesStruct* tempstruct = pDM->deviceman->FindStructForButtonNumber(ButtonNumber); if (tempstruct == NULL) return;  // Find the device's struct
        // If this is a device with removable media eg dvdrom, & is automounted, we need to see if the disc has been changed, as that will affect the partitions/mtpts
if (pDM->deviceman->DevInfo->ThisDevicetypeAutomounts(tempstruct->devicetype)  
                    && pDM->deviceman->DevInfo->HasRemovableMedia(tempstruct->devicetype))
  { FileData stat(tempstruct->devicenode); if (!stat.IsValid()) return;    // Beware of the symlink;  eg /dev/dvd linked to  eg /dev/hdc
    pDM->SearchTable(tempstruct->partitions, tempstruct->mountpts, stat.GetSymlinkDestination(), pDM->deviceman->DevInfo->GetIcon(tempstruct->devicetype));
  }

wxString mountpt = pDM->Mount((size_t)ButtonNumber, true);      // Mount it 
if (mountpt.IsEmpty()) return;
        // Still here?  We must have mounted successfully.  Now display the device's contents in active treectrl
MyGenericDirCtrl* Active = MyFrame::mainframe->GetActivePane();
if (Active)  Active->OnOutsideSelect(mountpt, false);   // Call 'active' treectrl.  False because otherwise OnOutsideSelect reMounts
}

void DeviceBitmapButton::OnRightButtonClick(wxContextMenuEvent& event)
{
wxString display, undisplay; bool DvdRamOnly = false;
wxArrayString DvdRamMounts;                                         // If this device 'does' dvdram too, we'll store a list of current mounts here

if (ButtonNumber < ID_FLOPPY1 || ButtonNumber >= MAXDEVICE) return; // Check it's a device
DeviceAndMountManager* pDMM = MyFrame::mainframe->Layout->m_notebook->DeviceMan; if (pDMM==NULL) return;
DeviceManager* pDMdm = pDMM->deviceman; if (pDMdm==NULL) return;

DevicesStruct* tempstruct = pDMdm->FindStructForButtonNumber(ButtonNumber); if (tempstruct == NULL) return;  // Find its struct
bool Automounts = pDMdm->DevInfo->ThisDevicetypeAutomounts(tempstruct->devicetype);

if (Automounts && pDMdm->DevInfo->HasRemovableMedia(tempstruct->devicetype))// If this is a device with removable media eg dvdrom, we need to see if the disc has been changed, as that will affect the partitions/mtpts
  { FileData stat(tempstruct->devicenode); if (!stat.IsValid()) return;     // Beware of the symlink;  eg /dev/dvd linked to  eg /dev/hdc
    pDMM->SearchTable(tempstruct->partitions, tempstruct->mountpts, stat.GetSymlinkDestination(), pDMdm->DevInfo->GetIcon(tempstruct->devicetype));
  }

bool CopesWithDvdRaM = pDMdm->DevInfo->GetDvdRam(tempstruct->devicetype);
if (CopesWithDvdRaM)
  { pDMM->SearchMtabForStandardMounts(tempstruct->devicenode, DvdRamMounts);// Load array with any DVD-RAM mtpts
    for (int n = (int)tempstruct->partitions.GetCount()-1; n >= 0; --n)
      if (DvdRamMounts.Index(tempstruct->mountpts[n]) != wxNOT_FOUND)       //   & remove them from the main array
        tempstruct->partitions.RemoveAt(n);
  }

size_t partitioncount = tempstruct->partitions.GetCount();
if (!partitioncount)
  { if (!CopesWithDvdRaM) return;  // Shouldn't happen, but if there is no partition data, abort as 1) it won't do anything useful, & 2) It'll crash!
      else DvdRamOnly = true;      //  unless this is an unmounted dvd-ram, in which case just show that menu item
  }

wxString vendor(tempstruct->Vendorstr); if (vendor.empty()) vendor = tempstruct->name1; // Use the user-friendly labels if they exist
wxString model(tempstruct->Productstr); if (model.empty()) model = tempstruct->name2;
wxMenu menu(vendor + wxT("  ") + model + _(" menu"));

partitioncount = wxMin(partitioncount, (size_t)MAX_NO_OF_PARTITONS);

wxString GoTo(_("&Display    "));
if (Automounts)  { display = GoTo; undisplay = _("&Undisplay   "); }
 else 
  { display = _("&Mount    "); undisplay = _("&UnMount   "); 
    if (partitioncount > 1 &&  tempstruct->mountpts[tempstruct->defaultpartition].IsEmpty())
        tempstruct->defaultpartition = 0;  // If we're not automounting, & >1 partition, try to ensure that defaultpartition indexes a valid mountpt if one exists
  }

for (size_t n=0; n < partitioncount; ++n)
  { wxString marker(wxT("  ")), ptn, space, mnt = tempstruct->mountpts[n];
    if (partitioncount > 1 && !Automounts)  // If we're not automounting, & >1 partition, display them all, not just those in fstab, to allow user to choose to su-mount them
      { ptn = tempstruct->partitions[n]; space = wxT("   "); }
    if (partitioncount > 1 && tempstruct->defaultpartition == (int)n)   marker = wxT("* ");  // Mark the default partition if there's more than 1
    if (pDMM->ReadMtab(tempstruct->partitions[n]) == NULL)
          menu.Append(ID_MOUNT+n, display + marker + ptn + space + mnt);
     else  menu.Append(ID_MOUNT+n, GoTo + marker + ptn + space + mnt);  // If it's already mounted, this says "Display " even if not automounting
  }
  
if (Automounts && partitioncount > 1) menu.AppendSeparator();

if (!DvdRamOnly)
 { bool SeparatorPrepended = !(partitioncount > 1);
  if (Automounts)
     menu.Append(ID_UNMOUNT+tempstruct->defaultpartition, undisplay + tempstruct->mountpts[tempstruct->defaultpartition]);
   else
    for (size_t n=0; n < partitioncount; ++n)  // If we're not automounting, list all available mounted partitions, as >1 just could have been mounted
      { if (!pDMM->Checkmtab(tempstruct->mountpts[n], tempstruct->partitions[n]))  // See if this partition's mounted.  Can't unmount otherwise!
          { FileData fd(tempstruct->partitions[n]); if (!fd.IsSymlink()) continue; // If apparently not, look harder if it's a symlink
            if (!pDMM->Checkmtab(tempstruct->mountpts[n], fd.GetUltimateDestination()))  continue;
          }
        if (!SeparatorPrepended) { menu.AppendSeparator(); SeparatorPrepended = true; } // If there's something to unmount, prepend a separator if not already done
        menu.Append(ID_UNMOUNT+n, undisplay + tempstruct->mountpts[n]);
      }
 }

if (CopesWithDvdRaM)                                        // If this is (also) a DVD-RAM device
  { if (!DvdRamOnly)  menu.AppendSeparator();
    size_t count = DvdRamMounts.GetCount();
    if (!count)                                             // See if this device is already mounted ext2 or whatever
      menu.Append(ID_MOUNT_DVDRAM, _("Mount a DVD-&RAM"));  // If not, offer to mount it.  We call it "mount" even if other things automount
     else
      { for (size_t n=0; n < count; ++n)                    // Offer to display any mounted dvdram discs
          { wxString label(_("Display ") + DvdRamMounts[n]); menu.Append(ID_DISPLAY_DVDRAM, label); }
        menu.Append(ID_UNMOUNT_DVDRAM, _("UnMount DVD-&RAM")); // & unmount them too
      }
  }

if (pDMdm->DevInfo->HasRemovableMedia(tempstruct->devicetype))  // If it's a cdrom/floppy, add eject option
  { menu.AppendSeparator();
    menu.Append(ID_EJECT, _("&Eject"));
  }

wxPoint pt = ScreenToClient(wxGetMousePosition());
PopupMenu(&menu, pt.x, pt.y);
}

void DeviceBitmapButton::OnEject(wxCommandEvent& WXUNUSED(event))
{
if (ButtonNumber < ID_FLOPPY1 || ButtonNumber >= MAXDEVICE) return; // Check it's a device
DeviceAndMountManager* pDM = MyFrame::mainframe->Layout->m_notebook->DeviceMan; if (pDM==NULL) return;
DevicesStruct* tempstruct = pDM->deviceman->FindStructForButtonNumber(ButtonNumber); if (tempstruct == NULL) return;

wxExecute(wxT("eject ")+tempstruct->devicenode);
}

void DeviceBitmapButton::OnUnMount(wxCommandEvent& event)
{
if (ButtonNumber < ID_FLOPPY1 || ButtonNumber >= MAXDEVICE) return; // Check it's a device

DeviceManager* pDM = MyFrame::mainframe->Layout->m_notebook->DeviceMan->deviceman; if (pDM==NULL) return;

DevicesStruct* tempstruct = pDM->FindStructForButtonNumber(ButtonNumber); if (tempstruct == NULL) return;
tempstruct->defaultpartition = event.GetId() - ID_UNMOUNT;     // If the partition chosen wasn't the current default, it is now!

wxString mountpt = MyFrame::mainframe->Layout->m_notebook->DeviceMan->Mount((size_t)ButtonNumber, false);  // Try to umount (false flags umount)
if (!mountpt.IsEmpty())                                       // Empty string flags failure
  { MyFrame::mainframe->OnUnmountDevice(mountpt);             // If it worked, go thru all tabs to change any treectrl(s) displaying this mount, to  eg HOME
    if (!pDM->DevInfo->ThisDevicetypeAutomounts(tempstruct->devicetype) && tempstruct->defaultpartition > 0)  // If the default partition isn't 0, we may have (un)mounted su root, so need to delete this mountpts entry
      { wxString devpath = tempstruct->devicenode.BeforeLast(wxFILE_SEP_PATH) + wxFILE_SEP_PATH;    // The safest way to do this is to reload the partition/mtpt data
        MyFrame::mainframe->Layout->m_notebook->DeviceMan->SearchTable(tempstruct->partitions, tempstruct->mountpts, devpath + (tempstruct->devicenode.AfterLast(wxFILE_SEP_PATH)).Left(3), 
                                                                                                                          pDM->DevInfo->GetIcon(tempstruct->devicetype));
      }
  }
}

void DeviceBitmapButton::OnMountDvdRam(wxCommandEvent& event)
{
if (ButtonNumber < ID_FLOPPY1 || ButtonNumber >= MAXDEVICE)  return;  // Check it's a device
DeviceAndMountManager* pDM = MyFrame::mainframe->Layout->m_notebook->DeviceMan; if (pDM==NULL) return;

DevicesStruct* tempstruct = pDM->deviceman->FindStructForButtonNumber(ButtonNumber); if (tempstruct == NULL) return;  // Find its struct
if (!pDM->deviceman->DevInfo->GetDvdRam(tempstruct->devicetype)) return;

wxDialog dlg;
wxXmlResource::Get()->LoadDialog(&dlg,MyFrame::mainframe, wxT("MountDvdRamDlg"));
wxTextCtrl* DeviceTxt = (wxTextCtrl*)dlg.FindWindow(wxT("DeviceTxt"));
wxComboBox* MountptCombo = (wxComboBox*)dlg.FindWindow(wxT("MountptCombo"));
DeviceTxt->SetValue(tempstruct->devicenode);
wxArrayString MountptsArray; pDM->FindUnmountedFstabEntry(tempstruct->devicenode, MountptsArray);  // Get a list of the unmounted, non-subfs, device entries in fstab
for (size_t n=0; n < MountptsArray.GetCount(); ++n)  MountptCombo->Append(MountptsArray[n]);
#if (defined(__WXGTK__) || defined(__WXX11__))  
  if (MountptsArray.GetCount()) MountptCombo->SetSelection(0);
#endif
if (dlg.ShowModal() != wxID_OK) return;

wxString device = DeviceTxt->GetValue();               // Read the data from the dialog, into fresh strings in case they've been altered
wxString mount = MountptCombo->GetValue();
DevicesStruct datastruct; datastruct.partitions.Add(device); datastruct.mountpts.Add(mount);
datastruct.readonly = false; datastruct.next=NULL; datastruct.defaultpartition = 0;
wxString mountpt = pDM->DoMount(&datastruct, true, 0, true);
if (mountpt.IsEmpty()) return;
        // Still here?  We must have mounted successfully.  Now display the device's contents in active treectrl
MyGenericDirCtrl* Active = MyFrame::mainframe->GetActivePane();
if (Active)  Active->OnOutsideSelect(mountpt, false);  // Call 'active' treectrl.  False because otherwise OnOutsideSelect reMounts
}

void DeviceBitmapButton::OnDisplayDvdRam(wxCommandEvent& event)
{
if (ButtonNumber < ID_FLOPPY1 || ButtonNumber >= MAXDEVICE) return;  // Check it's a device
DeviceAndMountManager* pDM = MyFrame::mainframe->Layout->m_notebook->DeviceMan; if (pDM==NULL) return;

DevicesStruct* tempstruct = pDM->deviceman->FindStructForButtonNumber(ButtonNumber); if (tempstruct == NULL) return;
if (!pDM->deviceman->DevInfo->GetDvdRam(tempstruct->devicetype)) return;

wxArrayString DvdRamMounts;
pDM->SearchMtabForStandardMounts(tempstruct->devicenode, DvdRamMounts); // We need a list of the mounted dvdram discs, to compare with the event id
size_t count = DvdRamMounts.GetCount(); if (!count) return;
size_t eventcount = size_t(event.GetId() - ID_UNMOUNT_DVDRAM - 1);      // This should show where we are in the event range
if (eventcount >= count) return;                                        // Shouldn't be possible

MyGenericDirCtrl* Active = MyFrame::mainframe->GetActivePane();         // So now we know which mount was requested, GoTo it.
if (Active)  Active->OnOutsideSelect(DvdRamMounts.Item(eventcount), false);  // Call 'active' treectrl.  False because otherwise OnOutsideSelect reMounts
}

void DeviceBitmapButton::OnUnMountDvdRam(wxCommandEvent& event)
{
if (ButtonNumber < ID_FLOPPY1 || ButtonNumber >= MAXDEVICE) return;  // Check it's a device
DeviceAndMountManager* pDM = MyFrame::mainframe->Layout->m_notebook->DeviceMan; if (pDM==NULL) return;

DevicesStruct* tempstruct = pDM->deviceman->FindStructForButtonNumber(ButtonNumber); if (tempstruct == NULL) return;
if (!pDM->deviceman->DevInfo->GetDvdRam(tempstruct->devicetype)) return;

wxArrayString mountpts; wxString Mount;
pDM->SearchMtabForStandardMounts(tempstruct->devicenode, mountpts);
size_t count = mountpts.GetCount(); if (!count) return;
if (count==1) Mount = mountpts[0];
 else Mount = wxGetSingleChoice(_("Which mount do you wish to remove?"), _("Unmount a DVD-RAM disc"), mountpts);
if (Mount.IsEmpty()) return;

DevicesStruct datastruct; datastruct.partitions.Add(tempstruct->devicenode); datastruct.mountpts.Add(Mount); datastruct.next=NULL; datastruct.defaultpartition = 0;
wxString mountpt = pDM->DoMount(&datastruct, false, 0, true);
if (!mountpt.IsEmpty())                                        // Empty string flags failure
    MyFrame::mainframe->OnUnmountDevice(mountpt);              // If it worked, go thru all tabs to change any treectrl(s) displaying this mount, to  eg HOME
}

void DeviceBitmapButton::OnEndDrag()      // Used when DragnDrop deposits on one of our toolbar buttons
{
if (!MyGenericDirCtrl::DnDfilecount)                           // If there's nothing in the 'clipboard' abort
  { MyFrame::mainframe->m_TreeDragMutex.Unlock();  return; }
  
if (!(ButtonNumber >= ID_FLOPPY1 && ButtonNumber < MAXDEVICE)) // Confirm it's a device
  { MyFrame::mainframe->m_TreeDragMutex.Unlock();  return; }
  
wxString mountpt = MyFrame::mainframe->Layout->m_notebook->DeviceMan->Mount((size_t)ButtonNumber, true);  //  Mount it
if (mountpt.IsEmpty()) { MyFrame::mainframe->m_TreeDragMutex.Unlock();  return; }

MyGenericDirCtrl::DnDDestfilepath = mountpt;                      // Still here?  Then put the device mount-point in DnDDestfilepath

MyGenericDirCtrl* active = MyFrame::mainframe->GetActivePane();   // Find the active pane, that we're presumably dragging from
if (active == NULL)
  { MyFrame::mainframe->m_TreeDragMutex.Unlock();  return; }

enum myDragResult dragtype = MyFrame::mainframe->ParseMetakeys(); // Find out if we've been moving, copying etc and react accordingly

bool ClusterWasNeeded = UnRedoManager::StartClusterIfNeeded();

switch (dragtype)
 {
  case myDragCopy:      active->OnPaste(true); break;
  case myDragMove:      active->OnDnDMove(); break;
  case myDragHardLink:   
  case myDragSoftLink:  active->OnLink(true, dragtype); break;
  case myDragNone:      break;
   default:             break;
 }
 
if (ClusterWasNeeded && !myDragCopy) UnRedoManager::EndCluster(); // Don't end the cluster if threads are involved; it'll be too soon
 
MyFrame::mainframe->m_TreeDragMutex.Unlock();                    // Release the tree-mutex lock
}

EditorBitmapButton::EditorBitmapButton(wxWindow* dad, size_t which)  : parent(dad)
{
if (which > (MAXEDITOR - ID_EDITOR)) return;

wxConfigBase* config = wxConfigBase::Get(); if (config==NULL)  return;
size_t NoOfEditors = (size_t)config->Read(wxT("/Editors/NoOfEditors"), 0l);
if (which >= NoOfEditors) return;

config->Read(wxT("/Editors/")+CreateSubgroupName(which,NoOfEditors)+wxT("/Editor"), &Label);
config->Read(wxT("/Editors/")+CreateSubgroupName(which,NoOfEditors)+wxT("/LaunchString"), &Launch); if (Launch.IsEmpty())  return;  // because there's not much point!
config->Read(wxT("/Editors/")+CreateSubgroupName(which,NoOfEditors)+wxT("/WorkingDir"), &WorkingDir);
config->Read(wxT("/Editors/")+CreateSubgroupName(which,NoOfEditors)+wxT("/CanUseTabs"), &Multiple, false);
config->Read(wxT("/Editors/")+CreateSubgroupName(which,NoOfEditors)+wxT("/Ignore"), &Ignore, false);

long IconNo = config->Read(wxT("/Editors/")+CreateSubgroupName(which,NoOfEditors)+wxT("/IconNo"), 0l);
long NoOfKnownIcons = config->Read(wxT("/Editors/Icons/NoOfKnownIcons"), 0l);

config->Read(wxT("/Editors/Icons/")+CreateSubgroupName(IconNo,NoOfKnownIcons)+wxT("/Icon"), &Icon);
Icontype = config->Read(wxT("/Editors/Icons/")+CreateSubgroupName(IconNo,NoOfKnownIcons)+wxT("/Icontype"), -1l);  // Can be wxBITMAP_TYPE_PNG, wxBITMAP_TYPE_XPM or wxBITMAP_TYPE_BMP (other handlers aren't loaded)
if (Icontype==-1) return;

wxString path; if ((!Icon.empty()) && Icon.Left(1) != wxFILE_SEP_PATH) path = BITMAPSDIR; // If icon is just a filename, we'll prepend with BITMAPSDIR
if (!wxFileExists(path+Icon))                                             // If it doesn't exist, don't use it
  { if (MyBitmapButton::UnknownButtonNo == wxNOT_FOUND) return;           // Even the unknown icon is unknown :/
    path = BITMAPSDIR;
    Icon = wxT("unknown.xpm"); Icontype = wxBITMAP_TYPE_XPM;
  }
wxBitmap bitmap(path+Icon, (wxBitmapType)Icontype);
if (!bitmap.IsOk()) return;

Create(parent, wxID_ANY, bitmap, wxDefaultPosition, wxDefaultSize, wxNO_BORDER);
ButtonNumber = ID_EDITOR + which;                 // Store the ButtonNumber
SetName(wxT("Editor"));                           // This lets us tell that it's an editor
if (!Label.IsEmpty())  SetToolTip(_("Click or Drag here to invoke ") + Label);

SetIsOk(true);                                    // Success!
}


void EditorBitmapButton::OnButtonClick(wxCommandEvent& WXUNUSED(event))
{
wxString Execute;
PPath_returncodes ans = PrependPathToCommand(WorkingDir, Launch, wxT('\''), Execute, false);
if (ans == Oops) return;
if (ans == unaltered) Execute = QuoteSpaces(Launch);  // Best quote the launch string in case of oddities in its path. If altered, that'll already have happened
wxExecute(Execute);
}

void EditorBitmapButton::OnEndDrag()  // Used when DragnDrop deposits on one of our toolbar Editor buttons
{
if (!MyGenericDirCtrl::DnDfilecount)              // If there's nothing in the 'clipboard' abort
  { MyFrame::mainframe->m_TreeDragMutex.Unlock();  return; }

bool InArchive = false;                           // We might be trying to extract from an archive into this editor
MyGenericDirCtrl* active = MyFrame::mainframe->GetActivePane();
if (active != NULL && active->arcman != NULL && active->arcman->IsArchive())   InArchive = true;

size_t success=0, cantopen=0;                     // so that for multiple selections, we can if necessary report on partial success

if (InArchive)
  { wxArrayString tempfilepaths = active->arcman->ExtractToTempfile(MyGenericDirCtrl::DnDfilearray);  // This extracts this filepath to a temporary file, which it returns
    if (tempfilepaths.IsEmpty()) return;          // Empty == failed
    
    MyGenericDirCtrl::DnDfilearray.Clear();       // Get rid of the internal filepaths, and replace with the tempfiles
    for (size_t n = 0; n < tempfilepaths.GetCount(); ++n)
      { FileData tfp(tempfilepaths[n]); tfp.DoChmod(0444);  // Make the file readonly, as we won't be saving any alterations and exec.ing it is unlikely to be sensible
        MyGenericDirCtrl::DnDfilearray.Add(tempfilepaths[n]);
      }
    MyGenericDirCtrl::DnDfilecount = MyGenericDirCtrl::DnDfilearray.GetCount(); success = MyGenericDirCtrl::DnDfilearray.GetCount();
  }
 else                                // If we're not in an archive
   for (int c = MyGenericDirCtrl::DnDfilecount-1; c >= 0; --c)    // For every origin file
    { FileData* fd = new FileData(MyGenericDirCtrl::DnDfilearray[c]);
      if (fd->IsSymlink())                        // If we're dropping a symlink, change it to its target, as this is surely what would be wanted
        { wxString newpath = fd->GetUltimateDestination();
          delete fd; fd = new FileData(newpath);  // Reconstitute fd with the new target.  We'll check below that it's now a file
        }
      if (!fd->IsRegularFile()                    // If the overall path isn't a file
                        || !fd->CanTHISUserRead()) //  or it's a file we can't read, kill it
        { MyGenericDirCtrl::DnDfilearray.RemoveAt(c);  // Remove the offending origin
          MyGenericDirCtrl::DnDfilecount--;            //  and dec count
          size_t readfail = (fd->IsRegularFile() == true); // Record the reason for the failure
          delete fd;
          if (!MyGenericDirCtrl::DnDfilecount && !success)  // If that leaves nothing to drag, bail out
            { if (readfail)                       // but if the reason was lack of Read permission, first tell the world
                { wxMessageBox(_("I'm afraid you don't have permission to Read this file"),   _("Failed") , wxICON_EXCLAMATION | wxOK); }
              MyFrame::mainframe->m_TreeDragMutex.Unlock();  return;
            }
          cantopen += readfail;
        }
       else ++success;
     }


if (cantopen)                                     // Own up to any failures
  { wxString msg; msg.Printf(wxT("%u of the %u %s"), (uint)cantopen, (uint)(success+cantopen), _("file(s) couldn't be opened due to lack of Read permission"));
    wxMessageBox(msg,  _("Warning"), wxICON_EXCLAMATION | wxOK);
  }

                                    // We have a file (or files) to open.  Let's find out how.
wxString Execute;
PPath_returncodes ans = PrependPathToCommand(WorkingDir, Launch, wxT('\''), Execute, true);
if (ans == Oops) return;
if (ans == unaltered) Execute = QuoteSpaces(Launch);        // Best quote the launch string in case of oddities in the path. If altered, that'll already have happened

if (Multiple)                                               // If this program can cope with multiple files eg GEdit
  for (size_t c=0; c < MyGenericDirCtrl::DnDfilecount; c++) // For every openable file
    { Execute += wxT(" \"");                                //  add a space and open-quote (lest filename contains spaces)
      Execute += MyGenericDirCtrl::DnDfilearray[c];         //  add the filepath to open
      Execute += wxT('\"');
    }
 else  { Execute += wxT(" \""); Execute += MyGenericDirCtrl::DnDfilearray[0]; Execute +=wxT('\"'); }  //  Otherwise just use the first one

if (ans == needsquote) Execute +=wxT('\'');       // If there's a Path, and PrependPathToCommand has added it, we need to close its single-quote

wxExecute(Execute);                               // Finally, launch the editor
 
MyFrame::mainframe->m_TreeDragMutex.Unlock();     // and release the tree-mutex lock
}



BEGIN_EVENT_TABLE(EditorBitmapButton, MyBitmapButton)
  EVT_BUTTON(-1, EditorBitmapButton::OnButtonClick)
END_EVENT_TABLE()
//---------------------------------------------------------------------------------------------------------------------------

GvfsBitmapButton::GvfsBitmapButton(wxWindow* parent, const wxString& mtpt) : m_mountpt(mtpt)
{
static size_t which = 0; // Used to provide an ID number for the buttons

wxString name = m_mountpt.AfterLast(wxFILE_SEP_PATH).BeforeFirst(':');

enum wxBitmapType Icontype(wxBITMAP_TYPE_PNG);
wxString path(BITMAPSDIR);
if (name == "cdda") { path << "/cdda.png"; m_buttonname = "Disc player"; }
 else if (name == "gphoto2") { path << "/gphoto2.png"; m_buttonname = "Camera"; }
 else if (name == "mtp") { path << "/mtp.png"; m_buttonname = "Media device"; }
  else { path << "/unknown.xpm"; Icontype = wxBITMAP_TYPE_XPM;  m_buttonname = "Unknown device"; }

wxCHECK_RET(wxFileExists(path), "Gvfs bitmap button not found");

m_originalMountpt = MyFrame::mainframe->GetActivePane()->GetPathIfRelevant();
if (m_originalMountpt.IsEmpty()) m_originalMountpt = wxGetHomeDir();

wxBitmap bitmap(path, Icontype);
wxCHECK_RET(bitmap.IsOk(), "Failed to create gvfs bitmap button");

Create(parent, wxID_ANY, bitmap, wxDefaultPosition, wxDefaultSize, wxNO_BORDER);
}

void GvfsBitmapButton::OnButtonClick(wxCommandEvent& (event)) 
{
wxString mtp(GetMountpt());
FileData CheckStillExists(mtp);
MyGenericDirCtrl* Active = MyFrame::mainframe->GetActiveTab()->GetActiveDirview();
if (Active && CheckStillExists.IsValid() && Active->startdir != mtp) // Display the gio mount, providing it's still alive, and the active pane isn't already displaying it
  { m_originalMountpt = Active->startdir; // Update the 'original' dir: it might have changed since the button was created
    m_originalSelection = Active->GetPath();
    m_originalFileviewSelection = Active->partner->GetPath();
    Active->OnOutsideSelect(mtp, false);   // Call 'active' treectrl.  False because otherwise OnOutsideSelect reMounts
  }
}

void GvfsBitmapButton::OnUnMount(wxCommandEvent& WXUNUSED(event)) 
{
wxString dest(GetOriginalMountpt());
if (dest.IsEmpty()) dest = wxGetHomeDir();
MyGenericDirCtrl* Active = MyFrame::mainframe->GetActivePane();
if (Active && Active->startdir == GetMountpt())
  { Active->OnOutsideSelect(dest, false);
    if (!m_originalSelection.IsEmpty()) Active->SetPath(m_originalSelection);
    if (!m_originalFileviewSelection.IsEmpty()) Active->partner->SetPath(m_originalFileviewSelection);
  }
else
  { Active = Active->GetOppositeDirview();
    if (Active && Active->startdir == GetMountpt())
      { Active->OnOutsideSelect(dest, false);
        Active->SetPath(m_originalSelection);
        Active->partner->SetPath(m_originalFileviewSelection);
      }
  }
}

void GvfsBitmapButton::OnRightButtonClick(wxContextMenuEvent& WXUNUSED(event))
{
wxMenu menu(m_buttonname + _(" menu"));
menu.Append(ID_MOUNT,  _("Display ") + GetMountpt());
menu.Append(ID_UNMOUNT,  _("Undisplay ") + GetMountpt());

wxPoint pt = ScreenToClient(wxGetMousePosition());
PopupMenu(&menu, pt.x, pt.y);
}

BEGIN_EVENT_TABLE(GvfsBitmapButton, MyBitmapButton)
  EVT_BUTTON(wxID_ANY, GvfsBitmapButton::OnButtonClick)
  EVT_CONTEXT_MENU(GvfsBitmapButton::OnRightButtonClick)
  EVT_MENU(ID_MOUNT, GvfsBitmapButton::OnButtonClick)      // Same as above, but from context menu choice
  EVT_MENU(ID_UNMOUNT, GvfsBitmapButton::OnUnMount)
END_EVENT_TABLE()
//---------------------------------------------------------------------------------------------------------------------------

DeviceInfo::DeviceInfo()
{
devinfo = new ArrayofDevInfoStructs;

if (!Load()) LoadDefaults();                      // If there isn't any stored typeinfo, use defaults
}

bool DeviceInfo::Load()
{
wxConfigBase* config = wxConfigBase::Get();   if (config==NULL)  return false;

long count = config->Read(wxT("Devices/Misc/NoOfDeviceTypes"), 0l);  // How many known devicetypes have info stored?
if (!count) return false;

devinfo->Clear();

wxString Rootname(wxT("Devices/DeviceTypes/"));

for (int n=0; n < count; ++n)
  { wxString subgrp = CreateSubgroupName(n, count);    // Create a subgroup name: "a", "b" etc
    wxString name; config->Read(Rootname+subgrp+wxT("/Name"), &name);
    devicecategory cat = (devicecategory)config->Read(Rootname+subgrp+wxT("/Cat"), (long)harddiskcat);
    treerooticon icon = (treerooticon)config->Read(Rootname+subgrp+wxT("/Icon"), (long)ordinary);
    bool DvdRam; config->Read(Rootname+subgrp+wxT("/CanDoDvdRam"), &DvdRam, false);
    Add(name, cat, icon, DvdRam);
  }

return true;
}

void DeviceInfo::Save()
{
wxConfigBase* config = wxConfigBase::Get();   if (config==NULL)   return;

size_t count = devinfo->GetCount();                 // How many known devicetypes are there?
if (!count) return;
config->Write(wxT("Devices/Misc/NoOfDeviceTypes"), (long)count);

config->DeleteGroup(wxT("Devices/DeviceTypes"));    // Delete previous info

wxString Rootname(wxT("Devices/DeviceTypes/"));
for (size_t n=0; n < count; ++n)
  { wxString subgrp = CreateSubgroupName(n, count); // Create a subgroup name: "a", "b" etc
    config->Write(Rootname+subgrp+wxT("/Name"), GetName(n));
    config->Write(Rootname+subgrp+wxT("/Cat"), (long)GetCat(n));
    config->Write(Rootname+subgrp+wxT("/Icon"), (long)GetIcon(n));
    config->Write(Rootname+subgrp+wxT("/CanDoDvdRam"), GetDvdRam(n));
  }
}

void DeviceInfo::LoadDefaults()
{
devinfo->Clear();

Add(wxT("floppy"), floppycat, floppytype);        // 0
Add(wxT("cdrom"), cdromcat, cdtype);              // 1
Add(wxT("cdr"), cdrcat, cdtype);                  // 2
Add(wxT("dvdrom"), cdromcat, cdtype);             // 3
Add(wxT("dvdrw"), cdrcat, cdtype);                // 4
Add(wxT("dvdram"), cdrcat, cdtype, true);         // 5  The 'true' means it can do dvdram
Add(wxT("usb pen"), usbpencat, usbtype);          // 6
Add(wxT("usb memory"), usbmemcat, usbtype);       // 7
Add(wxT("usb reader"), usbmultcardcat, usbtype);  // 8
}

void DeviceInfo::Add(wxString name, devicecategory cat, treerooticon icon, bool CanDoDvdRam /*=false*/)
{
DevInfo* infostruct = new struct DevInfo;
infostruct->name= name;
infostruct->cat = cat;
infostruct->icon = icon;
infostruct->CanDoDvdRam = CanDoDvdRam;

devinfo->Add(infostruct);
}

bool DeviceInfo::ThisDevicetypeAutomounts(size_t index)
{
enum treerooticon icon = (treerooticon)GetIcon(index); // Note that I'm courageously assessing by icon-type rather than trying to conflate the cat.s  BEWARE IF CHANGES HAPPEN
switch(icon)
  { case ordinary:   return false;                    // I can't imagine any distro automounting every hd partition
    case floppytype: return FLOPPY_DEVICES_AUTOMOUNT;
    case cdtype:     return CDROM_DEVICES_AUTOMOUNT;
    case usbtype:    return USB_DEVICES_AUTOMOUNT;
     default:        return USB_DEVICES_AUTOMOUNT;    // This covers user-defined "unknown" types, which are most likely to be usb-ish
  }
}

enum discoverytable DeviceInfo::GetDiscoveryTableForThisDevicetype(enum treerooticon icon)  // Does this type of device get its data hotplugged into fstab, as opposed to mtab (or nowhere)
{
      // Note that I'm courageously assessing by icon-type rather than trying to conflate the cat.s  BEWARE IF CHANGES HAPPEN
switch(icon)
  { case ordinary:  return TableUnused;                        // Harddisk partitions are irrelevant here
    case floppytype:  if (FLOPPY_DEVICES_SUPERMOUNT)  return FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB==FstabInsert ? FstabSupermount : MtabSupermount;  // If supermounts, won't be NoInsert!
                    else if (FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB==NoInsert) return DoNowt;
                    else return FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB==FstabInsert ? FstabOrdinary : MtabOrdinary;
    case cdtype:      if (CDROM_DEVICES_SUPERMOUNT)  return CD_DEVICES_DISCOVERY_INTO_FSTAB==FstabInsert ? FstabSupermount : MtabSupermount;
                    else if (CD_DEVICES_DISCOVERY_INTO_FSTAB==NoInsert) return DoNowt;
                    else return CD_DEVICES_DISCOVERY_INTO_FSTAB==FstabInsert ? FstabOrdinary : MtabOrdinary;
    case usbtype:    
     default:       if (USB_DEVICES_SUPERMOUNT)  return USB_DEVICES_DISCOVERY_INTO_FSTAB==FstabInsert ? FstabSupermount : MtabSupermount;  //This covers user-defined "unknown" types, which are most likely to be usb-ish
                    else if (USB_DEVICES_DISCOVERY_INTO_FSTAB==NoInsert) return DoNowt;
                    else return USB_DEVICES_DISCOVERY_INTO_FSTAB==FstabInsert ? FstabOrdinary : MtabOrdinary;
  }
}
//---------------------------------------------------------------------------------------------------------------------------

DeviceManager::~DeviceManager()
{
for (int n = (int)DeviceStructArray->GetCount();  n > 0; --n)
  { DevicesStruct* item = DeviceStructArray->Item(n-1); delete item; DeviceStructArray->RemoveAt(n-1); }

delete DeviceStructArray; delete DevInfo;
}

void DeviceManager::LoadDevices()  // Loads device config data and thence their toolbar icons
{
wxBoxSizer* sizerControls =  MyFrame::mainframe->sizerControls;   // Get the device-holding sizer of the panelette that doubles as the 2nd half of the toolbar
if (sizerControls==NULL) return;

wxConfigBase* config = wxConfigBase::Get(); if (config == NULL) return;

LoadMiscData();                                                   // Load the "Does this distro automount"-type data

wxString Rootname(wxT("Devices/ToLoad/"));
long count = config->Read(Rootname + wxT("NoOfDevices"), 0l);     // Get the no of standard devices eg dvdroms, & load them

for (long n=0; n < count; ++n)
  { wxString str; wxString subgrp = CreateSubgroupName(n, count);
    config->Read(Rootname+subgrp+wxT("/Device"), &str, wxEmptyString); if (str.IsEmpty()) continue;     // Load the Device, & check it exists
    struct DevicesStruct* tempstruct = new struct DevicesStruct;  // There is data, so make a new struct for storage
    tempstruct->partitions.Add(str);                              // For these devices there'll only be 1 partition, so put the device-name there for now.  Automounting distros will overwrite it later
    tempstruct->devicenode = str;                                 // Store device name (in the device-types where it matters)
    config->Read(Rootname+subgrp+wxT("/Mountpoint"), &str, wxEmptyString);          // Similarly store mount-point
    tempstruct->mountpts.Add(str);
    config->Read(Rootname+subgrp+wxT("/Name"), &tempstruct->name1, wxEmptyString);  // Store buttontype eg dvdrom
    config->Read(Rootname+subgrp+wxT("/Readonly"), &tempstruct->readonly, 0l);      //  & its readonly status
    config->Read(Rootname+subgrp+wxT("/Devicetype"), &tempstruct->devicetype);      //  & its devicetype
    config->Read(Rootname+subgrp+wxT("/Ignore"), &tempstruct->ignore, false);       //  & whether to ignore it anyway
    
    tempstruct->OriginallyMounted = parent->Checkmtab(tempstruct->mountpts[0]);     // & check if it's already mounted by another program.  If so, don't umount on exit
    tempstruct->IsMounted = tempstruct->OriginallyMounted;        // Set IsMounted.  We toggle this when we mount/umount
    tempstruct->HasButton = false;
    tempstruct->defaultpartition = 0;                             // There's no sense in having a default for a device with removable media, which probably can't be partitioned anyway
    tempstruct->haschild = 0; tempstruct->next = NULL;            // These only pertain to usb in a HAL/sysfs/udev setup, but we still need to zero them

    DeviceStructArray->Add(tempstruct);                           // Store all this in structarray
    
    if (tempstruct->ignore) continue;                             // If we've been told to ignore this device, do so
  }

FirstOtherDevice = DeviceStructArray->GetCount();                 // Set the index that records where in the array fixed devices stop & usb-types start

DisplayFixedDevicesInToolbar();                                   // We've created the buttons, now add them

LoadPossibleUsbDevices();                                         // These are flash-pens etc, devices which may be plugged in at any time
}

//static
void DeviceManager::GetFloppyInfo(ArrayofDeviceStructs* DeviceStructArray, size_t FirstOtherDevice,  // Try to determine what floppy drives are attached
                                          DeviceAndMountManager* parent, bool FromWizard/*=false*/)  // Called by Configure to try to determine what floppy drives are attached
{
        // FLOPPYINFOPATH is probably /sys/block/fd     /sys/block/ should contain a dir for each drive, eg /sys/block/fd0/
wxLogNull WeDontWantMissingDirMsgs;
wxArrayString dirarray, floppyarray;
wxString dirname = wxFindFirstFile(FLOPPYINFOPATH + wxT("*"), wxDIR);  // Get 1st subdir that matches, probably /sys/block/fd0
while (!dirname.IsEmpty())
  { dirarray.Add(dirname);                                      // Add the dir to array
    dirname = wxFindNextFile();                                 // Now try for /sys/block/fd1 etc
  }

for (size_t c=0; c < dirarray.GetCount(); ++c)
  { wxString name = dirarray.Item(c);
    while (name.Right(1) == wxFILE_SEP_PATH && name.Len() > 1)  name.RemoveLast();  // Remove any terminal /
    name = name.AfterLast(wxFILE_SEP_PATH);                     // Make name into fd0 etc
    if (name.Len() < 5) floppyarray.Add(wxT("/dev/") + name);   // Add it to the array if it's not too long (I'll believe fd99 but not fd999 ;)
  }

wxArrayString devicenodes, mountpts; parent->LoadFstabEntries(devicenodes, mountpts);

for (size_t c=0; c < floppyarray.GetCount(); ++c)               // Make a new struct for each unknown drive
  { bool known = false;
    for (size_t n=0; n < DeviceStructArray->GetCount(); ++n)
      { DevicesStruct* temp = DeviceStructArray->Item(n);
        if (floppyarray.Item(c) == temp->devicenode) { known = true; break; }  // This one is already known about
        FileData fd1(temp->devicenode); FileData fd2(floppyarray.Item(c)); 
        if (fd1.GetUltimateDestination() == fd2.GetUltimateDestination()) { known = true; break; }  // In case either is a symlink, dereference & try again
      }
    if (known) continue;
    
    DevicesStruct* tempstruct = new struct DevicesStruct;       // Aha, a new device.  Make a struct & add to the array
    wxString node = floppyarray.Item(c);                        // Put this here as a fall-back, but see if there's a better choice in fstab (probably not for floppies)
    for (size_t d = 0; d < devicenodes.GetCount(); ++d)         // Go thru the fstab devicenodes to see if our device is present, perhaps as a symlink
      { wxString target; bool found = false;
        if (node == devicenodes[d])                             // Found it; not a symlink
          { tempstruct->mountpts.Add(mountpts[d]); break; }     //  so just add the corresponding mountpt
          
        FileData* fd = new FileData(devicenodes[d]);
        while (true)                                            // Try again, walking thru any chain of symlinks
          { if (!fd->IsSymlink()) break;
            target = fd->GetSymlinkDestination();
            if (node == target)
              { node = devicenodes[d]; tempstruct->mountpts.Add(mountpts[d]);  found = true; break; }  // If found, use the symlink name in the tempstruct entry
            delete fd; fd = new FileData(target);               // If not found, loop so long as target is itself a symlink
          }
        delete fd;
        if (found) break;
        
                      // If not found above, see if there's a supermounted floppy (which usually means the fstab entry starts with 'none')
        wxArrayString smdevicenodes, smmountpts; parent->CheckSupermountTab(smdevicenodes, smmountpts, node, false);
        if (smmountpts.GetCount())                              // If successful, mountpts will hold one mountpt
          { tempstruct->mountpts.Add(smmountpts[0]);break; }
      }
    
    tempstruct->devicenode = node;
    tempstruct->devicetype = 0;
    tempstruct->defaultpartition = 0; 
    tempstruct->readonly = false;
    
    if (FromWizard)           // If this is from the initial wizard, we don't want any user input. Just assume we're right ;)
      { tempstruct->name1 = _("Floppy"); if (floppyarray.GetCount() > 1) tempstruct->name1 << wxChar(wxT('A') + c);  // For only 1 device, call it Floppy, otherwise FloppyA etc
        // It's now 2013, and floppies are 1) rarely seen in the wild, and 2) aren't supported out of the box in recent distro versions
        // So rather than displaying an icon that won't work, by default ignore it. The user can always unignore if he wishes.
        tempstruct->ignore = 1;
        DeviceStructArray->Insert(tempstruct, FirstOtherDevice++); continue;  // Insert into the array, at the end of the fixed-devices bit
      }

    MyButtonDialog dlg;
    wxXmlResource::Get()->LoadDialog(&dlg, MyFrame::mainframe->GetActivePane(), wxT("NewDeviceDetectedDlg"));
    wxString title = node.AfterLast(wxFILE_SEP_PATH);
    dlg.SetTitle(title);
    int ans = dlg.ShowModal();
    if (ans == wxID_OK) parent->ConfigureNewFixedDevice(tempstruct);  // Get user input for the name, and to allow editing
      else if (ans == wxID_CANCEL) tempstruct->ignore = DS_true;
            else tempstruct->ignore = DS_defer;
    
    DeviceStructArray->Insert(tempstruct, FirstOtherDevice++); // Insert into the array, at the end of the fixed-devices bit
  }
}

//static
bool DeviceManager::GetCdromInfo(ArrayofDeviceStructs* DeviceStructArray, size_t FirstOtherDevice,   // Try to determine what cd/dvdrom/r/ram devices are attached
                                                          DeviceAndMountManager* parent, bool FromWizard/*=false*/)
{
#ifdef __GNU_HURD__ 
  return false; // There's no point doing this in hurd atm, as the relevant file doesn't exist
#endif

wxLogNull WeDontWantMissingDirMsgs;                     // CDROMINFO is probably /proc/sys/dev/cdrom/info
FileData fd(CDROMINFO); if (!fd.IsValid()) return false; // Don't shout if it doesn't exist, though: nowadays not everyone has a dvd reader, or not a permanently-attached one

wxFileInputStream file_input(CDROMINFO);                // I'd originally used wxTextFile here, but in 2.7.0-8.1 this won't work with non-seekables
wxBufferedInputStream input(file_input);                // Without the buffer, only 1 byte gets read !?
wxTextInputStream text(input);
wxArrayString array;
for (; ;)
  { wxString line = text.ReadLine();
    if (input.Eof() && line.IsEmpty()) break;
    array.Add(line);
  }

size_t count = array.GetCount(); if (!count) return false;

size_t n; int NumDrives=0;
for (n=0; n < count; ++n)
  { wxString line = array[n];
    wxStringTokenizer tkz(line, wxT(":"));
    if (tkz.HasMoreTokens())                            // Make sure it's not blank!
      { wxString label = tkz.GetNextToken();            // Get first token. It should be "CD-ROM information, Id". We don't want this, we want the next: "drive name"
        if (label.find(wxT("drive name")) != wxString::npos) 
          { wxStringTokenizer tkno(tkz.GetString());    // Tokenise using white space the rest of the line
            NumDrives = tkno.CountTokens();  break;     // This gives the number of data columns, 1 for each drive
          }
      }
  }  
  
if (!NumDrives) return false;                           // Unlikely, but just in case there are no drives

struct datastruct                                       // Define a struct for temporary storage, and make an array of them
  { wxString name; bool cdr; bool dvd; bool dvdr; bool dvdram; };
datastruct* dataarray = new datastruct[NumDrives];

for (; n < count; ++n)                                  // We've now skipped the rubbish. Start again at the "drive name" line
  { wxString line = array[n];
    wxStringTokenizer tkz(line, wxT(":"));
    if (!tkz.HasMoreTokens())  continue;
    
    wxArrayString DriveData;
    wxString label = tkz.GetNextToken();                // Each loop, get the 1st, label, token
    wxStringTokenizer tkno(tkz.GetString());
    
    if (label.find(wxT("drive name")) != wxString::npos) // Then try to match it.
      for (int c=0; c < NumDrives; ++c) 
        { wxString nodule = tkno.GetNextToken(); if (nodule.IsEmpty()) continue;  // Usually hdb etc, but if recent kernel, could be sr0 etc
          wxString node = DEVICEDIR + nodule; FileData stat(node);  // DEVICEDIR will be /dev/
          if (!stat.IsValid())
            { node = DEVICEDIR + wxT("cdrom-"); node << nodule;      // In Fedora7, nodule is sr0, but the node needs 2b /dev/cdrom-sr0
              FileData stat(node); if (!stat.IsValid()) continue;
              if (stat.IsSymlink()) node = stat.GetSymlinkDestination();  // As fstab is more likely to contain this
            }
          (dataarray+c)->name = node;                     // When we have sorted this out, temporarily store each drive's data
        }
    
    else if (label.find(wxT("write CD-R")) != wxString::npos)
      for (int c=0; c < NumDrives; ++c) 
        (dataarray+c)->cdr = (tkno.GetNextToken() == wxT("1"));
    
    else if (label.find(wxT("read DVD")) != wxString::npos)  
      for (int c=0; c < NumDrives; ++c) 
        (dataarray+c)->dvd = (tkno.GetNextToken() == wxT("1"));    
    
    else if (label.find(wxT("DVD-RAM")) != wxString::npos)  // Must do this before "write DVD-R", as otherwise that'll match
      for (int c=0; c < NumDrives; ++c) 
        (dataarray+c)->dvdram = (tkno.GetNextToken() == wxT("1"));
    
    else if (label.find(wxT("write DVD-R")) != wxString::npos)  
      for (int c=0; c < NumDrives; ++c) 
        (dataarray+c)->dvdr = (tkno.GetNextToken() == wxT("1"));

  }

wxArrayString devicenodes, mountpts; DeviceAndMountManager::LoadFstabEntries(devicenodes, mountpts);

for (int c=0; c < NumDrives; ++c)                       // Make a new struct for each drive
  { bool known = false; wxString nodetarget;
    FileData fdarray((dataarray+c)->name); if (fdarray.IsSymlink())  nodetarget = fdarray.GetUltimateDestination();
    for (size_t n=0; n < DeviceStructArray->GetCount(); ++n)
      { DevicesStruct* temp = DeviceStructArray->Item(n);
        if ((dataarray+c)->name == temp->devicenode) { known = true; break; }  // This one is already known about
        FileData fd1(temp->devicenode);
        if (fd1.GetUltimateDestination() == nodetarget) { known = true; break; }  // In case either is a symlink, dereference & try again
      }
    if (known) continue;
    
    DevicesStruct* tempstruct = new struct DevicesStruct; // Aha, a new device.  Make a struct & add to the array
    wxString node = (dataarray+c)->name;                // Put this here as a fall-back, but see if there's a better choice in fstab
    for (size_t d = 0; d < devicenodes.GetCount(); ++d) // Go thru the fstab devicenodes to see if our device is present, perhaps as a symlink
      { wxString target; bool found = false;
        if (node == devicenodes[d] || nodetarget == devicenodes[d])  // Found it; allowing for symlinks
          { node = devicenodes[d]; tempstruct->mountpts.Add(mountpts[d]); break; }  //  so just add the corresponding mountpt
          
        FileData* fd = new FileData(devicenodes[d]);
        while (true)                                    // Try again, walking thru any chain of symlinks
          { if (!fd->IsSymlink()) break;
            target = fd->GetSymlinkDestination();
            if (node == target || nodetarget == target) // If we have a match, including any symlink stuff
              { node = devicenodes[d]; tempstruct->mountpts.Add(mountpts[d]);  found = true; break; }  // If found, use the fstab name in the tempstruct entry
            delete fd; fd = new FileData(target);       // If not found, loop so long as target is itself a symlink
          }
        delete fd;
        if (found) break;
      }
    
    tempstruct->devicenode = node;
    
    if ((dataarray+c)->dvdram)  tempstruct->devicetype = 5;
     else if ((dataarray+c)->dvdr)  tempstruct->devicetype = 4;
     else if ((dataarray+c)->dvd)  tempstruct->devicetype = 3;
     else if ((dataarray+c)->cdr)  tempstruct->devicetype = 2;
     else tempstruct->devicetype = 1;                   // Plain vanilla cdrom

    tempstruct->defaultpartition = 0; 
    tempstruct->readonly = true;                        // because we can't write to cdr/rw etc. DVD-RAM is done separately
    
    if (FromWizard)                                     // If this is from the initial wizard, we don't want any user input. Just assume we're right ;)
      { tempstruct->name1 = tempstruct->devicenode.AfterLast(wxFILE_SEP_PATH); 
        DeviceStructArray->Insert(tempstruct, FirstOtherDevice++); continue; // Insert into the array, at the end of the fixed-devices bit
      }
    
    MyButtonDialog dlg;
    wxXmlResource::Get()->LoadDialog(&dlg, MyFrame::mainframe->GetActivePane(), wxT("NewDeviceDetectedDlg"));
    wxString title = node.AfterLast(wxFILE_SEP_PATH);
    dlg.SetTitle(title);
    int ans = dlg.ShowModal();
    if (ans == wxID_OK) parent->ConfigureNewFixedDevice(tempstruct);  // Get user input for the name, and to allow editing
      else if (ans == wxID_CANCEL) tempstruct->ignore = DS_true;
            else tempstruct->ignore = DS_defer;
    
    DeviceStructArray->Insert(tempstruct, FirstOtherDevice++); // Insert into the array, at the end of the fixed-devices bit
  }

delete[] dataarray;
return true;
}

void DeviceManager::DisplayFixedDevicesInToolbar()  // These are stored in a separate sizer within the panelette sizer, for ease of recreation
{
wxPanel* panel = MyFrame::mainframe->panelette; if (panel == NULL) return;
wxSizer* fdsizer = MyFrame::mainframe->FixedDevicesSizer; if (fdsizer == NULL) return;
            // We need to start by deleting any existing buttons, in case we're here from config & we're about to recreate
wxWindowList::compatibility_iterator node = panel->GetChildren().GetLast(); // Start at the top, since we're deleting
while (node)                                            // Iterate through the list of the sizer's children
  { wxWindow* win = node->GetData();
    if (fdsizer->GetItem(win) != NULL) win->Destroy();  // If this window is found in the fixed-devices sizer, destroy it. If not, let it live
    node = node->GetPrevious();
  }

for (size_t n=0; n < FirstOtherDevice; ++n)             // Grab all the fixed devices in the struct
  { DevicesStruct* tempstruct = DeviceStructArray->Item(n);
    if (tempstruct == NULL || tempstruct->ignore == DS_true) continue;  // If we've just turned on the Ignore flag in Edit(), this ensures we don't reload the button
    DeviceBitmapButton* button = new DeviceBitmapButton(DevInfo->GetCat(tempstruct->devicetype));  // If OK, create the button
    button->SetName(wxT("Device"));                     // Specify the name, so we can tell that it's a device
    tempstruct->buttonnumber= GetUniqueDeviceButtonNo();
    tempstruct->button = button;                        // Store a ptr to the button, so that it can be located for removal when detached
    button->ButtonNumber = tempstruct->buttonnumber;    // Finally, synchronise struct & button's no
    
    fdsizer->Add(tempstruct->button, 0, wxEXPAND  | wxLEFT, 5); // Add to fixed-devices sizer
  }

MyFrame::mainframe->sizerControls->Layout();
}

void DeviceManager::SaveFixedDevices()  // Saves cdrom etc data
{
wxConfigBase* config = wxConfigBase::Get(); if (config==NULL) return;

config->DeleteGroup(wxT("Devices/ToLoad"));             // Out with the old . . .

wxString Rootname(wxT("Devices/ToLoad/"));
size_t count = FirstOtherDevice;                        // We don't want to save usb devices here

for (size_t n = 0; n < count; ++n)                      // Go thru the list of cdrom drives etc
  { DevicesStruct* tempstruct = FindStructForDevice(n);
    if (tempstruct == NULL) continue;
    if (tempstruct->ignore == DS_defer) continue;       // If we've chosen not to configure the device at this time, don't save its (limited) data
    
    wxString subgrp = CreateSubgroupName(n, count);

    config->Write(Rootname+subgrp+wxT("/Device"), tempstruct->devicenode);
    config->Write(Rootname+subgrp+wxT("/Name"), tempstruct->name1);
    wxString mtpt; if (tempstruct->mountpts.GetCount()) mtpt = tempstruct->mountpts[0];
    config->Write(Rootname+subgrp+wxT("/Mountpoint"), mtpt); // Save any fstab mountpt
    config->Write(Rootname+subgrp+wxT("/Readonly"), tempstruct->readonly);
    config->Write(Rootname+subgrp+wxT("/Devicetype"), tempstruct->devicetype);
    config->Write(Rootname+subgrp+wxT("/Ignore"), tempstruct->ignore);  // If true, don't actually use this one
  }
  
config->Write(Rootname + wxT("NoOfDevices"), (long)count);  // How many to reload next time
config->Flush();
}

void DeviceManager::LoadMiscData()  // Loads misc device config data
{
wxConfigBase* config = wxConfigBase::Get(); if (config==NULL)  return;
config->Read(wxT("Devices/Misc/FloppyDevicesAutomount"), &FLOPPY_DEVICES_AUTOMOUNT, false);  // Some distros (eg SuSE >9.0) automount certain types of devices
config->Read(wxT("Devices/Misc/CDRomDevicesAutomount"), &CDROM_DEVICES_AUTOMOUNT, false);
config->Read(wxT("Devices/Misc/UsbDevicesAutomount"), &USB_DEVICES_AUTOMOUNT, false);
config->Read(wxT("Devices/Misc/FloppyDevicesSupermount"), &FLOPPY_DEVICES_SUPERMOUNT, false);// Some distros (eg Mandriva 10.1) use supermount
config->Read(wxT("Devices/Misc/CDRomDevicesSupermount"), &CDROM_DEVICES_SUPERMOUNT, false);
config->Read(wxT("Devices/Misc/UsbDevicesSupermount"), &USB_DEVICES_SUPERMOUNT, false);
DEVICE_DISCOVERY_TYPE = (enum DiscoveryMethod)config->Read(wxT("Devices/Misc/DeviceDiscoveryType"), 2l);
FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB = (TableInsertMethod)config->Read(wxT("Devices/Misc/FloppyDevicesInFstab"), 0l);
CD_DEVICES_DISCOVERY_INTO_FSTAB = (TableInsertMethod)config->Read(wxT("Devices/Misc/CDDevicesInFstab"), 0l);
USB_DEVICES_DISCOVERY_INTO_FSTAB = (TableInsertMethod)config->Read(wxT("Devices/Misc/UsbDevicesInFstab"), 0l);
config->Read(wxT("Devices/Misc/SuppressEmptyUsbDevices"), &SUPPRESS_EMPTY_USBDEVICES, true);
config->Read(wxT("Devices/Misc/ASK_BEFORE_UMOUNT_ON_EXIT"), &ASK_BEFORE_UMOUNT_ON_EXIT);
long time = config->Read(wxT("Devices/Misc/CHECK_USB_TIMEINTERVAL"), 0l); if (time) CHECK_USB_TIMEINTERVAL = (size_t)time;

DevicesAdvancedPanel::LoadConfig();    // This does some other stuff, which is configured in the AdvancedDialog, so encapsulated there
}

//static
void DeviceManager::SaveMiscData(wxConfigBase* config /*= NULL*/)  // Saves misc device config data
{
if (config==NULL)                                              // It'll usually be null, the exception being if coming from ConfigureMisc::OnExportBtn
   config = wxConfigBase::Get();  if (config==NULL)  return; 

config->Write(wxT("Devices/Misc/FloppyDevicesAutomount"), FLOPPY_DEVICES_AUTOMOUNT);    // Some distros (eg SuSE >9.0) automount certain types of devices
config->Write(wxT("Devices/Misc/CDRomDevicesAutomount"), CDROM_DEVICES_AUTOMOUNT);
config->Write(wxT("Devices/Misc/UsbDevicesAutomount"), USB_DEVICES_AUTOMOUNT);
config->Write(wxT("Devices/Misc/FloppyDevicesSupermount"), FLOPPY_DEVICES_SUPERMOUNT);  // Some distros (eg Mandriva 10.1) use supermount
config->Write(wxT("Devices/Misc/CDRomDevicesSupermount"), CDROM_DEVICES_SUPERMOUNT);
config->Write(wxT("Devices/Misc/UsbDevicesSupermount"), USB_DEVICES_SUPERMOUNT);
config->Write(wxT("Devices/Misc/DeviceDiscoveryType"), (long)DEVICE_DISCOVERY_TYPE);
config->Write(wxT("Devices/Misc/FloppyDevicesInFstab"), (long)FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB);
config->Write(wxT("Devices/Misc/CDDevicesInFstab"), (long)CD_DEVICES_DISCOVERY_INTO_FSTAB);
config->Write(wxT("Devices/Misc/UsbDevicesInFstab"), (long)USB_DEVICES_DISCOVERY_INTO_FSTAB);

config->Write(wxT("Devices/Misc/SuppressEmptyUsbDevices"), SUPPRESS_EMPTY_USBDEVICES);

config->Flush();
}

void DeviceManager::LoadPossibleUsbDevices()    // These are flash-pens etc, devices which may be plugged in at any time
{
wxConfigBase* config = wxConfigBase::Get(); if (config == NULL)  return;

long count = config->Read(wxT("Devices/Misc/NoOfKnownUsbDevices"), (long)0); // Load the no of known usb devices

for (long n=0; n < count; ++n)
  { wxString Rootname(wxT("Devices/Other/")); wxString subgrp = CreateSubgroupName(n, count);
    
    struct DevicesStruct* tempstruct = new struct DevicesStruct;       // There is data, so make a new struct for storage
      // Load the other strings that scsi will use to identify the device, though nowadays we pay more attention to idVendor and idProduct
    config->Read(Rootname+subgrp+wxT("/Name1"), &tempstruct->name1);   // The vendor (which may occasionally be empty!)
    config->Read(Rootname+subgrp+wxT("/Name2"), &tempstruct->name2);   // The model name
    config->Read(Rootname+subgrp+wxT("/Name3"), &tempstruct->name3);   // I don't think this is used nowadays
    config->Read(Rootname+subgrp+wxT("/Vendorstr"), &tempstruct->Vendorstr); // These are the user-visible (and adjustable) strings
    config->Read(Rootname+subgrp+wxT("/Productstr"), &tempstruct->Productstr);
    tempstruct->idVendor = (int)config->Read(Rootname+subgrp+wxT("/idVendor"), 0l); // The default of 0 means unknown
    tempstruct->idProduct = (int)config->Read(Rootname+subgrp+wxT("/idProduct"), 0l);
    if (tempstruct->Vendorstr.empty()) tempstruct->Vendorstr = tempstruct->name1;
    if (tempstruct->Productstr.empty()) tempstruct->Vendorstr = tempstruct->name2;
        
    config->Read(Rootname+subgrp+wxT("/Readonly"), &tempstruct->readonly);     // Store its readonly status
    config->Read(Rootname+subgrp+wxT("/Devicetype"), &tempstruct->devicetype); //  & its devicetype
    config->Read(Rootname+subgrp+wxT("/Ignore"), &tempstruct->ignore, false);  //  & whether to ignore it anyway
    config->Read(Rootname+subgrp+wxT("/DefaultPartition"), &tempstruct->defaultpartition, 0l);  // For multi-partition devices, which one is default

    tempstruct->IsMounted = false;                              // Reset IsMounted.  We toggle this when we mount/umount
    tempstruct->HasButton = false;
    tempstruct->haschild = 0; tempstruct->next = NULL;          // These only pertain in a HAL/sysfs/udev setup

    tempstruct->buttonnumber = GetUniqueDeviceButtonNo();       // Finally, set a no. to id the struct & its button
  
    DeviceStructArray->Add(tempstruct);                         // Store all this in structarray
  }
}

void DeviceManager::SaveUsbDevices()    // These are flash-pens etc, devices which may be plugged in at any time
{
wxConfigBase* config = wxConfigBase::Get(); if (config == NULL)   return;

config->DeleteGroup(wxT("Devices/Other"));                    // Out with the old . . .

wxString Rootname(wxT("Devices/Other/"));
size_t count = DeviceStructArray->GetCount();
for (size_t n = FirstOtherDevice; n < count; ++n)             // Go thru the list of usb devices
  { DevicesStruct* tempstruct = FindStructForDevice(n);
    if (tempstruct == NULL) continue;
    if (tempstruct->ignore == DS_defer) continue;             // If we've chosen not to configure the device at this time, don't save its (limited) data
    
    wxString subgrp = CreateSubgroupName(n - FirstOtherDevice, count), str;

    config->Write(Rootname+subgrp+wxT("/Device"), tempstruct->devicenode);
    config->Write(Rootname+subgrp+wxT("/idVendor"), (long)tempstruct->idVendor);
    config->Write(Rootname+subgrp+wxT("/idProduct"), (long)tempstruct->idProduct);
    config->Write(Rootname+subgrp+wxT("/Name1"), tempstruct->name1);
    config->Write(Rootname+subgrp+wxT("/Name2"), tempstruct->name2);
    config->Write(Rootname+subgrp+wxT("/Name3"), tempstruct->name3);
    config->Write(Rootname+subgrp+wxT("/Vendorstr"), tempstruct->Vendorstr);
    config->Write(Rootname+subgrp+wxT("/Productstr"), tempstruct->Productstr);
    config->Write(Rootname+subgrp+wxT("/Readonly"), tempstruct->readonly);
    config->Write(Rootname+subgrp+wxT("/Devicetype"), tempstruct->devicetype);
    config->Write(Rootname+subgrp+wxT("/Ignore"), tempstruct->ignore);  // If true, don't actually use this one
  }
  
config->Write(wxT("Devices/Misc/NoOfKnownUsbDevices"), (long)(count - FirstOtherDevice));  // How many to reload next time
config->Flush();
}

void DeviceManager::SetupConfigureFixedorUsbDevices(ArrayofDeviceStructs& TempDeviceStructArray)    // From Configure
{
GetFloppyInfo(DeviceStructArray, FirstOtherDevice, parent); GetCdromInfo(DeviceStructArray, FirstOtherDevice, parent);   // See what we can find from sys & proc & fstab

for (size_t n=0; n < DeviceStructArray->GetCount(); ++n)
  { DevicesStruct* tempstruct = new struct DevicesStruct(*DeviceStructArray->Item(n));
    TempDeviceStructArray.Add(tempstruct);
  }
}

void DeviceManager::ReturnFromConfigureFixedorUsbDevices(const ArrayofDeviceStructs& TempDeviceStructArray, const ConfigDeviceType whichtype)  // Stores results of SetupConfigureFixedorUsbDevices()
{
for (int n = (int)DeviceStructArray->GetCount();  n > 0; --n)               // First clear the old data
       { DevicesStruct* item = DeviceStructArray->Item(n-1); delete item; DeviceStructArray->RemoveAt(n-1); }

for (size_t n=0; n < TempDeviceStructArray.GetCount(); ++n)                 // Then replace with each element of the new data
  { DevicesStruct* tempstruct = new struct DevicesStruct(*TempDeviceStructArray.Item(n));
    DeviceStructArray->Add(tempstruct);
  }

if (whichtype == CDT_fixed)
  { SaveFixedDevices();
    DisplayFixedDevicesInToolbar();                                         // Delete old buttons and replace with new set
  }
 else
   SaveUsbDevices();
}

void DeviceManager::Add(DevicesStruct* newitem)
{
if (newitem == NULL) return;

for (size_t n = 0; n < DeviceStructArray->GetCount(); ++n)        // Go thru the list of devices
  { DevicesStruct* tempstruct = DeviceStructArray->Item(n);
    if (tempstruct == NULL) continue;
    
    if (DevicesMatch(tempstruct, newitem))                        // If we've found a device with identical names, add newitem as a child
      { newitem->next = tempstruct->next;                         // Do this by inserting it as the first child, transferring any current progeny to itself
        newitem->haschild = tempstruct->haschild;                 // Therefore the haschild bool also need to be transferred
        tempstruct->next = newitem; tempstruct->haschild = true;  // Do the actual attachment.  Of course the parent is now with child
        return;
      }
  }

newitem->haschild = false; newitem->next = NULL; DeviceStructArray->Add(newitem);  // If we're here, there wasn't already a device with this name, so just append to array
}

bool DeviceManager::DevicesMatch(DevicesStruct* item1, DevicesStruct* item2)  // See if the names of items 1 & 2 match
{
if (item1 == NULL  || item2 == NULL) return false;

//if (DevInfo->ThisDevicetypeAutomounts(item1->devicetype)) return false;  // If this devicetype automounts, forget it**** THIS WRONG but ? should be IF NOT AUTOMOUNTS ****
return (item1->name1 == item2->name1 && item1->name2 == item2->name2 && item1->name3 == item2->name3);
}

DevicesStruct* DeviceManager::FindStructForDevice(size_t index) // Returns DeviceStructArray-entry after errorcheck
{
size_t count = DeviceStructArray->GetCount();
if (index >= count) return NULL;
  
return DeviceStructArray->Item(index);
}

DevicesStruct* DeviceManager::GetNextDevice(size_t& count, size_t& child)  // Iterates thru DeviceStructArray returning the next struct, but sidetracking down any children
{
DevicesStruct* tempstruct = FindStructForDevice(count); // First ensure the main struct exists
if (tempstruct == NULL) return NULL;

if (tempstruct->haschild)                            // If there is a child struct,
  { if (child > 0)                                   //   & we want to know, iterate down until we get there
      for (size_t n=0; n < child; ++n)
        { tempstruct = tempstruct->next; if (tempstruct == NULL) return NULL;  // always assuming that the particular child exists
          if (!tempstruct->haschild && (n+1) < child)  return NULL;            // Ditto
        }
              // However we've reached here, tempstruct now points to the desired struct, main or child
    if (tempstruct->haschild) ++child;              // If there will be a further child available, inc ready for next iteration
      else { child=0; ++count; }                    // If not, zero & inc count instead
  }
 else                                               // If there isn't a child struct
   { if (child > 0)  return NULL;                   // If we were specifically asked about a child which doesn't exist, return null
    ++count;                                        // We've found the required struct, there aren't any progeny problems, so inc count ready for next iteration
  }

return tempstruct;
}

DevicesStruct* DeviceManager::FindStructForButtonNumber(int buttonnumber)  // Returns DeviceStructArray item with the passed buttonnumber
{
size_t count=0, child=0; DevicesStruct* tempstruct;

while ((tempstruct = GetNextDevice(count, child)) != NULL) // Use GetNextDevice() as it searches children too.  It autoincs the two indices
  if (tempstruct->buttonnumber == buttonnumber) return tempstruct;
  
return NULL;  // If we're here, GetNextDevice() ran out of devices without finding a match
}

//---------------------------------------------------------------------------------------------------------------------------
wxArrayString DeviceAndMountManager::RealFilesystemsArray;

DeviceAndMountManager::DeviceAndMountManager()
{
deviceman = new DeviceManager(this);
m_gvfsinfo = new GvfsInfo();
PartitionArray = new ArrayofPartitionStructs;

BLKID = wxT("/sbin/blkid");  // See if blkid is installed. It should be, and most likely here
if (!wxFileExists(BLKID))
  { BLKID.Clear();
    if (wxFileExists(wxT("/usr/bin/blkid"))) BLKID = wxT("/usr/bin/blkid");
     else
      { wxArrayString output, errors;long ans;
        if (wxGetOsDescription().Contains(wxT("FreeBSD"))) // FreeBSD's forkpty() hangs
          ans = wxExecute(wxT("which blkid"), output, errors);
         else
          ans = ExecuteInPty(wxT("which blkid"), output, errors);
        if (!ans && (output.GetCount() > 0)) { BLKID = output.Item(0); }
      }
  }

LSBLK = wxT("/bin/lsblk");  // Similarly lsblk
if (!wxFileExists(LSBLK))
  { LSBLK.Clear();
    if (wxFileExists(wxT("/sbin/lsblk"))) LSBLK = wxT("/sbin/lsblk");
     else
      { wxArrayString output, errors; long ans;
        if (wxGetOsDescription().Contains(wxT("FreeBSD")))
          ans = wxExecute(wxT("which lsblk"), output, errors);
         else
          ans = ExecuteInPty(wxT("which lsblk"), output, errors);
        if (!ans && (output.GetCount() > 0)) { LSBLK = output.Item(0); }
      }
  }

#ifndef __GNU_HURD__   // which, atm, doesn't have these
FINDMNT = wxT("/bin/findmnt");  // Similarly findmnt
if (!wxFileExists(FINDMNT))
  { FINDMNT.Clear();
    if (wxFileExists(wxT("/sbin/findmnt"))) FINDMNT = wxT("/sbin/findmnt");
     else
      { wxArrayString output, errors;
        long ans = wxExecute(wxT("which findmnt"), output, errors);
        if (!ans && (output.GetCount() > 0)) { FINDMNT = output.Item(0); }
      }
  }

FUSERMOUNT = wxT("/bin/fusermount");  // Similarly fusermount
if (!wxFileExists(FUSERMOUNT))
  { FINDMNT.Clear();
    if (wxFileExists(wxT("/sbin/fusermount"))) FUSERMOUNT = wxT("/sbin/fusermount");
     else
      { wxArrayString output, errors;
        long ans = wxExecute(wxT("which fusermount"), output, errors);
        if (!ans && (output.GetCount() > 0)) { FUSERMOUNT = output.Item(0); }
      }
  }
#endif

// These are plausible 1st 3 letters of DVD-RAM filesystems
const wxChar* DvdRamFilesystems[] = { wxT("aut"), wxT("ext"), wxT("rei"), wxT("xfs"), wxT("btr"), wxT("jfs") }; 
for (size_t n=0; n < sizeof(DvdRamFilesystems)/sizeof(wxChar*); ++n)      // Transfer them to wxArrayString for ease of use & addition
  RealFilesystemsArray.Add(DvdRamFilesystems[n]);
  
deviceman->LoadDevices();

usbtimer = new MyUsbDevicesTimer; usbtimer->Init(this);                   // Set up the timer that checks for usb drives
usbtimer->Start(CHECK_USB_TIMEINTERVAL * 1000, wxTIMER_CONTINUOUS);

#if wxVERSION_NUMBER < 3000
    m_MtabpollTimer = new MtabPollTimer(this);                            // The timer that polls for a successful sshfs mount
#endif
}

DeviceAndMountManager::~DeviceAndMountManager()  // Apart from cleaning up, this unmounts any devices that we mounted, & then forgot to unmount
{
size_t count=0, child=0; DevicesStruct* item;
while ((item = deviceman->GetNextDevice(count, child)) != NULL)         // Use GetNextDevice() as it searches children too.  It autoincs the two indices
  if (!deviceman->DevInfo->ThisDevicetypeAutomounts(item->devicetype))   // If this item doesn't automount
      { if (deviceman->DevInfo->HasPartitions(item->devicetype))         // For partitioned devices eg usbpens
          { if (item->IsMounted && !(item->OriginallyMounted && item->defaultpartition==0) && !item->mountpts.IsEmpty())  // If WE mounted this device, and then forgot to unmount
              { int ans=0;
                if (ASK_BEFORE_UMOUNT_ON_EXIT && item->defaultpartition==0)        // Only ask if it's partition 0 that's mounted, as that's the only one that OriginallyMounted checks
                  { wxString msg; msg.Printf(wxT("Device %s is still mounted at %s\n   Do you want to unmount it?"), item->partitions[0].c_str(), item->mountpts[0].c_str());
                    ans = wxMessageBox(msg, wxEmptyString, wxYES_NO | wxICON_QUESTION);
                  }
                if (ans==wxYES || !ASK_BEFORE_UMOUNT_ON_EXIT || item->defaultpartition>0)  // If we asked & answer was Yes, or we didn't need to ask,
                      wxExecute(wxT("umount ") + item->mountpts[item->defaultpartition]);
              }
          }
         else                                                               // For devices which can't be partitioned eg floppies, cdroms
          if (item->IsMounted && !item->OriginallyMounted && !item->mountpts.IsEmpty())
            wxExecute(wxT("umount ") + item->mountpts[item->defaultpartition]);  // If WE mounted this device, and then forgot to unmount
      }


delete deviceman;                                      // Delete the arrays
for (int n = (int)PartitionArray->GetCount();  n > 0; --n)  { PartitionStruct* item = PartitionArray->Item(n-1); delete item; PartitionArray->RemoveAt(n-1); }
delete PartitionArray;
delete usbtimer;
delete m_gvfsinfo;

#if wxVERSION_NUMBER < 3000
    delete m_MtabpollTimer;
#endif
}

wxString DeviceAndMountManager::Mount(size_t whichdevice, bool OnOff)
{
wxString mountpt;

DevicesStruct* tempstruct = deviceman->FindStructForButtonNumber(whichdevice);  // First find which struct has the data for this device
if (tempstruct==NULL) {wxMessageBox(_("Oops, that drive doesn't seem to be available.\n\n            Have a nice day!")); return wxEmptyString; }

return DoMount(tempstruct, OnOff);
}

wxString DeviceAndMountManager::DoMount(DevicesStruct* tempstruct, bool mounting, int partition /*= -1*/, bool DvdRam /*=false*/)    // Mounts or Unmounts the device in tempstruct
{
if (tempstruct->partitions.GetCount() == 0) return wxEmptyString; // Shouldn't happen, but if there is no partition data, abort as 1) it won't do anything useful, & 2) It'll crash!

if (partition == -1) partition = tempstruct->defaultpartition;    // If no different partition was requested, use the default one --- usually 0 (and always 0 for single-partition devices!)
wxString devicenode = tempstruct->partitions[partition];          // Put device name in local string
wxString mountpt = tempstruct->mountpts[partition];               // Ditto its designated mountpt (if not automounting)
if (devicenode.IsEmpty()) { wxMessageBox(_("Oops, that partition doesn't seem to be available.\n\n            Have a nice day!")); return wxEmptyString; }

if ((!DvdRam) && deviceman->DevInfo->ThisDevicetypeAutomounts(tempstruct->devicetype))  // DvdRam ext2 partitions don't automount
  return WhereIsDeviceMounted(devicenode, tempstruct->devicetype); // If we're not responsible for mounting, just assume it is & find out where.  Use devicenode not mountpt as some automounters mount by volume-name etc

if (Checkmtab(mountpt))       // Check to see if it's already mounted.  NB This will fail if someone maliciously used this mountpt for a different device!
  { if (mounting == false)    // If it is, & we're supposed to be unmounting, do so
      { wxArrayString output, errors;
        long ans = wxExecute(wxT("umount -l ") + mountpt, output, errors);
        if (!ans)  { tempstruct->IsMounted = false; return mountpt; }

        if (getuid() != 0)   // If we're here it probably failed due to permissions.  If user isn't super:
          { if (WHICH_SU==mysu)  
              { if (USE_SUDO) ans = ExecuteInPty(wxT("sudo sh -c \"umount -l ") + EscapeSpaceStr(mountpt) + wxT('\"'));
                 else         ans = ExecuteInPty(wxT("su -c \"umount -l \'") + mountpt + wxT("\'\""));
              }
             else if (WHICH_SU==kdesu)  ans = wxExecute(wxT("kdesu \"umount -l ") + mountpt + wxT("\""), wxEXEC_SYNC);
             else if (WHICH_SU==gksu)  ans = wxExecute(wxT("gksu \"umount -l ") + mountpt + wxT("\" -d"), wxEXEC_SYNC);
             else if (WHICH_SU==gnomesu)  ans = wxExecute(wxT("gnomesu --title "" -c \"umount -l ") + mountpt + wxT("\" -d"), wxEXEC_SYNC);
             else if (WHICH_SU==othersu && !OTHER_SU_COMMAND.IsEmpty())  ans = wxExecute(OTHER_SU_COMMAND + wxT("\"umount -l ") + mountpt + wxT("\" -d"), wxEXEC_SYNC);
              else { wxMessageBox(_("Sorry, you need to be root to unmount non-fstab partitions"), wxT(":-(")); return wxEmptyString; }
            
            if (!ans)  { tempstruct->IsMounted = false; return mountpt; }
            if (ans == CANCEL_RETURN_CODE) return wxEmptyString; 
          }
        wxMessageBox(_("Sorry, failed to unmount")); return wxEmptyString; 
      }
    else                    // If it's already mounted, and now we're being asked to mount again, treat is as a refresh request instead
      { MyGenericDirCtrl* active = MyFrame::mainframe->GetActivePane();
        if (active) { MyFrame::mainframe->GetActivePane()->OnOutsideSelect(mountpt, false); return mountpt; }
         else return wxEmptyString;
      }
  }
  else
  if (mounting == false) return wxEmptyString;        // If it wasn't mounted, & we were being asked to umount, sign off

  
wxString dev;
if (mountpt.IsEmpty())                                // If we're trying to mount a non-fstab partition, eg on a multiple-partition usbpen, mountpt will be empty at this stage
  { wxString msg, asroot, defaultmtpt;                // So ask the user where to mount
    if (getuid() != 0) asroot = _("\n(You'll need to su to root)");
    msg.Printf(wxT("%s%s%s%s"),  _("The partition "), devicenode.c_str(), _(" doesn't have an fstab entry.\nWhere would you like to mount it?"), asroot.c_str());
    for (size_t n=0; n < tempstruct->mountpts.GetCount(); ++n)  // If there's an unused mountpt available for a different partition of this device, offer it as the default
      if (!tempstruct->mountpts[n].IsEmpty() && !Checkmtab(tempstruct->mountpts[n]))
          { defaultmtpt = tempstruct->mountpts[n]; break; }
    mountpt = wxGetTextFromUser(msg, _("Mount a non-fstab partition"), defaultmtpt);
    if (mountpt.IsEmpty())  return wxEmptyString;     // If still empty, give up
    dev = devicenode + wxT(" ");                      // Otherwise put the requested partition into another string, so that Mount will see it
  }

FileData mountdir(mountpt);
if (!mountdir.IsValid())
  { if (wxMessageBox(_("The mount-point for this device doesn't exist.  Create it?"), wxEmptyString, wxYES_NO) != wxYES) return wxEmptyString;
    if (!MkDir(mountpt))  return wxEmptyString;
  }

wxString mount(wxT("mount "));                        // Now do the mounting
if (tempstruct->readonly)  mount += wxT("-r ");       // readonly

          // Note that dev will usually be empty, so that Mount will be able to use the fstab entry.  However if a different partition . . .
wxArrayString output, errors;                         // We don't use these, they just absorb any errors
long ans = wxExecute(mount + dev + mountpt, output, errors);  // Call mount sync (ie wait for answer) as otherwise we'll update the tree display before the data arrives
switch(ans)
  { case 0: tempstruct->IsMounted = true;             // Success, so flag mounted and return the mountpt
            tempstruct->mountpts[tempstruct->defaultpartition] = mountpt;      // Store the new mountpt, mainly for display reasons
            return mountpt;
    case 1:  if (getuid() != 0)                       // Failed due to permissions.  If user isn't super:
            { // As this is a device, the most likely filetype is vfat. When mounted by root, this will not be writable by other users.
              // Solution: pass the uid=100,gid=100 option. However this will bork (and may hang) if the filesystem is ext? etc, where it isn't needed anyway
              // So go with the statistical flow and try it first, retrying without if it fails. If the user ticks the "keep password" box he shouldn't need to reenter.
              mount += wxT("-t auto "); if (dev.IsEmpty()) dev = devicenode + wxT(" ");  // This time we can't just use mountpt, we need the device too 
              wxString extra; extra.Printf(wxT("-o uid=%u,gid=%u "), getuid(), getgid());
              wxString command1 = mount +extra + dev + mountpt, command2 = mount + dev + mountpt;

              if (WHICH_SU==mysu)  
                { if (USE_SUDO) ans = ExecuteInPty(wxT("sudo sh -c \"") + command1 + wxT("\""), output, errors);
                   else         ans = ExecuteInPty(wxT("su -c \"") + command1 + wxT("\""), output, errors);
                }
               else if (WHICH_SU==kdesu)  
                  { ans = wxExecute(wxT("kdesu \"") + command1 + wxT("\""), output, errors);
                    if (ans==32) ans = wxExecute(wxT("kdesu \"") + command2 + wxT("\""), output, errors);
                  }
               else 
                 if (WHICH_SU==gksu)  // Sadly, gksu seems always to return 0, even on failure; and errno always seems to be 22. So we need to rely on the error-count, which _is_ valid :)
                  { errors.Clear(); ans = wxExecute(wxT("gksu \"") + command1 + wxT("\""), output, errors);
                    if (ans==32 || errors.GetCount() > 0) { errors.Clear(); ans = wxExecute(wxT("gksu \"") + command2 + wxT("\""), output, errors); }
                    if (errors.GetCount() > 0) ans = 999;  // Change to an arbitrary error no if there were errors, otherwise success will be falsely assumed
                  }
               else if (WHICH_SU==gnomesu)  
                  { ans = wxExecute(wxT("gnomesu --title "" -c \"") + command1 + wxT("\" -d"), output, errors);
                    if (ans==32) ans = wxExecute(wxT("gnomesu --title "" -c \"") + command2 + wxT("\" -d"), output, errors);
                  }
               else if (WHICH_SU==othersu && !OTHER_SU_COMMAND.IsEmpty())  
                  { ans = wxExecute(OTHER_SU_COMMAND + wxT("\"") + command1 + wxT("\" -d"), output, errors);
                    if (ans==32) ans = wxExecute(OTHER_SU_COMMAND + wxT("\"") + command2 + wxT("\" -d"), output, errors);
                  }
                else { wxMessageBox(_("Sorry, you need to be root to mount non-fstab partitions"), wxT(":-(")); return wxEmptyString; }
              
              switch(ans)
                { case 0:  tempstruct->IsMounted = true;    // Success, so flag mounted and return the mountpt
                          tempstruct->mountpts[tempstruct->defaultpartition] = mountpt;      // Store the new mountpt, mainly for display reasons
                          return mountpt;
                  case CANCEL_RETURN_CODE: return wxEmptyString;
                  case 16:  wxMessageBox(_("Oops, failed to mount successfully\nThere was a problem with /etc/mtab"));  return wxEmptyString; 
                  case 32:  wxMessageBox(_("Oops, failed to mount successfully\n  Try inserting a functioning disc"));  return wxEmptyString;
                   default:  wxString msg; msg.Printf(wxT("%s%lu") , _("Oops, failed to mount successfully due to error "), ans); wxMessageBox(msg);  return wxEmptyString; 
                }
            }
           else  { wxMessageBox(_("Oops, failed to mount successfully\nDo you have permission to do this?"));  return wxEmptyString; }  // If we're already root, abort
  
    case 16:  wxMessageBox(_("Oops, failed to mount successfully\nThere was a problem with /etc/mtab"));  return wxEmptyString; 
    case 32:  wxMessageBox(_("Oops, failed to mount successfully\n  Try inserting a functioning disc"));  return wxEmptyString; 
     default:  wxString msg; msg.Printf(wxT("%s%lu"), _("Oops, failed to mount successfully due to error "), ans); wxMessageBox(msg);  return wxEmptyString; 
  }
}

bool DeviceAndMountManager::Checkmtab(wxString& mountpoint, wxString device /*=wxEmptyString*/)    // Is mountpt the currently-mounted mount-point of a device?
{
#ifdef __GNU_HURD__
  return DeviceAndMountManager::GnuCheckmtab(mountpoint, device);
#endif

#ifdef __linux__
struct mntent* mnt = NULL;

FILE* fmp = setmntent (_PATH_MOUNTED, "r");                // Get a file* to (probably) /etc/mtab
if (fmp==NULL) return false;
#else
struct statfs* fslist;

int numfs = getmntinfo(&fslist, MNT_NOWAIT);
if (numfs < 1) return false;
#endif

wxString mountpt(mountpoint); FileData mp(mountpt);
if (mp.IsSymlink()) mountpt = mp.GetUltimateDestination(); // Cope with a symlink

#ifdef __linux__
while (1)                                                  // For every mtab entry
  { mnt = getmntent(fmp);                                  // Get a struct representing a mounted partition
    if (mnt == NULL)
      { endmntent(fmp); return false; }                    // If it's null, we've run out of candidates.  Return false

    wxString mntdir(mnt->mnt_dir, wxConvUTF8);
    wxString type(mnt->mnt_type, wxConvUTF8); type.MakeLower();
#else
for (int i = 0; i < numfs; ++i)
  { wxString mntdir(fslist[i].f_mntonname, wxConvUTF8);
    wxString type(fslist[i].f_fstypename, wxConvUTF8); type.MakeLower();
#endif
    // Don't try to create a FileData if we're testing a network mount. It's less likely to be symlinked, and e.g. a stale nfs mount hangs lstat!
    if (!type.StartsWith(wxT("nfs")) && !type.Contains(wxT("sshfs")) && type != wxT("cifs") && type != wxT("smbfs"))
      { FileData mt(mntdir); if (mt.IsSymlink()) mntdir = mt.GetUltimateDestination(); } // Cope with a symlink
    if (mntdir == mountpt)                                 // This is the one we're looking for
#ifdef __linux__
      { endmntent(fmp);
        if (device.IsEmpty()) return true;                 // If we don't care which device is mounted there

        wxString mntfsname(mnt->mnt_fsname, wxConvUTF8);   // in case we DO care
#else
      { if (device.IsEmpty()) return true;
        wxString mntfsname(fslist[i].f_mntfromname, wxConvUTF8);
#endif
        FileData dev(device); FileData mntfs(mntfsname);   // Cope with any symlinks
        if (dev.IsSymlink()) device = dev.GetUltimateDestination();
        if (mntfs.IsSymlink()) mntfsname = mntfs.GetUltimateDestination();
        return  (mntfsname ==  device);                  
      }
  }
}

#ifdef __GNU_HURD__
//static
int DeviceAndMountManager::GnuCheckmtab(wxString& mountpoint, wxString device /*=wxEmptyString*/, TranslatorType type /*=TT_either*/) // Is mountpt a mounted device?
{
if (type == TT_either)
  return (GnuCheckmtab(mountpoint, device, TT_active) || GnuCheckmtab(mountpoint, device, TT_passive)); // If unspecified, check for both
  
wxString mountpt(mountpoint); FileData mp(mountpt);
if (mp.IsSymlink()) mountpt = mp.GetUltimateDestination(); // Cope with a symlink

wxArrayString output, errors; int failure(true);
if (type == TT_active)
  failure = wxExecute(wxT("/hurd/mtab -I ") + mountpt, output, errors); // Can't use ExecuteInPty() here: the output may contain /foo/bar/password, which triggers the GetPassword dialog
 else
  failure = ExecuteInPty(wxT("showtrans ") + mountpt, output, errors);

if (!failure && !device.empty())
  { for (size_t n = 0; n < output.GetCount(); ++n)
      if (output.Item(n).Contains(device)) return true;
    return false;
  }

return !failure;
}
#endif

wxString DeviceAndMountManager::WhereIsDeviceMounted(wxString device, size_t type)
{
#ifdef __linux__
struct mntent* mnt = NULL;

FILE* fmp = setmntent (_PATH_MOUNTED, "r");             // Get a file* to (probably) /etc/mtab
if (fmp==NULL) return wxEmptyString;

while (1)                                               // For every mtab entry
  { mnt = getmntent(fmp);                               // Get a struct representing a mounted partition
    if (mnt == NULL)  break;                            // If it's null, we've run out of candidates
    wxString mntfsname(mnt->mnt_fsname, wxConvUTF8);
    if (mntfsname == device)
      { endmntent(fmp); return wxString(mnt->mnt_dir, wxConvUTF8); }  // If it's the one we're looking for, return the associated mountpt
#else
struct statfs* fslist;

int numfs = getmntinfo(&fslist, MNT_NOWAIT);
if (numfs < 1) return wxEmptyString;

for (int i = 0; i < numfs; ++i)
  { wxString mntfsname(fslist[i].f_mntfromname, wxConvUTF8);
    if (mntfsname == device)
      return wxString(fslist[i].f_mntonname, wxConvUTF8);
#endif
  }

            // If we're here, the device wasn't found.  See if it's actually a symlink for a different device  eg /dev/dvd -> /dev/hdc.  If so, try that
FileData stat(device);
if (stat.IsSymlink())   return WhereIsDeviceMounted(stat.GetSymlinkDestination(), type);

            // No good?  Let's check to see if this device is supermounted.  If so, the mtab device name will probably be "none".  So rather inelegantly reuse CheckSupermountTab() to find the mountpt
enum discoverytable table = deviceman->DevInfo->GetDiscoveryTableForThisDevicetype(deviceman->DevInfo->GetIcon(type));
if (table==MtabSupermount || table==FstabSupermount)
  { wxArrayString partitions, mountpts;
    CheckSupermountTab(partitions, mountpts,  device, true);
    if (mountpts.GetCount()) return mountpts[0];        // If successful, mountpts will hold one mountpt
  }

return wxEmptyString;                                   // If we're here, everything failed
}

//static
#ifdef __linux__
struct mntent* DeviceAndMountManager::ReadMtab(const wxString& partition, bool DvdRamFS /*=false*/)    // Is 'partition' currently mounted? Returns struct of data if it is, NULL if it isn't
{
struct mntent* mnt = NULL;

FILE* fmp = setmntent (_PATH_MOUNTED, "r");           // Get a file* to (probably) /etc/mtab
if (fmp==NULL) return mnt;
#else
struct statfs* DeviceAndMountManager::ReadMtab(const wxString& partition, bool DvdRamFS /*=false*/)
{
struct statfs* fslist;

int numfs = getmntinfo(&fslist, MNT_NOWAIT);
if (numfs < 1) return NULL;
#endif

wxString partitionwithsep(partition),  partitionwithout(partition); // Avoid the usual '/' issue
if (partition.Right(1) == wxFILE_SEP_PATH) partitionwithout = partition.BeforeLast(wxFILE_SEP_PATH);
 else partitionwithsep << wxFILE_SEP_PATH;

#ifdef __linux__
while (1)                                             // For every mtab entry
  { mnt = getmntent(fmp);                             // Get a struct representing a mounted partition
    if (mnt == NULL)  break;                          // If it's null, we've run out of candidates.  Return null as flag
    wxString mntfsname(mnt->mnt_fsname, wxConvUTF8);
    if (!mntfsname.empty() && ((mntfsname == partitionwithsep) || (mntfsname == partitionwithout))) // If it's the one we're looking for, return it
      { if (!DvdRamFS) { endmntent(fmp); return mnt; }
        wxString mnttype(mnt->mnt_type, wxConvUTF8);
        for (size_t n=0; n < RealFilesystemsArray.GetCount(); ++n)  // For DVD-RAM, we're only interested in filesystems like ext2 not eg subfs
          if (mnttype.Left(3) == RealFilesystemsArray.Item(n).Left(3)) { endmntent(fmp); return mnt; }  // Found one
      }                                               // If we're here, this was something irrelevant like a subfs mount, so ignore
  }

endmntent(fmp);        // If we're here, the device wasn't found.  See if it's actually a symlink for a different device  eg /dev/dvd -> /dev/hdc.  If so, try that
FileData stat(partition);
if (stat.IsSymlink())   { wxString target = stat.GetSymlinkDestination(); return ReadMtab(target, DvdRamFS); }

mnt = NULL; return mnt;                               // If we're here, it failed. Return null as flag
#else
for (int i = 0; i < numfs; ++i)
  { wxString mntfsname(fslist[i].f_mntfromname, wxConvUTF8);
    if (!mntfsname.empty() && ((mntfsname == partitionwithsep) || (mntfsname == partitionwithout)))
      { if (!DvdRamFS) return &fslist[i];
        wxString mnttype(fslist[i].f_fstypename, wxConvUTF8);
        for (size_t n=0; n < RealFilesystemsArray.GetCount(); ++n)
          if (mnttype.Left(3) == RealFilesystemsArray.Item(n).Left(3)) return &fslist[i];
      }
  }
FileData stat(partition);
if (stat.IsSymlink())
  { wxString target = stat.GetSymlinkDestination();
    return ReadMtab(target, DvdRamFS);
  } else return NULL;
#endif
}


void DeviceAndMountManager::GetPartitionList(wxArrayString& array, bool ignoredevices /*=false*/)    // Parse the system file that holds details of all known partitions
{
FileData fd(PARTITIONS);
if (!fd.IsValid())
  { 
    #ifndef __LINUX__
      return FillGnuHurdPartitionList(array);         // gnu-hurd doesn't have a /proc/partitions atm
    #endif
    
    wxString msg; msg.Printf(wxT("%s%s%s"),_("I can't find the file with the list of partitions,  "), PARTITIONS.c_str(), _("\n\nYou need to use Configure to sort things out"));
    wxMessageBox(msg); return;
  }

wxFileInputStream file_input(PARTITIONS);             // I'd originally used wxTextFile here, but in 2.7.0-8.1 this won't work with non-seekables
wxBufferedInputStream input(file_input);              // Without the buffer, only 1 byte gets read !?
wxTextInputStream text(input);
wxArrayString linearray;
wxRegEx ramdiskchk(wxT(" ram[0-9]*"));
for (;;)
  { wxString line = text.ReadLine();
    if (input.Eof() && line.IsEmpty()) break;
    if (!ramdiskchk.Matches(line))  // Filter out /dev/ram0 etc. They're rarely used and we probably can't cope if one was; yet they pollute the mount dialog
      linearray.Add(line);
  }

size_t count = linearray.GetCount(); 
if (!count)
  { wxString msg; msg.Printf(wxT("%s%s%s"),_("File "), PARTITIONS.c_str(), (" exists, but seems to be empty.\n\n         Please check."));
    wxMessageBox(msg); return; 
  }

for (size_t n=0; n < count; ++n)
  { wxString line = linearray[n];                     // Read a line from the file
    wxStringTokenizer tkz(line);                      // Tokenise it
    if (tkz.HasMoreTokens())                          // Make sure it's not blank!
      { wxString token = tkz.GetNextToken();          // Get first token
        long l; bool flag=false;
        for (int no=0; no < 3; ++no)                  // For a valid line, the 1st 3 tokens will be numbers.  Use this to check validity
          if (!token.ToLong(&l))                      // If it's NOT a number, we don't want to know any more about this line, so flag to abort
            { flag=true; break; }
           else
            { if ((no==0) && ignoredevices            // The first one, major, will be 8 for a sata hd partition, but for an ide one it could be 3, 22, 33/4, 56/7 etc
                      && ((l == 2) || (l == 11)))     // and the ide ones don't distinguish between hard drives and cdroms anyway
                { flag=true; break; }                 // So just exclude floppies and sata cdroms which, for a modern distro...
               else
                token = tkz.GetNextToken();
            }
           
        if (flag) continue;                           // If flag, skip the rest of this line
        
        if (token.Left(3) == LVMPREFIX && wxIsdigit(token.GetChar(3)))
              { AddLVMDevice(array, token); continue; } // If this looks like a lvm device, play with it elsewhere
        
        token = DEVICEDIR + token;                    // This token should be the device name.  Convert it into /dev/foo
        FileData test(token); if (!test.IsValid())   continue;  // Check this device actually exists
                                      // We've found a partition, so add it to the array
        size_t c = array.GetCount();  // First do the following, which avoids having an entry called hda when there is also one called hda1
        if (c)                                        // Assuming the array already has data
          { wxString previous = array[c-1];           // Get the last entry
            if (previous == token.Left(token.Len() - 1))  // If the last entry was /dev/foo, and the new one is /dev/foo1, remove /dev/foo
              array.RemoveAt(c-1);
          }
        array.Add(token);                             // Finally add the new entry
      }
  }        
}

void DeviceAndMountManager::FillGnuHurdPartitionList(wxArrayString& array) // gnu-hurd doesn't have a /proc/partitions atm, so just add everything
{
const wxString DEVHD(wxT("/dev/hd"));
for (size_t n=0; n < 6; ++n)
  { wxString disc(DEVHD); disc << n;
    FileData fd(disc);
    if (fd.IsValid())
      for (size_t p=1; p < 17; ++p) // There _is_ a /dev/hd0 (or 1 or...)  Add each possible partition i.e. /dev/hd0s1, 2,...
      { wxString partition(disc); partition << 's' << p;
        array.Add(partition);
      }
  }
}

void DeviceAndMountManager::FillGnuHurdMountsList(ArrayofPartitionStructs& array)    // Find mounts (active translators) in gnu-hurd
{
#ifdef __GNU_HURD__
array.Clear();
wxArrayString output, errors;
// Can't use ExecuteInPty() here: the output may contain /foo/bar/password, which triggers the GetPassword dialog
wxExecute(wxT("/hurd/mtab /"), output, errors); // This should fill 'output' with active translator lines
for (size_t n=0; n < output.GetCount(); ++n)
  { wxString partition, mountpt;
    wxArrayString tokens = wxStringTokenize(output.Item(n), wxT(" \t\r\n"));
    if (tokens.GetCount() < 2) continue;
    if (tokens.Item(0) == "none" || tokens.Item(0) == "/") continue; // We don't want to unmount root, or any oddities

    PartitionStruct* pstruct = new PartitionStruct;
    pstruct->device = tokens.Item(0);
    pstruct->mountpt = tokens.Item(1);
    pstruct->type = tokens.Item(2); // Unused atm, but...
    pstruct->IsMounted = true;
    array.Add(pstruct);
  }
#endif
}

void DeviceAndMountManager::AddLVMDevice(wxArrayString& array, wxString& dev)  // Called by GetPartitionList() to process lvm "partitions"
{
wxTextFile file(LVM_INFO_FILE_PREFIX + dev);
if (!file.Exists())  return;
  
file.Open(); if (!file.IsOpened())  return;

size_t count = file.GetLineCount(); 
if (!count)  return;

for (size_t n=0; n < count; ++n)
  { wxString line = file.GetLine(n);
    if (line.Left(2) == wxT("N:"))                      // The line we want begins with N:  The data we want follows immediately
      { wxString ans = line.AfterLast(wxFILE_SEP_PATH); // The name here is /mapper/foo.  We want the name of the symlink pointing to this, "foo", as that's what ReadFstab() & ReadMtab() will get from /proc
        ans = wxT("/dev/") + ans;
        FileData test(ans); 
        if (test.IsValid()) array.Add(ans);             // Check there IS a /dev/foo.  If so, add it to the array
        return;
      }
  }
}


static int wxCMPFUNC_CONV SortByDevicename(PartitionStruct **first, PartitionStruct **second)  // Sort PartitionArray by devicename
{
  // This is non-trivial as we want /dev/sda10 to come _after_ /dev/sda9, while not breaking too badly on oddities e.g. loop0
#ifdef __LINUX__
  static const int baselen = 8;
#else
  static const int baselen = 9; // In gnu-hurd we're looking for /dev/hd0s9, /dev/hd0s10
#endif
wxString device1 = (*first)->device, device2 = (*second)->device;
if (device1.Left(baselen) == device2.Left(baselen))
  { long val1, val2;
    bool b1 = device1.Mid(baselen).ToLong(&val1); bool b2 = device2.Mid(baselen).ToLong(&val2);
    if (b1 && b2) return (val1 > val2);
  }

return device1.Cmp(device2);
}

void DeviceAndMountManager::FillPartitionArray(bool justpartitions /*=true*/)  // Load all known hard-disc partitions into PartitionArray, find their mountpts etc.  Optionally mounted iso-images too
{
for (int n = (int)PartitionArray->GetCount(); n > 0; --n)       // Delete any current data
  { PartitionStruct* item = PartitionArray->Item(n-1); delete item; PartitionArray->RemoveAt(n-1); }

if (!FillPartitionArrayUsingLsblk(true))                        // Do this the easy, reliable, modern way if possible. 'true' == ignore removeable stuff
  {                                                             // Otherwise the harder ways
    // First use blkid as this gets a partition's UUID too. If it fails/is absent, we'll still find partitions below using GetPartitionList()
    bool blkidOK = FillPartitionArrayUsingBlkid();

    wxArrayString partitions;
    GetPartitionList(partitions, true);                         // Load array with list of all known partitions
    if (!justpartitions && !blkidOK) // If from Unmount, add any mounted iso-images found in mtab; but not if blkid has already done it
      FindMountedImages();

    for (size_t n=0; n < partitions.GetCount(); ++n)
      { PartitionStruct* pstruct = NULL; bool already_known(false);
        for (size_t i=0; i < PartitionArray->GetCount(); ++i)
          if (PartitionArray->Item(i)->device == partitions[n])
            { pstruct = PartitionArray->Item(i);
              already_known = true; break;
            }

        if (!already_known)
          { pstruct = new PartitionStruct;
            pstruct->device = partitions[n];                    // Store in it the device name eg /dev/hda1  
          }

        struct fstab* fs = ReadFstab(pstruct);                  // Use the device name (or uuid) to try to find a fstab entry for this partition
        if (fs != NULL)                                         // Null would have meant not found
          { pstruct->mountpt = wxString(fs->fs_file, wxConvUTF8); // Store the mountpt
            pstruct->IsMounted = Checkmtab(pstruct->mountpt, pstruct->device);// & whether it's mounted.  Pass the device-name too, so that we only record if the mountpt contains THIS device
            if (!pstruct->IsMounted)                            // If it isn't mounted where it's supposed to be, look elsewhere in case it was su-mounted in the 'wrong' place
#ifdef __linux__
              { struct mntent* mnt = ReadMtab(pstruct->device);
                if (mnt != NULL) { pstruct->mountpt = wxString(mnt->mnt_dir, wxConvUTF8); pstruct->IsMounted = true; }
#else
              { struct statfs* mnt = ReadMtab(pstruct->device);
                if (mnt != NULL) { pstruct->mountpt = wxString(mnt->f_mntonname, wxConvUTF8); pstruct->IsMounted = true; }
#endif
              }
          }
         else                                                   // If we didn't find it in fstab, check mtab anyway, in case someone mounted it the hard way
#ifdef __linux__
          { struct mntent* mnt = ReadMtab(pstruct->device);
            if (mnt != NULL)
              { pstruct->mountpt = wxString(mnt->mnt_dir, wxConvUTF8);  // Store the mountpt
#else
          { struct statfs* mnt = ReadMtab(pstruct->device);
            if (mnt != NULL)
              { pstruct->mountpt = wxString(mnt->f_mntonname, wxConvUTF8);
#endif
                pstruct->IsMounted = true;                      // It's mounted by definition:  it's in mtab
              }
             else pstruct->IsMounted = false;                   // Otherwise reset this bool, as it's otherwise undefined
          }
          
        if (!already_known)  PartitionArray->Add(pstruct);      // Add any new struct to its array
      }

      // If we want to ignore swap partitions, now is the best time to delete them: blkid is good at finding the type, but ReadFstab() isn't, so would have added them back
     if (SUPPRESS_SWAP_PARTITIONS)
      for (int n = (int)PartitionArray->GetCount(); n > 0; --n)
       if (PartitionArray->Item(n-1)->type == wxT("swap"))
          { delete PartitionArray->Item(n-1); PartitionArray->RemoveAt(n-1);}
  }

PartitionArray->Sort(SortByDevicename);                         // Finally, sort them
}

wxArrayString ParseLsblkLine(const wxString line)
{
wxArrayString parsed;

// RM="0" TYPE="part" NAME="sda1" SIZE="123456789" UUID="1-2-3-4" LABEL="foo" MOUNTPOINT="bar"
static wxRegEx rxRM("RM=\"([01]+)\"");  // RM == ReMovable device e.g. usbpen
wxCHECK_MSG(rxRM.Matches(line), parsed, "Failed to parse the 'RM' section of lsblk");
parsed.Add(rxRM.Matches(line) ? rxRM.GetMatch(line, 1) : "");

static wxRegEx rxTYPE("TYPE=\"([adilmoprtvADILMOPRTV0-9]+)\""); // We don't want 'disk', which will be sda as opposed to sda1, but keep raid 'partitions' e.g. md0, which will have type raid0 or raid1 etc
parsed.Add(rxTYPE.Matches(line) ? rxTYPE.GetMatch(line, 1) : ""); // Should match part, loop, lvm, raid0

static wxRegEx rxNAME("NAME=\"([0-9a-zA-z]+)\"");
parsed.Add(rxNAME.Matches(line) ? rxNAME.GetMatch(line, 1) : "");

static wxRegEx rxSIZE("SIZE=\"([0-9]+)\"");
parsed.Add(rxSIZE.Matches(line) ? rxSIZE.GetMatch(line, 1) : "");

static wxRegEx rxUUID("UUID=\"([0-9a-fA-F-]+)\"");
parsed.Add(rxUUID.Matches(line) ? rxUUID.GetMatch(line, 1) : "");

static wxRegEx rxLABEL("LABEL=\"([0-9a-zA-z]+)\"");
parsed.Add(rxLABEL.Matches(line) ? rxLABEL.GetMatch(line, 1) : "");

static wxRegEx rxMOUNTPOINT("MOUNTPOINT=\"(.+)\"");
parsed.Add(rxMOUNTPOINT.Matches(line) ? rxMOUNTPOINT.GetMatch(line, 1) : "");

return parsed;
}

bool DeviceAndMountManager::FillPartitionArrayUsingLsblk(bool fixed_only /*=false*/)
{ 
if (LSBLK.empty()) return false;

wxString command = LSBLK + wxT(" -nbP -o RM,TYPE,NAME,SIZE,UUID,LABEL,MOUNTPOINT");  // No header, raw, bytes, columns: ?removeable, type, name, size, uuid, label, mtpt
if (fixed_only) command << wxT(" -e 2,11");                     // Exclude floppies/cdroms

wxArrayString output, errors;
if ((wxExecute(command, output, errors) != 0) || !output.GetCount()) return false;

  // In theory lsblk could return all the necessary info. In practice it doesn't always for all versions/users, so combine it with blkid if available
wxArrayString blkoutput;
if (!BLKID.empty())
  wxExecute(BLKID, blkoutput, errors);

wxArrayString namearray;                                        // Store each accepted device name here, to check for dups

for (size_t n=0; n < output.GetCount(); ++n)
  { wxString name, type, mountpt;
    // I'd previously just tokenised, but now trying for UUID/LABEL too makes it impossible to guess which empty string is which
    wxArrayString tokens = ParseLsblkLine(output.Item(n));      // So I'm now using -nbP to get key/value pairs. Should be RM, TYPE, NAME, SIZE, plus hopefully UUID and LABEL, with optional MOUNTPOINT 
    if (tokens.IsEmpty()) continue;                        // 

    if (fixed_only && tokens.Item(0) == wxT("1")) continue;     // Skip floppies, cdroms
    type = tokens.Item(1); 
    if (type.empty()) continue;                                 // Skip unwanted 'partitions' e.g. sda (cf sda1)
    name = tokens.Item(2);
    // We need to exclude any extended partition too. FSTYPE should have helped, but unfortunately in debian distros (2012) it only works for root
    // So use the size instead; it returns 1024 bytes for my ext
    long size; if (!tokens.Item(3).ToLong(&size)) continue;
    if (size < 500000) continue;                                // I doubt if there'll be a mountable partition smaller than this

    if (tokens.GetCount() > 4) mountpt = tokens.Last();
    if (mountpt == wxT("[SWAP]")) continue;                     // That's the string returned for a swap mountpt. Except when nothing is returned :/
    wxString blktype = GetTypeFromBlkid(blkoutput, name);
    if (blktype == wxT("swap")) continue;                       // so try again using the blkid output, which does seem to be reliable
    if (blktype == wxT("linux_raid_member")) continue;          // Also skip the partitions e.g. sdb1,sdc1 that hold raid md0

    if (namearray.Index(name) != wxNOT_FOUND) continue;         // We need to check for duplication; this happens with raid, where both partitions contain md0
    namearray.Add(name);

    PartitionStruct* pstruct = new PartitionStruct;
    pstruct->device = wxT("/dev/") + name;                      // 'name' is just e.g. sda1
    pstruct->uuid = GetUUIDFromBlkid(blkoutput, name);
    if (pstruct->uuid.empty() && tokens.GetCount() > 4) pstruct->uuid = tokens.Item(4);
    if (tokens.GetCount() > 5) pstruct->label = tokens.Item(5);
    pstruct->mountpt = mountpt; 
    pstruct->IsMounted = !mountpt.empty();

    // lsblk doesn't look in fstab for potential mountpts. We must do this here, ready for the first 'Mount a partition' dialog
    if (mountpt.empty())
      { struct fstab* fs = ReadFstab(pstruct);
        if (fs)
          pstruct->mountpt = wxString(fs->fs_file, wxConvUTF8);
      }

    PartitionArray->Add(pstruct);
  }

return (PartitionArray->GetCount() > 0);
}

bool DeviceAndMountManager::FillPartitionArrayUsingBlkid()
{ 
if (BLKID.empty()) return false;

wxArrayString output, errors;
if ((wxExecute(BLKID, output, errors) != 0) || !output.GetCount()) return false;

for (size_t n=0; n < output.GetCount(); ++n)
  { wxString device, type, uuid;
    ParseBlkidOutput(output.Item(n), device, type, uuid);     // Parse each line of output for the devicename, type and uuid
    if (device.empty()) continue;

    PartitionStruct* pstruct = new PartitionStruct;
    pstruct->device = device; pstruct->type = type; pstruct->uuid = uuid;
    PartitionArray->Add(pstruct);
  }

return (PartitionArray->GetCount() > 0);
}

void DeviceAndMountManager::FindMountedImages()  // Add mounted iso-images from mtab to array
{
#ifdef __linux__
FILE* fmp = setmntent (_PATH_MOUNTED, "r");                 // Get a file* to (probably) /etc/mtab
if (fmp==NULL) return;

while (1)                                                   // For every mtab entry
  { struct mntent* mnt = getmntent(fmp);                    //  get a struct representing a mounted partition
    if (mnt == NULL)  
      { endmntent(fmp);  return; }                          // If it's null, we've finished mtab
    
    wxString device(mnt->mnt_fsname, wxConvUTF8);           // Make the 'device' into a wxString for convenience
#else
struct statfs* fslist;

int numfs = getmntinfo(&fslist, MNT_NOWAIT);
if (numfs < 1) return;

for (int i = 0; i < numfs; ++i)
  { wxString device(fslist[i].f_mntfromname, wxConvUTF8);
#endif
    // If it starts with a '/dev/loop' or with a '/' but not with /dev/ or //  it's probably an iso-image
    if ((device.Left(9) == wxT("/dev/loop")) || (device.GetChar(0) == wxT('/') && 
                            !(device.Left(5) == wxT("/dev/") || device.Left(2) == wxT("//"))))
      { struct PartitionStruct* newmnt = new struct PartitionStruct;  // store in structarray
#ifdef __linux__
        newmnt->device = wxString(mnt->mnt_fsname, wxConvUTF8);
        newmnt->mountpt = wxString(mnt->mnt_dir, wxConvUTF8);
#else
        newmnt->device = wxString(fslist[i].f_mntfromname, wxConvUTF8);
        newmnt->mountpt = wxString(fslist[i].f_mntonname, wxConvUTF8);
#endif
        newmnt->IsMounted = true;                           // By definition
        PartitionArray->Add(newmnt);                      
      }
  }
}

void DeviceAndMountManager::CheckForOtherDevices()  // Check for any other mountables eg USB devices.  Called by a timer
{
if (DEVICE_DISCOVERY_TYPE != OriginalMethod) // If this distro uses a modern method of IDing usb devices  eg SuSE >=9.1, do it a different way
  { CheckForGioAutomountedDevices();
    return CheckForAutomountedDevices();
  }

CheckForNovelDevices();                                     // First check that there hasn't been a novelty plugged in

size_t count=deviceman->FirstOtherDevice, child=0; DevicesStruct* tempstruct;
while ((tempstruct = deviceman->GetNextDevice(count, child)) != NULL)  // Use GetNextDevice() as it searches children too.  It autoincs the two indices
  { if (tempstruct->ignore) continue;                       // If we've been told to ignore this device, do so
    
    int attached = ReadScsiUsbStorage(tempstruct);          // See if it's currently attached, & if so, in which dir    
    if (!attached)
      { if (!tempstruct->HasButton) continue;               // If it's not attached, & there's no visible button, nothing to do
                              // If it's not attached but there IS a button, the device must just have been removed, so unmount anything & remove button
        for (size_t p = 0; p < tempstruct->mountpts.GetCount(); ++p)
          if (!tempstruct->mountpts[p].IsEmpty())  
            { if (!DoMount(tempstruct, false, p).IsEmpty())
                MyFrame::mainframe->OnUnmountDevice(tempstruct->mountpts[p]);  // If the umount worked, replace it in its pane(s) with the default startdir
              tempstruct->mountpts[p].Empty();              // Clear the mountpt
            }

        if (tempstruct->button != NULL)                     // Now dispose of the button
          { if (MyFrame::mainframe->sizerControls->Detach(tempstruct->button))
               { tempstruct->button->Destroy(); tempstruct->button = NULL; MyFrame::mainframe->sizerControls->Layout(); }
          }
        tempstruct->HasButton = false;
      }
    
     else                     // We've found an attached device.  Is it already attached?
      { if (tempstruct->HasButton)  continue;               // Yes, this is old news.
                              // No, this is a new attachment.  Find its associated device, which will depend on the order of device use since booting

        GetDevicePartitions(tempstruct->partitions, tempstruct->mountpts, tempstruct->devicenode);  // Find all the partitions on this device
        if (tempstruct->partitions.IsEmpty()) continue;         // If there aren't any, no point continuing
        struct fstab* fs = ReadFstab(tempstruct->partitions[0]);// Use this to try to find the first partition's mountpt
        if (fs == NULL)  continue;                              // It there isn't one, this may be because fstab hasn't yet been updated.  Maybe next cycle
        tempstruct->mountpts[tempstruct->defaultpartition] = wxString(fs->fs_file, wxConvUTF8);  // fs_file holds the mountpoint
        if (tempstruct->mountpts[tempstruct->defaultpartition].IsEmpty())  continue;
        
        DeviceBitmapButton* button = new DeviceBitmapButton(deviceman->DevInfo->GetCat(tempstruct->devicetype));// There's a new device attached.  Load an appropriate button from XRC eg Pen, Mem
        if (button==NULL)  continue;
        
        button->ButtonNumber = tempstruct->buttonnumber;    // Provide the ButtonNumber
        button->SetName(wxT("Device"));                     // Specify the name, so we can tell that it's an device
        tempstruct->button = button;                        // Store a ptr to the button, so that it can be located for removal when detached
        tempstruct->HasButton = true;
        if (!(tempstruct->Vendorstr.IsEmpty() && tempstruct->Productstr.IsEmpty()))
            button->SetToolTip(tempstruct->Vendorstr + wxT(" ") + tempstruct->Productstr); // The vendor/model is probably a more useful tip than 'usbpen'
        
        MyFrame::mainframe->sizerControls->Add(button, 0, wxEXPAND | wxLEFT, 5);  // Add button to panelette sizer
        MyFrame::mainframe->sizerControls->Layout();
      }
  }
}
//static
struct fstab* DeviceAndMountManager::ReadFstab(const wxString& dev, const wxString& uuid/*=""*/, const wxString& label/*=""*/)  // Search fstab for a line for this device
{
struct fstab* fs = NULL;
if (dev.empty() && uuid.empty()) return fs;    // dev holds the devicename we're looking for eg '/dev/sda1'. Optionally, uuid holds any UUID

if (!setfsent()) return fs;                    // Initialise using setfsent().  False means failure

fs = getfsspec(dev.mb_str(wxConvUTF8));        // Searches thru fstab, looking for an entry that begins with dev
if ((!fs) && (!label.empty()))                 // or LABEL=
  { wxString LABEL(wxT("LABEL=") + label);
    fs = getfsspec(LABEL.mb_str(wxConvUTF8));
  }

if ((!fs) && (!uuid.empty()))                 // or UUID=
  { wxString UUID(wxT("UUID=") + uuid);
    fs = getfsspec(UUID.mb_str(wxConvUTF8));
  }

endfsent(); 
return fs;                                     // Return the struct.  If the device was found, it will hold the data.  If not, NULL
}

//static
bool DeviceAndMountManager::FindUnmountedFstabEntry(wxString& dev, wxArrayString& answerarray)  // Goes thru fstab, looking for Unmounted entries that begins with string dev.  Used for DVD-RAM
{
wxArrayString partitions, mountpts;

struct fstab* fs = NULL;
if (dev.IsEmpty()) return false;               // Dev holds the device we're looking for   eg "/dev/sda1"
if (!setfsent()) return false;                 // Initialise using setfsent().  False means failure
do
  { fs = getfsent();                           // Searches thru fstab, looking for an entry that begins with dev
    if (fs == NULL)  endfsent();               // If null, we're done
     else
      { for (size_t n=0; n < RealFilesystemsArray.GetCount(); ++n)  // For DVD-RAM, we're only interested in filesystems like ext2 not eg subfs
          if (wxString(fs->fs_vfstype,wxConvUTF8).Left(3) == RealFilesystemsArray.Item(n).Left(3))  // See if the filetype begins with eg ext, rei, aut
            { partitions.Add(wxString(fs->fs_spec,wxConvUTF8)); mountpts.Add(wxString(fs->fs_file,wxConvUTF8)); }  // If so, we're interested
      }
  }
 while (fs != NULL);

FileData fd(dev); wxString symtarget = fd.GetUltimateDestination();
answerarray.Empty();

for (size_t n=0; n < partitions.GetCount(); ++n)
  { wxString item = partitions.Item(n);
    if (item == dev || item == symtarget)
#ifdef __linux__
      { struct mntent* mnt = NULL;             // Found an entry.  Look for it in mtab
        FILE* fmp = setmntent (_PATH_MOUNTED, "r"); if (fmp==NULL) { endfsent(); return answerarray.GetCount() > 0; }
        while (1)                    
          { mnt = getmntent(fmp);
            if (mnt == NULL)                   // If it's null, we've run out of candidates, so this entry can't be mounted
              { answerarray.Add(mountpts.Item(n));
                endmntent(fmp); break; 
              }

            if (wxString(mnt->mnt_fsname,wxConvUTF8) == dev || wxString(mnt->mnt_fsname,wxConvUTF8) == symtarget) // Found the device
              if (wxString(mnt->mnt_dir,wxConvUTF8) == mountpts.Item(n))                                          //   but it's already mounted here
                  { endmntent(fmp);  break; }  // so look in 'partitions' array for another entry
          }
#else
      { struct statfs* fslist;
        int numfs = getmntinfo(&fslist, MNT_NOWAIT);
        if (numfs < 1) return answerarray.GetCount() > 0;
        for (int i = 0; i < numfs; ++i)
          if (wxString(fslist[i].f_mntfromname, wxConvUTF8) == dev || wxString(fslist[i].f_mntfromname, wxConvUTF8) == symtarget)
            if (wxString(fslist[i].f_mntonname, wxConvUTF8) == mountpts.Item(n)) goto another;
        answerarray.Add(mountpts.Item(n));
another:;
#endif
      }
  }

endfsent(); return answerarray.GetCount() > 0;
}

size_t DeviceAndMountManager::ReadFstab(ArrayofPartitionStructs& PartitionArray, const wxString& type) const // Uses findmnt to look for entries of a particular type(s) e.g. cifs
{
wxCHECK_MSG(!type.empty(), 0, wxT("Empty type supplied"));
if (FINDMNT.empty()) return 0;

WX_CLEAR_ARRAY(PartitionArray);
wxString command = FINDMNT + wxT(" -nrs -o SOURCE,TARGET,OPTIONS"); // No header, raw, fstab, columns: 'device', mtpt, options
command << wxT(" -t ") << type;                                     // 'type' may contain >1 type, comma-separated  

wxArrayString output, errors;
if ((wxExecute(command, output, errors) != 0) || !output.GetCount()) return false;

for (size_t n=0; n < output.GetCount(); ++n)
  { wxArrayString tokens = wxStringTokenize(output.Item(n));
    if (tokens.GetCount() < 3) continue;                            // Should be 'device', mtpt, options

    PartitionStruct* pstruct = new PartitionStruct;
    pstruct->device = tokens.Item(0);
    pstruct->mountpt = tokens.Item(1);
    pstruct->uuid = tokens.Item(2);                                 // Abuse the uuid field to store mount-options
    PartitionArray.Add(pstruct);
  }

return PartitionArray.GetCount();
}

//static
void DeviceAndMountManager::LoadFstabEntries(wxArrayString& devicenodes, wxArrayString& mountpts)  // Loads all valid fstab entries into arrays
{
struct fstab* fs = NULL;
if (!setfsent()) return;                       // Initialise using setfsent().  False means failure
while (1)
  { fs = getfsent();                           // Searches thru fstab
    if (fs == NULL)  { endfsent(); return; }   // If null, we're done

    devicenodes.Add(wxString(fs->fs_spec,wxConvUTF8)); mountpts.Add(wxString(fs->fs_file,wxConvUTF8));  // Add the device & mountpt data to arrays
  }
}

bool DeviceAndMountManager::ReadScsiUsbStorage(DevicesStruct* tempstruct)  // Look thru proc/scsi/usb-storage*, to see if this usb device is attached, & if so where.  Only for non-automounting distros
{
if (tempstruct==NULL) return false;
                // Any usb devices should each be attached with a dir under /proc/scsi/, probably called usb-storage-0, usb-storage-2 etc
                // However these dirs aren't created until a device is 1st attached (after each computer boot), though once there, they persist until a reboot
wxArrayString dirarray;

wxLogNull WeDontWantMissingDirMsgs;
wxString dirname = wxFindFirstFile(SCSIUSBDIR + wxT("*"), wxDIR);  // Get 1st subdir that matches, probably /proc/scsi/usb-storage-0
while (!dirname.IsEmpty())
  { dirarray.Add(dirname);                           // Add the dir to array
    dirname = wxFindNextFile();                      // Now try for /proc/scsi/usb-storage-1 etc
  }

dirarray.Sort();                                     // wxFindFirstFile finds the dirs randomly.  Sort so that usb-storage-0 precedes usb-storage-1

                // We now have a list of subdirs.  Look thru each in turn for relevant files
for (size_t n=0; n < dirarray.GetCount(); ++n)
  { wxString filename = wxFindFirstFile(dirarray[n] + wxT("/*"), wxFILE);  // Search thru this directory, acquiring the names of each file
    while (!filename.IsEmpty())
      { wxFile file(filename);                       // Open file filename
        if (!file.IsOpened())  continue;
        if (!file.Exists(filename)) { file.Close(); continue; }  // Shouldn't happen      
  
        char buf[500]; 
        off_t count = file.Read(buf, 500); file.Close(); if (!count)  continue;

        wxString contents(buf, wxConvUTF8, count);   // Make the contents into a wxString
        if (contents.Contains(tempstruct->name1) && contents.Contains(tempstruct->name2))  // See if the passed strings are present
          { int index = contents.Find(wxT("Attached"));  // If so, find the bit of the string after "Attached"
            if (index != -1)
              { wxString terminus = contents.Mid(index);
                if (!(terminus.Contains(wxT("Yes")) || terminus.Contains(wxT("1"))))  return false;  // Found it but it isn't attached (RedHat 7.2 uses '1' instead of 'Yes')
                if (tempstruct->HasButton) return true;  // Found it attached.  See if we already knew about it.  If so, nothing interesting to do
                    // We've found a device that is attached but not yet displayed.  We need to find its device node, which will vary according to the order of device attachment
                long no; if (!filename.AfterLast(wxFILE_SEP_PATH).ToLong(&no))  return false;  // The file is called "1" or "2" etc.  Make it into an int
                wxArrayString notused; return ParseProcScsiScsi(tempstruct, no, notused);      // Do the rest of the job in another function, as it involves a different file
              }
          }
      
        filename = wxFindNextFile();                 // Try elsewhere
      }
  }

return false;
}

bool DeviceAndMountManager::ParseProcScsiScsi(DevicesStruct* tempstruct, long devno, wxArrayString& names)  // Look thru proc/scsi/scsi to deduce the current major no for this device
{
char devicecount = 0;
  // This method has 2 uses:  to load the array names with the device's Model string(s) while configuring; & to see if tempstruct contains (one of) these when a known device has been plugged in
  // We can tell which use is which by seeing which parameter is null.
bool fromconfig = (tempstruct==NULL);
  // The file /proc/scsi/scsi has a list of attached/previously-attached devices.  It maps the device to the file found in ReadScsiUsbStorage(), & importantly (for my machines at least) it gives a multiple count for eg multi-card readers
  // So for a usbpen + 4*multicard + usbpen, with usb-storage filenames 6, 7 & 8,  the scsiX numbers will be scsi6, scsi7, scsi7, scsi7, scsi7, scsi8.
  // The corresponding devicenodes should be /dev/sda,   /dev/sdb /dev/sdc /dev/sdd /dev/sde,  /dev/sdf
  // The 2nd line for a device holds the vendor & model names (etc).  The model name is different to that in /proc/scsi/usb-storage* which is why we have to look at both!
FileData fd(SCSISCSI); if (!fd.IsValid())
  { wxString msg; msg.Printf(wxT("%s%s%s"),_("I can't find the file with the list of scsi entries,  "), SCSISCSI.c_str(), _("\n\nYou need to use Configure to sort things out"));
    wxLogMessage(msg); return false;
  }
wxFileInputStream file_input(SCSISCSI);                 // I'd originally used wxTextFile here, but in 2.7.0-8.1 this won't work with non-seekables
wxBufferedInputStream input(file_input);                // Without the buffer, only 1 byte gets read !?
wxTextInputStream text(input);
wxArrayString array;
for (; ;)
  { wxString line = text.ReadLine();
    if (input.Eof() && line.IsEmpty()) break;
    array.Add(line);
  }

size_t count = array.GetCount(); if (!count) return false;

for (size_t n=0; n < count;)
  { wxString line1 = array[n++];                        // Read a line
    wxStringTokenizer tkz(line1);                       // Tokenise it
    if (tkz.HasMoreTokens())                            // Make sure it's not blank!
      { wxString token = tkz.GetNextToken();
        if (token != wxString(wxT("Host:"))) continue;  // Each device's info is spread over 3 lines.  The first starts with Host:
        token = tkz.GetNextToken();                     // This one should be "scsi0" or "scsi1" etc
        if (token.Left(4) != wxString(wxT("scsi"))) continue;
        long thisone; if (!token.Mid(4).ToLong(&thisone)) continue;  // Get the "0" or "1" bit as an int
        
        wxString line2 = array[n++];
        wxString line3 = array[n++];
        wxStringTokenizer tkz3(line3);                  // Now parse line 3 of the device's info.  It should say "Type:   Direct-Access"
        if (tkz3.HasMoreTokens())
          { wxString token = tkz3.GetNextToken();
            if (token != wxString(wxT("Type:"))) continue;
            if (tkz3.GetNextToken() != wxString(wxT("Direct-Access"))) continue;  // If it doesn't, we don't want to know about this device
          }
        
        if (thisone < devno) { ++devicecount; continue; }  // It's a valid usb device. If its number is less than that of the device we're interested in, just inc the counter and loop
        if (thisone > devno) 
          { if (!fromconfig) return false;              // I don't think this can happen
            return !names.IsEmpty();                    // If from config, return false if no names were found
          }
        
        wxStringTokenizer tkz2(line2);
        if (tkz2.HasMoreTokens())
          { wxString token = tkz2.GetNextToken();
            if (token != wxString(wxT("Vendor:"))) continue;
            if (fromconfig)                             // If we're configuring, we need to find the Model name for this device
              { while (tkz2.HasMoreTokens())
                  { wxString token = tkz2.GetNextToken();  // Carry on thru the line until we find the label Model:
                    if (token != wxString(wxT("Model:"))) continue;
                    wxString remainder = tkz2.GetString(); // Get the rest of the line.  It has our info, plus unwanted bits starting with Rev:
                    size_t end = remainder.find(wxT("Rev:"));  // So chop off the unwanted bits
                    if (end != wxString::npos)          // If end is a sensible figure, we have our info.  Add it to the array
                      { remainder = remainder.Left(end); names.Add(remainder); }
                    break;
                  }
                continue;                               // Don't stop here.  Multicard readers will have more matching entries
              }
                              // If we're here, we're matching a device, not configuring it
            if (line2.find(tempstruct->name1) != wxString::npos && line2.find(tempstruct->name3) != wxString::npos)
              {              // If the vendor & model match the passed device, we're there
                tempstruct->devicenode = AUTOMOUNT_USB_PREFIX + wxChar(wxT('a') + devicecount);   // ie if devicecount is 2, devicenode will be /dev/sdc
                return true;
              }
             else { ++devicecount; continue; }          // Otherwise it's a device with the same no but different name:  probably the wrong part of a multi-card reader
          }
    }
  }

if (!fromconfig) return false;                          // I don't think this can happen
return !names.IsEmpty();                                // If from config, return false if no names were found --- rather unlikely too
}


void DeviceAndMountManager::CheckForNovelDevices()  // Look thru proc/scsi/usb-storage*, checking for unknown devices.  Only for non-automounting distros
{
                // Any usb devices should each be attached with a dir under /proc/scsi/, probably called usb-storage-0, usb-storage-2 etc
                // However these dirs aren't created until a device is 1st attached (after each computer boot), though once there, they persist until a reboot
wxArrayString dirarray;

wxLogNull WeDontWantMissingDirMsgs;
wxString dirname = wxFindFirstFile(SCSIUSBDIR + wxT("*"), wxDIR);  // Get 1st subdir that matches, probably /proc/scsi/usb-storage-0
while (!dirname.IsEmpty())
  { dirarray.Add(dirname);                              // Add the dir to array
    dirname = wxFindNextFile();                         // Now try for /proc/scsi/usb-storage-1 etc
  }

dirarray.Sort();                                        // wxFindFirstFile finds the dirs randomly.  Sort so that usb-storage-0 precedes usb-storage-1

                // We now have a list of subdirs.  Compare each with known devices
for (size_t n=0; n < dirarray.GetCount(); ++n)
  { wxString filename = wxFindFirstFile(dirarray[n] + wxT("/*"), wxFILE);  // Search thru this directory, acquiring the names of each file
    while (!filename.IsEmpty())
      { wxFile file(filename);                          // Open file filename
        if (!file.IsOpened())  continue;
        if (!file.Exists(filename)) { file.Close(); continue; }
  
        char buf[500]; 
        off_t cnt = file.Read(buf, 500); file.Close(); if (!cnt)  continue;
        wxString contents(buf, wxConvUTF8, cnt);        // Make the contents into a wxString

        bool flag=false;
        size_t count=deviceman->FirstOtherDevice, child=0; DevicesStruct* tempstruct;
        while ((tempstruct = deviceman->GetNextDevice(count, child)) != NULL)  // Use GetNextDevice() as it searches children too.  It autoincs the two indices  
          if (contents.Contains(tempstruct->name1) && contents.Contains(tempstruct->name2))  // This is a previously known device
              { flag=true; break; }
          
        if (flag) { filename = wxFindNextFile(); continue; }  // Device was recognised, so ignore & try another

        wxStringTokenizer tkz(contents, wxT(":\n"));    // Aha!We don't know this one, so look thru for the relevant data
        wxString product, vendor;
        while (tkz.HasMoreTokens())
          { wxString token = tkz.GetNextToken();        // Look first for "Vendor"
            if (!token.Contains(wxT("Vendor")))  continue;
            if (tkz.HasMoreTokens())  
              { vendor = tkz.GetNextToken();            // Store the vendor bit of the description
                vendor.Trim(false);                     //   less the preceding white-space
                if (tkz.HasMoreTokens() && (tkz.GetNextToken().Contains(wxT("Product"))))
                  { product = tkz.GetNextToken(); product.Trim(false); }
                break;
              }
          }
        
        usbtimer->Stop();                               // Stop the timer, to prevent re-entry, then configure the new device

        DevicesStruct* newstruct = new DevicesStruct;
        newstruct->name1 = newstruct->Vendorstr = vendor; newstruct->name2 = newstruct->Productstr = product; newstruct->devicenode = AUTOMOUNT_USB_PREFIX+wxT("a1"); // == /dev/sda1

        MyButtonDialog dlg;
        wxXmlResource::Get()->LoadDialog(&dlg,MyFrame::mainframe, wxT("NewDeviceDetectedDlg"));
        wxString title = (product.Len() > vendor.Len() ? product : vendor);  // Use the longer string as the dlg title
        dlg.SetTitle(title);
        int ans = dlg.ShowModal();
        if (ans == wxID_OK) ConfigureNewDevice(newstruct);
          else if (ans == wxID_CANCEL) newstruct->ignore = DS_true;
                else newstruct->ignore = DS_defer;
        
        long no; wxArrayString names; bool found = false;
        if (filename.AfterLast(wxFILE_SEP_PATH).ToLong(&no))  // The file we found above is called "1" or "14" etc.  Make it into an int
          found = ParseProcScsiScsi((DevicesStruct*)NULL, no, names);  // Use this to find the model name(s)
        if (found)
          for (size_t n=0; n < names.GetCount(); ++n)   // For every name in array:  usually 1, but a multicard reader may have more
            { newstruct->name3 = names[n];              // Store the model name we just found
              newstruct->buttonnumber = GetUniqueDeviceButtonNo(); newstruct->HasButton = false; newstruct->defaultpartition = 0;
              deviceman->Add(newstruct);                // Store it in array
              if (n+1 < names.GetCount())               // Now see if there'll be another iteration of the loop.  If so, clone the struct so it can be added again.
                newstruct = new DevicesStruct(*newstruct);
            }
         else    // Shouldn't happen, but if we couldn't find anything in /proc/scsi/scsi, store what data we have
          { newstruct->buttonnumber = GetUniqueDeviceButtonNo(); // Set a no. to id the struct & its button
            newstruct->HasButton = false; newstruct->defaultpartition = 0;
            deviceman->Add(newstruct);                  // Whatever the outcome, store it in array.  If we don't, it'll keep on being queried
          }
        
        if (newstruct->ignore != DS_defer) deviceman->SaveUsbDevices();  // Similarly, unless we want the message next time, save configuration
        
        usbtimer->Start();
        filename = wxFindNextFile();                    // Try another file
      }
  }
}

void DeviceAndMountManager::CheckForAutomountedNovelDevices(wxArrayString& partitions, wxArrayString& mountpts)  // Similarly, for mtab-style auto-mounting distros
{
if (DEVICE_DISCOVERY_TYPE == SysHal) return;            // Not ones using HAL eg SuSE >= 9.2, as CheckForHALAutomountedDevices() does the novel ones too

for (size_t n = 0; n < mountpts.GetCount(); ++n)        // For each usb-type mount-point in existence
  { bool flag = false; size_t count=deviceman->FirstOtherDevice, child=0; DevicesStruct* tempstruct;
    while ((tempstruct = deviceman->GetNextDevice(count, child)) != NULL)  // Use GetNextDevice() as it searches children too.  It autoincs the two indices
      if (mountpts[n] == tempstruct->name1)             // This is a previously known device
          { flag=true; break; }

    if (flag) continue;
                              // Before we decide this is new, check that this isn't a multipartition device with multiple mountpts:  we wouldn't want a button for each
    char last = partitions[n].at(partitions[n].size() -1);  // Get the last char of the string, see if it's a digit & if so >1
    if (wxIsdigit(last) && last > wxT('1')) continue;   // So /dev/sda or /dev/sda1 are OK, but /dev/sda2 etc are ignored
    
                              // We've not seen this one before
                          
    usbtimer->Stop();                                   // Stop the timer, to prevent re-entry, then configure the new device

    DevicesStruct* newstruct = new DevicesStruct;
    newstruct->name1 = mountpts[n]; newstruct->name2.Empty(); newstruct->name3.Empty(); newstruct->devicenode = AUTOMOUNT_USB_PREFIX+wxT("a1"); // == /dev/sda1;

    MyButtonDialog dlg;
    wxXmlResource::Get()->LoadDialog(&dlg,MyFrame::mainframe, wxT("NewDeviceDetectedDlg"));
    dlg.SetTitle(mountpts[n]);
    int ans = dlg.ShowModal();
    if (ans == wxID_OK) ConfigureNewDevice(newstruct);
      else if (ans == wxID_CANCEL) newstruct->ignore = DS_true;
            else newstruct->ignore = DS_defer;
    
    newstruct->buttonnumber = GetUniqueDeviceButtonNo();// Set a no.  to id the struct & its button
    newstruct->HasButton = false; newstruct->defaultpartition = 0;
    
    deviceman->Add(newstruct);                          // Whatever the outcome, store it in array.  If we don't, it'll keep on being queried
    if (newstruct->ignore != DS_defer) deviceman->SaveUsbDevices();  // Similarly, unless we want the message next time, save configuration
    
    usbtimer->Start();
  }
}

enum { NoMatch, Match, NewChild, Replug, Fubar }; // Possible return codes for MatchHALInfoToDevicestruct()

int DeviceAndMountManager::MatchHALInfoToDevicestruct(DevicesStruct* tempstruct, int idVendor, int idProduct, wxString& vendor, wxString& model, wxString& devnode)    // See if this devicestruct matches the host/model data from HAL
{
if ((!idVendor && !idProduct) && vendor.IsEmpty() && model.IsEmpty()) return Fubar;

if (tempstruct->idVendor) // We used to rely on the names; the new way is to use IDs instead. This copes with the occasional venderless device
  { if (!((tempstruct->idVendor == idVendor) && (tempstruct->idProduct == idProduct)))
      return NoMatch;                                   // If the vendor & model don't match, forget it
    // OK, we know the device. However it might be a different slot in a multicard reader
    if (!(tempstruct->name1==vendor && tempstruct->name2==model))
      return NoMatch;                                   // Yes, it's a different slot
  }
 else
   if (!(tempstruct->name1==vendor && tempstruct->name2==model)) return NoMatch;  // Ditto, the old way using strings

  // OK, they match.  4 possibilities: This might be an attachment of a known device;  it might be a duplicate attachment as the user has 2 identical devices;  it might be a replugin before we'd noticed the unplug;  or Hal might be fubared
if (tempstruct->fname.IsEmpty()) return Match;          // Signal that this is the correct struct, ready for attachment

wxString dev = tempstruct->devicenode.AfterLast(wxFILE_SEP_PATH);
if (dev.Left(3) == devnode.Left(3))                     // If there's already a stored device, see if it's a valid replug or rubbish
  { wxArrayString partitions, mountpts;
    SearchTable(partitions, mountpts, AUTOMOUNT_USB_PREFIX + devnode.Mid(2));// Load arrays with the mountpt corresponding to each partition of devnode.  Prepend (eg)  '/dev/sd' to (eg) 'a' 
    if (partitions.GetCount() != tempstruct->partitions.GetCount())  return Fubar;  //  If the no of partitions differs,  Hal must be having conniptions again
                
    for (size_t n=0; n < partitions.GetCount();  ++n)   // Check every device entry, as partitions will be mounted individually  eg sda1, sda2 etc have different mountpts  
        if (tempstruct->mountpts[n] != mountpts[n])     // If the old/new mountpts aren't identical
                  return Fubar;                         //  Hal/sys must be having conniptions again        

    return Replug;                                      // Otherwise it must've been a quick unplug/replug.  Tell caller so that fname can be updated 
  }
                                                        // If we're here, there are 2 or more different devices with the same host/model info
if (tempstruct->haschild)  return NoMatch;              // If this duplication is known, abort here.  The function will be called again with the child tempstruct

                          // If we're here, there are more identical devices attached than we have structs for, so add another one
DevicesStruct* newstruct = new DevicesStruct(*tempstruct);  // Make a clone struct
newstruct->defaultpartition = 0; newstruct->mountpts.Clear();
newstruct->buttonnumber = GetUniqueDeviceButtonNo();    // Set a no. to id the struct & its button
deviceman->Add(newstruct);                              // Add to the array.  Since the names are identical, it'll end up as a child of tempstruct
return NewChild;                                        // The function will be called again in 3 seconds time, with the new tempstruct
}

bool DeviceAndMountManager::IsRemovable(const wxString& dev)  // Is this a fixed e.g. sata device, or a removable e.g. usb pen
{
if (dev.IsEmpty()) return false;

wxString filename(wxT("/sys/block/")); filename << dev.Left(3)// This should make it /sys/block/sda (as opposed to sda2 etc)
                                << wxT("/removable");         // and now /sys/block/sda/removable
FileData stat(filename); if (!stat.IsValid()) return true;    // The file *should* exist, but in case it doesn't, the least-harmful response is to assume it *is* removable

wxFileInputStream file_input(filename); wxBufferedInputStream input(file_input); wxTextInputStream text(input);
return text.ReadLine() == wxT("1");                     // The file should contain 0 or 1
}

void DeviceAndMountManager::CheckForHALAutomountedDevices()  // Check for any other auto-mountables eg USB devices, in Hal-automounting distros
{
static wxArrayString AttachedUsbDevices;                // Holds strings like "8:0:0:0", which are the symlinks from (probably) /sys/bus/scsi/devices, one for each plugged-in usbpen etc
                                                        // New ones are added when they appear, old ones deleted when they're unplugged

wxArrayString CurrentAttached;                          // This string-array is temporary, to hold the entries present on this iteration
                                // In what follows,  SCSI_DEVICES is probably "/sys/bus/scsi/devices", where some HAL/Sys data can be found
wxLogNull WeDontWantMissingDirMsgs;
//wxDir::GetAllFiles(SCSI_DEVICES, &CurrentAttached, wxEmptyString, wxDIR_FILES);  // This doesn't currently work:  GetAllFiles() ignores contents as they're symlinks-to-dirs
wxDir d; d.Open(SCSI_DEVICES); if (!d.IsOpened()) return;                          // So DIY
wxString eachFilename;
if (d.GetFirst(&eachFilename))
  do if ((eachFilename != wxT(".")) && (eachFilename != wxT("..")))
       CurrentAttached.Add(SCSI_DEVICES + wxFILE_SEP_PATH + eachFilename);  // Load each file (actually they're symlinks) into the array
    while (d.GetNext(&eachFilename));

                                // First look for any previously-attached devices that have just been removed
for (size_t n=AttachedUsbDevices.GetCount(); n > 0; --n)  // Go thru from the top down as we're removing
  if (CurrentAttached.Index(AttachedUsbDevices.Item(n-1)) == wxNOT_FOUND)    // This one isn't in the new list, so look for it in the list of possible usb devices
    { size_t count=deviceman->FirstOtherDevice, child=0; DevicesStruct* tempstruct;
      while ((tempstruct = deviceman->GetNextDevice(count, child)) != NULL)  // Use GetNextDevice() as it searches children too.  It autoincs the two indices  
        { if (tempstruct->fname == AttachedUsbDevices.Item(n-1))
            { for (size_t p = 0; p < tempstruct->mountpts.GetCount(); ++p)   // For every partition in this device (usually 1, but you never know)  
                MyFrame::mainframe->OnUnmountDevice(tempstruct->mountpts[p]);// Replace it in any pane it's showing in, with the default startdir

              if (!(SUPPRESS_EMPTY_USBDEVICES && !tempstruct->HasButton))  // Now remove the button from its sizer, and delete it, provided it wasn't suppressed because the device was empty
                if (tempstruct->button != NULL)          
                  { if (MyFrame::mainframe->sizerControls->Detach(tempstruct->button))
                      { tempstruct->button->Destroy(); tempstruct->button = NULL; MyFrame::mainframe->sizerControls->Layout(); }
                  }
                
              tempstruct->mountpts.Empty(); tempstruct->partitions.Empty(); tempstruct->devicenode.Empty();  // Remove the mountpt, partitions & device entries
              tempstruct->fname.Empty(); tempstruct->HasButton = false; // Mark unattached
              // DON'T leave the loop here: multicard readers will have >1 entries, each with the same fname
            }
        }
            
      AttachedUsbDevices.RemoveAt(n-1);                 // Now remove from the string array too
    }

                                // Now go thru looking for newly-attached devices
for (size_t n=0; n < CurrentAttached.GetCount(); ++n)
  if (AttachedUsbDevices.Index(CurrentAttached.Item(n)) == wxNOT_FOUND)    // This one isn't in the old list
    { int idVendor = 0, idProduct = 0; wxString vendor, model, vendoridstr, productidstr, devnode; bool flag = false;
      if (!ExtractHalData(CurrentAttached.Item(n), idVendor, idProduct, vendor, model, vendoridstr, productidstr, devnode)) continue;  // Get the data about the device

      size_t count=deviceman->FirstOtherDevice, child=0; DevicesStruct* tempstruct;    //   & see if it's one we've met already
      while ((tempstruct = deviceman->GetNextDevice(count, child)) != NULL)  // Use GetNextDevice() as it searches children too.  It autoincs the two indices
        switch(MatchHALInfoToDevicestruct(tempstruct, idVendor, idProduct, vendor, model, devnode))
          { case NoMatch: continue;                                       // This tempstruct doesn't match.  Try another
            case Match: if (tempstruct->ignore) { flag = true; break; }   // We've found a known device.  Check that we care

                        if (USB_DEVICES_AUTOMOUNT)                        // Most Hal/sys systems automount
                           SearchTable(tempstruct->partitions, tempstruct->mountpts, AUTOMOUNT_USB_PREFIX + devnode.Mid(2));// Get the mountpt corresponding to each partition of devnode.  Prepended (eg)  '/dev/sd' to (eg) 'a' 
                          else                                            //   but some don't e.g. Mepis 6.5, so look in fstab to see if the user has put a mountpt there
                           SearchFstab(tempstruct->partitions, tempstruct->mountpts, AUTOMOUNT_USB_PREFIX + devnode.Mid(2));
                      
                        tempstruct->devicenode = wxT("/dev/") + devnode;  // Store the device node
                        tempstruct->fname = CurrentAttached.Item(n);      // Store the filename of the device's symlink, so we'll know we've already noticed it
                        AttachedUsbDevices.Add(CurrentAttached.Item(n));  // Store it in the "permanent" array too, for the same reason

                        if (SUPPRESS_EMPTY_USBDEVICES && tempstruct->partitions.IsEmpty())  // If this device has no partitions, we may have chosen not to display
                          { tempstruct->HasButton = false; flag = true; break; }            // If so, the false HasButton reminds us there's no button to remove
                        DeviceBitmapButton* button;
                        button = new DeviceBitmapButton(deviceman->DevInfo->GetCat(tempstruct->devicetype)); // There's a new device attached.  Make an appropriate button
                        if (button==NULL)  { flag = true; break; }

                        button->ButtonNumber = tempstruct->buttonnumber;
                        button->SetName(wxT("Device"));                   // Specify the name, so we can tell that it's a device rather that an editor
                        tempstruct->button = button;                      // Store a ptr to the button, so that it can be located for removal when detached
                        tempstruct->HasButton = true;                     // Flag that this button is being displayed
                        if (!(tempstruct->Vendorstr.IsEmpty() && tempstruct->Productstr.IsEmpty())) // The vendor/model is probably a more useful tip than 'usbpen'
                          button->SetToolTip(tempstruct->Vendorstr + wxT(" ") + tempstruct->Productstr);

                        MyFrame::mainframe->sizerControls->Add(button, 0, wxEXPAND | wxLEFT, 5);  // Add button to panelette sizer
                        MyFrame::mainframe->sizerControls->Layout();
                        flag = true; break;
            case NewChild:  count=deviceman->FirstOtherDevice; child=0; continue;  // A child struct has just been added. We need to restart the 'while' loop to process it
            case Replug:    tempstruct->fname = CurrentAttached.Item(n);  // The device has been unplugged & quickly replugged, so only the fname has changed
                        flag = true; break;
            case Fubar: tempstruct->fname.Empty();                        // Something in the Hal/sys/udev system has gone horribly wrong, usually because devices have been swapped around too quickly
                        flag = true; break;                               // There is no known (to me) answer short of a reboot
          }

      if (flag) continue;                               // If we've dealt with this device successfully, get another
      
      usbtimer->Stop();                                 // If we're here, we've not seen this one before. Stop the timer, to prevent re-entry, then configure the new device

      DevicesStruct* newstruct = new DevicesStruct;
      newstruct->idVendor = idVendor; newstruct->idProduct = idProduct;
      newstruct->name1 = newstruct->Vendorstr = vendor; newstruct->name2 = newstruct->Productstr = model;

      if (!IsRemovable(devnode)) newstruct->ignore = DS_true;  // Don't query fixed devices e.g. sata hard-drives
        else
          { bool already_known_multi = false;           // Next, see if this is e.g. another slot in an already-known multicard reader
            size_t count=deviceman->FirstOtherDevice; DevicesStruct* tempstruct;
            while ((tempstruct = FindStructForDevice(count++)) != NULL) // For every usb device
              { if ((tempstruct->idVendor == newstruct->idVendor) && (tempstruct->idProduct == newstruct->idProduct))
                  { already_known_multi = true;         // Yes, already known so don't spam the user with n dialogs for an n-slot reader!
                    newstruct->devicetype = tempstruct->devicetype;
                    break;
                  } 
              }
            
            if (!already_known_multi)                   // If this is a genuinely unknown device, we may need to ask the user which icon to use etc
              { int type = ParseUsbIdsFile(vendoridstr, productidstr); // But first see if this device is 'known' to usb.ids
                if (type != wxNOT_FOUND) newstruct->devicetype = type;
                 else
                  { MyButtonDialog dlg;                 // No, there's no shortcut. We need to ask
                    wxXmlResource::Get()->LoadDialog(&dlg,MyFrame::mainframe, wxT("NewDeviceDetectedDlg"));
                    dlg.SetTitle(newstruct->Vendorstr + wxT("  ") + newstruct->Productstr);
                    int ans = dlg.ShowModal();
                    if (ans == wxID_OK) ConfigureNewDevice(newstruct);
                      else if (ans == wxID_CANCEL) newstruct->ignore = DS_true;
                            else newstruct->ignore = DS_defer;
                  }
              }
          }
      
      wxString devpath = AUTOMOUNT_USB_PREFIX.BeforeLast(wxFILE_SEP_PATH) + wxFILE_SEP_PATH;
      SearchTable(newstruct->partitions, newstruct->mountpts, devpath + devnode.Left(3));
      newstruct->buttonnumber = GetUniqueDeviceButtonNo();  newstruct->HasButton = false; newstruct->defaultpartition = 0;
      newstruct->haschild = false; newstruct->next = NULL;

      deviceman->Add(newstruct);                        // Whatever the outcome, store it in array.  If we don't, it'll keep on being queried
      if (newstruct->ignore != DS_defer) deviceman->SaveUsbDevices();  // Similarly, unless we want the message next time, save configuration
      
      flag = false; usbtimer->Start();                  // Reset & loop for any other devices
    }
  
if (!SUPPRESS_EMPTY_USBDEVICES) // Even if we don't care if empty devices have an icon, we have to check for new (un)plugs, to keep the partition data up-to-date, & to undisplay from any pane displaying removed data
  { size_t count=deviceman->FirstOtherDevice, child=0; DevicesStruct* tempstruct;
    while ((tempstruct = deviceman->GetNextDevice(count, child)) != NULL)  // For every usb device
      { if (tempstruct->ignore || tempstruct->fname.IsEmpty()) continue;   // Ignore if we're supposed to, or if there's no fname (perhaps due to fubar above)
        wxArrayString partitions, mountpts;  // Use temporary arrays here, not the tempstruct ones.  Otherwise the mountpts will be cleared before they can be unmounted
        wxString devpath = tempstruct->devicenode.BeforeLast(wxFILE_SEP_PATH) + wxFILE_SEP_PATH;
        SearchTable(partitions, mountpts, devpath + (tempstruct->devicenode.AfterLast(wxFILE_SEP_PATH)).Left(3));  // Find the _current_ state, then compare it with the stored state
        
        if (partitions.GetCount() == tempstruct->partitions.GetCount()) continue;                              // If they're the same, that's all folks
        
        if (partitions.IsEmpty())                       // If the partition count is newly empty, undisplay any "mounts"
          { for (size_t p = 0; p < tempstruct->mountpts.GetCount(); ++p)   MyFrame::mainframe->OnUnmountDevice(tempstruct->mountpts[p]);
            tempstruct->partitions.Empty(); tempstruct->mountpts.Empty(); // Delete the old data
            continue;
          }
        tempstruct->partitions = partitions; tempstruct->mountpts = mountpts;  // If new partitions were found, just copy in the new data
        continue;
      }
    return;
  }

                      // If we ARE suppressing empties, we now have to go thru each device to check that eg no memory cards have been (un)plugged, so that the display status should change
size_t count=deviceman->FirstOtherDevice, child=0; DevicesStruct* tempstruct;
while ((tempstruct = deviceman->GetNextDevice(count, child)) != NULL)     // For every usb device
  { if (tempstruct->ignore || tempstruct->fname.IsEmpty()) continue;      // Ignore if we're supposed to, or if there's no fname (perhaps due to fubar above)
    
    if (!tempstruct->HasButton)                     // This device is flagged as empty
      { wxString devpath = tempstruct->devicenode.BeforeLast(wxFILE_SEP_PATH) + wxFILE_SEP_PATH;
        SearchTable(tempstruct->partitions, tempstruct->mountpts, devpath + (tempstruct->devicenode.AfterLast(wxFILE_SEP_PATH)).Left(3));
        if (tempstruct->partitions.GetCount())      // The previously-empty device has just had a card inserted (or equivalent)
          { DeviceBitmapButton* button = new DeviceBitmapButton(deviceman->DevInfo->GetCat(tempstruct->devicetype)); // There's a new device attached.  Make an appropriate button
            if (button==NULL)  continue;

            
            button->ButtonNumber = tempstruct->buttonnumber;
            button->SetName(wxT("Device"));
            tempstruct->button = button;
            tempstruct->HasButton = true;
            MyFrame::mainframe->sizerControls->Add(button, 0, wxEXPAND | wxLEFT, 5);
            MyFrame::mainframe->sizerControls->Layout();
          }
      }
    else              // See if a device has just had a card removed (or equivalent)
      { wxArrayString partitions, mountpts;         // Use temporary arrays here, not the tempstruct ones.  Otherwise the mountpts will be cleared before they can be unmounted
        wxString devpath = tempstruct->devicenode.BeforeLast(wxFILE_SEP_PATH) + wxFILE_SEP_PATH;
        SearchTable(partitions, mountpts, devpath + (tempstruct->devicenode.AfterLast(wxFILE_SEP_PATH)).Left(3));
        if (partitions.IsEmpty())                   // If the partition count is newly empty, remove the button
          { for (size_t p = 0; p < tempstruct->mountpts.GetCount(); ++p)   // Similarly this is recycled from the "just been removed" section above
              MyFrame::mainframe->OnUnmountDevice(tempstruct->mountpts[p]);
            
            tempstruct->partitions.Empty(); tempstruct->mountpts.Empty();  // We can now delete the old data
            
            if (tempstruct->button != NULL)          
              { if (MyFrame::mainframe->sizerControls->Detach(tempstruct->button))
                     { tempstruct->button->Destroy(); tempstruct->button = NULL; MyFrame::mainframe->sizerControls->Layout(); }
              }              
            tempstruct->HasButton = false;
          }
      }
  }
}

bool DeviceAndMountManager::ExtractHalData(wxString& symlink, int& idVendor, int& idProduct, wxString& vendor, wxString& model, wxString& vendoridstr, wxString& productidstr, wxString& devnode)  // Searches in the innards of Hal/sys to extract a usb device's data
{
        // There's a dir, probably /sys/bus/scsi/devices, which  contains symlinks, 1 for each plugged-in usb device (or several for a multi-card reader)
        // The passed wxString symlink is one of those.  It looks like 8:0:0:0, or for a multi-card reader  9:0:0:0, 9:0:0:1, 9:0:0:2, 9:0:0:3
FileData stat(symlink); if (!stat.IsValid()) return false;

        // Find the symlink target, which will be something like /sys/devices/pci0000:00/0000:00:02.1/usb2/2-4/2-4:1.0/host10/target8:0:0/8:0:0:0
wxString target = stat.GetUltimateDestination(); if (target.IsEmpty()) return false;

        // We now have the target directory, which contains 2 files with vendor and model strings
        // However there's also a higher dir (.../usb2/2-4/ in the above example) with their IDs
wxTextFile file;
size_t pos = target.rfind(wxT("usb"));
if (pos != wxString::npos)
  { pos = target.find(wxT('/'), pos);
    if (pos != wxString::npos)
      { pos = target.find(wxT('/'), pos+1); // This should give us the position of the second '/' after usb
        if (pos != wxString::npos)
          { wxString higherdir = target.Left(pos);
            file.Open(higherdir + wxT("/idVendor"));  // Read the file with the vendor id
            if (file.IsOpened())
              { long l;
                for (size_t line = 0; line < file.GetLineCount(); ++line)  vendoridstr << file.GetLine(line);  // Get each line of data into string --- there's probably only 1
                file.Close();
                vendoridstr.Trim();  // as there's sometimes some spurious terminal white-space
                if (vendoridstr.ToLong(&l, 16)) idVendor = (int)l;
              }

            file.Open(higherdir + wxT("/idProduct"));  // and product id
            if (file.IsOpened())
              { long l;
                for (size_t line = 0; line < file.GetLineCount(); ++line)  productidstr << file.GetLine(line);  // Get each line of data into string --- there's probably only 1
                file.Close();
                productidstr.Trim();
                if (productidstr.ToLong(&l, 16)) idProduct = (int)l;
              }
          }
      }
  }

      // OK, now we're back on 'target'
file.Open(target + wxT("/vendor"));  // Read the file with the vendor name
if (file.IsOpened())
  { for (size_t line = 0; line < file.GetLineCount(); ++line)  vendor += file.GetLine(line);
    file.Close();
    vendor.Trim();
    if (vendor.empty()) vendor = wxT("ID: ") + vendoridstr;  // Irritatingly, occasional devices claim no vendor. Use the id as a backstop
  }

file.Open(target + wxT("/model"));   // Ditto model
if (file.IsOpened())
  { for (size_t line = 0; line < file.GetLineCount(); ++line)  model += file.GetLine(line);
    file.Close();
    model.Trim(true).Trim(false);
    if (model.empty()) model = wxT("ID: ") + productidstr;   // Similarly, though I've not yet found an instance of this
  }

/*static bool logging(false);
if (logging)
  wxPrintf("target=%s IDs %x %x vendor %s model %s productidstr %s\n", target,idVendor,idProduct,vendor,model, productidstr);*/

        // The device name is more complicated.  The dir contains a symlink, called 'block' in most distros, pointing to /sys/block/sda or whichever, so we just need the sda bit for device
        // Note that the minor no isn't available, but we shouldn't really need it (Stop Press: it is in fedora9, kernel 2.6.25-14)
wxDir d; d.Open(target); if (!d.IsOpened()) return false;  // However FC5 calls the symlink 'block:sda' etc. so we have to cater for this too. Sigh!
wxString filename; if (!d.GetFirst(&filename, wxT("block*"))) return false;

target = target + wxFILE_SEP_PATH + filename;
FileData block(target); if (!block.IsValid())  return false;
if (block.IsSymlink())  devnode = (block.GetUltimateDestination()).AfterLast(wxFILE_SEP_PATH); // Until fedora9/kernel 2.6.25-14 'block' was a symlink  
  else                                  // Since then, it's become a dir, with the desired filename as a subdir
    { wxDir d; d.Open(target); if (!d.IsOpened()) return false;
      wxString fname; if (!d.GetFirst(&fname)) return false;
      devnode = fname;
      devnode.Trim();  // Probably unnecessary, but will do no harm
    }

        // One last quibble: the latest, all-scsi, kernels call (fixed) cdroms sr0, sr1 etc. We don't want these here
if (devnode.Left(2) == wxT("sr")) return false;

return true;
}

void DeviceAndMountManager::CheckForAutomountedDevices()  // Check for any other auto-mountables eg USB devices, in non-HAL automounting distros
{

if (DEVICE_DISCOVERY_TYPE == SysHal) return CheckForHALAutomountedDevices();  // If this is a Hal distro, do it differently

wxArrayString partitions, mountpts;
SearchMtab(partitions, mountpts);                     // Load arrays with the /dev & mountpt data
CheckForAutomountedNovelDevices(partitions, mountpts);// Make sure there aren't any novelties

size_t count=deviceman->FirstOtherDevice, child=0; DevicesStruct* tempstruct;
while ((tempstruct = deviceman->GetNextDevice(count, child)) != NULL)    // For every usb device
  { if (tempstruct->ignore) continue;                 // If we've been told to ignore this device, do so

    int attached = mountpts.Index(tempstruct->name1); // See if it's currently attached, & if so, in which dir    
    if (attached == wxNOT_FOUND)
      { if (!tempstruct->HasButton)  continue;        // If it's not attached, & we weren't expecting it to be, try another device
                                // If it's not attached but there IS a button, the device must just have been removed, so unmount anything & remove button
        for (size_t p = 0; p < tempstruct->mountpts.GetCount(); ++p)
          if (!tempstruct->mountpts[p].IsEmpty())     // If it's 'mounted', unmount
            { MyFrame::mainframe->OnUnmountDevice(tempstruct->mountpts[p]);  // Replace it in its pane with the default startdir
              tempstruct->mountpts[p].Empty();  
            }                  
            
                                // Now remove the button from its sizer, and delete it
        if (tempstruct->button != NULL)
          { if (MyFrame::mainframe->sizerControls->Detach(tempstruct->button))
                 { tempstruct->button->Destroy(); tempstruct->button = NULL; MyFrame::mainframe->sizerControls->Layout(); }
          }
        tempstruct->HasButton = false;
      }
     else                       // We've found an attached device.  Is it already attached?
      { if (tempstruct->HasButton)  continue;         // Yes, this is old news.
                                // No, this is a new attachment.  Put the correct device into the struct, as it depends on the order of device use since booting
        wxString devpath = (partitions.Item(attached)).BeforeLast(wxFILE_SEP_PATH) + wxFILE_SEP_PATH;
        SearchMtab(tempstruct->partitions, tempstruct->mountpts, devpath + (partitions.Item(attached)).AfterLast(wxFILE_SEP_PATH).Left(3));  // Fill the arrays with info for each partition

        DeviceBitmapButton* button = new DeviceBitmapButton(deviceman->DevInfo->GetCat(tempstruct->devicetype));// Can be Pen, Mem
        if (button==NULL)  continue;
        
        button->ButtonNumber = tempstruct->buttonnumber;// Provide the ButtonNumber
        button->SetName(wxT("Device"));                 // Specify the name, so we can tell that it's a device
        tempstruct->button = button;                    // Store a ptr to the button, so that it can be located for removal when detached
        tempstruct->HasButton = true;
        
        MyFrame::mainframe->sizerControls->Add(button, 0, wxEXPAND | wxLEFT, 5);  // Add button to panelette sizer
        MyFrame::mainframe->sizerControls->Layout();
      }
  }
}

void DeviceAndMountManager::CheckForGioAutomountedDevices() // Check for devices e.g. cds, mounted by GIO inside /run/user/
{
static int BitmapsAvailable = -1;
if (BitmapsAvailable == -1)
  { BitmapsAvailable = wxFileExists(BITMAPSDIR + "/mtp.png"); if (!BitmapsAvailable) wxLogDebug("gvfs icons not available: probably using a stale BITMAPSDIR"); }
if (!BitmapsAvailable) return;

static size_t UserID = getuid();
static wxString XdgRuntimeDir("");
if (XdgRuntimeDir.IsEmpty()) wxGetEnv("XDG_RUNTIME_DIR", &XdgRuntimeDir);
if (XdgRuntimeDir.IsEmpty() && !wxDirExists(wxGetHomeDir()+"/.gvfs/")) return; // Not in use

static wxString Gvfs;
if (Gvfs.IsEmpty()) Gvfs = XdgRuntimeDir + "/gvfs/";
if (!wxDirExists(Gvfs))
  Gvfs = wxGetHomeDir()+"/.gvfs/";
if (!wxDirExists(Gvfs)) return;


wxDir d; d.Open(Gvfs); wxCHECK_RET(d.IsOpened(), "Failed to open the Gvfs dir");
wxArrayString CurrentlyAttached;
wxString eachFilename;
if (d.GetFirst(&eachFilename))
  do if ((eachFilename != wxT(".")) && (eachFilename != wxT("..")))
       CurrentlyAttached.Add(Gvfs + eachFilename);
    while (d.GetNext(&eachFilename));

  // Check for any removals
for (size_t c = m_gvfsinfo->GetCount(); c > 0; --c)
  { wxString mpt = m_gvfsinfo->GetMountpt(c-1);
    FileData fd(mpt);
    if (!fd.IsValid())
      { GvfsBitmapButton* button = m_gvfsinfo->GetButton(c-1);
        if (button)
          { wxString mountpt = button->GetMountpt();
            wxString revertdir = button->GetOriginalMountpt();
            if (MyFrame::mainframe->sizerControls->Detach(button))
              { button->Destroy(); MyFrame::mainframe->sizerControls->Layout(); 
                MyFrame::mainframe->OnUnmountDevice(mountpt, revertdir);
              }
          }
        m_gvfsinfo->Remove(c-1);
      }
  }
      
  // Now look for additions
bool alreadyKnown(false);
for (size_t c = 0; c < CurrentlyAttached.GetCount(); ++c)
  { wxString mpt = CurrentlyAttached.Item(c);
    if (!mpt.IsEmpty() && m_gvfsinfo->Find(mpt) == wxNOT_FOUND)
      { GvfsBitmapButton* button = new GvfsBitmapButton(MyFrame::mainframe->panelette, mpt);
        button->SetToolTip("Click to open " + mpt);
        m_gvfsinfo->Add(mpt, button);
        
        MyFrame::mainframe->sizerControls->Add(button, 0, wxEXPAND | wxLEFT, 5);  // Add button to panelette sizer
        MyFrame::mainframe->sizerControls->Layout();
      }
  }
}

void DeviceAndMountManager::GetDevicePartitions(wxArrayString& partitions, wxArrayString& mountpts,  wxString dev)  // Parse /proc/partitions for dev's partitions. Return them in array, with any corresponding mountpts
{
if (dev.IsEmpty()) return;
int len = dev.BeforeLast(wxFILE_SEP_PATH).Len() + 4;  // dev should hold "/dev/xxx", so length should be 8, but just in case measure 'path' then add 4 for '/xxx'.  This also avoids problems if dev is "/dev/xxx1"

partitions.Empty(); mountpts.Empty();                 // Empty the arrays of any old data:  we don't want to append!

wxArrayString allpartitions;
GetPartitionList(allpartitions);                      // Load temp array with list of _all_ known partitions.  We then ignore those that don't relate to dev

for (size_t n=0; n < allpartitions.GetCount(); ++n)
  { if (allpartitions[n].Left(len) == dev.Left(len))  // If the 1st 'len' letters of the NAME bit of this partition match dev  ie  /dev/sda1 matches /dev/sda
      { partitions.Add(allpartitions[n]);             //   store it in the real partition array
    
#ifdef __linux__
        struct mntent* mnt = ReadMtab(allpartitions[n]); // Now see if this partition is currently mounted
        if (mnt != NULL)  mountpts.Add(wxString(mnt->mnt_dir, wxConvUTF8));  // If so, store the mountpt
#else
        struct statfs* mnt = ReadMtab(allpartitions[n]);
        if (mnt != NULL) mountpts.Add(wxString(mnt->f_mntonname, wxConvUTF8));
#endif
          else mountpts.Add(wxEmptyString);           // Otherwise store ""
      }
  }
}

void DeviceAndMountManager::SearchTable(wxArrayString& partitions, wxArrayString& mountpts,  wxString dev, enum treerooticon type/*=usbtype*/)  // Relays to either SearchMtab() or SearchFstab()
{
enum discoverytable which = deviceman->DevInfo->GetDiscoveryTableForThisDevicetype(type);
switch(which)
  { case DoNowt:          return GetDevicePartitions(partitions, mountpts, dev);        // Out-of-the-box primitive Gentoo, for example, detects devices but makes no attempt to mount/add to either table :(
    case FstabOrdinary:   return SearchFstab(partitions, mountpts, dev);                // In some distros (eg Mandriva 10.2) hotplug inserts device info into fstab
    case FstabSupermount: return CheckSupermountTab(partitions, mountpts, dev, false);  // In others (eg Mandriva 10.1) hotplug inserts device info into fstab, using the name "none", which supermount prefers.  This is how we cope
    case MtabSupermount:  return CheckSupermountTab(partitions, mountpts, dev, true);   // Ditto but supermount in mtab.  I don't know of any distros that do this
    case MtabOrdinary:
     default:             return SearchMtab(partitions, mountpts, dev);                 // In others (eg SuSE 9.3) hotplug inserts device info into mtab
  }
}

void DeviceAndMountManager::CheckSupermountTab(wxArrayString& partitions, wxArrayString& mountpts,  wxString dev, bool SearchMtab)    // In Supermount systems, finds data for this device
{
wxTextFile file;                                    // Create a textfile
#ifdef __linux__
if (SearchMtab)  file.Create(wxT(_PATH_MOUNTED));   //  using mtab
#else
if (SearchMtab) return;
#endif
 else  file.Create(wxT(_PATH_FSTAB));               //   or fstab
if (!file.Exists())    return;
file.Open(); if (!file.IsOpened())  return;

partitions.Empty(); mountpts.Empty();               // Empty the arrays of any old data:  we don't want to append!

size_t count = file.GetLineCount();

for (size_t n=0; n < count; n++)
  { wxString line = file.GetLine(n);                // Read a line from the file
    if (line.find(dev) != wxString::npos)           // Found a substring matching "/dev/foo", so add this to partitions
      { partitions.Add(dev);

        wxStringTokenizer tkz(line);                // Tokenise it
        if (tkz.HasMoreTokens())
          { wxString token = tkz.GetNextToken();    // Get first token, which should be "none", to ignore it
            token = tkz.GetNextToken();             // Next should be the mount-point
             if (!token.IsEmpty())  mountpts.Add(token);  // Add it to array
          }
        return;                                     // Now stop.  I don't _think_ supermounted devices will have >1 partition;  & if one does, too bad!
      }
  }
}

void DeviceAndMountManager::SearchMtab(wxArrayString& partitions, wxArrayString& mountpts,  wxString dev /*=wxEmptyString*/)  // Loads arrays with data for the named device, or for all devices. NB Only finds mounted partitions
{
#ifdef __linux__
struct mntent* mnt = NULL;
#else
struct statfs* fslist;
#endif

partitions.Empty(); mountpts.Empty();               // Empty the arrays of any old data:  we don't want to append!

#ifdef __linux__
FILE* fmp = setmntent (_PATH_MOUNTED, "r");         // Get a file* to (probably) /etc/mtab
if (fmp==NULL) return;
#else
int numfs = getmntinfo(&fslist, MNT_NOWAIT);
if (numfs < 1) return;
#endif

wxString mask;
if (dev.IsEmpty()) mask = AUTOMOUNT_USB_PREFIX;     // We normally search for all AUTOMOUNT_USB_PREFIX devices, probably "/dev/sd"
  else mask = dev;                                  //   but if dev isn't empty, use this instead

#ifdef __linux__
while (1)                                           // For every mtab entry
  { mnt = getmntent(fmp);                           // Get a struct representing a mounted partition
    if (mnt == NULL) { endmntent(fmp); return; }    // If it's null, we've run out of candidates
    
                          // See if this mount's device begins with the appropriate letters
    if (wxString(mnt->mnt_fsname, wxConvUTF8).Left(mask.Len()) == mask)
      { partitions.Add(wxString(mnt->mnt_fsname, wxConvUTF8));  // If so, store the devnode and mountpt
        mountpts.Add(wxString(mnt->mnt_dir, wxConvUTF8));
#else
for (int i = 0; i < numfs; ++i)
  { if (wxString(fslist[i].f_mntfromname, wxConvUTF8).Left(mask.Len()) == mask)
      { partitions.Add(wxString(fslist[i].f_mntfromname, wxConvUTF8));
        mountpts.Add(wxString(fslist[i].f_mntonname, wxConvUTF8));
#endif
      }
  }
}

void DeviceAndMountManager::SearchMtabForStandardMounts(wxString& device, wxArrayString& mountpts)  // Finds ext2 etc mountpts for the device ie not subfs. Used for DVD-RAM
{
#ifdef __linux__
struct mntent* mnt = NULL;
#else
struct statfs* fslist;
#endif
mountpts.Empty();                                   // Empty the array of any old data:  we don't want to append!

wxString symtarget; FileData fd(device);            // Beware of the symlink
if (fd.IsSymlink()) symtarget = fd.GetUltimateDestination();

#ifdef __linux__
FILE* fmp = setmntent (_PATH_MOUNTED, "r");         // Get a file* to (probably) /etc/mtab
if (fmp==NULL) return;

while (1)                                           // For every mtab entry
  { mnt = getmntent(fmp);                           // Get a struct representing a mounted partition
    if (mnt == NULL) { endmntent(fmp); return; }    // If it's null, we've run out of candidates
    
    bool found = false;
    if (wxString(mnt->mnt_fsname, wxConvUTF8).Left(device.Len()) == device)  found=true;   // See if this is a mount for the device we're interested in
     else if (!symtarget.IsEmpty() && wxString(mnt->mnt_fsname, wxConvUTF8).Left(symtarget.Len()) == symtarget)  found=true;   // Try again with any symlink target
#else
int numfs = getmntinfo(&fslist, MNT_NOWAIT);
if (numfs < 1) return;

for (int i = 0; i < numfs; ++i)
  { bool found = false;
    if (wxString(fslist[i].f_mntfromname, wxConvUTF8).Left(device.Len()) == device) found=true;
    else if (!symtarget.IsEmpty() && wxString(fslist[i].f_mntfromname, wxConvUTF8).Left(symtarget.Len()) == symtarget) found=true;
#endif

     if (!found) continue;                          // Wrong device
     
    for (size_t n=0; n < RealFilesystemsArray.GetCount(); ++n)  // For DVD-RAM, we're only interested in filesystems like ext2 not eg subfs
#ifdef __linux__
      if (wxString(mnt->mnt_type, wxConvUTF8).Left(3) == RealFilesystemsArray.Item(n).Left(3))
          { mountpts.Add(wxString(mnt->mnt_dir, wxConvUTF8)); break; } // If the type is right,  this mountpt is one we want to store  
#else
      if (wxString(fslist[i].f_fstypename, wxConvUTF8).Left(3) == RealFilesystemsArray.Item(n).Left(3))
          { mountpts.Add(wxString(fslist[i].f_mntonname, wxConvUTF8)); break; }
#endif
    }
}

void DeviceAndMountManager::SearchFstab(wxArrayString& partitions, wxArrayString& mountpts,  wxString dev)  // Loads arrays with data for the named device
{
GetDevicePartitions(partitions, mountpts, dev);     // Find all the partitions on this device
for (size_t p=0; p < partitions.GetCount(); ++p)
  { struct fstab* fs = ReadFstab(partitions[p]);    // For each, try to find a corresponding mountpt-to-be entry in fstab
    if (fs != NULL)  mountpts[p] = wxString(fs->fs_file, wxConvUTF8);  // fs_file holds the mountpoint
  }
}

void DeviceAndMountManager::ConfigureNewDevice(DevicesStruct* tempstruct, bool FromEdit/*=false*/)
{
wxDialog dlg;
if (DEVICE_DISCOVERY_TYPE == MtabMethod)            // If this is a distro that automounts using mountpoint, use a slightly different dialog
  wxXmlResource::Get()->LoadDialog(&dlg,MyFrame::mainframe, wxT("ConfigureAutomountDeviceDlg"));
 else    
  { wxXmlResource::Get()->LoadDialog(&dlg,MyFrame::mainframe, wxT("ConfigureDeviceDlg"));
    ((wxTextCtrl*)(dlg.FindWindow(wxT("TxtName2"))))->ChangeValue(tempstruct->Productstr);
  }
if (FromEdit) 
  { dlg.SetTitle(wxT("Edit Device"));
    ((wxCheckBox*)(dlg.FindWindow(wxT("ro"))))->SetValue(tempstruct->readonly==true);
    ((wxCheckBox*)(dlg.FindWindow(wxT("Ignore"))))->SetValue(tempstruct->ignore==DS_true);
  }

wxTextCtrl* TxtName1 = (wxTextCtrl*)(dlg.FindWindow(wxT("TxtName1"))); TxtName1->ChangeValue(tempstruct->Vendorstr);

wxComboBox* TypeCombo = (wxComboBox*)(dlg.FindWindow(wxT("TypeCombo")));
for (size_t n=0; n < deviceman->DevInfo->GetCount(); ++n)
  TypeCombo->Append(deviceman->DevInfo->GetName(n));// Load the known types into the combobox
TypeCombo->Append(wxT("Other"));                    //   plus Other
if (FromEdit)  TypeCombo->SetSelection(tempstruct->devicetype);
  else TypeCombo->SetSelection(6);                  // This will give a default selection of Usb Pen, which is not unreasonable

if (dlg.ShowModal() != wxID_OK) 
  { if (!FromEdit)  tempstruct->ignore = DS_defer;  // If Cancel, assume user doesn't want to bother right now (but not if we're only editing!)
    return;
  }                  

#if defined(__WXX11__)
  tempstruct->devicetype = TypeCombo->GetSelection();
#else
  tempstruct->devicetype = TypeCombo->GetCurrentSelection();
#endif
if (tempstruct->devicetype == (int)deviceman->DevInfo->GetCount())  // If the type-choice was "Other", we need more info
  { wxDialog dlg; wxString name;
    wxXmlResource::Get()->LoadDialog(&dlg,MyFrame::mainframe, wxT("ConfigureNewDeviceTypeDlg"));
    
    wxComboBox* TypeCombo = (wxComboBox*)(dlg.FindWindow(wxT("TypeCombo")));
    TypeCombo->Append(wxT("Hard disc drive"));      // Load the known types into the combobox
    TypeCombo->Append(wxT("Floppy disc drive"));
    TypeCombo->Append(wxT("CD/DVD drive"));
    TypeCombo->Append(wxT("Usb drive"));
    TypeCombo->Append(wxT("Other"));
#if (defined(__WXGTK__) || defined(__WXX11__))
    TypeCombo->SetSelection(0);
#endif
    int ret;
    do
        ret = dlg.ShowModal();
     while ((ret == wxID_OK) && (name = ((wxTextCtrl*)(dlg.FindWindow(wxT("TxtName"))))->GetValue()).IsEmpty());
    
    if (ret == wxID_OK) // ...and if not, we'll end up with the type undefined; but the user can edit it again
      { treerooticon icon = (treerooticon)TypeCombo->GetSelection();
        devicecategory cat; if (icon < usbtype) cat = (devicecategory)icon; 
          else if (icon == usbtype) cat = usbharddrivecat;  // Translate into the most plausible category
                else cat = unknowncat;
      
        deviceman->DevInfo->Add(name, cat, icon);     // Add the new type to the array
        deviceman->DevInfo->Save();
      }
  }

tempstruct->Vendorstr = TxtName1->GetValue();       // as the user may have altered these
if (DEVICE_DISCOVERY_TYPE != MtabMethod) tempstruct->Productstr = ((wxTextCtrl*)(dlg.FindWindow(wxT("TxtName2"))))->GetValue();

tempstruct->readonly = ((wxCheckBox*)(dlg.FindWindow(wxT("ro"))))->IsChecked();
tempstruct->ignore = ((wxCheckBox*)(dlg.FindWindow(wxT("Ignore"))))->IsChecked();
}

void DeviceAndMountManager::ConfigureNewFixedDevice(DevicesStruct* tempstruct, bool FromEdit/*=false*/)
{
wxDialog dlg;
wxXmlResource::Get()->LoadDialog(&dlg,MyFrame::mainframe, wxT("ConfigureFixedDeviceDlg"));
if (FromEdit) dlg.SetTitle(wxT("Edit Device"));

wxComboBox* TypeCombo = (wxComboBox*)(dlg.FindWindow(wxT("TypeCombo")));
wxTextCtrl* TxtName1 = (wxTextCtrl*)(dlg.FindWindow(wxT("TxtName1"))); TxtName1->ChangeValue(tempstruct->devicenode);
wxTextCtrl* TxtName2 = (wxTextCtrl*)(dlg.FindWindow(wxT("TxtName2"))); 
if (tempstruct->mountpts.GetCount())  TxtName2->ChangeValue(tempstruct->mountpts[0]);
wxString nodename(tempstruct->devicenode.AfterLast(wxFILE_SEP_PATH));
if (!FromEdit  && nodename.Left(2) == wxT("fd"))   // If the device is floppy, use FloppyA etc instead of fd0 as the suggested name
      nodename.Printf(wxT("%s%c"),  wxT("Floppy"), wxT('A') + (nodename.GetChar(2) - wxT('0')));
wxTextCtrl* TxtName3 = (wxTextCtrl*)(dlg.FindWindow(wxT("TxtName3")));
if (!FromEdit)  TxtName3->ChangeValue(nodename);
  else TxtName3->ChangeValue(tempstruct->name1);


for (size_t n=0; n < deviceman->DevInfo->GetCount(); ++n)
  TypeCombo->Append(deviceman->DevInfo->GetName(n));// Load the known types into the combobox
TypeCombo->Append(wxT("Other"));                    //   plus Other

if (FromEdit) TypeCombo->SetSelection(tempstruct->devicetype);
 else                  // Try to guess the devicetype from the device name
  { wxString dev(tempstruct->devicenode.AfterLast(wxFILE_SEP_PATH)); 
    if (dev == wxT("cdrom")) TypeCombo->SetSelection(1);
     else if (dev.Left(3) == wxT("cdr")) TypeCombo->SetSelection(2);  // Should cope with cdr, cdrw, cdrecorder
     else if (dev == wxT("dvdrom") || dev == wxT("dvd")) TypeCombo->SetSelection(3);
     else if (dev == wxT("dvdram")) TypeCombo->SetSelection(5);
     else if (dev.Left(4) == wxT("dvdr")) TypeCombo->SetSelection(4);
  }
  
((wxCheckBox*)(dlg.FindWindow(wxT("ro"))))->SetValue(tempstruct->readonly==true);
((wxCheckBox*)(dlg.FindWindow(wxT("Ignore"))))->SetValue(tempstruct->ignore==DS_true);

if (dlg.ShowModal() != wxID_OK) 
  { if (!FromEdit)  tempstruct->ignore = DS_defer;  // If Cancel (and not editing), assume user doesn't want to bother right now
    return;
  }

tempstruct->devicetype = TypeCombo->GetSelection();
if (tempstruct->devicetype == (int)deviceman->DevInfo->GetCount())  // If the type-choice was "Other", we need more info
  { wxDialog dlg; wxString name;
    wxXmlResource::Get()->LoadDialog(&dlg,MyFrame::mainframe, wxT("ConfigureNewDeviceTypeDlg"));
    
    wxComboBox* TypeCombo = (wxComboBox*)(dlg.FindWindow(wxT("TypeCombo")));
    TypeCombo->Append(wxT("Hard disc drive"));      // Load the known types into the combobox
    TypeCombo->Append(wxT("Floppy disc drive"));
    TypeCombo->Append(wxT("CD/DVD drive"));
    TypeCombo->Append(wxT("Usb drive"));
    TypeCombo->Append(wxT("Other"));
#if (defined(__WXGTK__) || defined(__WXX11__))
    TypeCombo->SetSelection(0);
#endif
    do
      dlg.ShowModal();
      while ((name = ((wxTextCtrl*)(dlg.FindWindow(wxT("TxtName"))))->GetValue()).IsEmpty());
    
    treerooticon icon = (treerooticon)TypeCombo->GetSelection();
    devicecategory cat; if (icon < usbtype) cat = (devicecategory)icon; 
      else if (icon == usbtype) cat = usbharddrivecat;  // Translate into the most plausible category
            else cat = unknowncat;
      
    deviceman->DevInfo->Add(name, cat, icon);       // Add the new type to the array
    deviceman->DevInfo->Save();
  }

tempstruct->devicenode = TxtName1->GetValue();      // Reload the data, in case any has been changed (most likely the name)
wxString str(TxtName2->GetValue());
tempstruct->mountpts.Clear(); if (!str.IsEmpty()) tempstruct->mountpts.Add(str);
tempstruct->name1 = TxtName3->GetValue();

tempstruct->readonly = ((wxCheckBox*)(dlg.FindWindow(wxT("ro"))))->IsChecked();
tempstruct->ignore = ((wxCheckBox*)(dlg.FindWindow(wxT("Ignore"))))->IsChecked();
}

DevicesStruct* DeviceAndMountManager::QueryAMountpoint(const wxString& path, bool contains/*=false */)  // Called by MyGenericDirCtrl, to see if a particular path is a valid mountpt eg /mount/floppy
{
if (path.IsEmpty()) return NULL;

size_t count=0, child=0; DevicesStruct* tempstruct;
while ((tempstruct = deviceman->GetNextDevice(count, child)) != NULL)  // For every device
  { for (size_t p=0; p < tempstruct->mountpts.GetCount(); ++p)         //   & in each partition of that device
      { if (tempstruct->ignore || tempstruct->mountpts.Item(p).IsEmpty()) continue;
        if (contains)                                                  // 'contains' flags that we are just as interested in /device/foo as in /device
          { if (path.Contains(tempstruct->mountpts.Item(p)))  return tempstruct; }
         else if (tempstruct->mountpts.Item(p) == path)       return tempstruct;
      }
  }
  
return NULL;                                        // If we're here, there was no match
}

//----------------------------------------------------------------------------------------------------------

void DeviceManager::ConfigureEditors()  // From Configure. Choose what editors to display on the toolbar. Yes, I know that editors aren't devices, but here for historical reasons
{
EditorsDialog dlg;
wxXmlResource::Get()->LoadDialog(&dlg,MyFrame::mainframe, wxT("ConfigEditorsEtcDlg"));
dlg.Init(this);
dlg.ShowModal();

dlg.Save();                                           // Save data, whether on not altered. It's really not worth checking
MyFrame::mainframe->DisplayEditorsInToolbar();        //  and redisplay
MyFrame::mainframe->sizerControls->Layout();          // Do this here, as otherwise we get m_tbText initial display problems in wxX11
}

void EditorsDialog::Init(DeviceManager* dad)
{
parent = dad;

wxConfigBase* config = wxConfigBase::Get(); if (config==NULL)  return;  // Load current data
NoOfEditors = (size_t)config->Read(wxT("/Editors/NoOfEditors"), 0l);

for (size_t n=0; n < NoOfEditors; ++n)
  { bool Ignore, tabbed; long IconNo; wxString str, key=wxT("/Editors/")+CreateSubgroupName(n,NoOfEditors);
    config->Read(key+wxT("/Editor"), &str); Labels.Add(str);
    config->Read(key+wxT("/LaunchString"), &str); LaunchStrings.Add(str);
    wxString empty; config->Read(key+wxT("/WorkingDir"), &empty); WorkingDirs.Add(empty); // Use an empty string here, as it'll usually be empty
    config->Read(key+wxT("/CanUseTabs"), &tabbed, false); UseTabs.Add(tabbed);
    config->Read(key+wxT("/Ignore"), &Ignore, false); Ignores.Add(Ignore);
    config->Read(key+wxT("/IconNo"), &IconNo, 0l); IconNos.Add(IconNo);
  }

LoadIcons();

list = (wxListBox*)FindWindow(wxT("List"));
FillListbox();
}

void EditorsDialog::SaveEditors(wxConfigBase* config /*= NULL*/)
{
int unknown(0);
wxArrayString IconNames;

if (config == NULL)                                           // It'll usually be null, the exception being if coming from ConfigureMisc::OnExportBtn
  { config = wxConfigBase::Get();  if (config == NULL)  return; 
    for (size_t n=0; n < Icons.GetCount(); ++n)               // Cache the icon filenames *before* we delete the group info!
      { wxString Icon; config->Read(wxT("/Editors/Icons/")+CreateSubgroupName(n,Icons.GetCount())+wxT("/Icon"), &Icon);
        IconNames.Add(Icon);
      }
  }
 else
  { LoadIcons();        // If we're saving a .conf file, we must load the icons first; otherwise everything gets the Unknown icon
    IconNames = Icons;  // In this situation Icons will already hold the icon-names
  }

unknown = (MyBitmapButton::UnknownButtonNo != wxNOT_FOUND ? MyBitmapButton::UnknownButtonNo : 0); // Store the no of unknown.xpm too

if (NoOfEditors) config->DeleteGroup(wxT("/Editors"));

config->Write(wxT("/Editors/NoOfEditors"), (long)NoOfEditors);
for (size_t n=0; n < NoOfEditors; ++n)
  { wxString key=wxT("/Editors/")+CreateSubgroupName(n,NoOfEditors);
    config->Write(key+wxT("/Editor"),  Labels[n]);
    config->Write(key+wxT("/LaunchString"),  LaunchStrings[n]);
    if (!WorkingDirs[n].empty())
      config->Write(key+wxT("/WorkingDir"),  WorkingDirs[n]);
    config->Write(key+wxT("/CanUseTabs"),  (long)UseTabs[n]);
    config->Write(key+wxT("/Ignore"), (long)Ignores[n]);
    // Before saving, check the button's icon exists. If not, use unknown.xpm if _that_ does
    int usethisone = unknown;
    if (IconNos[n] < (int)IconNames.GetCount())
      { wxString Icon = IconNames[ IconNos[n] ];
        if ((!Icon.empty()) && (wxFileExists(Icon) || wxFileExists(BITMAPSDIR + Icon)))
           usethisone = IconNos[n];
      }
    config->Write(key+wxT("/IconNo"), usethisone);
  }

config->Flush();
}

void EditorsDialog::LoadIcons()
{
wxConfigBase* config = wxConfigBase::Get(); wxCHECK_RET(config, wxT("Invalid config"));

Icons.Clear(); Icontypes.Clear();
long NoOfKnownIcons = config->Read(wxT("/Editors/Icons/NoOfKnownIcons"), 0l);

for (long n=0; n < NoOfKnownIcons; ++n)
  { wxString Icon;
    config->Read(wxT("/Editors/Icons/")+CreateSubgroupName(n,NoOfKnownIcons)+wxT("/Icon"), &Icon);
    long Icontype = config->Read(wxT("/Editors/Icons/")+CreateSubgroupName(n,NoOfKnownIcons)+wxT("/Icontype"), -1l);
    Icons.Add(Icon);
    Icontypes.Add(Icontype);
  }
}

void EditorsDialog::SaveIcons(wxConfigBase* config /*= NULL*/)
{
size_t NoOfKnownIcons = Icons.GetCount(); if (!NoOfKnownIcons) return;

if (config == NULL)                                   // It'll usually be null, the exception being if coming from ConfigureMisc::OnExportBtn
   config = wxConfigBase::Get();  if (config == NULL)  return; 

int unknown = (MyBitmapButton::UnknownButtonNo != wxNOT_FOUND ? MyBitmapButton::UnknownButtonNo : 0); // We'll use the unknown icon below, so ID it first

config->DeleteGroup(wxT("/Editors/Icons"));
config->Write(wxT("/Editors/Icons/NoOfKnownIcons"), (long)NoOfKnownIcons); // Write this first, so it's above the icons
long NoOfIconsSaved = 0;                                                   //  but also count the number actually saved, and...
for (size_t n=0; n < NoOfKnownIcons; ++n)
  { // First check the icon exists: what if its app has been uninstalled?
    // If not, substitute the unknown icon again. Dups are bad, but altering the index of subsequent icons would be much worse!
    int usethisone = unknown;
    if (!Icons[n].empty() && (wxFileExists(Icons[n]) || wxFileExists(BITMAPSDIR + Icons[n])))
      usethisone = n;
      
    config->Write(wxT("/Editors/Icons/")+CreateSubgroupName(NoOfIconsSaved,NoOfKnownIcons)+wxT("/Icon"), Icons[usethisone]);
    config->Write(wxT("/Editors/Icons/")+CreateSubgroupName(NoOfIconsSaved,NoOfKnownIcons)+wxT("/Icontype"), Icontypes[usethisone]);
    ++NoOfIconsSaved;
  }
config->Write(wxT("/Editors/Icons/NoOfKnownIcons"), NoOfIconsSaved);      // ...ReWrite it, now we know the number that actually exist

config->Flush();
}

void EditorsDialog::AddIcon(wxString& newfilepath, size_t type)
{
if (newfilepath.IsEmpty()) return;
if (newfilepath.find(BITMAPSDIR) == 0) newfilepath = newfilepath.AfterLast(wxFILE_SEP_PATH);  // If stored in BITMAPSDIR, use only the filename

Icons.Add(newfilepath); Icontypes.Add(type);
SaveIcons();
}

void EditorsDialog::FillListbox()
{
list->Clear();                                        // In case this is a refill

for (size_t n=0; n < NoOfEditors; ++n)  
  { wxString item = Labels[n];
    if (Ignores[n]) item += wxT(" (ignored)");
    list->Append(item);
  }

if (NoOfEditors) list->SetSelection(0);
}

void EditorsDialog::OnAddButton(wxCommandEvent& WXUNUSED(event))
{
wxString launch, wd;
NewEditorDlg dlg;
wxXmlResource::Get()->LoadDialog(&dlg, this, wxT("NewEditorDlg"));
dlg.Init(this, wxEmptyString, wxEmptyString, wxEmptyString, 0, 0, 0);
do
  { if (dlg.ShowModal() != wxID_OK) return;
    SaveIcons(); // In case one has been added or found to be invalid

    launch = dlg.Launch->GetValue();
    if (launch.IsEmpty())
      { if (wxMessageBox(_("You must enter a valid command. Try again?"), _("No app entered"), wxYES_NO) != wxYES)  return;
        continue;
      }

    if (Configure::TestExistence(launch, true)) break;  // Either it's a valid absolute filepath, or a relative one in the PATH
     else
      if (wxMessageBox(_("That filepath doesn't seem currently to exist.\n                Use it anyway?"), _("App not found"), wxYES_NO) == wxYES)  break;
  }
  while (true);

Labels.Add(dlg.Label->GetValue());                    // Get new data
LaunchStrings.Add(launch);
WorkingDirs.Add(dlg.WorkingDir->GetValue());
UseTabs.Add(dlg.Multiple->GetValue());
Ignores.Add(dlg.Ignore->GetValue());
IconNos.Add(dlg.iconno);

++NoOfEditors;
FillListbox();                                        // Redo the listbox to reflect the new entry
}

void EditorsDialog::OnEditButton(wxCommandEvent& WXUNUSED(event))
{
wxString launch;
int index = list->GetSelection(); if (index == wxNOT_FOUND) return;
NewEditorDlg dlg;
wxXmlResource::Get()->LoadDialog(&dlg, this, wxT("NewEditorDlg"));
dlg.Init(this, Labels[index], LaunchStrings[index], WorkingDirs[index], UseTabs[index], Ignores[index], IconNos[index]);
do
  { if (dlg.ShowModal() != wxID_OK) return;
    launch = dlg.Launch->GetValue();
    if (launch.IsEmpty())
      { if (wxMessageBox(_("You must enter a valid command. Try again?"), _("No app entered"), wxYES_NO) != wxYES)  return;
        continue;
      }

    if (Configure::TestExistence(launch, true)) break; // Either it's a valid absolute filepath, or a relative one in the PATH
      else
      if (wxMessageBox(_("That filepath doesn't seem currently to exist.\n                Use it anyway?"), _("App not found"), wxYES_NO) == wxYES)  break;
  }
  while (true);

Labels[index] = dlg.Label->GetValue();                // Get any changed data
LaunchStrings[index] = launch;
WorkingDirs[index] = dlg.WorkingDir->GetValue();
UseTabs[index] = dlg.Multiple->GetValue();
Ignores[index] = dlg.Ignore->GetValue();
IconNos[index] = dlg.iconno;

FillListbox();                                        // Redo the listbox in case a item is now labelled ignore
}

void EditorsDialog::OnDeleteButton(wxCommandEvent& WXUNUSED(event))
{
int index = list->GetSelection(); if (index == wxNOT_FOUND) return;
if (Ignores[index]) return;                           // Can't ignore it twice

wxMessageDialog ask(this,_("Delete this Editor?"),_("Are you sure?"), wxYES_NO | wxICON_QUESTION);
if (ask.ShowModal() == wxID_YES)
  { Ignores[index] = true;
    FillListbox();                                    // Redo the listbox
  }
}

int EditorsDialog::SetBitmap(wxBitmapButton* btn, wxWindow* pa, long iconno)
{
wxConfigBase* config = wxConfigBase::Get(); if (config==NULL)  return false;
if (iconno == -1) return false; // In case we're looking for unknown.xpm, and _that_ doesn't exist either!

long NoOfKnownIcons = config->Read(wxT("/Editors/Icons/NoOfKnownIcons"), 0l);

wxString Icon; config->Read(wxT("/Editors/Icons/")+CreateSubgroupName(iconno,NoOfKnownIcons)+wxT("/Icon"), &Icon);
long Icontype = config->Read(wxT("/Editors/Icons/")+CreateSubgroupName(iconno,NoOfKnownIcons)+wxT("/Icontype"), -1l);  // Can be wxBITMAP_TYPE_PNG, wxBITMAP_TYPE_XPM or wxBITMAP_TYPE_BMP (other handlers aren't loaded)
if (Icontype==-1) return false;

wxString path; if ((!Icon.empty()) && Icon.Left(1) != wxFILE_SEP_PATH) path = BITMAPSDIR;     // If icon is just a filename, we'll prepend with BITMAPSDIR
if (!wxFileExists(path+Icon)) return SetBitmap(btn, pa, MyBitmapButton::UnknownButtonNo);   // If it doesn't exist, don't use it

wxBitmap bitmap(path+Icon , (wxBitmapType)Icontype);
int id = NextID++;
if (!btn->Create(pa, id, bitmap, wxDefaultPosition, wxDefaultSize, wxNO_BORDER)) return false;
return id;
}

void EditorsDialog::DoUpdateUI(wxUpdateUIEvent& event)
{
event.Enable(list->GetCount() > 0); 
}


void NewEditorDlg::Init(EditorsDialog* dad, wxString label, wxString launch, wxString wd, int multiple, int ignore, long icon)
{
parent = dad; iconno = icon; btn = NULL;

Label = (wxTextCtrl*)(FindWindow(wxT("Label"))); Label->SetValue(label);
Launch = (wxTextCtrl*)(FindWindow(wxT("Launch"))); Launch->SetValue(launch);
WorkingDir = (wxTextCtrl*)(FindWindow(wxT("WorkingDir"))); if (wxDirExists(wd)) WorkingDir->SetValue(wd);
Multiple = (wxCheckBox*)(FindWindow(wxT("Multiple"))); Multiple->SetValue(multiple);
Ignore = (wxCheckBox*)(FindWindow(wxT("Ignore"))); Ignore->SetValue(ignore);

int id = CreateButton();                              // Create the current bitmapbutton
if (id) Connect(id, wxEVT_COMMAND_BUTTON_CLICKED, (wxObjectEventFunction)&NewEditorDlg::OnIconClicked);  // & connect it to Select Icon dialog
}

int NewEditorDlg::CreateButton()
{
wxStaticText* st = (wxStaticText*)(FindWindow(wxT("StaticText")));  // Find the sizer to which to add the bitmapbutton
wxSizer* sizer = st->GetContainingSizer();

if (btn != NULL) btn->Destroy();                     // Kill any old button, in case we're changing a bitmap
btn = new wxBitmapButton; 

wxWindow* buttonparent;
#if wxVERSION_NUMBER < 2904 
  buttonparent = this;
#else
  buttonparent = st->GetParent();                   // Since 2.9.4, in a wxStaticBoxSizer XRC (correctly) uses the wxStaticBox as parent
#endif
int id = parent->SetBitmap(btn, buttonparent, iconno); if (id) sizer->Add(btn, 0,  wxALIGN_CENTRE);  // Create the current icon and add to sizer
sizer->Layout();
return id;
}

void NewEditorDlg::OnIconClicked(wxCommandEvent& WXUNUSED(event))
{
EditorsIconSelectDlg dlg;
wxXmlResource::Get()->LoadDialog(&dlg, this, wxT("IconSelectDlg"));
dlg.Init(parent, iconno);
if (dlg.ShowModal() != wxID_OK) return;

iconno = dlg.iconno;                                  // Get any altered icon choice
int id = CreateButton();                              // Recreate the current bitmapbutton
if (id) Connect(id, wxEVT_COMMAND_BUTTON_CLICKED, (wxObjectEventFunction)&NewEditorDlg::OnIconClicked);  // & connect the new version to Select Icon dialog
}

void IconSelectBaseDlg::Init(TBOrEditorBaseDlg* granddad, long icon)
{
grandparent = granddad; iconno = icon; SelectedBtn = NULL; panel = NULL;

GetSize(&width, &height);                            // Store the pre-panel size of the dialog

wxStaticText* ChoicesStatic = (wxStaticText*)(FindWindow(wxT("ChoicesStatic")));  // Find the sizer to which to add the available bitmapbuttons
bsizer = ChoicesStatic->GetContainingSizer();

wxButton* browse = (wxButton*)(FindWindow(wxT("Browse")));
if (browse != NULL) Connect(XRCID("Browse"), wxEVT_COMMAND_BUTTON_CLICKED, (wxObjectEventFunction)&IconSelectBaseDlg::OnBrowse);

DisplaySelectedButton();
DisplayButtonChoices();
}

void IconSelectBaseDlg::DisplaySelectedButton()
{
if (SelectedBtn != NULL) SelectedBtn->Destroy();     // Kill any old button, in case we're changing a bitmap
SelectedBtn = new wxBitmapButton; 

wxStaticText* st = (wxStaticText*)(FindWindow(wxT("SelectionStatic"))); // Find the sizer to which to add the bitmapbutton
wxSizer* sizer = st->GetContainingSizer();

SelectedEventId = grandparent->SetBitmap(SelectedBtn, this, iconno);    // Create the current icon and add to sizer. NB we use 1 of grandpa's methods
if (SelectedEventId) sizer->Add(SelectedBtn, 0,  wxALIGN_CENTRE);
}

#include <math.h>              // For sqrt
void EditorsIconSelectDlg::DisplayButtonChoices()
{
wxConfigBase* config = wxConfigBase::Get(); wxCHECK_RET(config, wxT("Invalid config"));
long NoOfKnownIcons = config->Read(wxT("/Editors/Icons/NoOfKnownIcons"), 0l);

if (panel != NULL) panel->Destroy();                // In case this is a rerun
panel = new wxPanel(this, wxID_ANY);
size_t cols = (size_t)sqrt(NoOfKnownIcons); cols = wxMax(cols, 1); size_t colsremainder = (((size_t)NoOfKnownIcons) - (cols*cols)) > 0;
size_t rows = ((size_t)NoOfKnownIcons)/(cols+1) + (((size_t)NoOfKnownIcons)%(cols+1) > 0);
gridsizer = new wxGridSizer(cols + colsremainder, 3, 3);

int AverageIconHt=0;  // We'll need this to work out how big to make the panel, as a wrong guess resulted in invisible OK/Cancel buttons. Width doesn't matter so much.
StartId = NextID;                                   // Store the id that'll be given to the 1st button
for (int n=0; n < (int)NoOfKnownIcons; ++n)
  { wxString confpath(wxT("/Editors/Icons/")+CreateSubgroupName(n,NoOfKnownIcons));
    wxString Icon; config->Read(confpath+wxT("/Icon"), &Icon);
    long Icontype = config->Read(confpath+wxT("/Icontype"), -1l);  // Can be wxBITMAP_TYPE_PNG, wxBITMAP_TYPE_XPM or wxBITMAP_TYPE_BMP (other handlers aren't loaded)
    if (Icontype == -1) continue;
    
    wxString path; if ((!Icon.empty()) && Icon.Left(1) != wxFILE_SEP_PATH) path = BITMAPSDIR;  // If icon is just a filename, we'll prepend with BITMAPSDIR
    if (!wxFileExists(path+Icon))
      { if (config->DeleteGroup(confpath)) // This one no longer exists, so remove it
          { config->Write(wxT("/Editors/Icons/NoOfKnownIcons"), (long)--NoOfKnownIcons);
            static_cast<EditorsDialog*>(grandparent)->SaveAndReloadIcons(); // Update with the new indices

            wxArrayLong& ics = static_cast<EditorsDialog*>(grandparent)->GetIconNos();
            for (size_t i = 0; i < ics.GetCount(); ++i)
              if (ics[i] >= n) --ics[i];   // and dec the value for any affected editors
            --n; 
          }
        continue; 
      }

    wxBitmap bitmap(path+Icon, (wxBitmapType)Icontype);
    wxBitmapButton* btn = new wxBitmapButton(panel, NextID++, bitmap, wxDefaultPosition, wxDefaultSize, wxNO_BORDER);
    if (btn == NULL) { --NextID; continue; }

    int iconht, iconwd; btn->GetSize(&iconwd,&iconht); AverageIconHt += iconht;
    gridsizer->Add(btn);
  }  

AverageIconHt /= NoOfKnownIcons;
panel->SetSize((cols+1)*30, rows*AverageIconHt);
panel->SetSizer(gridsizer);
bsizer->Add(panel, 1, wxALIGN_CENTRE);

SetSize(width+(cols+1)*30, height+(rows+1)*AverageIconHt);  // Resize the whole dialog to take the panel into account. I couldn't find a pure sizer way to do this :(
GetSizer()->Layout();

EndId = NextID - 1;                                         // Store the id that was given to the last button
Connect(StartId, EndId, wxEVT_COMMAND_BUTTON_CLICKED, (wxObjectEventFunction)&EditorsIconSelectDlg::OnNewSelection);
}

void IconSelectBaseDlg::OnNewSelection(wxCommandEvent& event)
{
iconno = event.GetId() - StartId;
DisplaySelectedButton();
Update();
GetSizer()->Layout();                                       // Otherwise the new button sometimes appears in the top right corner
}

void IconSelectBaseDlg::OnBrowse(wxCommandEvent& WXUNUSED(event))
{
IconFileDialog fdlg(this);
if (fdlg.ShowModal()  != wxID_OK) return;    
wxString filepath = fdlg.GetPath();
grandparent->AddIcon(filepath, fdlg.type);
DisplayButtonChoices();
}

void IconFileDialog::EndModal(int retCode)
{
if (retCode != wxID_OK) return wxDialog::EndModal(retCode);

if (!GetIcontype()) return wxDialog::EndModal(wxID_CANCEL);        // If we can't determine the icon-type, or user selected cancel, abort

wxDialog dlg;
wxXmlResource::Get()->LoadDialog(&dlg, this, wxT("CheckNewIconDlg"));
wxStaticText* Static = (wxStaticText*)(dlg.FindWindow(wxT("Static")));  // Find the sizer to which to add the prospective new bitmapbutton
wxSizer* sizer = Static->GetContainingSizer();
wxBitmap bitmap(GetPath(), (wxBitmapType)type);
wxWindow* buttonparent;
#if wxVERSION_NUMBER < 2904 
  buttonparent = &dlg;
#else
  buttonparent = Static->GetParent(); // Since 2.9.4, in a wxStaticBoxSizer XRC (correctly) uses the wxStaticBox as parent
#endif
wxBitmapButton* btn = new wxBitmapButton(buttonparent, wxID_ANY, bitmap, wxDefaultPosition, wxDefaultSize, wxNO_BORDER);
if (btn == NULL)  return wxDialog::EndModal(wxID_CANCEL);
sizer->Add(btn, 0, wxALIGN_CENTRE); 
int width, height; dlg.GetSize(&width, &height); dlg.SetSize(width, height+40);  // Make room in the dialog for the button
dlg.GetSizer()->Layout();

if (dlg.ShowModal() != wxID_OK) return;

return wxDialog::EndModal(retCode);
}

bool IconFileDialog::GetIcontype()
{
FileData fd(GetPath()); if (!fd.IsValid()) return false;
wxString ext = fd.GetUltimateDestination().AfterLast(wxT('.'));
type = -1;                                          // This is where the correct answer will be stored
do
  { if (ext == wxT("png") || ext == wxT("PNG")) type=15;
      else if (ext == wxT("xpm") || ext == wxT("PNG")) type=9;
        else if (ext == wxT("bmp") || ext == wxT("BMP")) type=1;
    
    if (type == -1)                                 // If we can't guess the type of icon, ask
      { wxArrayString choices; choices.Add(_("png")); choices.Add(_("xpm")); choices.Add(_("bmp")); choices.Add(_("Cancel"));
        ext = wxGetSingleChoice(_("Please select the correct icon-type"), wxT(""), choices, this);
        if (ext.IsEmpty() || ext == _("Cancel")) return false;
      }
  } 
  while (type == -1);                               // If we couldn't guess at first, retry with user's info

return true;
}

BEGIN_EVENT_TABLE(EditorsDialog,wxDialog)
  EVT_BUTTON(XRCID("AddBtn"), EditorsDialog::OnAddButton)
  EVT_BUTTON(XRCID("EditBtn"), EditorsDialog::OnEditButton)
  EVT_BUTTON(XRCID("DeleteBtn"), EditorsDialog::OnDeleteButton)
  EVT_UPDATE_UI(XRCID("EditBtn"), EditorsDialog::DoUpdateUI)
  EVT_UPDATE_UI(XRCID("DeleteBtn"), EditorsDialog::DoUpdateUI)
END_EVENT_TABLE()
