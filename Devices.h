/////////////////////////////////////////////////////////////////////////////
// Name:       Devices.h
// Purpose:    Mounting etc of Devices
// Part of:    4Pane
// Author:     David Hart
// Copyright:  (c) 2020 David Hart
// Licence:    GPL v3
/////////////////////////////////////////////////////////////////////////////
#ifndef DEVICESH
#define DEVICESH

#include "wx/wx.h"
#include "wx/config.h"

#include <fstab.h>
#ifdef __linux__
#include <mntent.h>
#else
#include <sys/mount.h>
#endif

#include "Externs.h"

class MyButtonDialog : public wxDialog
{
protected:
void OnButtonPressed(wxCommandEvent& event){ EndModal(event.GetId()); }

DECLARE_EVENT_TABLE()
};

class MyBitmapButton   :     public wxBitmapButton
{
public:
MyBitmapButton(wxWindow *parent, wxWindowID id, const wxBitmap& bitmap,  const wxPoint& pos = wxDefaultPosition,
                   const wxSize& size = wxDefaultSize, long style = wxBU_AUTODRAW,  const wxValidator& validator = wxDefaultValidator,
                   const wxString& name = wxT("MyBitmapButton")) : wxBitmapButton(parent, id, bitmap){ Ok = false; }
MyBitmapButton(){ Ok = false; }

static void SetUnknownButtonNo();               // Tries to locate the iconno of unknown.xpm
bool IsOk() const { return Ok; }

long ButtonNumber;                              // This distinguishes between the various buttons
static long UnknownButtonNo;                    // In case a bitmap isn't found, use unknown.xpm

protected:
void SetIsOk(bool value) { Ok = value; }
bool Ok;
};


class DeviceBitmapButton  :  public MyBitmapButton
{
public:
DeviceBitmapButton(enum devicecategory which);

void OnEndDrag();                                // Used when DragnDrop deposits on one of our toolbar buttons
static void SaveDefaults(wxConfigBase* config=NULL);

protected:
void OnButtonClick(wxCommandEvent& event);
void OnRightButtonClick(wxContextMenuEvent& event);
#ifdef __WXX11__
void OnRightUp(wxMouseEvent& event){ OnRightButtonClick((wxContextMenuEvent&)event); }  // EVT_CONTEXT_MENU doesn't seem to work in X11
#endif
void OnMount(wxCommandEvent& event);
void OnUnMount(wxCommandEvent& event);
void OnMountDvdRam(wxCommandEvent& event);
void OnDisplayDvdRam(wxCommandEvent& event);
void OnUnMountDvdRam(wxCommandEvent& event);
void OnEject(wxCommandEvent& event);

DECLARE_EVENT_TABLE()
};


class EditorBitmapButton   :   public MyBitmapButton
{
public:
EditorBitmapButton(wxWindow *parent, size_t which);

void OnEndDrag();                              // Used when DragnDrop deposits on one of our toolbar buttons

bool Ignore;
protected:
void OnButtonClick(wxCommandEvent& event);

wxString Label, Launch, WorkingDir, Icon;
long Icontype;
bool Multiple;
wxWindow* parent;
private:
DECLARE_EVENT_TABLE()
};


class GvfsBitmapButton : public MyBitmapButton
{
public:
GvfsBitmapButton(wxWindow* parent, const wxString& mntpt );

wxString GetOriginalMountpt() { return m_originalMountpt; }
wxString GetOriginalSelection() { return m_originalSelection; }
const wxString GetMountpt() { return m_mountpt; }

protected:
void OnButtonClick(wxCommandEvent& event);
void OnUnMount(wxCommandEvent& event);
void OnRightButtonClick(wxContextMenuEvent& event);

wxString m_buttonname;
const wxString m_mountpt;
wxString m_originalMountpt, m_originalSelection, m_originalFileviewSelection;
DECLARE_EVENT_TABLE()
};


