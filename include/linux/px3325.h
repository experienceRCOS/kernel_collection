#ifndef _PX3325_h_
#define	_PX3325_h_

struct px3325_platform_data {
	void (*power_on) (bool);
	void (*led_on) (bool);
	unsigned int irq_gpio;
	int irq;
};

#endif
