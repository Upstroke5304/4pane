/////////////////////////////////////////////////////////////////////////////
// Name:       Externs.h
// Purpose:    Extern declarations, enums
// Part of:    4Pane
// Author:     David Hart
// Copyright:  (c) 2020 David Hart
// Licence:    GPL v3
/////////////////////////////////////////////////////////////////////////////
#ifndef EXTERNSH
#define EXTERNSH

#include "wx/artprov.h"

#ifndef __LINUX__
  #define __GNU_HURD__
  enum TranslatorType { TT_active, TT_passive, TT_either };
#endif
extern bool SvgToPng(const wxString& svgfilepath, const wxString& outputfilepath, void* dlhandle); // Renders an svg file to an image
extern wxString LocateLibrary(const wxString& lib); // Search ldconfig output to find a lib's filepath. 'lib' can/will be a partial name e.g. 'rsvg'

extern wxString HOME;             // In case we want to pass as a parameter an alternative $HOME
                                  // Externs for some global functions
extern wxString ParseSize(wxULongLong ull, bool longer);          // Returns filesize parsed into bytes/K/M/GB as appropriate.  In Misc.cpp
extern wxString CreateSubgroupName(size_t no, size_t total);      // Creates a unique subgroup name in ascending alphabetical order

enum changerelative { makeabsolute, makerelative, nochange };     // Used by:
extern bool CreateSymlinkWithParameter(wxString oldname, wxString newname, enum changerelative rel);  // In Tools.  CreateSymlink() with the option of changing from abs<->relative
extern bool CreateSymlink(wxString oldname, wxString newname);    // In Tools.  Encapsulates creating a symlink.  Used also to rename, move etc
extern bool CreateHardlink(wxString oldname, wxString newname);   // In Tools.  Encapsulates creating a hard-link
extern void HtmlDialog(const wxString& title, const wxString& file2display, const wxString& dialogname, wxWindow* parent=NULL, wxSize size=wxSize(0,0));  // In Tools.  Sets up a dialog with just a wxHtmlWindow & a Finished button
extern size_t IsPlausibleIPaddress(wxString& str);                // In Mounts.cpp
extern wxString StrWithSep(const wxString& path);                 // In Misc, returns path with a '/' if necessary
extern wxString StripSep(const wxString& path);                   // In Misc, returns path without any terminal '/'
extern bool IsDescendentOf(const wxString& dir, const wxString& child); // In Misc, checks if child is a descendent of dir
extern wxString EscapeQuote(wxString& filepath);                  // In Misc, used by Filetypes & Archive.  Escapes any apostrophes in a filepath
extern wxString EscapeQuoteStr(const wxString& filepath);
extern wxString EscapeSpace(wxString& filepath);                  // Escapes spaces within e.g. filepaths
extern wxString EscapeSpaceStr(const wxString& filepath);
extern wxString QuoteSpaces(wxString& str);                       // Intelligently cover with "". Do each filepath separately, in case we're launching a script with a parameter which takes parameters . . .
extern void QuoteExcludingWildcards(wxString& command);           // Wraps command with " " if appropriate, excluding any terminal globs
extern void SetSortMethod(const bool LC_COLLATE_aware);           // Do we want filename etc sorting to be LC_COLLATE aware?
extern void SetDirpaneSortMethod(const bool LC_COLLATE_aware);    // Ditto for dirpane sorting
extern wxString TruncatePathtoFitBox(wxString filepath, wxWindow* box); // In Misc, used by MyFiles
extern wxWindow* InsertEllipsizingStaticText(wxString filepath, wxWindow* original, int insertAt = -1); // In Misc, used by MyFiles and MyDirs
extern wxString MakeTempDir(const wxString& name = wxT(""));      // In Misc, Makes a unique dir in /tmp/4Pane/
extern wxBitmap GetBestBitmap(const wxArtID& artid, const wxArtClient& client, const wxBitmap& fallback_xpm); // In Misc, returns a wxArtProvider bitmap or the default one
extern void QueryWrapWithShell(wxString& command);                //  In Misc. Wraps command with sh -c if appropriate
extern bool KernelLaterThan(wxString minimum);                    // In Misc, used by Devices
extern bool GetFloat(wxString& str, double* fl, size_t skip);     // In Misc, used by Configure
extern void DoBriefLogStatus(int no, wxString msgA, wxString msgB); // In Misc. A convenience function, to call BriefLogStatus with the commonest requirements
extern bool LoadPreviousSize(wxWindow* tlwin, const wxString& name); // In Misc. Retrieve and apply any previously-saved size for the top-level window
extern void SaveCurrentSize(wxWindow* tlwin, const wxString& name);  // In Misc. The 'save' for LoadPreviousSize()

