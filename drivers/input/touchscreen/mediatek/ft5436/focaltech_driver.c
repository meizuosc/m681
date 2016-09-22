/*
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * * VERSION        DATE            AUTHOR          Note
 *    1.0         2013-7-16         Focaltech        initial  based on MTK platform
 *
 */

#include "../tpd.h"
#include "tpd_custom_fts.h"
#include "mtk_kpd.h"
#include "cust_gpio_usage.h"

#ifdef FTS_CTL_IIC
#include "focaltech_ctl.h"
#endif

#ifdef TPD_SYSFS_DEBUG
#include "focaltech_ex_fun.h"
#endif

#define TYPE_B_PROTOCOL
#ifdef TYPE_B_PROTOCOL
#include <linux/input/mt.h>
#endif

//#include <linux/input/mz_gesture_ts.h>

#undef TPD_DEVICE
#define TPD_DEVICE "tpd_FOCALTECH"

#include <mach/battery_common.h>
//#include <linux/meizu-sys.h>
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/dma-mapping.h>

/*result*/
#define TPD_OK                          0

/*coordinate register define begin*/
#define DEVICE_MODE                     0x00
#define GEST_ID                         0x01
#define TD_STATUS                       0x02
//point1 info from 0x03~0x08
//point2 info from 0x09~0x0E
//point3 info from 0x0F~0x14
//point4 info from 0x15~0x1A
//point5 info from 0x1B~0x20
/*coordinate register define end*/

/*max detect count in fuction probe*/
#define TPD_RESET_ISSUE_WORKAROUND
#define TPD_MAX_RESET_COUNT         2
//#define GPIO_CTP_EINT_PIN     EINT_CTP_PIN
//#define GPIO_CTP_EINT_PIN_M_EINT  EINT_CTP_PIN_M_EINT

struct Upgrade_Info fts_updateinfo_curr =
{
    /*id, chip, point, calibration, aa time, 55 time, id1, id2, read id time, earse time*/
    0x54, "FT5x46", TPD_MAX_POINTS_5, AUTO_CLB_NEED, 10, 10,
    #ifdef TPD_AUTO_DOWNLOAD
    0x54,
    0x2b,
    #endif
    0x54, 0x2c, 10, 2500
};

#ifdef FTS_GESTURE
#define GESTURE_LEFT                0x20
#define GESTURE_RIGHT               0x21
#define GESTURE_UP                  0x22
#define GESTURE_DOWN                0x23
#define GESTURE_DOUBLECLICK         0x24
#define GESTURE_O                   0x30
#define GESTURE_W                   0x31
#define GESTURE_M                   0x32
#define GESTURE_E                   0x33
#define GESTURE_C                   0x34
#define GESTURE_S                   0x46
#define GESTURE_V                   0x54
#define GESTURE_Z                   0x65

//add by linan gesture
#define GESTURE_ERROR       0x00
/*double tap */
#define DOUBLE_TAP          0xA0  
/*swipe  */
#define SWIPE_X_LEFT        0xB0 /*б┴ио?? */
#define SWIPE_X_RIGHT       0xB1 /*иои░?? */
#define SWIPE_Y_UP          0xB2 /*иж??? */
#define SWIPE_Y_DOWN        0xB3 /*???? */

/*Unicode */
#define UNICODE_E           0xC0
#define UNICODE_C           0xC1
#define UNICODE_W           0xC2
#define UNICODE_M           0xC3
#define UNICODE_O           0xC4
#define UNICODE_S           0xC5
#define UNICODE_V_UP        0xC6
#define UNICODE_V_DOWN      0xC7
#define UNICODE_V_L         0xC8
#define UNICODE_V_R         0xC9
#define UNICODE_Z           0xCA
#define KEY_GESTURE    195

#define KEY_GESTURE_DOWN 					KEY_DOWN
#define KEY_GESTURE_NONE            0xFF
#define FTS_GESTURE_POINTS              255
#define FTS_GESTURE_POINTS_ONETIME      62
#define FTS_GESTURE_POINTS_HEADER       8
#define FTS_GESTURE_OUTPUT_ADRESS       0xD3
#define FTS_GESTURE_OUTPUT_UNIT_LENGTH  4
#define KEY_TPGESTURE_DOUBLE            0x1f9
#define FTS_GESTURE_RETRY_CNT           5

#define GESTURE_ENABLE                  0x01
#define GESTURE_DISABLE                 0x00

#define MZ_GESTURE_ENABLE                  1
#define MZ_GESTURE_DISABLE                 0
static u8 ft5x46_gesture_state = MZ_GESTURE_ENABLE;
static u8 ft5x46_gesture_state_pre = MZ_GESTURE_ENABLE;

extern u8 cur_fw_version;
struct GestureCtrl
{
    unsigned int  msg_switch;
    unsigned int  state;
    unsigned short point_num;
    unsigned short x[FTS_GESTURE_POINTS];
    unsigned short y[FTS_GESTURE_POINTS];
};

static struct GestureCtrl G_stGestureCtrl = {0};



#endif

#ifdef FTS_GLOVE
#define FTS_GLOVE_RETRY_CNT    5

#define GLOVE_ENABLE           0x01
#define GLOVE_DISABLE          0x00

struct GloveCtrl
{
    unsigned int  msg_switch;
    unsigned int  state;
};

static struct GloveCtrl G_stGloveCtrl = {0};

#endif

#ifdef FTS_MCAP_TEST
struct i2c_client *g_focalclient = NULL;
#endif

/* ROI */
#ifdef FTS_ROI
struct RoiCtrl
{
    unsigned int  msg_switch;
    unsigned int  pre_finger_status;
    unsigned char roi_data[ROI_DATA_SEND_LENGTH];
};

static struct RoiCtrl G_stRoi = {0};
#endif


extern void upmu_set_rg_pwrkey_int_sel(kal_uint32 val);

extern struct tpd_device *tpd;
extern char tp_info[20];

struct i2c_client *i2c_client = NULL;
struct task_struct *thread = NULL;

static DECLARE_WAIT_QUEUE_HEAD(waiter);
static DEFINE_MUTEX(i2c_access);


static void tpd_eint_interrupt_handler(void);
// Start:Here maybe need port to different platform,like MT6575/MT6577
extern void mt_eint_unmask(unsigned int line);
extern void mt_eint_mask(unsigned int line);
//extern void mt65xx_eint_set_hw_debounce(unsigned int eint_num, unsigned int ms);
//extern unsigned int mt65xx_eint_set_sens(unsigned int eint_num, unsigned int sens);
//extern void mt65xx_eint_registration(unsigned int eint_num, unsigned int is_deb_en, unsigned int pol, void (EINT_FUNC_PTR)(void), unsigned int is_auto_umask);
// End:Here maybe need port to different platform,like MT6575/MT6577

static int tpd_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tpd_detect (struct i2c_client *client, struct i2c_board_info *info);
static int tpd_remove(struct i2c_client *client);
static int touch_event_handler(void *unused);

#ifdef FTS_GESTURE
unsigned int inline tpd_gesture_switch_open(void);
unsigned int inline tpd_gesture_switch_close(void);
#endif

#ifdef FTS_GLOVE
unsigned int tpd_glove_switch_open(void);
unsigned int  tpd_glove_switch_close(void);
#endif

#ifdef FTS_ROI
void tpd_roi_switch_open(void);
void tpd_roi_switch_close(void);
unsigned int tpd_roi_enable(void);
unsigned int tpd_roi_disable(void);
unsigned int tpd_roi_is_enable(void);
unsigned char* tpd_roi_get_rawdata(void);
void tpd_roi_clear_rawdata(void);
#endif

static int tpd_flag                     = 0;
static int tpd_halt                     = 0;
static int point_num                    = 0;
static int p_point_num                  = 0;

#ifdef FTS_ESD_CHECKER
static struct workqueue_struct *fts_esd_wq;
static struct delayed_work fts_esd_checker;
int apk_debug_flag = 0;
#endif

//extern int tpd_mstar_status ;  // compatible mstar and ft6306 chenzhecong

