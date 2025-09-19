/////////////////////////////////////////////////////////////////////////////
// Name:       Redo.cpp
// Purpose:    Undo-redo and manager
// Part of:    4Pane
// Author:     David Hart
// Copyright:  (c) 2020 David Hart
// Licence:    wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/log.h"
#include "wx/app.h"
#include "wx/frame.h" 
#include "wx/scrolwin.h"
#include "wx/menu.h"
#include "wx/dirctrl.h"
#include "wx/stdpaths.h"

#include "Externs.h"
#include "MyDirs.h"
#include "MyGenericDirCtrl.h"
#include "Devices.h"
#include "MyFrame.h"
#include "Filetypes.h"
#include "Archive.h"
#include "Redo.h"
#include "Misc.h"

//--------------------------------------------------------------------------------------------------------------------------------------------------------------
                                  // This is the pop-down menu used for Previously-Visited Directories
MyPopupMenu::MyPopupMenu(DirGenericDirCtrl* dad, wxPoint& startpt, bool backwards, bool right)
      :  wxWindow((wxWindow*)dad, -1, wxDefaultPosition, wxSize(0,0)),    location(startpt), previous(backwards), isright(right), parent(dad),  FIRST_ID(6000)
{ 
arrayno = parent->GetNavigationManager().GetCount();
count = parent->GetNavigationManager().GetCurrentIndex();

    // The dir-control font-size may have been changed eg in gtk2.  We need to use the standard size here, as the pop-up menu certainly will, & otherwise GetTextExtent will be wrong
SetFont(wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT));

ShowMenu(); 
}

void MyPopupMenu::ShowMenu() // Prepares & displays a pop-up menu
{
int x,y, extent=0;
int fiddlefactor=15;                                 // Guesstimate the width of the menu border

if (!LoadArray())   return;                          // If loading array fails, depart forthwith

wxMenu menu;


size_t no = entries.GetCount();                      // How many items made it to the local array?
for (size_t c = 0; c < no; ++c)                      // Load each into the menu, together with a unique id
  { menu.Append(FIRST_ID + c, entries[c]);
    if (isright)                                     // If this is the righthand dirview, we want the menu to be to the left of the button, not the right
      { GetTextExtent(entries[c], &x, &y);           //  So make a valiant attempt to estimate the relevant offset by measuring the string
        extent = wxMax(extent, x);
      }
  }
if (isright) extent += fiddlefactor;                 // Add the guesstimated border width

PopupMenu(&menu,location.x - extent, location.y);    // Now show the menu.  If a lefthand dirview, extent will be 0
}


void MyPopupMenu::GetAnswer(wxCommandEvent& event)   // Gets the selection from the selection event and returns it
{
int id = event.GetId() - FIRST_ID;                        // Find which entry was selected
if (id < 0 || (size_t)id >= entries.GetCount())  return;  // In case of rubbish, bail out

parent->RetrieveEntry(idarray[ id ]);                     // Decode selection's index within calling array, & pass this to RetrieveEntry to do the work
}

bool MyPopupMenu::LoadArray()        // Facilitates display of revisitable dirs by copying them into member array
{
if (previous && !count) return false;                // Not a lot to do, as going backwards & count is already 0
if (!previous && (count+1)>=arrayno) return false;   // Not a lot to do, as going forwards & count is already indexing end of array

size_t items;
if (previous)
  { items = (count > MAX_DROPDOWN_DISPLAY ? MAX_DROPDOWN_DISPLAY : count);  // Don't display more than max permitted no of dirs
    for (size_t c = count-1, n=0; n < items; --c, ++n)
      { entries.Add(parent->GetNavigationManager().GetDir(c));        // Copy dir into our array. Note reverse order
        idarray.Add(-(1 + n));                       //  & save it's displacement from count.  This is a LOT easier than decoding it later
      }
  }
 else  
  { items = (arrayno - (count+1) > MAX_DROPDOWN_DISPLAY ? MAX_DROPDOWN_DISPLAY : arrayno - (count+1));    // Ditto for forward
    for (size_t c = count+1, n=0; n < items; ++c, ++n)
      { entries.Add(parent->GetNavigationManager().GetDir(c));
        idarray.Add(n+1);
      }
  }

return true;
}


BEGIN_EVENT_TABLE(MyPopupMenu,wxWindow)
EVT_MENU(-1, MyPopupMenu::GetAnswer)
END_EVENT_TABLE()

//--------------------------------------------------------------------------------------------------------------------------------------------------------------



SidebarPopupMenu::SidebarPopupMenu(wxWindow* dad, wxPoint& startpt, wxArrayString& names, int& ans)
  :  wxWindow(dad, -1, wxDefaultPosition, wxSize(0,0)),    location(startpt), answer(ans), unredonames(names), parent(dad), FIRST_ID(6000)
{ 
arrayno = unredonames.GetCount();
count = 0;

ShowMenu();
}

void SidebarPopupMenu::ShowMenu()  // Prepares & displays a pop-up menu
{
wxMenu menu;

for (size_t c = 0; c < arrayno; ++c)            // Load each item into the menu, together with a unique id
  menu.Append(FIRST_ID + c, unredonames[c]);

PopupMenu(&menu,location.x, location.y);        // Now show the menu.  The event macro will make us resume in GetAnswer
}


void SidebarPopupMenu::GetAnswer(wxCommandEvent& event)  // Gets the selection from the selection event and returns it
{
int id = event.GetId() - FIRST_ID;              // Find which entry was selected

if (id < 0 || (size_t)id >= arrayno)  return ;

answer =  id;                                   // answer is a reference, so doing this makes result available to caller
}

BEGIN_EVENT_TABLE(SidebarPopupMenu,wxWindow)
EVT_MENU(-1, SidebarPopupMenu::GetAnswer)
END_EVENT_TABLE()

//--------------------------------------------------------------------------------------------------------------------------------------------------------------

ArrayOfUnRedos UnRedoManager::UnRedoArray;  // Initialise static data members
size_t UnRedoManager::m_count=0;
size_t UnRedoManager::cluster=0;
size_t UnRedoManager::currentcluster=0;
bool UnRedoManager::ClusterIsOpen=false;
bool UnRedoManager::Supercluster=false;
size_t UnRedoManager::currentSupercluster = (uint)-1;
UnRedoImplementer UnRedoManager::m_implementer;
MyFrame* UnRedoManager::frame;

//static
void UnRedoManager::ClearUnRedoArray()
{
int arrcount = (int)UnRedoArray.GetCount();         // Get the real size of array (can't use count, as that is altered by undos)
for (int n = arrcount;  n > 0; --n)                 // Delete all outstanding unredo objects pointed to by array
  { UnRedo* item = UnRedoArray[n-1]; delete item; 
    UnRedoArray.RemoveAt(n-1);
  }

m_count = 0;
}



//static
bool UnRedoManager::StartClusterIfNeeded()    // Start a new UnRedoManager cluster if none was open. Returns whether it was needed
{
if (ClusterIsOpen) return false;

StartCluster();
return true;
}

//static
void UnRedoManager::StartCluster() // Start a new UnRedoManager cluster.  The cluster will autoincrement with each AddEntry
{
/*if (ClusterIsOpen) 
  { wxLogError(_("Oops, we're trying to open a new cluster when one is already open!?")); } // which should never ever happen*/

currentcluster = cluster++;        // Get next available cluster-code.  Inc cluster: if this cluster aborts, who cares --- there are plenty more ints where this came from!
ClusterIsOpen = true;
MyGenericDirCtrl::Clustering = true;
}

//static 
void UnRedoManager::EndCluster()  // Closes the currently-open cluster
{
/*if (!ClusterIsOpen) 
  { wxLogError(_("Oops, we're trying to close a cluster when none is open!?")); } // which should never ever happen either*/

ClusterIsOpen = false;        // There isn't really anything to do, except reset the flag
frame->UpdateTrees();         //   and release the Updating of the trees
}

//static
int UnRedoManager::StartSuperCluster()  // Start a Supercluster.  Used eg for Cut/Paste combo, where we want undo to do both at once
{
StartCluster();
if (ClusterIsOpen == true)  Supercluster=true;    // Assuming that worked (& it's difficult for it not to!), set the Supercluster flag
currentSupercluster = currentcluster;
return currentSupercluster;
}

//static
void UnRedoManager::ContinueSuperCluster()  // Continue a Supercluster.  eg 1 or subsequent Pastes following a cut
{ // The next line is currently the ONLY check on whether we should continue.  This may change in future, if there are >1 sort of super-pairs & they get confused.
if (Supercluster==true && currentSupercluster==currentcluster)   // Check we're within a supercluster, & there've been no intervening entries
      ClusterIsOpen = true;                     // If that's the case, reopen ClusterIsOpen without inc.ing currentcluster
  else 
    { Supercluster=false;  StartCluster(); }    // If it's not, close any open Supercluster & make a standard start
}

//static
void UnRedoManager::CloseSuperCluster()  // Close a Supercluster.  Called eg by Copy, so that a Cut, then an Copy, then a Paste, won't be aggregated
{
Supercluster=false;
}

