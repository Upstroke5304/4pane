/////////////////////////////////////////////////////////////////////////////
// Name:        Misc.cpp
// Purpose:     Misc stuff
// Part of:     4Pane
// Author:      David Hart
// Copyright:   (c) 12 David Hart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h" 

#include "wx/artprov.h"
#include "wx/dirctrl.h"
#include "wx/stdpaths.h"
#include "wx/longlong.h"
#include "wx/mstream.h"
#include "wx/xrc/xmlres.h"
#include "wx/confbase.h"
#if wxVERSION_NUMBER >= 3000
  #include <wx/html/helpdlg.h>
#endif

#include "Externs.h"
#include "Misc.h"
#include "Filetypes.h"
#include "Redo.h"
#include "MyGenericDirCtrl.h"
#include "MyFrame.h"

const int DEFAULT_BRIEFMESSAGEBOX_TIME = 2;

BriefMessageBox::BriefMessageBox(wxString Message, double secondsdisplayed /*= -1*/, wxString Caption /*= wxEmptyString*/)
                            :  wxDialog(NULL, -1, Caption)
{
wxStaticBox* staticbox = new wxStaticBox(this, -1, wxT(""));    // Start by doing sizer things, so that there's somewhere to put the message
wxStaticBoxSizer *textsizer = new wxStaticBoxSizer(staticbox, wxHORIZONTAL);  
textsizer->Add(CreateTextSizer(Message), 1, wxCENTRE | wxALL, 10);

wxBoxSizer *mainsizer = new wxBoxSizer(wxHORIZONTAL);
mainsizer->Add(textsizer, 1, wxCENTER | wxALL, 20);

SetSizer(mainsizer); mainsizer->Fit(this);
Centre(wxBOTH | wxCENTER_FRAME);

BriefTimer.Setup(this);                                         // Let the timer know who's boss

double length;
if (secondsdisplayed <= 0)                                      // Minus number means default, and zero would mean infinite which we don't want here
  length = DEFAULT_BRIEFMESSAGEBOX_TIME;
 else length = secondsdisplayed;
 
BriefTimer.Start((int)(length * 1000), wxTIMER_ONE_SHOT);       // Start timer as one-shot.  *1000 gives seconds

ShowModal();                                                    // Display the message
}

#if wxVERSION_NUMBER >= 3000
bool HtmlHelpController::Display(const wxString& x)
{
if (m_helpDialog && m_helpDialog->IsShown())
  return false; // Someone pressed the F1 key twice!
CreateHelpWindow();
if (m_helpDialog) // Which it now should be. This is our first opportunity to grab the close event
  m_helpDialog->Bind(wxEVT_CLOSE_WINDOW, &HtmlHelpController::OnCloseWindow, this);

bool success = m_helpWindow->Display(x);
MakeModalIfNeeded();
return success;
}

void HtmlHelpController::OnCloseWindow(wxCloseEvent& evt)
{
// Catching this event (and not skipping) prevents the baseclass OnCloseWindow() from being called
// That means that m_helpDialog isn't nulled without the dialog first being destroyed
// That will then happen in ~wxHtmlHelpController() as m_helpDialog will still be non-null
if (m_helpDialog)
  m_helpDialog->EndModal(wxID_OK);
}
#endif

wxString ParseSize(wxULongLong bytes, bool longer)  // Returns a string filled with the size, but in bytes, KB, MB or GB according to magnitude
{
wxString text;

if (bytes < 1024)
	{ text = bytes.ToString() + wxT('B'); return text; }

double display_number; wxChar units;
if (bytes < 1048576)
  { display_number = bytes.ToDouble() / 1024; units = wxT('K'); }
 else if (bytes < 1073741824)
  { display_number = bytes.ToDouble() / 1048576; units = wxT('M'); }
 else
  { display_number = bytes.ToDouble() / 1073741824; units = wxT('G'); }

if (longer)
      text.Printf(wxT("%.2f%cB"), display_number, units); // Show 2 decimal places in the statusbar, where there's more room
 else text.Printf(wxT("%.1f%c"),  display_number, units);

return text;
}

wxString DoEscape(wxString& filepath, const wxString& target)  // Escapes e.g. spaces or quotes within e.g. filepaths
{
wxString replacement =(wxT('\\')); replacement << target;
size_t index, nStart = 0;    
while (1)
  { index = filepath.find(target, nStart);                    // This std::string find starts at nStart, so we can reset this after each Find
    if (index == wxString::npos) break;                       // npos means no more found
    if (filepath.GetChar(index-1) != wxT('\\'))               // If it's not already escaped, do it, and change nStart accordingly.  +2, one for the \ one for the space
      { filepath.replace(index, 1, replacement); nStart = index+2; }
      else nStart = index+1;                                  // Already escaped so just inc over the it
  }
return filepath;
}

wxString EscapeQuote(wxString& filepath)    // Often, we use single quotes around a filepath to avoid problems with spaces etc.
{                                           // However this CAUSES a problem with apostrophes!  This function goes thru filepath, Escaping any we find
return DoEscape(filepath, wxT("\'"));
}

wxString EscapeQuoteStr(const wxString& filepath)
{
wxString fp(filepath);
return EscapeQuote(fp);
}

wxString EscapeSpace(wxString& filepath)    // Escapes spaces within e.g. filepaths
{
return DoEscape(filepath, wxT(" "));
}

wxString EscapeSpaceStr(const wxString& filepath)
{
wxString fp(filepath);
return EscapeSpace(fp);
}

wxString StrWithSep(const wxString& path)  // Returns path with a '/' if necessary
{
if (path.empty()) return path;

wxString str(path);
if (path.Last() != wxFILE_SEP_PATH) str << wxFILE_SEP_PATH;
return str;
}

wxString StripSep(const wxString& path)  // Returns path without any terminal '/' (modulo root)
{
if (path.empty()) return path;

wxString str(path);
while ((str.Last() == wxFILE_SEP_PATH) && (str != wxT("/"))) str.RemoveLast();
return str;
}

bool IsDescendentOf(const wxString& dir, const wxString& child) 
{
wxCHECK_MSG(dir != child, false, wxT("We shouldn't be able to get here if the strings are equal"));

if (StripSep(dir).Len() > StripSep(child).Len()) return false;

if (!child.StartsWith(dir)) return false;

  // OK, there are 2 possibilities: 1) /path/to/foo and /path/to/foo/bar (is a child) 2) /path/to/foo and /path/to/foobar (isn't)
wxFileName fndir(StrWithSep(dir)); wxFileName fnchild(StrWithSep(child));
wxArrayString parentdirs(fndir.GetDirs()); wxArrayString childdirs(fnchild.GetDirs());
if (parentdirs.Item(fndir.GetDirCount()-1) != childdirs.Item(fndir.GetDirCount()-1)) return false; // /path/to/foo and /path/to/foobar

wxCHECK_MSG(fndir.GetDirCount() != fnchild.GetDirCount(), false, wxT("We shouldn't be able to get here if the strings are equal")); // Let's be paranoid and check again
return true; // /path/to/foo and /path/to/foo/bar
}


wxString TruncatePathtoFitBox(wxString filepath, wxWindow* box)  // Returns a string small enough to be displayed in box, with ../ prepend if needed
{
if (filepath.IsEmpty() || box==NULL) return wxEmptyString;

int x, y, clx, cly;
box->GetClientSize(&clx, &cly); box->GetTextExtent(filepath, &x, &y);
if (x > clx)                                                    // If the textextent is greater than the clientsize, 
  { wxString dots(wxT(".../")); int dx, dy; box->GetTextExtent(dots, &dx, &dy);  // make a string with .../ in it, and get *its* textextent too
    filepath = filepath.AfterFirst(wxFILE_SEP_PATH);            // We know it'll begin with /
    do
      { if (filepath.Find(wxFILE_SEP_PATH) == wxNOT_FOUND) return filepath; // Better an overlong string than an empty one
        filepath = filepath.AfterFirst(wxFILE_SEP_PATH);        // Now go thru the path, progressively chopping off its head until it plus dots will fit
        box->GetTextExtent(filepath, &x, &y);
      }
     while  ((x+dx) > clx);
    return (dots + filepath);                                   // Return the truncated filepath, preceeded by ../
  }
return filepath;                                                // Return the untruncated version, as it fitted anyway
}

