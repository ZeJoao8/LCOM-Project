# name of the program (Minix service)
PROG=lab2 

# source code files to be compiled (Apenas o lab2.c fica aqui agora!)
SRCS = lab2.c

# additional compilation flags
# "-Wall -Wextra -Werror -I . -std=c11 -Wno-unused-parameter" are already set
CFLAGS += -pedantic

LIB_NAME = libtimer.a
LIB_SRCS = timer.c utils.c
LIB_OBJS = ${LIB_SRCS:.c=.o}
CLEANFILES += ${LIB_NAME} ${LIB_OBJS}

# list of library dependencies
DPADD += ${LIBLCF} ${LIB_NAME}
# -L. diz para procurar bibliotecas na pasta atual
# -ltimer diz para linkar o ficheiro libtimer.a
LDADD += -llcf -L. -ltimer

# Regras de compilação (Têm de vir antes do .include final)
all: lib

lib: ${LIB_OBJS}
	ar rcs ${LIB_NAME} ${LIB_OBJS}

# include LCOM's makefile that does all the "heavy lifting"
.include <minix.lcom.mk>