#ifdef TPD_HAVE_BUTTON
static int tpd_keys_local[TPD_KEY_COUNT] = TPD_KEYS;
static int tpd_keys_dim_local[TPD_KEY_COUNT][4] = TPD_KEYS_DIM;
#endif
#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
static int tpd_wb_start_local[TPD_WARP_CNT] = TPD_WARP_START;
static int tpd_wb_end_local[TPD_WARP_CNT]   = TPD_WARP_END;
#endif
#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
static int tpd_calmat_local[8]     = TPD_CALIBRATION_MATRIX;
static int tpd_def_calmat_local[8] = TPD_CALIBRATION_MATRIX;
#endif

#define VELOCITY_CUSTOM_FT5206
#ifdef VELOCITY_CUSTOM_FT5206
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>

// for magnify velocity********************************************

#ifndef TPD_VELOCITY_CUSTOM_X
#define TPD_VELOCITY_CUSTOM_X           10
#endif
#ifndef TPD_VELOCITY_CUSTOM_Y
#define TPD_VELOCITY_CUSTOM_Y           10
#endif

#define TOUCH_IOC_MAGIC                 'A'

#define TPD_GET_VELOCITY_CUSTOM_X _IO(TOUCH_IOC_MAGIC,0)
#define TPD_GET_VELOCITY_CUSTOM_Y _IO(TOUCH_IOC_MAGIC,1)


static int g_v_magnify_x = TPD_VELOCITY_CUSTOM_X;
static int g_v_magnify_y = TPD_VELOCITY_CUSTOM_Y;

static int tpd_misc_open(struct inode *inode, struct file *file)
{
    /*
    file->private_data = adxl345_i2c_client;

    if(file->private_data == NULL)
    {
        FTS_DBG("tpd: null pointer!!\n");
        return -EINVAL;
    }
    */
    return nonseekable_open(inode, file);
}
/*----------------------------------------------------------------------------*/
static int tpd_misc_release(struct inode *inode, struct file *file)
{
    //file->private_data = NULL;
    return 0;
}
/*----------------------------------------------------------------------------*/
//static int adxl345_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
//       unsigned long arg)
static long tpd_unlocked_ioctl(struct file *file, unsigned int cmd,
                               unsigned long arg)
{
    //struct i2c_client *client = (struct i2c_client*)file->private_data;
    //struct adxl345_i2c_data *obj = (struct adxl345_i2c_data*)i2c_get_clientdata(client);
    //char strbuf[256];
    void __user *data;

    long err = 0;

    #ifdef FTS_GESTURE
    unsigned int value = 0;
    #endif

    if (_IOC_DIR(cmd) & _IOC_READ)
    {
        err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
    }
    else if (_IOC_DIR(cmd) & _IOC_WRITE)
    {
        err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
    }

    if (err)
    {
        FTS_DBG("tpd: access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
        return -EFAULT;
    }

    switch (cmd)
    {
        case TPD_GET_VELOCITY_CUSTOM_X:
            data = (void __user *) arg;
            if (data == NULL)
            {
                err = -EINVAL;
                break;
            }

            if (copy_to_user(data, &g_v_magnify_x, sizeof(g_v_magnify_x)))
            {
                err = -EFAULT;
                break;
            }
            break;

        case TPD_GET_VELOCITY_CUSTOM_Y:
            data = (void __user *) arg;
            if (data == NULL)
            {
                err = -EINVAL;
                break;
            }

            if (copy_to_user(data, &g_v_magnify_y, sizeof(g_v_magnify_y)))
            {
                err = -EFAULT;
                break;
            }
            break;

            #ifdef FTS_GESTURE
        case GESTURE_SWITCH_OPEN:
            data = (void __user *) arg;
            if (data == NULL)
            {
                err = -EINVAL;
                break;
            }
            value = tpd_gesture_switch_open();
            if (copy_to_user(data, &value, sizeof(value)))
            {
                err = -EFAULT;
                break;
            }
            break;

        case GESTURE_SWITCH_CLOSE:
            data = (void __user *) arg;
            if (data == NULL)
            {
                err = -EINVAL;
                break;
            }
            value = tpd_gesture_switch_close();
            if (copy_to_user(data, &value, sizeof(value)))
            {
                err = -EFAULT;
                break;
            }
            break;
            #endif

            #ifdef FTS_GLOVE
        case GLOVE_SWITCH_OPEN:
            data = (void __user *) arg;
            if (data == NULL)
            {
                err = -EINVAL;
                break;
            }
            value = tpd_glove_switch_open();
            if (copy_to_user(data, &value, sizeof(value)))
            {
                err = -EFAULT;
                break;
            }
            break;
        case GLOVE_SWITCH_CLOSE:
            data = (void __user *) arg;
            if (data == NULL)
            {
                err = -EINVAL;
                break;
            }
            value = tpd_glove_switch_close();
            if (copy_to_user(data, &value, sizeof(value)))
            {
                err = -EFAULT;
                break;
            }
            break;
            #endif


            #ifdef FTS_ROI
        case ROI_SWITCH_OPEN:
            data = (void __user *) arg;
            if (data == NULL)
            {
                err = -EINVAL;
                break;
            }
            value = tpd_roi_enable();
            if (copy_to_user(data, &value, sizeof(value)))
            {
                err = -EFAULT;
                break;
            }
            break;
        case ROI_SWITCH_CLOSE:
            data = (void __user *) arg;
            if (data == NULL)
            {
                err = -EINVAL;
                break;
            }
            value = tpd_roi_disable();
            if (copy_to_user(data, &value, sizeof(value)))
            {
                err = -EFAULT;
                break;
            }
            break;
            #endif

        default:
            FTS_DBG("tpd: unknown IOCTL: 0x%08x\n", cmd);
            err = -ENOIOCTLCMD;
            break;
    }

    return err;
}


static struct file_operations tpd_fops =
{
    //  .owner = THIS_MODULE,
    .open = tpd_misc_open,
    .release = tpd_misc_release,
    .unlocked_ioctl = tpd_unlocked_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice tpd_misc_device =
{
    .minor = MISC_DYNAMIC_MINOR,
    .name = TPD_NAME,
    .fops = &tpd_fops,
};

//**********************************************
#endif

struct touch_info
{
    int y[FTS_MAX_TOUCH];
    int x[FTS_MAX_TOUCH];
    int p[FTS_MAX_TOUCH];
    int id[FTS_MAX_TOUCH];
    int count;
};

static const struct i2c_device_id ft5206_tpd_id[] = {{TPD_NAME, 0}, {}};
//unsigned short force[] = {0,0x70,I2C_CLIENT_END,I2C_CLIENT_END};
//static const unsigned short * const forces[] = { force, NULL };
//static struct i2c_client_address_data addr_data = { .forces = forces, };
static struct i2c_board_info __initdata ft5206_i2c_tpd = { I2C_BOARD_INFO(TPD_NAME, (0x70 >> 1))};

static struct i2c_driver tpd_i2c_driver =
{
    .driver = {
        .name   = TPD_NAME,
        //  .owner  = THIS_MODULE,
    },
    .probe      = tpd_probe,
    .remove     = tpd_remove,
    .id_table   = ft5206_tpd_id,
    .detect     = tpd_detect,
    //  .shutdown   = tpd_shutdown,
    //  .address_data = &addr_data,
};

/* ROI */
#ifdef FTS_ROI
unsigned char* tpd_roi_get_rawdata(void)
{
    return &G_stRoi.roi_data;
}

void tpd_roi_clear_rawdata(void)
{
    memset(&G_stRoi.roi_data, 0, ROI_DATA_SEND_LENGTH*sizeof(unsigned char));
}

unsigned int tpd_roi_switch_state(void)
{
    return G_stRoi.msg_switch;
}

void tpd_roi_switch_open(void)
{
    G_stRoi.msg_switch = ROI_ENABLE;
}

void tpd_roi_switch_close(void)
{
    G_stRoi.msg_switch = ROI_DISABLE;
}

unsigned int tpd_roi_enable(void)
{
    char data = 0x00;

    fts_write_reg(i2c_client, FTS_ROI_SWITCH_REG, ROI_ENABLE);

    msleep(5);

    fts_read_reg(i2c_client, FTS_ROI_SWITCH_REG, &data);

    return data;
}

unsigned int tpd_roi_disable(void)
{
    char data = 0x01;

    fts_write_reg(i2c_client, FTS_ROI_SWITCH_REG, ROI_DISABLE);

    msleep(5);

    fts_read_reg(i2c_client, FTS_ROI_SWITCH_REG, &data);

    return data;
}

unsigned int tpd_roi_is_enable(void)
{
    u8 reg_value = 0;

    if (fts_read_reg(i2c_client, FTS_ROI_SWITCH_REG, &reg_value) < 0)
    {
        FTS_DBG("tpd_roi_is_enable read iic error...\n");
        reg_value = 0;
    }

    FTS_DBG("tpd_roi_is_enable state=%d...\n", reg_value);

    return reg_value;
}
#endif


#ifdef FTS_GLOVE
unsigned int tpd_glove_switch_is_opened(void)
{
    return G_stGloveCtrl.msg_switch;
}

unsigned int tpd_glove_switch_open(void)
{
    G_stGloveCtrl.msg_switch = 1;
    return G_stGloveCtrl.msg_switch ;
}

unsigned int  tpd_glove_switch_close(void)
{
    G_stGloveCtrl.msg_switch  = 0;
    return G_stGloveCtrl.msg_switch ;
}

int tpd_glove_is_enable(void)
{
    int ret = 0;
    u8 reg_value = 0;

    fts_read_reg(i2c_client, 0xc0, &reg_value);
    FTS_DBG("tpd_glove_is_enable glove state=%d...\n", reg_value);

    ret = reg_value;
    G_stGloveCtrl.state = reg_value;

    return reg_value;
}

int tpd_glove_enable(void)
{
    char data = 0x00;

    fts_write_reg(i2c_client, 0xc0, GLOVE_ENABLE);

    msleep(5);

    fts_read_reg(i2c_client, 0xc0, &data);

    return data;
}

int tpd_glove_disable(void)
{
    char data = 0x01;

    fts_write_reg(i2c_client, 0xc0, GLOVE_DISABLE);

    msleep(5);

    fts_read_reg(i2c_client, 0xc0, &data);

    return data;
}

#endif

DEFINE_MUTEX(ft_suspend_lock);

// track which id is down
static unsigned long ft5x46_finger_status = 0;

static  void tpd_down(int x, int y, int p, int id)
{
    #ifdef TYPE_B_PROTOCOL
    input_mt_slot(tpd->dev, id);
    input_mt_report_slot_state(tpd->dev, MT_TOOL_FINGER, 1);
    #endif
    input_report_abs(tpd->dev, ABS_PRESSURE, p);
    //input_report_key(tpd->dev, BTN_TOUCH, 1); modify by xunanbin 20151103
    input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 20);
    input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
    input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
    set_bit(id, &ft5x46_finger_status);
    FTS_DBG("D[%4d %4d %4d %4d] ", x, y, p, id);

    #ifndef TYPE_B_PROTOCOL
    /* track id Start 0 */
    input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, p);
    input_mt_sync(tpd->dev);
    #endif

    #ifndef MT6572
    if (FACTORY_BOOT == get_boot_mode() || RECOVERY_BOOT == get_boot_mode())
    #endif
    {
        tpd_button(x, y, 1);
    }

    TPD_EM_PRINT(x, y, x, y, p - 1, 1);
}

