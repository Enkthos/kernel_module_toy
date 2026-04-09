/*
 * Userspace test program for embedded I/O kernel module
 * 
 * This program demonstrates how to use the embedded_module.ko
 * to control GPIO pins, PWM, and read ADC values.
 * 
 * Compile: gcc -o embedded_test embedded_test.c
 * Run: sudo ./embedded_test <command> [args]
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>

#define DEVICE_PATH "/dev/embedded_io"

/* IOCTL command definitions - MUST match kernel module */
#define EMBEDDED_IOC_MAGIC  'E'
#define GPIO_SET_OUTPUT     _IOW(EMBEDDED_IOC_MAGIC, 1, int)
#define GPIO_GET_INPUT      _IOR(EMBEDDED_IOC_MAGIC, 2, int)
#define GPIO_PIN_WRITE      _IOW(EMBEDDED_IOC_MAGIC, 3, struct gpio_data)
#define GPIO_PIN_READ       _IOR(EMBEDDED_IOC_MAGIC, 4, struct gpio_data)
#define PWM_SET_DUTY        _IOW(EMBEDDED_IOC_MAGIC, 5, struct pwm_config)
#define SENSOR_READ_ADC     _IOR(EMBEDDED_IOC_MAGIC, 6, struct adc_data)
#define DEVICE_GET_STATUS   _IOR(EMBEDDED_IOC_MAGIC, 7, struct device_status)

/* Data structures - MUST match kernel module */
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

/* Color codes for output */
#define RED     "\x1b[31m"
#define GREEN   "\x1b[32m"
#define YELLOW  "\x1b[33m"
#define BLUE    "\x1b[34m"
#define RESET   "\x1b[0m"

/* Function prototypes */
void print_help(const char *prog);
int gpio_set_output(int fd, int pin);
int gpio_write(int fd, int pin, int value);
int gpio_read(int fd, int pin);
int pwm_set(int fd, int pin, unsigned int freq, unsigned int duty);
int adc_read(int fd, int channel);
int device_status(int fd);
int device_read_status(int fd);

int main(int argc, char *argv[]) {
    int fd;
    
    if (argc < 2) {
        print_help(argv[0]);
        return 1;
    }
    
    /* Open device */
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, RED "ERROR: Cannot open %s\n" RESET, DEVICE_PATH);
        fprintf(stderr, "Make sure module is loaded: sudo insmod embedded_module.ko\n");
        return 1;
    }
    
    /* Parse commands */
    if (strcmp(argv[1], "gpio-out") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Usage: %s gpio-out <pin>\n", argv[0]);
            close(fd);
            return 1;
        }
        gpio_set_output(fd, atoi(argv[2]));
        
    } else if (strcmp(argv[1], "gpio-write") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Usage: %s gpio-write <pin> <value (0 or 1)>\n", argv[0]);
            close(fd);
            return 1;
        }
        gpio_write(fd, atoi(argv[2]), atoi(argv[3]));
        
    } else if (strcmp(argv[1], "gpio-read") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Usage: %s gpio-read <pin>\n", argv[0]);
            close(fd);
            return 1;
        }
        gpio_read(fd, atoi(argv[2]));
        
    } else if (strcmp(argv[1], "pwm") == 0) {
        if (argc != 5) {
            fprintf(stderr, "Usage: %s pwm <pin> <frequency_hz> <duty_percent>\n", argv[0]);
            close(fd);
            return 1;
        }
        pwm_set(fd, atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));
        
    } else if (strcmp(argv[1], "adc") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Usage: %s adc <channel (0-7)>\n", argv[0]);
            close(fd);
            return 1;
        }
        adc_read(fd, atoi(argv[2]));
        
    } else if (strcmp(argv[1], "status") == 0) {
        device_status(fd);
        
    } else if (strcmp(argv[1], "read") == 0) {
        device_read_status(fd);
        
    } else if (strcmp(argv[1], "test") == 0) {
        printf(BLUE "=== Running embedded module test sequence ===" RESET "\n\n");
        
        printf(YELLOW "1. Setting GPIO pins as outputs..." RESET "\n");
        gpio_set_output(fd, 0);
        gpio_set_output(fd, 1);
        gpio_set_output(fd, 2);
        
        printf(YELLOW "\n2. Writing GPIO values..." RESET "\n");
        gpio_write(fd, 0, 1);
        sleep(1);
        gpio_write(fd, 1, 1);
        sleep(1);
        gpio_write(fd, 2, 1);
        
        printf(YELLOW "\n3. Reading GPIO values..." RESET "\n");
        gpio_read(fd, 0);
        gpio_read(fd, 1);
        gpio_read(fd, 2);
        
        printf(YELLOW "\n4. Setting PWM channels..." RESET "\n");
        pwm_set(fd, 0, 1000, 50);  /* 1kHz @ 50% duty */
        pwm_set(fd, 1, 2000, 75);  /* 2kHz @ 75% duty */
        
        printf(YELLOW "\n5. Reading ADC channels..." RESET "\n");
        adc_read(fd, 0);
        adc_read(fd, 1);
        adc_read(fd, 2);
        
        printf(YELLOW "\n6. Getting device status..." RESET "\n");
        device_status(fd);
        
        printf(GREEN "\n✓ Test sequence completed!\n" RESET);
        
    } else {
        fprintf(stderr, RED "Unknown command: %s\n" RESET, argv[1]);
        print_help(argv[0]);
        close(fd);
        return 1;
    }
    
    close(fd);
    return 0;
}

