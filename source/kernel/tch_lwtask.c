/**
 *
 * Created by Sharukh Hasan on 8/11/16.
 * Copyright © 2016 HyperCryptic Solutions, LLC. All rights reserved.
 *
 */

#include "tch_kernel.h"
#include "tch_lwtask.h"
#include "cdsl_nrbtree.h"

#define LWSTATUS_PENDING		((uint8_t) 1)
#define LWSTATUS_DONE			((uint8_t) 2)
#define LWSTATUS_RUN			((uint8_t) 4)

struct lw_task {
	tch_kobj            __kobj;
	nrbtreeNode_t       rbn;
	lwtask_routine      do_job;
	uint8_t	            prior;
	void*               arg;
	uint8_t             status;
	tch_mtxId           lock;
	tch_condvId         condv;
	dlistNode_t         tsk_qn;
};

static DECLARE_COMPARE_FN(lwtask_priority_rule);

static tch_waitqId		looper_wq;
static dlistEntry_t     tsk_queue;
static nrbtreeRoot_t	tsk_root;
static uint32_t			tsk_cnt;


static tch_mtxId		tsk_lock;
static tch_condvId		tsk_condv;

void __lwtsk_init(void)
{
//	looper_rendv = tch_rti->Rendezvous->create();
	looper_wq = tch_rti->WaitQ->create(WAITQ_POL_FIFO);
	tsk_cnt = 0;
	cdsl_dlistEntryInit(&tsk_queue);
	cdsl_nrbtreeRootInit(&tsk_root);
	tsk_lock = tch_rti->Mtx->create();
	tsk_condv = tch_rti->Condv->create();
}


void __lwtsk_start_loop(void)
{

	struct lw_task* tsk = NULL;
	Debug->print(0,0,"- Kernel task loop begin\n\r");
	while(TRUE){
		do {
			tch_port_atomicBegin();
			tsk = (struct lw_task*) cdsl_dlistDequeue(&tsk_queue);
			tch_port_atomicEnd();

			if (!tsk) {
				/*if (tch_rti->Rendezvous->sleep(looper_rendv, tchWaitForever) != tchOK)
				{
					return;	//break loop and return (means system termination)
				}
				*/
				if(tch_rti->WaitQ->sleep(looper_wq, tchWaitForever) != tchOK)
				{
					return;
				}
			}
		} while (!tsk);

		tsk = container_of(tsk,struct lw_task,tsk_qn);
		tch_rti->Mtx->lock(tsk->lock,tchWaitForever);
		if (tsk->status != LWSTATUS_DONE)
		{
			tsk->status = LWSTATUS_RUN;
			tsk->do_job(tsk->rbn.key, tch_rti, tsk->arg);// dead lock alert!! (any invocation of lwtask function causes dead lock
														 // note : task struct is locked at every lwtask interface
			tsk->status = LWSTATUS_DONE;
			tch_rti->Condv->wakeAll(tsk->condv);
		}	// else there is nothing to do with task
		tch_rti->Mtx->unlock(tsk->lock);
	}
}



int tch_lwtsk_registerTask(lwtask_routine fnp,uint8_t prior)
{
	if(!fnp)
		return -1;
	struct lw_task* tsk = (struct lw_task*) kmalloc(sizeof(struct lw_task));
	tsk->condv = tch_rti->Condv->create();
	tsk->lock = tch_rti->Mtx->create();
	tsk->arg = NULL;
	tsk->do_job = fnp;
	tsk->prior = prior;
	tsk->status = LWSTATUS_DONE;
	cdsl_dlistNodeInit(&tsk->tsk_qn);
	cdsl_nrbtreeNodeInit(&tsk->rbn,tsk_cnt++);

	tch_port_atomicBegin();
	cdsl_nrbtreeInsert(&tsk_root,&tsk->rbn);
	tch_port_atomicEnd();

	return tsk->rbn.key;
}

void tch_lwtsk_unregisterTask(int tsk_id)
{
	if(tsk_id < 0)
		return;
	struct lw_task* tsk = (struct lw_task*) cdsl_nrbtreeDelete(&tsk_root,tsk_id);
	if(!tsk)
		return;
	tsk = container_of(tsk,struct lw_task,rbn);
	if(tch_rti->Mtx->lock(tsk->lock,tchWaitForever) != tchOK)
		return;		// may be this task is already destroyed or invalid
	while(tsk->status != LWSTATUS_DONE)
	{			//wait until done
		if(tch_rti->Condv->wait(tsk->condv,tsk->lock,tchWaitForever) != tchOK)
			return;
	}
	tch_rti->Mtx->unlock(tsk->lock);


	tch_rti->Mtx->destroy(tsk->lock);
	tch_rti->Condv->destroy(tsk->condv);
	tchk_kernelHeapFree(tsk);

}


void tch_lwtsk_request(int tsk_id,void* arg, BOOL canblock)
{
	if((tsk_id > tsk_cnt) || (tsk_id < 0))
		return;

	struct lw_task* tsk = (struct lw_task*) cdsl_nrbtreeLookup(&tsk_root,tsk_id);
	if(!tsk)
		return;

	tsk = container_of(tsk,struct lw_task,rbn);
	if(tch_port_isISR())
	{
		canblock = FALSE;
	}

	if (canblock)
	{
		if (tch_rti->Mtx->lock(tsk->lock, tchWaitForever) != tchOK)
			return;
		while (tsk->status != LWSTATUS_DONE) {
			tch_rti->Condv->wait(tsk->condv, tsk->lock, tchWaitForever);
		}
		tsk->status = LWSTATUS_PENDING;
		tsk->arg = arg;
		tch_rti->Mtx->unlock(tsk->lock);
	}
	else
	{
		tch_port_atomicBegin();
		if(tsk->status != LWSTATUS_DONE){
			tch_port_atomicEnd();
			return;
		}

		tsk->status = LWSTATUS_PENDING;
		tsk->arg = arg;
		tch_port_atomicEnd();
	}

	tch_port_atomicBegin();
	cdsl_dlistEnqueuePriority(&tsk_queue,&tsk->tsk_qn,lwtask_priority_rule);
	tch_port_atomicEnd();

//	tch_rti->Rendezvous->wake(looper_rendv);
	tch_rti->WaitQ->wake(looper_wq);


	if(canblock)
	{
		if (tch_rti->Mtx->lock(tsk->lock, tchWaitForever) != tchOK)
			return;
		while (tsk->status != LWSTATUS_DONE) {
			tch_rti->Condv->wait(tsk->condv, tsk->lock, tchWaitForever);
		}
		tch_rti->Mtx->unlock(tsk->lock);
	}
}

void tch_lwtsk_cancel(int tsk_id){
	if((tsk_id > tsk_cnt) || (tsk_id < 0))
		return;

	struct lw_task* tsk = (struct lw_task*) cdsl_nrbtreeLookup(&tsk_root,tsk_id);
	if(!tsk)
		return;

	tsk = container_of(tsk,struct lw_task,rbn);
	if(tch_rti->Mtx->lock(tsk->lock,tchWaitForever) != tchOK)
		return;
	tsk->status = LWSTATUS_DONE;
	tch_port_atomicBegin();
	cdsl_dlistRemove(&tsk->tsk_qn);
	tch_port_atomicEnd();
	tch_rti->Mtx->unlock(tsk->lock);
}

static DECLARE_COMPARE_FN(lwtask_priority_rule){
	return ((struct lw_task*)container_of(a,struct lw_task,tsk_qn))->prior > ((struct lw_task*)container_of(b,struct lw_task,tsk_qn))->prior? a : b;
}

