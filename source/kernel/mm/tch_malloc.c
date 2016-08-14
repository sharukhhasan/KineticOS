/**
 *
 * Created by Sharukh Hasan on 8/07/16.
 * Copyright © 2016 HyperCryptic Solutions, LLC. All rights reserved.
 *
 */




#include "tch_kernel.h"
#include "tch_malloc.h"

__USER_RODATA__ tch_malloc_api_t UMem_IX = {
		.alloc = tch_malloc,
		.free = tch_free,
		.mstat = tch_mstat
};

__USER_RODATA__ const tch_malloc_api_t* uMem = &UMem_IX;

__USER_API__ void* tch_malloc(size_t sz){
	void* result;
	if(!sz)
		return NULL;
	result = wt_cacheMalloc(current->cache, sz);
	if (!result) {
		tch_mtxId mtx = current->mtx;
		if (Mtx->lock(mtx, tchWaitForever) != tchOK)
			return NULL;
		result = wt_malloc(current->heap, sz);
		Mtx->unlock(mtx);
	}
	return result;
}

__USER_API__ void tch_free(void* ptr){
	if(!ptr)
		return;
	int result;
	if(WT_OK == (result = wt_cacheFree(current->cache,ptr)))
		return;
	if(result == WT_ERROR)
		goto ERR_HEAP_FREE;
	tch_mtxId mtx = current->mtx;
	if(Mtx->lock(mtx,tchWaitForever) != tchOK)
		return;
	result = wt_free(current->heap,ptr);
	Mtx->unlock(mtx);
	if(result == WT_ERROR)
		goto ERR_HEAP_FREE;
	return;

ERR_HEAP_FREE :
	tch_kernel_onSoftException(current,tchErrorHeapCorruption,"heap corrupted");
}


__USER_API__ void tch_mstat(mstat* statp){
	if(!statp)
		return;

	wt_heapRoot_t* uheap = (wt_heapRoot_t*) current->heap;

	statp->total = uheap->size;
	statp->used = uheap->size - uheap->free_sz;
	statp->cached = ((wt_cache_t* ) current->cache)->size;
}
