/* lcreate.c - lcreate, newlock */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <sem.h>
#include <stdio.h>
#include <lock.h>


extern lentry locks[];
LOCAL int newlock();

/*------------------------------------------------------------------------
 * lcreate  --  create and initialize a lock, returning its id
 *------------------------------------------------------------------------
 */
SYSCALL lcreate()
{
        STATWORD ps;
        int     lock_i;
	//int i;
	lentry *lptr;
	//struct pentry *pptr;

        disable(ps);
        if ((lock_i=newlock()) == SYSERR ) {
                restore(ps);
                return(SYSERR);
        }

	//init readers count for this lock to 0
	(lptr=&locks[lock_i])->ltype = READ;	
	lptr->rcount = 0;
	lptr->lprio = -1;
	lptr->lprocs = 0;
	lptr->hprocs = 0;

	/*
	//whichever procs have held this lock_i those procs were
	//waiting for this lock_i but this lock was deleted 
	//leaving proc pentry variables in inconsistent state
	//reset all procs variables which have this lock_i
	for(i=0; i<NPROC; i++) {
		if(((pptr=&proctab[i])->plock) == lock_i) {
			pptr->plock = -1;
			pptr->plockret = OK;
			pptr->lclkstart = 0;
		}
	}
	*/

        /* lrqhead\lwqhead and lrqtail/lwqtail were initialized at system startup */
        restore(ps);
        return(lock_i);
}

/*------------------------------------------------------------------------
 * newlock  --  allocate an unused lock and return its index
 *------------------------------------------------------------------------
 */
LOCAL int newlock()
{
        int     lock_i;
        int     i;

        for (i=0 ; i<NLOCKS ; i++) {
                lock_i=nextlock--;
                if (nextlock < 0)
                        nextlock = NLOCKS-1;
                if (locks[lock_i].lstate==LFREE) {
                        locks[lock_i].lstate = LUSED;
                        return(lock_i);
                }
        }
        return(SYSERR);
}