//static
bool UnRedoManager::AddEntry(UnRedo *newentry)  // Adds a new undoable action to the array
{
if (!ClusterIsOpen) { //wxLogError(_("Oops, we're trying to add to a cluster when none is open!?")); // which also should never ever happen
                      StartCluster(); // Start one anyway, otherwise we'll always get the wxLogError
                    }

if (newentry==NULL) return false;                 // Some implausible problem occurred
if (m_count == MAX_NUMBER_OF_UNDOS)                 // Check to see if too many entries.  It doesn't matter if count < arraysize, as then we'd be deleting later entries anyway
  { size_t oldcluster = UnRedoArray[0]->clusterno;  // Oops.  Do a FIFO by removing oldest cluster of entries
    do
      { UnRedo* item = UnRedoArray[0]; 
        UnRedoArray.RemoveAt(0); --m_count;         // Remove its pointer from the bottom of the array, so shifting the others down
        delete item;                              // Delete the item
      }
     while (m_count && UnRedoArray[0]->clusterno==oldcluster);  // Carry on deleting until we've removed the whole cluster (but check count 1st in case there IS only 1 big cluster!)
  }
      // Check that there haven't already been some undos.  If so, adding another undo invalidates any redos, so delete them
size_t arrno = UnRedoArray.GetCount();            // Get the real size of array
if (m_count < arrno)                                // If it's bigger than count, there must be redundant entries
  { for (int n = (int)arrno-1; n >= (int)m_count; --n)
      { UnRedo* item = UnRedoArray[n]; delete item; UnRedoArray.RemoveAt(n); }  // Delete any entries >= count
  }                                               // This time we don't care about clusters: we must've been at a cluster start anyway, & we're deleting the rest

UnRedoArray.Add(newentry);                        // Append new entry.  NB: Pointer.  It will need to be deleted in destructor when the app exits
UnRedoArray[m_count]->clusterno = currentcluster;   // Tell the entry to which cluster it belongs
UnRedoArray[m_count]->UndoPossible = true;          // Set the flag to say there's valid data for a future undo

++m_count;                                          // Finally inc the count
return true;  
}

//static
void UnRedoManager::LoadArray(wxArrayString& namearray, int& index, bool redo)    // From Sidebars, loads a clutch of clusters from main array into temp one
{
wxString prefix;
if (redo) prefix = _("  Redo:  "); else prefix = _("  Undo:  ");
int IncDec = (redo? 1 : -1);                      // If redoing, IncDec is 1 so increments. Otherwise decs
int arrno = UnRedoArray.GetCount();               // Get the size of the real array

                    // We start with index indexing the beginning of the 1st cluster to deal with
for (size_t n=0; n < MAX_DROPDOWN_DISPLAY; n++)
  { if ((redo && (index >=arrno))  
      || (!redo && (index < 0))) return;          // No more to do
    
    UnRedo* ptr = UnRedoArray[index];             // Get ptr to the first undo of this cluster
    namearray.Add(prefix + ptr->clustername + wxT("  "));  // Add its name to the array

                        // Now we skip over the rest of the cluster
    size_t thiscluster = ptr->clusterno;          // Get the id of the cluster
    size_t no=0; wxString extra;
    while (UnRedoArray[index]->clusterno == thiscluster)            // While clusterno matches (& it must at least once!)
      { if  (UnRedoArray[index]->clustername == ptr->clustername)   // Only do this if the name matches, so that we don't double-count eg Cut & Paste
          { if  (++no > 1) 
              { extra.Printf(_("  (%zu Items)  "), no);             //  If 2nd item or more,  add to standard label the no of cluster items
  // If we're Undoing, use the 1st clustername we came to, plus the itemno-to-date.  That will be the important one, eg "Paste" in a series of "Paste/Delete" twins
                if (!redo)  namearray.Last() = prefix + ptr->clustername + extra;
              }
          }
        if (redo)  // If Redoing, use the current clustername, +/- the itemno-to-date.  The last time we do this, we'll catch the important one, eg "Paste" in a series of "Paste/Delete" twins
          { if (no > 1)  namearray.Last() = prefix + UnRedoArray[index]->clustername + extra;
              else       namearray.Last() = prefix + UnRedoArray[index]->clustername + wxT("  ");
          }
        index += IncDec;                          // Inc or Dec index, depending on whether we're undoing or redoing
                  // Being neurotic, again check for out of bounds, though we should be safe unless the cluster system has broken
        if ((redo && (index >=arrno)) || (!redo && (index < 0)))    return;
      }
  }
}

//static
void UnRedoManager::OnUndoSidebar()    // Implements Undoing of multiple clusters
{
if (!m_count) return;

wxString more(_("  ---- MORE ----  ")), previous(_("  -- PREVIOUS --  "));
wxArrayString undonames;                        // Hold the strings to be listed on the pop-up menu
int index = m_count-1, NumberLoaded;              // index->count-1 as count points to vacant slot or next redo
int offset = 0;                                 // This copes with displays of >1 page.  answer is returned mod current page, so offset stores the extra

    // We need to pop-up the sidebar menu below the Undo button.  WxWidgets doesn't let us locate this, & the mouse pos could be anywhere within the button.
wxPoint pt;                     // The solution is to cheat, & place it where calcs say it ought to be.  This may fail on future resizes etc
int ht = MyFrame::mainframe->toolbar->GetSize().GetHeight() + 2;
pt.y = ht;                                      // The y pos could be anywhere within the button, so replace with the known toolbar-height, plus a fiddle-factor
pt.x = (MyFrame::mainframe->buttonsbeforesidebar * ht) + (MyFrame::mainframe->separatorsbeforesidebar * 7); // ht is a good proxy for the width; & we counted the tools on insertion

do
  {
    undonames.Empty();                          // In case this isn't the 1st iteration
    if (offset > 0)                             // If there's a previous batch, add something to click on to get to it
      undonames.Add(previous);  

    NumberLoaded = undonames.GetCount();        // May be used in Previous below.  Currently 0 or 1
    LoadArray(undonames, index, false);         // This loads a batch of display-names into the array. Note that index is passed as a reference, so will be altered
    NumberLoaded = undonames.GetCount() - NumberLoaded;  // Will now hold the no loaded this loop

    if (index >= 0)                             // If there're more to come, add something to click on to get to them
      undonames.Add(more);  

    int answer = -1;                            // This is where the answer will appear.  Give it a daft value so that we'll recognise a true entry
    SidebarPopupMenu popup(MyFrame::mainframe, pt, undonames, answer);  // Now do the pop-up menu

    if (answer == -1) return;                   // Now let's try & make sense of the answer.  If it hasn't changed, ESC was pressed so go home
    if (undonames[ answer ] == previous)        // Go into reverse
      { index += (NumberLoaded + MAX_DROPDOWN_DISPLAY);  // Remove the number just loaded, plus another page-worth
        index = wxMin(index, ((int)m_count)-1); //   but ensure we don't overdo it
        offset -= MAX_DROPDOWN_DISPLAY;         // Similarly deal with offset
        offset = wxMax(offset, 0);
        continue;
      }
    if (undonames[ answer ] == more)            // If more requested, just loop again
      { offset += MAX_DROPDOWN_DISPLAY; continue; } // after incrementing offset so that we'll know what page we're on

    if (answer > (int)MAX_DROPDOWN_DISPLAY) return;  // Just to avoid rubbish

                      // If we're here, congratulations: there's a valid answer
    if (undonames[0] == previous) --answer;     // If the "Previous" string was displayed, this will occupy slot 1 in the menu, so dec answer to compensate
    UndoEntry(offset + answer+1);               // Do the Undo.   The +1 is because undonames[0] is the first undo, represented by answer==1
    return;
  } while (1);                                  // The only escape is by returning
}

//static
void UnRedoManager::UndoEntry(int clusters) // Undoes 'clusters' of clusters of stored actions
{
if (m_implementer.IsActive()) return;

  // We need to pass the unredos of each cluster separately to UnRedoImplementer as we must unredo a Move of foo, bar and baz together
  // otherwise a Redo of the move followed by e.g. a rename of foo is likely to break when foo is large: foo won't have finshed moving when the rename happens
wxArrayInt clusterarr;
size_t first = m_count -1;
for (int n=0; n < clusters; ++n)                // For every cluster-to-be-undone
  { if (!m_count) break;
    size_t count(0);
    
    UnRedo* ptr = UnRedoArray[m_count-1];       // Retrieve ptr to the first undo of this cluster
    size_t thisclusterno = ptr->clusterno;        
    wxString thisclustername = ptr->clustername;
    while (ptr->clusterno == thisclusterno)     // While clusterno matches (& it must at least once!) dec the counts
      { if (!ptr->clustername.IsSameAs(thisclustername)) // But don't if the name changed; that would mean a Cut/Paste move, which must be redone separately & it costs nothing to Undo it separately too, just in case
          { --n; break; } // so break the while loop, but dec the counter so this clusterno will be continued without causing the last cluster to be missed
        --m_count; ++count;
        if (!m_count)  break;                   // Check we haven't come to the end of the first cluster in the array
        ptr = UnRedoArray[m_count-1];
      }
    
    clusterarr.Add(count);                      // Store the number of items in this cluster
  }

if (clusterarr.GetCount())
  { MyGenericDirCtrl::Clustering = true;
    m_implementer.DoUnredos(UnRedoArray, first, clusterarr, true);
  }
}

