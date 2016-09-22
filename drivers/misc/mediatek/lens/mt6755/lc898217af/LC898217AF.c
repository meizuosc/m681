/*
 * MD218A voice coil motor driver
 *
 *
 */
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include "LC898217AF.h"
#include <linux/xlog.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif


#define LENS_I2C_BUSNUM 2

#define AF_DRVNAME "LC898217AF"
#define I2C_SLAVE_ADDRESS        0xE4
#define I2C_REGISTER_ID            0x35
#define PLATFORM_DRIVER_NAME "lens_actuator_LC898217af"
#define AF_DRIVER_CLASS_NAME "actuatordrv_LC898217af"
#define RESCAILING 	1


static struct i2c_board_info kd_lens_dev __initdata = {
	I2C_BOARD_INFO(AF_DRVNAME, I2C_REGISTER_ID)
};

#define AF_DEBUG
#ifdef AF_DEBUG
#define LOG_INF(format, args...) pr_debug(AF_DRVNAME " [%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#endif

static spinlock_t g_AF_SpinLock;

static struct i2c_client *g_pstAF_I2Cclient;

static dev_t g_AF_devno;
static struct cdev *g_pAF_CharDrv;
static struct class *actuator_class;

static int g_s4AF_Opened;
static long g_i4MotorStatus;
static long g_i4Dir;
static unsigned long g_u4AF_INF;
static unsigned long g_u4AF_MACRO = 1023;
static unsigned long g_u4TargetPosition;
static unsigned long g_u4CurrPosition;

static int g_sr = 3;

static int ReadEEPROM(u16 addr, u16 *data)
{
	u8 u8data = 0;
	u8 pu_send_cmd[2] = { (u8) (addr >> 8), (u8) (addr & 0xFF) };
	g_pstAF_I2Cclient->addr = (0xA0) >> 1;
	if (i2c_master_send(g_pstAF_I2Cclient, pu_send_cmd, 2) < 0) {
		LOG_INF("[ReadEEPROM] read I2C send failed!!\n");
		return -1;
	}
	if (i2c_master_recv(g_pstAF_I2Cclient, &u8data, 1) < 0) {
		LOG_INF("ReadI2C failed!!\n");
		return -1;
	}
	*data = u8data;
	LOG_INF("ReadEEPROM2 0x%x, 0x%x\n", addr, *data);

	return 0;
}

static int ReadI2C(u8 length, u8 addr, u16 *data)
{
	u8 pBuff[2];
	u8 u8data = 0;

	g_pstAF_I2Cclient->addr = (I2C_SLAVE_ADDRESS) >> 1;
	if (i2c_master_send(g_pstAF_I2Cclient, &addr, 1) < 0) {
		LOG_INF("[CAMERA SENSOR] read I2C send failed!!\n");
		return -1;
	}

	if (length == 0) {
		if (i2c_master_recv(g_pstAF_I2Cclient, &u8data, 1) < 0) {
			LOG_INF("ReadI2C failed!!\n");
			return -1;
		}
		*data = u8data;
	} else if (length == 1) {
		if (i2c_master_recv(g_pstAF_I2Cclient, pBuff, 2) < 0) {
			LOG_INF("ReadI2C 2 failed!!\n");
			return -1;
		}

		*data = (((u16) pBuff[0]) << 8) + ((u16) pBuff[1]);
	}
	LOG_INF("ReadI2C 0x%x, 0x%x, 0x%x\n", length, addr, *data);

	return 0;
}

