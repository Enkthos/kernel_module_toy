#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/ioctl.h>
#include <linux/jiffies.h>
#include <linux/timer.h>

#define DEVICE_NAME "embedded_io"
#define CLASS_NAME "embedded_class"

/* IOCTL command definitions */
#define EMBEDDED_IOC_MAGIC  'E'
#define GPIO_SET_OUTPUT     _IOW(EMBEDDED_IOC_MAGIC, 1, int)
#define GPIO_GET_INPUT      _IOR(EMBEDDED_IOC_MAGIC, 2, int)
#define GPIO_PIN_WRITE      _IOW(EMBEDDED_IOC_MAGIC, 3, struct gpio_data)
#define GPIO_PIN_READ       _IOR(EMBEDDED_IOC_MAGIC, 4, struct gpio_data)
#define PWM_SET_DUTY        _IOW(EMBEDDED_IOC_MAGIC, 5, struct pwm_config)
#define SENSOR_READ_ADC     _IOR(EMBEDDED_IOC_MAGIC, 6, struct adc_data)
#define DEVICE_GET_STATUS   _IOR(EMBEDDED_IOC_MAGIC, 7, struct device_status)

/* UART Simulation Commands */
#define UART_SEND           _IOW(EMBEDDED_IOC_MAGIC, 8, struct uart_data)
#define UART_RECEIVE        _IOR(EMBEDDED_IOC_MAGIC, 9, struct uart_data)


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Salvador");
MODULE_DESCRIPTION("Embedded I/O Control Module - GPIO, PWM, ADC simulation for development");
MODULE_VERSION("2.0");

/* Data structures for IOCTL */
struct gpio_data {
    unsigned int pin;
    unsigned int value;
};

struct pwm_config {
    unsigned int pin;
    unsigned int frequency;  /* Hz */
    unsigned int duty_cycle; /* 0-100% */
};

struct adc_data {
    unsigned int channel;
    unsigned int value;      /* 0-4095 (12-bit) */
    unsigned int voltage_mv; /* Voltage in millivolts */
};

struct device_status {
    unsigned int gpio_states[8];
    unsigned int pwm_active[4];
    unsigned int adc_channels[8];
    unsigned long uptime_ms;
    unsigned int operation_count;
};

/* UART simulation data structure */
struct uart_data {
    unsigned int port;        /* UART port (0 = UART0) */
    char buffer[256];         /* Data to send/receive */
    unsigned int length;      /* Number of bytes */
    unsigned int baudrate;    /* Optional - for info only */
};

/* Device state structure */
typedef struct {
    unsigned int gpio_pins[32];      /* GPIO pin states (0 or 1) */
    unsigned int gpio_directions[32]; /* 0 = input, 1 = output */
    struct pwm_config pwm_channels[4];
    unsigned int adc_channels[8];
    unsigned long device_start_time;
    unsigned int operation_count;
    struct semaphore sem;

     /* UART simulation */
    char uart_rx_buffer[256];
    unsigned int uart_rx_len;
    unsigned int uart_port;
} embedded_device_t;

/* Device variables */
static int majorNumber;
static struct class* embeddedClass = NULL;
static struct device* embeddedDevice = NULL;
static embedded_device_t *device_data = NULL;

/* Forward declarations */
static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);
static long dev_ioctl(struct file *, unsigned int, unsigned long);

/* File operations structure */
static struct file_operations fops = {
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
    .unlocked_ioctl = dev_ioctl,
};

/**
 * Simulate ADC reading with realistic values
 */
static unsigned int simulate_adc_read(unsigned int channel) {
    unsigned int base_value = 2048; /* Mid-range value */
    unsigned int variance = (jiffies * 17 + channel * 23) % 256;
    return (base_value + variance) & 0xFFF; /* 12-bit value */
}

/**
 * Module initialization
 */
