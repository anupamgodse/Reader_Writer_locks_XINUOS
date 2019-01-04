#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <sem.h>
#include <stdio.h>

#include <lock.h>

extern lentry locks[];

int linit() {
	int i;
	lentry *lptr;
	for(i=0; i<NLOCKS; i++) {
		(lptr = &locks[i])->lstate = LFREE;
		lptr->ltype = READ;
		lptr->rcount = 0;
		/*lptr->lrqtail = 1 + (lptr->lrqhead = newqueue());
		lptr->lwqtail = 1 + (lptr->lwqhead = newqueue());*/
		lptr->lqtail = 1 + (lptr->lqhead = newqueue());
		lptr->lprio = -1;
		lptr->lprocs = 0;
		lptr->hprocs = 0;
	}

	//initialize value of nextlock to be used in lcreate()
	nextlock = NLOCKS-1;	

	return OK;
}