//static
void UnRedoManager::OnRedoSidebar()    // Implements Redoing of multiple clusters
{
size_t arrno = UnRedoArray.GetCount();      // Get the size of the array
if (arrno==m_count) return;                   // None to redo

wxString more(_("  ---- MORE ----  ")), previous(_("  -- PREVIOUS --  "));
wxArrayString redonames;                    // Hold the strings to be listed on the pop-up menu
int index = m_count, NumberLoaded;            // count points to the 1st item to be redone
int offset = 0;                             // This copes with displays of >1 page.  answer is returned mod current page, so offset stores the extra

    // We need to pop-up the sidebar menu below the Redo button.  WxWidgets doesn't let us locate this, & the mouse pos could be anywhere within the button.
wxPoint pt;                     // The solution is to cheat, & place it where calcs say it ought to be.  This may fail on future resizes etc
int ht = MyFrame::mainframe->toolbar->GetSize().GetHeight() + 2;
pt.y = ht;                                  // The y pos could be anywhere within the button, so replace with the known toolbar-height, plus a fiddle-factor
pt.x = (MyFrame::mainframe->buttonsbeforesidebar * ht) + ht + 13  // (The extra 'ht' is for the Undo button, the 13 for the Undo sidebar buttton)
          + (MyFrame::mainframe->separatorsbeforesidebar* 7);     // ht is a good proxy for the width; & we counted the other tools on insertion

do
  {
    redonames.Empty();                        // In case this isn't the 1st iteration
    if (offset > 0)                           // If there's a previous batch, add something to click on to get to it
      redonames.Add(previous);  
    NumberLoaded = redonames.GetCount();      // May be used in Previous below.  Currently 0 or 1

    LoadArray(redonames, index, true);        // This loads a batch of display-names into the array. Note that index is passed as a reference, so will be altered
    NumberLoaded = redonames.GetCount() - NumberLoaded;  // Will now hold the no loaded this loop

    if (index < (int)arrno)                   // If there're more to come, add something to click on to get to them
      redonames.Add(more);  

    int answer = -1;                          // This is where the answer will appear.  Give it a daft value so that we'll recognise a true entry
    SidebarPopupMenu popup(MyFrame::mainframe, pt, redonames, answer);  // Now do the pop-up menu

    if (answer == -1) return;                 // Now let's try & make sense of the answer.  If it hasn't changed, ESC was pressed so go home
    if (redonames[ answer ] == previous)      // Go into reverse
      { index -= (NumberLoaded + MAX_DROPDOWN_DISPLAY);  // Remove the no last displayed, plus another page-worth
        index = wxMax(index, (int)m_count);   //   but ensure we don't overdo it
        offset -= MAX_DROPDOWN_DISPLAY;       // Similarly deal with offset
        offset = wxMax(offset, 0);
        continue;
      }
    if (redonames[ answer ] == more)          // If more requested, just loop again
      { offset += MAX_DROPDOWN_DISPLAY; continue; }  // after incrementing offset so that we'll know what page we're on

    if (answer > (int)MAX_DROPDOWN_DISPLAY) return;  // Just to avoid rubbish

                      // If we're here, congratulations: there's a valid answer
    if (redonames[0] == previous) --answer;   // If the "Previous" string was displayed, this will occupy slot 1 in the menu, so dec answer to compensate
    RedoEntry(offset + answer+1);             // Do the Redo.   The +1 is because redonames[0] is the first redo, represented by answer==1
    return;
  } while (1);                                // The only escape is by returning
}

