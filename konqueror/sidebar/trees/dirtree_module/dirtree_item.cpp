/* This file is part of the KDE project
   Copyright (C) 2000 David Faure <faure@kde.org>
   Copyright (C) 2003 Waldo Bastian <bastian@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

//#include "konq_treepart.h"
#include "dirtree_item.h"
#include "dirtree_module.h"
#include <konq_operations.h>
#include <konq_drag.h>
#include <kdebug.h>
#include <kglobalsettings.h>
#include <kuserprofile.h>
#include <qapplication.h>
#include <qclipboard.h>
#include <kio/paste.h>
#include <qfile.h>
#include <qpainter.h>
#include <kiconloader.h>
#include <qcursor.h>

#define MYMODULE static_cast<KonqSidebarDirTreeModule*>(module())

KonqSidebarDirTreeItem::KonqSidebarDirTreeItem( KonqSidebarTreeItem *parentItem, KonqSidebarTreeTopLevelItem *topLevelItem, KFileItem *fileItem )
    : KonqSidebarTreeItem( parentItem, topLevelItem ), m_fileItem( fileItem )
{
    if ( m_topLevelItem )
        MYMODULE->addSubDir( this );
    reset();
}

KonqSidebarDirTreeItem::KonqSidebarDirTreeItem( KonqSidebarTree *parent, KonqSidebarTreeTopLevelItem *topLevelItem, KFileItem *fileItem )
    : KonqSidebarTreeItem( parent, topLevelItem ), m_fileItem( fileItem )
{
    if ( m_topLevelItem )
        MYMODULE->addSubDir( this );
    reset();
}

KonqSidebarDirTreeItem::~KonqSidebarDirTreeItem()
{
}

void KonqSidebarDirTreeItem::reset()
{
    bool expandable = true;
    // For local dirs, find out if they have no children, to remove the "+"
    if ( m_fileItem->isDir() )
    {
        KURL url = m_fileItem->url();
        if ( url.isLocalFile() )
        {
            QCString path( QFile::encodeName(url.path()));
            struct stat buff;
            if ( ::stat( path.data(), &buff ) != -1 )
            {
                //kdDebug() << "KonqSidebarDirTreeItem::init " << path << " : " << buff.st_nlink << endl;
                // The link count for a directory is generally subdir_count + 2.
                // One exception is if there are hard links to the directory, in this case
                // the link count can be > 2 even if no subdirs exist.
                // The other exception are smb (and maybe netware) mounted directories
                // of which the link count is always 1. Therefore, we only set the item
                // as non-expandable if it's exactly 2 (one link from the parent dir,
                // plus one from the '.' entry).
                if ( buff.st_nlink == 2 )
                    expandable = false;
            }
        }
    }
    setExpandable( expandable );
    id = m_fileItem->url().url(-1);
}

void KonqSidebarDirTreeItem::setOpen( bool open )
{
    kdDebug(1201) << "KonqSidebarDirTreeItem::setOpen " << open << endl;
    if ( open && !childCount() && m_bListable )
        MYMODULE->openSubFolder( this );
    else if ( hasStandardIcon() )
    {
        int size = KGlobal::iconLoader()->currentSize( KIcon::Small );
        if ( open )
            setPixmap( 0, DesktopIcon( "folder_open", size ) );
        else
            setPixmap( 0, m_fileItem->pixmap( size ) );
    }
    KonqSidebarTreeItem::setOpen( open );
}

bool KonqSidebarDirTreeItem::hasStandardIcon()
{
    // The reason why we can't use KFileItem::iconName() is that it doesn't
    // take custom icons in .directory files into account
    return m_fileItem->iconName() == "folder";
}

void KonqSidebarDirTreeItem::paintCell( QPainter *_painter, const QColorGroup & _cg, int _column, int _width, int _alignment )
{
    if (m_fileItem->isLink())
    {
        QFont f( _painter->font() );
        f.setItalic( TRUE );
        _painter->setFont( f );
    }
    QListViewItem::paintCell( _painter, _cg, _column, _width, _alignment );
}

KURL KonqSidebarDirTreeItem::externalURL() const
{
    return m_fileItem->url();
}

QString KonqSidebarDirTreeItem::externalMimeType() const
{
    if (m_fileItem->isMimeTypeKnown())
        return m_fileItem->mimetype();
    else
        return QString::null;
}

bool KonqSidebarDirTreeItem::acceptsDrops( const QStrList & formats )
{
    if ( formats.contains("text/uri-list") )
        return m_fileItem->acceptsDrops();
    return false;
}

void KonqSidebarDirTreeItem::drop( QDropEvent * ev )
{
    KonqOperations::doDrop( m_fileItem, externalURL(), ev, tree() );
}

QDragObject * KonqSidebarDirTreeItem::dragObject( QWidget * parent, bool move )
{
    KURL::List lst;
    lst.append( m_fileItem->url() );

    KonqDrag * drag = KonqDrag::newDrag( lst, false, parent );
    drag->setMoveSelection( move );

    return drag;
}

void KonqSidebarDirTreeItem::itemSelected()
{
    bool bInTrash = false;

    if ( m_fileItem->url().directory(false) == KGlobalSettings::trashPath() )
        bInTrash = true;

    QMimeSource *data = QApplication::clipboard()->data();
    bool paste = ( data->encodedData( data->format() ).size() != 0 );

    tree()->enableActions( true, true, paste, true && !bInTrash, true, true );
}

void KonqSidebarDirTreeItem::middleButtonClicked()
{
    // Duplicated from KonqDirPart :(
    // Optimisation to avoid KRun to call kfmclient that then tells us
    // to open a window :-)
    KService::Ptr offer = KServiceTypeProfile::preferredService(m_fileItem->mimetype(), "Application");
    if (offer) kdDebug(1201) << "KonqDirPart::mmbClicked: got service " << offer->desktopEntryName() << endl;
    if ( offer && offer->desktopEntryName().startsWith("kfmclient") )
    {
        kdDebug(1201)<<"Emitting createNewWindow"<<endl;
        KParts::URLArgs args;
        args.serviceType = m_fileItem->mimetype();
        emit tree()->createNewWindow( m_fileItem->url(), args );
    }
    else
        m_fileItem->run();
}

void KonqSidebarDirTreeItem::rightButtonPressed()
{
    KFileItemList lstItems;
    lstItems.append( m_fileItem );
    emit tree()->popupMenu( QCursor::pos(), lstItems );
}

void KonqSidebarDirTreeItem::paste()
{
    // move or not move ?
    bool move = false;
    QMimeSource *data = QApplication::clipboard()->data();
    if ( data->provides( "application/x-kde-cutselection" ) ) {
        move = KonqDrag::decodeIsCutSelection( data );
        kdDebug(1201) << "move (from clipboard data) = " << move << endl;
    }

    KIO::pasteClipboard( m_fileItem->url(), move );
}

void KonqSidebarDirTreeItem::trash()
{
    delOperation( KonqOperations::TRASH );
}

void KonqSidebarDirTreeItem::del()
{
    delOperation( KonqOperations::DEL );
}

void KonqSidebarDirTreeItem::shred()
{
    delOperation( KonqOperations::SHRED );
}

void KonqSidebarDirTreeItem::delOperation( int method )
{
    KURL::List lst;
    lst.append(m_fileItem->url());

    KonqOperations::del(tree(), method, lst);
}

QString KonqSidebarDirTreeItem::toolTipText() const
{
    return m_fileItem->url().pathOrURL();
}

void KonqSidebarDirTreeItem::rename()
{
    tree()->rename( this, 0 );
}

void KonqSidebarDirTreeItem::rename( const QString & name )
{
    KonqOperations::rename( tree(), m_fileItem->url(), name );
}
