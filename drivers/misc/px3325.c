/*
 * This file is part of the PX3325 sensor driver.
 * PX3325 is combined proximity sensor and IRLED.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 *
 * Filename: px3325.c
 *
 * Summary:
 *	PX3325 sensor dirver.
 *
 * Modification History:
 * Date     By       Summary
 * -------- -------- -------------------------------------------------------
 * 07/10/13 YC       Original Creation (version:1.00 base on datasheet rev0.50)
 * 01/02/14 YC       Set default settings to 2X, 25T for recommend settings. 
 *                   Ver 1.01.
 * 01/03/14 YC       Modify to support devicetree. Change to ver 1.02.
 * 01/23/14 YC       Change the interrupt clean method to manual. 
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/string.h>
#include <linux/irq.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>

#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/of_i2c.h>

#include <linux/px3325.h>
#include <linux/sensors_core.h>

#define PX3325_DRV_NAME	"px3325"

#define DRIVER_VERSION		"1.03"

#define PX3325_NUM_CACHABLE_REGS	19

#define PX3325_MODE_COMMAND	0x00
#define PX3325_MODE_SHIFT	(0)
#define PX3325_MODE_MASK	0x07

#define PX3325_PX_INT_CLEAN 0x02
#define PX3325_PX_INT_CLEAN_MASK 0x01
#define PX3325_PX_INT_CLEAN_SHIFT 0x00

#define PX3325_PX_LTHL			0x2a
#define PX3325_PX_LTHL_SHIFT	(0)
#define PX3325_PX_LTHL_MASK		0xff

#define PX3325_PX_LTHH			0x2b
#define PX3325_PX_LTHH_SHIFT	(0)
#define PX3325_PX_LTHH_MASK		0x03

#define PX3325_PX_HTHL			0x2c
#define PX3325_PX_HTHL_SHIFT	(0)
#define PX3325_PX_HTHL_MASK		0xff

#define PX3325_PX_HTHH			0x2d
#define PX3325_PX_HTHH_SHIFT	(0)
#define PX3325_PX_HTHH_MASK		0x03

#define PX3325_OBJ_COMMAND	0x01
#define PX3325_OBJ_MASK		0x10
#define PX3325_OBJ_SHIFT	(4)

#define PX3325_INT_COMMAND	0x01
#define PX3325_INT_SHIFT	(0)
#define PX3325_INT_MASK		0x02
#define PX3325_INT_PMASK	0x02

#define PX3325_PX_LSB		0x0e
#define PX3325_PX_MSB		0x0f
#define PX3325_PX_LSB_MASK	0xff
#define PX3325_PX_MSB_MASK	0x03

#define PX3325_PX_CALI_L	0x28
#define PX3325_PX_CALI_L_MASK   0xff
#define PX3325_PX_CALI_L_SHIFT	(0)

#define PX3325_PX_CALI_H	0x29
#define PX3325_PX_CALI_H_MASK	0x01
#define PX3325_PX_CALI_H_SHIFT	(0)

#define PX3325_PX_GAIN	0x20
#define PX3325_PX_GAIN_MASK	0x0c
#define PX3325_PX_GAIN_SHIFT	(2)

#define PX3325_PX_INTEGRATE	0x25
#define PX3325_PX_INTEGRATE_MASK	0x3f
#define PX3325_PX_INTEGRATE_SHIFT	(0)

#define PS_ACTIVE    0x02

#define PROX_READ_NUM 10
#define PROX_ADC_MIN        50
#define PROX_ADC_MAX        1023
#define PROX_DEFAULT_ADC 50
#define OFFSET_ARRAY_LENGTH		10

#define PX3325_PX_DEFAULT_THREH     625
#define PX3325_PX_DEFAULT_THREL     440
#define PX3325_PX_INTEGRATED_TIME    0x35  //5.0+53x0.0627=8.3231ms

/*----------------------------------------------------------------------------*/
#define AUTO_CLEAN_INT  1
#define SOFT_CLEAN_INT  2
/*----------------------------------------------------------------------------*/
#define PX3325_INT_CLEAN_METHOD AUTO_CLEAN_INT  // define the clean method here.
/*----------------------------------------------------------------------------*/

/*============================================================================*/
#define DI_DBG
/*----------------------------------------------------------------------------*/
#ifdef DI_DBG
#define PX_TAG                  "[PX3325] "
#define PX_FUN(f)               printk(KERN_ERR PX_TAG"<%s>\n", __FUNCTION__)
#define PX_ERR(fmt, args...)    printk(KERN_ERR PX_TAG"<%s> %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define PX_LOG(fmt, args...)    printk(KERN_ERR PX_TAG fmt, ##args)
#define PX_DBG(fmt, args...)    printk(KERN_ERR PX_TAG fmt, ##args)                
#else
#define PX_FUN(f)               {}
#define PX_ERR(fmt, args...)    {}
#define PX_LOG(fmt, args...)    {}
#define PX_DBG(fmt, args...)    {}        
#endif
/*============================================================================*/

struct px3325_data {
	void (*power_on) (bool);
	void (*led_on) (bool);
	unsigned int irq_gpio;
	struct i2c_client *client;
	struct mutex lock;
	u8 reg_cache[PX3325_NUM_CACHABLE_REGS];
	int irq;

