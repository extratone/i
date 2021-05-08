/* Definitions for using a zipped' archive.
   Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2003
   Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to
the Free Software Foundation, 51 Franklin Street, Fifth Floor,
Boston, MA 02110-1301, USA.  

Java and all Java-based marks are trademarks or registered trademarks
of Sun Microsystems, Inc. in the United States and other countries.
The Free Software Foundation is independent of Sun Microsystems, Inc.  */

struct ZipFile {
  char *name;
  int fd;
  long size;
  long count;
  long dir_size;
  char *central_directory;

  /* Chain together in SeenZipFiles. */
  struct ZipFile *next;
};

typedef struct ZipFile ZipFile;

struct ZipDirectory {
  int direntry_size;
  int filename_offset;
  int compression_method;
  unsigned size; /* length of file */
  unsigned uncompressed_size; /* length of uncompressed data */
  unsigned filestart;  /* start of file in archive */
  ZipFile *zipf;
  int filename_length;
  /* char mid_padding[...]; */
  /* char filename[filename_length]; */
  /* char end_padding[...]; */
};

typedef struct ZipDirectory ZipDirectory;

extern struct ZipFile *SeenZipFiles;

#define ZIPDIR_FILENAME(ZIPD) ((char*)(ZIPD)+(ZIPD)->filename_offset)
#define ZIPDIR_NEXT(ZIPD) \
   ((ZipDirectory*)((char*)(ZIPD)+(ZIPD)->direntry_size))
#define ZIPMAGIC 0x504b0304	

extern ZipFile * opendir_in_zip (const char *, int);
extern int read_zip_archive (ZipFile *);
#ifdef GCC_JCF_H
extern int read_zip_member (JCF*, ZipDirectory*, ZipFile *);
extern int open_in_zip (struct JCF *, const char *, const char *, int);
#endif
