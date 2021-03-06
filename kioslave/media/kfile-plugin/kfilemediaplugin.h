/* This file is part of the KDE project
   Copyright (C) 2004 Kevin Ottens <ervin ipsquad net>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef _KFILE_MEDIA_H_
#define _KFILE_MEDIA_H_

#include <kfilemetainfo.h>
#include <kurl.h>
#include <kio/global.h>

#include "medium.h"

class KFileMediaPlugin : public KFilePlugin
{
Q_OBJECT
public:
	KFileMediaPlugin(QObject *parent, const char *name,
	                 const QStringList &args);

	bool readInfo(KFileMetaInfo &info, uint what = KFileMetaInfo::Fastest);

private:
	void addMimeType(const char *mimeType);
	const Medium askMedium(KFileMetaInfo &info);

	KIO::filesize_t m_total;
	KIO::filesize_t m_used;
	KIO::filesize_t m_free;
};

#endif
