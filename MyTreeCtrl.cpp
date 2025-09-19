/////////////////////////////////////////////////////////////////////////////
// Name:       MyTreeCtrl.cpp
// Purpose:    Tree and Treelistctrl.
// Derived:    The listctrl parts adapted from wxTreeListCtrl (c) Alberto Griggio, Otto Wyss
// See:        http://wxcode.sourceforge.net/components/treelistctrl/
// Part of:    4Pane
// Author:     David Hart
// Copyright:  (c) 2020 David Hart
// Licence:    GPL v3
/////////////////////////////////////////////////////////////////////////////

#include "wx/log.h"
#include "wx/dirctrl.h"
#include "wx/dragimag.h"
#include "wx/renderer.h"
#include "wx/filename.h" 


#ifdef __WXGTK__
  #include <gtk/gtk.h>
#endif

#include "MyGenericDirCtrl.h"
#include "Dup.h"
#include "MyFiles.h"
#include "MyDirs.h"
#include "MyFrame.h"
#include "Filetypes.h"
#include "Redo.h"
#include "Devices.h"
#include "MyTreeCtrl.h"
#include "Accelerators.h"

#include "bitmaps/include/columnheaderdown.xpm"  // // Icons for the fileview columns, selected & selected-reverse-sort 
#include "bitmaps/include/columnheaderup.xpm"

class WXDLLEXPORT wxGenericTreeItem;

WX_DEFINE_EXPORTED_ARRAY(wxGenericTreeItem *, wxArrayGenericTreeItems);

enum { ColDownIcon = 0, ColUpIcon };          // These are the indices of the Down/Up column icons within HeaderimageList

class WXDLLEXPORT wxGenericTreeItem
{
public:
    // ctors & dtor
    wxGenericTreeItem() { m_data = NULL; }
    wxGenericTreeItem(wxGenericTreeItem *parent,
                       const wxString& text,
                       int image,
                       int selImage,
                       wxTreeItemData *data);

    ~wxGenericTreeItem();

    // trivial accessors
    wxArrayGenericTreeItems& GetChildren() { return m_children; }

    const wxString& GetText() const { return m_text; }
    int GetImage(wxTreeItemIcon which = wxTreeItemIcon_Normal) const
        { return m_images[which]; }
    wxTreeItemData *GetData() const { return m_data; }

    // returns the current image for the item (depending on its
    // selected/expanded/whatever state)
    int GetCurrentImage() const;

    void SetText(const wxString &text);
    void SetImage(int image, wxTreeItemIcon which) { m_images[which] = image; }
    void SetData(wxTreeItemData *data) { m_data = data; }

    void SetHasPlus(bool has = true) { m_hasPlus = has; }

    void SetBold(bool bold) { m_isBold = bold; }

    int GetX() const { return m_x; }
    int GetY() const { return m_y; }

    void SetX(int x) { m_x = x; }
    void SetY(int y) { m_y = y; }

    int  GetHeight() const { return m_height; }
    int  GetWidth()  const { return m_width; }

    void SetHeight(int h) { m_height = h; }
    void SetWidth(int w) { m_width = w; }

    wxGenericTreeItem *GetParent() const { return m_parent; }

    // operations
    // deletes all children notifying the treectrl about it if !NULL pointer given
    void DeleteChildren(wxGenericTreeCtrl *tree = NULL);

    // get count of all children (and grand children if 'recursively')
    size_t GetChildrenCount(bool recursively = true) const;

    void Insert(wxGenericTreeItem *child, size_t index)
    { m_children.Insert(child, index); }

    void GetSize(int &x, int &y, const wxGenericTreeCtrl*);

    // return the item at given position (or NULL if no item), onButton is
    // true if the point belongs to the item's button, otherwise it lies
    // on the button's label
    wxGenericTreeItem *HitTest(const wxPoint& point,
                                const wxGenericTreeCtrl *,
                                int &flags,
                                int level);

    void Expand() { m_isCollapsed = false; }
    void Collapse() { m_isCollapsed = true; }

    void SetHilight(bool set = true) { m_hasHilight = set; }

    // status inquiries
    bool HasChildren() const { return !m_children.IsEmpty(); }
    bool IsSelected()  const { return m_hasHilight != 0; }
    bool IsExpanded()  const { return !m_isCollapsed; }
    bool HasPlus()     const { return m_hasPlus || HasChildren(); }
    bool IsBold()      const { return m_isBold != 0; }

    // attributes
    // get them - may be NULL
    wxTreeItemAttr *GetAttributes() const { return m_attr; }
    // get them ensuring that the pointer is not NULL
    wxTreeItemAttr& Attr()
    {
        if (!m_attr)
        {
            m_attr = new wxTreeItemAttr;
            m_ownsAttr = true;
        }
        return *m_attr;
    }
        // set them
    void SetAttributes(wxTreeItemAttr *attr)
    {
        if (m_ownsAttr) delete m_attr;
        m_attr = attr;
        m_ownsAttr = false;
    }
        // set them and delete when done
    void AssignAttributes(wxTreeItemAttr *attr)
    {
        SetAttributes(attr);
        m_ownsAttr = true;
    }

private:
    // since there can be very many of these, we save size by chosing
    // the smallest representation for the elements and by ordering
    // the members to avoid padding.
    wxString            m_text;         // label to be rendered for item

    wxTreeItemData     *m_data;         // user-provided data
    int                 m_state; 
    int                 m_widthText;
    int                 m_heightText;

    wxArrayGenericTreeItems m_children; // list of children
    wxGenericTreeItem  *m_parent;       // parent of this item

    wxTreeItemAttr     *m_attr;         // attributes???

    // tree ctrl images for the normal, selected, expanded and
    // expanded+selected states
    int                 m_images[wxTreeItemIcon_Max];
    wxCoord             m_x;            // (virtual) offset from top
    wxCoord             m_y;            // (virtual) offset from left
    int                 m_width;        // width of this item
    int                 m_height;       // height of this item
    // use bitfields to save size
    int                 m_isCollapsed :1;
    int                 m_hasHilight  :1; // same as focused
    int                 m_hasPlus     :1; // used for item which doesn't have
                                          // children but has a [+] button
    int                 m_isBold      :1; // render the label in bold font
    int                 m_ownsAttr    :1; // delete attribute when done
};


//-----------------------------------------------------------------------------
//  Originally wxTreeListHeaderWindow  from wxTreeListCtrl 
//-----------------------------------------------------------------------------

#include <wx/arrimpl.cpp>
WX_DEFINE_OBJARRAY(wxArrayTreeListColumnInfo);

IMPLEMENT_DYNAMIC_CLASS(TreeListHeaderWindow,wxWindow);

TreeListHeaderWindow::TreeListHeaderWindow()
{
    Init();

    m_owner = (MyTreeCtrl*) NULL;
    m_resizeCursor = (wxCursor*) NULL;
}

TreeListHeaderWindow::TreeListHeaderWindow(wxWindow *win,
            wxWindowID id,
            MyTreeCtrl* owner,
            const wxPoint& pos,
            const wxSize& size,
            long style,
            const wxString &name)
    : wxWindow(win, id, pos, size, style, name)
{
    Init();

    m_owner = owner;
    m_resizeCursor = new wxCursor(wxCURSOR_SIZEWE);

p_BackgroundColour = wxGetApp().GetBackgroundColourUnSelected(); // //
}

TreeListHeaderWindow::~TreeListHeaderWindow()
{
    delete m_resizeCursor;
}

void TreeListHeaderWindow::Init()
{
    m_currentCursor = (wxCursor*) NULL;
    m_isDragging = false;
    m_dirty = false;
    
m_total_col_width = 0;                      // // (Originally this wasn't given a default)
selectedcolumn = filename;                  // // The next 2 should be set by config but let's give them sensible defaults anyway
reversesort = false;
}



// shift the DC origin to match the position of the main window horz
// scrollbar: this allows us to always use logical coords
void TreeListHeaderWindow::AdjustDC(wxDC& dc)
{
    int xpix;
    m_owner->GetScrollPixelsPerUnit(&xpix, NULL);

    int x;
    m_owner->GetViewStart(&x, NULL);

    // account for the horz scrollbar offset
    dc.SetDeviceOrigin(-x * xpix, 0);
}

void TreeListHeaderWindow::OnPaint(wxPaintEvent &WXUNUSED(event))
{
    static const int HEADER_OFFSET_X = 1, HEADER_OFFSET_Y = 1;
#ifdef __WXGTK__
    wxClientDC dc(this);
#else
    wxPaintDC dc(this);
#endif

    PrepareDC(dc);
    AdjustDC(dc);

    // //dc.BeginDrawing();

    dc.SetFont(GetFont());

    // width and height of the entire header window
    int w, h;
    GetClientSize(&w, &h);
    m_owner->CalcUnscrolledPosition(w, 0, &w, NULL);

    dc.SetBackgroundMode(wxTRANSPARENT);

    // do *not* use the listctrl colour for headers - one day we will have a
    // function to set it separately
    //dc.SetTextForeground(*wxBLACK);
    dc.SetTextForeground(wxSystemSettings::
                            GetColour(wxSYS_COLOUR_WINDOWTEXT));      // //
  SetBackgroundColour(*p_BackgroundColour); // //
    int x = HEADER_OFFSET_X;

    int numColumns = GetColumnCount();
    for (int i = 0; i < numColumns && x < w; i++)
    {
  wxTreeListColumnInfo& column = GetColumn(i);
        int wCol = column.GetWidth(); 

        // the width of the rect to draw: make it smaller to fit entirely
        // inside the column rect
        int cw = wCol - 2;

        dc.SetPen(*wxWHITE_PEN);

  wxRendererNative& renderer = wxRendererNative::Get();
        renderer.DrawHeaderButton(this, dc, wxRect(x, HEADER_OFFSET_Y, cw, h-2));

        // if we have an image, draw it on the right of the label  // // except that I want it in the middle --- see below 
        int image = column.GetImage(); //item.m_image;
  int ix = -2, iy = 0;
  wxImageList* imageList = m_owner->HeaderimageList;    // // m_owner->GetImageList();    I've had to make my own list, as my bitmaps are 16*10, not 16*16
        if(image != -1) {
            if(imageList) {
                imageList->GetSize(image, ix, iy);
      }
            //else: ignore the column image
        }

        // extra margins around the text label
  static const int EXTRA_WIDTH = 3;
  static const int EXTRA_HEIGHT = 4;

  int text_width = 0;
  int text_x = x;
  // // int image_offset = cw - ix - 1;
  int image_offset = (cw - ix)/2;                        // // The way to get a central image
  
  switch(column.GetAlignment()) {
  case wxTL_ALIGN_LEFT:
      text_x += EXTRA_WIDTH;
      cw -= ix + 2;
      break;
  case wxTL_ALIGN_RIGHT:
      dc.GetTextExtent(column.GetText(), &text_width, NULL);
      text_x += cw - text_width - EXTRA_WIDTH;
      image_offset = 0;
      break;
  case wxTL_ALIGN_CENTER:
      dc.GetTextExtent(column.GetText(), &text_width, NULL);
      text_x += (cw - text_width)/2 + ix + 2;
      image_offset = (cw - text_width - ix - 2)/2;
      break;
  }

  // draw the image
  if(image != -1 && imageList) {
      imageList->Draw(image, dc, x + image_offset/*cw - ix - 1*/,
          HEADER_OFFSET_Y + (h - 4 - iy)/2,
          wxIMAGELIST_DRAW_TRANSPARENT);
  }
  
        // draw the text clipping it so that it doesn't overwrite the column
        // boundary
        wxDCClipper clipper(dc, x, HEADER_OFFSET_Y, cw, h - 4);

  dc.DrawText(column.GetText(),
                     text_x, HEADER_OFFSET_Y + EXTRA_HEIGHT);

        x += wCol;
    }

    // //dc.EndDrawing();
}

