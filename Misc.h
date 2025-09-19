/////////////////////////////////////////////////////////////////////////////
// Name:       Misc.h
// Purpose:    Misc stuff
// Part of:    4Pane
// Author:     David Hart
// Copyright:  (c) 2016 David Hart
// Licence:    wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef MISCH
#define MISCH

#include "wx/wx.h"
#include "wx/dirctrl.h"
#if wxVERSION_NUMBER >= 3000
  #include <wx/html/helpctrl.h>
#endif

#include <vector>
#include <deque>

extern wxString GetCwd();
extern bool SetWorkingDirectory(const wxString& dir);
extern wxColour GetSaneColour(wxWindow* win, bool bg, wxSystemColour type);
extern void UnexecuteImages(const wxString& filepath);
extern void KeepShellscriptsExecutable(const wxString& filepath, size_t perms);
extern bool IsThreadlessMovePossible(const wxString& origin, const wxString& destination);
#if wxVERSION_NUMBER > 3101
  extern wxFontWeight LoadFontWeight(const wxString& configpath);
#else
  extern int LoadFontWeight(const wxString& configpath);
#endif
extern void SaveFontWeight(const wxString& configpath, int fontweight);

// A class that displays a plain message, rather like wxMessageBox, but without buttons.  It autocloses after a user-defined no of seconds
class BriefMessageBox;

class MyBriefMessageTimer : public wxTimer  // Used by BriefMessageBox.  Notify() kills the BriefMessageBox when the timer 'fires'
{
public:
void Setup(wxDialog* ptrtobox ){ p_box = ptrtobox; }
protected:
void Notify(){ p_box->EndModal(wxID_CANCEL); }

wxDialog* p_box;
};

class BriefMessageBox : public wxDialog  // Displays a message dialog for a defined time only
{
public:
BriefMessageBox(wxString Message, double secondsdisplayed = -1, wxString Caption = wxEmptyString);
protected:
MyBriefMessageTimer BriefTimer;
};

enum  // for FileDirDlg
{
  FD_MULTIPLE = 0x0001,
  FD_SHOWHIDDEN = 0x0002,
  FD_RETURN_FILESONLY = 0x0004
};

class FileDirDlg  :  public wxDialog  // A dialog that can select both files and dirs
{
public:
FileDirDlg(){}
FileDirDlg(wxWindow* parent, int style=FD_MULTIPLE | FD_SHOWHIDDEN, wxString defaultpath=wxEmptyString, wxString label =_( "File and Dir Select Dialog" ));
wxString GetFilepath(){ return filepath; }              // Get a single filepath
size_t GetPaths(wxArrayString& filepaths);            // Get multiple filepaths. Returns number found

protected:
void OnSelChanged(wxTreeEvent& event);
void OnHome(wxCommandEvent& event);
void OnHidden(wxCommandEvent& event);
#if !defined(__WXGTK3__)
  void OnEraseBackground(wxEraseEvent& event);
#endif

wxString filepath;
wxGenericDirCtrl* GDC;
wxTextCtrl* Text;
int SHCUT_HOME;
bool filesonly;

DECLARE_DYNAMIC_CLASS(FileDirDlg)
};

class PasswordManager : public wxEvtHandler
{
public:
const wxString GetPassword(const wxString& cmd, wxWindow* caller = NULL); // Returns the password, first asking the user for it if need be
void ForgetPassword() { m_password.Clear(); } // Clear the stored pw, presumably because it was rejected by su

static PasswordManager& Get() { if (!ms_instance)	ms_instance = new PasswordManager; return *ms_instance; }
static void Release() { delete ms_instance; ms_instance = NULL; }

protected:
void SetPasswd(const wxString& password);   // Encrypts and stores the password
wxString GetPasswd() const;                 // Decrypts and returns the password

void OnPasswordTimer(wxTimerEvent& WXUNUSED(event)) { m_password.Clear(); }

wxTimer m_timer;
bool m_extendLife;                          // If true, extend the life of the password every time it's used
wxString m_password;
wxString m_key;

private:
PasswordManager() : wxEvtHandler()
    { m_timer.SetOwner(this);
      Connect(wxEVT_TIMER, wxTimerEventHandler(PasswordManager::OnPasswordTimer), NULL, this);
    }

static PasswordManager* ms_instance;
};


