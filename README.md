# lua_mp

To compile and install:

    $ cpp -DMPZ template.c | sed 's/\$/z/g' > mpz.c
    $ cpp -DMPQ template.c | sed 's/\$/q/g' > mpq.c
    $ gcc $CFLAGS -shared -o mp.so mp.c -lgmp
    $ cp mp.so $PREFIX/lib/lua/5.3/

See `gibbons.lua` for usage. 
