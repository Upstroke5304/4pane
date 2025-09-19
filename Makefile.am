
## Check the user actually wants to install the binary
if AMINSTALL_install_app
  bin_PROGRAMS = 4Pane
endif

## automake seems to derive $(pkgdatadir) from AC_INIT, which lowercases it!
pkgdatadir = $(datadir)/4Pane

ACLOCAL_AMFLAGS = -I .build --install


## Variables: ##

CC = @CC@
CXX = @CXX@

WX_CFLAGS = @WX_CFLAGS@
WX_CXXFLAGS = @WX_CXXFLAGS@
WX_LIBS = @WX_LIBS@

GTKPKG_CFLAGS = @GTKPKG_CFLAGS@
GTKPKG_LDFLAGS = @GTKPKG_LDFLAGS@
XZFLAGS = @XZFLAGS@
BZIP2_FLAGS = @BZIP2_FLAGS@
EXTRA_CPPFLAGS = @EXTRA_CPPFLAGS@
EXTRA_CFLAGS = @EXTRA_CFLAGS@
EXTRA_CXXFLAGS = @EXTRA_CXXFLAGS@
EXTRA_LDFLAGS = @EXTRA_LDFLAGS@


## The following prevents outputting, in every compile element, several inches of unnecessary '-DPACKAGE_FOO=bar' type definitions
DEFS = 

## I'm intentionally omitting $CPPFLAGS here: they're a dup of WX_C(XX)FLAGS, which would fill every compile line with cruft
## I'm similarly omitting $CFLAGS & $CXXFLAGS to prevent duplication as they're auto-added later
4Pane_CFLAGS = $(WX_CFLAGS) $(EXTRA_CPPFLAGS) $(PREVENT_ASSERTS) $(XZFLAGS) $(BZIP2_FLAGS) $(EXTRA_CFLAGS)
4Pane_CXXFLAGS = -std=c++11 $(WX_CXXFLAGS)  $(EXTRA_CPPFLAGS) $(GTKPKG_CFLAGS) $(PREVENT_ASSERTS) $(XZFLAGS) $(BZIP2_FLAGS) $(EXTRA_CXXFLAGS)

## LIBS is an autoconf builtin (like CXXFLAGS etc). It seems 2b the best place for lib stuff as it gets appended to the link line
LIBS = $(WX_LIBS)  $(GTKPKG_LDFLAGS) $(AM_LDFLAGS) $(LDFLAGS) $(XTRLIBS)
## We can't let 4Pane_LINK work as usual: it appends the objects, which means link fails. So override it
4Pane_LINK=$(CXX) -o 4Pane 

## Sources: ##

4Pane_SOURCES = \
  Accelerators.cpp  Accelerators.h \
  Archive.cpp Archive.h \
  ArchiveStream.cpp ArchiveStream.h \
  Bookmarks.cpp Bookmarks.h \
  Configure.cpp Configure.h \
  Devices.cpp Devices.h \
  Dup.cpp Dup.h \
  ExecuteInDialog.cpp ExecuteInDialog.h \
  Filetypes.cpp Filetypes.h \
  Misc.cpp Misc.h \
  Mounts.cpp Mounts.h \
  MyDirs.cpp MyDirs.h \
  MyDragImage.cpp MyDragImage.h \
  MyFiles.cpp MyFiles.h \
  MyFrame.cpp MyFrame.h \
  MyGenericDirCtrl.cpp MyGenericDirCtrl.h \
  MyNotebook.cpp MyNotebook.h \
  MyTreeCtrl.cpp MyTreeCtrl.h \
  Otherstreams.cpp Otherstreams.h \
  Redo.cpp Redo.h \
  Tools.cpp Tools.h \
  bzipstream.cpp bzipstream.h \
  config.h \
  Externs.h \
  sdk/rsvg/rsvg.cpp \
  sdk/fswatcher/MyFSWatcher.cpp \
  sdk/fswatcher/MyFSWatcher.h

if AMBUILTIN_BZIP
4Pane_SOURCES += \
  sdk/bzip/blocksort.c \
  sdk/bzip/bzlib.c sdk/bzip/bzlib.h \
  sdk/bzip/compress.c \
  sdk/bzip/crctable.c \
  sdk/bzip/decompress.c \
  sdk/bzip/huffman.c \
  sdk/bzip/randtable.c \
  sdk/bzip/bzlib_private.h\
  sdk/bzip/LICENSE
endif

