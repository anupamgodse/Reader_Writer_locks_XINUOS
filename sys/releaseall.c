/* signal.c - signal */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <sem.h>
#include <stdio.h>

#include <lock.h>

/*------------------------------------------------------------------------
 * releaseall  -- 	release all locks held by the current process and 
   		  	resched appropirate higher priority process next 
		 	waiting on the same lock
 *------------------------------------------------------------------------
 */

SYSCALL releaselock(int lock_i);
LOCAL int getnext(int lock_i);
LOCAL int highestpriowtr(int lock_i);
LOCAL int findmax(int lock_i);
LOCAL int resetpinh();

extern lentry locks[];

SYSCALL releaseall(int numlocks,long args, ...)
{
        STATWORD ps;
        register lentry  *lptr;
	struct pentry *pptr = &proctab[currpid];
	int error;
	int lock_i;
	int next;
	int type;
	//unsigned long *pbase = (unsigned long *)((pptr=&proctab[currpid])->pbase);
	//unsigned long *pesp = (unsigned long *)(pptr->pesp);
	int hpriowtr;
	int hprio;
	int head;
	int tail;
	int temp;
	int i;

        disable(ps);
	
	//int nargs = numlocks;
	unsigned long *a = (unsigned long *)(&args) + (numlocks-1); /* last argument      */

	//process lock release request one by one
	for(i = 0; i<numlocks; i++) {
		//take the ith passed argument
		lock_i = *a--;
        	if (isbadlock(lock_i) || (lptr= &locks[lock_i])->lstate==LFREE) {
			error=1;
			continue;
        	}
		

		//if the lock is not hold by the process skip the lock release and
		//mark error as set otherwise release the lock
		if(((lock_i < 32) && !(error = !((((pptr->llocks)>>lock_i)&1) == 1))) || 
			((lock_i >= 32) && !(error = !((((pptr->hlocks)>>(lock_i-32))&1) == 1))) || 
			(!isdeleted(lock_i))) {

				
			//account old lock type as releaselock() changes it
			type = lptr->ltype;
	

			//release the lock
			releaselock(lock_i);

			//if the releasing proc was a writer or last reader
			//holding the lock then choose next appropriate process to
			//which the lock has to be granted 
			if(type == WRITE || lptr->rcount == 0)	{
				next = getnext(lock_i);
		
				//grant lock to this waiting process if any
				if(next != SYSERR) {
					ready(dequeue(next), RESCHNO);
					//if released proc is reader
					//then release all other readers
					//having priority less than the 
					//max priority writer
					if(proctab[next].plwtype == READ) {
						//get highest priority writer
						hpriowtr = highestpriowtr(lock_i);
						if(hpriowtr!=SYSERR)
							hprio = proctab[hpriowtr].plprio;
						else 
							hprio = 0;
						
						//remove all readers with priority greater
						//than highest priority writer and
						//release them (make them ready)	
						head = lptr->lqhead;
						tail = lptr->lqtail;
						temp = q[head].qnext;
				
						while(temp != tail) {
							//if waiting reader has priority not less
							//than highest waiting writer then dequeue
							//the proc and make it ready
							if((proctab[temp].plwtype == READ) && 
								(proctab[temp].plprio >= hprio)) {
								next = temp;
								temp = q[temp].qnext;
								ready(dequeue(next), RESCHNO);
							}
							else 
								temp = q[temp].qnext;
						}
					}
				}
			}
		}
        }
	//set priority of this process to max
	//of all the procs waiting on all the locks
	//still held by this proc
	resetpinh();	

	resched();
	if(error == 1) {
		restore(ps);
		return SYSERR;
	}
	else {
		restore(ps);
        	return(OK);
	}
}

SYSCALL releaselock(int lock_i) {	
	lentry *lptr = &locks[lock_i];
	unsigned int lprocs = lptr->lprocs;
	unsigned int hprocs = lptr->hprocs;
	struct pentry *pptr = &proctab[currpid];
	unsigned int llocks = pptr->llocks;
	unsigned int hlocks = pptr->hlocks;

	//if released lock is reader then decrement rcount
	if(lptr->ltype == READ)
		(lptr->rcount)--;
	//else if the lock held was writer then make ltype as READ
	else
		lptr->ltype = READ;		
	

	//mark this lock is not hold by the process
	if(currpid < 32)
		lprocs ^= (1<<currpid);
	else
		hprocs ^= (1<<(currpid-32));

	//mark this process no longer holds this lock
	if(lock_i < 32)
		llocks ^= (1<<lock_i);
	else
		hlocks ^= (1<<(lock_i-32));

	//pptr->plockret = OK;
	//pptr->plock = -1;
	pptr->llocks = llocks;
	pptr->hlocks = hlocks;
	lptr->lprocs = lprocs;
	lptr->hprocs = hprocs;
	
	return OK;
}

