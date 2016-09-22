/*
 $License:
    Copyright (C) 2011 InvenSense Corporation, All Rights Reserved.
 $
 */

/* WARNING: autogenerated code, do not modify */
/**
 *  @defgroup DMPDEFAULT
 *  @brief
 *
 *  @{
 *      @file   dmpDefaultMPU6050.c
 *      @brief
 *
 */

#include <linux/module.h>
#include "dmpKey.h"
#include "dmpmap.h"

#define CFG_FB_GAIN_GYRO_ON         (1750)
//#define CFG_OUT_PRESS       (2172)
#define CFG_PED_ENABLE      (2315)
#define CFG_OUT_GYRO        (2191)
#define CFG_PEDSTEP_DET     (3143)
#define OUT_GYRO_DAT        (2201)
#define CFG_FIFO_INT        (2313)
//#define OUT_CPASS_DAT       (2147)
#define CFG_AUTH            (1306)
#define OUT_ACCL_DAT        (2166)
#define FCFG_1              (1334)
#define FCFG_3              (1364)
#define FCFG_2              (1338)
//#define CFG_OUT_CPASS       (2138)
#define FCFG_7              (1345)
#define CFG_OUT_3QUAT       (2049)
//#define OUT_PRESS_DAT       (2181)
#define OUT_3QUAT_DAT       (2060)
#define CFG_7               (1685)
#define OUT_PQUAT_DAT       (2131)
#define CFG_OUT_6QUAT       (2085)
#define CFG_PED_INT         (3132)
#define SMD_TP2             (1634)
#define SMD_TP1             (1611)
#define CFG_MOTION_BIAS     (1687)
#define CFG_OUT_ACCL        (2156)
#define CFG_OUT_STEPDET     (2018)
#define OUT_6QUAT_DAT       (2096)
#define CFG_OUT_PQUAT       (2121)
//#define CFG_DISPLAY_ORIENT_INT  (1754)
//#define FCFG_6                  (1121)
//#define CFG_EXT_GYRO_BIAS       (1200)
//#define CFG_GYRO_RAW_DATA       (2193)

//keys required for turning off sensors
#define QUAT6_FIFO_RATE     (2071)
#define PQUAT_FIFO_RATE     (2107)
#define ACCEL_FIFO_RATE     (2142)
#define GYRO_FIFO_RATE      (2177)
#define PREV_PQUAT          (2212)
#define STEPDET_END         (2035)
#define PED_STEP_COUNT2_DETECTED    (3149)

#define D_0_22                  (22+512)
#define D_0_24                  (24+512)

#define D_0_36                  (36)
#define D_0_52                  (52)
#define D_0_96                  (96)
#define D_0_104                 (104)
#define D_0_108                 (108)
#define D_0_163                 (163)
#define D_0_188                 (188)
#define D_0_192                 (192)
#define D_0_224                 (224)
#define D_0_228                 (228)
#define D_0_232                 (232)
#define D_0_236                 (236)

#define D_1_2                   (256 + 2)
#define D_1_4                   (256 + 4)
#define D_1_8                   (256 + 8)
#define D_1_10                  (256 + 10)
#define D_1_24                  (256 + 24)
#define D_1_28                  (256 + 28)
#define D_1_36                  (256 + 36)
#define D_1_40                  (256 + 40)
#define D_1_44                  (256 + 44)
#define D_1_72                  (256 + 72)
#define D_1_74                  (256 + 74)
#define D_1_79                  (256 + 79)
#define D_1_88                  (256 + 88)
#define D_1_90                  (256 + 90)
#define D_1_92                  (256 + 92)
#define D_1_96                  (256 + 96)
#define D_1_98                  (256 + 98)
#define D_1_106                 (256 + 106)
#define D_1_108                 (256 + 108)
#define D_1_112                 (256 + 112)
#define D_1_128                 (256 + 144)
#define D_1_152                 (256 + 12)
#define D_1_160                 (256 + 160)
#define D_1_176                 (256 + 176)
#define D_1_178                 (256 + 178)
#define D_1_218                 (256 + 218)
#define D_1_232                 (256 + 232)
#define D_1_236                 (256 + 236)
#define D_1_240                 (256 + 240)
#define D_1_244                 (256 + 244)
#define D_1_250                 (256 + 250)
#define D_1_252                 (256 + 252)
#define D_2_12                  (512 + 12)
#define D_2_96                  (512 + 96)
#define D_2_108                 (512 + 108)
#define D_2_208                 (512 + 208)
#define D_2_224                 (512 + 224)
#define D_2_236                 (512 + 236)
#define D_2_244                 (512 + 244)
#define D_2_248                 (512 + 248)
#define D_2_252                 (512 + 252)
#define D_EXT_GYRO_BIAS_X       (61 * 16)
#define D_EXT_GYRO_BIAS_Y       (61 * 16) + 4
#define D_EXT_GYRO_BIAS_Z       (61 * 16) + 8
#define D_ACT0                  (40 * 16)
#define D_ACSX                  (40 * 16 + 4)
#define D_ACSY                  (40 * 16 + 8)
#define D_ACSZ                  (40 * 16 + 12)