static int WriteI2C(u8 length, u8 addr, u16 data)
{
	u8 puSendCmd[2] = { addr, (u8) (data & 0xFF) };
	u8 puSendCmd2[3] = { addr, (u8) ((data >> 8) & 0xFF), (u8) (data & 0xFF) };
	LOG_INF("WriteI2C 0x%x, 0x%x, 0x%x\n", length, addr, data);

	g_pstAF_I2Cclient->addr = (I2C_SLAVE_ADDRESS) >> 1;
	g_pstAF_I2Cclient->ext_flag |= I2C_A_FILTER_MSG;
	if (length == 0) {
		if (i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2) < 0) {
			LOG_INF("WriteI2C failed!!\n");
			return -1;
		}
	} else if (length == 1) {
		if (i2c_master_send(g_pstAF_I2Cclient, puSendCmd2, 3) < 0) {
			LOG_INF("WriteI2C 2 failed!!\n");
			return -1;
		}
	}

	return 0;
}

static inline int getAFInfo(__user stLC898217AF_MotorInfo * pstMotorInfo)
{
	stLC898217AF_MotorInfo stMotorInfo;
	stMotorInfo.u4MacroPosition = g_u4AF_MACRO;
	stMotorInfo.u4InfPosition = g_u4AF_INF;
	stMotorInfo.u4CurrentPosition = g_u4CurrPosition;
	stMotorInfo.bIsSupportSR = 1;

	if (g_i4MotorStatus == 1)
		stMotorInfo.bIsMotorMoving = 1;
	else
		stMotorInfo.bIsMotorMoving = 0;

	if (g_s4AF_Opened >= 1)
		stMotorInfo.bIsMotorOpen = 1;
	else
		stMotorInfo.bIsMotorOpen = 0;

	if (copy_to_user(pstMotorInfo, &stMotorInfo, sizeof(stLC898217AF_MotorInfo)))
		LOG_INF("copy to user failed when getting motor information\n");

	return 0;
}

void LC898217AF_init_drv(void)
{
	unsigned char UcLsiVer;
	unsigned char	UcStatus, UcReadDat ;
	unsigned short	i ;
	unsigned char UcRescailMode = RESCAILING;

	msleep(8);
	
	ReadI2C(0, 0xF0, &UcLsiVer );
	if((UcLsiVer == 0x71) || (UcLsiVer == 0x72))
	{
		for( i = 0 ; i < 3000 ; i++  ) {
			ReadI2C(0, 0xB3, &UcReadDat ) ;
			if(UcRescailMode == RESCAILING){
				if( UcReadDat == 0x00 )             //Rescailing Mode
					break ;
			}
			else
			{
				if( !UcReadDat )
					break ;			
			}
		}

		if( i == 3000 ) {
			UcStatus	= DOWNLOAD_ERROR ;
		}
		
		WriteI2C(0, PINC, 0x02 ) ;
		WriteI2C(0, TESTC, 0x20 ) ;

		if (UcStatus != SUCCESS)
			return FAILURE;

		WriteI2C(0, FUNCRUN2	, 0x02 );  //0xA1 02h, After Calibration Data Writing, Rescailing Run
		LOG_INF("LC898217AF_init_drv success!\n");
		msleep(1) ;//WAIT	1ms
	}
	else
	{
		LOG_INF("LC898217AF_init_drv failed UcLsiVer=%d!\n",UcLsiVer);
	}
	
}

//********************************************************************************
// Function Name 	: Stmv217
// Retun Value		: Stepmove Parameter
// Argment Value	: Stepmove Parameter, Target Position
// Explanation		: Stpmove Function
// History		: First edition 		    2015.07.08 John.Jeong
//********************************************************************************
#define TIME_OUT 1000
#define TIMEOUT_ERROR 10

