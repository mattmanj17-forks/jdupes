/*
 * Windows-native code for getting stat()-like information
 *
 * Copyright (C) 2016-2023 by Jody Bruchon <jody@jodybruchon.com>
 * Released under The MIT License
 */

#ifndef WIN32_LEAN_AND_MEAN
 #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <limits.h>
#include "win_stat.h"
#include <stdint.h>

/* Convert NT epoch to UNIX epoch */
time_t nttime_to_unixtime(const uint64_t * const restrict timestamp)
{
	uint64_t newstamp;

	memcpy(&newstamp, timestamp, sizeof(uint64_t));
	newstamp /= 10000000LL;
	if (newstamp <= 11644473600LL) return 0;
	newstamp -= 11644473600LL;
	return (time_t)newstamp;
}

/* Convert UNIX epoch to NT epoch */
time_t unixtime_to_nttime(const uint64_t * const restrict timestamp)
{
	uint64_t newstamp;

	memcpy(&newstamp, timestamp, sizeof(uint64_t));
	newstamp += 11644473600LL;
	newstamp *= 10000000LL;
	if (newstamp <= 11644473600LL) return 0;
	return (time_t)newstamp;
}

/* Get stat()-like extra information for a file on Windows */
int win_stat(const char * const filename, struct winstat * const restrict buf)
{
  HANDLE hFile;
  BY_HANDLE_FILE_INFORMATION bhfi;
  uint64_t timetemp;

#ifdef UNICODE
  static wchar_t wname2[WPATH_MAX];

  if (!buf) return -127;
  if (!M2W(filename,wname2)) return -126;
  hFile = CreateFileW(wname2, 0, FILE_SHARE_READ, NULL, OPEN_EXISTING,
		  FILE_FLAG_BACKUP_SEMANTICS, NULL);
#else
  if (!buf) return -127;
  hFile = CreateFile(filename, 0, FILE_SHARE_READ, NULL, OPEN_EXISTING,
		  FILE_FLAG_BACKUP_SEMANTICS, NULL);
#endif

  if (hFile == INVALID_HANDLE_VALUE) goto failure;
  if (!GetFileInformationByHandle(hFile, &bhfi)) goto failure2;

  buf->st_ino = ((uint64_t)(bhfi.nFileIndexHigh) << 32) + (uint64_t)bhfi.nFileIndexLow;
  buf->st_size = ((int64_t)(bhfi.nFileSizeHigh) << 32) + (int64_t)bhfi.nFileSizeLow;
  timetemp = ((uint64_t)(bhfi.ftCreationTime.dwHighDateTime) << 32) + bhfi.ftCreationTime.dwLowDateTime;
  buf->st_ctime = nttime_to_unixtime(&timetemp);
  timetemp = ((uint64_t)(bhfi.ftLastWriteTime.dwHighDateTime) << 32) + bhfi.ftLastWriteTime.dwLowDateTime;
  buf->st_mtime = nttime_to_unixtime(&timetemp);
  timetemp = ((uint64_t)(bhfi.ftLastAccessTime.dwHighDateTime) << 32) + bhfi.ftLastAccessTime.dwLowDateTime;
  buf->st_atime = nttime_to_unixtime(&timetemp);
  buf->st_dev = (uint32_t)bhfi.dwVolumeSerialNumber;
  buf->st_nlink = (uint32_t)bhfi.nNumberOfLinks;
  buf->st_mode = (uint32_t)bhfi.dwFileAttributes;

  CloseHandle(hFile);
  return 0;

failure:
  CloseHandle(hFile);
  return -1;
failure2:
  CloseHandle(hFile);
  return -2;
}
