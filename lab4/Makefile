PROG=lab4

SRCS = lab4.c kbc.c utils.c mouse.c	

CFLAGS += -pedantic
CFLAGS += -DLAB3
CFLAGS += -I../lab2

DPADD += ${LIBLCF}
LDADD += -llcf -L. -L../lab2
LDADD += -ltimer


# include LCOM's makefile that does all the "heavy lifting"
.include <minix.lcom.mk>
