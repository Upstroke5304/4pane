/////////////////////////////////////////////////////////////////////////////
// Name:       MyTreeCtrl.h
// Purpose:    Tree and Treelistctrl.
// Derived:    The listctrl parts adapted from wxTreeListCtrl (c) Alberto Griggio, Otto Wyss
// See:        http://wxcode.sourceforge.net/components/treelistctrl/
// Part of:    4Pane
// Author:     David Hart
// Copyright:  (c) 2020 David Hart
// Licence:    wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef MYTREECTRL
#define MYTREECTRL

#include "wx/wx.h"

#include "wx/imaglist.h"
#include "wx/treectrl.h"
#include "wx/listctrl.h" // for wxListEvent


#include "Externs.h"  

class DirSplitterWindow;
class MyGenericDirCtrl;
class MyTab;  
class MyTreeCtrl;


#if wxVERSION_NUMBER > 3100
    // The 'Tree' of wxTreeItemAttr no longer exists
    #define wxTreeItemAttr wxItemAttr
#endif

//-----------------------------------------------------------------------------
// wxTreeListColumnAttrs  from wxTreeListCtrl
//-----------------------------------------------------------------------------

enum wxTreeListColumnAlign {
    wxTL_ALIGN_LEFT,
    wxTL_ALIGN_RIGHT,
    wxTL_ALIGN_CENTER
};

class wxTreeListColumnInfo: public wxObject {
public:
    static const size_t DEFAULT_COL_WIDTH = 100;
    wxTreeListColumnInfo(const wxChar* text = wxT(""), int image = -1, size_t width = DEFAULT_COL_WIDTH,
                                                                        wxTreeListColumnAlign alignment = wxTL_ALIGN_LEFT)
    {
      m_image = image;
      m_selected_image = -1;
      m_text = text;
      m_width = width;
      desired_width = width;                // // Save the preferred width, so that forced changes can be reversed where possible
      ishidden = (width==0);                // // Flag zero-width column as hidden
      m_alignment = alignment;
    }
    // getters
    wxTreeListColumnAlign GetAlignment() const { return m_alignment; }
    wxString GetText() const { return m_text; }
    int GetImage() const { return m_image; }
    int GetSelectedImage() const { return m_selected_image; }
    size_t GetWidth() const { return m_width; }
    size_t GetDesiredWidth() const { return desired_width; } // //
    bool IsHidden() const { return ishidden; }               // //
    // setters
    wxTreeListColumnInfo& SetAlignment(wxTreeListColumnAlign alignment){ m_alignment = alignment; return *this; }

    wxTreeListColumnInfo& SetText(const wxString& text){ m_text = text; return *this; }

    wxTreeListColumnInfo& SetImage(int image){ m_image = image; return *this; }

    wxTreeListColumnInfo& SetSelectedImage(int image){ m_selected_image = image; return *this; }    
    
    wxTreeListColumnInfo& SetWidth(size_t width){ m_width = width; ishidden = (width==0); return *this; }
    
    void SetDesiredWidth(size_t width) { desired_width = width; }  // //

private:
    wxTreeListColumnAlign m_alignment;
    wxString m_text;
    int m_image;
    int m_selected_image;
    size_t m_width;
    size_t desired_width;                  // // Holds the original width (or as user-amended). So if width is altered by a size event, we know to what to revert later
    bool ishidden;                         // // ishidden if col has zero width
};


#include <wx/dynarray.h>
WX_DECLARE_OBJARRAY(wxTreeListColumnInfo, wxArrayTreeListColumnInfo);

class  TreeListHeaderWindow : public wxWindow
{
protected:
    MyTreeCtrl    *m_owner;                // //
    wxCursor      *m_currentCursor;
    wxCursor      *m_resizeCursor;
    bool           m_isDragging;

    // column being resized
    int m_column;

    // divider line position in logical (unscrolled) coords
    int m_currentX;

    // minimal position beyond which the divider line can't be dragged in
    // logical coords
    int m_minX;

    wxArrayTreeListColumnInfo m_columns;

    // total width of the columns
    int m_total_col_width; 
enum columntype selectedcolumn;           // // Which col is 'selected' ie has the up/down icon to control sort-orientation    
bool reversesort;                         // // Are we reverse-sorting?

const wxColour* p_BackgroundColour;       // // Points to the currently appropriate colour for when this pane has focus

public:
    TreeListHeaderWindow();

    TreeListHeaderWindow(wxWindow *win,
                        wxWindowID id,
                        MyTreeCtrl *owner,                              // //
                        const wxPoint &pos = wxDefaultPosition,
                        const wxSize &size = wxDefaultSize,
                        long style = 0,
                        const wxString &name = wxT("wxtreelistctrlcolumntitles"));

    virtual ~TreeListHeaderWindow();
#if wxVERSION_NUMBER < 2900
    void DoDrawRect(wxDC *dc, int x, int y, int w, int h);
#endif
    void DrawCurrent();
    void AdjustDC(wxDC& dc);