void TreeListHeaderWindow::DrawCurrent()
{
    int x1 = m_currentX;
    int y1 = 0;
    ClientToScreen(&x1, &y1);

    int x2 = m_currentX-1;
#ifdef __WXMSW__
    ++x2; // but why ? 
#endif
    int y2 = 0;
    m_owner->GetClientSize(NULL, &y2);
    m_owner->ClientToScreen(&x2, &y2);

    wxScreenDC dc;
    dc.SetLogicalFunction(wxINVERT);
    dc.SetPen(wxPen(*wxBLACK, 2, wxPENSTYLE_SOLID));


    dc.SetBrush(*wxTRANSPARENT_BRUSH);

    AdjustDC(dc);

    dc.DrawLine(x1, y1, x2, y2);

    dc.SetLogicalFunction(wxCOPY);

    dc.SetPen(wxNullPen);
    dc.SetBrush(wxNullBrush);
}

void TreeListHeaderWindow::OnMouse(wxMouseEvent &event)
{
    // we want to work with logical coords
    int x;
    m_owner->CalcUnscrolledPosition(event.GetX(), 0, &x, NULL);
    int y = event.GetY();

    if (m_isDragging)
    {
        SendListEvent(wxEVT_COMMAND_LIST_COL_DRAGGING,
                      event.GetPosition());

  // we don't draw the line beyond our window, but we allow dragging it
        // there
        int w = 0;
        GetClientSize(&w, NULL);
        m_owner->CalcUnscrolledPosition(w, 0, &w, NULL);
        w -= 6;

        // erase the line if it was drawn
        if (m_currentX < w)
            DrawCurrent();

        if (event.ButtonUp())
        {
            ReleaseMouse();
            m_isDragging = false;
            m_dirty = true;
            
size_t newwidth = wxMax((m_currentX - m_minX), 10);             // // Find the new width --- may be negative, so don't let it get too small
int widthchange = newwidth - GetColumnWidth(m_column);
if (m_column == GetLastVisibleCol() && newwidth-current > 0)    // // If we're trying to drag-bigger the last visible column,
  { newwidth += widthchange * 2;                                // //    double the increment as the tiny drag-onto-able space makes it so slow otherwise
    m_columns[m_column].SetDesiredWidth(newwidth);              // //        and make it stick
  }

SetColumnWidth(m_column, newwidth);

  SizeColumns();  // //
            Refresh();
            SendListEvent(wxEVT_COMMAND_LIST_COL_END_DRAG,
                          event.GetPosition());
        }
        else
        {
            if (x > m_minX + 7)
                m_currentX = x;
            else
                m_currentX = m_minX + 7;

            // draw in the new location
            if (m_currentX < w)
                DrawCurrent();
        }
    }
    else // not dragging
    {
  if (event.RightUp())
    { int xpos = 0, clm = 0;  // find the column where this event occured
      for (size_t col = 0; col < GetColumnCount(); ++col)
        { if (IsHidden(col))   continue;
          xpos += GetColumnWidth(col);
          clm = col;
          if (x < xpos)
              break;       // inside the column
        }
      wxContextMenuEvent conevent(wxEVT_CONTEXT_MENU, clm, ClientToScreen(event.GetPosition()));
      return ShowContextMenu(conevent);
    }

    m_minX = 0;
        bool hit_border = false;

        // end of the current column
        int xpos = 0;

        // find the column where this event occured
        int countCol = GetColumnCount();
        for (int col = 0; col < countCol; col++)
        {
if (IsHidden(col))   continue;                                     // // I've 'hidden' some cols by giving them widths of zero.  This takes it into account
            xpos += GetColumnWidth(col);
            m_column = col;

            if ((abs(x-xpos) < 3) && (y < (HEADER_HEIGHT-1)))      // // I've changed "y <" comparator to HEADER_HEIGHT-1: originally the figure 22 was given
            {
                // near the column border
                hit_border = true;
                break;
            }

            if (x < xpos)
            {
                // inside the column
                break;
            }

            m_minX = xpos;
        }

        if (event.LeftDown() /*|| event.RightUp()*/)                 // // I've grabbed RtUp earlier to fire the context menu.  What it was doing here anyway?
        {
            if (hit_border/* && event.LeftDown()*/)                  // // See above comment
            {
                m_isDragging = true;
                m_currentX = x;
                DrawCurrent();
                CaptureMouse();
                SendListEvent(wxEVT_COMMAND_LIST_COL_BEGIN_DRAG,
                              event.GetPosition());
            }
            else // click on a column
            {
                SendListEvent(event.LeftDown()
             ? wxEVT_COMMAND_LIST_COL_CLICK
             : wxEVT_COMMAND_LIST_COL_RIGHT_CLICK,
             event.GetPosition());
            }
        }
        else if (event.Moving())
        {
            bool setCursor;
            if (hit_border)
            {
                setCursor = m_currentCursor == wxSTANDARD_CURSOR;
                m_currentCursor = m_resizeCursor;
            }
            else
            {
                setCursor = m_currentCursor != wxSTANDARD_CURSOR;
                m_currentCursor = (wxCursor*)wxSTANDARD_CURSOR;
            }

            if (setCursor)
                SetCursor(*m_currentCursor);
        }
    }
}

int TreeListHeaderWindow::GetNextVisibleCol(int currentcolumn)    // // Finds the next column with non-zero width
{
int colcount = GetColumnCount(); if (!colcount || currentcolumn==(colcount-1)) return -1;
for (int col=currentcolumn+1; col < colcount; ++col)
  if (!IsHidden(col)) return col;
  
return -1;
}

int TreeListHeaderWindow::GetLastVisibleCol()    // // Finds the last column with non-zero width.  0 flags failure (but shouldn't happen!)
{
int colcount = GetColumnCount(); if (!colcount) return 0;  // Shouldn't happen as col 0 will always exist & be visible
for (int col=colcount; col > 0; --col)
  if (!IsHidden(col-1)) return col-1;
  
return 0;
}

int TreeListHeaderWindow::GetTotalPreferredWidth()  // // Omits the extra amount added to the last column to fill the whole client-size
{
int lastvisiblecol = GetLastVisibleCol();// if (!lastvisiblecol) return GetWidth();
return GetWidth() - (GetColumnWidth(lastvisiblecol) - GetColumnDesiredWidth(lastvisiblecol));
}

void TreeListHeaderWindow::OnSetFocus(wxFocusEvent &WXUNUSED(event))
{
    m_owner->SetFocus();
}

void TreeListHeaderWindow::ShowIsFocused(bool focus)
{
p_BackgroundColour = (focus ? wxGetApp().GetBackgroundColourSelected(true) : wxGetApp().GetBackgroundColourUnSelected());
Refresh();
}

void TreeListHeaderWindow::SendListEvent(wxEventType type, wxPoint pos,
                                                          int column/*=-1*/)  // // I've added int column, as otherwise we have a problem when the event refers to hiding a col
{
    wxWindow *parent = GetParent();
    wxListEvent le(type, parent->GetId());
    le.SetEventObject(parent);
    le.m_pointDrag = pos;

    // the position should be relative to the parent window, not
    // this one for compatibility with MSW and common sense: the
    // user code doesn't know anything at all about this header
    // window, so why should it get positions relative to it?
    le.m_pointDrag.y -= GetSize().y;

    if (column == -1)                                           // //
    le.m_col = m_column;
   else le.m_col = -1;                                            // // Otherwise pass an intentionally invalid column.  This tells FileGenericDirCtrl::HeaderWindowClicked() not to (re)reverse the column
   
    parent->GetEventHandler()->ProcessEvent(le);
}


void TreeListHeaderWindow::AddColumn(const wxTreeListColumnInfo& col)
{
    m_columns.Add(col);
    m_total_col_width += col.GetWidth();
    //m_owner->GetHeaderWindow()->Refresh();
    //m_dirty = true;
//SizeColumns();                                      // //
    m_owner->AdjustMyScrollbars();
    m_owner->m_dirty = true;
    Refresh();
}

void TreeListHeaderWindow::SetColumnWidth(size_t column, size_t width)
{
    if(column < GetColumnCount()) {
  m_total_col_width -= m_columns[column].GetWidth();
  m_columns[column].SetWidth(width);
  m_total_col_width += width;
  // // m_owner->AdjustMyScrollbars();  (Don't bother:  this is always done afterwards by the calling functions)
  m_owner->m_dirty = true;
  //m_dirty = true;
  Refresh();
    }
}

void TreeListHeaderWindow::InsertColumn(size_t before,
            const wxTreeListColumnInfo& col)
{
    wxCHECK_RET(before < GetColumnCount(), _("Invalid column index"));
    m_columns.Insert(col, before);
    m_total_col_width += col.GetWidth();
    //m_dirty = true;
    //m_owner->GetHeaderWindow()->Refresh();
SizeColumns();                                      // //
    // //m_owner->AdjustMyScrollbars();
    m_owner->m_dirty = true;
    Refresh();
}


