/////////////////////////////////////////////////////////////////////////////
// Name:       Tools.h
// Purpose:    Find/Grep/Locate, Terminal Emulator
// Part of:    4Pane
// Author:     David Hart
// Copyright:  (c) 2020 David Hart
// Licence:    wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef TOOLSH
#define TOOLSH

#include "wx/wx.h"

#include "wx/notebook.h"
#include "wx/config.h"
#include "wx/process.h"
#include "wx/txtstrm.h"


class LayoutWindows;
class MyBottomPanel;
class TerminalEm;
class MyPipedProcess;
class ExecInPty;
class Tools;
class MyGenericDirCtrl;

class MyFindDialog  :  public wxDialog    // The dialog that holds the Find notebook.  Needed for the OnClearButton method
{
public:
MyFindDialog(){}
void Init(Tools* dad, const wxString& command){ parent = dad; commandstart = command; }

protected:
void OnClearButtonPressed(wxCommandEvent& event);
void OnQuickGrep(wxCommandEvent& event){ EndModal(event.GetId()); }
wxString commandstart;                  // May be "find" or "grep"
Tools* parent;
private:
DECLARE_EVENT_TABLE()
};

class QuickFindDlg;

class MyFindNotebook  :  public wxNotebook
{
friend class QuickFindDlg;
enum pagename { Path, Options, Operators, Name, Time, Size, Owner, Actions, NotSelected = -1 };

public:
MyFindNotebook(){ currentpage = Path; DataToAdd = false; }
~MyFindNotebook(){ Disconnect(wxID_ANY, wxEVT_COMMAND_NOTEBOOK_PAGE_CHANGING); } // to avoid repeated "Are you sure?" messages if the dlg is cancelled

void Init(Tools* dad, wxComboBox* searchcombo);
protected:
void GetXRCIDsIntoArrays();                        // For ease of use

void LoadCombosHistory();                          // Load the comboboxes history from (parent's) arrays
void SaveCombosHistory(int which);                 // Save this combobox's history into (parent's) array

void OnCheckBox(wxCommandEvent& event);
void OnCheckBoxPath(wxCommandEvent& event);

wxString OnAddCommandOptions();                    // Depending on which Options checkboxes were checked, return an appropriate string
wxString OnAddCommandOperators();                  // Ditto
wxString OnAddCommandName();
wxString OnAddCommandTime();
wxString OnAddCommandSize();
wxString OnAddCommandOwner();
wxString OnAddCommandAction();  
void OnAddCommand(wxCommandEvent& event);

void CalculatePermissions();                      // Parses the arrays of checkboxes, putting the result as octal string in textctrl
void OnPageChanging(wxNotebookEvent& event);
void OnPageChange(wxNotebookEvent& event);
void OnSetFocusEvent(wxCommandEvent& event);      // Does a SetFocus, delayed by wxPostEvent

void ClearPage(enum pagename page, int exceptthisitem = -1);
void OnUpdateUI(wxUpdateUIEvent& event);
void AddCommandUpdateUI();
void AddString(wxString& addition);

enum { MyFindGrepEvent_PathCombo, MyFindGrepEvent_name };

wxArrayInt PathArray;

wxArrayInt OptionsArray;
static const wxChar* OptionsStrings[];

wxArrayInt OperatorArray;
static const wxChar* OperatorStrings[];

static const wxChar* NameStrings4[]; // The version <5.0 name-order
static const wxChar* NameStrings5[];

wxArrayInt TimeArray;

wxArrayInt ActionArray;
static const wxChar* ActionStrings[];

wxArrayInt SizeArray;
static const wxChar* SizeStrings[];
wxArrayInt OwnerArray;
static const wxChar* OwnerStrings[];
static const wxChar* OwnerPermissionStrings[];
static unsigned int OwnerPermissionOctal[];
static const wxChar* OwnerStaticStrings[];

Tools* parent;
enum pagename currentpage;
bool DataToAdd;                                   // Is there data on the current page that hasn't yet been added to the final command?
wxComboBox* combo;                                // Parent dlg's command combobox
private:

DECLARE_DYNAMIC_CLASS(MyFindNotebook)
DECLARE_EVENT_TABLE()
};

