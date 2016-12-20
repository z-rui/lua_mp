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

static void _conversion_error(lua_State *L, const char *srctyp, const char *typ, int base)
{
	if (base)
		luaL_error(L, "cannot convert %s to %s in base %d", srctyp, typ, base);
	else
		luaL_error(L, "cannot convert %s to %s", srctyp, typ);
}

static void _check_divisor(lua_State *L, mpz_ptr divisor)
{
	if (mpz_sgn(divisor) == 0)
		luaL_error(L, "division by zero");
}

static unsigned long _checkulong(lua_State *L, int i)
{
	lua_Integer val;

	val = luaL_checkinteger(L, i);
	luaL_argcheck(L, CAN_HOLD(unsigned long, val), i, "integer overflow");
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

static mpz_ptr _tompz(lua_State *, int);
static mpq_ptr _tompq(lua_State *, int);
static mpz_ptr _checkmpz(lua_State *, int, int);
static mpq_ptr _checkmpq(lua_State *, int, int);

static int mp__cmp(lua_State *L)
{
	void *a, *b;
	int ta, tb, ret, isint, issi = 0;
	long si;

	ta = lua_type(L, 1);
	tb = lua_type(L, 2);
	if (tb == LUA_TNUMBER) {
		lua_Integer val;
		si = val = lua_tointegerx(L, 2, &isint);
		issi = isint && CAN_HOLD(long, val);
	}
	if ((a = _checkmpq(L, 1, 0))) {
		if (issi) {
			ret = mpq_cmp_si((mpq_ptr) a, si, 1);
		} else if ((b = _checkmpz(L, 2, 0))) {
			ret = mpq_cmp_z(a, b);
		} else {
			b = _tompq(L, 2);
			ret = mpq_cmp(a, b);
		}
	} else if ((a = _checkmpz(L, 1, 0)) != 0) {
		switch (tb) {
			case LUA_TNUMBER:
				if (issi) {
					ret = mpz_cmp_si((mpz_ptr) a, si);
				} else if (!isint) {
					ret = mpz_cmp_d(a, lua_tonumber(L, 2));
				} else {
					goto z_general_case;
				}
				break;
			case LUA_TUSERDATA:
				if ((b = _checkmpq(L, 2, 0))) {
					ret = -mpq_cmp_z(b, a);
					break;
				}
				/* fallthrough */
			default:
z_general_case:
				b = _tompz(L, 2);
				ret = mpz_cmp(a, b);
		}
	} else if (ta == LUA_TUSERDATA || tb != LUA_TUSERDATA) {
		return luaL_error(L, "unsupported comparison");
	} else {
		lua_insert(L, -2); /* swap the two on top */
		return -mp__cmp(L);
	}
	return ret;
}

static int mp_cmp(lua_State *L)
{
	lua_pushinteger(L, mp__cmp(L));
	return 1;
}

static int mp_eq(lua_State *L)
{
	mpq_ptr a, b;
	a = _checkmpq(L, 1, 0);
	if (a && (b = _checkmpq(L, 2, 0))) {
		lua_pushboolean(L, mpq_equal(a, b));
	} else {
		lua_pushboolean(L, mp__cmp(L) == 0);
	}
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