class ExecInPty  // Wrapper for ExecuteInPty()
{
#define ERROR_RETURN_CODE -1    // We're always executing wxEXEC_SYNC
#define CANCEL_RETURN_CODE -2   // If the user cancelled in the password dialog
public:
ExecInPty(wxTextCtrl* caller = NULL) : m_caller(caller) {}

long ExecuteInPty(const wxString& cmd);

wxTextCtrl* GetCallerTextCtrl() { return m_caller; } // Get calling textctrl. May return NULL
wxArrayString& GetOutputArray() { return m_outputArray; }
wxArrayString& GetErrorsArray() { return m_errorsArray; }

  // 'Process' interface
void AddInputString(const wxString& input) { m_inputArray.Add(input); } // User-input from caller
void WriteOutputToTextctrl();                        // The caller wants any pty output

protected:
void InformCallerOnTerminate();                      // Let the caller know we're finished
int CloseWithError(int fd, const wxString& msg, int errorcode = ERROR_RETURN_CODE); // Close on error or empty password

wxTextCtrl* m_caller;
wxArrayString m_inputArray;
wxArrayString m_outputArray;
wxArrayString m_errorsArray;
};

extern long ExecuteInPty(const wxString& cmd, wxArrayString& output, wxArrayString& errors);
extern long ExecuteInPty(const wxString& cmd);

#if wxVERSION_NUMBER >= 3000
class HtmlHelpController : public wxHtmlHelpController
{
public:
HtmlHelpController(int style=wxHF_DEFAULT_STYLE, wxWindow *parentWindow=NULL) : wxHtmlHelpController(style, parentWindow){}
virtual ~HtmlHelpController() {}

virtual bool DisplaySection(const wxString& section) { return Display(section); }
bool Display(const wxString& x);

void OnCloseWindow(wxCloseEvent& evt);
};
#endif

class ThreadSuperBlock;
class ThreadBlock;
class UnRedo;

struct PasteData
{
PasteData(const wxString& o, const wxString& d, const wxString& dl, const wxString& ow, bool p = false)
  : origin(o.c_str()), dest(d.c_str()), del(dl.c_str()), overwrite(ow.c_str()), precreated(p) {}
PasteData& operator=(const PasteData& pdata) 
  { origin=pdata.origin; dest=pdata.dest; del=pdata.del; overwrite=pdata.overwrite; precreated= pdata.precreated; return *this; }

wxString origin;
wxString dest;
wxString del;
wxString overwrite;
bool     precreated;
};

class PasteThread : public wxThread
{
public:
  PasteThread(wxWindow* caller, int ID, const std::vector<PasteData>& pdata, bool ismoving = false)
                        : wxThread(), m_caller(caller), m_ID(ID), m_PasteData(pdata), m_ismoving(ismoving) {}
  virtual ~PasteThread() { m_PasteData.clear(); }
  void SetUnRedoType(enum UnRedoType type) { m_fromunredo = type; }
  void* Entry();
  void OnExit();

protected:
  virtual bool ProcessEntry(const PasteData& data);
  bool CopyFile(const wxString& origin, const wxString& destination); // Does an interruptable copy

  wxWindow* m_caller;
  int m_ID;
  std::vector<PasteData> m_PasteData;
  wxArrayString m_successfulpastes; // These are the ones that didn't fail/weren't cancelled, and so can be UnRedone
  bool m_ismoving;
  enum UnRedoType m_fromunredo;
  wxArrayString m_needsrefreshes; // For passing overwritten filepaths that FSWatcher may fail with, as a delete is followed by a paste
};

class PastesCollector // Collects a clipboardful of Paste/Move data and, when fully loaded, feeds it to PasteThread
{
public:
PastesCollector() : m_moving(false) {}
~PastesCollector() { m_PasteData.clear(); }
void SetParent(ThreadSuperBlock* tsb) { m_tsb = tsb; }
void SetIsMoves(bool moving) {m_moving = moving; }
bool GetIsMoves() const { return m_moving; }
void Add(const wxString& origin, const wxString& dest, const wxString& originaldest, bool createddir = false);
void AddPreCreatedItem(const wxString& origin, const wxString& dest) 
  { Add(origin, dest, wxT(""), true); }
void StartThreads();

size_t GetCount() const { return m_PasteData.size(); }
  
protected:
void DoStartThread(const std::vector<PasteData>& pastedata, ThreadBlock* block, int threadID);

bool m_moving; // Is it for Moves instead of Pastes?
std::vector<PasteData> m_PasteData;
ThreadSuperBlock* m_tsb;
};

class ThreadData
{ 
public:
ThreadData() : ID(0), completed(false), thread(NULL) {}
~ThreadData();
int ID;
bool completed;
wxThread* thread;
};

