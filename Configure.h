/////////////////////////////////////////////////////////////////////////////
// Name:       Configure.h
// Purpose:    Configuration
// Part of:    4Pane
// Author:     David Hart
// Copyright:  (c) 2016 David Hart
// Licence:    GPL v3
/////////////////////////////////////////////////////////////////////////////
#ifndef CONFIGUREH
#define CONFIGUREH

#include <vector>
#include <algorithm>
#include "wx/spinctrl.h"
#include "wx/treebook.h"
#include "wx/wizard.h"

#include "Devices.h"
#include "Tools.h"

class NoConfigWizard;

class MyWizardPage : public wxWizardPage
{
public:
MyWizardPage(wxWizard *parent)  :  wxWizardPage(parent){}
virtual void OnPageDisplayed()=0;    // Abstract, as we certainly don't want any objects,
};

class FirstPage : public MyWizardPage
{
public:
FirstPage(wxWizard *parent, wxWizardPage* Page2, wxWizardPage* Page3); 

    // implement wxWizardPage functions
virtual wxWizardPage* GetPrev() const { return (wxWizardPage*)NULL; }
virtual wxWizardPage* GetNext() const { return RCFound ? m_Page3 : m_Page2; }  // Return the correct next page, depending on whether resources were found
void OnPageDisplayed(){}
void SetRCFound(bool found) { RCFound = found; }

protected:
wxWizardPage *m_Page2, *m_Page3;
bool RCFound;
};

class SecondPage : public MyWizardPage
{
public:
SecondPage(wxWizard *parent,  wxWizardPage* Page3); 

virtual wxWizardPage* GetPrev() const { return (wxWizardPage*)NULL; }
virtual wxWizardPage* GetNext() const  { return m_Page3; }
void OnPageDisplayed();

protected:
wxWizard *parent;
wxWizardPage* m_Page3;
};

class ThirdPage : public MyWizardPage
{
public:
ThirdPage(NoConfigWizard* parent, const wxString& configFP); 

virtual wxWizardPage* GetPrev() const { return (wxWizardPage*)NULL; }
virtual wxWizardPage* GetNext() const { return (wxWizardPage*)NULL; }
void OnPageDisplayed();
bool m_ConfigAlreadyFound;

protected:
const wxString configFPath;
};

class NoConfigWizard  :  public wxWizard
{
public:
NoConfigWizard(wxWindow* parent, int id, const wxString& title, const wxString& configFPath);
FirstPage* Page1;
SecondPage* Page2;
ThirdPage* Page3;

protected:
void OnPageChanged(wxWizardEvent& event) { ((MyWizardPage*)event.GetPage())->OnPageDisplayed(); }
void OnEventSetFocusNext(wxCommandEvent& event) { if (m_Next) m_Next->SetFocus(); } // Set focus to the 'Next' button
void OnButton(wxCommandEvent& event);

wxString m_configFPath; // Normally ~/.config/4Pane/.4Pane, but not if the instance was invoked with the -c option
wxButton* m_Next;
private:
DECLARE_EVENT_TABLE()
};

class MyConfigureDialog    :    public wxDialog
{
public:
void OnEndModal(wxCommandEvent& event);
void OnClose(wxCloseEvent& event);
};


class Configure; class ArrayOfToolFolders;

class BackgroundColoursDlg  :  public wxDialog
{
public:
void Init(wxColour c0, wxColour c1, bool single);
wxColour Col0, Col1;
wxCheckBox* IdenticalColsChk;

protected:
bool dirty;
void DisplayCols();
void OnChangeButton(wxCommandEvent& event);
void OnCheckboxUpdateUI(wxUpdateUIEvent& event);
DECLARE_EVENT_TABLE()
};

class ChooseStripesDlg  :  public wxDialog
{
public:
void Init(wxColour c0, wxColour c1){ Col0 = c0 ; Col1 = c1; DisplayCols(); }
wxColour Col0, Col1;

protected:
bool dirty;
void DisplayCols();
void OnChangeButton(wxCommandEvent& event);
DECLARE_EVENT_TABLE()
};

class ConfigureFileOpenDlg  :  public wxDialog
{
public:
void Init();
bool GetUseSystem() const { return m_UseSystem->IsChecked(); }
bool GetUseBuiltin() const { return m_UseBuiltin->IsChecked(); }
bool GetPreferSystem() const { return m_PreferSystem->GetValue(); }

protected:
void OnRadioUpdateUI(wxUpdateUIEvent& event);

bool dirty;

wxCheckBox* m_UseSystem;
wxCheckBox* m_UseBuiltin;
wxRadioButton* m_PreferSystem;
wxRadioButton* m_PreferBuiltin;
};