wxWindow* InsertEllipsizingStaticText(wxString filepath, wxWindow* original, int insertAt/* = -1*/)  // Insert a self-truncating wxStaticText
{
if (filepath.empty() || !original) return (wxWindow*)NULL;

// Since wx3.0 a wxStaticText can self-truncate. However wxCrafter can't currently set that styleflag, and it can't be applied retrospectively
// So hide the XRC-loaded control and use a new one. Not the most elegant solution, but it works
wxStaticText* ElipsizePathStatic = new wxStaticText(original->GetParent(), wxID_ANY, filepath, wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_START);
wxSizer* psizer = original->GetContainingSizer();
if (psizer)
 { if (insertAt > -1) psizer->Insert(insertAt, ElipsizePathStatic, 0, wxEXPAND);
     else psizer->Add(ElipsizePathStatic, 0, wxEXPAND);
   psizer->Layout();
 }
original->Hide();
return ElipsizePathStatic;
}

void QueryWrapWithShell(wxString& command)  // Wraps command with sh -c if appropriate
{
if (command.Contains(wxT("sh -c"))) return;     // We don't need two of them

const wxChar* relevantchars = wxT("~|><*?;");
if ((command.find_first_of(relevantchars) == wxString::npos) && !command.Contains(wxT("&&")))
  return;  // If the command contains none of these chars (nor the string "&&") we don't need a wrapper
       
EscapeQuote(command);                           // We're going to wrap with sh -c '  ' to avoid clashes with double-quotes, so escape any single-quotes first
command = wxT("sh -c \'") + command; command << wxT('\''); 
}

void QuoteExcludingWildcards(wxString& command)  // Wraps command with " " if appropriate, excluding any terminal globs. Needed for grep
{
if (command.IsEmpty()) return;
if (command.find_first_of(wxT('\"')) != wxString::npos) return;     // If there's a double-quote in the command, leave well alone: surrounding by " " will be guaranteed to fail

const wxChar* relevantchars = wxT("' ");
if (command.find_first_of(relevantchars) == wxString::npos) return; // If there isn't a single-quote or space, nothing needs be done

wxString path(command.BeforeLast(wxFILE_SEP_PATH)); if (path.IsEmpty()) return;  // If there isn't a '/', we're buggered anyway so ignore
wxString name(command.AfterLast(wxFILE_SEP_PATH)); path << wxFILE_SEP_PATH;

const wxChar* globs = wxT("?*");           // Now see if there's a wildcard in *name* (if it were in path (?allowed), quoting would tame its wildness)
if (name.find_first_of(globs) != wxString::npos)                    // If one is found, double-quote path, but not name
  { command = wxT("\\\"") + path; command << wxT("\\\"") << name; }
 else
  { command = wxT("\\\"") + command; command << wxT("\\\""); }      // Otherwise escape-quote the lot
}

wxString MakeTempDir(const wxString& fname/*= wxT("")*/)  // Makes a unique dir, in /tmp/ unless we were passed a filepath
{
wxString tempdir, name(fname);

if (name.empty() || (name.GetChar(0) != wxFILE_SEP_PATH))
  { // If we're about to try to create in /tmp/, make sure there's an existing /tmp/4Pane as mkdtemp isn't recursive
    wxString tmp = wxStandardPaths::Get().GetTempDir();
    if (!tmp.empty() && !wxDirExists(tmp + wxT("/4Pane/")))
      wxMkdir(tmp + wxT("/4Pane/"));
  }

if (name.empty()) name = wxStandardPaths::Get().GetTempDir() + wxT("/4Pane/4Pane.");
 else if (name.GetChar(0) != wxFILE_SEP_PATH) name = wxStandardPaths::Get().GetTempDir() + wxT("/4Pane/") + name;
name << wxT("XXXXXX");

#if wxVERSION_NUMBER < 2900
  tempdir = wxString(mkdtemp(const_cast<char*>((const char*)name.mb_str(wxConvUTF8))), wxConvUTF8);
#else
  tempdir = wxString(mkdtemp(name.char_str()), wxConvUTF8);
#endif
if (tempdir.empty())  wxLogWarning(_("Failed to create a temporary directory")); 
 else tempdir << wxT('/');

return tempdir;
}

  // Returns a stock icon if available and so configured, otherwise the 4Pane builtin one
wxBitmap GetBestBitmap(const wxArtID& artid, const wxArtClient& client, const wxBitmap& fallback_xpm)
{
wxBitmap bm;

if (USE_STOCK_ICONS)
  { bm = wxArtProvider::GetBitmap(artid, client);
    if (bm.IsOk()) return bm;
  }

bm = fallback_xpm;
return bm;
}

wxString GetCwd() // Protects the wx function with wxLogNull
{
wxLogNull avoid_Failed_to_get_cwd_warning;
return wxGetCwd();
}

bool SetWorkingDirectory(const wxString& dir) // Protects the wx function with wxLogNull
{
wxLogNull avoid_Cant_set_cwd_warning;
return wxSetWorkingDirectory(dir);
}

bool LoadPreviousSize(wxWindow* tlwin, const wxString& name) // Retrieve and apply any previously-saved size for the top-level window
{
if (!tlwin || name.empty()) return false;

wxString address = wxT("/ToplevelWindowSize/") + name;
long height = wxConfigBase::Get()->Read(address + wxT("/ht"), -1l); // Restore any saved size
long width = wxConfigBase::Get()->Read(address + wxT("/wd"), -1l);
if (height != -1 && width != -1)
  { tlwin->SetSize(width, height); return true; }

return false;
}

void SaveCurrentSize(wxWindow* tlwin, const wxString& name) // The 'save' for LoadPreviousSize()
{
if (!tlwin || name.empty()) return;

wxString address = wxT("/ToplevelWindowSize/") + name;
wxSize sz = tlwin->GetSize();
wxConfigBase::Get()->Write(address + wxT("/ht"), sz.GetHeight());
wxConfigBase::Get()->Write(address + wxT("/wd"), sz.GetWidth());
}

// The next 2 functions are compatability ones: a change in wx3.1.2 gave e.g. wxFONTWEIGHT_NORMAL a value of 400; previously it was 90
#if wxVERSION_NUMBER > 3101
  wxFontWeight LoadFontWeight(const wxString& configpath)
#else
  int LoadFontWeight(const wxString& configpath)
#endif
{
int wt = wxConfigBase::Get()->Read(configpath + "weight", (long)-1); // See if there's a <wx3.1.2 value stored
int wt32 = wxConfigBase::Get()->Read(configpath + "weight_wx3.2", (long)-1); // and a >wx3.1.1 one
#if wxVERSION_NUMBER > 3101
  if (wt32 == -1)
    { switch(wt)
        { case 91: wt32 = wxFONTWEIGHT_LIGHT; break;
          case 92: wt32 = wxFONTWEIGHT_BOLD;  break;
          default: wt32 = wxFONTWEIGHT_NORMAL;
        }
      wxConfigBase::Get()->Write(configpath + "weight_wx3.2", (long)wt32);
    }
  return (wxFontWeight)wt32;
#else // < wx3.1.2
  if ((wt == -1) || (wt < 90) || (wt > 92)) // Either no previous value, or a value set by an unfixed >wx3.1.1 instance
    { switch(wt32)
        { case 300: wt = wxFONTWEIGHT_LIGHT; break;
          case 700: wt = wxFONTWEIGHT_BOLD;  break;
          default:  wt = wxFONTWEIGHT_NORMAL;
        }
      wxConfigBase::Get()->Write(configpath + "weight", (long)wt);
    }
  return wt;
#endif
}

