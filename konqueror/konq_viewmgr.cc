// -*- mode: c++; c-basic-offset: 2 -*-
/*  This file is part of the KDE project
    Copyright (C) 1999 Simon Hausmann <hausmann@kde.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

#include "konq_viewmgr.h"
#include "konq_view.h"
#include "konq_frame.h"
#include "konq_tabs.h"
#include "konq_profiledlg.h"
#include "konq_events.h"
#include "konq_settingsxt.h"

#include <qfileinfo.h>
#include <qptrlist.h>

#include <kaccelgen.h>
#include <kstandarddirs.h>
#include <kdebug.h>
#include <kapplication.h>
#include <kglobalsettings.h>
#include <ktempfile.h>
#include <dcopclient.h>
#include <klocale.h>
#include <kmessagebox.h>

#include <assert.h>
#include <kpopupmenu.h>

// #define DEBUG_VIEWMGR

template class QPtrList<KonqView>;

KonqViewManager::KonqViewManager( KonqMainWindow *mainWindow )
 : KParts::PartManager( mainWindow )
{
  m_pMainWindow = mainWindow;
  m_pDocContainer = 0L;

  m_pamProfiles = 0L;
  m_bProfileListDirty = true;
  m_bLoadingProfile = false;

  m_activePartChangedTimer = new QTimer(this);
  connect(m_activePartChangedTimer, SIGNAL(timeout()), this, SLOT(emitActivePartChanged()));
  connect( this, SIGNAL( activePartChanged ( KParts::Part * ) ), this, SLOT( slotActivePartChanged ( KParts::Part * ) ) );
}

KonqView* KonqViewManager::Initialize( const QString &serviceType, const QString &serviceName )
{
  //kdDebug(1202) << "KonqViewManager::Initialize()" << endl;
  KService::Ptr service;
  KTrader::OfferList partServiceOffers, appServiceOffers;
  KonqViewFactory newViewFactory = createView( serviceType, serviceName, service, partServiceOffers, appServiceOffers, true /*forceAutoEmbed*/ );
  if ( newViewFactory.isNull() )
  {
    kdDebug(1202) << "KonqViewManager::Initialize() No suitable factory found." << endl;
    return 0;
  }

  KonqView* childView = setupView( m_pMainWindow, newViewFactory, service, partServiceOffers, appServiceOffers, serviceType, false );

  setActivePart( childView->part() );
  m_pDocContainer = childView->frame();

    convertDocContainer();
  static_cast<KonqFrameTabs*>( m_pDocContainer )->setAlwaysTabbedMode(
      KonqSettings::alwaysTabbedMode() );

  m_pDocContainer->widget()->show();
  return childView;
}

KonqViewManager::~KonqViewManager()
{
  //kdDebug(1202) << "KonqViewManager::~KonqViewManager()" << endl;
  clear();
}

KonqView* KonqViewManager::splitView ( Qt::Orientation orientation,
                                       const QString &serviceType,
                                       const QString &serviceName,
                                       bool newOneFirst, bool forceAutoEmbed )
{
#ifdef DEBUG_VIEWMGR
  kdDebug(1202) << "KonqViewManager::splitView(ServiceType)" << endl;
  m_pMainWindow->dumpViewList();
  printFullHierarchy( m_pMainWindow );
#endif

  KonqFrame* splitFrame = m_pMainWindow->currentView()->frame();

  KService::Ptr service;
  KTrader::OfferList partServiceOffers, appServiceOffers;

  KonqViewFactory newViewFactory = createView( serviceType, serviceName, service, partServiceOffers, appServiceOffers, forceAutoEmbed );

  if( newViewFactory.isNull() )
    return 0L; //do not split at all if we can't create the new view

  assert( splitFrame );

  KonqFrameContainerBase* parentContainer = splitFrame->parentContainer();

  bool moveNewContainer = false;
  QValueList<int> splitterSizes;
  int index= -1;

  if (parentContainer->frameType()=="Container") {
    moveNewContainer = (static_cast<KonqFrameContainer*>(parentContainer)->idAfter( splitFrame->widget() ) != 0);
    splitterSizes = static_cast<KonqFrameContainer*>(parentContainer)->sizes();
  }
  else if (parentContainer->frameType()=="Tabs")
    index = static_cast<KonqFrameTabs*>(parentContainer)->indexOf( splitFrame->widget() );

#ifndef NDEBUG
  //printSizeInfo( splitFrame, parentContainer, "before split");
#endif

  parentContainer->widget()->setUpdatesEnabled( false );

  //kdDebug(1202) << "Move out child" << endl;
  QPoint pos = splitFrame->widget()->pos();
  parentContainer->removeChildFrame( splitFrame );
  splitFrame->widget()->reparent( m_pMainWindow, pos );

  //kdDebug(1202) << "Create new Container" << endl;
  KonqFrameContainer *newContainer = new KonqFrameContainer( orientation, parentContainer->widget(), parentContainer );
  connect(newContainer,SIGNAL(ctrlTabPressed()),m_pMainWindow,SLOT(slotCtrlTabPressed()));

  parentContainer->insertChildFrame( newContainer, index );
  if ( moveNewContainer ) {
    static_cast<KonqFrameContainer*>(parentContainer)->moveToFirst( newContainer );
    static_cast<KonqFrameContainer*>(parentContainer)->swapChildren();
  }

  //kdDebug(1202) << "Move in child" << endl;
  splitFrame->widget()->reparent( newContainer, pos );
  newContainer->insertChildFrame( splitFrame );

#ifndef NDEBUG
  //printSizeInfo( splitFrame, parentContainer, "after reparent" );
#endif

  //kdDebug(1202) << "Create new child" << endl;
  KonqView *newView = setupView( newContainer, newViewFactory, service, partServiceOffers, appServiceOffers, serviceType, false );

#ifndef DEBUG
  //printSizeInfo( splitFrame, parentContainer, "after child insert" );
#endif

  if ( newOneFirst )
  {
    newContainer->moveToLast( splitFrame->widget() );
    newContainer->swapChildren();
  }

  QValueList<int> newSplitterSizes;
  newSplitterSizes << 50 << 50;
  newContainer->setSizes( newSplitterSizes );

  if (parentContainer->frameType()=="Container") {
    static_cast<KonqFrameContainer*>(parentContainer)->setSizes( splitterSizes );
  }
  else if (parentContainer->frameType()=="Tabs")
    static_cast<KonqFrameTabs*>(parentContainer)->showPage( newContainer );

  splitFrame->show();
  //newView->frame()->show();
  newContainer->show();

  parentContainer->widget()->setUpdatesEnabled( true );

  if (m_pDocContainer == splitFrame) m_pDocContainer = newContainer;

  assert( newView->frame() );
  assert( newView->part() );
  newContainer->setActiveChild( newView->frame() );
  setActivePart( newView->part(), false );

#ifdef DEBUG_VIEWMGR
  m_pMainWindow->dumpViewList();
  printFullHierarchy( m_pMainWindow );
  kdDebug(1202) << "KonqViewManager::splitView(ServiceType) done" << endl;
#endif

  return newView;
}

KonqView* KonqViewManager::splitWindow( Qt::Orientation orientation,
                                        const QString &serviceType,
                                        const QString &serviceName,
                                        bool newOneFirst )
{
  kdDebug(1202) << "KonqViewManager::splitWindow(default)" << endl;

  // Don't crash when doing things too quickly.
  if (!m_pMainWindow || !m_pMainWindow->currentView())
    return 0L;

  KURL url = m_pMainWindow->currentView()->url();
  QString locationBarURL = m_pMainWindow->currentView()->locationBarURL();

  KService::Ptr service;
  KTrader::OfferList partServiceOffers, appServiceOffers;

  KonqViewFactory newViewFactory = createView( serviceType, serviceName, service, partServiceOffers, appServiceOffers );

  if( newViewFactory.isNull() )
    return 0L; //do not split at all if we can't create the new view

  KonqFrameBase* mainFrame = m_pMainWindow->childFrame();

  mainFrame->widget()->setUpdatesEnabled( false );

  //kdDebug(1202) << "Move out child" << endl;
  QPoint pos = mainFrame->widget()->pos();
  m_pMainWindow->removeChildFrame( mainFrame );

  KonqFrameContainer *newContainer = new KonqFrameContainer( orientation, m_pMainWindow, 0L);
  connect(newContainer,SIGNAL(ctrlTabPressed()),m_pMainWindow,SLOT(slotCtrlTabPressed()));

  m_pMainWindow->insertChildFrame( newContainer );

  newContainer->insertChildFrame( mainFrame );
  mainFrame->widget()->reparent( newContainer, pos );

  KonqView* childView = setupView( newContainer, newViewFactory, service, partServiceOffers, appServiceOffers, serviceType, true );

  if( newOneFirst ) {
      static_cast<KonqFrameContainer*>(newContainer)->moveToFirst( childView->frame() );
      static_cast<KonqFrameContainer*>(newContainer)->swapChildren();
  }

  newContainer->show();

  mainFrame->widget()->setUpdatesEnabled( true );

  if( childView )
    childView->openURL( url, locationBarURL );

  newContainer->setActiveChild( mainFrame );

#ifdef DEBUG_VIEWMGR
  m_pMainWindow->dumpViewList();
  printFullHierarchy( m_pMainWindow );
  kdDebug(1202) << "KonqViewManager::splitWindow(default) done" << endl;
#endif

  return childView;
}