class PaneHighlightDlg  :  public wxDialog
{
public:
void Init(int dv, int fv);
int GetDirSpinValue() { return dvspin->GetValue(); }
int GetFileSpinValue() { return fvspin->GetValue(); }

protected:
void OnSpin(wxSpinEvent& event);

wxSpinCtrl *dvspin, *fvspin;
wxPanel *dvpanel, *fvpanel; 
DECLARE_EVENT_TABLE()
};


struct DirCtrlTBStruct                              // Holds info on the desired extra buttons to load to each Dirview mini-toolbar
  { wxString bitmaplocation;                        // Load the bitmap from here
    int iconNo;                                     // Which icon to use
    wxBitmapType icontype;                          // Can be wxBITMAP_TYPE_PNG, wxBITMAP_TYPE_XPM or wxBITMAP_TYPE_BMP (other handlers aren't loaded)
    wxString label;
    wxString tooltip;
    wxString destination;                           // The "url" to go to when clicked  eg The "Home" button will contain /home
  };

WX_DEFINE_ARRAY(DirCtrlTBStruct*, ArrayofDirCtrlTBStructs);

class ToolbarIconsDialog : public TBOrEditorBaseDlg
{
public:
void Init(ArrayofDirCtrlTBStructs* DCTBarray);
virtual int SetBitmap(wxBitmapButton* btn, wxWindow* pa, long iconno);  // Returns buttons event id if it works, else false
virtual void AddIcon(wxString& newfilepath, size_t type);
static void LoadDirCtrlTBinfo(ArrayofDirCtrlTBStructs* DCTBarray);
void Save();

protected:
void OnAddButton(wxCommandEvent& event);
void OnEditButton(wxCommandEvent& event);
void OnDeleteButton(wxCommandEvent& event);
void DoUpdateUI(wxUpdateUIEvent& event);
void FillListbox();
void SaveIcons();
void OnInitDialog(wxInitDialogEvent& event);

size_t NoOfTools;
wxArrayString Icons;
wxArrayInt Icontypes;

ArrayofDirCtrlTBStructs* DirCtrlTBinfoarray;
wxListBox* list;
DeviceManager* parent;
private:
DECLARE_EVENT_TABLE()
};

class AddToobarIconDlg : public wxDialog
{
public:
void Init(ToolbarIconsDialog* dad, const wxString& defaultfilepath, const wxString& defaultLabel, const wxString& defaulttooltip, long icon);

TBOrEditorBaseDlg* grandparent;
wxTextCtrl* Filepath;
wxTextCtrl* Label;
wxTextCtrl* Tooltip;
long iconno;

protected:
void OnIconClicked(wxCommandEvent& event);
void OnBrowseBtn(wxCommandEvent& event);
int CreateButton();

wxBitmapButton* btn;
};

class TBIconSelectDlg : public IconSelectBaseDlg
{
protected:
virtual void DisplayButtonChoices();
};

class ConfigureNotebook;