void SaveFontWeight(const wxString& configpath, int fontweight)
{
int wt, wt32;
#if wxVERSION_NUMBER > 3101
  wxConfigBase::Get()->Write(configpath + "weight_wx3.2", (long)fontweight);
  if (wxConfigBase::Get()->Exists(configpath + "weight"))
    { switch(fontweight)  // There's a pre-wx3.1.2 entry in the config, so update that too
        { case 300: wt = 91; break;
          case 700: wt = 92; break;
          default:  wt = 90;
        }
      wxConfigBase::Get()->Write(configpath + "weight", (long)wt);
    }
#else
  wxConfigBase::Get()->Write(configpath + "weight", (long)fontweight);
  switch(fontweight) // Future-proof by saving in >3.1.1 format too
    { case 91: wt32 = 300; break;
      case 92: wt32 = 700; break;
      default: wt32 = 400;
    }
  wxConfigBase::Get()->Write(configpath + "weight_wx3.2", (long)wt32);
#endif
}

#include <sys/utsname.h>
#include "wx/tokenzr.h"

bool GetFloat(wxString& datastr, double* fl, size_t skip)  // Parses datastr to find a float. Skip means skip over the first n of them
{
wxString str;

wxStringTokenizer tknr(datastr); size_t count = tknr.CountTokens();
for (size_t c=0; c < count; ++c)  
  { wxString token = tknr.GetNextToken();
    size_t index = token.find_first_of(wxT("0123456789"));  // We need to cope with "foo bar 1.2.3"
    if (index == wxString::npos) continue;
    if (skip) { --skip; continue; }                         // Skip allows us to cope with  "Foo i386 version 2.3"
    
    str = token.Mid(index); break;
    }
  
if (str.IsEmpty()) return false;                            // We're digitless

wxStringTokenizer tkn(str, wxT("."));                       // We need to cope with "1.2.3"
wxString token = tkn.GetNextToken();
if (token.ToDouble(fl) && tkn.HasMoreTokens())              // Because of the above, the first token really ought to be a digit or 3
  { token += wxT('.') + tkn.GetNextToken();                 // We've got the 1, so add '.' plus the next digit, and ignore any others
    token.ToDouble(fl);
  }

return true;
}

bool KernelLaterThan(wxString minimum)  // Tests whether the active kernel is >= the parameter
{
struct utsname krnl;
if (uname(&krnl) == -1)  return false;                      // If -1 the function call failed

wxString kernel(krnl.release, wxConvLocal);  
wxStringTokenizer knl(kernel, wxT(".")); wxStringTokenizer mnl(minimum, wxT("."));  // Tokenise both
int knlcount = knl.CountTokens(), minimalcount = mnl.CountTokens();                 // Count tokens;  we may need later to check for equality
while (knl.HasMoreTokens() && mnl.HasMoreTokens())
  { wxString kerneltkn = knl.GetNextToken(), minimaltkn = mnl.GetNextToken();       // Get first token of each & compare
    long kernellong, minimallong; 
    if (!kerneltkn.ToLong(&kernellong) || !minimaltkn.ToLong(&minimallong))  return false;  // If either conversion to numeral failed, abort
    if (kernellong == minimallong) continue;                // If this number==, check the next pair of tokens
    
    return  kernellong > minimallong;                       // We have an inequality.  Return 0 or 1, depending on which is larger
  }
        // If we're here, 1 or both strings has run out.  We may be in a 2.4.10 versus 2.4.10 situation, or a 2.4.10 versus 2.4 situation
return knlcount >= minimalcount;                            // If the kernel has >= numbers, it passes
}

wxColour GetSaneColour(wxWindow* win, bool bg, wxSystemColour type) // Returns a valid colour that's fit-for-purpose. Works around theme issues with early gtk3 versions
{
#if wxVERSION_NUMBER < 3000 || !defined(__WXGTK3__)
  wxUnusedVar(win);
  return wxSystemSettings::GetColour(type);
#else
  // The problem this code tries to fix is exemplified by gtk+ 3.4.2 in debian wheezy. This gives wxTreeCtrls a grey background by default,
  // and always seems to return black from GetDefaultAttributes() and wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW) and similar.
  // This no longer happens in fedora 20's gtk+ 3.10.
  if (type == wxSYS_COLOUR_WINDOW) // All the current callers are treectrls, which in wx3 use wxSYS_COLOUR_LISTBOX (doing it here avoids lots of #ifs)
    type = wxSYS_COLOUR_LISTBOX;
  wxColour col, deflt = wxSystemSettings::GetColour(type);
  if (win)
    { wxVisualAttributes va = win->GetDefaultAttributes();
      col = bg ? va.colBg : va.colFg;
    }

  if (!col.IsOk())
    col = deflt;

  if (col.Red() + col.Blue() + col.Green() == 0)
    col = (type == wxSYS_COLOUR_BTNFACE) ? *wxLIGHT_GREY : *wxWHITE; // This won't be right for a dark theme, but how to detect one if wxSystemSettings gives wrong answers?
  
  return col;
#endif
}

void UnexecuteImages(const wxString& filepath) // See if it's an image or text file; if so, ensure it's not executable
{
FileData fd(filepath);
wxCHECK_RET(fd.IsValid(), wxT("Passed an invalid file"));
if (fd.IsRegularFile())
  { // See if it's likely to be an image or a text file; just quick'n'simple, it's not important enough for mimetypes or wxImage::CanRead
  wxString ext = filepath.AfterLast(wxT('.')).Lower();
  if (!(ext==wxT("png") || ext==wxT("jpg") || ext==wxT("jpeg") || ext==wxT("bmp") || ext==wxT("tiff") || ext==wxT("txt")))
    return;

  static const mode_t mask = ~(S_IXUSR | S_IXGRP | S_IXOTH) & 07777;
  fd.DoChmod(fd.GetPermissions() & mask); // No need to check if there's a change; it happens inside the call
  }
 else if (fd.IsDir())
  { wxDir dir(filepath);                              // Use wxDir to deal sequentially with each child file & subdir
    if (!dir.IsOpened())  return;
    if (!dir.HasSubDirs() && !dir.HasFiles()) return; // If the dir as no progeny, we're OK

    wxString filename;
    bool cont = dir.GetFirst(&filename, wxEmptyString, wxDIR_FILES | wxDIR_DIRS |wxDIR_HIDDEN);
    while  (cont)
      { wxString file = wxFILE_SEP_PATH + filename;
        FileData stat(filepath + file);
        if (stat.IsRegularFile() || stat.IsDir())     // If it's a normal file or a dir, recurse to process. If not, we don't care
            UnexecuteImages(filepath + file);
        cont = dir.GetNext(&filename);
      }
  }
}

void KeepShellscriptsExecutable(const wxString& filepath, size_t perms) // If foo.sh is rwx, it makes sense to keep it that way in the copy
{
FileData fd(filepath);
wxCHECK_RET(fd.IsValid(), wxT("Passed an invalid file"));
if (!(perms & S_IXUSR)) return; // Nothing to do; the original file wasn't exec

if (fd.IsRegularFile())
  { if (!(filepath.AfterLast(wxT('.')).Lower() == wxT("sh")))
      return; // We don't want to impose exec permissions on all files just because they came from a FAT partition

  fd.DoChmod(perms); // No need to check if there's a change; it happens inside the call
  }
  // Don't descend into a passed dir; for a Move we'd have no way to check what the originals' permissions were 
}

