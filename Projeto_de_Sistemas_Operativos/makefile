CC=gcc
FLAGS=-pthread -Wall -g
PROG=test
OBJS=projetoSO.o

all: ${PROG}

clean:
			rm ${OBJS} *~ ${PROG}

${PROG}:	${OBJS}
			${CC} ${FLAGS} ${OBJS} -lm -o $@

.c.o:
			${CC} ${FLAGS} $< -c -o $@

##########################################

03.o:	projetoSO.c