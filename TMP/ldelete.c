/* sdelete.c - sdelete */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <sem.h>
#include <stdio.h>

#include <lock.h>

extern lentry locks[];

/*------------------------------------------------------------------------
 *  * ldelete  --  delete a lock by releasing its table entry
 *   *------------------------------------------------------------------------
 *    */
SYSCALL ldelete(int lock_i)
{
        STATWORD ps;    
        int     pid;
        lentry  *lptr;
	int head, tail, temp;
	unsigned int lprocs, hprocs;
        int lpid=0;

        disable(ps);
        if (isbadlock(lock_i) || (lptr=&locks[lock_i])->lstate==LFREE) {
                restore(ps);
                return(SYSERR);
        }
        lptr->lstate = LFREE;

	//mark lock as deleted for processes
	//waiting on this lock and put them
	//to ready queue
	head = lptr->lqhead;
	tail = lptr->lqtail;
	temp = q[head].qnext;
	while(temp != tail) {
		pid = temp;
		temp = q[temp].qnext;
		proctab[pid].plockret = DELETED;
		//this process can never use this lock_i
		//again as it is deleted now
		//in future if this process tries to acquire
		//lock for this lock descripter it cannot
		//it will return SYSERR
		if(lock_i < 32) {
			proctab[pid].dllocks |= (1<<lock_i);
		}
		else {
			proctab[pid].dhlocks |= (1<<(lock_i-32));
		}
		ready(dequeue(pid), RESCHNO);	
	}

	//for all the processes currently holding the lock
        //set the d(h/l)locks bitmap field corresponding to this lock 
	//so that when they attempt to release this lock don't 
	//release it but just return OK (from releaseall.c)
	//also when they will again try to acquire this lock
	//lock.c will return SYSERR
	//also update the bitmap field of processes((l/h)locks) 
	//indicating that this process no longer holds this lock
        
        lprocs = lptr->lprocs;
        hprocs = lptr->hprocs;

        while(lprocs>0) {
                if(lprocs&1) {
			if(lock_i < 32) {
				proctab[lpid].dllocks |= (1<<lock_i);
				proctab[lpid].llocks ^= (1<<lock_i);
			}
			else {
				proctab[lpid].dhlocks |= (1<<(lock_i-32));
				proctab[lpid].hlocks ^= (1<<(lock_i-32));
			}
                }
                lprocs>>=1;
                lpid++;
        }
        lpid = 32;
        while(hprocs>0) {
                if(hprocs&1) {
			if(lock_i < 32) {
				proctab[lpid].dllocks |= (1<<lock_i);
				proctab[lpid].llocks ^= (1<<lock_i);
			}
			else {
				proctab[lpid].dhlocks |= (1<<(lock_i-32));
				proctab[lpid].hlocks ^= (1<<(lock_i-32));
			}
                }
                hprocs>>=1;
                lpid++;
        }
        //set no one is holding the lock now
        lptr->lprocs = 0;
        lptr->hprocs = 0;
	
	resched();

        restore(ps);
        return(OK);
}
