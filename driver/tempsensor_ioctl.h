#ifndef TEMPSENSOR_IOCTL_H
#define TEMPSENSOR_IOCTL_H

#include <linux/ioctl.h>

#define TEMP_IOC_MAGIC 'T'

// Reset the simulated temperature back to 25.0°C.
#define TEMP_IOC_RESET      _IO(TEMP_IOC_MAGIC, 0)

// Set the maximum drift in tenths of a degree per reading.
// Example: 10 = ±1.0°C, 40 = ±4.0°C.
#define TEMP_IOC_SET_DRIFT  _IOW(TEMP_IOC_MAGIC, 1, int)

#endif /* TEMPSENSOR_IOCTL_H */
