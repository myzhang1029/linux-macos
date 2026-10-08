/* Identify my kernel. */
/*
 *  zmy.c
 *  Copyright (C) 2021 Zhang Maiyun <me@maiyun.me>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#define USE_MISC_DEVICE 0
#define ZMY_MINOR 0

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>

#if USE_MISC_DEVICE == 0
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#else
#include <linux/miscdevice.h>
#endif

/* File operation functions. */
static int zmy_open(struct inode *inode, struct file *fp);
static int zmy_release(struct inode *inode, struct file *fp);
static ssize_t zmy_read(struct file *fp, char __user *buf, size_t count,
                        loff_t *pos);
static ssize_t zmy_write(struct file *fp, const char __user *buf, size_t count,
                         loff_t *pos);
static loff_t zmy_llseek(struct file *fp, loff_t off, int whence);

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .write = zmy_write,
    .read = zmy_read,
    .open = zmy_open,
    .release = zmy_release,
    .llseek = zmy_llseek,
};

/* Data to be read. */
static const char payload[] = "This is Zhang Maiyun's linux kernel\n";
/* -1 to remove \0. */
static size_t payload_size = sizeof(payload) - 1;
/* Character special data. */
#if USE_MISC_DEVICE == 0
static struct class *cls;
static dev_t dev;
static struct cdev cdev;
#else
static struct miscdevice zmy_miscdevice = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "zmy",
    .fops = &fops,
    .mode = 0644,
};
#endif

/* Module initializer. */
static int __init zmy_init(void)
{
#if USE_MISC_DEVICE == 0
    /* Allocate a node major number. */
    if (alloc_chrdev_region(&dev, ZMY_MINOR, 1, "zmy") < 0)
    {
        printk(KERN_ERR "zmy: alloc_chrdev_region failed");
        return -1;
    }
    printk(KERN_INFO "zmy: Got major number %d", dev >> 20);

    /* Initialize charater special data. */
    cdev_init(&cdev, &fops);
    cdev.owner = THIS_MODULE;
    cdev.ops = &fops;
    if (cdev_add(&cdev, dev, 1) != 0)
    {
        printk(KERN_ERR "zmy: cdev_add failed");
        return -2;
    }

    /* Create /sys class. */
    cls = class_create("zmy");
    if (IS_ERR(cls))
    {
        printk(KERN_ERR "zmy: class_create failed");
        return -3;
    }

    /* Create device. */
    if (IS_ERR(device_create(cls, NULL, dev, NULL, "zmy")))
    {
        printk(KERN_ERR "zmy: device_create failed");
        return -4;
    }
#else
    misc_register(&zmy_miscdevice);
#endif
    printk(KERN_INFO "zmy: Kernel Identification Module Loaded");
    return 0;
}

static void __exit zmy_cleanup(void)
{
#if USE_MISC_DEVICE == 0
    device_destroy(cls, dev);
    class_destroy(cls);
    cdev_del(&cdev);
    unregister_chrdev_region(dev, 1);
#else
    misc_deregister(&zmy_miscdevice);
#endif
    printk(KERN_INFO "zmy: Kernel Identification Module Exited");
}

static int zmy_open(struct inode *inode, struct file *fp) { return 0; }

static int zmy_release(struct inode *inode, struct file *fp) { return 0; }

static ssize_t zmy_read(struct file *fp, char __user *buf, size_t count,
                        loff_t *pos)
{
    /* Write payload to buf */
    if (*pos >= payload_size)
        return 0;
    /* Over-read. */
    if (count + *pos >= payload_size)
        count = payload_size - *pos;
    printk(KERN_INFO "zmy: read %s", payload);
    if (copy_to_user(buf, payload + *pos, count) != 0)
        return -EFAULT;
    *pos += count;
    return count;
}

static ssize_t zmy_write(struct file *fp, const char __user *buf, size_t count,
                         loff_t *pos)
{
    char *buffer = kmalloc(count, GFP_KERNEL);
    /* BUF is too big. */
    if (buffer == NULL)
        return -EINVAL;
    /* Print buf to dmesg ring buffer. */
    if (copy_from_user(buffer, buf, count) != 0)
        return -EFAULT;
    printk(KERN_INFO "zmy: Received %s", buffer);
    kfree(buffer);
    *pos += count;
    return count;
}

static loff_t zmy_llseek(struct file *fp, loff_t off, int whence)
{
    loff_t newpos = off;
    switch (whence)
    {
        case 0: /* SEEK_SET */
            break;
        case 1: /* SEEK_CUR */
            newpos += fp->f_pos;
            break;
        case 2: /* SEEK_END */
            newpos += payload_size;
            break;
        default:
            return -EINVAL;
    }
    if (newpos < 0)
        return -EINVAL;
    fp->f_pos = newpos;
    return newpos;
}

module_init(zmy_init);
module_exit(zmy_cleanup);
MODULE_AUTHOR("Zhang Maiyun <me@maiyun.me>");
MODULE_DESCRIPTION("Zhang Maiyun Kernel Identification Module");
MODULE_LICENSE("GPL");