	struct workqueue_struct *wq;
	struct input_dev *proximity_input_dev;
	struct work_struct work_proximity;      
	ktime_t proximity_poll_delay;
	struct wake_lock prx_wake_lock;
    
	struct device *proximity_sensor_device;
    	int avg[3];
	int offset_value;
	int cal_result;
    	int threshold_high;
	int average[PROX_READ_NUM];	/*for proximity adc average */    
};

// PX3325 register
static u8 px3325_reg[PX3325_NUM_CACHABLE_REGS] = 
	{0x00,0x01,0x02,0x06,0x0e,0x0f,
	 0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x28,0x29,0x2a,0x2b,0x2c,0x2d};

#define ADD_TO_IDX(addr,idx)	{														\
									int i;												\
									for(i = 0; i < PX3325_NUM_CACHABLE_REGS; i++)						\
									{													\
										if (addr == px3325_reg[i])						\
										{												\
											idx = i;									\
											break;										\
										}												\
									}													\
								}
 


/*
 * register access helpers
 */

static int __px3325_read_reg(struct i2c_client *client,
			       u32 reg, u8 mask, u8 shift)
{
	struct px3325_data *data = i2c_get_clientdata(client);
	u8 idx = 0xff;

	ADD_TO_IDX(reg,idx)
	return (data->reg_cache[idx] & mask) >> shift;
}

static int __px3325_write_reg(struct i2c_client *client,
				u32 reg, u8 mask, u8 shift, u8 val)
{
	struct px3325_data *data = i2c_get_clientdata(client);
	int ret = 0;
	u8 tmp;
	u8 idx = 0xff;

	ADD_TO_IDX(reg,idx)
	if (idx >= PX3325_NUM_CACHABLE_REGS)
		return -EINVAL;

	mutex_lock(&data->lock);

	tmp = data->reg_cache[idx];
	tmp &= ~mask;
	tmp |= val << shift;

	ret = i2c_smbus_write_byte_data(client, reg, tmp);
	if (!ret)
		data->reg_cache[idx] = tmp;

	mutex_unlock(&data->lock);
	return ret;
}

/*
 * internally used functions
 */

/* mode */
static int px3325_get_mode(struct i2c_client *client)
{
	int ret;

	ret = __px3325_read_reg(client, PX3325_MODE_COMMAND,
			PX3325_MODE_MASK, PX3325_MODE_SHIFT);
	return ret;
}

static int px3325_set_mode(struct i2c_client *client, int mode)
{
	//struct px3325_data *data = i2c_get_clientdata(client);
	
	if (mode != px3325_get_mode(client))
	{
                PX_LOG("px3325_set_mode: mode=%d\n", mode);
                
		__px3325_write_reg(client, PX3325_MODE_COMMAND,
			  PX3325_MODE_MASK, PX3325_MODE_SHIFT, mode);

		/* Enable/Disable PS */        
		/*if (PS_ACTIVE & mode){
			wake_lock(&data->prx_wake_lock);
		}
		else {
			wake_unlock(&data->prx_wake_lock);
		}*/
	}
	return 0;
}

/* PX interrupt clean method */
static int px3325_set_int_clean(struct i2c_client *client, u8 val)
{
    int err;
    err = __px3325_write_reg(client, PX3325_PX_INT_CLEAN,
		PX3325_PX_INT_CLEAN_MASK, PX3325_PX_INT_CLEAN_SHIFT, val);
	if (err)
		return err;
    return 0;
}

static int px3325_int_clean(struct i2c_client *client)
{
    int err;
    err = __px3325_write_reg(client, PX3325_INT_COMMAND,
		0xff, 0, 0);
	if (err)
		return err;
    return 0;
}

/* PX low threshold */
static int px3325_get_plthres(struct i2c_client *client)
{
	int lsb, msb;
	lsb = __px3325_read_reg(client, PX3325_PX_LTHL,
				PX3325_PX_LTHL_MASK, PX3325_PX_LTHL_SHIFT);
	msb = __px3325_read_reg(client, PX3325_PX_LTHH,
				PX3325_PX_LTHH_MASK, PX3325_PX_LTHH_SHIFT);
	return ((msb << 8) | lsb);
}

static int px3325_set_plthres(struct i2c_client *client, int val)
{
	int lsb, msb, err;
	
	msb = (val >> 8) & PX3325_PX_LTHH_MASK;
	lsb = val & PX3325_PX_LTHL_MASK;
	
	err = __px3325_write_reg(client, PX3325_PX_LTHL,
		PX3325_PX_LTHL_MASK, PX3325_PX_LTHL_SHIFT, lsb);
	if (err)
		return err;

	err = __px3325_write_reg(client, PX3325_PX_LTHH,
		PX3325_PX_LTHH_MASK, PX3325_PX_LTHH_SHIFT, msb);

	return err;
}

/* PX high threshold */
static int px3325_get_phthres(struct i2c_client *client)
{
	int lsb, msb;
	lsb = __px3325_read_reg(client, PX3325_PX_HTHL,
				PX3325_PX_HTHL_MASK, PX3325_PX_HTHL_SHIFT);
	msb = __px3325_read_reg(client, PX3325_PX_HTHH,
				PX3325_PX_HTHH_MASK, PX3325_PX_HTHH_SHIFT);
	return ((msb << 8) | lsb);
}

