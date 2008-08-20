#ifndef LOCK_H
#define LOCK_H


/*
 * Author:	Arvin Schnell <aschnell@suse.de>
 */


#include <stdexcept>
#include <boost/noncopyable.hpp>


namespace storage
{

    class LockException : public std::exception
    {

    public:

	LockException(pid_t locker_pid);
	virtual ~LockException() throw();

	pid_t getLockerPid() const { return locker_pid; }

    protected:

	pid_t locker_pid;

    };


    /**
     * Implement a global read-only or read-write lock.
     */
    class Lock : boost::noncopyable
    {

    public:

	Lock(bool readonly, bool disable = false);
	~Lock() throw();

    private:

	bool disabled;
	int fd;

    };
}


#endif
