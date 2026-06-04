PROG=proj

# 1. List ONLY the file names, without any folder paths!
SRCS = main.c timer.c kbc.c mouse.c video_gr.c utils.c rtc.c serial.c \
       game.c controller.c net.c scores.c settings.c \
       model.c model_geometry.c model_effects.c model_spawn.c \
       view.c view_draw.c view_sprites.c view_world.c view_background.c view_ui.c

# 2. Tell the MINIX make tool where to look for these .c files
.PATH.c: ./src ./src/game ./src/game/model ./src/game/view ./include/game \
         ./src/devices/kbc ./src/devices/mouse ./src/devices/rtc ./src/devices/serial \
         ./src/devices/timer ./src/devices/utils ./src/devices/video_gr

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