bool IsThreadlessMovePossible(const wxString& origin, const wxString& destination) // Can origin be moved to dest using rename, rather than copy/delete?
{
FileData orig(origin), dest(destination);
wxCHECK_MSG(orig.IsValid() && dest.IsValid(), false, wxT("Passed invalid strings"));

if (orig.GetDeviceID() != dest.GetDeviceID())
  return false; // Can't rename between different filesystems

  // See if we're moving a symlink with a relative target, which we don't want it to retain. If so, we'll need to use Paste()
if (!RETAIN_REL_TARGET && ContainsRelativeSymlinks(orig.GetFilepath()))
  return false;

return true;
}

wxString LocateLibrary(const wxString& lib) // Search ldconfig output to find a lib's filepath. 'lib' can/will be a partial name e.g. 'rsvg'
{
wxCHECK_MSG(!lib.empty(), "", "Empty parameter");

wxArrayString output, errors;
#ifdef __linux__
long ans = wxExecute("sh -c \"/sbin/ldconfig -p | grep " + lib + '\"', output,errors);
#else // Maybe FreeBSD etc
long ans = wxExecute("sh -c \"/sbin/ldconfig -r | grep " + lib + '\"', output,errors);
#endif
if (ans != 0 || output.IsEmpty()) return "";

wxString fpath = output.Item(0).AfterLast(' ');
FileData stat(fpath);
if (stat.IsValid())
  return fpath;

return "";
}

//-----------------------------------------------------------------------------------------------------------------------
IMPLEMENT_DYNAMIC_CLASS(FileDirDlg, wxDialog)

FileDirDlg::FileDirDlg(wxWindow* parent, int style/*FD_MULTIPLE | FD_SHOWHIDDEN*/, wxString defaultpath/*=wxEmptyString*/, wxString label/*=_("File and Dir Select Dialog")*/)
                    : wxDialog(parent, wxNewId(), label)
{
SHCUT_HOME = wxNewId();

wxBoxSizer* mainsizer = new wxBoxSizer(wxVERTICAL);

wxBoxSizer* sizer1 = new wxBoxSizer(wxHORIZONTAL);
wxBitmap bmp = wxArtProvider::GetBitmap(wxART_GO_HOME);
wxBitmapButton* Home = new wxBitmapButton(this, SHCUT_HOME, bmp);
sizer1->Add(0,0, 4); sizer1->Add(Home, 0, wxRIGHT, 30); sizer1->Add(0,0, 1);
mainsizer->Add(sizer1, 0, wxTOP|wxBOTTOM|wxEXPAND, 10);

wxStaticBoxSizer* sizer2 = new wxStaticBoxSizer(new wxStaticBox(this, -1, wxEmptyString), wxVERTICAL);
if (defaultpath.IsEmpty()) defaultpath = GetCwd();

int GDC_style = wxDIRCTRL_3D_INTERNAL;
#if wxVERSION_NUMBER > 2900
  if (style & FD_MULTIPLE) GDC_style |= wxDIRCTRL_MULTIPLE;
#endif

GDC = new wxGenericDirCtrl(this, wxID_ANY, defaultpath, wxDefaultPosition, wxDefaultSize, GDC_style);

#if wxVERSION_NUMBER < 2901
  int treestyle = GDC->GetTreeCtrl()->GetWindowStyle();
  if (style & FD_MULTIPLE) treestyle |= wxTR_MULTIPLE;
  GDC->GetTreeCtrl()->SetWindowStyle(treestyle);
#endif
GDC->ShowHidden(style & FD_SHOWHIDDEN);

#if wxVERSION_NUMBER > 2402 && wxVERSION_NUMBER < 2600
  #ifdef __UNIX__
        GDC->SetFilter(wxT("*"));
  #else
      GDC->SetFilter(wxT("*.*"));
  #endif
#endif
sizer2->Add(GDC, 1, wxEXPAND);
mainsizer->Add(sizer2, 1, wxLEFT|wxRIGHT|wxEXPAND, 15);

wxBoxSizer* sizer3 = new wxBoxSizer(wxHORIZONTAL);
wxBoxSizer* sizer4 = new wxBoxSizer(wxVERTICAL);
Text = new wxTextCtrl(this, wxID_ANY);
wxCheckBox* ShowHidden =  new wxCheckBox(this, wxID_ANY, _("Show Hidden"));
ShowHidden->SetValue(style & FD_SHOWHIDDEN);
sizer4->Add(Text, 0, wxTOP|wxBOTTOM|wxEXPAND, 3);
sizer4->Add(ShowHidden, 0, wxALIGN_CENTRE);

wxBoxSizer* sizer5 = new wxBoxSizer(wxVERTICAL);
wxButton* OK = new wxButton(this, wxID_OK);
wxButton* CANCEL = new wxButton(this, wxID_CANCEL);
sizer5->Add(OK);
sizer5->Add(CANCEL, 0, wxTOP, 5);
sizer3->Add(sizer4, 1);
sizer3->Add(sizer5, 0, wxLEFT, 15);
mainsizer->Add(sizer3, 0, wxALL|wxEXPAND, 15);

SetSizer(mainsizer);
mainsizer->Layout();

wxSize size = wxGetDisplaySize();
SetSize(size.x / 2, size.y / 2);

filesonly =  style & FD_RETURN_FILESONLY;

  // Use Connect() here instead of an event table entry, as otherwise an event is fired before the dialog is created and GDC and Text initialised
Connect(wxID_ANY, wxEVT_COMMAND_TREE_SEL_CHANGED, (wxObjectEventFunction)&FileDirDlg::OnSelChanged);
Connect(wxID_ANY, wxEVT_COMMAND_CHECKBOX_CLICKED,  (wxObjectEventFunction)&FileDirDlg::OnHidden);
Connect(SHCUT_HOME, wxEVT_COMMAND_BUTTON_CLICKED,  (wxObjectEventFunction)&FileDirDlg::OnHidden);
#if !defined(__WXGTK3__)
  GDC->GetTreeCtrl()->Connect(wxID_ANY,wxEVT_ERASE_BACKGROUND,  (wxObjectEventFunction)&FileDirDlg::OnEraseBackground);
#endif
}

void FileDirDlg::OnHome(wxCommandEvent& WXUNUSED(event))
{
wxString home = wxGetHomeDir(); GDC->SetPath(home);
}

void FileDirDlg::OnHidden(wxCommandEvent& event)
{
GDC->ShowHidden(event.IsChecked());
}

void FileDirDlg::OnSelChanged(wxTreeEvent& WXUNUSED(event))
{
#if wxVERSION_NUMBER > 2900
  if (GDC->HasFlag(wxDIRCTRL_MULTIPLE))           // From 2.9.0 GetPath() etc assert if wxTR_MULTIPLE
    { wxArrayString filepaths; size_t count = GetPaths(filepaths); // GetPaths() handles filepath internally
       if (count) filepath = filepaths[0];
    }
   else
    {
#endif
  if (filesonly) filepath = GDC->GetFilePath();   // If we don't want dirs, use GetFilePath() which ignores them
    else filepath = GDC->GetPath();               // Otherwise GetPath() which returns anything
#if wxVERSION_NUMBER > 2900
    }
#endif

Text->SetValue(filepath);
}

size_t FileDirDlg::GetPaths(wxArrayString& filepaths)  // Get multiple filepaths
{
size_t count;
wxArrayTreeItemIds selectedIds;

count = GDC->GetTreeCtrl()->GetSelections(selectedIds);

for (size_t c=0; c < count; ++c)
  { wxDirItemData* data = (wxDirItemData*) GDC->GetTreeCtrl()->GetItemData(selectedIds[c]);
    if (filesonly && wxDirExists(data->m_path)) continue;  // If we only want files, skip dirs
    filepaths.Add(data->m_path);
  }

return filepaths.GetCount();
}