static int __init embedded_init(void) {
    int i;
    
    pr_info("Embedded I/O: Initializing module...\n");
    
    /* Allocate device data structure */
    device_data = kmalloc(sizeof(embedded_device_t), GFP_KERNEL);
    if (!device_data) {
        pr_err("Embedded I/O: Memory allocation failed\n");
        return -ENOMEM;
    }
    
    /* Initialize device state */
    device_data->device_start_time = jiffies;
    device_data->operation_count = 0;
    sema_init(&device_data->sem, 1);
    
    /* Initialize GPIO pins (all as inputs by default) */
    for (i = 0; i < 32; i++) {
        device_data->gpio_pins[i] = 0;
        device_data->gpio_directions[i] = 0; /* Input */
    }
    
    /* Initialize PWM channels */
    for (i = 0; i < 4; i++) {
        device_data->pwm_channels[i].pin = i;
        device_data->pwm_channels[i].frequency = 0;
        device_data->pwm_channels[i].duty_cycle = 0;
    }
    
    /* Initialize ADC channels with simulated values */
    for (i = 0; i < 8; i++) {
        device_data->adc_channels[i] = simulate_adc_read(i);
    }

     /* Initialize UART simulation */
    device_data->uart_rx_len = 0;
    device_data->uart_port = 0;
    memset(device_data->uart_rx_buffer, 0, sizeof(device_data->uart_rx_buffer));
    
    /* Register character device */
    majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
    if (majorNumber < 0) {
        pr_err("Embedded I/O: Failed to register character device\n");
        kfree(device_data);
        return majorNumber;
    }
    pr_info("Embedded I/O: Registered with major number %d\n", majorNumber);
    
    /* Create device class */
    embeddedClass = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(embeddedClass)) {
        unregister_chrdev(majorNumber, DEVICE_NAME);
        kfree(device_data);
        pr_err("Embedded I/O: Failed to register device class\n");
        return PTR_ERR(embeddedClass);
    }
    
    /* Create device node */
    embeddedDevice = device_create(embeddedClass, NULL, MKDEV(majorNumber, 0), 
                                   NULL, DEVICE_NAME);
    if (IS_ERR(embeddedDevice)) {
        class_destroy(embeddedClass);
        unregister_chrdev(majorNumber, DEVICE_NAME);
        kfree(device_data);
        pr_err("Embedded I/O: Failed to create device\n");
        return PTR_ERR(embeddedDevice);
    }
    
    pr_info("Embedded I/O: Device created at /dev/%s\n", DEVICE_NAME);
    pr_info("Embedded I/O: Features: 32 GPIO pins, 4 PWM channels, 8 ADC channels\n");
    
    return 0;
}

/**
 * Module cleanup
 */
static void __exit embedded_exit(void) {
    if (device_data) {
        kfree(device_data);
    }
    device_destroy(embeddedClass, MKDEV(majorNumber, 0));
    class_unregister(embeddedClass);
    class_destroy(embeddedClass);
    unregister_chrdev(majorNumber, DEVICE_NAME);
    pr_info("Embedded I/O: Module unloaded\n");
}

/**
 * Device open
 */
static int dev_open(struct inode *inodep, struct file *filep) {
    if (down_interruptible(&device_data->sem)) {
        return -ERESTARTSYS;
    }
    device_data->operation_count++;
    pr_info("Embedded I/O: Device opened (operations: %u)\n", 
            device_data->operation_count);
    return 0;
}

/**
 * Device release
 */
static int dev_release(struct inode *inodep, struct file *filep) {
    up(&device_data->sem);
    pr_info("Embedded I/O: Device released\n");
    return 0;
}

