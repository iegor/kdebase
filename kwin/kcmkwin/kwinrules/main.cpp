/*
 * Copyright (c) 2004 Lubos Lunak <l.lunak@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/* Qt */
#include <qtcommon.hpp> /* include/qtcommon.hpp */

/* KDE */
#include <kdecommon.hpp> /* include/kdecommon.hpp */
#include <kcmdlineargs.h>
#include <dcopclient.h>

/* Xorg */
#include <X11/SM/SMlib.h>

/* KWin */
#include <core/common.hpp>

/* kwinrules */
#include "ruleswidget.h"

namespace KWinInternal
{

static void loadRules( QValueList< Rules* >& rules )
    {
    KConfig cfg( "kwinrulesrc", true );
    cfg.setGroup( "General" );
    int count = cfg.readNumEntry( "count" );
    for( int i = 1;
         i <= count;
         ++i )
        {
        cfg.setGroup( QString::number( i ));
        Rules* rule = new Rules( cfg );
        rules.append( rule );
        }
    }

static void saveRules( const QValueList< Rules* >& rules )
    {
    KConfig cfg( "kwinrulesrc" );
    QStringList groups = cfg.groupList();
    for( QStringList::ConstIterator it = groups.begin();
         it != groups.end();
         ++it )
        cfg.deleteGroup( *it );
    cfg.setGroup( "General" );
    cfg.writeEntry( "count", rules.count());
    int i = 1;
    for( QValueList< Rules* >::ConstIterator it = rules.begin();
         it != rules.end();
         ++it )
        {
        cfg.setGroup( QString::number( i ));
        (*it)->write( cfg );
        ++i;
        }
    }

static Rules* findRule( const QValueList< Rules* >& rules, Window wid, bool whole_app )
    {
    KWin::WindowInfo info = KWin::windowInfo( wid,
        NET::WMName | NET::WMWindowType,
        NET::WM2WindowClass | NET::WM2WindowRole | NET::WM2ClientMachine );
    if( !info.valid()) // shouldn't really happen
        return NULL;
    QCString wmclass_class = info.windowClassClass().lower();
    QCString wmclass_name = info.windowClassName().lower();
    QCString role = info.windowRole().lower();
    NET::WindowType type = info.windowType( NET::NormalMask | NET::DesktopMask | NET::DockMask
        | NET::ToolbarMask | NET::MenuMask | NET::DialogMask | NET::OverrideMask | NET::TopMenuMask
        | NET::UtilityMask | NET::SplashMask );
    QString title = info.name();
//    QCString extrarole = ""; // TODO
    QCString machine = info.clientMachine().lower();
    Rules* best_match = NULL;
    int match_quality = 0;
    for( QValueList< Rules* >::ConstIterator it = rules.begin();
         it != rules.end();
         ++it )
        {
        // try to find an exact match, i.e. not a generic rule
        Rules* rule = *it;
        int quality = 0;
        bool generic = true;
        if( rule->wmclassmatch != Rules::ExactMatch )
            continue; // too generic
        if( !rule->matchWMClass( wmclass_class, wmclass_name ))
            continue;
        // from now on, it matches the app - now try to match for a specific window
        if( rule->wmclasscomplete )
            {
            quality += 1;
            generic = false;  // this can be considered specific enough (old X apps)
            }
        if( !whole_app )
            {
            if( rule->windowrolematch != Rules::UnimportantMatch )
                {
                quality += rule->windowrolematch == Rules::ExactMatch ? 5 : 1;
                generic = false;
                }
            if( rule->titlematch != Rules::UnimportantMatch )
                {
                quality += rule->titlematch == Rules::ExactMatch ? 3 : 1;
                generic = false;
                }
            if( rule->types != NET::AllTypesMask )
                {
                int bits = 0;
                for( unsigned int bit = 1;
                     bit < 1U << 31;
                     bit <<= 1 )
                    if( rule->types & bit )
                        ++bits;
                if( bits == 1 )
                    quality += 2;
                }
            if( generic ) // ignore generic rules, use only the ones that are for this window
                continue;
            }
        else
            {
            if( rule->types == NET::AllTypesMask )
                quality += 2;
            }
        if( !rule->matchType( type )
            || !rule->matchRole( role )
            || !rule->matchTitle( title )
            || !rule->matchClientMachine( machine ))
            continue;
        if( quality > match_quality )
            {
            best_match = rule;
            match_quality = quality;
            }
        }
    if( best_match != NULL )
        return best_match;
    Rules* ret = new Rules;
    if( whole_app )
        {
        ret->description = i18n( "Application settings for %1" ).arg( wmclass_class );
        // TODO maybe exclude some types? If yes, then also exclude them above
        // when searching.
        ret->types = NET::AllTypesMask;
        ret->titlematch = Rules::UnimportantMatch;
        ret->clientmachine = machine; // set, but make unimportant
        ret->clientmachinematch = Rules::UnimportantMatch;
        ret->extrarolematch = Rules::UnimportantMatch;
        ret->windowrolematch = Rules::UnimportantMatch;
        if( wmclass_name == wmclass_class )
            {
            ret->wmclasscomplete = false;
            ret->wmclass = wmclass_class;
            ret->wmclassmatch = Rules::ExactMatch;
            }
        else
            {
            // WM_CLASS components differ - perhaps the app got -name argument
            ret->wmclasscomplete = true;
            ret->wmclass = wmclass_name + ' ' + wmclass_class;
            ret->wmclassmatch = Rules::ExactMatch;
            }
        return ret;
        }
    ret->description = i18n( "Window settings for %1" ).arg( wmclass_class );
    if( type == NET::Unknown )
        ret->types = NET::NormalMask;
    else
        ret->types = 1 << type; // convert type to its mask
    ret->title = title; // set, but make unimportant
    ret->titlematch = Rules::UnimportantMatch;
    ret->clientmachine = machine; // set, but make unimportant
    ret->clientmachinematch = Rules::UnimportantMatch;
//    ret->extrarole = extra; TODO
    ret->extrarolematch = Rules::UnimportantMatch;
    if( !role.isEmpty()
        && role != "unknown" && role != "unnamed" ) // Qt sets this if not specified
        {
        ret->windowrole = role;
        ret->windowrolematch = Rules::ExactMatch;
        if( wmclass_name == wmclass_class )
            {
            ret->wmclasscomplete = false;
            ret->wmclass = wmclass_class;
            ret->wmclassmatch = Rules::ExactMatch;
            }
        else
            {
            // WM_CLASS components differ - perhaps the app got -name argument
            ret->wmclasscomplete = true;
            ret->wmclass = wmclass_name + ' ' + wmclass_class;
            ret->wmclassmatch = Rules::ExactMatch;
            }
        }
    else // no role set
        {
        if( wmclass_name != wmclass_class )
            {
            ret->wmclasscomplete = true;
            ret->wmclass = wmclass_name + ' ' + wmclass_class;
            ret->wmclassmatch = Rules::ExactMatch;
            }
        else
            {
            // This is a window that has no role set, and both components of WM_CLASS
            // match (possibly only differing in case), which most likely means either
            // the application doesn't give a damn about distinguishing its various
            // windows, or it's an app that uses role for that, but this window
            // lacks it for some reason. Use non-complete WM_CLASS matching, also
            // include window title in the matching, and pray it causes many more positive
            // matches than negative matches.
            ret->titlematch = Rules::ExactMatch;
            ret->wmclasscomplete = false;
            ret->wmclass = wmclass_class;
            ret->wmclassmatch = Rules::ExactMatch;
            }
        }
    return ret;
    }

static int edit( Window wid, bool whole_app )
    {
    QValueList< Rules* > rules;
    loadRules( rules );
    Rules* orig_rule = findRule( rules, wid, whole_app );
    RulesDialog dlg;
    // dlg.edit() creates new Rules instance if edited
    Rules* edited_rule = dlg.edit( orig_rule, wid, true );
    if( edited_rule == NULL || edited_rule->isEmpty())
        {
        rules.remove( orig_rule );
        delete orig_rule;
        if( orig_rule != edited_rule )
            delete edited_rule;
        }
    else if( edited_rule != orig_rule )
        {
        QValueList< Rules* >::Iterator pos = rules.find( orig_rule );
        if( pos != rules.end())
            *pos = edited_rule;
        else
            rules.prepend( edited_rule );
        delete orig_rule;
        }
    saveRules( rules );
    if( !kapp->dcopClient()->isAttached())
        kapp->dcopClient()->attach();
    kapp->dcopClient()->send("kwin*", "", "reconfigure()", "");
    return 0;
    }

} // namespace

static const KCmdLineOptions options[] =
    {
    // no need for I18N_NOOP(), this is not supposed to be used directly
        { "wid <wid>", "WId of the window for special window settings.", 0 },
        { "whole-app", "Whether the settings should affect all windows of the application.", 0 },
        KCmdLineLastOption
    };

extern "C"
KDE_EXPORT int kdemain( int argc, char* argv[] )
    {
    KLocale::setMainCatalogue( "kcmkwinrules" );
    KCmdLineArgs::init( argc, argv, "kwin_rules_dialog", I18N_NOOP( "KWin" ),
	I18N_NOOP( "KWin helper utility" ), "1.0" );
    KCmdLineArgs::addCmdLineOptions( options );
    KApplication app;
    KCmdLineArgs* args = KCmdLineArgs::parsedArgs();
    bool id_ok = false;
    Window id = args->getOption( "wid" ).toULong( &id_ok );
    bool whole_app = args->isSet( "whole-app" );
    args->clear();
    if( !id_ok || id == None )
        {
	KCmdLineArgs::usage( i18n( "This helper utility is not supposed to be called directly." ));
	return 1;
        }
    return KWinInternal::edit( id, whole_app );
    }