struct DevicesStruct   // Holds data for a device
  { wxString devicenode;          // Holds eg /dev/fd0 or /dev/sda, even in the case  of multipartition devices  eg /dev/sda1, sda2 etc  ie major number only
    int idVendor; int idProduct;  // For usb etc devices, hold the (hopefully unique) ids of a device 
    wxString Vendorstr, Productstr;  // Again for usb devices, the vendor/model user-visible strings (which may have been altered by the user)
    wxString name1; wxString name2; wxString name3;  // For dvds etc, name1 holds the 'typename' eg "cdrom".  For Usb etc devices, hold the id strings that scsi uses for this device eg Hi SPACE, flash disk
    wxArrayString partitions;     // Hold eg /dev/sda, or /dev/sda1, /dev/sda2 etc if this device has multiple partitions
    wxArrayString mountpts;       // Hold eg /mount/floppy, or a series of mountpts for a device, 1 for each entry in wxArrayString 'partitions'
    int defaultpartition;         // If this device contains several partitions, this indexes the one to use by default if not otherwise specified
    bool readonly;
    int ignore;                   // We know it exists, but we don't care! 0=false, 1=always ignore, 2=ignore today only
    int devicetype;               // Indexes an item in the  DeviceInfo::devinfo array, holding type info
    int buttonnumber;             // Matches the ButtonNumber of the toolbar button
    bool OriginallyMounted;       // When this prog started, was the device already mounted? If so, don't automatically umount on exit
    bool IsMounted;               // Set/Unset by us when we mount/unmount
    bool HasButton;               // Flags whether or not the device has already got a button on the toolbar
    DeviceBitmapButton* button;   // The & of the corresponding button is stored here, so that we can find it within the device sizer for removal
                  // The following are used for usb devices in a HAL/sys/udev setup
    wxString fname;               // Holds a filename such as 8:0:0:0 which is a symlink to Hal data
    bool haschild;                // Normally false but (for when several identical devices are attached at once) this flags that there's another device in 'next'
    struct DevicesStruct* next;   // If dup > 0, this links to a struct holding data for another of these identical devices, linkedlist-wise
    
    DevicesStruct(){ idVendor=idProduct=0; devicetype = unknowncat; haschild=false; next=NULL; ignore = DS_false; readonly = false; }
    DevicesStruct(const DevicesStruct& ds)
                { devicenode=ds.devicenode; idVendor=ds.idVendor; idProduct=ds.idProduct;
                  name1=ds.name1; name2=ds.name2; name3=ds.name3; Vendorstr=ds.Vendorstr; Productstr=ds.Productstr;
                  readonly=ds.readonly; ignore=ds.ignore; defaultpartition = ds.defaultpartition;
                  devicetype=ds.devicetype; haschild=false; next=NULL; for (size_t n=0; n < ds.mountpts.GetCount(); ++n) mountpts.Add(ds.mountpts.Item(n));
                }
    ~DevicesStruct(){ if (next) delete next; }
    bool operator !=(const DevicesStruct& ds) const
      { if (idVendor && idProduct) return ((idVendor!=ds.idVendor) || (idProduct!=ds.idProduct));     // For usb devices these should be available and unique
        return (devicetype!=ds.devicetype) || (mountpts!=ds.mountpts) || (devicenode!=ds.devicenode)  // Otherwise do it the hard way
                                                                            || (name1!=ds.name1) || (ignore!=ds.ignore) || (readonly!=ds.readonly);
      }
  };

struct PartitionStruct  // Holds possible partitions, mountpts, mount status
  { wxString device;              // Holds eg /dev/sda2
    wxString type;                // e.g. ext4 or swap. Used (iiuc) to avoid displaying swap partitions when not using lsblk
    wxString uuid;                // Holds the uuid for the device, for use when /etc/fstab is uuid-based
    wxString label;               // Similarly for mounting by label
    wxString mountpt;             // Holds fstab's mountpt for this device
    bool IsMounted;     
  };

