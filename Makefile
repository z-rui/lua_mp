CC=gcc
CPP=cpp
CFLAGS=-Wall -O2 -g
LIBS+=-lgmp

SO=mp.so

all: $(SO)

mpz.c: template.c
	$(CPP) -DMPZ $< | sed 's/\$$/z/g' > $@

mpq.c: template.c
	$(CPP) -DMPQ $< | sed 's/\$$/q/g' > $@

$(SO): mp.c mpz.c mpq.c rand.c
	$(CC) $(CFLAGS) -shared -o $@ $< $(LIBS)