enum distro { gok, suse, redhat, fedora, mandrake, slackware, sabayon, gentoo, debian, mepis, ubuntu, kubuntu, pclos, damnsmall, puppy, knoppix, gnoppix, zenwalk, mint, other };

enum emptyable { CannotBeEmptied = 0, CanBeEmptied, SubdirCannotBeEmptied, RubbishPassed = -1 };    // Can a dir and its progeny be deleted from?  
extern enum emptyable CanFoldertreeBeEmptied(wxString path);      // In Filetypes, used by Delete
extern bool HasThisDirSubdirs(wxString& dirname);                 // In Filetypes, used in DirGenericDirCtrl::ReloadTree()
extern bool ReallyIsDir(const wxString& path, const wxString& item); // In Filetypes, used in DirGenericDirCtrl::ExpandDir
extern bool RecursivelyGetFilepaths(wxString path, wxArrayString& array);  // Global recursively to fill array with all descendants of this dir
extern wxULongLong GetDirSizeRecursively(wxString& dirname);      // Get the recursive size of the dir's contents  Used by FileGenericDirCtrl::UpdateStatusbarInfo
extern wxULongLong GetDirSize(wxString& dirname);                 // Get the non-recursive size of the dir's contents  Used by FileGenericDirCtrl::UpdateStatusbarInfo
extern bool ContainsRelativeSymlinks(wxString fd);                // Used by MyGenericDirCtrl::Move to see if a relative symlink is present (as it Moves differently)

extern bool FLOPPY_DEVICES_AUTOMOUNT;                 // Some distos automount floppies
extern bool CDROM_DEVICES_AUTOMOUNT;                  // Some distos automount dvds etc
extern bool USB_DEVICES_AUTOMOUNT;                    // Some distos automount usb devices

extern bool FLOPPY_DEVICES_SUPERMOUNT;                // Similarly, some distos use supermount instead or as well
extern bool CDROM_DEVICES_SUPERMOUNT;
extern bool USB_DEVICES_SUPERMOUNT;

enum DiscoveryMethod { OriginalMethod=0, MtabMethod, SysHal };  // Used below
extern enum DiscoveryMethod DEVICE_DISCOVERY_TYPE;    // Which method of info-discovery is used for hotplugged devices

enum TableInsertMethod { MtabInsert=0, FstabInsert, NoInsert };     // Used below
extern enum TableInsertMethod FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB;  // Where is floppy-device data hotplugged to? fstab or mtab or nowt
extern enum TableInsertMethod CD_DEVICES_DISCOVERY_INTO_FSTAB;      // Ditto cdrom
extern enum TableInsertMethod USB_DEVICES_DISCOVERY_INTO_FSTAB;     // Ditto usb

extern wxString SCSI_DEVICES;                         // Some HAL/Sys data is discovered here
extern wxString AUTOMOUNT_USB_PREFIX;                 // See above.  This holds the path + first 2 letters of the /devs onto which usb devices are loaded.  Probably "/dev/sd"
extern bool SUPPRESS_EMPTY_USBDEVICES;                // Do we want to avoid displaying usb devices like multi-card readers, that will otherwise have multiple icons despite being largely empty
extern bool SUPPRESS_SWAP_PARTITIONS;                 // Do we want to avoid displaying swap partitions in the Mount/Unmount dialogs. Surely we do . . .

extern wxString PROGRAMDIR;
extern wxString BITMAPSDIR;
extern wxString RCDIR;
extern wxString HELPDIR;

extern wxWindowID NextID;