EXTRA_DIST = \
  4Pane.1 \
  4Pane.appdata.xml \
  changelog \
  LICENCE \
  PACKAGERS \
  .build/autogen.sh \
  .build/configure.ac \
  .build/DONT_README \
  bitmaps \
  doc \
  locale/4Pane.pot \
  locale/ja/LC_MESSAGES/ja.po locale/fa/LC_MESSAGES/fa.po locale/fr/LC_MESSAGES/fr.po locale/da/LC_MESSAGES/da.po locale/it/LC_MESSAGES/it.po locale/ca/LC_MESSAGES/ca.po locale/es/LC_MESSAGES/es.po locale/pl/LC_MESSAGES/pl.po locale/de/LC_MESSAGES/de.po locale/ar/LC_MESSAGES/ar.po locale/pt_BR/LC_MESSAGES/pt_BR.po locale/et/LC_MESSAGES/et.po locale/nl/LC_MESSAGES/nl.po locale/ru/LC_MESSAGES/ru.po locale/vi/LC_MESSAGES/vi.po locale/zh/LC_MESSAGES/zh.po locale/zh_CN/LC_MESSAGES/zh_CN.po locale/zh_TW/LC_MESSAGES/zh_TW.po locale/el/LC_MESSAGES/el.po locale/fi_FI/LC_MESSAGES/fi_FI.po locale/fr_FR/LC_MESSAGES/fr_FR.po locale/tr/LC_MESSAGES/tr.po locale/uk_UA/LC_MESSAGES/uk_UA.po \
  rc/4Pane.desktop rc/dialogs.xrc rc/configuredialogs.xrc rc/moredialogs.xrc

if AMMSGFMT_AVAILABLE
  SUBDIRS = locale
endif

if AMINSTALL_install_rc
  rcdir = $(pkgdatadir)/rc
  rc_DATA = rc/dialogs.xrc rc/moredialogs.xrc rc/configuredialogs.xrc rc/4Pane.desktop

  bitmapdir = $(pkgdatadir)/bitmaps
  bitmap_DATA = bitmaps/4PaneIcon48.png bitmaps/4Pane.png bitmaps/chrome-chromium.png bitmaps/DelTab.png bitmaps/DnDSelectedCursor.png bitmaps/DnDStdCursor.png bitmaps/dragicon.png bitmaps/featherpad.png bitmaps/firefox.png bitmaps/gjots.png bitmaps/hardlink.png bitmaps/help.png bitmaps/iceweasel.png bitmaps/largedropdown.png bitmaps/libreoffice.png bitmaps/mate-text-editor.png bitmaps/mousepad.png bitmaps/mozillacrystal.png bitmaps/NewTab.png bitmaps/openoffice.png bitmaps/palemoon.png bitmaps/photocopier_0.png bitmaps/photocopier_10.png bitmaps/photocopier_11.png bitmaps/photocopier_12.png bitmaps/photocopier_13.png bitmaps/photocopier_14.png bitmaps/photocopier_15.png bitmaps/photocopier_16.png bitmaps/photocopier_17.png bitmaps/photocopier_18.png bitmaps/photocopier_19.png bitmaps/photocopier_1.png bitmaps/photocopier_20.png bitmaps/photocopier_21.png bitmaps/photocopier_22.png bitmaps/photocopier_23.png bitmaps/photocopier_24.png bitmaps/photocopier_25.png bitmaps/photocopier_26.png bitmaps/photocopier_27.png bitmaps/photocopier_28.png bitmaps/photocopier_29.png bitmaps/photocopier_2.png bitmaps/photocopier_30.png bitmaps/photocopier_31.png bitmaps/photocopier_32.png bitmaps/photocopier_33.png bitmaps/photocopier_34.png bitmaps/photocopier_35.png bitmaps/photocopier_36.png bitmaps/photocopier_37.png bitmaps/photocopier_38.png bitmaps/photocopier_39.png bitmaps/photocopier_3.png bitmaps/photocopier_40.png bitmaps/photocopier_41.png bitmaps/photocopier_42.png bitmaps/photocopier_43.png bitmaps/photocopier_4.png bitmaps/photocopier_5.png bitmaps/photocopier_6.png bitmaps/photocopier_7.png bitmaps/photocopier_8.png bitmaps/photocopier_9.png bitmaps/Preview.png bitmaps/seamonkey.png bitmaps/smalldropdown.png bitmaps/softlink.png bitmaps/cdda.png bitmaps/gphoto2.png bitmaps/mtp.png bitmaps/4PaneIcon16.xpm bitmaps/4PaneIcon32.xpm bitmaps/4PaneIcon40x32.xpm bitmaps/4PaneIcon48.xpm bitmaps/back.xpm bitmaps/bm1_button.xpm bitmaps/bm2_button.xpm bitmaps/bm3_button.xpm bitmaps/cdrom.xpm bitmaps/cdr.xpm bitmaps/clear_right.xpm bitmaps/connect_no.xpm bitmaps/dir_up.xpm bitmaps/down.xpm bitmaps/abiword.png bitmaps/evince.xpm bitmaps/fileopen.xpm bitmaps/floppy.xpm bitmaps/forward.xpm bitmaps/gedit.xpm bitmaps/gohome.xpm bitmaps/harddisk-usb.xpm bitmaps/harddisk.xpm bitmaps/kedit.xpm bitmaps/kwrite.xpm bitmaps/largedropdown.xpm bitmaps/MyDocuments.xpm bitmaps/new_dir.xpm bitmaps/smalldropdown.xpm bitmaps/toparent.xpm bitmaps/unknown.xpm bitmaps/UsbMem.xpm bitmaps/UsbMulticard.xpm bitmaps/UsbPen.xpm

  appsvgicondir=$(datadir)/icons/hicolor/scalable/apps
  appsvgicon_DATA=bitmaps/4Pane.svg
  appicondir=$(datadir)/icons/hicolor/48x48/apps
  appicon_DATA=bitmaps/4Pane.png
