/*
 * Author:	Arvin Schnell <aschnell@suse.de>
 */


#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>

#include "y2storage/AppUtil.h"
#include "y2storage/Lock.h"


namespace storage
{

    LockException::LockException(pid_t locker_pid)
	: locker_pid(locker_pid)
    {
    }


    LockException::~LockException() throw()
    {
    }


    Lock::Lock(bool readonly)
	: fd(-1)
    {
	y2mil("getting " << (readonly ? "read-only" : "read-write") << " lock");

	if (mkdir("/var/lock/libstorage", 0622) < 0)
	{
	    // Creating directory failed. Not fatal (should already exist).
	    y2deb("creating directory for lock-file failed: " << strerror(errno));
	}

	fd = open("/var/lock/libstorage/lock", (readonly ? O_RDONLY : O_WRONLY) | O_CREAT,
		  S_IRUSR | S_IWUSR);
	if (fd < 0)
	{
	    // Opening lock-file failed.
	    y2err("opening lock-file failed: " << strerror(errno));
	    throw(LockException(0));
	}

	struct flock lock;
	memset(&lock, 0, sizeof(lock));
	lock.l_whence = SEEK_SET;
	lock.l_type = (readonly ? F_RDLCK : F_WRLCK);
	if (fcntl(fd, F_SETLK, &lock) < 0)
	{
	    switch (errno)
	    {
		case EACCES:
		case EAGAIN:
		    // Another process has a lock. Between the two fcntl
		    // calls the lock of the other process could be
		    // release. In that case we don't get the pid (and it is
		    // still 0).
		    fcntl(fd, F_GETLK, &lock);
		    y2err("locked by process " << lock.l_pid);
		    throw(LockException(lock.l_pid));

		default:
		    // Some other error.
		    y2err("getting lock failed: " << strerror(errno));
		    throw(LockException(0));
	    }
	}

	y2mil("lock succeeded");
    }


    Lock::~Lock() throw()
    {
	y2mil("releasing lock");
	close(fd);

	// Do not bother deleting lock-file. This is difficult if there are
	// several read-only locks.
    }

}
