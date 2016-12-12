/*
 * lmapm.c
 * big-number library for Lua 5.3 based on the MAPM library
 *
 * Zhang Rui <zrui16@hotmail.com>
 *
 * Luiz Henrique de Figueiredo <lhf@tecgraf.puc-rio.br>
 * 09 May 2012 22:21:40
 * This code is hereby placed in the public domain.
 */

#include <stdlib.h>
#include <string.h>

#include "gmp.h"

#include "lua.h"
#include "lauxlib.h"

#define MYVERSION	"mp library for " LUA_VERSION " / Dec 2016 / "\
			"using GMP %s"

#define CAN_HOLD(type, val) ((type) (val) == (val))

static void _conversion_error(lua_State *L, const char *typ, int base)
{
	if (base)
		luaL_error(L, "cannot convert string to %s in base %d", typ, base);
	else
		luaL_error(L, "cannot convert string to %s", typ);
}

static void _check_divisor(lua_State *L, mpz_ptr divisor)
{
	if (mpz_sgn(divisor) == 0)
		luaL_error(L, "division by zero");
}

static int _open_common(lua_State *L)
{
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");

	lua_newtable(L);
	lua_newtable(L);
	lua_pushvalue(L, -3);
	lua_setfield(L, -2, "__index");
	lua_pushvalue(L, -4);
	lua_setfield(L, -2, "__call");
	lua_setmetatable(L, -2);

	return 1;
}

#include "mpz.c"
#include "mpq.c"

LUALIB_API int luaopen_mp(lua_State *L)
{
	lua_newtable(L);
	lua_pushfstring(L, MYVERSION, gmp_version);
	lua_setfield(L, -2, "version");
	luaL_requiref(L, "mp.z", luaopen_mp_z, 0);
	lua_setfield(L, -2, "z");
	luaL_requiref(L, "mp.q", luaopen_mp_q, 0);
	lua_setfield(L, -2, "q");


	return 1;
}