void KonqViewManager::convertDocContainer()
{
  // Must create a tab container since one is not present,
  // then insert the existing frame as a tab

  KonqFrameContainerBase* parentContainer = m_pDocContainer->parentContainer();

  bool moveNewContainer = false;
  QValueList<int> splitterSizes;
  if (parentContainer->frameType()=="Container") {
    moveNewContainer = (static_cast<KonqFrameContainer*>(parentContainer)->idAfter( m_pDocContainer->widget() ) != 0);
    splitterSizes = static_cast<KonqFrameContainer*>(parentContainer)->sizes();
  }

  parentContainer->widget()->setUpdatesEnabled( false );

  //kdDebug(1202) << "Move out child" << endl;
  QPoint pos = m_pDocContainer->widget()->pos();
  parentContainer->removeChildFrame( m_pDocContainer );
  m_pDocContainer->widget()->reparent( m_pMainWindow, pos );

  KonqFrameTabs* newContainer = new KonqFrameTabs( parentContainer->widget() , parentContainer, this);
  parentContainer->insertChildFrame( newContainer );
  connect( newContainer, SIGNAL(ctrlTabPressed()), m_pMainWindow, SLOT(slotCtrlTabPressed()) );

  m_pDocContainer->widget()->reparent( newContainer, pos );
  newContainer->insertChildFrame( m_pDocContainer );

  if ( moveNewContainer ) {
    static_cast<KonqFrameContainer*>(parentContainer)->moveToFirst( newContainer );
    static_cast<KonqFrameContainer*>(parentContainer)->swapChildren();
  }
  if (parentContainer->frameType()=="Container")
    static_cast<KonqFrameContainer*>(parentContainer)->setSizes( splitterSizes );

  newContainer->show();

  parentContainer->widget()->setUpdatesEnabled( true );

  m_pDocContainer = newContainer;
}

KonqView* KonqViewManager::addTab(const QString &serviceType, const QString &serviceName, bool passiveMode, bool openAfterCurrentPage  )
{
#ifdef DEBUG_VIEWMGR
  kdDebug(1202) << "------------- KonqViewManager::addTab starting -------------" << endl;
  m_pMainWindow->dumpViewList();
  printFullHierarchy( m_pMainWindow );
#endif

  if (m_pDocContainer == 0L)
  {
    if (m_pMainWindow &&
        m_pMainWindow->currentView() &&
        m_pMainWindow->currentView()->frame()) {
       m_pDocContainer = m_pMainWindow->currentView()->frame();
    } else {
       kdDebug(1202) << "This view profile does not support tabs." << endl;
       return 0L;
    }
  }

  KService::Ptr service;
  KTrader::OfferList partServiceOffers, appServiceOffers;

  KonqViewFactory newViewFactory = createView( serviceType, serviceName, service, partServiceOffers, appServiceOffers, true /*forceAutoEmbed*/ );

  if( newViewFactory.isNull() )
    return 0L; //do not split at all if we can't create the new view

  if (m_pDocContainer->frameType() != "Tabs") convertDocContainer();

  KonqView* childView = setupView( static_cast<KonqFrameTabs*>(m_pDocContainer), newViewFactory, service, partServiceOffers, appServiceOffers, serviceType, passiveMode, openAfterCurrentPage );

#ifdef DEBUG_VIEWMGR
  m_pMainWindow->dumpViewList();
  printFullHierarchy( m_pMainWindow );
  kdDebug(1202) << "------------- KonqViewManager::addTab done -------------" << endl;
#endif

  return childView;
}

KonqView* KonqViewManager::addTabFromHistory( int steps, bool openAfterCurrentPage )
{
  if (m_pDocContainer == 0L)
  {
      if (m_pMainWindow &&
	  m_pMainWindow->currentView() &&
	  m_pMainWindow->currentView()->frame()) {
	  m_pDocContainer = m_pMainWindow->currentView()->frame();
      } else {
	  kdDebug(1202) << "This view profile does not support tabs." << endl;
	  return 0L;
      }
  }
  if (m_pDocContainer->frameType() != "Tabs") convertDocContainer();

  int oldPos = m_pMainWindow->currentView()->historyPos();
  int newPos = oldPos + steps;

  const HistoryEntry * he = m_pMainWindow->currentView()->historyAt(newPos);  
  if(!he)
      return 0L;

  KonqView* newView = 0L;
  newView  = addTab( he->strServiceType, he->strServiceName, false, openAfterCurrentPage );
    
  if(!newView)
      return 0L;

  newView->copyHistory(m_pMainWindow->currentView());
  newView->setHistoryPos(newPos);
  newView->restoreHistory();

  return newView;
}


void KonqViewManager::duplicateTab( KonqFrameBase* tab, bool openAfterCurrentPage )
{
#ifdef DEBUG_VIEWMGR
  kdDebug(1202) << "---------------- KonqViewManager::duplicateTab( " << tab << " ) --------------" << endl;
  m_pMainWindow->dumpViewList();
  printFullHierarchy( m_pMainWindow );
#endif

  if (m_pDocContainer == 0L)
  {
    if (m_pMainWindow &&
        m_pMainWindow->currentView() &&
        m_pMainWindow->currentView()->frame()) {
       m_pDocContainer = m_pMainWindow->currentView()->frame();
    } else {
       kdDebug(1202) << "This view profile does not support tabs." << endl;
       return;
    }
  }

  if (m_pDocContainer->frameType() != "Tabs") convertDocContainer();

  KonqFrameTabs* tabContainer = static_cast<KonqFrameTabs*>(m_pDocContainer);

  KonqFrameBase* currentFrame;
  if ( tab == 0L )
    currentFrame = dynamic_cast<KonqFrameBase*>(tabContainer->currentPage());
  else
    currentFrame = tab;

  if (!currentFrame) {
    return;
  }

  KTempFile tempFile;
  tempFile.setAutoDelete( true );
  KConfig config( tempFile.name() );
  config.setGroup( "View Profile" );

  QString prefix = QString::fromLatin1( currentFrame->frameType() ) + QString::number(0);
  config.writeEntry( "RootItem", prefix );
  prefix.append( '_' );
  currentFrame->saveConfig( &config, prefix, true, 0L, 0, 1);

  QString rootItem = config.readEntry( "RootItem", "empty" );

  if (rootItem.isNull() || rootItem == "empty") return;

  // This flag is used by KonqView, to distinguish manual view creation
  // from profile loading (e.g. in switchView)
  m_bLoadingProfile = true;

  loadItem( config, tabContainer, rootItem, KURL(""), true, openAfterCurrentPage );

  m_bLoadingProfile = false;

  m_pMainWindow->enableAllActions(true);

  // This flag disables calls to viewCountChanged while creating the views,
  // so we do it once at the end :
  m_pMainWindow->viewCountChanged();

  if (openAfterCurrentPage)
    tabContainer->setCurrentPage( tabContainer->currentPageIndex () + 1 );
  else
  tabContainer->setCurrentPage( tabContainer->count() - 1 );

  KonqFrameBase* duplicatedFrame = dynamic_cast<KonqFrameBase*>(tabContainer->currentPage());
  if (duplicatedFrame)
    duplicatedFrame->copyHistory( currentFrame );

#ifdef DEBUG_VIEWMGR
  m_pMainWindow->dumpViewList();
  printFullHierarchy( m_pMainWindow );
  kdDebug(1202) << "------------- KonqViewManager::duplicateTab done --------------" << endl;
#endif
}