struct NetworkStruct   // Holds NFS or samba info
  { wxString ip;                  // Holds eg "192.168.0.1"
    wxString hostname;            // The host or samba name for this machine
    wxArrayString shares;         // Holds this machine's export or share-list           
    wxString mountpt;             // Holds fstab's mountpt for the highlit share, if exists
    bool readonly;                // Available if loaded from /etc/fstab
    wxString user;                // Ditto
    wxString passwd;              // Ditto
    wxString texture;             // Ditto (nfs only: may be hard or soft)
    
    NetworkStruct() : readonly(false) {}
  };

struct GvfsStruct   // Holds gphoto2: cdda: mtp: gvfs 'mounts'
  { const wxString mountpt; // e.g. /run/user/1000/gvfs/cdda:host=sr0 (or ~/.gvfs/cdda:host=sr0)
    const wxString type;    // currently gphoto2: mtp: or cdda:
    GvfsBitmapButton* button; // The & of the corresponding button is stored here, so that we can find it within the device sizer for removal
    
    GvfsStruct(const wxString& mpt, GvfsBitmapButton* btn) : mountpt(mpt), type(mountpt.AfterLast(wxFILE_SEP_PATH).BeforeFirst(':') + ':'), button(btn) {}
  };

WX_DEFINE_ARRAY(DevicesStruct*, ArrayofDeviceStructs);      // Define the arrays we'll be using
WX_DEFINE_ARRAY(PartitionStruct*, ArrayofPartitionStructs);
WX_DEFINE_ARRAY(NetworkStruct*, ArrayofNetworkStructs);
WX_DEFINE_ARRAY(GvfsStruct*, ArrayofGvfsStructs);

struct DevInfo    // Used by DeviceInfo to store info about a device
{
wxString name;                    // eg floppy, floppy A, dvdrom
enum devicecategory cat;          // eg floppycat, cdromcat
enum treerooticon icon;           // eg floppytype, cdtype
bool CanDoDvdRam;
};

WX_DEFINE_ARRAY(DevInfo*, ArrayofDevInfoStructs);

class DeviceInfo  // Holds & provides data about a device --- displayname, type, icon
{
public:
DeviceInfo();
~DeviceInfo(){ for (int n = (int)devinfo->GetCount();  n > 0; --n )  {  DevInfo* item = devinfo->Item(n-1); delete item; } delete devinfo; }
void Save();

wxString GetName(size_t index){ if (index >= devinfo->GetCount()) return wxEmptyString; return devinfo->Item(index)->name; }
devicecategory GetCat(size_t index){ if (index >= devinfo->GetCount()) return unknowncat; return devinfo->Item(index)->cat; }
treerooticon GetIcon(size_t index){ if (index >= devinfo->GetCount()) return twaddle; return devinfo->Item(index)->icon; }
bool GetDvdRam(size_t index){ if (index >= devinfo->GetCount()) return false; return devinfo->Item(index)->CanDoDvdRam; }

bool HasRemovableMedia(size_t index){ return ! HasPartitions(index); }  // This is currently true: HasPartitions() is everything but cdroms & floppies.  However BEWARE CHANGES
bool HasPartitions(size_t index){ if (index >= devinfo->GetCount()) return false; return GetCat(index) < floppycat || GetCat(index) >= usbpencat; }
bool ThisDevicetypeAutomounts(size_t index);
bool CanDoDvdRam(size_t index){ if (index >= devinfo->GetCount()) return false; return devinfo->Item(index)->CanDoDvdRam; }
enum discoverytable GetDiscoveryTableForThisDevicetype(enum treerooticon index);  // Does this type of device get its data hotplugged into fstab, as opposed to mtab (or nowhere).  And what about supermount?
size_t GetCount(){ return devinfo->GetCount(); }
void Add(wxString name, devicecategory cat, treerooticon icon, bool CanDoDvdRam=false);

protected:
bool Load();
void LoadDefaults();

ArrayofDevInfoStructs* devinfo;
};

