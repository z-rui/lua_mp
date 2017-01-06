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

static void _conversion_error(lua_State *L, int i, char t, int base)
{
	const char *T = 0;

	switch (t) {
		case 'z': T = "integer"; break;
		case 'q': T = "rational"; break;
		case 'f': T = "float"; break;
	}
	lua_pushfstring(L, "cannot convert to %s", T);
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
		prec = _castbitcnt(L, -1);
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
	if (rc != 0)
		_conversion_error(L, i, t, base);
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
				goto error;
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
								goto error;
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
								goto error;
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
			_conversion_error(L, i, '$', 0);
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
