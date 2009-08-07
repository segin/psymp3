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
	QApplication app(argc, argv);
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

void messageBox(char *msg) 
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
	messageBox(getFile());
}
#endif