class GvfsInfo  // Holds & provides data about gvfs mounts e.g. gphoto2:
{
public:
GvfsInfo() { m_gvfsmounts = new ArrayofGvfsStructs; }
~GvfsInfo() { for (int n = (int)m_gvfsmounts->GetCount();  n > 0; --n )  {  GvfsStruct* item = m_gvfsmounts->Item(n-1); delete item; } delete m_gvfsmounts; }

wxString GetMountpt(size_t index) { if (index >= m_gvfsmounts->GetCount()) return ""; return m_gvfsmounts->Item(index)->mountpt; }
wxString GetType(size_t index) { if (index >= m_gvfsmounts->GetCount()) return ""; return m_gvfsmounts->Item(index)->type; }
GvfsBitmapButton* GetButton(size_t index) { if (index >= m_gvfsmounts->GetCount()) return NULL; return m_gvfsmounts->Item(index)->button; }
size_t GetCount() { return m_gvfsmounts->GetCount(); }
void Add(const wxString& mountpt, GvfsBitmapButton* button) { GvfsStruct* gvfsstruct = new struct GvfsStruct(mountpt, button); m_gvfsmounts->Add(gvfsstruct); }
void Remove(size_t i) { wxCHECK_RET(i < m_gvfsmounts->GetCount(), "Incorrect index"); delete m_gvfsmounts->Item(i); m_gvfsmounts->RemoveAt(i); }
int Find(const wxString& MountptToFind) 
  { if (MountptToFind.IsEmpty()) return (wxNOT_FOUND-1); 
    for (int n=0; n < GetCount(); ++n)
      { if (GetMountpt(n) == MountptToFind) return n; }
    return wxNOT_FOUND; 
  }

protected:
ArrayofGvfsStructs* m_gvfsmounts;
};

class DeviceAndMountManager;

class DeviceManager                              // Class to manipulate device data & things like its mount & button status
{
public:
DeviceManager(DeviceAndMountManager* dad)  :  parent(dad)  { DeviceStructArray = new ArrayofDeviceStructs; DevInfo = new class DeviceInfo; }
~DeviceManager();

DevicesStruct* FindStructForDevice(size_t index);     // Returns DeviceStructArray-entry after errorcheck
DevicesStruct* FindStructForButtonNumber(int buttonnumber);  // Returns DeviceStructArray-entry for the given buttonnumber
bool Add(DevicesStruct* item1, DevicesStruct* item2); // See if the names of items 1 & 2 match

DevicesStruct* GetNextDevice(size_t& count, size_t& child);  // Iterates thru DeviceStructArray returning the next struct, but sidetracking down any children
void LoadDevices();                                     // Loads cdrom etc config data and thence their toolbar icons
void SaveFixedDevices();                                // Saves cdrom etc data
void SaveUsbDevices();
void Add(DevicesStruct* newitem);
bool DevicesMatch(DevicesStruct* item1, DevicesStruct* item2);  // See if the names of items 1 & 2 match
void SetupConfigureFixedorUsbDevices(ArrayofDeviceStructs& tempDeviceStructArray);  // Provides info about fixed or usb devices for Configure
void ReturnFromConfigureFixedorUsbDevices(const ArrayofDeviceStructs& tempDeviceStructArray, const ConfigDeviceType whichtype);  // Stores results of SetupConfigureFixedorUsbDevices()
static void GetFloppyInfo(ArrayofDeviceStructs* DeviceStructArray, size_t FirstOtherDevice, DeviceAndMountManager* parent, bool FromWizard = false);  // Try to determine what floppy drives are attached
static bool GetCdromInfo(ArrayofDeviceStructs* DeviceStructArray, size_t FirstOtherDevice, DeviceAndMountManager* parent, bool FromWizard = false);  // Try to determine what cd/dvdrom/r/ram devices are attached
void ConfigureEditors();                                // Similarly editors. Yes, I know that editors aren't devices, but it fits best here
static void SaveMiscData(wxConfigBase* config = NULL);// Saves misc device config data
ArrayofDeviceStructs* GetDevicearray() const { return DeviceStructArray; }

size_t FirstOtherDevice;                                // Indexes the 1st item within DeviceStructArray that is an Other device, not a standard cdrom etc
DeviceInfo* DevInfo;

protected:
void DisplayFixedDevicesInToolbar();
void LoadPossibleUsbDevices();                    // These are flash-pens etc, devices which may be plugged in at any time
void LoadMiscData();                              // Loads misc device config data

ArrayofDeviceStructs* DeviceStructArray;          // To hold array of DevicesStructs
DeviceAndMountManager* parent;
};