endif


if AMINSTALL_install_docs
  docsdir = $(datadir)/doc/4Pane
  docs_DATA = doc/About.htm doc/Archive.htm doc/ArchiveBrowse.htm doc/Bookmarks.htm doc/Chapt.con doc/Chapt.hhc doc/Chapt.hhk doc/Chapt.hhp doc/Configure.htm doc/ConfigureUserDefTools.htm doc/ConfiguringDevices.htm doc/ConfiguringDisplay.htm doc/ConfiguringMisc.htm doc/ConfiguringNetworks.htm doc/ConfiguringShortcuts.htm doc/ConfiguringTerminals.htm doc/Contents.htm doc/ContextMenu.htm doc/Copier.png doc/Devices.htm doc/Display.htm doc/DnD.htm doc/DnDSelectedCursor.png doc/DnDStdCursor.png doc/Edit.htm doc/Editors.htm doc/Export.htm doc/FAQ.htm doc/Features.htm doc/FileviewCols.htm doc/Filter.htm doc/Hardlink.png doc/Introduction.htm doc/KeyboardNavigation.htm doc/Licence.htm doc/Menu.htm doc/Mount.htm doc/Move.png doc/MultipleRenDup.htm doc/Open.htm doc/OpenWith.htm doc/Options.htm doc/Previews.htm doc/Properties.htm doc/Quickstart.htm doc/RAQ.htm doc/RegExpHelp.htm doc/Running.htm doc/Softlink.png doc/Statusbar.htm doc/Tabs.htm doc/TerminalEm.htm doc/Toolbar.htm doc/Tools.htm doc/UnRedo.htm doc/Using4Pane.htm doc/View.htm doc/back.gif doc/forward.gif doc/up.gif
endif

if AMMSGFMT_AVAILABLE
if AMINSTALL_locale
  locldir = $(datadir)
  nobase_locl_DATA = locale/ar/LC_MESSAGES/4Pane.mo locale/ca/LC_MESSAGES/4Pane.mo locale/da/LC_MESSAGES/4Pane.mo locale/de/LC_MESSAGES/4Pane.mo locale/es/LC_MESSAGES/4Pane.mo locale/et/LC_MESSAGES/4Pane.mo locale/fa/LC_MESSAGES/4Pane.mo locale/fr/LC_MESSAGES/4Pane.mo locale/it/LC_MESSAGES/4Pane.mo locale/ja/LC_MESSAGES/4Pane.mo locale/nl/LC_MESSAGES/4Pane.mo locale/pl/LC_MESSAGES/4Pane.mo locale/pt_BR/LC_MESSAGES/4Pane.mo locale/ru/LC_MESSAGES/4Pane.mo locale/vi/LC_MESSAGES/4Pane.mo locale/zh/LC_MESSAGES/4Pane.mo locale/zh_CN/LC_MESSAGES/4Pane.mo locale/zh_TW/LC_MESSAGES/4Pane.mo locale/el/LC_MESSAGES/4Pane.mo locale/fi_FI/LC_MESSAGES/4Pane.mo locale/fr_FR/LC_MESSAGES/4Pane.mo locale/tr/LC_MESSAGES/4Pane.mo locale/uk_UA/LC_MESSAGES/4Pane.mo
endif
endif

appdatadir = $(datadir)/metainfo
appdata_DATA = 4Pane.appdata.xml

## Hooks: ##

install-exec-hook:
  ## Create a pP symlink if requested, but only if the app itself is being installed
if AMINSTALL_install_app
if AMINSTALL_symlink
	  ln -fs 4Pane $(DESTDIR)$(bindir)/4pane;
endif
endif
##-----------------------------------------------------------

install-data-hook:
  ## Install the desktop file if requested, but only if the datadir exists
if AMINSTALL_install_rc
if AMINSTALL_desktop
	  if test -d ~/Desktop/; then \
	  cp -up $(DESTDIR)$(pkgdatadir)/rc/4Pane.desktop ~/Desktop; \
	  chmod 0755 ~/Desktop/4Pane.desktop; \
	  fi
endif
endif
##-----------------------------------------------------------

uninstall-hook:
  ## Remove the pP symlink if requested, but only if the app itself is being uninstalled
if AMUNINSTALL_install_app
if AMUNINSTALL_symlink
	  rm -rf $(DESTDIR)$(bindir)/4pane;
endif
endif

  ## /usr/share/4Pane/ is autocreated by install, but isn't autodeleted by uninstall. So do it here
if AMUNINSTALL_install_rc
	  rm -rf $(DESTDIR)$(pkgdatadir);
endif
  ## Similarly /usr/share/doc/4Pane/
if AMUNINSTALL_install_docs
	  rm -rf $(DESTDIR)$(datadir)/doc/4Pane/;
endif

  ## Remove any unwanted desktop file
if AMUNINSTALL_desktop
	  rm -f ~/Desktop/4Pane.desktop;
endif
##-----------------------------------------------------------