class ConfigureDisplay      // Encapsulates the Display notebookpage stuff
{
enum whichpage { CD_trees, CD_font, CD_misc };
public:
ConfigureDisplay(ConfigureNotebook* nb) : m_nb(nb) { pageTree=pageFont=pageMisc = NULL; dirty[0]=dirty[1]=dirty[2] = false; fontchanged = false; subpage = CD_trees; InitialiseData(); }
void InitTrees(wxNotebookPage* pg){ pageTree = pg; InsertDataTrees(); subpage=CD_trees; }
void InitFont(wxNotebookPage* pg) { pageFont = pg; InsertDataFont(); subpage=CD_font; }
void InitMisc(wxNotebookPage* pg) { pageMisc = pg; InsertDataMisc(); subpage=CD_misc; }
void InitialiseData();              // Fill the temp vars with the old data. Used initially and also to revert on Cancel
void InsertDataTrees();
void InsertDataFont();
void InsertDataMisc();
void OnChangeFont();
void OnDefaultFont();
void OnCancel();
void OnOK();
void OnSelectBackgroundColours();
void OnSelectColours();
void OnConfigurePaneHighlighting();
void OnConfigureTBIcons();
void OnConfigurePreviews();
static void LoadDisplayConfig();
void OnUpdateUI();

bool dirty[3];

protected:
void Save();

bool tempSHOW_TREE_LINES;
int tempTREE_INDENT;
size_t tempTREE_SPACING;
bool tempSHOWHIDDEN;
bool tempUSE_LC_COLLATE;
bool tempTREAT_SYMLINKTODIR_AS_DIR;
bool tempCOLOUR_PANE_BACKGROUND;
bool tempSINGLE_BACKGROUND_COLOUR;
wxColour tempBACKGROUND_COLOUR_DV, tempBACKGROUND_COLOUR_FV;
bool tempUSE_STRIPES;
wxColour tempSTRIPE_0;
wxColour tempSTRIPE_1;
bool tempHIGHLIGHT_PANES;
int tempDIRVIEW_HIGHLIGHT_OFFSET;
int tempFILEVIEW_HIGHLIGHT_OFFSET;
bool tempSHOW_DIR_IN_TITLEBAR;
bool tempSHOW_TB_TEXTCTRL;
bool tempASK_ON_TRASH;
bool tempASK_ON_DELETE;
bool tempUSE_STOCK_ICONS;
wxString tempTRASHCAN;
wxFont tempTREE_FONT;
wxFont tempCHOSEN_TREE_FONT;
bool tempUSE_DEFAULT_TREE_FONT;
bool tempUSE_FSWATCHER;

wxTextCtrl *fonttext, *defaultfonttext;
bool fontchanged;

enum whichpage subpage;
wxNotebookPage *pageTree, *pageFont, *pageMisc;
ConfigureNotebook* m_nb; // The parent notebook
};


class DevicesPanel : public wxEvtHandler
{
enum whichpage { CD_AM, CD_Mt, CD_Usb, CD_Rem, CD_Fix };
public:
DevicesPanel(ConfigureNotebook* nb) : m_nb(nb) { pageAM=pageMt=pageUsb=pageRem=pageFix = NULL; dirty[0]=dirty[1]=dirty[2]=dirty[3]=dirty[4] = false;
                DeviceStructArray = new ArrayofDeviceStructs; InitialiseData(); }
~DevicesPanel() { for (int n = (int)DeviceStructArray->GetCount();  n > 0; --n)
                        {  DevicesStruct* item = DeviceStructArray->Item(n-1); delete item; DeviceStructArray->RemoveAt(n-1); }
                  delete DeviceStructArray; 
                }

void InitPgAM(wxNotebookPage* pg){ pageAM = pg; subpage = CD_AM; InsertDataPgAM(); }
void InitPgMt(wxNotebookPage* pg){ pageMt = pg; subpage = CD_Mt; InsertDataPgMt(); }
void InitPgUsb(wxNotebookPage* pg){ pageUsb = pg; subpage = CD_Usb; InsertDataPgUsb(); }
void InitPgRem(wxNotebookPage* pg);
void InitPgFix(wxNotebookPage* pg);

void OnCancel();

void InitialiseData();
void InsertDataPgAM();
void InsertDataPgMt();
void InsertDataPgUsb();
void InsertDataPgRem();
void InsertDataPgFix();

void OnOK();
void OnUpdateUI();
static void LoadConfig();
static void SaveConfig(wxConfigBase* config = NULL);

bool dirty[5];

protected:
enum DiscoveryMethod tempDEVICE_DISCOVERY_TYPE;
enum TableInsertMethod tempFLOPPY_DEVICES_DISCOVERY_INTO_FSTAB, tempCD_DEVICES_DISCOVERY_INTO_FSTAB, tempUSB_DEVICES_DISCOVERY_INTO_FSTAB;
size_t tempCHECK_USB_TIMEINTERVAL;
bool tempFLOPPY_DEVICES_AUTOMOUNT,tempCDROM_DEVICES_AUTOMOUNT,tempUSB_DEVICES_AUTOMOUNT,tempFLOPPY_DEVICES_SUPERMOUNT,tempCDROM_DEVICES_SUPERMOUNT,tempUSB_DEVICES_SUPERMOUNT,
      tempSUPPRESS_EMPTY_USBDEVICES, tempASK_BEFORE_UMOUNT_ON_EXIT;

  // FixedorUsbDevicesPanelstuff
void OnAddButton(wxCommandEvent& event);
void OnEditButton(wxCommandEvent& event);
void OnDeleteButton(wxCommandEvent& event);
void DoUpdateUI(wxUpdateUIEvent& event);
void FillListbox();
ArrayofDeviceStructs* DeviceStructArray;
wxListBox* list;
DeviceManager* pDM;

enum whichpage subpage;
wxNotebookPage *pageAM, *pageMt, *pageUsb, *pageRem, *pageFix;
ConfigureNotebook* m_nb; // The parent notebook
};

