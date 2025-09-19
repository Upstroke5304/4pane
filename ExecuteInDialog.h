/////////////////////////////////////////////////////////////////////////////
// Name:       ExecuteInDialog.h
// Purpose:    Displays exec output in a dialog
// Part of:    4Pane
// Author:     David Hart
// Copyright:  (c) 2016 David Hart
// Licence:    wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef EXECUTEINDIALOGH
#define EXECUTEINDIALOGH

#include "wx/wx.h"
#include "wx/process.h"
#include "wx/txtstrm.h"

class CommandDisplay;

class DisplayProcess : public wxProcess         // Executes a command, displaying output in a textctrl.  Adapted from the exec sample
{                                               // Very similar to MyPipedProcess in Tools.cpp, but enough differences to make derivation difficult
public:
DisplayProcess(CommandDisplay* display);

bool HasInput();
void OnKillProcess();
void SetPid(long pid) { m_pid = pid; }

protected:
void OnTerminate(int pid, int status);
bool AlreadyCancelled() const { return m_WasCancelled; }

long m_pid;                                     // Stores the process's pid, in case we want to kill it
bool m_WasCancelled;
CommandDisplay* m_parent;
wxTextCtrl* m_text;
};

class CommandDisplay : public wxDialog  // A dialog with a textctrl that displays output from a wxExecuted command
{
enum timertype { wakeupidle=5000, endmodal };
public:
void Init(wxString& command);                   // Do the ctor work here, as otherwise wouldn't be done under xrc. Also calls RunCommand()

void AddInput(wxString input);                  // Displays input received from the running Process
void OnProcessTerminated(DisplayProcess *process);
void OnProcessKilled(DisplayProcess *process);  // Similar to OnProcessTerminated but Detaches rather than deletes, following a Cancel

int exitstatus;
wxTextCtrl* text;
protected:
void RunCommand(wxString& command);         // Do the Process/Execute things to run the command
void AddAsyncProcess(DisplayProcess *process);
void OnCancel(wxCommandEvent& event);
void SetupClose();                          // Re-labels Cancel button as Close, & starts the endmodal timer if desired

void OnProcessTimer(wxTimerEvent& event);   // Called by timer to generate wakeupidle events
void OnEndmodalTimer(wxTimerEvent& event);  // Called when the one-shot shutdown timer fires
void OnIdle(wxIdleEvent& event);
void OnAutocloseCheckBox(wxCommandEvent& event);

DisplayProcess* m_running;
wxTimer m_timerIdleWakeUp;                  // The idle-event-wake-up timer
wxTimer shutdowntimer;                      // The shutdown timer
private:
   DECLARE_EVENT_TABLE()
};


#endif
    //EXECUTEINDIALOGH

