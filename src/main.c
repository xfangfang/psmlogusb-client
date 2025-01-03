#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <libusb.h>

#define VENDOR_ID      0x054c   // Sony Corp.
#define PRODUCT_ID     0x069b   // PS Vita

#define ACM_CTRL_DTR   0x01
#define ACM_CTRL_RTS   0x02

static struct libusb_device_handle *devh = NULL;

static int ep_in_addr  = 0x83;

int read_log(unsigned char* data, int size)
{
    int actual_length;
    int rc = libusb_bulk_transfer(devh, ep_in_addr, data, size, &actual_length, 1000);
    if (rc < 0)
        return rc;

    return actual_length;
}

void usb_event()
{
    int rc;
    devh = libusb_open_device_with_vid_pid(NULL, VENDOR_ID, PRODUCT_ID);
    if (!devh) {
        fprintf(stderr, "Error finding USB device (Press Ctrl-C to stop)\n");
        goto out;
    }
    fprintf(stdout, "PSMLOGUSB Started (Press Ctrl-C to stop)\n");

    // detach kernel driver, if attached
    for (int if_num = 0; if_num < 2; if_num++) {
        if (libusb_kernel_driver_active(devh, if_num)) {
            libusb_detach_kernel_driver(devh, if_num);
        }
        rc = libusb_claim_interface(devh, if_num);
        if (rc < 0) {
            fprintf(stderr, "Error claiming interface: %s\n", libusb_error_name(rc));
            goto out;
        }
    }

    rc = libusb_control_transfer(devh, 0x21, 0x22, ACM_CTRL_DTR | ACM_CTRL_RTS, 0, NULL, 0, 0);
    if (rc < 0) {
        fprintf(stderr, "Error during control transfer: %s\n", libusb_error_name(rc));
    }

    // set line encoding: 57600 8N1
    unsigned char encoding[] = { 0x00, 0xe1, 0x00, 0x00, 0x00, 0x00, 0x08 };
    rc = libusb_control_transfer(devh, 0x21, 0x20, 0, 0, encoding, sizeof(encoding), 0);
    if (rc < 0) {
        fprintf(stderr, "Error during control transfer: %s\n", libusb_error_name(rc));
    }

    unsigned char buf[512+1]; // PSMLogUSB uses 512b ringbuffer. add 1 for null
    int len;

    while(1) {
        len = read_log(buf, 512);
        if (len > 0)
        {
            buf[len] = 0;
            fprintf(stdout, "%s",buf);
        } else if (len != LIBUSB_ERROR_TIMEOUT) {
            // usb error
            break;
        }
    }

    libusb_release_interface(devh, 0);

out:
    if (devh)
        libusb_close(devh);
}

static void signal_handler(int sig_num) {
    signal(sig_num, signal_handler);
    fprintf(stdout, "\nByeBye.\n");
    libusb_exit(NULL);
    exit(sig_num);
}

int main(int argc, char **argv)
{
    int rc;

    rc = libusb_init(NULL);
    if (rc < 0) {
        fprintf(stderr, "Error initializing libusb: %s\n", libusb_error_name(rc));
        exit(1);
    }

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    while(1) {
        usb_event();
        sleep(5);
    }

    libusb_exit(NULL);
    return rc;
}