extern wxFont TREE_FONT;         // Which font is currently used in the panes
extern wxFont CHOSEN_TREE_FONT;  // Which font to use in the panes if USE_DEFAULT_TREE_FONT is false
extern bool USE_DEFAULT_TREE_FONT;
extern bool SHOW_TREE_LINES;     // Whether (esp in gtk2) to show the branches, or just the leaves
extern int TREE_INDENT;          // Holds the horizontal indent for each subbranch relative to its parent
extern size_t TREE_SPACING;      // Holds the indent for each branch, +-box to Folder
extern int DnDXTRIGGERDISTANCE;  // How far before D'n'D triggers
extern int DnDYTRIGGERDISTANCE;
extern int METAKEYS_COPY, METAKEYS_MOVE, METAKEYS_HARDLINK, METAKEYS_SOFTLINK;  // These hold the flags that signify DnD Copy, Softlink etc

extern bool SHOWHIDDEN;
extern bool SHOWPREVIEW;
extern bool USE_LC_COLLATE;
extern bool TREAT_SYMLINKTODIR_AS_DIR;
extern bool ASK_ON_TRASH;       // Do we show the "Are you sure?" message
extern bool ASK_ON_DELETE;
extern bool USE_STOCK_ICONS;
extern bool SHOW_DIR_IN_TITLEBAR;
extern bool USE_FSWATCHER;

extern bool ASK_BEFORE_UMOUNT_ON_EXIT;

extern bool SHOW_TB_TEXTCTRL;    // The textctrl in the toolbar that shows filepaths
extern wxString TRASHCAN;        // Where to base the rubbish dirs

extern bool KONSOLE_PRESENT;     // Which terminal emulators are present on the system
extern bool GNOME_TERM_PRESENT;
extern bool XTERM_PRESENT;
extern bool XCFE4_PRESENT;
extern bool LXTERMINAL_PRESENT;
extern bool QTERMINAL_PRESENT;
extern bool MATETERMINAL_PRESENT;
extern wxString TERMINAL;        // The name of the terminal for terminal sessions.  Default "konsole"
extern wxString TERMINAL_COMMAND; // Command to prepend to an applic to launch in terminal window.  Default "xterm -e"
extern wxString TOOLS_TERMINAL_COMMAND; // Similarly for the Tools > Run a Program command
extern wxArrayString FilterHistory;

extern bool KDESU_PRESENT;
extern bool XSU_PRESENT;
extern bool GKSU_PRESENT;
enum sutype { dunno, kdesu, gnomesu, gksu, othersu, mysu }; // Choices for su-ing
extern sutype WHICH_SU;                               // Which of the above do we use?
extern bool USE_SUDO;                                // Is this a su or a sudo distro?
extern bool STORE_PASSWD;                             // Do we cache the password (from the builtin su)?
extern size_t PASSWD_STORE_TIME;                      // If so, for how many minutes?
extern bool RENEW_PASSWD_TTL;                         // and do we extend the time-to-live whenever the cached password is supplied?
extern sutype WHICH_EXTERNAL_SU;                      // If WHICH_SU == mysu, this stores any external-su preference
extern wxString OTHER_SU_COMMAND;                     // A user-provided kdesu-equivalent, used when WHICH_SU==othersu

extern bool LIBLZMA_LOADED;

extern const bool ISLEFT;         // The flag for a directory window
extern const bool ISRIGHT;        //  as opposed to a file window

extern bool SHOW_RECURSIVE_FILEVIEW_SIZE;
extern bool RETAIN_REL_TARGET;              // If we Move a relative symlink, keep the original target
extern bool RETAIN_MTIME_ON_PASTE;          // Should Move/Paste keep the modification time of the origin file
extern size_t MAX_NUMBER_OF_UNDOS;          // Amount of memory to allocate for UnRedo
extern size_t MAX_NUMBER_OF_PREVIOUSDIRS;   // Similarly for previously-visited dirs
extern size_t MAX_DROPDOWN_DISPLAY;         // How many to display at a time on a dropdown menu
extern size_t MAX_COMMAND_HISTORY;          // How many recent commands from eg grep or locate, should be stored
extern const int MAX_NO_TABS;               // Can't have >26 tabs, mostly because they are labelled in config-file by a-z!

extern size_t MAX_TERMINAL_HISTORY;         // How many recent terminal-emulator commands  should be stored
extern wxString TERMEM_PROMPTFORMAT;        // The preferred format for the terminal-emulator prompt
extern wxFont TERMINAL_FONT;
extern wxFont CHOSEN_TERMINAL_FONT;
extern bool USE_DEFAULT_TERMINALEM_FONT;

extern size_t CHECK_USB_TIMEINTERVAL;

