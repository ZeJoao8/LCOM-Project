PROG=lab5

SRCS = lab5.c

CFLAGS += -pedantic -DLAB5
LAB3_DIR = ../lab3


SRCS = lab5.c video_gr.c 



CPPFLAGS += -I$(LAB3_DIR)



CFLAGS += -pedantic -DLAB5 




DPADD += ${LIBLCF}
LDADD += -llcf


# Compile lab3 sources into local .o before the main build
lab5.o video_gr.o: kbc.o utils.o
 
kbc.o: ${LAB3_DIR}/kbc.c
	${CC} ${CFLAGS} ${CPPFLAGS} -c ${LAB3_DIR}/kbc.c -o kbc.o
 
utils.o: ${LAB3_DIR}/utils.c
	${CC} ${CFLAGS} ${CPPFLAGS} -c ${LAB3_DIR}/utils.c -o utils.o
 
DPADD += kbc.o utils.o
LDADD += kbc.o utils.o



.include <minix.lcom.mk>
