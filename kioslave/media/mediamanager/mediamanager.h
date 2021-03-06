/* This file is part of the KDE Project
   Copyright (c) 2004 K�vin Ottens <ervin ipsquad net>

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

#ifndef _MEDIAMANAGER_H_
#define _MEDIAMANAGER_H_

#include <kdedmodule.h>
#include <qstring.h>
#include <qstringlist.h>

#include "medialist.h"
#include "backendbase.h"
#include "removablebackend.h"
#include "mediadirnotify.h"

class HALBackend;
class FstabBackend;

class MediaManager : public KDEDModule
{
Q_OBJECT
K_DCOP
public:
	MediaManager(const QCString &obj);
	~MediaManager();

k_dcop:
	QStringList fullList();
	QStringList properties(const QString &name);
	QStringList mountoptions(const QString &name);
	bool setMountoptions(const QString &name, const QStringList &options);

	QString mount(const QString &uid);
	QString unmount(const QString &uid);
	QString decrypt(const QString &uid, const QString &password);
	QString undecrypt(const QString &uid);

	QString nameForLabel(const QString &label);
	ASYNC setUserLabel(const QString &name, const QString &label);

	ASYNC reloadBackends();

	// Removable media handling (for people not having HAL)
	bool removablePlug(const QString &devNode, const QString &label);
	bool removableUnplug(const QString &devNode);
	bool removableCamera(const QString &devNode);

        /**
         * Unmount manually all partitions when going to suspend
         *
         * @return last error if any
         */
        QString unmountAllSuspend();

        /**
         * Remount previously unmounted partitions in unmountAllSuspend()
         *
         * @return last error if any
         */
        QString remountAllResume();

k_dcop_signals:
	void mediumAdded(const QString &name, bool allowNotification);
	void mediumRemoved(const QString &name, bool allowNotification);
	void mediumChanged(const QString &name, bool allowNotification);

	// For compatibility purpose, not needed for KDE4
	void mediumAdded(const QString &name);
	void mediumRemoved(const QString &name);
	void mediumChanged(const QString &name);

private slots:
	void loadBackends();

	void slotMediumAdded(const QString &id, const QString &name,
	                     bool allowNotification);
	void slotMediumRemoved(const QString &id, const QString &name,
	                       bool allowNotification);
	void slotMediumChanged(const QString &id, const QString &name,
	                       bool mounted, bool allowNotification);

private:
	MediaList m_mediaList;
	QValueList<BackendBase*> m_backends;
	RemovableBackend *mp_removableBackend;
	HALBackend *m_halbackend;
	MediaDirNotify m_dirNotify;
	FstabBackend *m_fstabbackend;
        QStringList m_suspendResumeMountList;
};

#endif
