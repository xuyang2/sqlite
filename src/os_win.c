/*
** 2004 May 22
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
******************************************************************************
**
** This file contains code that is specific to windows.
*/
#include "os.h"          /* Must be first to enable large file support */
#if OS_WIN               /* This file is used for windows only */
#include "sqliteInt.h"

#include <winbase.h>

/*
** Macros used to determine whether or not to use threads.
*/
#if defined(THREADSAFE) && THREADSAFE
# define SQLITE_W32_THREADS 1
#endif

/*
** Include code that is common to all os_*.c files
*/
#include "os_common.h"

/*
** Delete the named file
*/
int sqlite3OsDelete(const char *zFilename){
  DeleteFile(zFilename);
  TRACE2("DELETE \"%s\"\n", zFilename);
  return SQLITE_OK;
}

/*
** Return TRUE if the named file exists.
*/
int sqlite3OsFileExists(const char *zFilename){
  return GetFileAttributes(zFilename) != 0xffffffff;
}

/*
** Attempt to open a file for both reading and writing.  If that
** fails, try opening it read-only.  If the file does not exist,
** try to create it.
**
** On success, a handle for the open file is written to *id
** and *pReadonly is set to 0 if the file was opened for reading and
** writing or 1 if the file was opened read-only.  The function returns
** SQLITE_OK.
**
** On failure, the function returns SQLITE_CANTOPEN and leaves
** *id and *pReadonly unchanged.
*/
int sqlite3OsOpenReadWrite(
  const char *zFilename,
  OsFile *id,
  int *pReadonly
){
  HANDLE h = CreateFile(zFilename,
     GENERIC_READ | GENERIC_WRITE,
     FILE_SHARE_READ | FILE_SHARE_WRITE,
     NULL,
     OPEN_ALWAYS,
     FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
     NULL
  );
  if( h==INVALID_HANDLE_VALUE ){
    h = CreateFile(zFilename,
       GENERIC_READ,
       FILE_SHARE_READ,
       NULL,
       OPEN_ALWAYS,
       FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
       NULL
    );
    if( h==INVALID_HANDLE_VALUE ){
      return SQLITE_CANTOPEN;
    }
    *pReadonly = 1;
  }else{
    *pReadonly = 0;
  }
  id->h = h;
  id->locktype = NO_LOCK;
  OpenCounter(+1);
  TRACE3("OPEN R/W %d \"%s\"\n", h, zFilename);
  return SQLITE_OK;
}


/*
** Attempt to open a new file for exclusive access by this process.
** The file will be opened for both reading and writing.  To avoid
** a potential security problem, we do not allow the file to have
** previously existed.  Nor do we allow the file to be a symbolic
** link.
**
** If delFlag is true, then make arrangements to automatically delete
** the file when it is closed.
**
** On success, write the file handle into *id and return SQLITE_OK.
**
** On failure, return SQLITE_CANTOPEN.
*/
int sqlite3OsOpenExclusive(const char *zFilename, OsFile *id, int delFlag){
  HANDLE h;
  int fileflags;
  if( delFlag ){
    fileflags = FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_RANDOM_ACCESS 
                     | FILE_FLAG_DELETE_ON_CLOSE;
  }else{
    fileflags = FILE_FLAG_RANDOM_ACCESS;
  }
  h = CreateFile(zFilename,
     GENERIC_READ | GENERIC_WRITE,
     0,
     NULL,
     CREATE_ALWAYS,
     fileflags,
     NULL
  );
  if( h==INVALID_HANDLE_VALUE ){
    return SQLITE_CANTOPEN;
  }
  id->h = h;
  id->locktype = NO_LOCK;
  OpenCounter(+1);
  TRACE3("OPEN EX %d \"%s\"\n", h, zFilename);
  return SQLITE_OK;
}

/*
** Attempt to open a new file for read-only access.
**
** On success, write the file handle into *id and return SQLITE_OK.
**
** On failure, return SQLITE_CANTOPEN.
*/
int sqlite3OsOpenReadOnly(const char *zFilename, OsFile *id){
  HANDLE h = CreateFile(zFilename,
     GENERIC_READ,
     0,
     NULL,
     OPEN_EXISTING,
     FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
     NULL
  );
  if( h==INVALID_HANDLE_VALUE ){
    return SQLITE_CANTOPEN;
  }
  id->h = h;
  id->locktype = NO_LOCK;
  OpenCounter(+1);
  TRACE3("OPEN RO %d \"%s\"\n", h, zFilename);
  return SQLITE_OK;
}

