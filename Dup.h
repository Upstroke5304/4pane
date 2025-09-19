/////////////////////////////////////////////////////////////////////////////
// Name:       Dup.h
// Purpose:    Checks/manages Duplication & Renaming
// Part of:    4Pane
// Author:     David Hart
// Copyright:  (c) 2016 David Hart
// Licence:    GPL v3
/////////////////////////////////////////////////////////////////////////////
#ifndef DUPH
#define DUPH
#include "wx/wx.h"
#include <wx/spinctrl.h>

class PasteThreadSuperBlock;

class CheckDupRen
{
public:
CheckDupRen(MyGenericDirCtrl* caller, const wxString& origin_path, const wxString& dest_path, bool fromarchive=false, time_t modtime = 0)  
                                : parent(caller), incomingpath(origin_path), destpath(dest_path), FromArchive(fromarchive), FromModTime(modtime)
      { IsMultiple = false; WhattodoifCantRead = WhattodoifClash = DR_Unknown; m_tsb = NULL; }
bool CheckForAccess(bool recurse = true);   // This is a convenient place to check for read permission when in a Moving/Pasting loop.  Recurse means check permissions of dir contents too --- not needed for Rename
bool CheckForIncest();                      // Ensure we don't try to move a dir onto a descendant --- which would cause an indef loop
enum CDR_Result CheckForPreExistence();     // Ensure we don't accidentally overwrite
int CheckForPreExistenceInArchive();        // Should we overwrite or dup inside an archive
static int RenameFile(wxString& originpath, wxString& originname, wxString& newname, bool dup);  // The ultimate mechanisms.  Static as used by UnRedo too
static int RenameDir(wxString& destpath, wxString& newpath, bool dup);
static bool MultipleRename(wxWindow* parent, wxArrayString& OldFilepaths, wxArrayString& NewFilepaths, bool dup);
static bool ChangeLinkTarget(wxString& linkfilepath, wxString& destfilepath);    // Here for convenience:  change the target of a symlink
void SetTSB(PasteThreadSuperBlock* tsb) { m_tsb = tsb; }
wxString finalpath;
wxString finalbit;
bool ItsADir;
bool IsMultiple;
enum DupRen WhattodoifClash;
enum DupRen WhattodoifCantRead;  // We only use SkipAll this time
wxString GetOverwrittenFilepath() const { return m_overwrittenfilepath; }

protected:
MyGenericDirCtrl* parent;
wxString incomingpath;
wxString inname;
wxString destpath;
wxString destname;
wxString newname;               // The name to call the duplicated/renamed dir/file
wxString stubpath;              // The original destpath splits into stubpath & lastseg
wxString lastseg;

wxString m_overwrittenfilepath;

wxArrayInt IDs;

bool IsDir;
bool dup;                       // Flags whether we're duplicating (intentionally or otherwise)

bool FromArchive;               // Used to flag that we're Pasting from an archive, so can't conveniently rename if there's a clash
time_t FromModTime;             // It's easier to pass the modtime of an archive entry than to work it out here!
PasteThreadSuperBlock* m_tsb;   // Used to coordinate a trashdir

CDR_Result DirAdjustForPreExistence(bool intoArchive = false, time_t modtime = 0);
CDR_Result FileAdjustForPreExistence(bool intoArchive = false, time_t modtime = 0);
enum DupRen OfferToRename(int dialogversion, bool intoArchive = false, time_t modtime = 0);
bool GetNewName(wxString& destpath, wxString& destname);

static bool DoDup(wxString& originalpath, wxString& newpath);  // Used by RenameDir to duplicate dirs, so must be static too
};

class MultipleRenameDlg  :  public wxDialog
{
public:
MultipleRenameDlg(){}
void Init();

wxTextCtrl* IncText;
wxRadioBox* DigitLetterRadio;
wxRadioBox* CaseRadio;
wxRadioButton *ReplaceAllRadio, *ReplaceOnlyRadio;
wxSpinCtrl* MatchSpin;
wxComboBox *ReplaceCombo, *WithCombo;
wxComboBox *PrependCombo, *AppendCombo;
int IncWith, Inc;

protected:
void OnRegexCrib(wxCommandEvent& event);
void OnRadioChanged(wxCommandEvent& event);
void OnSpinUp(wxSpinEvent& event);
void OnSpinDown(wxSpinEvent& event);
void SetIncText();
void PanelUpdateUI(wxUpdateUIEvent& event);
void ReplaceAllRadioClicked(wxCommandEvent& event);
void ReplaceOnlyRadioClicked(wxCommandEvent& event);

enum { digit, upper, lower };
private:
DECLARE_DYNAMIC_CLASS(MultipleRenameDlg)
DECLARE_EVENT_TABLE()
};

class MultipleRename    // Class to implement multiple rename/dup
{
public:
MultipleRename(wxWindow* dad, wxArrayString& OldFP, bool duplicate) : parent(dad), OldFilepaths(OldFP), dup(duplicate) {}
wxArrayString DoRename();
static wxString IncText(int type, int start);
protected:
bool DoRegEx(wxString& String, wxString& ReplaceThis, wxString& WithThis, size_t matches );
bool DoPrependAppend(bool body, wxString& Prepend, wxString& Append);
bool DoInc(bool body, int InitialValue, int type, bool AlwaysInc);
bool CheckForClash(wxString Filepath, size_t index);
void LoadHistory();
void SaveHistory();

enum { digit, upper, lower };
wxWindow* parent;
wxArrayString& OldFilepaths; // This must be a reference, otherwise an only-alter-matching-files situation will make caller lose registration
wxArrayString NewFilepaths;
bool dup;
MultipleRenameDlg dlg;
};

#endif
    // DUPH