void TreeListHeaderWindow::RemoveColumn(size_t column)
{
    wxCHECK_RET(column < GetColumnCount(), _("Invalid column"));
    m_total_col_width -= m_columns[column].GetWidth();
    m_columns.RemoveAt(column);
    //m_dirty = true;
SizeColumns();                                      // //
    // //m_owner->AdjustMyScrollbars();
    m_owner->m_dirty = true;
    Refresh();
}


void TreeListHeaderWindow::SetColumn(size_t column,
               const wxTreeListColumnInfo& info)
{
    wxCHECK_RET(column < GetColumnCount(), _("Invalid column"));
    size_t w = m_columns[column].GetWidth();
    m_columns[column] = info;
    //m_owner->GetHeaderWindow()->Refresh();
    //m_dirty = true;
    if(w != info.GetWidth()) {
  m_total_col_width += info.GetWidth() - w;
  SizeColumns();                                      // //
  // //m_owner->AdjustMyScrollbars();
  m_owner->m_dirty = true;
    }
    Refresh();
}

void TreeListHeaderWindow::ToggleColumn(int col)    // // If it's hidden, show it & vice versa
{
if (IsHidden(col))    ShowColumn(col);
 else ShowColumn(col, false);
}

void TreeListHeaderWindow::ShowColumn(int col, bool show /*=true*/)    // // Show (or hide) the relevant column
{
if (IsHidden(col) == !show)  return;                  // If it's already doing what we want, wave goodbye

int width;
if (show)
  { int last = GetLastVisibleCol();
    if (col > last)                                   // If the newly-shown column will become the last visible, 
      { if (GetColumnDesiredWidth(last) > 0)          //   we must adjust the current last col first, otherwise it'll never revert to its preferred width
          SetColumnWidth(last, GetColumnDesiredWidth(last));
         else
          SetColumnWidth(last, m_owner->GetCharWidth() * DEFAULT_COLUMN_WIDTHS[last]);
      }
    width = GetColumnDesiredWidth(col);               // We need to know how big to make it
    if (width <= 0) width = m_owner->GetCharWidth() * DEFAULT_COLUMN_WIDTHS[col];  // If there's no valid size, use default, adjusted for fontsize
  }
 else 
  { width = 0;                                        // Hiding means zero width
    if (GetSelectedColumn() == col)  
      { SelectColumn(0);                              // If this col is currently selected, select col 0 instead
        SendListEvent(wxEVT_COMMAND_LIST_COL_CLICK,  wxPoint(0,0), -2);  // This tells the fileview not to re-order the files by col, as we just did
      }
  }
  
SetColumnWidth(col, width);                           // This also sets/resets the ishidden flag
if (show) SizeColumns();
  else ((FileGenericDirCtrl*)m_owner->GetParent())->DoSize();
m_dirty = true;                                       // We'll need to refresh the column display, as some names may be differently clipped
}

void TreeListHeaderWindow::SetSelectedColumn(size_t col)  // // Sets the selected column, the one that controls Sorting
{
GetColumn(GetSelectedColumn()).SetImage(-1);          // Turn off the image from the previously-selected column
selectedcolumn = (enum columntype)col;
}

void TreeListHeaderWindow::SelectColumn(int col)
{
ShowColumn(col);                                      // Better make sure it's visible!

SetSortOrder(0);                                      // In case we were previously reverse-sorting, set to normal
SetSelectedColumn(col);
GetColumn(col).SetImage(ColDownIcon);
Refresh();
}

bool TreeListHeaderWindow::SetSortOrder(int order /*=-1*/)    // // Set whether we're reverse-sorting.  If no parameter, negates sort-order
{
switch(order)
  { case 0:  reversesort = false; return reversesort;           // If 0, set order to normal
    case 1:  reversesort = true;  return reversesort;           // If 1, set order to reverse
     default:  reversesort = !reversesort; return reversesort;  // Otherwise reverse the current order
  }
}

void TreeListHeaderWindow::SetSelectedColumnImage(bool order)    // // Sets the correct up/down image according to sort-order parameter
{
GetColumn(GetSelectedColumn()).SetImage(ColDownIcon + order);
Refresh();
}

void TreeListHeaderWindow::ReverseSelectedColumn()
{
SetSelectedColumnImage(SetSortOrder()); // Calling SetSortOrder() to get the parameter has the effect of toggling reverse
}

void TreeListHeaderWindow::SizeColumns()    // // Resize the last visible col to occupy any spare space
{
size_t colcount = GetColumnCount(); if (!colcount) return;

int width, h; GetClientSize(&width, &h);
int diff = width - m_total_col_width;               // Is the new width > or < than the previous value?
if (diff == 0) return;                              // If ISQ, not much to do

int lastvisiblecol = GetLastVisibleCol();
if (diff < 0)                                       // If negative, reduce the last col's width towards its preferred value if currently higher
  { int spare =  GetColumnWidth(lastvisiblecol) - GetColumnDesiredWidth(lastvisiblecol);
    if (spare > 0)
      SetColumnWidth(lastvisiblecol, GetColumnWidth(lastvisiblecol) - wxMin(abs(diff), spare));
  }
 else
  SetColumnWidth(lastvisiblecol, GetColumnWidth(lastvisiblecol) + diff);  // Otherwise just dump the spare space onto the last visible col, to avoid a silly-looking blank

m_owner->AdjustMyScrollbars();
Refresh(); 
}

void TreeListHeaderWindow::ShowContextMenu(wxContextMenuEvent& event)  // // Menu to choose which columns to display
{
int columns[] = { SHCUT_SHOW_COL_EXT, SHCUT_SHOW_COL_SIZE, SHCUT_SHOW_COL_TIME, SHCUT_SHOW_COL_PERMS, SHCUT_SHOW_COL_OWNER,
                  SHCUT_SHOW_COL_GROUP, SHCUT_SHOW_COL_LINK, wxID_SEPARATOR, SHCUT_SHOW_COL_ALL, SHCUT_SHOW_COL_CLEAR };
wxMenu colmenu;
colmenu.SetClientData((wxClientData*)this); // In >wx2.8 we must store ourself in the menu, otherwise MyFrame::DoColViewUI can't work out who we are

for (size_t n=0; n < 10; ++n)
  { MyFrame::mainframe->AccelList->AddToMenu(colmenu, columns[n], wxEmptyString,  (n < 7 ? wxITEM_CHECK : wxITEM_NORMAL));
  #ifdef __WXX11__
    if (n < 7) colmenu.Check(columns[n], !IsHidden(n+1)); // The usual updateui way of doing this doesn't work in X11
  #endif
  }

if (event.GetId() == 1) // If the right-click was over the 'ext' menu, add a submenu
  { int extsubmenu[] = { SHCUT_EXT_FIRSTDOT, SHCUT_EXT_MIDDOT, SHCUT_EXT_LASTDOT };
    wxMenu* submenu = new wxMenu(_("An extension starts at the filename's..."));
    for (size_t n=0; n < 3; ++n)
      MyFrame::mainframe->AccelList->AddToMenu(*submenu, extsubmenu[n], wxEmptyString, wxITEM_RADIO);

    MyFrame::mainframe->AccelList->AddToMenu(colmenu, wxID_SEPARATOR);
    colmenu.AppendSubMenu(submenu, _("Extension definition..."));

    submenu->FindItem(extsubmenu[EXTENSION_START])->Check();
  }

colmenu.SetTitle(wxT("Columns to Display"));

wxPoint pt = event.GetPosition();
ScreenToClient(&pt.x, &pt.y);
#ifdef __WXX11__
  m_owner->PopupMenu(&colmenu, pt.x, 0);  // In X11 a headerwindow doesn't seem to be able to maintain a popup window, so use the treectrl
#else
  PopupMenu(&colmenu, pt.x, pt.y);
#endif
}

void TreeListHeaderWindow::OnExtSortMethodChanged(wxCommandEvent& event)  // From the ext-column submenu of the context menu
{
switch(event.GetId() - SHCUT_EXT_FIRSTDOT)
  { case 0:   EXTENSION_START = 0; break;
    case 1:   EXTENSION_START = 1; break;
    case 2:   EXTENSION_START = 2; break;
  }

wxConfigBase::Get()->Write(wxT("/Misc/Display/EXTENSION_START"), (long)EXTENSION_START);

MyFrame::mainframe->RefreshAllTabs(NULL);  // This makes all the fileviews update
}

void TreeListHeaderWindow::OnColumnSelectMenu(wxCommandEvent& event)
{
bool flag;                   // First deal with ShowAll and Clear
if ((flag = (event.GetId() == SHCUT_SHOW_COL_ALL)) ||  (event.GetId() == SHCUT_SHOW_COL_CLEAR))
  { for (size_t col = 1; col < TOTAL_NO_OF_FILEVIEWCOLUMNS; ++col)
        ShowColumn(col, flag);
    return;
  }
                            // Otherwise it's a straight-forward column-toggle
size_t col = event.GetId() - SHCUT_SHOW_COL_EXT + 1;  // +1 because Name column is always visible
if (col > TOTAL_NO_OF_FILEVIEWCOLUMNS) return;        // Sanity check

ToggleColumn(col);
}

BEGIN_EVENT_TABLE(TreeListHeaderWindow,wxWindow)
  EVT_PAINT         (TreeListHeaderWindow::OnPaint)
  EVT_MOUSE_EVENTS  (TreeListHeaderWindow::OnMouse)
  EVT_SET_FOCUS     (TreeListHeaderWindow::OnSetFocus)
  EVT_MENU_RANGE(SHCUT_SHOW_COL_EXT, SHCUT_SHOW_COL_CLEAR, TreeListHeaderWindow::OnColumnSelectMenu)
  EVT_MENU_RANGE(SHCUT_EXT_FIRSTDOT, SHCUT_EXT_LASTDOT, TreeListHeaderWindow::OnExtSortMethodChanged)
  EVT_CONTEXT_MENU(TreeListHeaderWindow::ShowContextMenu)
END_EVENT_TABLE()
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------

