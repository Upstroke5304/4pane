/////////////////////////////////////////////////////////////////////////////
// Name:       Mounts.h
// Purpose:    Mounting partitions
// Part of:    4Pane
// Author:     David Hart
// Copyright:  (c) 2016 David Hart
// Licence:    GPL v3
/////////////////////////////////////////////////////////////////////////////
#ifndef MOUNTSH
#define MOUNTSH

#include "wx/wx.h"

class MountPartitionDialog  :  public wxDialog  // Select partition & mount-point
{
public:
MountPartitionDialog(){}
bool Init(DeviceAndMountManager* dad,  wxString device = wxEmptyString);

wxComboBox* PartitionsCombo;
wxTextCtrl* FstabMountptTxt;
wxComboBox* MountptCombo;

protected:
void OnBrowseButton(wxCommandEvent& event);
void OnButtonPressed(wxCommandEvent& event){ if (event.GetId() == wxID_OK) SaveMountptHistory(); EndModal(event.GetId()); }
void OnSelectionChanged(wxCommandEvent& WXUNUSED(event)){ DisplayMountptForPartition(); }
void DisplayMountptForPartition(bool GetDataFromMtab=false);    // This enters the correct mountpt for the selected partition, in the fstab version of the dialog
void LoadMountptHistory() const;
void SaveMountptHistory() const;

DeviceAndMountManager* parent;
wxString historypath;

private:
DECLARE_EVENT_TABLE()
};

class MountSshfsDlg : public MountPartitionDialog
{
  struct SshfsNetworkStruct
    { wxString user;                // Holds the remote username
      wxString hostname;
      wxString remotedir;           // Holds any non-default remote 'root' dir
      wxString localmtpt;           // Holds any local mountpt
      bool idmap; bool readonly;
      wxString otheroptions;
      SshfsNetworkStruct(const wxString& u, const wxString& h, const wxString& rm, const wxString& lm, bool map, bool ro, const wxString& o)
        : user(u), hostname(h), remotedir(rm), localmtpt(lm), idmap(map), readonly(ro), otheroptions(o) {}
      bool IsEqualTo(const SshfsNetworkStruct* s) const // Only compare the combo data: we don't want duplicates of these just because of e.g. a readonly mount
        { return ((user==s->user) && (hostname==s->hostname) && (remotedir==s->remotedir)); }
    };
WX_DEFINE_ARRAY(SshfsNetworkStruct*, ArrayofSshfsNetworkStructs);

public:
MountSshfsDlg(){}
bool Init();

wxComboBox* UserCombo;
wxComboBox* HostnameCombo;
wxComboBox* RemotedirCombo;
wxCheckBox* Idmap;
wxCheckBox* Readonly;
wxTextCtrl* OtherOptions;

protected:
void OnSelectionChanged(wxCommandEvent& event);
void OnOK(wxCommandEvent& event);
void OnOKUpdateUI(wxUpdateUIEvent& event);

ArrayofSshfsNetworkStructs SshfsHistory;
};

class MountSambaDialog  :  public MountPartitionDialog
{
public:
MountSambaDialog(){}
~MountSambaDialog(){ ClearNetworkArray(); }
bool Init();
void OnUpdateUI(wxUpdateUIEvent& event);

wxComboBox* IPCombo;
wxComboBox* HostnameCombo;
wxComboBox* SharesCombo;
wxTextCtrl* Username;
wxRadioBox* RwRadio;
wxTextCtrl* Password;
wxStaticText* AlreadyMounted;

bool InFstab, IsMounted; wxString AtMountPt, FstabMt;    // These hold data for any share that is in fstab, and/or is already mounted 

protected:
bool FindData();
void GetFstabShares();
bool GetSharesForServer(wxArrayString& array, wxString& hostname, bool findprinters = false);  // Return  hostname's available shares
void LoadSharesForThisServer(int sel);                  // Fill the relevant combo with available shares for the displayed server
void GetMountptForShare();                              // Enter into MountptTxt any known mountpoint for the selection in SharesCombo
void OnSelectionChanged(wxCommandEvent& event);
void ClearNetworkArray(){ for (int n = (int)NetworkArray.GetCount();  n > 0; --n ) { NetworkStruct* item = NetworkArray.Item(n-1); delete item; NetworkArray.RemoveAt(n-1);} }

ArrayofNetworkStructs NetworkArray;
private:
DECLARE_EVENT_TABLE()
};

class MountLoopDialog  :  public MountSambaDialog     // For mounting iso-images
{
public:
MountLoopDialog(){}
bool Init();
wxTextCtrl* ImageTxt;

protected:
void OnIsoBrowseButton(wxCommandEvent& event);

void OnSelectionChanged(wxCommandEvent& WXUNUSED(event)){ DisplayMountptForImage(); }
void DisplayMountptForImage();                      // This enters the correct mountpt for the selected Image if it's in fstab

private:
DECLARE_EVENT_TABLE()
};

class MountNFSDialog  :  public MountSambaDialog
{
public:
MountNFSDialog(){}
bool Init();

wxRadioBox* MounttypeRadio; // Hard or soft

protected:
bool FindData();                                        // Query the network to find NFS servers
void GetFstabShares();                                  // But check in /etc/fstab first
void OnAddServerButton(wxCommandEvent& event);
void LoadExportsForThisServer(int sel);                 // Fill the relevant combo with available exports for the displayed server
bool GetExportsForServer(wxArrayString& exports, wxString& server);  // Return server's available exports
void OnSelectionChanged(wxCommandEvent& event);
void OnGetMountptForShare(wxCommandEvent& WXUNUSED(event)){ GetMountptForShare(); }
void GetMountptForShare();                              // Enter into MountptTxt any known mountpoint for the selection in SharesCombo
void SaveIPAddresses() const;

private:
DECLARE_EVENT_TABLE()
};

class UnMountPartitionDialog  :  public MountPartitionDialog    // Unmount a mounted partition
{
public:
UnMountPartitionDialog(){}
void Init(DeviceAndMountManager* dad);  // Everything else uses the base class methods & variables.  Init is different as it enters the Mounted partitions, not the unmounted
};

class UnMountSambaDialog  :  public MountPartitionDialog    // Unmount a network mount
{
enum MtType { MT_invalid = -1, MT_nfs, MT_sshfs, MT_samba };

public:
UnMountSambaDialog(){}
~UnMountSambaDialog(){ for (int n=(int)Mntarray.GetCount(); n > 0; --n)  { PartitionStruct* item = Mntarray.Item(n-1); delete item; Mntarray.RemoveAt(n-1); } }
size_t Init();
void DisplayMountptForShare();                      // Show mountpt corresponding to a share or export
void OnSelectionChanged(wxCommandEvent& WXUNUSED(event)){ DisplayMountptForShare(); }
void SearchForNetworkMounts();                      // Scans mtab for established NFS & samba mounts
bool IsNfs() const  { return m_Mounttype == MT_nfs; }
bool IsSshfs() const  { return m_Mounttype == MT_sshfs; }
bool IsSamba() const  { return m_Mounttype == MT_samba; }

protected:
enum MtType ParseNetworkFstype(const wxString& type) const; // Is this a mount that we're interested in: nfs, sshfs or samba
ArrayofPartitionStructs Mntarray;
enum MtType m_Mounttype;                            // Stores the type of the current selection

DECLARE_EVENT_TABLE()
};

#endif
    // MOUNTSH