extern size_t AUTOCLOSE_TIMEOUT;
extern size_t AUTOCLOSE_FAILTIMEOUT;
extern size_t DEFAULT_BRIEFLOGSTATUS_TIME;

extern bool COLOUR_PANE_BACKGROUND;
extern bool SINGLE_BACKGROUND_COLOUR;
extern wxColour BACKGROUND_COLOUR_DV, BACKGROUND_COLOUR_FV;

extern bool USE_STRIPES;                    // For those who like a naffly-striped fileview
extern wxColour STRIPE_0, STRIPE_1;
extern const size_t TOTAL_NO_OF_FILEVIEWCOLUMNS;
extern bool HIGHLIGHT_PANES;                // Do we subtly emphasise which pane has focus?
extern int DIRVIEW_HIGHLIGHT_OFFSET;        // If so, how subtly?
extern int FILEVIEW_HIGHLIGHT_OFFSET;

extern const int HEADER_HEIGHT;             // The ht of the header column.  Originally 23
extern size_t DEFAULT_COLUMN_WIDTHS[];      // How wide is a visible column by default
extern bool DEFAULT_COLUMN_TEMPLATE[];      // and which of them are shown by default
extern unsigned int EXTENSION_START;        // Do the exts shown in the 'ext' column start with the first, penultimate or last dot?

enum split_type { initialsplit, vertical = 1, horizontal, unsplit };
extern split_type WINDOW_SPLIT_TYPE;        // Split each pane vertically by default

extern wxString FLOPPYINFOPATH;
extern wxString CDROMINFO;
extern wxString PARTITIONS;
extern wxString SCSIUSBDIR;
extern wxString SCSISCSI;
extern wxString DEVICEDIR;

extern wxString LVMPREFIX;
extern wxString LVM_INFO_FILE_PREFIX;

extern size_t LINES_PER_MOUSEWHEEL_SCROLL;

extern wxString SMBPATH;                    // Where is the dir with samba stuff? Might be symlinked from here instead
extern wxString SHOWMOUNTFPATH;             // Where is the dir containing showmount?
extern wxArrayString NFS_SERVERS;           // Any known servers

extern int DEFAULT_STATUSBAR_WIDTHS[];

extern bool USE_SYSTEM_OPEN;
extern bool USE_BUILTIN_OPEN;
extern bool PREFER_SYSTEM_OPEN;

enum                  //  menuitem IDs
{
  GAP_FOR_NEW_SHORTCUTS = 200,
  SHCUT_START = 20000,
  SHCUT_CUT = SHCUT_START,  // The first batch are UpdateUIed according to whether there's a selection, in DoQueryEmptyUI()
  SHCUT_COPY,
  SHCUT_TRASH,
  SHCUT_DELETE,
  SHCUT_REALLYDELETE,
  SHCUT_RENAME,
  SHCUT_DUP,
  SHCUT_PROPERTIES,
  SHCUT_OPEN,
  SHCUT_OPENWITH,
  SHCUT_OPENWITH_KDESU,
  
  SHCUT_PASTE,                // These are done in DoMiscUI()
  SHCUT_HARDLINK,
  SHCUT_SOFTLINK,
  SHCUT_DELTAB,
  SHCUT_RENAMETAB,
  SHCUT_UNDO,
  SHCUT_REDO,

  SHCUT_SELECTALL,
  SHCUT_NEW,
  SHCUT_NEWTAB,

  SHCUT_INSERTTAB,
  SHCUT_DUPLICATETAB,
  SHCUT_ALWAYS_SHOW_TAB,
  SHCUT_SAME_TEXTSIZE_TAB,
  SHCUT_REPLICATE,
  SHCUT_SWAPPANES,
  SHCUT_SPLITPANE_VERTICAL,
  SHCUT_SPLITPANE_HORIZONTAL,
  SHCUT_SPLITPANE_UNSPLIT,
  
  SHCUT_SHOW_COL_EXT,
  SHCUT_SHOW_COL_SIZE,
  SHCUT_SHOW_COL_TIME,
  SHCUT_SHOW_COL_PERMS,
  SHCUT_SHOW_COL_OWNER,
  SHCUT_SHOW_COL_GROUP,
  SHCUT_SHOW_COL_LINK,
  SHCUT_SHOW_COL_ALL,
  SHCUT_SHOW_COL_CLEAR,
  
