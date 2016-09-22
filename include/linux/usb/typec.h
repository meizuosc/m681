/*
 * This is used to for host and peripheral modes of the tyoec driver.
 */

#ifndef USB_TYPEC_H
#define	USB_TYPEC_H

/* ConnSide */
#define DONT_CARE	0
#define UP_SIDE		1
#define DOWN_SIDE	2

/* Stat */
#define	DISABLE		0
#define	ENABLE		1

/* DriverType */
#define DEVICE_TYPE	1
#define HOST_TYPE	2

/* USBRdCtrlPin */
#define U3_EQ_C1	0
#define U3_EQ_C2	1

/* USBRdStat */
#define U3_EQ_LOW	0
#define U3_EQ_HZ	1
#define U3_EQ_HIGH	2


struct usb3_switch {
	int sel_gpio;
	int en_gpio;
	int sel;
	int en;
};

struct usb_redriver {
	int c1_gpio;
	int c2_gpio;
	int eq_c1;
	int eq_c2;
};

struct typec_switch_data {
	char *name;
	int type;
	int on;
	int (*enable)(void *);
	int (*disable)(void *);
	void *priv_data;
};

/*
 * struct usbtypc - Driver instance data.
 */
struct usbtypc {
	int irqnum;
	int en_irq;
	spinlock_t	fsm_lock;
	struct delayed_work fsm_work;
	struct i2c_client *i2c_hd;
	struct hrtimer toggle_timer;
	struct hrtimer debounce_timer;
	struct typec_switch_data *host_driver;
	struct typec_switch_data *device_driver;
	struct usb3_switch *u3_sw;
	struct usb_redriver *u_rd;
};


extern int register_typec_switch_callback(struct typec_switch_data *new_driver);
extern int unregister_typec_switch_callback(struct typec_switch_data *new_driver);
extern int usb_redriver_config(struct usbtypc *typec, int ctrl_pin, int stat);
extern int usb_redriver_enter_dps(struct usbtypc *typec);
extern int usb_redriver_exit_dps(struct usbtypc *typec);

#endif	/* USB_TYPEC_H */