    void OnPaint(wxPaintEvent &event);
    void OnMouse(wxMouseEvent &event);
    void OnSetFocus(wxFocusEvent &event);


    // columns manipulation

    size_t GetColumnCount() const { return m_columns.GetCount(); }
    
    void AddColumn(const wxTreeListColumnInfo& col);

    void InsertColumn(size_t before, const wxTreeListColumnInfo& col);

    void RemoveColumn(size_t column);
    
    void SetColumn(size_t column, const wxTreeListColumnInfo& info);
    
    const wxTreeListColumnInfo& GetColumn(size_t column) const
    {
      static wxTreeListColumnInfo tmp;
      wxCHECK_MSG(column < GetColumnCount(), tmp, wxT("Invalid column"));
      return m_columns[column];
    }
    wxTreeListColumnInfo& GetColumn(size_t column)
    {
      static wxTreeListColumnInfo tmp;
      wxCHECK_MSG(column < GetColumnCount(), tmp, wxT("Invalid column"));
      return m_columns[column];
    }

    void SetColumnWidth(size_t column, size_t width);
    
    void SetColumnText(size_t column, const wxString& text)
    {
      wxCHECK_RET(column < GetColumnCount(), wxT("Invalid column"));
      m_columns[column].SetText(text);
    }

    wxString GetColumnText(size_t column) const
    {
      wxCHECK_MSG(column < GetColumnCount(), wxT(""), wxT("Invalid column"));
      return m_columns[column].GetText();
    }
    
    int GetColumnWidth(size_t column) const
    {
      wxCHECK_MSG(column < GetColumnCount(), -1, wxT("Invalid column"));
      return m_columns[column].GetWidth();
    }
    
int GetColumnDesiredWidth(size_t column) const          // //  Gets the column's preferred width
  {
    wxCHECK_MSG(column < GetColumnCount(), -1, wxT("Invalid column"));
    return m_columns[column].GetDesiredWidth();
  }

bool IsHidden(size_t column) const { return m_columns[column].IsHidden(); }  // // Does the column have zero width?

    int GetWidth() const { return m_total_col_width; }
int GetTotalPreferredWidth();                        // // Similar, but without the extra amount added to the last column to fill the whole client-size

void SizeColumns();                                  // // Resize columns to give spare room to col 0
size_t GetMinimumSize();                             // // What is the smallest window that can fit the shown columns, bearing in mind their min sizes
int GetNextVisibleCol(int currentcolumn);            // // Finds the next column with non-zero width.  -1 flags failure
int GetLastVisibleCol();                             // // Finds the last column with non-zero width.  0 flags failure (but shouldn't happen!)
void ToggleColumn(int col);                          // // If it's hidden, show it & vice versa
void ShowColumn(int col, bool show = true);          // // Show (or hide) the relevant column
void SelectColumn(int col);                          // // Set this as the selected column, the one that controls Sorting
void SetSelectedColumnImage(bool order);             // // Sets the correct up/down image according to sort-order parameter
void ReverseSelectedColumn();
enum columntype GetSelectedColumn(){ return selectedcolumn; }  // // Returns which is the selected column, the one that controls Sorting
void SetSelectedColumn(size_t col);                  // // Sets the selected column, the one that controls Sorting
bool SetSortOrder(int order = -1);                   // // Set whether we're reverse-sorting
bool GetSortOrder() { return reversesort; }          // // Are we sorting or reverse-sorting?
void OnColumnSelectMenu(wxCommandEvent& event);      // // An event occurred to make (in)visible a column
void OnExtSortMethodChanged(wxCommandEvent& event);  // // An event occurred to change how the ext column defines an ext
void ShowIsFocused(bool focus);                      // // Toggles the header's colour to denote focus

    // needs refresh
    bool m_dirty;

private:
    // common part of all ctors
    void Init();

    void SendListEvent(wxEventType type, wxPoint pos, int column = -1);  // // I've added int column, as otherwise we have a problem when the event refers to hiding a col);
    void ShowContextMenu(wxContextMenuEvent& event);
  
    DECLARE_DYNAMIC_CLASS(TreeListHeaderWindow)
    DECLARE_EVENT_TABLE()
};

//---------------------------------------------------------------------------
#include <wx/popupwin.h>

class PreviewPopup: public wxPopupTransientWindow
{
public:
PreviewPopup(wxWindow *parent);

virtual bool Show( bool show = true )  // We must override wxPopupTransientWindow::Show, otherwise the mouse is permanently captured until the user L-clicks
  { return wxPopupWindow::Show( show ); }

wxSize GetPanelSize() const { return m_Size; }
bool GetCanDisplay() const { return m_CanDisplay; } // Returns true if there's a valid image/textfile to preview

protected:
void DisplayImage(const wxString& filepath);
void DisplayText(const wxString& filepath);
bool IsImage(const wxString& filepath);
bool IsText(const wxString& filepath);
void OnLeavingWindow(wxMouseEvent& event);

bool m_CanDisplay;
wxSize m_Size;
};

class PreviewManagerTimer;