  SHCUT_SAVETABS,
  SHCUT_SAVETABS_ONEXIT,
  SHCUT_TAB_SAVEAS_TEMPLATE,
  LoadTabTemplateMenu,             // Different format to signify that it's not really a shortcut: it's here only for UpdateUI
  SHCUT_TAB_DELETE_TEMPLATE,
  SHCUT_REFRESH,
  SHCUT_LAUNCH_TERMINAL,
  SHCUT_FILTER,
  SHCUT_TOGGLEHIDDEN,
  
  SHCUT_ADD_TO_BOOKMARKS,
  SHCUT_MANAGE_BOOKMARKS,
  
  SHCUT_MOUNT_MOUNTPARTITION,  
  SHCUT_MOUNT_UNMOUNTPARTITION,
  SHCUT_MOUNT_MOUNTISO,
  SHCUT_MOUNT_MOUNTNFS,  
  SHCUT_MOUNT_MOUNTSAMBA,  
  SHCUT_MOUNT_UNMOUNTNETWORK,
  
  SHCUT_TOOL_LOCATE,
  SHCUT_TOOL_FIND,
  SHCUT_TOOL_GREP,
  SHCUT_TERMINAL_EMULATOR,
  SHCUT_COMMANDLINE,
  SHCUT_TERMINALEM_GOTO,          // Used in terminal context menu
  SHCUT_EMPTYTRASH,
  SHCUT_EMPTYDELETED,
  
  SHCUT_ARCHIVE_EXTRACT,
  SHCUT_ARCHIVE_CREATE,
  SHCUT_ARCHIVE_APPEND,
  SHCUT_ARCHIVE_TEST,
  SHCUT_ARCHIVE_COMPRESS,
  
  SHCUT_SHOW_RECURSIVE_SIZE,
  SHCUT_RETAIN_REL_TARGET,
  SHCUT_CONFIGURE,
  
  SHCUT_EXIT,
  
  SHCUT_F1,
  SHCUT_HELP,
  SHCUT_FAQ,
  SHCUT_ABOUT,
  
  SHCUT_CONFIG_SHORTCUTS,
  SHCUT_GOTO_SYMLINK_TARGET,
  SHCUT_GOTO_ULTIMATE_SYMLINK_TARGET,
  
  SHCUT_EDIT_BOOKMARK,    // Bookmarks mostly reuse SHCUT_COPY etc, but we need these 2 specific ones
  SHCUT_NEW_SEPARATOR,
  SHCUT_TOOLS_REPEAT,

  // ***** Add any future shortcuts here, so they won't affect old values or those of UDTools
  SHCUT_SWITCH_FOCUS_OPPOSITE,
  SHCUT_SWITCH_FOCUS_ADJACENT,
  SHCUT_SWITCH_FOCUS_PANES,
  SHCUT_SWITCH_FOCUS_TERMINALEM,
  SHCUT_SWITCH_FOCUS_COMMANDLINE,
  SHCUT_SWITCH_FOCUS_TOOLBARTEXT,
  SHCUT_SWITCH_TO_PREVIOUS_WINDOW,
  SHCUT_PREVIOUS_TAB,
  SHCUT_NEXT_TAB,

  SHCUT_PASTE_DIR_SKELETON,

  SHCUT_EXT_FIRSTDOT, SHCUT_EXT_MIDDOT, SHCUT_EXT_LASTDOT,

  SHCUT_MOUNT_MOUNTSSHFS,

  SHCUT_PREVIEW,
  
  SHCUT_ESCAPE, // For interrupting threads
  
  SHCUT_DECIMALAWARE_SORT, // In the current fileview, sort foo1, foo2 above foo11
  
  SHCUT_RETAIN_MTIME_ON_PASTE, // When Moving/Pasting, try to leave the modification time ISQ (like cp -a)

  SHCUT_NAVIGATE_DIR_UP,  // Directory navigation: the dirview toolbar Up/Left/Right arrows
  SHCUT_NAVIGATE_DIR_PREVIOUS,
  SHCUT_NAVIGATE_DIR_NEXT,


  // *****

  SHCUT_TOOLS_LAUNCH = SHCUT_TOOLS_REPEAT + 1 + GAP_FOR_NEW_SHORTCUTS,
  SHCUT_TOOLS_LAUNCH_LAST = SHCUT_TOOLS_LAUNCH + 500,


