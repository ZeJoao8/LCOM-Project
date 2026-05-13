PROG=proj

# 1. List ONLY the file names, without any folder paths!
SRCS = main.c timer.c kbc.c mouse.c video_gr.c utils.c

# 2. Tell the MINIX make tool where to look for these .c files
.PATH.c: ./src ./src/devices ./src/game ./include/game

# 3. Compiler flags
CFLAGS += -pedantic

# 4. Tell the compiler where to look for .h files
CFLAGS += -I./include

# 5. LCF Dependencies
DPADD += ${LIBLCF}
LDADD += -llcf 

.include <minix.lcom.mk>