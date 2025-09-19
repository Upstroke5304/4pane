/////////////////////////////////////////////////////////////////////////////
// Name:       Accelerators.h
// Purpose:    Accelerators and shortcuts
// Part of:    4Pane
// Author:     David Hart
// Copyright:  (c) 2016 David Hart
// Licence:    wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef ACCELERATORSH
#define ACCELERATORSH

#include <wx/accel.h>
#include <wx/listctrl.h>

#if wxVERSION_NUMBER < 2900
  DECLARE_EVENT_TYPE(MyListCtrlEvent, wxID_ANY)           // Used to size listctrl columns
  DECLARE_EVENT_TYPE(MyReloadAcceleratorsEvent, wxID_ANY) // Sent by AcceleratorList to tell everyone to reload their menu/acceltables
#else
  wxDECLARE_EVENT(MyListCtrlEvent, wxNotifyEvent);
  wxDECLARE_EVENT(MyReloadAcceleratorsEvent, wxNotifyEvent);
#endif

#define EVT_RELOADSHORTCUTS(fn) \
        DECLARE_EVENT_TABLE_ENTRY(\
            MyReloadAcceleratorsEvent, wxID_ANY, wxID_ANY, \
            (wxObjectEventFunction)(wxEventFunction)(wxNotifyEventFunction)&fn, \
            (wxObject*) NULL),

enum { UD_NONE, UD_KEYCODEORFLAGS, UD_LABEL, UD_HELP = 4 }; // Which bits if any of a shortcut are user-defined
enum { SH_UNCHANGED = -2 };

class AccelEntry    // Encapsulates the data of wxAcceleratorEntry and wxMenuItem
{
public:
enum AE_type { AE_normal, AE_udtool, AE_unknown = wxNOT_FOUND }; // Used to distinguish between normal shortcuts and those of user-defined tools

AccelEntry(AE_type t) : flags(0), defaultflags(0), keyCode(0), defaultkeyCode(0), cmd(0), type(t), UserDefined(0) {}
AccelEntry(AccelEntry& oldentry){ *this = oldentry; }

bool Load(wxConfigBase* config, wxString& key);          // Given info about where it's stored, load data for this entry
bool Save(wxConfigBase* config, wxString& key);          // Given info about where it's stored, save data for this entry
AccelEntry& operator=(const AccelEntry& Entry)
           { flags=Entry.flags; defaultflags=Entry.defaultflags;  keyCode=Entry.keyCode; defaultkeyCode=Entry.defaultkeyCode; cmd=Entry.cmd;
             type=Entry.type; label=Entry.label; help=Entry.help; UserDefined=Entry.UserDefined; return *this; }

int GetFlags(bool usedefault=false) const { return usedefault ?  defaultflags : flags; } // Returns standard answer, or the saved-but-unused default
int GetKeyCode(bool usedefault=false) const { return usedefault ? defaultkeyCode : keyCode; } // Ditto
int GetShortcut() const { return cmd; }
wxString GetLabel() const { return label; }
wxString GetHelpstring() const { return help; }
bool IsKeyFlagsUserDefined() const { return UserDefined & UD_KEYCODEORFLAGS; }
int GetUserDefined() const { return UserDefined; }
AE_type GetType() const { return type; }

void SetFlags(int fl){ flags = fl; }
void SetKeyCode(int kc){ keyCode = kc; }
void SetDefaultFlags(int fl){ defaultflags = fl; }        // Sets defaultflags, where there is new user-defined data too
void SetDefaultKeyCode(int kc){ defaultkeyCode = kc; }    // Similarly
void SetShortcut(int sh){ cmd = sh; }
void SetLabel(wxString str){ label = str; }
void SetHelpstring(wxString str){ help = str; }
void SetKeyFlagsUserDefined(bool flag = true){ if (flag) UserDefined |= UD_KEYCODEORFLAGS; else UserDefined &= ~UD_KEYCODEORFLAGS; }
void SetUserDefined(int flags){ UserDefined = flags; }
void SetType(AE_type t){ type = t; }

protected:
void AdjustShortcutIfNeeded();                             // Transitional function, to adjust if needed for 0.8.0 change

int flags; int defaultflags;
int keyCode; int defaultkeyCode;
int cmd;
enum AE_type type;
wxString label;
wxString help;
int UserDefined;                                           // Flags whether this entry is user-defined/altered, & so needs to be saved on exit
};

WX_DEFINE_ARRAY(AccelEntry*, ArrayOfAccels);               // Define the array we'll be using


