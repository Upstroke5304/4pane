/////////////////////////////////////////////////////////////////////////////
// Name:       config.h
// Purpose:    Constants and similar
// Part of:    4Pane
// Author:     David Hart
// Copyright:  (c) 2016 David Hart
// Licence:    GPL v3
/////////////////////////////////////////////////////////////////////////////
#ifndef CONFIGH
#define CONFIGH

size_t MyGenericDirCtrl::filecount = 0;            // Initialisation of static members that act as a clipboard
wxArrayString MyGenericDirCtrl::filearray;
wxWindowID MyGenericDirCtrl::FromID = -1;
MyGenericDirCtrl* MyGenericDirCtrl::Originpane = NULL;
size_t MyGenericDirCtrl::DnDfilecount = 0;        // Similarly for DragnDrop
wxArrayString MyGenericDirCtrl::DnDfilearray;
wxString MyGenericDirCtrl::DnDDestfilepath;
wxWindowID MyGenericDirCtrl::DnDFromID = -1;
wxWindowID MyGenericDirCtrl::DnDToID = -1;

bool MyGenericDirCtrl::Clustering = false;

MyFrame* MyFrame::mainframe;                // To facilitate access to the frame from deep inside    

bool FLOPPY_DEVICES_AUTOMOUNT;              // Some distos automount floppies
bool CDROM_DEVICES_AUTOMOUNT;               // Some distos automount dvds etc
bool USB_DEVICES_AUTOMOUNT;                 // Some distos automount usb devices

bool FLOPPY_DEVICES_SUPERMOUNT = false;     // Similarly, some distos use supermount instead or as well
bool CDROM_DEVICES_SUPERMOUNT = false;
bool USB_DEVICES_SUPERMOUNT = false;

enum DiscoveryMethod DEVICE_DISCOVERY_TYPE;  // Which method of info-discovery is used for hotplugged devices
enum TableInsertMethod FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB;// Where is floppy-device data hotplugged to? fstab or mtab
enum TableInsertMethod CD_DEVICES_DISCOVERY_INTO_FSTAB;    // Ditto cdrom
enum TableInsertMethod USB_DEVICES_DISCOVERY_INTO_FSTAB = MtabInsert;  // Ditto usb
wxString AUTOMOUNT_USB_PREFIX(wxT("/dev/sd"));  // See above.  This holds the constant bit of the dir onto which usb devices are loaded.  Probably "/dev/sd"
wxString SCSI_DEVICES(wxT("/sys/bus/scsi/devices"));  // Some HAL/Sys data is discovered here
bool SUPPRESS_EMPTY_USBDEVICES = true;      // Do we want to avoid displaying usb devices like multi-card readers, that will otherwise have multiple icons despite being largely empty
bool SUPPRESS_SWAP_PARTITIONS = true;       // Do we want to avoid displaying swap partitions in the Mount/Unmount dialogs. Surely we do . . .

wxString PROGRAMDIR;
wxString BITMAPSDIR;
wxString RCDIR;
wxString HELPDIR;

size_t MAX_COMMAND_HISTORY = 25;            // How many recent commands from eg grep or locate, should be stored
wxArrayString FilterHistory;

enum greptype WHICHGREP = PREFER_QUICKGREP;
enum findtype WHICHFIND = PREFER_QUICKFIND;

split_type WINDOW_SPLIT_TYPE = vertical;    // Split each pane vertically by default

wxFont TREE_FONT;                           // Which font is currently used in the panes
wxFont CHOSEN_TREE_FONT;                    // Which font to use in the panes if USE_DEFAULT_TREE_FONT is false
bool USE_DEFAULT_TREE_FONT = true;
bool SHOW_TREE_LINES = true;                // Whether (esp in gtk2) to show the branches, or just the leaves
int TREE_INDENT = 10;                       // Holds the horizontal indent for each subbranch relative to its parent
size_t TREE_SPACING = 10;                   // Holds the indent for each branch, +-box to Folder
int DnDXTRIGGERDISTANCE = 10;               // How far before D'n'D triggers
int DnDYTRIGGERDISTANCE = 15;
int METAKEYS_COPY = wxACCEL_CTRL, METAKEYS_MOVE = 0,  // These hold the flags that signify DnD Copy, Softlink etc
    METAKEYS_HARDLINK = wxACCEL_CTRL+wxACCEL_SHIFT, METAKEYS_SOFTLINK = wxACCEL_ALT+wxACCEL_SHIFT;

bool TREAT_SYMLINKTODIR_AS_DIR = true;
bool SHOWHIDDEN = true;
bool SHOWPREVIEW = false;
bool USE_LC_COLLATE = true;

bool ASK_ON_TRASH = true;                   // Do we show the "Are you sure?" message
bool ASK_ON_DELETE = true;
bool USE_STOCK_ICONS = true;
bool SHOW_DIR_IN_TITLEBAR = true;
bool USE_FSWATCHER = true;

bool ASK_BEFORE_UMOUNT_ON_EXIT = false;

bool SHOW_TB_TEXTCTRL = true;               // The textctrl in the toolbar that shows filepaths
wxString TRASHCAN;                          // Where to base the rubbish dirs
bool KONSOLE_PRESENT;                       // Which terminal emulators are present on the system
bool GNOME_TERM_PRESENT;
bool XTERM_PRESENT;
bool XCFE4_PRESENT;
bool LXTERMINAL_PRESENT;
bool QTERMINAL_PRESENT;
bool MATETERMINAL_PRESENT;
wxString TERMINAL = wxT("konsole");              // The name of the terminal for terminal sessions
wxString TERMINAL_COMMAND = wxT("xterm -e ");    // Command to prepend to an applic to launch in terminal window.  Could also use konsole -e
            // Ditto for Tools>Run a Program.  --noclose or -hold mean don't close the terminal afterwards, so user can actually see the displayed results!