/*
** Attempt to open a file descriptor for the directory that contains a
** file.  This file descriptor can be used to fsync() the directory
** in order to make sure the creation of a new file is actually written
** to disk.
**
** This routine is only meaningful for Unix.  It is a no-op under
** windows since windows does not support hard links.
**
** On success, a handle for a previously open file is at *id is
** updated with the new directory file descriptor and SQLITE_OK is
** returned.
**
** On failure, the function returns SQLITE_CANTOPEN and leaves
** *id unchanged.
*/
int sqlite3OsOpenDirectory(
  const char *zDirname,
  OsFile *id
){
  return SQLITE_OK;
}

/*
** Create a temporary file name in zBuf.  zBuf must be big enough to
** hold at least SQLITE_TEMPNAME_SIZE characters.
*/
int sqlite3OsTempFileName(char *zBuf){
  static char zChars[] =
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789";
  int i, j;
  char zTempPath[SQLITE_TEMPNAME_SIZE];
  GetTempPath(SQLITE_TEMPNAME_SIZE-30, zTempPath);
  for(i=strlen(zTempPath); i>0 && zTempPath[i-1]=='\\'; i--){}
  zTempPath[i] = 0;
  for(;;){
    sprintf(zBuf, "%s\\"TEMP_FILE_PREFIX, zTempPath);
    j = strlen(zBuf);
    sqlite3Randomness(15, &zBuf[j]);
    for(i=0; i<15; i++, j++){
      zBuf[j] = (char)zChars[ ((unsigned char)zBuf[j])%(sizeof(zChars)-1) ];
    }
    zBuf[j] = 0;
    if( !sqlite3OsFileExists(zBuf) ) break;
  }
  TRACE2("TEMP FILENAME: %s\n", zBuf);
  return SQLITE_OK; 
}

/*
** Close a file.
*/
int sqlite3OsClose(OsFile *id){
  CloseHandle(id->h);
  OpenCounter(-1);
  return SQLITE_OK;
}

/*
** Read data from a file into a buffer.  Return SQLITE_OK if all
** bytes were read successfully and SQLITE_IOERR if anything goes
** wrong.
*/
int sqlite3OsRead(OsFile *id, void *pBuf, int amt){
  DWORD got;
  SimulateIOError(SQLITE_IOERR);
  TRACE2("READ %d\n", id->h);
  if( !ReadFile(id->h, pBuf, amt, &got, 0) ){
    got = 0;
  }
  if( got==(DWORD)amt ){
    return SQLITE_OK;
  }else{
    return SQLITE_IOERR;
  }
}

/*
** Write data from a buffer into a file.  Return SQLITE_OK on success
** or some other error code on failure.
*/
int sqlite3OsWrite(OsFile *id, const void *pBuf, int amt){
  int rc;
  DWORD wrote;
  SimulateIOError(SQLITE_IOERR);
  TRACE2("WRITE %d\n", id->h);
  while( amt>0 && (rc = WriteFile(id->h, pBuf, amt, &wrote, 0))!=0 && wrote>0 ){
    amt -= wrote;
    pBuf = &((char*)pBuf)[wrote];
  }
  if( !rc || amt>(int)wrote ){
    return SQLITE_FULL;
  }
  return SQLITE_OK;
}

/*
** Move the read/write pointer in a file.
*/
int sqlite3OsSeek(OsFile *id, off_t offset){
  LONG upperBits = offset>>32;
  LONG lowerBits = offset & 0xffffffff;
  DWORD rc;
  SEEK(offset/1024 + 1);
  rc = SetFilePointer(id->h, lowerBits, &upperBits, FILE_BEGIN);
  TRACE3("SEEK %d %lld\n", id->h, offset);
  return SQLITE_OK;
}

/*
** Make sure all writes to a particular file are committed to disk.
*/
int sqlite3OsSync(OsFile *id){
  TRACE2("SYNC %d\n", id->h);
  if( FlushFileBuffers(id->h) ){
    return SQLITE_OK;
  }else{
    return SQLITE_IOERR;
  }
}

