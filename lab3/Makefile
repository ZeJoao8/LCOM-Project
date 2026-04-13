# name of the program (Minix service)
PROG = lab3
# source code files to be compiled
SRCS = lab3.c kbc.c utils.c
# additional compilation flags
CFLAGS += -pedantic
CFLAGS += -DLAB3
# list of library dependencies (for Lab 3, only LCF library)
DPADD += ${LIBLCF}
LDADD += -llcf
# include LCOM's makefile that does all the "heavy lifting"
.include <minix.lcom.mk>