unsigned char Stmv217( unsigned char DH, unsigned char DL )
{
	unsigned char	UcConvCheck;
	unsigned char	UcSetTime;
	unsigned char	UcCount;	
	
	//AF Operation
	WriteI2C(0, TARGETH	, DH );  //0x84, DH 0x0X
	WriteI2C(0, TARGETL	, DL );  //0x85, DL 0xXX
	msleep(2);//WAIT	5ms
	UcCount = 0;
	do
	{
		ReadI2C(0, SRVSTATE1, &UcConvCheck); 	//0xB0
		UcConvCheck = UcConvCheck & 0x80;  	//bit7 == 0 or not
		ReadI2C(0, TGTCNVTIM, &UcSetTime); 	//0xB2 Settle Time check Settle Time Calculate =
							// ( B2h Read Value x 0.171mS ) - 2.731mS Condition:
							//EEPROM 55h(THD1) = 0x20
							//82h:bit2-0( LOCCNVCNT ) = 100b
		msleep(2);
		UcCount++;
		if(UcCount == TIME_OUT)	{
			LOG_INF("Stmv217 fail because timeout UcConvCheck=%d,UcCount=%d!\n",UcConvCheck,UcCount);
			return TIMEOUT_ERROR;
		}
		LOG_INF("Stmv217 UcConvCheck=%d,UcCount=%d!\n",UcConvCheck,UcCount);
	}while(UcConvCheck); 	
	
	return SUCCESS;
}


static int SetVCMPos(u16 UsPosition)
{
	unsigned char UcPosH;
    unsigned char UcPosL;
    unsigned char UcAFSts;
    //unsigned char UcMaxPosition = 1023;  

	LOG_INF("SetVCMPos UsPosition=%d\n",UsPosition);
    //UsPosition = 1023 - UsPosition; //Only SI1330C, SI1331C

    UcPosH = (unsigned char)(UsPosition >> 8);
    UcPosL = (unsigned char)(UsPosition & 0x00FF); 

	UcAFSts = Stmv217( UcPosH, UcPosL );   
    if (UcAFSts != SUCCESS)
	return FAILURE;	

    return  UcAFSts; 
}

static inline int moveAF(unsigned long a_u4Position)
{
	int ret = 0;
	if ((a_u4Position > g_u4AF_MACRO) || (a_u4Position < g_u4AF_INF)) {
		LOG_INF("out of range\n");
		return -EINVAL;
	}

	if (g_s4AF_Opened == 1) {
		u16 InitPos;
		LC898217AF_init_drv();

		ret = ReadI2C(1, 0x3C, &InitPos);

		if (ret == 0) {
			LOG_INF("Init Pos %6d\n", InitPos);

			spin_lock(&g_AF_SpinLock);
			g_u4CurrPosition = (unsigned long)InitPos;
			spin_unlock(&g_AF_SpinLock);

		} else {
			spin_lock(&g_AF_SpinLock);
			g_u4CurrPosition = 0;
			spin_unlock(&g_AF_SpinLock);
		}

		spin_lock(&g_AF_SpinLock);
		g_s4AF_Opened = 2;
		spin_unlock(&g_AF_SpinLock);
		return 0;
	}

	if (g_u4CurrPosition < a_u4Position) {
		spin_lock(&g_AF_SpinLock);
		g_i4Dir = 1;
		spin_unlock(&g_AF_SpinLock);
	} else if (g_u4CurrPosition > a_u4Position) {
		spin_lock(&g_AF_SpinLock);
		g_i4Dir = -1;
		spin_unlock(&g_AF_SpinLock);
	} else {
		return 0;
	}

	spin_lock(&g_AF_SpinLock);
	g_u4TargetPosition = a_u4Position;
	spin_unlock(&g_AF_SpinLock);

	LOG_INF("move [curr] %lu [target] %lu\n", g_u4CurrPosition, g_u4TargetPosition);

	spin_lock(&g_AF_SpinLock);
	g_sr = 3;
	g_i4MotorStatus = 0;
	spin_unlock(&g_AF_SpinLock);

	if (SetVCMPos((u16) g_u4TargetPosition) == 0) {
		spin_lock(&g_AF_SpinLock);
		g_u4CurrPosition = (unsigned long)g_u4TargetPosition;
		spin_unlock(&g_AF_SpinLock);
	} else {
		LOG_INF("set I2C failed when moving the motor\n");

		spin_lock(&g_AF_SpinLock);
		g_i4MotorStatus = -1;
		spin_unlock(&g_AF_SpinLock);
	}

	return 0;
}

