/* wait.c - wait */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <sem.h>
#include <stdio.h>

#include <lock.h>

extern lentry locks[];
extern unsigned long clktime;

//a function to check if higher priority writer is present
//in a waiting lock queue

LOCAL int highpriowtr(int lock_i, int priority);
LOCAL int alreadyholds(int lock_i);
SYSCALL rampupprio(int lock_i);
SYSCALL isdeleted(int lock_i);


/*------------------------------------------------------------------------
 * lock  --  make current process wait on a lock for type type with
 * 	     priority priority
 *------------------------------------------------------------------------
 */
SYSCALL lock(int lock_i, int type, int priority)
{
        STATWORD ps;
        lentry  *lptr;
        struct  pentry  *pptr = &proctab[currpid];
	int nowait = 1;
	//int holds=0;

        disable(ps);


	//kprintf("%d call for lock\n", currpid);
	
	/* ---------------------------------------------------------------
         * Return SYSERR in following conditions: 
         * 1. If lock is DELETED by some process and this process tries to 
 	 * acquire this lock then it should return SYSERR. When the lock is
 	 * deleted the plockret variable of proc in made DELETED but
 	 * the procs plock variable has lock index value.
 	 * 2. If bad lock descripter or the lock is not created.
 	 *----------------------------------------------------------------
	 */ 	
        if (isbadlock(lock_i) || (lptr= &locks[lock_i])->lstate==LFREE 
		|| isdeleted(lock_i)) {
                restore(ps);
                return(SYSERR);
        }

	/*---------------------------------------------------------------- 
 	 * If the process holds this lock of with other permissions type 
 	 * and wants to now hold lock of some other type then release 
 	 * the earlier lock and retry of new lock type.
 	 * ---------------------------------------------------------------
 	 */	
	if(alreadyholds(lock_i)) {
		//if already holding lock of type WRITE and requesting for READ
		//then don't do anything as the process already has READ permissions
		//assumed that WRITE lock has implicit READ permissions
		if(lptr->ltype == type || lptr->ltype == WRITE) {
			restore(ps);
			return OK;
		}
		
		//if already holding lock of type READ and wants to upgrade to
		//WRITE then release READ lock and the process will get WRITE lock
		//if no other reader already holds the lock
		releaselock(lock_i);
	}
	


	/*----------------------------------------------------------------    
 	 *    1. if ltype is WRITE then reader or writer trying 
	 *    to acquire the lock has to wait.
         *    2. if ltype is READ and readers count is 0 that means 
	 *    lock is not given to any reader (or writer).
	 *    3. if the ltype is READ and the acquirer is reader and 
	 *    if there is higher priority writer already waiting on
	 *    lock then the reader should wait else reader gets 
	 *    the lock.
	 *    4. if the ltype is READ and the acquirer is a writer 
	 *    then acquirer has to wait.
	 *----------------------------------------------------------------    
	 */
  
	
	//kprintf("currpid = %d called lock\n", currpid);

	/*if ltype is not WRITE then it has to be READ*/
        if ((lptr->ltype == WRITE) || (lptr->rcount > 0 && type == READ && highpriowtr(lock_i, priority))
		|| (lptr->rcount > 0 && type == WRITE)) {

		//kprintf("currpid = %d called lock and waiting on it \n", currpid);
		nowait = 0;
                pptr->pstate = PRWAIT;
                pptr->plock = lock_i;
		pptr->lclkstart = clktime;
		pptr->plprio = priority;
		pptr->plwtype = type;
                //if the higher priority process(this) waiting on lock than highest 
                //priority process already waiting update the lprio field 
                //in lock table entry 
                if(((pptr->pinh!=0)?pptr->pinh:pptr->pprio) > lptr->lprio) {
			lptr->lprio = (pptr->pinh!=0)?pptr->pinh:pptr->pprio;
			//if this proc has higher prio than proc(s) holding
			//this lock then the pinh of all procs holding this
			//lock needs to be set to prio of this process
			rampupprio(lock_i);
		}
			

		//enqueue in queue of procs waiting for this lock
		enqueue(currpid, lptr->lqtail);
                pptr->plockret = OK;
                resched();
        }
	
	
	//kprintf("currpid = %d called lock and got it \n", currpid);

	//process can get lock in following two conditions
	//1. without waiting i.e. no process has taken the lock
	//2. pocess it gets lock after waiting and 
	//   lock is not deleted during waiting	
	if(nowait == 1 || pptr->plockret == OK) {
		//mark this lock as granted to currpid
		if(currpid < 32) {
			lptr->lprocs |= (1<<currpid);	
		}
		else {
			lptr->hprocs |= (1<<(currpid-32));
		}
		//set ltype and rcount if reader
		if(type == READ) {
			lptr->rcount++;
			lptr->ltype = READ;
		}
		else {
			lptr->ltype = WRITE;	
		}

		//mark the process now owns this lock
		if(lock_i < 32)
			pptr->llocks |= (1<<lock_i);
		else
			pptr->hlocks |= (1<<(lock_i-32));
		
		pptr->plock = -1;
		pptr->lclkstart = 0;
		
        	restore(ps);
        	return(OK);
	}

	//if lock is deleted
	//then proc no longer waiting for this lock
	//and reset its lock wait start time
	pptr->plock = -1;
	pptr->lclkstart = 0;
        restore(ps);
	return pptr->plockret;
}

