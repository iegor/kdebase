
INCLUDES = -I$(top_srcdir)/kicker/libkicker -I$(top_srcdir)/kicker/kicker/ui $(all_includes)

kde_module_LTLIBRARIES = launcher_panelapplet.la

launcher_panelapplet_la_SOURCES = quicklauncher.skel quicklauncher.cpp quickbutton.cpp quickaddappsmenu.cpp flowgridmanager.cpp popularity.cpp configdlgbase.ui prefs.kcfgc configdlg.cpp

METASOURCES = AUTO
noinst_HEADERS = quicklauncher.h quickbutton.h quickaddappsmenu.h easyvector.h quickbuttongroup.h flowgridmanager.h popularity.h configdlg.h

lnkdir = $(kde_datadir)/kicker/applets
lnk_DATA = quicklauncher.desktop

EXTRA_DIST = $(lnk_DATA)

launcher_panelapplet_la_LDFLAGS = -module $(KDE_RPATH) $(all_libraries) -avoid-version -no-undefined
launcher_panelapplet_la_LIBADD = ../../kicker/core/libkicker_core.la  ../../kicker/buttons/libkicker_buttons.la \
				../../kicker/ui/libkicker_ui.la ../../libkicker/libkickermain.la $(LIB_KIO) \
				$(LIB_KSYCOCA) $(LIB_KDEUI) $(LIB_KUTILS)

kde_kcfg_DATA = launcherapplet.kcfg

messages: rc.cpp
	$(XGETTEXT) *.cpp *.h -o $(podir)/quicklauncher.pot

srcdoc:
	kdoc -a -p -H -d $$HOME/web/src/quicklauncher quicklauncher *.h -lqt -lkdecore -lkdeui