#include "wx/textfile.h"
class MyUsbDevicesTimer;
#if wxVERSION_NUMBER < 3000
    class MtabPollTimer;
#endif

class DeviceAndMountManager
{
public:
DeviceAndMountManager();
~DeviceAndMountManager();

wxString Mount(size_t whichdevice, bool OnOff=true);  // Called when  eg floppy-button is clicked, to (u)mount & display a floppy etc.
wxString DoMount(DevicesStruct* tempstruct, bool OnOff, int partition = -1, bool DvdRam =false);  // Mounts or Unmounts the device in tempstruct.  Used mostly by Mount()
void CheckForOtherDevices();                      // Check for any other mountables eg USB devices.  Called by timer

DevicesStruct* QueryAMountpoint(const wxString& path, bool contains=false);  // Called by MyGenericDirCtrl, to see if a particular path is a valid mountpt eg /mount/floppy

void SearchTable(wxArrayString& partitions, wxArrayString& mountpts,  wxString dev, enum treerooticon type=usbtype);  // Relays to either SearchMtab() or SearchFstab() appropriate for this type of device
void GetDevicePartitions(wxArrayString& partitions, wxArrayString& mountpts,  wxString dev);  // Parse /proc/partitions for dev's partitions. Return them in array, with any corresponding mountpts
DevicesStruct* FindStructForDevice(size_t whichdevice){ return deviceman->FindStructForDevice(whichdevice); }
void FillGnuHurdMountsList(ArrayofPartitionStructs& array);   // Find mounts (active translators) in gnu-hurd

void OnMountPartition();                          // From menubar Tools>Mount Partition
void OnUnMountPartition();
void OnMountIso();
void OnMountNfs();
void OnMountSshfs();
void OnMountSamba();
void OnUnMountNetwork();
#ifdef __linux__
static struct mntent* ReadMtab(const wxString& partition, bool DvdRamFS=false); // Goes thru mtab, to find if 'partition' currently mounted. If DvdRamFS, ignores eg subfs mounts (used for DVD-RAM)
#else
static struct statfs* ReadMtab(const wxString& partition, bool DvdRamFS=false);
#endif
static struct fstab* ReadFstab(const wxString& dev, const wxString& uuid = "", const wxString& label = ""); // Search fstab for a line for this device
static struct fstab* ReadFstab(const PartitionStruct* ps) { return ReadFstab(ps->device, ps->uuid, ps->label); }
static bool FindUnmountedFstabEntry(wxString& dev, wxArrayString& answerarray); // Ditto but only returning Unmounted entries.  Used for DVD-RAM
size_t ReadFstab(ArrayofPartitionStructs& PartitionArray, const wxString& type) const; // Uses findmnt to look for entries of a particular type or types e.g. cifs
static void LoadFstabEntries(wxArrayString& devicenodes, wxArrayString& mountpts);  // Loads all valid fstab entries into arrays
void ConfigureNewFixedDevice(DevicesStruct* tempstruct, bool FromEdit=false);
void CheckSupermountTab(wxArrayString& partitions, wxArrayString& mountpts,  wxString dev, bool SearchMtab);    // In Supermount systems, finds data for this device
void ConfigureNewDevice(DevicesStruct* tempstruct, bool FromEdit=false);

bool Checkmtab(wxString& mountpt, wxString device = wxEmptyString);  // Searches mtab to see if a device is already mounted. Optionally specifies the device we're interested in
void SearchMtabForStandardMounts( wxString& device, wxArrayString& mountpts);  // Finds ext2 etc mountpts for the device ie not subfs. Used for DVD-RAM

#ifdef __GNU_HURD__
  static int GnuCheckmtab(wxString& mountpoint, wxString device = wxEmptyString, TranslatorType type = TT_either); // Is mountpoint a mountpoint, optionally of 'device'?
#endif

DeviceManager* deviceman;                         // Class that does the dirty work with devices
GvfsInfo* m_gvfsinfo;                             // Detects & displays gvfs mounts
ArrayofPartitionStructs* PartitionArray;          // To hold partitions, mountpt, status
wxString DefaultUsbDevicename;                    // If there's ever an unattached device to configure, use this as its /dev location
MyUsbDevicesTimer* usbtimer;

protected:
void CheckForNovelDevices();                      // Look thru proc/scsi/usb-storage*, checking for unknown devices
wxString WhereIsDeviceMounted(wxString device, size_t type);  // If device is mounted, returns its mountpt 
bool ReadScsiUsbStorage(DevicesStruct* tempstruct);           // Look thru proc/scsi/usb-storage*, to see if this usb device is attached
bool ParseProcScsiScsi(DevicesStruct* tempstruct, long devno, wxArrayString& names);  // Look thru proc/scsi/scsi to deduce the current major no for this device, or to find the device's name(s)
void SearchMtab(wxArrayString& devices, wxArrayString& mountpts,  wxString dev = wxEmptyString);    // Loads arrays with data for a device
void SearchFstab(wxArrayString& devices, wxArrayString& mountpts,  wxString dev);                // Ditto using fstab
void GetPartitionList(wxArrayString& array, bool ignoredevices = false); // Parse the system file that holds details of all known partitions
void FillGnuHurdPartitionList(wxArrayString& array);          // Find possible partitions in gnu-hurd
void AddLVMDevice(wxArrayString& array, wxString& dev);       // Called by GetPartitionList() to process lvm "partitions"
void FillPartitionArray(bool justpartitions = true);          // Load all known partitions into PartitionArray, find their mountpts etc. Optionally mounted iso-images too
bool FillPartitionArrayUsingLsblk(bool fixed_only = false);   // Ditto, using lsblk
bool FillPartitionArrayUsingBlkid();                          // Ditto, using blkid
void FindMountedImages();                                     // Add mounted iso-images from mtab to array
bool MkDir(wxString& mountpt);                                // Make a dir onto which to mount
bool WeCanUseSmbmnt(wxString& mountpt, bool TestUmount = false);  // Do we have enough kudos to (u)mount with smb
                              // These are used in automounting distros
void CheckForGioAutomountedDevices();                         // Check for devices e.g. cds, mounted by GIO inside /run/user/
void CheckForAutomountedDevices();                            // Check for any other auto-mountables eg USB devices, in automounting distros
void CheckForHALAutomountedDevices();                         // Ditto in Hal-automounting distros
int MatchHALInfoToDevicestruct(DevicesStruct* tempstruct, int idVendor, int idProduct, wxString& vendor, wxString& model, wxString& devnode);  // See if this devicestruct matches the host/model data from HAL
bool ExtractHalData(wxString& symlinktarget, int& idVendor, int& idProduct, wxString& vendor, wxString& model, wxString& vendoridstr, wxString& productidstr, wxString& devnode); // Searches in the innards of Hal/sys to extract a usb device's data
void CheckForAutomountedNovelDevices(wxArrayString& partitions, wxArrayString& mountpts);                          // Any novelties? For mtab-style auto-mounting distros
bool IsRemovable(const wxString& dev);                        // Is this e.g. a sata fixed drive, or a removable usb pen?

static wxArrayString RealFilesystemsArray;
wxString LSBLK;                                               // The filepath of lsblk (or empty)
wxString BLKID;                                               // Similarly
wxString FINDMNT;                                             // Ditto
wxString FUSERMOUNT;                                          // Ditto
#if wxVERSION_NUMBER < 3000
MtabPollTimer* m_MtabpollTimer;                               // When active, polls mtab to look for a delayed sshfs mount. Only needed pre-wx3
#endif
};

