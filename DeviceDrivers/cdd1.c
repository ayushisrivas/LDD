#include<linux/kernel.h>
#include<linux/module.h>
#include<linux/fs.h>
#include<linux/cdev.h>
#include<linux/uaccess.h>

#define DEVICE_NAME "my_char_device"
#define BUFFER_SIZE 1024

static int majorNumber;
static char message[BUFFER_SIZE]={0};
static short size_of_message;
static struct class* mycharClass = NULL;
static struct device* mycharDevice = NULL;
static struct cdev my_cdev;


