#!/bin/bash

# Embedded I/O Module Testing Script
# Handles building, loading, testing, and debugging the embedded_module.ko

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

MODULE_NAME="embedded_module"
DEVICE_NAME="embedded_io"
DEVICE_PATH="/dev/$DEVICE_NAME"

echo -e "${BLUE}╔════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║  Embedded I/O Module Test Suite       ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════╝${NC}"

check_root() {
    if [[ $EUID -ne 0 ]]; then
        echo -e "${RED}[ERROR] This script must be run as root${NC}"
        echo "Use: sudo $0 <command>"
        exit 1
    fi
}

is_module_loaded() {
    lsmod | grep -q "^$MODULE_NAME"
}

build_module() {
    echo -e "${BLUE}[*] Building embedded I/O module...${NC}"
    if make -f Makefile_embedded; then
        echo -e "${GREEN}[✓] Build successful${NC}"
        ls -lh *.ko
    else
        echo -e "${RED}[✗] Build failed${NC}"
        return 1
    fi
}

build_test_program() {
    echo -e "${BLUE}[*] Building test program...${NC}"
    if gcc -o embedded_test embedded_test.c 2>&1; then
        echo -e "${GREEN}[✓] Test program compiled${NC}"
        ls -lh embedded_test
    else
        echo -e "${RED}[✗] Compilation failed${NC}"
        return 1
    fi
}

load_module() {
    echo -e "${BLUE}[*] Loading kernel module...${NC}"
    
    if is_module_loaded; then
        echo -e "${YELLOW}[!] Module already loaded${NC}"
        return 0
    fi
    
    if insmod "$MODULE_NAME.ko"; then
        echo -e "${GREEN}[✓] Module loaded successfully${NC}"
        sleep 1
        echo -e "${BLUE}Kernel messages:${NC}"
        dmesg | grep "Embedded I/O" | tail -5
        return 0
    else
        echo -e "${RED}[✗] Failed to load module${NC}"
        return 1
    fi
}

unload_module() {
    echo -e "${BLUE}[*] Unloading kernel module...${NC}"
    
    if ! is_module_loaded; then
        echo -e "${YELLOW}[!] Module not loaded${NC}"
        return 0
    fi
    
    if rmmod "$MODULE_NAME"; then
        echo -e "${GREEN}[✓] Module unloaded${NC}"
        sleep 1
        dmesg | tail -3
        return 0
    else
        echo -e "${RED}[✗] Unload failed${NC}"
        return 1
    fi
}

check_device() {
    if [ -c "$DEVICE_PATH" ]; then
        echo -e "${GREEN}[✓] Device found: $DEVICE_PATH${NC}"
        ls -l "$DEVICE_PATH"
        return 0
    else
        echo -e "${RED}[✗] Device not found${NC}"
        return 1
    fi
}

test_gpio() {
    echo -e "${BLUE}[*] Testing GPIO functionality...${NC}"
    
    if [ ! -f embedded_test ]; then
        echo -e "${RED}[✗] Test program not found. Build it first:${NC}"
        echo "    sudo $0 build-all"
        return 1
    fi
    
    echo -e "${YELLOW}Setting GPIO 0 as output...${NC}"
    ./embedded_test gpio-out 0 || return 1
    
    echo -e "${YELLOW}Writing GPIO 0 = 1...${NC}"
    ./embedded_test gpio-write 0 1 || return 1
    
    echo -e "${YELLOW}Reading GPIO 0...${NC}"
    ./embedded_test gpio-read 0 || return 1
    
    echo -e "${YELLOW}Writing GPIO 0 = 0...${NC}"
    ./embedded_test gpio-write 0 0 || return 1
    
    echo -e "${GREEN}[✓] GPIO tests passed${NC}"
}

test_pwm() {
    echo -e "${BLUE}[*] Testing PWM functionality...${NC}"
    
    echo -e "${YELLOW}Setting PWM channel 0 (1kHz, 50% duty)...${NC}"
    ./embedded_test pwm 0 1000 50 || return 1
    
    echo -e "${YELLOW}Setting PWM channel 1 (2kHz, 75% duty)...${NC}"
    ./embedded_test pwm 1 2000 75 || return 1
    
    echo -e "${GREEN}[✓] PWM tests passed${NC}"
}

