/* resched.c  -  resched */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>

unsigned long currSP;	/* REAL sp of current process */
extern int ctxsw(int, int, int, int);
/*-----------------------------------------------------------------------
 * resched  --  reschedule processor to highest priority ready process
 *
 * Notes:	Upon entry, currpid gives current process id.
 *		Proctab[currpid].pstate gives correct NEXT state for
 *			current process if other than PRREADY.
 *------------------------------------------------------------------------
 */
int resched()
{
	register struct	pentry	*optr;	/* pointer to old process entry */
	register struct	pentry	*nptr;	/* pointer to new process entry */

	/* no switch needed if current process priority higher than next*/

	if ( ( (optr= &proctab[currpid])->pstate == PRCURR) &&
	   (lastkey(rdytail)<getprio(currpid))) {
		return(OK);
	}
	
	/* force context switch */

	if (optr->pstate == PRCURR) {
		optr->pstate = PRREADY;
		//kprintf("inserting %s having priority %d\n", nptr->pname, getprio(currpid));
		insert(currpid,rdyhead,getprio(currpid));
	}

	/* remove highest priority process at end of ready list */

	//now as the process's priority in ready queue can have been changed
	//deque and reinsert all the processes in the ready queue
	
	int pids[NPROC], i, x;
	for(i=0;i<NPROC;i++) {
		pids[i] = -1;
	}
	i = 0;
	while((x = getlast(rdytail))!=EMPTY) {
		//kprintf("lsat = %d\n", x);
		pids[i++] = x;
	}
	for(i=i-1;i>=0;i--) {
		insert(pids[i],rdyhead,getprio(pids[i]));
	}
	
	nptr = &proctab[ (currpid = getlast(rdytail)) ];
	nptr->pstate = PRCURR;		/* mark it currently running	*/
#ifdef	RTCLOCK
	preempt = QUANTUM;		/* reset preemption counter	*/
#endif
	

	//kprintf("scheduling %s having priority %d\n", nptr->pname, getprio(currpid));
	ctxsw((int)&optr->pesp, (int)optr->pirmask, (int)&nptr->pesp, (int)nptr->pirmask);
	
	/* The OLD process returns here when resumed. */
	return OK;
}