void KonqViewManager::breakOffTab( KonqFrameBase* tab )
{
#ifdef DEBUG_VIEWMGR
  kdDebug(1202) << "---------------- KonqViewManager::breakOffTab( " << tab << " ) --------------" << endl;
  m_pMainWindow->dumpViewList();
  printFullHierarchy( m_pMainWindow );
#endif

  if (m_pDocContainer == 0L) return;
  if (m_pDocContainer->frameType() != "Tabs") return;

  KonqFrameTabs* tabContainer = static_cast<KonqFrameTabs*>(m_pDocContainer);

  int width = m_pMainWindow->width();
  int height = m_pMainWindow->height();

  KonqFrameBase* currentFrame;
  if ( tab == 0L )
    currentFrame = dynamic_cast<KonqFrameBase*>(tabContainer->currentPage());
  else
    currentFrame = tab;

  if (!currentFrame) {
    return;
  }

  KTempFile tempFile;
  tempFile.setAutoDelete( true );
  KConfig config( tempFile.name() );
  config.setGroup( "View Profile" );

  QString prefix = QString::fromLatin1( currentFrame->frameType() ) + QString::number(0);
  config.writeEntry( "RootItem", prefix );
  prefix.append( '_' );
  config.writeEntry( QString::fromLatin1( "docContainer" ).prepend( prefix ), true );
  currentFrame->saveConfig( &config, prefix, true, 0L, 0, 1);

  KonqMainWindow *mainWindow = new KonqMainWindow( KURL(), false );
  if (mainWindow == 0L) return;

  mainWindow->viewManager()->loadViewProfile( config, "" );

  KonqFrameBase * newDocContainer = mainWindow->viewManager()->docContainer();
  if( newDocContainer && newDocContainer->frameType() == "Tabs")
  {
    KonqFrameTabs *kft = static_cast<KonqFrameTabs *>(newDocContainer);
    KonqFrameBase *newFrame = dynamic_cast<KonqFrameBase*>(kft->currentPage());
    if(newFrame)
      newFrame->copyHistory( currentFrame );
  }

  removeTab( currentFrame );

  mainWindow->enableAllActions( true );

  mainWindow->resize( width, height );

  mainWindow->activateChild();

  mainWindow->show();

#ifdef DEBUG_VIEWMGR
  m_pMainWindow->dumpViewList();
  printFullHierarchy( m_pMainWindow );

  mainWindow->dumpViewList();
  mainWindow->viewManager()->printFullHierarchy( mainWindow );

  kdDebug(1202) << "------------- KonqViewManager::breakOffTab done --------------" << endl;
#endif
}

void KonqViewManager::removeTab( KonqFrameBase* tab )
{
#ifdef DEBUG_VIEWMGR
  kdDebug(1202) << "---------------- KonqViewManager::removeTab( " << tab << " ) --------------" << endl;
  m_pMainWindow->dumpViewList();
  printFullHierarchy( m_pMainWindow );
#endif

  if (m_pDocContainer == 0L)
    return;
  if (m_pDocContainer->frameType() != "Tabs" )
    return;

  KonqFrameTabs* tabContainer = static_cast<KonqFrameTabs*>(m_pDocContainer);

  KonqFrameBase* currentFrame;
  if ( tab != 0L ) {
    currentFrame = tab;
  } else {
    currentFrame = dynamic_cast<KonqFrameBase*>(tabContainer->currentPage());
    if (!currentFrame) {
      return;
    }
  }

  if ( tabContainer->count() == 1 )
    return;

  if (currentFrame->widget() == tabContainer->currentPage())
    setActivePart( 0L, true );

  tabContainer->removeChildFrame(currentFrame);

  QPtrList<KonqView> viewList;
  QPtrListIterator<KonqView> it( viewList );

  currentFrame->listViews( &viewList );

  for ( it.toFirst(); it != 0L; ++it )
  {
    if (it.current() == m_pMainWindow->currentView())
      setActivePart( 0L, true );
    m_pMainWindow->removeChildView( it.current() );
    delete it.current();
  }

  delete currentFrame;

  tabContainer->slotCurrentChanged(tabContainer->currentPage());

#ifdef DEBUG_VIEWMGR
  m_pMainWindow->dumpViewList();
  printFullHierarchy( m_pMainWindow );
  kdDebug(1202) << "------------- KonqViewManager::removeTab done --------------" << endl;
#endif
}

void KonqViewManager::reloadAllTabs( )
{
  if (m_pDocContainer == 0L) return;
  if (m_pDocContainer->frameType() != "Tabs") return;

  KonqFrameTabs* tabContainer = static_cast<KonqFrameTabs*>(m_pDocContainer);

  QPtrList<KonqFrameBase> frameList = *tabContainer->childFrameList();
  QPtrListIterator<KonqFrameBase> it( frameList );

  for ( it.toFirst(); it != 0L; ++it )
  {
      if ( it.current()->activeChildView())
      {
          if( !it.current()->activeChildView()->locationBarURL().isEmpty())
              it.current()->activeChildView()->openURL( it.current()->activeChildView()->url(),it.current()->activeChildView()->locationBarURL());
      }
  }
}

void KonqViewManager::removeOtherTabs( KonqFrameBase* tab )
{

  if (m_pDocContainer == 0L) return;
  if (m_pDocContainer->frameType() != "Tabs") return;

  KonqFrameTabs* tabContainer = static_cast<KonqFrameTabs*>(m_pDocContainer);

  KonqFrameBase *currentFrame;

  if ( tab == 0L )
    currentFrame = dynamic_cast<KonqFrameBase*>(tabContainer->currentPage());
  else
    currentFrame = tab;

  if (!currentFrame) {
    return;
  }

  QPtrList<KonqFrameBase> frameList = *tabContainer->childFrameList();
  QPtrListIterator<KonqFrameBase> it( frameList );

  for ( it.toFirst(); it != 0L; ++it )
  {
    if( it.current() != currentFrame )
      removeTab(it.current());
  }

}

void KonqViewManager::moveTabBackward()
{
    if (m_pDocContainer == 0L) return;
    if (m_pDocContainer->frameType() != "Tabs") return;
    KonqFrameTabs* tabContainer = static_cast<KonqFrameTabs*>(m_pDocContainer);
    if( tabContainer->count() == 1 ) return;

    int iTab = tabContainer->currentPageIndex();
    kdDebug()<<" tabContainer->currentPageIndex(); :"<<iTab<<endl;
    tabContainer->moveTabBackward(iTab);
}

void KonqViewManager::moveTabForward()
{
    if (m_pDocContainer == 0L) return;
    if (m_pDocContainer->frameType() != "Tabs") return;
    KonqFrameTabs* tabContainer = static_cast<KonqFrameTabs*>(m_pDocContainer);
    if( tabContainer->count() == 1 ) return;

    int iTab = tabContainer->currentPageIndex();
    tabContainer->moveTabForward(iTab);
}

void KonqViewManager::activateNextTab()
{
  if (m_pDocContainer == 0L) return;
  if (m_pDocContainer->frameType() != "Tabs") return;

  KonqFrameTabs* tabContainer = static_cast<KonqFrameTabs*>(m_pDocContainer);
  if( tabContainer->count() == 1 ) return;

  int iTab = tabContainer->currentPageIndex();

  iTab++;

  if( iTab == tabContainer->count() )
    iTab = 0;

  tabContainer->setCurrentPage( iTab );
}

void KonqViewManager::activatePrevTab()
{
  if (m_pDocContainer == 0L) return;
  if (m_pDocContainer->frameType() != "Tabs") return;

  KonqFrameTabs* tabContainer = static_cast<KonqFrameTabs*>(m_pDocContainer);
  if( tabContainer->count() == 1 ) return;

  int iTab = tabContainer->currentPageIndex();

  iTab--;

  if( iTab == -1 )
    iTab = tabContainer->count() - 1;

  tabContainer->setCurrentPage( iTab );
}

void KonqViewManager::activateTab(int position)
{
  if (m_pDocContainer == 0L) return;
  if (m_pDocContainer->frameType() != "Tabs") return;

  KonqFrameTabs* tabContainer = static_cast<KonqFrameTabs*>(m_pDocContainer);
  if (position<0 || tabContainer->count() == 1 || position>=tabContainer->count()) return;

  tabContainer->setCurrentPage( position );
}

void KonqViewManager::showTab( KonqView *view )
{
  KonqFrameTabs *tabContainer = static_cast<KonqFrameTabs*>( docContainer() );
  if (tabContainer->currentPage() != view->frame())
  {
    tabContainer->showPage( view->frame() );
    emitActivePartChanged();
  }
}

void KonqViewManager::updatePixmaps()
{
  if (m_pDocContainer == 0L) return;
  if (m_pDocContainer->frameType() != "Tabs") return;

  KonqFrameTabs* tabContainer = static_cast<KonqFrameTabs*>(m_pDocContainer);

  QPtrList<KonqView> viewList;
  QPtrListIterator<KonqView> it( viewList );

  tabContainer->listViews( &viewList );
  for ( it.toFirst(); it != 0L; ++it )
    it.current()->setTabIcon( KURL::fromPathOrURL( it.current()->locationBarURL() ) );
}