class MyUsbDevicesTimer  :   public wxTimer  // Used by DeviceManager.  Notify() looks for attached Usb drives when the timer 'fires'
{
public:
void Init(DeviceAndMountManager* dman){ devman = dman; }
void Notify(){ devman->CheckForOtherDevices(); }

protected:
DeviceAndMountManager* devman;
};

#if wxVERSION_NUMBER < 3000
class MtabPollTimer  :   public wxTimer  // Used by DeviceAndMountManager::OnMountSshfs.  Notify() looks for a newly-mounted sshfs mount
{
public:
MtabPollTimer(DeviceAndMountManager* dman) : m_devman(dman) {}
void Begin(const wxString& mountpt, int freq = 1000, int tries = 10)
  { remaining = tries; Start(freq, wxTIMER_CONTINUOUS); m_Mountpt1 = mountpt;
    if (mountpt.Right(1) == wxT("/")) m_Mountpt2 = mountpt.BeforeLast(wxT('/'));
     else m_Mountpt2 = mountpt + wxT('/');
  }
void Notify();

protected:
int remaining;
wxString m_Mountpt1, m_Mountpt2;
DeviceAndMountManager* m_devman;
};
#endif

class TBOrEditorBaseDlg : public wxDialog  // Virtual base class to facilitate code-sharing of IconSelectDlg
{
public:
virtual int SetBitmap(wxBitmapButton* btn, wxWindow* pa, long iconno)=0;
virtual void AddIcon(wxString& newfilepath, size_t type)=0;
};

