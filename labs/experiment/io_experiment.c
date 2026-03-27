#include <minix/syslib.h>
#include <minix/sef.h>
#include <stdio.h>

int main() {
    sef_startup(); // register as service
    freopen("/dev/console", "w", stdout);

    int ret = sys_outb(0x70, 0xA);

    printf("sys_outb returned: %d\n", ret);

    return 0;
}