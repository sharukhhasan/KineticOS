/**
 *
 * Created by Sharukh Hasan on 8/09/16.
 * Copyright © 2016 HyperCryptic Solutions, LLC. All rights reserved.
 *
 */


#include "kernel/tch_err.h"
#include "kernel/tch_kernel.h"
#include "kernel/tch_event.h"
#include "kernel/tch_kobj.h"

#ifndef EVENT_CLASS_KEY
#define EVENT_CLASS_KEY                        ((uint16_t )0xDABC)
#error "might not configured properly"
#endif

#define EVENT_VALIDATE(ins)                 do {\
	((tch_eventCb*) ins)->status = (((uint32_t) ins ^ EVENT_CLASS_KEY) & 0xFFFF);\
}while(0)

#define EVENT_INVALIDATE(ins)               do {\
	((tch_eventCb*) ins)->status &= ~0xFFFF;\
}while(0)

#define EVENT_ISVALID(ins)    ((((tch_eventCb*) ins)->status & 0xFFFF) == (((uint32_t) ins ^ EVENT_CLASS_KEY) & 0xFFFF))


__USER_API__ static tch_eventId tch_eventCreate();
__USER_API__ static int32_t tch_eventSet(tch_eventId ev,int32_t ev_signal);
__USER_API__ static int32_t tch_eventClear(tch_eventId ev,int32_t ev_signal);
__USER_API__ static tchStatus tch_eventWait(tch_eventId ev,int32_t signal_msk,uint32_t millisec);
__USER_API__ static tchStatus tch_eventDestroy(tch_eventId ev);


static tch_eventId event_init(tch_eventCb* evcb,BOOL is_static);
static tchStatus event_deinit(tch_eventCb* evcb);



__USER_RODATA__ tch_event_api_t Event_IX = {
		tch_eventCreate,
		tch_eventSet,
		tch_eventClear,
		tch_eventWait,
		tch_eventDestroy
};


__USER_RODATA__ const tch_event_api_t* Event = &Event_IX;


DECLARE_SYSCALL_0(event_create,tch_eventId);
DECLARE_SYSCALL_2(event_set,tch_eventId,int32_t,int32_t);
DECLARE_SYSCALL_2(event_clear,tch_eventId,int32_t,int32_t);
DECLARE_SYSCALL_3(event_wait,tch_eventId,int32_t,uint32_t,tchStatus);
DECLARE_SYSCALL_1(event_destroy,tch_eventId,tchStatus);
DECLARE_SYSCALL_1(event_init,tch_eventCb*,tchStatus);
DECLARE_SYSCALL_1(event_deinit,tch_eventCb*,tchStatus);



DEFINE_SYSCALL_0(event_create,tch_eventId){
	tch_eventCb* evcb = (tch_eventCb*) kmalloc(sizeof(tch_eventCb));
	if(!evcb)
		KERNEL_PANIC("can't create event object");
	return (tch_eventId) event_init(evcb,FALSE);
}

DEFINE_SYSCALL_2(event_set,tch_eventId,evid,int32_t,ev_signal,int32_t){
	if(!evid || !EVENT_ISVALID(evid))
		return tchErrorParameter;
	tch_eventCb* evcb = (tch_eventCb*) evid;
	int32_t psig = evcb->ev_signal;
	evcb->ev_signal |= ev_signal;
	if(((evcb->ev_msk & evcb->ev_signal) == evcb->ev_msk) && (!cdsl_dlistIsEmpty(&evcb->ev_blockq))){
		tch_schedWake(&evcb->ev_blockq,1,tchOK,TRUE);
	}
	return psig;
}

DEFINE_SYSCALL_2(event_clear,tch_eventId,evid,int32_t,ev_signal,int32_t){
	if(!evid || !EVENT_ISVALID(evid))
		return tchErrorParameter;
	tch_eventCb* evcb = (tch_eventCb*) evid;
	int32_t psig = evcb->ev_signal;
	evcb->ev_signal &= ~ev_signal;
	if(((evcb->ev_msk & evcb->ev_signal) == evcb->ev_msk) && (!cdsl_dlistIsEmpty(&evcb->ev_blockq))){
		tch_schedWake(&evcb->ev_blockq,1,tchOK,TRUE);
	}
	return psig;
}


