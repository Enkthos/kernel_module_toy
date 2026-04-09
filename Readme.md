
# Embedded I/O Simulator - Kernel Module Toy

A virtual embedded development board that runs inside Linux.
This project contains the following files:
```
embedded_module.c           - Kernel module 
embedded_test.c             - Control program 
Makefile_embedded           - Build configuration
test_embedded.sh            - Automated build/test
```

## What is this?

This program creates a **fake electronic development board** directly inside your Linux system — no real hardware required.

It creates a virtual device file `/dev/embedded_io` that your programs can talk to using standard Linux system calls.

## Features

The simulator currently supports:

- **GPIO Pins** (32 pins)  
  Turn pins on/off — like controlling LEDs or simple digital devices.

- **PWM Channels** (4 channels)  
  Control motor speed or LED brightness (frequency + duty cycle).

- **ADC / Analog Sensors** (8 channels)  
  Simulate reading real-world sensors (temperature, light, voltage, etc.) with realistic varying values.

- **UART / Serial Communication**  
  Send and receive text messages, simulating serial communication between devices.

## How to Use

Use the included test program `embedded_test`:

```bash
# Basic examples
sudo ./embedded_test gpio-write 0 1          # Turn GPIO pin 0 ON
sudo ./embedded_test adc 0                    # Read sensor on channel 0
sudo ./embedded_test uart-send 0 "Hello!"     # Send message via UART
sudo ./embedded_test uart-receive 0           # Read message from UART

# Run full demonstration
sudo ./embedded_test test
```
---

# Deployment
## Install Tools

```bash
# Ubuntu/Debian:
sudo apt update && sudo apt install -y build-essential linux-headers-$(uname -r)

# Verify tools installed
gcc --version && make --version
```

### Build the Module

```bash
#Since the makefile is not called makefile, create a symbolic link to prevent errors

ln -s Makefile_embedded Makefile

# Make test script executable
chmod +x test_embedded.sh

# Build everything
sudo ./test_embedded.sh build-all
```

**Expected output:**
```

[✓] Build successful
-rw-r--r-- 1 root root 303K Apr  9 22:58 embedded_module.ko

[*] Building test program...
[✓] Test program compiled
-rwxr-xr-x 1 root root 21K Apr  9 22:58 embedded_test

```

### Test 

```bash
# Load module into kernel
sudo ./test_embedded.sh load

# Run complete test suite
sudo ./test_embedded.sh test
```

**Expected output:**
```
[✓] Device found: /dev/embedded_io
crw------- 1 root root 238, 0 Apr  9 23:00 /dev/embedded_io

[*] Testing GPIO functionality...
Setting GPIO 0 as output...
✓ GPIO pin 0 set as OUTPUT

...

════════════════════════════════════════
✓ All tests passed!
════════════════════════════════════════

```

### Play!

Your kernel module is now running!

```bash
# Verify
lsmod | grep embedded_module
ls -l /dev/embedded_io
dmesg | tail -5
```

---

## Some important commands

```bash
# Build (from ~/embedded-dev)
sudo ./test_embedded.sh build-all

# Load
sudo ./test_embedded.sh load

# Test
sudo ./test_embedded.sh test

# Control GPIO
sudo ./embedded_test gpio-out 0        # Set as output
sudo ./embedded_test gpio-write 0 1    # Write HIGH
sudo ./embedded_test gpio-read 0       # Read value

# Control PWM
sudo ./embedded_test pwm 0 1000 50     # 1kHz @ 50% duty

# Read ADC
sudo ./embedded_test adc 0             # Read channel 0

#UART test send
sudo ./embedded_test uart-send 0 "Hello from userspace via UART!"

#UART receive test
sudo ./embedded_test uart-receive 0

# Monitor
dmesg | tail -20                       # Last 20 kernel messages
sudo dmesg -w                          # Real-time messages (Ctrl+C to exit)

# Unload when done
sudo ./test_embedded.sh unload
```

