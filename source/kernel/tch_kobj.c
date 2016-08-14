/**
 *
 * Created by Sharukh Hasan on 8/11/16.
 * Copyright © 2016 HyperCryptic Solutions, LLC. All rights reserved.
 *
 */


#include "kernel/tch_kobj.h"
#include "kernel/tch_kernel.h"
#include "cdsl_dlist.h"

/**
 *  kobject is to provide basic framework for kernel level object whose leak causes more serious problem than simple memory leak.
 *  for example, mutex lock has its own control block which has wait queue for blocking request thread. when the thread that creates the mutex
 *  lock is terminated without destroying the mutex. it's not only memory leak but also many threads can be unintentionally lost.
 *  kobject is simple data struct to manage those critical data within kernel by linking those in the list from thread control block.
 *  if thread is terminated, kernel follow the list and forces destructor of any object to be invoked.
 *
 */




tchStatus tch_registerKobject(tch_kobj* obj, tch_kobjDestr destfn){
	if(!obj || !destfn)
		return tchErrorParameter;
	cdsl_dlistNodeInit(&obj->lhead);
	obj->__destr_fn = destfn;
	cdsl_dlistPutTail((dlistEntry_t*) &current_mm->kobj_list,&obj->lhead);
	return tchOK;
}

tchStatus tch_unregisterKobject(tch_kobj* obj){
	if(!obj)
		return tchErrorParameter;

	if(!obj->lhead.next && !obj->lhead.prev)
		return tchErrorParameter;

	cdsl_dlistRemove(&obj->lhead);
	obj->lhead.next = obj->lhead.prev = NULL;
	return tchOK;
}


tchStatus tch_destroyAllKobjects(){
	while(!cdsl_dlistIsEmpty(&current_mm->kobj_list)){
		tch_kobj* obj = (tch_kobj*) cdsl_dlistDequeue((dlistEntry_t*) &current_mm->kobj_list);
		obj = container_of(obj,tch_kobj,lhead);
		obj->__destr_fn(obj);
	}
	return tchOK;
}


tchStatus __tch_noop_destr(tch_kobj* obj){return tchOK; }