static int px3325_set_phthres(struct i2c_client *client, int val)
{
	int lsb, msb, err;
	
	msb = (val >> 8) & PX3325_PX_HTHH_MASK ;
	lsb = val & PX3325_PX_HTHL_MASK;
	
	err = __px3325_write_reg(client, PX3325_PX_HTHL,
		PX3325_PX_HTHL_MASK, PX3325_PX_HTHL_SHIFT, lsb);
	if (err)
		return err;

	err = __px3325_write_reg(client, PX3325_PX_HTHH,
		PX3325_PX_HTHH_MASK, PX3325_PX_HTHH_SHIFT, msb);

	return err;
}

/* object */
static int px3325_get_object(struct i2c_client *client)
{
	return __px3325_read_reg(client, PX3325_OBJ_COMMAND,
				PX3325_OBJ_MASK, PX3325_OBJ_SHIFT);
}

#if 0   // YC, skip checking interrupt status.
/* int status */
static int px3325_get_intstat(struct i2c_client *client)
{
	int val;
	
	val = i2c_smbus_read_byte_data(client, PX3325_INT_COMMAND);
	val &= PX3325_INT_MASK;

	return val >> PX3325_INT_SHIFT;
}
#endif

/* PX value */
static int px3325_get_px_value(struct i2c_client *client, int lock)
{
	struct px3325_data *data = i2c_get_clientdata(client);
	u8 lsb, msb;
	u16 raw;

	if (!lock) mutex_lock(&data->lock);
	
	lsb = i2c_smbus_read_byte_data(client, PX3325_PX_LSB);
	msb = i2c_smbus_read_byte_data(client, PX3325_PX_MSB);

	if (!lock) mutex_unlock(&data->lock);

	raw = (msb & 0x03) * 256 + lsb;

	return raw;
}

/* calibration */
static int px3325_get_calibration(struct i2c_client *client)
{
	int lsb, msb;
	lsb = __px3325_read_reg(client, PX3325_PX_CALI_L,
				PX3325_PX_CALI_L_MASK, PX3325_PX_CALI_L_SHIFT);
	msb = __px3325_read_reg(client, PX3325_PX_CALI_H,
				PX3325_PX_CALI_H_MASK, PX3325_PX_CALI_H_MASK);
	return ((msb << 8) | lsb);
}

static int px3325_set_calibration(struct i2c_client *client, int val)
{
	int lsb, msb, err;
	
	msb = (val >> 8) & PX3325_PX_CALI_H_MASK ;
	lsb = val & PX3325_PX_CALI_L_MASK;
	
	err = __px3325_write_reg(client, PX3325_PX_CALI_L,
		PX3325_PX_CALI_L_MASK, PX3325_PX_CALI_L_SHIFT, lsb);
	if (err)
		return err;

	err = __px3325_write_reg(client, PX3325_PX_CALI_H,
		PX3325_PX_CALI_H_MASK, PX3325_PX_CALI_H_MASK, msb);

	return err;
}

/* gain */
static int px3325_get_gain(struct i2c_client *client)
{
	int gain;
	gain = __px3325_read_reg(client, PX3325_PX_GAIN,
				PX3325_PX_GAIN_MASK, PX3325_PX_GAIN_SHIFT);
	return gain;
}

static int px3325_set_gain(struct i2c_client *client, int val)
{
	int err;
	
	err = __px3325_write_reg(client, PX3325_PX_GAIN,
		PX3325_PX_GAIN_MASK, PX3325_PX_GAIN_SHIFT, val);

	return err;
}

/* integrated time */
static int px3325_get_integrate(struct i2c_client *client)
{
	int inte;
	inte = __px3325_read_reg(client, PX3325_PX_INTEGRATE,
				PX3325_PX_INTEGRATE_MASK, PX3325_PX_INTEGRATE_SHIFT);
	return inte;
}

static int px3325_set_integrate(struct i2c_client *client, int val)
{
	int err;
	
	err = __px3325_write_reg(client, PX3325_PX_INTEGRATE,
		PX3325_PX_INTEGRATE_MASK, PX3325_PX_INTEGRATE_SHIFT, val);

	return err;
}

/*
 * sysfs layer
 */

/* mode */
static ssize_t px3325_show_mode(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct px3325_data *data = input_get_drvdata(input);
	return sprintf(buf, "%d\n", px3325_get_mode(data->client));
}

static ssize_t px3325_store_mode(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct px3325_data *data = input_get_drvdata(input);
	unsigned long val;
	int ret;

	if ((strict_strtoul(buf, 10, &val) < 0) || (val > 6))
		return -EINVAL;

	ret = px3325_set_mode(data->client, val);
	
	if (ret < 0)
		return ret;
	return count;
}

static DEVICE_ATTR(mode,  S_IRUGO | S_IWUSR | S_IWGRP,
		   px3325_show_mode, px3325_store_mode);

/* enable_ps_sensor */
static ssize_t proximity_enable_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct px3325_data *data = input_get_drvdata(input);
	return sprintf(buf, "%d\n", px3325_get_mode(data->client)&0x02);
}