class QuickFindDlg  :  public wxDialog  // Does a simpler find than the full version
{
public:
QuickFindDlg(){}
~QuickFindDlg(){}
void Init(Tools* dad);


protected:
void OnButton(wxCommandEvent& event);
void OnCheck(wxCommandEvent& event);
void OnKeyDown(wxKeyEvent &event){ if (event.GetKeyCode() == wxID_CANCEL) EndModal(wxID_CANCEL); } // For some reason this doesn't happen otherwise :/
void OnUpdateUI(wxUpdateUIEvent& event);
void OnAsterixUpdateUI(wxUpdateUIEvent& event);

void LoadCheckboxState();
void SaveCheckboxState();
void LoadCombosHistory();
void SaveCombosHistory(int which);

wxComboBox *SearchPattern, *PathFind;
wxCheckBox *IgnoreCase, *PrependAsterix, *AppendAsterix, *PathCurrentDir, *PathHome, *PathRoot;
wxRadioButton *RBName, *RBPath, *RBRegex;
Tools* parent;

private:
DECLARE_DYNAMIC_CLASS(QuickFindDlg)
DECLARE_EVENT_TABLE()
};

class MyGrepNotebook  :  public wxNotebook
{
enum pagename { Main, Dir, File, Output, Pattern, Path, NotSelected = -1 };

public:
MyGrepNotebook(){ currentpage = Main; }
~MyGrepNotebook(){ Disconnect(wxID_ANY, wxEVT_COMMAND_NOTEBOOK_PAGE_CHANGING); } // to avoid repeated "Are you sure?" messages if the dlg is cancelled

void Init(Tools* dad, wxComboBox* searchcombo);
protected:
void GetXRCIDsIntoArrays();                       // For ease of use

void LoadCombosHistory();                         // Load the comboboxes history from (parent's) arrays
void SaveCombosHistory(int which);                // Save this combobox's history into (parent's) array

void OnCheckBox(wxCommandEvent& event);
void OnCheckBoxPath(wxCommandEvent& event);

wxString OnAddCommandMain();                      // Depending on which Options checkboxes were checked, return an appropriate string
wxString OnAddCommandFile();                      // Ditto
wxString OnAddCommandOutput();
wxString OnAddCommandPattern();
void OnAddCommand(wxCommandEvent& event);

void OnRegexCrib(wxCommandEvent& event);
void OnPageChanging(wxNotebookEvent& event);
void OnPageChange(wxNotebookEvent& event);
void OnPatternPage(wxCommandEvent& event);  // Does a SetFocus, delayed by wxPostEvent

void ClearPage(enum pagename page, int exceptthisitem = -1);
void OnUpdateUI(wxUpdateUIEvent& event);
void AddCommandUpdateUI();
void AddString(wxString& addition);

wxArrayInt MainArray;
static const wxChar* MainStrings[];

wxArrayInt FileArray;

wxArrayInt OutputArray;
static const wxChar* OutputStrings[];

wxArrayInt PatternArray;
static const wxChar* PatternStrings[];

wxArrayInt PathArray;


Tools* parent;
enum pagename currentpage;
bool DataToAdd;                                   // Is there data on the current page that hasn't yet been added to the final command?
wxComboBox* combo;                                // Parent dlg's command combobox
private:

DECLARE_DYNAMIC_CLASS(MyGrepNotebook)
DECLARE_EVENT_TABLE()
};

