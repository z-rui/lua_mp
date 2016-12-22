# lua_mp

See `Makefile` for building steps.

See `gibbons.lua` for usage. 

## Use builtin operators

`meta.lua` enables you to write code like `z = x * y % 3` instead of

    local z = mp.z()
    z:mul(x, y)
    z:mod(z, 3)

However it is generally slower as more allocations are made.
