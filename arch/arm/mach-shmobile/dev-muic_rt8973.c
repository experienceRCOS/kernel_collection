#include <linux/kernel.h>
#include <linux/module.h>
#include <mach/gpio.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/switch.h>
#include <linux/ctype.h>
#include <mach/dev-muic_rt8973.h>
#include <linux/mfd/rt8973.h>
#include <linux/spa_power.h>

#ifdef CONFIG_USB_R8A66597
extern void send_usb_insert_event(int);
#else
void send_usb_insert_event(int i) {}
#endif

static int set_cable_status = CABLE_TYPE_NONE;
static struct wake_lock rt8973_jig_wakelock;

#ifdef USE_USB_UART_SWITCH
static int usb_uart_switch_state;
char at_isi_switch_buf[1000] = {0};
int KERNEL_LOG=1;

static struct switch_dev switch_usb_uart = {
        .name = "tsu6712",
};
#endif

int get_cable_type(void)
{
	return set_cable_status;
}
EXPORT_SYMBOL(get_cable_type);

#ifdef CONFIG_TOUCHSCREEN_IST30XXB
extern void ist30xx_set_ta_mode(bool charging);
#endif

//rt8973_charger_cb
static void rt8973_charger_cb(int32_t cable_type)
{
	int spa_data = POWER_SUPPLY_TYPE_BATTERY;
	pr_info("%s charger type %d\n",__func__,cable_type);
	set_cable_status = cable_type ? CABLE_TYPE_AC : CABLE_TYPE_NONE;

	switch (cable_type) {
	case MUIC_RT8973_CABLE_TYPE_REGULAR_TA:
	case MUIC_RT8973_CABLE_TYPE_ATT_TA:
	case MUIC_RT8973_CABLE_TYPE_0x15:
	case MUIC_RT8973_CABLE_TYPE_TYPE1_CHARGER:
	case MUIC_RT8973_CABLE_TYPE_0x1A:
	case MUIC_RT8973_CABLE_TYPE_JIG_UART_OFF_WITH_VBUS:
			spa_data = POWER_SUPPLY_TYPE_USB_DCP;
#ifdef FG_TEST
			usb_uart_switch_state = CABLE_AC_ATTACHED;
			switch_set_state(&switch_usb_uart, CABLE_AC_ATTACHED);
#endif

#ifdef CONFIG_TOUCHSCREEN_IST30XXB
                       ist30xx_set_ta_mode(1);
#endif
			send_usb_insert_event(0);
			pr_info("%s TA attached\n",__func__);
		break;
	case MUIC_RT8973_CABLE_TYPE_NONE:
#ifdef FG_TEST
			usb_uart_switch_state = CABLE_AC_DETACHED;
			switch_set_state(&switch_usb_uart, CABLE_AC_DETACHED);
#endif

#ifdef CONFIG_TOUCHSCREEN_IST30XXB
                        ist30xx_set_ta_mode(0);
#endif
			pr_info("%s TA removed\n",__func__);
		break;
	default:
			set_cable_status = CABLE_TYPE_NONE;
			pr_info("%s None support\n", __func__);
	    break;
	}

#ifdef CONFIG_SEC_CHARGING_FEATURE
	spa_event_handler(SPA_EVT_CHARGER, (u32 *)spa_data);
#endif
}

//rt8973_ovp_cb
static void rt8973_ovp_cb(void)
{
	pr_info("%s\n",__func__);
#ifdef CONFIG_SEC_CHARGING_FEATURE
	spa_event_handler(SPA_EVT_OVP, (void*)1);
	spa_event_handler(SPA_EVT_CHARGER, (void*)POWER_SUPPLY_TYPE_BATTERY);
#endif
}