/**
 * Device read - returns device status in text format
 */
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
    char kbuffer[512];
    int bytes_written;
    unsigned long uptime_ms;
    int i;
    
    if (*offset > 0) {
        return 0; /* EOF */
    }
    
    uptime_ms = jiffies_to_msecs(jiffies - device_data->device_start_time);
    
    /* Build status string */
    bytes_written = snprintf(kbuffer, sizeof(kbuffer),
        "=== Embedded I/O Status ===\n"
        "Uptime: %lu ms\n"
        "Operations: %u\n"
        "GPIO Outputs: ",
        uptime_ms,
        device_data->operation_count
    );
    
    /* Add GPIO output states */
    for (i = 0; i < 8; i++) {
        if (device_data->gpio_directions[i] == 1) {
            bytes_written += snprintf(kbuffer + bytes_written, 
                sizeof(kbuffer) - bytes_written,
                "Pin%d=%d ", i, device_data->gpio_pins[i]);
        }
    }
    
    bytes_written += snprintf(kbuffer + bytes_written, 
        sizeof(kbuffer) - bytes_written,
        "\nPWM Active: ");
    
    /* Add PWM status */
    for (i = 0; i < 4; i++) {
        if (device_data->pwm_channels[i].frequency > 0) {
            bytes_written += snprintf(kbuffer + bytes_written,
                sizeof(kbuffer) - bytes_written,
                "Ch%d(%uHz,%u%%) ", i,
                device_data->pwm_channels[i].frequency,
                device_data->pwm_channels[i].duty_cycle);
        }
    }
    
    bytes_written += snprintf(kbuffer + bytes_written,
        sizeof(kbuffer) - bytes_written,
        "\nADC Samples: ");
    
    /* Add ADC values */
    for (i = 0; i < 4; i++) {
        bytes_written += snprintf(kbuffer + bytes_written,
            sizeof(kbuffer) - bytes_written,
            "Ch%d=%u ", i, device_data->adc_channels[i]);
    }
    
    bytes_written += snprintf(kbuffer + bytes_written,
        sizeof(kbuffer) - bytes_written, "\n");
    
    /* Copy to userspace */
    if (bytes_written > len) {
        bytes_written = len;
    }
    
    if (copy_to_user(buffer, kbuffer, bytes_written)) {
        pr_err("Embedded I/O: Failed to copy data to userspace\n");
        return -EFAULT;
    }
    
    *offset += bytes_written;
    return bytes_written;
}

/**
 * Device write - accepts commands
 */
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
    char kbuffer[256];
    int cmd;
    
    if (len >= sizeof(kbuffer)) {
        len = sizeof(kbuffer) - 1;
    }
    
    if (copy_from_user(kbuffer, buffer, len)) {
        pr_err("Embedded I/O: Failed to copy data from userspace\n");
        return -EFAULT;
    }
    
    kbuffer[len] = '\0';
    
    /* Simple command parsing */
    if (sscanf(kbuffer, "gpio %d %d", &cmd, &cmd) == 2) {
        pr_info("Embedded I/O: GPIO command received\n");
    } else if (strncmp(kbuffer, "reset", 5) == 0) {
        int i;
        for (i = 0; i < 32; i++) {
            device_data->gpio_pins[i] = 0;
            device_data->gpio_directions[i] = 0;
        }
        pr_info("Embedded I/O: Device reset\n");
    }
    
    return len;
}

/**
 * IOCTL handler - main control interface
 */