static inline int setAFInf(unsigned long a_u4Position)
{
	spin_lock(&g_AF_SpinLock);
	g_u4AF_INF = a_u4Position;
	spin_unlock(&g_AF_SpinLock);
	return 0;
}

static inline int setAFMacro(unsigned long a_u4Position)
{
	spin_lock(&g_AF_SpinLock);
	g_u4AF_MACRO = a_u4Position;
	spin_unlock(&g_AF_SpinLock);
	return 0;
}

/* ////////////////////////////////////////////////////////////// */
static long AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command, unsigned long a_u4Param)
{
	long i4RetValue = 0;

	switch (a_u4Command) {
	case LC898217AFIOC_G_MOTORINFO:
		i4RetValue = getAFInfo((__user stLC898217AF_MotorInfo *) (a_u4Param));
		break;
#ifdef LensdrvCM3
	case LC898217AFIOC_G_MOTORMETAINFO:
		i4RetValue = getAFMETA((__user stLC898217AF_MotorMETAInfo*) (a_u4Param));
		break;
#endif
	case LC898217AFIOC_T_MOVETO:
		i4RetValue = moveAF(a_u4Param);
		break;

	case LC898217AFIOC_T_SETINFPOS:
		i4RetValue = setAFInf(a_u4Param);
		break;

	case LC898217AFIOC_T_SETMACROPOS:
		i4RetValue = setAFMacro(a_u4Param);
		break;

	default:
		LOG_INF("No CMD\n");
		i4RetValue = -EPERM;
		break;
	}

	return i4RetValue;
}

/* Main jobs: */
/* 1.check for device-specified errors, device not ready. */
/* 2.Initialize the device if it is opened for the first time. */
/* 3.Update f_op pointer. */
/* 4.Fill data structures into private_data */
/* CAM_RESET */
static int AF_Open(struct inode *a_pstInode, struct file *a_pstFile)
{


	LOG_INF("Start\n");
	if (g_s4AF_Opened) {
		LOG_INF("The device is opened\n");
		return -EBUSY;
	}

	spin_lock(&g_AF_SpinLock);
	g_s4AF_Opened = 1;
	spin_unlock(&g_AF_SpinLock);
	LOG_INF("End\n");

	return 0;
}

/* Main jobs: */
/* 1.Deallocate anything that "open" allocated in private_data. */
/* 2.Shut down the device on last close. */
/* 3.Only called once on last time. */
/* Q1 : Try release multiple times. */
static int AF_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
	LOG_INF("Start\n");

	if (g_s4AF_Opened == 2)
		g_sr = 5;

	if (g_s4AF_Opened) {
		LOG_INF("Free\n");

		spin_lock(&g_AF_SpinLock);
		g_s4AF_Opened = 0;
		spin_unlock(&g_AF_SpinLock);
	}

	LOG_INF("End\n");

	return 0;
}

static const struct file_operations g_stAF_fops = {
	.owner = THIS_MODULE,
	.open = AF_Open,
	.release = AF_Release,
	.unlocked_ioctl = AF_Ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = AF_Ioctl,
#endif
};

static inline int Register_AF_CharDrv(void)
{
	struct device *vcm_device = NULL;

	LOG_INF("Start\n");

	/* Allocate char driver no. */
	if (alloc_chrdev_region(&g_AF_devno, 0, 1, AF_DRVNAME)) {
		LOG_INF("Allocate device no failed\n");

		return -EAGAIN;
	}
	/* Allocate driver */
	g_pAF_CharDrv = cdev_alloc();

	if (NULL == g_pAF_CharDrv) {
		unregister_chrdev_region(g_AF_devno, 1);

		LOG_INF("Allocate mem for kobject failed\n");

		return -ENOMEM;
	}
	/* Attatch file operation. */
	cdev_init(g_pAF_CharDrv, &g_stAF_fops);

	g_pAF_CharDrv->owner = THIS_MODULE;

	/* Add to system */
	if (cdev_add(g_pAF_CharDrv, g_AF_devno, 1)) {
		LOG_INF("Attatch file operation failed\n");

		unregister_chrdev_region(g_AF_devno, 1);

		return -EAGAIN;
	}

	actuator_class = class_create(THIS_MODULE, AF_DRIVER_CLASS_NAME);
	if (IS_ERR(actuator_class)) {
		int ret = PTR_ERR(actuator_class);
		LOG_INF("Unable to create class, err = %d\n", ret);
		return ret;
	}

	vcm_device = device_create(actuator_class, NULL, g_AF_devno, NULL, AF_DRVNAME);

	if (NULL == vcm_device)
		return -EIO;

	LOG_INF("End\n");
	return 0;
}

