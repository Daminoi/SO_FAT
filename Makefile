CFLAGS=
PROGRAM_NAME=so_fat_exec

all: so_fat

so_fat: common.h fatFS.h fatFS.c minilogger.h minilogger.c main.c trashBash.h trashBash.c fsFunctions.h fsFunctions.c fsUtils.h fsUtils.c
	gcc $(CFLAGS) -o $(PROGRAM_NAME) minilogger.c fatFS.c fsFunctions.c fsUtils.c main.c trashBash.c

.PHONY: clean
clean:
	rm -f $(PROGRAM_NAME)