static long dev_ioctl(struct file *filep, unsigned int cmd, unsigned long arg) {
    struct gpio_data gpio_data;
    struct pwm_config pwm_config;
    struct adc_data adc_data;
    struct device_status status;
    struct uart_data uart_data;
    int retval = 0;
    int i;
    
    if (_IOC_TYPE(cmd) != EMBEDDED_IOC_MAGIC) {
        return -ENOTTY;
    }
    
    switch (cmd) {
    case GPIO_SET_OUTPUT:
        if (get_user(i, (int __user *)arg)) {
            return -EFAULT;
        }
        if (i >= 0 && i < 32) {
            device_data->gpio_directions[i] = 1;
            pr_info("Embedded I/O: GPIO pin %d set as OUTPUT\n", i);
        }
        break;
        
    case GPIO_PIN_WRITE:
        if (copy_from_user(&gpio_data, (struct gpio_data __user *)arg, 
                          sizeof(gpio_data))) {
            return -EFAULT;
        }
        if (gpio_data.pin < 32 && device_data->gpio_directions[gpio_data.pin] == 1) {
            device_data->gpio_pins[gpio_data.pin] = gpio_data.value ? 1 : 0;
            pr_info("Embedded I/O: GPIO pin %u set to %u\n", 
                   gpio_data.pin, device_data->gpio_pins[gpio_data.pin]);
        }
        break;
        
    case GPIO_PIN_READ:
        if (copy_from_user(&gpio_data, (struct gpio_data __user *)arg, 
                          sizeof(gpio_data))) {
            return -EFAULT;
        }
        if (gpio_data.pin < 32) {
            gpio_data.value = device_data->gpio_pins[gpio_data.pin];
            if (copy_to_user((struct gpio_data __user *)arg, &gpio_data, 
                            sizeof(gpio_data))) {
                return -EFAULT;
            }
            pr_info("Embedded I/O: GPIO pin %u read as %u\n", 
                   gpio_data.pin, gpio_data.value);
        }
        break;
        
    case PWM_SET_DUTY:
        if (copy_from_user(&pwm_config, (struct pwm_config __user *)arg, 
                          sizeof(pwm_config))) {
            return -EFAULT;
        }
        if (pwm_config.pin < 4 && pwm_config.duty_cycle <= 100) {
            device_data->pwm_channels[pwm_config.pin] = pwm_config;
            pr_info("Embedded I/O: PWM pin %u set to %uHz @ %u%% duty\n",
                   pwm_config.pin, pwm_config.frequency, pwm_config.duty_cycle);
        }
        break;
        
    case SENSOR_READ_ADC:
        if (copy_from_user(&adc_data, (struct adc_data __user *)arg, 
                          sizeof(adc_data))) {
            return -EFAULT;
        }
        if (adc_data.channel < 8) {
            /* Refresh ADC reading with simulated value */
            adc_data.value = simulate_adc_read(adc_data.channel);
            adc_data.voltage_mv = (adc_data.value * 3300) / 4096; /* 3.3V reference */
            
            device_data->adc_channels[adc_data.channel] = adc_data.value;
            
            if (copy_to_user((struct adc_data __user *)arg, &adc_data, 
                            sizeof(adc_data))) {
                return -EFAULT;
            }
                   pr_info("Embedded I/O: ADC ch%u read: %u raw → %u.%02u V\n",
                   adc_data.channel,
                   adc_data.value,
                   adc_data.voltage_mv / 1000,      // integer part
                   adc_data.voltage_mv % 1000);     // two decimal places
        }
        break;
        
    case DEVICE_GET_STATUS:
        /* Gather device status */
        for (i = 0; i < 8; i++) {
            status.gpio_states[i] = device_data->gpio_pins[i];
            status.adc_channels[i] = device_data->adc_channels[i];
            if (i < 4) {
                status.pwm_active[i] = device_data->pwm_channels[i].frequency > 0 ? 1 : 0;
            }
        }
        status.uptime_ms = jiffies_to_msecs(jiffies - device_data->device_start_time);
        status.operation_count = device_data->operation_count;
        
        if (copy_to_user((struct device_status __user *)arg, &status, 
                        sizeof(status))) {
            return -EFAULT;
        }
        pr_info("Embedded I/O: Status reported\n");
        break;
        
    /* UART SIM  */
    case UART_SEND:
        if (copy_from_user(&uart_data, (struct uart_data __user *)arg, sizeof(uart_data))) {
            return -EFAULT;
        }
        if (uart_data.length > 0 && uart_data.length <= 256) {
            memcpy(device_data->uart_rx_buffer, uart_data.buffer, uart_data.length);
            device_data->uart_rx_len = uart_data.length;
            device_data->uart_port = uart_data.port;

            pr_info("Embedded I/O: UART%u received %u bytes\n", 
                   uart_data.port, uart_data.length);
        }
        break;

    case UART_RECEIVE:
        if (copy_from_user(&uart_data, (struct uart_data __user *)arg, sizeof(uart_data))) {
            return -EFAULT;
        }
        if (device_data->uart_rx_len > 0) {
            uart_data.length = device_data->uart_rx_len;
            uart_data.port = device_data->uart_port;
            memcpy(uart_data.buffer, device_data->uart_rx_buffer, device_data->uart_rx_len);

            if (copy_to_user((struct uart_data __user *)arg, &uart_data, sizeof(uart_data))) {
                return -EFAULT;
            }

            pr_info("Embedded I/O: UART%u sent %u bytes to user\n", 
                   uart_data.port, uart_data.length);

            device_data->uart_rx_len = 0;   /* Clear after reading */
        } else {
            uart_data.length = 0;
            copy_to_user((struct uart_data __user *)arg, &uart_data, sizeof(uart_data));
        }
        break;

    default:
        return -ENOTTY;
    }
    
    return retval;
}

module_init(embedded_init);
module_exit(embedded_exit);