class DevicesAdvancedPanel : public wxEvtHandler  // Does the advanced device/mount config stuff like AUTOMOUNT_USB_PREFIX
{
enum whichpage { DAP_A, DAP_B, DAP_C };
public:
DevicesAdvancedPanel(ConfigureNotebook* nb) : m_nb(nb) { pageA=pageB=pageC = NULL; dirty[0]=dirty[1]=dirty[2] = false; InitialiseData(); }

void InitPgA(wxNotebookPage* pg);
void InitPgB(wxNotebookPage* pg);
void InitPgC(wxNotebookPage* pg);
void InitialiseData();
void InsertDataPgA();
void InsertDataPgB();
void InsertDataPgC();

void OnOK();
void OnUpdateUI();
static void LoadConfig();
static void SaveConfig(wxConfigBase* config = NULL);

bool dirty[3];

protected:
void OnRestoreDefaultsBtn(wxCommandEvent& event);

wxString tempPARTITIONS,tempDEVICEDIR,tempFLOPPYINFOPATH,tempCDROMINFO,tempSCSIUSBDIR,tempSCSISCSI,tempAUTOMOUNT_USB_PREFIX,tempSCSI_DEVICES,tempLVMPREFIX, tempLVM_INFO_FILE_PREFIX;

enum whichpage subpage;
wxNotebookPage *pageA, *pageB, *pageC;
ConfigureNotebook* m_nb; // The parent notebook
};

class ConfigureTerminals    // Encapsulates the Terminals notebookpage stuff
{
enum whichpage { CT_terminals, CT_emulator };
public:
ConfigureTerminals(ConfigureNotebook* nb) : fontchanged(false), m_nb(nb) { dirty[0]=dirty[1] = false; pageTerm = NULL; pageEm = NULL; combo1=NULL; InitialiseData(); }
void Init(wxNotebookPage* pg, bool real){ pageTerm = pg; subpage = CT_terminals; InsertDataReal(); }
void Init(wxNotebookPage* pg, wxNotebookPage* pgprev){ pageEm = pg; pageTerm = pgprev; subpage = CT_emulator; InsertDataEmul(); }
void InitialiseData();              // Fill the temp vars with the old data. Used initially and also to revert on Cancel
void InsertDataReal();
void InsertDataEmul();
void OnChangeFont();
void OnDefaultFont();
void OnCancel();
void OnOK();
void OnAccept(int id);
static void LoadTerminalsConfig();
static void Save(wxConfigBase* config = NULL);
void OnUpdateUI();

bool dirty[2];
protected:

wxString tempTERMINAL;
wxString tempTERMINAL_COMMAND;
wxString tempTOOLS_TERMINAL_COMMAND;
wxString tempTERMEM_PROMPTFORMAT;
wxFont tempTERMINAL_FONT;
wxFont tempCHOSEN_TERMINAL_FONT;
bool tempUSE_DEFAULT_TERMINALEM_FONT;

wxComboBox *combo1, *combo2, *combo3;
wxTextCtrl *fonttext, *defaultfonttext;
bool fontchanged;
wxArrayString tempPossibleTerminals, tempPossibleTerminalCommands, tempPossibleToolsTerminalCommands;

enum whichpage subpage;
wxNotebookPage* pageTerm, *pageEm;
ConfigureNotebook* m_nb; // The parent notebook
};

class ConfigureNet      // Encapsulates the Network notebookpage stuff
{
public:
ConfigureNet(ConfigureNotebook* nb) : m_nb(nb) { page = NULL; dirty = false; InitialiseData(); }
void Init(wxNotebookPage* pg){ page = pg; InsertData(); }
void InitialiseData();              // Fill the temp vars with the old data. Used initially and also to revert on Cancel
void InsertData();
void OnOK();
static void LoadNetworkConfig();
void AddServer();
void DeleteServer();
void OnUpdateUI();

bool dirty;
bool serverschanged;

protected:
void Save();

wxString tempSMBPATH;
wxString tempSHOWMOUNTFPATH;
wxArrayString tempNFS_SERVERS;

wxListBox* ServerList;
wxTextCtrl* NewServer;
wxTextCtrl* Showmount;
wxTextCtrl* Samba;

wxNotebookPage* page;
ConfigureNotebook* m_nb; // The parent notebook
};

