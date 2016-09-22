/* 
 * (C) Copyright 2015 
 * MediaTek <www.mediatek.com>
 *
 * GMA302/GMA303/GMA305 driver for MT65XX
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef GMA30x_H
#define GMA30x_H
	 
#include <linux/ioctl.h>

//#define GMA_DEBUG_DATA		/* Default Disable.	1:Enable Gsensor debug data. */
#define GMA1302_I2C_SLAVE_ADDR		0x30

#define SENSOR_I2C_ADDR		0x18
/* Registers */
#define GMA1302_REG_PID 	0x00
#define GMA1302_REG_PD 		0x01
#define GMA1302_REG_ACTR 	0x02
#define GMA1302_REG_MTHR 	0x03
#define GMA1302_REG_STADR 	0x04
#define GMA1302_REG_STATUS 	0x05
#define GMA1302_REG_DX	 	0x06
#define GMA1302_REG_INTCR 	0x15
#define GMA1302_REG_CONTR1 	0x16
#define GMA1302_REG_CONTR2 	0x17
#define GMA1302_REG_CONTR3 	0x18
#define GMA1302_REG_OSM	 	0x38

#define GMA1302_MODE_RESET		0x02
#define GMA1302_MODE_POWERDOWN	0x05

#define GMA302_VAL_WMI			0x02
#define GMA303_VAL_WMI			0x03
#define GMA305_VAL_WMI			0x05
#define GME605_VAL_WMI			0xe5
#define GMA303_VAL_WMI_RD		0x33

#define GMA1302_VAL_OFFSET_TC_ON		0x40
#define GMA1302_VAL_DATA_READY_ON		0x2a
#define GMA1302_VAL_OFF					0x00
#define GMA1302_VAL_LPF_ON				0x09 /* low-pass filter on*/
#define GMA1302_VAL_HPF_ON				0x1b /* high-pass filter on*/
#define GMA1302_VAL_TRESHOLD_MAX		0x1F /* treshold set to max */
#define GMA1302_VAL_LOW_NOISE			0x5F /* Oversampling low noise */
#define GMA1302_VAL_ACTR_RESET			0x00 /* Reset DSP and AFE */
#define GMA1302_VAL_ACTR_STOP			0x01 /* Stop DSP*/
#define GMA1302_VAL_ACTR_CONTINUOUS		0x02 /* Enter continuous mode */
#define GMA1302_VAL_ACTR_NON_CONTINUOUS	0x04 /* Enter non-continuous mode */

#define AVG_NUM 				8	/* for calibration */
#define SENSOR_DATA_SIZE 		3 

typedef union {
	struct {
		int	x;
		int	y;
		int	z;
	} u;
	int	v[SENSOR_DATA_SIZE];
} raw_data;

#define GMA30x_SUCCESS						0
#define GMA30x_ERR_I2C						-1
#define GMA30x_ERR_STATUS					-3
#define GMA30x_ERR_SETUP_FAILURE			-4
#define GMA30x_ERR_GETGSENSORDATA			-5
#define GMA30x_ERR_IDENTIFICATION			-6
	 
	 
	 
#define GMA30x_BUFSIZE				256
	 
#endif

