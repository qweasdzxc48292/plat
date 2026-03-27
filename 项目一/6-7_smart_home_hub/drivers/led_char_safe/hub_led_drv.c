#include <linux/cdev.h>
#include <linux/class.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define HUB_LED_NAME "hub_led"
#define HUB_LED_CLASS "hub_led_class"
#define HUB_LED_DEVNODE "hub_led"
#define HUB_LED_MAX 1

static int led_gpio = 131;
module_param(led_gpio, int, 0644);
MODULE_PARM_DESC(led_gpio, "GPIO number for LED0 on IMX6ULL board");

static int major;
static struct class *hub_led_class;
static DEFINE_MUTEX(hub_led_lock);

static int hub_led_open(struct inode *inode, struct file *file)
{
    (void)inode;
    (void)file;
    return 0;
}

static int hub_led_release(struct inode *inode, struct file *file)
{
    (void)inode;
    (void)file;
    return 0;
}

/*
 * userspace protocol:
 * write 2 bytes: [led_index, value]
 *   - value 0: ON  (active-low board)
 *   - value 1: OFF
 */
static ssize_t hub_led_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
    u8 kbuf[2];
    int val;

    (void)file;
    (void)ppos;

    if (size < sizeof(kbuf)) {
        return -EINVAL;
    }
    if (copy_from_user(kbuf, buf, sizeof(kbuf)) != 0) {
        return -EFAULT;
    }
    if (kbuf[0] >= HUB_LED_MAX) {
        return -EINVAL;
    }

    val = kbuf[1] ? 1 : 0;

    mutex_lock(&hub_led_lock);
    gpio_set_value(led_gpio, val);
    mutex_unlock(&hub_led_lock);

    return sizeof(kbuf);
}

/*
 * userspace protocol:
 * read/write same 2-byte buffer:
 *   input : [led_index, ignored]
 *   output: [led_index, gpio_value]
 */
static ssize_t hub_led_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
    u8 kbuf[2] = {0};

    (void)file;
    (void)ppos;

    if (size < sizeof(kbuf)) {
        return -EINVAL;
    }
    if (copy_from_user(kbuf, buf, 1) != 0) {
        return -EFAULT;
    }
    if (kbuf[0] >= HUB_LED_MAX) {
        return -EINVAL;
    }

    mutex_lock(&hub_led_lock);
    kbuf[1] = gpio_get_value(led_gpio) ? 1 : 0;
    mutex_unlock(&hub_led_lock);

    if (copy_to_user(buf, kbuf, sizeof(kbuf)) != 0) {
        return -EFAULT;
    }
    return sizeof(kbuf);
}

static const struct file_operations hub_led_fops = {
    .owner = THIS_MODULE,
    .open = hub_led_open,
    .release = hub_led_release,
    .read = hub_led_read,
    .write = hub_led_write,
};

static int __init hub_led_init(void)
{
    int ret;

    ret = gpio_request(led_gpio, HUB_LED_NAME);
    if (ret) {
        pr_err("hub_led: gpio_request(%d) failed: %d\n", led_gpio, ret);
        return ret;
    }

    ret = gpio_direction_output(led_gpio, 1);
    if (ret) {
        pr_err("hub_led: gpio_direction_output(%d) failed: %d\n", led_gpio, ret);
        gpio_free(led_gpio);
        return ret;
    }

    major = register_chrdev(0, HUB_LED_NAME, &hub_led_fops);
    if (major < 0) {
        pr_err("hub_led: register_chrdev failed: %d\n", major);
        gpio_free(led_gpio);
        return major;
    }

    hub_led_class = class_create(THIS_MODULE, HUB_LED_CLASS);
    if (IS_ERR(hub_led_class)) {
        pr_err("hub_led: class_create failed\n");
        unregister_chrdev(major, HUB_LED_NAME);
        gpio_free(led_gpio);
        return PTR_ERR(hub_led_class);
    }

    if (IS_ERR(device_create(hub_led_class, NULL, MKDEV(major, 0), NULL, HUB_LED_DEVNODE))) {
        pr_err("hub_led: device_create failed\n");
        class_destroy(hub_led_class);
        unregister_chrdev(major, HUB_LED_NAME);
        gpio_free(led_gpio);
        return -ENODEV;
    }

    pr_info("hub_led: loaded, /dev/%s major=%d gpio=%d\n", HUB_LED_DEVNODE, major, led_gpio);
    return 0;
}

static void __exit hub_led_exit(void)
{
    device_destroy(hub_led_class, MKDEV(major, 0));
    class_destroy(hub_led_class);
    unregister_chrdev(major, HUB_LED_NAME);
    gpio_set_value(led_gpio, 1);
    gpio_free(led_gpio);
    pr_info("hub_led: unloaded\n");
}

module_init(hub_led_init);
module_exit(hub_led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Smart Home Hub");
MODULE_DESCRIPTION("Safe LED char driver with copy checks and mutex");

