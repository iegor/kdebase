/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000    Daniel M. Duley <mosfet@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

/* Qt */
#include <qtcommon.hpp> /* include/qtcommon.hpp */
#include <qpixmap.h>

/* KDE */
#include <kdecommon.hpp> /* include/kdecommon.hpp */

/* Xorg */
#include <X11/SM/SMlib.h>

/* KWin */
#include <core/common.hpp>

namespace KWinInternal {
  PluginMgr::PluginMgr()
  : KDecorationPlugins( KGlobal::config()) {
    defaultPlugin = (QPixmap::defaultDepth() > 8) ? "kwin3_plastik" : "kwin3_quartz";
    loadPlugin(""); // load the plugin specified in cfg file
  }

  void PluginMgr::error( const QString &error_msg ) {
    qWarning( "%s", (i18n("KWin: ") + error_msg + i18n("\nKWin will now exit...")).local8Bit().data() );
    exit(1);
  }

  bool PluginMgr::provides( Requirement ) { return false; }
}; /* namespace KWinInternal */