  SHCUT_RANGESTART,
  SHCUT_RANGEFINISH = SHCUT_RANGESTART+500,    // I expect 500 submenu items will suffice
  SHCUT_TABTEMPLATERANGE_START,
  SHCUT_TABTEMPLATERANGE_FINISH = SHCUT_TABTEMPLATERANGE_START + 26,  // as they are labelled by lower-case letters 26
  
  SHCUT_FINISH
};

enum
{
  ID_FIRST_BOOKMARK = 25000,
  ID__LAST_BOOKMARK = ID_FIRST_BOOKMARK+6000  // Surely no one wants more than 6k, even with avoiding duplicates while pasting
};

enum                      //  to do with toolbars
{
  ID_TOOLBAR = 500,
  ID_FRAMETOOLBAR,
  ID_FILEPATH,
   
  IDM_TOOLBAR_Range = 15,                    // Define a range extent for the no. of user-defined buttons
  IDM_TOOLBAR_fulltree = 200,
  IDM_TOOLBAR_up,
  IDM_TOOLBAR_LargeDropdown1,
  IDM_TOOLBAR_LargeDropdown2,
  IDM_TOOLBAR_bmfirst,
  IDM_TOOLBAR_bmlast = IDM_TOOLBAR_bmfirst + IDM_TOOLBAR_Range
};

enum myDragResult          // Extended version of wxDragResult  
{
  myDragError,  
  myDragNone, 
  myDragCopy, 
  myDragMove, 
  myDragHardLink, 
  myDragCancel,
  myDragSoftLink      // Mine  
};

enum deviceID      //  to do with toolbar bitmap-buttons
{
  ID_FLOPPY1 = 0,  
  ID_UNKNOWN=98,  
  MAXDEVICE = 99,
  
  ID_EDITOR = 100,
  MAXEDITOR = ID_EDITOR+50,
  
  MAX_NO_OF_PARTITONS = 1000,         // Should suffice
  MAX_NO_OF_DVDRAM_MOUNTS = 100,      // Should definitely suffice!

  ID_MOUNT=200,
  ID_UNMOUNT=ID_MOUNT+1 + MAX_NO_OF_PARTITONS,
  
  ID_EJECT=ID_UNMOUNT+1 + MAX_NO_OF_PARTITONS,
  ID_MOUNT_DVDRAM,
  ID_UNMOUNT_DVDRAM,
  ID_DISPLAY_DVDRAM    // Don't forget this is a range, max MAX_NO_OF_DVDRAM_MOUNTS
};

enum whichcan { delcan, trashcan, tempfilecan };  // Deals with deleted/trash/tempfile dirs

enum devicecategory { harddiskcat = 0, floppycat, cdromcat, cdrcat, usbpencat, usbmemcat, usbmultcardcat, usbharddrivecat, unknowncat, cddacat, gphoto2cat, mtpcat, rubbish=-1 };
  
enum treerooticon { ordinary = 0, floppytype, cdtype, usbtype, twaddle };  // Which icons to use for the treectrl root image: folder, floppy etc.  All the cd-types share an icon

enum discoverytable { TableUnused = 0, MtabOrdinary, MtabSupermount, FstabOrdinary, FstabSupermount, DoNowt };  // Some distros insert device info into fstab, others mtab.  Some use supermount with a device-name of "none"!

enum { current = 0, selected, home };                            //  On Unmounting, which dir to revert to:  HOME, CurrentWorkingDir or user configured

enum DupRen{ DR_ItsAFile, DR_ItsADir, DR_Rename, DR_RenameAll, DR_Overwrite, DR_OverwriteAll, DR_Skip, DR_SkipAll, DR_Store, DR_StoreAll, DR_Cancel, DR_Unknown };  // Used by CheckDupRen & its callers

enum toolchoice { null = -1, locate = 0, find, grep, terminalem, cmdline };  // Which tool are we using in the bottom (or top) pane?

enum columntype { filename = 0, ext, filesize, modtime, permissions, owner, group, linkage };  // Fileview column types

enum DnDCursortype { dndStdCursor = 0, dndSelectedCursor, dndScrollCursor, dndUnselect, dndOriginalCursor }; // Cursor types for use by MyDragImage