class PreviewManager
{
public:
~PreviewManager();
static void Init();
static void Save(int Iwt, int Iht, int Twt, int Tht, size_t dwell);
static void ProcessMouseMove(wxTreeItemId item, int flags, const wxString& filepath, wxMouseEvent& event);
static void Clear();
static const wxString GetFilepath() { return m_Filepath; }
static void OnTimer();
static void ClearIfNotInside();

static int MAX_PREVIEW_IMAGE_HT;
static int MAX_PREVIEW_IMAGE_WT;
static int MAX_PREVIEW_TEXT_HT;
static int MAX_PREVIEW_TEXT_WT;
static size_t GetDwellTime() { return m_DwellTime; }

protected:
static wxTreeItemId m_LastItem;
static PreviewManagerTimer* m_Timer;
static wxWindow* m_Tree;
static wxString m_Filepath;
static wxPoint m_InitialPos;
static PreviewPopup* m_Popup;
static size_t m_DwellTime;
};

class PreviewManagerTimer : public wxTimer
{
public:
PreviewManagerTimer() : wxTimer() {}

protected:
void Notify(){ PreviewManager::OnTimer(); }
};

//---------------------------------------------------------------------------
class MyTreeCtrl : public wxTreeCtrl  
{
public:
MyTreeCtrl(wxWindow *parentwin, wxWindowID id = -1,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               long style = wxTR_DEFAULT_STYLE,
               const wxValidator &validator = wxDefaultValidator,
               const wxString& name = wxString(wxT("MyTreeCtrl")));

~MyTreeCtrl(){ delete HeaderimageList; }

void OnEndDrag(wxTreeEvent& event);
MyGenericDirCtrl* GetParent(){ return parent; }
bool QueryIgnoreRtUp();
void RecreateAcceleratorTable();                         // When shortcuts are reconfigured, deletes old table, then calls CreateAcceleratorTable()
void ResetPreDnD(){ dragging = false; startpt.x = startpt.y = 0; }// Cancel the 'Should we DnD?' sequence

bool m_dirty;
TreeListHeaderWindow* headerwindow;
wxImageList* HeaderimageList;                           // This holds the down & up symbols for the header of the selected fileview column
void ScrollToOldPosition(const wxTreeItemId& item);     // An altered ScrollTo() that tries as much as possible to maintain what the user had seen
void ScrollToMidScreen(const wxTreeItemId& item, const wxString& START_DIR); // Try to show 'item' in the middle of the screen
void AdjustMyScrollbars();
void OnMouseMovement(wxMouseEvent& event);

void CallCalculateLineHeight() { CalculateLineHeight(); } // Relay to generic treectrl protected function
#if wxVERSION_NUMBER > 3102
  virtual void Expand(const wxTreeItemId& item) wxOVERRIDE; // Needed in wx3.2 as otherwise non-fulltree dirview roots don't get children added. See wx git a6b92cb313 and https://trac.wxwidgets.org/ticket/13886
#endif

protected:
void CreateAcceleratorTable();
void OnCtrlA(wxCommandEvent& event);
void AdjustForNumlock(wxKeyEvent& event);
void OnRtDown(wxMouseEvent& event){}                    // Just swallows the event, to stop it triggering an unwanted context menu in >GTK-2.5.3
void OnRtUp(wxMouseEvent& event);                       // Takes over the context menu generation
void OnLtDown(wxMouseEvent& event);                     // Set-up starting DnD pre-processing
void OnBeginDrag(wxMouseEvent& event);
void OnMiddleDown(wxMouseEvent& event);                 // Allows middle-button paste
void OnReceivingFocus(wxFocusEvent& event);             // This just tells grandparent window which of the 2 dirctrls is the one to consider active
void OnLosingFocus(wxFocusEvent& event);                // This tells the headerwindow to unhighlight itself
void OnLeavingWindow(wxMouseEvent& event){ ResetPreDnD(); PreviewManager::ClearIfNotInside(); }  // If we leave this window before dragging 'catches', reset so that another window doesn't reuse the data
void PaintLevel(wxGenericTreeItem *item, wxDC &dc, int level, int &y, int index=0);  // I've added the index parameter
void PaintItem(wxGenericTreeItem *item, wxDC& dc, int index=0);                        // I've added the index parameter
void OnPaint(wxPaintEvent& event);
#if !defined(__WXGTK3__)
  void OnEraseBackground(wxEraseEvent& event);
#endif
void OnIdle(wxIdleEvent& event);
void OnSize(wxSizeEvent& event);                        // Keeps the headerwindows in step with the treectrl colums
void OnScroll(wxScrollWinEvent& event);                 // Keeps the headerwindows in step with the treectrl colums

wxColour Col0, Col1;                                    // Used when drawing a fileview in stripes
bool IgnoreRtUp;
bool dragging;
wxPoint startpt;                                        // Holds the initial position of a drag-event, so we can delay starting too soon
MyGenericDirCtrl* parent;

    DECLARE_EVENT_TABLE()
};

#endif
    // MYTREECTRL