/*
** Truncate an open file to a specified size
*/
int sqlite3OsTruncate(OsFile *id, off_t nByte){
  LONG upperBits = nByte>>32;
  TRACE3("TRUNCATE %d %lld\n", id->h, nByte);
  SimulateIOError(SQLITE_IOERR);
  SetFilePointer(id->h, nByte, &upperBits, FILE_BEGIN);
  SetEndOfFile(id->h);
  return SQLITE_OK;
}

/*
** Determine the current size of a file in bytes
*/
int sqlite3OsFileSize(OsFile *id, off_t *pSize){
  DWORD upperBits, lowerBits;
  SimulateIOError(SQLITE_IOERR);
  lowerBits = GetFileSize(id->h, &upperBits);
  *pSize = (((off_t)upperBits)<<32) + lowerBits;
  return SQLITE_OK;
}

/*
** Windows file locking notes:
**
** We cannot use LockFileEx() or UnlockFileEx() on Win95/98/ME because
** those functions are not available.  So we use only LockFile() and
** UnlockFile().
**
** LockFile() prevents not just writing but also reading by other processes.
** (This is a design error on the part of Windows, but there is nothing
** we can do about that.)  So the region used for locking is at the
** end of the file where it is unlikely to ever interfere with an
** actual read attempt.
**
** A SHARED_LOCK is obtained by locking a single randomly-chosen 
** byte out of a specific range of bytes. The lock byte is obtained at 
** random so two separate readers can probably access the file at the 
** same time, unless they are unlucky and choose the same lock byte.
** An EXCLUSIVE_LOCK is obtained by locking all bytes in the range.
** There can only be one writer.  A RESERVED_LOCK is obtained by locking
** a single byte of the file that is designated as the reserved lock byte.
** A PENDING_LOCK is obtained by locking a designated byte different from
** the RESERVED_LOCK byte.
**
** On WinNT/2K/XP systems, LockFileEx() and UnlockFileEx() are available,
** which means we can use reader/writer locks.  When reader/writer locks
** are used, the lock is placed on the same range of bytes that is used
** for probabilistic locking in Win95/98/ME.  Hence, the locking scheme
** will support two or more Win95 readers or two or more WinNT readers.
** But a single Win95 reader will lock out all WinNT readers and a single
** WinNT reader will lock out all other Win95 readers.
**
** The following #defines specify the range of bytes used for locking.
** SHARED_SIZE is the number of bytes available in the pool from which
** a random byte is selected for a shared lock.  The pool of bytes for
** shared locks begins at SHARED_FIRST.  
*/
#define SHARED_SIZE       10238
#define SHARED_FIRST      (0xffffffff - SHARED_SIZE + 1)
#define RESERVED_BYTE     (SHARED_FIRST - 1)
#define PENDING_BYTE      (RESERVED_BYTE - 1)

/*
** Return true (non-zero) if we are running under WinNT, Win2K or WinXP.
** Return false (zero) for Win95, Win98, or WinME.
**
** Here is an interesting observation:  Win95, Win98, and WinME lack
** the LockFileEx() API.  But we can still statically link against that
** API as long as we don't call it win running Win95/98/ME.  A call to
** this routine is used to determine if the host is Win95/98/ME or
** WinNT/2K/XP so that we will know whether or not we can safely call
** the LockFileEx() API.
*/
static int isNT(void){
  static int osType = 0;   /* 0=unknown 1=win95 2=winNT */
  if( osType==0 ){
    OSVERSIONINFO sInfo;
    sInfo.dwOSVersionInfoSize = sizeof(sInfo);
    GetVersionEx(&sInfo);
    osType = sInfo.dwPlatformId==VER_PLATFORM_WIN32_NT ? 2 : 1;
  }
  return osType==2;
}

/*
** Acquire a reader lock on the range of bytes from iByte...iByte+nByte-1.
** Different API routines are called depending on whether or not this
** is Win95 or WinNT.
*/
static int getReadLock(HANDLE h, unsigned int iByte, unsigned int nByte){
  int res;
  if( isNT() ){
    OVERLAPPED ovlp;
    ovlp.Offset = iByte;
    ovlp.OffsetHigh = 0;
    ovlp.hEvent = 0;
    res = LockFileEx(h, LOCKFILE_FAIL_IMMEDIATELY, 0, nByte, 0, &ovlp);
  }else{
    res = LockFile(h, iByte, 0, nByte, 0);
  }
  return res;
}

