/**
 *
 * Created by Sharukh Hasan on 8/11/16.
 * Copyright © 2016 HyperCryptic Solutions, LLC. All rights reserved.
 *
 */



#include "kernel/tch_mpool.h"
#include "kernel/tch_kernel.h"

#ifndef MPOOL_CLASS_KEY
#define MPOOL_CLASS_KEY             ((uint16_t) 0x2D05)
#error "might not configured properly"
#endif


typedef struct tch_mpoolCb tch_mpoolCb;


struct tch_mpoolCb {
	tch_kobj             __obj;
	uint32_t             bstate;
	void*                bend;
	void*                bfree;
	uint32_t             balign;
	void*                bpool;
} __attribute__((aligned(4)));

static inline void tch_mpoolValidate(tch_mpoolId mp);
static inline void tch_mpoolInvalidate(tch_mpoolId mp);
static inline BOOL tch_mpoolIsValid(tch_mpoolId mp);

__USER_RODATA__ tch_mempool_api_t MPool_IX = {
		tch_mpoolCreate,
		tch_mpoolAlloc,
		tch_mpoolCalloc,
		tch_mpoolFree,
		tch_mpoolDestroy
};

__USER_RODATA__ const tch_mempool_api_t* Mempool = &MPool_IX;

__USER_API__ tch_mpoolId tch_mpoolCreate(size_t sz,uint32_t plen){
	tch_mpoolCb* mpcb = (tch_mpoolCb*) tch_shmAlloc(sizeof(tch_mpoolCb) + sz * plen);
	mset(mpcb,0,sizeof(tch_mpoolCb) + sz * plen);
	mpcb->bpool = (tch_mpoolCb*) mpcb + 1;
	mpcb->balign = sz;

	mpcb->__obj.__destr_fn = (tch_kobjDestr) tch_mpoolDestroy;
	mset(mpcb->bpool,0,sz * plen);
	void* next = NULL;
	uint8_t* blk = (uint8_t*) mpcb->bpool;
	uint8_t* end = blk + sz * plen;
	mpcb->bend = end;
	mpcb->bfree = blk;
	end = end - mpcb->balign;
	while(1){
		next = blk + mpcb->balign;
		if(next > (void*)end) break;
		*((void**) blk) = next;
		blk = (uint8_t*) next;
	}

	*((void**) blk) = 0;				// null indicate end of mem pool
	tch_mpoolValidate(mpcb);
	return (tch_mpoolId) mpcb;
}


__USER_API__ void* tch_mpoolAlloc(tch_mpoolId mpool){
	if(!tch_mpoolIsValid(mpool))
		return NULL;
	tch_mpoolCb* mpcb = (tch_mpoolCb*) mpool;
	if(!mpcb->bfree)
		return NULL;
	tch_port_atomicBegin();
	void** free = (void**)mpcb->bfree;
	if(free){
		mpcb->bfree = *free;
	}
	tch_port_atomicEnd();
	return free;
}


__USER_API__ void* tch_mpoolCalloc(tch_mpoolId mpool){
	tch_mpoolCb* mpcb = (tch_mpoolCb*) mpool;
	void* free = tch_mpoolAlloc(mpool);
	if(free){
		mset(free,0,mpcb->balign);
	}
	return free;
}

__USER_API__ tchStatus tch_mpoolFree(tch_mpoolId mpool,void* block){
	tch_mpoolCb* mpcb = (tch_mpoolCb*) mpool;
	if((block < mpcb->bpool) || (block > mpcb->bend))
		return tchErrorParameter;
	tch_port_atomicBegin();
	*((void**)block) = mpcb->bfree;
	mpcb->bfree = block;
	tch_port_atomicEnd();
	return tchOK;
}


__USER_API__ tchStatus tch_mpoolDestroy(tch_mpoolId mpool){
	if(!tch_mpoolIsValid(mpool)){
		return tchErrorParameter;
	}
	tch_mpoolInvalidate(mpool);
	tch_mpoolCb* mcb = (tch_mpoolCb*) mpool;
	tch_shmFree(mcb);
	return tchOK;
}


static inline void tch_mpoolValidate(tch_mpoolId mp){
	((tch_mpoolCb*) mp)->bstate |= (((uint32_t) mp & 0xFFFF) ^ MPOOL_CLASS_KEY);
}

static inline void tch_mpoolInvalidate(tch_mpoolId mp){
	((tch_mpoolCb*) mp)->bstate &= ~(0xFFFF);
}

static inline BOOL tch_mpoolIsValid(tch_mpoolId mp){
	return (((tch_mpoolCb*) mp)->bstate & 0xFFFF) == (((uint32_t) mp & 0xFFFF) ^ MPOOL_CLASS_KEY);
}

