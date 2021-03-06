//
//  Copyright (C) 1998 Matthias Hoelzer <hoelzer@kde.org>
//  Copyright (C) 2002 David Faure <faure@kde.org>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the7 implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//


#include <klocale.h>
#include <kmessagebox.h>

#include "widgets.h"
#include "klistboxdialog.h"
#include "progressdialog.h"
#include <kinputdialog.h>
#include <kpassdlg.h>
#include <kcombobox.h>
#include <kdebug.h>
#include <kapplication.h>

#include <qlabel.h>
#include <ktextedit.h>
#include <qvbox.h>
#include <qfile.h>

#if defined Q_WS_X11 && ! defined K_WS_QTONLY
#include <netwm.h>
#endif

void Widgets::handleXGeometry(QWidget * dlg)
{
#ifdef Q_WS_X11
    if ( ! kapp->geometryArgument().isEmpty()) {
	int x, y;
	int w, h;
	int m = XParseGeometry( kapp->geometryArgument().latin1(), &x, &y, (unsigned int*)&w, (unsigned int*)&h);
	if ( (m & XNegative) )
	    x = KApplication::desktop()->width()  + x - w;
	if ( (m & YNegative) )
	    y = KApplication::desktop()->height() + y - h;
	dlg->setGeometry(x, y, w, h);
	// kdDebug() << "x: " << x << "  y: " << y << "  w: " << w << "  h: " << h << endl;
    }
#endif
}

bool Widgets::inputBox(QWidget *parent, const QString& title, const QString& text, const QString& init, QString &result)
{
  bool ok;
  QString str = KInputDialog::text( title, text, init, &ok, parent, 0, 0, QString::null );
  if ( ok )
    result = str;
  return ok;
}

bool Widgets::passwordBox(QWidget *parent, const QString& title, const QString& text, QCString &result)
{
  KPasswordDialog dlg( KPasswordDialog::Password, false, 0, parent );

  kapp->setTopWidget( &dlg );
  dlg.setCaption(title);
  dlg.setPrompt(text);

  handleXGeometry(&dlg);

  bool retcode = (dlg.exec() == QDialog::Accepted);
  if ( retcode )
    result = dlg.password();
  return retcode;
}

int Widgets::textBox(QWidget *parent, int width, int height, const QString& title, const QString& file)
{
//  KTextBox dlg(parent, 0, TRUE, width, height, file);
  KDialogBase dlg( parent, 0, true, title, KDialogBase::Ok, KDialogBase::Ok );

  kapp->setTopWidget( &dlg );
  KTextEdit *edit = new KTextEdit( dlg.makeVBoxMainWidget() );
  edit->setReadOnly(TRUE);

  QFile f(file);
  if (!f.open(IO_ReadOnly))
  {
    kdError() << i18n("kdialog: could not open file ") << file << endl;
    return -1;
  }
  QTextStream s(&f);

  while (!s.eof())
    edit->append(s.readLine());

  edit->moveCursor(QTextEdit::MoveHome, false);

  f.close();

  if ( width > 0 && height > 0 )
      dlg.setInitialSize( QSize( width, height ) );

  handleXGeometry(&dlg);
  dlg.setCaption(title);
  dlg.exec();
  return 0;
}

int Widgets::textInputBox(QWidget *parent, int width, int height, const QString& title, const QStringList& args, QCString &result)
{
//  KTextBox dlg(parent, 0, TRUE, width, height, file);
  KDialogBase dlg( parent, 0, true, title, KDialogBase::Ok, KDialogBase::Ok );

  kapp->setTopWidget( &dlg );
  QVBox* vbox = dlg.makeVBoxMainWidget();

  if( args.count() > 0 )
  {
    QLabel *label = new QLabel(vbox);
    label->setText(args[0]);
  }

  KTextEdit *edit = new KTextEdit( vbox );
  edit->setReadOnly(FALSE);
  edit->setTextFormat( Qt::PlainText );
  edit->setFocus();

  if( args.count() > 1 )
    edit->setText( args[1] );

  if ( width > 0 && height > 0 )
    dlg.setInitialSize( QSize( width, height ) );

  handleXGeometry(&dlg);
  dlg.setCaption(title);
  dlg.exec();
  result = edit->text().local8Bit();
  return 0;
}

