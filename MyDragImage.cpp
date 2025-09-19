/////////////////////////////////////////////////////////////////////////////
// Name:       MyDragImage.cpp
// Purpose:    Pseudo D'n'D
// Part of:    4Pane
// Author:     David Hart
// Copyright:  (c) 2019 David Hart
// Licence:    GPL v3
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/log.h"
#include "wx/app.h"
#include "wx/frame.h" 
#include "wx/scrolwin.h"
#include "wx/menu.h"
#include "wx/dirctrl.h"

#if defined __WXGTK__
  #include <gtk/gtk.h>
#endif

#include "MyDragImage.h"
#include "MyTreeCtrl.h"
#include "MyDirs.h"
#include "MyGenericDirCtrl.h"
#include "MyFrame.h"

const size_t NO_OF_COPIER_IMAGES = 44;

MyDragImage::MyDragImage() : m_PreviousWin(NULL), WindowToScroll(NULL)
{
slide = 0;
dragtype = myDragMove;

m_standardcursor = MyFrame::mainframe->GetStandardCursor();
if (!m_standardcursor.IsOk()) m_standardcursor = wxCursor(*wxSTANDARD_CURSOR); // In the unlikely event of it being invalid...
m_textctlcursor  = MyFrame::mainframe->GetTextCrlCursor();
if (!m_textctlcursor.IsOk()) m_textctlcursor = wxCursor(wxCURSOR_IBEAM);

wxImage di_img = wxGetApp().CorrectForDarkTheme(BITMAPSDIR+wxT("dragicon.png"));
drag_icon.CopyFromBitmap(wxBitmap(di_img));

hardlink_icon.LoadFile(BITMAPSDIR+wxT("hardlink.png"), wxBITMAP_TYPE_PNG);
softlink_icon.LoadFile(BITMAPSDIR+wxT("softlink.png"), wxBITMAP_TYPE_PNG);
copier_icon = new wxIcon[NO_OF_COPIER_IMAGES];

for (uint n = 0; n < NO_OF_COPIER_IMAGES; ++n)
  { wxString iconfile; iconfile.Printf(wxT("%sphotocopier_%u.png"), BITMAPSDIR.c_str(), n);
    copier_icon[n].LoadFile(iconfile, wxBITMAP_TYPE_PNG);
  }

m_icon =  drag_icon; currentcursor = dndStdCursor;

slidetimer = new MyDragImageSlideTimer; slidetimer->Init(this);     // Set up the timer that incs slide-show
scrolltimer = new MyDragImageScrollTimer; scrolltimer->Init(this);  // Set up the timer that scrolls a pane's tree
}

MyDragImage::~MyDragImage()
{
slidetimer->Stop(); delete slidetimer;
scrolltimer->Stop(); delete scrolltimer;
delete[] copier_icon;
}

void MyDragImage::IncSlide()    // For animation, inc and redraw the image
{
m_icon = copier_icon[slide];                              // Change the icon
RedrawImage(pt - m_offset, pt - m_offset, true, true);    // We must redraw here, otherwise there's no update when the mouse isn't moving
if (++slide >= NO_OF_COPIER_IMAGES) slide = 0;            // Inc ready for next time
}

void MyDragImage::SetImageType(enum myDragResult type)    // Are we (currently) copying, softlinking etc? Sets the image appropriately
{
if (type == dragtype)  return;                            // No change needed

slidetimer->Stop();                                       // Stop the timer, in case we're animating
slide = 0;                                                //   & reset the slide number
if (m_isShown) Hide();                                    // The 'if' is in case entry is pre wxGenericDragImage::BeginDrag()
dragtype = type;                                          // Set the new drag type

switch (dragtype)
 {
  case myDragMove:      m_icon = drag_icon; break;
  case myDragHardLink:  m_icon = hardlink_icon; break;
  case myDragSoftLink:  m_icon = softlink_icon; break;
  case myDragCopy:      m_icon = copier_icon[slide++]; slidetimer->Start(150, wxTIMER_CONTINUOUS); break;  
   default:             return;
 }
 
if (m_windowDC) Show();                                   // The 'if' is in case entry is pre wxGenericDragImage::BeginDrag()
}

void MyDragImage::DoPageScroll(wxMouseEvent& event)
{
wxPoint pt;
wxWindow *win = wxFindWindowAtPointer(pt);          // Find window under mouse
if (win == NULL) { return; }

if (win->GetName() != wxT("MyTreeCtrl"))   return;  // We only want to scroll treectrls

pt = win->ScreenToClient(pt);
int ht = win->GetClientSize().GetHeight();
bool goingdown = pt.y > ht / 2;                     // If the click is in the top half of the pane, do a page-up;  otherwise page-down

Hide();                                             // We must Hide while scrolling, otherwise the tree moves relative to the image, which leaves ghost images

wxScrollWinEvent scrollevent(goingdown ? wxEVT_SCROLLWIN_PAGEDOWN : wxEVT_SCROLLWIN_PAGEUP, win->GetId(), 0);
win->GetEventHandler()->ProcessEvent(scrollevent);

win->Refresh(); win->Update();
Show();
}