void KonqViewManager::removeView( KonqView *view )
{
#ifdef DEBUG_VIEWMGR
  kdDebug(1202) << "---------------- removeView --------------" << view << endl;
  m_pMainWindow->dumpViewList();
  printFullHierarchy( m_pMainWindow );
#endif

  if (!view)
    return;

  KonqFrame* frame = view->frame();
  KonqFrameContainerBase* parentContainer = frame->parentContainer();

  kdDebug(1202) << "view=" << view << " frame=" << frame << " parentContainer=" << parentContainer << endl;

  if (parentContainer->frameType()=="Container")
  {
    kdDebug(1202) << "parentContainer is a KonqFrameContainer" << endl;

    KonqFrameContainerBase* grandParentContainer = parentContainer->parentContainer();
    kdDebug(1202) << "grandParentContainer=" << grandParentContainer << endl;

    setActivePart( 0L, true );

    int index = -1;
    QValueList<int> splitterSizes;
    bool moveOtherChild = false;

    if (grandParentContainer->frameType()=="Tabs")
      index = static_cast<KonqFrameTabs*>(grandParentContainer)->indexOf( parentContainer->widget() );
    else if (grandParentContainer->frameType()=="Container")
    {
      moveOtherChild = (static_cast<KonqFrameContainer*>(grandParentContainer)->idAfter( parentContainer->widget() ) != 0);
      splitterSizes = static_cast<KonqFrameContainer*>(grandParentContainer)->sizes();
    }

    KonqFrameBase* otherFrame = static_cast<KonqFrameContainer*>(parentContainer)->otherChild( frame );
    kdDebug(1202) << "otherFrame=" << otherFrame << endl;

    if( otherFrame == 0L )
    {
        kdWarning(1202) << "KonqViewManager::removeView: This shouldn't happen!" << endl;
        return;
    }

    if (m_pDocContainer == parentContainer) m_pDocContainer = otherFrame;

    grandParentContainer->widget()->setUpdatesEnabled( false );
    static_cast<KonqFrameContainer*>(parentContainer)->setAboutToBeDeleted();

    //kdDebug(1202) << "--- Reparenting otherFrame to m_pMainWindow " << m_pMainWindow << endl;
    QPoint pos = otherFrame->widget()->pos();
    otherFrame->reparentFrame( m_pMainWindow, pos );

    //kdDebug(1202) << "--- Removing otherFrame from parentContainer" << endl;
    parentContainer->removeChildFrame( otherFrame );

    //kdDebug(1202) << "--- Removing parentContainer from grandParentContainer" << endl;
    grandParentContainer->removeChildFrame( parentContainer );

    //kdDebug(1202) << "--- Removing view from view list" << endl;
    m_pMainWindow->removeChildView(view);
    //kdDebug(1202) << "--- Deleting view " << view << endl;
    delete view; // This deletes the view, which deletes the part, which deletes its widget

    //kdDebug(1202) << "--- Deleting parentContainer " << parentContainer
    //              << ". Its parent is " << parentContainer->widget()->parent() << endl;
    delete parentContainer;

    //kdDebug(1202) << "--- Reparenting otherFrame to grandParentContainer" << grandParentContainer << endl;
    otherFrame->reparentFrame( grandParentContainer->widget(), pos );

    //kdDebug(1202) << "--- Inserting otherFrame into grandParentContainer" << grandParentContainer << endl;
    grandParentContainer->insertChildFrame( otherFrame, index );

    if( moveOtherChild )
    {
      static_cast<KonqFrameContainer*>(grandParentContainer)->moveToFirst( otherFrame->widget() );
      static_cast<KonqFrameContainer*>(grandParentContainer)->swapChildren();
    }

    if (grandParentContainer->frameType()=="Container")
      static_cast<KonqFrameContainer*>(grandParentContainer)->setSizes( splitterSizes );

    otherFrame->widget()->show();

    grandParentContainer->setActiveChild( otherFrame );
    grandParentContainer->activateChild();

    grandParentContainer->widget()->setUpdatesEnabled( true );
  }
  else if (parentContainer->frameType()=="Tabs") {
    kdDebug(1202) << "parentContainer " << parentContainer << " is a KonqFrameTabs" << endl;

    removeTab( frame );
  }
  else if (parentContainer->frameType()=="MainWindow")
    kdDebug(1202) << "parentContainer is a KonqMainWindow.  This shouldn't be removeable, not removing." << endl;
  else
    kdDebug(1202) << "Unrecognized frame type, not removing." << endl;

#ifdef DEBUG_VIEWMGR
  printFullHierarchy( m_pMainWindow );
  m_pMainWindow->dumpViewList();

  kdDebug(1202) << "------------- removeView done --------------" << view << endl;
#endif
}

// reimplemented from PartManager
void KonqViewManager::removePart( KParts::Part * part )
{
  kdDebug(1202) << "KonqViewManager::removePart ( " << part << " )" << endl;
  // This is called when a part auto-deletes itself (case 1), or when
  // the "delete view" above deletes, in turn, the part (case 2)

  kdDebug(1202) << "Calling KParts::PartManager::removePart " << part << endl;
  KParts::PartManager::removePart( part );

  // If we were called by PartManager::slotObjectDestroyed, then the inheritance has
  // been deleted already... Can't use inherits().

  KonqView * view = m_pMainWindow->childView( static_cast<KParts::ReadOnlyPart *>(part) );
  if ( view ) // the child view still exists, so we are in case 1
  {
      kdDebug(1202) << "Found a child view" << endl;
      view->partDeleted(); // tell the child view that the part auto-deletes itself
      if (m_pMainWindow->mainViewsCount() == 1)
      {
        kdDebug(1202) << "Deleting last view -> closing the window" << endl;
        clear();
        kdDebug(1202) << "Closing m_pMainWindow " << m_pMainWindow << endl;
        m_pMainWindow->close(); // will delete it
        return;
      } else { // normal case
        removeView( view );
      }
  }

  kdDebug(1202) << "KonqViewManager::removePart ( " << part << " ) done" << endl;
}

void KonqViewManager::slotPassiveModePartDeleted()
{
  // Passive mode parts aren't registered to the part manager,
  // so we have to handle suicidal ones ourselves
  KParts::ReadOnlyPart * part = const_cast<KParts::ReadOnlyPart *>( static_cast<const KParts::ReadOnlyPart *>( sender() ) );
  disconnect( part, SIGNAL( destroyed() ), this, SLOT( slotPassiveModePartDeleted() ) );
  kdDebug(1202) << "KonqViewManager::slotPassiveModePartDeleted part=" << part << endl;
  KonqView * view = m_pMainWindow->childView( part );
  kdDebug(1202) << "view=" << view << endl;
  if ( view != 0L) // the child view still exists, so the part suicided
  {
    view->partDeleted(); // tell the child view that the part deleted itself
    removeView( view );
  }
}

void KonqViewManager::viewCountChanged()
{
  bool bShowActiveViewIndicator = ( m_pMainWindow->viewCount() > 1 );
  bool bShowLinkedViewIndicator = ( m_pMainWindow->linkableViewsCount() > 1 );

  KonqMainWindow::MapViews mapViews = m_pMainWindow->viewMap();
  KonqMainWindow::MapViews::Iterator it = mapViews.begin();
  KonqMainWindow::MapViews::Iterator end = mapViews.end();
  for (  ; it != end ; ++it )
  {
    KonqFrameStatusBar* sb = it.data()->frame()->statusbar();
    sb->showActiveViewIndicator( bShowActiveViewIndicator && !it.data()->isPassiveMode() );
    sb->showLinkedViewIndicator( bShowLinkedViewIndicator && !it.data()->isFollowActive() );
  }
}

void KonqViewManager::clear()
{
  kdDebug(1202) << "KonqViewManager::clear" << endl;
  setActivePart( 0L, true /* immediate */ );

  if (m_pMainWindow->childFrame() == 0L) return;

  QPtrList<KonqView> viewList;

  m_pMainWindow->listViews( &viewList );

  kdDebug(1202) << viewList.count() << " items" << endl;

  QPtrListIterator<KonqView> it( viewList );
  for ( it.toFirst(); it.current(); ++it ) {
    m_pMainWindow->removeChildView( it.current() );
    kdDebug(1202) << "Deleting " << it.current() << endl;
    delete it.current();
  }

  kdDebug(1202) << "deleting mainFrame " << endl;
  KonqFrameBase* frame = m_pMainWindow->childFrame();
  m_pMainWindow->removeChildFrame( frame ); // will set childFrame() to NULL
  delete frame;

  m_pDocContainer = 0L;
}

