/**
 *
 * Created by Sharukh Hasan on 8/11/16.
 * Copyright © 2016 HyperCryptic Solutions, LLC. All rights reserved.
 *
 */
#include "tch_err.h"
#include "tch_kernel.h"
#include "cdsl_dlist.h"
#include "tch_sched.h"
#include "tch_thread.h"


/* =================  private internal function declaration   ========================== */

/***
 *  Scheduling policy Implementation
 */
static DECLARE_COMPARE_FN(tch_schedReadyQRule);
static DECLARE_COMPARE_FN(tch_schedWqRule);

/***
 *  Invoked when new thread start
 */
static inline void tch_schedStartKernelThread(tch_threadId thr)__attribute__((always_inline));
static BOOL tch_schedIsPreemptable(tch_thread_kheader* nth);

tch_thread_queue procList;
static tch_thread_queue tch_readyQue;        ///< thread wait to become running state
tch_thread_uheader* current;




void tch_schedInit(void* init_thread){

	/**
	 *  Initialize ready & pend queue
	 *   - ready queue : waiting queue in which active thread is waiting to occupy cpu run time (to be executed)
	 *   - pend queue : waiting queue in which suspended thread is waiting to be re activated
	 *
	 */
	cdsl_dlistEntryInit(&procList.thque);
	cdsl_dlistEntryInit(&tch_readyQue.thque);

	tch_schedStartKernelThread(init_thread);

	while(TRUE){     // Unreachable Code. Ensure to protect from executing when undetected schedulr failure happens
		__WFI();
	}
}


/** Note : should not called any other program except kernel mode program
 *  @brief start thread based on its priority (thread can be started in preempted way)
 */
tchStatus tch_schedStart(tch_threadId thread){
	if(!thread || (tch_thread_isValid(thread) != tchOK))
		return tchErrorParameter;
	tch_thread_uheader* thr = (tch_thread_uheader*) thread;
	tch_thread_kheader* kth = thr->kthread;
	tch_thread_kheader* pkth = NULL;
	if(tch_schedIsPreemptable(thr->kthread)){
		cdsl_dlistEnqueuePriority(&tch_readyQue.thque, &current->kthread->t_schedNode, tch_schedReadyQRule);			///< put current thread in ready queue
		pkth = current->kthread;
		pkth->state = READY;
		current = thr;
		tch_kernel_set_result(thread,tchOK);
		tch_schedSwitch(kth, pkth);
	}else{
		thr->kthread->state = READY;
		tch_kernel_set_result(current,tchOK);
		cdsl_dlistEnqueuePriority(&tch_readyQue.thque, &thr->kthread->t_schedNode, tch_schedReadyQRule);
	}
	return tchOK;
}

void tch_schedSwitch(tch_thread_kheader* next, tch_thread_kheader* cur)
{
	tch_port_enablePrivilegedThread();
	tch_port_atomicBegin();
	tch_port_setJmp((uwaddr_t) tch_port_switch, (uword_t) next, (uword_t) cur, 0);
}


/**
 * put new thread into ready queue
 */
void tch_schedReady(tch_threadId thread){
	tch_thread_uheader* thr = (tch_thread_uheader*) thread;
	thr->kthread->state = READY;
	cdsl_dlistEnqueuePriority(&tch_readyQue.thque, &thr->kthread->t_schedNode, tch_schedReadyQRule);
}

/** Note : should not called any other program except kernel mode program
 *  make current thread sleep for specified amount of time (yield cpu time)
 */
tchStatus tch_schedYield(uint32_t timeout,tch_timeunit tu,tch_threadState nextState){
	tch_thread_kheader* nth = NULL;
	tch_thread_kheader* pth = NULL;
	if(timeout == tchWaitForever)
		return tchErrorParameter;
	tch_systimeSetTimeout(current,timeout,tu);
	nth = (tch_thread_kheader*) cdsl_dlistDequeue(&tch_readyQue.thque);
	if(!nth)
		KERNEL_PANIC("Abnormal Task Queue : No Thread");
	tch_kernel_set_result(current,tchEventTimeout);
	pth = get_thread_kheader(current);
	pth->state = nextState;
	current = nth->uthread;
	tch_schedSwitch(nth, pth);
	return tchOK;
}


tchStatus tch_schedWait(tch_thread_queue* wq,uint32_t timeout){
	tch_thread_kheader* nth;
	tch_thread_kheader* pth;
	if(timeout != tchWaitForever){
		tch_systimeSetTimeout(current,timeout,mSECOND);
	}
	cdsl_dlistEnqueuePriority(&wq->thque,&get_thread_kheader(current)->t_waitNode,tch_schedWqRule);
	nth = (tch_thread_kheader*) cdsl_dlistDequeue(&tch_readyQue.thque);
	if(!nth)
		KERNEL_PANIC("Abnormal Task Queue : No Thread");

	pth = get_thread_kheader(current);
	pth->state = WAIT;
	pth->t_waitQ = &wq->thque;
	current = nth->uthread;
	tch_schedSwitch( nth, pth);
	return tchOK;
}


