/**
MIT License

Copyright (c) 2025 Keaud, Inc.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define VENDOR  0x0eef
#define PRODUCT 0x0005
#define PKT_LEN 38          

/* Check if a HID device at our base path matches the target Vendor:Product ID for the display */
static int match_vidpid(const char *base) {
    char uevent[512];
    snprintf(uevent, sizeof(uevent), "%s/uevent", base);

    FILE *f = fopen(uevent, "r");
    if (!f) return 0;

    char line[256];
    int ok = 0;

    /* Parse uevent to find our HID_ID entry */
    while (fgets(line, sizeof(line), f)) {
        unsigned int bus, vid, pid;
        /* HID_ID format is: bus:vendor:product (hex) */
        if (sscanf(line, "HID_ID=%x:%x:%x", &bus, &vid, &pid) == 3) {
            if (vid == VENDOR && pid == PRODUCT) ok = 1;
        }
    }

    fclose(f);
    return ok;
}

/* Search /sys/bus/hid/devices for our screen and return its /dev/hidrawX path */
static int find_hidraw(char *out, size_t outsz) {
    DIR *d = opendir("/sys/bus/hid/devices");
    if (!d) return -1;

    struct dirent *e;

    // Iterate through all HID device directories
    while ((e = readdir(d))) {
      // skip . and ..
        if (e->d_name[0] == '.') continue;

        char base[512];
        snprintf(base, sizeof(base), "/sys/bus/hid/devices/%s", e->d_name);

        if (!match_vidpid(base)) continue;

        // Look for hidraw subdirectory
        char hdpath[600];
        snprintf(hdpath, sizeof(hdpath), "%s/hidraw", base);

        DIR *hd = opendir(hdpath);
        if (!hd) continue;

        struct dirent *he;
        // Find the hidrawX device file
        while ((he = readdir(hd))) {
            if (strncmp(he->d_name, "hidraw", 6) == 0) {
                snprintf(out, outsz, "/dev/%s", he->d_name);
                closedir(hd);
                closedir(d);
                return 0;  // For the win! 
            }
        }
        closedir(hd);
    }

    closedir(d);
    return -1;  // Aren't the droids you're looking for
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <0-100>\n", argv[0]);
        return 1;
    }

    int level = atoi(argv[1]);
    if (level < 0 || level > 100) {
        fprintf(stderr, "brightness must be 0-100\n");
        return 1;
    }

    char hidpath[256];
    if (find_hidraw(hidpath, sizeof(hidpath)) != 0) {
        fprintf(stderr, "no matching Waveshare Touchpanel device found\n");
        return 1;
    }

    int fd = open(hidpath, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        perror("open hidraw");
        return 1;
    }

    // Prepare HID packet with brightness command
    unsigned char pkt[PKT_LEN];
    memset(pkt, 0, PKT_LEN);

    pkt[0] = 0x04;
    pkt[1] = 0xaa;
    pkt[2] = 0x01;
    pkt[3] = 0x00;
    pkt[6] = (unsigned char)level;  // brightness value at offset 6

    // Ready, aim, fire...
    ssize_t w = write(fd, pkt, PKT_LEN);
    if (w < 0) {
        perror("write");
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}