static  void tpd_up(int x, int y, int p, int id)
{
    //input_report_key(tpd->dev, BTN_TOUCH, 0); modify by xunanbin 20151103
    #ifdef TYPE_B_PROTOCOL
    input_mt_slot(tpd->dev, id);
    input_mt_report_slot_state(tpd->dev, MT_TOOL_FINGER, 0);
    #endif
    clear_bit(id, &ft5x46_finger_status);
    FTS_DBG("U[%4d %4d %4d %4d] ", x, y, p, id);
    #ifndef TYPE_B_PROTOCOL
    input_mt_sync(tpd->dev);
    #endif
    TPD_EM_PRINT(x, y, x, y, 0, 0);

    #ifndef MT6572
    if (FACTORY_BOOT == get_boot_mode() || RECOVERY_BOOT == get_boot_mode())
    #endif
    {
        tpd_button(x, y, 0);
    }
}

static int tpd_touchinfo(struct touch_info *cinfo, struct touch_info *pinfo)
{
    int i = 0;
    int ret = TRUE;
    char data[128] = {0};
    u16 high_byte, low_byte, reg;
    u8 report_rate = 0;
    
#ifdef FTS_ROI
    unsigned int temp_finger_status = 0;
#endif

    memset(cinfo, 0, sizeof(struct touch_info));

    p_point_num = point_num;
    if (tpd_halt)
    {
        TPD_DMESG( "tpd_touchinfo tpd_halt = 1, return FALSE..\n");
        ret = FALSE;
        goto exit;
    }
    
    mutex_lock(&i2c_access);

    reg = 0x00;
    ret = fts_i2c_Read(i2c_client, &reg, 1, data, 64);
    //mutex_unlock(&i2c_access);

    if (ret < 0)
    {
        FTS_DBG("tpd_touchinfo i2c read error, ret = %d\n", ret);
        ret = FALSE;
        goto exit;
    }

    /*get the number of the touch points*/
    point_num = data[2] & 0x0f;

    FTS_DBG("tpd_touchinfo raw point_num =%d\n", point_num);

    if (point_num < 0)
    {
        point_num = 0;
    }

    if (point_num > FTS_MAX_TOUCH)
    {
        point_num = FTS_MAX_TOUCH;
    }

    FTS_DBG("tpd_touchinfo point_num =%d\n", point_num);

    for (i = 0; i < FTS_MAX_TOUCH; i++)
    {
        cinfo->p[i] = data[3 + 6 * i] >> 6; //event flag
        cinfo->id[i] = data[3 + 6 * i + 2] >> 4; //touch id
        // max id value is 64
        if (cinfo->id[i] > 0x40)
        {
            ret = FALSE;
            goto exit;

            //return 0;    //return false
        }
        /*get the X coordinate, 2 bytes*/
        high_byte = data[3 + 6 * i];
        high_byte <<= 8;
        high_byte &= 0x0f00;
        low_byte = data[3 + 6 * i + 1];
        cinfo->x[i] = high_byte | low_byte;

        //cinfo->x[i] =  cinfo->x[i] * 480 >> 11; //calibra

        /*get the Y coordinate, 2 bytes*/

        high_byte = data[3 + 6 * i + 2];
        high_byte <<= 8;
        high_byte &= 0x0f00;
        low_byte = data[3 + 6 * i + 3];
        cinfo->y[i] = high_byte | low_byte;
        cinfo->count++;

        //cinfo->y[i]=  cinfo->y[i] * 800 >> 11;
    }

    
#ifdef FTS_ROI
    do
    {
        temp_finger_status = point_num;

        FTS_DBG("pre_finger_status=%d, temp_finger_status=%d\n", G_stRoi.pre_finger_status, temp_finger_status);
    
        if ((temp_finger_status != G_stRoi.pre_finger_status && ((temp_finger_status & G_stRoi.pre_finger_status) != temp_finger_status)))
        {
            if (ROI_ENABLE == tpd_roi_switch_state())
            {
                unsigned char buf[1];
                unsigned char *p_roi_data;

                buf[0] = FTS_ROI_DATA_ADDR;
                p_roi_data = tpd_roi_get_rawdata();

                if (p_roi_data == NULL)
                {
                    pr_err( "%s p_roi_data NULL failed...\n", __func__);
                    break;
                }
                
                ret = fts_i2c_Read(i2c_client, buf, 1, p_roi_data, ROI_DATA_READ_LENGTH);
                
                if (ret < 0)
                {
                    pr_err( "%s read roi data failed...\n", __func__);
                    ret = TRUE;
                    break;
                }
            }
        }
    
        G_stRoi.pre_finger_status = temp_finger_status;
    
    } while(0);
#endif

exit:
    mutex_unlock(&i2c_access);
    
    FTS_DBG("tpd_touchinfo cinfo->x[0] = %d, cinfo->y[0] = %d, cinfo->p[0] = %d, point_num = %d\n", cinfo->x[0], cinfo->y[0], cinfo->p[0], point_num);

    return ret;
};

