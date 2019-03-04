#define _POSIX_C_SOURCE 200112L

#include <netdb.h>
#include <string.h>

#include <lua.h>
#include <lauxlib.h>
#include <maxminddb.h>

#if LUA_VERSION_NUM >= 502

#define luaL_register(L,n,f) luaL_newlib(L,f)

#else

static void luaL_setmetatable(lua_State *L, const char *tname)
{
	luaL_getmetatable(L, tname);
	lua_setmetatable(L, -2);
}

static int lua_absindex(lua_State *L, int idx)
{
	return (idx > 0 || idx <= LUA_REGISTRYINDEX)? idx : lua_gettop(L) + idx + 1;
}

static void luaL_setfuncs(lua_State *L, const luaL_Reg *l, int nup)
{
	int i, t = lua_absindex(L, -1 - nup);

	for (; l->name; l++) {
		for (i = 0; i < nup; i++)
			lua_pushvalue(L, -nup);
		lua_pushcclosure(L, l->func, nup);
		lua_setfield(L, t, l->name);
	}
	lua_pop(L, nup);
}

#endif

static void addclass(lua_State *L, const char *name, const luaL_Reg *methods, const luaL_Reg *metamethods)
{
	if (!luaL_newmetatable(L, name)) return;
	if (metamethods)
		luaL_setfuncs(L, metamethods, 0);
	if (methods) {
		lua_newtable(L);
		luaL_setfuncs(L, methods, 0);
		lua_setfield(L, -2, "__index");
	}
	lua_pop(L, 1);
}

static void *allocudata(lua_State *L, size_t size, const char *tname)
{
	void *p = memset(lua_newuserdata(L, size), 0, size);
	luaL_setmetatable(L, tname);
	return p;
}

static int mm_error(lua_State *L, int err)
{
	lua_pushstring(L, MMDB_strerror(err));
	return lua_error(L);
}

#define MMDB_RESULT_CLASS "MMDB_lookup_result_s"

static int mmdbres_get(lua_State *L)
{
	MMDB_entry_data_s data;
	MMDB_lookup_result_s *res = luaL_checkudata(L, 1, MMDB_RESULT_CLASS);
	const char *keys[16];
	int r, i;

	for (i = 2; i <= lua_gettop(L) && i < 15; i++)
		keys[i-2] = luaL_checkstring(L, i);
	keys[i-2] = 0;

	r = MMDB_aget_value(&res->entry, &data, keys);
	if (!data.has_data) return mm_error(L, r);

	switch (data.type) {
	case MMDB_DATA_TYPE_BYTES:
		lua_pushlstring(L, (const char *)data.bytes, data.data_size);
		return 1;
	case MMDB_DATA_TYPE_UTF8_STRING:
		lua_pushlstring(L, data.utf8_string, data.data_size);
		return 1;
	case MMDB_DATA_TYPE_DOUBLE:
		lua_pushnumber(L, data.double_value);
		return 1;
	case MMDB_DATA_TYPE_FLOAT:
		lua_pushnumber(L, data.float_value);
		return 1;
	case MMDB_DATA_TYPE_UINT16:
		lua_pushinteger(L, data.uint16);
		return 1;
	case MMDB_DATA_TYPE_UINT32:
		lua_pushinteger(L, data.uint32);
		return 1;
	case MMDB_DATA_TYPE_INT32:
		lua_pushinteger(L, data.int32);
		return 1;
	}

	lua_pushstring(L, "Unsupported maxmind data type");
	return lua_error(L);
}

static const luaL_Reg mmdbres_methods[] = {
	{ "get", mmdbres_get },
	{ 0, 0 },
};


#define MMDB_CLASS "MMDB_s"

static int mmdb_gc(lua_State *L)
{
	MMDB_s *mmdb = luaL_checkudata(L, 1, MMDB_CLASS);
	MMDB_close(mmdb);
	return 0;
}

static const luaL_Reg mmdb_metatable[] = {
	{ "__gc", mmdb_gc },
	{ 0, 0 },
};

static int mmdb_lookup(lua_State *L)
{
	int gaierr, mmerr;
	MMDB_s *mmdb = luaL_checkudata(L, 1, MMDB_CLASS);
	const char *addr = luaL_checkstring(L, 2);
	MMDB_lookup_result_s *res = allocudata(L, sizeof(MMDB_lookup_result_s), MMDB_RESULT_CLASS);

	*res = MMDB_lookup_string(mmdb, addr, &gaierr, &mmerr);
	if (gaierr != 0) {
		lua_pushstring(L, gai_strerror(gaierr));
		return lua_error(L);
	}
	if (!res->found_entry) return mm_error(L, mmerr);

	return 1;
}

static const luaL_Reg mmdb_methods[] = {
	{ "lookup", mmdb_lookup },
	{ 0, 0 },
};

static int mm_open(lua_State *L)
{
	const char *filename = luaL_checkstring(L, 1);
	MMDB_s *mmdb = allocudata(L, sizeof(MMDB_s), MMDB_CLASS);
	int r;

	r = MMDB_open(filename, 0, mmdb);
	if (r != MMDB_SUCCESS) return mm_error(L, r);

	return 1;
}

LUALIB_API int luaopen_maxminddb(lua_State * L)
{
	static const luaL_Reg api[] = {
		{ "open", mm_open },
		{ 0, 0 },
	};

	luaL_register(L, "maxminddb", api);
	addclass(L, MMDB_CLASS, mmdb_methods, mmdb_metatable);
	addclass(L, MMDB_RESULT_CLASS, mmdbres_methods, NULL);
	return 1;
}