class ThreadBlock
{
public:
ThreadBlock(size_t count, unsigned int firstID, ThreadData* data, ThreadSuperBlock* parent = NULL)
          : m_count(count), m_firstID(firstID), m_successes(0), m_failures(0), m_parent(parent), m_data(data) {}
virtual ~ThreadBlock() { delete[] m_data; }

bool Contains(unsigned int ID) const;
bool IsCompleted() const; // Have all the block's threads completed (or been aborted)
void AbortThreads() const;
virtual void CompleteThread(unsigned int ID,  const wxArrayString& successes, const wxArrayString& array); // Set that ThreadData as completed (or aborted)

size_t GetCount() const { return m_count; }
size_t GetFirstID() const { return m_firstID; }
ThreadSuperBlock* GetParent() const { return m_parent; }
void SetSuperblock(ThreadSuperBlock* tsb) { m_parent = tsb; }

wxThread* GetThreadPointer(unsigned int ID) const;
void SetThreadPointer(unsigned int ID, wxThread* thread);

protected:
virtual void OnBlockCompleted(); // Any action to be taken when all the threads have completed

size_t m_count;
unsigned int m_firstID;
size_t m_successes;
size_t m_failures;
ThreadSuperBlock* m_parent;

ThreadData* m_data;
};

class ThreadSuperBlock  // Manages a collection of items to be threaded
{
public:
ThreadSuperBlock(unsigned int firstID)
        : m_count(0), m_FirstThreadID(firstID), m_fromunredo(URT_notunredo), m_redoID(-1), m_successes(0), m_failures(0), m_overallsuccesses(0), m_overallfailures(0), m_messagetype(_("completed"))
          { m_collector.SetParent(this); }
virtual ~ThreadSuperBlock();

void StoreBlock(ThreadBlock* block);
bool Contains(unsigned int ID) const;
bool IsCompleted() const; // Have all the contained block's threads completed (or been aborted)
void CompleteThread(unsigned int ID, const wxArrayString& successes, const wxArrayString& stringarray); // Set that ThreadData as completed (or aborted)
void SetThreadPointer(unsigned int ID, wxThread* thread);
int  GetBlockForID(unsigned int containedID) const; // Returns the block index or wxNOT_FOUND
UnRedoType GetUnRedoType() const { return m_fromunredo; }
void SetFromUnRedo(UnRedoType unredotype) { m_fromunredo = unredotype; }
size_t GetUnRedoId() const { return m_redoID; }
void SetUnRedoId(size_t ID) { m_redoID = ID; }
void AddSuccesses(size_t successes) { m_successes += successes; }
void AddFailures(size_t failures) { m_failures += failures; }
void AddOverallSuccesses(size_t successes) { m_overallsuccesses += successes; }
void AddOverallFailures(size_t failures) { m_overallfailures += failures; }
void SetMessageType(const wxString& type) { m_messagetype = type; }
virtual void OnCompleted() {}      // Any action to be taken when all the blocks have completed

void AbortThreads() const;

void AddToCollector(const wxString& origin, const wxString& dest, const wxString& originaldest) { m_collector.Add(origin, dest, originaldest); }
void AddPreCreatedItem(const wxString& origin, const wxString& dest) { m_collector.AddPreCreatedItem(origin, dest); }
bool GetCollectorIsMoves() const { return m_collector.GetIsMoves(); }
void SetCollectorIsMoves(bool moves) { m_collector.SetIsMoves(moves); }
void StartThreads() { m_collector.StartThreads(); }

PastesCollector& GetCollector() { return m_collector; }

protected:

PastesCollector m_collector;
size_t m_count;
unsigned int m_FirstThreadID;
std::vector<ThreadBlock*> m_blocks;
enum UnRedoType m_fromunredo; // Is this tsb being used by the UnRedoManager? If so, is it an undo or a redo?
size_t m_redoID;           // If so, this stores the ID of the unredo
size_t m_successes;        // The individual successes/failures i.e. every file, every dir plus all the files in that dir...
size_t m_failures;
size_t m_overallsuccesses; // Overall successes/failures of the items initially passed to the superblock i.e. +/-1 for a file, and +/-1 for a dir too, ignoring its contents
size_t m_overallfailures;
wxString m_messagetype;    // Is it a paste, is it an unredo...?
};

