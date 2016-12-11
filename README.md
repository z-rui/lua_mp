# lua_mp

To compile and install:

    $ gcc $CFLAGS -shared -o mp.so mp.c -lgmp
    $ cp mp.so $PREFIX/lib/lua/5.3/

See `gibbons.lua` for usage. 
