/////////////////////////////////////////////////////////////////////////////
// Name:       ExecuteInDialog.cpp
// Purpose:    Displays exec output in a dialog
// Part of:    4Pane
// Author:     David Hart
// Copyright:  (c) 2019 David Hart
// Licence:    GPL v3
/////////////////////////////////////////////////////////////////////////////
#include "wx/wxprec.h" 

#include "wx/xrc/xmlres.h"

#include "Externs.h"
#include "ExecuteInDialog.h"

DisplayProcess::DisplayProcess(CommandDisplay* display)   :  wxProcess(wxPROCESS_REDIRECT), m_WasCancelled(false), m_parent(display)
{
m_text = m_parent->text;
}

bool DisplayProcess::HasInput()    // The following methods are adapted from the exec sample.  This one manages the stream redirection
{
char c;

bool hasInput = false;
    // The original used wxTextInputStream to read a line at a time.  Fine, except when there was no \n, whereupon the thing would hang
    // Instead, read the stream (which may occasionally contain non-ascii bytes e.g. from g++) into a memorybuffer, then to a wxString
while (IsInputAvailable())                                  // If there's std input
  { wxMemoryBuffer buf;
    do
      { c = GetInputStream()->GetC();                       // Get a char from the input
        if (GetInputStream()->Eof()) break;                 // Check we've not just overrun
        
        buf.AppendByte(c);
        if (c==wxT('\n')) break;                            // If \n, break to print the line
      }
      while (IsInputAvailable());                           // Unless \n, loop to get another char
    wxString line((const char*)buf.GetData(), wxConvUTF8, buf.GetDataLen()); // Convert the line to utf8

    m_parent->AddInput(line);                               // Either there's a full line in 'line', or we've run out of input. Either way, print it

    hasInput = true;
  }

while (IsErrorAvailable())
  { wxMemoryBuffer buf;
    do
      { c = GetErrorStream()->GetC();
        if (GetErrorStream()->Eof()) break;
        
        buf.AppendByte(c);
        if (c==wxT('\n')) break;
      }
      while (IsErrorAvailable());
    wxString line((const char*)buf.GetData(), wxConvUTF8, buf.GetDataLen());

    m_parent->AddInput(line);

    hasInput = true;
  }

return hasInput;
}

void DisplayProcess::OnTerminate(int pid, int status)  // When the subprocess has finished, show the rest of the output, then an exit message
{
while (HasInput());

wxString exitmessage;
if (!status) exitmessage = _("Success\n");
 else
   if (!AlreadyCancelled()) exitmessage = _("Process failed\n");

*m_text << exitmessage;

m_parent->exitstatus = status;                                // Tell the dlg what the exit status was, in case caller is interested
m_parent->OnProcessTerminated(this);
}

void DisplayProcess::OnKillProcess()
{
if (!m_pid) return;

  // Take the pid we stored earlier & kill it.  SIGTERM seems to work now we use wxEXEC_MAKE_GROUP_LEADER and wxKILL_CHILDREN but first send a SIGHUP because of 
  // the stream redirection (else if the child is saying "Are you sure?" and waiting for input, SIGTERM fails in practice -> zombie processes and a non-deleted wxProcess)
Kill(m_pid, wxSIGHUP, wxKILL_CHILDREN);

wxKillError result = Kill(m_pid, wxSIGTERM, wxKILL_CHILDREN);
if (result == wxKILL_OK)
  { (*m_text) << _("Process successfully aborted\n");
    m_parent->exitstatus = 1;                                 // Tell the dlg we exited abnormally, in case caller is interested
    m_parent->OnProcessKilled(this);                          // Go thru the termination process, but detaching rather than deleting
  }
 else
  { (*m_text) << _("SIGTERM failed\nTrying SIGKILL\n");       // If we could be bothered, result holds the index in an enum that'd tell us why it failed
    result = Kill(m_pid, wxSIGKILL, wxKILL_CHILDREN);         // Fight dirty
    if (result == wxKILL_OK)
      { (*m_text) << _("Process successfully killed\n");
        m_parent->exitstatus = 1;
        m_parent->OnProcessKilled(this);
      }
     else
      { (*m_text) << _("Sorry, Cancel failed\n");
        m_parent->OnProcessKilled(this);                      // We need to do this anyway, otherwise the dialog can't be closed
      }
  }

m_WasCancelled = true;
}

//-----------------------------------------------------------------------------------------------------------------------
void CommandDisplay::Init(wxString& command)
{
m_running = NULL;                                           // Null to show there isn't currently a running process
m_timerIdleWakeUp.SetOwner(this, wakeupidle);               // Initialise the process timer
shutdowntimer.SetOwner(this, endmodal);                     //   & the shutdown timer
text = (wxTextCtrl*)FindWindow(wxT("text"));

RunCommand(command);                                        // Actually run the process. This has to happen before ShowModal()
}

