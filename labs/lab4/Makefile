PROG=lab4

SRCS = lab4.c mouse.c kbc.c utils.c

CFLAGS += -pedantic -DLAB4 -I../lab2

DPADD += ${LIBLCF}
LDADD += -llcf -L. -L../lab2 -ltimer

.include <minix.lcom.mk>