BOOL tch_schedWake(tch_thread_queue* wq,int cnt, tchStatus res, BOOL preemt){
	tch_thread_kheader* nth = NULL;
	tch_thread_kheader* tpreempt = NULL;
	if(cdsl_dlistIsEmpty(wq))
		return FALSE;
	while((!cdsl_dlistIsEmpty(wq)) && (cnt--)){
		nth = (tch_thread_kheader*)cdsl_dlistDequeue(&wq->thque);
		if(!nth)
			return FALSE;
		nth = container_of(nth, tch_thread_kheader, t_waitNode);
		nth->t_waitQ = NULL;
		if(nth->to)
			tch_systimeCancelTimeout(nth->uthread);
		tch_kernel_set_result(nth->uthread,res);
		if(tch_schedIsPreemptable(nth) && preemt){
			tpreempt = nth;
		}else{
			tch_schedReady(nth->uthread);
		}
	}
	if(tpreempt && preemt){
		tch_schedReady(current);
		nth = get_thread_kheader(current);
		current = tpreempt->uthread;
		tch_schedSwitch(tpreempt, nth);
	}
	return TRUE;
}


void tch_schedUpdate(void){
	tch_thread_kheader* nth = NULL;
	tch_thread_kheader* cth = current->kthread;
	if(cdsl_dlistIsEmpty(&tch_readyQue) || !tch_schedIsPreemptable((tch_thread_kheader*) tch_readyQue.thque.head))
		return;
	nth = container_of(cdsl_dlistDequeue(&tch_readyQue.thque), tch_thread_kheader, t_schedNode);
	cdsl_dlistEnqueuePriority(&tch_readyQue.thque, &cth->t_schedNode, tch_schedReadyQRule);
	cth->state = READY;
	current = nth->uthread;

	tch_schedSwitch(nth, cth);
}

BOOL tch_schedIsEmpty(){
	return cdsl_dlistIsEmpty(&tch_readyQue);
}

static BOOL tch_schedIsPreemptable(tch_thread_kheader* nth){
	if(!nth)
		return FALSE;
	return (tch_schedReadyQRule(nth,get_thread_kheader(current)) == nth) || ((get_thread_kheader(current)->tslot > 10) && (nth->prior != Idle));
}

void tch_schedTerminate(tch_threadId thread, int result){
	tch_thread_kheader* jth = 0;
	tch_thread_kheader* thr = ((tch_thread_uheader*) thread)->kthread;
	thr->state = TERMINATED;															/// change state of thread to terminated
	while(!cdsl_dlistIsEmpty(&thr->t_joinQ)){
		jth = (tch_thread_kheader*)  cdsl_dlistDequeue(&thr->t_joinQ);
		if(!jth)
			return;
		jth = container_of(jth, tch_thread_kheader, t_waitNode);
		jth->t_waitQ = NULL;
		cdsl_dlistEnqueuePriority( &tch_readyQue.thque, &jth->t_schedNode, tch_schedReadyQRule);
		tch_kernel_set_result(jth,result);
	}
	jth = (tch_thread_kheader*) cdsl_dlistDequeue(&tch_readyQue.thque);									/// get new thread from readyque
	if(!jth)
		return;
	jth = container_of(jth, tch_thread_kheader, t_schedNode);
	current = jth->uthread;
	tch_schedSwitch(jth,thr);
}


static inline void tch_schedStartKernelThread(tch_threadId init_thr){
	current = init_thr;
	current_mm = &current->kthread->mm;
	tch_thread_kheader* thr_p = (tch_thread_kheader*) get_thread_kheader(init_thr);
	tch_port_setUserSP((uwaddr_t)thr_p->ctx);
#if FEATURE_FLOAT > 0
	float _force_fctx = 0.1f;
	_force_fctx += 0.1f;
#endif
	thr_p->state = RUNNING;
	int result = get_thread_header(init_thr)->fn(get_thread_header(init_thr)->t_arg);
	Thread->exit(current,result);
}

static DECLARE_COMPARE_FN(tch_schedReadyQRule){
	return ((tch_thread_kheader*) a)->prior > ((tch_thread_kheader*) b)->prior?  a : b;
}

static DECLARE_COMPARE_FN(tch_schedWqRule){
	return NULL;
}

