/*  This file is part of the KDE project
    Copyright (C) 2000 Alexander Neundorf <neundorf@kde.org>

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
#include "kshellcmdexecutor.h"

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>

#include <qsocketnotifier.h>

#include <kinputdialog.h>
#include <kglobalsettings.h>
#include <kdesu/process.h>
#include <klocale.h>

KShellCommandExecutor::KShellCommandExecutor(const QString& command, QWidget* parent)
:QTextView(parent)
,m_shellProcess(0)
,m_command(command)
,m_readNotifier(0)
,m_writeNotifier(0)
{
   setTextFormat(PlainText);
   setFont( KGlobalSettings::fixedFont() );
}

KShellCommandExecutor::~KShellCommandExecutor()
{
   if (m_shellProcess!=0)
   {
      ::kill(m_shellProcess->pid()+1, SIGTERM);
      delete m_shellProcess;
   };
}

int KShellCommandExecutor::exec()
{
   //kdDebug()<<"---------- KShellCommandExecutor::exec()"<<endl;
   setText("");
   if (m_shellProcess!=0)
   {
      ::kill(m_shellProcess->pid(),SIGTERM);
      delete m_shellProcess;
   };
   if (m_readNotifier!=0)
      delete m_readNotifier;
   if (m_writeNotifier!=0)
      delete m_writeNotifier;

   m_shellProcess=new PtyProcess();
   m_shellProcess->setTerminal(true);

   QCStringList args;
   args+="-c";
   args+=m_command.local8Bit();
   //kdDebug()<<"------- executing: "<<m_command.local8Bit()<<endl;

   QCString shell( getenv("SHELL") );
   if (shell.isEmpty())
      shell = "sh";

   int ret = m_shellProcess->exec(shell, args);
   if (ret < 0)
   {
      //kdDebug()<<"could not execute"<<endl;
      return 0;
   }

   m_readNotifier=new QSocketNotifier(m_shellProcess->fd(),QSocketNotifier::Read, this);
   m_writeNotifier=new QSocketNotifier(m_shellProcess->fd(),QSocketNotifier::Write, this);
   m_writeNotifier->setEnabled(false);
   connect (m_readNotifier, SIGNAL(activated(int)), this,SLOT(readDataFromShell()));
   connect (m_writeNotifier, SIGNAL(activated(int)), this,SLOT(writeDataToShell()));

   return 1;
}

void KShellCommandExecutor::readDataFromShell()
{
   //kdDebug()<<"--------- reading ------------"<<endl;
   char buffer[16*1024];
   int bytesRead=::read(m_shellProcess->fd(), buffer, 16*1024-1);
   //0-terminate the buffer
   //process exited
   if (bytesRead<=0)
   {
      slotFinished();
   }
   else if (bytesRead>0)
   {
      //kdDebug()<<"***********************\n"<<buffer<<"###################\n"<<endl;
      buffer[bytesRead]='\0';
      this->append(QString::fromLocal8Bit(buffer));
      setTextFormat(PlainText);
   };
}

void KShellCommandExecutor::writeDataToShell()
{
   //kdDebug()<<"--------- writing ------------"<<endl;
   bool ok;
   QString str = KInputDialog::getText( QString::null,
      i18n( "Input Required:" ), QString::null, &ok, this );
   if ( ok )
   {
      QCString input=str.local8Bit();
      ::write(m_shellProcess->fd(),input,input.length());
      ::write(m_shellProcess->fd(),"\n",1);
   }
   else
      slotFinished();

   if (m_writeNotifier)
   {
      m_writeNotifier->setEnabled(false);
   }
}

void KShellCommandExecutor::slotFinished()
{
   setTextFormat(PlainText);
   if (m_shellProcess!=0)
   {
      delete m_readNotifier;
      m_readNotifier = 0;
      delete m_writeNotifier;
      m_writeNotifier = 0;

      //kdDebug()<<"slotFinished: pid: "<<m_shellProcess->pid()<<endl;
      ::kill(m_shellProcess->pid()+1, SIGTERM);
      ::kill(m_shellProcess->pid(), SIGTERM);
   };
   delete m_shellProcess;
   m_shellProcess=0;
   emit finished();
}

#include "kshellcmdexecutor.moc"