#if !defined(__WXGTK3__)
void FileDirDlg::OnEraseBackground(wxEraseEvent& event)  // This is only needed in gtk2 under kde and brain-dead theme-manager, but can cause a blackout in some gtk3(themes?)
{
wxClientDC* clientDC = NULL;  // We may or may not get a valid dc from the event, so be prepared
if (!event.GetDC()) clientDC = new wxClientDC(GDC->GetTreeCtrl());
wxDC* dc = clientDC ? clientDC : event.GetDC();

wxColour colBg = GetBackgroundColour();  // Use the chosen background if there is one
if (!colBg.Ok()) colBg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);

#if wxVERSION_NUMBER < 2900
  dc->SetBackground(wxBrush(colBg, wxSOLID));
#else
  dc->SetBackground(wxBrush(colBg, wxBRUSHSTYLE_SOLID));
#endif
dc->Clear();

if (clientDC) delete clientDC;
}
#endif
//-----------------------------------------------------------------------------------------------------------------------

#ifdef __linux__
#include <pty.h>
#else
#include <termios.h>
#include <libutil.h>
#endif
#include <errno.h>
#include <sys/wait.h>

#include "wx/cmdline.h"
#include "wx/apptrait.h"


// helper class for storing arguments as char** array suitable for passing to
// execvp(), whatever form they were passed to us
class ArgsArray
{
public:
    ArgsArray(const wxArrayString& args)
    {
        Init(args.size());

        for ( int i = 0; i < m_argc; i++ )
        {
#if wxVERSION_NUMBER < 2900
                m_argv[i] = new char[args[i].length() + 1];
                strcpy(m_argv[i], (const char*)args[i].mb_str());
#else
            m_argv[i] = wxStrdup(args[i]);
#endif
        }
    }

#if wxUSE_UNICODE
    ArgsArray(wchar_t **wargv)
    {
        int argc = 0;
        while ( wargv[argc] )
            argc++;

        Init(argc);

        for ( int i = 0; i < m_argc; i++ )
        {
#if wxVERSION_NUMBER > 2802
            m_argv[i] = wxSafeConvertWX2MB(wargv[i]).release();
#else
            m_argv[i] = wxConvertWX2MB(wargv[i]).release();
#endif
        }
    }
#endif // wxUSE_UNICODE

    ~ArgsArray()
    {
        for ( int i = 0; i < m_argc; i++ )
        {
            free(m_argv[i]);
        }

        delete [] m_argv;
    }

    operator char**() const { return m_argv; }

private:
    void Init(int argc)
    {
        m_argc = argc;
        m_argv = new char *[m_argc + 1];
        m_argv[m_argc] = NULL;
    }

    int m_argc;
    char **m_argv;

    //wxDECLARE_NO_COPY_CLASS(ArgsArray);
};

int ReadLine(int fd, wxString& line)
{
wxMemoryBuffer buf;

while (true)
  { char c;
    int nread = read(fd, &c, 1);                 // Get a char from the input
    
    if (nread == -1)
      { if (buf.GetDataLen())                   // There may be a problem: salvage the data we've already read
          { wxString ln((const char*)buf.GetData(), wxConvUTF8, buf.GetDataLen());
            if (ln.Len()) 
                line = ln;
          }
        if ((errno == EAGAIN) || (errno == 0)) return 2; // No more data is available atm. That may be because a line doesn't end in \n e.g. 'Password: '

        if (errno == EIO) return -2;            // This is the errno we get when the slave has died, presumably successfully

        wxLogDebug(wxT("Error %d while trying to read input"), errno); return -1;
      }

    if (nread == 0) return 0;                   // We've got ahead of the available data
    
    if (c == wxT('\n')) 
      { wxString ln((const char*)buf.GetData(), wxConvUTF8, buf.GetDataLen()); // Convert the line to utf8
        line = ln;
        return true;
      }

    buf.AppendByte(c);                          // Otherwise just append the char and ask for more please
  }
}



long ExecInPty::ExecuteInPty(const wxString& cmd)
{
if (cmd.empty()) return ERROR_RETURN_CODE;

if (wxGetOsDescription().Contains(wxT("FreeBSD"))) // FreeBSD's forkpty() hangs
  { if (GetCallerTextCtrl())
      InformCallerOnTerminate();
    return ERROR_RETURN_CODE;
  }

#if wxVERSION_NUMBER < 2900
    ArgsArray argv(wxCmdLineParser::ConvertStringToArgs(cmd));
#else
    ArgsArray argv(wxCmdLineParser::ConvertStringToArgs(cmd, wxCMD_LINE_SPLIT_UNIX));
#endif

GetOutputArray().Clear(); GetErrorsArray().Clear();
static size_t count;
count = 0;                                       // Set it here, not above: otherwise it'll retain its value for subsequent calls

int fd;
pid_t pid = forkpty(&fd, NULL, NULL, NULL);
if (pid == -1)
  return CloseWithError(0, wxT("Failed to create a separate process"));

if (pid == 0)                                   // The child process
  { setsid();
  
    struct termios tos;                         // Turn off echo
    tcgetattr(STDIN_FILENO, &tos);
    tos.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
    tos.c_oflag &= ~(ONLCR);
    tcsetattr (STDIN_FILENO, TCSANOW, &tos);

    if (int ret =  execvp(*argv, argv) == -1) 
      return CloseWithError(fd, wxString::Format(wxT("program exited with code %i\n"), ret));
  }

                                                // The parent process

int fl; if ((fl = fcntl(fd, F_GETFL, 0)) == -1)  fl = 0;
fcntl(fd, F_SETFL, fl | O_NONBLOCK);            // Make non-blocking   

int status, ret = 1;
 
fd_set fd_in, fd_out;
do
  { struct timeval tv;
    tv.tv_sec = 0; tv.tv_usec = 20000;
    
    FD_ZERO(&fd_in); FD_ZERO(&fd_out);
    FD_SET(fd, &fd_in); FD_SET(fd, &fd_out);

    int rc = select(fd + 1, &fd_in, &fd_out, NULL, &tv);
    if (rc == -1)
      return CloseWithError(fd, wxString::Format(wxT("Error %d on select()"), errno));
  
    if (!rc) continue;

    if (FD_ISSET(fd, &fd_in))
      { wxString line;
        ret = ReadLine(fd, line);

        if (!ret) continue;                     // We've temporarily read all the data, but there should be more. Reselect

        if (ret == -1) 
          { if (line.Len()) GetOutputArray().Add(line);   // There's a problem, but salvage any data we've already read
            GetErrorsArray() = GetOutputArray();
            return CloseWithError(fd, wxT("")); 
          }

        // If we're here, ret was >0. 1 means there's a full line; 2 that we got EAGAIN, which may mean it's waiting for a password
        if (FD_ISSET(fd, &fd_out) && line.Lower().Contains(wxT("password")) && !line.Lower().Contains(wxT("incorrect")))
          { wxString pw(PasswordManager::Get().GetPassword(cmd));
            if (pw.empty()) return CloseWithError(fd, wxT(""), CANCEL_RETURN_CODE); // Presumably the user cancelled

            pw << wxT('\n');
            if (write(fd, pw.mb_str(wxConvUTF8), pw.Len()) == -1)
              return CloseWithError(fd, wxT("Writing the string failed"));
          }
         else   // Either a single 'su' failure (you only get 1 chance) or 3 'sudo' ones
           if ((line.Lower().Contains(wxT("authentication failure"))) || (line.Lower().Contains(wxT("incorrect password"))))
              { PasswordManager::Get().ForgetPassword();  // Forget the failed attempt, or it'll automatically be offered again and again and...
                GetOutputArray().Add(line);               // Store the 'fail' line so that it's available to show the user
              }
         else
           if (line.Lower().Contains(wxT("try again")))   // The first or second sudo failure
              PasswordManager::Get().ForgetPassword();
         else
          { if (!(GetOutputArray().IsEmpty() && (line == wxT("\r"))))  // When a password is accepted, su emits a \r for some reason
              GetOutputArray().Add(line);
          }

        continue;
      }

    if (FD_ISSET(fd, &fd_out))
      { if (!GetCallerTextCtrl())
          { // Though there shouldn't be any input required for these commands (apart from the password, handled above)
            // for some reason we do land here several times per command. So we can't just abort, and 'continue' seems to work
            // Nevertheless limit the number of tries, just in case someone does try an input-requiring command
            if (++count < 1000)
              { wxMilliSleep(10);
                if (!(count % 10)) wxYieldIfNeeded();
#ifdef __GNU_HURD__
                if (!(count % 30))
                  break; // In hurd (currently) we often linger here until 1000, and so return -1. idk why, but avoid the situation
#endif
                continue;
              }

           return CloseWithError(fd, wxT("Expecting input that we can't provide. Bail out to avoid a hang"));
          }

        if (!m_inputArray.empty())
          { wxString line = m_inputArray.Item(0);
            m_inputArray.RemoveAt(0);
            line << wxT('\n');                            // as this will have been swallowed by the textctrl
            if (write(fd, line.mb_str(wxConvUTF8), line.Len()) == -1)
              return CloseWithError(fd, wxT("Writing the string failed"));

            count = 0;                                    // reset the anti-hang device
            wxYieldIfNeeded();
          }
         else
          { if (++count < 10000)
              { wxMilliSleep(10);
                if (!(count % 10)) wxYieldIfNeeded();
                continue;
              }

           return CloseWithError(fd, wxT("\nTimed out\n")); // If we're here, bail out to avoid hanging
          }
      }
  }
  while (ret >= 0);

while (!GetOutputArray().IsEmpty())                       // Remove any terminal emptyish lines
  { wxString last(GetOutputArray().Last());
    if (last.empty() || (last == wxT("\n")) || (last == wxT("\r"))) GetOutputArray().RemoveAt(GetOutputArray().GetCount()-1); 
      else break;
  }

waitpid(pid, &status, 0);

close(fd);

if (WIFEXITED(status))
  { int retcode = WEXITSTATUS(status);
    if (retcode > 0) { GetErrorsArray() = GetOutputArray(); GetOutputArray().Clear(); } // If there's been a problem, it makes more sense for the output to be here
    if (GetCallerTextCtrl())
      InformCallerOnTerminate();                          // Let the terminalem know we're finished

    return retcode;
  }

return ERROR_RETURN_CODE;  // For want of something more sensible. We should seldom reach here anyway; only from signals, I think
}

