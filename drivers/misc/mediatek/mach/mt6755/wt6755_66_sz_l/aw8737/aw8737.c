#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/ioctl.h>
#include <linux/platform_device.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <mach/mt_reg_base.h>
#include <mach/mt_boot.h>
#include <mach/mt_gpio.h>

/* ioctl command, use to open or close device */
#define AW8737_OPEN                     _IOC(_IOC_NONE, 0x07, 0x03, 0)
#define AW8737_CLOSE                    _IOC(_IOC_NONE, 0x07, 0x04, 0)

#define AW8737_LOG_TAG                  "aw8737"
#define AW8737_MODE_PIN                 (GPIO100 | 0x80000000)

#define AW8737_DEBUG
#ifdef AW8737_DEBUG
#define AW8737_LOGD(format, args...)    do                              \
                {                                                       \
                    printk(KERN_DEBUG AW8737_LOG_TAG format, ##args);   \
                }while(0)
#define AW8737_LOGE(format, args...)    do                              \
                {                                                       \
                    printk(KERN_ERR AW8737_LOG_TAG format, ##args);     \
                }while(0)
#else
#define AW8737_LOGD(format, args...)    do{}while(0)
#define AW8737_LOGE(format, args...)    do{}while(0)
#endif

static dev_t aw8737_devno;
static struct cdev aw8737_cdev;
static struct class *aw8737_class = NULL;
static struct device *aw8737_device = NULL;
static struct mutex aw8737_mutex;

/* xOHM : Speaker Resistor
 * xDxW or xW : Speaker Power, such as 1.2W = 1D2W
 * NCN : Non-Crack-Noise
 * THD : Total Harmonic Distortion , use to test      */
typedef enum {
    AW8737_MODE_MIX = 0,
    AW8737_MODE_8OHM_1D2W_NCN = 1,
    AW8737_MODE_8OHM_1W_NCN,
    AW8737_MODE_8OHM_0D8W_NCN,
    AW8737_MODE_8OHM_1D83W_THD,
    AW8737_MODE_4OHM_2D35_NCN = 1,
    AW8737_MODE_4OHM_2W_NCN,
    AW8737_MODE_4OHM_1D6_NCN,
    AW8737_MODE_4OHM_2D6_THD,
    AW8737_MODE_CLOSE,
    AW8737_MODE_MAX,
}aw8737_mode;

static void aw8737_io_config(void)
{
    AW8737_LOGD("+%s\n", __func__);

    /* init the aw8737 enable pin */
    mt_set_gpio_mode(AW8737_MODE_PIN, GPIO_MODE_GPIO);
    mt_set_gpio_dir(AW8737_MODE_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(AW8737_MODE_PIN, GPIO_OUT_ZERO);
}

static int aw8737_set_mode(aw8737_mode mode)
{
    int i;
    
    AW8737_LOGD("+%s mode %d\n", __func__, mode);
    
    if ((mode >= AW8737_MODE_MAX)
        || (mode <= AW8737_MODE_MIX)) {
        AW8737_LOGE(KERN_ERR "mode argument is error !\n");
        return -1;
    }

    if (mode == AW8737_MODE_CLOSE) {
        mt_set_gpio_out(AW8737_MODE_PIN, GPIO_OUT_ZERO);
        msleep(1);                      // sleep 1ms, make sure the aw8737 is closed
    } else {
        msleep(1);                      // 1ms use to close the mode before set it
        for (i = 0; i < mode; ++i)
        {
            ndelay(2000);               // 2us
            mt_set_gpio_out(AW8737_MODE_PIN, GPIO_OUT_ZERO);
            ndelay(2000);               // 2us
            mt_set_gpio_out(AW8737_MODE_PIN, GPIO_OUT_ONE);
        }
        msleep(45);                     // 42ms to 50ms, make sure the mode is built
    }

    return 0;
}

static int aw8737_ops_open(struct inode *inode, struct file *file)
{
    AW8737_LOGD("+%s\n", __func__);
    aw8737_io_config();
    return 0;
}

static int aw8737_ops_release(struct inode *inode, struct file *file)
{
   AW8737_LOGD("+%s\n", __func__);
    return 0;
}

static long aw8737_ops_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    long ret;

    AW8737_LOGD("+%s, cmd = %d, arg = %ld\n", __func__, cmd, arg);

    switch (cmd)
    {
    case AW8737_OPEN:
        ret = aw8737_set_mode((aw8737_mode)arg);
        break;
    case AW8737_CLOSE:
        ret = aw8737_set_mode((aw8737_mode)arg);
        break;

    default:
        ret = -1;
        break;
    }

    return ret;
}

static struct file_operations aw8737_fops = {
    .owner          = THIS_MODULE,
    .unlocked_ioctl = aw8737_ops_ioctl,
    .open           = aw8737_ops_open,
    .release        = aw8737_ops_release,
};

static int aw8737_drv_probe(struct platform_device *dev)
{
    int ret = 0, err = 0;

    AW8737_LOGD("+%s\n", __func__);

    ret = alloc_chrdev_region(&aw8737_devno, 0, 1, "aw8737");
    if (ret) {
        AW8737_LOGE("alloc_chrdev_region fail: %d\n", ret);
        goto aw8737_probe_error;
    } else {
        AW8737_LOGD("major: %d, minor: %d\n", MAJOR(aw8737_devno), MINOR(aw8737_devno));
    }
    cdev_init(&aw8737_cdev, &aw8737_fops);
    aw8737_cdev.owner = THIS_MODULE;
    err = cdev_add(&aw8737_cdev, aw8737_devno, 1);
    if (err) {
        AW8737_LOGE("cdev_add fail: %d\n", err);
        goto aw8737_probe_error;
    }

    aw8737_class = class_create(THIS_MODULE, "aw8737");
    if (IS_ERR(aw8737_class)) {
        AW8737_LOGE("Unable to create class\n");
        goto aw8737_probe_error;
    }

    aw8737_device = device_create(aw8737_class, NULL, aw8737_devno, NULL, "aw8737");
    if(aw8737_device == NULL) {
        AW8737_LOGE("device_create fail\n");
        goto aw8737_probe_error;
    }

    return 0;

aw8737_probe_error:
    if (err == 0)
        cdev_del(&aw8737_cdev);
    if (ret == 0)
        unregister_chrdev_region(aw8737_devno, 1);

    return -1;
}

static int aw8737_drv_remove(struct platform_device *dev)
{
    AW8737_LOGD("+%s\n", __func__);
    cdev_del(&aw8737_cdev);
    unregister_chrdev_region(aw8737_devno, 1);
    device_destroy(aw8737_class, aw8737_devno);
    class_destroy(aw8737_class);

    return 0;
}

static void aw8737_dev_release(struct device *dev)
{
    return ;
}

static struct platform_driver aw8737_platform_driver =
{
    .probe      = aw8737_drv_probe,
    .remove     = aw8737_drv_remove,
    .driver     = {
        .name   = "aw8737",
        .owner  = THIS_MODULE,
    },
};

static struct platform_device aw8737_platform_device = {
    .name = "aw8737",
    .id   = -1,
    .dev  = {
        .release = aw8737_dev_release,
    }
};

static int __init aw8737_init(void)
{
    int ret = 0;
    AW8737_LOGD("+%s\n", __func__);
    ret = platform_device_register (&aw8737_platform_device);
    if (ret) {
        AW8737_LOGE("platform_device_register fail\n");
        return ret;
    }

    ret = platform_driver_register(&aw8737_platform_driver);
    if(ret) {
        AW8737_LOGE("platform_driver_register fail\n");
        return ret;
    }
    return ret;
}

static void __exit aw8737_exit(void)
{
    AW8737_LOGD("+%s\n", __func__);
    platform_driver_unregister(&aw8737_platform_driver);
    platform_device_unregister(&aw8737_platform_device);
}

module_init(aw8737_init);
module_exit(aw8737_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Wingtech Multimedia Group");
MODULE_DESCRIPTION("aw8737 control Driver");