#ifdef FTS_GESTURE



unsigned int tpd_gesture_switch_is_opened(void)
{
    return G_stGestureCtrl.msg_switch;
}

unsigned int inline tpd_gesture_switch_open(void)
{
    G_stGestureCtrl.msg_switch = GESTURE_SWITCH_OPEN;
    return G_stGestureCtrl.msg_switch;
}

unsigned int tpd_gesture_switch_close(void)
{
    G_stGestureCtrl.msg_switch = GESTURE_SWITCH_CLOSE;
    return G_stGestureCtrl.msg_switch;
}

static int tpd_gesture_is_enable(void)
{
    int ret = 0;
    u8 reg_value = 0;
    ret = fts_read_reg(i2c_client, 0xd0, &reg_value);
    pr_emerg("tpd_gesture_is_enable FTS_GESTURE state=%d, ret = %d.....\n", reg_value, ret);

    if  ((ret < 0) || (reg_value != GESTURE_ENABLE))
    {
        reg_value = GESTURE_DISABLE;
    }

    //G_stGestureCtrl.state = reg_value;

    return reg_value;
}

static int tpd_gesture_enable(void)
{
    int ret = 0;

    do
    {
        u8 reg_value = 0;

        ret = fts_write_reg(i2c_client, 0xd0, 0x01);

        if (ret < 0)
        {
            G_stGestureCtrl.state = GESTURE_DISABLE;
            pr_err("tpd_gesture_enable write 0xd0 failed, ret = %d.....\n", ret);
            break;
        }

        fts_write_reg(i2c_client, 0xd1, 0x3f);
        fts_write_reg(i2c_client, 0xd2, 0x1f);
        fts_write_reg(i2c_client, 0xd5, 0x40);
        fts_write_reg(i2c_client, 0xd6, 0x10);
        fts_write_reg(i2c_client, 0xd7, 0x20);
        fts_write_reg(i2c_client, 0xd8, 0x00);

        ret = fts_read_reg(i2c_client, 0xd0, &reg_value);

        pr_emerg("tpd_gesture_enable state=%d, ret = %d.....\n", reg_value, ret);

        if (ret < 0)
        {
            G_stGestureCtrl.state = GESTURE_DISABLE;
            pr_err("tpd_gesture_enable read 0xd0 failed, ret = %d.....\n", ret);
            break;
        }
        else
        {
            if (reg_value == GESTURE_ENABLE)
            {
                G_stGestureCtrl.state = GESTURE_ENABLE;
            }
            else
            {
                G_stGestureCtrl.state = GESTURE_DISABLE;
            }
        }

        ret = G_stGestureCtrl.state;

    }
    while (0);

    return ret;
}

static int tpd_gesture_disable(void)
{
    int ret = 0;

    do
    {
        u8 reg_value = 0;

        ret = fts_write_reg(i2c_client, 0xd0, 0x00);

        if (ret < 0)
        {
            FTS_DBG("tpd_gesture_disable write 0xd0 failed, ret = %d.....\n", ret);
            break;
        }

        ret = fts_read_reg(i2c_client, 0xd0, &reg_value);
        pr_emerg("tpd_gesture_disable state = %d, ret = %d.....\n", reg_value, ret);

        if (ret < 0)
        {
            FTS_DBG("tpd_gesture_enable read 0xd0 failed, ret = %d.....\n", ret);
            break;
        }

        G_stGestureCtrl.state = reg_value;

        ret = G_stGestureCtrl.state;

    }
    while (0);

    return ret;
}

