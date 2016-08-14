/*
 * tch_thread.h
 *
 *
 * Copyright (C) 2014 doowoong,lee
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the LGPL v3 license.  See the LICENSE file for details.
 *
 *  Created on: 2014. 6. 30.
 *      Author: innocentevil
 */

#ifndef TCH_THREAD_H_
#define TCH_THREAD_H_


#include "tch_types.h"
#include "tch_loader.h"

#if defined(__cplusplus)
extern "C"{
#endif




/**\brief create new thread
 * \note this function should be invoked from privilidged mode.
 * \param[in] cfg thread configuration struct
 * \param[in] arg thread execution arguement
 * \param[in] root whether new thread is root or not
 *
 */
extern tch_thread_api_t Thread_IX;
extern tch_threadId tch_thread_createThread(thread_config_t* cfg,void* arg,BOOL isroot,BOOL ispriv,struct proc_header* proc);
extern tchStatus tch_thread_isValid(tch_threadId thread);
extern BOOL tch_thread_isPrivilidged(tch_threadId thread);
extern BOOL tch_thread_isRoot(tch_threadId thread);
extern BOOL tch_thread_isLive(tch_threadId thread);
extern void tch_thread_setPriority(tch_threadId tid,tch_threadPrior nprior);
extern tch_threadPrior tch_thread_getPriority(tch_threadId tid);
extern tchStatus tch_thread_exit(tch_threadId thread,tchStatus err);


#if defined(__cplusplus)
}
#endif

#endif /* TCH_THREAD_H_ */