//rt8973_usb_cb
static void rt8973_usb_cb(uint8_t attached)
{
#if defined(CONFIG_SEC_CHARGING_FEATURE)
	int spa_data = POWER_SUPPLY_TYPE_BATTERY;
#endif
	pr_info("rt8973_usb_cb attached %d\n", attached);

	set_cable_status = attached ? CABLE_TYPE_USB : CABLE_TYPE_NONE;

#ifdef USE_USB_UART_SWITCH
		if(attached)
		{
			printk("USB attached : MUIC send switch state 100");
			usb_uart_switch_state = CABLE_USB_ATTACHED;
			switch_set_state(&switch_usb_uart, CABLE_USB_ATTACHED);
#ifdef CONFIG_TOUCHSCREEN_IST30XXB
                        ist30xx_set_ta_mode(1);
#endif
		}
		else
		{
			printk("USB detached : MUIC send switch state 101");
			usb_uart_switch_state = CABLE_USB_DETACHED;
			set_cable_status = CABLE_TYPE_NONE;
			switch_set_state(&switch_usb_uart, CABLE_USB_DETACHED);
#ifdef CONFIG_TOUCHSCREEN_IST30XXB
                        ist30xx_set_ta_mode(0);
#endif
		}
#endif

	switch (set_cable_status) {
	case CABLE_TYPE_USB:
#if defined(CONFIG_SEC_CHARGING_FEATURE)
		if(attached == 1)
			spa_data = POWER_SUPPLY_TYPE_USB;
		else if(attached == 2)
			spa_data = POWER_SUPPLY_TYPE_USB_CDP;
#endif
		spa_event_handler(SPA_EVT_CHARGER, (void*) spa_data);
		send_usb_insert_event(1);
		pr_info("%s USB attached\n",__func__);
		break;

	case CABLE_TYPE_NONE:
		spa_event_handler(SPA_EVT_CHARGER, (void*) POWER_SUPPLY_TYPE_BATTERY);
		send_usb_insert_event(0);
		set_cable_status = CABLE_TYPE_NONE;
		pr_info("%s USB removed\n",__func__);
		break;
	default:
		break;
	}
}

//rt8973_uart_cb
static void rt8973_uart_cb(uint8_t attached)
{
	pr_info("rt8973_uart_cb attached %d\n", attached);

#ifdef USE_USB_UART_SWITCH
   if(attached)
   {
      printk("UART attached : send switch state 200");
      usb_uart_switch_state = CABLE_UART_ATTACHED;
      switch_set_state(&switch_usb_uart, CABLE_UART_ATTACHED);
   }
   else
   {
      printk("UART detached : send switch state 201");
      usb_uart_switch_state = CABLE_UART_DETACHED;
      switch_set_state(&switch_usb_uart, CABLE_UART_DETACHED);
   }
#endif

#ifndef CONFIG_SEC_MAKE_LCD_TEST
	if(attached)
		wake_lock(&rt8973_jig_wakelock);
	else
		wake_unlock(&rt8973_jig_wakelock);
#endif
}

//rt8973_jig_cb
static void rt8973_jig_cb(jig_type_t type, uint8_t attached)
{
	pr_info("rt8973_jig_cb attached %d %d\n", type, attached);
}


#ifdef USE_USB_UART_SWITCH
static ssize_t rt8973_show_UUS_state(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	return snprintf(buf, 4, "%d\n", usb_uart_switch_state);
}

#define	SWITCH_AT	103
#define	SWITCH_ISI	104

/* AT-ISI Separation starts */
extern int stop_isi;
static int isi_mode; /* initialized to 0 */
char at_isi_mode[100] = {0};

static ssize_t ld_show_mode(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	strcpy(buf, at_isi_mode);
	printk("LD MODE from TSU %s\n", at_isi_mode);
	return 3;
}

static ssize_t ld_show_manualsw(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return 0;
}

ssize_t ld_set_manualsw(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	printk(KERN_CRIT"ld_set_manualsw invoked\n");
	if (0 == strncmp(buf, "AT+ATSTART", 10)) {
		printk(KERN_CRIT"switch at %d\n", SWITCH_AT);
		memset((char *)at_isi_mode, 0, 100);
		strcpy((char *)at_isi_mode, "at");
		switch_set_state(&switch_usb_uart, SWITCH_AT);
		stop_isi = 1;
		isi_mode = 0;
	}
	if (0 == strncmp(buf, "AT+MODECHAN=0,2", 15)) {
		printk(KERN_CRIT"modechan 0,2 %d\n", SWITCH_MODECHAN_02);
		memset((char *)at_isi_mode, 0, 100);
		strcpy((char *)at_isi_mode, "at");
		switch_set_state(&switch_usb_uart, SWITCH_MODECHAN_02);

		stop_isi = 1;
		isi_mode = 0;
	}

	if (0 == strncmp(buf, "AT+ISISTART", 11)) {
		printk(KERN_CRIT"switch isi %d\n", SWITCH_ISI);
		memset((char *)at_isi_mode, 0, 100);
		strcpy((char *)at_isi_mode, "isi");
		switch_set_state(&switch_usb_uart, SWITCH_ISI);
		stop_isi = 0;
		isi_mode = 1;
	}

	if (0 == strncmp(buf, "AT+MODECHAN=0,0", 15)) {
		printk(KERN_CRIT"modechan 0,0 %d\n", SWITCH_MODECHAN_00);
		memset((char *)at_isi_mode, 0, 100);
		strcpy((char *)at_isi_mode, "isi");
		switch_set_state(&switch_usb_uart, SWITCH_MODECHAN_00);
		stop_isi = 0;
		isi_mode = 1;
	}
	return count;
}

