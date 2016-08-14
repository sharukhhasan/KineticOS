/**
 *
 * Created by Sharukh Hasan on 8/09/16.
 * Copyright © 2016 HyperCryptic Solutions, LLC. All rights reserved.
 *
 */

#include "kernel/tch_kernel.h"
#include "kernel/tch_err.h"

/**
 * exception detected by software(kernel)
 */
void tch_kernel_onSoftException(tch_threadId who, tchStatus err, const char* msg)
{
	while (TRUE)
	{
		__WFE();
	}
}

void tch_kernel_onPanic(const char* floc, int lno, const char* msg)
{
	while (TRUE)
	{
		__WFE();
	}
}

void tch_kernel_onHardException(int fault, int spec)
{
	while (TRUE)
	{
		__WFE();
	}
}