class QuickGrepDlg  :  public wxDialog  // Does a simpler grep than the full version
{
public:
QuickGrepDlg(){}
~QuickGrepDlg(){}
void Init(Tools* dad);


protected:
void OnButton(wxCommandEvent& event);
void OnCheck(wxCommandEvent& event);
void OnKeyDown(wxKeyEvent &event){ if (event.GetKeyCode() == wxID_CANCEL) EndModal(wxID_CANCEL); } // For some reason this doesn't happen otherwise :/
void OnUpdateUI(wxUpdateUIEvent& event);
void AddWildcardIfNeeded(const bool dir_recurse, wxString& path) const; // Adds a * to the path if appropriate
void LoadCheckboxState();                     // Load the last-used which-boxes-were-checked state, + directories radio
void SaveCheckboxState();
void LoadCombosHistory();                     // Load the relevant history-arrays into comboboxes
void SaveCombosHistory(int which);            // Save the relevant command history into their arrays

wxComboBox *SearchPattern, *PathGrep;
wxCheckBox *WholeWord, *IgnoreCase, *PrefixLineno, *Binaries, *Devices, *PathCurrentDir, *PathHome, *PathRoot;
wxRadioBox* DirectoriesRadio;
Tools* parent;


private:
DECLARE_DYNAMIC_CLASS(QuickGrepDlg)
DECLARE_EVENT_TABLE()
};



class Tools
{
public:
Tools(LayoutWindows* layout);
~Tools();

void Init(enum toolchoice toolindex);
void Close();

void Clear();
void Locate();
int FindOrGrep(const wxString commandstart);
void DoGrep();                                      // Does the preferred grep variety: Quick or Full
void DoFind();                                      // Similarly
void CommandLine();

wxArrayString History;                              // Stores the command history
wxArrayString* historylist[8];                      // Lists the Find/Grep wxArrayStrings
static const wxChar* FindSubpathArray[];            // Lists the corresponding combobox names

enum toolchoice WhichTool;                          // Passed by caller, so we know which tool we're supposed to be using
TerminalEm* text;                                   // To hold caller's derived textctrl, where the output goes
wxComboBox* combo;                                  // Holds the command history, & receives input for current command
protected:
void LoadHistory();                                 // Load the relevant history into the stringarray & combobox
void LoadSubHistories();                            // Load histories for Find & Grep comboboxes
void SaveHistory();                                 // Save the relevant command history
void SaveSubHistories();                            // Save histories for Find & Grep comboboxes

MyBottomPanel* bottom;
LayoutWindows* Layout;
wxArrayString ToolNamesArray;
wxArrayString ToolTipsArray;

wxArrayString Paths;                                // These 4 store the Find histories
wxArrayString Name;
wxArrayString NewerFile;
wxArrayString ExecCmd;
wxArrayString ExcludePattern;                       // These 4 store the Grep histories
wxArrayString IncludePattern;
wxArrayString Pattern;
wxArrayString PathGrep;

wxConfigBase* config;
};



class MyBottomPanel    :    public wxPanel
{
public:
MyBottomPanel(){}

void Init(Tools* tools){ tool = tools; }

Tools* tool;
protected:
void OnButtonPressed(wxCommandEvent& event);

private:
DECLARE_DYNAMIC_CLASS(MyBottomPanel)
DECLARE_EVENT_TABLE()
};


class MyPipedProcess : public wxProcess      // Adapted from the exec sample
{
public:
MyPipedProcess(TerminalEm* dad, bool displaymessage)   :  wxProcess(wxPROCESS_REDIRECT), text(dad), DisplayMessage(displaymessage)
                                                                                    { matches = false; }
void SendOutput();                          // Sends user input from the terminal to a running process
bool HasInput();
void OnKillProcess();

long m_pid;                                 // Stores the process's pid, in case we want to kill it
wxString InputString;                       // This is where parent will temp store any user-input to be passed interactively to the command

protected:
void OnTerminate(int pid, int status);


bool matches;
TerminalEm* text;
bool DisplayMessage;
};