/*
** Undo a readlock
*/
static int unlockReadLock(OsFile *id){
  int res;
  if( isNT() ){
    res = UnlockFile(id->h, SHARED_FIRST, 0, SHARED_SIZE, 0);
  }else{
    res = UnlockFile(id->h, SHARED_FIRST + id->sharedLockByte, 0, 1, 0);
  }
  return res;
}

/*
** Acquire a lock of the given type on the specified file.  If an
** appropriate lock already exists, this routine is a no-op.  Return
** SQLITE_OK on success and SQLITE_BUSY if another thread is already
** holding a conflicting lock.
*/
int sqlite3OsLock(OsFile *id, int locktype){
  int rc = SQLITE_OK;    /* Return code from subroutines */
  int res = 1;           /* Result of a windows lock call */

  TRACE4("LOCK %d %d was %d\n", id->h, locktype, id->locktype);

  /* If there is already a lock of this type or more restrictive on the
  ** OsFile, do nothing. Don't use the end_lock: exit path, as
  ** sqlite3OsEnterMutex() hasn't been called yet.
  */
  if( id->locktype>=locktype ){
    return SQLITE_OK;
  }

  /* Lock the PENDING_LOCK byte if we need to acquire a PENDING lock or
  ** a SHARED lock.  If we are acquiring a SHARED lock, the acquisition of
  ** the PENDING_LOCK byte is temporary.
  */
  if( id->locktype==NO_LOCK || locktype==PENDING_LOCK ){
    int cnt = 4;
    while( cnt-->0 && (res = LockFile(id->h, PENDING_BYTE, 0, 1, 0))==0 ){
      /* Try 4 times to get the pending lock.  The pending lock might be
      ** held by another reader process who will release it momentarily.
      */
      Sleep(1);
    }
  }

  /* Acquire a shared lock
  */
  if( locktype>=SHARED_LOCK && id->locktype<SHARED_LOCK && res ){
    if( isNT() ){
      res = getReadLock(id->h, SHARED_FIRST, SHARED_SIZE);
    }else{
      int lk;
      sqlite3Randomness(sizeof(lk), &lk);
      id->sharedLockByte = (lk & 0x7fffffff)%(SHARED_SIZE - 1);
      res = LockFile(id->h, SHARED_FIRST+id->sharedLockByte, 0, 1, 0);
    }
    if( locktype<PENDING_LOCK ){
      UnlockFile(id->h, PENDING_BYTE, 0, 1, 0);
    }
  }

  /* Acquire a RESERVED lock
  */
  if( locktype>=RESERVED_LOCK && id->locktype<RESERVED_LOCK && res ){
    res = getReadLock(id->h, RESERVED_BYTE, 1);
  }

  /* Acquire an EXCLUSIVE lock
  */
  if( locktype==EXCLUSIVE_LOCK ){
    if( id->locktype>=SHARED_LOCK ){
      res = unlockReadLock(id);
    }
    if( res ){
      res = LockFile(id->h, SHARED_FIRST, 0, SHARED_SIZE, 0);
    }else{
      res = 0;
    }
  }

  /* Update the state of the lock has held in the file descriptor then
  ** return the appropriate result code.
  */
  if( res ){
    id->locktype = locktype;
    rc = SQLITE_OK;
  }else{
    TRACE2("LOCK FAILED %d\n", id->h);
    rc = SQLITE_BUSY;
  }
  return rc;
}

/*
** This routine checks if there is a RESERVED lock held on the specified
** file by this or any other process. If such a lock is held, return
** non-zero, otherwise zero.
*/
int sqlite3OsCheckWriteLock(OsFile *id){
  int rc;
  if( id->locktype>=RESERVED_LOCK ){
    rc = 1;
  }else{
    rc = getReadLock(id->h, RESERVED_BYTE, 1);
    if( rc ){
      UnlockFile(id->h, RESERVED_BYTE, 0, 1, 0);
    }
  }
  return 0;
}

/*
** Unlock the given file descriptor.  If the file descriptor was
** not previously locked, then this routine is a no-op.  If this
** library was compiled with large file support (LFS) but LFS is not
** available on the host, then an SQLITE_NOLFS is returned.
*/
int sqlite3OsUnlock(OsFile *id){
  int rc;
  TRACE3("UNLOCK %d was %d\n", id->h, id->locktype);
  if( id->locktype>=EXCLUSIVE_LOCK ){
    UnlockFile(id->h, SHARED_FIRST, 0, SHARED_SIZE, 0);
  }
  if( id->locktype>=PENDING_LOCK ){
    UnlockFile(id->h, PENDING_BYTE, 0, 1, 0);
  }
  if( id->locktype>=RESERVED_LOCK ){
    UnlockFile(id->h, RESERVED_BYTE, 0, 1, 0);
  }
  if( id->locktype==SHARED_LOCK ){
    unlockReadLock(id);
  }
  id->locktype = NO_LOCK;
  return SQLITE_OK;
}