class ConfigureMisc : public wxEvtHandler     // Encapsulates the Misc notebookpage stuff
{
enum whichpage { CM_undo, CM_times, CM_su, CM_other };
public:
ConfigureMisc(ConfigureNotebook* nb) : m_nb(nb) { pageUndo=pageBrieftimes=pageSu=pageOther = NULL; dirty[0]=dirty[1]=dirty[2]=dirty[3] = false; InitialiseData(); }
void InitUndo(wxNotebookPage* pg){ pageUndo = pg; InsertDataUndo(); subpage=CM_undo; }
void InitBriefTimes(wxNotebookPage* pg){ pageBrieftimes = pg; InsertDataBrieftimes(); subpage=CM_times; }
void InitSu(wxNotebookPage* pg);
void InitOther(wxNotebookPage* pg){ pageOther = pg; subpage=CM_other; }
void InitialiseData();              // Fill the temp vars with the old data. Used initially and also to revert on Cancel
void InsertDataUndo();
void InsertDataBrieftimes();
void InsertDataSu();
void OnOK();
static void LoadMiscConfig();
static void LoadStatusbarWidths(int* widths);
void OnConfigureOpen();
void OnConfigureDnD();
void OnConfigureStatusbar();
void OnExportBtn(ConfigureNotebook* note);
void OnUpdateUI();
void OnInternalExternalSu(wxCommandEvent& event); // The user switched between using the built-in or external su
void OnOtherRadio(wxCommandEvent& event); // Gets a different gui su command from the user

bool dirty[4];

protected:
void Save();

size_t tempMAX_NUMBER_OF_UNDOS;
size_t tempMAX_DROPDOWN_DISPLAY;
size_t tempAUTOCLOSE_TIMEOUT;
size_t tempAUTOCLOSE_FAILTIMEOUT;
size_t tempDEFAULT_BRIEFLOGSTATUS_TIME;
size_t tempLINES_PER_MOUSEWHEEL_SCROLL;
sutype tempWHICH_SU, tempWHICH_EXTERNAL_SU;
bool tempSTORE_PASSWD, tempUSE_SUDO;
size_t tempPASSWD_STORE_TIME;
bool tempRENEW_PASSWD_TTL;
wxString tempOTHER_SU_COMMAND;
int tempDnDXTRIGGERDISTANCE;
int tempDnDYTRIGGERDISTANCE;
int tempMETAKEYS_COPY;
int tempMETAKEYS_MOVE;
int tempMETAKEYS_HARDLINK;
int tempMETAKEYS_SOFTLINK;
int widths[4], tempwidths[4];

wxCheckBox *MySuChk, *OtherSuChk, *StorePwChk, *RenewTimetoliveChk;
wxRadioBox* BackendRadio;
wxSpinCtrl* PwTimetoliveSpin;
wxRadioButton *Ksu, *Gksu, *Gnsu, *Othersu;
enum whichpage subpage;
wxNotebookPage *pageUndo, *pageBrieftimes, *pageSu, *pageOther;
ConfigureNotebook* m_nb; // The parent notebook
};

