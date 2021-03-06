/*
 *  searchwidget.h - part of the KDE Help Center
 *
 *  Copyright (C) 1999 Matthias Elter (me@kde.org)
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __searchwidget_h__
#define __searchwidget_h__

#include <qwidget.h>

#include <dcopobject.h>

#include "docmetainfo.h"

class QCheckBox;
class QListView;
class QListViewItem;
class QComboBox;

class KLanguageCombo;
class KConfig;

class KCMHelpCenter;

namespace KHC {

class ScopeItem;
class SearchEngine;

class SearchWidget : public QWidget, public DCOPObject
{
    Q_OBJECT
    K_DCOP

  k_dcop:
    ASYNC searchIndexUpdated(); // called from kcmhelpcenter

  public:
    SearchWidget ( SearchEngine *, QWidget *parent = 0 );
    ~SearchWidget();

    QString method();
    int pages();
    QString scope();

    QListView *listView() { return mScopeListView; }

    enum { ScopeDefault, ScopeAll, ScopeNone, ScopeCustom, ScopeNum };

    QString scopeSelectionLabel( int ) const;

    void readConfig( KConfig * );
    void writeConfig( KConfig * );

    int scopeCount() const;

    SearchEngine *engine() const { return mEngine; }

  signals:
    void searchResult( const QString &url );
    void scopeCountChanged( int );
    void showIndexDialog();

  public slots:
    void slotSwitchBoxes();
    void scopeSelectionChanged( int );
    void updateScopeList();

  protected:
    void checkScope();

  protected slots:
    void scopeDoubleClicked( QListViewItem * );
    void scopeClicked( QListViewItem * );

  private:
    void loadLanguages();

    SearchEngine *mEngine;

    QComboBox *mMethodCombo;
    QComboBox *mPagesCombo;
    QComboBox *mScopeCombo;
    QListView *mScopeListView;

    int mScopeCount;
};

}

#endif
// vim:ts=2:sw=2:et
