/*
 * tch_spi.h
 *
 * Copyright (C) 2014 doowoong,lee
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the LGPL v3 license.  See the LICENSE file for details.
 *
 *
 *  Created on: 2014. 6. 15.
 *      Author: innocentevil
 */

#ifndef TCH_SPI_H_
#define TCH_SPI_H_

#include "kernel/tch_ktypes.h"

#if defined(__cplusplus)
extern "C"{
#endif

#define tch_spi0                     ((spi_t) 0)
#define tch_spi1                     ((spi_t) 1)
#define tch_spi2                     ((spi_t) 2)
#define tch_spi3                     ((spi_t) 3)
#define tch_spi4                     ((spi_t) 4)
#define tch_spi5                     ((spi_t) 5)
#define tch_spi6                     ((spi_t) 6)
#define tch_spi7                     ((spi_t) 7)
#define tch_spi8                     ((spi_t) 8)

#define SPI_FRM_FORMAT_16B           ((uint8_t) 1)
#define SPI_FRM_FORMAT_8B            ((uint8_t) 0)

#define SPI_FRM_ORI_MSBFIRST         ((uint8_t) 0)
#define SPI_FRM_ORI_LSBFIRST         ((uint8_t) 1)

#define SPI_ROLE_MASTER              ((uint8_t) 0)
#define SPI_ROLE_SLAVE               ((uint8_t) 1)

#define SPI_CLKMODE_0                ((uint8_t) 0)
#define SPI_CLKMODE_1                ((uint8_t) 1)
#define SPI_CLKMODE_2                ((uint8_t) 2)
#define SPI_CLKMODE_3                ((uint8_t) 3)

#define SPI_BAUDRATE_HIGH            ((uint8_t) 0)
#define SPI_BAUDRATE_NORMAL          ((uint8_t) 2)
#define SPI_BAUDRATE_LOW             ((uint8_t) 4)

typedef uint8_t spi_t;
typedef struct tch_spi_handle tch_spiHandle_t;


typedef struct tch_spi_cfg_t {
	uint8_t ClkMode;
	uint8_t FrmFormat;
	uint8_t FrmOrient;
	uint8_t Role;
	uint8_t Baudrate;
}spi_config_t;

struct tch_spi_handle {
	tchStatus (*close)(tch_spiHandle_t* self);
	tchStatus (*write)(tch_spiHandle_t* self,const void* wb,size_t sz);
	tchStatus (*read)(tch_spiHandle_t* self,void* rb,size_t sz, uint32_t timeout);
	tchStatus (*transceive)(tch_spiHandle_t* self,const void* wb,void* rb,size_t sz,uint32_t timeout);
};

typedef struct tch_hal_module_spi {
	const uint16_t count;
	void (*initCfg)(spi_config_t* cfg);
	tch_spiHandle_t* (*allocSpi)(const tch_core_api_t* env,spi_t spi,spi_config_t* cfg,uint32_t timeout,tch_PwrOpt popt);
}tch_hal_module_spi_t;

#if defined(__cplusplus)
}
#endif

#endif /* TCH_SPI_H_ */