MyTreeCtrl::MyTreeCtrl(wxWindow *parentwin, wxWindowID id , const wxPoint& pos, const wxSize& size,
                               long style, const wxValidator &validator, const wxString& name)
                                              :  wxTreeCtrl(parentwin, id, pos, size, style, validator, name), m_dirty(false)
{
headerwindow = NULL;                                  // To flag to premature OnIdle events that there's nothing safe to do

HeaderimageList = new wxImageList(16, 10, true);      // This holds the down & up symbols for the header of the selected fileview column
HeaderimageList->Add(wxIcon(columnheaderdown_xpm));
HeaderimageList->Add(wxIcon(columnheaderup_xpm));

// >2.6.3 the gtk2 default was changed to no lines, and hard-coded into create! So we need to do this here, not by adjusting style
if (SHOW_TREE_LINES) SetWindowStyle(GetWindowStyle() & ~wxTR_NO_LINES);
  else SetWindowStyle(GetWindowStyle() | wxTR_NO_LINES);
  
parent = (MyGenericDirCtrl*)parentwin;

IgnoreRtUp = false;
dragging = false;
startpt.x = startpt.y = 0;

CreateAcceleratorTable();
}

void MyTreeCtrl::CreateAcceleratorTable()
{
wxAcceleratorEntry entries[1];
MyFrame::mainframe->AccelList->FillEntry(entries[0], SHCUT_SELECTALL);  // AcceleratorList supplies the keycode etc
wxAcceleratorTable accel(1, entries);
SetAcceleratorTable(accel);
}

void MyTreeCtrl::RecreateAcceleratorTable()
{
        // It would be nice to be able to empty the old accelerator table before replacing it, but trying caused segfaulting!
CreateAcceleratorTable();                                   // Make a new one, with up-to-date data
}
   
void MyTreeCtrl::OnCtrlA(wxCommandEvent& event)             // Does Select All on Ctrl-A
{
if (parent->fileview==ISLEFT)   return;    // I can't make it work properly for dir-views, & I'm not sure it's a good idea anyway --- select & delete whole filesystem?
if (!m_current) return;                // Don't ask me why, but if there isn't a current selection, SelectItemRange segfaults


wxTreeItemIdValue cookie; 
wxTreeItemId firstItem = GetFirstChild(GetRootItem(), cookie);        // Use FirstChild as 1st item
if (!firstItem.IsOk()) return;
wxGenericTreeItem* first = (wxGenericTreeItem*)firstItem.m_pItem;

wxTreeItemId lastItem = GetLastChild(GetRootItem());                  // & LastChild as last
if (!lastItem.IsOk()) return;
wxGenericTreeItem* last = (wxGenericTreeItem*)lastItem.m_pItem;

SelectItemRange(first, last);

wxArrayString selections; static_cast<MyGenericDirCtrl*>(parent)->GetMultiplePaths(selections);
static_cast<FileGenericDirCtrl*>(parent)->UpdateStatusbarInfo(selections); // Update the statusbar info
}

void MyTreeCtrl::OnReceivingFocus(wxFocusEvent& event)  // This tells grandparent window which of the 2 twin-ctrls is the one to consider active
{
if (!MyFrame::mainframe->SelectionAvailable) return;    // If we're constructing/destructing, do nothing lest we break a non-existent window
parent->ParentTab->SetActivePane(parent);               // This also stores the pane in FocusController

wxString selected = parent->GetPath();
wxArrayString selections; static_cast<MyGenericDirCtrl*>(parent)->GetMultiplePaths(selections);
parent->DisplaySelectionInTBTextCtrl(selected);         // Take the opportunity also to display current selection in textctrl
MyFrame::mainframe->Layout->OnChangeDir(selected);      //  & the Terminal etc if necessary
static_cast<MyGenericDirCtrl*>(parent)->m_StatusbarInfoValid = false;
if (parent->fileview == ISLEFT)                         //  & in the statusbar
  ((DirGenericDirCtrl*)parent)->UpdateStatusbarInfo(selections);
 else    ((FileGenericDirCtrl*)parent)->UpdateStatusbarInfo(selections);

((DirGenericDirCtrl*)parent)->CopyToClipboard(selections, true); // FWIW, copy the filepath(s) to the primary selection too

if (headerwindow)
  { parent->ParentTab->SwitchOffHighlights(); // Start with a blank canvas
    headerwindow->ShowIsFocused(true);
  }

HelpContext = HCunknown;
event.Skip();
}

void MyTreeCtrl::OnLosingFocus(wxFocusEvent& event)   // Unhighlights the headerwindow
{
if (headerwindow)
  headerwindow->ShowIsFocused(false);
}

void MyTreeCtrl::AdjustForNumlock(wxKeyEvent& event)  // The fix for the X11 Numlock problem
{
event.m_metaDown = false;                   // Makes it look as if numlock is off, so Accelerators are properly recognised

if (event.GetKeyCode() == WXK_ESCAPE)       // This is used in DnD, to abort it if ESC pressed
  MyFrame::mainframe->OnESC_Pressed();

event.Skip();
}

bool MyTreeCtrl::QueryIgnoreRtUp()
{
if (IgnoreRtUp) 
    { IgnoreRtUp = false; return true; }    // If we've been Rt-dragging, and now stopped, ignore the event so that we don't have a spurious context menu
 else return false;                         // Otherwise proceed as normal
}

void MyTreeCtrl::OnMiddleDown(wxMouseEvent& event)  // Allows middle-button paste
{
event.Skip();

int flags; wxTreeItemId item = HitTest(event.GetPosition(), flags); // Find out if we're on a tree item
if (item.IsOk()) SelectItem(item);                                  // If so, select it so that any paste gets deposited there, not in any currently selected folder.  That's _probably_ what's expected

wxCommandEvent cevent;
parent->OnShortcutPaste(cevent);
}

void MyTreeCtrl::OnLtDown(wxMouseEvent& event)  // Start DnD pre-processing if we're near a selection
{
event.Skip();                                   // First allow base-class to process it

int flags; wxTreeItemId item = HitTest(event.GetPosition(), flags);  // Find out if we're on a tree item
if (item.IsOk()) OnBeginDrag(event);            // If so, if it's a selection start drag-processing
                                                // If we're NOT near a selection, ignore
}

void MyTreeCtrl::OnMouseMovement(wxMouseEvent& event)
{
if (!event.Dragging())
  { if (dragging)  ResetPreDnD();               // If we had been seeing whether or not to drag, & now here's a non-drag event, cancel start-up

    if (SHOWPREVIEW)
      { if (parent->fileview==ISRIGHT)
          { int flags; wxTreeItemId item = HitTest(event.GetPosition(), flags);
            if (item.IsOk())
              { wxDirItemData* data = (wxDirItemData*)GetItemData(item);  
                PreviewManager::ProcessMouseMove(item, flags, data->m_path, event);
              }
          }
         else PreviewManager::Clear();
      }

    event.Skip();   return;
  }
 else
  PreviewManager::Clear();

                      // If we're dragging but with the Rt button, do a multiple select if wxTR_MULTIPLE is true
if (event.RightIsDown() && !event.LeftIsDown())
  { if (!HasFlag(wxTR_MULTIPLE)) return;        // Can't have multiple items selected so abort
    
    int flags; wxArrayTreeItemIds selection;
    wxTreeItemId item = HitTest(event.GetPosition(), flags);  // Find out if we're on the level of a tree item
    bool alreadyaselection = GetSelections(selection);        //  & whether or not the tree already has >0 items selected
    if (item.IsOk() && !IsSelected(item))       // if we're on an item,  & it isn't already selected
      { if (!alreadyaselection)                 // If there is currently no selection, select it
              SelectItem(item);
          else
              DoSelectItem(item, false, true);  // Otherwise extend the current selection
        IgnoreRtUp = true;
      }
    return; 
  }

if (dragging) OnBeginDrag(event);               // Otherwise, if pre-drag has been initiated, go to OnBeginDrag to continue it
}

#if wxVERSION_NUMBER >2503
void MyTreeCtrl::OnRtUp(wxMouseEvent& event)    // In >wxGTK-2.5.3 the wxTreeCtrl has started to interfere with Context Events.  This is a workaround
{
wxContextMenuEvent conevent(wxEVT_CONTEXT_MENU, wxID_ANY, ClientToScreen(event.GetPosition()));
if (parent->fileview == ISLEFT)                 // Send the event to the correct type of parent
  ((DirGenericDirCtrl*)parent)->ShowContextMenu(conevent);
 else  
  ((FileGenericDirCtrl*)parent)->ShowContextMenu(conevent);
}
#endif

void MyTreeCtrl::OnBeginDrag(wxMouseEvent& event)
{
  // I've taken over the detection of dragging from wxTreeCtrl, which tends to be trigger-happy:  the slightest movement 
  // with a mouse-key down sometimes will trigger it. So I've added a distance requirement.  Note that this is asymmetrical, the x axis requirement being less.
  // This is because of the edge effect:  if the initial click is too near the edge of a pane & the mouse is then moved edgewards, it may go into the adjacent pane
  // before the drag commences, whereupon it picks up the selection in THAT pane, not the original.
  // I've also added a time element
  // There was also a problem with selections:  when a LClick arrives here it hasn't had time to establish a Selection, so we have to assume it will, & check later that is was valid
  
static wxDateTime dragstarttime;  
static wxTreeItemId dragitem;      // We need this static, so that we can check that the item that starts the process is actually selected by the time dragging is triggered
static MyTreeCtrl* originaltree;

if (startpt.x == 0 && startpt.y == 0)           // 0,0 means not yet set, so this is what we do the first time thru
  { int flags; dragitem = HitTest(event.GetPosition(), flags);  // Find out if we're near a tree item (& not in the space to the right of it)
    if (!dragitem.IsOk()  || flags & wxTREE_HITTEST_ONITEMRIGHT) 
          return;                // Abort if we're not actually over the selected item.  Otherwise we'll start dragging an old selection even if we're now miles away
    if (dragitem == GetRootItem()) return;      // Or if it's root
    // It's a valid LClick, so get things ready for the next time through
    startpt.x = event.GetPosition().x; startpt.y = event.GetPosition().y;
    dragstarttime = wxDateTime::UNow();         // Reset the time to 'now'
    originaltree = this;                        // In case we change panes before the drag 'catches'
    dragging = true;  return; 
  }
  
if ((abs(startpt.x - event.GetPosition().x)) < DnDXTRIGGERDISTANCE   // If we've not moved far enough in either direction, return
              &&  (abs(startpt.y - event.GetPosition().y)) < DnDYTRIGGERDISTANCE)      return;

wxTimeSpan diff = wxDateTime::UNow() - dragstarttime;
if (diff.GetMilliseconds() < 100)  return;      // If 0.1 sec hasn't passed since dragging was first noticed, return

if (!originaltree->IsSelected(dragitem)) { ResetPreDnD(); return; }  // If we've got this far, we can trigger provided that by now the item has become selected. Otherwise abort the process

                          // If we're here, there's a genuine drag event
ResetPreDnD();                                  // Reset dragging & startpt for the future

if (MyFrame::mainframe->m_TreeDragMutex.TryLock() != wxMUTEX_NO_ERROR) 
  { wxLogTrace(wxT("%s"),wxT("MyTreeCtrl::OnBeginDrag - - - - - - - - - - - - - Mutex was locked")); 
    MyFrame::mainframe->m_TreeDragMutex.Unlock(); // If we don't unlock it here, DnD will never again work.  Surely it will be safe by now . . . .?
    return; 
  }

size_t count;                                   // No of selections (may be multiple, after all) 
wxArrayString paths;                            // Array to store them

count = parent->GetMultiplePaths(paths);        // Fill the array with selected pathname/filenames
if (!count) return;

MyGenericDirCtrl::DnDfilearray = paths;         // Fill the DnD "clipboard" with paths
MyGenericDirCtrl::DnDfilecount = count;         // Store their number
MyGenericDirCtrl::DnDFromID = parent->GetId();  // and the window ID (for refreshing)  Note the reasonable assumption that all selections belong to same pane

MyFrame::mainframe->m_CtrlPressed = event.m_controlDown;  // Store metakey pattern, so that MyFrame::OnBeginDrag will know if we're copying, moving etc 
MyFrame::mainframe->m_ShPressed = event.ShiftDown();
MyFrame::mainframe->m_AltPressed = event.AltDown();

MyFrame::mainframe->OnBeginDrag(this);              // Pass control to main Drag functions
}