LOCAL int highpriowtr(int lock_i, int priority) {
	lentry *lptr = &locks[lock_i];

	int head = lptr->lqhead;
	int tail = lptr->lqtail;
	int temp = q[head].qnext;
	//kprintf("head=%d\ttail=%d\ttemp=%d\n", head, tail, temp);

	//skip waiting readers and low prio writers
	while((temp != tail) && ((proctab[temp].plwtype == READ) || proctab[temp].plprio <= priority)) {
		temp = q[temp].qnext;
	}
	
	//if temp != tail that means there is a higher or same priority
	//writer waiting for the lock
	//kprintf("head=%d\ttail=%d\ttemp=%d\n", head, tail, temp);
	if(temp != tail) {
		return OK;
	}

	return 0;
}

SYSCALL rampupprio(int lock_i) {
	unsigned int lprocs;
	unsigned int hprocs;
	int lpid;
	lentry *lptr = &locks[lock_i];
	struct pentry *pptr;

	//for all procs holding this lock ramp up their
	//priority to this proc priority at it is maximum
	//of all procs waiting on this lock
	lprocs = lptr->lprocs;
	hprocs = lptr->hprocs;
	lpid = 0;
	while(lprocs != 0) {
		//if proc lpid holds the lock
		//then increase its prio to lptr->lprio
		if(lprocs & 1) {
			//prio = ((pptr=&proctab[lpid])->pinh != 0)?pptr->pinh:pptr->pprio;
			//if(prio < lptr->lprio) {
			if((pptr=&proctab[lpid])->pprio >= lptr->lprio) {
				pptr->pinh = 0;
				//now as the priority of process has changed 
				//dequeue and reinsert this process in ready
				//queue

			}
			else {
				pptr->pinh = lptr->lprio;	
				//also if this process is waiting on other lock
				//then increase the prio of other procs(if less)
				//holding the lock for which this proc is waiting 
				//to new increased prio of this proc 
				if(pptr->plock != -1) {
					if(locks[pptr->plock].lprio < pptr->pinh) {
						locks[pptr->plock].lprio = pptr->pinh;
						rampupprio(pptr->plock);
					}
				}
			}
		}	
		lprocs>>=1;
		lpid++;
	}
	lpid = 32;
	while(hprocs != 0) {
		//if proc lpid holds the lock
		//then increase its prio to lptr->lprio
		if(hprocs & 1) {
			if((pptr=&proctab[lpid])->pprio >= lptr->lprio)
				pptr->pinh = 0;
			else {
				pptr->pinh = lptr->lprio;	
				//also if this process is waiting on other lock
				//then increase the prio of other procs(if less)
				//holding the lock for which this proc is waiting 
				//to new increased prio of this proc 
				if(pptr->plock != -1) {
					if(locks[pptr->plock].lprio < pptr->pinh) {
						locks[pptr->plock].lprio = pptr->pinh;
						rampupprio(pptr->plock);
					}
				}
			}
		}	
		hprocs>>=1;
		lpid++;
	}
	
	return OK;
}
LOCAL int alreadyholds(lock_i) {
	lentry *lptr = &locks[lock_i];
	unsigned int lprocs = lptr->lprocs;
	unsigned int hprocs = lptr->hprocs;

	if((currpid < 32 && ((lprocs>>currpid)&1)) || 
		((hprocs>>(currpid-32))&1)) {
		return TRUE;
	}
	return FALSE;
}
SYSCALL isdeleted(int lock_i) {
	struct pentry *pptr = &proctab[currpid];
	return (lock_i<32)?(((pptr->dllocks)>>lock_i)&1):(((pptr->dhlocks)>>(lock_i-32))&1);
}
