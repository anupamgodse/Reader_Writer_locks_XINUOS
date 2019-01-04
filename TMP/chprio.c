/* chprio.c - chprio */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <stdio.h>
#include <lock.h>
/*------------------------------------------------------------------------
 * chprio  --  change the scheduling priority of a process
 *------------------------------------------------------------------------
 */

extern lentry locks[];

SYSCALL chprio(int pid, int newprio)
{
	STATWORD ps;    
	struct	pentry	*pptr;
	lentry *lptr;
	int max = -1;
	int head;
	int tail;
	int temp;
	int prio;

	disable(ps);
	if (isbadpid(pid) || newprio<=0 ||
	    (pptr = &proctab[pid])->pstate == PRFREE) {
		restore(ps);
		return(SYSERR);
	}
	int oldprio = (pptr->pinh!=0)?pptr->pinh:pptr->pprio;
	pptr->pprio = newprio;
	
	//recalculate max prio if this proc 
	//is waiting on some lock
	if(pptr->plock != -1) {
		//if newprio is greater than maxprio in the
		//queue then update maxprio
		if((lptr=&locks[pptr->plock])->lprio < newprio)
			lptr->lprio = newprio;
		//otherwise if newprio is less than the oldprio
		//and oldprio is equal to current lprio value
		//of lock then recalculate the max of all 
		//waiting procs if this procs prio
		//was max earlier
		else if(oldprio == lptr->lprio && newprio < oldprio) {
			head = lptr->lqhead;
			tail = lptr->lqtail;
			temp = q[head].qnext;
			while(temp != tail) {
				prio =(proctab[temp].pinh != 0)?proctab[temp].pinh:proctab[temp].pprio;
				if(prio > max)
					max = prio;	
				temp = q[temp].qnext;
			}
			lptr->lprio = max;
		}
		//also update the priority of all the processes holding this lock
		//and set them to updated lprio value
		rampupprio(pptr->plock);
	}


	restore(ps);
	return(newprio);
}
