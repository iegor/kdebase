/*
 * Copyright (c) 2000 Yves Arrouye <yves@realnames.com>
 * Copyright (c) 2001, 2002 Dawit Alemayehu <adawit@kde.org>
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

#include <qcheckbox.h>
#include <qfile.h>
#include <qgroupbox.h>
#include <qheader.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qpushbutton.h>
#include <qvbox.h>
#include <qwhatsthis.h>

#include <kdebug.h>
#include <kglobal.h>
#include <dcopref.h>
#include <kapplication.h>
#include <kcombobox.h>
#include <kconfig.h>
#include <kiconloader.h>
#include <klistview.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kservice.h>
#include <ksimpleconfig.h>
#include <kstandarddirs.h>
#include <ktrader.h>

#include "ikwsopts.h"
#include "ikwsopts_ui.h"
#include "kuriikwsfiltereng.h"
#include "searchprovider.h"
#include "searchproviderdlg.h"


class SearchProviderItem : public QCheckListItem
{
public:
    SearchProviderItem(QListView *parent, SearchProvider *provider)
    :QCheckListItem(parent, provider->name(), CheckBox), m_provider(provider)
    {
      update();
    }

    virtual ~SearchProviderItem()
    {
      delete m_provider;
    }

    void update()
    {
      setText(0, m_provider->name());
      setText(1, m_provider->keys().join(","));
    }

    SearchProvider *provider() const { return m_provider; }

private:
    SearchProvider *m_provider;
};

FilterOptions::FilterOptions(KInstance *instance, QWidget *parent, const char *name)
              :KCModule(instance, parent, name)
{
    QVBoxLayout *mainLayout = new QVBoxLayout( this, KDialog::marginHint(),
        KDialog::spacingHint());

    m_dlg = new FilterOptionsUI (this);
    mainLayout->addWidget(m_dlg);

    m_dlg->lvSearchProviders->header()->setLabel(0, SmallIconSet("bookmark"),i18n("Name"));
    m_dlg->lvSearchProviders->setSorting(0);

    // Load the options
    load();
}

QString FilterOptions::quickHelp() const
{
    return i18n("In this module you can configure the web shortcuts feature. "
                "Web shortcuts allow you to quickly search or lookup words on "
                "the Internet. For example, to search for information about the "
                "KDE project using the Google engine, you simply type <b>gg:KDE</b> "
                "or <b>google:KDE</b>."
                "<p>If you select a default search engine, normal words or phrases "
                "will be looked up at the specified search engine by simply typing "
                "them into applications, such as Konqueror, that have built-in support "
                "for such a feature.");
}

void FilterOptions::load()
{
   load( false );
}

void FilterOptions::load( bool useDefaults )
{
    // Clear state first.
    m_dlg->lvSearchProviders->clear();

    KConfig config( KURISearchFilterEngine::self()->name() + "rc", false, false );

    config.setReadDefaults( useDefaults );

    config.setGroup("General");

    QString defaultSearchEngine = config.readEntry("DefaultSearchEngine");

    m_favoriteEngines.clear();
    m_favoriteEngines << "google" << "google_groups" << "google_news" << "webster" << "dmoz" << "wikipedia";
    m_favoriteEngines = config.readListEntry("FavoriteSearchEngines", m_favoriteEngines);

    const KTrader::OfferList services = KTrader::self()->query("SearchProvider");

    for (KTrader::OfferList::ConstIterator it = services.begin();
         it != services.end(); ++it)
    {
      displaySearchProvider(new SearchProvider(*it),
                            ((*it)->desktopEntryName() == defaultSearchEngine));
    }

    bool webShortcutsEnabled = config.readBoolEntry("EnableWebShortcuts", true);
    m_dlg->cbEnableShortcuts->setChecked( webShortcutsEnabled );

    setDelimiter (config.readNumEntry ("KeywordDelimiter", ':'));

    // Update the GUI to reflect the config options read above...
    setWebShortcutState();

    if (m_dlg->lvSearchProviders->childCount())
      m_dlg->lvSearchProviders->setSelected(m_dlg->lvSearchProviders->firstChild(), true);

    // Connect all the signals/slots...
    connect(m_dlg->cbEnableShortcuts, SIGNAL(clicked()), this,
            SLOT(setWebShortcutState()));
    connect(m_dlg->cbEnableShortcuts, SIGNAL(clicked()), this,
            SLOT(configChanged()));

    connect(m_dlg->lvSearchProviders, SIGNAL(selectionChanged(QListViewItem *)),
           this, SLOT(updateSearchProvider()));
    connect(m_dlg->lvSearchProviders, SIGNAL(doubleClicked(QListViewItem *)),
           this, SLOT(changeSearchProvider()));
    connect(m_dlg->lvSearchProviders, SIGNAL(returnPressed(QListViewItem *)),
           this, SLOT(changeSearchProvider()));
    connect(m_dlg->lvSearchProviders, SIGNAL(executed(QListViewItem *)),
           this, SLOT(checkFavoritesChanged()));
    connect(m_dlg->lvSearchProviders, SIGNAL(spacePressed(QListViewItem *)),
           this, SLOT(checkFavoritesChanged()));
    connect(m_dlg->lvSearchProviders, SIGNAL(pressed(QListViewItem *)),
           this, SLOT(checkFavoritesChanged()));
    connect(m_dlg->lvSearchProviders, SIGNAL(clicked(QListViewItem *)),
           this, SLOT(checkFavoritesChanged()));

    connect(m_dlg->cmbDefaultEngine, SIGNAL(activated(const QString &)), this,
            SLOT(configChanged()));
    connect(m_dlg->cmbDelimiter, SIGNAL(activated(const QString &)), this,
            SLOT(configChanged()));

    connect(m_dlg->pbNew, SIGNAL(clicked()), this, SLOT(addSearchProvider()));
    connect(m_dlg->pbChange, SIGNAL(clicked()), this, SLOT(changeSearchProvider()));
    connect(m_dlg->pbDelete, SIGNAL(clicked()), this, SLOT(deleteSearchProvider()));

    emit changed( useDefaults );
}

char FilterOptions::delimiter ()
{
  switch (m_dlg->cmbDelimiter->currentItem())
  {
    case 1:
      return ' ';
    case 0:
    default:
      return ':';
  };
}

void FilterOptions::setDelimiter (char sep)
{
  switch (sep)
  {
    case ' ':
      m_dlg->cmbDelimiter->setCurrentItem (1);
      break;
    case ':':
    default:
      m_dlg->cmbDelimiter->setCurrentItem (0);
  };
}

void FilterOptions::save()
{
  KConfig config( KURISearchFilterEngine::self()->name() + "rc", false, false );

  config.setGroup("General");
  config.writeEntry("EnableWebShortcuts", m_dlg->cbEnableShortcuts->isChecked());
  config.writeEntry("KeywordDelimiter", delimiter() );

  QString engine;

  if (m_dlg->cmbDefaultEngine->currentItem() != 0)
    engine = m_dlg->cmbDefaultEngine->currentText();

  config.writeEntry("DefaultSearchEngine", m_defaultEngineMap[engine]);

  // kdDebug () << "Engine: " << m_defaultEngineMap[engine] << endl;

  int changedProviderCount = 0;
  QString path = kapp->dirs()->saveLocation("services", "searchproviders/");

  m_favoriteEngines.clear();

  for (QListViewItemIterator it(m_dlg->lvSearchProviders); it.current(); ++it)
  {
    SearchProviderItem *item = dynamic_cast<SearchProviderItem *>(it.current());

    Q_ASSERT(item);

    SearchProvider *provider = item->provider();

    QString name = provider->desktopEntryName();

    if (item->isOn())
      m_favoriteEngines << name;

    if (provider->isDirty())
    {
      changedProviderCount++;

      if (name.isEmpty())
      {
        // New provider
        // Take the longest search shortcut as filename,
        // if such a file already exists, append a number and increase it
        // until the name is unique
        for (QStringList::ConstIterator it = provider->keys().begin(); it != provider->keys().end(); ++it)
        {
            if ((*it).length() > name.length())
                name = (*it).lower();
        }
        for (int suffix = 0; ; ++suffix)
        {
            QString located, check = name;
            if (suffix)
                check += QString().setNum(suffix);
            if ((located = locate("services", "searchproviders/" + check + ".desktop")).isEmpty())
            {
                name = check;
                break;
            }
            else if (located.left(path.length()) == path)
            {
                // If it's a deleted (hidden) entry, overwrite it
                if (KService(located).isDeleted())
                    break;
            }
        }
      }

      KSimpleConfig service(path + name + ".desktop");
      service.setGroup("Desktop Entry");
      service.writeEntry("Type", "Service");
      service.writeEntry("ServiceTypes", "SearchProvider");
      service.writeEntry("Name", provider->name());
      service.writeEntry("Query", provider->query(), true, false, true);
      service.writeEntry("Keys", provider->keys());
      service.writeEntry("Charset", provider->charset());

      // we might be overwriting a hidden entry
      service.writeEntry("Hidden", false);
    }
  }

  for (QStringList::ConstIterator it = m_deletedProviders.begin();
      it != m_deletedProviders.end(); ++it)
  {
      QStringList matches = kapp->dirs()->findAllResources("services", "searchproviders/" + *it + ".desktop");

      // Shouldn't happen
      if (!matches.count())
          continue;

      if (matches.count() == 1 && matches[0].left(path.length()) == path)
      {
          // If only the local copy existed, unlink it
          // TODO: error handling
          QFile::remove(matches[0]);
          continue;
      }
      KSimpleConfig service(path + *it + ".desktop");
      service.setGroup("Desktop Entry");
      service.writeEntry("Type", "Service");
      service.writeEntry("ServiceTypes", "SearchProvider");
      service.writeEntry("Hidden", true);
  }

  config.writeEntry("FavoriteSearchEngines", m_favoriteEngines);
  config.sync();

  emit changed(false);

  // Update filters in running applications...
  (void) DCOPRef("*", "KURIIKWSFilterIface").send("configure");
  (void) DCOPRef("*", "KURISearchFilterIface").send("configure");

  // If the providers changed, tell sycoca to rebuild its database...
  if (changedProviderCount)
    KService::rebuildKSycoca(this);
}

void FilterOptions::defaults()
{
   load( true );
}

void FilterOptions::configChanged()
{
  // kdDebug () << "FilterOptions::configChanged: TRUE" << endl;
  emit changed(true);
}

void FilterOptions::checkFavoritesChanged()
{
  QStringList currentFavoriteEngines;
  for (QListViewItemIterator it(m_dlg->lvSearchProviders); it.current(); ++it)
  {
    SearchProviderItem *item = dynamic_cast<SearchProviderItem *>(it.current());

    Q_ASSERT(item);

    if (item->isOn())
      currentFavoriteEngines << item->provider()->desktopEntryName();
  }

  if (!(currentFavoriteEngines==m_favoriteEngines)) {
    m_favoriteEngines=currentFavoriteEngines;
    configChanged();
  }
}

void FilterOptions::setWebShortcutState()
{
  bool use_keywords = m_dlg->cbEnableShortcuts->isChecked();
  m_dlg->lvSearchProviders->setEnabled(use_keywords);
  m_dlg->pbNew->setEnabled(use_keywords);
  m_dlg->pbChange->setEnabled(use_keywords);
  m_dlg->pbDelete->setEnabled(use_keywords);
  m_dlg->lbDelimiter->setEnabled (use_keywords);
  m_dlg->cmbDelimiter->setEnabled (use_keywords);
  m_dlg->lbDefaultEngine->setEnabled (use_keywords);
  m_dlg->cmbDefaultEngine->setEnabled (use_keywords);
}

void FilterOptions::addSearchProvider()
{
  SearchProviderDialog dlg(0, this);
  if (dlg.exec())
  {
      m_dlg->lvSearchProviders->setSelected(displaySearchProvider(dlg.provider()), true);
      configChanged();
  }
}

void FilterOptions::changeSearchProvider()
{
  SearchProviderItem *item = dynamic_cast<SearchProviderItem *>(m_dlg->lvSearchProviders->currentItem());
  Q_ASSERT(item);

  SearchProviderDialog dlg(item->provider(), this);

  if (dlg.exec())
  {
    m_dlg->lvSearchProviders->setSelected(displaySearchProvider(dlg.provider()), true);
    configChanged();
  }
}

void FilterOptions::deleteSearchProvider()
{
  SearchProviderItem *item = dynamic_cast<SearchProviderItem *>(m_dlg->lvSearchProviders->currentItem());
  Q_ASSERT(item);

  // Update the combo box to go to None if the fallback was deleted.
  int current = m_dlg->cmbDefaultEngine->currentItem();
  for (int i = 1, count = m_dlg->cmbDefaultEngine->count(); i < count; ++i)
  {
    if (m_dlg->cmbDefaultEngine->text(i) == item->provider()->name())
    {
      m_dlg->cmbDefaultEngine->removeItem(i);
      if (i == current)
        m_dlg->cmbDefaultEngine->setCurrentItem(0);
      else if (current > i)
        m_dlg->cmbDefaultEngine->setCurrentItem(current - 1);

      break;
    }
  }

  if (item->nextSibling())
      m_dlg->lvSearchProviders->setSelected(item->nextSibling(), true);
  else if (item->itemAbove())
      m_dlg->lvSearchProviders->setSelected(item->itemAbove(), true);

  if (!item->provider()->desktopEntryName().isEmpty())
      m_deletedProviders.append(item->provider()->desktopEntryName());

  delete item;
  updateSearchProvider();
  configChanged();
}

void FilterOptions::updateSearchProvider()
{
  m_dlg->pbChange->setEnabled(m_dlg->lvSearchProviders->currentItem());
  m_dlg->pbDelete->setEnabled(m_dlg->lvSearchProviders->currentItem());
}

SearchProviderItem *FilterOptions::displaySearchProvider(SearchProvider *p, bool fallback)
{
  // Show the provider in the list.
  SearchProviderItem *item = 0L;

  QListViewItemIterator it(m_dlg->lvSearchProviders);

  for (; it.current(); ++it)
  {
    if (it.current()->text(0) == p->name())
    {
      item = dynamic_cast<SearchProviderItem *>(it.current());
      Q_ASSERT(item);
      break;
    }
  }

  if (item)
    item->update ();
  else
  {
    // Put the name in the default search engine combo box.
    int itemCount;
    int totalCount = m_dlg->cmbDefaultEngine->count();

    item = new SearchProviderItem(m_dlg->lvSearchProviders, p);

    if (m_favoriteEngines.find(p->desktopEntryName())!=m_favoriteEngines.end())
       item->setOn(true);

    for (itemCount = 1; itemCount < totalCount; itemCount++)
    {
      if (m_dlg->cmbDefaultEngine->text(itemCount) > p->name())
      {
        int currentItem = m_dlg->cmbDefaultEngine->currentItem();
        m_dlg->cmbDefaultEngine->insertItem(p->name(), itemCount);
        m_defaultEngineMap[p->name ()] = p->desktopEntryName ();
        if (currentItem >= itemCount)
          m_dlg->cmbDefaultEngine->setCurrentItem(currentItem+1);
        break;
      }
    }

    // Append it to the end of the list...
    if (itemCount == totalCount)
    {
      m_dlg->cmbDefaultEngine->insertItem(p->name(), itemCount);
      m_defaultEngineMap[p->name ()] = p->desktopEntryName ();
    }

    if (fallback)
      m_dlg->cmbDefaultEngine->setCurrentItem(itemCount);
  }

  if (!it.current())
    m_dlg->lvSearchProviders->sort();

  return item;
}

#include "ikwsopts.moc"
