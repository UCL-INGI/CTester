CC=gcc
EXEC=tests
LDFLAGS=-lcunit -lm -lpthread -ldl -rdynamic
SRC=$(wildcard *.c) $(wildcard CTester/*.c)
OBJ=$(SRC:.c=.o)
CFLAGS=-Wall -Werror -DC99 -std=gnu99 -ICTester -D_POSIX_C_SOURCE=200112L -D_DEFAULT_SOURCE
WRAP += -Wl,-wrap=exit
WRAP += -Wl,-wrap=open -Wl,-wrap=creat -Wl,-wrap=close -Wl,-wrap=read -Wl,-wrap=write -Wl,-wrap=stat -Wl,-wrap=fstat -Wl,-wrap=lseek
WRAP += -Wl,-wrap=getpid
WRAP += -Wl,-wrap=malloc -Wl,-wrap=free -Wl,-wrap=realloc -Wl,-wrap=calloc
WRAP += -Wl,-wrap=pthread_mutex_lock -Wl,-wrap=pthread_mutex_unlock -Wl,-wrap=pthread_mutex_trylock -Wl,-wrap=pthread_mutex_init -Wl,-wrap=pthread_mutex_destroy
WRAP += -Wl,-wrap=sleep

all: $(EXEC)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $< 

$(EXEC): $(OBJ)
	$(CC) $(WRAP) -o $@ $(OBJ) $(LDFLAGS)

create-po:
	mkdir -p po/fr/
	xgettext --keyword=_ --language=C --add-comments --sort-output --from-code=UTF-8 -o po/tests.pot $(SRC)
	msginit --input=po/tests.pot --locale=fr_BE.utf8 --output=po/fr/tests.po

update-po:
	xgettext --keyword=_ --language=C --add-comments --sort-output --from-code=UTF-8 -o po/tests.pot $(SRC)
	msgmerge --update po/fr/tests.po po/tests.pot

compile-mo:
	msgfmt --no-hash --output-file=po/fr/tests.mo po/fr/tests.po
	mkdir -p fr/LC_MESSAGES/
	cp po/fr/tests.mo fr/LC_MESSAGES/tests.mo

clean:
	rm -f $(EXEC) $(OBJ)

.PHONY: tests