void MyTreeCtrl::OnEndDrag(wxTreeEvent& event)
{
wxYieldIfNeeded();
                    // Now we need to find if we've found somewhere to drop onto
wxPoint pt = ScreenToClient(wxGetMousePosition());          // Locate the mouse?  Screen coords so -> client
int flags;
wxTreeItemId item = HitTest(pt, flags);                     // Find out if we're over a valid tree item (& not in the space to the right of it)
if (item.IsOk()  && !(flags & wxTREE_HITTEST_ONITEMRIGHT))
  { wxDirItemData* data = (wxDirItemData*) GetItemData(item);  
    MyGenericDirCtrl::DnDDestfilepath = data->m_path;       //   so get its path
    MyGenericDirCtrl::DnDToID = parent->GetId();            //     and its ID (for refreshing)
    if (parent->fileview == ISRIGHT)                        // However, if we're in a fileview pane, need to check that we're not dropping on top of a file
      { FileData fd(MyGenericDirCtrl::DnDDestfilepath);
        if (fd.IsSymlinktargetADir(true))                   // If we're dropping onto a symlink & its target is a dir
          MyGenericDirCtrl::DnDDestfilepath = fd.GetUltimateDestination(); // replace path with the symlink target. NB If the target isn't a dir, we don't want to deref the symlink at all; just paste into its parent dir as usual
         else
          if (fd.IsValid() && !fd.IsDir()) MyGenericDirCtrl::DnDDestfilepath = fd.GetPath();  // If the overall path isn't a dir, remove the filename, so we drop onto the parent dir
      }
  }
 else
  { MyGenericDirCtrl::DnDDestfilepath = wxEmptyString;      // If HitTest() missed,  mark invalid 
    MyFrame::mainframe->m_TreeDragMutex.Unlock();  
    return;                                                 // and bail out
  }
if (!MyGenericDirCtrl::DnDfilecount) return;

enum myDragResult dragtype = MyFrame::mainframe->ParseMetakeys();  // First find out if we've been moving, copying or linking

bool ClusterWasNeeded = UnRedoManager::StartClusterIfNeeded();
switch (dragtype)
 {
  case myDragCopy:      parent->OnPaste(true); break;
  case myDragMove:      {DevicesStruct* ds = MyFrame::mainframe->Layout->m_notebook->
                                      DeviceMan->QueryAMountpoint(MyGenericDirCtrl::DnDfilearray[0], true); // See if we're trying to drag from a ro device
                        if (ds != NULL && ds->readonly)  parent->OnPaste(true);  // If we _are_ trying to move from a cdrom or similar, silently alter to Paste
                         else parent->OnDnDMove(); break;}                           // Otherwise do a Move as requested
  case myDragHardLink: 
  case myDragSoftLink:  parent->OnLink(true, dragtype); break;
  case myDragNone:      break;
   default:  break;
 }
if (ClusterWasNeeded && !(dragtype==myDragCopy || dragtype==myDragMove)) UnRedoManager::EndCluster(); // Don't end the cluster if threads are involved; it'll be too soon
 
MyFrame::mainframe->m_TreeDragMutex.Unlock();               // Finally, release the tree-mutex lock
}

void MyTreeCtrl::OnPaint(wxPaintEvent& event)  // // Copied from wxTreeCtrl only because it's the caller of the overridden (PaintLevel()->) PaintItem
{
if (parent->fileview == ISLEFT) return wxTreeCtrl::OnPaint(event);    // // If it's a dirview, use the original version

Col0 = STRIPE_0; Col1 = STRIPE_1;  // // Copy these at the beginning of the paint so, if the defaults get changed halfway thru, we don't end up looking stupid
    wxPaintDC dc(this);
    PrepareDC(dc);

    if (!m_anchor)
        return;

    dc.SetFont(m_normalFont);
    dc.SetPen(m_dottedPen);

    // this is now done dynamically
    //if(GetImageList() == NULL)
    // m_lineHeight = (int)(dc.GetCharHeight() + 4);

    int y = 2;
    PaintLevel(m_anchor, dc, 0, y, 0);
}

void MyTreeCtrl::PaintLevel(wxGenericTreeItem *item, wxDC &dc, int level, int &y, int index)  // // I've added index
{
    int x = level*m_indent;
    if (!HasFlag(wxTR_HIDE_ROOT))
    {
        x += m_indent;
    }
    else 
    { if (level == 0)
        {
          // always expand hidden root
          int origY = y;
          wxArrayGenericTreeItems& children = item->GetChildren();
          int count = children.Count();
          if (count > 0)
          {
        int n = 0, oldY;
        do {
            oldY = y;
            PaintLevel(children[n], dc, 1, y, n);
        } while (++n < count);

        if (!HasFlag(wxTR_NO_LINES) && HasFlag(wxTR_LINES_AT_ROOT) && count > 0)
        {
            // draw line down to last child
            origY += GetLineHeight(children[0])>>1;
            oldY += GetLineHeight(children[n-1])>>1;
            dc.DrawLine(3, origY, 3, oldY);
        }
          }
          return;
        }
        else
        {
#if defined(__WXGTK3__)
            // Fiddle-factor for gtk3 (at least in fedora 20, & does no harm elsewhere). Otherwise the left edge of each dir icon is lost
            x += 5;
#endif
        }
    }

    item->SetX(x+m_spacing);
    item->SetY(y);

    int h = GetLineHeight(item);
    int y_top = y;
    int y_mid = y_top + (h>>1);
    y += h;

    int exposed_x = dc.LogicalToDeviceX(0);
    int exposed_y = dc.LogicalToDeviceY(y_top);

    if (IsExposed(exposed_x, exposed_y, 10000, h))  // 10000 = very much
    {
        const wxPen *pen =
#ifndef __WXMAC__
            // don't draw rect outline if we already have the
            // background color under Mac
            (item->IsSelected() && m_hasFocus) ? wxBLACK_PEN :
#endif // !__WXMAC__
            wxTRANSPARENT_PEN;

        wxColour colText;
        if (item->IsSelected())
        {
            colText = wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT);
        }
        else
        {
            wxTreeItemAttr *attr = item->GetAttributes();
            if (attr && attr->HasTextColour())
                colText = attr->GetTextColour();
            else
                colText = GetForegroundColour();
        }

        // prepare to draw
        dc.SetTextForeground(colText);
        dc.SetPen(*pen);

        // draw
        PaintItem(item, dc, index);

        if (HasFlag(wxTR_ROW_LINES))
        {
            // if the background colour is white, choose a
            // contrasting color for the lines
            dc.SetPen(*((GetBackgroundColour() == *wxWHITE)
                         ? wxMEDIUM_GREY_PEN : wxWHITE_PEN));
            dc.DrawLine(0, y_top, 10000, y_top);
            dc.DrawLine(0, y, 10000, y);
        }

        // restore DC objects
        dc.SetBrush(*wxWHITE_BRUSH);
        dc.SetPen(m_dottedPen);
        dc.SetTextForeground(*wxBLACK);

        if (item->HasPlus() && HasButtons())  // should the item show a button?
        {
            if (!HasFlag(wxTR_NO_LINES))
            {
                if (x > (signed)m_indent)
                    dc.DrawLine(x - m_indent, y_mid, x - 5, y_mid);
                else if (HasFlag(wxTR_LINES_AT_ROOT))
                    dc.DrawLine(3, y_mid, x - 5, y_mid);
                dc.DrawLine(x + 5, y_mid, x + m_spacing, y_mid);
            }
#if wxVERSION_NUMBER > 3105
            if ( m_imagesButtons.HasImages() )
            {
                // draw the image button here
                int image_h = 0,  image_w = 0;
                int image = item->IsExpanded() ? wxTreeItemIcon_Expanded
                                               : wxTreeItemIcon_Normal;
                if ( item->IsSelected() )
                    image += wxTreeItemIcon_Selected - wxTreeItemIcon_Normal;

                wxImageList* const
                    imageListButtons = m_imagesButtons.GetImageList();
                imageListButtons->GetSize(image, image_w, image_h);
                int xx = x - image_w/2;
                int yy = y_mid - image_h/2;

                wxDCClipper clip(dc, xx, yy, image_w, image_h);
                imageListButtons->Draw(image, dc, xx, yy,
                                         wxIMAGELIST_DRAW_TRANSPARENT);
            }
#else // !wxVERSION_NUMBER > 3105
            if (m_imageListButtons != NULL)
            {
                // draw the image button here
                int image_h = 0, image_w = 0, image = wxTreeItemIcon_Normal;
                if (item->IsExpanded()) image = wxTreeItemIcon_Expanded;
                if (item->IsSelected())
                    image += wxTreeItemIcon_Selected - wxTreeItemIcon_Normal;
                m_imageListButtons->GetSize(image, image_w, image_h);
                int xx = x - (image_w>>1);
                int yy = y_mid - (image_h>>1);
                dc.SetClippingRegion(xx, yy, image_w, image_h);
                m_imageListButtons->Draw(image, dc, xx, yy,
                                         wxIMAGELIST_DRAW_TRANSPARENT);
                dc.DestroyClippingRegion();
            }
#endif
    }
  }

           else // no custom buttons
            {
                static const int wImage = 9;
                static const int hImage = 9;
                
                int flag = 0;
                if (item->IsExpanded())
                    flag |= wxCONTROL_EXPANDED;
                if (item == m_underMouse)
                    flag |= wxCONTROL_CURRENT;
                                            
                wxRendererNative::Get().DrawTreeItemButton
                                        (
                                            this,
                                            dc,
                                            wxRect(x - wImage/2,
                                                   y_mid - hImage/2,
                                                   wImage, hImage),
                                            flag
                                    );
            }



    if (item->IsExpanded())
    {
        wxArrayGenericTreeItems& children = item->GetChildren();
        int count = children.Count();
        if (count > 0)
        {
            int n = 0, oldY;
            ++level;
            do {
                oldY = y;
                PaintLevel(children[n], dc, level, y);
            } while (++n < count);

            if (!HasFlag(wxTR_NO_LINES) && count > 0)
            {
                // draw line down to last child
                oldY += GetLineHeight(children[n-1])>>1;
                if (HasButtons()) y_mid += 5;

                // Only draw the portion of the line that is visible, in case it is huge
                wxCoord  xOrigin=0, yOrigin=0, width, height;
                dc.GetDeviceOrigin(&xOrigin, &yOrigin);
                yOrigin = abs(yOrigin);
                GetClientSize(&width, &height);

                // Move end points to the begining/end of the view?
                if (y_mid < yOrigin)
                    y_mid = yOrigin;
                if (oldY > yOrigin + height)
                    oldY = yOrigin + height;

                // after the adjustments if y_mid is larger than oldY then the line
                // isn't visible at all so don't draw anything
                if (y_mid < oldY)
                    dc.DrawLine(x, y_mid, x, oldY);
            }
        }
    }
}

