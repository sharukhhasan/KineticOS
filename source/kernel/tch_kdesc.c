/**
 *
 * Created by Sharukh Hasan on 8/10/16.
 * Copyright © 2016 HyperCryptic Solutions, LLC. All rights reserved.
 *
 */


#include "kernel/tch_kernel.h"


#ifndef VER_MAJOR
#define VER_MAJOR	0
#endif

#ifndef VER_MINOR
#define VER_MINOR	0
#endif

#ifndef ARCH_NAME
#define ARCH_NAME "not specified"
#endif

#ifndef PLATFORM_NAME
#define PLATFORM_NAME "not specified"
#endif



const tch_kernel_descriptor kernel_descriptor = {
		.version_major = VER_MAJOR,
		.version_minor = VER_MINOR,
		.arch_name = ARCH_NAME,
		.pltf_name = PLATFORM_NAME,
};