#if defined(__WXX11__)  
  #include <X11/Xlib.h>
#endif

class TextCtrlBase : public wxTextCtrl        // Needed in gtk2 to cure event-hijacking problems
{
public:
TextCtrlBase(){ Init(); }
TextCtrlBase(wxWindow* parent, wxWindowID id, const wxString& value = wxT(""), bool multline = true, const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize, long style = 0, const wxValidator& validator = wxDefaultValidator, const wxString& name = wxT("TextCtrlBase"))
              : wxTextCtrl(parent, id, value, pos, size, style, validator , name)
                { Init(); Connect(GetId(), wxEVT_COMMAND_TEXT_ENTER, (wxObjectEventFunction)&TextCtrlBase::OnTextFilepathEnter, NULL, this); }  // Only the toolbar textctrl uses this ctor...

void Init();
void CreateAcceleratorTable();

#if defined(__WXGTK__) || defined(__WXX11__)        
  void OnShortcut(wxCommandEvent& event, bool ChildIsTerminalEm = true);
#endif

protected:
void OnTextFilepathEnter(wxCommandEvent& event);
void OnFocus(wxFocusEvent& event);

#if defined(__WXX11__)  
  void SetupX();
  virtual void Copy();                    // These are not currently implemented in the X11 wxTextCtrl
  virtual void Cut();
  virtual void Paste();
  void DoThePaste(wxString& str);
  void OnMiddleClick(wxMouseEvent& event);
  void OnLeftUp(wxMouseEvent& event);
  void DoPrimarySelection(bool select);
  void OnCtrlA();

  Atom CLIPBOARD, TARGETS, SELECTION_PROPty;
  static wxString Selection;             // The 'Clipboard', shared amoungst all instances
  static bool OwnCLIPBOARD;              // Do we (one of the TextCtrlBases) own it?
  bool OwnPRIMARY;                       // Does *this* instance own PRIMARY?

  public:
  bool OnXevent(XEvent& xevent);

  DECLARE_EVENT_TABLE()
#endif

DECLARE_DYNAMIC_CLASS(TextCtrlBase)
};

