/*
    $Id $

    This file is part of PsyMP3.

    PsyMP3 is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    PsyMP3 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with PsyMP3; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <string>

#include "psymp3-release.bi"

void libui_init(int argc, char *argv[])
{
	QApplication *app = new QApplication(argc, argv);
	// while(1) sleep(100);
	QMessageBox msgBox;
	QString titleName = "PsyMP3 " PSYMP3_RELEASE " [pid: " +
		QString::number(getpid()) + "]";
	QString message = "test";
	msgBox.setIcon(QMessageBox::Information);
	msgBox.setDefaultButton(QMessageBox::Ok);
	msgBox.setStandardButtons(QMessageBox::Ok);
	msgBox.setWindowTitle(titleName);
	msgBox.setText(message);
}

char *getFile(void)
{
	QString fileName;
	QByteArray ba;
	static char ret[1024];	
  	char cwd[512];
	getcwd(cwd, 512);
	fileName = QFileDialog::getOpenFileName(NULL,
		"PsyMP3 - Select a MP3, Ogg Vorbis, or FLAC audio",
		 cwd, "ISO/MPEG Layer-III Audio/MP3 (*.mp3);;"
		 "Ogg Vorbis/Ogg FLAC (*.ogg);;"
		 "Free Lossless Audio Codec/FLAC (*.flac);;"
		 "M3U Playlist (*.m3u *.m3u8);;"
		 "All files (*)"
		 );
	ba = fileName.toLatin1();
	strncpy(ret, ba.data(), 511);
	return(ret);
}

void messageBox(int UNUSED, char *msg) 
{
	QMessageBox msgBox;
	QString titleName = "PsyMP3 " PSYMP3_RELEASE " [pid: " +
		QString::number(getpid()) + "]";
	QString message = msg;
	msgBox.setIcon(QMessageBox::Information);
	msgBox.setDefaultButton(QMessageBox::Ok);
	msgBox.setStandardButtons(QMessageBox::Ok);
	msgBox.setWindowTitle(titleName);
	msgBox.setText(message);
	msgBox.exec();
}

#ifndef _LIBUI_QT4
int main(int argc, char **argv)
{
	libui_init(argc, argv);
	messageBox(0,getFile());
}
#endif