void MyDragImage::DoLineScroll(wxMouseEvent& event)
{
wxPoint pt;
wxWindow *win = wxFindWindowAtPointer(pt);          // Find window under mouse
if (win == NULL)  return; 

if (win->GetName() != wxT("MyTreeCtrl"))   return;  // We only want to scroll treectrls

Hide();       // We must Hide while scrolling, otherwise the tree moves relative to the image, which leaves ghost images

bool goingdown = event.GetWheelRotation() < 0;      // Which way are we scrolling
int lines = event.GetLinesPerAction();              // Unfortunately, in wxGTK-2.4.2 this is always zero, so:
if (!lines) lines = LINES_PER_MOUSEWHEEL_SCROLL;

for (int n=0; n < lines; ++n)                       // Send a scroll event for every line to be scrolled
  { wxScrollWinEvent scrollevent(goingdown ? wxEVT_SCROLLWIN_LINEDOWN : wxEVT_SCROLLWIN_LINEUP, win->GetId(), 0);
    win->GetEventHandler()->ProcessEvent(scrollevent);
  }

win->Refresh(); win->Update();
Show();
}

void MyDragImage::OnLeavingPane(wxWindow* lastwin)  // Called when leaving a pane, in case we're lurking at the edge in order to scroll
{
static size_t delay = 500;
if (lastwin == NULL) return;

WindowToScroll = lastwin; scrollno = 0;

wxPoint mousePt = WindowToScroll->ScreenToClient(wxGetMousePosition());  // Find where the mouse is in relation to the (exited) pane
WindowToScroll->GetSize(&width,  &height);          // Store window dimensions

if (mousePt.x < 0  ||  mousePt.x > width)           // If we are outside the width of the pane, to the left or the right, abort
  { WindowToScroll = NULL; return; }

if (mousePt.y <= 0)                                 // We're above the pane
  { scrolltimer->Stop();                            // In case we're whipping from 1 pane to another
    scrolltimer->Start(delay, wxTIMER_ONE_SHOT);    // Start the timer, with an initial longish pause
    
    exitlocation = 0;                               // Pretend the mouse is exactly on the top edge
  }
 else if (mousePt.y >= height)                      // We're below
  { scrolltimer->Stop();
    scrolltimer->Start(delay, wxTIMER_ONE_SHOT);
    
    exitlocation = height;                          // Pretend the mouse is exactly on the bottom edge
  }

SetCursor(dndScrollCursor);
}

void MyDragImage::QueryScroll()  // Called by the scroll timer. Scrolls if appropriate, faster each time, then redoes the timer
{
static size_t experimentalerror = 20;               // This allows a bit of leeway either side of the pane's edge

if (WindowToScroll == NULL) return;

wxPoint mousePt = WindowToScroll->ScreenToClient(wxGetMousePosition());

if (mousePt.x < 0  ||  mousePt.x > width)           // If we're now outside the width of the pane, to the left or the right, abort
  { WindowToScroll = NULL; SetCursor(dndStdCursor); return; }
  
                // See if we're still on the fringe of the pane, within experimental error
int top = exitlocation - experimentalerror;
int bottom = exitlocation + experimentalerror;
if (mousePt.y < top  ||  mousePt.y > bottom)        // If we're now outside the 'keep going' zone, abort
  { WindowToScroll = NULL; SetCursor(dndStdCursor); return; }

Hide();

if (++scrollno < 30)                                // If <30, scroll n lines at a time, inc.ing n every 3rd beat
  { wxScrollWinEvent scrollevent(exitlocation ? wxEVT_SCROLLWIN_LINEDOWN : wxEVT_SCROLLWIN_LINEUP, WindowToScroll->GetId(), 0);
    for (size_t n=0; n < scrollno/3; ++n)
      WindowToScroll->GetEventHandler()->ProcessEvent(scrollevent);
  }
 else                                               // If already >=30, scroll a page at a time
  { wxScrollWinEvent scrollevent(exitlocation ? wxEVT_SCROLLWIN_PAGEDOWN : wxEVT_SCROLLWIN_PAGEUP, WindowToScroll->GetId(), 0);
    WindowToScroll->GetEventHandler()->ProcessEvent(scrollevent);
  }

WindowToScroll->Refresh(); WindowToScroll->Update();
Show();

if (scrolltimer != NULL) scrolltimer->Start(200, wxTIMER_ONE_SHOT);  // Re-set the timer for another go. Check for null because we've just Yielded, and might have been destroyed
}

