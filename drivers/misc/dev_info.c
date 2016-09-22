#include<linux/module.h>
#include<linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/device.h>

#include<linux/kdev_t.h>
#include <linux/miscdevice.h>

#define READ_LCD 0x11
#define READ_F_CAMERA 0x12
#define READ_B_CAMERA 0x13
#define READ_TP 0x14
#define READ_G_SENSOR 0x15
#define READ_E_SENSOR 0x16
#define READ_L_SENSOR 0x17
#define READ_P_SENSOR 0x18
#define READ_MCP 0x19
#define READ_GYRO 0x1A
#define READ_BATTERY 0x1B
#define READ_EMMC   0x1C //Req_135100 /dev/dev_info add emmc_info, lizheng, Add extern code, 2016/01/05
char lcd_info[40]={"NO THIS DEVICE"};
char camera_f_info[20]={"NO THIS DEVICE"};
char camera_b_info[20]={"NO THIS DEVICE"};
char tp_info[20]={"NO THIS DEVICE"};
char g_sensor_info[20]={"NO THIS DEVICE"};
char e_sensor_info[20]={"NO THIS DEVICE"};
char l_sensor_info[20]={"NO THIS DEVICE"};
char p_sensor_info[20]={"NO THIS DEVICE"};
char mcp_info[40] = {"NO THIS DEVICE"};
char gyro_sensor_info[20]={"NO THIS DEVICE"};
char battery_info[40]={"NO THIS DEVICE"};
char emmc_info[20]={"NO THIS DEVICE"}; //Req_135100 /dev/dev_info add emmc_info, lizheng, Add extern code, 2016/01/05


int myopen (struct inode *nod, struct file *fl)
{
    printk("%s\n",__FUNCTION__);
    return 0;
}
ssize_t myread (struct file *fl, char __user *user, size_t num, loff_t *loff)
{
    printk("%s\n",__FUNCTION__);
    return 0;
}
ssize_t mywrite (struct file *fl, const char __user *user, size_t num, loff_t *loff)
{

    printk("%s\n",__FUNCTION__);
    return 0;
}


long myioctl (struct file *fl, unsigned int cmd, unsigned long arg)
{

    void __user *argp = (void __user *)arg;
    int tmp;
    switch(cmd)
    {
        case READ_LCD:
        {
            tmp = copy_to_user(argp,lcd_info,sizeof(lcd_info));
            break;
        }
        case READ_F_CAMERA:
        {
            tmp = copy_to_user(argp,camera_f_info,20);
            break;
        }

        case READ_B_CAMERA:
        {
            tmp = copy_to_user(argp,camera_b_info,20);
            break;
        }

        case READ_TP:
        {
            tmp = copy_to_user(argp,tp_info,20);
            break;
        }
        case READ_G_SENSOR:
        {
            tmp = copy_to_user(argp,g_sensor_info,20);
            break;
        }
        case READ_E_SENSOR:
        {
            tmp = copy_to_user(argp,e_sensor_info,20);
            break;
        }
        case READ_P_SENSOR:
        {
            tmp = copy_to_user(argp,p_sensor_info,20);
            break;
        }
        case READ_L_SENSOR:
        {
            tmp = copy_to_user(argp,l_sensor_info,20);
            break;
        }
        case READ_MCP:
        {
            tmp = copy_to_user(argp,mcp_info,sizeof(mcp_info));
            break;
        }
        case READ_GYRO:
        {
            tmp = copy_to_user(argp,gyro_sensor_info,20);
            break;
        }
	case READ_BATTERY:
        {
            tmp = copy_to_user(argp,battery_info,sizeof(battery_info));
            break;
        }
        //Start: Req_135100 /dev/dev_info add emmc_info, lizheng, Add extern code, 2016/01/05
        case READ_EMMC:
        {
            tmp = copy_to_user(argp,emmc_info,sizeof(emmc_info));
            break;
        }
        //End: Req_135100 /dev/dev_info add emmc_info, lizheng, Add extern code, 2016/01/05
    }
    return tmp;
}

struct file_operations myfile={
    .owner = THIS_MODULE,
    .open = myopen,
    .read = myread,
    .write = mywrite,
    .unlocked_ioctl = myioctl,
};

struct miscdevice mydev = {
        .minor = 119,
        .name = "dev_info",
        .fops = &myfile,
};

int test_init(void)
{
    int ret;
    ret = misc_register(&mydev);
    if(ret < 0)
    {
        printk("register_chrdev_region error\n");
    }

    printk(KERN_ERR"hello init\n");
    return 0;
}
                
void test_exit(void)
{
    misc_deregister(&mydev);
    printk("hello exit\n");
}



module_init(test_init);
module_exit(test_exit);
MODULE_LICENSE("GPL");