class EditorsDialog : public TBOrEditorBaseDlg
{
public:
void Init(DeviceManager* dad);
virtual int SetBitmap(wxBitmapButton* btn, wxWindow* pa, long iconno);  // Returns buttons event id if it works, else false
virtual void AddIcon(wxString& newfilepath, size_t type);
void Save(wxConfigBase* config = NULL){ SaveEditors(config); SaveIcons(config); }
wxArrayLong& GetIconNos() { return IconNos; }
void SaveAndReloadIcons() { SaveIcons(); LoadIcons(); } // Used when one has disappeared

protected:
void OnAddButton(wxCommandEvent& event);
void OnEditButton(wxCommandEvent& event);
void OnDeleteButton(wxCommandEvent& event);
void DoUpdateUI(wxUpdateUIEvent& event);
void FillListbox();
void SaveEditors(wxConfigBase* config = NULL);
void LoadIcons();
void SaveIcons(wxConfigBase* config = NULL);

size_t NoOfEditors;
wxArrayString LaunchStrings, WorkingDirs, Labels, Icons;
wxArrayInt UseTabs, Ignores, Icontypes;
wxArrayLong IconNos;

wxListBox* list;
DeviceManager* parent;
private:
DECLARE_EVENT_TABLE()
};

class NewEditorDlg : public wxDialog
{
public:
void Init(EditorsDialog* dad, wxString label, wxString launch, wxString wd, int multiple, int ignore, long icon);

EditorsDialog* parent;
wxTextCtrl *Label, *Launch, *WorkingDir;
wxCheckBox *Multiple, *Ignore; 
long iconno;

protected:
void OnIconClicked(wxCommandEvent& event);
int CreateButton();

wxBitmapButton* btn;
};

class IconSelectBaseDlg : public wxDialog
{
public:
void Init(TBOrEditorBaseDlg* granddad, long icon);
long iconno;

protected:
void DisplaySelectedButton();
void OnNewSelection(wxCommandEvent& event);
void OnBrowse(wxCommandEvent& event);

virtual void DisplayButtonChoices()=0;

wxPanel* panel;
wxGridSizer* gridsizer;
wxSizer* bsizer;
wxBitmapButton* SelectedBtn;

int width, height;
int StartId, EndId;
TBOrEditorBaseDlg* grandparent;
int SelectedEventId;
};

class EditorsIconSelectDlg : public IconSelectBaseDlg
{
protected:
virtual void DisplayButtonChoices();
};

class IconFileDialog  :  public wxFileDialog
{
public:
IconFileDialog(wxWindow* parent) : wxFileDialog(parent, _("Browse for new Icons"), BITMAPSDIR, wxT(""), wxT("*"), wxFD_OPEN) {}

int type;
protected:
void EndModal(int retCode);
bool GetIcontype();
};

#endif
    // DEVICESH
