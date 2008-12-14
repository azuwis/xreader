/*
 * This file is part of xReader.
 *
 * Copyright (C) 2008 hrimfaxi (outmatch@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

struct ID3Tag
{
	char ID3Title[100];
	char ID3Artist[100];
	char ID3Album[100];
	char ID3Year[10];
	char ID3Comment[200];
	char ID3GenreCode[10];
	char ID3GenreText[100];
	char versionfound[10];
	int ID3Track;
	char ID3TrackText[4];
	int ID3EncapsulatedPictureOffset;	/* Offset address of an attached picture, NULL if no attached picture exists */

};

int ID3v2TagSize(const char *mp3path);
struct ID3Tag ParseID3(char *mp3path);

//Helper functions (used also by aa3mplayerME to read tags):
void readTagData(FILE * fp, int tagLength, char *tagValue);
int swapInt32BigToHost(int arg);

//short int swapInt16BigToHost(short int arg);