static void tpd_gesture_report_key(u8 key1, u8 key2)
{
    if (key1 != KEY_GESTURE_NONE)
    {
        input_report_key(tpd->dev, key1, 1);
        input_sync(tpd->dev);
        input_report_key(tpd->dev, key1, 0);
        input_sync(tpd->dev);
    }

    if (key2 != KEY_GESTURE_NONE)
    {
        input_report_key(tpd->dev, key2, 1);
        input_sync(tpd->dev);
        input_report_key(tpd->dev, key2, 0);
        input_sync(tpd->dev);
    }
}
//add by linan for ft5436 gesture
static struct device *mx_tsp;   
static u8 gesture_three_byte_one = 0;
static u8 gesture_three_byte_two = 0;
static u8 gesture_three_byte_three = 0;
static u8 gesture_three_byte_four = 0;
static int gesture_code=0;
static ssize_t gesture_data_store(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{  
    return 0;
}
/*----------------------------------------------------------------------------*/
static ssize_t gesture_data_show(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	FTS_DBG("gesture_data_show gesture_data=%x\n",gesture_code);
	return snprintf(buf, PAGE_SIZE, "%d\n", gesture_code);
}

static ssize_t gesture_control_node_store(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{  
    u32 value,value1,value2;
    sscanf(buf, "%x", &value);
    value1 = value&0x000000ff;
    value2 = (value>>16)&0x000000ff;
    if(value2 == 1)
    {
    	gesture_three_byte_one = value1;
    }
    else if(value2 == 2)
    {
    	gesture_three_byte_two = value1;
    }
    else if(value2 == 3)
    {
    	gesture_three_byte_three = value1;
    }
    else if(value2 == 4)
    {
    	gesture_three_byte_four = value1;
    }
    TPD_DMESG("==gesture_three_byte_one=%x,gesture_three_byte_two=%x,gesture_three_byte_three=%x,gesture_three_byte_four=%x\n",gesture_three_byte_one,gesture_three_byte_two,gesture_three_byte_three,gesture_three_byte_four);
    return count;
}
static ssize_t gesture_control_node_show(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	return 0;
}
static ssize_t gesture_control_store(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{  
    if(buf[2] == 1)
    {
    	gesture_three_byte_one = buf[0];
    }
    else if(buf[2] == 2)
    {
    	gesture_three_byte_two = buf[0];
    }
    else if(buf[2] == 4)
    {
    	gesture_three_byte_three = buf[0];
    }
    else if(buf[2] == 3)
    {
    	gesture_three_byte_four = buf[0];
    }
    if((gesture_three_byte_one == 0) || ((gesture_three_byte_two == 0) && (gesture_three_byte_three == 0) && (gesture_three_byte_four == 0)))
		ft5x46_gesture_state_pre = MZ_GESTURE_DISABLE;
    else
		ft5x46_gesture_state_pre = MZ_GESTURE_ENABLE;
	
    FTS_DBG("ft5x46_gesture_state_pre=%d,gesture_three_byte_one=%x,gesture_three_byte_two=%x,gesture_three_byte_three=%x,gesture_three_byte_four=%x\n",ft5x46_gesture_state_pre,gesture_three_byte_one,gesture_three_byte_two,gesture_three_byte_three,gesture_three_byte_four);
    	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t gesture_control_show(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	return 0; 
}




static DEVICE_ATTR(gesture_data,     		S_IWUSR | S_IRUGO, gesture_data_show, gesture_data_store);
static DEVICE_ATTR(gesture_control_node,      		S_IWUSR | S_IRUGO, gesture_control_node_show,  gesture_control_node_store);
static DEVICE_ATTR(gesture_control,      		S_IWUSR | S_IRUGO, gesture_control_show,  gesture_control_store);

static struct attribute *gesture_attributes[] = {
	&dev_attr_gesture_data.attr,
	&dev_attr_gesture_control_node.attr,
	&dev_attr_gesture_control.attr,
	NULL
};

static struct attribute_group gesture_attribute_group = {
	.attrs = gesture_attributes
};
//add end
static void tpd_gesture_report(int gesture_id)
{
    FTS_DBG("tpd_gesture_report gesture_id = 0x%x...\n", gesture_id);
	FTS_DBG("tpd_gesture_report-----?ft5x46_gesture_state_pre=%d,gesture_three_byte_one=%x,gesture_three_byte_two=%x,gesture_three_byte_three=%x,gesture_three_byte_four=%x\n",ft5x46_gesture_state_pre,gesture_three_byte_one,gesture_three_byte_two,gesture_three_byte_three,gesture_three_byte_four);
    //gesture_three_byte_one=0x1;
    switch (gesture_id)
    {
        case GESTURE_DOUBLECLICK:
	     if((gesture_three_byte_one==0x1) && ((gesture_three_byte_two & 0x01)==0x1))
	     {
			gesture_code = DOUBLE_TAP;
			FTS_DBG("+++++++++++++++tpd_gesture_report==>0x%x\n",gesture_code);
			input_report_key(tpd->dev, KEY_GESTURE, 1);
			input_sync(tpd->dev);
			input_report_key(tpd->dev, KEY_GESTURE, 0);
			input_sync(tpd->dev);
	     }
            break;
        case GESTURE_LEFT:
		if((gesture_three_byte_one==0x1) && ((gesture_three_byte_three & 0x2)==0x2))
		{
			gesture_code = SWIPE_X_LEFT;
			FTS_DBG("+++++++++++++++tpd_gesture_report==>0x%x\n",gesture_code);
			input_report_key(tpd->dev, KEY_GESTURE, 1);
			input_sync(tpd->dev);
			input_report_key(tpd->dev, KEY_GESTURE, 0);
			input_sync(tpd->dev);
		} 
            break;
        case GESTURE_RIGHT:
		if((gesture_three_byte_one==0x1) && ((gesture_three_byte_three & 0x01)==0x01))
		{
			gesture_code = SWIPE_X_RIGHT;
			FTS_DBG("+++++++++++++++tpd_gesture_report==>0x%x\n",gesture_code);
			input_report_key(tpd->dev, KEY_GESTURE, 1);
			input_sync(tpd->dev);
			input_report_key(tpd->dev, KEY_GESTURE, 0);
			input_sync(tpd->dev);
		} 
            break;
        case GESTURE_UP:
		if((gesture_three_byte_one==0x1) && ((gesture_three_byte_three & 0x08)==0x08))
		{
			gesture_code = SWIPE_Y_UP;
			FTS_DBG("+++++++++++++++tpd_gesture_report==>0x%x\n",gesture_code);
			input_report_key(tpd->dev, KEY_GESTURE, 1);
			input_sync(tpd->dev);
			input_report_key(tpd->dev, KEY_GESTURE, 0);
			input_sync(tpd->dev);
		} 
            break;
        case GESTURE_DOWN:
		if((gesture_three_byte_one==0x1) && ((gesture_three_byte_three & 0x04)==0x04))
		{
			gesture_code = SWIPE_Y_DOWN;
			FTS_DBG("+++++++++++++++tpd_gesture_report==>0x%x\n",gesture_code);
			input_report_key(tpd->dev, KEY_GESTURE, 1);
			input_sync(tpd->dev);
			input_report_key(tpd->dev, KEY_GESTURE, 0);
			input_sync(tpd->dev);
		} 
            break;
        case GESTURE_O:
		if((gesture_three_byte_one==0x1) && ((gesture_three_byte_four & 0x80)==0x80))
		{
			gesture_code = UNICODE_O;
			FTS_DBG("+++++++++++++++tpd_gesture_report==>0x%x\n",gesture_code);
			input_report_key(tpd->dev, KEY_GESTURE, 1);
			input_sync(tpd->dev);
			input_report_key(tpd->dev, KEY_GESTURE, 0);
			input_sync(tpd->dev);
		} 
            break;
        case GESTURE_W:
		if((gesture_three_byte_one==0x1) && ((gesture_three_byte_four & 0x08)==0x08))
		{
			gesture_code = UNICODE_W;
			FTS_DBG("+++++++++++++++tpd_gesture_report==>0x%x\n",gesture_code);
			input_report_key(tpd->dev, KEY_GESTURE, 1);
			input_sync(tpd->dev);
			input_report_key(tpd->dev, KEY_GESTURE, 0);
			input_sync(tpd->dev);
		} 
            break;
        case GESTURE_M:
		if((gesture_three_byte_one==0x1) && ((gesture_three_byte_four & 0x10)==0x10))
		{
			gesture_code = UNICODE_M;
			FTS_DBG("+++++++++++++++tpd_gesture_report==>0x%x\n",gesture_code);
			input_report_key(tpd->dev, KEY_GESTURE, 1);
			input_sync(tpd->dev);
			input_report_key(tpd->dev, KEY_GESTURE, 0);
			input_sync(tpd->dev);
		} 
            break;
        case GESTURE_E:
		if((gesture_three_byte_one==0x1) && ((gesture_three_byte_four & 0x04)==0x04))
		{
			gesture_code = UNICODE_E;
			FTS_DBG("+++++++++++++++tpd_gesture_report==>0x%x\n",gesture_code);
			input_report_key(tpd->dev, KEY_GESTURE, 1);
			input_sync(tpd->dev);
			input_report_key(tpd->dev, KEY_GESTURE, 0);
			input_sync(tpd->dev);
		} 
            break;
        case GESTURE_C:
		if((gesture_three_byte_one==0x1) && ((gesture_three_byte_four & 0x02)==0x02))
		{
			gesture_code = UNICODE_C;
			FTS_DBG("+++++++++++++++tpd_gesture_report==>0x%x\n",gesture_code);
			input_report_key(tpd->dev, KEY_GESTURE, 1);
			input_sync(tpd->dev);
			input_report_key(tpd->dev, KEY_GESTURE, 0);
			input_sync(tpd->dev);
		} 
            break;
        case GESTURE_S:
		if((gesture_three_byte_one==0x1) && ((gesture_three_byte_four & 0x20)==0x20))
		{
			gesture_code = UNICODE_S;
			FTS_DBG("+++++++++++++++tpd_gesture_report==>0x%x\n",gesture_code);
			input_report_key(tpd->dev, KEY_GESTURE, 1);
			input_sync(tpd->dev);
			input_report_key(tpd->dev, KEY_GESTURE, 0);
			input_sync(tpd->dev);
		} 
            break;
        case GESTURE_V:
		if((gesture_three_byte_one==0x1) && ((gesture_three_byte_four & 0x01)==0x01))
		{
			gesture_code = UNICODE_V_DOWN;
			FTS_DBG("+++++++++++++++tpd_gesture_report==>0x%x\n",gesture_code);
			input_report_key(tpd->dev, KEY_GESTURE, 1);
			input_sync(tpd->dev);
			input_report_key(tpd->dev, KEY_GESTURE, 0);
			input_sync(tpd->dev);
		} 
            break;
        case GESTURE_Z:
		if((gesture_three_byte_one==0x1) && ((gesture_three_byte_four & 0x40)==0x40))
		{
			gesture_code = UNICODE_Z;
			FTS_DBG("+++++++++++++++tpd_gesture_report==>0x%x\n",gesture_code);
			input_report_key(tpd->dev, KEY_GESTURE, 1);
			input_sync(tpd->dev);
			input_report_key(tpd->dev, KEY_GESTURE, 0);
			input_sync(tpd->dev);
		} 
            break;
        default:
            gesture_code = GESTURE_ERROR;
            break;
    }

}

static int tpd_gesture_read_data(void)
{
    unsigned char buf[FTS_GESTURE_POINTS * 2] = { 0 };
    int ret = -1;
    int i = 0;
    int gesture_id = 0;
    short pointnum = 0;

    buf[0] = 0xd3;
    ret = fts_i2c_Read(i2c_client, buf, 1, buf, FTS_GESTURE_POINTS_HEADER);

    if (ret < 0)
    {
        pr_err( "%s read touchdata failed...\n", __func__);
        goto exit;
    }

    gesture_id = buf[0];
    pointnum = (short)(buf[1]) & 0xff;

    if (pointnum < 0)
    {
        goto exit;
    }

    if (pointnum > FTS_GESTURE_POINTS)
    {
        pointnum = FTS_GESTURE_POINTS;
    }

    G_stGestureCtrl.point_num = pointnum;

    memset(G_stGestureCtrl.x, 0, sizeof(G_stGestureCtrl.x));
    memset(G_stGestureCtrl.y, 0, sizeof(G_stGestureCtrl.y));

    pr_emerg("%s gesture_id = 0x%x...\n", __func__, gesture_id);
    FTS_DBG("%s pointnum = %d...\n", __func__, pointnum);
    /*
        buf[0] = 0xd3;
        ret = fts_i2c_Read(i2c_client, buf, 1, buf, (pointnum * 4 + 2));
        if (ret < 0)
        {
            pr_err( "%s read touchdata failed...\n", __func__);
        }
        else
        {
            for (i = 0; i < pointnum; i++)
            {
                G_stGestureCtrl.x[i] = (((s16) buf[0 + (4 * i)]) & 0x0F) << 8
                                       | (((s16) buf[1 + (4 * i)]) & 0xFF);
                G_stGestureCtrl.y[i] = (((s16) buf[2 + (4 * i)]) & 0x0F) <<8
                                       | (((s16) buf[3 + (4 * i)]) & 0xFF);
            }
        }
        */
    tpd_gesture_report(gesture_id);

exit:
    return -1;
}

#endif

static void tpd_clear_all_touch(void)
{
    int i = 0;
    while (ft5x46_finger_status != 0)
    {
        tpd_up(0, 0, 0, i);
        i++;
        if (i > FTS_MAX_TOUCH)
        {
            break;
        }
    }
    input_sync(tpd->dev);
}

extern kal_bool upmu_is_chr_det(void);

static void tpd_charger_check(int resume)
{
    static int ft5x46_charger_state = 0;

    int ret = 0;
    int charger_state = upmu_is_chr_det();
    pr_debug("[FTS] charger_state: %d, cur_state: %d\n",
             charger_state, ft5x46_charger_state);

    if (resume || (ft5x46_charger_state != charger_state))
    {
        ft5x46_charger_state = charger_state;

        if (ft5x46_charger_state != 0)  // charger plugged in
        {
            ret = fts_write_reg(i2c_client, 0x8b, 0x03);
        }
        else
        {
            ret = fts_write_reg(i2c_client, 0x8b, 0x01);
        }

        if (ret < 0)
        {
            pr_err("fts i2c write 0x8b error\n");
        }
    }
}

static int touch_event_handler(void *unused)
{
    struct touch_info cinfo, pinfo;
    int i = 0;

    struct sched_param param = { .sched_priority = RTPM_PRIO_TPD };
    sched_setscheduler(current, SCHED_RR, &param);

    u8 state;
    int charger_state;

    do
    {
        set_current_state(TASK_INTERRUPTIBLE);

        if (tpd_halt)
        {
            tpd_flag = 0;
        }

        wait_event_interruptible(waiter, tpd_flag != 0);

        tpd_flag = 0;

        set_current_state(TASK_RUNNING);

        mutex_lock(&ft_suspend_lock);
        tpd_charger_check(0);
        #ifdef FTS_GESTURE
        if ((G_stGestureCtrl.state == GESTURE_ENABLE)
            && (tpd_gesture_is_enable() == GESTURE_ENABLE))
        {
            tpd_gesture_read_data();
            goto exit_lock;
        }
        #endif

        if (tpd_touchinfo(&cinfo, &pinfo))
        {
            //FTS_DBG("point_num = %d\n",point_num);
            TPD_DEBUG_SET_TIME;
            if (point_num > 0)
            {
                input_report_key(tpd->dev, BTN_TOUCH, 1);
            }

            for (i = 0; i < FTS_MAX_TOUCH; i++)
            {
                if (cinfo.p[i] == 0 || cinfo.p[i] == 2)
                    tpd_down(cinfo.x[i], cinfo.y[i],
                             cinfo.p[i], cinfo.id[i]);
                else if (cinfo.p[i] == 1)
                    tpd_up(cinfo.x[i], cinfo.y[i],
                           cinfo.p[i], cinfo.id[i]);
            }
            if (point_num <= 0)
            {
                input_report_key(tpd->dev, BTN_TOUCH, 0);//add by xunanbin 20151109
                #ifdef TYPE_B_PROTOCOL
                tpd_clear_all_touch();
                #else
                tpd_up(0, 0, 0, 0);
                #endif
            }
            input_sync(tpd->dev);
        }


        if (tpd_mode == 12)
        {
            //power down for desence debug
            //power off, need confirm with SA
            //ctp_power_off();
            msleep(20);
        }
        mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
    exit_lock:
        mutex_unlock(&ft_suspend_lock);
    }
    while (!kthread_should_stop());

    return 0;
}

static int tpd_detect (struct i2c_client *client, struct i2c_board_info *info)
{
    strcpy(info->type, TPD_DEVICE);
    return 0;
}

static void tpd_eint_interrupt_handler(void)
{
    TPD_DEBUG_PRINT_INT;
    tpd_flag = 1;
    wake_up_interruptible(&waiter);
}

int tpd_firmware_upgrade_thread(void *unused)
{
    int err = 0;

    // disable esd checker work when fw is updating
    #ifdef FTS_ESD_CHECKER
    cancel_delayed_work_sync(&fts_esd_checker);
    #endif
    mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
    err = fts_ctpm_auto_upgrade(i2c_client);
    mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);

    #ifdef FTS_ESD_CHECKER
    queue_delayed_work(fts_esd_wq, &fts_esd_checker, 0);
    #endif
    return err;
}

static int tpd_ioctl(unsigned int cmd)
{
    FTS_DBG("[FTS] tpd_ioctl  enter cmd = %x...\r\n", cmd);

    switch (cmd)
    {
        case GESTURE_ENABLE:
            tpd_gesture_switch_open();
            break;

        case GESTURE_DISABLE:
            tpd_gesture_switch_close();
            break;

        default:
            break;
    }

    FTS_DBG("[FTS] tpd_ioctl end...\n");

    return 0;
}

/*
Get /dev/devices  sysfs kobject struct
*/
static struct kobject * sysfs_get_devices_parent(void)
{
    struct device *tmp = NULL;
    struct kset *pdevices_kset;

    tmp = kzalloc(sizeof(*tmp), GFP_KERNEL);
    if (!tmp)
    {
        return NULL;
    }

    device_initialize(tmp);
    pdevices_kset = tmp->kobj.kset;
    kfree(tmp);
    return &pdevices_kset->kobj;
}

void ft5x46_gesture_handler(u8 state)
{
    ft5x46_gesture_state = state;
}

#ifdef FTS_ESD_CHECKER
static void fts_esd_checker_handler(struct work_struct *work)
{
    u8 data = 0x1;
#define FTS_ESD_REG 0xE1

    if (apk_debug_flag)
    {
        pr_warn("apk debug node is opened, disable esd check, %d\n",
                apk_debug_flag);
        goto exit;
    }

    if (fts_read_reg(g_focalclient, FTS_ESD_REG, &data) < 0)
    {
        pr_err("%s: read esd i2c error\n", __func__);
    }
    pr_err("%s: read esd %02x\n", __func__, data);

    if (data != 0)
    {
        pr_err("!!! ic work abnormally, reset ic\n");
        fts_ctpm_hw_reset();
    }
    else
    {
        fts_write_reg(g_focalclient, FTS_ESD_REG, 0x1);
    }

exit:

    #ifdef FTS_ROI
    if ((tpd_roi_switch_state() == ROI_ENABLE) && (tpd_roi_is_enable() != ROI_ENABLE))
    {
        tpd_roi_enable();
    }
    #endif

    if (!tpd_halt)
    {
        queue_delayed_work(fts_esd_wq, &fts_esd_checker, 2 * HZ);
    }
}
#endif

static int tpd_irq_registration(void)
{
       struct device_node *node = NULL;
      int ret = 0;
       u32 ints[2] = {0,0};
       unsigned int touch_irq = 0;
       printk("Device Tree Tpd_irq_registration!");
       
       node = of_find_compatible_node(NULL, NULL, "mediatek, TOUCH_PANEL-eint");
       if(node){
               of_property_read_u32_array(node , "debounce", ints, ARRAY_SIZE(ints));
               gpio_set_debounce(ints[0], ints[1]);

               touch_irq = irq_of_parse_and_map(node, 0);
               ret = request_irq(touch_irq, (irq_handler_t)tpd_eint_interrupt_handler, EINTF_TRIGGER_RISING, "TOUCH_PANEL-eint", NULL);
               if(ret > 0){
                   ret = -1;
                   printk("tpd request_irq IRQ LINE NOT AVAILABLE!.");
               }
       }else{
               printk("tpd request_irq can not find touch eint device node!.");
               ret = -1;
       }
       printk("[%s]irq:%d, debounce:%d-%d:", __FUNCTION__, touch_irq, ints[0], ints[1]);
       return ret;
}


static int tpd_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int retval = TPD_OK;
    int err = 0;
    int reset_count = 0;
    char data;
    u8 chip_id, i;
    u8 vendor_id=0;
   char *info = tp_info;
reset_proc:
    i2c_client = client;

    #ifdef FTS_MCAP_TEST
    g_focalclient = client;
    #endif
    FTS_DBG("[FTS] enter tpd_probe...\r\n ");

    //enable gpio
    mt_set_gpio_pull_enable(GPIO_CTP_RST_PIN, GPIO_PULL_ENABLE);
    mt_set_gpio_pull_select(GPIO_CTP_RST_PIN, GPIO_PULL_UP);
    mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);

    msleep(50);
    FTS_DBG("[FTS] tpd_probe power on begin...\r\n ");

    #ifdef TPD_POWER_SOURCE_CUSTOM
    hwPowerOn(TPD_POWER_SOURCE_CUSTOM, VOL_2800, "TP");
    #else
    hwPowerOn(TPD_POWER_SOURCE, VOL_3300, "TP");
    #endif

    #ifdef TPD_POWER_SOURCE_1800
    hwPowerOn(TPD_POWER_SOURCE_1800, VOL_1800, "TP");
    #endif
    msleep(100);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
    mdelay(200);

    FTS_DBG("[FTS] tpd_probe power on end...\r\n ");
	
    if (fts_dma_buffer_init() < 0)
    {
        return -1;
    }

    if ((fts_read_reg(i2c_client, 0x00, &data)) < 0)
    {
        TPD_DMESG("I2C transfer error, line: %d\n", __LINE__);

        fts_dma_buffer_deinit();
        #ifdef TPD_RESET_ISSUE_WORKAROUND
        if ( reset_count < TPD_MAX_RESET_COUNT )
        {
            reset_count++;
            goto reset_proc;
        }
        #endif
        return -1;
    }

    mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
    mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
    mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
    mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);
    msleep(50);

    tpd_irq_registration();

    FTS_DBG("[FTS] tpd_probe mt_eint_unmask end...\r\n ");

    msleep(100);

//add TP vendor  linan
    err = fts_read_reg(i2c_client, FT_REG_VENDOR_ID, &vendor_id);

    if (err < 0)
    {
        pr_err("arad FT_REG_VENDOR_ID failed, ret: %d value: %d\n",err, vendor_id);
    }

    sprintf(info,"FT5X46_JL_%04x",vendor_id);
    FTS_DBG("info:%s\n", info);
//end
    tpd_load_status = 1;

    #ifdef VELOCITY_CUSTOM_FT5206
    if ((err = misc_register(&tpd_misc_device)))
    {
        FTS_DBG("[FTS] probe tpd_misc_device register failed...\r\n");

    }
    #endif
	
    #ifdef TYPE_B_PROTOCOL
    input_mt_init_slots(tpd->dev, FTS_MAX_TOUCH, INPUT_MT_DIRECT);
    #endif

    thread = kthread_run(touch_event_handler, 0, TPD_DEVICE);
    if (IS_ERR(thread))
    {
        retval = PTR_ERR(thread);
        TPD_DMESG("%s: [FTS] failed to create kernel thread: %d\n", __func__, retval);
    }

    //mz_gesture_handle_register(ft5x46_gesture_handler);

    #ifdef FTS_ESD_CHECKER
    INIT_DELAYED_WORK(&fts_esd_checker, fts_esd_checker_handler);
    fts_esd_wq = create_singlethread_workqueue("FTS-ESD");
    pr_err("start fts esd checking ...\n");
    queue_delayed_work(fts_esd_wq, &fts_esd_checker, 0);
    #endif

    #ifdef TPD_AUTO_UPGRADE
    //  fts_ctpm_auto_upgrade(i2c_client);
    kthread_run(tpd_firmware_upgrade_thread, 0, TPD_DEVICE);
    #endif

    TPD_DMESG("%s: [FTS] Touch Panel Device Probe %s\n",
              __func__, ((retval < TPD_OK) ? "FAIL" : "PASS"));


    #ifdef FTS_APK_DEBUG
    ft5x0x_create_apk_debug_channel(client);
    #endif

    #ifdef TPD_SYSFS_DEBUG
    fts_create_sysfs(i2c_client);
    #endif

	err = sysfs_create_group(&i2c_client->dev.kobj, &gesture_attribute_group);
	if (err < 0)
	{
	   TPD_DMESG("unable to create gesture attribute file\n");
	}
    sysfs_create_link(sysfs_get_devices_parent(), &client->dev.kobj, "mx_tsp");
    //meizu_sysfslink_register_n(&client->dev, "tp");

    #ifdef FTS_CTL_IIC
    if (ft_rw_iic_drv_init(i2c_client) < 0)
    {
        TPD_DMESG("%s:[FTS] create fts control iic driver failed\n", __func__);
    }
    #endif
     //add gesture event  linan
#if FTS_GESTURE
    input_set_capability(tpd->dev, EV_KEY, KEY_GESTURE);
#endif

    return 0;

}