DEFINE_SYSCALL_3(event_wait,tch_eventId,evid,int32_t,ev_signal,uint32_t,timeout,tchStatus){
	if(!evid || !EVENT_ISVALID(evid))
		return tchErrorParameter;
	tch_eventCb* evcb = (tch_eventCb*) evid;
	evcb->ev_msk = ev_signal;
	if((evcb->ev_msk & evcb->ev_signal) != evcb->ev_msk){
		tch_schedWait(&evcb->ev_blockq,timeout);
	}
	return tchOK;
}

DEFINE_SYSCALL_1(event_destroy,tch_eventId,evid,tchStatus){
	tchStatus result = event_deinit(evid);
	kfree(evid);
	return result;
}

DEFINE_SYSCALL_1(event_init,tch_eventCb*,ep,tchStatus){
	if(!ep)
		return tchErrorParameter;
	event_init(ep,TRUE);
	return tchOK;
}

DEFINE_SYSCALL_1(event_deinit,tch_eventCb*,ep,tchStatus){
	if(!ep || !EVENT_ISVALID(ep))
		return tchErrorParameter;
	event_deinit(ep);
	return tchOK;
}


tch_eventId event_init(tch_eventCb* evcb,BOOL is_static){
	mset(evcb,0,sizeof(tch_eventCb));
	tch_registerKobject(&evcb->__obj,is_static? (tch_kobjDestr) tch_eventDeinit :  (tch_kobjDestr) tch_eventDestroy);
	cdsl_dlistEntryInit(&evcb->ev_blockq.thque);
	EVENT_VALIDATE(evcb);
	return evcb;
}

tchStatus event_deinit(tch_eventCb* evcb){
	if(!evcb || !EVENT_ISVALID(evcb))
		return tchErrorParameter;
	if(!cdsl_dlistIsEmpty(&evcb->ev_blockq))
		tch_schedWake(&evcb->ev_blockq,SCHED_THREAD_ALL,tchErrorResource,FALSE);
	tch_unregisterKobject(&evcb->__obj);
	return tchOK;
}


__USER_API__ static tch_eventId tch_eventCreate(){
	if(tch_port_isISR())
		return NULL;
	return (tch_eventId) __SYSCALL_0(event_create);
}


__USER_API__ static int32_t tch_eventSet(tch_eventId ev,int32_t ev_signal){
	if(!ev || !ev_signal)
		return 0;
	if(tch_port_isISR())
		return __event_set(ev,ev_signal);
	else
		return __SYSCALL_2(event_set,ev,ev_signal);
}

__USER_API__ static int32_t tch_eventClear(tch_eventId ev,int32_t ev_signal){
	if(!ev || !EVENT_ISVALID(ev))
		return 0;
	if(tch_port_isISR())
		return __event_clear(ev,ev_signal);
	else
		return __SYSCALL_2(event_clear,ev,ev_signal);
}

__USER_API__ static tchStatus tch_eventWait(tch_eventId ev,int32_t signal_msk,uint32_t millisec){
	if(!ev || !EVENT_ISVALID(ev))
		return tchErrorParameter;
	if(tch_port_isISR())
		return tchErrorISR;
	return __SYSCALL_3(event_wait,ev,signal_msk,millisec);
}

__USER_API__ static tchStatus tch_eventDestroy(tch_eventId ev){
	if(!ev)
		return tchErrorParameter;
	if(!EVENT_ISVALID(ev))
		return tchErrorParameter;
	if(tch_port_isISR())
		return tchErrorISR;
	return __SYSCALL_1(event_destroy,ev);
}


tchStatus tch_evnetInit(tch_eventCb* evcb)
{
	if(!evcb)
		return  tchErrorParameter;
	if(tch_port_isISR() || !kernel_ready)
	{
		return __event_init(evcb);
	}
	return __SYSCALL_1(event_init,evcb);
}

tchStatus tch_eventDeinit(tch_eventCb* evcb)
{
	if(!evcb)
		return tchErrorParameter;
	if(tch_port_isISR() || !kernel_ready)
	{
		return __event_deinit(evcb);
	}
	return __SYSCALL_1(event_deinit,evcb);
}
