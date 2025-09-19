/////////////////////////////////////////////////////////////////////////////
// Name:       Archive.h
// Purpose:    Non-virtual archive stuff
// Part of:    4Pane
// Author:     David Hart
// Copyright:  (c) 2016 David Hart
// Licence:    GPL v3
/////////////////////////////////////////////////////////////////////////////
#ifndef ARCHIVEH
#define ARCHIVEH

#include "wx/xrc/xmlres.h"

enum returntype { cancel, valid, retry };

class Archive;

class DecompressOrExtractDlg  :  public wxDialog      // Simple dialog to allow choice between Archive & Compressed in ExtractOrVerify
{
protected:
void OnDecompressButton(wxCommandEvent& event){ EndModal(XRCID("Compressed")); }
void OnExtractButton(wxCommandEvent& event){ EndModal(XRCID("Archive")); }
DECLARE_EVENT_TABLE()
};

class MyListBox  :  public wxListBox                  // A control in the Create dialog.  Subclassed so permit user to delete unwanted entries
{
public:
MyListBox(){}
void Add(wxString newfile);                           // Appends to the list only if not already present
protected:
void OnKey(wxKeyEvent& event);
private:
DECLARE_DYNAMIC_CLASS(MyListBox)
DECLARE_EVENT_TABLE()
};

class ArchiveDialogBase  :  public wxDialog
{
public:
ArchiveDialogBase(Archive* parent = NULL) : m_parent(parent) {}
void Init();

static bool IsAnArchive(const wxString& filename, bool append = false);     // Decide if filename is an archive suitable for extracting and/or appending to
static wxString FindUncompressedName(const wxString& archivename, enum ziptype ztype);  // What'd a compressed archive be called if uncompressed?

protected:

void OnAddFile(wxCommandEvent& event);       // Add the contents of combo to the listbox
void OnFileBrowse(wxCommandEvent& event);

wxComboBox* combo;
MyListBox* list;
Archive* m_parent;
private:
DECLARE_EVENT_TABLE()
};

class MakeArchiveDialog  :  public ArchiveDialogBase
{
public:
MakeArchiveDialog(){}
MakeArchiveDialog(Archive* parent, bool newarch);

enum returntype GetCommand();                       // Parse entered options into a command

protected:
void OnCreateInBrowse(wxCommandEvent& event);
void OnCheckBox(wxCommandEvent& event);
void OnUpdateUI(wxUpdateUIEvent& event);
enum returntype GetNewCommand();                    // Parse entered options into command for a new archive
enum returntype GetAppendCommand();                 // Parse entered options into command to append to an archive
void OnOK(wxCommandEvent& event);
wxString MakeTarCommand(const wxString& archivename);  //Submethod of GetNewCommand/GetAppendCommand
wxString AppendCompressedTar(const wxString& archivename, enum ziptype ztype );  // Handles the uncompress, append, recompress contruction
wxString AddSortedFiles(bool prependingpaths = true);  // Returns files sorted by dir, each dir preceded by a -C unless prependingpaths is false

wxRadioBox* m_compressorradio;
wxComboBox* createincombo;
bool OKEnabled;                                     // Flags whether the OK button should be enabled
bool NewArchive;
private:
DECLARE_EVENT_TABLE()
};

class CompressDialog  :  public ArchiveDialogBase
{
public:
CompressDialog(){}
CompressDialog(Archive* parent);

enum returntype GetCommand();                       // Parse entered options into a command

protected:
void OnUpdateUI(wxUpdateUIEvent& event);
void AddRecursively(const wxString& dir, wxArrayString& filearray, bool recurse); // Put files from this dir, & optionally all its subdirs, into filearray

private:
DECLARE_EVENT_TABLE()
};


class DecompressDialog  :  public ArchiveDialogBase
{
public:
DecompressDialog(){}
DecompressDialog(Archive* parent);

enum returntype GetCommand();                       // Parse entered options into a command

protected:
bool SortFiles(const wxArrayString& selected, wxArrayString& gz, wxArrayString& bz, wxArrayString& xz, wxArrayString& sevenz, wxArrayString& lzo, bool recurse, bool archivestoo = false);  //  Sort selected files into .bz, .gz & rubbish.  Recurses into dirs if recurse
void OnUpdateUI(wxUpdateUIEvent& event);

private:
DECLARE_EVENT_TABLE()
};