static int tpd_remove(struct i2c_client *client)
{
    #ifdef FTS_APK_DEBUG
    ft5x0x_release_apk_debug_channel();
    #endif
    #ifdef TPD_SYSFS_DEBUG
    fts_release_sysfs(client);
    #endif

    #ifdef FTS_CTL_IIC
    ft_rw_iic_drv_exit();
    #endif

    fts_dma_buffer_deinit();

    FTS_DBG("[FTS] TPD removed\n");

    return 0;
}

static int tpd_local_init(void)
{


	//linan
 	#if !defined CONFIG_MTK_LEGACY
	int ret;
	char *cust_tpd_name = NULL;
	struct device_node *node = NULL, *tpd_node;
	GTP_INFO("Device Tree get regulator!");
	node = of_find_compatible_node(NULL, NULL, "mediatek,regulator_supply");
	if(node){
		cust_tpd_name = of_get_property(node, "CAP_TOUCH_VDD", NULL);
		if (cust_tpd_name == NULL){
			tpd->reg=regulator_get(tpd->tpd_dev,"VTOUCH");
			ret=regulator_set_voltage(tpd->reg, 2800000, 2800000);	// set 2.8v
			if (ret){
				GTP_ERROR("regulator_set_voltage(%d) failed!\n", ret);
				return -1;
			}
		}else{
			GTP_INFO("Using Cust-LDO. Touch regulator name =%s!\n", cust_tpd_name);
			tpd_node = tpd->tpd_dev->of_node;
			tpd->tpd_dev->of_node = node;
			tpd->reg=regulator_get(tpd->tpd_dev,"CAP_TOUCH_VDD");
			ret=regulator_set_voltage(tpd->reg, 2800000, 2800000);	// set 2.8v
			if (ret){
				GTP_ERROR("regulator_set_voltage() failed!\n");
			return -1;
		}
			tpd->tpd_dev->of_node = tpd_node;
		}
	}else{
		GTP_ERROR("regulator get touch node failed!\n");
		return -1;
	}
#endif


    TPD_DMESG("FTS I2C Touchscreen Driver (Built %s @ %s)\n", __DATE__, __TIME__);

    if (i2c_add_driver(&tpd_i2c_driver) != 0)
    {
        TPD_DMESG("FTS unable to add i2c driver.\n");
        return -1;
    }
    if (tpd_load_status == 0)
    {
        TPD_DMESG("FTS add error touch panel driver.\n");
        i2c_del_driver(&tpd_i2c_driver);
        return -1;
    }

    #ifdef TPD_HAVE_BUTTON
    tpd_button_setting(TPD_KEY_COUNT, tpd_keys_local, tpd_keys_dim_local);// initialize tpd button data
    #endif

    #if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
    TPD_DO_WARP = 1;
    memcpy(tpd_wb_start, tpd_wb_start_local, TPD_WARP_CNT * 4);
    memcpy(tpd_wb_end, tpd_wb_start_local, TPD_WARP_CNT * 4);
    #endif

    #if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
    memcpy(tpd_calmat, tpd_def_calmat_local, 8 * 4);
    memcpy(tpd_def_calmat, tpd_def_calmat_local, 8 * 4);
    #endif

    TPD_DMESG("end %s, %d\n", __FUNCTION__, __LINE__);
    tpd_type_cap = 1;
    return 0;
}