static ssize_t proximity_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
        struct input_dev *input = to_input_dev(dev);
        struct px3325_data *data = input_get_drvdata(input);
        bool new_value;

        if (sysfs_streq(buf, "1"))
            new_value = true;
        else if (sysfs_streq(buf, "0"))
            new_value = false;
        else {
            pr_err("%s: invalid value %d\n", __func__, *buf);
            return -EINVAL;
        }

        PX_LOG("proximity_enable_store: new_value=%d offset=%d\n", new_value, data->offset_value);

        if (new_value ){            
            input_report_abs(data->proximity_input_dev, ABS_DISTANCE, 1);
            input_sync(data->proximity_input_dev);

            if (data->led_on){
                data->led_on(1);
            }            
            px3325_set_mode(data->client, 2);
        }
        else if (!new_value ) {            
            px3325_set_mode(data->client, 0);

            if (data->led_on){
                data->led_on(0);
            }            
        }
    
        return count;    
}

static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
		   proximity_enable_show, proximity_enable_store);
           
/* ps_poll_delay */
static ssize_t px3325_show_poll_delay(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct px3325_data *data = input_get_drvdata(input);
	return sprintf(buf, "%d\n", (int)ktime_to_ns(data->proximity_poll_delay)/1000);
}

static ssize_t px3325_store_poll_delay(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct px3325_data *data = input_get_drvdata(input);
	unsigned long val;

	if (strict_strtoul(buf, 10, &val) < 0)
		return -EINVAL;
    
	data->proximity_poll_delay = ns_to_ktime(val*1000);
	return count;
}

static DEVICE_ATTR(poll_delay, S_IWUSR | S_IRUGO,
		   px3325_show_poll_delay, px3325_store_poll_delay);

/* Px data */
static ssize_t px3325_show_pxvalue(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct px3325_data *data = input_get_drvdata(input);

	/* No Px data if non active */
	if (px3325_get_mode(data->client) != PS_ACTIVE)
		return -EBUSY;

	return sprintf(buf, "%d\n", px3325_get_px_value(data->client,0));
}

static DEVICE_ATTR(pxvalue, S_IRUGO, px3325_show_pxvalue, NULL);

/* proximity object detect */
static ssize_t px3325_show_object(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct px3325_data *data = input_get_drvdata(input);
	return sprintf(buf, "%d\n", px3325_get_object(data->client));
}

static DEVICE_ATTR(object, S_IRUGO, px3325_show_object, NULL);

/* Px low threshold */
static ssize_t px3325_show_plthres(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct px3325_data *data = input_get_drvdata(input);
	return sprintf(buf, "%d\n", px3325_get_plthres(data->client));
}

static ssize_t px3325_store_plthres(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct px3325_data *data = input_get_drvdata(input);
	unsigned long val;
	int ret;

	if (strict_strtoul(buf, 10, &val) < 0)
		return -EINVAL;

	ret = px3325_set_plthres(data->client, val);
	if (ret < 0)
		return ret;

	return count;
}

static DEVICE_ATTR(plthres, S_IWUSR | S_IRUGO,
		   px3325_show_plthres, px3325_store_plthres);

/* Px high threshold */
static ssize_t px3325_show_phthres(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct px3325_data *data = input_get_drvdata(input);
	return sprintf(buf, "%d\n", px3325_get_phthres(data->client));
}

static ssize_t px3325_store_phthres(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct px3325_data *data = input_get_drvdata(input);
	unsigned long val;
	int ret;

	if (strict_strtoul(buf, 10, &val) < 0)
		return -EINVAL;

	ret = px3325_set_phthres(data->client, val);
	if (ret < 0)
		return ret;

	return count;
}

static DEVICE_ATTR(phthres, S_IWUSR | S_IRUGO,
		   px3325_show_phthres, px3325_store_phthres);


/* calibration */
static ssize_t px3325_show_calibration_state(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct px3325_data *data = input_get_drvdata(input);
	return sprintf(buf, "%d\n", px3325_get_calibration(data->client));
}

static ssize_t px3325_store_calibration_state(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct px3325_data *data = input_get_drvdata(input);
	unsigned long val;
	int ret;

	if (strict_strtoul(buf, 10, &val) < 0)
		return -EINVAL;

	ret = px3325_set_calibration(data->client, val);
	if (ret < 0)
		return ret;

	return count;
}

static DEVICE_ATTR(calibration, S_IWUSR | S_IRUGO,
		   px3325_show_calibration_state, px3325_store_calibration_state);

/* gain */
static ssize_t px3325_show_gain_state(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct px3325_data *data = input_get_drvdata(input);
	return sprintf(buf, "%d\n", px3325_get_gain(data->client));
}

static ssize_t px3325_store_gain_state(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct px3325_data *data = input_get_drvdata(input);
	unsigned long val;
	int ret;

	if (strict_strtoul(buf, 10, &val) < 0)
		return -EINVAL;

	ret = px3325_set_gain(data->client, val);
	if (ret < 0)
		return ret;

	return count;
}

static DEVICE_ATTR(gain, S_IWUSR | S_IRUGO,
		   px3325_show_gain_state, px3325_store_gain_state);

/* integrated time */
static ssize_t px3325_show_integrate_state(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct px3325_data *data = input_get_drvdata(input);
	return sprintf(buf, "%d\n", px3325_get_integrate(data->client));
}

static ssize_t px3325_store_integrate_state(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct px3325_data *data = input_get_drvdata(input);
	unsigned long val;
	int ret;

	if (strict_strtoul(buf, 10, &val) < 0)
		return -EINVAL;

	ret = px3325_set_integrate(data->client, val);
	if (ret < 0)
		return ret;

	return count;
}