class TerminalEm : public TextCtrlBase    // Derive from this so that (in gtk2) we can get access to misdirected shortcuts
{
  // This is based on a  comp.soft-sys.wxwindows.users post by Werner Smekal on 24/1/04, though much altered and expanded
public:
TerminalEm(){};
TerminalEm(wxWindow* parent, wxWindowID id, const wxString& value = wxT(""), bool multline = true,  const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize, long style = wxTE_PROCESS_ENTER, const wxValidator& validator = wxDefaultValidator,  const wxString& name = wxT("TerminalEm"))
            :     TextCtrlBase(parent, id, value, multline, pos, size, style, validator, name), multiline(multline) { Init(); }
~TerminalEm();
void Init();                                               // Do the ctor work here, as otherwise wouldn't be done under xrc

void SetOutput(TerminalEm* TE){ display=TE; }              // For singleline command-line version, tells it where its output should go

void RunCommand(wxString& command, bool external = true);  // Do the Process/Execute things to run the command
void HistoryAdd(wxString& command);
void AddInput(wxString input);                             // Displays input received from the running Process
void OnProcessTerminated(MyPipedProcess* process = NULL);
void AddAsyncProcess(MyPipedProcess* process = NULL);
void RemoveAsyncProcess(MyPipedProcess* process = NULL);
void OnCancel();
void OnClear();
void OnChangeDir(const wxString& newdir, bool ReusePrompt);// Change the cwd, this being the only way to cd in the terminal/commandline
void WritePrompt();
void OnEndDrag();                                          // Drops filenames into the prompt-line
void OnKey(wxKeyEvent& event);

bool multiline;                                            // Is this the single-line version for command-entry only, or multiline to display results

protected:
void LoadHistory();
void SaveHistory();
wxString GetPreviousCommand();
wxString GetNextCommand();
int CheckForSu(const wxString& command);  // su fails and probably hangs, so check and abort here. su -c and sudo need different treatment
bool CheckForCD(wxString command);        // cd refuses to work, so fake it here
void ClearHistory();
inline void FixInsertionPoint();
void OnMouseDClick(wxMouseEvent& event);
void OnSetFocus(wxFocusEvent& event);

void ShowContextMenu(wxContextMenuEvent& event);
#ifdef __WXX11__
void OnRightUp(wxMouseEvent& event);      // EVT_CONTEXT_MENU doesn't seem to work in X11
#endif
wxString ExtendSelection(long f = -1, long t = -1); // Extend selected text to encompass the whole contiguous word
void OnOpenOrGoTo(wxCommandEvent& event); // Called by Context Menu
void DoOpenOrGoTo(bool GoTo, const wxString& selection = wxT(""));             // Used by OnMouseDClick() & Context Menu

void OnTimer(wxTimerEvent& event);
void OnIdle(wxIdleEvent& event);
MyPipedProcess* m_running;
ExecInPty* m_ExecInPty;
wxTimer m_timerIdleWakeUp;                // The idle event wake up timer

wxString GetCurrentPrompt() const { return m_CurrentPrompt; }
int GetCommandStart() const { return m_CommandStart; }

static wxArrayString History;             // We want only 1 history, shared between instances
static int HistoryCount;
static bool historyloaded;                //   & we want only to load it once!
static int QuerySuperuser;                // Helps determine the command prompt.  See Init()
int m_CommandStart;
wxString UnenteredCommand;                // This is for an entry that has been typed in, but before a <CR> the command-history is invoked
wxString PromptFormat;                    // User-definable prompt appearance definition
wxString m_CurrentPrompt;                 // The current prompt string
TerminalEm* display;                      // Where to output.  If multiline, it's 'this', otherwise it's set by SetOutput()
wxString originaldir;
bool busy;                                // Set by OnKey when it's processing a command to prevent an undesirable cd
bool promptdir;                           // If set, the cwd features in the prompt, so redo the prompt with changes of selection
private:
DECLARE_DYNAMIC_CLASS(TerminalEm)
DECLARE_EVENT_TABLE()
};


class MyBriefLogStatusTimer  :   public wxTimer  // Used by BriefLogStatus.  Notify() clears the zero field of the statusbar when the timer 'fires'
{
public:
void Init(wxFrame* frme, wxString& olddisplay, int pne = 0){ frame = frme; pane = pne; originaldisplay = olddisplay; }
protected:
void Notify() { if (originaldisplay.IsEmpty())  originaldisplay = wxT(" ");    // If empty, replace with a space, as "" doesn't erase
                frame->SetStatusText(originaldisplay, pane); 
              }

wxString originaldisplay;
wxFrame* frame;
int pane;
};

class BriefLogStatus  // Prints a LogStatus message that displays only for a defined time
{
public:
BriefLogStatus(wxString Message, int secondsdisplayed = -1, int whichstatuspane = 1);
static void DeleteTimer() { delete BriefLogStatusTimer; BriefLogStatusTimer = NULL; }
protected:
static MyBriefLogStatusTimer* BriefLogStatusTimer;
};

// ------------------------------------------------------------------------------------------------------------------------------

class MyProcess : public wxProcess
{
public:
MyProcess(const wxString& cmd, const wxString& toolname, bool terminal, bool RefreshPane, bool RefreshOpposite)
      : wxProcess(), m_cmd(cmd), m_toolname(toolname), m_UseTerminal(terminal), m_RefreshPane(RefreshPane), m_RefreshOpposite(RefreshOpposite) {}

virtual void OnTerminate(int pid, int status);

protected:
wxString m_cmd;
wxString m_toolname;
bool m_UseTerminal;
bool m_RefreshPane;
bool m_RefreshOpposite;
};