LOCAL int getnext(int lock_i) {
	lentry *lptr = &locks[lock_i];
	/*
	int whead = lptr->lwqhead;
	int wtail = lptr->lwqtail;
	int rhead = lptr->lrqhead;
	int rtail = lptr->lrqtail;
	*/
	int head = lptr->lqhead;
	int tail = lptr->lqtail;
	int temp;
	int buff[NPROC];
	int inbuff=0;
	int bidx = 0;
	//int flag=0;

	struct pentry *pptr;
	int procid;
	int maxprio;
	int prio;
	int i;
	int maxwaitingproc;
	int next;

	for(i=0; i<NPROC; i++) {
		buff[i] = 0;
	}


	//find out the max waiting priority proc waiting
	procid = findmax(lock_i);

	//return SYSERR if no process is waiting
	if(procid == EMPTY) {
		return SYSERR;
	}
	
	maxprio = proctab[procid].plprio;

	//get all the same priority procs
	//waiting on the lock

	temp = q[head].qnext;
	//take same waiting prio procs in the buff
	while(temp != tail){
		prio = (pptr=&proctab[temp])->plprio;
		if(prio == maxprio)
			buff[bidx++] = temp;
		temp = q[temp].qnext;
	}

	/*		
	//if procid is empty then don't worry
	//but if procid is not empty then we have to insert that process back 
	//to the waiting procs queue
	if(procid != EMPTY)
		insert(procid, tail, proctab[procid].plprio);
	*/

	//now we have all waiting proc with same priority in buff
	//now choose the right reader or writer to next get
	//the lock  
	
	maxwaitingproc = 0;
	inbuff = bidx;
	//lets first find out the max waiting process having same prioritites
	for(i=1; i<bidx; i++) {
		//if the current process dosen't have waiting time atlease equal to 
		//the maxwaitingproc then invalidate buff entry
		//note here waiting time is higher if lclkstart is lower
		if(proctab[buff[maxwaitingproc]].lclkstart < proctab[buff[i]].lclkstart) {
			buff[i] = 0;	
			inbuff--;
			//insert(buff[i], tail, proctab[buff[i]].plprio);
		}
		//if waiting time of curr proc is higher than max waiting proc then
		//invalidate maxwaiting proc 
		//also update maxwaiting proc to curr proc
		else if(proctab[buff[maxwaitingproc]].lclkstart > proctab[buff[i]].lclkstart) {
			buff[maxwaitingproc] = 0;	
			inbuff--;
			//insert(buff[maxwaitingproc], tail, proctab[buff[maxwaitingproc]].plprio);
			maxwaitingproc = i;
		}
		//if waiting time is equal then keep both maxwaiting proc and 
		//currproc in buffer
	}


	//if we have only one proc in buff then choose it otherwise
	//if multiple procs then choose writer
	if(inbuff == 1) {
		next = buff[maxwaitingproc];
	}
	else {
		for(i=0; i<bidx; i++) {
			//if entry is not valid then continue
			if(buff[i] == 0)
				continue;
			//if the proc is of type write then 
			//give writer priority
			//choose this writer
			if(proctab[buff[i]].plwtype == WRITE) {
				next = buff[i];
				buff[i] = 0;
				inbuff--;
				break;
			}
		}
		//if there is no writer and all readers are waiting 
		//then schedule the first reader rest will be scheduled
		//by the caller
		if(i==bidx) {
			for(i=0; i<bidx; i++) {
				if(buff[i] != 0) {
					next = buff[i];
					buff[i] = 0;
					break;
				}
			}
		}
		/*//for all remaining unlucky procs in buf
		//reinsert them to the waiting procs queue
		for(i=0; i<bidx;i++) {
			if(buff[i] == 0)
				continue;
			insert(buff[i], tail, proctab[buff[i]].plprio);
		}*/
	}

	return next;		
}

LOCAL int highestpriowtr(int lock_i) {
        lentry *lptr = &locks[lock_i];
        int head = lptr->lqhead;
	int tail = lptr->lqtail;
        int temp = q[head].qnext;
	int maxpriowtr;
	int flag = 0;

        while((temp != tail)) {
		if(proctab[temp].plwtype == WRITE) {
			if(flag == 0) {
				flag = 1;
				maxpriowtr = temp;
			}
			else {
				if(proctab[maxpriowtr].plprio < proctab[temp].plprio) {
					maxpriowtr = temp;
				}
			}
		}
			
                temp = q[temp].qnext;
        }
        
        //flag == 1 then there is atlease one writer
        if(flag == 1)
		return maxpriowtr;

        return SYSERR;
}
LOCAL int findmax(int lock_i) {
	lentry *lptr = &locks[lock_i];
	int head = lptr->lqhead;
	int tail = lptr->lqtail; 
	int temp = q[head].qnext;
	int maxid;
	int flag = 0;
	
	while(temp != tail) {
		if(flag == 0) {
			maxid = temp;
			flag = 1;
		}	
		else if(proctab[temp].plprio > proctab[maxid].plprio) {
			maxid = temp;
		}
		temp = q[temp].qnext;
	}
	if(flag == 0)
		return SYSERR;
	
	return maxid;

}
LOCAL int resetpinh() {
	struct pentry *pptr = &proctab[currpid];
	unsigned int llocks = pptr->llocks;
	unsigned int hlocks = pptr->hlocks;
	int max = -1;
	int lock_i = 0;
	int temp;

	while(llocks > 0) {
		if(llocks & 1) {
			if((temp = locks[lock_i].lprio) > max) {
				max = temp;
			}
		}	
		llocks>>=1;
		lock_i++;
	}
	lock_i = 32;
	while(hlocks > 0) {
		if(hlocks & 1) {
			if((temp = locks[lock_i].lprio) > max) {
				max = temp;
			}
		}	
		hlocks>>=1;
		lock_i++;
	}
	
	pptr->pinh = (max != -1)?max:0;
	return OK;
}