static ssize_t ld_show_switch_buf(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	strcpy(buf, at_isi_switch_buf);
	printk("BUF from TSU %s\n", at_isi_switch_buf);
	return strlen(at_isi_switch_buf);
}

ssize_t ld_set_switch_buf(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int i;
	char temp[100];
	char *ptr = NULL;
	char *ptr2 = NULL;
	char atbuf[] = "AT+ATSTART\r";
	char atmodechanbuf[] = "AT+MODECHAN=0,2\r";
	char isi_cmd_buf[] = "switch isi";
	int error = 0;

	/* If UART is not connected ignore this sysfs access*/
	if (200 != usb_uart_switch_state)
		return 0;

	memset(temp, 0, 100);
	for (i = 0; i < count; i++)
		temp[i] = toupper(buf[i]);

	/* This will clear any partial commands due to intermediate inputs */
	if ((!strcmp("AT+MODECHAN=0, 0", buf)) \
			 || (!strcmp("AT+ISISTART", buf))) {
		memset(at_isi_switch_buf, 0, 400);
	}

	strncat((char *)at_isi_switch_buf, temp, count);

	if ((strncmp((char *)at_isi_switch_buf, "\n", 1) == 0) || \
		(strncmp((char *)at_isi_switch_buf, "\r", 1) == 0) || \
		(strncmp((char *)at_isi_switch_buf, "\r\n", 2) == 0)) {
		memset(at_isi_switch_buf, 0, 400);
		KERNEL_LOG = 0;
		return UART_EMPTY_CRLF;
	}

	if (strstr(at_isi_switch_buf, "\r\n"))
		printk("###TEST0### r n\n");
	else if (strstr(at_isi_switch_buf, "\t\n"))
		printk("###TEST1### t n\n");
	else if (strstr(at_isi_switch_buf, "\n"))
		printk("###TEST2### n\n");

	ptr = strstr(atbuf, at_isi_switch_buf);
	ptr2 = strstr(atmodechanbuf, at_isi_switch_buf);
	if ( ((NULL == ptr) || (ptr != atbuf)) &&
		 ((NULL == ptr2) || (ptr2 != atmodechanbuf)) ) {
		if (strstr("AT+ISISTART", at_isi_switch_buf) == NULL &&
			strstr("AT+MODECHAN=0,0", at_isi_switch_buf) == NULL)
			error = 1;
	}

	if (strstr(at_isi_switch_buf, atbuf) != NULL) {
		KERNEL_LOG = 0;
		memset(at_isi_switch_buf, 0, 400);
		return UART_AT_MODE;
	} else if (strstr(at_isi_switch_buf, atmodechanbuf) != NULL) {
		KERNEL_LOG = 0;
		memset(at_isi_switch_buf, 0, 400);
		return UART_AT_MODE_MODECHAN;
	} else if (strstr(at_isi_switch_buf, "AT+ISISTART") != NULL ||
		   strstr(at_isi_switch_buf, "AT+MODECHAN=0,0") != NULL) {
		/*do not switch to isi mode if isi mode already set*/
		if (isi_mode == 0) {
			KERNEL_LOG = 0;

			ld_set_manualsw(NULL, NULL, at_isi_switch_buf,
				strlen(at_isi_switch_buf));
			memset(at_isi_switch_buf, 0, 400);
			return count;
		}
	}
	/* this sends response if at+isistart is given in isi mode */
	if (strstr(at_isi_switch_buf, "AT+ISISTART\r") != NULL ||
		strstr(at_isi_switch_buf, "AT+MODECHAN=0,0\r") != NULL) {
		memset(at_isi_switch_buf, 0, 400);
		ld_set_manualsw(NULL, NULL, isi_cmd_buf, strlen(isi_cmd_buf));
		return UART_INVALID_MODE;
	}
	if (error != 0) {
		count = UART_INVALID_MODE;
		memset(at_isi_switch_buf, 0, 400);
	}
	return count;
}
#endif

/* JIRA ID 1362/1396
Sysfs interface to release and acquire
uart-wakelock from user space */
ssize_t ld_uart_wakelock(struct device *dev,
                                    struct device_attribute *attr,
                                    const char *buf, size_t count)
{
	int ret;
	int buf_val = 0;

	ret = sscanf(buf, "%d", &buf_val);
	if (1 != ret) {
	      printk(KERN_ERR \
	              "ld_uart_wakelock - Failed to read value\n");
	      return -EINVAL;
	}
	if (buf_val == 1) {
	      /* Release wakelock
	      to allow device to get into deep sleep
	      when UART JIG is disconnected */
	      wake_unlock(&rt8973_jig_wakelock);
	      ret = count ;
	} else if (buf_val == 0) {
	      /* Acquire wakelock
	      to avoid device getting into deep sleep
	      when UART JIG is connected*/
	      wake_lock(&rt8973_jig_wakelock);
	      ret = count ;
	}

	return ret ;
}