static DEVICE_ATTR(integrate, S_IWUSR | S_IRUGO,
		   px3325_show_integrate_state, px3325_store_integrate_state);

#ifdef DI_DBG
/* engineer mode */
static ssize_t px3325_em_read(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct px3325_data *data = i2c_get_clientdata(client);
	int i;
	u8 tmp;
	
	for (i = 0; i < PX3325_NUM_CACHABLE_REGS; i++)
	{
		mutex_lock(&data->lock);
		tmp = i2c_smbus_read_byte_data(data->client, px3325_reg[i]);
		mutex_unlock(&data->lock);

		printk("Reg[0x%x] Val[0x%x]\n", px3325_reg[i], tmp);
	}

	return 0;
}

static ssize_t px3325_em_write(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct px3325_data *data = i2c_get_clientdata(client);
	u32 addr,val,idx=0;
	int ret = 0;

	sscanf(buf, "%x%x", &addr, &val);

	printk("Write [%x] to Reg[%x]...\n",val,addr);
	mutex_lock(&data->lock);

	ret = i2c_smbus_write_byte_data(data->client, addr, val);
	ADD_TO_IDX(addr,idx)
	if (!ret)
		data->reg_cache[idx] = val;

	mutex_unlock(&data->lock);

	return count;
}
static DEVICE_ATTR(em, S_IWUSR |S_IRUGO,
				   px3325_em_read, px3325_em_write);
#endif

static struct attribute *px3325_ps_attributes[] = {
	&dev_attr_mode.attr,
	&dev_attr_enable.attr,
	&dev_attr_poll_delay.attr,
	&dev_attr_object.attr,
	&dev_attr_pxvalue.attr,
	&dev_attr_plthres.attr,
	&dev_attr_phthres.attr,
	&dev_attr_calibration.attr,
	&dev_attr_gain.attr,
	&dev_attr_integrate.attr,
#ifdef DI_DBG
	&dev_attr_em.attr,
#endif
	NULL
};

static const struct attribute_group px3325_ps_attr_group = {
	.attrs = px3325_ps_attributes,
};

/*============================================================================*/

static ssize_t proximity_state_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct px3325_data *data = dev_get_drvdata(dev);	
	static int count;		/*count for proximity average */
	int adc = 0;
    
	adc = px3325_get_px_value(data->client,1);
	data->average[count] = adc;
	count++;
	if (count == PROX_READ_NUM)
            count = 0;
    
	return sprintf(buf, "%d\n", adc);
}
static struct device_attribute dev_attr_proximity_sensor_state =
	__ATTR(state, S_IRUSR | S_IRGRP, proximity_state_show, NULL);


static ssize_t proximity_avg_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct px3325_data *data = dev_get_drvdata(dev);    
	int min = 0, max = 0, avg = 0;
	int i;

	for (i = 0; i < PROX_READ_NUM; i++) {
        	int proximity_value = 0;
		proximity_value = data->average[i];
		if (proximity_value > 0) {

			avg += proximity_value;

			if (!i)
				min = proximity_value;
			else if (proximity_value < min)
				min = proximity_value;

			if (proximity_value > max)
				max = proximity_value;
		}
	}
	avg /= i;

	return snprintf(buf, PAGE_SIZE, "%d, %d, %d\n", min, avg, max);
    
}
static struct device_attribute dev_attr_proximity_sensor_prox_avg =
	__ATTR(prox_avg, S_IRUSR | S_IRGRP,	proximity_avg_show, NULL);


static int px3325_open_calibration(struct px3325_data  *data)
{
	struct file *cal_filp = NULL;
	int err = 0;
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open("/efs/prox_cal", O_RDONLY, 0660);
	if (IS_ERR(cal_filp)) {
		pr_err("%s: Can't open calibration file\n", __func__);
		set_fs(old_fs);
		err = PTR_ERR(cal_filp);
		goto done;
	}

	err = cal_filp->f_op->read(cal_filp,
		(char *)&data->offset_value,
			sizeof(int), &cal_filp->f_pos);
	if (err != sizeof(int)) {
		pr_err("%s: Can't read the cal data from file\n", __func__);
		err = -EIO;
	}

	pr_info("%s: (%d)\n", __func__, data->offset_value);

	filp_close(cal_filp, current->files);

        /* update offest */
        if(data->offset_value >= PROX_ADC_MIN 
            && data->offset_value <= PROX_ADC_MAX){
            px3325_set_calibration(data->client, data->offset_value);
            px3325_set_plthres(data->client, data->offset_value);
            px3325_set_phthres(data->client, data->offset_value);
        }
    
done:
	set_fs(old_fs);
	return err;
}

static ssize_t proximity_cal_open_show(struct device *dev,
                 struct device_attribute *attr, char *buf)
{
    	struct px3325_data *data = dev_get_drvdata(dev);
        int err;
	err=px3325_open_calibration(data);
	return sprintf(buf, "%d\n", err);
}
static struct device_attribute dev_attr_proximity_sensor_prox_cal_open =
	__ATTR(prox_cal_open, S_IRUSR | S_IRGRP,	proximity_cal_open_show, NULL);