/*
** Get information to seed the random number generator.  The seed
** is written into the buffer zBuf[256].  The calling function must
** supply a sufficiently large buffer.
*/
int sqlite3OsRandomSeed(char *zBuf){
  /* We have to initialize zBuf to prevent valgrind from reporting
  ** errors.  The reports issued by valgrind are incorrect - we would
  ** prefer that the randomness be increased by making use of the
  ** uninitialized space in zBuf - but valgrind errors tend to worry
  ** some users.  Rather than argue, it seems easier just to initialize
  ** the whole array and silence valgrind, even if that means less randomness
  ** in the random seed.
  **
  ** When testing, initializing zBuf[] to zero is all we do.  That means
  ** that we always use the same random number sequence.* This makes the
  ** tests repeatable.
  */
  memset(zBuf, 0, 256);
  GetSystemTime((LPSYSTEMTIME)zBuf);
  return SQLITE_OK;
}

/*
** Sleep for a little while.  Return the amount of time slept.
*/
int sqlite3OsSleep(int ms){
  Sleep(ms);
  return ms;
}

/*
** Static variables used for thread synchronization
*/
static int inMutex = 0;
#ifdef SQLITE_W32_THREADS
  static CRITICAL_SECTION cs;
#endif

/*
** The following pair of routine implement mutual exclusion for
** multi-threaded processes.  Only a single thread is allowed to
** executed code that is surrounded by EnterMutex() and LeaveMutex().
**
** SQLite uses only a single Mutex.  There is not much critical
** code and what little there is executes quickly and without blocking.
*/
void sqlite3OsEnterMutex(){
#ifdef SQLITE_W32_THREADS
  static int isInit = 0;
  while( !isInit ){
    static long lock = 0;
    if( InterlockedIncrement(&lock)==1 ){
      InitializeCriticalSection(&cs);
      isInit = 1;
    }else{
      Sleep(1);
    }
  }
  EnterCriticalSection(&cs);
#endif
  assert( !inMutex );
  inMutex = 1;
}
void sqlite3OsLeaveMutex(){
  assert( inMutex );
  inMutex = 0;
#ifdef SQLITE_W32_THREADS
  LeaveCriticalSection(&cs);
#endif
}

/*
** Turn a relative pathname into a full pathname.  Return a pointer
** to the full pathname stored in space obtained from sqliteMalloc().
** The calling function is responsible for freeing this space once it
** is no longer needed.
*/
char *sqlite3OsFullPathname(const char *zRelative){
  char *zNotUsed;
  char *zFull;
  int nByte;
  nByte = GetFullPathName(zRelative, 0, 0, &zNotUsed) + 1;
  zFull = sqliteMalloc( nByte );
  if( zFull==0 ) return 0;
  GetFullPathName(zRelative, nByte, zFull, &zNotUsed);
  return zFull;
}

/*
** The following variable, if set to a non-zero value, becomes the result
** returned from sqlite3OsCurrentTime().  This is used for testing.
*/
#ifdef SQLITE_TEST
int sqlite3_current_time = 0;
#endif

/*
** Find the current time (in Universal Coordinated Time).  Write the
** current time and date as a Julian Day number into *prNow and
** return 0.  Return 1 if the time and date cannot be found.
*/
int sqlite3OsCurrentTime(double *prNow){
  FILETIME ft;
  /* FILETIME structure is a 64-bit value representing the number of 
     100-nanosecond intervals since January 1, 1601 (= JD 2305813.5). 
  */
  double now;
  GetSystemTimeAsFileTime( &ft );
  now = ((double)ft.dwHighDateTime) * 4294967296.0; 
  *prNow = (now + ft.dwLowDateTime)/864000000000.0 + 2305813.5;
#ifdef SQLITE_TEST
  if( sqlite3_current_time ){
    *prNow = sqlite3_current_time/86400.0 + 2440587.5;
  }
#endif
  return 0;
}

#endif /* OS_WIN */