void MyDragImage::SetCorrectCursorForWindow(wxWindow* win) const
{
wxCHECK_RET(win, wxT("Invalid window"));

if (dynamic_cast<wxTextCtrl*>(win))
      win->SetCursor(m_textctlcursor);
 else win->SetCursor(m_standardcursor);
}

#if wxVERSION_NUMBER <= 3000 
void MyDragImage::SetCursor(enum DnDCursortype cursortype)  // Sets either Standard, Selected or Scroll cursor
{
if (currentcursor == cursortype) return;

bool needsrecapture = false;
if (MyFrame::mainframe->HasCapture())
  { MyFrame::mainframe->ReleaseMouse(); // If we don't release the mouse, the change doesn't take effect
    needsrecapture = true;
  }

switch(cursortype)
  { case dndSelectedCursor: MyFrame::mainframe->SetCursor(MyFrame::mainframe->DnDSelectedCursor); currentcursor = dndSelectedCursor; break;
    case dndUnselect:       if (currentcursor != dndSelectedCursor)  break;  // The idea is to ->std if selected, otherwise ignore
    case dndStdCursor:      MyFrame::mainframe->SetCursor(MyFrame::mainframe->DnDStdCursor); currentcursor = dndStdCursor; break;
    case dndScrollCursor:   MyFrame::mainframe->SetCursor(*wxCROSS_CURSOR); currentcursor = dndScrollCursor; break;
     default: ;
  }

if (needsrecapture)
  MyFrame::mainframe->CaptureMouse();
}

#else // !wxVERSION_NUMBER <= 3000
// wxGTK's cursor management changed considerably soon after the 3.0.0 release. Now it's the child window, not the frame, that's relevant 
void MyDragImage::SetCursor(enum DnDCursortype cursortype)  // Sets either Standard, Selected or Scroll cursor
{
if (m_PreviousWin && cursortype == dndOriginalCursor)
 { SetCorrectCursorForWindow(m_PreviousWin); return; } // Ended DnD, so revert the current window's original cursor

wxPoint pt;
wxWindow* currentwin = wxFindWindowAtPointer(pt);
// Don't 'optimise' here by returning if currentwin == m_PreviousWin and their cursor enum types are the same; the actual cursor may have changed e.g. over a pane divider

if (currentwin && m_PreviousWin && currentwin != m_PreviousWin)
  SetCorrectCursorForWindow(m_PreviousWin); // We're leaving a window, so revert to its original cursor; if we don't here, no one else will

#if GTK_CHECK_VERSION(3,0,0) && !GTK_CHECK_VERSION(3,10,0)
  wxString prevwinname; if (m_PreviousWin) prevwinname = m_PreviousWin->GetName(); // See below
#endif

m_PreviousWin = currentwin;
if (!currentwin) currentwin = MyFrame::mainframe;

wxWindow* capturewin = wxWindow::GetCapture();

bool needsrecapture = false, needsrefresh = false;
if (capturewin)
  { capturewin->ReleaseMouse(); // If we don't release the mouse, the change doesn't take effect
    needsrecapture = true;
  }
switch(cursortype)
  { case dndSelectedCursor: currentwin->SetCursor(MyFrame::mainframe->DnDSelectedCursor); currentcursor = dndSelectedCursor; needsrefresh = true; break;
    case dndUnselect:       if (currentcursor != dndSelectedCursor)  break;  // The idea is to ->std if selected, otherwise ignore
    case dndStdCursor:      currentwin->SetCursor(MyFrame::mainframe->DnDStdCursor); currentcursor = dndStdCursor; needsrefresh = true; break;
    case dndScrollCursor:   currentwin->SetCursor(*wxCROSS_CURSOR); currentcursor = dndScrollCursor; needsrefresh = true; break;
     default: break; // We dealt with dndOriginalCursor earlier
  }

#if GTK_CHECK_VERSION(3,0,0) && !GTK_CHECK_VERSION(3,10,0)
// Debian's gtk3 manages to crash inside cairo_fill when we leave an editor/device button. So don't refresh then, even though it means editors never get dndSelectedCursor
  if (needsrecapture && needsrefresh 
      && (currentwin->GetName() == "MyTreeCtrl" || currentwin->GetName() == "MyFrame" || currentwin->GetName() == "TerminalEm")
      && (prevwinname == "MyTreeCtrl" || prevwinname == "MyFrame" || prevwinname == "TerminalEm"))
#else
  if (needsrefresh)
#endif
  { currentwin->Refresh(); wxYieldIfNeeded(); }

if (needsrecapture)
  capturewin->CaptureMouse();
}
#endif // !wxVERSION_NUMBER <= 3000