#ifdef MZ_HALL_MODE
extern int ft5x46_hall_mode_state;
int ft5x46_hall_mode_switch(int state);
#endif

extern void mz_tp_set_mode(int mode);
static void tpd_resume( struct early_suspend *h )
{
    int i = 0;
    int iRet = 0;
    u8  uc_tp_fm_ver = 0;

    TPD_DMESG("TPD wake up\n");

    mutex_lock(&ft_suspend_lock);
    #ifdef TYPE_B_PROTOCOL
    tpd_clear_all_touch();
    #else
    tpd_up(0, 0, 0, 0);
    input_sync(tpd->dev);
    #endif
FTS_DBG("======tpd_resume======ft5x46_gesture_state_pre=%d\n",ft5x46_gesture_state_pre);
    #ifdef FTS_GESTURE
    if (ft5x46_gesture_state_pre != MZ_GESTURE_DISABLE)
    {
        tpd_gesture_disable();
        fts_ctpm_hw_reset();
        msleep(200);
    }
    else
    {
        #ifdef TPD_CLOSE_POWER_IN_SLEEP
        hwPowerOn(TPD_POWER_SOURCE_CUSTOM, VOL_2800, "TP");
        #else
        mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
        mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
        mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
        msleep(10);
        mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
        msleep(10);
        mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
        #endif
        msleep(200);
    }
    #endif

    #ifdef FTS_GLOVE
    if (tpd_glove_switch_is_opened())
    {
        for (i = 0; i < FTS_GLOVE_RETRY_CNT; i++)
        {
            if (GLOVE_ENABLE == tpd_glove_enable())
            {
                FTS_DBG("[FTS] tpd_resume glove state = enable...\n");
                break;
            }
        }

        if (i >= FTS_GLOVE_RETRY_CNT)
        {
            tpd_glove_switch_close();
            tpd_glove_disable();
        }
    }
    #endif

    #ifdef FTS_ROI
    if ((tpd_roi_switch_state() == ROI_ENABLE) && (tpd_roi_is_enable() != ROI_ENABLE))
    {
        tpd_roi_enable();
    }
    #endif

    tpd_charger_check(1);

    #ifdef MZ_HALL_MODE
    ft5x46_hall_mode_switch(ft5x46_hall_mode_state);
    #endif

    if (ft5x46_hall_mode_state)
    {
        mz_tp_set_mode(MZ_COVER_MODE);
    }
    else
    {
        mz_tp_set_mode(MZ_HAND_MODE);
    }

    mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
    msleep(30);

    tpd_halt = 0;

    #ifdef FTS_ESD_CHECKER
    pr_err("start fts esd checking ...\n");
    queue_delayed_work(fts_esd_wq, &fts_esd_checker, 0);
    #endif
    mutex_unlock(&ft_suspend_lock);
    TPD_DMESG("TPD wake up done\n");
	
    fts_write_reg(i2c_client, 0xe7, 0x01);//add by xunanbin for ESD test on 20151204
}