test_adc() {
    echo -e "${BLUE}[*] Testing ADC functionality...${NC}"
    
    for ch in 0 1 2 3; do
        echo -e "${YELLOW}Reading ADC channel $ch...${NC}"
        ./embedded_test adc $ch || return 1
    done
    
    echo -e "${GREEN}[✓] ADC tests passed${NC}"
}

test_status() {
    echo -e "${BLUE}[*] Retrieving device status...${NC}"
    ./embedded_test status || return 1
}

full_test() {
    echo -e "${BLUE}════════════════════════════════════════${NC}"
    echo -e "${BLUE}Full Automated Test Sequence${NC}"
    echo -e "${BLUE}════════════════════════════════════════${NC}\n"
    
    check_device || {
        echo -e "${RED}Device not ready. Load module:${NC}"
        load_module || exit 1
        sleep 1
    }
    
    echo ""
    test_gpio || exit 1
    
    echo ""
    test_pwm || exit 1
    
    echo ""
    test_adc || exit 1
    
    echo ""
    test_status || exit 1
    
    echo -e "\n${GREEN}════════════════════════════════════════${NC}"
    echo -e "${GREEN}✓ All tests passed!${NC}"
    echo -e "${GREEN}════════════════════════════════════════${NC}"
}

show_logs() {
    echo -e "${BLUE}[*] Embedded I/O kernel messages:${NC}"
    dmesg | grep "Embedded I/O" | tail -30
}

show_help() {
    cat << EOF
${BLUE}Embedded I/O Module Test Script${NC}

${YELLOW}Build Commands:${NC}
  build          Build kernel module only
  build-test     Build test program only
  build-all      Build everything (module + test program)
  clean          Remove all build artifacts

${YELLOW}Module Commands (require root):${NC}
  load           Load module into kernel
  unload         Unload module from kernel
  reload         Unload and reload module
  check          Check if device exists

${YELLOW}Test Commands (require root & loaded module):${NC}
  test           Run full automated test suite
  test-gpio      Test GPIO functionality
  test-pwm       Test PWM functionality
  test-adc       Test ADC functionality
  test-status    Get device status
  logs           Show kernel messages
  help           Show this help

${YELLOW}Examples:${NC}
  sudo $0 build-all
  sudo $0 load
  sudo $0 test
  sudo $0 test-gpio
  sudo $0 logs
  sudo $0 unload

${YELLOW}Typical Workflow:${NC}
  1. sudo $0 build-all     # Build everything
  2. sudo $0 load          # Load module
  3. sudo $0 test          # Run tests
  4. sudo $0 logs          # Check results
  5. sudo $0 unload        # Clean up
EOF
}

# Main script
case "${1:-help}" in
    build)
        check_root
        build_module
        ;;
    build-test)
        check_root
        build_test_program
        ;;
    build-all)
        check_root
        build_module || exit 1
        echo ""
        build_test_program || exit 1
        ;;
    clean)
        echo -e "${BLUE}[*] Cleaning build artifacts...${NC}"
        make -f Makefile_embedded clean
        rm -f embedded_test
        echo -e "${GREEN}[✓] Cleaned${NC}"
        ;;
    load)
        check_root
        load_module
        ;;
    unload)
        check_root
        unload_module
        ;;
    reload)
        check_root
        unload_module
        sleep 1
        load_module
        ;;
    check)
        check_device
        ;;
    test)
        check_root
        full_test
        ;;
    test-gpio)
        check_root
        test_gpio
        ;;
    test-pwm)
        check_root
        test_pwm
        ;;
    test-adc)
        check_root
        test_adc
        ;;
    test-status)
        check_root
        test_status
        ;;
    logs)
        show_logs
        ;;
    help)
        show_help
        ;;
    *)
        echo -e "${RED}[✗] Unknown command: $1${NC}"
        show_help
        exit 1
        ;;
esac

echo ""