#include "Tools.h"

void ExecInPty::WriteOutputToTextctrl()
{
TerminalEm* term = wxDynamicCast(m_caller, TerminalEm);
wxCHECK_RET(term, wxT("Non-terminalem caller?!"));

for (size_t n=0; n < GetOutputArray().GetCount(); ++n)
  { wxString line = GetOutputArray().Item(n);
    if (!line.empty() && (line.Last() != wxT('\n')) && (line.Last() != wxT('\r')))  // Ensure the line is terminated
      line << wxT("\n");
    term->AddInput(line);
  }

for (size_t n=0; n < GetErrorsArray().GetCount(); ++n)    // Rarely there may be some
  { wxString line = GetErrorsArray().Item(n);
    if (!line.empty() && (line.Last() != wxT('\n')) && (line.Last() != wxT('\r')))  // Ensure the line is terminated
      line << wxT("\n");
    term->AddInput(line);
  }

GetOutputArray().Clear(); GetErrorsArray().Clear();
}

void ExecInPty::InformCallerOnTerminate() // Let the terminalem know we're finished
{
TerminalEm* term = wxDynamicCast(m_caller, TerminalEm);
wxCHECK_RET(term, wxT("Non-terminalem caller?!"));

WriteOutputToTextctrl();                                  // Show any remaining output
term->OnProcessTerminated();
}

int ExecInPty::CloseWithError(int fd, const wxString& msg, int errorcode/*=ERROR_RETURN_CODE*/)   // Close on error or empty password
{
if (fd) close(fd);                                        // We'd be passed 0 if the fork failed

if (m_caller)
  { TerminalEm* term = wxDynamicCast(m_caller, TerminalEm);
    wxCHECK_MSG(term, errorcode, wxT("Non-terminalem caller?!"));
    WriteOutputToTextctrl();                              // Show any remaining output
    if (!msg.empty())
      *m_caller << msg;

    term->OnProcessTerminated();
  }
 else
  wxLogDebug(msg);

return errorcode;
}

  // Convenient ways to call the ExecInPty class
long ExecuteInPty(const wxString& cmd, wxArrayString& output, wxArrayString& errors)
{
ExecInPty eip;
long ret = eip.ExecuteInPty(cmd);

output = eip.GetOutputArray();
errors = eip.GetErrorsArray();
return ret;
}

long ExecuteInPty(const wxString& cmd)  // For when the caller doesn't care about the output
{
wxArrayString output, errors;
return ExecuteInPty(cmd, output, errors);
}
//-------------------------------------------------------------------------------------------------------------------------------------------

PasswordManager* PasswordManager::ms_instance = NULL;

const wxString PasswordManager::GetPassword(const wxString& cmd, wxWindow* caller/*=NULL*/) // Returns the password, first asking the user for it if need be
{
if (cmd.empty()) return cmd;
if (!caller) caller = wxTheApp->GetTopWindow();

bool StorePassword = STORE_PASSWD;

wxString pw(GetPasswd());
if (!pw.empty())                                // We have it stored, so just return it
  { if (RENEW_PASSWD_TTL)
      m_timer.Start(PASSWD_STORE_TIME * 60 * 1000, true); // after first resetting its lifespan if desired
    return pw;
  }

wxDialog dlg;
wxXmlResource::Get()->LoadDialog(&dlg, caller, wxT("SuDlg"));
LoadPreviousSize(&dlg, wxT("SuDlg"));

wxTextCtrl* CommandTxt = XRCCTRL(dlg, "CommandTxt", wxTextCtrl);
wxCHECK_MSG(CommandTxt, wxT(""), wxT("Failed to load the command textctrl"));
CommandTxt->ChangeValue(cmd);

XRCCTRL(dlg, "StorePwCheck", wxCheckBox)->SetValue(StorePassword);

int ans = dlg.ShowModal();
SaveCurrentSize(&dlg, wxT("SuDlg"));
if (ans == wxID_OK)
  { StorePassword = XRCCTRL(dlg, "StorePwCheck", wxCheckBox)->IsChecked();
    if (StorePassword != STORE_PASSWD)
      { STORE_PASSWD = StorePassword; wxConfigBase::Get()->Write(wxT("/Misc/Store_Password"), STORE_PASSWD); }
    if (!StorePassword && m_timer.IsRunning()) m_timer.Stop();  // in case it was just changed

    pw = XRCCTRL(dlg, "PasswordTxt", wxTextCtrl)->GetValue();
    if (StorePassword && (PASSWD_STORE_TIME > 0)) // Store it, if we're supposed to
      SetPasswd(pw);                            // SetPasswd() ignores empty strings, so no need to check here
  }

return pw;  // which might be empty, if the user aborted or supplied an empty password
}

wxString docrypt(const wxString& password, const wxString& key)
{
wxString result;

for (size_t n=0; n < password.Len(); ++n)
    result << (wxChar)(((wxChar)password.GetChar(n) ^ (wxChar)key.GetChar(n)));

return result;
}

