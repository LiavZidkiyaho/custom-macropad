#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <linux/uinput.h>

#define SERIAL_PORT "/dev/ttyUSB0"

// Emits a keystroke to the Linux kernel
void emit(int fd, int type, int code, int val) {
    struct input_event ie;
    ie.type = type;
    ie.code = code;
    ie.value = val;
    ie.time.tv_sec = 0;
    ie.time.tv_usec = 0;
    write(fd, &ie, sizeof(ie));
}

// Configures the serial port to read raw bytes at 115200 baud
int setup_serial() {
    int fd = open(SERIAL_PORT, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        perror("Error opening serial port");
        return -1;
    }

    struct termios tty;
    tcgetattr(fd, &tty);
    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; // 8-bit chars
    tty.c_iflag &= ~IGNBRK; // disable break processing
    tty.c_lflag = 0;        // no signaling chars, no echo
    tty.c_oflag = 0;        // no remapping, no delays
    tty.c_cc[VMIN]  = 4;    // read blocks until at least 4 bytes arrive (our packet size)
    tty.c_cc[VTIME] = 1;    // 0.1 seconds read timeout

    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl
    tty.c_cflag |= (CLOCAL | CREAD); // ignore modem controls, enable reading
    tcsetattr(fd, TCSANOW, &tty);

    return fd;
}

// Sets up the virtual keyboard in the OS
int setup_uinput() {
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        perror("Error opening uinput");
        return -1;
    }

    // Tell the OS this device can send key presses
    ioctl(fd, UI_SET_EVBIT, EV_KEY);

    // Register the specific keys we plan to use
    ioctl(fd, UI_SET_KEYBIT, KEY_PLAYPAUSE);
    ioctl(fd, UI_SET_KEYBIT, KEY_NEXTSONG);
    ioctl(fd, UI_SET_KEYBIT, KEY_PREVIOUSSONG);
    ioctl(fd, UI_SET_KEYBIT, KEY_MUTE);

    struct uinput_setup usetup;
    memset(&usetup, 0, sizeof(usetup));
    usetup.id.bustype = BUS_USB;
    usetup.id.vendor = 0x1234; // Fake vendor ID
    usetup.id.product = 0x5678; // Fake product ID
    strcpy(usetup.name, "MacroPad_Virtual_Device");

    ioctl(fd, UI_DEV_SETUP, &usetup);
    ioctl(fd, UI_DEV_CREATE);

    return fd;
}

int main() {
    int serial_fd = setup_serial();
    if (serial_fd < 0) return 1;

    int uinput_fd = setup_uinput();
    if (uinput_fd < 0) return 1;

    printf("Daemon running. Listening for packets...\n");

    unsigned char buffer[4];

    while (1) {
        // Read exactly 4 bytes from the Arduino
        int n = read(serial_fd, buffer, 4);
        if (n == 4) {
            // Verify our Sync Byte (0xAA) and Checksum Byte (0x55)
            if (buffer[0] == 0xAA && buffer[3] == 0x55) {
                unsigned char id = buffer[1];
                unsigned char state = buffer[2];

                // Logic Mapper: Map hardware IDs to Linux Keycodes
                if (id == 0x01) { // Button 1
                    if (state == 0x01) {
                        printf("Button 1 Pressed -> Play/Pause\n");
                        emit(uinput_fd, EV_KEY, KEY_PLAYPAUSE, 1); // Press
                        emit(uinput_fd, EV_SYN, SYN_REPORT, 0);    // Sync event
                    } else if (state == 0x00) {
                        emit(uinput_fd, EV_KEY, KEY_PLAYPAUSE, 0); // Release
                        emit(uinput_fd, EV_SYN, SYN_REPORT, 0);    // Sync event
                    }
                }

                // Add more logic here for IDs 0x02 to 0x05...
                // And for the knobs (0x11 to 0x15)...
            }
        }
    }

    // Cleanup (though standard daemons run infinitely)
    ioctl(uinput_fd, UI_DEV_DESTROY);
    close(uinput_fd);
    close(serial_fd);
    return 0;
}