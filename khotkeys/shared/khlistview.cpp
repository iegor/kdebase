/****************************************************************************

 KHotKeys
 
 Copyright (C) 1999-2002 Lubos Lunak <l.lunak@kde.org>

 Distributed under the terms of the GNU General Public License version 2.
 
****************************************************************************/

#define _KHLISTVIEW_CPP_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "khlistview.h"

#include <kdebug.h>

namespace KHotKeys
{

KHListView::KHListView( QWidget* parent_P, const char* name_P )
    : KListView( parent_P, name_P ), saved_current_item( NULL ),
        in_clear( false ), ignore( false ), force_select( false )
    {
    connect( this, SIGNAL( selectionChanged( QListViewItem* )),
        SLOT( slot_selection_changed( QListViewItem* )));
    connect( this, SIGNAL( currentChanged( QListViewItem* )),
        SLOT( slot_current_changed( QListViewItem* )));
    // CHECKME grrr
    connect( this, SIGNAL( selectionChanged()),
        SLOT( slot_selection_changed()));
    connect( &insert_select_timer, SIGNAL( timeout()),
        SLOT( slot_insert_select()));
    }

void KHListView::slot_selection_changed()
    {
    if( ignore )
        return;
    if( saved_current_item == NULL )
        slot_selection_changed( NULL );
    else if( !saved_current_item->isSelected()) // no way
        setSelected( saved_current_item, true );
    }
    
void KHListView::slot_selection_changed( QListViewItem* item_P )
    {
    if( ignore )
        return;
    if( item_P == saved_current_item )
        return;
    saved_current_item = item_P;
    setCurrentItem( saved_current_item );
    emit current_changed( saved_current_item );
    }
    
void KHListView::slot_current_changed( QListViewItem* item_P )
    {
    if( ignore )
        return;
    insert_select_timer.stop();
    if( item_P == saved_current_item )
        return;
    saved_current_item = item_P;
    setSelected( saved_current_item, true );
    emit current_changed( saved_current_item );
    }

void KHListView::clear()
    {
    in_clear = true;
    KListView::clear();
    in_clear = false;
    slot_selection_changed( NULL );
    }
    
void KHListView::insertItem( QListViewItem* item_P )
    {
    bool set = false;
    if( !in_clear )
        set = childCount() == 0;
    KListView::insertItem( item_P );
    if( set && force_select )
        {
        bool block = signalsBlocked();
        blockSignals( true );
// SELI	tohle spis jen blokovat sebe?
        setCurrentItem( item_P );
        blockSignals( block );
        insert_select_timer.start( 0, true );
        }
    }

void KHListView::clearSelection()
    {
    KListView::clearSelection();
    slot_current_changed( currentItem());
    }

// items are often inserted using the QListViewItem constructor,
// which means that a derived class are not yet fully created
void KHListView::slot_insert_select()
    {
    if( ignore )
        return;
    slot_current_changed( currentItem());
    }

void KHListView::contentsDropEvent( QDropEvent* e )
    {
    bool save_ignore = ignore;
    ignore = true;
    KListView::contentsDropEvent( e );
    ignore = save_ignore;
    }

} // namespace KHotKeys

#include "khlistview.moc"