class AcceleratorList
{
public:
AcceleratorList(wxWindow* dad) : parent(dad){}
~AcceleratorList();

void Init();
void ImplementDefaultShortcuts();                          // Hack in the defaults
void LoadUserDefinedShortcuts();                           // Add any user-defined tools, & override default shortcuts with user prefs
void SaveUserDefinedShortcuts(ArrayOfAccels& array);
AccelEntry* GetEntry(int shortcutId);                      // Returns the entry that contains this shortcut ID
void CreateAcceleratorTable(wxWindow* win, int AccelEntries[], size_t shortcutNo);  // Creates that window's accel table for it
void FillMenuBar(wxMenuBar* MenuBar, bool refilling=false);// Creates/refills the menubar menus, attaches them to menubar
void AddToMenu(wxMenu& menu, int shortcutId, wxString overridelabel=wxEmptyString, wxItemKind kind=wxITEM_NORMAL, wxString helpString=wxEmptyString);  // Fills menu with the data corresponding to this shortcut
void FillEntry(wxAcceleratorEntry& entry, int shortcutId); // Fills entry with the data corresponding to this shortcut
void UpdateMenuItem(wxMenu* menu, int shortcutId);
void ConfigureShortcuts();
const ArrayOfAccels& GetAccelarray() { return Accelarray; }
static wxString ConstructLabel(AccelEntry* arrayentry, const wxString& overridelabel);  // Make a label/accelerator combination

bool dirty;

protected:
void LoadUDToolData(wxString name, int& index);          // Recursively add to array a (sub)menu 'name' of user-defined commands
int FindEntry(int shortcutId);                           // Returns the index of the shortcut within Accelarray, or -1 for not found

wxWindow* parent;
ArrayOfAccels Accelarray;
};


class MyShortcutPanel    :  public wxPanel  // Loaded by either a standalone dialog, or the appropriate page of ConfigureNotebook
{
public:
MyShortcutPanel(){ list = NULL; }
~MyShortcutPanel();
void Init(AcceleratorList* parent, bool fromNB);
int CheckForDuplication(int key, int flags, wxString& label, wxString& help); // Returns the entry corresponding to this accel if there already is one, else wxNOT_FOUND

ArrayOfAccels Temparray;
size_t DuplicateAccel;                               // Store here any entry that's just had its accel stolen from it
wxCheckBox* HideMnemonicsChk;                        // Display (or otherwise) 'Cut' instead of 'C&ut'

protected:
void OnEdit(wxListEvent& event);
void LoadListCtrl();
void DisplayItem(size_t no, bool refresh=false);
void OnHideMnemonicsChk(wxCommandEvent& event);
void SizeCols(wxNotifyEvent& event);
void OnSize(wxSizeEvent& event);
void OnColDrag(wxListEvent& event);                  // If the user adjusts a col header position
void DoSizeCols();
long GetListnoFromArrayNo(int arrayno);              // Translates from array index to list index
void OnFinished(wxCommandEvent& event);              // On Apply or Cancel.  Saves data if appropriate, does dtor things
wxListCtrl* list;
AcceleratorList* AL;
int ArrayIndex;
bool FromNotebook;                                   // Flags whether the panel was called by a standalone dialog, or by ConfigureNotebook
enum helpcontext CallersHelpcontext;                 // Stores the original value of the help context, to be replaced on finishing

private:
DECLARE_DYNAMIC_CLASS(MyShortcutPanel)
DECLARE_EVENT_TABLE()
};

class NewAccelText;

class ChangeShortcutDlg  :  public wxDialog  // The dialog that accepts the new keypress combination
{
public:
ChangeShortcutDlg(){}
void Init(wxString& action, wxString& current, wxString& dfault, AccelEntry* ntry);

NewAccelText* text;
wxStaticText* defaultstatic;                         // Holds the default keypresses, if any
wxString label;                                      // Store altered values here
wxString help;
MyShortcutPanel* parent;
protected:
void OnButton(wxCommandEvent& event);
void EndModal(int retCode);

AccelEntry* entry;
private:
DECLARE_DYNAMIC_CLASS(ChangeShortcutDlg)
DECLARE_EVENT_TABLE()
};

class NewAccelText  :  public wxTextCtrl    // Subclassed to collect key-presses
{
public:
NewAccelText(){}
void Init(wxString& current){ keycode = flags = 0; SetValue(current); }  // Set the text to the current accelerator

int keycode;
int flags;

protected:
void OnKey(wxKeyEvent& event);

private:
DECLARE_DYNAMIC_CLASS(NewAccelText)
DECLARE_EVENT_TABLE()
};

class DupAccelDlg    :    public wxDialog
{
protected:
void OnButtonPressed(wxCommandEvent& event){ EndModal(event.GetId()); }
DECLARE_EVENT_TABLE()
};

#endif
    // ACCELERATORSH