void print_help(const char *prog) {
    printf(BLUE "Embedded I/O Module Control Program\n" RESET);
    printf("\nUsage: %s <command> [args]\n\n", prog);
    printf("Commands:\n");
    printf("  " GREEN "gpio-out <pin>" RESET "           - Set GPIO pin as output\n");
    printf("  " GREEN "gpio-write <pin> <val>" RESET "    - Write GPIO pin (0 or 1)\n");
    printf("  " GREEN "gpio-read <pin>" RESET "           - Read GPIO pin value\n");
    printf("  " GREEN "pwm <pin> <freq> <duty>" RESET "   - Configure PWM (freq in Hz, duty 0-100%%)\n");
    printf("  " GREEN "adc <channel>" RESET "             - Read ADC channel (0-7)\n");
    printf("  " GREEN "status" RESET "                    - Get device status\n");
    printf("  " GREEN "read" RESET "                      - Read device status formatted\n");
    printf("  " GREEN "test" RESET "                      - Run full test sequence\n");
    printf("\nExamples:\n");
    printf("  sudo %s gpio-out 0\n", prog);
    printf("  sudo %s gpio-write 0 1\n", prog);
    printf("  sudo %s pwm 0 1000 50\n", prog);
    printf("  sudo %s adc 0\n", prog);
    printf("  sudo %s test\n", prog);
}

int gpio_set_output(int fd, int pin) {
    if (ioctl(fd, GPIO_SET_OUTPUT, &pin) < 0) {
        perror(RED "ioctl GPIO_SET_OUTPUT failed" RESET);
        return -1;
    }
    printf(GREEN "✓ GPIO pin %d set as OUTPUT\n" RESET, pin);
    return 0;
}

int gpio_write(int fd, int pin, int value) {
    struct gpio_data data = {.pin = pin, .value = value ? 1 : 0};
    
    if (ioctl(fd, GPIO_PIN_WRITE, &data) < 0) {
        perror(RED "ioctl GPIO_PIN_WRITE failed" RESET);
        return -1;
    }
    printf(GREEN "✓ GPIO pin %d written: %d\n" RESET, pin, data.value);
    return 0;
}

int gpio_read(int fd, int pin) {
    struct gpio_data data = {.pin = pin, .value = 0};
    
    if (ioctl(fd, GPIO_PIN_READ, &data) < 0) {
        perror(RED "ioctl GPIO_PIN_READ failed" RESET);
        return -1;
    }
    printf(GREEN "✓ GPIO pin %d value: %d\n" RESET, pin, data.value);
    return 0;
}

int pwm_set(int fd, int pin, unsigned int freq, unsigned int duty) {
    struct pwm_config config = {.pin = pin, .frequency = freq, .duty_cycle = duty};
    
    if (duty > 100) {
        fprintf(stderr, RED "Error: Duty cycle must be 0-100%%\n" RESET);
        return -1;
    }
    
    if (ioctl(fd, PWM_SET_DUTY, &config) < 0) {
        perror(RED "ioctl PWM_SET_DUTY failed" RESET);
        return -1;
    }
    printf(GREEN "✓ PWM pin %d set: %uHz @ %u%% duty\n" RESET, pin, freq, duty);
    return 0;
}

int adc_read(int fd, int channel) {
    struct adc_data data = {.channel = channel, .value = 0, .voltage_mv = 0};
    
    if (channel > 7) {
        fprintf(stderr, RED "Error: ADC channel must be 0-7\n" RESET);
        return -1;
    }
    
    if (ioctl(fd, SENSOR_READ_ADC, &data) < 0) {
        perror(RED "ioctl SENSOR_READ_ADC failed" RESET);
        return -1;
    }
    printf(GREEN "✓ ADC channel %d: %u (%.2fV)\n" RESET, 
           channel, data.value, (float)data.voltage_mv / 1000.0f);
    return 0;
}

int device_status(int fd) {
    struct device_status status = {0};
    int i;
    
    if (ioctl(fd, DEVICE_GET_STATUS, &status) < 0) {
        perror(RED "ioctl DEVICE_GET_STATUS failed" RESET);
        return -1;
    }
    
    printf(BLUE "\n=== Device Status ===\n" RESET);
    printf("Uptime: %lu ms\n", status.uptime_ms);
    printf("Operations: %u\n", status.operation_count);
    
    printf("\nGPIO States (first 8 pins):\n");
    for (i = 0; i < 8; i++) {
        printf("  Pin %d: %s\n", i, status.gpio_states[i] ? "HIGH" : "LOW");
    }
    
    printf("\nPWM Active Channels:\n");
    for (i = 0; i < 4; i++) {
        printf("  Channel %d: %s\n", i, status.pwm_active[i] ? "ACTIVE" : "INACTIVE");
    }
    
    printf("\nADC Channels (first 8):\n");
    for (i = 0; i < 8; i++) {
        float voltage = (status.adc_channels[i] * 3.3) / 4096.0f;
        printf("  Channel %d: %u (%.2fV)\n", i, status.adc_channels[i], voltage);
    }
    
    return 0;
}

int device_read_status(int fd) {
    char buffer[512];
    ssize_t bytes_read;
    
    bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    if (bytes_read < 0) {
        perror(RED "read() failed" RESET);
        return -1;
    }
    
    buffer[bytes_read] = '\0';
    printf(BLUE "%s" RESET, buffer);
    return 0;
}