void CommandDisplay::RunCommand(wxString& command) // Do the Process/Execute things to run the command
{
if (command.IsEmpty()) return;

DisplayProcess* process = new DisplayProcess(this);         // Make a new Process, the sort with built-in redirection
long pid = wxExecute(command, wxEXEC_ASYNC | wxEXEC_MAKE_GROUP_LEADER, process); // Try & run the command.  pid of 0 means failed

if (!pid)
  { wxLogError(_("Execution of '%s' failed."), command.c_str());
    delete process;
  }
 else
  { process->SetPid(pid);                                   // Store the pid in case Cancel is pressed
    AddAsyncProcess(process);                               // Start the timer to poll for output
  }
}

void CommandDisplay::AddInput(wxString input)    // Displays input received from the running Process
{
if (input.IsEmpty()) return;

text->AppendText(input);                                    // Add string to textctrl
}

void CommandDisplay::OnCancel(wxCommandEvent& event)
{
if (m_running)    m_running->OnKillProcess();               // If there's a running process, murder it
 else                                                       // If not, the user just pressed Close
  { shutdowntimer.Stop();                                   // Stop the shutdowntimer, in case it was running (to avoid 2 coinciding EndModals)
     wxDialog::EndModal(!exitstatus);
  }
}

void CommandDisplay::AddAsyncProcess(DisplayProcess *process)  // Starts up the timer that generates wake-ups for OnIdle
{ 
if (m_running==NULL)                                        // If it's not null, there's already a running process
  { m_timerIdleWakeUp.Start(100);                           // Tell the timer to create idle events every 1/10th sec
    m_running = process;                                    // Store the process
  }
}

void CommandDisplay::OnProcessTerminated(DisplayProcess *process)  // Stops everything, deletes process
{
m_timerIdleWakeUp.Stop();                                   // Stop the process timer

delete process;
m_running = NULL;

SetupClose();                                               // Relabels Cancel button as Close, & starts the endmodal timer if desired
}

void CommandDisplay::OnProcessKilled(DisplayProcess *process)  // Similar to OnProcessTerminated but Detaches rather than deletes, following a Cancel
{
m_timerIdleWakeUp.Stop();

process->Detach();
m_running = NULL;

SetupClose();
}

void CommandDisplay::SetupClose()  // Relabels Cancel button as Close, & starts the endmodal timer if desired
{
if (((wxCheckBox*)FindWindow(wxT("Autoclose")))->IsChecked())
  shutdowntimer.Start(exitstatus ? AUTOCLOSE_FAILTIMEOUT : AUTOCLOSE_TIMEOUT, wxTIMER_ONE_SHOT);

((wxButton*)FindWindow(wxT("wxID_CANCEL")))->SetLabel(_("Close"));
}

void CommandDisplay::OnProcessTimer(wxTimerEvent& WXUNUSED(event))  // When the timer fires, it wakes up the OnIdle method
{
wxWakeUpIdle();
}

void CommandDisplay::OnEndmodalTimer(wxTimerEvent& WXUNUSED(event))  // Called when the one-shot shutdown timer fires
{
wxDialog::EndModal(!exitstatus);
}

void CommandDisplay::OnIdle(wxIdleEvent& event)  // Made to happen by the process timer firing; runs HasInput()
{
if (m_running==NULL) return;                                // Ensure there IS a process

m_running->HasInput();                                      // Check for input from the process
}

void CommandDisplay::OnAutocloseCheckBox(wxCommandEvent& event)
{
shutdowntimer.Stop();                                       // Either way, start with stop

if (event.IsChecked()                                       // If the autoclose checkbox was unchecked, & has just been checked
    && m_running==NULL)                                     // If the process has finished/aborted, start the shutdown timer
  shutdowntimer.Start(exitstatus ? AUTOCLOSE_FAILTIMEOUT : AUTOCLOSE_TIMEOUT, wxTIMER_ONE_SHOT);
}

BEGIN_EVENT_TABLE(CommandDisplay, wxDialog)
  EVT_CHECKBOX(XRCID("Autoclose"), CommandDisplay::OnAutocloseCheckBox)
  EVT_IDLE(CommandDisplay::OnIdle)
  EVT_BUTTON(wxID_CANCEL, CommandDisplay::OnCancel)
  EVT_TIMER(wakeupidle, CommandDisplay::OnProcessTimer)
  EVT_TIMER(endmodal, CommandDisplay::OnEndmodalTimer)
END_EVENT_TABLE()

