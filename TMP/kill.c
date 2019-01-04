/* kill.c - kill */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <sem.h>
#include <mem.h>
#include <io.h>
#include <q.h>
#include <stdio.h>

#include <lock.h>

extern lentry locks[];
LOCAL int resetlockprio(int pid);
/*------------------------------------------------------------------------
 * kill  --  kill a process and remove it from the system
 *------------------------------------------------------------------------
 */
SYSCALL kill(int pid)
{
	STATWORD ps;    
	struct	pentry	*pptr = &proctab[pid];		/* points to proc. table for pid*/
	int	dev;
	unsigned int llocks;
	unsigned int hlocks;
	int lid = 0;

	disable(ps);
	if (isbadpid(pid) || pptr->pstate==PRFREE) {
		restore(ps);
		return(SYSERR);
	}
	if (--numproc == 0)
		xdone();

	dev = pptr->pdevs[0];
	if (! isbaddev(dev) )
		close(dev);
	dev = pptr->pdevs[1];
	if (! isbaddev(dev) )
		close(dev);
	dev = pptr->ppagedev;
	if (! isbaddev(dev) )
		close(dev);
	
	send(pptr->pnxtkin, pid);

	freestk(pptr->pbase, pptr->pstklen);
	switch (pptr->pstate) {

	case PRCURR:	pptr->pstate = PRFREE;	/* suicide */
			//reset waiting lock prio if this proc is waiting on 
			//some lock
			resetlockprio(pid);
			//also release all the locks held by this process
			llocks = pptr->llocks;
			hlocks = pptr->hlocks;

			while(llocks > 0) {
				if(llocks&1) {
					releaseall(1, lid);
				}	
				llocks>>=1;
				lid++;
			}
			lid = 32;
			while(hlocks > 0) {
				if(hlocks&1) {
					releaseall(1, lid);
				}	
				hlocks>>=1;
				lid++;
			}
			resched();

	case PRWAIT:	semaph[pptr->psem].semcnt++;

	case PRREADY:	dequeue(pid);
			pptr->pstate = PRFREE;
			break;

	case PRSLEEP:
	case PRTRECV:	unsleep(pid);
						/* fall through	*/
	default:	pptr->pstate = PRFREE;
	}

	//reset waiting lock prio if this proc is waiting on 
	//some lock
	resetlockprio(pid);

	//also release all the locks held by this process
	llocks = pptr->llocks;
	hlocks = pptr->hlocks;

	while(llocks > 0) {
		if(llocks&1) {
			releaseall(1, lid);
		}	
		llocks>>=1;
		lid++;
	}
	lid = 32;
	while(hlocks > 0) {
		if(hlocks&1) {
			releaseall(1, lid);
		}	
		hlocks>>=1;
		lid++;
	}

	restore(ps);
	return(OK);
}
LOCAL int resetlockprio(int pid) {
	struct  pentry  *pptr = &proctab[pid];
        lentry *lptr;
        int max = -1;
        int head;
        int tail;
        int temp;
        int prio;

	//recalculate max prio if this proc 
        //is waiting on some lock
	//and dequeue this proc from 
	//waiting procs queue
        if(pptr->plock != -1) {
                //if this proc is max prio proc of all
                //procs waiting in the queue only then it
                //will affect he lock lprio value
		dequeue(pid);
                prio =(pptr->pinh != 0)?pptr->pinh:pptr->pprio;
                if((lptr=&locks[pptr->plock])->lprio == prio) {
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
                        //also update the priority(pinh) of all the processes holding this lock
                        //and set them to max above if max is greater than original priority
                        //of the process or 0 otherwise
                        rampupprio(pptr->plock);
                }
        }
	return OK;	
}
