/////////////////////////////////////////////////////////////////////////////
// Name:       MyDragImage.h
// Purpose:    Pseudo D'n'D
// Part of:    4Pane
// Author:     David Hart
// Copyright:  (c) 2016 David Hart
// Licence:    GPL v3
/////////////////////////////////////////////////////////////////////////////
#ifndef MYDRAGIMAGEH
#define MYDRAGIMAGEH

#include "wx/wx.h"
#include "wx/dragimag.h"

#include "Externs.h"  

class MyDragImageSlideTimer;
class MyDragImageScrollTimer;

class MyDragImage  :   public wxDragImage
{
public:
MyDragImage();
~MyDragImage();

void SetCursor(enum DnDCursortype cursortype);
wxPoint pt;

void IncSlide();
void SetImageType(enum myDragResult type);                // Are we (currently) copying, softlinking etc? Sets the image appropriately
void DoLineScroll(wxMouseEvent& event);                   // Scrolling a line during DnD in response to scrollwheel
void DoPageScroll(wxMouseEvent& event);                   // Scrolling a page during DnD in response to Rt-click

                            // The next paragraph deals with scroll during DnD in response to mouse-positioning
void OnLeavingPane(wxWindow* lastwin);                    // Checks to see whether to start the scroll timer
void QueryScroll();                                       // Called by the scroll timer, scrolls if appropriate

protected:
void SetCorrectCursorForWindow(wxWindow* win) const;	  // When leaving a window, or cancelling DnD, revert to the correct cursor

int height; int width;                                    // Holds the size of WindowToScroll
int exitlocation;                                         // The y-location where we left the WindowToScroll
size_t scrollno;                                          // Remembers the no of previous scrolls, so that acceleration can be intelligent
wxWindow* m_PreviousWin;
wxWindow* WindowToScroll;
MyDragImageScrollTimer* scrolltimer;

wxIcon drag_icon;
wxIcon* copier_icon;
wxIcon hardlink_icon;
wxIcon softlink_icon;

MyDragImageSlideTimer* slidetimer;
enum myDragResult dragtype;                               // Drag, copy, hardlink, softlink
size_t slide;                                             // For animated images, this holds which slide is next to be shown

wxCursor m_standardcursor;
wxCursor m_textctlcursor;
enum DnDCursortype currentcursor;
};

class MyDragImageSlideTimer  :   public wxTimer  // Used by MyDragImage.  Notify() incs the slide-show counter
{
public:
void Init(MyDragImage* mdi){ parent = mdi; }
void Notify(){ parent->IncSlide(); }

protected:
MyDragImage* parent;
};

class MyDragImageScrollTimer  :   public wxTimer  // Used by MyDragImage.  Notify() scrolls a pane's tree
{
public:
void Init(MyDragImage* mdi){ parent = mdi; }
void Notify(){ parent->QueryScroll(); }

protected:
MyDragImage* parent;
};

#endif
    // MYDRAGIMAGEH