#define D_SMD_ENABLE            (18 * 16)
#define D_SMD_MOT_THLD          (21 * 16 + 8)
#define D_SMD_DELAY_THLD        (21 * 16 + 4)
#define D_SMD_DELAY2_THLD       (21 * 16 + 12)
#define D_SMD_EXE_STATE         (22 * 16)
#define D_SMD_DELAY_CNTR        (21 * 16)

#define D_VIB_DET_RATIO         (30 * 16 + 0)
#define D_VIB_MAG_TH            (31 * 16 + 0)
#define D_VIB_K                 (13 * 16 + 8)
#define D_VIB_K_1               (13 * 16 + 12)

#define D_ACC_SCALE             (29 * 16 + 0)

#define D_AUTH_OUT              (992)
#define D_AUTH_IN               (996)
#define D_AUTH_A                (1000)
#define D_AUTH_B                (1004)

#define D_PEDSTD_BP_B           (768 + 0x1C)
#define D_PEDSTD_BP_A4          (768 + 0x40)
#define D_PEDSTD_BP_A3          (768 + 0x44)
#define D_PEDSTD_BP_A2          (768 + 0x48)
#define D_PEDSTD_BP_A1          (768 + 0x4C)
#define D_PEDSTD_SB             (768 + 0x28)
#define D_PEDSTD_SB_TIME        (768 + 0x2C)
#define D_PEDSTD_PEAKTHRSH      (768 + 0x98)
#define D_PEDSTD_TIML           (768 + 0x2A)
#define D_PEDSTD_TIMH           (768 + 0x2E)
#define D_PEDSTD_PEAK           (768 + 0X94)
#define D_PEDSTD_STEPCTR        (768 + 0x60)
#define D_PEDSTD_STEPCTR2       (58 * 16 + 8)
#define D_PEDSTD_TIMECTR        (964)
#define D_PEDSTD_DECI           (768 + 0xA0)
#define D_PEDSTD_SB2			(60 * 16 + 14)
#define D_STPDET_TIMESTAMP      (28 * 16 + 8)
#define D_PEDSTD_DRIVE_STATE    (58)

#define D_HOST_NO_MOT           (976)
#define D_ACCEL_BIAS            (660)

//#define D_ORIENT_GAP            (76)

//#define D_TILT0_H               (48)
//#define D_TILT0_L               (50)
//#define D_TILT1_H               (52)
//#define D_TILT1_L               (54)
//#define D_TILT2_H               (56)
//#define D_TILT2_L               (58)
//#define D_TILT3_H               (60)
//#define D_TILT3_L               (62)


// Batch mode
#define D_BM_BATCH_CNTR         (27*16+4)
#define D_BM_BATCH_THLD         (27*16+12)
//#define D_BM_WFILL_CNTR         (28*16+0)
#define D_BM_ENABLE             (28*16+6)
#define D_BM_NUMWORD_TOFILL     (28*16+4)

// DMP running counter
#define D_DMP_RUN_CNTR          (24*16)

