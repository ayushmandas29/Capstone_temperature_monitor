#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/random.h>
#include <linux/mutex.h>
#include <linux/version.h>
#include "tempsensor_ioctl.h"

#define DEVICE_NAME "tempsensor"
#define CLASS_NAME  "tempsensor"

static int major_number;
static struct class *tempsensor_class;
static struct device *tempsensor_device;
static DEFINE_MUTEX(temp_mutex);

// Simulated temperature: starts at 25.0°C (stored as tenths of °C).
static int temperature_tenths = 250;

// Maximum random drift per read, in tenths of °C. Default ±1.0°C.
static int drift_tenths = 10;

static int temp_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int temp_release(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t temp_read(struct file *file, char __user *buffer,
                         size_t len, loff_t *offset)
{
    char temp_string[32];
    int written;
    int delta;
    u32 random_value;

    if (*offset > 0)
        return 0;

    mutex_lock(&temp_mutex);

    // Random walk: change temperature by [-drift, +drift].
    get_random_bytes(&random_value, sizeof(random_value));
    if (drift_tenths > 0)
        delta = (int)(random_value % (2 * drift_tenths + 1)) - drift_tenths;
    else
        delta = 0;

    temperature_tenths += delta;

    // Keep the simulation in a sensible range.
    if (temperature_tenths < -400)
        temperature_tenths = -400;
    if (temperature_tenths > 1200)
        temperature_tenths = 1200;

    written = scnprintf(temp_string, sizeof(temp_string), "%d.%d\n",
                        temperature_tenths / 10,
                        abs(temperature_tenths % 10));

    mutex_unlock(&temp_mutex);

    if (len < written)
        return -EINVAL;

    if (copy_to_user(buffer, temp_string, written))
        return -EFAULT;

    *offset += written;
    return written;
}

static long temp_ioctl(struct file *file, unsigned int cmd,
                       unsigned long arg)
{
    int new_drift;

    if (_IOC_TYPE(cmd) != TEMP_IOC_MAGIC)
        return -ENOTTY;

    mutex_lock(&temp_mutex);

    switch (cmd) {
    case TEMP_IOC_RESET:
        temperature_tenths = 250;
        break;

    case TEMP_IOC_SET_DRIFT:
        if (copy_from_user(&new_drift, (int __user *)arg, sizeof(new_drift))) {
            mutex_unlock(&temp_mutex);
            return -EFAULT;
        }

        if (new_drift < 0 || new_drift > 1000) {
            mutex_unlock(&temp_mutex);
            return -EINVAL;
        }

        drift_tenths = new_drift;
        break;

    default:
        mutex_unlock(&temp_mutex);
        return -ENOTTY;
    }

    mutex_unlock(&temp_mutex);
    return 0;
}

static const struct file_operations fops = {
    .owner          = THIS_MODULE,
    .open           = temp_open,
    .read           = temp_read,
    .unlocked_ioctl = temp_ioctl,
    .release        = temp_release,
};

static int __init tempsensor_init(void)
{
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        pr_err("tempsensor: failed to register character device\n");
        return major_number;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    tempsensor_class = class_create(CLASS_NAME);
#else
    tempsensor_class = class_create(THIS_MODULE, CLASS_NAME);
#endif
    if (IS_ERR(tempsensor_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(tempsensor_class);
    }

    tempsensor_device = device_create(tempsensor_class, NULL,
                                      MKDEV(major_number, 0), NULL,
                                      DEVICE_NAME);
    if (IS_ERR(tempsensor_device)) {
        class_destroy(tempsensor_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(tempsensor_device);
    }

    pr_info("tempsensor: module loaded, /dev/%s ready\n", DEVICE_NAME);
    return 0;
}

static void __exit tempsensor_exit(void)
{
    device_destroy(tempsensor_class, MKDEV(major_number, 0));
    class_destroy(tempsensor_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    pr_info("tempsensor: module unloaded\n");
}

module_init(tempsensor_init);
module_exit(tempsensor_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Capstone Team");
MODULE_DESCRIPTION("Simulated temperature sensor character device driver");
MODULE_VERSION("1.0");