static int proximity_adc_read(struct px3325_data *data)
{
	int sum[OFFSET_ARRAY_LENGTH];
	int i = OFFSET_ARRAY_LENGTH-1;
	int avg;
	int min;
	int max;
	int total=0;

	do {
		msleep(50);
		sum[i] = px3325_get_px_value(data->client,1);
		if (i == 0) {
			min = sum[i];
			max = sum[i];
		} else {
			if (sum[i] < min)
				min = sum[i];
			else if (sum[i] > max)
				max = sum[i];
		}
		total += sum[i];
	} while (i--);

	total -= (min + max);
	avg = (total / (OFFSET_ARRAY_LENGTH - 2));

	return avg;
}

static int proximity_do_calibrate(struct px3325_data  *data,
			bool do_calib, bool thresh_set)
{
	struct file *cal_filp;
	int err;
	int xtalk_avg = 0;
	mm_segment_t old_fs;

	if (do_calib) {
		if (thresh_set) {
			/* for proximity_thresh_store */
                        if(data->threshold_high > PROX_ADC_MAX){
                            data->threshold_high = PROX_ADC_MAX;
                        } else if(data->threshold_high < PROX_ADC_MIN){
                            data->threshold_high = PROX_ADC_MIN;
                        }
			data->offset_value = data->threshold_high;
		} else {
			/* tap offset button */
			/* get offset value */
			xtalk_avg = proximity_adc_read(data);
			if (xtalk_avg < PROX_DEFAULT_ADC) {
			/* do not need calibration */
				data->cal_result = 0;
				err = 0;
				goto no_cal;
			}
			data->offset_value = xtalk_avg;// - PX_PROX_DEFAULT_ADC;
		}
		/* update offest */
		px3325_set_calibration(data->client, data->offset_value);
		
		px3325_set_plthres(data->client, data->offset_value);        
		px3325_set_phthres(data->client, data->offset_value);
        
		/* calibration result */
		data->cal_result = 1;
	} else {
		/* tap reset button */
		data->offset_value = 0;
		/* update offest */
		px3325_set_calibration(data->client, data->offset_value);

		px3325_set_plthres(data->client, PX3325_PX_DEFAULT_THREL);
		px3325_set_phthres(data->client, PX3325_PX_DEFAULT_THREH);
		/* calibration result */
		data->cal_result = 2;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open("/efs/prox_cal", O_CREAT | O_TRUNC | O_WRONLY | O_SYNC, 0660);
	if (IS_ERR(cal_filp)) {
		pr_err("%s: Can't open calibration file\n", __func__);
		set_fs(old_fs);
		err = PTR_ERR(cal_filp);
		goto done;
	}

	err = cal_filp->f_op->write(cal_filp,
		(char *)&data->offset_value, sizeof(int),
			&cal_filp->f_pos);
	if (err != sizeof(int)) {
		pr_err("%s: Can't write the cal data to file\n", __func__);
		err = -EIO;
	}

	filp_close(cal_filp, current->files);
	PX_LOG("%s: offset_value = %d\n", __func__, data->offset_value);
done:
	set_fs(old_fs);
no_cal:
	return err;
}

static ssize_t proximity_cal_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct px3325_data *data = dev_get_drvdata(dev);
	msleep(20);
	return sprintf(buf, "%d,%d\n",
			data->offset_value, px3325_get_phthres(data->client));
}

static ssize_t proximity_cal_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct px3325_data *data = dev_get_drvdata(dev);
	bool do_calib;
	int err;

	if (sysfs_streq(buf, "1")) { /* calibrate cancelation value */
		do_calib = true;
	} else if (sysfs_streq(buf, "0")) { /* reset cancelation value */
		do_calib = false;
	} else {
		pr_err("%s: invalid value %d\n", __func__, *buf);
		err = -EINVAL;
		goto done;
	}
	err = proximity_do_calibrate(data, do_calib, false);
	if (err < 0) {
		pr_err("%s: proximity_store_offset() failed\n", __func__);
		goto done;
	} else
		err = size;
done:
	return err;
}
static struct device_attribute dev_attr_proximity_sensor_prox_cal =
	__ATTR(prox_cal, S_IRUGO | S_IWUSR | S_IWGRP,
				proximity_cal_show, proximity_cal_store);


static ssize_t proximity_thresh_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct px3325_data *data = dev_get_drvdata(dev);    
	int thresh_hi = 0;

	msleep(20);
	thresh_hi = px3325_get_phthres(data->client);
	pr_info("%s: THRESHOLD = %d\n", __func__, thresh_hi);

	return sprintf(buf, "prox_threshold = %d\n", thresh_hi);
}

static ssize_t proximity_thresh_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct px3325_data *data = dev_get_drvdata(dev);
	long thresh_value = 0;
	int err = 0;

	err = strict_strtol(buf, 10, &thresh_value);
	if (unlikely(err < 0)) {
		pr_err("%s, kstrtoint failed.", __func__);
		goto done;
	}
	data->threshold_high = thresh_value;
	err = proximity_do_calibrate(data, true, true);
	if (err < 0) {
		pr_err("%s: thresh_store failed\n", __func__);
		goto done;
	}
	msleep(20);
done:
	return size;
}
static struct device_attribute dev_attr_proximity_sensor_prox_thresh =
	__ATTR(prox_thresh, S_IRUGO | S_IWUSR | S_IWGRP,
				proximity_thresh_show, proximity_thresh_store);