KonqView *KonqViewManager::chooseNextView( KonqView *view )
{
  //kdDebug(1202) << "KonqViewManager(" << this << ")::chooseNextView(" << view << ")" << endl;
  KonqMainWindow::MapViews mapViews = m_pMainWindow->viewMap();

  KonqMainWindow::MapViews::Iterator it = mapViews.begin();
  KonqMainWindow::MapViews::Iterator end = mapViews.end();
  if ( view ) // find it in the map - can't use the key since view->part() might be 0L
      while ( it != end && it.data() != view )
          ++it;

  // the view should always be in the list
   if ( it == end ) {
     if ( view )
       kdWarning() << "View " << view << " is not in list !" << endl;
     it = mapViews.begin();
     if ( it == end )
       return 0L; // We have no view at all - this used to happen with totally-empty-profiles
   }

  KonqMainWindow::MapViews::Iterator startIt = it;

  //kdDebug(1202) << "KonqViewManager::chooseNextView: count=" << mapViews.count() << endl;
  while ( true )
  {
    //kdDebug(1202) << "*KonqViewManager::chooseNextView going next" << endl;
    if ( ++it == end ) // move to next
      it = mapViews.begin(); // rewind on end

    if ( it == startIt && view )
      break; // no next view found

    KonqView *nextView = it.data();
    if ( nextView && !nextView->isPassiveMode() )
      return nextView;
    //kdDebug(1202) << "KonqViewManager::chooseNextView nextView=" << nextView << " passive=" << nextView->isPassiveMode() << endl;
  }

  //kdDebug(1202) << "KonqViewManager::chooseNextView: returning 0L" << endl;
  return 0L; // no next view found
}

KonqViewFactory KonqViewManager::createView( const QString &serviceType,
                                          const QString &serviceName,
                                          KService::Ptr &service,
                                          KTrader::OfferList &partServiceOffers,
                                          KTrader::OfferList &appServiceOffers,
                                          bool forceAutoEmbed )
{
  kdDebug(1202) << "KonqViewManager::createView" << endl;
  KonqViewFactory viewFactory;

  if( serviceType.isEmpty() && m_pMainWindow->currentView() ) {
    //clone current view
    KonqView *cv = m_pMainWindow->currentView();
    QString _serviceType, _serviceName;
    if ( cv->service()->desktopEntryName() == "konq_sidebartng" ) {
      _serviceType = "text/html";
    }
    else {
      _serviceType = cv->serviceType();
      _serviceName = cv->service()->desktopEntryName();
    }

    viewFactory = KonqFactory::createView( _serviceType, _serviceName,
                                           &service, &partServiceOffers, &appServiceOffers, forceAutoEmbed );
  }
  else {
    //create view with the given servicetype
    viewFactory = KonqFactory::createView( serviceType, serviceName,
                                           &service, &partServiceOffers, &appServiceOffers, forceAutoEmbed );
  }

  return viewFactory;
}

KonqView *KonqViewManager::setupView( KonqFrameContainerBase *parentContainer,
                                      KonqViewFactory &viewFactory,
                                      const KService::Ptr &service,
                                      const KTrader::OfferList &partServiceOffers,
                                      const KTrader::OfferList &appServiceOffers,
                                      const QString &serviceType,
                                      bool passiveMode,
                                      bool openAfterCurrentPage )
{
  kdDebug(1202) << "KonqViewManager::setupView passiveMode=" << passiveMode << endl;

  QString sType = serviceType;

  if ( sType.isEmpty() )
    sType = m_pMainWindow->currentView()->serviceType();

  //kdDebug(1202) << "KonqViewManager::setupView creating KonqFrame with parent=" << parentContainer << endl;
  KonqFrame* newViewFrame = new KonqFrame( parentContainer->widget(), parentContainer, "KonqFrame" );
  newViewFrame->setGeometry( 0, 0, m_pMainWindow->width(), m_pMainWindow->height() );

  //kdDebug(1202) << "Creating KonqView" << endl;
  KonqView *v = new KonqView( viewFactory, newViewFrame,
                              m_pMainWindow, service, partServiceOffers, appServiceOffers, sType, passiveMode );
  //kdDebug(1202) << "KonqView created - v=" << v << " v->part()=" << v->part() << endl;

  QObject::connect( v, SIGNAL( sigPartChanged( KonqView *, KParts::ReadOnlyPart *, KParts::ReadOnlyPart * ) ),
                    m_pMainWindow, SLOT( slotPartChanged( KonqView *, KParts::ReadOnlyPart *, KParts::ReadOnlyPart * ) ) );

  m_pMainWindow->insertChildView( v );


  int index = -1;

  if (m_pDocContainer && m_pDocContainer->frameType() == "Tabs")
  {
      KonqFrameTabs* tabContainer = static_cast<KonqFrameTabs*>(m_pDocContainer);
      if ( openAfterCurrentPage )
          index = tabContainer->currentPageIndex() +1 ;
  }

  parentContainer->insertChildFrame( newViewFrame, index );

  if (parentContainer->frameType() != "Tabs") newViewFrame->show();

  // Don't register passive views to the part manager
  if ( !v->isPassiveMode() ) // note that KonqView's constructor could set this to true even if passiveMode is false
    addPart( v->part(), false );
  else
  {
    // Passive views aren't registered, but we still want to detect the suicidal ones
    connect( v->part(), SIGNAL( destroyed() ), this, SLOT( slotPassiveModePartDeleted() ) );
  }

  //kdDebug(1202) << "KonqViewManager::setupView done" << endl;
  return v;
}

///////////////// Profile stuff ////////////////

void KonqViewManager::saveViewProfile( const QString & fileName, const QString & profileName, bool saveURLs, bool saveWindowSize )
{

  QString path = locateLocal( "data", QString::fromLatin1( "konqueror/profiles/" ) +
                                          fileName, KGlobal::instance() );

  if ( QFile::exists( path ) )
    QFile::remove( path );

  KSimpleConfig cfg( path );
  cfg.setGroup( "Profile" );
  if ( !profileName.isEmpty() )
      cfg.writePathEntry( "Name", profileName );

  saveViewProfile( cfg, saveURLs, saveWindowSize );

}

void KonqViewManager::saveViewProfile( KConfig & cfg, bool saveURLs, bool saveWindowSize )
{
  //kdDebug(1202) << "KonqViewManager::saveViewProfile" << endl;
  if( m_pMainWindow->childFrame() != 0L ) {
    QString prefix = QString::fromLatin1( m_pMainWindow->childFrame()->frameType() )
                     + QString::number(0);
    cfg.writeEntry( "RootItem", prefix );
    prefix.append( '_' );
    m_pMainWindow->saveConfig( &cfg, prefix, saveURLs, m_pDocContainer, 0, 1);
  }

  cfg.writeEntry( "FullScreen", m_pMainWindow->fullScreenMode());
  cfg.writeEntry("XMLUIFile", m_pMainWindow->xmlFile());
  if ( saveWindowSize )
  {
    cfg.writeEntry( "Width", m_pMainWindow->width() );
    cfg.writeEntry( "Height", m_pMainWindow->height() );
  }

  // Save menu/toolbar settings in profile. Relys on konq_mainwindow calling
  // setAutoSaveSetting( "KongMainWindow", false ). The false is important,
  // we do not want this call save size settings in the profile, because we
  // do it ourselves. Save in a separate group than the rest of the profile.
  QString savedGroup = cfg.group();
  m_pMainWindow->saveMainWindowSettings( &cfg, "Main Window Settings" );
  cfg.setGroup( savedGroup );

  cfg.sync();
}

void KonqViewManager::loadViewProfile( const QString & path, const QString & filename,
                                       const KURL & forcedURL, const KonqOpenURLRequest &req,
                                       bool resetWindow, bool openURL )
{
  KConfig cfg( path, true );
  cfg.setDollarExpansion( true );
  cfg.setGroup( "Profile" );
  loadViewProfile( cfg, filename, forcedURL, req, resetWindow, openURL );
}