class PasteThreadStatuswriter : public wxEvtHandler
{
public:
PasteThreadStatuswriter();

void StartTimer() { m_timer.Start(100); }
void OnTimer(wxTimerEvent& event) { PrintCumulativeSize(); }

void SetTotalSize(wxULongLong total) { m_expectedsize = total; } // This is the total size of the files to be pasted
void OnProgress(unsigned int size){ m_cumsize += size; } // A paste thread has reported that some bytes have been processed
void PasteFinished() { m_timer.Stop(); }

protected:
void PrintCumulativeSize();  // Prints progress-to-date to the statusbar

wxTimer m_timer;
wxULongLong m_cumsize;
wxULongLong m_expectedsize;
};

class PasteThreadSuperBlock : public ThreadSuperBlock  // Manages a clipboardful of Pastes/Moves. When all threads have completed, passes the collected unredos to the manager
{
public:
virtual ~PasteThreadSuperBlock();

PasteThreadSuperBlock(unsigned int firstID) : ThreadSuperBlock(firstID), m_DestID(0) {}

wxArrayString& GetSuccessfullyPastedFilepaths() { return m_SuccessfullyPastedFilepaths; }
void StoreUnRedoPaste(UnRedo* entry, const wxString& filepath) { m_UnRedos.push_back( std::make_pair(entry, filepath) ); }
void StoreChangeMtimes(const wxString& filepath, time_t referencetime) { m_ChangeMtimes.push_back( std::make_pair(filepath, referencetime) ); }
void ReportProgress(unsigned int size) { m_StatusWriter.OnProgress(size); }
virtual void OnCompleted();      // Any action to be taken when all the blocks have completed
void DeleteAfter(const wxString& dirname) { if (!dirname.empty()) m_DeleteAfter.Add(dirname); } // In a Move, a dir will need 2b deleted in OnCompleted()
const wxString GetTrashdir() const { return m_trashdir; }
void SetTrashdir(const wxString& trashdir);
void StartThreadTimer() { m_StatusWriter.StartTimer(); }
void SetTotalExpectedSize(wxULongLong total) { m_StatusWriter.SetTotalSize(total); }

protected:

int m_DestID;                 // The ID of the destination pane; may need updating in OnCompleted()
wxString m_DestPath;          // Which path may need selecting in OnCompleted()
wxArrayString m_DeleteAfter;  // Which moved dirpaths may need deleting in OnCompleted()
wxString m_trashdir;          // Used to store/coordinate any trashdir used when files are being overwritten
wxArrayString m_SuccessfullyPastedFilepaths;
std::deque< std::pair<UnRedo*, const wxString> > m_UnRedos;
std::vector< std::pair<const wxString, time_t> > m_ChangeMtimes; // Stores original mod-times for pasted fpaths for feeding to FileData::ModifyFileTimes
PasteThreadStatuswriter m_StatusWriter;
};

class ThreadsManager /*: public wxEvtHandler*/
{
public:
ThreadSuperBlock* StartSuperblock(const wxString& type = wxT("")); // Create a superblock, in which future blocks will be stored. This helps multiple blocks report a combined success-count
ThreadBlock* AddBlock(size_t count, const wxString& type, int& threadID, ThreadSuperBlock* tsb = NULL); // Reserve a block of 'count' items, each of which will later contain a thread. Returns the index of the first of these
void AbortThreadSuperblock(ThreadSuperBlock* tsb); // Kill a tsb that's not going to be used. Called by UnRedoImplementer::StartItems when undoing a simple paste

bool IsActive() const;
bool PasteIsActive() const;
void CancelThread(int threadtype) const;

void OnThreadProgress(unsigned int ID, unsigned int size);
void OnThreadAborted(unsigned int ID) { OnThreadCompleted(ID); }
void OnThreadCompleted(unsigned int ID, const wxArrayString& array = wxArrayString(), const wxArrayString& successes = wxArrayString());

wxCriticalSection& GetPasteCriticalSection() { return m_PastingCritSection; }
void SetThreadPointer(unsigned int ID, wxThread* thread);

static size_t GetCPUCount();

static ThreadsManager& Get() { if (!ms_instance)	ms_instance = new ThreadsManager; return *ms_instance; }
static void Release() { delete ms_instance; ms_instance = NULL; }

protected:
int GetSuperBlockForID(unsigned int containedID) const; // Returns the superblock index or wxNOT_FOUND


unsigned int m_nextfree;              // The index of the first unused thread ID
std::vector<ThreadSuperBlock*> m_sblocks;

wxCriticalSection m_PastingCritSection;

private:
ThreadsManager() : m_nextfree(1) {}
~ThreadsManager();

static ThreadsManager* ms_instance;
};


#endif    //MISCH