//static
void UnRedoManager::RedoEntry(int clusters)   // Redoes 'clusters' of clusters of stored actions
{
if (m_implementer.IsActive()) return;

size_t arraysize = UnRedoArray.GetCount();
if (m_count >= arraysize)  return;            // If count < array size, there are items available to be redone.

size_t first = m_count;
wxArrayInt clusterarr;
for (int n=0; n < clusters; ++n)
  { if (m_count == arraysize)  break;         // No more to redo
    size_t count(0);
    UnRedo* ptr = UnRedoArray[m_count];       // Retrieve ptr to the highest redoable item
    size_t thisclusterno = ptr->clusterno;
    wxString thisclustername = ptr->clustername;

    while (ptr->clusterno == thisclusterno)           // While clusterno matches, inc the counts
      { if (!ptr->clustername.IsSameAs(thisclustername)) // Except don't if the name has changed; that would mean a Cut/Paste move, which must be redone separately
          { --n; break; } // so break the while loop, but dec the counter so this clusterno will be continued without causing the last cluster to be missed
        ++count;
        ++m_count;
        if (m_count == arraysize)  break;     // Check we haven't come to the end of the last cluster in the array
        ptr = UnRedoArray[m_count];
      }
    
    clusterarr.Add(count);
  }

if (clusterarr.GetCount())
  { MyGenericDirCtrl::Clustering = true;
    m_implementer.DoUnredos(UnRedoArray, first, clusterarr, false);
  }
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------

bool UnRedoImplementer::DoUnredos(ArrayOfUnRedos& urarr, size_t first, const wxArrayInt& countarr, bool undoing)
{
wxCHECK_MSG(countarr.GetCount() > 0, false, wxT("Passed an empty count array")); 

m_UnRedoArray = &urarr; m_NextItem = first; m_CountArray = countarr; m_CountarrayIndex = 0;
m_IsUndoing = undoing; m_IsActive = true;

m_Successcount = 0;
StartItems();
return true;
}

void UnRedoImplementer::StartItems()
{
wxCHECK_RET(IsActive(), wxT("Trying to unredo when we're not active"));
wxCHECK_RET(m_NextItem < m_UnRedoArray->GetCount() && ((int)m_NextItem >= 0), wxT("Passed an invalid unredo index")); 

  // We are here to do a single UnRedo cluster e.g. if 3 selected items were deleted, m_CountArray[m_CountarrayIndex] will be 3; if 1 item was renamed, 1
  // These can all safely be UnRedone at once, even in a thread situation (whereas 2 clusters containing e.g. a Move followed by a rename can't be threaded together safely for timing reasons
size_t count = m_CountArray[m_CountarrayIndex];
wxCHECK_RET(count > 0, wxT("Passed an unredo with a zero count of items"));

bool ThreadUsed(false); // This will be set if we actually use a thread below. It lets us start threads if any exist, or clean up if none was needed

  // Before we start the loop, do a one-off sort out of the PasteThreadSuperBlock if this is a Move or Paste
PasteThreadSuperBlock* tsb = NULL;
UnRedo* ptr = (*m_UnRedoArray)[m_NextItem];
if (dynamic_cast<UnRedoPaste*>(ptr))
  { tsb = (PasteThreadSuperBlock*)ThreadsManager::Get().StartSuperblock(wxT("paste"));
    dynamic_cast<UnRedoPaste*>(ptr)->SetThreadSuperblock(tsb);
  }
 else if (dynamic_cast<UnRedoMove*>(ptr))
  { tsb = (PasteThreadSuperBlock*)ThreadsManager::Get().StartSuperblock(wxT("move"));
    dynamic_cast<UnRedoMove*>(ptr)->SetThreadSuperblock(tsb);
  }
if (tsb)
  tsb->SetFromUnRedo(ptr->IsUndoing() ? URT_undo : URT_redo);

  // Now go through all of this cluster's items (often that'll only be 1)
for (size_t n=0; n < count; ++n)
  { wxCHECK_RET(m_NextItem != size_t(-1), wxT("Probably overran the count"));
    UnRedo* ptr = (*m_UnRedoArray)[m_NextItem];
    wxCHECK_RET(ptr, wxT("Passed an invalid unredo"));
    if (dynamic_cast<UnRedoPaste*>(ptr))
      { dynamic_cast<UnRedoPaste*>(ptr)->SetThreadSuperblock(tsb);
        if (!m_IsUndoing) ThreadUsed = true; // Redo paste should start a thread
      }
     else if (dynamic_cast<UnRedoMove*>(ptr))
      { dynamic_cast<UnRedoMove*>(ptr)->SetThreadSuperblock(tsb);
        ThreadUsed = true; // Move always starts a thread
      }

    bool result;
    if (m_IsUndoing)
      result = ptr->Undo();
     else
      result = ptr->Redo();
    if (!result)
      { OnAllItemsCompleted(false);
        return; 
      }
    ptr->ToggleFlags();

    // The following line sets ThreadUsed for a paste-undo that doesn't have to retrieve a previously-overwritten file
    if (tsb && !ThreadUsed && !dynamic_cast<UnRedoFile*>(ptr)->GetOverwrittenFile().empty())
      ThreadUsed = true;

    GetNextItem();
  }

if (tsb)
  { tsb->SetMessageType(m_IsUndoing ? _("undone") : _("redone"));
    tsb->SetUnRedoId(m_CountarrayIndex);
    tsb->StartThreads();

    if (!ThreadUsed)
      ThreadsManager::Get().AbortThreadSuperblock(tsb); // None of the unredos needed a thread so abort; otherwise it'll never be removed
  }
 else
  OnItemCompleted(m_CountarrayIndex); // If we're not using threads the unredo will already have completed 
}

void UnRedoImplementer::OnItemCompleted(size_t item)
{
wxASSERT(item == m_CountarrayIndex);

++m_Successcount;
++m_CountarrayIndex;
if (!IsCompleted()) 
  StartItems();
 else
  OnAllItemsCompleted();
}

bool UnRedoImplementer::IsCompleted() const
{
if (!IsActive() || !m_UnRedoArray || m_CountArray.IsEmpty()) return true;

return (m_CountarrayIndex >= m_CountArray.GetCount()); 
}

size_t UnRedoImplementer::GetNextItem()
{
if (m_IsUndoing)
  { if ((int)m_NextItem <= 0) 
      return size_t(-1); 
    return --m_NextItem;
  }

if (m_NextItem+1 >= m_UnRedoArray->GetCount())
  return size_t(-1);

return ++m_NextItem;
}

void UnRedoImplementer::OnAllItemsCompleted(bool successfully/*=true*/) 
{
m_IsActive = false; m_UnRedoArray = NULL;

if (UnRedoManager::ClusterIsOpen) UnRedoManager::EndCluster();
MyFrame::mainframe->UpdateTrees();

wxString msg = m_Successcount > 1 ? _(" actions ") : _(" action ");
wxString type = m_IsUndoing ? _("undone") : _("redone");
if (!successfully || (m_Successcount < m_CountArray.GetCount()))
  type << wxT(", ") << (m_CountArray.GetCount()-m_Successcount) << wxT(" failed");

DoBriefLogStatus(m_Successcount, msg, type);
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------

                    // A global helper for the archive UnRedos
MyGenericDirCtrl* ReopenArchiveIfNeeded(MyGenericDirCtrl* pane, wxString filepath, MyGenericDirCtrl* anotherpane)  // Ensures the pane is (still) open to the archive containing filepath
{
if (filepath.IsEmpty()) return NULL;

if (pane==NULL || pane->arcman==NULL)
  { pane = MyFrame::mainframe->GetActivePane(); if (pane==NULL || pane->arcman==NULL)  return NULL;  // In case the tab has been shut!
    if (anotherpane != NULL && (pane == anotherpane || pane == anotherpane->partner))  // anotherpane will be NULL for callers like Rename, where only 1 pane is relevant
      pane = pane->GetOppositeDirview();  // If we're interested in *both* dirctrls, and we've just grabbed the wrong one, swap to the other
     else
      if (pane!=NULL && pane->arcman!=NULL && pane->arcman->IsArchive()) 
          pane = pane->GetOppositeDirview();  // For moving between archives, we don't want both to be on the same pane!
  }

if (pane==NULL || pane->arcman==NULL) return NULL;
  
if  (!pane->arcman->IsArchive() || !pane->arcman->GetArc()->IsWithin(filepath))
  { while (Archive::Categorise(filepath) == zt_invalid)   // If that archive is no longer open, we need to find the distal archive in path and open it  
      { if (filepath.IsEmpty()) return NULL;
        filepath = filepath.BeforeLast(wxFILE_SEP_PATH);  // Amputate until we find an archive
      }
    pane->OnOutsideSelect(filepath);                      // Open it
    if  (!pane->arcman->IsArchive() || !pane->arcman->GetArc()->IsWithin(filepath)) return NULL;  // Just a check: shouldn't happen
  }

return pane; // Returning a non-null ptr signifies success. It will be the pane that's used by the caller
}

bool UnRedoMove::Undo()  // Undoing a Move means Moving in reverse
{
if (!UndoPossible) return false; 

wxString originalpath = original.GetPath();             // We use this pre-Move version for UpdateTrees after

bool result = MyGenericDirCtrl::Move(&final, &original, originalfinalbit, m_tsb, GetOverwrittenFile()); // Do the Undo. NB we pass any overwritten file too

if (result)                                             // Assuming it worked, we need to remove what was undone from 'final', ready for any redo
  { if (!ItsADir)                                       // If we've just unmoved a file
      { if (!FromDelete)                                // Don't Update the trashcan: unnecessary, and also leaves the dirview showing it in FullTree mode
          MyFrame::mainframe->OnUpdateTrees(final.GetPath(), IDs); 
        MyFrame::mainframe->OnUpdateTrees(originalpath, IDs);
        if (USE_FSWATCHER && GetNeedsYield()) // No need if !USE_FSWATCHER, or we're not undoing an overwrite
          { PasteThreadEvent event(PasteThreadType, -1); // The ID of -1 flags that this is a fake theadevent
            wxArrayString paths; paths.Add(originalpath);
            event.SetRefreshesArrayString(paths);
            wxPostEvent(MyFrame::mainframe, event);
          }
        wxString ToName(finalbit);
        if (!ToName.IsEmpty() && ToName.Last() == wxFILE_SEP_PATH) ToName = ToName.BeforeLast(wxFILE_SEP_PATH);  // Sanity-check, but this *shouldn't* happen in a file
        if (ToName.find(wxFILE_SEP_PATH) != wxString::npos)  // All this is to defend against the case where finalbit is foo/bar instead of just bar
          { wxFileName temp(ToName);                    // So make a wxFileName, then count the dirs
            for (size_t n=0; n < temp.GetDirCount(); ++n) final.RemoveDir(final.GetDirCount() - 1);
          }
        
        final.SetFullName(wxEmptyString);               //  then remove filename from 'final'
      }
     else
      { wxString ToName(finalbit); if (!ToName.IsEmpty() && ToName.Last() != wxFILE_SEP_PATH) ToName << wxFILE_SEP_PATH;
        wxFileName temp(ToName);
        for (size_t n=0; n < temp.GetDirCount(); ++n)   // Again defend against the case where finalbit is foo/bar/ instead of just bar/
          final.RemoveDir(final.GetDirCount() - 1);     // It's a dir, so remove each subdirname from 'final'
        if (!FromDelete)  MyFrame::mainframe->OnUpdateTrees(final.GetPath(), IDs);   // & Refresh.  See above comment re FromDelete
        MyFrame::mainframe->OnUpdateTrees(originalpath, IDs);  // NB use original original-path
      }
  }  

return result;
}

bool UnRedoMove::Redo()  // Re-doing a Move means re-Moving
{
if (!RedoPossible) return false;

wxString finalpath = final.GetPath();                   // We'll need to use this pre-Move version for UpdateTrees in a moment

bool result = MyGenericDirCtrl::Move(&original, &final, finalbit, m_tsb); 
if (result)                                             // Assuming it worked, we need to remove what was redone from 'original', ready for any re-undo
  { if (!ItsADir)                                       // If we've just ReMoved a file
      { if (USE_FSWATCHER && GetNeedsYield()) // No need if !USE_FSWATCHER, or we're not redoing an overwrite
          { PasteThreadEvent event(PasteThreadType, -1); // The ID of -1 flags that this is a fake theadevent
            wxArrayString paths; paths.Add(original.GetPath());
            event.SetRefreshesArrayString(paths);
            wxPostEvent(MyFrame::mainframe, event);
          }
        original.SetFullName(wxEmptyString);              // Remove its name from 'original'
      }
     else
      original.RemoveDir(original.GetDirCount() - 1);   // Otherwise it's a dir.  Remove this dirname from 'original'  
              
    MyFrame::mainframe->OnUpdateTrees(original.GetPath(), IDs);
    if (!FromDelete)                                    // Don't Update the trashcan: unnecessary, and also leaves the dirview showing it in FullTree mode
        MyFrame::mainframe->OnUpdateTrees(finalpath, IDs); 
  }  

return result;
}

bool UnRedoPaste::Undo()  // Undoing a Paste means deleting the copy
{
if (!UndoPossible) return false;

bool result = MyGenericDirCtrl::ReallyDelete(&final);
if (result && !GetOverwrittenFile().empty()) // We also have to replace any would-have-been-overwritten file. It was stored in the trashcan
  { wxSafeYield(); // For some reason, if we don't do this the FSWatcher delete events are processed _after_ the create ones, so the pane thinks the file is gone
    wxFileName overwritten(GetOverwrittenFile());
    result = MyGenericDirCtrl::Paste(&overwritten, &final, GetOverwrittenFile().AfterLast(wxFILE_SEP_PATH), false, false, false, m_tsb);
  }

if (result)                                       // Assuming it worked, we need to remove what was undone from 'final', ready for any redo
  { if (!ItsADir)                                 // If we've just uncopied a file
      { MyFrame::mainframe->OnUpdateTrees(final.GetPath(), IDs);
        
        wxString ToName(finalbit);
        if (!ToName.IsEmpty() && ToName.Last() == wxFILE_SEP_PATH) ToName = ToName.BeforeLast(wxFILE_SEP_PATH);  // Sanity-check, but this *shouldn't* happen in a file
        if (ToName.find(wxFILE_SEP_PATH) != wxString::npos)  // All this is to defend against the case where finalbit is foo/bar instead of just bar
          { wxFileName temp(ToName);              // So make a wxFileName, then count the dirs
            for (size_t n=0; n < temp.GetDirCount(); ++n) final.RemoveDir(final.GetDirCount() - 1);
          }
        
        final.SetFullName(wxEmptyString);         // Remove filename from 'final'
      }
     else
      { wxString ToName(finalbit); if (!ToName.IsEmpty() && ToName.Last() != wxFILE_SEP_PATH) ToName << wxFILE_SEP_PATH;
        wxFileName temp(ToName);
        for (size_t n=0; n < temp.GetDirCount(); ++n) // Again defend against the case where finalbit is foo/bar/ instead of just bar/
          final.RemoveDir(final.GetDirCount() - 1);   // It's a dir, so remove each subdirname from 'final'

        MyFrame::mainframe->OnUpdateTrees(final.GetPath(), IDs);  //  & Update what's left
      }
  }  

return result;
}

bool UnRedoPaste::Redo()
{
if (!RedoPossible) return false;

wxString finalpath = final.GetPath();                 // We'll need to use this pre-Paste version for UpdateTrees in a moment

bool result = MyGenericDirCtrl::Paste(&original, &final, finalbit, ItsADir, m_dirs_only, false, m_tsb);  // Do the Redo

if (result)
  MyFrame::mainframe->OnUpdateTrees(finalpath, IDs);  // If successful, Update the stubpath, as any new file/subdir won't yet be in the treectrl

return result;
}

bool UnRedoLink::Undo()  // Undoing a Link means ReallyDeleting the link
{
if (!UndoPossible) return false;

wxFileName fp(linkfilepath);                             // ReallyDelete needs a wxFileName, so make one
bool result = MyGenericDirCtrl::ReallyDelete(&fp);

if (result)
  MyFrame::mainframe->OnUpdateTrees(linkfilepath.BeforeLast(wxFILE_SEP_PATH), IDs);

if (result)  DoBriefLogStatus(1, _("action"),  _(" undone"));
return result;
}

bool UnRedoLink::Redo()
{
if (!RedoPossible) return false;

bool  result;
if (linkage)  result = CreateSymlinkWithParameter(originfilepath, linkfilepath, relativity);  // Create a new symlink. relativity may be absolute, relative or no change
  else        result = CreateHardlink(originfilepath, linkfilepath);                          // or hard-link

if (result)
  MyFrame::mainframe->OnUpdateTrees(linkfilepath.BeforeLast(wxFILE_SEP_PATH), IDs);

if (result)  DoBriefLogStatus(1, _("action"),  _(" redone"));
return result;
}

bool UnRedoChangeLinkTarget::Undo()  // Undoing a ChangeLinkTarget means changing in reverse
{
if (!UndoPossible) return false;

bool result = CheckDupRen::ChangeLinkTarget(linkname, oldtarget);

if (result)
  MyFrame::mainframe->OnUpdateTrees(linkname.BeforeLast(wxFILE_SEP_PATH), IDs, "", true); // 'true' forces a refresh in case the symlink icon needs to change to/from broken

if (result)  DoBriefLogStatus(1, _("action"),  _(" undone"));
return result;
}

bool UnRedoChangeLinkTarget::Redo()
{
if (!RedoPossible) return false;

bool result = CheckDupRen::ChangeLinkTarget(linkname, newtarget);

if (result)
  MyFrame::mainframe->OnUpdateTrees(linkname.BeforeLast(wxFILE_SEP_PATH), IDs, "", true);

if (result)  DoBriefLogStatus(1, _("action"),  _(" redone"));
return result;
}

bool UnRedoChangeAttributes::Undo()  // Undoing an attribute change means changing in reverse
{
if (!UndoPossible) return false;

size_t successes=0, failures=0;           // We don't actually use these, but they're needed for the next function
int result = RecursivelyChangeAttributes(filepath, IDs, OriginalAttribute, successes, failures, whichattribute, -1);

if (result > 0)                           // 0==failure, 1==unqualified success, 2==partial success, though this is an unlikely result in an unredo
  MyFrame::mainframe->OnUpdateTrees(filepath.BeforeLast(wxFILE_SEP_PATH), IDs);

if (result)  DoBriefLogStatus(1, _("action"),  _(" undone"));
return (result > 0);
}

bool UnRedoChangeAttributes::Redo()
{
if (!RedoPossible) return false;

size_t successes=0, failures=0;            // We don't actually use these, but they're needed for the next function
int result = RecursivelyChangeAttributes(filepath, IDs, NewAttribute, successes, failures, whichattribute, -1);

if (result > 0)
  MyFrame::mainframe->OnUpdateTrees(filepath.BeforeLast(wxFILE_SEP_PATH), IDs);

if (result)  DoBriefLogStatus(1, _("action"),  _(" redone"));
return (result > 0);
}

bool UnRedoNewDirFile::Undo()  // Undoing a New means ReallyDeleting it
{
if (!UndoPossible)  return false;

wxString name = filename;                           // Make a local copy of filename, in case we amend it below

if (ItsADir)
  { // We need to check if we're undoing a multiple NewDir, e.g. adding bar/baz on to ~/foo. If so we just delete bar/
    wxString bar(name); // However we must protect against someone undoing the creation of /bar/baz/, or //bar/baz/, or...; otherwise we'd end up deleting foo/ instead
    size_t pos = bar.find_first_not_of(wxFILE_SEP_PATH);
    if (pos && pos != wxString::npos)
      { bar = bar.Mid(pos); // Skip over the initial '/'s before doing BeforeFirst()
        bar = bar.BeforeFirst(wxFILE_SEP_PATH);
        name = wxFILE_SEP_PATH + bar; // Replace the initial '/', just in case it really is necessary for some reason
      }
     else name = name.BeforeFirst(wxFILE_SEP_PATH); // No initial '/' so it's easy
 
    name += wxFILE_SEP_PATH;                        // Add a terminal '/' for the usual reason!
  }
wxFileName fp(path + name);                         // ReallyDelete needs a wxFileName, so make one
bool result = MyGenericDirCtrl::ReallyDelete(&fp);
if (result)                                         // Assuming it worked, we just need to UpdateTrees
    MyFrame::mainframe->OnUpdateTrees(path, IDs);     

if (result)  DoBriefLogStatus(1, _("action"),  _(" undone"));
return result;
}

bool UnRedoNewDirFile::Redo()
{
if (!RedoPossible) return false;

int result = 0;                                                   // Set to false in case Active==NULL below
MyGenericDirCtrl* Active = MyFrame::mainframe->GetActivePane();   // We need a pane ptr. May as well use the active one
if (Active != NULL)  result = Active->OnNewItem(filename, path, ItsADir);  // Redo the "New"

if (result)                                                       // Assuming it worked, we just need to UpdateTrees
    MyFrame::mainframe->OnUpdateTrees(path, IDs);

if (result)  DoBriefLogStatus(1, _("action"),  _(" redone"));
return result;  
}

bool UnRedoDup::Undo()  // Undoing a Dup means ReallyDeleting the duplicate
{                       // (Of course, for a file we could have used wxRemoveFile, but what if it's a dir?)
if (!UndoPossible)  return false;

wxFileName fp;                                        // ReallyDelete needs a wxFileName

if (ItsADir)  fp.Assign(newpath + newname);
  else        fp.Assign(destpath, newname);

bool result = MyGenericDirCtrl::ReallyDelete(&fp);
if (result)                                        // Assuming it worked, we just need to UpdateTrees
  MyFrame::mainframe->OnUpdateTrees(destpath, IDs);     

if (result)  DoBriefLogStatus(1, _("action"),  _(" undone"));
return result;
}

bool UnRedoDup::Redo()
{
int result;                                                      // CheckDupRen::RenameDir* returns an int, may be 0, 1, 2
if (!RedoPossible) return false;

if (ItsADir)   
  { wxString originalfilepath = destpath + originalname, finalfilepath = newpath + newname;
    result = CheckDupRen::RenameDir(originalfilepath, finalfilepath, true);
  }
 else
    result = CheckDupRen::RenameFile(destpath, originalname, newname, true);

if (result > 0)                                               // Assuming it worked, we just need to UpdateTrees
      MyFrame::mainframe->OnUpdateTrees(destpath, IDs); 

if (result)  DoBriefLogStatus(1, _("action"),  _(" redone"));
return (result > 0);
}

bool UnRedoRen::Undo()  // Undoing a Rename means ReRenaming
{
int result;                                                         // CheckDupRen::RenameDir* returns an int, may be 0, 1, 2
if (!UndoPossible) return false;

if (ItsADir)   
  { result = CheckDupRen::RenameDir(newpath, destpath, false);      // Note the reversal of newname/originalname
    if (result > 0) 
      { if (newname.IsEmpty())                                      // For a dir, newname should be empty.  If so, update the standard way
          MyFrame::mainframe->OnUpdateTrees(newpath.BeforeLast(wxFILE_SEP_PATH), IDs);
         else                                                       // If not, it flags that we renamed startdir, so tell UpdateTrees that
          MyFrame::mainframe->OnUpdateTrees(newpath, IDs, destpath);// Note here we use newpath, not its parent
      }
  }
 else
  { result = CheckDupRen::RenameFile(destpath, newname, originalname, false);
    if (result > 0) MyFrame::mainframe->OnUpdateTrees(destpath, IDs); // Assuming it worked, we just need to UpdateTrees
  }

if (result)  DoBriefLogStatus(1, _("action"),  _(" undone"));
return (result > 0);
}

bool UnRedoRen::Redo()
{
int result;                                                         // CheckDupRen::RenameDir* returns an int, may be 0, 1, 2
if (!RedoPossible) return false;

if (ItsADir)   
  { result = CheckDupRen::RenameDir(destpath, newpath, false);
    if (result > 0) 
      { if (newname.IsEmpty())                                      // For a dir, newname should be empty.  If so, update the standard way
          MyFrame::mainframe->OnUpdateTrees(destpath.BeforeLast(wxFILE_SEP_PATH), IDs);
         else                                                       // If not, it flags that we renamed startdir, so tell UpdateTrees that
          MyFrame::mainframe->OnUpdateTrees(destpath, IDs, newname);// Note here we use destpath, not its parent
      }
  }
 else
  { result = CheckDupRen::RenameFile(destpath, originalname, newname, false);
    if (result > 0) MyFrame::mainframe->OnUpdateTrees(destpath, IDs);  // Assuming it worked, we just need to UpdateTrees
  }

if (result)  DoBriefLogStatus(1, _("action"),  _(" redone"));
return (result > 0);
}


bool UnRedoArchiveRen::Undo()  // Undoing a Rename or Move means ReRenaming. Undoing a Paste means Removing
{
if (!UndoPossible) return false;

wxString filepath(newfilepaths[0]);
  // Though this is UnRedoArchiveREN it's used for Moving/Pasting within an archive too.
  // Here we're trying to cope with Moving within an archive that is/was open in both fileviews. If one was subsequently closed, we don't want to reopen it to unredo
if (!other)   // First overcome the small hurdle that none of the functions that create this UnRedo actually _set_ other!
  { other = active->GetOppositeDirview();
    if (other                               // other will still be null if the panes are unsplit!
          && active->isright != other->isright) other = other->partner;
  }
bool activeOK = (active!=NULL && active->arcman!=NULL && active->arcman->IsArchive() && active->arcman->GetArc()->IsWithin(filepath));
bool otherOK = (other!=NULL && other->arcman!=NULL && other->arcman->IsArchive() && other->arcman->GetArc()->IsWithin(filepath));
if (!activeOK && !otherOK) { active = ReopenArchiveIfNeeded(active, filepath, NULL); if (active==NULL) return false; }
 else if (!activeOK && otherOK) { active = other; otherOK = false; }  // If only the opposite pane has the archive open, swap

if (!active->arcman->GetArc()->Alter(newfilepaths, dup ? arc_remove : arc_rename, originalfilepaths)) return false;  // Remove duplicate, or replace new with original
if (otherOK) other->arcman->GetArc()->Alter(newfilepaths, dup ? arc_remove : arc_rename, originalfilepaths);  // If appropriate, do this in the other pane too

MyFrame::mainframe->OnUpdateTrees(filepath.BeforeLast(wxFILE_SEP_PATH), IDs, wxT(""), true);
MyFrame::mainframe->OnUpdateTrees(originalfilepaths[0].BeforeLast(wxFILE_SEP_PATH), IDs, wxT(""), true);
DoBriefLogStatus(1, _("action"),  _(" undone"));
return true;
}

bool UnRedoArchiveRen::Redo()
{
if (!RedoPossible) return false;

wxString filepath(originalfilepaths[0]);
  // Though this is UnRedoArchiveREN it's used for Moving/Pasting within an archive too.
  // Here we're trying to cope with Moving within an archive that is/was open in both fileviews. If one was subsequently closed, we don't want to reopen it to unredo
if (!other)   // First overcome the small hurdle that none of the functions that create this UnRedo actually _set_ other!
  { other = active->GetOppositeDirview();
    if (other                               // other will still be null if the panes are unsplit!
      && active->isright != other->isright) other = other->partner;
  }
bool activeOK = (active!=NULL && active->arcman!=NULL && active->arcman->IsArchive() && active->arcman->GetArc()->IsWithin(filepath));
bool otherOK = (other!=NULL && other->arcman!=NULL && other->arcman->IsArchive() && other->arcman->GetArc()->IsWithin(filepath));
if (!activeOK && !otherOK) { active = ReopenArchiveIfNeeded(active, filepath, NULL); if (active==NULL) return false; }
 else if (!activeOK && otherOK) { active = other; otherOK = false; }  // If only the opposite pane has the archive open, swap

if (!active->arcman->GetArc()->Alter(originalfilepaths, dup ? arc_dup : arc_rename, newfilepaths, m_dirs_only)) return false;
if (otherOK) other->arcman->GetArc()->Alter(originalfilepaths, dup ? arc_dup : arc_rename, newfilepaths, m_dirs_only);

MyFrame::mainframe->OnUpdateTrees(filepath.BeforeLast(wxFILE_SEP_PATH), IDs, wxT(""), true);
MyFrame::mainframe->OnUpdateTrees(newfilepaths[0].BeforeLast(wxFILE_SEP_PATH), IDs, wxT(""), true);
DoBriefLogStatus(1, _("action"),  _(" redone"));
return true;
}

bool UnRedoExtract::Undo()  // Undoing an Extract means deleting it i.e. Moving to the Deleted bin
{
if (!UndoPossible || filepaths.IsEmpty() || path.IsEmpty()) return false; 

if (trashpath.IsEmpty() || !wxDirExists(trashpath))              // If there's already a deleted subdir from a previous undo, use it
  { wxFileName trash;
    if (!DirectoryForDeletions::GetUptothemomentDirname(trash, delcan)) return false; // Create a unique subdir in Deleted bin, using current date/time
    trashpath =  trash.GetPath(); if (trashpath.Last() != wxFILE_SEP_PATH)  trashpath << wxFILE_SEP_PATH;
  }

if (path.Last() != wxFILE_SEP_PATH)  path << wxFILE_SEP_PATH;

for (size_t n=0; n < filepaths.GetCount(); ++n)
  { wxFileName ToTrash(trashpath);
    wxString finalbit(filepaths[n]); FileData fd(finalbit);
    if (fd.IsDir())
      { if (finalbit.Last() == wxFILE_SEP_PATH) finalbit = finalbit.BeforeLast(wxFILE_SEP_PATH);
        wxString LastSeg = finalbit.AfterLast(wxFILE_SEP_PATH);
        wxFileName fn(finalbit+wxFILE_SEP_PATH);                // If finalbit is multiple, this will Move it plus all descendants at once
        if (!MyGenericDirCtrl::Move(&fn, &ToTrash, LastSeg, NULL))  return false;  // LastSeg *mustn't* have a terminal '/' here!
        while ((n+1) < filepaths.GetCount() && filepaths[n+1].StartsWith(finalbit))  ++n;  // Since we've just undone a dir, skip over any of its contents (as they'll no longer be there!)
      }
     else
      { wxString finalbitpath = finalbit.BeforeLast(wxFILE_SEP_PATH);  // These 3 lines cater for the deep-within filepaths: we can't assign /foo/bar/fubar directly in wxFileName
        if (!finalbitpath.IsEmpty()) 
          { finalbitpath << wxFILE_SEP_PATH; finalbit = finalbit.AfterLast(wxFILE_SEP_PATH); }
        
        wxFileName fn(finalbitpath + finalbit);
        if (!MyGenericDirCtrl::Move(&fn, &ToTrash, finalbit, NULL))  return false;
      }
  }

MyFrame::mainframe->OnUpdateTrees(path, IDs);
DoBriefLogStatus(1, _("action"),  _(" undone"));
return true;
}

bool UnRedoExtract::Redo()  // Re-doing an Extract means undeleting it
{
if (!RedoPossible || trashpath.IsEmpty()) return false;

for (size_t n=0; n < filepaths.GetCount(); ++n)
  { wxFileName fn(path);
    wxString finalbit; if (!filepaths[n].StartsWith(path, &finalbit)) return false;
    FileData fd(trashpath+finalbit);
    if (fd.IsDir())
      { finalbit = finalbit.BeforeFirst(wxFILE_SEP_PATH);                     // If finalbit is multiple, this will Move it plus all descendants at once
        wxFileName FromTrash(trashpath+finalbit);
        if (!MyGenericDirCtrl::Move(&FromTrash, &fn, finalbit, NULL))  return false;
        while ((n+1) < filepaths.GetCount() && filepaths[n+1].StartsWith(path+finalbit))  ++n;  // See the Undo comment
      }
     else
      { wxFileName FromTrash(trashpath, finalbit);
        if (!MyGenericDirCtrl::Move(&FromTrash, &fn, finalbit, NULL))  return false;
      }
  }
MyFrame::mainframe->OnUpdateTrees(path, IDs);
DoBriefLogStatus(1, _("action"),  _(" redone"));
return true;
}

bool UnRedoArchivePaste::Undo()  // Undoing a Paste means Removing
{
if (!UndoPossible || newfilepaths.IsEmpty()) return false;  // newfilepaths is what's in the archive
destctrl = ReopenArchiveIfNeeded(destctrl, destpath, NULL); if (destctrl == NULL) return false;

wxArrayString unused; destctrl->arcman->GetArc()->Alter(newfilepaths, arc_remove, unused, m_dirs_only);

MyFrame::mainframe->OnUpdateTrees(destpath, IDs, wxT(""), true);
DoBriefLogStatus(1, _("action"), _(" undone"));
return true;
}

bool UnRedoArchivePaste::Redo()  // Re-doing a Paste means, er, redoing the paste
{
if (!RedoPossible || destpath.IsEmpty()) return false; 
destctrl = ReopenArchiveIfNeeded(destctrl, destpath, NULL); if (destctrl == NULL) return false;

wxArrayString Path; Path.Add(destpath); Path.Add(originpath); // We have to provide a 2nd arraystring anyway due to the Alter API, so use it to transport the paths
if (!destctrl->arcman->GetArc()->Alter(originalfilepaths, arc_add, Path, m_dirs_only)) return false;

MyFrame::mainframe->OnUpdateTrees(destpath, IDs, wxT(""), true);
DoBriefLogStatus(1, _("action"), _(" redone"));
return true;
}

bool UnRedoArchiveDelete::Undo()  // Undoing a Delete means reloading from trashcan
{
if (!UndoPossible || newfilepaths.IsEmpty()) return false;    // newfilepaths is what's in the trashpath
originctrl = ReopenArchiveIfNeeded(originctrl, originpath, NULL); if (originctrl==NULL) return false;

wxArrayString Path; Path.Add(originpath); Path.Add(destpath); // We have to provide a 2nd arraystring anyway due to the Alter API, so use it to transport the paths
if (!originctrl->arcman->GetArc()->Alter(newfilepaths, arc_add, Path)) return false;

MyFrame::mainframe->OnUpdateTrees(originpath, IDs, wxT(""), true);
DoBriefLogStatus(1, _("action"),  _(" undone"));
return true;
}

bool UnRedoArchiveDelete::Redo()  // Re-doing a Delete means Removing from archive
{
if (!RedoPossible || originpath.IsEmpty()) return false; 
originctrl = ReopenArchiveIfNeeded(originctrl, originpath, NULL); if (originctrl==NULL) return false;

wxArrayString unused; originctrl->arcman->GetArc()->Alter(originalfilepaths, arc_remove, unused);

MyFrame::mainframe->OnUpdateTrees(originpath, IDs, wxT(""), true);

DoBriefLogStatus(1, _("action"),  _(" redone"));
return true;
}

bool UnRedoCutPasteToFromArchive::Undo()  // Undoing a Move means Un-Moving it
{
if (!UndoPossible || newfilepaths.IsEmpty()) return false; 

if (which == amt_from)                                    // If the original Move was From an archive to outside
  { originctrl = ReopenArchiveIfNeeded(originctrl, originpath, destctrl); if (originctrl == NULL) return false;
    wxArrayString Path; Path.Add(originpath);             // We have to provide a 2nd arraystring anyway due to the Alter API, so use it to transport the path
    Path.Add(destpath);                                   // and this one
    if (!originctrl->arcman->GetArc()->Alter(newfilepaths, arc_add, Path)) return false;  // Undo the Paste bit, but leave the item in the trashcan ready for a redo

    if (originctrl->fileview == ISLEFT)  originctrl->SetPath(originpath.BeforeLast(wxFILE_SEP_PATH));
      else originctrl->partner->SetPath(originpath.BeforeLast(wxFILE_SEP_PATH));
      
    MyFrame::mainframe->OnUpdateTrees(originpath.BeforeLast(wxFILE_SEP_PATH), IDs, wxT(""), true);
    MyFrame::mainframe->OnUpdateTrees(destpath, IDs, wxT(""), true);
  }

if (which == amt_to)                                      // If the original Move was To an archive from outside
  { destctrl = ReopenArchiveIfNeeded(destctrl, destpath, originctrl); if (destctrl==NULL) return false;
    wxArrayString unused;
    if (!destctrl->arcman->GetArc()->Extract(newfilepaths, originpath, unused)) return false;  // Do the Paste bit of the Move
    unused.Clear(); 
    if (!destctrl->arcman->GetArc()->Alter(newfilepaths, arc_remove, unused)) return false;    // Now the delete bit

    MyFrame::mainframe->OnUpdateTrees(originpath, IDs, wxT(""), true);
    MyFrame::mainframe->OnUpdateTrees(destpath,   IDs, wxT(""), true);
  }

DoBriefLogStatus(1, _("action"),  _(" undone"));
return true;
}

bool UnRedoCutPasteToFromArchive::Redo()  // Re-doing a Move means Re-Moving it
{
if (!RedoPossible || originalfilepaths.IsEmpty()) return false;

if (which == amt_from)                                    // If the original Move was From an archive to outside
  { originctrl = ReopenArchiveIfNeeded(originctrl, originpath, destctrl); if (originctrl==NULL) return false;
    wxArrayString unused;
    if (!originctrl->arcman->GetArc()->Alter(originalfilepaths, arc_remove, unused)) return false;  // The RemoveFromArchive bit of the Move. The Paste happens elsewhere in the supercluster

    MyFrame::mainframe->OnUpdateTrees(originpath.BeforeLast(wxFILE_SEP_PATH), IDs, wxT(""), true);
  }

if (which == amt_to)                                      // If the original Move was To an archive from outside
  { destctrl = ReopenArchiveIfNeeded(destctrl, destpath, originctrl); if (destctrl==NULL) return false;
    wxArrayString fpaths, Path; Path.Add(destpath); Path.Add(originpath); // We have to provide a 2nd arraystring anyway due to the Alter API, so use it to transport the paths
    for (size_t n=0; n < originalfilepaths.GetCount(); ++n)               // We need to go thru the filepaths to add the children of any dirs
      { fpaths.Add(originalfilepaths.Item(n));
        RecursivelyGetFilepaths(originalfilepaths.Item(n), fpaths);
      }
      
    if (!destctrl->arcman->GetArc()->Alter(fpaths, arc_add, Path)) return false;  // Do the Paste bit of the Move

	bool needsyield(false);    
    for (size_t n=0; n < originalfilepaths.GetCount(); ++n)                       // Now the delete bit
      { wxFileName From(originalfilepaths[n]);
        originctrl->ReallyDelete(&From);
#if wxVERSION_NUMBER > 2812 && (wxVERSION_NUMBER < 3003 || wxVERSION_NUMBER == 3100)
		// Earlier wx3 version have a bug (http://trac.wxwidgets.org/ticket/17122) that can cause segfaults if we SetPath() on returning from here
		if (From.IsDir()) needsyield = true; // Yielding works around the problem by altering the timing of incoming fswatcher events
#endif
      }
    if (needsyield) wxSafeYield();

    MyFrame::mainframe->OnUpdateTrees(originpath, IDs, wxT(""), true);
    MyFrame::mainframe->OnUpdateTrees(destpath, IDs, wxT(""), true);
  }

DoBriefLogStatus(1, _("action"),  _(" redone"));
return true;
}


bool UnRedoMoveToFromArchive::Undo()  // Undoing a Move means Un-Moving it
{
if (!UndoPossible || newfilepaths.IsEmpty()) return false; 

if (which == amt_from)                                    // If the original Move was From an archive to outside
  { originctrl = ReopenArchiveIfNeeded(originctrl, originpath, destctrl); if (originctrl == NULL) return false;
    wxArrayString Path; Path.Add(originpath);             // We have to provide a 2nd arraystring anyway due to the Alter API, so use it to transport the path
    Path.Add(destpath);                                   // and this one
    if (!originctrl->arcman->GetArc()->Alter(newfilepaths, arc_add, Path)) return false;  // Do the Paste bit of the Move, back into the archive

	bool needsyield(false);    
    for (size_t n=0; n < newfilepaths.GetCount(); ++n) // And now delete the outside one
      { FileData fd(newfilepaths[n]);
        if (!fd.IsValid()) continue;
        wxFileName From(newfilepaths[n]);
        originctrl->ReallyDelete(&From);
#if wxVERSION_NUMBER > 2812 && (wxVERSION_NUMBER < 3003 || wxVERSION_NUMBER == 3100)
		// Earlier wx3 version have a bug (http://trac.wxwidgets.org/ticket/17122) that can cause segfaults if we SetPath() on returning from here
		if (From.IsDir()) needsyield = true; // Yielding works around the problem by altering the timing of incoming fswatcher events
#endif
      }
    if (needsyield) wxSafeYield();

    if (originctrl->fileview == ISLEFT)  originctrl->SetPath(originpath.BeforeLast(wxFILE_SEP_PATH));
      else originctrl->partner->SetPath(originpath.BeforeLast(wxFILE_SEP_PATH));
      
    MyFrame::mainframe->OnUpdateTrees(originpath.BeforeLast(wxFILE_SEP_PATH), IDs, wxT(""), true);
    MyFrame::mainframe->OnUpdateTrees(destpath, IDs, wxT(""), true);
  }

if (which == amt_to)                                      // If the original Move was To an archive from outside
  { destctrl = ReopenArchiveIfNeeded(destctrl, destpath, originctrl); if (destctrl==NULL) return false;
    wxArrayString unused;
    if (!destctrl->arcman->GetArc()->Extract(newfilepaths, originpath, unused)) return false;  // Do the Paste bit of the Move
    unused.Clear(); 
    if (!destctrl->arcman->GetArc()->Alter(newfilepaths, arc_remove, unused)) return false;    // Now the delete bit

    MyFrame::mainframe->OnUpdateTrees(originpath, IDs, wxT(""), true);
    MyFrame::mainframe->OnUpdateTrees(destpath,   IDs, wxT(""), true);
  }

DoBriefLogStatus(1, _("action"),  _(" undone"));
return true;
}

bool UnRedoMoveToFromArchive::Redo()  // Re-doing a Move means Re-Moving it
{
if (!RedoPossible || originalfilepaths.IsEmpty()) return false;

if (which == amt_from)                                    // If the original Move was From an archive to outside
  { originctrl = ReopenArchiveIfNeeded(originctrl, originpath, destctrl); if (originctrl==NULL) return false;
    wxArrayString unused;
    originctrl->arcman->GetArc()->DoExtract(originalfilepaths, destpath, unused, true);  // Do the Paste bit of the Move
    unused.Clear(); 
    if (!originctrl->arcman->GetArc()->Alter(originalfilepaths, arc_remove, unused)) return false;  // The RemoveFromArchive bit of the Move

    MyFrame::mainframe->OnUpdateTrees(originpath.BeforeLast(wxFILE_SEP_PATH), IDs, wxT(""), true);
  }

if (which == amt_to)                                      // If the original Move was To an archive from outside
  { destctrl = ReopenArchiveIfNeeded(destctrl, destpath, originctrl); if (destctrl==NULL) return false;
    wxArrayString fpaths, Path; Path.Add(destpath); Path.Add(originpath); // We have to provide a 2nd arraystring anyway due to the Alter API, so use it to transport the paths
    for (size_t n=0; n < originalfilepaths.GetCount(); ++n)               // We need to go thru the filepaths to add the children of any dirs
      { fpaths.Add(originalfilepaths.Item(n));
        RecursivelyGetFilepaths(originalfilepaths.Item(n), fpaths);
      }
      
    if (!destctrl->arcman->GetArc()->Alter(fpaths, arc_add, Path)) return false;  // Do the Paste bit of the Move

	bool needsyield(false);    
    for (size_t n=0; n < originalfilepaths.GetCount(); ++n)                       // Now the delete bit
      { wxFileName From(originalfilepaths[n]);
        originctrl->ReallyDelete(&From);
#if wxVERSION_NUMBER > 2812 && (wxVERSION_NUMBER < 3003 || wxVERSION_NUMBER == 3100)
		// Earlier wx3 version have a bug (http://trac.wxwidgets.org/ticket/17122) that can cause segfaults if we SetPath() on returning from here
		if (From.IsDir()) needsyield = true; // Yielding works around the problem by altering the timing of incoming fswatcher events
#endif
      }
    if (needsyield) wxSafeYield();

    MyFrame::mainframe->OnUpdateTrees(originpath, IDs, wxT(""), true);
    MyFrame::mainframe->OnUpdateTrees(destpath, IDs, wxT(""), true);
  }

DoBriefLogStatus(1, _("action"),  _(" redone"));
return true;
}



DirectoryForDeletions::DirectoryForDeletions()  // Sets up appropriately named dirs to receive files/dirs that have been deleted/trashed
{
if (TRASHCAN.IsEmpty())
  { wxString cachedir = StrWithSep(wxGetApp().GetXDGcachedir()) + wxT("4Pane/.Trash/");
    if (!wxFileName::Mkdir(cachedir, 0777, wxPATH_MKDIR_FULL))
      { cachedir = StrWithSep(wxGetApp().GetHOME()) + wxT(".cache/4Pane/.Trash/"); // Try again with any user-provided 'home'
        if (!wxFileName::Mkdir(cachedir, 0777, wxPATH_MKDIR_FULL))
          { cachedir = StrWithSep(wxGetHomeDir()) + wxT(".cache/4Pane/.Trash/");
            if (!wxFileName::Mkdir(cachedir, 0777, wxPATH_MKDIR_FULL))
              { cachedir = StrWithSep(wxStandardPaths::Get().GetTempDir()) + wxT("4Pane/.Trash/");
                if (!wxFileName::Mkdir(cachedir, 0777, wxPATH_MKDIR_FULL))
                  { wxLogWarning(_("Failed to create a directory to store deletions")); return; }
              }
          }
      }
    TRASHCAN = StrWithSep(cachedir);
    wxConfigBase::Get()->Write(wxT("/Misc/Display/TRASHCAN"), TRASHCAN);
  }

TRASHCAN = StrWithSep(TRASHCAN);

wxString refuse(TRASHCAN);
if (refuse.GetChar(0) == wxT('~'))
  refuse = StripSep(wxGetHomeDir()) + refuse.Mid(1);

DeletedName = refuse + wxT("DeletedBy4Pane/"); CreateCan(delcan); // Deletes

TrashedName = refuse + wxT("TrashedBy4Pane/"); CreateCan(trashcan); // Trashed

TempfileDir = refuse + wxT("Tempfiles/"); CreateCan(tempfilecan);  // And a location for tempfiles
}

DirectoryForDeletions::~DirectoryForDeletions()
{
wxFileName deleted(DeletedName);  // Deletes the files/dirs that have been deleted into DeletedBy4Pane
ReallyDelete(&deleted);

wxFileName tmp(wxStandardPaths::Get().GetTempDir() + wxT("/4Pane/")); // Ditto for any temp files in /tmp/
ReallyDelete(&tmp);

wxFileName temps(TempfileDir); // and any temp files put in the old-fashioned place
ReallyDelete(&temps);
      // Don't do the Trashed dir, it's supposed to stay
}

//static
void DirectoryForDeletions::CreateCan(enum whichcan can)  // Create a trash-can or deleted-can, depending on bool
{
if (can == trashcan)
  { if (!wxDirExists(TrashedName))
      { if (!wxFileName::Mkdir(TrashedName, 0777, wxPATH_MKDIR_FULL))
          wxLogError(_("Sorry, can't create your selected Trashed subdirectory.  A permissions problem perhaps?\nI suggest you use Configure to choose another location"));  
        else
          { FileData tn(TrashedName); tn.DoChmod(0777); }
      }
  }

  else if (can == delcan)
  { if (!wxDirExists(DeletedName))                                    // Check it doesn't already exist, perhaps because of a previous crash-before-exit
      { if (!wxFileName::Mkdir(DeletedName, 0777, wxPATH_MKDIR_FULL)) // and create it
          wxLogError(_("Sorry, can't create your selected Deleted subdirectory.  A permissions problem perhaps?\nI suggest you use Configure to choose another location."));                                                                                                
         else
          { FileData dn(DeletedName); dn.DoChmod(0777); }  // Yes, I know we just tried to create it with these permissions, but umask probably interfered.
      }
  }

  else if (can == tempfilecan)
  { if (!wxDirExists(TempfileDir))
      { if (!wxFileName::Mkdir(TempfileDir, 0777, wxPATH_MKDIR_FULL))  
          wxLogError(_("Sorry, can't create your selected temporary file subdirectory.  A permissions problem perhaps?\nI suggest you use Configure to choose another location."));                                                                                                
         else
          { FileData dn(TempfileDir); dn.DoChmod(0777); }
      }
  }
}

void DirectoryForDeletions::EmptyTrash(bool trash)  // Empties trash-can or 'deleted'-can, depending on the bool
{
if (trash)
   { wxString msg(_("Emptying 4Pane's Trash-can will permanently delete any files that have been saved there!\n\n                                              Continue?"));
    wxMessageDialog ask(MyFrame::mainframe->GetActivePane(), msg, wxString(_("Are you SURE?")), wxYES_NO | wxICON_QUESTION);
    if (ask.ShowModal() != wxID_YES) return;  // 2nd thoughts
    
    wxFileName deleted(TrashedName);          // Make DeletedName into a wxFileName
    ReallyDelete(&deleted);                   //  and delete it & contents
    CreateCan(trashcan);                      // Recreate an empty dir
    BriefLogStatus bls(_("Trashcan emptied"));
  }
  
 else  
   { wxString msg(_("Emptying 4Pane's Deleted-can happens automatically when 4Pane is closed\nDoing it prematurely will delete any files stored there.  All Undo-Redo data will be lost!\n\n                                              Continue?"));
    wxMessageDialog ask(MyFrame::mainframe->GetActivePane(), msg, wxString(_("Are you SURE?")), wxYES_NO | wxICON_QUESTION);
    if (ask.ShowModal() != wxID_YES) return;  // 2nd thoughts
    
    wxFileName deleted(DeletedName);          // Make DeletedName into a wxFileName
    ReallyDelete(&deleted);                   //  and delete it & contents
    CreateCan(delcan);                        // Recreate an empty dir
    
    UnRedoManager::ClearUnRedoArray();        // Safer to get rid of any Undo/Redo, as most of them would no longer work
    BriefLogStatus bls(_("Stored files permanently deleted"));
  }
}

bool DirectoryForDeletions::ReallyDelete(wxFileName *PathName)  // Really deletes the files/dirs that have been 'deleted' during this session
{                                 // An amended version of DirGenericDirCtrl::ReallyDelete.  Can't easily use the original: it'll be out of scope
class FileData stat(PathName->GetFullPath());
if (stat.IsDir())                             // If it's really a dir
  { if (!PathName->GetFullName().IsEmpty())   // and providing there IS a "filename"
        PathName->AppendDir(PathName->GetFullName());  PathName->SetFullName(wxEmptyString); // make it into the real path
  }
 else
  { if (stat.IsSymlink())
      { if (!wxRemoveFile(stat.GetFilepath()))
          return false;  // Shouldn't fail, but exit ungracefully if somehow it does eg due to permissions
      }      
      else    // It must be a file, which we're really-deleting by recursion
      { if (!stat.IsValid())                          // Check it exists as a file (or pipe or . . .).  If not, exit ungracefully
           return false; 
        if (!wxRemoveFile(PathName->GetFullPath()))   // It's alive, so kill it
           return false;                      // Shouldn't fail, but exit ungracefully if somehow it does eg due to permissions
      }
    return true;
  }

                        // If we're here, it's a dir
wxDir dir(PathName->GetPath());           // Use helper class to deal sequentially with each child file & subdir
if (!dir.IsOpened())        return false;

wxString filename;
while (dir.GetFirst(&filename))           // Go thru the dir, committing infanticide 1 child @ a time
  {
    wxFileName child(PathName->GetPath(), filename);  // Make a new wxFileName for child
    if (!ReallyDelete(&child))            // Slaughter it by recursion, whether it's a dir or file
        return false;                     // If this fails, bug out
  }
  
if (!PathName->Rmdir())  return false;    // Finally, kill the dir itself

return true;
}

//static
bool DirectoryForDeletions::GetUptothemomentDirname(wxFileName& trashdir, enum whichcan can_type)  // Create unique subdir
{
wxString dirname;

if (can_type == tempfilecan)  // For true tempfiles, create a temporary dir
  dirname = MakeTempDir(TempfileDir);
if (!dirname.empty())
  { trashdir.AssignDir(dirname);
    return true;
  }

  // For other cans, or if mkdtemp somehow failed, do it the original way
if (can_type == trashcan) dirname = TrashedName;  // Get correct base location
 else 
   if (can_type == tempfilecan)
    { dirname = TempfileDir;              // If it's TempfileDir, recheck that the directory exists.
      if (!wxDirExists(TempfileDir))      // This is because if we run an instance of the program, then another, then kill the 1st, 
        CreateCan(tempfilecan);           // this leaves the 2nd instance without its dir, & no tempfiles can be made
    }
 else 
  { dirname = DeletedName;                // If it's Delete, recheck that the directory exists.
    if (!wxDirExists(DeletedName))        // See above for reason
      CreateCan(delcan);
  }

wxDateTime now = wxDateTime::Now();

dirname += now.Format(wxT("%d-%m-%y__%H-%M-%S"));

if (wxDirExists(dirname))                 // Check that this unique path doesn't already exist! Maybe we're doing >1 delete per sec
  { int n=0;
    wxString addon;
    do                                    // If it DOES exist, add to the dirname until we find a novelty
        addon.Printf(wxT("%i"), n++);     // Create an add-on for filename, itoa(n)
     while (wxDirExists(dirname + addon));
     
     dirname += addon;                    // Once we're here, the name IS unique
  }
      
trashdir.AssignDir(dirname + wxFILE_SEP_PATH); // We have a unique dirname, so make the wxFileName
trashdir.Mkdir();
return trashdir.DirExists();
}

wxString DirectoryForDeletions::DeletedName;        // Initialise names
wxString DirectoryForDeletions::TrashedName;
wxString DirectoryForDeletions::TempfileDir;
 