bool Widgets::comboBox(QWidget *parent, const QString& title, const QString& text, const QStringList& args, 
		       const QString& defaultEntry, QString &result)
{
  KDialogBase dlg( parent, 0, true, title, KDialogBase::Ok|KDialogBase::Cancel,
                   KDialogBase::Ok );

  kapp->setTopWidget( &dlg );
  dlg.setCaption(title);
  QVBox* vbox = dlg.makeVBoxMainWidget();

  QLabel label (vbox);
  label.setText (text);
  KComboBox combo (vbox);
  combo.insertStringList (args);
  combo.setCurrentItem( defaultEntry, false );

  handleXGeometry(&dlg);

  bool retcode = (dlg.exec() == QDialog::Accepted);

  if (retcode)
    result = combo.currentText();

  return retcode;
}

bool Widgets::listBox(QWidget *parent, const QString& title, const QString& text, const QStringList& args, 
		      const QString& defaultEntry, QString &result)
{
  KListBoxDialog box(text,parent);

  kapp->setTopWidget( &box );
  box.setCaption(title);

  for (unsigned int i = 0; i+1<args.count(); i += 2) {
    box.insertItem(args[i+1]);
  }
  box.setCurrentItem( defaultEntry );

  handleXGeometry(&box);

  bool retcode = (box.exec() == QDialog::Accepted);
  if ( retcode )
    result = args[ box.currentItem()*2 ];
  return retcode;
}


bool Widgets::checkList(QWidget *parent, const QString& title, const QString& text, const QStringList& args, bool separateOutput, QStringList &result)
{
  QStringList entries, tags;
  QString rs;

  result.clear();

  KListBoxDialog box(text,parent);

  QListBox &table = box.getTable();

  kapp->setTopWidget( &box );
  box.setCaption(title);

  for (unsigned int i=0; i+2<args.count(); i += 3) {
    tags.append(args[i]);
    entries.append(args[i+1]);
  }

  table.insertStringList(entries);
  table.setMultiSelection(TRUE);
  table.setCurrentItem(0); // This is to circumvent a Qt bug

  for (unsigned int i=0; i+2<args.count(); i += 3) {
    table.setSelected( i/3, args[i+2] == QString::fromLatin1("on") );
  }

  handleXGeometry(&box);

  bool retcode = (box.exec() == QDialog::Accepted);

  if ( retcode ) {
    if (separateOutput) {
      for (unsigned int i=0; i<table.count(); i++)
        if (table.isSelected(i))
          result.append(tags[i]);
    } else {
      for (unsigned int i=0; i<table.count(); i++)
        if (table.isSelected(i))
          rs += QString::fromLatin1("\"") + tags[i] + QString::fromLatin1("\" ");
      result.append(rs);
    }
  }
  return retcode;
}


bool Widgets::radioBox(QWidget *parent, const QString& title, const QString& text, const QStringList& args, QString &result)
{
  QStringList entries, tags;

  KListBoxDialog box(text,parent);

  QListBox &table = box.getTable();

  kapp->setTopWidget( &box );
  box.setCaption(title);

  for (unsigned int i=0; i+2<args.count(); i += 3) {
    tags.append(args[i]);
    entries.append(args[i+1]);
  }

  table.insertStringList(entries);

  for (unsigned int i=0; i+2<args.count(); i += 3) {
    table.setSelected( i/3, args[i+2] == QString::fromLatin1("on") );
  }

  handleXGeometry(&box);

  bool retcode = (box.exec() == QDialog::Accepted);
  if ( retcode )
    result = tags[ table.currentItem() ];
  return retcode;
}

bool Widgets::progressBar(QWidget *parent, const QString& title, const QString& text, int totalSteps)
{
  ProgressDialog dlg( parent, title, text, totalSteps );
  kapp->setTopWidget( &dlg );
  dlg.setCaption( title );
  handleXGeometry(&dlg);
  dlg.exec();
  return dlg.wasCancelled();
}
