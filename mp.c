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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gmp.h"

#include "lua.h"
#include "lauxlib.h"

#define MYVERSION	"mp library for " LUA_VERSION " / Dec 2016 / "\
			"using GMP %s"

#define CAN_HOLD(type, val) ((type) (val) == (val))

static void _conversion_error(lua_State *L, int i, const char *typ, int base)
{
	lua_pushfstring(L, "cannot convert to %s", typ);
	if (base) {
		lua_pushfstring(L, " in base %d", base);
		lua_concat(L, 2);
	}
	luaL_argerror(L, i, lua_tostring(L, -1));
}

static void _check_divisor(lua_State *L, mpz_ptr divisor)
{
	if (mpz_sgn(divisor) == 0)
		luaL_error(L, "division by zero");
}

static unsigned long _castulong(lua_State *L, int i)
{
	lua_Unsigned val;

	val = (lua_Unsigned) luaL_checkinteger(L, i);
	luaL_argcheck(L, CAN_HOLD(unsigned long, val), i,
			"unsigned long overflow");
	return (unsigned long) val;
}

static FILE *_checkfile(lua_State *L, int i)
{
	luaL_Stream *fs;

	fs = luaL_checkudata(L, i, LUA_FILEHANDLE);
	if (!fs->closef)
		    luaL_error(L, "attempt to use a closed file");
	return fs->f;
}

static int _open_common(lua_State *L)
{
	lua_pushvalue(L, -2);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1); /* pop metatable */
	lua_newtable(L);
	lua_pushvalue(L, -3);
	lua_setfield(L, -2, "__call");
	lua_setmetatable(L, -2);

	return 1;
}

static int _check_outbase(lua_State *L, int i)
{
	lua_Integer base;

	base = luaL_optinteger(L, i, 10);
	luaL_argcheck(L, (-36 <= base && base <= -2) || (2 <= base && base <= 62), i, "base not in range [-36,-2] and [2,62]");
	return (int) base;
}

static int _check_inbase(lua_State *L, int i)
{
	lua_Integer base;

	base = luaL_optinteger(L, i, 0);
	luaL_argcheck(L, base == 0 || (2 <= base && base <= 62), 2, "base neither 0 nor in range [2,62]");
	return (int) base;
}

static mpz_ptr _checkmpz(lua_State *L, int, int);
static mpq_ptr _checkmpq(lua_State *L, int, int);

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