static inline void Unregister_AF_CharDrv(void)
{
	LOG_INF("Start\n");

	/* Release char driver */
	cdev_del(g_pAF_CharDrv);

	unregister_chrdev_region(g_AF_devno, 1);

	device_destroy(actuator_class, g_AF_devno);

	class_destroy(actuator_class);

	LOG_INF("End\n");
}

/* //////////////////////////////////////////////////////////////////// */

static int AF_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int AF_i2c_remove(struct i2c_client *client);
static const struct i2c_device_id AF_i2c_id[] = { {AF_DRVNAME, 0}, {} };

static struct i2c_driver AF_i2c_driver = {
	.probe = AF_i2c_probe,
	.remove = AF_i2c_remove,
	.driver.name = AF_DRVNAME,
	.id_table = AF_i2c_id,
};

#if 0
static int AF_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info)
{
	strcpy(info->type, AF_DRVNAME);
	return 0;
}
#endif
static int AF_i2c_remove(struct i2c_client *client)
{
	return 0;
}

/* Kirby: add new-style driver {*/
static int AF_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int i4RetValue = 0;

	LOG_INF("Start\n");

	/* Kirby: add new-style driver { */
	g_pstAF_I2Cclient = client;

	g_pstAF_I2Cclient->addr = I2C_SLAVE_ADDRESS;

	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;

	/* Register char driver */
	i4RetValue = Register_AF_CharDrv();

	if (i4RetValue) {

		LOG_INF(" register char device failed!\n");

		return i4RetValue;
	}

	spin_lock_init(&g_AF_SpinLock);

	LOG_INF("Attached!!\n");

	return 0;
}

static int AF_probe(struct platform_device *pdev)
{
	return i2c_add_driver(&AF_i2c_driver);
}

static int AF_remove(struct platform_device *pdev)
{
	i2c_del_driver(&AF_i2c_driver);
	return 0;
}

static int AF_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	return 0;
}

static int AF_resume(struct platform_device *pdev)
{
	return 0;
}

/* platform structure */
static struct platform_driver g_stAF_Driver = {
	.probe = AF_probe,
	.remove = AF_remove,
	.suspend = AF_suspend,
	.resume = AF_resume,
	.driver = {
		   .name = PLATFORM_DRIVER_NAME,
		   .owner = THIS_MODULE,
		   }
};

static struct platform_device g_stAF_device = {
	.name = PLATFORM_DRIVER_NAME,
	.id = 0,
	.dev = {}
};

static int __init LC898217AF_i2C_init(void)
{
	i2c_register_board_info(LENS_I2C_BUSNUM, &kd_lens_dev, 1);

	if (platform_device_register(&g_stAF_device)) {
		LOG_INF("failed to register AF driver\n");
		return -ENODEV;
	}

	if (platform_driver_register(&g_stAF_Driver)) {
		LOG_INF("Failed to register AF driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit LC898217AF_i2C_exit(void)
{
	platform_driver_unregister(&g_stAF_Driver);
}
module_init(LC898217AF_i2C_init);
module_exit(LC898217AF_i2C_exit);

MODULE_DESCRIPTION("LC898217AF lens module driver");
MODULE_AUTHOR("KY Chen <ky.chen@Mediatek.com>");
MODULE_LICENSE("GPL");