void MyTreeCtrl::PaintItem(wxGenericTreeItem *item, wxDC& dc, int index)
{
static const int NO_IMAGE = -1;                                  // //
    // TODO implement "state" icon on items

    wxTreeItemAttr *attr = item->GetAttributes();
    if (attr && attr->HasFont())
        dc.SetFont(attr->GetFont());
    else if (item->IsBold())
        dc.SetFont(m_boldFont);

    int text_w = 0, text_h = 0;
    
    dc.GetTextExtent(item->GetText(), &text_w, &text_h);

    int total_h = GetLineHeight(item);

    if (item->IsSelected())
    {
#if wxVERSION_NUMBER < 3104
        dc.SetBrush(*(m_hasFocus ? m_hilightBrush : m_hilightUnfocusedBrush));
#else
        dc.SetBrush(m_hasFocus ? m_hilightBrush : m_hilightUnfocusedBrush);
#endif
    }
    else
    {
        wxColour colBg;
        if (attr && attr->HasBackgroundColour())
            colBg = attr->GetBackgroundColour();
        else
      if (USE_STRIPES)                                          // // If we want the fileview background to be striped with alternate colours
              colBg = (index % 2) ? Col1 : Col0;
       else colBg = m_backgroundColour;

        dc.SetBrush(wxBrush(colBg, wxBRUSHSTYLE_SOLID));
    }

    int offset = HasFlag(wxTR_ROW_LINES) ? 1 : 0;

    dc.DrawRectangle(0, item->GetY()+offset,
         headerwindow->GetWidth(),                                  // //
         total_h-offset);

    dc.SetBackgroundMode(wxTRANSPARENT);
    int extraH = (total_h > text_h) ? (total_h - text_h)/2 : 0;
    int extra_offset = 0;

DataBase* stat = NULL;                                            // //
          // // After simplifying (& so probably speeding up) DirGenericDirCtrl::ReloadTree(), I started getting rare Asserts where index==GetCount() during Moving/Pasting
          // // I suspect data is sometimes being added to the treectrl faster than to the array, If a refresh then occurs at the wrong time . . .
if (((FileGenericDirCtrl*)parent)->CombinedFileDataArray.GetCount() <= (size_t)index)  return;  // //
stat = &((FileGenericDirCtrl*)parent)->CombinedFileDataArray.Item(index); // // Get a ptr to the current stat data
if (!stat) return;                                                        // // With USE_FSWATCHER this avoids rare assert, presumably a race

for(size_t i = 0; i < headerwindow->GetColumnCount(); ++i)                // //
 {
if (headerwindow->IsHidden(i)) continue;                                  // // Not a lot to do for a hidden column

  int coord_x = extra_offset, image_x = coord_x;
  int clip_width = headerwindow->GetColumnWidth(i);                       // //
  int image_h = 0, image_w = 0; //2;
  int image = NO_IMAGE;
  
  if(i == 0) {                                                            // //
      image = item->GetCurrentImage();
      coord_x = item->GetX();
          }
  else {
      image = NO_IMAGE;  // //item->GetImage(i);
      }
  
    if (image != NO_IMAGE) 
     {
#if wxVERSION_NUMBER > 3105
      if (GetImageList()) {
          GetImageList()->GetSize(image, image_w, image_h);
#else
      if (m_imageListNormal) {
        m_imageListNormal->GetSize(image, image_w, image_h);
#endif // wxVERSION_NUMBER > 3105
        image_w += 4;
      } else { image = NO_IMAGE;  }
   }

  // honor text alignment
  wxString text("?"); // // If the file is corrupt, display _something_
  if (!i)                                                     // // If col 0, do it the standard way
    text = stat->ReallyGetName();                             // // but use stat->ReallyGetName() as this is might hold a useful string even if the file is corrupt

   else                                                       // // The whole 'else' is mine
    { const static time_t YEAR2038(2145916800);
      wxDateTime time;
      if (stat && !headerwindow->IsHidden(i))
        switch(i)
        { case ext:        if (stat->IsValid())
                             {  wxString fname(stat->GetFilename().Mid(1));       // Mid(1) to avoid hidden-file confusion
                                if (EXTENSION_START == 0)                         // Use the first dot. This one's easy: if AfterFirst() fails it returns ""
                                  { text = fname.AfterFirst(wxT('.')); break; }
                                 else
                                  { size_t pos = fname.rfind(wxT('.'));
                                    if (pos != wxString::npos)
                                      { text = fname.Mid(pos+1);                  // We've found a last dot. Store the remaining string
                                        if (EXTENSION_START == 1)                 // and then, if so configured, look for a penultimate one
                                          { pos = fname.rfind(wxT('.'), pos-1);
                                            if (pos != wxString::npos)
                                              text = fname.Mid(pos+1);
                                          }
                                      }
                                      else text.Clear(); // We don't want to display '?' just because there's no ext
                                  }
                              }
                            break;
        
          case filesize:    if (stat->IsValid()) text = stat->GetParsedSize();
                            break;
          case modtime:     if (stat->IsValid() && stat->ModificationTime() < YEAR2038) // If it's > 2038 the datetime is highly likely to be invalid, and will assert below
                              { time.Set(stat->ModificationTime());
                                if (time.IsValid())
                                  text = time.Format("%x %R"); // %x means d/m/y but arranged according to locale. %R is %H:%M
                              }
                             else
                              text = "00/00/00";
                            break;
          case permissions: if (stat->IsValid()) text = stat->PermissionsToText();
                            break;
          case owner:       if (stat->IsValid()) text = stat->GetOwner(); if (text.IsEmpty())  text.Printf(wxT("%u"), stat->OwnerID());
                            break;
          case group:       if (stat->IsValid()) text = stat->GetGroup(); if (text.IsEmpty())  text.Printf(wxT("%u"), stat->GroupID());
                            break;
          case linkage:     if (stat->IsValid() && stat->IsSymlink())
                              { if (stat->GetSymlinkData() && stat->GetSymlinkData()->IsValid())
                                  text.Printf(wxT("-->%s"), stat->GetSymlinkDestination(true).c_str());
                                 else text = _(" *** Broken symlink ***");
                              }
                             else text.Clear(); // We don't want to display '?' just because it's not a link
        }    
    }
    
  switch(headerwindow->GetColumn(i).GetAlignment()) {
  case wxTL_ALIGN_LEFT:
      coord_x += image_w + 2;
      image_x = coord_x - image_w;
      break;
  case wxTL_ALIGN_RIGHT:
      dc.GetTextExtent(text, &text_w, NULL);
      coord_x += clip_width - text_w - image_w - 2;
      image_x = coord_x - image_w;
      break;
  case wxTL_ALIGN_CENTER:
      dc.GetTextExtent(text, &text_w, NULL);
      //coord_x += (clip_width - text_w)/2 + image_w;
      image_x += (clip_width - text_w - image_w)/2 + 2;
      coord_x = image_x + image_w;
  }  

  wxDCClipper clipper(dc, /*coord_x,*/ extra_offset,
          item->GetY() + extraH, clip_width,
          total_h);

  if (image != NO_IMAGE) {
#if wxVERSION_NUMBER > 3105
      GetImageList()->Draw(image, dc, image_x,
#else
      m_imageListNormal->Draw(image, dc, image_x,
#endif
             item->GetY() +((total_h > image_h)?
                ((total_h-image_h)/2):0),
             wxIMAGELIST_DRAW_TRANSPARENT);
  }

  dc.DrawText(text,
         (wxCoord)(coord_x /*image_w + item->GetX()*/),
         (wxCoord)(item->GetY() + extraH));
  extra_offset += headerwindow->GetColumnWidth(i);
 }

    // restore normal font
    dc.SetFont(m_normalFont);
}


void MyTreeCtrl::OnIdle(wxIdleEvent& event)  // // Makes sure any change in column width is reflected within
{
if (headerwindow==NULL) { event.Skip(); return; }   // Necessary as event may be called before there IS a headerwindow  

if (headerwindow->m_dirty)                          // Although MyTreeCtrl has an m_dirty too, that's just there because TreeListHeaderWindow writes to it!
  { headerwindow->m_dirty = false;                  // The one that is changed when columns are resized is the TreeListHeaderWindow one
    Refresh();
  }

event.Skip();                                       // Without this, gtk1 has focus problems; and in X11 the scrollbars get stuck in the top-left corner on creation!
}

void MyTreeCtrl::OnSize(wxSizeEvent& event)  // // Keeps the treectrl columns in step with the headerwindows
{
if (headerwindow) headerwindow->m_dirty = true;     // In >=2.5.1, the headerwindows don't otherwise seem to get refreshed on splitter dragging

AdjustMyScrollbars();

event.Skip();
}

void MyTreeCtrl::ScrollToOldPosition(const wxTreeItemId& item) // An altered ScrollTo() that tries as much as possible to maintain what the user had seen
{
wxCHECK_RET(!IsFrozen(), wxT("DoDirtyProcessing() won't work when frozen, at least in 2.9"));

    if (!item.IsOk()) return;
    if (m_dirty)
        DoDirtyProcessing();

    wxGenericTreeItem *gitem = (wxGenericTreeItem*) item.m_pItem;
    int item_y = gitem->GetY();

static const int PIXELS_PER_UNIT = 10;    
const int fiddlefactor = 1;
    int start_x = 0; int start_y = 0;
    GetViewStart( &start_x, &start_y );

    const int clientHeight = GetClientSize().y;
    const int itemHeight = GetLineHeight(gitem) + 2;

    if ( item_y + itemHeight > start_y*PIXELS_PER_UNIT + clientHeight )
    {
        // need to scroll up by enough to show this item fully
        item_y += itemHeight - clientHeight;
    }
/*    else if ( itemY > start_y*PIXELS_PER_UNIT )
    {
        // item is already fully visible, don't do anything
        return;
    }*/
    //else: scroll down to make this item the top one displayed

    Scroll(-1, (item_y/PIXELS_PER_UNIT) + fiddlefactor);
}

void MyTreeCtrl::AdjustMyScrollbars()  // // Taken from src/generic/treectlg.cpp, because of the headerwindow line
{
const int PIXELS_PER_UNIT = 10;  // // This was set in src/generic/treectlg.cpp

if (m_anchor)
    {
        int x = 0, y = 0;
        m_anchor->GetSize(x, y, this);
    if (headerwindow != NULL)  x = headerwindow->GetTotalPreferredWidth();  // // This means we get the column width, not just that of m_anchor's name

    y += PIXELS_PER_UNIT+2; // one more scrollbar unit + 2 pixels
      // //  x += PIXELS_PER_UNIT+2; // one more scrollbar unit + 2 pixels                This actually makes things worse
        int x_pos = GetScrollPos(wxHORIZONTAL);
        int y_pos = GetScrollPos(wxVERTICAL);
        SetScrollbars(PIXELS_PER_UNIT, PIXELS_PER_UNIT, x/PIXELS_PER_UNIT, y/PIXELS_PER_UNIT, x_pos, y_pos);
    }
 else
    {
        SetScrollbars(0, 0, 0, 0);
    }
}

void MyTreeCtrl::ScrollToMidScreen(const wxTreeItemId& item, const wxString& START_DIR) // Improved version of wxGenericTreeCtrl::ScrollTo (which seems to show the item at the top/bottom)
{
if (!item.IsOk())
  return;

// const int PIXELS_PER_UNIT = 10;  // // This was set in src/generic/treectlg.cpp but the method below is more future-proof
int xScrollUnits, PIXELS_PER_UNIT;
GetScrollPixelsPerUnit(&xScrollUnits, &PIXELS_PER_UNIT);


wxGenericTreeItem *gitem = (wxGenericTreeItem*) item.m_pItem;
int itemY = gitem->GetY();

int viewstart_x = 0;
int viewstart_y = 0;
GetViewStart( &viewstart_x, &viewstart_y );

const int clientHeight = GetClientSize().y;
int ScrollPageSize = GetScrollPageSize(wxVERTICAL);
const int itemHeight = GetLineHeight(gitem) + 2;

if ( itemY + itemHeight > viewstart_y*PIXELS_PER_UNIT + clientHeight )
{ // ** I've left this branch here but, for our use-case, it's never entered **
    // need to scroll down by enough to show this item fully
    itemY += itemHeight - (clientHeight/2);

    // because itemY below will be divided by PIXELS_PER_UNIT it may
    // be rounded down, with the result of the item still only being
    // partially visible, so make sure we are rounding up
    itemY += PIXELS_PER_UNIT - 1;

    Scroll(-1, (itemY - (clientHeight/2))/PIXELS_PER_UNIT);
}
/*else if ( itemY > viewstart_y*PIXELS_PER_UNIT )
{
    // item is already fully visible, don't do anything
   // return;
}*/

 else
  Scroll(-1, (viewstart_y + (ScrollPageSize/2)));
}


void MyTreeCtrl::OnScroll(wxScrollWinEvent& event)  // // Keeps the headerwindows in step with the treectrl colums
{
if (headerwindow != NULL) headerwindow->Refresh();
event.Skip();
}

#if !defined(__WXGTK3__)
void MyTreeCtrl::OnEraseBackground(wxEraseEvent& event)  // This is only needed in gtk2 under kde and brain-dead theme-manager, but can cause a blackout in some gtk3(themes?)
{
wxClientDC* clientDC = NULL;  // We may or may not get a valid dc from the event, so be prepared
if (!event.GetDC()) clientDC = new wxClientDC(this);
wxDC* dc = clientDC ? clientDC : event.GetDC();

wxColour colBg = GetBackgroundColour();  // Use the chosen background if there is one
if (!colBg.Ok()) colBg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);

dc->SetBackground(wxBrush(colBg, wxBRUSHSTYLE_SOLID));
dc->Clear();

if (clientDC) delete clientDC;
}
#endif

#if wxVERSION_NUMBER > 3102
void MyTreeCtrl::Expand(const wxTreeItemId& itemId)  // Needed in wx3.2 as otherwise non-fulltree dirview roots don't get children added. See wx git a6b92cb313 and https://trac.wxwidgets.org/ticket/13886
{
wxGenericTreeItem *item = (wxGenericTreeItem*)itemId.m_pItem;

wxCHECK_RET( item, wxT("invalid item in wxGenericTreeCtrl::Expand") );
wxCHECK_RET( !HasFlag(wxTR_HIDE_ROOT) || itemId != GetRootItem(),
              wxT("can't expand hidden root") );

if (!item->HasPlus())    return;

if (item->IsExpanded())  return;

wxTreeEvent event(wxEVT_TREE_ITEM_EXPANDING, this, item);
if (GetEventHandler()->ProcessEvent( event ) && !event.IsAllowed())    return;    // cancelled by program

item->Expand();
if (!IsFrozen())
  { CalculatePositions();
    RefreshSubtree(item);
  }
else m_dirty = true;  // frozen

event.SetEventType(wxEVT_TREE_ITEM_EXPANDED);
GetEventHandler()->ProcessEvent(event);
}

#endif

BEGIN_EVENT_TABLE(MyTreeCtrl,wxTreeCtrl)
    EVT_KEY_DOWN(MyTreeCtrl::AdjustForNumlock)
    EVT_SET_FOCUS(MyTreeCtrl::OnReceivingFocus)
    EVT_KILL_FOCUS(MyTreeCtrl::OnLosingFocus)
    EVT_MENU(SHCUT_SELECTALL,MyTreeCtrl::OnCtrlA)
    EVT_RIGHT_DOWN(MyTreeCtrl::OnRtDown)            // Just swallows the event, to stop it triggering an unwanted context menu in >GTK-2.5.3
    EVT_RIGHT_UP(MyTreeCtrl::OnRtUp)                // Takes over the context menu generation
    EVT_LEFT_DOWN(MyTreeCtrl::OnLtDown)
    EVT_MOTION(MyTreeCtrl::OnMouseMovement)
    EVT_MIDDLE_DOWN(MyTreeCtrl::OnMiddleDown)       // Allows middle-button paste
    EVT_LEAVE_WINDOW(MyTreeCtrl::OnLeavingWindow)
    EVT_PAINT          (MyTreeCtrl::OnPaint)
    #if !defined(__WXGTK3__)
      EVT_ERASE_BACKGROUND(MyTreeCtrl::OnEraseBackground)
    #endif
    EVT_IDLE(MyTreeCtrl::OnIdle)
    EVT_SIZE(MyTreeCtrl::OnSize)
    EVT_SCROLLWIN(MyTreeCtrl::OnScroll)
END_EVENT_TABLE()

//---------------------------------------------------------------------------

#include <wx/mimetype.h>
#include <wx/txtstrm.h>
#include <wx/wfstream.h>

wxTreeItemId PreviewManager::m_LastItem = wxTreeItemId();
PreviewManagerTimer* PreviewManager::m_Timer = NULL;
wxWindow* PreviewManager::m_Tree = NULL;
wxString PreviewManager::m_Filepath;
wxPoint PreviewManager::m_InitialPos = wxPoint();
PreviewPopup* PreviewManager::m_Popup = NULL;
size_t PreviewManager::m_DwellTime;
int PreviewManager::MAX_PREVIEW_IMAGE_HT;
int PreviewManager::MAX_PREVIEW_IMAGE_WT;
int PreviewManager::MAX_PREVIEW_TEXT_HT;
int PreviewManager::MAX_PREVIEW_TEXT_WT;

PreviewManager::~PreviewManager()
{
Clear();
delete m_Timer;  m_Timer = NULL;
}

//static
void PreviewManager::Init()
{
wxConfigBase* config = wxConfigBase::Get();

m_DwellTime = (size_t)config->Read(wxT("/Misc/Display/Preview/dwelltime"), 1000l);
MAX_PREVIEW_IMAGE_WT = (int)config->Read(wxT("/Misc/Display/Preview/MAX_PREVIEW_IMAGE_WT"), 100l);
MAX_PREVIEW_IMAGE_HT = (int)config->Read(wxT("/Misc/Display/Preview/MAX_PREVIEW_IMAGE_HT"), 100l);
MAX_PREVIEW_TEXT_WT = (int)config->Read(wxT("/Misc/Display/Preview/MAX_PREVIEW_TEXT_WT"), 300l);
MAX_PREVIEW_TEXT_HT = (int)config->Read(wxT("/Misc/Display/Preview/MAX_PREVIEW_TEXT_HT"), 300l);
}

//static
void PreviewManager::Save(int Iwt, int Iht, int Twt, int Tht, size_t dwell)
{
wxConfigBase* config = wxConfigBase::Get();

config->Write(wxT("/Misc/Display/Preview/dwelltime"), (long)dwell);
config->Write(wxT("/Misc/Display/Preview/MAX_PREVIEW_IMAGE_WT"), (long)Iwt);
config->Write(wxT("/Misc/Display/Preview/MAX_PREVIEW_IMAGE_HT"), (long)Iht);
config->Write(wxT("/Misc/Display/Preview/MAX_PREVIEW_TEXT_WT"), (long)Twt);
config->Write(wxT("/Misc/Display/Preview/MAX_PREVIEW_TEXT_HT"), (long)Tht);

Init(); // We get here when the user has changed the values; so reload to keep the vars current
}

//static
void PreviewManager::ProcessMouseMove(wxTreeItemId item, int flags, const wxString& filepath, wxMouseEvent& event)
{
if (item.IsOk() && !(flags & wxTREE_HITTEST_ONITEMRIGHT)) // Find out if we're near a tree item (& not in the space to the right of it)
  { if (item != m_LastItem)
      { Clear();
        m_LastItem = item; m_Tree = (wxWindow*)event.GetEventObject(); m_InitialPos = wxGetMousePosition();

        bool InArchive = false;                           // We might be trying to extract from an archive into this editor
        MyGenericDirCtrl* active = MyFrame::mainframe->GetActivePane();
        if (active != NULL && active->arcman != NULL && active->arcman->IsArchive())   InArchive = true;

        if (InArchive)
          { FakeFiledata ffd(filepath); if (!ffd.IsValid()) return;
            wxArrayString arr; arr.Add(filepath);
            wxArrayString tempfilepaths = active->arcman->ExtractToTempfile(arr, false, true);  // This extracts filepath to a temporary file, which it returns. The bool params mean not-dirsonly, files-only
            if (tempfilepaths.IsEmpty()) return;          // Empty == failed
            
            FileData tfp(tempfilepaths[0]); tfp.DoChmod(0444);  // Make the file readonly, as we won't be saving any alterations and exec.ing it is unlikely to be sensible
            m_Filepath = tempfilepaths[0];
          }
         else
          { FileData fd(filepath);
            if (fd.IsDir() || fd.IsSymlinktargetADir() || !fd.IsValid()) return;
            m_Filepath = fd.GetUltimateDestination();
          }

        if (!m_Timer) m_Timer = new PreviewManagerTimer();
        m_Timer->Start(m_DwellTime, true);
      }
  }

 else Clear();
}

//static
void PreviewManager::Clear()
{
if (m_Timer) m_Timer->Stop();
m_LastItem = wxTreeItemId();
m_Filepath.Clear();
m_InitialPos = wxPoint();

if (m_Popup) 
  { m_Popup->Dismiss();
    m_Popup->Destroy();
    m_Popup = NULL;
  }
}

//static
void PreviewManager::ClearIfNotInside()
{
if (!m_Popup) 
  return;

  // If we're here, it's because the mouse moved out of the treectrl. However we don't want to close if it's actually _inside_ the popup
wxPoint pt = wxGetMousePosition();
if (!m_Popup->GetRect().Contains(pt)) 
  Clear();
}

//static
void PreviewManager::OnTimer()
{
wxPoint pt = wxGetMousePosition();
if (pt.y - m_InitialPos.y > 30)
  { Clear(); return; } // We must originally have been called on the bottom item, and now we're some way below it. So abort

delete m_Popup;
m_Popup = new PreviewPopup(m_Tree);
if (!m_Popup->GetCanDisplay()) return; // Not an image/txtfile*

wxSize ps = m_Popup->GetPanelSize(); // Ensure the popup will fit on the screen
if (pt.y < ps.GetHeight()) pt.y = ps.GetHeight() + 2;
if (pt.x + ps.GetWidth() >= wxGetClientDisplayRect().GetWidth()) 
  pt.x -= (pt.x + ps.GetWidth() - wxGetClientDisplayRect().GetWidth() + 2);

pt.x += 1; // +1 to prevent very small bitmaps from touching the pointer and immediately deleting
m_Popup->Position(pt, wxSize(0, -ps.GetHeight()));

m_Popup->Popup(m_Tree);
m_Popup->SetSize(m_Popup->GetPanelSize()); // Otherwise it doesn't seem to know its size
}

PreviewPopup::PreviewPopup(wxWindow* parent) : wxPopupTransientWindow(parent), m_CanDisplay(false), m_Size(wxSize())
{
wxString filepath = PreviewManager::GetFilepath();
wxCHECK_RET(!filepath.empty(), wxT("PreviewManager has an empty filepath"));

if (IsText(filepath)) // Try for text first: *.c files seem to return true from wxImage::CanRead :/
  { DisplayText(filepath); m_CanDisplay = true; }
 else if (IsImage(filepath))
  { DisplayImage(filepath); m_CanDisplay = true; }

Bind(wxEVT_LEAVE_WINDOW, &PreviewPopup::OnLeavingWindow, this);
}

bool PreviewPopup::IsImage(const wxString& filepath)
{
wxCHECK_MSG(!filepath.empty(), false, wxT("An empty filepath passed to IsImage()"));
wxLogNull NoErrorMessages;

if (filepath.Right(4) == ".svg") return true;
return (wxImage::CanRead(filepath));
}

bool PreviewPopup::IsText(const wxString& filepath)
{
wxCHECK_MSG(!filepath.empty(), false, wxT("An empty filepath passed to IsText()"));
wxLogNull NoErrorMessages;
wxString mt, ext = filepath.AfterLast(wxT('.')), filename = filepath.AfterLast(wxT('/')).Lower();

  // Do some specials first: not recognised by wxTheMimeTypesManager, but can usefully be previewed
  // Do 'txt' too, as wxTheMimeTypesManager doesn't spot this in some distros :/
if (ext == wxT("txt") || ext == wxT("sh") || ext == wxT("xrc") || ext == wxT("in") || ext == wxT("bkl") || filename == wxT("makefile") || filename == wxT("readme"))
  return true;

wxFileType* ft = wxTheMimeTypesManager->GetFileTypeFromExtension(ext);
if (!ft) return false;

bool result(false);
if (ft->GetMimeType(&mt))
  { if (mt.StartsWith(wxT("text")))
      result = true;
  }

delete ft;
return result;
}

void PreviewPopup::DisplayImage(const wxString& fpath)
{
wxLogNull NoErrorMessages;
wxString filepath(fpath);
wxString pngfilepath;
wxImage image;

if (filepath.Right(4) == ".svg")
  { void* handle = wxGetApp().GetRsvgHandle();
    if (!handle) return; // Presumably librsvg is not available at present
    
    // Create a filepath in /tmp/ to store the .png
    wxFileName fn(filepath);
    pngfilepath = wxFileName::CreateTempFileName(fn.GetName());
   if (pngfilepath.empty())
       wxLogWarning("CreateTempFileName() failed in DisplayImage()");
    
  if (SvgToPng(filepath, pngfilepath, handle))
      image = wxImage(pngfilepath);
    wxRemoveFile(pngfilepath);
  }
 else
   image = wxImage(filepath);


if (!image.IsOk()) return;

int ht = wxMin(image.GetHeight(), PreviewManager::MAX_PREVIEW_IMAGE_HT);
int wt = wxMin(image.GetWidth(), PreviewManager::MAX_PREVIEW_IMAGE_WT);
if (ht != image.GetHeight() || wt != image.GetWidth())
  { if (image.GetHeight() != image.GetWidth())
      { bool wider = image.GetWidth() > image.GetHeight(); // Scale retaining the aspect ratio
        if (wider) ht = (int)(ht * ((float)image.GetHeight()/(float)image.GetWidth()));
         else      wt = (int)(wt * ((float)image.GetWidth()/(float)image.GetHeight()));
      }
  
    image.Rescale(wt, ht);
  }

wxPanel* panel = new wxPanel(this, wxID_ANY);
panel->SetBackgroundColour(*wxLIGHT_GREY);

wxStaticBitmap* bitmap = new wxStaticBitmap(panel, wxID_ANY, wxBitmap(image));

wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);
topSizer->Add(bitmap, 0);

panel->SetClientSize(wxSize(wt,ht));
panel->SetSizer(topSizer);

m_Size = wxSize(wt,ht);
}