static ssize_t prox_offset_pass_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct px3325_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", data->cal_result);
}
static struct device_attribute dev_attr_proximity_sensor_offset_pass =
	__ATTR(prox_offset_pass,  S_IRUSR | S_IRGRP, prox_offset_pass_show, NULL);

static ssize_t prox_vendor_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", "LITEON");
}
static struct device_attribute dev_attr_proximity_sensor_vendor =
	__ATTR(vendor, S_IRUSR | S_IRGRP, prox_vendor_show, NULL);


static ssize_t prox_name_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", "PX3325");
}
static struct device_attribute dev_attr_proximity_sensor_name =
	__ATTR(name, S_IRUSR | S_IRGRP, prox_name_show, NULL);


static struct device_attribute *additional_proximity_attrs[] = {
	&dev_attr_proximity_sensor_state,
	&dev_attr_proximity_sensor_prox_avg,
	&dev_attr_proximity_sensor_prox_cal,
        &dev_attr_proximity_sensor_prox_cal_open,
	&dev_attr_proximity_sensor_prox_thresh,
	&dev_attr_proximity_sensor_offset_pass,
	&dev_attr_proximity_sensor_vendor,
	&dev_attr_proximity_sensor_name,
	NULL,
};

/*============================================================================*/

static int px3325_init_client(struct i2c_client *client)
{
	struct px3325_data *data = i2c_get_clientdata(client);
	int i;

	/* read all the registers once to fill the cache.
	 * if one of the reads fails, we consider the init failed */
	for (i = 0; i < PX3325_NUM_CACHABLE_REGS; i++) {
		int v = i2c_smbus_read_byte_data(client, px3325_reg[i]);
		if (v < 0)
			return -ENODEV;

		data->reg_cache[i] = v;
	}

	/* set defaults */
        if (PX3325_INT_CLEAN_METHOD == SOFT_CLEAN_INT)  {
            px3325_set_int_clean(client, 0x01);  // set interrupt clean method to manual.
            px3325_int_clean(client);   // clean all interrupt flag in the beginning.
        }

	px3325_set_gain(client, 0x01);  // 2X gain
	px3325_set_integrate(client, PX3325_PX_INTEGRATED_TIME);
        px3325_set_phthres(client, PX3325_PX_DEFAULT_THREH);
        px3325_set_plthres(client, PX3325_PX_DEFAULT_THREL);
	px3325_set_mode(client, 0);    // power down for power saving.

        PX_DBG("px3325_init_client done.\n");
	return 0;
}

static void px3325_work_func(struct work_struct *work)
{
	struct px3325_data *data = container_of(work, struct px3325_data, work_proximity);
	int Pval, phth, plth;

	mutex_lock(&data->lock);
	
	Pval = px3325_get_px_value(data->client,1);
    
	plth = px3325_get_plthres(data->client);
	phth = px3325_get_phthres(data->client);

	PX_LOG("PS data = [%d]\t", Pval);
	PX_LOG("%s\n", (Pval > phth) ? "obj near":((Pval < plth) ? "obj far":"obj in range"));

	mutex_unlock(&data->lock);

	input_report_abs(data->proximity_input_dev, ABS_DISTANCE, (Pval > phth) ? 0 : 1);
	input_sync(data->proximity_input_dev);
    
	// clean interrupt flag if using software clean (manual)
	if (PX3325_INT_CLEAN_METHOD == SOFT_CLEAN_INT){
            px3325_int_clean(data->client);
	}
    
}

static int px3325_input_init(struct px3325_data *data)
{
    struct input_dev *input_dev;
    int ret;

    /* allocate proximity input_device */
    input_dev = input_allocate_device();
    if (!input_dev) {
        PX_DBG("could not allocate input device\n");
        goto err_pxy_all;
    }
    data->proximity_input_dev = input_dev;
    input_set_drvdata(input_dev, data);
    input_dev->name = "proximity_sensor";
    input_set_capability(input_dev, EV_ABS, ABS_DISTANCE);
    input_set_abs_params(input_dev, ABS_DISTANCE, 0, 1, 0, 0);

    PX_DBG("registering proximity input device\n");
    ret = input_register_device(input_dev);
    if (ret < 0) {
        PX_DBG("could not register input device\n");
        goto err_pxy_reg;;
    }

    ret = sysfs_create_group(&input_dev->dev.kobj,
                 &px3325_ps_attr_group);
    if (ret) {
        PX_DBG("could not create sysfs group\n");
        goto err_pxy_sys;;
    }

    /* set initial proximity value as 1 */
    input_report_abs(data->proximity_input_dev, ABS_DISTANCE, 1);
    input_sync(data->proximity_input_dev);
    
    PX_DBG("px3325_input_init done.\n");
    return 0;

err_pxy_sys:
    input_unregister_device(data->proximity_input_dev);
err_pxy_reg:
    input_free_device(input_dev);
err_pxy_all:

    return (-1);   
}

static void px3325_input_fini(struct px3325_data *data)
{
    struct input_dev *dev = data->proximity_input_dev;

    input_unregister_device(dev);
    input_free_device(dev);
}

/*
 * I2C layer
 */

