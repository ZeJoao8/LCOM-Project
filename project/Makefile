PROG=proj

# 1. List ONLY the file names, without any folder paths!
SRCS = main.c timer.c kbc.c mouse.c video_gr.c utils.c rtc.c serial.c \
       game.c model.c view.c controller.c net.c

# 2. Tell the MINIX make tool where to look for these .c files
.PATH.c: ./src ./src/game ./include/game \
         ./devices/kbc ./devices/mouse ./devices/rtc ./devices/serial \
         ./devices/timer ./devices/utils ./devices/video_gr

# 3. Compiler flags
CFLAGS += -pedantic

# 4. Tell the compiler where to look for .h files
CFLAGS += -I./include
CFLAGS += -I./src/img
CFLAGS += -I.            # devices/<dev>/<file>.h headers

# 5. LCF Dependencies
DPADD += ${LIBLCF}
LDADD += -llcf 

.include <minix.lcom.mk>