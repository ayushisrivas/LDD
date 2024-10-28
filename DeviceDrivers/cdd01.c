#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "mychardev"
#define BUFFER_SIZE 1024

static int majorNumber;
static char message[BUFFER_SIZE] = {0};
static short size_of_message = 0;
static struct class* mycharClass = NULL;
static struct device* mycharDevice = NULL;
static struct cdev my_cdev;

static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

static struct file_operations fops = {
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};

static int __init mychardev_init(void) {
    int ret;

    printk(KERN_INFO "mychardev: initializing mychardev...\n");
    if ((ret = alloc_chrdev_region(&majorNumber, 0, 1, DEVICE_NAME)) < 0) {
        printk(KERN_ALERT "mychardev: Major number registration failed\n");
        return ret;
    }

    printk(KERN_INFO "mychardev: major number allocated\n");

    cdev_init(&my_cdev, &fops);
    my_cdev.owner = THIS_MODULE;

    if ((ret = cdev_add(&my_cdev, majorNumber, 1)) < 0) {
        printk(KERN_ALERT "mychardev: cdev structure could not be added\n");
        unregister_chrdev_region(majorNumber, 1);
        return ret;
    }

    mycharClass = class_create(DEVICE_NAME);
    if (IS_ERR(mycharClass)) {
        printk(KERN_ALERT "mychardev: Character class creation failed\n");
        cdev_del(&my_cdev);
        unregister_chrdev_region(majorNumber, 1);
        return PTR_ERR(mycharClass);
    }

    mycharDevice = device_create(mycharClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
    if (IS_ERR(mycharDevice)) {
        printk(KERN_ALERT "mychardev: Character device file creation failed\n");
        class_destroy(mycharClass);
        cdev_del(&my_cdev);
        unregister_chrdev_region(majorNumber, 1);
        return PTR_ERR(mycharDevice);
    }

    printk(KERN_INFO "mychardev: Chardev added successfully\n");
    return 0;
}

static void __exit mychardev_exit(void) {
    printk(KERN_INFO "mychardev: Removing module\n");
    if (mycharDevice) {
        device_destroy(mycharClass, MKDEV(majorNumber, 0));
    }
    if (mycharClass) {
        class_destroy(mycharClass);
    }
    cdev_del(&my_cdev);
    unregister_chrdev_region(majorNumber, 1);
    printk(KERN_INFO "mychardev: Goodbye from the LKM\n");
}

static int dev_open(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "mychardev: Device successfully opened\n");
    return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
    int bytes_read = 0, error_count = 0;

    if (len > size_of_message) {
        len = size_of_message;
    }

    error_count = copy_to_user(buffer, message, len);

    if (error_count == 0) {
        bytes_read = len;
        size_of_message = 0;  // Clear the message size after sending
        printk(KERN_INFO "mychardev: Sent %d characters to the user\n", bytes_read);
        return bytes_read;
    } else {
        printk(KERN_ALERT "mychardev: Failed to send %d characters to the user\n", error_count);
        return -EFAULT;
    }
}

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
    if (len > BUFFER_SIZE) {
        len = BUFFER_SIZE;
    }

    if (copy_from_user(message, buffer, len)) {
        printk(KERN_ALERT "mychardev: Failed to receive characters from the user\n");
        return -EFAULT;
    }

    size_of_message = len;
    printk(KERN_INFO "mychardev: Received %zu characters from the user\n", len);
    return len;
}

static int dev_release(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "mychardev: Device successfully closed\n");
    return 0;
}

module_init(mychardev_init);
module_exit(mychardev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ayushi");
MODULE_DESCRIPTION("Simple pseudo character device driver");
MODULE_VERSION("1.0");

