#ifndef _LOCKH_
#define _LOCKH_

#define LFREE		0	/*lock is free*/
#define LUSED 		1	/*lock is used*/
#define READ 		2	/*Lock acquired by reader*/
#define WRITE 		3	/*Lock acquired by writer*/
#define NLOCKS 		50	/*max available locks*/


//function prototypes

//creates a lock and returns lock descriptor
int lcreate(void);

//deletes a lock identified by a descriptor
int ldelete(int lockdescriptor);

//acquire a lock ldes1 of type type with priority priority
int lock(int ldes1, int type, int priority);

//release numlocks number of locks having descriptors pushed on stack
int releaseall(int numlocks, long args, ...);

typedef struct lentry{
	int lstate;	//free or used or DELETED
	int ltype;	//read or write
	int rcount;	//number of readers
	int lqhead;	//head of queue of procs waiting on this lock
	int lqtail;	//tail of queue of procs waiting on this lock
	int lprio;	//max prio proc waiting on this lock
	unsigned int lprocs;	//bitmap to indicate procs(0-31) waiting?
	unsigned int hprocs;	//bitmap to indicate procs(32-49) waiting?
}lentry;

extern	int	nextlock;

#define	isbadlock(lock_i)	(lock_i<0 || lock_i>=NLOCKS)

#endif