void PreviewPopup::DisplayText(const wxString& filepath)
{
wxLogNull NoErrorMessages;
wxPanel* toppanel = new wxPanel(this, wxID_ANY);
toppanel->SetBackgroundColour(*wxLIGHT_GREY);

wxPanel* whitepanel = new wxPanel(toppanel, wxID_ANY);
whitepanel->SetBackgroundColour(*wxWHITE);

wxString previewstring;
wxFileInputStream fis(filepath);
if (!fis.IsOk()) return;
wxTextInputStream tis(fis);
for (size_t n=0; n < 30; ++n)
  { if (!fis.CanRead()) break;
    previewstring << tis.ReadLine() << wxT('\n');
  }

wxStaticText* text = new wxStaticText(whitepanel, wxID_ANY, previewstring);
wxBoxSizer* whitesizer = new wxBoxSizer(wxVERTICAL);
whitesizer->Add(text, 1, wxEXPAND);
whitepanel->SetSizer(whitesizer);
whitepanel->SetClientSize(PreviewManager::MAX_PREVIEW_TEXT_WT-4, PreviewManager::MAX_PREVIEW_TEXT_HT-4);

wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);
topSizer->Add(whitepanel, 1, wxEXPAND | wxALL, 2);

toppanel->SetSizer(topSizer);
toppanel->SetClientSize(wxSize(PreviewManager::MAX_PREVIEW_TEXT_WT,PreviewManager::MAX_PREVIEW_TEXT_HT));

m_Size = wxSize(PreviewManager::MAX_PREVIEW_TEXT_WT, PreviewManager::MAX_PREVIEW_TEXT_HT);
}

void PreviewPopup::OnLeavingWindow(wxMouseEvent& event)
{
PreviewManager::ClearIfNotInside();
}
