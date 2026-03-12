#include <minix/syslib.h>

int main() {
    sef_startup(); // register self as a service
    freopen("/dev/console", "w", stdout); // redirect stdout to console
    int ret = sys_outb(0x70, 0xA); // Write the value 0xA to I/O port 0x70
    printf("sys_outb returned: %d\n", ret);
    return 0;
}