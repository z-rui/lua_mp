# lua_mp

To compile and install:

    $ cpp -DMPZ template.c | sed 's/\$/z/g' > mpz.c
    $ cpp -DMPQ template.c | sed 's/\$/q/g' > mpq.c
    $ gcc $CFLAGS -shared -o mp.so mp.c -lgmp
    $ cp mp.so $PREFIX/lib/lua/5.3/

See `gibbons.lua` for usage. 

## Use builtin operators

`meta.lua` enables you to write code like `z = x * y % 3` instead of

    local z = mp.z()
    z:mul(x, y)
    z:mod(z, 3)

However it is generally slower as more allocations are made.