static void tpd_suspend( struct early_suspend *h )
{
    unsigned char data = 0x3;
    unsigned char reg = 0xA5;
    int i = 0;

    mutex_lock(&ft_suspend_lock);
    #ifdef FTS_ESD_CHECKER
    pr_err("stop fts esd checking ...\n");
    cancel_delayed_work(&fts_esd_checker);
    #endif
FTS_DBG("============ft5x46_gesture_state_pre=%d\n",ft5x46_gesture_state_pre);
    #ifdef FTS_GESTURE
    if (ft5x46_gesture_state_pre != MZ_GESTURE_DISABLE)
    {
    FTS_DBG("TPD enter gesture\n");
        #ifdef FTS_GESTURE_DBG
        tpd_gesture_switch_open();
        #endif
        if (tpd_gesture_switch_is_opened() == GESTURE_SWITCH_OPEN)
        {
            for (i = 0; i < FTS_GESTURE_RETRY_CNT; i++)
            {
                if (GESTURE_ENABLE == tpd_gesture_enable())
                {
                    FTS_DBG("[FTS] tpd_suspend gesture \
							state = enable...\n");
                    mz_tp_set_mode(MZ_GESTURE_MODE);
                    goto exit;
                }
            }

            if (i >= FTS_GESTURE_RETRY_CNT)
            {
                tpd_gesture_switch_close();
                tpd_gesture_disable();
            }
        }
    }
    else
    {
        FTS_DBG("[FTS] tpd_suspend gesture state = disable...\n");

        tpd_halt = 1;

        FTS_DBG("TPD enter sleep---\n");
        mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);

        mz_tp_set_mode(MZ_SLEEP_MODE);
        mutex_lock(&i2c_access);
        fts_write_reg(i2c_client, reg, data);  //TP enter sleep mode
        mutex_unlock(&i2c_access);

        #ifdef TPD_CLOSE_POWER_IN_SLEEP
        msleep(5);
        hwPowerDown(TPD_POWER_SOURCE_CUSTOM, "TP");
        #endif
    }
    #else //FTS_GESTURE
    tpd_halt = 1;

    TPD_DMESG("TPD enter sleep\n");
    mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);

    mutex_lock(&i2c_access);
    fts_write_reg(i2c_client, reg, data);  //TP enter sleep mode
    mutex_unlock(&i2c_access);

    #ifdef TPD_CLOSE_POWER_IN_SLEEP
    msleep(5);
    hwPowerDown(TPD_POWER_SOURCE_CUSTOM, "TP");
    #endif

    #endif

exit:
    mutex_unlock(&ft_suspend_lock);
    TPD_DMESG("TPD enter sleep done\n");
}

static struct tpd_driver_t tpd_device_driver =
{
    .tpd_device_name = TPD_NAME,
    .tpd_local_init = tpd_local_init,
    .suspend = tpd_suspend,
    .resume = tpd_resume,
    #ifdef TPD_HAVE_BUTTON
    .tpd_have_button = 1,
    #else
    .tpd_have_button = 0,
    #endif
};

static int __init tpd_driver_init(void)
{
    TPD_DMESG("\n\nMediaTek FT5x46 touch panel driver init\n");
    i2c_register_board_info(0, &ft5206_i2c_tpd, 1);
    if (tpd_driver_add(&tpd_device_driver) < 0)
    {
        FTS_DBG("add FT5x46 driver failed\n");
    }
    return 0;
}

static void __exit tpd_driver_exit(void)
{
    TPD_DMESG("MediaTek FT5206 touch panel driver exit\n");
    tpd_driver_remove(&tpd_device_driver);
    sysfs_remove_group(&mx_tsp->kobj,&gesture_attribute_group);
    root_device_unregister(mx_tsp);
}



module_init(tpd_driver_init);
module_exit(tpd_driver_exit);