// Sensor's ODR
#define D_ODR_S0                (45*16+8)
#define D_ODR_S1                (45*16+12)
#define D_ODR_S2                (45*16+10)
#define D_ODR_S3                (45*16+14)
#define D_ODR_S4                (46*16+8)
#define D_ODR_S5                (46*16+12)
#define D_ODR_S6                (46*16+10)
#define D_ODR_S7                (46*16+14)
#define D_ODR_S8                (42*16+8) 
#define D_ODR_S9                (42*16+12)

#define D_ODR_CNTR_S0           (45*16)
#define D_ODR_CNTR_S1           (45*16+4)
#define D_ODR_CNTR_S2           (45*16+2)
#define D_ODR_CNTR_S3           (45*16+6)
#define D_ODR_CNTR_S4           (46*16)
#define D_ODR_CNTR_S5           (46*16+4)
#define D_ODR_CNTR_S6           (46*16+2)
#define D_ODR_CNTR_S7           (46*16+6)
#define D_ODR_CNTR_S8           (42*16)
#define D_ODR_CNTR_S9           (42*16+4)

// DMP Fusion LP-Quat
#define D_FS_LPQ0               (59*16)
#define D_FS_LPQ1               (59*16 + 4)
#define D_FS_LPQ2               (59*16 + 8)
#define D_FS_LPQ3               (59*16 + 12)

// DMP Fusion (Accel+Gyro) Quat
#define D_ACCEL_FB_GAIN			( 6*16)
#define D_GYRO_SF				( 6*16 + 8)
#define D_FS_Q0                 (12*16)
#define D_FS_Q1                 (12*16 + 4)
#define D_FS_Q2                 (12*16 + 8)
#define D_FS_Q3                 (12*16 + 12)

// AK compass support
#define D_CPASS_STATUS_CHK		(22*16 + 8)

// Compass data valid indicator
//#define D_CPASS_VALID           (27*16+2)

// 9-axis accuracy
//#define D_9A_ACCURACY           (27*16)