wxString TOOLS_TERMINAL_COMMAND = wxT("konsole --noclose -e ");  // Could also use xterm -hold -e. Can't use gnome-terminal or lxterminal here, as no equivalent option

bool KDESU_PRESENT;
bool XSU_PRESENT;
bool GKSU_PRESENT;
sutype WHICH_SU = mysu;
bool USE_SUDO = false;                     // Is this a su or a sudo distro?
bool STORE_PASSWD = false;                  // Do we cache the password (from the builtin su)?
size_t PASSWD_STORE_TIME = 15;              // If so, for how many minutes?
bool RENEW_PASSWD_TTL = false;              // and do we extend the time-to-live whenever the cached password is supplied?
sutype WHICH_EXTERNAL_SU;                   // If WHICH_SU == mysu, this stores any external-su preference
wxString OTHER_SU_COMMAND;                  // A user-provided kdesu-equivalent, used when WHICH_SU==othersu

bool LIBLZMA_LOADED = false;

const bool ISLEFT = 0;                      // The flag for a directory window
const bool ISRIGHT = 1;                     //  as opposed to a file window

bool SHOW_RECURSIVE_FILEVIEW_SIZE = false;  // in fileview display, calculate the size of a dir recursively (and slowly!)
bool RETAIN_REL_TARGET = false;             // If we Move a relative symlink, keep the original target
bool RETAIN_MTIME_ON_PASTE = false;         // Move/Paste should not keep the modification time of the origin file
size_t MAX_NUMBER_OF_UNDOS = 10000;         // Amount of memory to allocate for UnRedo
size_t MAX_NUMBER_OF_PREVIOUSDIRS = 1000;   // Similarly for previously-visited dirs.  I think 1000 should be enough.
size_t MAX_DROPDOWN_DISPLAY = 15;           // How many to display at a time on a dropdown menu

const int MAX_NO_TABS = 26;                 // Can't have >26 tabs, mostly because they are labelled in config-file by a-z!

size_t MAX_TERMINAL_HISTORY = 200;          // How many recent terminal-emulator commands  should be stored

wxString TERMEM_PROMPTFORMAT = wxT("%u@%h: %w %$>");  // The preferred format for the terminal-emulator prompt
wxFont TERMINAL_FONT;
wxFont CHOSEN_TERMINAL_FONT;                // This stores the user-defined terminalem font, so that it's still available if he reverts to default
bool USE_DEFAULT_TERMINALEM_FONT = true;

size_t CHECK_USB_TIMEINTERVAL = 3;          // No of seconds between checks for new usb devices

size_t AUTOCLOSE_TIMEOUT = 2000;            // How long to hold open an autoclose dialog
size_t AUTOCLOSE_FAILTIMEOUT = 5000;        // Ditto if the process failed
size_t DEFAULT_BRIEFLOGSTATUS_TIME = 10;    // The default BriefLogStatus message time, in seconds

bool COLOUR_PANE_BACKGROUND;
bool SINGLE_BACKGROUND_COLOUR;
wxColour BACKGROUND_COLOUR_DV, BACKGROUND_COLOUR_FV;

bool USE_STRIPES = false;                   // For those who like a naffly-striped fileview
wxColour STRIPE_0, STRIPE_1;
bool HIGHLIGHT_PANES;                       // Do we subtly emphasise which pane has focus?
int DIRVIEW_HIGHLIGHT_OFFSET;               // If so, how subtly?
int FILEVIEW_HIGHLIGHT_OFFSET;
const size_t TOTAL_NO_OF_FILEVIEWCOLUMNS = 8;
size_t DEFAULT_COLUMN_WIDTHS[ TOTAL_NO_OF_FILEVIEWCOLUMNS ] = { 30, 6, 6, 15, 9, 10, 6, 25 };  // How wide is a visible column by default
bool DEFAULT_COLUMN_TEMPLATE[] = { 1, 1, 1, 1, 0, 0, 0, 0 };  // and which of them are shown by default
unsigned int EXTENSION_START = 0;           // Do the exts shown in the 'ext' column start with the first, penultimate or last dot?

wxString FLOPPYINFOPATH( wxT("/sys/block/fd") );
wxString CDROMINFO( wxT("/proc/sys/dev/cdrom/info") );
wxString PARTITIONS( wxT("/proc/partitions") );
wxString SCSIUSBDIR( wxT("/proc/scsi/usb-storage-") );// Used by DeviceManager::ReadScsiUsbStorage()
wxString SCSISCSI( wxT("/proc/scsi/scsi") );          // Used by DeviceManager::ParseProcScsiScsi()
wxString DEVICEDIR( wxT("/dev/") );

wxString LVMPREFIX( wxT("dm-") );
wxString LVM_INFO_FILE_PREFIX( wxT("/dev/.udevdb/block@") );

size_t LINES_PER_MOUSEWHEEL_SCROLL = 5;

wxString SMBPATH = wxT("/usr/bin/");      // Where is the dir with samba stuff? Might be symlinked from here instead
wxString SHOWMOUNTFPATH = wxT("/usr/sbin/showmount");    // Where is showmount?
wxArrayString NFS_SERVERS;                // Any known servers

int DEFAULT_STATUSBAR_WIDTHS[ 4 ] = { 5, 3, 8, 2 };

bool USE_SYSTEM_OPEN(true);
bool USE_BUILTIN_OPEN(true);
bool PREFER_SYSTEM_OPEN(true);

#endif
      // #ifndef CONFIGH