void KonqViewManager::loadViewProfile( KConfig &cfg, const QString & filename,
                                       const KURL & forcedURL, const KonqOpenURLRequest &req,
                                       bool resetWindow, bool openURL )
{
  if ( docContainer() && docContainer()->frameType()=="Tabs" )
  {
      KonqFrameTabs* tabContainer = static_cast<KonqFrameTabs*>(docContainer());
      if ( tabContainer->count() > 1 )
      {
          if ( KMessageBox::warningContinueCancel( 0,
                  i18n("You have multiple tabs open in this window.\n"
                        "Loading a view profile will close them."),
                  i18n("Confirmation"),
                  i18n("Load View Profile"),
                  "LoadProfileTabsConfirm" ) == KMessageBox::Cancel )
              return;
      }

      KonqView *originalView = m_pMainWindow->currentView();
      QPtrList<KonqFrameBase> frameList = *tabContainer->childFrameList();
      QPtrListIterator<KonqFrameBase> it( frameList );
      for ( it.toFirst(); it != 0L; ++it )
      {
          KonqView *view = it.current()->activeChildView();
          if (view && view->part() && (view->part()->metaObject()->findProperty("modified") != -1)) {
            QVariant prop = view->part()->property("modified");
            if (prop.isValid() && prop.toBool()) {
                showTab( view );
                if ( KMessageBox::warningContinueCancel( 0,
                   i18n("This tab contains changes that have not been submitted.\nLoading a profile will discard these changes."),
                   i18n("Discard Changes?"), i18n("&Discard Changes"), "discardchangesloadprofile") != KMessageBox::Continue )
                {
                    showTab( originalView );
                    return;
                }
            }
         }
      }
      showTab( originalView );
  }
  else
  {
      KonqView *view = m_pMainWindow->currentView();
      if (view && view->part() && (view->part()->metaObject()->findProperty("modified") != -1)) {
        QVariant prop = view->part()->property("modified");
        if (prop.isValid() && prop.toBool())
            if ( KMessageBox::warningContinueCancel( 0,
               i18n("This page contains changes that have not been submitted.\nLoading a profile will discard these changes."),
	       i18n("Discard Changes?"), i18n("&Discard Changes"), "discardchangesloadprofile") != KMessageBox::Continue )
            return;
      }
  }

  bool alwaysTabbedMode = KonqSettings::alwaysTabbedMode();

  m_currentProfile = filename;
  m_currentProfileText = cfg.readPathEntry("Name",filename);
  m_profileHomeURL = cfg.readEntry("HomeURL", QString::null);

  m_pMainWindow->currentProfileChanged();
  KURL defaultURL;
  if ( m_pMainWindow->currentView() )
    defaultURL = m_pMainWindow->currentView()->url();

  clear();

  QString rootItem = cfg.readEntry( "RootItem", "empty" );

  //kdDebug(1202) << "KonqViewManager::loadViewProfile : loading RootItem " << rootItem << 
  //" forcedURL " << forcedURL.url() << endl;

  if ( forcedURL.url() != "about:blank" )
  {
    // This flag is used by KonqView, to distinguish manual view creation
    // from profile loading (e.g. in switchView)
    m_bLoadingProfile = true;

    loadItem( cfg, m_pMainWindow, rootItem, defaultURL, openURL && forcedURL.isEmpty() );

    m_bLoadingProfile = false;

    m_pMainWindow->enableAllActions(true);

    // This flag disables calls to viewCountChanged while creating the views,
    // so we do it once at the end :
    m_pMainWindow->viewCountChanged();
  }
  else
  {
    m_pMainWindow->disableActionsNoView();
    m_pMainWindow->action( "clear_location" )->activate();
  }

  //kdDebug(1202) << "KonqViewManager::loadViewProfile : after loadItem " << endl;

  if (m_pDocContainer == 0L)
  {
    if (m_pMainWindow &&
        m_pMainWindow->currentView() &&
        m_pMainWindow->currentView()->frame()) {
       m_pDocContainer = m_pMainWindow->currentView()->frame();
    } else {
       kdDebug(1202) << "This view profile does not support tabs." << endl;
       return;
    }
  }


  if ( m_pDocContainer->frameType() != "Tabs")
    convertDocContainer();

  static_cast<KonqFrameTabs*>( m_pDocContainer )->setAlwaysTabbedMode( alwaysTabbedMode );

  // Set an active part first so that we open the URL in the current view
  // (to set the location bar correctly and asap)
  KonqView *nextChildView = 0L;
  nextChildView = m_pMainWindow->activeChildView();
  if (nextChildView == 0L) nextChildView = chooseNextView( 0L );
  setActivePart( nextChildView ? nextChildView->part() : 0L, true /* immediate */);

  // #71164
  if ( !req.args.frameName.isEmpty() && nextChildView ) {
      nextChildView->setViewName( req.args.frameName );
  }

  if ( openURL && !forcedURL.isEmpty())
  {
      KonqOpenURLRequest _req(req);
      _req.openAfterCurrentPage = KonqSettings::openAfterCurrentPage();
      _req.forceAutoEmbed = true; // it's a new window, let's use it

      m_pMainWindow->openURL( nextChildView /* can be 0 for an empty profile */,
                              forcedURL, _req.args.serviceType, _req, _req.args.trustedSource );

      // TODO choose a linked view if any (instead of just the first one),
      // then open the same URL in any non-linked one
  }
  else
  {
      if ( m_pMainWindow->locationBarURL().isEmpty() ) // No URL -> the user will want to type one
          m_pMainWindow->focusLocationBar();
  }

  // Window size
  if ( !m_pMainWindow->initialGeometrySet() )
  {
     if (cfg.readBoolEntry( "FullScreen" ))
     {
         // Full screen on
         m_pMainWindow->showFullScreen();
     }
     else
     {
         // Full screen off
         if( m_pMainWindow->isFullScreen())
             m_pMainWindow->showNormal();

         QSize size = readConfigSize( cfg, m_pMainWindow );
         if ( size.isValid() )
             m_pMainWindow->resize( size );
         else  // no size in the profile; use last known size
             m_pMainWindow->restoreWindowSize();
     }
  }

  if( resetWindow )
  { // force default settings for the GUI
     m_pMainWindow->applyMainWindowSettings( KGlobal::config(), "KonqMainWindow", true );
  }

  // Apply menu/toolbar settings saved in profile. Read from a separate group
  // so that the window doesn't try to change the size stored in the Profile group.
  // (If applyMainWindowSettings finds a "Width" or "Height" entry, it
  // sets them to 0,0)
  QString savedGroup = cfg.group();
  m_pMainWindow->applyMainWindowSettings( &cfg, "Main Window Settings" );
  cfg.setGroup( savedGroup );

#ifdef DEBUG_VIEWMGR
  printFullHierarchy( m_pMainWindow );
#endif

  //kdDebug(1202) << "KonqViewManager::loadViewProfile done" << endl;
}

void KonqViewManager::setActivePart( KParts::Part *part, QWidget * )
{
    setActivePart( part, false );
}

void KonqViewManager::setActivePart( KParts::Part *part, bool immediate )
{
    //kdDebug(1202) << "KonqViewManager::setActivePart " << part << endl;
    //if ( part )
    //    kdDebug(1202) << "    " << part->className() << " " << part->name() << endl;

    // Due to the single-shot timer below, we need to also make sure that
    // the mainwindow also has the right part active already
    KParts::Part* mainWindowActivePart = (m_pMainWindow && m_pMainWindow->currentView())
                                         ? m_pMainWindow->currentView()->part() : 0;
    if (part == activePart() && (!immediate || mainWindowActivePart == part))
    {
      if ( part )
        kdDebug(1202) << "Part is already active!" << endl;
      return;
    }

    // Don't activate when part changed in non-active tab
    KonqView* partView = m_pMainWindow->childView((KParts::ReadOnlyPart*)part);
    if (partView)
    {
      KonqFrameContainerBase* parentContainer = partView->frame()->parentContainer();
      if (parentContainer->frameType()=="Tabs")
      {
        KonqFrameTabs* parentFrameTabs = static_cast<KonqFrameTabs*>(parentContainer);
        if (partView->frame() != parentFrameTabs->currentPage())
           return;
      }
    }

    if (m_pMainWindow && m_pMainWindow->currentView())
      m_pMainWindow->currentView()->setLocationBarURL(m_pMainWindow->locationBarURL());

    KParts::PartManager::setActivePart( part );

    if (part && part->widget())
        part->widget()->setFocus();

    if (!immediate && reason() != ReasonRightClick) {
        // We use a 0s single shot timer so that when left-clicking on a part,
        // we process the mouse event before rebuilding the GUI.
        // Otherwise, when e.g. dragging icons, the mouse pointer can already
        // be very far from where it was...
        m_activePartChangedTimer->start( 0, true );
        // This is not done with right-clicking so that the part is activated before the
        // popup appears (#75201)
    } else {
        emitActivePartChanged();
    }
}

