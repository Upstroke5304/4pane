/////////////////////////////////////////////////////////////////////////////
// Name:       Configure.cpp
// Purpose:    Configuration
// Part of:    4Pane
// Author:     David Hart
// Copyright:  (c) 2020 David Hart
// Licence:    GPL v3
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h" 

#include "wx/log.h"
#include "wx/wx.h"
#include "wx/app.h"
#include "wx/frame.h" 
#include "wx/config.h"
#include "wx/dirctrl.h"
#include "wx/xrc/xmlres.h"
#include "wx/spinctrl.h"
#include "wx/tooltip.h"
#include "wx/tokenzr.h"
#include "wx/statline.h"
#include "wx/colordlg.h"
#include "wx/fontdlg.h"
#include "wx/imaglist.h"

#include "Externs.h"
#include "Tools.h"
#include "MyGenericDirCtrl.h"
#include "MyDirs.h"
#include "MyFiles.h"
#include "MyFrame.h"
#include "Misc.h"
#include "Accelerators.h"
#include "Filetypes.h"
#include "Configure.h"
#include "MyTreeCtrl.h"
#include "wx/stdpaths.h"
#include "wx/ffile.h"

wxArrayString PossibleTerminals, PossibleTerminalCommands, PossibleToolsTerminalCommands;  // Global, used in ConfigureTerminals


int SHCUT_OldIniBrowse =    SHCUT_START - 1;  // Wizard button ids. It's pretty 'local' here so it should be safe
int SHCUT_ResourcesBrowse = SHCUT_START - 2;
int SHCUT_ResourcesAbort =  SHCUT_START - 3;
int SHCUT_Configure =       SHCUT_START - 4;


FirstPage::FirstPage(wxWizard *parent, wxWizardPage* Page2, wxWizardPage* Page3)    :  MyWizardPage(parent), m_Page2(Page2), m_Page3(Page3)
{
wxBoxSizer* mainsizer = new wxBoxSizer(wxVERTICAL);
wxBoxSizer* sizer1 = new wxBoxSizer(wxVERTICAL);
wxStaticText* welcome = new wxStaticText(this, wxID_ANY,  _("Welcome to 4Pane."));
wxFont font = welcome->GetFont(); font.SetPointSize(30); welcome->SetFont(font);
int x, y; welcome->GetTextExtent(_("Welcome to 4Pane."), &x,&y);
wxSize size(x+10,y); 
welcome->SetMinSize(size);

sizer1->Add(welcome, 0, wxTOP, 80);
  wxBoxSizer* sizer2 = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* sizer2_1 = new wxBoxSizer(wxVERTICAL);
    wxStaticText* FewMoments = new wxStaticText(this, wxID_ANY,  _("4Pane is a File Manager that aims to be fast and full-featured without bloat."));
    wxFont font1 = FewMoments->GetFont(); font1.SetPointSize(font1.GetPointSize() + 2); FewMoments->SetFont(font1);
    sizer2_1->Add(FewMoments, 0, wxBOTTOM, 40);
    wxStaticText* FourPaneNeeds = new wxStaticText(this, wxID_ANY,  _("Please click Next to configure 4Pane for your system.")); FourPaneNeeds->SetFont(font1);
    sizer2_1->Add(FourPaneNeeds, 0);
      wxBoxSizer* sizer2_1_1 = new wxBoxSizer(wxHORIZONTAL);
      wxStaticText* CopyAConfigFile = new wxStaticText(this, wxID_ANY,  _("Or if there's already a configuration file that you'd like to copy, click")); CopyAConfigFile->SetFont(font1);
      sizer2_1_1->Add(CopyAConfigFile, 0, wxALIGN_CENTRE);
      wxButton* OldIniBrowse = new wxButton(this, SHCUT_OldIniBrowse,  _("Here")); OldIniBrowse->SetFont(font1);
      sizer2_1_1->Add(OldIniBrowse, 0, wxLEFT | wxALIGN_CENTRE, 5);
      sizer2_1->Add(sizer2_1_1, 0, wxTOP, 40);
  sizer2->Add(sizer2_1, 1, wxTOP | wxBOTTOM, 25);
mainsizer->Add(sizer1, 0, wxTOP | wxBOTTOM, 40);
mainsizer->Add(sizer2, 0, wxEXPAND);
this->SetSizer(mainsizer);

  // Have a (slightly tentative) go at finding resources. If it succeeds, skip page 2
  // The GetConfigFilepath() will be any filepath passed with the -c commandline option, as the preferred configuration to use. If we've reached here, 
  // there wasn't one there yet, but we're about to create it. We therefore want to search for GetConfigFilepath()./rc/ etc *before* /usr/share/4Pane/rc/
wxString path = wxGetApp().GetConfigFilepath().BeforeLast(wxFILE_SEP_PATH); // Amputate the teminal /.4Pane
RCFound = Configure::LocateResourcesSilently(path);
}

SecondPage::SecondPage(wxWizard *dad,  wxWizardPage* Page3)    :  MyWizardPage(dad), parent(dad), m_Page3(Page3)
{
wxBoxSizer* m2ainsizer = new wxBoxSizer(wxVERTICAL);
wxBoxSizer* s2izer1 = new wxBoxSizer(wxVERTICAL);
wxStaticText* resourcestext = new wxStaticText(this, wxID_ANY,  _("Resources successfully located. Please press Next"));
wxFont font = resourcestext->GetFont(); font.SetPointSize(font.GetPointSize() + 2); resourcestext->SetFont(font);
int x, y; resourcestext->GetTextExtent(resourcestext->GetLabel(), &x,&y);
wxSize size(x+10,y); 
resourcestext->SetMinSize(size);
s2izer1->Add(resourcestext, 0, wxTOP | wxBOTTOM | wxALIGN_CENTRE, 60);

m2ainsizer->Add(s2izer1, 0, wxEXPAND | wxALL, 50);
this->SetSizer(m2ainsizer);
}

void SecondPage::OnPageDisplayed()
{
if (!Configure::LocateResources())  return parent->EndModal(wxID_CANCEL);
}

ThirdPage::ThirdPage(NoConfigWizard *parent, const wxString& configFP)    :  MyWizardPage(parent), m_ConfigAlreadyFound(false), configFPath(configFP)
{
wxBoxSizer* m3ainsizer = new wxBoxSizer(wxVERTICAL);
wxBoxSizer* s3izer1 = new wxBoxSizer(wxVERTICAL);
wxStaticText* hadalooktext = new wxStaticText(this, wxID_ANY,  _("I've had a look around your system, and created a configuration that should work."));
wxFont font = hadalooktext->GetFont(); font.SetPointSize(font.GetPointSize() + 2); hadalooktext->SetFont(font);
s3izer1->Add(hadalooktext, 0, wxTOP | wxBOTTOM | wxALIGN_CENTRE, 30);
wxStaticText* fullerconfigtext = new wxStaticText(this, wxID_ANY,  _("You can do much fuller configuration at any time from Options > Configure 4Pane.")); fullerconfigtext->SetFont(font);
s3izer1->Add(fullerconfigtext, 0, wxBOTTOM | wxALIGN_CENTRE, 30);

m3ainsizer->Add(s3izer1, 0, wxEXPAND | wxALL, 50);
this->SetSizer(m3ainsizer);
}

void ThirdPage::OnPageDisplayed()
{
if (m_ConfigAlreadyFound)
  return;

delete wxConfigBase::Set((wxConfigBase *) NULL);  // Delete any old wxConfig
Configure::CopyConf(configFPath);                 // Copy any master conf file into the new ini
wxConfig* localconfig = new wxConfig(wxGetApp().GetAppName(), wxT(""), configFPath);  //  & make a new, correct one
wxConfigBase::Set(localconfig);   // Needed to make it happen. There will already have been a ctor call, from the static wxConfigBase::Get()s (?), which might have created a null config

wxBusyCursor busy;                // Hardly worth bothering, but maybe on a very old, slow machine...
Configure::FindPrograms();
Configure::SaveDefaultsToIni();
Configure::DetectFixedDevices();  // Now there's an ini, add to it any detectable fixed drives
}
  
#include "bitmaps/include/wizardbitmap.xpm"

#if wxVERSION_NUMBER < 2900
  DEFINE_EVENT_TYPE(WizardCommandEvent)
#else
  wxDEFINE_EVENT(WizardCommandEvent, wxCommandEvent);
#endif

NoConfigWizard::NoConfigWizard(wxWindow* parent, int id, const wxString& title, const wxString& configFPath)
#if wxVERSION_NUMBER > 3105
    : wxWizard(parent, id, title,  wxBitmapBundle::FromBitmap(wizardbitmap)), m_configFPath(configFPath)
#else
    : wxWizard(parent, id, title, wizardbitmap), m_configFPath(configFPath)
#endif
{
m_Next = m_btnNext;

Page3 = new ThirdPage(this, configFPath);
Page2 = new SecondPage(this, Page3);
Page1 = new FirstPage(this, Page2, Page3);

GetPageAreaSizer()->Add(Page1);

   // We want to set focus to the 'Next' button, but that fails if it's done immediately. So...
wxCommandEvent event(WizardCommandEvent);
wxPostEvent(this, event);
Connect(WizardCommandEvent, wxCommandEventHandler(NoConfigWizard::OnEventSetFocusNext), NULL, this);
}

void NoConfigWizard::OnButton(wxCommandEvent& event)
{
bool success(false);

FileDirDlg fdlg(this, FD_SHOWHIDDEN | FD_RETURN_FILESONLY, wxGetApp().GetHOME(), _("Browse for the Configuration file to copy"));
while (true)
  { if (fdlg.ShowModal() != wxID_OK) return;
    bool AddIdentifier(false);
    
    wxString filepath = fdlg.GetFilepath();
    FileData fd(filepath, true);                      // Use the dereferencing form of FileData: we want any symlink's target
    if (!fd.IsValid() || !fd.IsRegularFile()) continue;
    if (!fd.CanTHISUserPotentiallyCopy())
      { wxMessageDialog dialog(this, _("I'm afraid you don't have permission to Copy this file.\nDo you want to try again?"),
                                                                                    _("No Exit!"), wxYES_NO | wxICON_QUESTION);
        if (dialog.ShowModal() != wxID_YES)    return;
        continue;
      }

    // Copy the file, but first check that the user isn't trying to use (a presumably an invalid) ~/.4Pane
    wxString teststring(wxT("grep -q 'Genuine config file' "));
    if (Configure::TestExistence(wxT("grep")) && 
                (wxExecute(teststring + wxT('"') + filepath + wxT('"'), wxEXEC_SYNC) != 0))
      { wxMessageDialog dialog(this, _("This file doesn't seem to be a valid 4Pane configuration file.\nAre you absolutely sure you want to use it?"),
                                                                                    _("Fake config file!"), wxYES_NO | wxICON_QUESTION);
        if (dialog.ShowModal() != wxID_YES) 
          break;
       
        AddIdentifier = true;
      }

    wxString configfpath = m_configFPath;
    if (configfpath.empty()) configfpath = StrWithSep(wxGetApp().GetHOME()) + wxT(".") + wxGetApp().GetAppName();

    if (filepath != configfpath)
      { wxLogNull NoFilesystemErrorsThanks;
        wxCopyFile(filepath, configfpath);              // Copy it. No need for subtlety: we've already checked it's a file, have permissions
        success = true;
      }

    delete wxConfigBase::Set((wxConfigBase *) NULL);    // Delete any old wxConfig
    wxConfig* localconfig = new wxConfig(wxGetApp().GetAppName(), wxT(""), configfpath);  //  & make a new, correct one
    wxConfigBase::Set(localconfig);  // Needed to make it happen. There will already have been a ctor call, from the static wxConfigBase::Get()s (?), which might have created a null config

    if (AddIdentifier)
       wxConfigBase::Get()->Write(wxT("Misc/Identifier"), wxT("Genuine config file"));

    break;
  }
if (success)
  { Page3->m_ConfigAlreadyFound = true;
     return (void)ShowPage(Page2, 1);
  }
 else
    EndModal(wxID_OK);
}

BEGIN_EVENT_TABLE(NoConfigWizard, wxWizard)
  EVT_WIZARD_PAGE_CHANGED(wxID_ANY, NoConfigWizard::OnPageChanged)
  EVT_BUTTON(SHCUT_OldIniBrowse, NoConfigWizard::OnButton)
END_EVENT_TABLE()

bool IsResourceValid(wxString& resourcepath, int type)  // Helper function to check that there are valid rc/bitmaps/doc paths in .4Pane
{
static const wxString subpath[] = { wxT("Misc/ResourceDir"), wxT("Misc/BitmapsDir"), wxT("Misc/HelpDir") };
static const wxString testitem[] = { wxT("/dialogs.xrc"), wxT("/back.xpm"), wxT("/About.htm") };

wxConfigBase::Get()->Read(subpath[type], &resourcepath);
if (!resourcepath.IsEmpty())
  { FileData stat(resourcepath + testitem[type]); if (stat.IsValid())  return true; }

return false;
}

//static
wxArrayString Configure::GetXDGValues() // Get values as per the freedesktop.org basedir-spec and the user's environment 
{
static const wxString strings[] = { // 1) preferred value, 2) other dirs to consider, 3) default value
                                    wxT("XDG_CONFIG_HOME"), wxT("XDG_CONFIG_DIRS"), wxT(".config"), 
                                    wxT("XDG_CACHE_HOME"), wxT(""), wxT(".cache"),
                                    wxT(""), wxT("XDG_DATA_DIRS"), wxT("/etc/xdg"), // We don't use XDG_DATA_HOME, but check DIRS for an /etc/xdg substitute
                                  };
wxArrayString results;

for (size_t count = 0, index=0; count < 3; ++count, index += 3)
  { wxString preferred(strings[index]), dirs(strings[index+1]), deflt(strings[index+2]), value, paths;
    if (!preferred.empty() && wxGetEnv(preferred, &value))
      { FileData stat(value);
        if (!stat.CanTHISUserRead() || !stat.CanTHISUserWrite() || !stat.CanTHISUserExecute()) // I think we need all 3
          value.Empty(); // Not much use if we can't read/write our file in there
      }
    if (value.empty())
      { if (!dirs.empty() && wxGetEnv(dirs, &paths)) // Try again, looking for second-choice paths 
          { wxArrayString patharray = wxStringTokenize(paths, wxT(':'), wxTOKEN_STRTOK);
            for (size_t n = 0; n < patharray.GetCount(); ++n)
              { FileData stat(patharray.Item(n));
                if (stat.CanTHISUserRead() && stat.CanTHISUserWrite() && stat.CanTHISUserExecute())
                  { value = patharray.Item(n); break; }
              }
          }
      }
    if (value.empty()) value = deflt;
    if (!value.empty() && value.Left(1) != wxT('/'))
      value = StrWithSep(wxGetHomeDir()) + value;
      
    results.Add(value);
  }

return results;
}

wxString CheckIfWeAreAnAppImage()
{
wxString fpath = wxStandardPaths::Get().GetExecutablePath(); // In an AppImage, this will probably be something like /tmp/.mount_4Pane1KLNDf9/usr/bin/4pane
if (fpath.Lower().Find(".mount_4pane") == wxNOT_FOUND)
  return "";

wxGetApp().SetAppName("4Pane"); // We don't want to be called something like 4Pane-6.0-x86_64
return fpath;
}

bool FindAppImageResources(const wxString& AppImageFpath, const wxString& ConfPath)
{
if (AppImageFpath.empty()) return false; // We're not one, so don't waste time

int stubpos = AppImageFpath.Find("usr/bin/");
if (stubpos == wxNOT_FOUND) return false; // which shouldn't happen

wxString basedir = AppImageFpath.Left(stubpos); basedir += "usr/share/";
wxString rcdir = basedir + "4Pane/rc";
wxString bmapdir = basedir + "4Pane/bitmaps";
wxString docdir = basedir + "doc/4Pane";
FileData stat(rcdir); 
wxCHECK_MSG(stat.IsValid(), false, "FindAppImageResources(): " + rcdir + " doesn't exist");

wxString FPversion("UnknownVersion"); // Try to find our version, to make a useful name for the subdir
wxFFile about(docdir+"/About.htm");
if (about.IsOpened())
  { wxString contents;
    if (about.ReadAll(&contents))
      { int pos = contents.Find("4Pane Version");
        if (pos != wxNOT_FOUND)
          FPversion = contents.Mid(pos + 14, 3); // This should return "6.0" or whatever
      }
    about.Close();
  }

// If the user has asked for a path, the end segment of which isn't '4Ppane', append it (unless one already exists, in which case use that)
// otherwise it'll be non-obvious to which program the rc/ etc dirs belong
wxString ConfigPath = StripSep(ConfPath);
FileData CP4P(ConfigPath + "/4Pane");
if (CP4P.IsValid() && CP4P.IsDir()) ConfigPath = CP4P.GetFilepath();
  else 
    { FileData CP4p(ConfigPath + "/4pane");
      if (CP4p.IsValid() && CP4p.IsDir()) ConfigPath = CP4p.GetFilepath();
       else
         { if (!ConfigPath.EndsWith("/4Pane") && !ConfigPath.EndsWith("/4pane"))
              ConfigPath = StrWithSep(ConfigPath) + "4Pane";
         }
    }

wxString CopiedFpath = StrWithSep(ConfigPath) + FPversion;

wxFileName::Mkdir(CopiedFpath, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
wxString cpa("/bin/cp -a ");
wxExecute(cpa + rcdir + ' ' + CopiedFpath, wxEXEC_SYNC);
wxExecute(cpa + bmapdir + ' ' + CopiedFpath, wxEXEC_SYNC);
wxConfigBase::Get()->Write("Misc/ResourceDir", CopiedFpath + "/rc/");
wxConfigBase::Get()->Write("Misc/BitmapsDir", CopiedFpath + "/bitmaps/");

wxString CopiedFpathDocs = StrWithSep(CopiedFpath) + "doc"; wxFileName::Mkdir(CopiedFpathDocs, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
wxExecute(cpa + docdir + ' ' + CopiedFpathDocs, wxEXEC_SYNC); // Copying to .../doc/4Pane instead of just /doc/ is ugly, but I can't find a way to get wxExecute to use globbing for .../doc/*.*

wxConfigBase::Get()->Write("Misc/HelpDir", CopiedFpathDocs + "/4Pane/");
if (IsResourceValid(RCDIR, 0) && IsResourceValid(BITMAPSDIR, 1) && IsResourceValid(HELPDIR, 2))
  return true;

wxCHECK_MSG(false, 0, "FindAppImageResources(): IsResourceValid() unexpectedly returned false :/");
}

//static
bool Configure::FindIni()  // If there's not a valid ini file in HOME (or provided), use a wizard to find/create one
{
wxString AppImageFpath = CheckIfWeAreAnAppImage(); // First, see if we're an AppImage, which will probably exist only temporarily; we'd need to cope with disappearing resource dirs

wxString ConfigFilepath = wxGetApp().GetConfigFilepath();  // See if the user particularly wants an unusual config dir
wxString ConfigPath = ConfigFilepath;
if (!ConfigFilepath.empty())
  { if (!ConfigFilepath.EndsWith(".4Pane", &ConfigPath) && !ConfigFilepath.EndsWith(".4pane", &ConfigPath))
      ConfigFilepath = StrWithSep(ConfigFilepath) + ".4Pane"; // The filepath needs to end in .4Pane, but we don't want to Mkdir that!
    if (!wxFileName::Mkdir(ConfigPath, 0755, wxPATH_MKDIR_FULL)) // Ensure the desired path exists. If it can't be created, forget the whole idea
      ConfigFilepath.Empty();
  }

if (ConfigFilepath.empty())
  { ConfigPath = StrWithSep(wxGetApp().GetHOME());
    ConfigFilepath = ConfigPath + "." + wxGetApp().GetAppName(); // If not, or the path isn't accessible, look in HOME
    if (!wxFileExists(ConfigFilepath))
      { ConfigPath = StrWithSep(wxGetApp().GetXDGconfigdir()) + "4Pane/"; // If not, try the XDG-correct place, probably ~/.config
        ConfigFilepath = ConfigPath + '.' + wxGetApp().GetAppName();
        wxFileName::Mkdir(ConfigPath, 0755, wxPATH_MKDIR_FULL); // and ensure the dir exists; if we've got here, we will be using it 
      }
  }

while (true)
  { delete wxConfigBase::Set((wxConfigBase *) NULL);            // Delete any old wxConfig & make a new, possibly correct, one
    wxConfig* localconfig = new wxConfig(wxGetApp().GetAppName(), wxT(""), ConfigFilepath);
    wxConfigBase::Set(localconfig);  // Needed to make it happen. There will already have been a ctor call, from the static wxConfigBase::Get()s (?), which might have created a null config
        
    FileData ini(ConfigFilepath);
    if (ini.IsValid())  
      { wxConfigBase* config = wxConfigBase::Get();             // See if there's (now) a valid config
        if (config != NULL)
          { wxString test = config->Read(wxT("Misc/Identifier"));
            if (test == wxT("Genuine config file"))  break;     // It's the real thing...
          }
      }

    // Now cope with AppImage issues: different runs of the image will have different mountpoints, so we'd forever be locating resources. Instead, copy them to the ini dir
    if (FindAppImageResources(AppImageFpath, ConfigPath))
      { FindPrograms(); // Save basic defaults
        SaveDefaultsToIni();
        DetectFixedDevices();
        return true;
      }

    if (!Wizard(ConfigFilepath)) return false;                  // Otherwise run the wizard to create one. False means failure, user abort
    wxConfigBase::Get()->Write(wxT("Misc/ResourceDir"), RCDIR); // Since there's a chance the user had to locate these, store the results
    wxConfigBase::Get()->Write(wxT("Misc/BitmapsDir"), BITMAPSDIR);
    wxConfigBase::Get()->Write(wxT("Misc/HelpDir"), HELPDIR);
  }

if (IsResourceValid(RCDIR, 0) && IsResourceValid(BITMAPSDIR, 1) && IsResourceValid(HELPDIR, 2))
  return true; // We succeeded inside the 'while' loop, so don't try again. Not only is that not needed, but we might find less desirable instances

bool ans = Configure::LocateResources();                        // Otherwise we need to locate /rc, /bitmaps, /docs
if (ans)
  { wxConfigBase::Get()->Write(wxT("Misc/ResourceDir"), RCDIR); // Since there's a chance the user had to locate these, store the results
    wxConfigBase::Get()->Write(wxT("Misc/BitmapsDir"), BITMAPSDIR);
    wxConfigBase::Get()->Write(wxT("Misc/HelpDir"), HELPDIR);
  }
return ans;
}

//static
bool Configure::LocateResources()  // Where is everything?
{
wxString extra;

while (!FindRC(extra))
  { wxString msg(_("Can't find 4Pane's resource files. There must be something wrong with your installation. :(\nDo you want to try to locate them manually?"));
    if (wxMessageBox(msg, _("Eek! Can't find resources."), wxYES_NO | wxICON_EXCLAMATION) != wxYES) return false;    
              // If the correct dir wasn't found, see if the user can find it
    wxDialog pseudo(NULL, wxID_ANY, wxT(""));  // We need a valid wxWindow to parent the wxDirDialog. Shouldn't need to Show() it
    #if defined(__WXGTK__)
    int style = wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST;        // the gtk2 dirdlg uses different flags :/
    #else
    int style = wxDEFAULT_DIALOG_STYLE;
    #endif
    wxDirDialog dlg(&pseudo,_("Browse for  ..../4Pane/rc/"), wxGetApp().GetHOME(), style);
    if (dlg.ShowModal() != wxID_OK) return false;
    extra = dlg.GetPath();                    // This is the selected dir. Putting it into 'extra' means that the next iteration of FindRC will look there too
    if (extra.Right(1) == wxFILE_SEP_PATH) extra = extra.BeforeLast(wxFILE_SEP_PATH);
    if (extra.Right(3) == wxT("/rc")) extra = extra.BeforeLast(wxFILE_SEP_PATH);  // The idea is that we get the Path to /rc/, which should be correct for /bitmaps/ & /docs/ too
  }

while (!FindBitmaps(extra))
  { wxString msg(_("Can't find 4Pane's bitmaps. There must be something wrong with your installation. :(\nDo you want to try to locate them manually?"));
    if (wxMessageBox(msg, _("Eek! Can't find bitmaps."), wxYES_NO | wxICON_EXCLAMATION) != wxYES) return false;    

    wxDialog pseudo(NULL, wxID_ANY, wxT(""));
#if defined(__WXGTK__)
    int style = wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST;        // the gtk2 dirdlg uses different flags :/
#else
    int style = wxDEFAULT_DIALOG_STYLE;
#endif
    wxDirDialog dlg(&pseudo,_("Browse for ..../4Pane/bitmaps/"), wxGetApp().GetHOME(), style);
    if (dlg.ShowModal() != wxID_OK) return false;
    extra = dlg.GetPath();
    if (extra.Right(1) == wxFILE_SEP_PATH) extra = extra.BeforeLast(wxFILE_SEP_PATH);
    if (extra.Right(8) == wxT("/bitmaps")) extra = extra.BeforeLast(wxFILE_SEP_PATH);
  }

while (!FindHelp(extra))
  { wxString msg(_("Can't find 4Pane's Help files. There must be something wrong with your installation. :(\nDo you want to try to locate them manually?"));
    if (wxMessageBox(msg, _("Eek! Can't find Help files."), wxYES_NO | wxICON_EXCLAMATION) != wxYES) return true;  // NB we can just about cope without help, so return true this time

    wxDialog pseudo(NULL, wxID_ANY, wxT(""));
#if defined(__WXGTK__)
    int style = wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST;        // the gtk2 dirdlg uses different flags :/
#else
    int style = wxDEFAULT_DIALOG_STYLE;
#endif
    wxDirDialog dlg(&pseudo,_("Browse for ..../4Pane/doc/"), wxGetApp().GetHOME(), style);
    if (dlg.ShowModal() != wxID_OK) return true;
    extra = dlg.GetPath();
    if (extra.Right(1) == wxFILE_SEP_PATH) extra = extra.BeforeLast(wxFILE_SEP_PATH);
    if (extra.Right(4) == wxT("/doc")) extra = extra.BeforeLast(wxFILE_SEP_PATH);
  }

return true;
}

//static
bool Configure::LocateResourcesSilently(const wxString& SuggestedPath /*= wxT("")*/)  // Where is everything? This is just a rough first check, so no error messages/try again
{
if (!FindRC(SuggestedPath)) return false;
if (!FindBitmaps(SuggestedPath)) return false;
if (!FindHelp(SuggestedPath)) return false;
return true;
}

//static
bool Configure::FindRC(const wxString& SuggestedPath)  // Find at least 1 valid source of XRC files
{
if (IsResourceValid(RCDIR, 0)) return true;  // Look in config to see if it's already there

FileData stat1((RCDIR = SuggestedPath+wxT("/rc/")) + wxT("dialogs.xrc")); if (stat1.IsValid()) return true;         // If we were passed a user-located dir, try there
FileData stat2((RCDIR = wxGetApp().StdDataDir+wxT("/rc/")) + wxT("dialogs.xrc")); if (stat2.IsValid()) return true; // If not, look in (probably) /usr/local/share/4Pane/rc
FileData stat3((RCDIR = wxT("/usr/local/share/4Pane/rc/")) + wxT("dialogs.xrc")); if (stat3.IsValid()) return true; // Or hard-wire this (might be needed in a livecd situation)
FileData stat4((RCDIR = wxT("/usr/share/4Pane/rc/")) + wxT("dialogs.xrc")); if (stat4.IsValid()) return true;       // Ditto in debianish systems
FileData stat5((RCDIR = GetCwd()+wxT("/rc/")) + wxT("dialogs.xrc")); if (stat5.IsValid()) return true;              // If still not, look in ./rc
FileData stat6((RCDIR = wxGetApp().GetHOME()+wxT("/rc/")) + wxT("dialogs.xrc")); if (stat6.IsValid()) return true;  // 1 last chance. Try in HOME

return false;
}

//static
bool Configure::FindBitmaps(const wxString& SuggestedPath)
{
if (IsResourceValid(BITMAPSDIR, 1)) return true;  // Look in config to see if it's already there

FileData stat1((BITMAPSDIR = SuggestedPath+wxT("/bitmaps/")) + wxT("back.xpm")); if (stat1.IsValid()) return true;          // If we were passed a user-located dir, try there
FileData stat2((BITMAPSDIR = wxGetApp().StdDataDir+wxT("/bitmaps/")) + wxT("back.xpm")); if (stat2.IsValid()) return true;  // If not, look in (probably) /usr/local/share/4Pane/bitmaps
FileData stat3((BITMAPSDIR = wxT("/usr/local/share/4Pane/bitmaps/")) + wxT("back.xpm")); if (stat3.IsValid()) return true;  // Or hard-wire this (might be needed in a livecd situation)
FileData stat4((BITMAPSDIR = wxT("/usr/share/4Pane/bitmaps/")) + wxT("back.xpm")); if (stat4.IsValid()) return true;// Ditto in debianish systems
FileData stat5((BITMAPSDIR = GetCwd()+wxT("/bitmaps/")) + wxT("back.xpm")); if (stat5.IsValid()) return true;       // If still not, look in ./bitmaps
FileData stat6((BITMAPSDIR = StrWithSep(wxGetApp().GetHOME())+wxT("bitmaps/")) + wxT("back.xpm")); if (stat6.IsValid()) return true; // 1 last chance. Try in HOME

return false;
}

//static
bool Configure::FindHelp(const wxString& SuggestedPath)
{
if (IsResourceValid(HELPDIR, 2)) return true;  // Look in config to see if it's already there

FileData stat1((HELPDIR = SuggestedPath+wxT("/About.htm"))); if (stat1.IsValid()) return true;               // If we were passed a user-located dir, try there
FileData stat2((HELPDIR = SuggestedPath+wxT("/doc/")) + wxT("About.htm")); if (stat2.IsValid()) return true; // or nearby
FileData stat3((HELPDIR = wxGetApp().StdDocsDir) + wxT("/About.htm")); if (stat3.IsValid()) return true;    // If not, look in (probably) /usr/local/share/doc/4Pane
FileData stat4((HELPDIR = wxT("/usr/local/share/4Pane/doc/")) + wxT("About.htm")); if (stat4.IsValid()) return true;// Or hard-wire this (might be needed in a livecd situation)
FileData stat5((HELPDIR = wxT("/usr/share/doc/4pane/")) + wxT("About.htm")); if (stat5.IsValid()) return true;      // Ditto in debianish systems
FileData stat6((HELPDIR = wxT("/usr/share/doc/4Pane/")) + wxT("About.htm")); if (stat6.IsValid()) return true;      //  or almost...
FileData official((HELPDIR = wxT("/usr/share/doc/4Pane/html/")) + wxT("About.htm")); if (official.IsValid()) return true; //  or in official builds
FileData stat7((HELPDIR = wxT("/usr/doc/4Pane/")) + wxT("About.htm")); if (stat7.IsValid()) return true;    // Ditto in slackware (Sigh! And even that's a symlink)
FileData stat8((HELPDIR = GetCwd()+wxT("/doc/")) + wxT("About.htm")); if (stat8.IsValid()) return true;     // If still not, look in ./docs
FileData stat9((HELPDIR = StrWithSep(wxGetApp().GetHOME())+wxT("doc/")) + wxT("About.htm")); if (stat9.IsValid()) return true; // 1 last chance. Try in HOME

return false;
}

void Configure::FindPrograms()
{
KDESU_PRESENT = TestExistence(wxT("kdesu"));
XSU_PRESENT = TestExistence(wxT("gnomesu"));
GKSU_PRESENT = TestExistence(wxT("gksu"));

KONSOLE_PRESENT = TestExistence(wxT("konsole"));
GNOME_TERM_PRESENT = TestExistence(wxT("gnome-terminal"));
XTERM_PRESENT = TestExistence(wxT("xterm"));
XCFE4_PRESENT = TestExistence(wxT("xfce4-terminal"));
LXTERMINAL_PRESENT = TestExistence(wxT("lxterminal"));
QTERMINAL_PRESENT = TestExistence(wxT("qterminal"));
MATETERMINAL_PRESENT = TestExistence(wxT("mate-terminal"));

if (TestExistence(wxT("/sbin/showmount"))) SHOWMOUNTFPATH = wxT("/sbin/showmount"); // This is normally /USR/sbin/showmount, but in debian...
}

//static
void Configure::DetectFixedDevices()
{
ArrayofDeviceStructs* DeviceStructArray = new ArrayofDeviceStructs;
DeviceManager::GetFloppyInfo(DeviceStructArray, 0, NULL, true);
DeviceManager::GetCdromInfo(DeviceStructArray, 0, NULL, true);

  // The rest is adapted from DeviceManager::SaveFixedDevices(). Unfortunately, esp. within a static function, it wasn't reusable
wxConfigBase* config = wxConfigBase::Get(); if (config==NULL) return;
config->DeleteGroup(wxT("Devices/ToLoad"));                       // Out with the old . . .

wxString Rootname(wxT("Devices/ToLoad/"));
size_t count = DeviceStructArray->GetCount();

for (size_t n = 0; n < count; ++n)                                // Go thru the list of cdrom drives etc
  { DevicesStruct* tempstruct = DeviceStructArray->Item(n);
    if (tempstruct == NULL) continue;
    wxString subgrp = CreateSubgroupName(n, count);

    config->Write(Rootname+subgrp+wxT("/Device"), tempstruct->devicenode);
    config->Write(Rootname+subgrp+wxT("/Name"), tempstruct->name1);
    wxString mtpt; if (tempstruct->mountpts.GetCount()) mtpt = tempstruct->mountpts[0];
    config->Write(Rootname+subgrp+wxT("/Mountpoint"), mtpt);      // Save any fstab mountpt
    config->Write(Rootname+subgrp+wxT("/Readonly"), tempstruct->readonly);
    config->Write(Rootname+subgrp+wxT("/Devicetype"), tempstruct->devicetype);
    config->Write(Rootname+subgrp+wxT("/Ignore"), tempstruct->ignore);  // If true, don't actually use this one
  }
  
config->Write(Rootname + wxT("NoOfDevices"), (long)count);        // How many to reload next time
config->Flush();

delete DeviceStructArray;
}

//static
enum distro Configure::DeduceDistro(double& Version)  // Try to work out which distro & version we're on
{
enum distro Distro = gok;

if (wxFileExists(wxT("/etc/SuSE-release")))                    // SuSE
  { Distro = suse;
    wxTextFile tf(wxT("/etc/SuSE-release"));
    if (!tf.Open())  return Distro;
    wxString line = tf.GetLine(1); 
    if (!GetFloat(line, &Version, 0)) Version = wxNOT_FOUND;   // This should put the first valid float into Version, or -1 if there isn't one
    return Distro;
  }

if (wxFileExists(wxT("/etc/pclinuxos-release")))               // PCLinuxOS
  { Distro = pclos;
    wxTextFile tf(wxT("/etc/pclinuxos-release"));              // Could also have used /etc/issue --- same contents:  PCLinuxOS release 2007
    if (!tf.Open())  return Distro;
    wxString line = tf.GetLine(0); 
    if (!GetFloat(line, &Version, 0)) Version = wxNOT_FOUND;   // This should put the first valid float into Version
    return Distro;
  }

wxString filename;  // Temp store for filename, see below
if (wxFileExists(filename=wxT("/etc/mandrakelinux-release"))   // Mandrake/Mandriva  (watch out: PCLinuxOS has symlinks with these names)
          ||  wxFileExists(filename=wxT("/etc/mandriva-release")))
  { Distro = mandrake;
    wxTextFile tf(filename);
    if (!tf.Open())  return Distro;
    wxString line = tf.GetLine(0); 
    if (!GetFloat(line, &Version, 0)) Version = wxNOT_FOUND;
    return Distro;
  }

if (wxFileExists(wxT("/etc/fedora-release")))                  // Fedora
  { Distro = fedora;
    wxTextFile tf(wxT("/etc/fedora-release"));
    if (!tf.Open())  return Distro;
    wxString line = tf.GetLine(0); 
    if (!GetFloat(line, &Version, 0)) Version = wxNOT_FOUND;
    return Distro;
  }

if (wxFileExists(wxT("/etc/zenwalk-version")))                 // Zenwalk. Must come before Slackware as it also has an /etc/slackware-version
  { Distro = zenwalk;
    wxTextFile tf(wxT("/etc/zenwalk-version"));
    if (!tf.Open())  return Distro;
    wxString line = tf.GetLine(0);                             // Should be e.g. Zenwalk 5.0
    if (!GetFloat(line, &Version, 0)) Version = wxNOT_FOUND;
    return Distro;
  }

if (wxFileExists(wxT("/etc/slackware-version")))               // Slackware
  { Distro = slackware;
    wxTextFile tf(wxT("/etc/slackware-version"));
    if (!tf.Open())  return Distro;
    wxString line = tf.GetLine(0); 
    if (!GetFloat(line, &Version, 0)) Version = wxNOT_FOUND;   // Using GetFloat() copes with foo bar 1.2.3, returning 1.2
    return Distro;
  }
                            // Sabayon, which we must do before gentoo
if (wxFileExists(wxT("/etc/sabayon-edition")))                 // Starting from 4.1, this file contains the real data
  { Distro = sabayon;
    wxTextFile tf(wxT("/etc/sabayon-edition"));
    if (!tf.Open())  return Distro;
    wxString line = tf.GetLine(0);
    if (!GetFloat(line, &Version, 0)) Version = wxNOT_FOUND;   // This should put the 1st valid float into Version (Sabayon Linux 4.1-r2 x86 G)
    return Distro;
  }
 else if (wxFileExists(wxT("/etc/sabayon-release")))           // Pre-4.1, it was all in here
  { Distro = sabayon;
    wxTextFile tf(wxT("/etc/sabayon-release"));
    if (!tf.Open())  return Distro;
    wxString line = tf.GetLine(0);
    if (!GetFloat(line, &Version, 1)) Version = wxNOT_FOUND;   // This should put the second valid float into Version (Sabayon Linux x86-64 3.4)
    return Distro;
  }

if (wxFileExists(wxT("/etc/gentoo-release")))                  // Gentoo
  { Distro = gentoo;
    wxTextFile tf(wxT("/etc/gentoo-release"));
    if (!tf.Open())  return Distro;
    wxString line = tf.GetLine(0);
    if (!GetFloat(line, &Version, 0)) Version = wxNOT_FOUND;
    return Distro;
  }
  
if (wxFileExists(wxT("/etc/puppyversion")))                    // Puppy Linux
  { Distro = puppy;
    wxTextFile tf(wxT("/etc/puppyversion"));
    if (!tf.Open())  return Distro;
    wxString line = tf.GetLine(0); 
    line.ToDouble(&Version); return Distro;                    // Puppy is easy: it's just 2.00 (or whatever)
  } 
if (wxFileExists(wxT("/etc/DISTRO_SPECS")))                    // More recent Puppies only have this
  { wxTextFile tf(wxT("/etc/DISTRO_SPECS"));
    if (!tf.Open())  return Distro;
    for (size_t n=0; n < tf.GetLineCount(); ++n)
      { wxString line = tf.GetLine(n);
        if (line.Contains(wxT("Puppy")))
          { Distro = puppy; break; }
      }

    if (Distro == puppy)
      for (size_t n=0; n < tf.GetLineCount(); ++n)
        { wxString line = tf.GetLine(n);
          if (line.Contains(wxT("DISTRO_VERSION")))            // This might be 'DISTRO_VERSION=520' for 5.2 lucid, or 'DISTRO_VERSION=5.3.1' for slacko
            { line = line.AfterFirst(wxT('='));
              line.Replace(wxT("."), wxT(""));
              line.ToDouble(&Version); return Distro; 
            }
        }
  }


if (wxFileExists(wxT("/etc/redhat-release")))                  // Red Hat (watch out: PCLinuxOS has a symlink with this name)
  { Distro = redhat;
    wxTextFile tf(wxT("/etc/redhat-release"));
    if (!tf.Open())  return Distro;
    wxString line = tf.GetLine(0);
    if (!GetFloat(line, &Version, 0)) Version = wxNOT_FOUND;
    return Distro;
  }

if (wxFileExists(wxT("/etc/debian_version")))                  // It's some type of Debian, but which?
  { if (wxFileExists(wxT("/etc/knoppix-version")))
      { Distro = knoppix;                                      // Knoppix
        wxTextFile tf(wxT("/etc/knoppix-version"));
        if (!tf.Open())  return Distro;
        wxString line = tf.GetLine(0); 
        if (!GetFloat(line, &Version, 0)) Version = wxNOT_FOUND;
        return Distro;
      }
    if (wxFileExists(wxT("/KNOPPIX")) && wxFileExists(wxT("/etc/debian-version"))) // More Knoppix: in 6.2 there isn't an /etc/knoppix-version
      { Distro = knoppix;                                         
        wxTextFile tf(wxT("/etc/debian-version"));
        if (!tf.Open())  return Distro;
        wxString line = tf.GetLine(0);
        if (line.Left(7) == wxT("squeeze"))                    // The line is: squeeze/sid
          { Version=6; return Distro; }
        return Distro;
      }

    if (wxFileExists(wxT("/etc/issue")))  
      { Distro = mepis;                                        // Mepis
        wxTextFile tf(wxT("/etc/issue"));
        if (!tf.Open())  return Distro;
        wxString line = tf.GetLine(0); 
        if (line.Contains(wxT("MEPIS")))                       // If it *doesn't* it's probably ubuntu proper. (In Mepis 3.1 MEPIS started the string; in 6.0 it's near the end)
          { if (wxFileExists(wxT("/etc/lsb-release")))         // In 6.0, it's come over all ubuntu-ish, so we have to use lsb-release
              { wxTextFile tf(wxT("/etc/lsb-release"));
                if (!tf.Open())  return Distro;
                wxString line = tf.GetLine(1); 
                if (!GetFloat(line, &Version, 0)) Version = wxNOT_FOUND;  // This should put the first valid float into Version
                return Distro;  // Though Mepis calls itself 6.0, its ubuntu version returns 6.06 :(
              }
             else if (wxFileExists(wxT("/etc/debian_version")))
              { wxTextFile debf(wxT("/etc/debian_version"));   // Mepis <6 doesn't store a version no, so use the debian one if it exists (as in Mepis 3.1)
                if (!debf.Open())  return Distro;
                line = debf.GetLine(0); 
                if (line.ToDouble(&Version)) return Distro;
              }

            Version = -1; return Distro;  // (Backstop)
          }
         else if (line.Contains(wxT("Mint")))                  // Mint
          { Distro = mint;
            if (!GetFloat(line, &Version, 0)) Version = wxNOT_FOUND; // From version 6, Mint calls itself by its name. Before it 'was' ubuntu
            return Distro;
          }
      }

    if (wxFileExists(wxT("/etc/lsb-release")))                 // Ubuntu
      { Distro = debian;    // Temporarily
        wxTextFile tf(wxT("/etc/lsb-release"));
        if (!tf.Open())  return Distro;
        wxString line = tf.GetLine(0); 
        if (line == wxT("DISTRIB_ID=Ubuntu")) Distro = ubuntu;
        line = tf.GetLine(1);
        if (!GetFloat(line, &Version, 0)) Version = wxNOT_FOUND;  // This should put the first valid float into Version
        return Distro;
      }

    else                                                       // Plain vanilla debian
      { Distro = debian;
        wxTextFile tf(wxT("/etc/debian_version"));
        if (!tf.Open())  return Distro;
        wxString line = tf.GetLine(0); 
        if (line.ToDouble(&Version)) return Distro;            // Etch, lenny, squeeze return their version numbers e.g. 6.0.4
        if (line.StartsWith(wxT("squeeze")))  { Version=6; return Distro; } // Unreleased versions hold strings e.g. wheezy/sid
         else if (line.StartsWith(wxT("wheezy")))  { Version=7; return Distro; }
         else if (line.StartsWith(wxT("jessie")))  { Version=8; return Distro; }
         else if (line.StartsWith(wxT("stretch")))  { Version=9; return Distro; }
         else if (line.StartsWith(wxT("buster")))  { Version=10; return Distro; }
         else if (line.StartsWith(wxT("bullseye")))  { Version=11; return Distro; }
         
        Version = -1; return Distro;
      }
  }

Version = -1; return (Distro=other);                           // If we're here, nothing of note was found
}

//static
void Configure::SaveProbableHAL_Sys_etcInfo(enum distro Distro, double Version)  // Using either the discovered distro, or stat + guesswork, save the probable discovery/loading setup
{
FLOPPY_DEVICES_AUTOMOUNT=CDROM_DEVICES_AUTOMOUNT=USB_DEVICES_AUTOMOUNT = true;      // Make the commonest answer the default as it saves a lot of typing
FLOPPY_DEVICES_SUPERMOUNT=CDROM_DEVICES_SUPERMOUNT=USB_DEVICES_SUPERMOUNT = false;  // Ditto
DEVICE_DISCOVERY_TYPE = SysHal;                                                     // Ditto

USE_SUDO = false; // It's off-topic, but this is the most convenient place to set the default for su/sudo

if (Distro==suse)
  { if (Version < 9.1)   // I'm guessing about 9.0, but this is certainly true for 8.2) 
    { FLOPPY_DEVICES_AUTOMOUNT=CDROM_DEVICES_AUTOMOUNT=USB_DEVICES_AUTOMOUNT= false;
      DEVICE_DISCOVERY_TYPE = OriginalMethod; SCSIUSBDIR = wxT("/proc/scsi/usb-storage-"); SCSISCSI =  wxT("/proc/scsi/scsi");    // (which they probably were anyway)
      FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB=CD_DEVICES_DISCOVERY_INTO_FSTAB = NoInsert; USB_DEVICES_DISCOVERY_INTO_FSTAB = FstabInsert;
    }
   else if (Version > 9.0 && Version < 9.3)        // SuSE's oddity: mounting to mtab without HAL/sys
    { DEVICE_DISCOVERY_TYPE = MtabMethod;
      FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB=CD_DEVICES_DISCOVERY_INTO_FSTAB=USB_DEVICES_DISCOVERY_INTO_FSTAB = MtabInsert;
    }
   else if (Version >= 10.1 && Version < 11.1)    // SuSE 10.1-2 seems to automount only via kde, and only usb devices! HAL/sys discovery
    { FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB=CD_DEVICES_DISCOVERY_INTO_FSTAB = NoInsert; USB_DEVICES_DISCOVERY_INTO_FSTAB = MtabInsert;
      FLOPPY_DEVICES_AUTOMOUNT=CDROM_DEVICES_AUTOMOUNT=false;
    }
   else if (Version >= 11.1)                      // SuSE 11.1 has a mandriva/sabayon-like loader for recently added devices. 
    { FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB = NoInsert; CD_DEVICES_DISCOVERY_INTO_FSTAB = MtabInsert; USB_DEVICES_DISCOVERY_INTO_FSTAB = MtabInsert;
      FLOPPY_DEVICES_AUTOMOUNT=false;
    }

  }

if (Distro==slackware)
  { if (Version < 11.1)
      { if (KernelLaterThan(wxT("2.5")))         // (I doubt if anyone's using precisely 2.5.0)
          { FLOPPY_DEVICES_AUTOMOUNT=CDROM_DEVICES_AUTOMOUNT=USB_DEVICES_AUTOMOUNT = false;  // True for 10.0 & 10.2, & 11.0 with the 2.6 kernel
            FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB=CD_DEVICES_DISCOVERY_INTO_FSTAB=USB_DEVICES_DISCOVERY_INTO_FSTAB = NoInsert;
          }
         else
          { FLOPPY_DEVICES_AUTOMOUNT=CDROM_DEVICES_AUTOMOUNT=USB_DEVICES_AUTOMOUNT= false;   // True for 10.0 & 10.2, & 11.0 with the 2.4 kernel
            DEVICE_DISCOVERY_TYPE = OriginalMethod; SCSIUSBDIR = wxT("/proc/scsi/usb-storage-"); SCSISCSI =  wxT("/proc/scsi/scsi"); // (which they probably were anyway)
            FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB=CD_DEVICES_DISCOVERY_INTO_FSTAB=USB_DEVICES_DISCOVERY_INTO_FSTAB = NoInsert;
          }
      }
     else if (Version >= 12.0)
          { CD_DEVICES_DISCOVERY_INTO_FSTAB=FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB = MtabInsert;  USB_DEVICES_DISCOVERY_INTO_FSTAB = NoInsert;
            FLOPPY_DEVICES_AUTOMOUNT= false;          // Floppies don't AUTOMOUNT
          }
  }

if (Distro==sabayon)
  { if ((Version < 4.0))        // I started with 3.4, which has a taskbar icon for each device, that lets you (un)mount it ; except for floppies which don't
      { FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB = NoInsert;  CD_DEVICES_DISCOVERY_INTO_FSTAB=USB_DEVICES_DISCOVERY_INTO_FSTAB = MtabInsert;
        FLOPPY_DEVICES_AUTOMOUNT= false;              // Floppies don't AUTOMOUNT
      }
    else if (Version == 4.0)    // In 4.0 floppies behave in the same way as other devices
      { FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB=CD_DEVICES_DISCOVERY_INTO_FSTAB=USB_DEVICES_DISCOVERY_INTO_FSTAB = MtabInsert;
      }
    else if (Version > 4.0)     // >= 4.1 don't :/
      { FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB = CD_DEVICES_DISCOVERY_INTO_FSTAB=USB_DEVICES_DISCOVERY_INTO_FSTAB = MtabInsert;
        FLOPPY_DEVICES_AUTOMOUNT= false;              // Floppies don't AUTOMOUNT
      }
  }

if (Distro==gentoo)    // Can't really say with gentoo, as it's up to the individual how he configures. Make minimal guesses
  { FLOPPY_DEVICES_AUTOMOUNT=false;
    FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB=CD_DEVICES_DISCOVERY_INTO_FSTAB=USB_DEVICES_DISCOVERY_INTO_FSTAB = NoInsert;
    DEVICE_DISCOVERY_TYPE =  KernelLaterThan(wxT("2.5")) ? SysHal : OriginalMethod; 
  }

if (Distro==mint)
  { USE_SUDO = true;

    if (Version >= 6)  // Until 6, Mint internally called itself ubuntu
      { FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB=USB_DEVICES_DISCOVERY_INTO_FSTAB = NoInsert;  CD_DEVICES_DISCOVERY_INTO_FSTAB = MtabInsert;
        FLOPPY_DEVICES_AUTOMOUNT= false;  // Floppies don't AUTOMOUNT (there's not even a /dev/fd0)
      }
  }

if (Distro==ubuntu)
  { USE_SUDO = true;

    if (Version < 6)
      { FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB=CD_DEVICES_DISCOVERY_INTO_FSTAB=USB_DEVICES_DISCOVERY_INTO_FSTAB = FstabInsert;
        FLOPPY_DEVICES_AUTOMOUNT=CDROM_DEVICES_AUTOMOUNT= false;  // Only USB_DEVICES_AUTOMOUNT
      }
    else if (Version >= 6.06)
      { FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB = NoInsert;  CD_DEVICES_DISCOVERY_INTO_FSTAB=USB_DEVICES_DISCOVERY_INTO_FSTAB = MtabInsert;
        FLOPPY_DEVICES_AUTOMOUNT= false;              // Floppies don't AUTOMOUNT
      }
  }

if (Distro==mepis)
  { if (Version < 6)
      { FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB=CD_DEVICES_DISCOVERY_INTO_FSTAB=USB_DEVICES_DISCOVERY_INTO_FSTAB = FstabInsert;
        FLOPPY_DEVICES_AUTOMOUNT=CDROM_DEVICES_AUTOMOUNT=USB_DEVICES_AUTOMOUNT= false;
      }
    else if (Version == 6.06)  // Mepis now returns a ubuntu version :(
      { FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB = CD_DEVICES_DISCOVERY_INTO_FSTAB = NoInsert;  USB_DEVICES_DISCOVERY_INTO_FSTAB = MtabInsert;
        FLOPPY_DEVICES_AUTOMOUNT=CDROM_DEVICES_AUTOMOUNT= false;    // Only USB_DEVICES_AUTOMOUNT
      } 
    else if (Version > 6.99 && Version < 11.0)    // Mepis no longer returns a ubuntu version!
      { FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB = NoInsert; CD_DEVICES_DISCOVERY_INTO_FSTAB = FstabInsert; USB_DEVICES_DISCOVERY_INTO_FSTAB = MtabInsert;
        FLOPPY_DEVICES_AUTOMOUNT=CDROM_DEVICES_AUTOMOUNT= false;    // Both usbs and dvds can automount, but it's better not to for dvds as they -> fstab anyway
      }  
    else if (Version >= 11.0)
      { FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB = NoInsert; CD_DEVICES_DISCOVERY_INTO_FSTAB = MtabInsert; USB_DEVICES_DISCOVERY_INTO_FSTAB = MtabInsert;
        FLOPPY_DEVICES_AUTOMOUNT = false;    // Both usbs and dvds automount using a systray icon
      } 
  }

if (Distro==debian)
  { if (Version==3.1)                               // Sarge
      { FLOPPY_DEVICES_AUTOMOUNT=CDROM_DEVICES_AUTOMOUNT=USB_DEVICES_AUTOMOUNT = false; 
        FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB=CD_DEVICES_DISCOVERY_INTO_FSTAB=USB_DEVICES_DISCOVERY_INTO_FSTAB = NoInsert;
      }
    else if (Version == 4.0)                        // Etch==4
      { FLOPPY_DEVICES_AUTOMOUNT=CDROM_DEVICES_AUTOMOUNT=USB_DEVICES_AUTOMOUNT = false;
        FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB=CD_DEVICES_DISCOVERY_INTO_FSTAB = USB_DEVICES_DISCOVERY_INTO_FSTAB = NoInsert;
      }
    else if (Version >= 5.0)                        // Lenny==5, Squeeze 6
      { FLOPPY_DEVICES_AUTOMOUNT = false;
        FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB=NoInsert; CD_DEVICES_DISCOVERY_INTO_FSTAB = USB_DEVICES_DISCOVERY_INTO_FSTAB = MtabInsert;
      }
  }

if (Distro==knoppix)
  { if (Version >= 5.3)                             // Uses Etch
      { FLOPPY_DEVICES_AUTOMOUNT = false; CDROM_DEVICES_AUTOMOUNT=USB_DEVICES_AUTOMOUNT = true;
        FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB=CD_DEVICES_DISCOVERY_INTO_FSTAB=USB_DEVICES_DISCOVERY_INTO_FSTAB = NoInsert;
      }
    else if (Version >= 6.0)                        // Uses lenny6.0/squeeze6.2
      { FLOPPY_DEVICES_AUTOMOUNT=CDROM_DEVICES_AUTOMOUNT=USB_DEVICES_AUTOMOUNT = false;
        FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB=CD_DEVICES_DISCOVERY_INTO_FSTAB = USB_DEVICES_DISCOVERY_INTO_FSTAB = NoInsert;
      }
  }

if (Distro==mandrake && Version == 10.1)    
  { FLOPPY_DEVICES_SUPERMOUNT = true;
    FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB = MtabInsert;
    FLOPPY_DEVICES_AUTOMOUNT=CDROM_DEVICES_AUTOMOUNT= false;  // Only USB_DEVICES_AUTOMOUNT, and then only sometimes!
  }
  
if (Distro==mandrake && Version == 10.2)    
  { FLOPPY_DEVICES_SUPERMOUNT = true;
    FLOPPY_DEVICES_AUTOMOUNT=CDROM_DEVICES_AUTOMOUNT=USB_DEVICES_AUTOMOUNT= false;  // Nothing properly automounts
    CD_DEVICES_DISCOVERY_INTO_FSTAB=FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB = MtabInsert; USB_DEVICES_DISCOVERY_INTO_FSTAB=FstabInsert;
  }
  
if (Distro==mandrake && Version == 2007)    
  { FLOPPY_DEVICES_SUPERMOUNT = true;  
    CDROM_DEVICES_AUTOMOUNT=USB_DEVICES_AUTOMOUNT = false;
    FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB=CD_DEVICES_DISCOVERY_INTO_FSTAB=USB_DEVICES_DISCOVERY_INTO_FSTAB = NoInsert;
  }
  
if (Distro==mandrake && Version == 2008)  // Assuming we allow Konq to do it  
  { FLOPPY_DEVICES_AUTOMOUNT = false;  
    FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB=USB_DEVICES_DISCOVERY_INTO_FSTAB = NoInsert;
    CD_DEVICES_DISCOVERY_INTO_FSTAB = MtabInsert;
  }
  
if (Distro==mandrake && Version >= 2008.1)  // There are now sabayon-like icons next to the clock
  { FLOPPY_DEVICES_AUTOMOUNT = false;  
    FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB=NoInsert;
    CD_DEVICES_DISCOVERY_INTO_FSTAB = USB_DEVICES_DISCOVERY_INTO_FSTAB = MtabInsert;
  }

if (Distro==fedora)
  { if (Version < 4)  FLOPPY_DEVICES_AUTOMOUNT=CDROM_DEVICES_AUTOMOUNT= false;    // Only USB_DEVICES_AUTOMOUNT
     else if (Version < 5)
      { FLOPPY_DEVICES_AUTOMOUNT=CDROM_DEVICES_AUTOMOUNT=USB_DEVICES_AUTOMOUNT= false;
        FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB=CD_DEVICES_DISCOVERY_INTO_FSTAB = MtabInsert; USB_DEVICES_DISCOVERY_INTO_FSTAB = FstabInsert;      
      }
     else if (Version < 7) // This works for 5 & 6
      { FLOPPY_DEVICES_AUTOMOUNT = false; FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB = NoInsert; 
        CD_DEVICES_DISCOVERY_INTO_FSTAB = USB_DEVICES_DISCOVERY_INTO_FSTAB = MtabInsert;
      }
    else //if (Version >= 7)  // It all depends on whether you want konq to do it for you
      { FLOPPY_DEVICES_AUTOMOUNT = false; CDROM_DEVICES_AUTOMOUNT=USB_DEVICES_AUTOMOUNT = true;
        FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB = NoInsert; CD_DEVICES_DISCOVERY_INTO_FSTAB = USB_DEVICES_DISCOVERY_INTO_FSTAB = MtabInsert;
      }
  }

if (Distro==pclos)
  { if (Version >= 2011)
      { FLOPPY_DEVICES_AUTOMOUNT = false; CDROM_DEVICES_AUTOMOUNT=USB_DEVICES_AUTOMOUNT = true;
        FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB = USB_DEVICES_DISCOVERY_INTO_FSTAB = NoInsert;
        CD_DEVICES_DISCOVERY_INTO_FSTAB = MtabInsert;
      } 
    else if (Version >= 2007)    // It all depends on whether you want konq to do it for you
      { FLOPPY_DEVICES_AUTOMOUNT = false; CDROM_DEVICES_AUTOMOUNT=USB_DEVICES_AUTOMOUNT = true;
        FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB=CD_DEVICES_DISCOVERY_INTO_FSTAB = USB_DEVICES_DISCOVERY_INTO_FSTAB = NoInsert;
      }
  }

if (Distro==puppy)
  { if (Version >= 500)
      { FLOPPY_DEVICES_AUTOMOUNT = false;  CDROM_DEVICES_AUTOMOUNT=USB_DEVICES_AUTOMOUNT = true; 
        FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB = NoInsert; CD_DEVICES_DISCOVERY_INTO_FSTAB=USB_DEVICES_DISCOVERY_INTO_FSTAB = MtabInsert;
      }
  }

if (Distro==zenwalk)   // I've only looked at 5.0. Standard version has automounting cdroms/usbs, but not floppies. There's a 'core' version that doesn't, though
  { FLOPPY_DEVICES_AUTOMOUNT = false; CDROM_DEVICES_AUTOMOUNT=USB_DEVICES_AUTOMOUNT = true;
    FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB = NoInsert; CD_DEVICES_DISCOVERY_INTO_FSTAB = USB_DEVICES_DISCOVERY_INTO_FSTAB = MtabInsert;
  }

  
if (Distro==gok || Distro==other)
  { FLOPPY_DEVICES_AUTOMOUNT = false;  CDROM_DEVICES_AUTOMOUNT=USB_DEVICES_AUTOMOUNT = true; // Use what are the commonest values nowadays
    FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB = NoInsert; CD_DEVICES_DISCOVERY_INTO_FSTAB=USB_DEVICES_DISCOVERY_INTO_FSTAB = MtabInsert;
    DEVICE_DISCOVERY_TYPE = SysHal;
  }

DeviceManager::SaveMiscData();                        // These effectively save what we've just done
DeviceBitmapButton::SaveDefaults();
}

//static
void Configure::SaveDefaultSus()    // Save a useful value for WHICH_SU
{
WHICH_SU = mysu;

wxConfigBase::Get()->Write(wxT("/Misc/Which_Su"),  (long)WHICH_SU);
wxConfigBase::Get()->Write(wxT("/Misc/Use_sudo"), USE_SUDO);   // which was set in SaveProbableHAL_Sys_etcInfo()
}

//static
void Configure::SaveDefaultTerminal(enum distro Distro, double Version)  // Save a useful value for TERMINAL etc
{
if (KONSOLE_PRESENT)
  { PossibleTerminals.Add(wxT("konsole")); PossibleTerminalCommands.Add(wxT("konsole -e")); PossibleToolsTerminalCommands.Add(wxT("konsole --noclose -e")); }
if (GNOME_TERM_PRESENT)
  { PossibleTerminals.Add(wxT("gnome-terminal")); PossibleTerminalCommands.Add(wxT("gnome-terminal -x")); }
if (XTERM_PRESENT)  
  { PossibleTerminals.Add(wxT("xterm")); PossibleTerminalCommands.Add(wxT("xterm -e")); PossibleToolsTerminalCommands.Add(wxT("xterm -hold -e")); }
if (XCFE4_PRESENT)  // For some reason, it can't cope with -e (at least not without quoting the command) but -x is fine
  { PossibleTerminals.Add(wxT("xfce4-terminal")); PossibleTerminalCommands.Add(wxT("xfce4-terminal -x")); PossibleToolsTerminalCommands.Add(wxT("xfce4-terminal -H -x")); }
if (LXTERMINAL_PRESENT)
  { PossibleTerminals.Add(wxT("lxterminal")); PossibleTerminalCommands.Add(wxT("lxterminal -e")); }
if (QTERMINAL_PRESENT)
  { PossibleTerminals.Add(wxT("qterminal")); PossibleTerminalCommands.Add(wxT("qterminal -e")); }
if (MATETERMINAL_PRESENT)
  { PossibleTerminals.Add(wxT("mate-terminal")); PossibleTerminalCommands.Add(wxT("mate-terminal -e")); }

if (KONSOLE_PRESENT || GNOME_TERM_PRESENT || XTERM_PRESENT || LXTERMINAL_PRESENT || QTERMINAL_PRESENT || MATETERMINAL_PRESENT || XCFE4_PRESENT) // We need at least 1 to be available
  { if (KONSOLE_PRESENT) 
      { TERMINAL = wxT("konsole"); TERMINAL_COMMAND = wxT("konsole -e ");
        if (Distro==fedora && Version == 9 && XTERM_PRESENT)        // This comes with a konsole that doesn't --noclose !
                TOOLS_TERMINAL_COMMAND = wxT("xterm -hold -e ");
          else  
            { if (Distro==fedora && Version >= 10 && XTERM_PRESENT)// FC10 konsole does --noclose, but launches in the background,
                  TOOLS_TERMINAL_COMMAND = wxT("xterm -hold -e ");    //  but check first as the livecd doesn't come with xterm!
                else TOOLS_TERMINAL_COMMAND = wxT("konsole --noclose -e ");
            }
      }
     else if (GNOME_TERM_PRESENT) 
        { TERMINAL = wxT("gnome-terminal"); TERMINAL_COMMAND = wxT("gnome-terminal -x ");
          if (XTERM_PRESENT)  TOOLS_TERMINAL_COMMAND = wxT("xterm -hold -e ");  // Use xterm if it's present, as gnome-terminal lacks a 'stay-open' flag
            else TOOLS_TERMINAL_COMMAND = wxT("gnome-terminal -x ");
        }
     else if (XCFE4_PRESENT)  { TERMINAL = wxT("xfce4-terminal"); TERMINAL_COMMAND = wxT("xfce4-terminal -x "); TOOLS_TERMINAL_COMMAND = wxT("xfce4-terminal -H -x "); }
     else if (XTERM_PRESENT)  { TERMINAL = wxT("xterm"); TERMINAL_COMMAND = wxT("xterm -e "); TOOLS_TERMINAL_COMMAND = wxT("xterm -hold -e "); }
     else if (LXTERMINAL_PRESENT)   // If we're here, xterm is absent, so use lxterminal -e even though it lacks a 'stay-open' flag
        { TERMINAL = wxT("lxterminal"); TERMINAL_COMMAND = wxT("lxterminal -e "); TOOLS_TERMINAL_COMMAND = wxT("lxterminal -e "); }
     else if (QTERMINAL_PRESENT)   // Ditto for qterminal
        { TERMINAL = wxT("qterminal"); TERMINAL_COMMAND = wxT("qterminal -e "); TOOLS_TERMINAL_COMMAND = wxT("qterminal -e "); }
     else if (MATETERMINAL_PRESENT)   // Ditto for mate-terminal
        { TERMINAL = wxT("mate-terminal"); TERMINAL_COMMAND = wxT("mate-terminal -e "); TOOLS_TERMINAL_COMMAND = wxT("mate-terminal -e "); }
  }

ConfigureTerminals::Save();              // Save all of this
}

//static
void Configure::SaveDefaultEditors(enum distro Distro, double Version)
{
wxConfigBase::Get()->Write(wxT("Editors/Icons/NoOfKnownIcons"),13l);            // Save the icons first, in case no editors are found (whereupon we'd abort)
wxConfigBase::Get()->Write(wxT("Editors/Icons/a/Icon"),  wxT("kwrite.xpm"));
wxConfigBase::Get()->Write(wxT("Editors/Icons/a/Icontype"), 9l);
wxConfigBase::Get()->Write(wxT("Editors/Icons/b/Icon"),  wxT("gedit.xpm"));
wxConfigBase::Get()->Write(wxT("Editors/Icons/b/Icontype"), 9l);
wxConfigBase::Get()->Write(wxT("Editors/Icons/c/Icon"),  wxT("kedit.xpm"));
wxConfigBase::Get()->Write(wxT("Editors/Icons/c/Icontype"), 9l);
if ((Distro==debian  && (Version>=4.0) && (Version<8.7)) || (Distro==zenwalk && Version>=5.0))
  wxConfigBase::Get()->Write(wxT("Editors/Icons/d/Icon"), wxT("iceweasel.png"));
else
  wxConfigBase::Get()->Write(wxT("Editors/Icons/d/Icon"), wxT("firefox.png"));
wxConfigBase::Get()->Write(wxT("Editors/Icons/d/Icontype"), 15l);
wxConfigBase::Get()->Write(wxT("Editors/Icons/e/Icon"),  wxT("chrome-chromium.png"));
wxConfigBase::Get()->Write(wxT("Editors/Icons/e/Icontype"), 15l);
wxConfigBase::Get()->Write(wxT("Editors/Icons/f/Icon"),  wxT("abiword.png"));
wxConfigBase::Get()->Write(wxT("Editors/Icons/f/Icontype"), 15l);
wxConfigBase::Get()->Write(wxT("Editors/Icons/g/Icon"),  wxT("mousepad.png"));
wxConfigBase::Get()->Write(wxT("Editors/Icons/g/Icontype"), 15l);
wxConfigBase::Get()->Write(wxT("Editors/Icons/h/Icon"),  wxT("openoffice.png"));
wxConfigBase::Get()->Write(wxT("Editors/Icons/h/Icontype"), 15l);
wxConfigBase::Get()->Write(wxT("Editors/Icons/i/Icon"),  wxT("libreoffice.png"));
wxConfigBase::Get()->Write(wxT("Editors/Icons/i/Icontype"), 15l);
wxConfigBase::Get()->Write(wxT("Editors/Icons/j/Icon"),  wxT("mate-text-editor.png"));
wxConfigBase::Get()->Write(wxT("Editors/Icons/j/Icontype"), 15l);
wxConfigBase::Get()->Write(wxT("Editors/Icons/k/Icon"),  wxT("gjots.png"));
wxConfigBase::Get()->Write(wxT("Editors/Icons/k/Icontype"), 15l);
wxConfigBase::Get()->Write(wxT("Editors/Icons/l/Icon"),  wxT("unknown.xpm"));
wxConfigBase::Get()->Write(wxT("Editors/Icons/l/Icontype"), 9l);
wxConfigBase::Get()->Write(wxT("Editors/Icons/m/Icon"),  wxT("palemoon.png"));
wxConfigBase::Get()->Write(wxT("Editors/Icons/m/Icontype"), 15l);
wxConfigBase::Get()->Write(wxT("Editors/Icons/n/Icon"),  wxT("featherpad.png"));
wxConfigBase::Get()->Write(wxT("Editors/Icons/n/Icontype"), 15l);

wxConfigBase::Get()->Write(wxT("/Editors/NoOfEditors"), 0l);    // Save a zero count first, just so that it's stored first (& so can be more easily seen)
long count=0; wxString path(wxT("/Editors/"));

if (TestExistence(wxT("kwrite"), true))                      // Now see which of the likely possibilities are available
  { wxString key(path + wxChar(wxT('a') + count++)); key += wxT('/');
    wxConfigBase::Get()->Write(key+wxT("Editor"), wxT("kwrite")); wxConfigBase::Get()->Write(key+wxT("LaunchString"), wxT("kwrite"));
  }  // We can skip the rest for kwrite, as the zero defaults are correct
 else
  if (TestExistence(wxT("kate"), true))  // I doubt if anyone would want icons for both kwrite and kate
    { wxString key(path + wxChar(wxT('a') + count++)); key += wxT('/');
      wxConfigBase::Get()->Write(key+wxT("Editor"), wxT("kate")); wxConfigBase::Get()->Write(key+wxT("LaunchString"), wxT("kate"));
      wxConfigBase::Get()->Write(key+wxT("CanUseTabs"), true); // Reuse the kwrite icon, which I think kate does anyway
    }

if (TestExistence(wxT("gedit"), true))
  { wxString key(path + wxChar(wxT('a') + count++)); key += wxT('/');
    wxConfigBase::Get()->Write(key+wxT("Editor"), wxT("gedit")); wxConfigBase::Get()->Write(key+wxT("LaunchString"), wxT("gedit"));
    wxConfigBase::Get()->Write(key+wxT("CanUseTabs"), true); wxConfigBase::Get()->Write(key+wxT("IconNo"), 1l);
  }

if (TestExistence(wxT("xed"), true)) // Mint 18 labels some apps 'x', and uses xed to mean pluma
  { wxString key(path + wxChar(wxT('a') + count++)); key += wxT('/');
    wxConfigBase::Get()->Write(key+wxT("Editor"), wxT("xed")); wxConfigBase::Get()->Write(key+wxT("LaunchString"), wxT("xed"));
    wxConfigBase::Get()->Write(key+wxT("CanUseTabs"), true); wxConfigBase::Get()->Write(key+wxT("IconNo"), 9l);
  }
 else if (TestExistence(wxT("pluma"), true)) // Recent versions of Mate use pluma, not mate-text-editor
  { wxString key(path + wxChar(wxT('a') + count++)); key += wxT('/');
    wxConfigBase::Get()->Write(key+wxT("Editor"), wxT("pluma")); wxConfigBase::Get()->Write(key+wxT("LaunchString"), wxT("pluma"));
    wxConfigBase::Get()->Write(key+wxT("CanUseTabs"), true); wxConfigBase::Get()->Write(key+wxT("IconNo"), 9l);
  }
 else if (TestExistence(wxT("mate-text-editor"), true))
  { wxString key(path + wxChar(wxT('a') + count++)); key += wxT('/');
    wxConfigBase::Get()->Write(key+wxT("Editor"), wxT("pluma")); wxConfigBase::Get()->Write(key+wxT("LaunchString"), wxT("mate-text-editor"));
    wxConfigBase::Get()->Write(key+wxT("CanUseTabs"), true); wxConfigBase::Get()->Write(key+wxT("IconNo"), 9l);
  }

if (TestExistence(wxT("mousepad"), true))
  { wxString key(path + wxChar(wxT('a') + count++)); key += wxT('/');
    wxConfigBase::Get()->Write(key+wxT("Editor"), wxT("mousepad")); wxConfigBase::Get()->Write(key+wxT("LaunchString"), wxT("mousepad"));
    wxConfigBase::Get()->Write(key+wxT("CanUseTabs"), false); wxConfigBase::Get()->Write(key+wxT("IconNo"), 6l);
  }

if (TestExistence(wxT("leafpad"), true))
  { wxString key(path + wxChar(wxT('a') + count++)); key += wxT('/');
    wxConfigBase::Get()->Write(key+wxT("Editor"), wxT("leafpad")); wxConfigBase::Get()->Write(key+wxT("LaunchString"), wxT("leafpad"));
    wxConfigBase::Get()->Write(key+wxT("CanUseTabs"), false); wxConfigBase::Get()->Write(key+wxT("IconNo"), 6l);
  }

if (TestExistence(wxT("featherpad"), true))
  { wxString key(path + wxChar(wxT('a') + count++)); key += wxT('/');
    wxConfigBase::Get()->Write(key+wxT("Editor"), wxT("featherpad")); wxConfigBase::Get()->Write(key+wxT("LaunchString"), wxT("featherpad"));
    wxConfigBase::Get()->Write(key+wxT("CanUseTabs"), true); wxConfigBase::Get()->Write(key+wxT("IconNo"), 13l);
  }

if (TestExistence(wxT("palemoon"), true))
  { wxString key(path + wxChar(wxT('a') + count++)); key += wxT('/');
    wxConfigBase::Get()->Write(key+wxT("palemoon"), wxT("palemoon")); wxConfigBase::Get()->Write(key+wxT("LaunchString"), wxT("palemoon"));
    wxConfigBase::Get()->Write(key+wxT("CanUseTabs"), false); wxConfigBase::Get()->Write(key+wxT("IconNo"), 12l);
  }
 else
	if (TestExistence(wxT("firefox"), true))
	  { wxString key(path + wxChar(wxT('a') + count++)); key += wxT('/');
		wxConfigBase::Get()->Write(key+wxT("Editor"), wxT("firefox")); wxConfigBase::Get()->Write(key+wxT("LaunchString"), wxT("firefox"));
		wxConfigBase::Get()->Write(key+wxT("CanUseTabs"), false); wxConfigBase::Get()->Write(key+wxT("IconNo"), 3l);
	  }

if (TestExistence(wxT("chromium-browser"), true))
  { wxString key(path + wxChar(wxT('a') + count++)); key += wxT('/');
    wxConfigBase::Get()->Write(key+wxT("Editor"), wxT("chromium")); wxConfigBase::Get()->Write(key+wxT("LaunchString"), wxT("chromium-browser"));
    wxConfigBase::Get()->Write(key+wxT("CanUseTabs"), false); wxConfigBase::Get()->Write(key+wxT("IconNo"), 4l);
  }

if (TestExistence(wxT("libreoffice"), true))
  { wxString key(path + wxChar(wxT('a') + count++)); key += wxT('/');
    wxConfigBase::Get()->Write(key+wxT("Editor"), wxT("libreoffice")); wxConfigBase::Get()->Write(key+wxT("LaunchString"), wxT("libreoffice"));
    wxConfigBase::Get()->Write(key+wxT("CanUseTabs"), false); wxConfigBase::Get()->Write(key+wxT("IconNo"), 8l);
  }
 else if (TestExistence(wxT("openoffice"), true))
  { wxString key(path + wxChar(wxT('a') + count++)); key += wxT('/');
    wxConfigBase::Get()->Write(key+wxT("Editor"), wxT("openoffice")); wxConfigBase::Get()->Write(key+wxT("LaunchString"), wxT("openoffice"));
    wxConfigBase::Get()->Write(key+wxT("CanUseTabs"), false); wxConfigBase::Get()->Write(key+wxT("IconNo"), 7l);
  }
 else if (TestExistence(wxT("ooffice"), true))
  { wxString key(path + wxChar(wxT('a') + count++)); key += wxT('/');
    wxConfigBase::Get()->Write(key+wxT("Editor"), wxT("openoffice")); wxConfigBase::Get()->Write(key+wxT("LaunchString"), wxT("ooffice"));
    wxConfigBase::Get()->Write(key+wxT("CanUseTabs"), false); wxConfigBase::Get()->Write(key+wxT("IconNo"), 7l);
  }

if (TestExistence(wxT("gjots2"), true))
  { wxString key(path + wxChar(wxT('a') + count++)); key += wxT('/');
    wxConfigBase::Get()->Write(key+wxT("Editor"), wxT("gjots2")); wxConfigBase::Get()->Write(key+wxT("LaunchString"), wxT("gjots2"));
    wxConfigBase::Get()->Write(key+wxT("CanUseTabs"), true); wxConfigBase::Get()->Write(key+wxT("IconNo"), 10l);
  }

wxConfigBase::Get()->Write(wxT("/Editors/NoOfEditors"), count); // Now save the actual count
}

//static
void Configure::SaveDefaultUDTools()
{
wxConfigBase* config = wxConfigBase::Get();
const long count = 4;

  // xterm will often be used for these tools, and v271 has a bug that breaks the display for command with a -h parameter
  // So check and filter it out for the affected distro versions e.g. fedora 16
bool remove_h = TestExistence(wxT("xterm")) &&  (wxExecute(wxT("sh -c \"xterm -v | grep -q 271\""), wxEXEC_SYNC) == 0);

config->Write(wxT("Tools/Launch/Label"), _("&Run a Program"));
config->Write(wxT("Tools/Launch/Count"), count);

config->Write(wxT("Tools/Launch/label_a"), wxT("df"));
config->Write(wxT("Tools/Launch/data_a"), remove_h ? wxT("df") : wxT("df -h"));
config->Write(wxT("Tools/Launch/terminal_a"), true);
config->Write(wxT("Tools/Launch/persists_a"), true);

config->Write(wxT("Tools/Launch/label_b"), wxT("du"));
config->Write(wxT("Tools/Launch/data_b"), remove_h ? wxT("du") : wxT("du -h"));
config->Write(wxT("Tools/Launch/terminal_b"), true);
config->Write(wxT("Tools/Launch/persists_b"), true);

config->Write(wxT("Tools/Launch/label_c"), wxT("md5sum"));
config->Write(wxT("Tools/Launch/data_c"), wxT("md5sum %f"));
config->Write(wxT("Tools/Launch/terminal_c"), true);
config->Write(wxT("Tools/Launch/persists_c"), true);

config->Write(wxT("Tools/Launch/label_d"), wxT("touch %a"));
config->Write(wxT("Tools/Launch/data_d"), wxT("touch %a"));
config->Write(wxT("Tools/Launch/terminal_d"), false);
config->Write(wxT("Tools/Launch/persists_d"), false);

if (wxFileExists(wxT("/etc/debian_version")))
  { config->Write(wxT("Tools/Launch/a/Label"), wxT("DEB"));
    config->Write(wxT("Tools/Launch/a/Count"), 6);
    
    config->Write(wxT("Tools/Launch/a/label_a"),wxString(_("Install .deb(s) as root:")) + wxT(" dpkg -i"));
    config->Write(wxT("Tools/Launch/a/data_a"), wxT("dpkg -i %a"));
    config->Write(wxT("Tools/Launch/a/asroot_a"), true);
    
    config->Write(wxT("Tools/Launch/a/label_b"),wxString(_("Remove the named.deb as root:")) + wxT(" dpkg -r"));
    config->Write(wxT("Tools/Launch/a/data_b"), wxT("dpkg -r %p"));
    config->Write(wxT("Tools/Launch/a/asroot_b"), true);
    
    config->Write(wxT("Tools/Launch/a/label_c"), _("List files provided by a particular .deb"));
    config->Write(wxT("Tools/Launch/a/data_c"), wxT("dpkg -L %p"));
    config->Write(wxT("Tools/Launch/a/terminal_c"), true);
    config->Write(wxT("Tools/Launch/a/persists_c"), true);
  
    config->Write(wxT("Tools/Launch/a/label_d"), _("List installed .debs matching the name:"));
    config->Write(wxT("Tools/Launch/a/data_d"), wxT("dpkg -l %p"));
    config->Write(wxT("Tools/Launch/a/terminal_d"), true);
    config->Write(wxT("Tools/Launch/a/persists_d"), true);
  
    config->Write(wxT("Tools/Launch/a/label_e"), _("Show if the named .deb is installed:"));
    config->Write(wxT("Tools/Launch/a/data_e"), wxT("sh -c \"dpkg -s %p | grep Status\""));
    config->Write(wxT("Tools/Launch/a/terminal_e"), true);
    config->Write(wxT("Tools/Launch/a/persists_e"), true);
  
    config->Write(wxT("Tools/Launch/a/label_f"), _("Show which package installed the selected file"));
    config->Write(wxT("Tools/Launch/a/data_f"), wxT("dpkg -S %f"));
    config->Write(wxT("Tools/Launch/a/terminal_f"), true);
    config->Write(wxT("Tools/Launch/a/persists_f"), true);
  }
 else if (wxDirExists(wxT("/etc/rpm")))
  { config->Write(wxT("Tools/Launch/a/Label"), wxT("RPM"));
    config->Write(wxT("Tools/Launch/a/Count"), 6);
    
    config->Write(wxT("Tools/Launch/a/label_a"),wxString(_("Install rpm(s) as root:")) + wxT(" rpm -U"));
    config->Write(wxT("Tools/Launch/a/data_a"), wxT("rpm -U %a"));
    config->Write(wxT("Tools/Launch/a/asroot_a"), true);
    
    config->Write(wxT("Tools/Launch/a/label_b"),wxString(_("Remove an rpm as root:")) + wxT(" rpm -e <name>"));
    config->Write(wxT("Tools/Launch/a/data_b"), wxT("rpm -e %p"));
    config->Write(wxT("Tools/Launch/a/asroot_b"), true);
    
    config->Write(wxT("Tools/Launch/a/label_c"), _("Query the selected rpm"));
    config->Write(wxT("Tools/Launch/a/data_c"), wxT("rpm -qpi %f"));
    config->Write(wxT("Tools/Launch/a/terminal_c"), true);
    config->Write(wxT("Tools/Launch/a/persists_c"), true);
    
    config->Write(wxT("Tools/Launch/a/label_d"), _("List files provided by the selected rpm"));
    config->Write(wxT("Tools/Launch/a/data_d"), wxT("rpm -qpl %f"));
    config->Write(wxT("Tools/Launch/a/terminal_d"), true);
    config->Write(wxT("Tools/Launch/a/persists_d"), true);
    
    config->Write(wxT("Tools/Launch/a/label_e"), _("Query the named rpm"));
    config->Write(wxT("Tools/Launch/a/data_e"), wxT("rpm -qi %p"));
    config->Write(wxT("Tools/Launch/a/terminal_e"), true);
    config->Write(wxT("Tools/Launch/a/persists_e"), true);
    
    config->Write(wxT("Tools/Launch/a/label_f"), _("List files provided by the named rpm"));
    config->Write(wxT("Tools/Launch/a/data_f"), wxT("rpm -ql %p"));
    config->Write(wxT("Tools/Launch/a/terminal_f"), true);
    config->Write(wxT("Tools/Launch/a/persists_f"), true);
  }
 else   // Provide a sample of a root command for other distro types
  { config->Write(wxT("Tools/Launch/a/Label"), wxT("'As root' example"));
    config->Write(wxT("Tools/Launch/a/Count"), 1);
    
    config->Write(wxT("Tools/Launch/a/label_a"),wxString(_("Create a directory as root:")) + wxT(" mkdir %p"));
    config->Write(wxT("Tools/Launch/a/data_a"), wxT("mkdir %p"));
    config->Write(wxT("Tools/Launch/a/asroot_a"), true);
  }
}

//static
void Configure::SaveDefaultFileAssociations()
{
wxConfigBase* config = wxConfigBase::Get();
bool kw=false, ge=false, kt=false, mp=false, lp=false, pl=false, fp=false, xe=false, mte=false,    kp=false, gp=false, ev=false, ac=false, xr=false, al=false, oo=false, lo=false,
     im=false, gm=false, xv=false, eg=false, em=false, lxiqt=false,  ff=false,  pm=false, ch=false,   kf=false, xp=false, tm=false;
long cumcount=0;  wxArrayString txtarray, pdfarray, mmarray, graflabelarray, grafcmdarray;

wxString name;       // See if kwrite, gedit, ImageMagick etc are available. If so, inc count
name = wxT("kwrite");             if (TestExistence(name))  kw=true;
name = wxT("gedit");              if (TestExistence(name))  ge=true;
name = wxT("kate");               if (TestExistence(name))  kt=true;
name = wxT("mousepad");           if (TestExistence(name))  mp=true;
name = wxT("leafpad");            if (TestExistence(name))  lp=true;
name = wxT("featherpad");         if (TestExistence(name))  fp=true;
name = wxT("xed");                if (TestExistence(name))  xe=true;
 else { name = wxT("pluma"); if (TestExistence(name))  pl=true;
          else { name = wxT("mate-text-editor"); if (TestExistence(name))  mte=true; }
      }

name = wxT("kpdf");               if (TestExistence(name))  kp=true;
name = wxT("gpdf");               if (TestExistence(name))  gp=true;
name = wxT("evince");             if (TestExistence(name))  ev=true;
name = wxT("acroread");           if (TestExistence(name))  ac=true;
name = wxT("xreader");            if (TestExistence(name))  xr=true;
name = wxT("atril");              if (TestExistence(name))  al=true;
name = wxT("ooffice");            if (TestExistence(name))  oo=true;
name = wxT("libreoffice");        if (TestExistence(name))  lo=true;

name = wxT("firefox");            if (TestExistence(name))  ff=true;
name = wxT("palemoon");      if (TestExistence(name))  pm=true;
name = wxT("chromium-browser");   if (TestExistence(name))  ch=true;

name = wxT("xviewer");            if (TestExistence(name))  xv=true;
name = wxT("display");            if (TestExistence(name))  im=true;
name = wxT("gimp");               if (TestExistence(name))  gm=true;
name = wxT("eog");                if (TestExistence(name))  eg=true;
name = wxT("eom");                if (TestExistence(name))  em=true;
name = wxT("lximage-qt");         if (TestExistence(name))  lxiqt=true;

name = wxT("kaffeine");           if (TestExistence(name))  kf=true;
name = wxT("xplayer");            if (TestExistence(name))  xp=true;
name = wxT("totem");              if (TestExistence(name))  tm=true;

                          // First the data for the OpenWith dialog
config->Write(wxT("FileAssociations/Audio/dummy"), wxT(""));
config->Write(wxT("FileAssociations/Development/dummy"), wxT(""));
if (kw)
  { config->Write(wxT("FileAssociations/Editors/KWrite/Ext"), "txt,tex,readme");
    config->Write(wxT("FileAssociations/Editors/KWrite/Filepath"), wxT("kwrite"));
    config->Write(wxT("FileAssociations/Editors/KWrite/Command"), wxT("kwrite %s"));
    txtarray.Add(wxT("kwrite"));
  }
if (ge)
  { config->Write(wxT("FileAssociations/Editors/gedit/Ext"), "txt,tex,readme");
    config->Write(wxT("FileAssociations/Editors/gedit/Filepath"), wxT("gedit"));
    config->Write(wxT("FileAssociations/Editors/gedit/Command"), wxT("gedit %s"));
    txtarray.Add(wxT("gedit"));
  }
if (kt)
  { config->Write(wxT("FileAssociations/Editors/kate/Ext"), "txt,tex,readme");
    config->Write(wxT("FileAssociations/Editors/kate/Filepath"), wxT("kate"));
    config->Write(wxT("FileAssociations/Editors/kate/Command"), wxT("kate %s"));
    txtarray.Add(wxT("kate"));
  }
if (mp)
  { config->Write(wxT("FileAssociations/Editors/mousepad/Ext"), "txt,tex,readme");
    config->Write(wxT("FileAssociations/Editors/mousepad/Filepath"), wxT("mousepad"));
    config->Write(wxT("FileAssociations/Editors/mousepad/Command"), wxT("mousepad %s"));
    txtarray.Add(wxT("mousepad"));
  }
if (lp)
  { config->Write(wxT("FileAssociations/Editors/leafpad/Ext"), "txt,tex,readme");
    config->Write(wxT("FileAssociations/Editors/leafpad/Filepath"), wxT("leafpad"));
    config->Write(wxT("FileAssociations/Editors/leafpad/Command"), wxT("leafpad %s"));
    txtarray.Add(wxT("leafpad"));
  }
if (fp)
  { config->Write(wxT("FileAssociations/Editors/featherpad/Ext"), "txt,tex,readme");
    config->Write(wxT("FileAssociations/Editors/featherpad/Filepath"), wxT("featherpad"));
    config->Write(wxT("FileAssociations/Editors/featherpad/Command"), wxT("featherpad %s"));
    txtarray.Add(wxT("featherpad"));
  }
if (xe)
  { config->Write(wxT("FileAssociations/Editors/xed/Ext"), "txt,tex,readme");
    config->Write(wxT("FileAssociations/Editors/xed/Filepath"), wxT("xed"));
    config->Write(wxT("FileAssociations/Editors/xed/Command"), wxT("xed %s"));
    txtarray.Add(wxT("xed"));
  }
 else if (pl)
  { config->Write(wxT("FileAssociations/Editors/pluma/Ext"), "txt,tex,readme");
    config->Write(wxT("FileAssociations/Editors/pluma/Filepath"), wxT("pluma"));
    config->Write(wxT("FileAssociations/Editors/pluma/Command"), wxT("pluma %s"));
    txtarray.Add(wxT("pluma"));
  }
 else if (mte)
  { config->Write(wxT("FileAssociations/Editors/mate-text-editor/Ext"), "txt,tex,readme");
    config->Write(wxT("FileAssociations/Editors/mate-text-editor/Filepath"), wxT("mate-text-editor"));
    config->Write(wxT("FileAssociations/Editors/mate-text-editor/Command"), wxT("mate-text-editor %s"));
    txtarray.Add(wxT("mate-text-editor"));
  }
if (!txtarray.GetCount())  config->Write(wxT("FileAssociations/Editors/dummy"), wxT(""));  // If none is present, we still need a dummy entry

if (im)
  { config->Write(wxT("FileAssociations/Graphics/ImageMagick/Ext"), "gif,xpm,png,jpg,jpeg,bmp,svg");
    config->Write(wxT("FileAssociations/Graphics/ImageMagick/Filepath"), wxT("display"));
    config->Write(wxT("FileAssociations/Graphics/ImageMagick/Command"), wxT("display %s"));
    graflabelarray.Add(wxT("ImageMagick")); grafcmdarray.Add(wxT("display"));
  }
if (xv)
  { config->Write(wxT("FileAssociations/Graphics/Xviewer/Ext"), "gif,xpm,png,jpg,jpeg,bmp,svg");
    config->Write(wxT("FileAssociations/Graphics/Xviewer/Filepath"), wxT("xviewer"));
    config->Write(wxT("FileAssociations/Graphics/Xviewer/Command"), wxT("xviewer %s"));
    graflabelarray.Add(wxT("Xviewer")); grafcmdarray.Add(wxT("xviewer"));
  }
 else if (eg)
  { config->Write(wxT("FileAssociations/Graphics/EyeOfGnome/Ext"), "gif,xpm,png,jpg,jpeg,bmp,svg");
    config->Write(wxT("FileAssociations/Graphics/EyeOfGnome/Filepath"), wxT("eog"));
    config->Write(wxT("FileAssociations/Graphics/EyeOfGnome/Command"), wxT("eog %s"));
    graflabelarray.Add(wxT("Eye of gnome")); grafcmdarray.Add(wxT("eog"));
  }
 else if (em)
  { config->Write(wxT("FileAssociations/Graphics/EyeOfMate/Ext"), "gif,xpm,png,jpg,jpeg,bmp,svg");
    config->Write(wxT("FileAssociations/Graphics/EyeOfMate/Filepath"), wxT("eom"));
    config->Write(wxT("FileAssociations/Graphics/EyeOfMate/Command"), wxT("eom %s"));
    graflabelarray.Add(wxT("Eye of mate")); grafcmdarray.Add(wxT("eom"));
  }
if (gm)
  { config->Write(wxT("FileAssociations/Graphics/The Gimp/Ext"), "gif,xpm,png,jpg,jpeg,bmp,svg");
    config->Write(wxT("FileAssociations/Graphics/The Gimp/Filepath"), wxT("gimp"));
    config->Write(wxT("FileAssociations/Graphics/The Gimp/Command"), wxT("gimp %s"));
    graflabelarray.Add(wxT("The Gimp")); grafcmdarray.Add(wxT("gimp"));
  }
if (lxiqt)
  { config->Write(wxT("FileAssociations/Graphics/LXImage-Qt/Ext"), "gif,xpm,png,jpg,jpeg,bmp,svg");
    config->Write(wxT("FileAssociations/Graphics/LXImage-Qt/Filepath"), wxT("lximage-qt"));
    config->Write(wxT("FileAssociations/Graphics/LXImage-Qt/Command"), wxT("lximage-qt %s"));
    graflabelarray.Add(wxT("LXImage-Qt")); grafcmdarray.Add(wxT("lximage-qt"));
  }
if (!graflabelarray.GetCount())   config->Write(wxT("FileAssociations/Graphics/dummy"), wxT(""));

if (pm)
  { config->Write(wxT("FileAssociations/Internet/palemoon/Ext"), "html,htm");
    config->Write(wxT("FileAssociations/Internet/palemoon/Filepath"), wxT("palemoon"));
    config->Write(wxT("FileAssociations/Internet/palemoon/Command"), wxT("palemoon %s"));
  }
  else if (ff)
  { config->Write(wxT("FileAssociations/Internet/firefox/Ext"), "html,htm");
    config->Write(wxT("FileAssociations/Internet/firefox/Filepath"), wxT("firefox"));
    config->Write(wxT("FileAssociations/Internet/firefox/Command"), wxT("firefox %s"));
  }
 else if (ch)
  { config->Write(wxT("FileAssociations/Internet/chromium/Ext"), "html,htm");
    config->Write(wxT("FileAssociations/Internet/chromium/Filepath"), wxT("chromium-browser"));
    config->Write(wxT("FileAssociations/Internet/chromium/Command"), wxT("chromium-browser %s"));
  }
 else config->Write(wxT("FileAssociations/Internet/dummy"), wxT(""));

if (gp)
  { config->Write(wxT("FileAssociations/Office/gpdf/Ext"), "pdf");
    config->Write(wxT("FileAssociations/Office/gpdf/Filepath"), "pdf");
    config->Write(wxT("FileAssociations/Office/gpdf/Command"), wxT("gpdf %s"));
    pdfarray.Add(wxT("gpdf"));  // This is used later
  }

if (ev)
  { config->Write(wxT("FileAssociations/Office/evince/Ext"), "pdf");
    config->Write(wxT("FileAssociations/Office/evince/Filepath"), wxT("evince"));
    config->Write(wxT("FileAssociations/Office/evince/Command"), wxT("evince %s"));
    pdfarray.Add(wxT("evince"));
  }

if (kp)
  { config->Write(wxT("FileAssociations/Office/kpdf/Ext"), "pdf");
    config->Write(wxT("FileAssociations/Office/kpdf/Filepath"), wxT("kpdf"));
    config->Write(wxT("FileAssociations/Office/kpdf/Command"), wxT("kpdf %s"));
    pdfarray.Add(wxT("kpdf"));
  }

if (ac)
  { config->Write(wxT("FileAssociations/Office/acroread/Ext"), "pdf");
    config->Write(wxT("FileAssociations/Office/acroread/Filepath"), wxT("acroread"));
    config->Write(wxT("FileAssociations/Office/acroread/Command"), wxT("acroread %s"));
    pdfarray.Add(wxT("acroread"));
  }

if (xr)
  { config->Write(wxT("FileAssociations/Office/Xreader/Ext"),"pdf");
    config->Write(wxT("FileAssociations/Office/Xreader/Filepath"), wxT("xreader"));
    config->Write(wxT("FileAssociations/Office/Xreader/Command"), wxT("xreader %s"));
    pdfarray.Add(wxT("xreader"));
  }
 else if (al)
  { config->Write(wxT("FileAssociations/Office/atril/Ext"), "pdf");
    config->Write(wxT("FileAssociations/Office/atril/Filepath"), wxT("atril"));
    config->Write(wxT("FileAssociations/Office/atril/Command"), wxT("atril %s"));
    pdfarray.Add(wxT("atril"));
  }

if (lo)
  { config->Write(wxT("FileAssociations/Office/Libreoffice calc/Ext"), wxT("sxc,xls,ods"));
    config->Write(wxT("FileAssociations/Office/Libreoffice writer/Ext"), wxT("sxw,doc,odt,rtf"));
    config->Write(wxT("FileAssociations/Office/Libreoffice calc/Filepath"), wxT("localc"));
    config->Write(wxT("FileAssociations/Office/Libreoffice writer/Filepath"), wxT("lowriter"));
    config->Write(wxT("FileAssociations/Office/Libreoffice calc/Command"), wxT("localc %s"));
    config->Write(wxT("FileAssociations/Office/Libreoffice writer/Command"), wxT("lowriter %s"));
  }
 else if (oo)
  { config->Write(wxT("FileAssociations/Office/Openoffice calc/Ext"), wxT("sxc,xls,ods"));
    config->Write(wxT("FileAssociations/Office/Openoffice writer/Ext"), wxT("sxw,doc,odt,rtf"));
    config->Write(wxT("FileAssociations/Office/Openoffice calc/Filepath"), wxT("oocalc"));
    config->Write(wxT("FileAssociations/Office/Openoffice writer/Filepath"), wxT("oowriter"));
    config->Write(wxT("FileAssociations/Office/Openoffice calc/Command"), wxT("oocalc %s"));
    config->Write(wxT("FileAssociations/Office/Openoffice writer/Command"), wxT("oowriter %s"));
  }

if (!(lo || oo || kp || gp || ev || ac))  config->Write(wxT("FileAssociations/Office/dummy"), wxT(""));

if (kf)
  { config->Write(wxT("FileAssociations/Multimedia/kaffeine/Ext"), wxT("avi,mov,mpeg,mpg"));
    config->Write(wxT("FileAssociations/Multimedia/kaffeine/Filepath"), wxT("kaffeine"));
    config->Write(wxT("FileAssociations/Multimedia/kaffeine/Command"), wxT("kaffeine %s"));
    mmarray.Add(wxT("kaffeine"));
  }
  
if (xp)
  { config->Write(wxT("FileAssociations/Multimedia/xplayer/Ext"), wxT("avi,mov,mpeg,mpg"));
    config->Write(wxT("FileAssociations/Multimedia/xplayer/Filepath"), wxT("xplayer"));
    config->Write(wxT("FileAssociations/Multimedia/xplayer/Command"), wxT("xplayer %s"));
    mmarray.Add(wxT("xplayer"));
  }  
 else if (tm)
  { config->Write(wxT("FileAssociations/Multimedia/totem/Ext"), wxT("avi,mov,mpeg,mpg"));
    config->Write(wxT("FileAssociations/Multimedia/totem/Filepath"), wxT("totem"));
    config->Write(wxT("FileAssociations/Multimedia/totem/Command"), wxT("totem %s"));
    mmarray.Add(wxT("totem"));
  }
  
if (!(kf || xp || tm))  config->Write(wxT("FileAssociations/Multimedia/dummy"), wxT(""));

config->Write(wxT("FileAssociations/Other/dummy"), wxT(""));
config->Write(wxT("FileAssociations/Utilities/dummy"), wxT(""));

              // Now the Default App for Ext thing, and which apps to display in the Open menu
wxChar group=wxT('a');                                  // This will provide e.g. the 'b' etc in LaunchExt/b
config->Write(wxT("LaunchExt/count"), 0);             // Do an initial write, just to make it the first entry

if (txtarray.GetCount())
  { const int txtcount = 4; long appcount = txtarray.GetCount();
    wxString defaultlabel, defaultcmd;
    if (appcount) { defaultlabel = txtarray[0]; defaultcmd = defaultlabel << wxT(" %s"); } // Use the first of any commands as the default

    const wxString ext[txtcount] = { "txt", "tex", "readme" };
    for (int n=0; n < txtcount; ++n, ++group)
      { wxString key = wxT("LaunchExt/"); key << group;
        config->Write(key + wxT("/Ext"), ext[n]);
        config->Write(key + wxT("/appcount"), appcount);
        config->Write(key + wxT("/DefaultLabel"), defaultlabel);
        config->Write(key + wxT("/DefaultCommand"), defaultcmd);
        
        wxChar which=wxT('a');
        for (int app=0; app < (int)appcount; ++app, ++which)
          { wxString appkey = key + wxT('/'); appkey << which;      // Thus making appkey LaunchExt/b/a, LaunchExt/b/b etc
            wxString command(txtarray[app]); command << wxT(" %s");
            config->Write(appkey + wxT("/Label"), txtarray[app]);
            config->Write(appkey + wxT("/Command"), command);
          }
      }

    cumcount += txtcount;
  }

if (pdfarray.GetCount())
  { const int pdfcount = 2; long appcount = pdfarray.GetCount();
    wxString defaultlabel, defaultcmd;
    if (appcount) { defaultlabel = pdfarray[0]; defaultcmd = defaultlabel << wxT(" %s"); } // Use the first of any commands as the default

    const wxString ext[pdfcount] = { "pdf" };
    for (int n=0; n < pdfcount; ++n, ++group)
      { wxString key = wxT("LaunchExt/"); key << group;
        config->Write(key + wxT("/Ext"), ext[n]);
        config->Write(key + wxT("/appcount"), appcount);
        config->Write(key + wxT("/DefaultLabel"), defaultlabel);
        config->Write(key + wxT("/DefaultCommand"), defaultcmd);
        
        wxChar which=wxT('a');
        for (int app=0; app < (int)appcount; ++app, ++which)
          { wxString appkey = key + wxT('/'); appkey << which;          // Thus making appkey LaunchExt/b/a, LaunchExt/b/b etc
            wxString command(pdfarray[app]); command << wxT(" %s");
            config->Write(appkey + wxT("/Label"), pdfarray[app]);
            config->Write(appkey + wxT("/Command"), command);
          }
      }

    cumcount += pdfcount;
  }

if (oo || lo)
  { const int oocount = 7;
    const wxString ext[oocount] = { wxT("sxw"), wxT("doc"), wxT("odt"), wxT("rtf"), wxT("xls"), wxT("ods"), wxT("sxc") };
    for (int n=0; n < 4; ++n, ++group)                    // First the 4 writer exts
      { wxString key = wxT("LaunchExt/"); key << group;
        config->Write(key + wxT("/Ext"), ext[n]);
        config->Write(key + wxT("/appcount"), 1l);
        config->Write(key + wxT("/DefaultLabel"), lo ? wxT("Libreoffice writer") : wxT("Openoffice writer"));
        config->Write(key + wxT("/DefaultCommand"), lo ? wxT("lowriter %s") : wxT("oowriter %s"));
        
        config->Write(key + wxT("/a/Label"), lo ? wxT("Libreoffice writer"): wxT("Openoffice writer"));
        config->Write(key + wxT("/a/Command"), lo ? wxT("lowriter %s") : wxT("oowriter %s"));
      }
    for (int n=4; n < 7; ++n, ++group)                    // Then the 3 calc ones
      { wxString key = wxT("LaunchExt/"); key << group;
        config->Write(key + wxT("/Ext"),ext[n]);
        config->Write(key + wxT("/appcount"), 1l);
        config->Write(key + wxT("/DefaultLabel"), lo ? wxT("Libreoffice calc") : wxT("Openoffice calc"));
        config->Write(key + wxT("/DefaultCommand"), lo ? wxT("localc %s") : wxT("oocalc %s"));
        
        config->Write(key + wxT("/a/Label"), lo ? wxT("Libreoffice calc") : wxT("Openoffice calc"));
        config->Write(key + wxT("/a/Command"), lo ? wxT("localc %s") : wxT("oocalc %s"));
      }
    cumcount += oocount;
  }

if (pm)
  { const int pmcount = 4; 
    const wxString ext[pmcount] = { "html", "htm" };
    for (int n=0; n < pmcount; ++n, ++group)
      { wxString key = wxT("LaunchExt/"); key << group;
        config->Write(key + wxT("/Ext"), ext[n]);
        config->Write(key + wxT("/appcount"), 1l);
        config->Write(key + wxT("/DefaultLabel"), wxT("palemoon"));
        config->Write(key + wxT("/DefaultCommand"), wxT("palemoon %s"));
        
        config->Write(key + wxT("/a/Label"), wxT("palemoon"));
        config->Write(key + wxT("/a/Command"), wxT("palemoon %s"));
      }
    cumcount += pmcount;
  }
 else if (ff)
  { const int ffcount = 4; 
    const wxString ext[ffcount] = { "html", "htm" };
    for (int n=0; n < ffcount; ++n, ++group)
      { wxString key = wxT("LaunchExt/"); key << group;
        config->Write(key + wxT("/Ext"), ext[n]);
        config->Write(key + wxT("/appcount"), 1l);
        config->Write(key + wxT("/DefaultLabel"), wxT("firefox"));
        config->Write(key + wxT("/DefaultCommand"), wxT("firefox %s"));
        
        config->Write(key + wxT("/a/Label"), wxT("firefox"));
        config->Write(key + wxT("/a/Command"), wxT("firefox %s"));
      }
    cumcount += ffcount;
  }
 else if (ch)
  { const int chcount = 4; 
    const wxString ext[chcount] = { "html", "htm" };
    for (int n=0; n < chcount; ++n, ++group)
      { wxString key = wxT("LaunchExt/"); key << group;
        config->Write(key + wxT("/Ext"), ext[n]);
        config->Write(key + wxT("/appcount"), 1l);
        config->Write(key + wxT("/DefaultLabel"), wxT("chromium"));
        config->Write(key + wxT("/DefaultCommand"), wxT("chromium-browser %s"));
        
        config->Write(key + wxT("/a/Label"), wxT("chromium"));
        config->Write(key + wxT("/a/Command"), wxT("chromium-browser %s"));
      }
    cumcount += chcount;
  }

if (graflabelarray.GetCount())
  { const int grafcount = 6;  long appcount = graflabelarray.GetCount();
    wxString defaultlabel, defaultcmd;
    if (xv) { defaultlabel = wxT("Xviewer"); defaultcmd = wxT("xviewer %s"); }
     else if (im) { defaultlabel = wxT("ImageMagick"); defaultcmd = wxT("display %s"); }
     else if (appcount)
      { defaultlabel = graflabelarray[0]; defaultcmd = grafcmdarray[0] << wxT(" %s"); } // Use the first of any available commands as the default
    const wxString ext[grafcount] = { "jpg", "png", "gif", "bmp", "xpm", "svg" };
    for (int n=0; n < grafcount; ++n, ++group)
      { wxString key = wxT("LaunchExt/"); key << group;
        config->Write(key + wxT("/Ext"), ext[n]);
        config->Write(key + wxT("/appcount"), appcount);
        config->Write(key + wxT("/DefaultLabel"), defaultlabel);
        config->Write(key + wxT("/DefaultCommand"), defaultcmd);
    
        wxChar which=wxT('a');
        for (int app=0; app < (int)appcount; ++app, ++which)
          { wxString appkey = key + wxT('/'); appkey << which;          // Thus making appkey LaunchExt/e/a, LaunchExt/e/b etc
            wxString command(grafcmdarray[app]); command << wxT(" %s");
            config->Write(appkey + wxT("/Label"), graflabelarray[app]);
            config->Write(appkey + wxT("/Command"), command);
          }
      }
    cumcount += grafcount;
  }

if (kf || xp || tm)
  { const int mmcount = 4; long appcount = kf + xp + tm;
    wxString defaultlabel, defaultcmd;
    if (xp) { defaultlabel = wxT("xplayer"); defaultcmd = wxT("xplayer %s"); }
     else if (kf) { defaultlabel = wxT("kaffeine"); defaultcmd = wxT("kaffeine %s"); }
     else  { defaultlabel = wxT("totem"); defaultcmd = wxT("totem %s"); }

    const wxChar* ext[mmcount] = { wxT("avi"), wxT("mov"), wxT("mpeg"), wxT("mpg") };
    for (int n=0; n < mmcount; ++n, ++group)
      { wxString key = wxT("LaunchExt/"); key << group;
        config->Write(key + wxT("/Ext"), wxString(ext[n]));
        config->Write(key + wxT("/appcount"), appcount);
        config->Write(key + wxT("/DefaultLabel"), defaultlabel);
        config->Write(key + wxT("/DefaultCommand"), defaultcmd);
        
        wxChar which=wxT('a');
        for (int app=0; app < (int)appcount; ++app, ++which)
          { wxString appkey = key + wxT('/'); appkey << which;
            wxString command(mmarray[app]); command << wxT(" %s");
            config->Write(appkey + wxT("/Label"), mmarray[app]);
            config->Write(appkey + wxT("/Command"), command);
          }
      }

    cumcount += mmcount;
  }

config->Write(wxT("LaunchExt/count"), cumcount);              // Write the final count
}

//static
void Configure::SaveDefaultDirCtrlTBButtons()
{
const long NoOfKnownIcons = 5;
wxConfigBase* config = wxConfigBase::Get();
                          // Start by creating the available-icon list
const wxChar* Icons[] = { wxT("gohome.xpm"), wxT("MyDocuments.xpm"), wxT("bm1_button.xpm"), wxT("bm2_button.xpm"), wxT("bm3_button.xpm") };
config->Write(wxT("/Misc/DirCtrlTB/Icons/NoOfKnownIcons"), NoOfKnownIcons);
for (long n=0; n < NoOfKnownIcons; ++n)
  { config->Write(wxT("/Misc/DirCtrlTB/Icons/")+CreateSubgroupName(n,NoOfKnownIcons)+wxT("/Icon"), Icons[n]);
    config->Write(wxT("/Misc/DirCtrlTB/Icons/")+CreateSubgroupName(n,NoOfKnownIcons)+wxT("/Icontype"), 9); // They're all xpms, as it happens
  }

config->Write(wxT("Misc/DirCtrlTB/Buttons/button_a/IconNo"),  0l);
config->Write(wxT("Misc/DirCtrlTB/Buttons/button_a/tooltip"),  _("Go to Home directory"));
config->Write(wxT("Misc/DirCtrlTB/Buttons/button_a/destination"),  wxGetApp().GetHOME());

wxString docs = StrWithSep(wxGetApp().GetHOME()) + wxT("Documents");
FileData stat(docs);                                     // Let's see if there's a dir called Documents
if (!stat.IsValid())
  { docs = StrWithSep(wxGetApp().GetHOME()) + wxT("documents");
    FileData stat(docs); if (!stat.IsValid()) return; // If not, & there isn't one called documents either, forget it
  }

config->Write(wxT("Misc/DirCtrlTB/Buttons/button_b/IconNo"),  1l);
config->Write(wxT("Misc/DirCtrlTB/Buttons/button_b/tooltip"),  _("Go to Documents directory"));
config->Write(wxT("Misc/DirCtrlTB/Buttons/button_b/destination"),  docs);
}

//static
void Configure::SaveDefaultMisc()
{
int width, height;
wxDisplaySize(&width, &height); width -= 30; height -= 100;           // Get the screen size, then chop off a bit for taskbar & contingencies
long vertsplit = width * 4 / 10, halfvertsplit = vertsplit / 2, horizsplit = height / 2;

wxConfigBase* config = wxConfigBase::Get();
config->Write(wxT("/Misc/AppHeight"), height);                        // Save 4Pane's initial size
config->Write(wxT("/Misc/AppWidth"), width);

config->Write(wxT("/Tabs/tab_a/verticalsplitpos"), vertsplit);        // Now the initial pane sizes etc
config->Write(wxT("/Tabs/tab_a/horizontalsplitpos"), horizsplit);
config->Write(wxT("/Tabs/tab_a/leftverticalsashpos"), halfvertsplit);
config->Write(wxT("/Tabs/tab_a/rightverticalsashpos"), halfvertsplit);// This actually allows more for the Rt fileview, which will have more cols
config->Write(wxT("/Tabs/tab_a/tophorizontalsashpos"), vertsplit);
config->Write(wxT("/Tabs/tab_a/bottomhorizontalsashpos"), vertsplit);
config->Write(wxT("/Tabs/tab_a/unsplitsashpos"), vertsplit);


wxString Lkey = wxT("/Tabs/tab_a/Lwidthcol"), Rkey = wxT("/Tabs/tab_a/Rwidthcol");
wxString LDeskey = wxT("LDesiredwidthcol"), RDeskey = wxT("RDesiredwidthcol");

config->Write(wxT("/Tabs/tab_a/Lwidthcol0"), halfvertsplit);
config->Write(wxT("/Tabs/tab_a/LDesiredwidthcol0"), halfvertsplit);
config->Write(wxT("/Tabs/tab_a/Rwidthcol0"), halfvertsplit);
config->Write(wxT("/Tabs/tab_a/RDesiredwidthcol0"), halfvertsplit);
config->Write(wxT("/Tabs/tab_a/Rwidthcol1"), halfvertsplit/4);    // Ext
config->Write(wxT("/Tabs/tab_a/RDesiredwidthcol1"), halfvertsplit/4);
config->Write(wxT("/Tabs/tab_a/Rwidthcol2"), halfvertsplit/4);    // Size
config->Write(wxT("/Tabs/tab_a/RDesiredwidthcol2"), halfvertsplit/4);
config->Write(wxT("/Tabs/tab_a/Rwidthcol3"), halfvertsplit/2);    // Time
config->Write(wxT("/Tabs/tab_a/RDesiredwidthcol3"), halfvertsplit/2);

config->Write(wxT("/Misc/FileOpen/UseSystem"), true);
config->Write(wxT("/Misc/FileOpen/UseBuiltin"), true);
config->Write(wxT("/Misc/FileOpen/PreferSystem"), true);
}

//static
void Configure::CopyConf(const wxString& iniFilepath)  // Copy any master conf file into the local ini
{
wxString conf; bool ConfAvailable = Configure::CheckForConfFile(conf);
if (ConfAvailable)
  wxCopyFile(conf, iniFilepath);
}

//static
void Configure::SaveDefaultsToIni()  // Make sure there is a valid config file, with reasonably-sensible defaults
{
wxConfigBase* config = wxConfigBase::Get(); 
if (config==NULL)    
  { wxString msg(_("If you are seeing this message (and you shouldn't!), it's because the configuration file couldn't be saved.\nThe least-unlikely reasons would be incorrect permissions or a Read Only partition."));
    wxMessageBox(msg); return;
  }

config->SetPath(wxT("/"));
config->Write(wxT("Aardvark"),  wxT("Dummy first line"));    // Since wxFileConfig tends to be alphabetical in its Group order, and sometimes crashes when it tries to insert at line 0

double Version; enum distro Distro = DeduceDistro(Version);  
if (!config->Exists(wxT("Devices/Misc/FloppyDevicesAutomount")))  SaveProbableHAL_Sys_etcInfo(Distro, Version);
SaveDefaultSus();
if (!config->Exists(wxT("/Misc/Terminal/TERMINAL")))  SaveDefaultTerminal(Distro, Version);
if (!config->Exists(wxT("/Editors/NoOfEditors"))) SaveDefaultEditors(Distro, Version);
if (!config->Exists(wxT("Tools/Launch/Label")))  SaveDefaultUDTools();
if (!config->Exists(wxT("FileAssociations/Audio")))  SaveDefaultFileAssociations();
SaveDefaultDirCtrlTBButtons();
SaveDefaultMisc();
config->Write(wxT("/Network/ShowmountFilepath"), SHOWMOUNTFPATH);      // In case we found it in /sbin/

config->Write(wxT("Misc/Identifier"),  wxT("Genuine config file"));    // Accept no substitutes
config->Flush();
}

//static
bool Configure::CheckForConfFile(wxString& conf)  // Is there a 4Pane.conf file anywhere, from which to load some config data?
{
wxString filepath = wxGetHomeDir() + wxT("/4Pane.conf"); FileData home(filepath);
if (home.IsValid()) { conf = home.GetUltimateDestination(); return true; }  // If there's a 4Pane.conf at  home, return it (allowing for symlinkage)

filepath = wxGetHomeDir() + wxT("/4pane.conf"); FileData home1(filepath);
if (home1.IsValid()) { conf = home1.GetUltimateDestination(); return true; }// Ditto lowercase

filepath = StripSep(wxGetApp().GetXDGdatadir()) + wxT("/4Pane.conf"); FileData xdg(filepath); // Be XDG-aware
if (xdg.IsValid()) { conf = xdg.GetUltimateDestination(); return true; }

filepath = StripSep(wxGetApp().GetXDGdatadir()) + wxT("/4pane.conf"); FileData xdg1(filepath);
if (xdg1.IsValid()) { conf = xdg1.GetUltimateDestination(); return true; }

filepath = GetCwd() + wxT("/4Pane.conf"); FileData Cwd(filepath);
if (Cwd.IsValid()) { conf = Cwd.GetUltimateDestination(); return true; }    // Now look in CWD

filepath = GetCwd() + wxT("/4pane.conf"); FileData Cwd1(filepath);
if (Cwd1.IsValid()) { conf = Cwd1.GetUltimateDestination(); return true; }

filepath = wxT("/etc/4Pane/4Pane.conf"); FileData etc(filepath);            // And finally,  /etc/4[Pp]ane/4[Pp]ane.conf
if (etc.IsValid()) { conf = etc.GetUltimateDestination(); return true; }

filepath = wxT("/etc/4Pane/4pane.conf"); FileData etc1(filepath);
if (etc1.IsValid()) { conf = etc1.GetUltimateDestination(); return true; }

filepath = wxT("/etc/4pane/4Pane.conf"); FileData etc2(filepath);
if (etc2.IsValid()) { conf = etc2.GetUltimateDestination(); return true; }

filepath = wxT("/etc/4pane/4pane.conf"); FileData etc3(filepath);
if (etc3.IsValid()) { conf = etc3.GetUltimateDestination(); return true; }

return false;
}

//static
bool Configure::Wizard(const wxString& configFPath)
{
NoConfigWizard* oz = new NoConfigWizard(NULL, wxID_ANY, wxT("4Pane configuration wizard"), configFPath);
bool result = oz->RunWizard(oz->Page1);
  
oz->Destroy();
return result;
}

bool Configure::TestExistence(wxString name, bool executable/*=false*/)  // Is 'name' in $PATH, and optionally is it executable by us
{
if (name.IsEmpty()) return false;

if (wxIsAbsolutePath(name))                           // First try it neat, if it's an absolute filepath (which it seldom will be)
  { FileData stat(name);
    if (stat.IsValid())
      return (executable ? stat.CanTHISUserExecute() : true);
  }

FileData stat(name);                                  // Next look in the cwd, just on the off-chance
if (stat.IsValid())
  return (executable ? stat.CanTHISUserExecute() : true);

wxString Path(wxT("PATH")), PATH;
wxGetEnv(Path, &PATH);                                // Get $PATH into PATH

wxStringTokenizer tkn(PATH, wxT(":"), wxTOKEN_STRTOK);
size_t count = tkn.CountTokens();
for (size_t c=0; c < count; ++c)                      // Go thru the PATH, looking for name in the filepath
  { wxString next = tkn.GetNextToken();
    FileData stat(next + wxFILE_SEP_PATH + name);
    if (!stat.IsValid()) continue;
    return (executable ? stat.CanTHISUserExecute() : true);  // Found it. If we were asked just for existence, return true. Else return true if we can execute.
  }

return false;                                         // If we're here, it wasn't found
}

void Configure::LoadConfiguration()
{
ToolbarIconsDialog::LoadDirCtrlTBinfo(DirCtrlTBinfoarray);  // Load info on the desired extra buttons to load to each Dirview mini-toolbar
LoadMisc();
}

void Configure::SaveConfiguration()
{
SaveMisc();
wxConfigBase::Get()->Flush();
}

void Configure::LoadMisc()    // Load the one-off things like Filter history
{
ConfigureDisplay::LoadDisplayConfig();              // Load the 'Display' stuff e.g. ASK_ON_DELETE
ConfigureTerminals::LoadTerminalsConfig();          // Ditto terminals
ConfigureNet::LoadNetworkConfig();                  // and network
ConfigureMisc::LoadMiscConfig();                    // and Misc

wxConfigBase* config = wxConfigBase::Get();  if (config==NULL)  return; 

MAX_COMMAND_HISTORY = (size_t)config->Read(wxT("/Misc/MaxCommandHistory"), 15l);
config->Read(wxT("/Misc/RETAIN_REL_TARGET"), &RETAIN_REL_TARGET, 1);  // Whether, when a relative symlink is moved, its target remains unchanged
config->Read(wxT("/Misc/RETAIN_MTIME_ON_PASTE"), &RETAIN_MTIME_ON_PASTE, 0); // Do we want Move/Paste to keep the modification time of the origin file

config->SetPath(wxT("/History/FilterHistory/"));
size_t count = config->GetNumberOfEntries();        // Count the entries
FilterHistory.Clear();

wxString item, key, histprefix(wxT("hist_"));  
for (size_t n=0; n < count; ++n)                    // Create the key for every item, from hist_a to hist_('a'+count-1)
  { key.Printf(wxT("%c"), wxT('a') + (wxChar)n);    // Make key hold a, b, c etc
    config->Read(histprefix+key, &item);
    if (item.IsEmpty())   continue;
    FilterHistory.Add(item);
  }

WHICHGREP = (greptype)config->Read(wxT("/Misc/whichgrep"), (long)PREFER_QUICKGREP); // Which flavour of grep does the user prefer

config->SetPath(wxT("/"));
}

void Configure::SaveMisc()    // Save the one-off things like Filter history
{
wxConfigBase* config = wxConfigBase::Get();
if (config==NULL)  { wxLogError(_("Couldn't load configuration!?")); return; }

config->Write(wxT("/Misc/RETAIN_REL_TARGET"), RETAIN_REL_TARGET);  // Whether, when a relative symlink is moved, its target remains unchanged
config->Write(wxT("/Misc/RETAIN_MTIME_ON_PASTE"), RETAIN_MTIME_ON_PASTE); // Do we want Move/Paste to keep the modification time of the origin file

config->DeleteGroup(wxT("/History/FilterHistory"));   // Delete current info, otherwise we'll end up with duplicates or worse
config->SetPath(wxT("/History/FilterHistory/"));

size_t count = wxMin(FilterHistory.GetCount(), MAX_COMMAND_HISTORY);  // Count the entries to be saved: not TOO many

wxString item, key, histprefix(wxT("hist_"));         // Create the key for every item, from hist_a to hist_('a'+count-1)
for (size_t n=0;  n < count; ++n)                     // The newest command is stored at the beginning of the array
  { key.Printf(wxT("%c"), wxT('a') + (wxChar)n);      // Make key hold a, b, c etc
    config->Write(histprefix+key, FilterHistory[n]);  // Save the entry
  }

config->Write(wxT("/Misc/whichgrep"), (long)WHICHGREP); // Which flavour of grep does the user prefer

config->SetPath(wxT("/"));
}

void Configure::ConfigureTooltips()  // Globally turn tooltips on/off, set their delay
{
#if defined __WXGTK__
  wxConfigBase* config = wxConfigBase::Get();
  if (config==NULL)  return;

  unsigned long delay = config->Read(wxT("/Misc/TooltipDelay"), 2000l);
  if (delay > 600000) delay = 2000;                    // If delay is more than 10 mins, it's probably an error!

  wxDialog dlg;
  wxXmlResource::Get()->LoadDialog(&dlg, wxWindow::FindFocus(), wxT("TooltipDlg"));
  if (!delay)  ((wxCheckBox*)dlg.FindWindow(wxT("Check")))->SetValue(true);  // If delay is 0, the 'Turned off' checkbox is set
    else ((wxSpinCtrl*)dlg.FindWindow(wxT("Spin")))->SetValue(delay / 1000);

  if (dlg.ShowModal() != wxID_OK)    return;          // If user cancelled, abort

  if (((wxCheckBox*)dlg.FindWindow(wxT("Check")))->IsChecked())
        delay = 0;
   else  delay = ((wxSpinCtrl*)dlg.FindWindow(wxT("Spin")))->GetValue() * 1000;

  config->Write(wxT("/Misc/TooltipDelay"), (long)delay);// Save the info
  config->Flush();

  wxToolTip::Enable(delay);                           // Implement any change
  if (delay)  wxToolTip::SetDelay(delay);
#endif
}

void Configure::ApplyTooltipPreferences()  // Loads tooltip choices, and makes it happen. Don't do this too early in the app, or it won't 'take'
{
#if defined __WXGTK__
  unsigned long delay = wxConfigBase::Get()->Read(wxT("/Misc/TooltipDelay"), 2000l);
  if (delay > 600000) delay = 2000;                   // If delay is more than 10 mins, it's probably an error!

  wxToolTip::Enable(delay);
  if (delay)  wxToolTip::SetDelay(delay);
#endif
}

/* XPM */
static const char * configure_xpm[] = {   // The spanner icon
"12 12 15 1",
" 	c None",
".	c #B6B6B6",
"+	c #DBDBDB",
"@	c #B6B6DB",
"#	c #FFFFFF",
"$	c #9292B6",
"%	c #929292",
"&	c #B6926D",
"*	c #6D6D92",
"=	c #DB9200",
"-	c #DB9224",
";	c #FF9200",
">	c #FFB624",
",	c #FFB600",
"'	c #B66D00",
"  .++@      ",
"   @##.     ",
"    +#+     ",
".$ .+#+     ",
".+.+++@     ",
"%@++++.&    ",
" *$...&==   ",
"     &-;;-  ",
"      ->>,- ",
"       ->;;'",
"        =;;'",
"         '' "};

/* XPM */
static const char * bullet6_xpm[] = {
"12 12 6 1",
" 	g None",
".	g #B6B6B6",
"+	g #6D6D6D",
"@	g #929292",
"#	g #494949",
"$	g #242424",
"            ",
"            ",
"            ",
"    .+++    ",
"   ..@+##   ",
"   +..#$$   ",
"   +++#$$   ",
"   ##$$$$   ",
"    $$$$    ",
"            ",
"            ",
"            "};
/* XPM */
static const char * configureRed_xpm[] = {
"12 12 10 1",
" 	c None",
".	c #FF4949",
"+	c #FF9F9F",
"@	c #FF4F4F",
"#	c #FFF9F9",
"$	c #FF0000",
"%	c #FF0404",
"&	c #FF1313",
"*	c #FF5757",
"=	c #FF5151",
"  .++@      ",
"   @##.     ",
"    +#+     ",
".$ .+#+     ",
".+.+++@     ",
"$@++++.$    ",
" $$...$$$   ",
"     $%&&%  ",
"      %**=% ",
"       %*&&$",
"        $&&$",
"         $$ "};
/* XPM */
static const char * bullet6Red_xpm[] = {
"12 12 6 1",
" 	c None",
".	c #FF9B9B",
"+	c #FF3D3D",
"@	c #FF6F6F",
"#	c #FF0F0F",
"$	c #DE0000",
"            ",
"            ",
"            ",
"    .+++    ",
"   ..@+##   ",
"   +..#$$   ",
"   +++#$$   ",
"   ##$$$$   ",
"    $$$$    ",
"            ",
"            ",
"            "};

void Configure::Configure4Pane()  // The main Configuration
{
const int wrongxrc = 1;
try 
  { // Protect against a user's ~/.4Pane pointing to the original-style configuredialogs.xrc
    wxLogNull shh; wxDialog dlg;
    bool loaded = wxXmlResource::Get()->LoadDialog(&dlg, NULL, wxT("FakeDlg")); // This doesn't exist in the old version
    if (!loaded) throw wrongxrc;
  }

catch(int i)
  { wxString errormsg(wxT("An error occured when trying to load the configure dialog.\nThat is probably because there are old configuration files on your computer.\
    \nI suggest you check your installation, and perhaps delete your ~/.4Pane file"));
    wxLogMessage(errormsg);
    return;
  }
catch(...){ return; }
  
MyConfigureDialog dlg;
wxXmlResource::Get()->LoadDialog(&dlg, MyFrame::mainframe, wxT("ConfigureDlg"));

CallersHelpcontext = HelpContext;                                         // Save the global to local store
HelpContext = HCconfigUDC;                                                //   & set it to UDCommands as this comes first

ConfigureNotebook* treebk = (ConfigureNotebook*)dlg.FindWindow(wxT("Treebook"));

wxImageList* imageList = new wxImageList(12, 12);
imageList->Add(wxIcon(configure_xpm));
imageList->Add(wxIcon(bullet6_xpm));
imageList->Add(wxIcon(configureRed_xpm));
imageList->Add(wxIcon(bullet6Red_xpm));
treebk->AssignImageList(imageList);

wxPanel* panel = (wxPanel*)wxXmlResource::Get()->LoadPanel(&dlg, wxT("ConfigureShortcuts"));
treebk->AddPage(panel, _("Shortcuts"), false, 0);

panel = (wxPanel*)wxXmlResource::Get()->LoadPanel(&dlg, wxT("UDCommands"));
treebk->AddPage(panel, _("Tools"), true, 0);
panel = (wxPanel*)wxXmlResource::Get()->LoadPanel(&dlg, wxT("UDCAddTool"));
treebk->AddSubPage(panel, _("Add a tool"), false, 1);
panel = (wxPanel*)wxXmlResource::Get()->LoadPanel(&dlg, wxT("UDCEditTool"));
treebk->AddSubPage(panel, _("Edit a tool"), false, 1);
panel = (wxPanel*)wxXmlResource::Get()->LoadPanel(&dlg, wxT("UDCDeleteTool"));
treebk->AddSubPage(panel, _("Delete a tool"), false, 1);

panel = (wxPanel*)wxXmlResource::Get()->LoadPanel(&dlg, wxT("DevicesSpiel"));
treebk->AddPage(panel, _("Devices"), false, 0);
panel = (wxPanel*)wxXmlResource::Get()->LoadPanel(&dlg, wxT("ConfigDevicesAutoMt"));
treebk->AddSubPage(panel,  _("Automount"), false, 1);
panel = (wxPanel*)wxXmlResource::Get()->LoadPanel(&dlg, wxT("ConfigDevicesMount"));
treebk->AddSubPage(panel, _("Mounting"), false, 1);
panel = (wxPanel*)wxXmlResource::Get()->LoadPanel(&dlg, wxT("ConfigDevicesUsb"));
treebk->AddSubPage(panel, _("Usb"), false, 1);
panel = (wxPanel*)wxXmlResource::Get()->LoadPanel(&dlg, wxT("ConfigRemovableDevices"));
treebk->AddSubPage(panel, _("Removable"), false, 1);
panel = (wxPanel*)wxXmlResource::Get()->LoadPanel(&dlg, wxT("ConfigFixedDevices"));
treebk->AddSubPage(panel, _("Fixed"), false, 1);
panel = (wxPanel*)wxXmlResource::Get()->LoadPanel(&dlg, wxT("DevicesAdvancedSpiel"));
treebk->AddSubPage(panel, _("Advanced"), false, 1);
panel = (wxPanel*)wxXmlResource::Get()->LoadPanel(&dlg, wxT("ConfigDevicesAdvancedA"));
treebk->InsertSubPage(11, panel, _("Advanced fixed"), false, 1);
panel = (wxPanel*)wxXmlResource::Get()->LoadPanel(&dlg, wxT("ConfigDevicesAdvancedB"));
treebk->InsertSubPage(11, panel, _("Advanced removable"), false, 1);
panel = (wxPanel*)wxXmlResource::Get()->LoadPanel(&dlg, wxT("ConfigDevicesAdvancedC"));
treebk->InsertSubPage(11, panel, _("Advanced lvm"), false, 1);


panel = (wxPanel*)wxXmlResource::Get()->LoadPanel(&dlg, wxT("DisplaySpiel"));
treebk->AddPage(panel, _("The Display"), false, 0);
panel = (wxPanel*)wxXmlResource::Get()->LoadPanel(&dlg, wxT("DisplayTrees"));
treebk->AddSubPage(panel, _("Trees"), false, 1);
panel = (wxPanel*)wxXmlResource::Get()->LoadPanel(&dlg, wxT("DisplayTreeFont"));
treebk->AddSubPage(panel, _("Tree font"), false, 1);
panel = (wxPanel*)wxXmlResource::Get()->LoadPanel(&dlg, wxT("DisplayMisc"));
treebk->AddSubPage(panel, _("Misc"), false, 1);

panel = (wxPanel*)wxXmlResource::Get()->LoadPanel(&dlg, wxT("TerminalsSpiel"));
treebk->AddPage(panel, _("Terminals"), false, 0);
panel = (wxPanel*)wxXmlResource::Get()->LoadPanel(&dlg, wxT("TerminalsReal"));
treebk->AddSubPage(panel, _("Real"), false, 1);
panel = (wxPanel*)wxXmlResource::Get()->LoadPanel(&dlg, wxT("TerminalsEmulator"));
treebk->AddSubPage(panel, _("Emulator"), false, 1);

panel = (wxPanel*)wxXmlResource::Get()->LoadPanel(&dlg, wxT("Networks"));
treebk->AddPage(panel, _("The Network"), false, 0);

panel = (wxPanel*)wxXmlResource::Get()->LoadPanel(&dlg, wxT("MiscSpiel"));
treebk->AddPage(panel, _("Miscellaneous"), false, 0);
panel = (wxPanel*)wxXmlResource::Get()->LoadPanel(&dlg, wxT("MiscUndo"));
treebk->AddSubPage(panel, _("Numbers"), false, 1);
panel = (wxPanel*)wxXmlResource::Get()->LoadPanel(&dlg, wxT("MiscBriefTimes"));
treebk->AddSubPage(panel, _("Times"), false, 1);
panel = (wxPanel*)wxXmlResource::Get()->LoadPanel(&dlg, wxT("MiscSU"));
treebk->AddSubPage(panel, _("Superuser"), false, 1);
panel = (wxPanel*)wxXmlResource::Get()->LoadPanel(&dlg, wxT("MiscOther"));
treebk->AddSubPage(panel, _("Other"), false, 1);

treebk->Init(this);
dlg.GetSizer()->Fit(&dlg); dlg.GetSizer()->Layout();

    // The treebook isn't in the chain of command for its pages' button events, so use the dialog as a relay
dlg.Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyConfigureDialog::OnEndModal), NULL, &dlg);
dlg.Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(MyConfigureDialog::OnClose), NULL, &dlg);

dlg.Connect(XRCID("Finished"), wxEVT_UPDATE_UI, wxUpdateUIEventHandler(ConfigureNotebook::OnFinishedBtnUpdateUI), NULL, treebk);

LoadPreviousSize(&dlg, wxT("MyConfigureDialog"));

dlg.CentreOnParent();
dlg.ShowModal();

SaveCurrentSize(&dlg, wxT("MyConfigureDialog"));

HelpContext = CallersHelpcontext;                                         // Revert to the original value
}


IMPLEMENT_DYNAMIC_CLASS(ConfigureNotebook, wxTreebook)

void MyConfigureDialog::OnEndModal(wxCommandEvent& event)
{
if (event.GetId() != XRCID("Finished"))
  { ConfigureNotebook* treebk = (ConfigureNotebook*)FindWindow(wxT("Treebook"));
    if (treebk) 
      { event.StopPropagation(); treebk->OnButtonPressed(event); return; }
  }

if (!((ConfigureNotebook*)FindWindow(wxT("Treebook")))->Dirty())  // If the notebook has unsaved data, give user a chance to save
    wxDialog::EndModal(event.GetId());
}

void MyConfigureDialog::OnClose(wxCloseEvent& event)  // If the notebook has unsaved data, give user a chance to save
{
if (!((ConfigureNotebook*)FindWindow(wxT("Treebook")))->Dirty())
  wxDialog::EndModal(wxID_CANCEL);                                       
 else
  event.Veto();
}


void ConfigureNotebook::Init(Configure* dad)
{
parent = dad;

long startpage = wxConfigBase::Get()->Read(wxT("/Configure/Startpage"), 0l);
if (startpage < (int)GetPageCount())  SetSelection(startpage);
 else SetSelection(0);
 
GetPage(startpage)->SetFocus(); // so that F1 will work even before the first click

ExpandNode(UDCommand);                                            // Make all (top-level) nodes expand
ExpandNode(ConfigDisplay);
ExpandNode(ConfigTerminals);
ExpandNode(ConfigMisc);
ExpandNode(ConfigDevices);

GetPage(UDCEditTool)->Connect(wxEVT_COMMAND_COMBOBOX_SELECTED, wxCommandEventHandler(ConfigureNotebook::OnUDCLabelComboSelectionChanged), NULL, this); 
GetPage(UDCDeleteTool)->Connect(wxEVT_COMMAND_COMBOBOX_SELECTED, wxCommandEventHandler(ConfigureNotebook::OnUDCLabelComboSelectionChanged), NULL, this); 


KONSOLE_PRESENT = Configure::TestExistence(wxT("konsole"));    // See if any terminals have been added/removed since last time
GNOME_TERM_PRESENT = Configure::TestExistence(wxT("gnome-terminal"));
XTERM_PRESENT = Configure::TestExistence(wxT("xterm"));
XCFE4_PRESENT = Configure::TestExistence(wxT("xfce4-terminal"));
LXTERMINAL_PRESENT = Configure::TestExistence(wxT("lxterminal"));
QTERMINAL_PRESENT = Configure::TestExistence(wxT("qterminal"));
MATETERMINAL_PRESENT = Configure::TestExistence(wxT("mate-terminal"));
                        // We can't just clear the arrays and start again: there may be user-provided entries too
if (KONSOLE_PRESENT)
  { if (PossibleTerminals.Index(wxT("konsole")) == wxNOT_FOUND) // If it's not in one, it won't be in the others either
      { PossibleTerminals.Add(wxT("konsole")); PossibleTerminalCommands.Add(wxT("konsole -e")); PossibleToolsTerminalCommands.Add(wxT("konsole --noclose -e")); }
  }
 else
  { if (PossibleTerminals.Index(wxT("konsole")) != wxNOT_FOUND)
      { PossibleTerminals.Remove(wxT("konsole")); PossibleTerminalCommands.Remove(wxT("konsole -e")); PossibleToolsTerminalCommands.Remove(wxT("konsole --noclose -e")); }
  }

if (GNOME_TERM_PRESENT)
  { if (PossibleTerminals.Index(wxT("gnome-terminal")) == wxNOT_FOUND)
      { PossibleTerminals.Add(wxT("gnome-terminal")); PossibleTerminalCommands.Add(wxT("gnome-terminal -x")); }
  }
 else
  { if (PossibleTerminals.Index(wxT("gnome-terminal")) != wxNOT_FOUND)
      { PossibleTerminals.Remove(wxT("gnome-terminal")); PossibleTerminalCommands.Remove(wxT("gnome-terminal -x")); }
  }

if (XTERM_PRESENT)  
  { if (PossibleTerminals.Index(wxT("xterm")) == wxNOT_FOUND)
      { PossibleTerminals.Add(wxT("xterm")); PossibleTerminalCommands.Add(wxT("xterm -e")); PossibleToolsTerminalCommands.Add(wxT("xterm -hold -e")); }
  }
 else
  { if (PossibleTerminals.Index(wxT("xterm")) != wxNOT_FOUND)
      { PossibleTerminals.Remove(wxT("xterm")); PossibleTerminalCommands.Remove(wxT("xterm -e")); PossibleToolsTerminalCommands.Remove(wxT("xterm -hold -e")); }
  }

if (XCFE4_PRESENT)  
  { if (PossibleTerminals.Index(wxT("xfce4-terminal")) == wxNOT_FOUND)
      { PossibleTerminals.Add(wxT("xfce4-terminal")); PossibleTerminalCommands.Add(wxT("xfce4-terminal -x")); PossibleToolsTerminalCommands.Add(wxT("xfce4-terminal -H -x")); }
  }
 else
  { if (PossibleTerminals.Index(wxT("xfce4-terminal")) != wxNOT_FOUND)
      { PossibleTerminals.Remove(wxT("xfce4-terminal")); PossibleTerminalCommands.Remove(wxT("xfce4-terminal -x")); PossibleToolsTerminalCommands.Remove(wxT("xfce4-terminal -H -x")); }
  }

if (LXTERMINAL_PRESENT)
  { if (PossibleTerminals.Index(wxT("lxterminal")) == wxNOT_FOUND)
      { PossibleTerminals.Add(wxT("lxterminal")); PossibleTerminalCommands.Add(wxT("lxterminal -e")); }
  }
 else
  { if (PossibleTerminals.Index(wxT("lxterminal")) != wxNOT_FOUND)
      { PossibleTerminals.Remove(wxT("lxterminal")); PossibleTerminalCommands.Remove(wxT("lxterminal -e")); }
  }

if (QTERMINAL_PRESENT)
  { if (PossibleTerminals.Index(wxT("qterminal")) == wxNOT_FOUND)
      { PossibleTerminals.Add(wxT("qterminal")); PossibleTerminalCommands.Add(wxT("qterminal -e")); }
  }
 else
  { if (PossibleTerminals.Index(wxT("qterminal")) != wxNOT_FOUND)
      { PossibleTerminals.Remove(wxT("qterminal")); PossibleTerminalCommands.Remove(wxT("qterminal -e")); }
  }

if (MATETERMINAL_PRESENT)  
  { if (PossibleTerminals.Index(wxT("mate-terminal")) == wxNOT_FOUND)
      { PossibleTerminals.Add(wxT("mate-terminal")); PossibleTerminalCommands.Add(wxT("mate-terminal -e")); }
  }
 else
  { if (PossibleTerminals.Index(wxT("mate-terminal")) != wxNOT_FOUND)
      { PossibleTerminals.Remove(wxT("mate-terminal")); PossibleTerminalCommands.Remove(wxT("mate-terminal -e")); }
  }

for (int n=0; n < NotSelected; ++n)   
  { InitPage((pagename)n);
    dirty[n] = false; 
  }
  // Now we've init.ed everything, it should be safe to allow UpdateUI to happen
Connect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(ConfigureNotebook::OnUpdateUI), NULL, this);
}


void ConfigureNotebook::InitPage(enum pagename page)
{
switch(page)
  { case UDCAddTool: {LMT = MyFrame::mainframe->Layout->m_notebook->LaunchFromMenu;
                      FolderArray = LMT->GetFolderArray(); ToolArray = LMT->GetToolarray();
                      
                      UDCCombo = (wxComboBox*)GetPage(UDCAddTool)->FindWindow(wxT("UDCMenus"));
                      UDCCombo->Clear(); size_t count=0; FillUDCMenusCombo(count, 0);  // Recursively fill the folders combo
                      #if (defined(__WXGTK__) || defined(__WXX11__))  // Show the first item in the combo textctrl
                        #if wxVERSION_NUMBER > 2902
                            if (!UDCCombo->IsListEmpty())  UDCCombo->SetSelection(0);
                        #else
                            if (!UDCCombo->IsEmpty())  UDCCombo->SetSelection(0);
                        #endif
                      #endif
                      wxCheckBox* asroot = (wxCheckBox*)(GetPage(UDCAddTool)->FindWindow(wxT("UDCAsRoot")));
                      if (asroot) asroot->SetValue(false);
                      ((wxCheckBox*)GetPage(UDCAddTool)->FindWindow(wxT("UDCTerminal")))->SetValue(false);
                      ((wxCheckBox*)GetPage(UDCAddTool)->FindWindow(wxT("UDCTerminalPersists")))->SetValue(false);
                      wxCheckBox* UDCRefreshPane = ((wxCheckBox*)GetPage(UDCAddTool)->FindWindow(wxT("UDCRefreshPane")));
                      if (UDCRefreshPane) // New fields so check they exist
                        { UDCRefreshPane->SetValue(false);
                          ((wxCheckBox*)GetPage(UDCAddTool)->FindWindow(wxT("UDCRefreshOpposite")))->SetValue(false);
                        }
                      StoreApplyBtn(GetPage(UDCAddTool)->FindWindow(wxT("UDCApply")));

                      UDCPage = UDCNew;
                      editing = false;                                              // Used by Edit, but needs to false in others
                      return;}
    case UDCEditTool: {UDCPage = UDCEdit;
                      LMT = MyFrame::mainframe->Layout->m_notebook->LaunchFromMenu;
                      FolderArray = LMT->GetFolderArray(); ToolArray = LMT->GetToolarray();
                           // Edit & Delete.  Reuse the ptrs, as there won't be any conflict
                      UDCCombo = (wxComboBox*)GetPage(UDCEditTool)->FindWindow(wxT("UDCLabelCombo"));
                      UDCCombo->Clear();                                        // Clear & fill the Labels combo
                      for (size_t n=0; n < ToolArray->GetCount(); ++n)  UDCCombo->Append(ToolArray->Item(n)->Label);
                      #if (defined(__WXGTK__) || defined(__WXX11__))
                        #if wxVERSION_NUMBER > 2902
                            if (!UDCCombo->IsListEmpty())  UDCCombo->SetSelection(0);
                        #else
                            if (!UDCCombo->IsEmpty())  UDCCombo->SetSelection(0);
                        #endif
                      #endif
                      wxCommandEvent event; event.SetInt(0); OnUDCLabelComboSelectionChanged(event);  // Fake an event to obtain the Filepath
                      StoreApplyBtn(GetPage(UDCEditTool)->FindWindow(wxT("UDCApply")));
                    
                      editing = false;                                              // Used by Edit, but needs to false in others
                      return;}
                      
    case UDCDeleteTool: {UDCPage = UDCDelete;
                      LMT = MyFrame::mainframe->Layout->m_notebook->LaunchFromMenu;
                      FolderArray = LMT->GetFolderArray(); ToolArray = LMT->GetToolarray();
                           // Edit & Delete.  Reuse the ptrs, as there won't be any conflict
                      UDCCombo = (wxComboBox*)GetPage(UDCDeleteTool)->FindWindow(wxT("UDCLabelCombo"));
                      UDCCombo->Clear();                                        // Clear & fill the Labels combo
                      for (size_t n=0; n < ToolArray->GetCount(); ++n)  UDCCombo->Append(ToolArray->Item(n)->Label);
                      #if (defined(__WXGTK__) || defined(__WXX11__))
                        #if wxVERSION_NUMBER > 2902
                            if (!UDCCombo->IsListEmpty())  UDCCombo->SetSelection(0);
                        #else
                            if (!UDCCombo->IsEmpty())  UDCCombo->SetSelection(0);
                        #endif
                      #endif
                      wxCommandEvent event; event.SetInt(0); OnUDCLabelComboSelectionChanged(event);  // Fake an event to obtain the Filepath
                      StoreApplyBtn(GetPage(UDCDeleteTool)->FindWindow(wxT("UDCApply")));
                    
                      editing = false;                                              // Used by Edit, but needs to false in others
                      return;}
                      
    case UDShortcuts: {
                      MyShortcutPanel* scpanel = (MyShortcutPanel*)GetPage(UDShortcuts);    // Get a ptr to the relevant panel, which is shared with AcceleratorList
                      scpanel->Init(MyFrame::mainframe->AccelList, true);
                      scpanel->GetSizer()->Layout();
                      
                      wxNotifyEvent event(MyListCtrlEvent);  // Send a custom event to panel to size listctrl columns.  Can't just tell it: list won't be properly made yet
                      wxPostEvent(scpanel, event);

                      return;}
                      
    case ConfigDevices:    ConfDevices = new DevicesPanel(this); return;
    case DevicesAdvanced:  ConfDevicesAdv = new DevicesAdvancedPanel(this); return;
    case ConfigDisplay:    ConfDisplay = new ConfigureDisplay(this); return;
    case ConfigTerminalsReal:  ConfTerminals = new ConfigureTerminals(this); return;
    case ConfigNet:        ConfNet = new ConfigureNet(this); return;
    case ConfigMisc:      ConfMisc = new ConfigureMisc(this); return;
       default:           return;
  }
}

void ConfigureNotebook::OnButtonPressed(wxCommandEvent& event)
{
int id = event.GetId();

if (id == XRCID("UDCNewSubmenu"))                                          // The Add LaunchCommand subfolder button
  { bool dup=0; wxString newfolderlabel; size_t current=UDCCombo->GetSelection();    // Get the index of the parent folder
    if (FolderArray->Item(current)->Subs > 25)
      { wxMessageBox(_("Sorry, there's no room for another submenu here\nI suggest you try putting it elsewhere")); return; }

    do
      { wxString currentfolder = UDCCombo->GetValue(); currentfolder.Trim(false); if (currentfolder.IsEmpty()) return;
        wxString msg(_("Enter the name of the new submenu to add to "));
        newfolderlabel = wxGetTextFromUser(msg+currentfolder);  if (newfolderlabel.IsEmpty()) return;
        
        for (size_t c=0;  c < FolderArray->Item(current)->Subs; ++c)          // Check the name isn't already taken
          { // We need to do a mnemonic-free comparison
            wxString MnFreeItem = FolderArray->Item(current+1+c)->Label; MnFreeItem.Replace(wxT("&"), wxT(""));
            wxString MnFreeLabel = newfolderlabel; MnFreeLabel.Replace(wxT("&"), wxT(""));
            if (MnFreeItem == MnFreeLabel)  { dup=1; break; }
          }
        if (dup)
          { wxMessageDialog dialog(this, _("Sorry, a menu with this name already exists\n                Try again?"), _("Oops!"), wxYES_NO | wxICON_ERROR);
            if (dialog.ShowModal() != wxID_YES)        return;
          }
      }
     while (dup);
    
    UDToolFolder* newfolder = new UDToolFolder;                               // Make a UDToolFolder to hold the new folder
    newfolder->Label = newfolderlabel;
    

    FolderArray->Item(current)->Subs++;                                       // Inc parent's count of subfolders
    if (current == FolderArray->GetCount()-1)   FolderArray->Add(newfolder);  // If parent is the last item, Append rather than Insert
      else   FolderArray->Insert(newfolder, current+1);                       // Otherwise InsertBefore current + 1

    UDCCombo->Clear(); size_t count=0; FillUDCMenusCombo(count, 0);           // The combo doesn't have an Insert, so empty & refill
    #if (defined(__WXGTK__) || defined(__WXX11__))
      #if wxVERSION_NUMBER > 2902
        if (!UDCCombo->IsListEmpty())  UDCCombo->SetSelection(0);
      #else
        if (!UDCCombo->IsEmpty())  UDCCombo->SetSelection(0);
      #endif
    #endif
    UDCCombo->SetSelection(current+1); dirty[UDCAddTool] = true; return;      // Set as the selection
  }

if (id == XRCID("UDCDeleteSubmenu"))                                          // The Delete LaunchCommand subfolder button
  { size_t DelFolder = UDCCombo->GetSelection();
    if (!DelFolder)
      { wxMessageDialog dialog(this, _("Sorry, you're not allowed to delete the root menu"), _("Oops!"), wxICON_ERROR);
        dialog.ShowModal(); return;
      }

    wxString msg, currentfolder = UDCCombo->GetValue(); currentfolder.Trim(false);
    msg.Printf(_("Delete menu \"%s\" and all its contents?"), currentfolder.c_str());
    wxMessageDialog dialog(this, msg, _("Are you sure?"), wxYES_NO | wxICON_QUESTION);
    if (dialog.ShowModal() != wxID_YES)    return;
    
    size_t entrycount=0, parentfolder=0; bool done=false;
    for (size_t n=0; n < DelFolder; ++n)
      { entrycount += FolderArray->Item(n)->Entries;                          // For every folder below the one 2b deleted, add its entries to entrycount
        if (!done)                                                            // If we haven't found the parent-folder yet,
          { if ((parentfolder + FolderArray->Item(n)->Subs) < DelFolder)      //  & this isn't it,
              parentfolder += FolderArray->Item(n)->Subs;                     //   add this folder's subfolders to the folder-count
             else done = true;
          }
      }
                                        // We now have enough info to do the deletion
    LMT->DeleteMenu(DelFolder, entrycount);
    FolderArray->Item(parentfolder)->Subs--;                                  // Dec parent's sub-count
    UDCCombo->Clear(); size_t count=0; FillUDCMenusCombo(count, 0);           // Instead of calculating how many menus to delete, it's easier to empty & refill
    #if (defined(__WXGTK__) || defined(__WXX11__))
      #if wxVERSION_NUMBER > 2902
        if (!UDCCombo->IsListEmpty())  UDCCombo->SetSelection(0);
      #else
        if (!UDCCombo->IsEmpty())  UDCCombo->SetSelection(0);
      #endif
    #endif
    dirty[UDCAddTool] = true; return;
  }

if (id == XRCID("UDCAddBtn"))                                                 // The UDC 'Add new tool' button
  { wxTextCtrl* UDCFilepath = (wxTextCtrl*)GetPage(UDCAddTool)->FindWindow(wxT("UDCFilepath")); 
    wxString command = UDCFilepath->GetValue();
    if (command.IsEmpty())  return;
    
    int folder = UDCCombo->GetSelection(); if (folder==-1) folder = 0;        // Add to this menu. If not found, use root  
      
    UDCFilepath->SetValue(wxT(""));
    wxString label = ((wxTextCtrl*)GetPage(UDCAddTool)->FindWindow(wxT("UDCLabel")))->GetValue();
    if (label.IsEmpty())  label = command;                                    // Suitable for simple tools like df -h
      else ((wxTextCtrl*)GetPage(UDCAddTool)->FindWindow(wxT("UDCLabel")))->ChangeValue(wxT(""));
    wxCheckBox* UDCAsroot = (wxCheckBox*)(GetPage(UDCAddTool)->FindWindow(wxT("UDCAsRoot")));
    bool asroot = (UDCAsroot && UDCAsroot->IsChecked());
    bool interminal = ((wxCheckBox*)GetPage(UDCAddTool)->FindWindow(wxT("UDCTerminal")))->IsChecked();
    bool terminalpersists = ((wxCheckBox*)GetPage(UDCAddTool)->FindWindow(wxT("UDCTerminalPersists")))->IsChecked();
    wxCheckBox* UDCRefreshPane = ((wxCheckBox*)GetPage(UDCAddTool)->FindWindow(wxT("UDCRefreshPane")));
    bool refreshpage(false), refreshopp(false);
    if (UDCRefreshPane) // New fields so check they exist
      { refreshpage = UDCRefreshPane->IsChecked();
        refreshopp = ((wxCheckBox*)GetPage(UDCAddTool)->FindWindow(wxT("UDCRefreshOpposite")))->IsChecked();
      }
    
    wxString commandstart = command.BeforeFirst(wxT(' '));                    // Get the real command, without parameters etc
    if (commandstart.GetChar(0) == wxT('~')) commandstart = wxGetHomeDir() + commandstart.Mid(1);
    if (!parent->TestExistence(commandstart, true))                           // Check there is such a command
      { wxString msg;  
        if (wxIsAbsolutePath(commandstart))  msg.Printf(_("I can't find an executable command \"%s\".\nContinue anyway?"), commandstart.c_str());
          else  msg.Printf(_("I can't find an executable command \"%s\" in your PATH.\nContinue anyway?"), commandstart.c_str());
        wxMessageDialog dialog(this, msg, _("Are you sure?"), wxYES_NO | wxICON_QUESTION);
        if (dialog.ShowModal() != wxID_YES)    return;
      }
    
    QueryWrapWithShell(command);                                              // Wraps command with sh -c if appropriate e.g. if pipes, or ~/foo
    bool result = LMT->AddTool(folder, command, asroot, interminal, terminalpersists, refreshpage, refreshopp, label);  // Do the addition
    
    dirty[UDCAddTool] = dirty[UDCAddTool] || result;    // It's dirty if it used to be, or if it needs to be now
    
    if (UDCAsroot) UDCAsroot->SetValue(false);          // Clear the checkboxes
    ((wxCheckBox*)GetPage(UDCAddTool)->FindWindow(wxT("UDCTerminal")))->SetValue(false);
    ((wxCheckBox*)GetPage(UDCAddTool)->FindWindow(wxT("UDCTerminalPersists")))->SetValue(false);
    if (UDCRefreshPane)
      { UDCRefreshPane->SetValue(false);
        ((wxCheckBox*)GetPage(UDCAddTool)->FindWindow(wxT("UDCRefreshOpposite")))->SetValue(false);
      }
    return;
  }

if (id == XRCID("UDCEditBtn"))                                                // The UDC 'Edit this tool' button
  { wxTextCtrl* UDCFilepath = (wxTextCtrl*)GetPage(UDCEditTool)->FindWindow(wxT("UDCFilepath"));
    wxButton* UDCEditBtn = (wxButton*)GetPage(UDCEditTool)->FindWindow(wxT("UDCEditBtn"));
    wxCheckBox* UDCAsroot = (wxCheckBox*)(GetPage(UDCEditTool)->FindWindow(wxT("UDCAsRoot")));
    wxCheckBox* UDCTerminal = ((wxCheckBox*)GetPage(UDCEditTool)->FindWindow(wxT("UDCTerminal")));
    wxCheckBox* UDCTerminalPersists = ((wxCheckBox*)GetPage(UDCEditTool)->FindWindow(wxT("UDCTerminalPersists")));
    wxCheckBox* UDCRefreshPane = ((wxCheckBox*)GetPage(UDCEditTool)->FindWindow(wxT("UDCRefreshPane")));
    wxCheckBox* UDCRefreshOpposite = ((wxCheckBox*)GetPage(UDCEditTool)->FindWindow(wxT("UDCRefreshOpposite")));
    if (!editing)                                                             // editing flags whether we're switching the edit on or off
      { UDCEditBtn->SetLabel(_(" Click when Finished ")); UDCEditBtn->GetContainingSizer()->Fit(UDCEditBtn);
        UDCCombo->Enable(false);                                              // Switch off the combo so that the selection can't be changed
        UDCFilepath->Enable(true); UDCAsroot && UDCAsroot->Enable(true); UDCTerminal->Enable(true); UDCRefreshPane && UDCRefreshPane->Enable(true); // Make editable
        editing = true; return;
      }
                      // Otherwise, this is the 2nd press to finish the editing, so process and revert the UI changes
    UDCEditBtn->SetLabel(_("  Edit this Command  ")); UDCEditBtn->GetContainingSizer()->Fit(UDCEditBtn);
    UDCCombo->Enable(true);  UDCFilepath->Enable(false); UDCTerminal->Enable(false); if (UDCRefreshPane) UDCRefreshPane->Enable(false);

    wxString newfpath = UDCFilepath->GetValue(); 
    if (newfpath.IsEmpty() || m_sel == wxNOT_FOUND) { editing = false; return; }
    QueryWrapWithShell(newfpath);                                             // Wraps command with sh -c if appropriate e.g. if pipes, or ~/foo
    if (newfpath != ToolArray->Item(m_sel)->Filepath
          || (UDCAsroot && (UDCAsroot->IsChecked() != ToolArray->Item(m_sel)->AsRoot))
          || UDCTerminal->IsChecked() != ToolArray->Item(m_sel)->InTerminal
          || UDCTerminalPersists->IsChecked() != ToolArray->Item(m_sel)->TerminalPersists
          || (UDCRefreshPane && UDCRefreshPane->IsChecked() != ToolArray->Item(m_sel)->RefreshPane)
          || (UDCRefreshOpposite && UDCRefreshOpposite->IsChecked() != ToolArray->Item(m_sel)->RefreshOpposite))
      dirty[UDCEditTool] = true;                                              // If the data's changed . . .
      
    ToolArray->Item(m_sel)->Filepath = newfpath;
    ToolArray->Item(m_sel)->AsRoot = UDCAsroot && UDCAsroot->IsChecked();
    ToolArray->Item(m_sel)->InTerminal = UDCTerminal->IsChecked();
    ToolArray->Item(m_sel)->TerminalPersists = UDCTerminalPersists->IsChecked();
    if (UDCRefreshPane) // New fields, so check they actually exist
      { ToolArray->Item(m_sel)->RefreshPane = UDCRefreshPane->IsChecked();
        ToolArray->Item(m_sel)->RefreshOpposite = UDCRefreshOpposite->IsChecked();
      }

    editing = false; return;
  }

if (id == XRCID("UDCDeleteBtn"))                                              // The UDC 'Delete this tool' button
  { int sel = UDCCombo->GetSelection(); if (sel == -1) return;
    wxString msg; msg.Printf(_("Delete command \"%s\"?"), UDCCombo->GetStringSelection().c_str());
    wxMessageDialog dialog(this, msg, _("Are you sure?"), wxYES_NO | wxICON_QUESTION);
    if (dialog.ShowModal() != wxID_YES)    return;
    
    LMT->DeleteTool(sel);                                                     // Remove the tool
    UDCCombo->Delete(sel);                                                    // & from the combo
    if (ToolArray->GetCount()) UDCCombo->SetSelection(wxMax(sel-1,0));
      else  UDCCombo->SetValue(wxT(""));                                      // If none left, need to clear the 'textctrl' bit
    wxCommandEvent event; event.SetInt(UDCCombo->GetSelection()); OnUDCLabelComboSelectionChanged(event); // Fake an event to update the Filepath
    
    dirty[UDCDeleteTool] = true; return;
  }

if (id == XRCID("UDCBrowse"))                                                 // The UDCBrowse button
  { static bool firsttime = true;          // The first time we run this, a default dir is needed.  After that, the last used dir is remembered
    wxString startdir; if (firsttime) startdir = StrWithSep(wxGetApp().GetHOME()); firsttime = false;
    wxFileDialog fdlg(this, _("Choose a file"), startdir, wxT(""), wxT("*"), wxFD_OPEN | wxFD_CHANGE_DIR);
    if (fdlg.ShowModal() == wxID_OK)  ((wxTextCtrl*)GetPage(UDCAddTool)->FindWindow(wxT("UDCFilepath")))->SetValue(fdlg.GetPath()); return;
  }

if (id == XRCID("UDCApply"))                                                  // The UDC 'OK' button
  { if (UDCPage==UDCEdit)                                                     // If this is the 'Edit' module, check the Label hasn't been altered
      { if (m_sel == wxNOT_FOUND) { dirty[UDCEditTool] = false; return; }
        wxString newstring = UDCCombo->GetValue();
        if (!newstring.IsEmpty() && newstring != ToolArray->Item(m_sel)->Label) // If the label has been changed, implement it
           ToolArray->Item(m_sel)->Label = newstring;
        dirty[UDCEditTool] = true;
      }
      
    if (!dirty[UDCAddTool] && !dirty[UDCEditTool] && !dirty[UDCDeleteTool]) return;
    LMT->SaveMenu();                                                    // Save the new data
    MyFrame::mainframe->AccelList->Init();                              // Now reload shortcut data, in case any UDtool shortcuts were just altered
    LMT->LoadMenu();                                                    // Now we can reload the UDtools, with any shortcuts correct
    InitPage((pagename)GetSelection()); dirty[UDCAddTool]=dirty[UDCEditTool]=dirty[UDCDeleteTool] = false; return;
  }

if (id == XRCID("UDCCancel"))                                           // The UDC 'Cancel' button
  { if (GetSelection()==UDCAddTool)  ((wxTextCtrl*)GetPage(UDCAddTool)->FindWindow(wxT("UDCFilepath")))->Clear();
    if (!dirty[UDCAddTool] && !dirty[UDCEditTool] && !dirty[UDCDeleteTool]) return;
    LMT->LoadArrays();  // Reload the arrays with data from config, in case of alterations
    InitPage((pagename)GetSelection()); dirty[UDCAddTool]=dirty[UDCEditTool]=dirty[UDCDeleteTool] = false; return;
  }

if (id == XRCID("Display_OK")) { ConfDisplay->OnOK(); return; }         // Display page stuff
if (id == XRCID("Display_CANCEL")) { ConfDisplay->InitialiseData();     // Reverts, then refills
                                     switch (GetSelection()) // Don't try to reinsert data for all 3 here: 1 or more may not yet have been initialised
                                        { case DisplayTrees:    ConfDisplay->InsertDataTrees(); return;
                                          case DisplayTreeFont: ConfDisplay->InsertDataFont(); return;
                                          case DisplayMisc:     ConfDisplay->InsertDataMisc(); return;
                                           default: return;
                                        }
                                   }
if (id == XRCID("DAMApply")) { ConfDevices->OnOK(); return; }           // Devices page stuff
if (id == XRCID("DAMCancel")) { ConfDevices->OnCancel(); }
if (id == XRCID("CADApply")) { ConfDevicesAdv->OnOK(); return; }        // AdvancesDevices page stuff
if (id == XRCID("CADCancel")) { ConfDevicesAdv->InitialiseData();
                                switch (GetSelection()) // Don't try to reinsert data for all 3 here: 1 or more may not yet have been initialised
                                  { case DevicesAdvancedA: ConfDevicesAdv->InsertDataPgA(); return;
                                    case DevicesAdvancedB: ConfDevicesAdv->InsertDataPgB(); return;
                                    case DevicesAdvancedC: ConfDevicesAdv->InsertDataPgC(); return;
                                     default: return;
                                  }
                              }
if (id == XRCID("BackgroundColoursBtn")) { ConfDisplay->OnSelectBackgroundColours(); return; }   // Calls a colour dialog
if (id == XRCID("SelectColoursBtn")) { ConfDisplay->OnSelectColours(); return; }   // Calls a different colour dialog, for stripes
if (id == XRCID("HighlightOffsetBtn")) { ConfDisplay->OnConfigurePaneHighlighting(); return; } // Calls pane-highlighting dialog
if (id == XRCID("TreeFontBtn")) { ConfDisplay->OnChangeFont(); return; } 
if (id == XRCID("TreeDefaultFontBtn")) { ConfDisplay->OnDefaultFont(); return; }
if (id == XRCID("TBIcons")) { ConfDisplay->OnConfigureTBIcons(); return; }
if (id == XRCID("Previews")) { ConfDisplay->OnConfigurePreviews(); return; }
if (id == XRCID("Tooltips")) { parent->ConfigureTooltips(); return; }
if (id == XRCID("Editors"))
  { DeviceManager* pDM = MyFrame::mainframe->Layout->m_notebook->DeviceMan->deviceman;
    if (pDM)   return pDM->ConfigureEditors();
  }

if (id == XRCID("Terminals_OK")) { ConfTerminals->OnOK(); return; }                // Terminal page stuff
if (id == XRCID("Terminals_CANCEL")) { ConfTerminals->OnCancel(); return; }
if (id == XRCID("TerminalAccept") || id == XRCID("TerminalCommandAccept") || id == XRCID("ToolsTerminalCommandAccept")) 
                                { ConfTerminals->OnAccept(id); return; }
if (id == XRCID("TerminalFontBtn")) { ConfTerminals->OnChangeFont(); return; } 
if (id == XRCID("TerminalDefaultFontBtn")) { ConfTerminals->OnDefaultFont(); return; }

if (id == XRCID("Net_OK")) { ConfNet->OnOK(); return; }                           // Network page stuff
if (id == XRCID("Net_CANCEL")) { ConfNet->InitialiseData(); ConfNet->InsertData(); return; }
if (id == XRCID("Net_Add")) { ConfNet->AddServer(); return; }
if (id == XRCID("Net_Delete")) { ConfNet->DeleteServer(); return; }

if (id == XRCID("Misc_OK")) { ConfMisc->OnOK(); return; }                         // Misc page stuff
if (id == XRCID("Misc_CANCEL")) { ConfMisc->InitialiseData();
                                  switch (GetSelection()) // Don't try to reinsert data for all 3 here: 1 or more may not yet have been initialised
                                    { case ConfigMiscUndo: ConfMisc->InsertDataUndo(); return;
                                      case ConfigMiscBriefTimes: ConfMisc->InsertDataBrieftimes(); return;
                                      case ConfigMiscSu: ConfMisc->InsertDataSu(); return;
                                       default: return;
                                    }
                                }
if (id == XRCID("ConfigureOpen")) { ConfMisc->OnConfigureOpen(); return; }
if (id == XRCID("DnD")) { ConfMisc->OnConfigureDnD(); HelpContext = HCconfigMisc; return; }
if (id == XRCID("Status")) { ConfMisc->OnConfigureStatusbar(); return; }
if (id == XRCID("Export")) { ConfMisc->OnExportBtn(this); return; }
}

void ConfigureNotebook::OnUDCLabelComboSelectionChanged(wxCommandEvent& event)
{
int sel = event.GetInt();     // I originally used GetSelection() here, but on a slower machine the selection didn't have time to update
if (UDCPage==UDCDelete || UDCPage==UDCEdit) 
  { enum pagename page = (UDCPage==UDCEdit) ? UDCEditTool : UDCDeleteTool;
    if (sel == -1 || sel >= (int)ToolArray->GetCount())
      { ((wxTextCtrl*)GetPage(page)->FindWindow(wxT("UDCFilepath")))->ChangeValue(wxT("")); return; }     // No data
     else ((wxTextCtrl*)(GetPage(page)->FindWindow(wxT("UDCFilepath"))))->ChangeValue(ToolArray->Item(sel)->Filepath);
  }

if (UDCPage==UDCEdit)                                                  // As Delete doesn't have a terminal checkbox
  { ((wxCheckBox*)GetPage(UDCEditTool)->FindWindow(wxT("UDCTerminal")))->SetValue(ToolArray->Item(sel)->InTerminal);  // Is this command run in a terminal?
    ((wxCheckBox*)GetPage(UDCEditTool)->FindWindow(wxT("UDCTerminalPersists")))->SetValue(ToolArray->Item(sel)->TerminalPersists);  // If so, does this persist or self-close?
    wxCheckBox* UDCAsroot = (wxCheckBox*)(GetPage(UDCEditTool)->FindWindow(wxT("UDCAsRoot"))); // New fields so check they exist
    if (UDCAsroot) UDCAsroot->SetValue(ToolArray->Item(sel)->AsRoot);
    wxCheckBox* UDCRefreshPane = ((wxCheckBox*)GetPage(UDCEditTool)->FindWindow(wxT("UDCRefreshPane")));
    if (UDCRefreshPane)
      { UDCRefreshPane->SetValue(ToolArray->Item(sel)->RefreshPane);                                                                  // Refresh pane after?
        ((wxCheckBox*)GetPage(UDCEditTool)->FindWindow(wxT("UDCRefreshOpposite")))->SetValue(ToolArray->Item(sel)->RefreshOpposite);  // Refresh opposite after?
      }
    m_sel = sel;                                                       // Store the selection: if the label gets edited, we'll not be able to find it again
  }
}

void ConfigureNotebook::OnUpdateUI(wxUpdateUIEvent& WXUNUSED(event))
{
bool enable;

switch(GetSelection())
  { case UDCAddTool:   {wxWindow* addpage = GetPage(UDCAddTool);
                        wxCheckBox* UDCTerminal = ((wxCheckBox*)addpage->FindWindow(wxT("UDCTerminal")));
                        wxCheckBox* UDCAsRoot = ((wxCheckBox*)addpage->FindWindow(wxT("UDCAsRoot")));
                        if (UDCAsRoot)
                          { UDCTerminal->Enable(!UDCAsRoot->IsChecked());
                            UDCAsRoot->Enable(!UDCTerminal->IsChecked());
                          }
                        addpage->FindWindow(wxT("UDCTerminalPersists"))->Enable(UDCTerminal->IsChecked());  // Can't persist if no terminal!
                        wxCheckBox* UDCRefreshPane = ((wxCheckBox*)GetPage(UDCAddTool)->FindWindow(wxT("UDCRefreshPane")));
                        if (UDCRefreshPane) // New fields so check they exist
                          { UDCRefreshPane->Enable((UDCAsRoot && UDCAsRoot->IsChecked()) || !UDCTerminal->IsChecked());
                            addpage->FindWindow(wxT("UDCRefreshOpposite"))->Enable(UDCRefreshPane->IsEnabled() && UDCRefreshPane->IsChecked()); // If not this, not that
                          }
                        bool enableUDCAddBtn = !(((wxTextCtrl*)addpage->FindWindow(wxT("UDCFilepath")))->GetValue().IsEmpty()
                                                || ((wxTextCtrl*)addpage->FindWindow(wxT("UDCLabel")))->GetValue().IsEmpty());
                        addpage->FindWindow(wxT("UDCAddBtn"))->Enable(enableUDCAddBtn);
                        enable = dirty[UDCAddTool];
                        addpage->FindWindow(wxT("UDCApply"))->Enable(enable); addpage->FindWindow(wxT("UDCCancel"))->Enable(enable);
                        SetPageStatus(enable); 
                        return;}
                      
     case UDCEditTool:  { int sel = m_sel; if (sel == wxNOT_FOUND || sel >= (int)ToolArray->GetCount()) return;
                          wxWindow* editpage = GetPage(UDCEditTool);
                          wxCheckBox* UDCTerminal = ((wxCheckBox*)editpage->FindWindow(wxT("UDCTerminal")));
                          wxCheckBox* UDCAsRoot = ((wxCheckBox*)editpage->FindWindow(wxT("UDCAsRoot")));
                          wxCheckBox* UDCRefreshPane = ((wxCheckBox*)editpage->FindWindow(wxT("UDCRefreshPane")));
                          if (((wxComboBox*)editpage->FindWindow(wxT("UDCLabelCombo")))->GetValue() != ToolArray->Item(sel)->Label)  // check the Label hasn't been altered
                                dirty[UDCEditTool] = true;
                          if (!editing)                        
                            { editpage->FindWindow(wxT("UDCFilepath"))->Disable();  // We don't want alterations when we're not editing
                              UDCTerminal->Disable();
                              editpage->FindWindow(wxT("UDCTerminalPersists"))->Disable();
                              UDCAsRoot && UDCAsRoot->Disable();                    // New fields so check they exist
                              if (UDCRefreshPane)
                                { UDCRefreshPane->Disable();
                                  editpage->FindWindow(wxT("UDCRefreshOpposite"))->Disable();
                                }
                            }
                           else
                            { editpage->FindWindow(wxT("UDCTerminalPersists"))->Enable(UDCTerminal->IsChecked());
                              if (UDCAsRoot)
                                { UDCTerminal->Enable(!UDCAsRoot->IsChecked());
                                  UDCAsRoot->Enable(!UDCTerminal->IsChecked());
                                }
                              if (UDCRefreshPane)
                                { UDCRefreshPane->Enable((UDCAsRoot && UDCAsRoot->IsChecked()) || !UDCTerminal->IsChecked());
                                  editpage->FindWindow(wxT("UDCRefreshOpposite"))->Enable(UDCRefreshPane->IsEnabled() && UDCRefreshPane->IsChecked());
                                }
                            }

                          enable = dirty[UDCEditTool] && !editing; // We don't want Apply pressed while we're in the middle of editing a command

                          editpage->FindWindow(wxT("UDCApply"))->Enable(enable); editpage->FindWindow(wxT("UDCCancel"))->Enable(enable);
                          SetPageStatus(enable); 
                          return;}

    case UDCDeleteTool: enable = dirty[UDCDeleteTool];
                        GetPage(UDCDeleteTool)->FindWindow(wxT("UDCApply"))->Enable(enable); GetPage(UDCDeleteTool)->FindWindow(wxT("UDCCancel"))->Enable(enable);
                        SetPageStatus(enable); 
                        return;

    case UDShortcuts: enable = MyFrame::mainframe->AccelList->dirty;
                      GetPage(UDShortcuts)->FindWindow(wxT("UDSApply"))->Enable(enable);
                      GetPage(UDShortcuts)->FindWindow(wxT("UDSCancel"))->Enable(enable);
                      SetPageStatus(enable); return;

    case DisplayTrees:case DisplayTreeFont: case DisplayMisc:   ConfDisplay->OnUpdateUI(); return;

    case DevicesAutoMt: case DevicesMount: case DevicesUsb: case DevicesRemovable: case DevicesFixed:   ConfDevices->OnUpdateUI(); return;
    case DevicesAdvancedA: case DevicesAdvancedB: case DevicesAdvancedC:   ConfDevicesAdv->OnUpdateUI(); return;

    case ConfigTerminalsReal: case ConfigTerminalsEm: ConfTerminals->OnUpdateUI(); return;
    
    case ConfigNet:      ConfNet->OnUpdateUI(); return;
    
    case ConfigMiscUndo: case ConfigMiscBriefTimes: case ConfigMiscSu: case ConfigMiscOther:  ConfMisc->OnUpdateUI(); return;

     default:           return;
  }
}

void ConfigureNotebook::FillUDCMenusCombo(size_t &item, size_t indent)  // Recursively fill the combo, indenting according to sub-ness of the menu
{
if (item >= FolderArray->GetCount())   return;

wxString label = FolderArray->Item(item)->Label;
label.Replace(wxT("&"), wxT(""));                                       // We don't want to see mnemonics here
wxString folder(wxT(' '), indent * 5); folder += label;                 // Prepend an appropriate indent for this (sub)folder
UDCCombo->Append(folder);

size_t Subs = FolderArray->Item(item)->Subs;
for (size_t c=0;  c < Subs; ++c)  FillUDCMenusCombo(++item, indent+1);  // Do children by recursion, indenting by an extra 1
}

bool ConfigureNotebook::Dirty()    // Check there's no unsaved data
{
static const wxString PageNames[] = 
  { _("Configure Shortcuts"), _("User-defined tools"), _("User-defined tools"), _("User-defined tools"), _("User-defined tools"),
    _("Devices"),_("Devices Automounting"), _("Devices Mount"), _("Devices Usb"), _("Removable Devices"), _("Fixed Devices"),
    _("Advanced Devices"),  _("Advanced Devices Fixed"), _("Advanced Devices Removable"), _("Advanced Devices LVM"),
    _("Display"), _("Display Trees"), _("Display Tree Font"), _("Display Misc"),
    _("Terminals"),_("Real Terminals"),_("Terminal Emulator"),  _("Networks"),
    _("Misc"), _("Misc Undo"), _("Misc Times"), _("Misc Superuser"), _("Misc Other") 
  };

dirty[UDShortcuts] = MyFrame::mainframe->AccelList->dirty;              // AccelList holds this info, as it's shared with the standalone version

dirty[ConfigDisplay] = false;                                           // ConfigureDisplay etc hold their own dirt
  dirty[DisplayTrees] = ConfDisplay->dirty[0]; dirty[DisplayTreeFont] = ConfDisplay->dirty[1]; dirty[DisplayMisc] = ConfDisplay->dirty[2];

dirty[ConfigDevices] = false; dirty[DevicesAdvanced] = false;
  dirty[DevicesAutoMt] = ConfDevices->dirty[0]; dirty[DevicesMount] = ConfDevices->dirty[1]; dirty[DevicesUsb] = ConfDevices->dirty[2]; dirty[DevicesRemovable] = ConfDevices->dirty[3]; dirty[DevicesFixed] = ConfDevices->dirty[4];
  dirty[DevicesAdvancedA] = ConfDevicesAdv->dirty[0]; dirty[DevicesAdvancedB] = ConfDevicesAdv->dirty[1]; dirty[DevicesAdvancedC] = ConfDevicesAdv->dirty[2];

dirty[ConfigTerminals] = false; dirty[ConfigTerminalsReal] = ConfTerminals->dirty[0]; dirty[ConfigTerminalsEm] = ConfTerminals->dirty[1];
dirty[ConfigNet] = ConfNet->dirty;
dirty[ConfigMisc] = false;
dirty[ConfigMiscUndo] = ConfMisc->dirty[0]; dirty[ConfigMiscBriefTimes] = ConfMisc->dirty[1];
dirty[ConfigMiscSu] = ConfMisc->dirty[2]; dirty[ConfigMiscOther] = ConfMisc->dirty[3];

for (int n=0; n < NotSelected; ++n)
  if (dirty[n])                                                         // If this page hasn't been saved, check
    { wxString msg;
      msg.Printf(_("There are unsaved changes on the \"%s\" page.\n                           Really close?"), PageNames[n].c_str());
      wxMessageDialog dialog(this, msg, _("Are you sure?"), wxYES_NO | wxICON_QUESTION);
      if (dialog.ShowModal() != wxID_YES)    
        { SetSelection(n); return true; }                               // If requested, set the appropriate page & signal to redo the config dialog
    }
    
return false;    
}

void ConfigureNotebook::OnPageChanged(wxTreebookEvent& event)
{
enum pagename page = (pagename)event.GetSelection();
if (currentpage == page) return;                                              // ISQ
switch(page)
  { case UDCommand:           currentpage = page; HelpContext = HCconfigUDC; break;
    case UDCAddTool:
    case UDCEditTool:
    case UDCDeleteTool:       currentpage = page; InitPage(page); HelpContext = HCconfigUDC; break;

    case UDShortcuts:         currentpage = page; HelpContext = HCconfigShortcuts; break;

    case ConfigDevices: case DevicesAdvanced: currentpage = page; HelpContext = HCconfigDevices;  break;
    case DevicesAutoMt:       currentpage = page; HelpContext = HCconfigDevices; ConfDevices->InitPgAM(GetPage(page)); break;
    case DevicesMount:        currentpage = page; HelpContext = HCconfigDevices; ConfDevices->InitPgMt(GetPage(page)); break;
    case DevicesUsb:          currentpage = page; HelpContext = HCconfigDevices; ConfDevices->InitPgUsb(GetPage(page)); break;
    case DevicesRemovable:    currentpage = page; HelpContext = HCconfigDevices; ConfDevices->InitPgRem(GetPage(page)); break;
    case DevicesFixed:        currentpage = page; HelpContext = HCconfigDevices; ConfDevices->InitPgFix(GetPage(page)); break;
    case DevicesAdvancedA:    currentpage = page; HelpContext = HCconfigDevices; ConfDevicesAdv->InitPgA(GetPage(page)); break;
    case DevicesAdvancedB:    currentpage = page; HelpContext = HCconfigDevices; ConfDevicesAdv->InitPgB(GetPage(page)); break;
    case DevicesAdvancedC:    currentpage = page; HelpContext = HCconfigDevices; ConfDevicesAdv->InitPgC(GetPage(page)); break;

    case ConfigDisplay:       currentpage = page; HelpContext = HCconfigDisplay; break;
    case DisplayTrees:        currentpage = page; HelpContext = HCconfigDisplay; ConfDisplay->InitTrees(GetPage(page)); break;
    case DisplayTreeFont:     currentpage = page; HelpContext = HCconfigDisplay; ConfDisplay->InitFont(GetPage(page)); break;
    case DisplayMisc:         currentpage = page; HelpContext = HCconfigDisplay; ConfDisplay->InitMisc(GetPage(page)); break;

    case ConfigTerminalsReal: currentpage = page; HelpContext = HCconfigterminal; ConfTerminals->Init(GetPage(page), true); break;
    case ConfigTerminalsEm:   currentpage = page; HelpContext = HCconfigterminal; ConfTerminals->Init(GetPage(page), GetPage(page-1)); break;

    case ConfigNet:           currentpage = page; HelpContext = HCconfigNetworks; ConfNet->Init(GetPage(page)); break;

    case ConfigMisc:          currentpage = page; HelpContext = HCconfigMisc; break;
    case ConfigMiscUndo:      currentpage = page; HelpContext = HCconfigMisc; ConfMisc->InitUndo(GetPage(page)); break;
    case ConfigMiscBriefTimes:currentpage = page; HelpContext = HCconfigMisc; ConfMisc->InitBriefTimes(GetPage(page)); break;
    case ConfigMiscSu:        currentpage = page; HelpContext = HCconfigMisc; ConfMisc->InitSu(GetPage(page)); break;
    case ConfigMiscOther:     currentpage = page; HelpContext = HCconfigMisc; ConfMisc->InitOther(GetPage(page)); break;

     default:             currentpage = NotSelected; HelpContext = HCunknown; break;
  }
}

void ConfigureNotebook::StoreApplyBtn(wxWindow* btn)
{
wxCHECK_RET(btn, wxT("NULL button pointer passed"));
 // ConfigureNotebook::InitPage may be called multiple times, so check for uniqueness
std::vector<wxWindow*>::iterator it = std::find(m_ApplyButtons.begin(), m_ApplyButtons.end(), btn);
if (it == m_ApplyButtons.end())
  { m_ApplyButtons.push_back(btn);
    btn->Disable(); // We need to ensure each button is disabled by default, otherwise it breaks the Finished button's updateUI
  }
 else
  // If this is a repeat visit we don't add it; but update the window's enabled status as in some code-paths that won't happen otherwise
  (*it)->Enable(btn->IsEnabled());
}

void ConfigureNotebook::SetPageStatus(bool dirty)
{
int sel = GetSelection();
wxCHECK_RET(sel != wxNOT_FOUND, wxT("Invalid current notebook page"));

int current = GetPageImage(sel);
switch(current)
 { case 0: if (dirty)
            { SetPageImage(sel, 2); }
           break; 
   case 1: if (dirty)
            { SetPageImage(sel, 3); }
           break; 
   case 2: if (!dirty)
            { SetPageImage(sel, 0); }
           break; 
   case 3: if (!dirty)
            { SetPageImage(sel, 1); }
           break;
 }

}

void ConfigureNotebook::OnFinishedBtnUpdateUI(wxUpdateUIEvent& event)
{
for (size_t n=0; n < m_ApplyButtons.size(); ++n)
  { wxWindow* btn = m_ApplyButtons.at(n);
    if (btn && btn->IsEnabled())
      { event.Enable(false); return; }
  }

event.Enable(true);
}

BEGIN_EVENT_TABLE(ConfigureNotebook, wxTreebook)
  EVT_TREEBOOK_PAGE_CHANGED(-1, ConfigureNotebook::OnPageChanged)
END_EVENT_TABLE()
//-------------------------------------------------------------------------------------------------------------------------------------------

void ConfigureDisplay::InitialiseData()  // Fill the temp vars with the old data. Used initially and also to revert on Cancel
{
tempTREE_INDENT = TREE_INDENT;
tempSHOW_TREE_LINES = SHOW_TREE_LINES;
tempTREE_SPACING = TREE_SPACING;
tempSHOWHIDDEN = SHOWHIDDEN;
tempUSE_LC_COLLATE = USE_LC_COLLATE;
tempTREAT_SYMLINKTODIR_AS_DIR = TREAT_SYMLINKTODIR_AS_DIR;
tempCOLOUR_PANE_BACKGROUND = COLOUR_PANE_BACKGROUND;
tempSINGLE_BACKGROUND_COLOUR = SINGLE_BACKGROUND_COLOUR;
tempBACKGROUND_COLOUR_DV = BACKGROUND_COLOUR_DV;
tempBACKGROUND_COLOUR_FV = BACKGROUND_COLOUR_FV;
tempUSE_STRIPES = USE_STRIPES;
tempSTRIPE_0 = STRIPE_0;
tempSTRIPE_1 = STRIPE_1;
tempHIGHLIGHT_PANES = HIGHLIGHT_PANES;
tempDIRVIEW_HIGHLIGHT_OFFSET = DIRVIEW_HIGHLIGHT_OFFSET;
tempFILEVIEW_HIGHLIGHT_OFFSET = FILEVIEW_HIGHLIGHT_OFFSET;
tempSHOW_DIR_IN_TITLEBAR = SHOW_DIR_IN_TITLEBAR;
tempSHOW_TB_TEXTCTRL = SHOW_TB_TEXTCTRL;
tempSHOW_DIR_IN_TITLEBAR = SHOW_DIR_IN_TITLEBAR;
tempASK_ON_TRASH = ASK_ON_TRASH;
tempASK_ON_DELETE = ASK_ON_DELETE;
tempUSE_STOCK_ICONS = USE_STOCK_ICONS;
tempTRASHCAN = TRASHCAN;
tempTREE_FONT = TREE_FONT;
tempCHOSEN_TREE_FONT = CHOSEN_TREE_FONT;
tempUSE_DEFAULT_TREE_FONT = USE_DEFAULT_TREE_FONT;
tempUSE_FSWATCHER = USE_FSWATCHER;
}

void ConfigureDisplay::InsertDataTrees()
{
((wxCheckBox*)pageTree->FindWindow(wxT("TREE_LINES")))->SetValue(tempSHOW_TREE_LINES);
((wxSpinCtrl*)pageTree->FindWindow(wxT("TREE_INDENT")))->SetValue(tempTREE_INDENT);
((wxSpinCtrl*)pageTree->FindWindow(wxT("TREE_SPACING")))->SetValue(tempTREE_SPACING);
((wxCheckBox*)pageTree->FindWindow(wxT("SHOWHIDDEN")))->SetValue(tempSHOWHIDDEN);
((wxCheckBox*)pageTree->FindWindow(wxT("USE_LC_COLLATE")))->SetValue(tempUSE_LC_COLLATE);
((wxCheckBox*)pageTree->FindWindow(wxT("TREAT_SYMLINKTODIR_AS_DIR")))->SetValue(tempTREAT_SYMLINKTODIR_AS_DIR);
wxCheckBox* colbackgrd = (wxCheckBox*)pageTree->FindWindow(wxT("COLOUR_PANE_BACKGROUND"));
if (colbackgrd) colbackgrd->SetValue(tempCOLOUR_PANE_BACKGROUND);
((wxCheckBox*)pageTree->FindWindow(wxT("USE_STRIPES")))->SetValue(tempUSE_STRIPES);
  // Recent additions, so check they exist in the user's xrc file
wxCheckBox* hp = (wxCheckBox*)pageTree->FindWindow(wxT("HIGHLIGHT_PANES"));
if (hp) hp->SetValue(tempHIGHLIGHT_PANES);
wxCheckBox* fsw = (wxCheckBox*)pageTree->FindWindow(wxT("UseFSWatcherCheck"));
if (fsw) fsw->SetValue(tempUSE_FSWATCHER);
m_nb->StoreApplyBtn(pageTree->FindWindow(wxT("Display_OK")));
}

void ConfigureDisplay::InsertDataFont()
{
fonttext = (wxTextCtrl*)pageFont->FindWindow(wxT("CurrentFontText"));
fonttext->SetValue(_("Selected tree Font")); fonttext->SetFont(tempCHOSEN_TREE_FONT);
defaultfonttext = (wxTextCtrl*)pageFont->FindWindow(wxT("DefaultFontText"));
defaultfonttext->SetValue(_("Default tree Font"));
m_nb->StoreApplyBtn(pageFont->FindWindow(wxT("Display_OK")));
}

void ConfigureDisplay::InsertDataMisc()
{
wxCheckBox* DirInTitle = (wxCheckBox*)(pageMisc->FindWindow(wxT("SHOW_DIR_IN_TITLEBAR")));
if (DirInTitle) // It's new in 1.1 to we'd better check
  DirInTitle->SetValue(tempSHOW_DIR_IN_TITLEBAR);
((wxCheckBox*)pageMisc->FindWindow(wxT("SHOW_TB_TEXTCTRL")))->SetValue(tempSHOW_TB_TEXTCTRL);
((wxCheckBox*)pageMisc->FindWindow(wxT("ASK_ON_TRASH")))->SetValue(tempASK_ON_TRASH);
((wxCheckBox*)pageMisc->FindWindow(wxT("ASK_ON_DELETE")))->SetValue(tempASK_ON_DELETE);
((wxCheckBox*)pageMisc->FindWindow(wxT("USE_STOCK_ICONS")))->SetValue(tempUSE_STOCK_ICONS);
((wxTextCtrl*)pageMisc->FindWindow(wxT("TRASHCAN")))->SetValue(tempTRASHCAN);
m_nb->StoreApplyBtn(pageMisc->FindWindow(wxT("Display_OK")));
}

void ConfigureDisplay::OnSelectBackgroundColours()
{
BackgroundColoursDlg dlg;
wxXmlResource::Get()->LoadDialog(&dlg, pageTree, wxT("BackgroundColoursDlg"));
dlg.Init(tempBACKGROUND_COLOUR_DV, tempBACKGROUND_COLOUR_FV, tempSINGLE_BACKGROUND_COLOUR);
if (dlg.ShowModal() == wxID_OK) 
  { tempBACKGROUND_COLOUR_DV = dlg.Col0; tempBACKGROUND_COLOUR_FV = dlg.Col1; 
    if (dlg.IdenticalColsChk) tempSINGLE_BACKGROUND_COLOUR = dlg.IdenticalColsChk->IsChecked(); 
  }
}

void ConfigureDisplay::OnSelectColours()
{
ChooseStripesDlg dlg;
wxXmlResource::Get()->LoadDialog(&dlg, pageTree, wxT("ChooseStripesDlg"));

  // If tempSTRIPE_1 isn't set, but tempBACKGROUND_COLOUR_FV is, use that as the default colour here
wxColour strip1 = tempSTRIPE_1;
if (strip1 == wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW))
  strip1 = tempBACKGROUND_COLOUR_FV;

dlg.Init(tempSTRIPE_0, strip1);
if (dlg.ShowModal() == wxID_OK) 
  { tempSTRIPE_0 = dlg.Col0; tempSTRIPE_1 = dlg.Col1; }
}

void ConfigureDisplay::OnConfigurePaneHighlighting()
{
PaneHighlightDlg dlg;
wxXmlResource::Get()->LoadDialog(&dlg, pageTree, wxT("PaneHighlightDlg"));
dlg.Init(tempDIRVIEW_HIGHLIGHT_OFFSET, tempFILEVIEW_HIGHLIGHT_OFFSET);
if (dlg.ShowModal() == wxID_OK) 
  { tempDIRVIEW_HIGHLIGHT_OFFSET = dlg.GetDirSpinValue(); tempFILEVIEW_HIGHLIGHT_OFFSET = dlg.GetFileSpinValue(); }
}

void ConfigureDisplay::OnConfigureTBIcons()  // Add/Edit the 'bookmark' icons in the dirview toolbar
{
ArrayofDirCtrlTBStructs* DirCtrlTBinfoarray = MyFrame::mainframe->configure->DirCtrlTBinfoarray;
ToolbarIconsDialog dlg;
wxXmlResource::Get()->LoadDialog(&dlg,MyFrame::mainframe, wxT("ToolbarIconsDialog"));
dlg.Init(DirCtrlTBinfoarray);
dlg.InitDialog();
dlg.ShowModal();

dlg.Save();                                               // Save data, whether on not altered. It's really not worth checking

for (int n = (int)DirCtrlTBinfoarray->GetCount();  n > 0; --n)  // Now delete the old data, reload
  { DirCtrlTBStruct* item = DirCtrlTBinfoarray->Item(n-1); delete item; DirCtrlTBinfoarray->RemoveAt(n-1); }
ToolbarIconsDialog::LoadDirCtrlTBinfo(DirCtrlTBinfoarray);
MyFrame::mainframe->RefreshAllTabs(NULL, true);        //  and redisplay
}

void ConfigureDisplay::OnConfigurePreviews()
{
wxDialog dlg;
if (!wxXmlResource::Get()->LoadDialog(&dlg,MyFrame::mainframe, wxT("DisplayPreviewsDlg"))) return;

wxSpinCtrl* Iwt = XRCCTRL(dlg, "ImageWt", wxSpinCtrl); wxCHECK_RET(Iwt, wxT("Couldn't load spinctrl"));
wxSpinCtrl* Iht = XRCCTRL(dlg, "ImageHt", wxSpinCtrl); wxCHECK_RET(Iht, wxT("Couldn't load spinctrl"));
wxSpinCtrl* Twt = XRCCTRL(dlg, "TextWt", wxSpinCtrl); wxCHECK_RET(Twt, wxT("Couldn't load spinctrl"));
wxSpinCtrl* Tht = XRCCTRL(dlg, "TextHt", wxSpinCtrl); wxCHECK_RET(Tht, wxT("Couldn't load spinctrl"));
wxSpinCtrl* Dwell = XRCCTRL(dlg, "Dwell", wxSpinCtrl); wxCHECK_RET(Dwell, wxT("Couldn't load spinctrl"));
Iwt->SetValue(PreviewManager::MAX_PREVIEW_IMAGE_WT);
Iht->SetValue(PreviewManager::MAX_PREVIEW_IMAGE_HT);
Twt->SetValue(PreviewManager::MAX_PREVIEW_TEXT_WT);
Tht->SetValue(PreviewManager::MAX_PREVIEW_TEXT_HT);
Dwell->SetValue(PreviewManager::GetDwellTime()/100);

if (dlg.ShowModal() != wxID_OK) return;

PreviewManager::Save(Iwt->GetValue(), Iht->GetValue(), Twt->GetValue(), Tht->GetValue(), Dwell->GetValue()*100);
}

void ConfigureDisplay::OnUpdateUI()
{
if (subpage==CD_trees)
  { // I'm null-testing the first control, as there were occasional segs when the notebook page changed; probably a race condition somehow
    if (pageTree == NULL) return;
    wxSpinCtrl* spin = (wxSpinCtrl*)pageTree->FindWindow(wxT("TREE_INDENT")); if (spin==NULL) return;
    tempTREE_INDENT = spin->GetValue();
    tempSHOW_TREE_LINES = ((wxCheckBox*)pageTree->FindWindow(wxT("TREE_LINES")))->IsChecked();
    tempTREE_SPACING = ((wxSpinCtrl*)pageTree->FindWindow(wxT("TREE_SPACING")))->GetValue();
    tempSHOWHIDDEN = ((wxCheckBox*)pageTree->FindWindow(wxT("SHOWHIDDEN")))->IsChecked();
    tempUSE_LC_COLLATE = ((wxCheckBox*)pageTree->FindWindow(wxT("USE_LC_COLLATE")))->IsChecked();
    tempTREAT_SYMLINKTODIR_AS_DIR = ((wxCheckBox*)pageTree->FindWindow(wxT("TREAT_SYMLINKTODIR_AS_DIR")))->IsChecked();
    wxCheckBox* cb = (wxCheckBox*)pageTree->FindWindow(wxT("COLOUR_PANE_BACKGROUND")); // Recent additions, so check they exist in the user's xrc file
    if (cb) 
      { tempCOLOUR_PANE_BACKGROUND = cb->IsChecked();
        pageTree->FindWindow(wxT("BackgroundColoursBtn"))->Enable(tempCOLOUR_PANE_BACKGROUND);
      }
    tempUSE_STRIPES = ((wxCheckBox*)pageTree->FindWindow(wxT("USE_STRIPES")))->IsChecked();
    pageTree->FindWindow(wxT("SelectColoursBtn"))->Enable(tempUSE_STRIPES);
    wxCheckBox* hp = (wxCheckBox*)pageTree->FindWindow(wxT("HIGHLIGHT_PANES")); // Recent additions, so check they exist in the user's xrc file
    if (hp) 
      { tempHIGHLIGHT_PANES = hp->IsChecked();
        pageTree->FindWindow(wxT("HighlightOffsetBtn"))->Enable(tempHIGHLIGHT_PANES);
      }
    wxCheckBox* fsw = (wxCheckBox*)pageTree->FindWindow(wxT("UseFSWatcherCheck"));
    #if defined(__WXX11__) || !defined(__LINUX__)
    if (fsw) fsw->Disable(); // as wxX11 cant cope, and nor can gnu/hurd atm
    #endif
    if (fsw) tempUSE_FSWATCHER = (fsw->IsEnabled() && fsw->IsChecked());

   dirty[subpage] = (SHOW_TREE_LINES!=tempSHOW_TREE_LINES) || (TREE_INDENT!=tempTREE_INDENT) || (tempTREE_SPACING!=TREE_SPACING)
          || (tempSHOWHIDDEN!=SHOWHIDDEN) || (tempUSE_LC_COLLATE!=USE_LC_COLLATE) || (tempTREAT_SYMLINKTODIR_AS_DIR!=TREAT_SYMLINKTODIR_AS_DIR)
          ||(tempCOLOUR_PANE_BACKGROUND!=COLOUR_PANE_BACKGROUND) ||(tempSINGLE_BACKGROUND_COLOUR!=SINGLE_BACKGROUND_COLOUR) 
                            || (tempBACKGROUND_COLOUR_DV!=BACKGROUND_COLOUR_DV) || (tempBACKGROUND_COLOUR_FV!=BACKGROUND_COLOUR_FV)
          ||(tempUSE_STRIPES!=USE_STRIPES) || (tempSTRIPE_0!=STRIPE_0) || (tempSTRIPE_1!=STRIPE_1)
          ||(tempHIGHLIGHT_PANES!=HIGHLIGHT_PANES) || (tempDIRVIEW_HIGHLIGHT_OFFSET!=DIRVIEW_HIGHLIGHT_OFFSET) || (tempFILEVIEW_HIGHLIGHT_OFFSET!=FILEVIEW_HIGHLIGHT_OFFSET)
          ||(tempUSE_FSWATCHER!=USE_FSWATCHER);

    pageTree->FindWindow(wxT("Display_OK"))->Enable(dirty[subpage]);
    pageTree->FindWindow(wxT("Display_CANCEL"))->Enable(dirty[subpage]);
  }

if (subpage==CD_font)
  { if (pageFont == NULL) return;

    dirty[subpage] = fontchanged;

    fonttext->Enable(!tempUSE_DEFAULT_TREE_FONT);
    defaultfonttext->Enable(tempUSE_DEFAULT_TREE_FONT); pageFont->FindWindow(wxT("TreeDefaultFontBtn"))->Enable(!tempUSE_DEFAULT_TREE_FONT);

    pageFont->FindWindow(wxT("Display_OK"))->Enable(dirty[subpage]);
    pageFont->FindWindow(wxT("Display_CANCEL"))->Enable(dirty[subpage]);
  }
if (subpage==CD_misc)
  { if (pageMisc == NULL) return;

    wxCheckBox* DirInTitle = (wxCheckBox*)(pageMisc->FindWindow(wxT("SHOW_DIR_IN_TITLEBAR")));
    if (DirInTitle) // It's new in 1.1 to we'd better check
      tempSHOW_DIR_IN_TITLEBAR = DirInTitle->IsChecked();
    tempSHOW_TB_TEXTCTRL = ((wxCheckBox*)pageMisc->FindWindow(wxT("SHOW_TB_TEXTCTRL")))->IsChecked();
    tempASK_ON_TRASH = ((wxCheckBox*)pageMisc->FindWindow(wxT("ASK_ON_TRASH")))->IsChecked();
    tempASK_ON_DELETE = ((wxCheckBox*)pageMisc->FindWindow(wxT("ASK_ON_DELETE")))->IsChecked();
    tempUSE_STOCK_ICONS = ((wxCheckBox*)pageMisc->FindWindow(wxT("USE_STOCK_ICONS")))->IsChecked();
    tempTRASHCAN = ((wxTextCtrl*)pageMisc->FindWindow(wxT("TRASHCAN")))->GetValue();

    dirty[subpage] = fontchanged || (tempSHOW_DIR_IN_TITLEBAR!=SHOW_DIR_IN_TITLEBAR) || (tempSHOW_TB_TEXTCTRL!=SHOW_TB_TEXTCTRL)
                     || (tempASK_ON_TRASH!=ASK_ON_TRASH) || (tempASK_ON_DELETE!=ASK_ON_DELETE) || (tempUSE_STOCK_ICONS!=USE_STOCK_ICONS) || (tempTRASHCAN!=TRASHCAN);

    pageMisc->FindWindow(wxT("Display_OK"))->Enable(dirty[subpage]);
    pageMisc->FindWindow(wxT("Display_CANCEL"))->Enable(dirty[subpage]);
  }

m_nb->SetPageStatus(dirty[subpage]);
}

void ConfigureDisplay::OnChangeFont()
{
wxFontData data;
data.SetInitialFont(tempCHOSEN_TREE_FONT);
    
wxFontDialog dialog(NULL, data);
if (dialog.ShowModal() == wxID_OK)
  { wxFontData retData = dialog.GetFontData();
    tempTREE_FONT = tempCHOSEN_TREE_FONT = retData.GetChosenFont();
    fonttext->SetFont(tempCHOSEN_TREE_FONT);    
    fontchanged = true; tempUSE_DEFAULT_TREE_FONT = false;
  }
}

void ConfigureDisplay::OnDefaultFont()
{
tempTREE_FONT = defaultfonttext->GetFont();
tempUSE_DEFAULT_TREE_FONT = true; fontchanged = (tempUSE_DEFAULT_TREE_FONT!=USE_DEFAULT_TREE_FONT);
}

void ConfigureDisplay::OnCancel()
{
InitialiseData(); 

switch(subpage)
  { case CD_trees: InsertDataTrees(); return;
    case CD_font:  InsertDataFont();  return;
    case CD_misc:  InsertDataMisc();  return;
  }
fontchanged = false;
}

void ConfigureDisplay::OnOK()
{
bool panehighlightingchanged = (tempDIRVIEW_HIGHLIGHT_OFFSET!=DIRVIEW_HIGHLIGHT_OFFSET) || (tempFILEVIEW_HIGHLIGHT_OFFSET!=FILEVIEW_HIGHLIGHT_OFFSET);
bool iconschanged = (USE_STOCK_ICONS != tempUSE_STOCK_ICONS);

SHOW_TREE_LINES = tempSHOW_TREE_LINES;
TREE_INDENT = tempTREE_INDENT;
TREE_SPACING = tempTREE_SPACING;
SHOWHIDDEN = tempSHOWHIDDEN;
TREAT_SYMLINKTODIR_AS_DIR = tempTREAT_SYMLINKTODIR_AS_DIR;
COLOUR_PANE_BACKGROUND = tempCOLOUR_PANE_BACKGROUND;
SINGLE_BACKGROUND_COLOUR = tempSINGLE_BACKGROUND_COLOUR;
BACKGROUND_COLOUR_DV = tempBACKGROUND_COLOUR_DV;
BACKGROUND_COLOUR_FV = tempBACKGROUND_COLOUR_FV;
USE_STRIPES = tempUSE_STRIPES;
STRIPE_0 = tempSTRIPE_0; STRIPE_1 = tempSTRIPE_1;
HIGHLIGHT_PANES = tempHIGHLIGHT_PANES;
DIRVIEW_HIGHLIGHT_OFFSET = tempDIRVIEW_HIGHLIGHT_OFFSET; FILEVIEW_HIGHLIGHT_OFFSET = tempFILEVIEW_HIGHLIGHT_OFFSET;
USE_FSWATCHER = tempUSE_FSWATCHER;

bool cleardir = (SHOW_DIR_IN_TITLEBAR && !tempSHOW_DIR_IN_TITLEBAR);
SHOW_DIR_IN_TITLEBAR = tempSHOW_DIR_IN_TITLEBAR;
if (cleardir)  // If displaying a titlebar dir has just been switched off, clear the currently-displayed one, or it'll linger
  MyFrame::mainframe->SetTitle(MyFrame::mainframe->GetTitle().BeforeFirst(wxT('[')));
  
SHOW_TB_TEXTCTRL = tempSHOW_TB_TEXTCTRL;
ASK_ON_TRASH = tempASK_ON_TRASH;
ASK_ON_DELETE = tempASK_ON_DELETE;
USE_STOCK_ICONS = tempUSE_STOCK_ICONS;
TRASHCAN = tempTRASHCAN;

if (USE_LC_COLLATE != tempUSE_LC_COLLATE)
  { SetSortMethod(tempUSE_LC_COLLATE);
    SetDirpaneSortMethod(tempUSE_LC_COLLATE);
    USE_LC_COLLATE = tempUSE_LC_COLLATE;
  }

if (fontchanged)
  { TREE_FONT = tempTREE_FONT; CHOSEN_TREE_FONT = tempCHOSEN_TREE_FONT;
    USE_DEFAULT_TREE_FONT = tempUSE_DEFAULT_TREE_FONT;
    fontchanged = false;
    MyFrame::mainframe->RefreshAllTabs(&TREE_FONT);
  }
 else
  MyFrame::mainframe->RefreshAllTabs(NULL);

if (iconschanged)
  { MyFrame::mainframe->LoadToolbarButtons();
    MyFrame::mainframe->RefreshAllTabs(NULL, true);
  }

if (panehighlightingchanged)
  wxGetApp().SetPaneHighlightColours();

Save();
}

//static 
void ConfigureDisplay::LoadDisplayConfig()
{
wxConfigBase* config = wxConfigBase::Get(); if (config==NULL)  return; 

config->Read(wxT("/Misc/Display/SHOW_TREE_LINES"), &SHOW_TREE_LINES, 1);
TREE_INDENT = (int)config->Read(wxT("/Misc/Display/TREE_INDENT"), 10l);
TREE_SPACING = (size_t)config->Read(wxT("/Misc/Display/TREE_SPACING"), 10l);
config->Read(wxT("/Misc/Display/SHOWHIDDEN"), &SHOWHIDDEN, 1);
config->Read(wxT("/Misc/Display/USE_LC_COLLATE"), &USE_LC_COLLATE, 1); SetSortMethod(USE_LC_COLLATE); SetDirpaneSortMethod(USE_LC_COLLATE);
config->Read(wxT("/Misc/Display/SYMLINKTODIR"), &TREAT_SYMLINKTODIR_AS_DIR, 1);

wxColour defaultcol;
  {
    wxDialog fake(NULL, -1, wxT("")); wxTreeCtrl* tree = new wxTreeCtrl(&fake);
    defaultcol = GetSaneColour(tree, true, wxSYS_COLOUR_WINDOW);
  }
long defaultcolour = (int)(defaultcol.Blue() << 16) + (int)(defaultcol.Green() << 8)  + defaultcol.Red();

config->Read(wxT("/Misc/Display/COLOUR_PANE_BACKGROUND"), &COLOUR_PANE_BACKGROUND, 0);
config->Read(wxT("/Misc/Display/SINGLE_BACKGROUND_COLOUR"), &SINGLE_BACKGROUND_COLOUR, 0);
long dvcol = config->Read(wxT("/Misc/Display/BACKGROUND_COLOUR_DV"), defaultcolour);
long fvcol = config->Read(wxT("/Misc/Display/BACKGROUND_COLOUR_FV"), defaultcolour);
BACKGROUND_COLOUR_DV.Set((unsigned char)dvcol, (unsigned char)(dvcol >> 8), (unsigned char)(dvcol >> 16));
BACKGROUND_COLOUR_FV.Set((unsigned char)fvcol, (unsigned char)(fvcol >> 8), (unsigned char)(fvcol >> 16));

config->Read(wxT("/Misc/Display/USE_STRIPES"), &USE_STRIPES, 0);
long stripe0 = config->Read(wxT("/Misc/Display/STRIPE_0"), defaultcolour);
long stripe1 = config->Read(wxT("/Misc/Display/STRIPE_1"), defaultcolour);
STRIPE_0.Set((unsigned char)stripe0, (unsigned char)(stripe0 >> 8), (unsigned char)(stripe0 >> 16));
STRIPE_1.Set((unsigned char)stripe1, (unsigned char)(stripe1 >> 8), (unsigned char)(stripe1 >> 16));

config->Read(wxT("/Misc/Display/HIGHLIGHT_PANES"), &HIGHLIGHT_PANES, 1);
DIRVIEW_HIGHLIGHT_OFFSET = (int)config->Read(wxT("/Misc/Display/DIRVIEW_HIGHLIGHT_OFFSET"), 25l);
FILEVIEW_HIGHLIGHT_OFFSET = (int)config->Read(wxT("/Misc/Display/FILEVIEW_HIGHLIGHT_OFFSET"), 15l);
config->Read(wxT("/Misc/Display/SHOW_DIR_IN_TITLEBAR"), &SHOW_DIR_IN_TITLEBAR, 1);
config->Read(wxT("/Misc/Display/SHOW_TB_TEXTCTRL"), &SHOW_TB_TEXTCTRL, 1);
config->Read(wxT("/Misc/Display/ASK_ON_TRASH"), &ASK_ON_TRASH, 1);
config->Read(wxT("/Misc/Display/ASK_ON_DELETE"), &ASK_ON_DELETE, 1);
config->Read(wxT("/Misc/Display/USE_STOCK_ICONS"), &USE_STOCK_ICONS, 1);
config->Read(wxT("/Misc/Display/TRASHCAN"), &TRASHCAN);
config->Read(wxT("/Misc/Display/USE_FSWATCHER"), &USE_FSWATCHER, 1);
#if defined(__WXX11__) || !defined(__LINUX__)
  USE_FSWATCHER = 0; // wxX11/hurd can't cope
#endif

config->Read(wxT("/Misc/Display/TREE_FONT/UseSystemDefault"), &USE_DEFAULT_TREE_FONT, true);
CHOSEN_TREE_FONT.SetPointSize((int)config->Read(wxT("/Misc/Display/TREE_FONT/pointsize"), 14l));
CHOSEN_TREE_FONT.SetFamily((wxFontFamily)config->Read(wxT("/Misc/Display/TREE_FONT/family"),  (long)wxFONTFAMILY_DEFAULT));
CHOSEN_TREE_FONT.SetStyle((wxFontStyle)config->Read(wxT("/Misc/Display/TREE_FONT/style"),  (long)wxFONTSTYLE_NORMAL));
CHOSEN_TREE_FONT.SetWeight(LoadFontWeight("/Misc/Display/TREE_FONT/"));
bool under; config->Read(wxT("/Misc/Display/TREE_FONT/underlined"), &under, false); CHOSEN_TREE_FONT.SetUnderlined(under);
wxString face; config->Read(wxT("/Misc/Display/TREE_FONT/facename"), &face); CHOSEN_TREE_FONT.SetFaceName(face);

  // This one's a bit separate: it's loaded here, but only saved from TreeListHeaderWindow::OnExtSortMethodChanged 
EXTENSION_START = (int)config->Read(wxT("/Misc/Display/EXTENSION_START"), 0l);

if (USE_DEFAULT_TREE_FONT || !CHOSEN_TREE_FONT.Ok()) 
      TREE_FONT = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
  else  TREE_FONT = CHOSEN_TREE_FONT;
}

void ConfigureDisplay::Save()
{
wxConfigBase* config = wxConfigBase::Get(); if (config==NULL)  return;

config->Write(wxT("/Misc/Display/SHOW_TREE_LINES"), SHOW_TREE_LINES);
config->Write(wxT("/Misc/Display/TREE_INDENT"), (long)TREE_INDENT);
config->Write(wxT("/Misc/Display/TREE_SPACING"), (long)TREE_SPACING);
config->Write(wxT("/Misc/Display/SHOWHIDDEN"), SHOWHIDDEN);
config->Write(wxT("/Misc/Display/USE_LC_COLLATE"), USE_LC_COLLATE);
config->Write(wxT("/Misc/Display/SYMLINKTODIR"), TREAT_SYMLINKTODIR_AS_DIR);

wxColour defaultcol = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
config->Write(wxT("/Misc/Display/COLOUR_PANE_BACKGROUND"), COLOUR_PANE_BACKGROUND);
config->Write(wxT("/Misc/Display/SINGLE_BACKGROUND_COLOUR"), SINGLE_BACKGROUND_COLOUR);
if (!BACKGROUND_COLOUR_DV.Ok()) BACKGROUND_COLOUR_DV = defaultcol;
if (!BACKGROUND_COLOUR_FV.Ok()) BACKGROUND_COLOUR_FV = defaultcol;
long colour = (int)(BACKGROUND_COLOUR_DV.Blue() << 16) + (int)(BACKGROUND_COLOUR_DV.Green() << 8)  + BACKGROUND_COLOUR_DV.Red();
config->Write(wxT("/Misc/Display/BACKGROUND_COLOUR_DV"), colour);
colour = (int)(BACKGROUND_COLOUR_FV.Blue() << 16) + (int)(BACKGROUND_COLOUR_FV.Green() << 8)  + BACKGROUND_COLOUR_FV.Red();
config->Write(wxT("/Misc/Display/BACKGROUND_COLOUR_FV"), colour);

config->Write(wxT("/Misc/Display/USE_STRIPES"), USE_STRIPES);
if (!STRIPE_0.Ok()) STRIPE_0 = defaultcol;
if (!STRIPE_1.Ok()) STRIPE_1 = defaultcol;
colour = (int)(STRIPE_0.Blue() << 16) + (int)(STRIPE_0.Green() << 8)  + STRIPE_0.Red();
config->Write(wxT("/Misc/Display/STRIPE_0"), colour);
colour = (int)(STRIPE_1.Blue() << 16) + (int)(STRIPE_1.Green() << 8)  + STRIPE_1.Red();
config->Write(wxT("/Misc/Display/STRIPE_1"), colour);

config->Write(wxT("/Misc/Display/HIGHLIGHT_PANES"), HIGHLIGHT_PANES);
config->Write(wxT("/Misc/Display/DIRVIEW_HIGHLIGHT_OFFSET"), (long)DIRVIEW_HIGHLIGHT_OFFSET);
config->Write(wxT("/Misc/Display/FILEVIEW_HIGHLIGHT_OFFSET"), (long)FILEVIEW_HIGHLIGHT_OFFSET);
config->Write(wxT("/Misc/Display/SHOW_DIR_IN_TITLEBAR"), SHOW_DIR_IN_TITLEBAR);
config->Write(wxT("/Misc/Display/SHOW_TB_TEXTCTRL"), SHOW_TB_TEXTCTRL);
config->Write(wxT("/Misc/Display/ASK_ON_TRASH"), ASK_ON_TRASH);
config->Write(wxT("/Misc/Display/ASK_ON_DELETE"), ASK_ON_DELETE);
config->Write(wxT("/Misc/Display/USE_STOCK_ICONS"), USE_STOCK_ICONS);
config->Write(wxT("/Misc/Display/TRASHCAN"), TRASHCAN);
config->Write(wxT("/Misc/Display/USE_FSWATCHER"), USE_FSWATCHER);

config->Write(wxT("/Misc/Display/TREE_FONT/UseSystemDefault"), USE_DEFAULT_TREE_FONT);
if (!CHOSEN_TREE_FONT.Ok()) CHOSEN_TREE_FONT = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);  // Will be needed on first ever Save
config->Write(wxT("/Misc/Display/TREE_FONT/pointsize"), (long)CHOSEN_TREE_FONT.GetPointSize());
config->Write(wxT("/Misc/Display/TREE_FONT/family"), (long)CHOSEN_TREE_FONT.GetFamily());
config->Write(wxT("/Misc/Display/TREE_FONT/style"), (long)CHOSEN_TREE_FONT.GetStyle());
SaveFontWeight("/Misc/Display/TREE_FONT/", CHOSEN_TREE_FONT.GetWeight());
config->Write(wxT("/Misc/Display/TREE_FONT/underlined"), CHOSEN_TREE_FONT.GetUnderlined());
config->Write(wxT("/Misc/Display/TREE_FONT/facename"), CHOSEN_TREE_FONT.GetFaceName());

config->Flush();
}
//-------------------------------------------------------------------------------------------------------------------------------------------

void DevicesPanel::InitialiseData() // Fill the temp vars with the old data. Used initially and also to revert on Cancel
{
tempDEVICE_DISCOVERY_TYPE = DEVICE_DISCOVERY_TYPE;
tempUSB_DEVICES_AUTOMOUNT = USB_DEVICES_AUTOMOUNT;
tempFLOPPY_DEVICES_AUTOMOUNT = FLOPPY_DEVICES_AUTOMOUNT;
tempCDROM_DEVICES_AUTOMOUNT = CDROM_DEVICES_AUTOMOUNT;

tempFLOPPY_DEVICES_DISCOVERY_INTO_FSTAB = FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB;
tempCD_DEVICES_DISCOVERY_INTO_FSTAB = CD_DEVICES_DISCOVERY_INTO_FSTAB;
tempUSB_DEVICES_DISCOVERY_INTO_FSTAB = USB_DEVICES_DISCOVERY_INTO_FSTAB;
tempFLOPPY_DEVICES_SUPERMOUNT = FLOPPY_DEVICES_SUPERMOUNT;
tempCDROM_DEVICES_SUPERMOUNT = CDROM_DEVICES_SUPERMOUNT;
tempUSB_DEVICES_SUPERMOUNT = USB_DEVICES_SUPERMOUNT;

tempSUPPRESS_EMPTY_USBDEVICES = SUPPRESS_EMPTY_USBDEVICES ;
tempASK_BEFORE_UMOUNT_ON_EXIT = ASK_BEFORE_UMOUNT_ON_EXIT;
tempCHECK_USB_TIMEINTERVAL = CHECK_USB_TIMEINTERVAL;

pDM = MyFrame::mainframe->Layout->m_notebook->DeviceMan->deviceman;
for (int n = (int)DeviceStructArray->GetCount();  n > 0; --n)  // Delete in case of re-entry
  { DevicesStruct* item = DeviceStructArray->Item(n-1); delete item; DeviceStructArray->RemoveAt(n-1); }
pDM->SetupConfigureFixedorUsbDevices(*DeviceStructArray);
}

void DevicesPanel::InitPgRem(wxNotebookPage* pg)
{
pageRem = pg; subpage = CD_Rem; InsertDataPgRem(); 
pg->Disconnect(XRCID("AddBtn"), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(DevicesPanel::OnAddButton), NULL, this); // In case of re-entry
pg->Disconnect(XRCID("EditBtn"), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(DevicesPanel::OnEditButton), NULL, this);
pg->Disconnect(XRCID("DeleteBtn"), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(DevicesPanel::OnDeleteButton), NULL, this);
pg->Connect(XRCID("AddBtn"), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(DevicesPanel::OnAddButton), NULL, this);
pg->Connect(XRCID("EditBtn"), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(DevicesPanel::OnEditButton), NULL, this);
pg->Connect(XRCID("DeleteBtn"), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(DevicesPanel::OnDeleteButton), NULL, this);

pg->Disconnect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(DevicesPanel::DoUpdateUI), NULL, this);
pg->Connect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(DevicesPanel::DoUpdateUI), NULL, this);
}

void DevicesPanel::InitPgFix(wxNotebookPage* pg)
{
pageFix = pg; subpage = CD_Fix; InsertDataPgFix(); 
pg->Disconnect(XRCID("AddBtn"), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(DevicesPanel::OnAddButton), NULL, this); // In case of re-entry
pg->Disconnect(XRCID("EditBtn"), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(DevicesPanel::OnEditButton), NULL, this);
pg->Disconnect(XRCID("DeleteBtn"), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(DevicesPanel::OnDeleteButton), NULL, this);
pg->Connect(XRCID("AddBtn"), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(DevicesPanel::OnAddButton), NULL, this);
pg->Connect(XRCID("EditBtn"), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(DevicesPanel::OnEditButton), NULL, this);
pg->Connect(XRCID("DeleteBtn"), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(DevicesPanel::OnDeleteButton), NULL, this);
pg->Disconnect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(DevicesPanel::DoUpdateUI), NULL, this);
pg->Connect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(DevicesPanel::DoUpdateUI), NULL, this);
}

void DevicesPanel::InsertDataPgAM()
{
((wxRadioBox*)pageAM->FindWindow(wxT("DiscoveryRadio")))->SetSelection((int)tempDEVICE_DISCOVERY_TYPE);
((wxCheckBox*)pageAM->FindWindow(wxT("FloppyAutomount")))->SetValue(tempFLOPPY_DEVICES_AUTOMOUNT);
((wxCheckBox*)pageAM->FindWindow(wxT("DVDAutomount")))->SetValue(tempCDROM_DEVICES_AUTOMOUNT);
((wxCheckBox*)pageAM->FindWindow(wxT("USBAutomount")))->SetValue(tempUSB_DEVICES_AUTOMOUNT);
m_nb->StoreApplyBtn(pageAM->FindWindow(wxT("DAMApply")));
}

void DevicesPanel::InsertDataPgMt()
{
((wxRadioBox*)pageMt->FindWindow(wxT("FloppyRadio")))->SetSelection(tempFLOPPY_DEVICES_DISCOVERY_INTO_FSTAB);
((wxRadioBox*)pageMt->FindWindow(wxT("CDRadio")))->SetSelection(tempCD_DEVICES_DISCOVERY_INTO_FSTAB);
((wxRadioBox*)pageMt->FindWindow(wxT("USBRadio")))->SetSelection(tempUSB_DEVICES_DISCOVERY_INTO_FSTAB);
((wxCheckBox*)pageMt->FindWindow(wxT("FloppySupermntChk")))->SetValue(tempFLOPPY_DEVICES_SUPERMOUNT);
((wxCheckBox*)pageMt->FindWindow(wxT("CDSupermntChk")))->SetValue(tempCDROM_DEVICES_SUPERMOUNT);
((wxCheckBox*)pageMt->FindWindow(wxT("UsbSupermntChk")))->SetValue(tempUSB_DEVICES_SUPERMOUNT);
m_nb->StoreApplyBtn(pageMt->FindWindow(wxT("DAMApply")));
}

void DevicesPanel::InsertDataPgUsb()
{
((wxSpinCtrl*)pageUsb->FindWindow(wxT("FreqSpin")))->SetValue(tempCHECK_USB_TIMEINTERVAL);
((wxCheckBox*)pageUsb->FindWindow(wxT("ShowEvenEmpties")))->SetValue(!tempSUPPRESS_EMPTY_USBDEVICES);
((wxCheckBox*)pageUsb->FindWindow(wxT("UmountCheck")))->SetValue(tempASK_BEFORE_UMOUNT_ON_EXIT);
m_nb->StoreApplyBtn(pageUsb->FindWindow(wxT("DAMApply")));
}

void DevicesPanel::InsertDataPgRem()
{
if (pDM==NULL) return;

list = (wxListBox*)pageRem->FindWindow(wxT("EditDrive"));
FillListbox();
dirty[subpage] = false; 
m_nb->StoreApplyBtn(pageRem->FindWindow(wxT("DAMApply")));
}

void DevicesPanel::InsertDataPgFix()
{
if (pDM==NULL) return;

list = (wxListBox*)pageFix->FindWindow(wxT("EditDrive"));
FillListbox();
dirty[subpage] = false; 
m_nb->StoreApplyBtn(pageFix->FindWindow(wxT("DAMApply")));
}

void DevicesPanel::OnUpdateUI()
{ // NB The CD_Rem and CD_Fix updateUI happens in DevicesPanel::DoUpdateUI
if (subpage==CD_AM)
  { if (pageAM==NULL) return;
    tempDEVICE_DISCOVERY_TYPE = (enum DiscoveryMethod)((wxRadioBox*)pageAM->FindWindow(wxT("DiscoveryRadio")))->GetSelection();
    tempFLOPPY_DEVICES_AUTOMOUNT = ((wxCheckBox*)pageAM->FindWindow(wxT("FloppyAutomount")))->IsChecked();
    tempCDROM_DEVICES_AUTOMOUNT = ((wxCheckBox*)pageAM->FindWindow(wxT("DVDAutomount")))->IsChecked();
    tempUSB_DEVICES_AUTOMOUNT = ((wxCheckBox*)pageAM->FindWindow(wxT("USBAutomount")))->IsChecked();


    dirty[subpage] = (DEVICE_DISCOVERY_TYPE!=tempDEVICE_DISCOVERY_TYPE) || (FLOPPY_DEVICES_AUTOMOUNT!=tempFLOPPY_DEVICES_AUTOMOUNT) || 
                                      (CDROM_DEVICES_AUTOMOUNT!=tempCDROM_DEVICES_AUTOMOUNT) || (USB_DEVICES_AUTOMOUNT!=tempUSB_DEVICES_AUTOMOUNT);

    pageAM->FindWindow(wxT("DAMApply"))->Enable(dirty[subpage]);
    pageAM->FindWindow(wxT("DAMCancel"))->Enable(dirty[subpage]);
    m_nb->SetPageStatus(dirty[subpage]);
  }
if (subpage==CD_Mt)
  { if (pageMt==NULL) return;
    tempFLOPPY_DEVICES_DISCOVERY_INTO_FSTAB = (TableInsertMethod)((wxRadioBox*)pageMt->FindWindow(wxT("FloppyRadio")))->GetSelection();
    tempCD_DEVICES_DISCOVERY_INTO_FSTAB = (TableInsertMethod)(((wxRadioBox*)pageMt->FindWindow(wxT("CDRadio")))->GetSelection());
    tempUSB_DEVICES_DISCOVERY_INTO_FSTAB = (TableInsertMethod)(((wxRadioBox*)pageMt->FindWindow(wxT("USBRadio")))->GetSelection());
    tempFLOPPY_DEVICES_SUPERMOUNT = ((wxCheckBox*)pageMt->FindWindow(wxT("FloppySupermntChk")))->IsChecked();
    tempCDROM_DEVICES_SUPERMOUNT = ((wxCheckBox*)pageMt->FindWindow(wxT("CDSupermntChk")))->IsChecked();
    tempUSB_DEVICES_SUPERMOUNT = ((wxCheckBox*)pageMt->FindWindow(wxT("UsbSupermntChk")))->IsChecked();
    dirty[subpage] = (FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB!=tempFLOPPY_DEVICES_DISCOVERY_INTO_FSTAB) || (CD_DEVICES_DISCOVERY_INTO_FSTAB!=tempCD_DEVICES_DISCOVERY_INTO_FSTAB) || 
        (USB_DEVICES_DISCOVERY_INTO_FSTAB!=tempUSB_DEVICES_DISCOVERY_INTO_FSTAB) || (FLOPPY_DEVICES_SUPERMOUNT!=tempFLOPPY_DEVICES_SUPERMOUNT) ||
        (CDROM_DEVICES_SUPERMOUNT!=tempCDROM_DEVICES_SUPERMOUNT) || (USB_DEVICES_SUPERMOUNT!=tempUSB_DEVICES_SUPERMOUNT);

    pageMt->FindWindow(wxT("DAMApply"))->Enable(dirty[subpage]);
    pageMt->FindWindow(wxT("DAMCancel"))->Enable(dirty[subpage]);
    m_nb->SetPageStatus(dirty[subpage]);
  }
if (subpage==CD_Usb)
  { if (pageUsb==NULL) return;
    tempCHECK_USB_TIMEINTERVAL = ((wxSpinCtrl*)pageUsb->FindWindow(wxT("FreqSpin")))->GetValue();
    tempSUPPRESS_EMPTY_USBDEVICES = (((wxCheckBox*)pageUsb->FindWindow(wxT("ShowEvenEmpties")))->IsChecked() == 0);
    tempASK_BEFORE_UMOUNT_ON_EXIT = ((wxCheckBox*)pageUsb->FindWindow(wxT("UmountCheck")))->IsChecked();

    dirty[subpage] = (CHECK_USB_TIMEINTERVAL!=tempCHECK_USB_TIMEINTERVAL) 
    || (SUPPRESS_EMPTY_USBDEVICES!=tempSUPPRESS_EMPTY_USBDEVICES)
    || (ASK_BEFORE_UMOUNT_ON_EXIT!=tempASK_BEFORE_UMOUNT_ON_EXIT);

    pageUsb->FindWindow(wxT("DAMApply"))->Enable(dirty[subpage]);
    pageUsb->FindWindow(wxT("DAMCancel"))->Enable(dirty[subpage]);
    m_nb->SetPageStatus(dirty[subpage]);
  }
}

void DevicesPanel::OnCancel()
{
InitialiseData();

switch(subpage)
  { case CD_AM:   InsertDataPgAM();  return;
    case CD_Mt:   InsertDataPgMt();  return;
    case CD_Usb:  InsertDataPgUsb(); return;
    case CD_Rem:  InsertDataPgRem(); return;
    case CD_Fix:  InsertDataPgFix(); return;
  }
}

void DevicesPanel::OnOK()
{
if ((subpage == CD_Fix) || (subpage == CD_Rem))   // which are rather different
  { pDM->ReturnFromConfigureFixedorUsbDevices(*DeviceStructArray, (subpage==CD_Fix) ? CDT_fixed : CDT_removable);  // The 'save' happens here too
    dirty[subpage] = false; return;
  }

DEVICE_DISCOVERY_TYPE = tempDEVICE_DISCOVERY_TYPE;
USB_DEVICES_AUTOMOUNT = tempUSB_DEVICES_AUTOMOUNT;
FLOPPY_DEVICES_AUTOMOUNT = tempFLOPPY_DEVICES_AUTOMOUNT;
CDROM_DEVICES_AUTOMOUNT = tempCDROM_DEVICES_AUTOMOUNT;

FLOPPY_DEVICES_DISCOVERY_INTO_FSTAB = tempFLOPPY_DEVICES_DISCOVERY_INTO_FSTAB;
CD_DEVICES_DISCOVERY_INTO_FSTAB = tempCD_DEVICES_DISCOVERY_INTO_FSTAB;
USB_DEVICES_DISCOVERY_INTO_FSTAB = tempUSB_DEVICES_DISCOVERY_INTO_FSTAB;
FLOPPY_DEVICES_SUPERMOUNT = tempFLOPPY_DEVICES_SUPERMOUNT;
CDROM_DEVICES_SUPERMOUNT = tempCDROM_DEVICES_SUPERMOUNT;
USB_DEVICES_SUPERMOUNT = tempUSB_DEVICES_SUPERMOUNT;

SUPPRESS_EMPTY_USBDEVICES = tempSUPPRESS_EMPTY_USBDEVICES;
ASK_BEFORE_UMOUNT_ON_EXIT = tempASK_BEFORE_UMOUNT_ON_EXIT;
CHECK_USB_TIMEINTERVAL = tempCHECK_USB_TIMEINTERVAL;

DeviceManager::SaveMiscData();

  // These 2 used to be done elsewhere, so aren't in a standard Save()
wxConfigBase::Get()->Write(wxT("Devices/Misc/ASK_BEFORE_UMOUNT_ON_EXIT"), ASK_BEFORE_UMOUNT_ON_EXIT);
wxConfigBase::Get()->Write(wxT("Devices/Misc/CHECK_USB_TIMEINTERVAL"), (long)CHECK_USB_TIMEINTERVAL);
wxConfigBase::Get()->Flush();

MyFrame::mainframe->Layout->m_notebook->DeviceMan->usbtimer->Stop();
MyFrame::mainframe->Layout->m_notebook->DeviceMan->usbtimer->Start(CHECK_USB_TIMEINTERVAL * 1000, wxTIMER_CONTINUOUS);  // Restart with new value
}


void DevicesPanel::FillListbox()
{
list->Clear();                                        // In case this is a refill

size_t first, last;
if (subpage==CD_Fix) { first = 0; last = pDM->FirstOtherDevice; }
  else { first = pDM->FirstOtherDevice; last = DeviceStructArray->GetCount(); }

for (size_t n=first; n < last; ++n)
  { wxString item;
    if (subpage==CD_Fix)  item = DeviceStructArray->Item(n)->devicenode;
      else item = DeviceStructArray->Item(n)->name2;
    if (DeviceStructArray->Item(n)->ignore) item += _(" (Ignored)");
    list->Append(item);
  }

if (last)  list->SetSelection(0);
}

void DevicesPanel::OnAddButton(wxCommandEvent& WXUNUSED(event))
{
DevicesStruct* tempstruct = new DevicesStruct;
tempstruct->defaultpartition = 0; 
if (subpage == CD_Fix)
  { tempstruct->readonly = true;
    MyFrame::mainframe->Layout->m_notebook->DeviceMan->ConfigureNewFixedDevice(tempstruct);
    if (tempstruct->ignore == DS_defer) { delete tempstruct; return; }; // DS_defer flags that the user pressed Cancel
    
    DeviceStructArray->Insert(tempstruct, pDM->FirstOtherDevice++);     // Insert into the array, at the end of the fixed-devices bit
  }
 else 
   { tempstruct->readonly = false;
    MyFrame::mainframe->Layout->m_notebook->DeviceMan->ConfigureNewDevice(tempstruct);
    if (tempstruct->ignore == DS_defer) { delete tempstruct; return; };  // DS_defer flags that the user pressed Cancel

    tempstruct->name1 = tempstruct->Vendorstr; tempstruct->name2 = tempstruct->Productstr; // Let's hope the user got them right :/
    DeviceStructArray->Add(tempstruct);
  }

FillListbox();                                        // Redo the listbox to reflect the new entry
}

void DevicesPanel::OnEditButton(wxCommandEvent& WXUNUSED(event))
{
int index = list->GetSelection(); if (index == wxNOT_FOUND) return;
if (subpage == CD_Rem)  index += pDM->FirstOtherDevice;  // Removable devices start here

DevicesStruct* tempstruct = DeviceStructArray->Item(index);
if (tempstruct == NULL) return;

wxString Vendorstr = tempstruct->Vendorstr; wxString Productstr = tempstruct->Productstr;
bool readonly = tempstruct->readonly; int ignore = tempstruct->ignore; int type = tempstruct->devicetype;

if (subpage == CD_Fix)
  MyFrame::mainframe->Layout->m_notebook->DeviceMan->ConfigureNewFixedDevice(tempstruct, true);
 else 
  MyFrame::mainframe->Layout->m_notebook->DeviceMan->ConfigureNewDevice(tempstruct, true);

FillListbox();                                        // Redo the listbox in case a item is now labelled ignore
if ((Vendorstr != tempstruct->Vendorstr) || (Productstr != tempstruct->Productstr)
      || (readonly != tempstruct->readonly) || (ignore != tempstruct->ignore) || (type != tempstruct->devicetype))
  dirty[subpage] = true;                              // If there were any changes
}

void DevicesPanel::OnDeleteButton(wxCommandEvent& WXUNUSED(event))
{
int index = list->GetSelection(); if (index == wxNOT_FOUND) return;
if (subpage == CD_Rem)  index += pDM->FirstOtherDevice;  // Removable devices start here

DevicesStruct* tempstruct = DeviceStructArray->Item(index);
if (tempstruct == NULL || tempstruct->ignore == DS_true) return;  // Can't ignore it twice

wxWindow* win = (subpage==CD_Rem) ? pageRem : pageFix;
wxMessageDialog ask(win,_("Delete this Device?"),_("Are you sure?"), wxYES_NO | wxICON_QUESTION);
int ans = ask.ShowModal(); if (ans != wxID_YES) return;

tempstruct->ignore = true;                            // "Delete" just means ignoring
FillListbox();                                        // Redo the listbox
}

void DevicesPanel::DoUpdateUI(wxUpdateUIEvent& event)
{
if ((event.GetId()==XRCID("DAMApply")) || (event.GetId()==XRCID("DAMCancel")))
  { if (DeviceStructArray->GetCount() != pDM->GetDevicearray()->GetCount()) { event.Enable(true); dirty[subpage] = true; m_nb->SetPageStatus(dirty[subpage]); return; } // Must have been an addition
    if (dirty[subpage]) { event.Enable(true); m_nb->SetPageStatus(dirty[subpage]); return; } // Marked within the subpage
    if (subpage==CD_Fix)
     { for (size_t n = 0; n < pDM->FirstOtherDevice && n < DeviceStructArray->GetCount(); ++n)
          if (*DeviceStructArray->Item(n) != *(pDM->GetDevicearray()->Item(n))) 
            { event.Enable(true); dirty[subpage] = true; m_nb->SetPageStatus(dirty[subpage]); return; } // Must have been an alteration
     }
    else
     { for (size_t n = pDM->FirstOtherDevice; n < DeviceStructArray->GetCount(); ++n)
          if (*DeviceStructArray->Item(n) != *(pDM->GetDevicearray()->Item(n))) 
            { event.Enable(true); dirty[subpage] = true; m_nb->SetPageStatus(dirty[subpage]); return; } // Must have been an alteration
     }
    
    event.Enable(false); dirty[subpage] = false;  m_nb->SetPageStatus(dirty[subpage]); return;
  }

  // Apart from Apply/Cancel, we're only interested in the Edit and Delete buttons. The Add button must always be valid
if (event.GetId() == XRCID("EditBtn"))  { event.Enable(list->GetCount()); return; }  // For Edit, enable if there're any items
if (event.GetId() == XRCID("DeleteBtn"))
  { if (!list->GetCount()) { event.Enable(false); return; }                            // For Delete too, no items mean no del
    int index = list->GetSelection(); if (index == wxNOT_FOUND) return;                // But also disable if the current sel is ignored: it can't be ignored twice
    if (subpage==CD_Rem)  index += pDM->FirstOtherDevice;  // Removable devices start here
    DevicesStruct* tempstruct = DeviceStructArray->Item(index); if (tempstruct == NULL) return;

    event.Enable(tempstruct->ignore != DS_true);
  }
}

//-------------------------------------------------------------------------------------------------------------------------------------------

void DevicesAdvancedPanel::InitialiseData() // Fill the temp vars with the old data. Used initially and also to revert on Cancel
{
tempPARTITIONS = PARTITIONS;
tempDEVICEDIR = DEVICEDIR;
tempFLOPPYINFOPATH = FLOPPYINFOPATH;
tempCDROMINFO = CDROMINFO;

tempSCSIUSBDIR = SCSIUSBDIR;
tempSCSISCSI = SCSISCSI;
tempAUTOMOUNT_USB_PREFIX = AUTOMOUNT_USB_PREFIX;
tempSCSI_DEVICES = SCSI_DEVICES;

tempLVMPREFIX = LVMPREFIX;
tempLVM_INFO_FILE_PREFIX = LVM_INFO_FILE_PREFIX;
}

void DevicesAdvancedPanel::InitPgA(wxNotebookPage* pg)
{
pageA = pg; InsertDataPgA(); subpage=DAP_A; 
pg->Disconnect(XRCID("Defaults"), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(DevicesAdvancedPanel::OnRestoreDefaultsBtn), NULL, this); // In case of re-entry
pg->Connect(XRCID("Defaults"), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(DevicesAdvancedPanel::OnRestoreDefaultsBtn), NULL, this);
}

void DevicesAdvancedPanel::InitPgB(wxNotebookPage* pg)
{
pageB = pg; InsertDataPgB(); subpage=DAP_B; 
pg->Disconnect(XRCID("Defaults"), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(DevicesAdvancedPanel::OnRestoreDefaultsBtn), NULL, this);
pg->Connect(XRCID("Defaults"), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(DevicesAdvancedPanel::OnRestoreDefaultsBtn), NULL, this);
}

void DevicesAdvancedPanel::InitPgC(wxNotebookPage* pg)
{
pageC = pg; InsertDataPgC(); subpage=DAP_C; 
pg->Disconnect(XRCID("Defaults"), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(DevicesAdvancedPanel::OnRestoreDefaultsBtn), NULL, this);
pg->Connect(XRCID("Defaults"), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(DevicesAdvancedPanel::OnRestoreDefaultsBtn), NULL, this);
}

void DevicesAdvancedPanel::InsertDataPgA()
{
if (!pageA) return;
((wxTextCtrl*)pageA->FindWindow(wxT("PARTITIONS")))->SetValue(tempPARTITIONS);
((wxTextCtrl*)pageA->FindWindow(wxT("DEVICEDIR")))->SetValue(tempDEVICEDIR);
((wxTextCtrl*)pageA->FindWindow(wxT("FLOPPYINFOPATH")))->SetValue(tempFLOPPYINFOPATH);
((wxTextCtrl*)pageA->FindWindow(wxT("CDROMINFO")))->SetValue(tempCDROMINFO);
m_nb->StoreApplyBtn(pageA->FindWindow(wxT("CADApply")));
}

void DevicesAdvancedPanel::InsertDataPgB()
{
if (!pageB) return;
((wxTextCtrl*)pageB->FindWindow(wxT("SCSIUSBDIR")))->SetValue(tempSCSIUSBDIR);
((wxTextCtrl*)pageB->FindWindow(wxT("SCSISCSI")))->SetValue(tempSCSISCSI);
((wxTextCtrl*)pageB->FindWindow(wxT("AUTOMOUNT_USB_PREFIX")))->SetValue(tempAUTOMOUNT_USB_PREFIX);
((wxTextCtrl*)pageB->FindWindow(wxT("SCSI_DEVICES")))->SetValue(tempSCSI_DEVICES);
m_nb->StoreApplyBtn(pageA->FindWindow(wxT("CADApply")));
}

void DevicesAdvancedPanel::InsertDataPgC()
{
if (!pageC) return;
((wxTextCtrl*)pageC->FindWindow(wxT("LVMPREFIX")))->SetValue(tempLVMPREFIX);
((wxTextCtrl*)pageC->FindWindow(wxT("LVM_INFO_FILE_PREFIX")))->SetValue(tempLVM_INFO_FILE_PREFIX);
m_nb->StoreApplyBtn(pageA->FindWindow(wxT("CADApply")));
}

void DevicesAdvancedPanel::OnUpdateUI()
{
if (subpage==DAP_A)
  { if (pageA==NULL) return;
    tempPARTITIONS = ((wxTextCtrl*)pageA->FindWindow(wxT("PARTITIONS")))->GetValue();
    tempDEVICEDIR = ((wxTextCtrl*)pageA->FindWindow(wxT("DEVICEDIR")))->GetValue();
    tempFLOPPYINFOPATH = ((wxTextCtrl*)pageA->FindWindow(wxT("FLOPPYINFOPATH")))->GetValue();
    tempCDROMINFO = ((wxTextCtrl*)pageA->FindWindow(wxT("CDROMINFO")))->GetValue();

    dirty[subpage] = (PARTITIONS!=tempPARTITIONS) || (DEVICEDIR!=tempDEVICEDIR) || (FLOPPYINFOPATH!=tempFLOPPYINFOPATH) || (CDROMINFO!=tempCDROMINFO);

    pageA->FindWindow(wxT("CADApply"))->Enable(dirty[subpage]);
    pageA->FindWindow(wxT("CADCancel"))->Enable(dirty[subpage]);
    m_nb->SetPageStatus(dirty[subpage]);
  }
if (subpage==DAP_B)
  { if (pageB==NULL) return;
    tempSCSIUSBDIR = ((wxTextCtrl*)pageB->FindWindow(wxT("SCSIUSBDIR")))->GetValue();
    tempSCSISCSI = ((wxTextCtrl*)pageB->FindWindow(wxT("SCSISCSI")))->GetValue();
    tempAUTOMOUNT_USB_PREFIX = ((wxTextCtrl*)pageB->FindWindow(wxT("AUTOMOUNT_USB_PREFIX")))->GetValue();
    tempSCSI_DEVICES = ((wxTextCtrl*)pageB->FindWindow(wxT("SCSI_DEVICES")))->GetValue();
    dirty[subpage] = (SCSIUSBDIR!=tempSCSIUSBDIR) || (SCSISCSI!=tempSCSISCSI) || (AUTOMOUNT_USB_PREFIX!=tempAUTOMOUNT_USB_PREFIX) || (SCSI_DEVICES!=tempSCSI_DEVICES);

    pageB->FindWindow(wxT("CADApply"))->Enable(dirty[subpage]);
    pageB->FindWindow(wxT("CADCancel"))->Enable(dirty[subpage]);
    m_nb->SetPageStatus(dirty[subpage]);
  }
if (subpage==DAP_C)
  { if (pageC==NULL) return;
    tempLVMPREFIX = ((wxTextCtrl*)pageC->FindWindow(wxT("LVMPREFIX")))->GetValue();
    tempLVM_INFO_FILE_PREFIX = ((wxTextCtrl*)pageC->FindWindow(wxT("LVM_INFO_FILE_PREFIX")))->GetValue();

    dirty[subpage] = (LVMPREFIX!=tempLVMPREFIX) || (LVM_INFO_FILE_PREFIX!=tempLVM_INFO_FILE_PREFIX);

    pageC->FindWindow(wxT("CADApply"))->Enable(dirty[subpage]);
    pageC->FindWindow(wxT("CADCancel"))->Enable(dirty[subpage]);
    m_nb->SetPageStatus(dirty[subpage]);
  }
}

//static 
void DevicesAdvancedPanel::LoadConfig()
{
wxConfigBase* config = wxConfigBase::Get(); if (config==NULL)  return;

wxString item;
config->Read(wxT("Devices/Misc/PARTITIONS"), &item); if (!item.IsEmpty()) PARTITIONS = item;
config->Read(wxT("Devices/Misc/DEVICEDIR"), &item); if (!item.IsEmpty()) DEVICEDIR = item;
config->Read(wxT("Devices/Misc/FLOPPYINFOPATH"), &item); if (!item.IsEmpty()) FLOPPYINFOPATH = item;
config->Read(wxT("Devices/Misc/CDROMINFO"), &item); if (!item.IsEmpty()) CDROMINFO = item;
config->Read(wxT("Devices/Misc/ScsiUsbDir"), &item); if (!item.IsEmpty()) SCSIUSBDIR = item;
config->Read(wxT("Devices/Misc/SCSISCSI"), &item); if (!item.IsEmpty()) SCSISCSI = item;
config->Read(wxT("Devices/Misc/AutomountUsbPrefix"), &item); if (!item.IsEmpty()) AUTOMOUNT_USB_PREFIX = item;
config->Read(wxT("Devices/Misc/ScsiDevicesDir"), &item); if (!item.IsEmpty()) SCSI_DEVICES = item;
config->Read(wxT("Devices/Misc/LVMPREFIX"), &item); if (!item.IsEmpty()) LVMPREFIX = item;
config->Read(wxT("Devices/Misc/LVM_INFO_FILE_PREFIX"), &item); if (!item.IsEmpty()) LVM_INFO_FILE_PREFIX = item;
}

void DevicesAdvancedPanel::OnOK() // Get results from the dialog into the globals
{
PARTITIONS = tempPARTITIONS;
DEVICEDIR = tempDEVICEDIR;
FLOPPYINFOPATH = tempFLOPPYINFOPATH;
CDROMINFO = tempCDROMINFO;
SCSIUSBDIR = tempSCSIUSBDIR;
SCSISCSI = tempSCSISCSI;
AUTOMOUNT_USB_PREFIX = tempAUTOMOUNT_USB_PREFIX;
SCSI_DEVICES = tempSCSI_DEVICES;
LVMPREFIX = tempLVMPREFIX;
LVM_INFO_FILE_PREFIX = tempLVM_INFO_FILE_PREFIX;

SaveConfig();
}

//static
void DevicesAdvancedPanel::SaveConfig(wxConfigBase* config /*= NULL*/)
{
if (config==NULL)                                              // It'll usually be null, the exception being if coming from ConfigureMisc::OnExportBtn
   config = wxConfigBase::Get();  if (config==NULL)  return; 

config->Write(wxT("Devices/Misc/PARTITIONS"), PARTITIONS);
config->Write(wxT("Devices/Misc/DEVICEDIR"), DEVICEDIR);
config->Write(wxT("Devices/Misc/FLOPPYINFOPATH"), FLOPPYINFOPATH);
config->Write(wxT("Devices/Misc/CDROMINFO"), CDROMINFO);
config->Write(wxT("Devices/Misc/ScsiUsbDir"), SCSIUSBDIR);
config->Write(wxT("Devices/Misc/SCSISCSI"), SCSISCSI);
config->Write(wxT("Devices/Misc/AutomountUsbPrefix"), AUTOMOUNT_USB_PREFIX);
config->Write(wxT("Devices/Misc/ScsiDevicesDir"), SCSI_DEVICES);
config->Write(wxT("Devices/Misc/LVMPREFIX"), LVMPREFIX);
config->Write(wxT("Devices/Misc/LVM_INFO_FILE_PREFIX"), LVM_INFO_FILE_PREFIX);

config->Flush();
}

void DevicesAdvancedPanel::OnRestoreDefaultsBtn(wxCommandEvent& WXUNUSED(event))
{
if (subpage==DAP_A)
  { PARTITIONS = tempPARTITIONS = wxT("/proc/partitions");
    DEVICEDIR = tempDEVICEDIR = wxT("/dev/");
    FLOPPYINFOPATH = tempFLOPPYINFOPATH = wxT("/sys/block/fd");
    CDROMINFO = tempCDROMINFO = wxT("/proc/sys/dev/cdrom/info");
    InsertDataPgA();
  }
if (subpage==DAP_B)
  { SCSIUSBDIR = tempSCSIUSBDIR = wxT("/proc/scsi/usb-storage-");
    SCSISCSI = tempSCSISCSI = wxT("/proc/scsi/scsi");
    AUTOMOUNT_USB_PREFIX = tempAUTOMOUNT_USB_PREFIX = wxT("/dev/sd");
    SCSI_DEVICES = tempSCSI_DEVICES = wxT("/sys/bus/scsi/devices");
    InsertDataPgB();
  }
if (subpage==DAP_C)
  { LVMPREFIX = tempLVMPREFIX = wxT("dm-");
    LVM_INFO_FILE_PREFIX = tempLVM_INFO_FILE_PREFIX = wxT("/dev/.udevdb/block@");
    InsertDataPgC();
  }

SaveConfig();                                         // Save the new values
}

//-------------------------------------------------------------------------------------------------------------------------------------------
void ConfigureTerminals::InitialiseData()  // Fill the temp vars with the old data. Used initially and also to revert on Cancel
{
tempTERMINAL = TERMINAL;
tempTERMINAL_COMMAND = TERMINAL_COMMAND.Left(TERMINAL_COMMAND.Len()-1);  // We don't want the terminal ' ' here
tempTOOLS_TERMINAL_COMMAND = TOOLS_TERMINAL_COMMAND.Left(TOOLS_TERMINAL_COMMAND.Len()-1);
tempTERMEM_PROMPTFORMAT = TERMEM_PROMPTFORMAT;
tempTERMINAL_FONT = TERMINAL_FONT;
tempCHOSEN_TERMINAL_FONT = CHOSEN_TERMINAL_FONT;
tempUSE_DEFAULT_TERMINALEM_FONT = USE_DEFAULT_TERMINALEM_FONT;
tempPossibleTerminals = PossibleTerminals;
tempPossibleTerminalCommands = PossibleTerminalCommands;
tempPossibleToolsTerminalCommands = PossibleToolsTerminalCommands;
}

void ConfigureTerminals::InsertDataReal()
{
combo1 = (wxComboBox*)pageTerm->FindWindow(wxT("TERMINAL")); combo1->Clear();
combo2 = (wxComboBox*)pageTerm->FindWindow(wxT("TERMINAL_COMMAND")); combo2->Clear();
combo3 = (wxComboBox*)pageTerm->FindWindow(wxT("TOOLS_TERMINAL_COMMAND")); combo3->Clear();

for (size_t n=0; n < tempPossibleTerminals.GetCount(); ++n) combo1->Append(tempPossibleTerminals.Item(n));
combo1->SetStringSelection(tempTERMINAL);

for (size_t n=0; n < tempPossibleTerminalCommands.GetCount(); ++n) combo2->Append(tempPossibleTerminalCommands.Item(n));
combo2->SetStringSelection(tempTERMINAL_COMMAND);

for (size_t n=0; n < tempPossibleToolsTerminalCommands.GetCount(); ++n) combo3->Append(tempPossibleToolsTerminalCommands.Item(n));
combo3->SetStringSelection(tempTOOLS_TERMINAL_COMMAND);

((wxTextCtrl*)pageTerm->FindWindow(wxT("TerminalText")))->Clear();
((wxTextCtrl*)pageTerm->FindWindow(wxT("TerminalCommandText")))->Clear();
((wxTextCtrl*)pageTerm->FindWindow(wxT("ToolsTerminalCommandText")))->Clear();
m_nb->StoreApplyBtn(pageTerm->FindWindow(wxT("Terminals_OK")));
}

void ConfigureTerminals::InsertDataEmul()
{
((wxTextCtrl*)pageEm->FindWindow(wxT("TERMEM_PROMPTFORMAT")))->SetValue(tempTERMEM_PROMPTFORMAT);

fonttext = (wxTextCtrl*)pageEm->FindWindow(wxT("CurrentFontText"));
fonttext->SetValue(_("Selected terminal emulator Font")); fonttext->SetFont(tempCHOSEN_TERMINAL_FONT);
defaultfonttext = (wxTextCtrl*)pageEm->FindWindow(wxT("DefaultFontText"));
defaultfonttext->SetValue(_("Default Font"));
m_nb->StoreApplyBtn(pageEm->FindWindow(wxT("Terminals_OK")));
}

void ConfigureTerminals::OnUpdateUI()
{
if (subpage==CT_terminals)
  { if (pageTerm == NULL || combo1 == NULL) return;    // Because we've not finished setup yet
  
    if (combo1->GetCount()) tempTERMINAL = combo1->GetStringSelection();
    if (combo2->GetCount()) tempTERMINAL_COMMAND = combo2->GetStringSelection();
    if (combo3->GetCount()) tempTOOLS_TERMINAL_COMMAND = combo3->GetStringSelection();

    dirty[subpage] = (tempTERMINAL!=TERMINAL) || (tempTERMINAL_COMMAND!=TERMINAL_COMMAND.Left(TERMINAL_COMMAND.Len()-1))
          || (tempTOOLS_TERMINAL_COMMAND!=TOOLS_TERMINAL_COMMAND.Left(TOOLS_TERMINAL_COMMAND.Len()-1)) 
          || (tempPossibleTerminals!=PossibleTerminals) || (tempPossibleTerminalCommands!=PossibleTerminalCommands) || (tempPossibleToolsTerminalCommands!=PossibleToolsTerminalCommands);

    pageTerm->FindWindow(wxT("TerminalAccept"))->Enable(!((wxTextCtrl*)pageTerm->FindWindow(wxT("TerminalText")))->IsEmpty());
    pageTerm->FindWindow(wxT("TerminalCommandAccept"))->Enable(!((wxTextCtrl*)pageTerm->FindWindow(wxT("TerminalCommandText")))->IsEmpty());
    pageTerm->FindWindow(wxT("ToolsTerminalCommandAccept"))->Enable(!((wxTextCtrl*)pageTerm->FindWindow(wxT("ToolsTerminalCommandText")))->IsEmpty());

    pageTerm->FindWindow(wxT("Terminals_OK"))->Enable(dirty[subpage]);
    pageTerm->FindWindow(wxT("Terminals_CANCEL"))->Enable(dirty[subpage]);
    m_nb->SetPageStatus(dirty[subpage]);
  }

if (subpage==CT_emulator)
  { if (pageEm == NULL) return;    // Because we've not finished setup yet
    tempTERMEM_PROMPTFORMAT = ((wxTextCtrl*)pageEm->FindWindow(wxT("TERMEM_PROMPTFORMAT")))->GetValue();
    dirty[subpage] = fontchanged || (tempTERMEM_PROMPTFORMAT!=TERMEM_PROMPTFORMAT);
  
    fonttext->Enable(!tempUSE_DEFAULT_TERMINALEM_FONT);
    defaultfonttext->Enable(tempUSE_DEFAULT_TERMINALEM_FONT);
    pageEm->FindWindow(wxT("TerminalDefaultFontBtn"))->Enable(!tempUSE_DEFAULT_TERMINALEM_FONT);

    pageEm->FindWindow(wxT("Terminals_OK"))->Enable(dirty[subpage]);
    pageEm->FindWindow(wxT("Terminals_CANCEL"))->Enable(dirty[subpage]);
    m_nb->SetPageStatus(dirty[subpage]);
  }
}

void ConfigureTerminals::OnAccept(int id)  // When the user has provided a new entry for TERMINAL etc, add it to the combobox
{
wxComboBox* combo; wxTextCtrl* text; wxArrayString* pArray;

if (id == XRCID("TerminalAccept"))
  { text = (wxTextCtrl*)pageTerm->FindWindow(wxT("TerminalText")); combo = combo1; pArray = &tempPossibleTerminals; }
  else if (id == XRCID("TerminalCommandAccept"))
  { text = (wxTextCtrl*)pageTerm->FindWindow(wxT("TerminalCommandText")); combo = combo2; pArray = &tempPossibleTerminalCommands; }
  else if (id == XRCID("ToolsTerminalCommandAccept"))
  { text = (wxTextCtrl*)pageTerm->FindWindow(wxT("ToolsTerminalCommandText")); combo = combo3; pArray = &tempPossibleToolsTerminalCommands; }
  else return;

wxString newstring = text->GetValue(); text->Clear();
newstring.Trim();                                            // Though for all but TERMINAL we'll need a terminal ' ', remove any for now for ease of comparison
if (combo->SetStringSelection(newstring))  return;       // Old news

pArray->Add(newstring);
combo->Append(newstring);
combo->SetStringSelection(newstring);
}

void ConfigureTerminals::OnChangeFont()
{
wxFontData data;
data.SetInitialFont(tempCHOSEN_TERMINAL_FONT);
    
wxFontDialog dialog(NULL, data);
if (dialog.ShowModal() == wxID_OK)
  { wxFontData retData = dialog.GetFontData();
    tempTERMINAL_FONT = tempCHOSEN_TERMINAL_FONT = retData.GetChosenFont();
    fonttext->SetFont(tempCHOSEN_TERMINAL_FONT);    
    fontchanged = true; tempUSE_DEFAULT_TERMINALEM_FONT = false;
  }
}

void ConfigureTerminals::OnDefaultFont()
{
tempTERMINAL_FONT = *wxNORMAL_FONT;//defaultfonttext->GetFont();
tempUSE_DEFAULT_TERMINALEM_FONT = true; fontchanged = (tempUSE_DEFAULT_TERMINALEM_FONT!=USE_DEFAULT_TERMINALEM_FONT);
}

void ConfigureTerminals::OnCancel()
{
InitialiseData(); 
if (subpage==CT_terminals) InsertDataReal();  // Don't do both here: one may still be uninitialised
 else InsertDataEmul();
fontchanged = false;
}

void ConfigureTerminals::OnOK()
{
TERMINAL = tempTERMINAL;
TERMINAL_COMMAND = tempTERMINAL_COMMAND + wxT(" ");           // We've been trimming the terminal ' ' up to now
TOOLS_TERMINAL_COMMAND = tempTOOLS_TERMINAL_COMMAND + wxT(" ");
TERMEM_PROMPTFORMAT = tempTERMEM_PROMPTFORMAT;
if (fontchanged) 
  { TERMINAL_FONT = tempTERMINAL_FONT; CHOSEN_TERMINAL_FONT = tempCHOSEN_TERMINAL_FONT;
    USE_DEFAULT_TERMINALEM_FONT = tempUSE_DEFAULT_TERMINALEM_FONT;
    MyFrame::mainframe->m_tbText->SetFont(TERMINAL_FONT);  // It makes sense to use the same font for the toolbar textctrl too
    MyFrame::mainframe->m_tbText->Refresh();
    fontchanged = false;
  }

PossibleTerminals = tempPossibleTerminals;
PossibleTerminalCommands = tempPossibleTerminalCommands;
PossibleToolsTerminalCommands = tempPossibleToolsTerminalCommands;

Save();
}

//static 
void ConfigureTerminals::LoadTerminalsConfig()
{
wxConfigBase* config = wxConfigBase::Get();  if (config==NULL)  return; 

PossibleTerminals.Clear(); PossibleTerminalCommands.Clear(); PossibleToolsTerminalCommands.Clear();

wxString item = config->Read(wxT("/Misc/Terminal/TERMINAL")); if (!item.IsEmpty())  TERMINAL = item;
item = config->Read(wxT("/Misc/Terminal/TERMEM_PROMPTFORMAT")); if (!item.IsEmpty())  TERMEM_PROMPTFORMAT = item;
size_t count = (size_t)config->Read(wxT("/Misc/Terminal/NoOfEntries"), 0l);
for (size_t n=0; n < count; ++n)
  { item = config->Read(wxT("/Misc/Terminal/")+CreateSubgroupName(n,count));  if (!item.IsEmpty())  PossibleTerminals.Add(item); }

config->Read(wxT("/Misc/Terminal/TERMINAL_FONT/UseSystemDefault"), &USE_DEFAULT_TERMINALEM_FONT, true);
CHOSEN_TERMINAL_FONT.SetPointSize((int)config->Read(wxT("/Misc/Terminal/TERMINAL_FONT/pointsize"), 14l));
CHOSEN_TERMINAL_FONT.SetFamily((wxFontFamily)config->Read(wxT("/Misc/Terminal/TERMINAL_FONT/family"),  (long)wxFONTFAMILY_DEFAULT));
CHOSEN_TERMINAL_FONT.SetStyle((wxFontStyle)config->Read(wxT("/Misc/Terminal/TERMINAL_FONT/style"),  (long)wxFONTSTYLE_NORMAL));
CHOSEN_TERMINAL_FONT.SetWeight(LoadFontWeight("/Misc/Display/TERMINAL_FONT/"));
bool under; config->Read(wxT("/Misc/Terminal/TERMINAL_FONT/underlined"), &under, false); CHOSEN_TERMINAL_FONT.SetUnderlined(under);
wxString face; config->Read(wxT("/Misc/Terminal/TERMINAL_FONT/facename"), &face); CHOSEN_TERMINAL_FONT.SetFaceName(face);

if (USE_DEFAULT_TERMINALEM_FONT || !CHOSEN_TERMINAL_FONT.Ok()) 
      TERMINAL_FONT = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
  else  TERMINAL_FONT = CHOSEN_TERMINAL_FONT;

item = config->Read(wxT("/Misc/TerminalCommand/TERMINAL_COMMAND"));
if (!item.IsEmpty())  TERMINAL_COMMAND = item + wxT(" ");  // wxConfig doesn't cope well with terminal whitespace, & we need to separate the terminal invocation from the command.
count = (size_t)config->Read(wxT("/Misc/TerminalCommand/NoOfEntries"), 0l);
for (size_t n=0; n < count; ++n)
  { item = config->Read(wxT("/Misc/TerminalCommand/")+CreateSubgroupName(n,count));  if (!item.IsEmpty())  PossibleTerminalCommands.Add(item); }
  
item = config->Read(wxT("/Misc/ToolsTerminalCommand/TOOLS_TERMINAL_COMMAND")); if (!item.IsEmpty())  TOOLS_TERMINAL_COMMAND = item + wxT(" ");
count = (size_t)config->Read(wxT("/Misc/ToolsTerminalCommand/NoOfEntries"), 0l);
for (size_t n=0; n < count; ++n)
  { item = config->Read(wxT("/Misc/ToolsTerminalCommand/")+CreateSubgroupName(n,count));  if (!item.IsEmpty())  PossibleToolsTerminalCommands.Add(item); }
}

//static
void ConfigureTerminals::Save(wxConfigBase* config)
{
if (config==NULL)                                             // It'll usually be null, the exception being if coming from ConfigureMisc::OnExportBtn
   config = wxConfigBase::Get();  if (config==NULL)  return; 

config->Write(wxT("/Misc/Terminal/TERMINAL"), TERMINAL);
config->Write(wxT("/Misc/Terminal/TERMEM_PROMPTFORMAT"), TERMEM_PROMPTFORMAT);
size_t count = PossibleTerminals.GetCount();
config->Write(wxT("/Misc/Terminal/NoOfEntries"), (long)count);
for (size_t n=0; n < count; ++n) config->Write(wxT("/Misc/Terminal/")+CreateSubgroupName(n,count), PossibleTerminals.Item(n));

config->Write(wxT("/Misc/Terminal/TERMINAL_FONT/UseSystemDefault"), USE_DEFAULT_TERMINALEM_FONT);
if (!CHOSEN_TERMINAL_FONT.Ok()) CHOSEN_TERMINAL_FONT = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);  // Will be needed on first ever Save
config->Write(wxT("/Misc/Terminal/TERMINAL_FONT/pointsize"), (long)CHOSEN_TERMINAL_FONT.GetPointSize());
config->Write(wxT("/Misc/Terminal/TERMINAL_FONT/family"), (long)CHOSEN_TERMINAL_FONT.GetFamily());
config->Write(wxT("/Misc/Terminal/TERMINAL_FONT/style"), (long)CHOSEN_TERMINAL_FONT.GetStyle());
SaveFontWeight("/Misc/Display/TERMINAL_FONT/", CHOSEN_TERMINAL_FONT.GetWeight());
config->Write(wxT("/Misc/Terminal/TERMINAL_FONT/underlined"), CHOSEN_TERMINAL_FONT.GetUnderlined());
config->Write(wxT("/Misc/Terminal/TERMINAL_FONT/facename"), CHOSEN_TERMINAL_FONT.GetFaceName());

wxString item = TERMINAL_COMMAND; item.Trim();                // Remove the space after the command, as wxConfig won't cope
config->Write(wxT("/Misc/TerminalCommand/TERMINAL_COMMAND"), item);
count = PossibleTerminalCommands.GetCount();
config->Write(wxT("/Misc/TerminalCommand/NoOfEntries"), (long)count);
for (size_t n=0; n < count; ++n) config->Write(wxT("/Misc/TerminalCommand/")+CreateSubgroupName(n,count), PossibleTerminalCommands.Item(n));

item = TOOLS_TERMINAL_COMMAND; item.Trim();
config->Write(wxT("/Misc/ToolsTerminalCommand/TOOLS_TERMINAL_COMMAND"), item);
count = PossibleToolsTerminalCommands.GetCount();
config->Write(wxT("/Misc/ToolsTerminalCommand/NoOfEntries"), (long)count);
for (size_t n=0; n < count; ++n) config->Write(wxT("/Misc/ToolsTerminalCommand/")+CreateSubgroupName(n,count), PossibleToolsTerminalCommands.Item(n));

config->Flush();
}
//-------------------------------------------------------------------------------------------------------------------------------------------

void ConfigureNet::InitialiseData()  // Fill the temp vars with the old data. Used initially and also to revert on Cancel
{
serverschanged = false;
tempSMBPATH = SMBPATH;
tempSHOWMOUNTFPATH  = SHOWMOUNTFPATH;
tempNFS_SERVERS = NFS_SERVERS;
}

void ConfigureNet::InsertData()
{
ServerList = (wxListBox*)page->FindWindow(wxT("ServerList")); ServerList->Clear(); ServerList->Append(tempNFS_SERVERS);
NewServer = (wxTextCtrl*)page->FindWindow(wxT("NewServer"));
Showmount = (wxTextCtrl*)page->FindWindow(wxT("Showmount")); Showmount->SetValue(tempSHOWMOUNTFPATH);
Samba = (wxTextCtrl*)page->FindWindow(wxT("Samba")); Samba->SetValue(tempSMBPATH);
m_nb->StoreApplyBtn(page->FindWindow(wxT("Net_OK")));
}

void ConfigureNet::DeleteServer()
{
int sel = ServerList->GetSelection(); if (sel == wxNOT_FOUND) return;
wxString server = ServerList->GetString(sel); if (server.IsEmpty()) return;

wxMessageDialog ask(MyFrame::mainframe,wxString(_("Delete "))+server+wxT(" ?"), wxString(_("Are you SURE?")), wxYES_NO | wxICON_QUESTION);
if (ask.ShowModal() != wxID_YES) return;

ServerList->Delete(sel); tempNFS_SERVERS.RemoveAt(sel);
serverschanged = true;
}

void ConfigureNet::AddServer()
{
wxString server = NewServer->GetValue(); if (server.IsEmpty()) return;
if (!IsPlausibleIPaddress(server))
  { wxMessageBox(_("That doesn't seem to be a valid ip address")); NewServer->SetValue(wxEmptyString); return; }

if (tempNFS_SERVERS.Index(server) != wxNOT_FOUND)
  { wxMessageBox(_("That server is already on the list")); NewServer->SetValue(wxEmptyString); return; }

ServerList->Append(server); tempNFS_SERVERS.Add(server);
NewServer->SetValue(wxEmptyString); serverschanged = true;
}

void ConfigureNet::OnUpdateUI()
{
    // I'm null-testing the first control, as there were occasional segs when the notebook page changed; probably a race condition somehow
if (page == NULL || Showmount == NULL) return;
tempSHOWMOUNTFPATH = Showmount->GetValue();
tempSMBPATH = Samba->GetValue();

dirty = serverschanged || (SHOWMOUNTFPATH != tempSHOWMOUNTFPATH) || (SMBPATH != tempSMBPATH);

page->FindWindow(wxT("Net_OK"))->Enable(dirty);
page->FindWindow(wxT("Net_CANCEL"))->Enable(dirty);
m_nb->SetPageStatus(dirty);
}

void ConfigureNet::OnOK()
{
SHOWMOUNTFPATH = tempSHOWMOUNTFPATH;
SMBPATH = tempSMBPATH;
NFS_SERVERS = tempNFS_SERVERS;
serverschanged = false;

Save();
}

//static 
void ConfigureNet::LoadNetworkConfig()
{
wxConfigBase* config = wxConfigBase::Get();  if (config==NULL)  return; 

wxString defaultvalue(SHOWMOUNTFPATH); if (defaultvalue.IsEmpty()) defaultvalue = wxT("/usr/sbin/showmount");
config->Read(wxT("/Network/ShowmountFilepath"), &SHOWMOUNTFPATH, defaultvalue);
defaultvalue = wxT("/usr/bin/"); config->Read(wxT("/Network/SMBPATH"), &SMBPATH, defaultvalue);

wxString ip, key(wxT("/Network/IPAddresses/"));
long count = config->Read(key+wxT("Count"), 0l);
for (long n=0; n < count; ++n)
  { config->Read(key + CreateSubgroupName(n, count), &ip);
    if (ip.IsEmpty()) continue;
    if (NFS_SERVERS.Index(ip) == wxNOT_FOUND)  
        NFS_SERVERS.Add(ip);                            // Check for duplication.  If it's OK, add it to the server array
  }
}

void ConfigureNet::Save()
{
wxConfigBase* config = wxConfigBase::Get();  if (config==NULL)  return; 
config->DeleteGroup(wxT("/Network"));

if (!SHOWMOUNTFPATH.IsEmpty())                        // Make sure SHOWMOUNTFPATH is sane
  { if  (SHOWMOUNTFPATH.Last() == wxFILE_SEP_PATH) SHOWMOUNTFPATH = SHOWMOUNTFPATH.BeforeLast(wxFILE_SEP_PATH);
    if (!SHOWMOUNTFPATH.Contains(wxT("showmount"))) SHOWMOUNTFPATH += wxT("/showmount");
    config->Write(wxT("/Network/ShowmountFilepath"), SHOWMOUNTFPATH);
  }

if (!SMBPATH.IsEmpty())                               // Similarly SMBPATH
  { if  (SMBPATH.Last() != wxFILE_SEP_PATH)   SMBPATH += wxFILE_SEP_PATH;
    config->Write(wxT("/Network/SMBPATH"), SMBPATH);
  }

wxString key(wxT("/Network/IPAddresses/"));
size_t count = NFS_SERVERS.GetCount();
config->Write(key+wxT("Count"), (long)count);
for (size_t n=0; n < count; ++n)
  config->Write(key + CreateSubgroupName(n, count), NFS_SERVERS.Item(n));

config->Flush();
}

//-------------------------------------------------------------------------------------------------------------------------------------------

void ConfigureMisc::InitSu(wxNotebookPage* pg)
{
pageSu = pg; 
InsertDataSu(); subpage = CM_su; 

Othersu->Disconnect(wxEVT_COMMAND_RADIOBUTTON_SELECTED, wxCommandEventHandler(ConfigureMisc::OnOtherRadio), NULL, this); // In case of re-entry
MySuChk->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(ConfigureMisc::OnInternalExternalSu), NULL, this);
OtherSuChk->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(ConfigureMisc::OnInternalExternalSu), NULL, this);

Othersu->Connect(wxEVT_COMMAND_RADIOBUTTON_SELECTED, wxCommandEventHandler(ConfigureMisc::OnOtherRadio), NULL, this);
MySuChk->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(ConfigureMisc::OnInternalExternalSu), NULL, this);
OtherSuChk->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(ConfigureMisc::OnInternalExternalSu), NULL, this);
}

void ConfigureMisc::InitialiseData()  // Fill the temp vars with the old data. Used initially and also to revert on Cancel
{
tempMAX_NUMBER_OF_UNDOS = MAX_NUMBER_OF_UNDOS;
tempMAX_DROPDOWN_DISPLAY = MAX_DROPDOWN_DISPLAY;
tempAUTOCLOSE_TIMEOUT = AUTOCLOSE_TIMEOUT;
tempAUTOCLOSE_FAILTIMEOUT = AUTOCLOSE_FAILTIMEOUT;
tempDEFAULT_BRIEFLOGSTATUS_TIME = DEFAULT_BRIEFLOGSTATUS_TIME;
tempWHICH_SU = WHICH_SU; tempWHICH_EXTERNAL_SU = WHICH_EXTERNAL_SU; tempOTHER_SU_COMMAND = OTHER_SU_COMMAND;
tempSTORE_PASSWD = STORE_PASSWD; tempUSE_SUDO = USE_SUDO; tempPASSWD_STORE_TIME = PASSWD_STORE_TIME; tempRENEW_PASSWD_TTL = RENEW_PASSWD_TTL;
tempMETAKEYS_MOVE = METAKEYS_MOVE; tempMETAKEYS_COPY = METAKEYS_COPY;
tempMETAKEYS_HARDLINK = METAKEYS_HARDLINK; tempMETAKEYS_SOFTLINK = METAKEYS_SOFTLINK;
tempLINES_PER_MOUSEWHEEL_SCROLL = LINES_PER_MOUSEWHEEL_SCROLL;
tempDnDXTRIGGERDISTANCE = DnDXTRIGGERDISTANCE; tempDnDYTRIGGERDISTANCE = DnDYTRIGGERDISTANCE;

LoadStatusbarWidths(widths); LoadStatusbarWidths(tempwidths);  // There isn't a global for widths, so load both here
}

void ConfigureMisc::InsertDataUndo()
{
((wxSpinCtrl*)pageUndo->FindWindow(wxT("MAX_NUMBER_OF_UNDOS")))->SetValue(tempMAX_NUMBER_OF_UNDOS);
((wxSpinCtrl*)pageUndo->FindWindow(wxT("MAX_DROPDOWN_DISPLAY")))->SetValue(tempMAX_DROPDOWN_DISPLAY);
m_nb->StoreApplyBtn(pageUndo->FindWindow(wxT("Misc_OK")));
}

void ConfigureMisc::InsertDataBrieftimes()
{
((wxSpinCtrl*)pageBrieftimes->FindWindow(wxT("AUTOCLOSE_TIMEOUT")))->SetValue(tempAUTOCLOSE_TIMEOUT / 1000);
((wxSpinCtrl*)pageBrieftimes->FindWindow(wxT("AUTOCLOSE_FAILTIMEOUT")))->SetValue(tempAUTOCLOSE_FAILTIMEOUT / 1000);
((wxSpinCtrl*)pageBrieftimes->FindWindow(wxT("DEFAULT_BRIEFLOGSTATUS_TIME")))->SetValue(tempDEFAULT_BRIEFLOGSTATUS_TIME);
m_nb->StoreApplyBtn(pageBrieftimes->FindWindow(wxT("Misc_OK")));
}

void ConfigureMisc::InsertDataSu()
{
MySuChk = (wxCheckBox*)pageSu->FindWindow(wxT("MySuChk")); if (!MySuChk) return; // Null-check since this is a recent innovation
MySuChk->SetValue(tempWHICH_SU == mysu);
BackendRadio = (wxRadioBox*)pageSu->FindWindow(wxT("BackendRadio")); BackendRadio->SetSelection(tempUSE_SUDO);
StorePwChk = (wxCheckBox*)pageSu->FindWindow(wxT("StorePwChk")); StorePwChk->SetValue(tempSTORE_PASSWD);
PwTimetoliveSpin = (wxSpinCtrl*)pageSu->FindWindow(wxT("PwTtlSpin")); PwTimetoliveSpin->SetValue(tempPASSWD_STORE_TIME);
RenewTimetoliveChk = (wxCheckBox*)pageSu->FindWindow(wxT("RenewTtlChk")); RenewTimetoliveChk->SetValue(tempRENEW_PASSWD_TTL);

OtherSuChk = (wxCheckBox*)pageSu->FindWindow(wxT("OtherSuChk")); OtherSuChk->SetValue(tempWHICH_SU != mysu);
Ksu = (wxRadioButton*)pageSu->FindWindow(wxT("Radiok"));
Gksu = (wxRadioButton*)pageSu->FindWindow(wxT("Radiogk"));
Gnsu = (wxRadioButton*)pageSu->FindWindow(wxT("Radiognome"));
Othersu = (wxRadioButton*)pageSu->FindWindow(wxT("RadioOther"));
  // Check again for available su-ers: it was done in Configure::FindPrograms(), but new ones might have been added since
KDESU_PRESENT = Configure::TestExistence(wxT("kdesu")); XSU_PRESENT = Configure::TestExistence(wxT("gnomesu")); GKSU_PRESENT = Configure::TestExistence(wxT("gksu"));
Ksu->Enable(KDESU_PRESENT); Gksu->Enable(GKSU_PRESENT); Gnsu->Enable(XSU_PRESENT);
if (!(KDESU_PRESENT || GKSU_PRESENT || XSU_PRESENT))
  { if ((tempWHICH_EXTERNAL_SU != othersu) || tempOTHER_SU_COMMAND.IsEmpty()) 
      WHICH_SU = tempWHICH_SU = mysu; // If the standard external su's aren't available, and there's no valid user-defined one, use 4Pane's
  }
switch(tempWHICH_EXTERNAL_SU)         // Protect against stale values
  { case gksu:    if (!GKSU_PRESENT) { WHICH_EXTERNAL_SU = tempWHICH_EXTERNAL_SU = dunno; tempWHICH_SU = WHICH_SU = mysu; MySuChk->SetValue(true); OtherSuChk->SetValue(false); break; }
                  Gksu->SetValue(true); break;
    case gnomesu: if (!XSU_PRESENT) { WHICH_EXTERNAL_SU = tempWHICH_EXTERNAL_SU = dunno; tempWHICH_SU = WHICH_SU = mysu; MySuChk->SetValue(true); OtherSuChk->SetValue(false); break; }
                  Gnsu->SetValue(true); break;
    case kdesu:   if (!KDESU_PRESENT) { WHICH_EXTERNAL_SU = tempWHICH_EXTERNAL_SU = dunno; tempWHICH_SU = WHICH_SU = mysu; MySuChk->SetValue(true); OtherSuChk->SetValue(false); break; }
                  Ksu->SetValue(true); break;
    case othersu: Othersu->SetValue(true); break;
     default:     MySuChk->SetValue(true); OtherSuChk->SetValue(false);
  }
m_nb->StoreApplyBtn(pageSu->FindWindow(wxT("Misc_OK")));
}

void ConfigureMisc::OnInternalExternalSu(wxCommandEvent& event) // The user switched between using the built-in or external su
{
bool check_internal = 
    ((event.GetId() == XRCID("MySuChk")) && event.IsChecked()) ||
    ((event.GetId() == XRCID("OtherSuChk")) && !event.IsChecked());

MySuChk->SetValue(check_internal); OtherSuChk->SetValue(!check_internal);
}

void ConfigureMisc::OnOtherRadio(wxCommandEvent& WXUNUSED(event)) // Gets a different gui su command from the user
{
wxString message(_("Please enter the command to use, including any required options"));
tempOTHER_SU_COMMAND = wxGetTextFromUser(message, _("Command for a different gui su program"), tempOTHER_SU_COMMAND, pageSu);

if (tempOTHER_SU_COMMAND.IsEmpty())     // If an empty command is entered, change the button-group selection to mysu
  { tempWHICH_EXTERNAL_SU = dunno; tempWHICH_SU = mysu; MySuChk->SetValue(true); OtherSuChk->SetValue(false); }
}

void ConfigureMisc::OnConfigureOpen()
{
ConfigureFileOpenDlg dlg;
wxXmlResource::Get()->LoadDialog(&dlg, pageOther, wxT("ConfigureFileOpenDlg"));
dlg.Init();

if (dlg.ShowModal() == wxID_OK)
  { USE_SYSTEM_OPEN = dlg.GetUseSystem();
    USE_BUILTIN_OPEN = dlg.GetUseBuiltin();
    PREFER_SYSTEM_OPEN = dlg.GetPreferSystem();

    wxConfigBase::Get()->Write(wxT("/Misc/FileOpen/UseSystem"), USE_SYSTEM_OPEN);
    wxConfigBase::Get()->Write(wxT("/Misc/FileOpen/UseBuiltin"), USE_BUILTIN_OPEN);
    wxConfigBase::Get()->Write(wxT("/Misc/FileOpen/PreferSystem"), PREFER_SYSTEM_OPEN);
  }
}

void ConfigureMisc::OnConfigureDnD()
{
int MOVE=0,COPY=0,HARDLINK=0,SOFTLINK=0; bool dup;
const wxChar* checks[] = { wxT("MS"), wxT("MC"), wxT("MA"), wxT("CS"), wxT("CC"), wxT("CA"), wxT("HS"), wxT("HC"), wxT("HA"), wxT("SS"), wxT("SC"), wxT("SA")  };
int metakeys[4] = { tempMETAKEYS_MOVE, tempMETAKEYS_COPY, tempMETAKEYS_HARDLINK, tempMETAKEYS_SOFTLINK };
int mask[3] = { wxACCEL_SHIFT, wxACCEL_CTRL, wxACCEL_ALT };

wxDialog dlg;
wxXmlResource::Get()->LoadDialog(&dlg, pageOther, wxT("ConfigDnDDlg"));

for (size_t n=0; n<12; ++n)
    ((wxCheckBox*)dlg.FindWindow(checks[n]))->SetValue(metakeys[n/3] & mask[n%3]);

((wxSpinCtrl*)dlg.FindWindow(wxT("Horiz")))->SetValue(tempDnDXTRIGGERDISTANCE);
((wxSpinCtrl*)dlg.FindWindow(wxT("Vertical")))->SetValue(tempDnDYTRIGGERDISTANCE);
((wxSpinCtrl*)dlg.FindWindow(wxT("LINES_PER_MOUSEWHEEL_SCROLL")))->SetValue(tempLINES_PER_MOUSEWHEEL_SCROLL);

HelpContext = HCdnd;
do
   { if (dlg.ShowModal() != wxID_OK) return;
    for (size_t n=0; n<3; ++n)
      if (((wxCheckBox*)dlg.FindWindow(checks[n]))->GetValue()) MOVE |= mask[n];
    for (size_t n=3; n<6; ++n)
      if (((wxCheckBox*)dlg.FindWindow(checks[n]))->GetValue()) COPY |= mask[n%3];
    for (size_t n=6; n<9; ++n)
      if (((wxCheckBox*)dlg.FindWindow(checks[n]))->GetValue()) HARDLINK |= mask[n%3];
    for (size_t n=9; n<12; ++n)
      if (((wxCheckBox*)dlg.FindWindow(checks[n]))->GetValue()) SOFTLINK |= mask[n%3];
    dup = (MOVE==COPY) || (MOVE==HARDLINK) || (MOVE==SOFTLINK) || (COPY==HARDLINK) || (COPY==SOFTLINK) || (HARDLINK==SOFTLINK);
    if (dup && wxMessageBox(_("Each metakey pattern must be unique. Try again?"), _("Oops"), wxYES_NO) != wxYES)  return;
  }
  while (dup);

tempMETAKEYS_MOVE = MOVE; tempMETAKEYS_COPY = COPY; tempMETAKEYS_HARDLINK = HARDLINK; tempMETAKEYS_SOFTLINK = SOFTLINK;
tempDnDXTRIGGERDISTANCE = ((wxSpinCtrl*)dlg.FindWindow(wxT("Horiz")))->GetValue();
tempDnDYTRIGGERDISTANCE = ((wxSpinCtrl*)dlg.FindWindow(wxT("Vertical")))->GetValue();
tempLINES_PER_MOUSEWHEEL_SCROLL = ((wxSpinCtrl*)dlg.FindWindow(wxT("LINES_PER_MOUSEWHEEL_SCROLL")))->GetValue();
m_nb->StoreApplyBtn(pageOther->FindWindow(wxT("Misc_OK")));
}

void ConfigureMisc::OnConfigureStatusbar()
{
const wxChar* spinnames[] = { wxT("Menu"), wxT("Success"), wxT("Filepath"), wxT("Filters") };
HelpContext = HCconfigMiscStatbar;
wxDialog dlg;
wxXmlResource::Get()->LoadDialog(&dlg, pageOther, wxT("ConfigStatusDlg"));

for (size_t n=0; n < 4; ++n)
    ((wxSpinCtrl*)dlg.FindWindow(spinnames[n]))->SetValue(-tempwidths[n]);    // View as +ve numbers, though they're used as negatives

if (dlg.ShowModal() == wxID_OK)
  for (size_t n=0; n < 4; ++n)
    tempwidths[n] = -((wxSpinCtrl*)dlg.FindWindow(spinnames[n]))->GetValue();  // See above

HelpContext = HCconfigMisc;
m_nb->StoreApplyBtn(pageOther->FindWindow(wxT("Misc_OK")));
}

void ConfigureMisc::OnExportBtn(ConfigureNotebook* note)  // Intended for packagers, to export distro-specific bits of the config
{
HelpContext = HCconfigMiscExport;
wxDialog dlg;
wxXmlResource::Get()->LoadDialog(&dlg, pageOther, wxT("ConfigExportDlg"));
int ans = dlg.ShowModal();
HelpContext = HCconfigMisc;
if (ans != wxID_OK) return;

bool misc = ((wxCheckBox*)dlg.FindWindow(wxT("CheckMisc")))->IsChecked(),
    edit = ((wxCheckBox*)dlg.FindWindow(wxT("CheckEditors")))->IsChecked(),
    files = ((wxCheckBox*)dlg.FindWindow(wxT("CheckFileAss")))->IsChecked(),
    term = ((wxCheckBox*)dlg.FindWindow(wxT("CheckTerminals")))->IsChecked(),
    tool = ((wxCheckBox*)dlg.FindWindow(wxT("CheckTools")))->IsChecked();

if (!(misc||edit||files||term||tool))
  { wxMessageBox(_("You chose not to export any data type! Aborting."), wxT(":/")); return; }

wxString conf(wxGetHomeDir() + wxT("/4Pane.conf"));
wxConfigBase* Export = new wxConfig(wxEmptyString, wxEmptyString, conf);

if (misc)
  { DeviceManager::SaveMiscData(Export);        // ConfigureDevices stuff
    DeviceBitmapButton::SaveDefaults(Export);
    DevicesAdvancedPanel::SaveConfig(Export);
  }

if (edit)
  { EditorsDialog edlg;                          // The easiest way to do Editors is to load the configure dialog, but not ShowModal()
    wxXmlResource::Get()->LoadDialog(&edlg, pageOther, wxT("ConfigEditorsEtcDlg")); edlg.Init((DeviceManager*)pageOther);
    edlg.Save(Export);
  }

if (files)
  { wxString home = wxGetHomeDir();              // just because we need a file to pass to...
    FiletypeManager manager(home);              // File Associations, Extensions
    manager.LoadFiletypes(); manager.LoadExtdata();
    manager.SaveFiletypes(Export); manager.SaveExtdata(Export);
  }

if (term)
  ConfigureTerminals::Save(Export);             // Terminals

if (tool)
  note->LMT->Export(Export);                    // UserDefined Tools

Export->Flush();
delete Export;
}
void ConfigureMisc::OnUpdateUI()
{
if (subpage==CM_undo)
  { if (pageUndo == NULL) return;
    wxSpinCtrl* spin = (wxSpinCtrl*)pageUndo->FindWindow(wxT("MAX_NUMBER_OF_UNDOS")); if (spin==NULL) return;
    tempMAX_NUMBER_OF_UNDOS = spin->GetValue();
    tempMAX_DROPDOWN_DISPLAY = ((wxSpinCtrl*)pageUndo->FindWindow(wxT("MAX_DROPDOWN_DISPLAY")))->GetValue();
    dirty[subpage] = (MAX_NUMBER_OF_UNDOS!=tempMAX_NUMBER_OF_UNDOS) || (tempMAX_DROPDOWN_DISPLAY!=MAX_DROPDOWN_DISPLAY);
    pageUndo->FindWindow(wxT("Misc_OK"))->Enable(dirty[subpage]);
    pageUndo->FindWindow(wxT("Misc_CANCEL"))->Enable(dirty[subpage]);
    m_nb->SetPageStatus(dirty[subpage]);
  }

if (subpage==CM_times)
  { if (pageBrieftimes == NULL) return;
    tempAUTOCLOSE_TIMEOUT = ((wxSpinCtrl*)pageBrieftimes->FindWindow(wxT("AUTOCLOSE_TIMEOUT")))->GetValue() * 1000;
    tempAUTOCLOSE_FAILTIMEOUT = ((wxSpinCtrl*)pageBrieftimes->FindWindow(wxT("AUTOCLOSE_FAILTIMEOUT")))->GetValue() * 1000;
    tempDEFAULT_BRIEFLOGSTATUS_TIME = ((wxSpinCtrl*)pageBrieftimes->FindWindow(wxT("DEFAULT_BRIEFLOGSTATUS_TIME")))->GetValue();

    dirty[subpage] = (tempAUTOCLOSE_TIMEOUT!=AUTOCLOSE_TIMEOUT) || (tempAUTOCLOSE_FAILTIMEOUT!=AUTOCLOSE_FAILTIMEOUT)|| (tempDEFAULT_BRIEFLOGSTATUS_TIME!=DEFAULT_BRIEFLOGSTATUS_TIME);
    pageBrieftimes->FindWindow(wxT("Misc_OK"))->Enable(dirty[subpage]);
    pageBrieftimes->FindWindow(wxT("Misc_CANCEL"))->Enable(dirty[subpage]);
    m_nb->SetPageStatus(dirty[subpage]);
  }

if (subpage==CM_su)
  { if (pageSu == NULL) return;
    if (MySuChk->IsChecked())
      { tempWHICH_SU = mysu;
        BackendRadio->Enable(); StorePwChk->Enable(); PwTimetoliveSpin->Enable(StorePwChk->IsChecked()); RenewTimetoliveChk->Enable(StorePwChk->IsChecked());
        Ksu->Disable(); Gksu->Disable(); Gnsu->Disable(); Othersu->Disable(); 
      }
     else
      { tempWHICH_EXTERNAL_SU = tempWHICH_SU = (sutype)((kdesu * (Ksu->IsEnabled() && Ksu->GetValue())) + (gksu * (Gksu->IsEnabled() && Gksu->GetValue()))
                        + (gnomesu * (Gnsu->IsEnabled() && Gnsu->GetValue())) + (othersu * (Othersu->IsEnabled() && Othersu->GetValue())));
        BackendRadio->Disable(); StorePwChk->Disable(); RenewTimetoliveChk->Disable(); PwTimetoliveSpin->Disable();
        Ksu->Enable(KDESU_PRESENT); Gksu->Enable(GKSU_PRESENT); Gnsu->Enable(XSU_PRESENT); Othersu->Enable();
      }

    tempUSE_SUDO = BackendRadio->GetSelection(); tempSTORE_PASSWD = StorePwChk->IsChecked();
    tempPASSWD_STORE_TIME = PwTimetoliveSpin->GetValue(); tempRENEW_PASSWD_TTL = RenewTimetoliveChk->IsChecked();

    dirty[subpage] = (tempWHICH_EXTERNAL_SU != WHICH_EXTERNAL_SU) || (tempWHICH_SU != WHICH_SU) || (tempOTHER_SU_COMMAND != OTHER_SU_COMMAND)
          || (tempUSE_SUDO != USE_SUDO) || (tempSTORE_PASSWD != STORE_PASSWD) || (tempPASSWD_STORE_TIME != PASSWD_STORE_TIME) || (tempRENEW_PASSWD_TTL != RENEW_PASSWD_TTL);
    pageSu->FindWindow(wxT("Misc_OK"))->Enable(dirty[subpage]);
    pageSu->FindWindow(wxT("Misc_CANCEL"))->Enable(dirty[subpage]);
    m_nb->SetPageStatus(dirty[subpage]);
  }

if (subpage==CM_other)
  { if (pageOther == NULL) return;
    dirty[subpage] =  (tempLINES_PER_MOUSEWHEEL_SCROLL!=LINES_PER_MOUSEWHEEL_SCROLL)
      || tempMETAKEYS_MOVE!=METAKEYS_MOVE || tempMETAKEYS_COPY!=METAKEYS_COPY || tempMETAKEYS_HARDLINK!=METAKEYS_HARDLINK
      || tempMETAKEYS_SOFTLINK!=METAKEYS_SOFTLINK || tempDnDXTRIGGERDISTANCE!=DnDXTRIGGERDISTANCE || tempDnDYTRIGGERDISTANCE!=DnDYTRIGGERDISTANCE
      || tempwidths[0]!=widths[0] || tempwidths[1]!=widths[1] || tempwidths[2]!=widths[2] || tempwidths[3]!=widths[3];
    
    pageOther->FindWindow(wxT("Misc_OK"))->Enable(dirty[subpage]);
    pageOther->FindWindow(wxT("Misc_CANCEL"))->Enable(dirty[subpage]);
    m_nb->SetPageStatus(dirty[subpage]);
  }
}

void ConfigureMisc::OnOK()
{
MAX_NUMBER_OF_UNDOS = tempMAX_NUMBER_OF_UNDOS;
MAX_DROPDOWN_DISPLAY = tempMAX_DROPDOWN_DISPLAY;
AUTOCLOSE_TIMEOUT = tempAUTOCLOSE_TIMEOUT;
AUTOCLOSE_FAILTIMEOUT = tempAUTOCLOSE_FAILTIMEOUT;
DEFAULT_BRIEFLOGSTATUS_TIME = tempDEFAULT_BRIEFLOGSTATUS_TIME;
LINES_PER_MOUSEWHEEL_SCROLL = tempLINES_PER_MOUSEWHEEL_SCROLL;
WHICH_SU = tempWHICH_SU;
WHICH_EXTERNAL_SU = tempWHICH_EXTERNAL_SU;
// Ensure the su command ends in whitespace. It might not save properly, but in case the command is to be used immediately...
if (!tempOTHER_SU_COMMAND.IsEmpty() && tempOTHER_SU_COMMAND.Right(0) != wxT(" "))  tempOTHER_SU_COMMAND << wxT(" ");
OTHER_SU_COMMAND = tempOTHER_SU_COMMAND;
USE_SUDO = tempUSE_SUDO;
STORE_PASSWD = tempSTORE_PASSWD;
PASSWD_STORE_TIME = tempPASSWD_STORE_TIME;
RENEW_PASSWD_TTL = tempRENEW_PASSWD_TTL;

METAKEYS_MOVE = tempMETAKEYS_MOVE; METAKEYS_COPY = tempMETAKEYS_COPY;
METAKEYS_HARDLINK = tempMETAKEYS_HARDLINK; METAKEYS_SOFTLINK = tempMETAKEYS_SOFTLINK;
DnDXTRIGGERDISTANCE = tempDnDXTRIGGERDISTANCE; DnDYTRIGGERDISTANCE = tempDnDYTRIGGERDISTANCE;
widths[0] = tempwidths[0]; widths[1] = tempwidths[1]; widths[2] = tempwidths[2]; widths[3] = tempwidths[3];

Save();
}

//static 
void ConfigureMisc::LoadMiscConfig()
{
wxConfigBase* config = wxConfigBase::Get();  if (config==NULL)  return; 

MAX_NUMBER_OF_UNDOS = (size_t)config->Read(wxT("/Misc/Misc/MAX_NUMBER_OF_UNDOS"), 10000l);
MAX_DROPDOWN_DISPLAY = (size_t)config->Read(wxT("/Misc/Misc/MAX_DROPDOWN_DISPLAY"), 15l);
AUTOCLOSE_TIMEOUT = config->Read(wxT("/Misc/Misc/AUTOCLOSE_TIMEOUT"), 2000l);
AUTOCLOSE_FAILTIMEOUT = config->Read(wxT("/Misc/Misc/AUTOCLOSE_FAILTIMEOUT"), 5000l);
DEFAULT_BRIEFLOGSTATUS_TIME = config->Read(wxT("/Misc/Misc/DEFAULT_BRIEFLOGSTATUS_TIME"), 10l);
LINES_PER_MOUSEWHEEL_SCROLL = config->Read(wxT("/Misc/Misc/LINES_PER_MOUSEWHEEL_SCROLL"), 5l);

bool use_builtin_su;
config->Read(wxT("/Misc/Use_Builtin_Su"), &use_builtin_su, 1);
WHICH_EXTERNAL_SU = (sutype)config->Read(wxT("/Misc/Which_Su"), 0l);  // Still stored as 'Which_Su' for backward-compatibility reasons
WHICH_SU = use_builtin_su ? mysu : WHICH_EXTERNAL_SU;
wxString defaultvalue; config->Read(wxT("/Misc/OtherSuCommand"), &OTHER_SU_COMMAND, defaultvalue);
if (!OTHER_SU_COMMAND.IsEmpty() && OTHER_SU_COMMAND.Right(0) != wxT(" ")) 
  OTHER_SU_COMMAND << wxT(" "); // Ensure the command ends in whitespace
if (!config->Exists(wxT("/Misc/Use_sudo"))) // For a while we'd better check explicitly, as buntu updaters will otherwise be su.ing
  { double Version; enum distro Distro = Configure::DeduceDistro(Version);
    USE_SUDO = ((Distro == ubuntu) || (Distro == mint));
  }
 else config->Read(wxT("/Misc/Use_sudo"), &USE_SUDO, 0);
config->Read(wxT("/Misc/Store_Password"), &STORE_PASSWD, 0);
PASSWD_STORE_TIME = (size_t)config->Read(wxT("/Misc/Password_Store_Time"), 15);
config->Read(wxT("/Misc/Renew_Password_TTL"), &RENEW_PASSWD_TTL, 0);

METAKEYS_MOVE = (int)config->Read(wxT("/Misc/Misc/METAKEYS_MOVE"), 0l);
METAKEYS_COPY = (int)config->Read(wxT("/Misc/Misc/METAKEYS_COPY"), 2l);
METAKEYS_HARDLINK = (int)config->Read(wxT("/Misc/Misc/METAKEYS_HARDLINK"), 6l);
METAKEYS_SOFTLINK = (int)config->Read(wxT("/Misc/Misc/METAKEYS_SOFTLINK"), 5l);
DnDXTRIGGERDISTANCE = (int)config->Read(wxT("/Misc/Misc/DnDXTRIGGERDISTANCE"), 10l);
DnDYTRIGGERDISTANCE = (int)config->Read(wxT("/Misc/Misc/DnDYTRIGGERDISTANCE"), 15l);

config->Read(wxT("/Misc/FileOpen/UseSystem"), &USE_SYSTEM_OPEN, true);
config->Read(wxT("/Misc/FileOpen/UseBuiltin"), &USE_BUILTIN_OPEN, true);
config->Read(wxT("/Misc/FileOpen/PreferSystem"), &PREFER_SYSTEM_OPEN, true);
}

//static 
void ConfigureMisc::LoadStatusbarWidths(int* widths)
{ // Load numbers and negate them: they're stored as positive numbers, but used as negative proportions
widths[0] = - (int)wxConfigBase::Get()->Read(wxT("/Misc/Misc/StatusbarWidth0"), (long)DEFAULT_STATUSBAR_WIDTHS[0]);
widths[1] = - (int)wxConfigBase::Get()->Read(wxT("/Misc/Misc/StatusbarWidth1"), (long)DEFAULT_STATUSBAR_WIDTHS[1]);
widths[2] = - (int)wxConfigBase::Get()->Read(wxT("/Misc/Misc/StatusbarWidth2"), (long)DEFAULT_STATUSBAR_WIDTHS[2]);
widths[3] = - (int)wxConfigBase::Get()->Read(wxT("/Misc/Misc/StatusbarWidth3"), (long)DEFAULT_STATUSBAR_WIDTHS[3]);
}

void ConfigureMisc::Save()
{
wxConfigBase* config = wxConfigBase::Get();  if (config==NULL)  return; 

config->Write(wxT("/Misc/Misc/MAX_NUMBER_OF_UNDOS"), (long)MAX_NUMBER_OF_UNDOS);
config->Write(wxT("/Misc/Misc/MAX_DROPDOWN_DISPLAY"), (long)MAX_DROPDOWN_DISPLAY);
config->Write(wxT("/Misc/Misc/AUTOCLOSE_TIMEOUT"), (long)AUTOCLOSE_TIMEOUT);
config->Write(wxT("/Misc/Misc/AUTOCLOSE_FAILTIMEOUT"), (long)AUTOCLOSE_FAILTIMEOUT);
config->Write(wxT("/Misc/Misc/DEFAULT_BRIEFLOGSTATUS_TIME"), (long)DEFAULT_BRIEFLOGSTATUS_TIME);
config->Write(wxT("/Misc/Misc/LINES_PER_MOUSEWHEEL_SCROLL"), (long)LINES_PER_MOUSEWHEEL_SCROLL);
config->Write(wxT("/Misc/Use_Builtin_Su"), WHICH_SU == mysu);
config->Write(wxT("/Misc/Which_Su"), (long)WHICH_EXTERNAL_SU);  // Still stored as 'Which_Su' for backward-compatibility reasons
config->Write(wxT("/Misc/OtherSuCommand"), OTHER_SU_COMMAND);
config->Write(wxT("/Misc/Use_sudo"), USE_SUDO);
config->Write(wxT("/Misc/Store_Password"), STORE_PASSWD);
config->Write(wxT("/Misc/Password_Store_Time"), (long)PASSWD_STORE_TIME);
config->Write(wxT("/Misc/Renew_Password_TTL"), RENEW_PASSWD_TTL);

config->Write(wxT("/Misc/Misc/METAKEYS_MOVE"), (long)METAKEYS_MOVE);
config->Write(wxT("/Misc/Misc/METAKEYS_COPY"), (long)METAKEYS_COPY);
config->Write(wxT("/Misc/Misc/METAKEYS_HARDLINK"), (long)METAKEYS_HARDLINK);
config->Write(wxT("/Misc/Misc/METAKEYS_SOFTLINK"), (long)METAKEYS_SOFTLINK);
config->Write(wxT("/Misc/Misc/DnDXTRIGGERDISTANCE"), (long)DnDXTRIGGERDISTANCE);
config->Write(wxT("/Misc/Misc/DnDYTRIGGERDISTANCE"), (long)DnDYTRIGGERDISTANCE);

config->Write(wxT("/Misc/Misc/StatusbarWidth0"), (long)-widths[0]);  // Save as positive numbers: widths are used as negative proportions, but easier to store as +ves
config->Write(wxT("/Misc/Misc/StatusbarWidth1"), (long)-widths[1]);
config->Write(wxT("/Misc/Misc/StatusbarWidth2"), (long)-widths[2]);
config->Write(wxT("/Misc/Misc/StatusbarWidth3"), (long)-widths[3]);

config->Flush();
}
//-------------------------------------------------------------------------------------------------------------------------------------------

void BackgroundColoursDlg::Init(wxColour c0, wxColour c1, bool single)
{
Col0 = c0 ; Col1 = c1;
IdenticalColsChk = (wxCheckBox*)FindWindow(XRCID("IdenticalColsChk"));
if (IdenticalColsChk) IdenticalColsChk->SetValue(single);

DisplayCols();
}

void BackgroundColoursDlg::DisplayCols()
{
FindWindow(wxT("Col0Text"))->SetBackgroundColour(Col0);
FindWindow(wxT("Col1Text"))->SetBackgroundColour(Col1);
dirty = false;
}

void BackgroundColoursDlg::OnChangeButton(wxCommandEvent& event)
{
wxColourData data;
for (int i = 0; i < 16; i++)
  { wxColour colour(i*16, i*16, i*16); data.SetCustomColour(i, colour); }
if (Col0 != wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW))  // If there are already custom choices, provide them in the dialog
  data.SetCustomColour(0, Col0);
if (Col1 != wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW))
  data.SetCustomColour(1, Col1);

wxColourDialog dialog(this, &data);
if (dialog.ShowModal() == wxID_OK)
  { wxColourData retData = dialog.GetColourData();
    wxColour col = retData.GetColour();
    if (event.GetId() == XRCID("Colour0Btn"))
      { if (Col0 != col)
          { Col0 = col; dirty = true; }
      }
      else
      { if (Col1 != col)
          { Col1 = col; dirty = true; }
      }
    if (dirty) DisplayCols();
  }
}

void BackgroundColoursDlg::OnCheckboxUpdateUI(wxUpdateUIEvent& event)
{
wxCheckBox* chk = (wxCheckBox*)event.GetEventObject();

FindWindow(wxT("Col1Statictext"))->Enable(!chk->IsChecked());
FindWindow(wxT("Col1Text"))->Enable(!chk->IsChecked());
FindWindow(wxT("Colour1Btn"))->Enable(!chk->IsChecked());
}

BEGIN_EVENT_TABLE(BackgroundColoursDlg, wxDialog)
  EVT_BUTTON(XRCID("Colour0Btn"), BackgroundColoursDlg::OnChangeButton)
  EVT_BUTTON(XRCID("Colour1Btn"), BackgroundColoursDlg::OnChangeButton)
  EVT_UPDATE_UI(XRCID("IdenticalColsChk"), BackgroundColoursDlg::OnCheckboxUpdateUI)
END_EVENT_TABLE()
//-------------------------------------------------------------------------------------------------------------------------------------------

void ChooseStripesDlg::DisplayCols()
{
FindWindow(wxT("Col0Text"))->SetBackgroundColour(Col0);
FindWindow(wxT("Col1Text"))->SetBackgroundColour(Col1);
dirty = false;
}

void ChooseStripesDlg::OnChangeButton(wxCommandEvent& event)
{
wxColourData data;
for (int i = 0; i < 16; i++)
  { wxColour colour(i*16, i*16, i*16); data.SetCustomColour(i, colour); }
if (Col0 != wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW))  // If there are already custom choices, provide them in the dialog
  data.SetCustomColour(0, Col0);
if (Col1 != wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW))
  data.SetCustomColour(1, Col1);

wxColourDialog dialog(this, &data);
if (dialog.ShowModal() == wxID_OK)
  { wxColourData retData = dialog.GetColourData();
    wxColour col = retData.GetColour();
    if (event.GetId() == XRCID("Colour0Btn"))
      { if (Col0 != col)
          { Col0 = col; dirty = true; }
      }
      else
      { if (Col1 != col)
          { Col1 = col; dirty = true; }
      }
    if (dirty) DisplayCols();
  }
}

BEGIN_EVENT_TABLE(ChooseStripesDlg, wxDialog)
  EVT_BUTTON(XRCID("Colour0Btn"), ChooseStripesDlg::OnChangeButton)
  EVT_BUTTON(XRCID("Colour1Btn"), ChooseStripesDlg::OnChangeButton)
END_EVENT_TABLE()

//-------------------------------------------------------------------------------------------------------------------------------------------

void PaneHighlightDlg::Init(int dv, int fv)
{
wxPanel* BackgroundPanel = (wxPanel*)FindWindow(wxT("BackgroundPanel"));
if (BackgroundPanel)
  { wxColour pcol = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE);
    bool isdark = (pcol.Red() + pcol.Green() + pcol.Blue()) < 382;
    int offset = isdark ? -30 : 30;
    BackgroundPanel->SetBackgroundColour(wxGetApp().AdjustColourFromOffset(pcol, offset));
  }

dvspin = (wxSpinCtrl*)FindWindow(wxT("DirviewSpin"));
fvspin = (wxSpinCtrl*)FindWindow(wxT("FileviewSpin"));
dvspin->SetValue(dv);
fvspin->SetValue(fv);

dvpanel = (wxPanel*)FindWindow(wxT("DirviewColour"));
dvpanel->SetBackgroundColour(wxGetApp().AdjustColourFromOffset(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE), dv));
fvpanel = (wxPanel*)FindWindow(wxT("FileviewColour"));
fvpanel->SetBackgroundColour(wxGetApp().AdjustColourFromOffset(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE), fv));
}

void PaneHighlightDlg::OnSpin(wxSpinEvent& event)
{
wxColour bg = wxGetApp().AdjustColourFromOffset(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE), event.GetPosition());
if (event.GetId() == XRCID("DirviewSpin"))
  dvpanel->SetBackgroundColour(bg);
 else
  fvpanel->SetBackgroundColour(bg);
}

BEGIN_EVENT_TABLE(PaneHighlightDlg, wxDialog)
  EVT_SPINCTRL(wxID_ANY,PaneHighlightDlg::OnSpin)
END_EVENT_TABLE()

//-------------------------------------------------------------------------------------------------------------------------------------------

void ToolbarIconsDialog::Init(ArrayofDirCtrlTBStructs* DCTBarray)
{
DirCtrlTBinfoarray = DCTBarray;

wxConfigBase* config = wxConfigBase::Get(); if (config==NULL)  return;
long NoOfKnownIcons = config->Read(wxT("/Misc/DirCtrlTB/Icons/NoOfKnownIcons"), 0l);
for (long n=0; n < NoOfKnownIcons; ++n)
  { wxString Icon;
    config->Read(wxT("/Misc/DirCtrlTB/Icons/")+CreateSubgroupName(n,NoOfKnownIcons)+wxT("/Icon"), &Icon); Icons.Add(Icon);
    long Icontype = config->Read(wxT("/Misc/DirCtrlTB/Icons/")+CreateSubgroupName(n,NoOfKnownIcons)+wxT("/Icontype"), -1l); Icontypes.Add(Icontype);
  }

list = (wxListBox*)FindWindow(wxT("List"));
}

void ToolbarIconsDialog::OnInitDialog(wxInitDialogEvent& WXUNUSED(event))
{
FillListbox();
}
//static
void ToolbarIconsDialog::LoadDirCtrlTBinfo(ArrayofDirCtrlTBStructs* DirCtrlTBinfoarray)  // Load info on the desired extra buttons to load to each Dirview mini-toolbar
{
wxConfigBase* config = wxConfigBase::Get(); if (config==NULL)   return;

config->SetPath(wxT("/Misc/DirCtrlTB/Buttons/"));
size_t count = config->GetNumberOfGroups();           // Count the subgroups
long NoOfKnownIcons = config->Read(wxT("/Misc/DirCtrlTB/Icons/NoOfKnownIcons"), 0l);

DirCtrlTBinfoarray->Clear();

wxString key, histprefix(wxT("button_"));  
for (size_t n=0; n < count; ++n)                    // Create the key for every item, from button_a to button_('a'+count-1)
  { wxString bitmaplocation, label, tooltip, destination; int iconNo;
    key.Printf(wxT("%c"), wxT('a') + (wxChar)n);     // Make key hold a, b, c etc
    config->Read(histprefix+key+wxT("/destination"), &destination);  if (destination.IsEmpty())   continue;
    iconNo = (int)config->Read(histprefix+key+wxT("/iconNo"), -1l); if (iconNo == -1 || iconNo >=NoOfKnownIcons) continue;
    config->Read(wxT("/Misc/DirCtrlTB/Icons/")+CreateSubgroupName(iconNo,NoOfKnownIcons)+wxT("/Icon"), &bitmaplocation);
    long Icontype = config->Read(wxT("/Misc/DirCtrlTB/Icons/")+CreateSubgroupName(iconNo,NoOfKnownIcons)+wxT("/Icontype"), -1l);
    config->Read(histprefix+key+wxT("/label"), &label);
    config->Read(histprefix+key+wxT("/tooltip"), &tooltip);
        
    DirCtrlTBStruct* tbs = new DirCtrlTBStruct;       // Make a new struct, & store the data in it
    wxString path; if (bitmaplocation.Left(1) != wxFILE_SEP_PATH) path = BITMAPSDIR;
    tbs->bitmaplocation = path + bitmaplocation;
    tbs->label = label;
    tbs->tooltip = tooltip;
    tbs->iconNo = iconNo;
    tbs->icontype = (wxBitmapType)Icontype;
    tbs->destination = destination;
    DirCtrlTBinfoarray->Add(tbs);                   // Store it in the struct array
  }

config->SetPath(wxT("/"));  
}

void ToolbarIconsDialog::Save()
{
wxConfigBase* config = wxConfigBase::Get(); if (config==NULL)  return;

config->DeleteGroup(wxT("/Misc/DirCtrlTB/Buttons"));// Delete current info
config->SetPath(wxT("/Misc/DirCtrlTB/Buttons/"));

size_t count = DirCtrlTBinfoarray->GetCount();        // Count the entries to be saved

wxString key, histprefix(wxT("button_"));  
for (size_t n=0; n < count; ++n)                    // Create the key for every item, from button_a to button_('a'+count-1)
  { key.Printf(wxT("%c"), wxT('a') + (wxChar)n);    // Make key hold a, b, c etc
    config->Write(histprefix+key+wxT("/IconNo"), DirCtrlTBinfoarray->Item(n)->iconNo);
    config->Write(histprefix+key+wxT("/label"), DirCtrlTBinfoarray->Item(n)->label);
    config->Write(histprefix+key+wxT("/tooltip"), DirCtrlTBinfoarray->Item(n)->tooltip);
    config->Write(histprefix+key+wxT("/destination"), DirCtrlTBinfoarray->Item(n)->destination);
  }


config->Flush();
config->SetPath(wxT("/"));
}

void ToolbarIconsDialog::SaveIcons()
{
size_t NoOfKnownIcons = Icons.GetCount(); if (!NoOfKnownIcons) return;
wxConfigBase* config = wxConfigBase::Get(); if (config==NULL)  return;

config->DeleteGroup(wxT("/Misc/DirCtrlTB/Icons"));
config->Write(wxT("/Misc/DirCtrlTB/Icons/NoOfKnownIcons"), (long)NoOfKnownIcons);
for (size_t n=0; n < NoOfKnownIcons; ++n)
  { config->Write(wxT("/Misc/DirCtrlTB/Icons/")+CreateSubgroupName(n,NoOfKnownIcons)+wxT("/Icon"), Icons[n]);
    config->Write(wxT("/Misc/DirCtrlTB/Icons/")+CreateSubgroupName(n,NoOfKnownIcons)+wxT("/Icontype"), Icontypes[n]);
  }

config->Flush();
}

void ToolbarIconsDialog::AddIcon(wxString& newfilepath, size_t type)
{
if (newfilepath.IsEmpty()) return;
if (newfilepath.find(BITMAPSDIR) == 0) newfilepath = newfilepath.AfterLast(wxFILE_SEP_PATH);  // If stored in BITMAPSDIR, use only the filename

Icons.Add(newfilepath); Icontypes.Add(type);
SaveIcons();
}

void ToolbarIconsDialog::FillListbox()
{
list->Clear();                                    // In case this is a refill

for (size_t n=0; n < DirCtrlTBinfoarray->GetCount(); ++n)  
  { wxString item = DirCtrlTBinfoarray->Item(n)->destination;
    list->Append(TruncatePathtoFitBox(item, list));
  }

if (DirCtrlTBinfoarray->GetCount()) list->SetSelection(0);
}

void ToolbarIconsDialog::OnAddButton(wxCommandEvent& WXUNUSED(event))
{
AddToobarIconDlg dlg;
wxXmlResource::Get()->LoadDialog(&dlg, this, wxT("AddToobarIconDlg"));
size_t nexticon = DirCtrlTBinfoarray->GetCount();     // See if we can guess the icon to use: the next available one
size_t NoOfKnownIcons = (size_t)wxConfigBase::Get()->Read(wxT("/Misc/DirCtrlTB/Icons/NoOfKnownIcons"), 0l);
size_t icon = nexticon < NoOfKnownIcons ? nexticon : 0;

wxString defaultfilepath = GetCwd();  // Provide a default for the default path in case there's no active pane
if (MyFrame::mainframe->GetActivePane()) defaultfilepath = MyFrame::mainframe->GetActivePane()->GetPath();
dlg.Init(this, defaultfilepath, wxT(""), wxT(""), icon);
if (dlg.ShowModal() != wxID_OK) return;

wxString Filepath = dlg.Filepath->GetValue(); if (Filepath.IsEmpty()) return;

DirCtrlTBStruct* tbs = new DirCtrlTBStruct;           // Make a new struct, & store the data in it
tbs->destination = Filepath; tbs->label = dlg.Label->GetValue(); tbs->tooltip = dlg.Tooltip->GetValue(); tbs->iconNo = dlg.iconno;
DirCtrlTBinfoarray->Add(tbs);

Save();                                               // Rather than faffing around with a 'dirty' flag, just save each time
FillListbox();                                        // Redo the listbox to reflect the new entry
}

void ToolbarIconsDialog::OnEditButton(wxCommandEvent& WXUNUSED(event))
{
int index = list->GetSelection(); if (index == wxNOT_FOUND) return;

AddToobarIconDlg dlg;
wxXmlResource::Get()->LoadDialog(&dlg, this, wxT("AddToobarIconDlg"));

DirCtrlTBStruct* tbs = DirCtrlTBinfoarray->Item(index);    // Readability
dlg.Init(this, tbs->destination, tbs->label, tbs->tooltip, tbs->iconNo);
if (dlg.ShowModal() != wxID_OK) return;

wxString Filepath = dlg.Filepath->GetValue(); if (Filepath.IsEmpty()) return;  // It's difficult to imagine what would be more appropriate
tbs->destination = Filepath; tbs->label = dlg.Label->GetValue(); tbs->tooltip = dlg.Tooltip->GetValue(); tbs->iconNo = dlg.iconno;

Save();                                             // Rather than faffing around with a 'dirty' flag, just save each time
FillListbox();                                      // Redo the listbox to reflect the alteration
}

void ToolbarIconsDialog::OnDeleteButton(wxCommandEvent& WXUNUSED(event))
{
int index = list->GetSelection(); if (index == wxNOT_FOUND) return;

wxMessageDialog ask(this,_("Delete this toolbar button?"),_("Are you sure?"), wxYES_NO | wxICON_QUESTION);
if (ask.ShowModal() == wxID_YES)
  { DirCtrlTBinfoarray->RemoveAt(index);
    Save(); FillListbox();
  }
}

int ToolbarIconsDialog::SetBitmap(wxBitmapButton* btn, wxWindow* pa, long iconno)
{
wxConfigBase* config = wxConfigBase::Get(); if (config==NULL)  return false;

long NoOfKnownIcons = config->Read(wxT("/Misc/DirCtrlTB/Icons/NoOfKnownIcons"), 0l);

wxString Icon; config->Read(wxT("/Misc/DirCtrlTB/Icons/")+CreateSubgroupName(iconno,NoOfKnownIcons)+wxT("/Icon"), &Icon);
long Icontype = config->Read(wxT("/Misc/DirCtrlTB/Icons/")+CreateSubgroupName(iconno,NoOfKnownIcons)+wxT("/Icontype"), -1l);  // Can be wxBITMAP_TYPE_PNG, wxBITMAP_TYPE_XPM or wxBITMAP_TYPE_BMP (other handlers aren't loaded)
if (Icontype==-1) return false;

wxString path; if (Icon.Left(1) != wxFILE_SEP_PATH) path = BITMAPSDIR;    // If icon is just a filename, we'll prepend with BITMAPSDIR
wxBitmap bitmap(path+Icon, (wxBitmapType)Icontype);
int id = NextID++;
if (!btn->Create(pa, id, bitmap, wxDefaultPosition, wxDefaultSize, wxNO_BORDER)) return false;
return id;
}

void ToolbarIconsDialog::DoUpdateUI(wxUpdateUIEvent& event)
{
event.Enable(list->GetCount() > 0); 
}

BEGIN_EVENT_TABLE(ToolbarIconsDialog,wxDialog)
  EVT_BUTTON(XRCID("AddBtn"), ToolbarIconsDialog::OnAddButton)
  EVT_BUTTON(XRCID("EditBtn"), ToolbarIconsDialog::OnEditButton)
  EVT_BUTTON(XRCID("DeleteBtn"), ToolbarIconsDialog::OnDeleteButton)
  EVT_INIT_DIALOG(ToolbarIconsDialog::OnInitDialog)
  EVT_UPDATE_UI(XRCID("EditBtn"), ToolbarIconsDialog::DoUpdateUI)
  EVT_UPDATE_UI(XRCID("DeleteBtn"), ToolbarIconsDialog::DoUpdateUI)
END_EVENT_TABLE()

void AddToobarIconDlg::Init(ToolbarIconsDialog* granddad, const wxString& defaultfilepath, const wxString& defaultLabel, const wxString& defaulttooltip, long icon)
{
grandparent = granddad; iconno = icon; btn = NULL;

Filepath = (wxTextCtrl*)(FindWindow(wxT("Filepath")));
if (Filepath && !defaultfilepath.IsEmpty())  Filepath->SetValue(defaultfilepath);
Label = (wxTextCtrl*)(FindWindow(wxT("Label")));
if (Label) Label->SetValue(defaultLabel);
Tooltip = (wxTextCtrl*)(FindWindow(wxT("Tooltip")));
if (Tooltip) Tooltip->SetValue(defaulttooltip);

int id = CreateButton();                                        // Create the current bitmapbutton
if (id) Connect(id, wxEVT_COMMAND_BUTTON_CLICKED, (wxObjectEventFunction)&AddToobarIconDlg::OnIconClicked);  // & connect it to Select Icon dialog

Connect(XRCID("Browse"), wxEVT_COMMAND_BUTTON_CLICKED, (wxObjectEventFunction)&AddToobarIconDlg::OnBrowseBtn); // Connect the browse button too
GetSizer()->Layout();
GetSizer()->Fit(this);
}

int AddToobarIconDlg::CreateButton()
{
wxStaticText* st = (wxStaticText*)(FindWindow(wxT("StaticText")));
wxSizer* sizer = st->GetContainingSizer();

if (btn != NULL) btn->Destroy();  // Kill any old button, in case we're changing a bitmap
btn = new wxBitmapButton; 

wxWindow* buttonparent;
#if wxVERSION_NUMBER < 2904 
  buttonparent = this;
#else
  buttonparent = st->GetParent(); // Since 2.9.4, in a wxStaticBoxSizer XRC (correctly) uses the wxStaticBox as parent
#endif
int id = grandparent->SetBitmap(btn, buttonparent, iconno); if (id) sizer->Add(btn, 0,  wxALIGN_CENTRE);  // Create the current icon and add to sizer
sizer->Layout();
return id;
}

void AddToobarIconDlg::OnIconClicked(wxCommandEvent& WXUNUSED(event))
{
TBIconSelectDlg dlg;
wxXmlResource::Get()->LoadDialog(&dlg, this, wxT("IconSelectDlg"));
dlg.Init(grandparent, iconno);
if (dlg.ShowModal() != wxID_OK) return;

iconno = dlg.iconno;                                          // Get any altered icon choice
int id = CreateButton();                                      // Recreate the current bitmapbutton
if (id) Connect(id, wxEVT_COMMAND_BUTTON_CLICKED, (wxObjectEventFunction)&AddToobarIconDlg::OnIconClicked);  // & connect the new version to Select Icon dialog
}

void AddToobarIconDlg::OnBrowseBtn(wxCommandEvent& WXUNUSED(event))
{
FileDirDlg fdlg(this, FD_SHOWHIDDEN, StrWithSep(wxGetApp().GetHOME()), _("Browse for the Filepath to Add"));
if (fdlg.ShowModal() != wxID_OK) return;

Filepath->SetValue(fdlg.GetFilepath());
}

#include <math.h>              // For sqrt
void TBIconSelectDlg::DisplayButtonChoices()
{
wxConfigBase* config = wxConfigBase::Get(); if (config==NULL)  return;
long NoOfKnownIcons = config->Read(wxT("/Misc/DirCtrlTB/Icons/NoOfKnownIcons"), 0l);

if (panel != NULL) panel->Destroy();                        // In case this is a rerun
panel = new wxPanel(this, wxID_ANY);
size_t cols = (size_t)sqrt(NoOfKnownIcons); cols = wxMax(cols, 1); size_t colsremainder = (((size_t)NoOfKnownIcons) - (cols*cols)) > 0;
size_t rows = ((size_t)NoOfKnownIcons)/(cols+1) + (((size_t)NoOfKnownIcons)%(cols+1) > 0);
gridsizer = new wxGridSizer(cols + colsremainder, 3, 3);

int AverageIconHt=0;  // We'll need this to work out how big to make the panel, as a wrong guess resulted in invisible OK/Cancel buttons. Width doesn't matter so much.
StartId = NextID;                                           // Store the id that'll be given to the 1st button
for (int n=0; n < (int)NoOfKnownIcons; ++n)
  { wxString Icon; config->Read(wxT("/Misc/DirCtrlTB/Icons/")+CreateSubgroupName(n,NoOfKnownIcons)+wxT("/Icon"), &Icon);
    long Icontype = config->Read(wxT("/Misc/DirCtrlTB/Icons/")+CreateSubgroupName(n,NoOfKnownIcons)+wxT("/Icontype"), -1l);  // Can be wxBITMAP_TYPE_PNG, wxBITMAP_TYPE_XPM or wxBITMAP_TYPE_BMP (other handlers aren't loaded)
    if (Icontype == -1) continue;
    
    wxString path; if (Icon.Left(1) != wxFILE_SEP_PATH) path = BITMAPSDIR;  // If icon is just a filename, we'll prepend with BITMAPSDIR
    wxBitmap bitmap(path+Icon, (wxBitmapType)Icontype);
    wxBitmapButton* btn = new wxBitmapButton(panel, NextID++, bitmap, wxDefaultPosition, wxDefaultSize, wxNO_BORDER);
    if (btn == NULL) { --NextID; continue; }
    
    int iconht, iconwd; btn->GetSize(&iconwd,&iconht); AverageIconHt += iconht;
     gridsizer->Add(btn);
  }  

AverageIconHt /= NoOfKnownIcons;
panel->SetSize((cols+1)*30, rows*AverageIconHt);
panel->SetSizer(gridsizer);
bsizer->Add(panel, 1, wxALIGN_CENTRE);

SetSize(width+(cols+1)*30, height+(rows+1)*AverageIconHt);  // Resize the whole dialog to take the panel into account. I couldn't find a pure sizer way to do this :(
GetSizer()->Layout();

EndId = NextID - 1;                                         // Store the id that was given to the last button
Connect(StartId, EndId, wxEVT_COMMAND_BUTTON_CLICKED, (wxObjectEventFunction)&TBIconSelectDlg::OnNewSelection);
}

void ConfigureFileOpenDlg::Init()
{
m_UseSystem  = XRCCTRL(*this, "UseMimetypes", wxCheckBox); wxCHECK_RET(m_UseSystem, wxT("Couldn't load spinctrl"));
m_UseBuiltin = XRCCTRL(*this, "UseBuiltin", wxCheckBox);
m_PreferSystem = XRCCTRL(*this, "SystemFirst", wxRadioButton);
m_PreferBuiltin = XRCCTRL(*this, "BuiltinFirst", wxRadioButton);

m_UseSystem->SetValue(USE_SYSTEM_OPEN);
m_UseBuiltin->SetValue(USE_BUILTIN_OPEN);
if (PREFER_SYSTEM_OPEN)
  m_PreferSystem->SetValue(true);
 else
  m_PreferBuiltin->SetValue(true);

m_PreferSystem->Connect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(ConfigureFileOpenDlg::OnRadioUpdateUI), NULL, this);
m_PreferBuiltin->Connect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(ConfigureFileOpenDlg::OnRadioUpdateUI), NULL, this);
}

void ConfigureFileOpenDlg::OnRadioUpdateUI(wxUpdateUIEvent& event)
{
event.Enable(m_UseSystem->IsChecked() && m_UseBuiltin->IsChecked());
}