class VerifyCompressedDialog  :  public DecompressDialog  // Note whence this class derives.  Means it can use SortFiles
{
public:
VerifyCompressedDialog(){}
VerifyCompressedDialog(Archive* parent);

enum returntype GetCommand();                       // Parse entered options into a command
};

class ExtractArchiveDlg  :  public wxDialog
{
public:
ExtractArchiveDlg(){}
ExtractArchiveDlg(Archive* parent);
void Init();

enum returntype GetCommand();                       // Parse entered options into a command

protected:
void OnUpdateUI(wxUpdateUIEvent& event);
void OnCreateInBrowse(wxCommandEvent& event);       // Browse for the dir into which to extract the archive
virtual void OnOK(wxCommandEvent& event);
void OnFileBrowse(wxCommandEvent& event);

wxTextCtrl* text;
wxComboBox* CreateInCombo;
Archive* m_parent;
private:
DECLARE_EVENT_TABLE()
};

class VerifyArchiveDlg  :  public ExtractArchiveDlg
{
public:
VerifyArchiveDlg(){}
VerifyArchiveDlg(Archive* parent);

enum returntype GetCommand();                       // Parse entered options into a command

protected:
virtual void OnOK(wxCommandEvent& event);
};

class MyGenericDirCtrl;

class Archive  // which, unsurprisingly, deals with archives and (de)compressing. Ordinary, not streams
{
enum ExtractCompressVerify_type { ecv_compress, ecv_decompress, ecv_verifycompressed, ecv_makearchive, ecv_addtoarchive, ecv_extractarchive, ecv_verifyarchive, ecv_invalid };

public:
Archive(MyGenericDirCtrl* current)  :  active(current) { LoadPrefs(); IsThisCompressorAvailable(zt_7z);  /* Do this here to set m_7zString*/ }
~Archive(){ SavePrefs(); }

void CreateOrAppend(bool NewArchive){ DoExtractCompressVerify(NewArchive ? ecv_makearchive : ecv_addtoarchive); }
void Compress(){ DoExtractCompressVerify(ecv_compress); }
void ExtractOrVerify(bool extract=true);    // Extracts or verifies either archives or compressed files, depending on which are selected

static enum ziptype Categorise(const wxString& filetype);        // Returns the type of file in filetype ie .gz, .tar etc
static enum GDC_images GetIconForArchiveType(const wxString& filename, bool IsFakeFS); // Returns the correct treectrl icon for this type of archive/package/compressed thing
static enum GDC_images GetIconForArchiveType(ziptype zt, bool IsFakeFS);
static bool IsThisCompressorAvailable(enum ziptype type);

wxString command;                           // The command to be executed
wxArrayString FilepathArray;                // Holds the files to be archived
wxArrayString ArchiveHistory;               // Stores the archive filepath history
MyGenericDirCtrl* active;
wxArrayString Updatedir;                    // Holds dirs that will need to be refreshed afterwards

unsigned int Slidervalue;                   // Store values, loaded from config, representing last-used choices for dialogs
unsigned int m_Compressionchoice;
bool Recurse;
bool DecompressRecurse;
bool Force;
bool DecompressForce;
bool DecompressArchivesToo;
bool ArchiveExtractForce;
bool DerefSymlinks;
bool m_Verify;
bool DeleteSource;
unsigned int ArchiveCompress;
const wxString& GetSevenZString() const { return m_7zString; }

protected:
void DoExtractCompressVerify(enum ExtractCompressVerify_type type);
void LoadHistory();                         // Load the relevant history for combobox
void SaveHistory();
void LoadPrefs();                           // Load the last-used values for checkboxes etc
void SavePrefs();
void UpdatePanes();                         // Work out which dirs are most likely to have been altered, & update them

static wxString m_7zString;
wxConfigBase* config;
};

#endif
    //ARCHIVEH