static irqreturn_t px3325_irq_handler(int irq, void *data_)
{
	struct px3325_data *data = data_;

        PX_DBG("px3325_irq_handler called.\n");
 
	mutex_lock(&data->lock);

	//px3325_get_intstat(data->client);
	
        wake_lock_timeout(&data->prx_wake_lock, 3 * HZ);
        queue_work(data->wq, &data->work_proximity);

	mutex_unlock(&data->lock);

	return IRQ_HANDLED;
}

static int px3325_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
    	struct px3325_platform_data *platform_data;
	struct px3325_data *data;
	int err = 0;

	PX_FUN();

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
		return -EIO;

	data = kzalloc(sizeof(struct px3325_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

    	platform_data = client->dev.platform_data;

	if (platform_data->power_on){
		data->power_on = platform_data->power_on;
                data->power_on(1);
	}

    	if (platform_data->led_on){
		data->led_on = platform_data->led_on;            
		data->led_on(1);
    	}
        
	data->client = client;
	i2c_set_clientdata(client, data);
	mutex_init(&data->lock);
	wake_lock_init(&data->prx_wake_lock, WAKE_LOCK_SUSPEND, "prx_wake_lock");

	/* initialize the PX3325 chip */
	err = px3325_init_client(client);
	if (err) {
      		PX_DBG("could not px3325_init_client\n");
		goto exit_kfree;
	}

	data->irq_gpio = platform_data->irq_gpio;
	/*Initialisation of GPIO_PS_OUT of proximity sensor*/
	if (gpio_request(data->irq_gpio, "Proximity Out")) {
      		PX_LOG("Proximity Request GPIO_%d failed!\n", data->irq_gpio);
	} 
        else {
      		PX_LOG("Proximity Request GPIO_%d Sucess!\n", data->irq_gpio);            
	}
	gpio_direction_input(data->irq_gpio);
    
	err = px3325_input_init(data);
	if (err)
		goto exit_kfree;

        data->irq = gpio_to_irq(data->irq_gpio);
        err = request_threaded_irq(data->irq, NULL, px3325_irq_handler,
                   IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_NO_SUSPEND,
                   "px3325_int", data);
                   
        if (err){
            dev_err(&client->dev, "ret: %d, could not get IRQ %d\n",err,client->irq);
            goto exit_irq;
        }
        else{
            PX_LOG("request_irq success IRQ_NO:%d, GPIO:%d\n", data->irq, data->irq_gpio);
        }
        enable_irq_wake(data->irq);

	INIT_WORK(&data->work_proximity, px3325_work_func);
    
	data->wq = create_singlethread_workqueue("px3325_wq");
	if (!data->wq) {
		PX_DBG("could not create workqueue\n");
		goto exit_work;
	}

	err = sensors_register(data->proximity_sensor_device,
		data, additional_proximity_attrs, "proximity_sensor");
	if (err) {
		pr_err("%s: cound not register sensor device\n", __func__);
		goto err_sysfs_create_factory_proximity;
	}

	/*Pulling the GPIO_PS_OUT Pin High*/
        PX_LOG("gpio_get_value of GPIO_PS_OUT is %d\n", gpio_get_value(data->irq_gpio));

	if (data->led_on) {
            data->led_on(0);
	}
    
        data->offset_value=0;        
	dev_info(&client->dev, "Driver version %s enabled\n", DRIVER_VERSION);   
	return 0;

err_sysfs_create_factory_proximity:
	free_irq(data->irq, data);
exit_work:
	destroy_workqueue(data->wq);
exit_irq:
	px3325_input_fini(data);	
exit_kfree:
	wake_lock_destroy(&data->prx_wake_lock);
	mutex_destroy(&data->lock);
	kfree(data);
	return err;
}

static int px3325_remove(struct i2c_client *client)
{
	struct px3325_data *data = i2c_get_clientdata(client);	
	free_irq(data->irq, data);
	sysfs_remove_group(&data->proximity_input_dev->dev.kobj, &px3325_ps_attr_group);
	input_unregister_device(data->proximity_input_dev);
	destroy_workqueue(data->wq);
	mutex_destroy(&data->lock);
	wake_lock_destroy(&data->prx_wake_lock);
	kfree(data);	
	return 0;
}


#ifdef CONFIG_PM
static int px3325_suspend(struct device *dev)
{   	   
    return 0;
}
static int px3325_resume(struct device *dev)
{  	   
    return 0;
}
#else
#define px3325_suspend NULL
#define px3325_resume NULL
#endif

static const struct i2c_device_id px3325_id[] = {
	{ "px3325", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, px3325_id);

static const struct dev_pm_ops px3325_pm_ops = {
	.suspend = px3325_suspend,
	.resume = px3325_resume,
};

static struct i2c_driver px3325_driver = {
	.driver = {
		.name	= PX3325_DRV_NAME,
		.owner	= THIS_MODULE,
                .pm = &px3325_pm_ops,		
	},
	.probe	= px3325_probe,
	.remove	= px3325_remove,
	.id_table = px3325_id,
};

static int __init px3325_init(void)
{
	PX_FUN();

	return i2c_add_driver(&px3325_driver);    
}

static void __exit px3325_exit(void)
{
	i2c_del_driver(&px3325_driver);
}

MODULE_AUTHOR("LiteOn-semi corporation.");
MODULE_DESCRIPTION("PX3325 driver.");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(DRIVER_VERSION);

module_init(px3325_init);
module_exit(px3325_exit);
