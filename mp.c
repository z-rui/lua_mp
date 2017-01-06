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

static mp_bitcnt_t _castbitcnt(lua_State *L, int i)
{
	lua_Unsigned val;

	val = (lua_Unsigned) luaL_checkinteger(L, i);
	luaL_argcheck(L, CAN_HOLD(mp_bitcnt_t, val), i,
			"mp_bitcnt_t overflow");
	return (mp_bitcnt_t) val;
}

static FILE *_checkfile(lua_State *L, int i)
{
	luaL_Stream *fs;

	fs = luaL_checkudata(L, i, LUA_FILEHANDLE);
	if (!fs->closef)
		    luaL_error(L, "attempt to use a closed file");
	return fs->f;
}

static int _testmp(lua_State *L, int i, const char *s, void **pz)
{
	char t;
	char T[] = "mp$_t";

	while ((t = *s++) != '\0') {
		void *z;

		T[2] = t;
		if (t == '*')
			return 0;
		z = luaL_testudata(L, i, T);
		if (z) {
			if (t == 'z') {
				if (lua_getuservalue(L, i) == LUA_TUSERDATA)
					z = *(mpz_ptr *) z; /* partial ref */
				lua_pop(L, 1); /* remove uservalue */
			}
			*pz = z;
			break;
		}
	}
	if (!t)
		luaL_argerror(L, i, "type error");
	return t;
}

static int _gc(lua_State *L)
{
	void *z;

	switch (_testmp(L, 1, "zqf", &z)) {
		case 'z': mpz_clear(z); break;
		case 'q': mpq_clear(z); break;
		case 'f': mpf_clear(z); break;
	}
	lua_pushnil(L);
	lua_setmetatable(L, 1);
	return 0;
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

static int _tostring(lua_State *L)	/** tostring(x) */
{
	void *z;
	char t;
	int base, absbase;
	luaL_Buffer B;
	size_t sz;
	char *s;

	t = _testmp(L, 1, "zqf", &z);
	base = _check_outbase(L, 2);
	absbase = abs(base);
	switch (t) {
		case 'z':
			sz = mpz_sizeinbase(z, absbase) + 2;
			s = luaL_buffinitsize(L, &B, sz);
			mpz_get_str(s, base, z);
			luaL_pushresultsize(&B, strlen(s));
			break;
		case 'q':
			sz = mpz_sizeinbase(mpq_numref((mpq_ptr) z), absbase)
			   + mpz_sizeinbase(mpq_denref((mpq_ptr) z), absbase)
			   + 3;
			s = luaL_buffinitsize(L, &B, sz);
			mpq_get_str(s, base, z);
			luaL_pushresultsize(&B, strlen(s));
			break;
		case 'f': {
			size_t n_digits;
			mp_exp_t exp;

			n_digits = (size_t) luaL_optinteger(L, 3, 0);
			if (n_digits == 0) {
				/* guess buffer size */
				n_digits = absbase;
				sz = 0;
				while (n_digits >>= 1) ++sz;
				sz = mpf_get_prec(z) / sz + 10;
			} else {
				sz = n_digits + 4;
			}
			s = luaL_buffinitsize(L, &B, sz);
			mpf_get_str(s+1, &exp, base, n_digits, z);
			/* assert(strlen(s+1) < sz-1); */
			sz = strlen(s+1);
			if (sz == 0) {
				memcpy(s, "0.", 2);
				luaL_addsize(&B, 2);
			} else {
				size_t n;
				int scientific = 0;

				n = s[1] == '-';
				if (0 <= exp && exp <= sz) {
					n += exp;
				} else {
					n++;
					scientific = 1;
				}
				memmove(s, s+1, n);
				s[n] = '.';
				luaL_addsize(&B, sz+1);
				if (scientific) {
					luaL_addchar(&B, (absbase > 10) ? '@' : 'e');
					lua_pushfstring(L, "%I", (lua_Integer) exp - 1);
					luaL_addvalue(&B);
				}
			}
			luaL_pushresult(&B);
		}
	}

	return 1;
}

static void q_checksanity(lua_State *L, int i, mpq_ptr z)
{
	/* sanity check to prevent fp exception */
	luaL_argcheck(L, mpz_sgn(mpq_denref(z)) > 0, i, "non-canonicalized rational");
}
static int _tonumber(lua_State *L)
{
	void *z;
	double val = 0;

	switch (_testmp(L, 1, "zqf", &z)) {
		case 'z':
			/* FIXME 'signed long' maybe shorter than 'lua_Integer' */
			if (mpz_fits_slong_p(z)) {
				long val;

				val = mpz_get_si(z);
				lua_pushinteger(L, val);
				return 1;
			}
			val = mpz_get_d(z);
			break;
		case 'q':
			q_checksanity(L, 1, z);
			val = mpq_get_d(z);
			break;
		case 'f':
			val = mpf_get_d(z);
			break;
	}
	lua_pushnumber(L, val);
	return 1;
}

#include "mpz.c"
#include "mpq.c"
#include "mpf.c"
#include "rand.c"

LUALIB_API int luaopen_mp(lua_State *L)
{
	lua_newtable(L);
	lua_pushfstring(L, MYVERSION, gmp_version);
	lua_setfield(L, -2, "version");
	luaL_requiref(L, "mp.z", luaopen_mp_z, 0);
	lua_setfield(L, -2, "z");
	luaL_requiref(L, "mp.q", luaopen_mp_q, 0);
	lua_setfield(L, -2, "q");
	luaL_requiref(L, "mp.f", luaopen_mp_f, 0);
	lua_setfield(L, -2, "f");
	luaL_requiref(L, "mp.rand", luaopen_mp_rand, 0);
	lua_setfield(L, -2, "rand");

	return 1;
}