enum devicestructignore { DS_false = 0, DS_true, DS_defer };     // Do we wish to ignore a particular device

enum ConfigDeviceType { CDT_fixed = 0, CDT_removable };          // Makes it easier to reuse a dialog class for both fixed and usb devices

enum ziptype
 { zt_gzip, zt_bzip, zt_lzma, zt_xz, zt_lzop, zt_compress, zt_lastcompressed = zt_compress,
   zt_firstarchive, zt_taronly = zt_firstarchive, zt_cpio, zt_rpm, zt_ar, zt_deb,
   zt_firstcompressedarchive, zt_targz = zt_firstcompressedarchive, zt_tarbz, zt_tarlzma, zt_tarxz, zt_tar7z, zt_tarlzop, zt_taZ, zt_7z, zt_zip, zt_rar, zt_htb, zt_lastcompressedarchive = zt_htb,
   zt_invalid 
 };

enum GDC_images { GDC_closedfolder, GDC_openfolder, GDC_file, GDC_computer, GDC_drive, GDC_cdrom, GDC_floppy, GDC_removable, GDC_symlink, GDC_brokensymlink, GDC_symlinktofolder, GDC_lockedfolder,
                  GDC_usb, GDC_pipe, GDC_blockdevice, GDC_chardevice, GDC_socket, 
                  GDC_compressedfile, GDC_tarball, GDC_compressedtar,
                  GDC_ghostclosedfolder,GDC_ghostopenfolder, GDC_ghostfile, GDC_ghostcompressedfile, GDC_ghosttarball, GDC_ghostcompressedtar,
                  GDC_unknownfolder, GDC_unknownfile
                };

enum alterarc { arc_add, arc_remove, arc_del, arc_rename, arc_dup };

enum adjFPs { afp_rename, afp_extract, afp_neither };

enum PPath_returncodes { Oops, unaltered, needsquote, noneed4quote };
extern enum PPath_returncodes PrependPathToCommand(const wxString& Path, const wxString& Command, wxChar quote, wxString& FinalCommand, bool ThereWillBeParameters);

enum archivemovetype { amt_from, amt_to, amt_bothsame, amt_bothdifferent }; // For unredoing archive Moves: are we Moving From an archive, To one, or both: within the same one or not

enum AttributeChange { ChangeOwner = 0, ChangeGroup, ChangePermissions };   // Which type of attribute-change do we wish for.  See RecursivelyChangeAttributes()
extern int RecursivelyChangeAttributes(wxString filepath, wxArrayInt& IDs, size_t newdata, size_t& successes, size_t& failures, enum AttributeChange whichattribute, int flag);  // In Filetypes, used by OnProperties()

enum NavigateArcResult { OutsideArc, FoundInArchive, Snafu };
enum OCA { OCA_false, OCA_true, OCA_retry  };

enum greptype { PREFER_QUICKGREP, PREFER_FULLGREP };          // Does the user prefer full or quick greps?
extern enum greptype WHICHGREP; /*=PREFER_QUICKGREP*/
enum findtype { PREFER_QUICKFIND, PREFER_FULLFIND };          // Similarly quick finds?
extern enum findtype WHICHFIND; /*=PREFER_QUICKFIND*/

enum helpcontext
  { HCunknown, HCconfigUDC, HCconfigDevices, HCconfigMount, HCconfigShortcuts, HCconfigNetworks, HCconfigterminal, HCconfigMisc, HCconfigMiscStatbar, HCconfigMiscExport, HCarchive, 
    HCconfigDisplay, HCterminalEm, HCsearch, HCmultiplerename , HCproperties, HCdnd, HCbookmarks, HCfilter, HCopenwith
  };
extern helpcontext HelpContext;                               // What Help to show on F1

enum InotifyEventType { IET_create, IET_delete, IET_rename, IET_modify, IET_attribute, IET_umount, IET_other };

enum UnRedoType { URT_notunredo, URT_undo, URT_redo };

enum CDR_Result { CDR_skip, CDR_ok, CDR_renamed = CDR_ok, CDR_overwrite };

enum MovePasteResult { MPR_fail, MPR_thread, MPR_completed };

enum Thread_Type { ThT_paste = 1, ThT_text = 2, /*ThT_SOMETHINGELSE = 4*/ ThT_all = -1}; // These are treated as flags

#endif
    // EXTERNSH 
