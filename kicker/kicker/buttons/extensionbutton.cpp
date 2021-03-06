/*****************************************************************

Copyright (c) 1996-2001 the kicker authors. See file AUTHORS.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

******************************************************************/

#include <qtooltip.h>

#include <kconfig.h>
#include <kpanelmenu.h>

#include "menuinfo.h"

#include "extensionbutton.h"
#include "extensionbutton.moc"

ExtensionButton::ExtensionButton( const QString& desktopFile, QWidget* parent )
    : PanelPopupButton( parent, "ExtensionButton" )
    , info( 0 )
    , menu( 0 )
{
    initialize( desktopFile );
}

ExtensionButton::ExtensionButton( const KConfigGroup& config, QWidget* parent )
    : PanelPopupButton( parent, "extensionbuttton" )
{
    initialize( config.readPathEntry("DesktopFile") );
}

void ExtensionButton::initialize( const QString& desktopFile )
{
    info = new MenuInfo(desktopFile);
    if (!info->isValid())
    {
       m_valid = false;
       return;
    }
    menu = info->load(this);
    setPopup(menu);

    QToolTip::add(this, info->comment());
    setTitle(info->name());
    setIcon(info->icon());
}

ExtensionButton::~ExtensionButton()
{
    delete info;
}

void ExtensionButton::saveConfig( KConfigGroup& config ) const
{
    config.writePathEntry("DesktopFile", info->desktopFile());
}

void ExtensionButton::initPopup()
{
    if( !menu->initialized() ) {
        menu->reinitialize();
    }
}
