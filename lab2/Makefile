# name of the program (Minix service)
PROG=lab2 

# source code files to be compiled
SRCS = lab2.c timer.c utils.c

# additional compilation flags
# "-Wall -Wextra -Werror -I . -std=c11 -Wno-unused-parameter" are already set
CFLAGS += -pedantic

DPADD += ${LIBLCF} ${LIB_NAME}              # Let make track the versions of the new target
LDADD += -llcf -L.                          # Tell the compiler to look in the current directory for libraries
LDADD += -ltimer                            #   and link the library named libtimers.a

LIB_NAME = libtimer.a                       # The name filename for your library
LIB_SRCS = timer.c utils.c                  # The source files to include
LIB_OBJS = ${LIB_SRCS:.c=.o}                # The rule used to convert your .c source files in .o object files
CLEANFILES += ${LIB_NAME} ${LIB_OBJS}       # Add the library file and object files to the files to be cleaned

all: lib                                    # Instruct make run the lib compilation target before building the lab2 program
lib: ${LIB_OBJS}                            # Define the lib compilation target, with its dependency source files
	ar rcs ${LIB_NAME} ${LIB_OBJS}          # Package the library
# ^ This MUST be a Tab character, not spaces
# Otherwise you will get a "missing separator" error!

# include LCOM's makefile that does all the "heavy lifting"
.include <minix.lcom.mk>
>>>>>>> ZeJoao