void PasswordManager::SetPasswd(const wxString& password) // Encrypts and stores the password
{
if (password.empty()) return;

wxString key;                                   // A gesture towards encryption. Create a random key the same length as the password
for (size_t n=0; n < password.Len(); ++n)
  key << wxString::Format(wxT("%c"), (char)(rand() % 50));
m_key = key;

m_password = docrypt(password, m_key);          // Store it, and
if (STORE_PASSWD && (PASSWD_STORE_TIME > 0))    // start the countdown to its deletion
  m_timer.Start(PASSWD_STORE_TIME * 60 * 1000, true);
}

wxString PasswordManager::GetPasswd() const // Decrypts and returns the password
{
return docrypt(m_password, m_key);
}

//-------------------------------------------------------------------------------------------------------------------------------------------

PasteThreadStatuswriter::PasteThreadStatuswriter()
  : m_cumsize(0), m_expectedsize(0)
{
m_timer.SetOwner(this);
Connect(wxEVT_TIMER, wxTimerEventHandler(PasteThreadStatuswriter::OnTimer), NULL, this);
}

void PasteThreadStatuswriter::PrintCumulativeSize()
{
static int count = 0;
static wxString throbbers[] = { wxT("*......"), wxT(".*....."), wxT("..*...."), wxT("...*..."), wxT("....*.."), wxT(".....*."), wxT("......*") };

wxString message(wxT("Pasting "));
message << throbbers[++count % 7];

MyFrame::mainframe->SetStatusText(message + wxString::Format(wxT("  %s of %s"), ParseSize(m_cumsize, true),  ParseSize(m_expectedsize, true)), 1);
}

ThreadsManager* ThreadsManager::ms_instance = NULL;

ThreadsManager::~ThreadsManager()
{
for (size_t n=0; n < m_sblocks.size(); ++n)
  delete m_sblocks.at(n);
m_sblocks.clear();
}

ThreadSuperBlock* ThreadsManager::StartSuperblock(const wxString& type)
{
ThreadSuperBlock* tsb;
if (type.Lower() == wxT("paste"))
  tsb = new PasteThreadSuperBlock(m_nextfree);
 else if (type.Lower() == wxT("move"))
  { tsb = new PasteThreadSuperBlock(m_nextfree);
    tsb->SetCollectorIsMoves(true);
  }
 else 
  tsb = new ThreadSuperBlock(m_nextfree);

m_sblocks.push_back(tsb);

return tsb;
}

ThreadBlock* ThreadsManager::AddBlock(size_t count, const wxString& type, int& firstthread, ThreadSuperBlock* tsb)
{
ThreadBlock* block = NULL;
wxCHECK_MSG(!type.empty(), block, wxT("A NULL type of block"));
wxCHECK_MSG(count > 0, block, wxT("An empty block"));

if (!tsb) // NULL means this will be a solitary block; but we still need a superblock to store it in
  tsb = StartSuperblock(type);

if (type.Lower() == wxT("paste"))
  { ThreadData* td = new ThreadData[count];
    block = new ThreadBlock(count, m_nextfree, td, tsb);
    tsb->StoreBlock(block);
  }
 // ...space for different thread types...
 else
  wxCHECK_MSG(false, block, wxT("Trying to add a block of unknown type")); // We really shouldn't get here

firstthread = (int)m_nextfree;
m_nextfree += count;

return block;
}

void ThreadsManager::AbortThreadSuperblock(ThreadSuperBlock* tsb)
{
wxCHECK_RET(tsb,  wxT("Passed a NULL superblock"));

for (size_t n=m_sblocks.size(); n > 0; --n)
  { if (m_sblocks.at(n-1) == tsb)
      { delete tsb;
        m_sblocks.erase(m_sblocks.begin() + n-1);
        return;
      }
  }
wxCHECK_RET(false, wxT("Couldn't find a matching tsb"));
}

bool ThreadsManager::IsActive() const
{
// Return true if there is a superblock, and it's incomplete. We're unlikely to be called in the gap between a block's creation and starting, but if we are it'll return true.
for (size_t n=0; n < m_sblocks.size(); ++n) 
  if (!m_sblocks.at(n)->IsCompleted())
    return true;

return false;
}

bool ThreadsManager::PasteIsActive() const
{
// Return true if there is an incomplete paste superblock
for (size_t n=0; n < m_sblocks.size(); ++n) 
  if (!m_sblocks.at(n)->IsCompleted() && dynamic_cast<PasteThreadSuperBlock*>(m_sblocks.at(n)))
    return true;

return false;
}

void ThreadsManager::CancelThread(int threadtype) const
{
// See if there's an active thread of the requested type
for (size_t n=0; n < m_sblocks.size(); ++n) 
  if (!m_sblocks.at(n)->IsCompleted())
    { if (/*(threadtype & ThT_text && dynamic_cast<TextThreadSuperBlock*>(m_sblocks.at(n)) *** future-proofing)
       || */(threadtype & ThT_paste && dynamic_cast<PasteThreadSuperBlock*>(m_sblocks.at(n))))
        m_sblocks.at(n)->AbortThreads();
    }
}

void ThreadsManager::SetThreadPointer(unsigned int ID, wxThread* thread)
{
int i = GetSuperBlockForID(ID);
wxCHECK_RET(i != wxNOT_FOUND, wxT("Unexpected thread ID"));

ThreadSuperBlock* tsb = m_sblocks.at(i);
tsb->SetThreadPointer(ID, thread);
}

void ThreadsManager::OnThreadProgress(unsigned int ID, unsigned int size)
{
int i = GetSuperBlockForID(ID);
wxCHECK_RET(i != wxNOT_FOUND, wxT("Unexpected thread ID"));

PasteThreadSuperBlock* ptsb = dynamic_cast<PasteThreadSuperBlock*>(m_sblocks.at(i));
wxCHECK_RET(ptsb, wxT("Got a paste progress event sent to a non-paste superblock"));

ptsb->ReportProgress(size);
}

void ThreadsManager::OnThreadCompleted(unsigned int ID, const wxArrayString& array, const wxArrayString& successes)
{
int i = GetSuperBlockForID(ID);
wxCHECK_RET(i != wxNOT_FOUND, wxT("Unexpected thread ID"));

ThreadSuperBlock* tsb = m_sblocks.at(i);
tsb->CompleteThread(ID, successes, array);

if (tsb->IsCompleted())
  { m_sblocks.erase(m_sblocks.begin()+i);
    delete tsb;
  }
}

int ThreadsManager::GetSuperBlockForID(unsigned int containedID) const
{
for (size_t n=0; n < m_sblocks.size(); ++n)
  if (m_sblocks.at(n)->Contains(containedID))
    return (int)n;

return wxNOT_FOUND;
}

//static
size_t ThreadsManager::GetCPUCount()
{
static int cpus = 0; // Make the currently-reasonable assumption that the number won't change dynamically

if (cpus < 1)
  { cpus = wxThread::GetCPUCount();
    if (cpus < 2) cpus = 1;
     else --cpus;    // Leave one free for non-thread use
  }

return (size_t)cpus;
}

PasteThreadSuperBlock::~PasteThreadSuperBlock()
{
for (size_t n=0; n < m_UnRedos.size(); ++n)
  delete m_UnRedos.at(n).first;

m_UnRedos.clear();
m_StatusWriter.PasteFinished();
}

ThreadSuperBlock::~ThreadSuperBlock()
{
for (size_t n=0; n < m_blocks.size(); ++n)
  delete m_blocks.at(n);
m_blocks.clear();
}

void ThreadSuperBlock::StoreBlock(ThreadBlock* block)
{
m_FirstThreadID = wxMin(m_FirstThreadID, block->GetFirstID());
m_count += block->GetCount();

m_blocks.push_back(block); 
}