static const tKeyLabel dmpTConfig[] = {
    {KEY_CFG_OUT_ACCL,              CFG_OUT_ACCL},  
    {KEY_CFG_OUT_GYRO,              CFG_OUT_GYRO}, 
    {KEY_CFG_OUT_3QUAT,             CFG_OUT_3QUAT},   
    {KEY_CFG_OUT_6QUAT,             CFG_OUT_6QUAT},    
	//{KEY_CFG_OUT_9QUAT, 			CFG_OUT_9QUAT},    
    {KEY_CFG_OUT_PQUAT,             CFG_OUT_PQUAT},
	//{KEY_CFG_OUT_PQUAT9, 			CFG_OUT_PQUAT9},
    {KEY_CFG_PED_ENABLE,            CFG_PED_ENABLE},
    {KEY_CFG_FIFO_INT,              CFG_FIFO_INT},
    {KEY_CFG_AUTH,                  CFG_AUTH},
    {KEY_FCFG_1,                    FCFG_1},
    {KEY_FCFG_3,                    FCFG_3},
    {KEY_FCFG_2,                    FCFG_2},
    //{KEY_CFG_DISPLAY_ORIENT_INT,    CFG_DISPLAY_ORIENT_INT},
    {KEY_FCFG_7,                    FCFG_7},
    //{KEY_FCFG_6,                    FCFG_6},
    //{KEY_CFG_EXT_GYRO_BIAS,         CFG_EXT_GYRO_BIAS},
    {KEY_CFG_7,                     CFG_7},
    {KEY_CFG_MOTION_BIAS,           CFG_MOTION_BIAS},
    //{KEY_CFG_GYRO_RAW_DATA,         CFG_GYRO_RAW_DATA},
    {KEY_CFG_PEDSTEP_DET,           CFG_PEDSTEP_DET},
	//{KEY_CFG_WF_GESTURE_INT,		CFG_WF_GESTURE_INT},
    {KEY_D_0_22,                    D_0_22},
    {KEY_D_0_96,                    D_0_96},
    {KEY_D_0_104,                   D_0_104},
    {KEY_D_0_108,                   D_0_108},
    {KEY_D_1_36,                    D_1_36},
    {KEY_D_1_40,                    D_1_40},
    {KEY_D_1_44,                    D_1_44},
    {KEY_D_1_72,                    D_1_72},
    {KEY_D_1_74,                    D_1_74},
    {KEY_D_1_79,                    D_1_79},
    {KEY_D_1_88,                    D_1_88},
    {KEY_D_1_90,                    D_1_90},
    {KEY_D_1_92,                    D_1_92},
    {KEY_D_1_160,                   D_1_160},
    {KEY_D_1_176,                   D_1_176},
    {KEY_D_1_218,                   D_1_218},
    {KEY_D_1_232,                   D_1_232},
    {KEY_D_1_250,                   D_1_250},
    //{KEY_DMP_TAP_GATE,              DMP_TAP_GATE},
    //{KEY_DMP_TAP_THR_Z,             DMP_TAP_THZ},
    //{KEY_DMP_TAP_PREV_JERK_Z,       DMP_TAP_PREV_JERK_Z},
    //{KEY_DMP_TAP_MIN_TAPS,          DMP_TAP_MIN_TAPS},
    //{KEY_DMP_TAPW_MIN,              DMP_TAPW_MIN},
    //{KEY_DMP_TAP_NEXT_TAP_THRES,    DMP_TAP_NEXT_TAP_THRES},
    //{KEY_DMP_TAP_SHAKE_REJECT,      DMP_TAP_SHAKE_REJECT},
    //{KEY_DMP_TAP_SHAKE_COUNT_MAX,   DMP_TAP_SHAKE_COUNT_MAX},
    //{KEY_DMP_TAP_SHAKE_TIMEOUT_MAX, DMP_TAP_SHAKE_TIMEOUT_MAX},
    //{KEY_DMP_TAP_DIRECTION,         DMP_TAP_DIRECTION},
    //{KEY_DMP_TAP_COUNT,             DMP_TAP_COUNT},
    {KEY_DMP_SH_TH_Y,               DMP_SH_TH_Y},
    {KEY_DMP_SH_TH_X,               DMP_SH_TH_X},
    {KEY_DMP_SH_TH_Z,               DMP_SH_TH_Z},
    {KEY_DMP_ORIENT,                DMP_ORIENT},
    {KEY_D_AUTH_OUT,                D_AUTH_OUT},              
    {KEY_D_AUTH_IN,                 D_AUTH_IN},               
    {KEY_D_AUTH_A,                  D_AUTH_A}, 
    {KEY_D_AUTH_B,                  D_AUTH_B}, 
    {KEY_D_ACT0,                    D_ACT0},
    {KEY_D_ACSX,                    D_ACSX},
    {KEY_D_ACSY,                    D_ACSY},
    {KEY_D_ACSZ,                    D_ACSZ},
    {KEY_D_PEDSTD_BP_B,             D_PEDSTD_BP_B},
    {KEY_D_PEDSTD_BP_A4,            D_PEDSTD_BP_A4},
    {KEY_D_PEDSTD_BP_A3,            D_PEDSTD_BP_A3},
    {KEY_D_PEDSTD_BP_A2,            D_PEDSTD_BP_A2},
    {KEY_D_PEDSTD_BP_A1,            D_PEDSTD_BP_A1},
    {KEY_D_PEDSTD_SB,               D_PEDSTD_SB},
    {KEY_D_PEDSTD_SB_TIME,          D_PEDSTD_SB_TIME},
    {KEY_D_PEDSTD_PEAKTHRSH,        D_PEDSTD_PEAKTHRSH},
    {KEY_D_PEDSTD_TIML,             D_PEDSTD_TIML},
    {KEY_D_PEDSTD_TIMH,             D_PEDSTD_TIMH},
    {KEY_D_PEDSTD_PEAK,             D_PEDSTD_PEAK},
    {KEY_D_PEDSTD_STEPCTR,          D_PEDSTD_STEPCTR},
    {KEY_D_PEDSTD_STEPCTR2,         D_PEDSTD_STEPCTR2},
    {KEY_D_PEDSTD_TIMECTR,          D_PEDSTD_TIMECTR},
    {KEY_D_PEDSTD_DECI,             D_PEDSTD_DECI},
	{KEY_D_PEDSTD_SB2,				D_PEDSTD_SB2},
    {KEY_D_PEDSTD_DRIVE_STATE,      D_PEDSTD_DRIVE_STATE},
	{KEY_D_STPDET_TIMESTAMP,		D_STPDET_TIMESTAMP},
    {KEY_D_HOST_NO_MOT,             D_HOST_NO_MOT},
    {KEY_D_ACCEL_BIAS,              D_ACCEL_BIAS},
    //{KEY_D_ORIENT_GAP,              D_ORIENT_GAP},           
    //{KEY_D_TILT0_H,                 D_TILT0_H},              
    //{KEY_D_TILT0_L,                 D_TILT0_L},               
    //{KEY_D_TILT1_H,                 D_TILT1_H},              
    //{KEY_D_TILT1_L,                 D_TILT1_L},               
    //{KEY_D_TILT2_H,                 D_TILT2_H},              
    //{KEY_D_TILT2_L,                 D_TILT2_L},               
    //{KEY_D_TILT3_H,                 D_TILT3_H},               
    //{KEY_D_TILT3_L,                 D_TILT3_L}, 
    {KEY_CFG_EXT_GYRO_BIAS_X,       D_EXT_GYRO_BIAS_X},
    {KEY_CFG_EXT_GYRO_BIAS_Y,       D_EXT_GYRO_BIAS_Y},
    {KEY_CFG_EXT_GYRO_BIAS_Z,       D_EXT_GYRO_BIAS_Z},
    {KEY_CFG_PED_INT,               CFG_PED_INT},
    {KEY_SMD_ENABLE,                D_SMD_ENABLE},
    {KEY_SMD_ACCEL_THLD,            D_SMD_MOT_THLD},
    {KEY_SMD_DELAY_THLD,            D_SMD_DELAY_THLD},
    {KEY_SMD_DELAY2_THLD,           D_SMD_DELAY2_THLD},
    {KEY_SMD_ENABLE_TESTPT1,        SMD_TP1},
    {KEY_SMD_ENABLE_TESTPT2,        SMD_TP2},
    {KEY_SMD_EXE_STATE,             D_SMD_EXE_STATE},
    {KEY_SMD_DELAY_CNTR,            D_SMD_DELAY_CNTR},
    {KEY_BM_ENABLE,                 D_BM_ENABLE},
    {KEY_BM_BATCH_CNTR,             D_BM_BATCH_CNTR},
    {KEY_BM_BATCH_THLD,             D_BM_BATCH_THLD},
    {KEY_BM_NUMWORD_TOFILL,         D_BM_NUMWORD_TOFILL},
    //{KEY_CFG_OUT_CPASS,             CFG_OUT_CPASS},
    //{KEY_CFG_OUT_PRESS,             CFG_OUT_PRESS},
    {KEY_CFG_OUT_STEPDET,           CFG_OUT_STEPDET},
    {KEY_CFG_3QUAT_ODR,             D_ODR_S1},
    {KEY_CFG_6QUAT_ODR,             D_ODR_S2},
    {KEY_CFG_PQUAT6_ODR,            D_ODR_S3},
    {KEY_CFG_ACCL_ODR,              D_ODR_S4},
    {KEY_CFG_GYRO_ODR,              D_ODR_S5},
    {KEY_CFG_CPASS_ODR,             D_ODR_S6},
    {KEY_CFG_PRESS_ODR,             D_ODR_S7},
	{KEY_CFG_9QUAT_ODR, 			D_ODR_S8},
	{KEY_CFG_PQUAT9_ODR, 			D_ODR_S9},
    {KEY_ODR_CNTR_3QUAT,            D_ODR_CNTR_S1},
    {KEY_ODR_CNTR_6QUAT,             D_ODR_CNTR_S2},
    {KEY_ODR_CNTR_PQUAT,            D_ODR_CNTR_S3},
    {KEY_ODR_CNTR_ACCL,             D_ODR_CNTR_S4},
    {KEY_ODR_CNTR_GYRO,             D_ODR_CNTR_S5},
    {KEY_ODR_CNTR_CPASS,            D_ODR_CNTR_S6},
    {KEY_ODR_CNTR_PRESS,            D_ODR_CNTR_S7},
	{KEY_ODR_CNTR_9QUAT,			D_ODR_CNTR_S8},
	{KEY_ODR_CNTR_PQUAT9,			D_ODR_CNTR_S9},
    {KEY_DMP_RUN_CNTR,              D_DMP_RUN_CNTR},
    {KEY_VIB_DET_RATIO,             D_VIB_DET_RATIO},
    {KEY_VIB_MAG_TH,                D_VIB_MAG_TH},
    {KEY_VIB_K,                     D_VIB_K},
    {KEY_VIB_K_1,                   D_VIB_K_1},
    {KEY_ACC_SCALE,                 D_ACC_SCALE},
    {KEY_DMP_LPQ0,	    			D_FS_LPQ0},
	{KEY_DMP_LPQ1,					D_FS_LPQ1},
	{KEY_DMP_LPQ2,					D_FS_LPQ2},
	{KEY_DMP_LPQ3,					D_FS_LPQ3},
	{KEY_CFG_FB_GAIN_GYRO_ON,		CFG_FB_GAIN_GYRO_ON},
	{KEY_DMP_Q0,					D_FS_Q0},
	{KEY_DMP_Q1,					D_FS_Q1},
	{KEY_DMP_Q2,					D_FS_Q2},
	{KEY_DMP_Q3,					D_FS_Q3},	
	{KEY_CPASS_STATUS_CHK,          D_CPASS_STATUS_CHK},
    //{KEY_CPASS_VALID,               D_CPASS_VALID},
	//{KEY_9AXIS_ACCURACY,            D_9A_ACCURACY},
	//{KEY_D_WF_GESTURE_TIME_THRSH,	D_WF_GESTURE_TIME_THRSH},
	//{KEY_D_WF_GESTURE_TILT_ERROR,	D_WF_GESTURE_TILT_ERROR},
	//{KEY_D_WF_GESTURE_TILT_THRSH,	D_WF_GESTURE_TILT_THRSH},
	//{KEY_D_WF_GESTURE_TILT_REJECT_THRSH,	D_WF_GESTURE_TILT_REJECT_THRSH},
#if 1
  	{KEY_TEST_01,                   OUT_ACCL_DAT},
  	{KEY_TEST_02,                   OUT_GYRO_DAT},
  	//{KEY_TEST_03,                   OUT_CPASS_DAT},
  	//{KEY_TEST_04,                   OUT_PRESS_DAT},
  	{KEY_TEST_05,                   OUT_3QUAT_DAT},
  	{KEY_TEST_06,                   OUT_6QUAT_DAT},
  	{KEY_TEST_07,                   OUT_PQUAT_DAT}
#endif
};

#define NUM_LOCAL_KEYS (sizeof(dmpTConfig)/sizeof(dmpTConfig[0]))
#define DMP_CODE_SIZE (3203 + 112) //3315
#define FIFO_SIZE     512
#define RESERVED_SIZE 32
#define MEMORY_SIZE   1024*4
#define DMP_SIZE_AVAILABLE (MEMORY_SIZE - FIFO_SIZE - RESERVED_SIZE - DMP_CODE_SIZE)
#if DMP_SIZE_AVAILABLE < 0
#error "DMP image size is larger than available memory"
#endif

static tKeyLabel keys[NUM_KEYS];

unsigned short inv_dmp_get_address(unsigned short key)
{
    static int isSorted = 0;
    if (!isSorted) {
        int kk;
        for (kk = 0; kk < NUM_KEYS; ++kk) {
            keys[kk].addr = 0xffff;
            keys[kk].key = kk;
        }
        for (kk = 0; kk < NUM_LOCAL_KEYS; ++kk) {
            keys[ dmpTConfig[kk].key ].addr = dmpTConfig[kk].addr;
        }
        isSorted = 1;
    }
	if (key >= NUM_KEYS) {
		return 0xffff;
	}
    
    return keys[key].addr;
}