struct UserDefinedTool; struct UDToolFolder;

WX_DEFINE_ARRAY(struct UserDefinedTool*, ArrayOfUserDefinedTools);
WX_DEFINE_ARRAY(struct UDToolFolder*, ArrayOfToolFolders);

struct UserDefinedTool
  { wxString Filepath;                // The launch path
    bool AsRoot;                      // Run it as root
    bool InTerminal;                  // Run it in a terminal?
    bool TerminalPersists;            // The terminal doesn't self-close
    bool RefreshPane;                 // Refresh current pane when the tool finishes
    bool RefreshOpposite;             // Refresh the opposite pane too
    wxString Label;                   // What the menu displays
    size_t ID;                        // The tool's event.Id
  };
  
struct UDToolFolder
  { UDToolFolder(){ Entries=0; Subs=0; }
    wxString Label;                   // What the menu displays
    size_t Entries;                   // How many entries does this menu have.  Tools, not submenus
    size_t Subs;                      // How many submenus does this menu have
  };
  

class LaunchMiscTools    // Class to display in a menu, manage and launch misc external programs eg du, df, KDiff
{
public:
LaunchMiscTools(){ EventId = SHCUT_TOOLS_LAUNCH; LastCommandNo = (size_t)-1; }
~LaunchMiscTools(){ ClearData(); }
void LoadMenu(wxMenu* tools = NULL);              // Load arrays & menu from config, If null, load to the menubar menu
void LoadArrays();
void SaveMenu();                                  // Save arrays, delete menu
void Save(wxString name, wxConfigBase* config);   // Does the recursive Save (also from Export())
void Export(wxConfigBase* config);                // Exports the data to a conf file
void DeleteMenu(size_t which, size_t &entries);   // Remove from the arrays this menu & all progeny
bool AddTool(int folder, wxString& command, bool asroot, bool interminal, bool terminalpersists, bool refreshpane, bool refreshopp, wxString& label);
bool DeleteTool(int sel);

void Run(size_t index);                           // To run an entry
void RepeatLastCommand();

ArrayOfUserDefinedTools* GetToolarray(){ return &ToolArray; }
ArrayOfToolFolders* GetFolderArray(){ return &FolderArray; }
size_t GetLastCommand(){ return LastCommandNo; }

static void SetMenuIndex(int index) { m_menuindex = index; }

protected:
void Load(wxString name);                     // Load (sub)menu 'name' from config
void ClearData();
void AddToolToArray(wxString& command, bool asroot, bool interminal, bool terminalpersists, bool refreshpane, bool refreshopp, wxString& label, int insertbeforeitem = -1);
void FillMenu(wxMenu* parentmenu);            // Recursively append entries and submenus to parentmenu
wxString ParseCommand(wxString& command);     // Goes thru command, looking for parameters to replace
bool SetCorrectPaneptrs(const wxChar* &pc, MyGenericDirCtrl* &first, MyGenericDirCtrl* &second); // Helper for ParseCommand(). Copes with modifiers

ArrayOfUserDefinedTools ToolArray;
ArrayOfToolFolders FolderArray;
int EventId;                                  // Holds the first spare ID, from SHCUT_TOOLS_LAUNCH to SHCUT_TOOLS_LAUNCH_LAST-1
static int m_menuindex;                       // Passed to wxMenuBar::GetMenu so we can ID the menu without using its label
size_t EntriesIndex;
size_t FolderIndex;
size_t LastCommandNo;                         // Holds the last tool used, so it can be repeated
MyGenericDirCtrl *active, *leftfv, *rightfv, *leftdv, *rightdv;
};

#if defined(__WXGTK__) 
  class EventDistributor : public wxEvtHandler  // Needed in gtk2 to cure event-hijacking problems
  {
  void CheckEvent(wxCommandEvent& event);
    DECLARE_EVENT_TABLE()
  };
#endif

#endif
    //TOOLSH