bool ThreadSuperBlock::Contains(unsigned int ID) const
{
return (ID >= m_FirstThreadID) && (ID < m_FirstThreadID+m_count);
}


int ThreadSuperBlock::GetBlockForID(unsigned int containedID) const
{
for (size_t n=0; n < m_blocks.size(); ++n)
  if (m_blocks.at(n)->Contains(containedID))
    return (int)n;

return wxNOT_FOUND;
}

bool ThreadSuperBlock::IsCompleted() const
{
for (size_t n=0; n < m_blocks.size(); ++n)
  if (!m_blocks.at(n)->IsCompleted())
    return false;

return true;
}

void ThreadSuperBlock::CompleteThread(unsigned int ID, const wxArrayString& successes, const wxArrayString& array)
{
int i = GetBlockForID(ID);
wxCHECK_RET(i != wxNOT_FOUND, wxT("Unexpected thread ID"));

PasteThreadSuperBlock* ptsb = dynamic_cast<PasteThreadSuperBlock*>(this);
if (ptsb)
  for (size_t n=0; n < successes.GetCount(); ++n)
    ptsb->GetSuccessfullyPastedFilepaths().Add(successes.Item(n));

m_blocks.at(i)->CompleteThread(ID, successes, array);

if (IsCompleted())
  OnCompleted();
}

void ThreadSuperBlock::AbortThreads() const
{
for (size_t n=0; n < m_blocks.size(); ++n)
  if (!m_blocks.at(n)->IsCompleted())
    m_blocks.at(n)->AbortThreads();
}

void ThreadSuperBlock::SetThreadPointer(unsigned int ID, wxThread* thread)
{
int i = GetBlockForID(ID);
wxCHECK_RET(i != wxNOT_FOUND, wxT("Unexpected thread ID"));

m_blocks.at(i)->SetThreadPointer(ID, thread);
}

void PasteThreadSuperBlock::SetTrashdir(const wxString& trashdir)
{
wxCHECK_RET(m_trashdir.empty(), wxT("Trying to set an already-set trashdir"));
m_trashdir = trashdir; 
}

void PasteThreadSuperBlock::OnCompleted()
{
  // First deal with setting any modification times. This is needed when pasting a dir with contents, retaining the original mtimes;
  // We can't set the time when creating the dir, as it'll be modified later when contents are added. So do it now the dust has settled
while (m_ChangeMtimes.size())
  { wxString filepath = m_ChangeMtimes.back().first;
    time_t mt = m_ChangeMtimes.back().second;
    FileData fd(filepath);
    if (fd.IsValid())
      fd.ModifyFileTimes(mt);
    m_ChangeMtimes.pop_back();
  }

while (m_UnRedos.size())
  { UnRedo* entry = m_UnRedos.front().first;
    if (m_SuccessfullyPastedFilepaths.Index(m_UnRedos.front().second) == wxNOT_FOUND)
      { m_UnRedos.pop_front(); continue; } // The corresponding thread must have failed or been cancelled
    if (dynamic_cast<UnRedoPaste*>(entry) || dynamic_cast<UnRedoMove*>(entry))
      UnRedoManager::AddEntry(entry);
     else 
      { wxASSERT(false); }

    m_UnRedos.pop_front();
  }

for (size_t n=0; n < m_DeleteAfter.GetCount(); ++n) // A Moved dir's contents will have been deleted, but the dir itself will need deleting here
  { FileData fd(m_DeleteAfter.Item(n));
    if (fd.IsValid())
      { wxFileName fn(m_DeleteAfter.Item(n));
        MyGenericDirCtrl::ReallyDelete(&fn);
      }
  }


if (m_DestID && GetUnRedoType() == URT_notunredo) // Use an ID and FindWindow() here, not a ptr which might have become stale e.g. on a tab deletion
  { MyGenericDirCtrl* destctrl = wxDynamicCast(MyFrame::mainframe->FindWindow(m_DestID), MyGenericDirCtrl);
    if (destctrl)
      { destctrl->GetTreeCtrl()->UnselectAll();             // Unselect to avoid ambiguity due to multiple selections
        if (USE_FSWATCHER)                                  // If so use SelectPath(). We don't need the extra overhead of SetPath()
          { wxTreeItemId id = destctrl->FindIdForPath(m_DestPath);
            if (id.IsOk()) destctrl->GetTreeCtrl()->SelectItem(id);
          }
         else if (!m_DestPath.empty())
          destctrl->SetPath(m_DestPath);  // Needed when moving onto the 'root' dir, otherwise the selection won't alter & so the fileview won't be updated
      }
  }

if (m_DestID && !USE_FSWATCHER)
  { wxArrayInt IDs; IDs.Add(m_DestID);
    MyFrame::mainframe->OnUpdateTrees(m_DestPath, IDs);
  }
  
if (UnRedoManager::ClusterIsOpen)
  UnRedoManager::EndCluster(); // This may call UpdateTrees() internally
 else
  MyFrame::mainframe->UpdateTrees();
if (m_messagetype != _("cut"))
  UnRedoManager::CloseSuperCluster(); // Close any supercluster; we want Cut/Paste/Paste to be undone in 2 stages, not 1

wxString msg;
if (m_failures) // If there were individual failures e.g. files inside dirs didn't paste, report the individual numbers
  msg << wxString::Format(_("%zu items "), m_successes) << m_messagetype <<  wxString::Format(wxT(" successfully, %zu failed"), m_failures);
 else           // Otherwise provide the complete list, so a 1000-file dir will report 1000 items
  { msg << wxString::Format(_("%zu items "), m_overallsuccesses) << m_messagetype;
    if (m_overallfailures) msg << wxString::Format(wxT(" successfully, %zu failed"), m_overallfailures);
  }

if (GetUnRedoType() != URT_notunredo)
  UnRedoManager::GetImplementer().OnItemCompleted(GetUnRedoId());
 else
  BriefLogStatus bls(msg);

}

bool ThreadBlock::Contains(unsigned int ID) const
{
return (ID >= m_firstID) && (ID < m_firstID+m_count);
}

void ThreadBlock::AbortThreads() const
{
for (size_t n=0; n < m_count; ++n)
  { wxThread* thread = m_data[n].thread;
    if (thread)
      thread->Delete();
  }
}

bool ThreadBlock::IsCompleted() const
{
for (size_t n=0; n < m_count; ++n)
  if (!m_data[n].completed)
    return false;

return true;
}

void ThreadBlock::CompleteThread(unsigned int ID,  const wxArrayString& successes, const wxArrayString& array)
{
wxCHECK_RET(Contains(ID), wxT("Unexpected thread ID"));

int scount = successes.GetCount();
if (scount > 0) m_successes += scount;
 else ++m_failures;

m_data[ID - m_firstID].completed = true;



if (IsCompleted()) // See if this was the last (or only) thread to complete. If so, take any appropriate action
  OnBlockCompleted();
}

void ThreadBlock::OnBlockCompleted()
{
m_parent->AddSuccesses(m_successes);
m_parent->AddFailures(m_failures);
}

void ThreadBlock::SetThreadPointer(unsigned int ID, wxThread* thread)
{
wxCHECK_RET(Contains(ID) && int(ID - m_firstID) >= 0, wxT("Passed an out-of-range ID"));

m_data[ID - m_firstID].thread = thread;
}

wxThread* ThreadBlock::GetThreadPointer(unsigned int ID) const
{
wxCHECK_MSG(Contains(ID), NULL, wxT("Passed an out-of-range ID"));

return m_data[ID].thread;
}

ThreadData::~ThreadData()
{
wxCriticalSectionLocker locker(ThreadsManager::Get().GetPasteCriticalSection());
if (thread)
  thread->Kill(); 
}