class ConfigureNotebook  :  public wxTreebook  // Holds the pages of configure dlgs
{
enum pagename { UDShortcuts, UDCommand,UDCAddTool,UDCEditTool,UDCDeleteTool, 
                ConfigDevices, DevicesAutoMt,DevicesMount,DevicesUsb,DevicesRemovable,DevicesFixed,DevicesAdvanced,DevicesAdvancedA,DevicesAdvancedB,DevicesAdvancedC,
                ConfigDisplay,DisplayTrees,DisplayTreeFont,DisplayMisc,
                ConfigTerminals,ConfigTerminalsReal,ConfigTerminalsEm, ConfigNet, ConfigMisc,ConfigMiscUndo,ConfigMiscBriefTimes,ConfigMiscSu,ConfigMiscOther, NotSelected };
enum UDCPagetype { UDCNew, UDCEdit, UDCDelete };  // To enumerate which of the UDC subpages is loaded

public:
ConfigureNotebook(){}
~ConfigureNotebook(){ delete ConfDevices; delete ConfDevicesAdv; delete ConfDisplay; delete ConfTerminals; delete ConfNet; delete ConfMisc; m_ApplyButtons.clear(); }
void Init(Configure* dad);
void OnButtonPressed(wxCommandEvent& event);

void OnFinishedBtnUpdateUI(wxUpdateUIEvent& event); // Disable the Finished button if any of the Apply ones are enabled

bool Dirty();                                    // Check there's no unsaved data
class LaunchMiscTools* LMT;

void StoreApplyBtn(wxWindow* btn);
void SetPageStatus(bool dirty); // Change the page's icon to/from red depending on whether it has unapplied changes

protected:
void InitPage(enum pagename page);
void FillUDCMenusCombo(size_t &item, size_t indent);  // Recursively fill a combo, indenting according to sub-ness of the menu
void OnUDCLabelComboSelectionChanged(wxCommandEvent& event);
void OnPageChanged(wxTreebookEvent& event);
void OnUpdateUI(wxUpdateUIEvent& event);

int m_sel;                                      // Stores the UDTool selection during editing
enum pagename currentpage;
enum UDCPagetype UDCPage;
Configure* parent;
ArrayOfToolFolders* FolderArray;
ArrayOfUserDefinedTools* ToolArray;
wxComboBox* UDCCombo;

class ConfigureDisplay* ConfDisplay;
class DevicesPanel* ConfDevices;
class DevicesAdvancedPanel* ConfDevicesAdv;
class ConfigureTerminals* ConfTerminals;
class ConfigureNet* ConfNet;
class ConfigureMisc* ConfMisc;

bool dirty[NotSelected];
bool editing;
std::vector<wxWindow*> m_ApplyButtons;

private:
DECLARE_DYNAMIC_CLASS(ConfigureNotebook)
DECLARE_EVENT_TABLE()
};

class Configure        // The class that does all the configure things
{
public:
Configure(){ DirCtrlTBinfoarray = new ArrayofDirCtrlTBStructs; }
~Configure(){  for (int n = (int)DirCtrlTBinfoarray->GetCount();  n > 0; --n )
              { DirCtrlTBStruct* item = DirCtrlTBinfoarray->Item(n-1); delete item; DirCtrlTBinfoarray->RemoveAt(n-1); }
            delete DirCtrlTBinfoarray; 
          }
void LoadConfiguration();
void SaveConfiguration();

static bool LocateResources();    // Where is everything?
static bool LocateResourcesSilently(const wxString& SuggestedPath = wxT(""));  // Where is everything? This is just a rough first check, so no error messages/try again
static void CopyConf(const wxString& iniFilepath);  // Copy any master conf file into the local ini
static void SaveDefaultsToIni();  // Make sure there is a valid config file, with reasonably-sensible defaults
static void SaveDefaultSus();     // Save a useful value for WHICH_SU
static void SaveDefaultTerminal(enum distro Distro, double Version);// Save a useful value for TERMINAL etc
static void SaveProbableHAL_Sys_etcInfo(enum distro Distro, double Version);  // Save the probable discovery/loading setup
static void SaveDefaultEditors(enum distro Distro, double Version);  // KWrite &/or GEdit
static void SaveDefaultUDTools(); // Starter pack of UDtools
static void SaveDefaultFileAssociations();// Similarly
static void SaveDefaultDirCtrlTBButtons();
static void SaveDefaultMisc();
static void DetectFixedDevices();

static enum distro DeduceDistro(double& Version);    // Try to work out which distro & version we're on
static bool FindIni();            // If there's not a valid ini file in $HOME, use a wizard to find/create one
static wxArrayString GetXDGValues(); // Get values as per the freedesktop.org basedir-spec and the user's environment 
static bool Wizard(const wxString& configFPath);
static void FindPrograms();
static bool TestExistence(wxString name, bool executable = false);  // Is 'name' in $PATH, and optionally is it executable by us

void Configure4Pane();
void ConfigureTooltips();                            // Globally turn tooltips on/off, set their delay
void ApplyTooltipPreferences();

ArrayofDirCtrlTBStructs* DirCtrlTBinfoarray;
static bool FindRC(const wxString& SuggestedFilepath);      // Find a valid source of XRC files
protected: 
static bool FindBitmaps(const wxString& SuggestedFilepath); // Find a valid source of bitmaps
static bool FindHelp(const wxString& SuggestedFilepath);    // Find a valid source of Help

static bool CheckForConfFile(wxString& conf);        // Is there a 4Pane.conf file anywhere, from which to load some config data?

void LoadMisc();                                     // Load the one-off things like Filter history
void SaveMisc();                                     // Save the one-off things like Filter history

enum helpcontext CallersHelpcontext;                 // Stores the original value of the help context, to be replaced on finishing config
};

#endif
    // CONFIGUREH
