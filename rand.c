static int rand_init(lua_State *L)
{
	static const char *modelst[] = {
		"default", "mt", "lc_2exp", "lc_2exp_size", "set"
	};
	int mode;
	gmp_randstate_t *state, *state1;
	mpz_ptr a;
	unsigned long c;
	mp_bitcnt_t n;

	mode = luaL_checkoption(L, 1, "default", modelst);
	state = lua_newuserdata(L, sizeof (gmp_randstate_t));
	if (luaL_getmetatable(L, "gmp_randstate_t") == LUA_TNIL)
		luaL_error(L, "gmp_randstate_t not registered");
	lua_setmetatable(L, -2);
	switch (mode) {
		default: /* default */
			gmp_randinit_default(*state);
			break;
		case 1: /* mt */
			gmp_randinit_mt(*state);
			break;
		case 2: /* lc_2exp */
			a = _tompz(L, 2);
			c = _castulong(L, 3);
			n = _castbitcnt(L, 4);
			gmp_randinit_lc_2exp(*state, a, c, n);
			break;
		case 3: /* lc_2exp_size */
			n = _castbitcnt(L, 2);
			gmp_randinit_lc_2exp_size(*state, n);
			break;
		case 4: /* set */
			state1 = luaL_testudata(L, 2, "gmp_randstate_t");
			gmp_randinit_set(*state, *state1);
	}
	return 1;
}

static int rand_gc(lua_State *L)
{
	gmp_randstate_t *state;

	state = luaL_checkudata(L, 1, "gmp_randstate_t");
	gmp_randclear(*state);
	lua_pushnil(L);
	lua_setmetatable(L, -2);
	return 0;
}

static int rand_seed(lua_State *L)
{
	gmp_randstate_t *state;
	lua_Integer val;
	int isnum;

	state = luaL_checkudata(L, 1, "gmp_randstate_t");
	val = lua_tointegerx(L, 2, &isnum);
	if (isnum && val >= 0 && CAN_HOLD(unsigned long, val)) {
		gmp_randseed_ui(*state, val);
	} else {
		gmp_randseed(*state, _tompz(L, 2));
	}
	return 0;
}

static int rand__urandom(lua_State *L, unsigned long (*op)(gmp_randstate_t, unsigned long))
{
	gmp_randstate_t *state;
	unsigned long n, res;

	state = luaL_checkudata(L, 1, "gmp_randstate_t");
	n = _castulong(L, 2);
	res = (*op)(*state, n);
	lua_pushinteger(L, res);
	return 1;
}

static int rand_urandomb(lua_State *L)
{
	return rand__urandom(L, gmp_urandomb_ui);
}

static int rand_urandomm(lua_State *L)
{
	return rand__urandom(L, gmp_urandomm_ui);
}

static luaL_Reg rand_reg[] = {
	{ "init", rand_init },
	{ "seed", rand_seed },
	{ "urandomb", rand_urandomb },
	{ "urandomm", rand_urandomm },
	{ 0, 0 }
};

LUALIB_API int luaopen_mp_rand(lua_State *L)
{
	luaL_newlib(L, rand_reg);
	luaL_newmetatable(L, "gmp_randstate_t");
	lua_pushcfunction(L, &rand_gc);
	lua_setfield(L, -2, "__gc");
	lua_pushvalue(L, -2);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);
	return 1;
}