#ifdef USE_USB_UART_SWITCH
/* AT-ISI Separation starts */
static DEVICE_ATTR(at_isi_switch, S_IRUGO | S_IWUSR, ld_show_manualsw, ld_set_manualsw);
static DEVICE_ATTR(at_isi_mode, S_IRUGO | S_IWUSR, ld_show_mode, NULL);
static DEVICE_ATTR(at_isi_switch_buf, S_IRUGO | S_IWUSR,	ld_show_switch_buf, ld_set_switch_buf);
/* AT-ISI Separation Ends */
static DEVICE_ATTR(UUS_state, S_IRUGO, rt8973_show_UUS_state, NULL);
#endif

static DEVICE_ATTR(uart_wakelock, S_IRUGO | S_IWUSR, NULL, ld_uart_wakelock);

static struct attribute *rt8973_attributes[] = {
#ifdef USE_USB_UART_SWITCH
	&dev_attr_at_isi_switch.attr,	/* AT-ISI Separation */
	&dev_attr_at_isi_mode.attr,		/* AT-ISI Separation */
	&dev_attr_at_isi_switch_buf.attr,	/* AT-ISI Separation */
	&dev_attr_UUS_state.attr,
#endif
	/* JIRA ID 1362/1396
	uart-wakelock release */
	&dev_attr_uart_wakelock.attr,
	NULL
};

int get_kernel_log_status(void)
{
	return KERNEL_LOG;
}
EXPORT_SYMBOL(get_kernel_log_status);

static struct kobject *usb_kobj;
#define USB_FS	"usb_atparser"
static const struct attribute_group rt8973_group = {
        .attrs = rt8973_attributes,
};

static int usb_sysfs_init(void)
{
 	int ret;
 	usb_kobj = kobject_create_and_add(USB_FS, kernel_kobj);
 	if (!usb_kobj)
 		return 0;
 	ret = sysfs_create_group(usb_kobj, &rt8973_group);
 	if (ret)
 		kobject_put(usb_kobj);
 	return ret;
}

void jig_force_sleep(void)
{
#ifdef CONFIG_HAS_WAKELOCK
	if (wake_lock_active(&rt8973_jig_wakelock)) {
		wake_unlock(&rt8973_jig_wakelock);
		pr_info("Force unlock jig_uart_wl\n");
	}
#else
	pr_info("Warning : %s - Empty function!!!\n", __func__);
#endif
}

static void rt8973_wakelock_init(void)
{
    wake_lock_init(&rt8973_jig_wakelock, WAKE_LOCK_SUSPEND,
    "rt8973_jig_wakelock");
}

#ifdef CONFIG_MFD_RT8973
static struct rt8973_platform_data  rt8973_pdata = {
    .irq_gpio = 97,
    .cable_chg_callback = rt8973_charger_cb,
    .ocp_callback = NULL,
    .otp_callback = NULL,
    .ovp_callback = rt8973_ovp_cb,
    .usb_callback = rt8973_usb_cb,
    .uart_callback = rt8973_uart_cb,
    .otg_callback = NULL,
    .jig_callback = rt8973_jig_cb,
};

#define MUSB_I2C_BUS_ID 3
/* For you device setting */
static struct i2c_board_info __initdata micro_usb_i2c_devices_info[]  = {
/* Add for Ricktek RT8969 */
	{I2C_BOARD_INFO("rt8973", 0x28>>1),
	.platform_data = &rt8973_pdata,},
};

void __init dev_muic_rt8973_init(void)
{
	int ret;
	pr_info("rt8973: micro_usb_i2c_devices_info\n");
	rt8973_wakelock_init();

	gpio_pull_off_port(GPIO_PORT97);

	i2c_register_board_info(MUSB_I2C_BUS_ID, micro_usb_i2c_devices_info,
		ARRAY_SIZE(micro_usb_i2c_devices_info));

#ifdef USE_USB_UART_SWITCH
	/* for usb uart switch */
	ret = switch_dev_register(&switch_usb_uart);
	if (ret < 0) {
		pr_err("Failed to register dock switch. %d\n", ret);
		return ;
	}
	usb_sysfs_init();
#endif
}
#endif /*CONFIG_MFD_RT8973*/
