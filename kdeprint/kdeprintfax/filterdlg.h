/*
 *   kdeprintfax - a small fax utility
 *   Copyright (C) 2001  Michael Goffioul
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#ifndef FILTERDLG_H
#define FILTERDLG_H

#include <kdialogbase.h>

class QLineEdit;

class FilterDlg : public KDialogBase
{
  	Q_OBJECT
public:
	FilterDlg(QWidget *parent = 0, const char *name = 0);

	static bool doIt(QWidget *parent = 0, QString* mime = 0, QString *cmd = 0);
protected slots:
    void slotTextFilterChanged();
private:
	QLineEdit	*m_mime, *m_cmd;
};

#endif