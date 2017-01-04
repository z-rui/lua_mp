CC=gcc
CPP=cpp
CFLAGS=-Wall -O2 -g
LIBS+=-lgmp

SO=mp.so

all: $(SO)

mpz.c: template.c
	$(CPP) -DMPZ $< | tr '$$' 'z' > $@

mpq.c: template.c
	$(CPP) -DMPQ $< | tr '$$' 'q' > $@

mpf.c: template.c
	$(CPP) -DMPF $< | tr '$$' 'f' > $@

$(SO): mp.c mpz.c mpq.c mpf.c rand.c
	$(CC) $(CFLAGS) -shared -o $@ $< $(LIBS)

