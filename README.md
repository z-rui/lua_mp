# lua_mp

To compile and install:

    $ gcc $CFLAGS -shared -o <library name> mp.c -llua -lgmp
    $ cp <library name> $PREFIX/share/lua/5.3/

See `gibbons.lua` for usage. 