void KonqViewManager::slotActivePartChanged ( KParts::Part *newPart )
{
    //kdDebug(1202) << "KonqViewManager::slotActivePartChanged " << newPart << endl;
    if (newPart == 0L) {
      //kdDebug(1202) << "newPart = 0L , returning" << endl;
      return;
    }
    KonqView * view = m_pMainWindow->childView( static_cast<KParts::ReadOnlyPart *>(newPart) );
    if (view == 0L) {
      kdDebug(1202) << k_funcinfo << "No view associated with this part" << endl;
      return;
    }
    if (view->frame()->parentContainer() == 0L) return;
    if (!m_bLoadingProfile)  {
        view->frame()->statusbar()->updateActiveStatus();
        view->frame()->parentContainer()->setActiveChild( view->frame() );
    }
    //kdDebug(1202) << "KonqViewManager::slotActivePartChanged done" << endl;
}

void KonqViewManager::emitActivePartChanged()
{
    // prevent unnecessary multiple calls to slotPartActivated:
    m_activePartChangedTimer->stop();
    m_pMainWindow->slotPartActivated( activePart() );
}


QSize KonqViewManager::readConfigSize( KConfig &cfg, QWidget *widget )
{
    bool ok;

    QString widthStr = cfg.readEntry( "Width" );
    QString heightStr = cfg.readEntry( "Height" );

    int width = -1;
    int height = -1;

    QRect geom = KGlobalSettings::desktopGeometry(widget);

    if ( widthStr.contains( '%' ) == 1 )
    {
        widthStr.truncate( widthStr.length() - 1 );
        int relativeWidth = widthStr.toInt( &ok );
        if ( ok ) {
            width = relativeWidth * geom.width() / 100;
        }
    }
    else
    {
        width = widthStr.toInt( &ok );
        if ( !ok )
            width = -1;
    }

    if ( heightStr.contains( '%' ) == 1 )
    {
        heightStr.truncate( heightStr.length() - 1 );
        int relativeHeight = heightStr.toInt( &ok );
        if ( ok ) {
            height = relativeHeight * geom.height() / 100;
        }
    }
    else
    {
        height = heightStr.toInt( &ok );
        if ( !ok )
            height = -1;
    }

    return QSize( width, height );
}

void KonqViewManager::loadItem( KConfig &cfg, KonqFrameContainerBase *parent,
                                const QString &name, const KURL & defaultURL, bool openURL, bool openAfterCurrentPage )
{
  QString prefix;
  if( name != "InitialView" )
    prefix = name + '_';

  //kdDebug(1202) << "KonqViewManager::loadItem: begin name " << name << " openURL " << openURL << endl;

  if( name.startsWith("View") || name == "empty" ) {
    //load view config
    QString serviceType;
    QString serviceName;
    if ( name == "empty" ) {
        // An empty profile is an empty KHTML part. Makes all KHTML actions available, avoids crashes,
        // makes it easy to DND a URL onto it, and makes it fast to load a website from there.
        serviceType = "text/html";
        serviceName = "html";
    } else {
        serviceType = cfg.readEntry( QString::fromLatin1( "ServiceType" ).prepend( prefix ), "inode/directory");
        serviceName = cfg.readEntry( QString::fromLatin1( "ServiceName" ).prepend( prefix ) );
    }
    //kdDebug(1202) << "KonqViewManager::loadItem: ServiceType " << serviceType << " " << serviceName << endl;

    KService::Ptr service;
    KTrader::OfferList partServiceOffers, appServiceOffers;

    KonqViewFactory viewFactory = KonqFactory::createView( serviceType, serviceName, &service, &partServiceOffers, &appServiceOffers, true /*forceAutoEmbed*/ );
    if ( viewFactory.isNull() )
    {
      kdWarning(1202) << "Profile Loading Error: View creation failed" << endl;
      return; //ugh..
    }

    bool passiveMode = cfg.readBoolEntry( QString::fromLatin1( "PassiveMode" ).prepend( prefix ), false );

    //kdDebug(1202) << "KonqViewManager::loadItem: Creating View Stuff" << endl;
    KonqView *childView = setupView( parent, viewFactory, service, partServiceOffers, appServiceOffers, serviceType, passiveMode, openAfterCurrentPage );

    if (!childView->isFollowActive()) childView->setLinkedView( cfg.readBoolEntry( QString::fromLatin1( "LinkedView" ).prepend( prefix ), false ) );
    childView->setToggleView( cfg.readBoolEntry( QString::fromLatin1( "ToggleView" ).prepend( prefix ), false ) );
    if( !cfg.readBoolEntry( QString::fromLatin1( "ShowStatusBar" ).prepend( prefix ), true ) )
      childView->frame()->statusbar()->hide();

    if (cfg.readBoolEntry( QString::fromLatin1( "docContainer" ).prepend( prefix ), false ))
      m_pDocContainer = childView->frame();

    if (!m_pDocContainer)
    {
      if (parent->frameType() == "MainWindow")
        m_pDocContainer = childView->frame(); // Child view of mainWindow

      else if (parent->frameType() == "Container")
      {
        KonqFrameContainer* parentContainer = static_cast<KonqFrameContainer*>(parent);
        KonqFrameBase* otherFrame = parentContainer->otherChild( childView->frame() );
        if (otherFrame)
        {
          if (childView->isPassiveMode())
          {
            if (otherFrame->frameType() == "View")
            {
              KonqFrame* viewFrame = static_cast<KonqFrame*>(otherFrame);
              if (viewFrame->childView()->isPassiveMode())
                m_pDocContainer = parentContainer; // Both views are passive, shouldn't happen
              else
                m_pDocContainer = viewFrame; // This one is passive, the other is active
            }
          }
          else
          {
            if (otherFrame->frameType() == "View")
            {
              KonqFrame* viewFrame = static_cast<KonqFrame*>(otherFrame);
              if (viewFrame->childView()->isPassiveMode())
                m_pDocContainer = childView->frame(); // This one is active, the other is passive
              else
                m_pDocContainer = parentContainer; // Both views are active
            }
            else
              m_pDocContainer = parentContainer; // This one is active, the other is a Container
          }
        }
      }
    }
    KonqConfigEvent ev( &cfg, prefix+"_", false/*load*/);
    QApplication::sendEvent( childView->part(), &ev );

    childView->frame()->show();

    QString key = QString::fromLatin1( "URL" ).prepend( prefix );
    if ( openURL )
    {
      KURL url;

      //kdDebug(1202) << "KonqViewManager::loadItem: key " << key << endl;
      if ( cfg.hasKey( key ) )
      {
        QString u = cfg.readPathEntry( key );
        if ( u.isEmpty() )
          u = QString::fromLatin1("about:blank");
        url = u;
      }
      else if(key == "empty_URL")
        url = QString::fromLatin1("about:blank");
      else
        url = defaultURL;

      if ( !url.isEmpty() )
      {
        //kdDebug(1202) << "KonqViewManager::loadItem: calling openURL " << url.prettyURL() << endl;
        //childView->openURL( url, url.prettyURL() );
        // We need view-follows-view (for the dirtree, for instance)
        KonqOpenURLRequest req;
        if (url.protocol() != "about")
          req.typedURL = url.prettyURL();
        m_pMainWindow->openView( serviceType, url, childView, req );
      }
      //else kdDebug(1202) << "KonqViewManager::loadItem: url is empty" << endl;
    }
    // Do this after opening the URL, so that it's actually possible to open it :)
    childView->setLockedLocation( cfg.readBoolEntry( QString::fromLatin1( "LockedLocation" ).prepend( prefix ), false ) );
  }
  else if( name.startsWith("Container") ) {
    //kdDebug(1202) << "KonqViewManager::loadItem Item is Container" << endl;

    //load container config
    QString ostr = cfg.readEntry( QString::fromLatin1( "Orientation" ).prepend( prefix ) );
    //kdDebug(1202) << "Orientation: " << ostr << endl;
    Qt::Orientation o;
    if( ostr == "Vertical" )
      o = Qt::Vertical;
    else if( ostr == "Horizontal" )
      o = Qt::Horizontal;
    else {
        kdWarning() << "Profile Loading Error: No orientation specified in " << name << endl;
      o = Qt::Horizontal;
    }

    QValueList<int> sizes =
        cfg.readIntListEntry( QString::fromLatin1( "SplitterSizes" ).prepend( prefix ));

    int index = cfg.readNumEntry( QString::fromLatin1( "activeChildIndex" ).prepend(prefix), -1 );

    QStrList childList;
    if( cfg.readListEntry( QString::fromLatin1( "Children" ).prepend( prefix ), childList ) < 2 )
    {
      kdWarning() << "Profile Loading Error: Less than two children in " << name << endl;
      // fallback to defaults
      loadItem( cfg, parent, "InitialView", defaultURL, openURL );
    }
    else
    {
      KonqFrameContainer *newContainer = new KonqFrameContainer( o, parent->widget(), parent );
      connect(newContainer,SIGNAL(ctrlTabPressed()),m_pMainWindow,SLOT(slotCtrlTabPressed()));
      
      int tabindex = -1;
      if(openAfterCurrentPage && parent->frameType() == "Tabs") // Need to honor it, if possible      
	tabindex = static_cast<KonqFrameTabs*>(parent)->currentPageIndex() + 1;
      parent->insertChildFrame( newContainer, tabindex );


      if (cfg.readBoolEntry( QString::fromLatin1( "docContainer" ).prepend( prefix ), false ))
        m_pDocContainer = newContainer;

      loadItem( cfg, newContainer, childList.at(0), defaultURL, openURL );
      loadItem( cfg, newContainer, childList.at(1), defaultURL, openURL );

      newContainer->setSizes( sizes );

      if (index == 1)
        newContainer->setActiveChild( newContainer->secondChild() );
      else if (index == 0)
        newContainer->setActiveChild( newContainer->firstChild() );

      newContainer->show();
    }
  }
  else if( name.startsWith("Tabs") )
  {
    //kdDebug(1202) << "KonqViewManager::loadItem: Item is a Tabs" << endl;

    KonqFrameTabs *newContainer = new KonqFrameTabs( parent->widget(), parent, this );
    connect(newContainer,SIGNAL(ctrlTabPressed()),m_pMainWindow,SLOT(slotCtrlTabPressed()));

    parent->insertChildFrame( newContainer );
    m_pDocContainer = newContainer;

    int index = cfg.readNumEntry( QString::fromLatin1( "activeChildIndex" ).prepend(prefix), 0 );

    QStringList childList = cfg.readListEntry( QString::fromLatin1( "Children" ).prepend( prefix ) );
    for ( QStringList::Iterator it = childList.begin(); it != childList.end(); ++it )
    {
        loadItem( cfg, newContainer, *it, defaultURL, openURL );
        QWidget* currentPage = newContainer->currentPage();
        if (currentPage != 0L) {
          KonqView* activeChildView = dynamic_cast<KonqFrameBase*>(currentPage)->activeChildView();
          if (activeChildView != 0L) {
            activeChildView->setCaption( activeChildView->caption() );
            activeChildView->setTabIcon( activeChildView->url() );
          }
        }
    }

    newContainer->setActiveChild( dynamic_cast<KonqFrameBase*>(newContainer->page(index)) );
    newContainer->setCurrentPage( index );

    newContainer->show();
  }
  else
      kdWarning() << "Profile Loading Error: Unknown item " << name;

  //kdDebug(1202) << "KonqViewManager::loadItem: end" << name << endl;
}

