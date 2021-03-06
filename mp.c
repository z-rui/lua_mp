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

static void q_checksanity(lua_State *L, int i, mpq_ptr z)
{
	/* sanity check to prevent fp exception */
	luaL_argcheck(L, mpz_sgn(mpq_denref(z)) > 0, i, "non-canonicalized rational");
}

static void f__get_default_prec(lua_State *L)
{
	luaL_getmetatable(L, "mpf_t");
	lua_getfield(L, -1, "__index"); /* lib table */
	lua_getfield(L, -1, "precision");
	lua_insert(L, -3);
	lua_pop(L, 2); /* metatable and lib table */
}

static void *mp_new(lua_State *L, char t)
{
	char T[] = "mp$_t";
	void *z = 0;
	mp_bitcnt_t prec;

	if (t == 'f') {
		lua_Integer val;
		int isint;

		val = lua_tointegerx(L, -1, &isint);
		if (!isint || !CAN_HOLD(mp_bitcnt_t, val))
			luaL_error(L, "bad precision");
		prec = val;
		lua_pop(L, 1); /* precision */
	}
	T[2] = t;
	if (luaL_getmetatable(L, T) == LUA_TNIL)
		luaL_error(L, "missing metatable");
	switch (t) {
		case 'z':
			z = lua_newuserdata(L, sizeof (mpz_t));
			mpz_init(z);
			break;
		case 'q':
			z = lua_newuserdata(L, sizeof (mpq_t));
			mpq_init(z);
			break;
		case 'f':
			z = lua_newuserdata(L, sizeof (mpf_t));
			mpf_init2(z, prec);
			break;
	}
	lua_insert(L, -2);
	lua_setmetatable(L, -2);
	return z;
}

static void mp__set_str(lua_State *L, int i, int base, void *z, char t)
{
	const char *s;
	int rc = -1;

	s = lua_tostring(L, i);
	switch (t) {
		case 'z': rc = mpz_set_str(z, s, base); break;
		case 'q': rc = mpq_set_str(z, s, base); break;
		case 'f': rc = mpf_set_str(z, s, base); break;
	}
	if (rc != 0) {
		if (base) {
			lua_pushfstring(L, "invalid string in base %d", base);
			luaL_argerror(L, i, lua_tostring(L, -1));
		} else {
			luaL_argerror(L, i, "invalid string");
		}
	}
}

static void mp__set_int(lua_State *L, int i, void *z, char t)
{
	lua_Integer val;

	val = lua_tointeger(L, i);
	if (CAN_HOLD(long, val)) {
		switch (t) {
			case 'z': mpz_set_si(z, (long) val); break;
			case 'q': mpq_set_si(z, (long) val, 1); break;
			case 'f': mpf_set_si(z, (long) val); break;
		}
	} else {
		mp__set_str(L, i, 0, z, t);
	}
}

static void mp__set(lua_State *L, int i, void *z, char t)
{
	switch (lua_type(L, i)) {
		case LUA_TNUMBER:
			if (lua_isinteger(L, i)) {
				mp__set_int(L, i, z, t);
			} else if (t != 'z') {
				lua_Number val;

				val = lua_tonumber(L, i);
				luaL_argcheck(L, val == val && val * 0.0 == 0.0, i,
						"infinity or NaN");
				switch (t) {
					case 'q': mpq_set_d(z, val); break;
					case 'f': mpf_set_d(z, val); break;
				}
			} else {
integer_check_fail:
				luaL_argerror(L, i, "number has no integer representation");
			}
			break;
		case LUA_TSTRING:
			mp__set_str(L, i, 0, z, t);
			break;
		case LUA_TUSERDATA: {
			void *p;

			switch (_testmp(L, i, "zqf*", &p)) {
				case 'z':
					switch (t) {
						case 'z': mpz_set(z, p); break;
						case 'q': mpq_set_z(z, p); break;
						case 'f': mpf_set_z(z, p); break;
					}
					break;
				case 'q':
					switch (t) {
						case 'z':
							if (mpz_cmp_si(mpq_denref((mpq_ptr) p), 1) != 0)
								goto integer_check_fail;
							mpz_set(z, mpq_numref((mpq_ptr) p));
							break;
						case 'q':
							mpq_set(z, p);
							break;
						case 'f':
							mpf_set_q(z, p);
							break;
					}
					break;
				case 'f':
					switch (t) {
						case 'z':
							if (!mpf_integer_p(p))
								goto integer_check_fail;
							mpz_set_f(z, p);
							break;
						case 'q':
							mpq_set_f(z, p);
							break;
						case 'f':
							mpf_set(z, p);
							break;
					}
					break;
				default:
					goto error;
			}
			break;
		}
		default:
error:
			lua_pushfstring(L, "number expected, got %s", lua_typename(L, lua_type(L, i)));
			luaL_argerror(L, i, lua_tostring(L, -1));
	}
}

static void *_tomp(lua_State *L, int i, char t)
{
	char s[] = "$*";
	void *z = 0;

	s[0] = t;
	_testmp(L, i, s, &z);
	if (!z) {
		luaL_checkany(L, i);
		if (t == 'f')
			f__get_default_prec(L);
		z = mp_new(L, t);
		mp__set(L, i, z, t);
		lua_replace(L, i);
	}
	if (t == 'q')
		q_checksanity(L, i, z);
	return z;
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