void KonqViewManager::setProfiles( KActionMenu *profiles )
{
  m_pamProfiles = profiles;

  if ( m_pamProfiles )
  {
    connect( m_pamProfiles->popupMenu(), SIGNAL( activated( int ) ),
             this, SLOT( slotProfileActivated( int ) ) );
    connect( m_pamProfiles->popupMenu(), SIGNAL( aboutToShow() ),
             this, SLOT( slotProfileListAboutToShow() ) );
  }
  //KonqMainWindow::enableAllActions will call it anyway
  //profileListDirty();
}

void KonqViewManager::showProfileDlg( const QString & preselectProfile )
{
  KonqProfileDlg dlg( this, preselectProfile, m_pMainWindow );
  dlg.exec();
  profileListDirty();
}

void KonqViewManager::slotProfileDlg()
{
  showProfileDlg( QString::null );
}

void KonqViewManager::profileListDirty( bool broadcast )
{
  //kdDebug(1202) << "KonqViewManager::profileListDirty()" << endl;
  if ( !broadcast )
  {
    m_bProfileListDirty = true;
#if 0
  // There's always one profile at least, now...
  QStringList profiles = KonqFactory::instance()->dirs()->findAllResources( "data", "konqueror/profiles/*", false, true );
  if ( m_pamProfiles )
      m_pamProfiles->setEnabled( profiles.count() > 0 );
#endif
    return;
  }

  kapp->dcopClient()->send( "konqueror*", "KonquerorIface", "updateProfileList()", QByteArray() );
}

void KonqViewManager::slotProfileActivated( int id )
{

  QMap<QString, QString>::ConstIterator iter = m_mapProfileNames.begin();
  QMap<QString, QString>::ConstIterator end = m_mapProfileNames.end();

  for(int i=0; iter != end; ++iter, ++i) {
    if( i == id ) {
      KURL u;
      u.setPath( *iter );
      loadViewProfile( *iter, u.fileName() );
      break;
    }
  }
}

void KonqViewManager::slotProfileListAboutToShow()
{
  if ( !m_pamProfiles || !m_bProfileListDirty )
    return;

  QPopupMenu *popup = m_pamProfiles->popupMenu();
  popup->clear();

  // Fetch profiles
  m_mapProfileNames = KonqProfileDlg::readAllProfiles();

  // Generate accelerators
  QStringList accel_strings;
  KAccelGen::generateFromKeys(m_mapProfileNames, accel_strings);

  // Store menu items
  QValueListIterator<QString> iter = accel_strings.begin();
  for( int id = 0;
       iter != accel_strings.end();
       ++iter, ++id ) {
      popup->insertItem( *iter, id );
  }

  m_bProfileListDirty = false;
}

void KonqViewManager::setLoading( KonqView *view, bool loading )
{
  KonqFrameContainerBase* parentContainer = view->frame()->parentContainer();
  if ( parentContainer->frameType() == "Tabs" ) {
    QColor color;
    KonqFrameTabs* konqframetabs = static_cast<KonqFrameTabs*>( parentContainer );
    if ( loading )
      color = QColor( (KGlobalSettings::linkColor().red()  + KGlobalSettings::inactiveTextColor().red())/2,
                      (KGlobalSettings::linkColor().green()+ KGlobalSettings::inactiveTextColor().green())/2,
                      (KGlobalSettings::linkColor().blue() + KGlobalSettings::inactiveTextColor().blue())/2 );
    else
    {
      if ( konqframetabs->currentPage() != view->frame() )
        color = KGlobalSettings::linkColor();
      else
        color = KGlobalSettings::textColor();
    }
    konqframetabs->setTabColor( view->frame(), color );
  }
}

void KonqViewManager::showHTML(bool b)
{
  if (m_pDocContainer == 0L) return;
  if (m_pDocContainer->frameType() != "Tabs") return;

  KonqFrameTabs* tabContainer = static_cast<KonqFrameTabs*>(m_pDocContainer);

  QPtrList<KonqFrameBase> frameList = *tabContainer->childFrameList();
  QPtrListIterator<KonqFrameBase> it( frameList );

  for ( it.toFirst(); it != 0L; ++it )
  {
      if ( it.current()->activeChildView() && it.current()->activeChildView() !=m_pMainWindow->currentView())
      {
        it.current()->activeChildView()->setAllowHTML( b );
          if( !it.current()->activeChildView()->locationBarURL().isEmpty())
          {

            m_pMainWindow->showHTML( it.current()->activeChildView(), b, false );
          }
      }
  }
}



///////////////// Debug stuff ////////////////

#ifndef NDEBUG
void KonqViewManager::printSizeInfo( KonqFrameBase* frame,
                                     KonqFrameContainerBase* parent,
                                     const char* msg )
{
  QRect r;
  r = frame->widget()->geometry();
  qDebug("Child size %s : x: %d, y: %d, w: %d, h: %d", msg, r.x(),r.y(),r.width(),r.height() );

  if ( parent->frameType() == "Container" )
  {
  QValueList<int> sizes;
    sizes = static_cast<KonqFrameContainer*>(parent)->sizes();
  printf( "Parent sizes %s :", msg );
  QValueList<int>::ConstIterator it;
  for( it = sizes.begin(); it != sizes.end(); ++it )
    printf( " %d", (*it) );
  printf("\n");
  }
}

void KonqViewManager::printFullHierarchy( KonqFrameContainerBase * container )
{
  kdDebug(1202) << "currentView=" << m_pMainWindow->currentView() << endl;
  kdDebug(1202) << "docContainer=" << m_pDocContainer << endl;

  if (container) container->printFrameInfo(QString::null);
  else m_pMainWindow->printFrameInfo(QString::null);
}
#endif

#include "konq_viewmgr.moc"
