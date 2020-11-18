#define lua_c

#include "lprefix.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <signal.h>

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"


#if !defined(LUA_PROGNAME)
#define LUA_PROGNAME            "lua"
#endif

#if !defined(LUA_INIT_VAR)
#define LUA_INIT_VAR            "LUA_INIT"
#endif

#define LUA_INITVARVERSION      LUA_INIT_VAR LUA_VERSUFFIX


static lua_State *globalL = NULL;

static const char *progname = LUA_PROGNAME;


/*
** Hook set by signal function to stop the interpreter.
*/
static void lstop (lua_State *L, lua_Debug *ar) {
  (void)ar;  /* unused arg. */
  lua_sethook(L, NULL, 0, 0);  /* reset hook */
  luaL_error(L, "interrupted!");
}


/*
** Function to be called at a C signal. Because a C signal cannot
** just change a Lua state (as there is no proper synchronization),
** this function only sets a hook that, when called, will stop the
** interpreter.
*/
static void laction (int i) {
  int flag = LUA_MASKCALL | LUA_MASKRET | LUA_MASKLINE | LUA_MASKCOUNT;
  signal(i, SIG_DFL); /* if another SIGINT happens, terminate process */
  lua_sethook(globalL, lstop, flag, 1);
}



/*
** Prints an error message, adding the program name in front of it
** (if present)
*/
static void l_message (const char *pname, const char *msg) {
  if (pname) lua_writestringerror("%s: ", pname);
  lua_writestringerror("%s\n", msg);
}


/*
** Check whether 'status' is not OK and, if so, prints the error
** message on the top of the stack. It assumes that the error object
** is a string, as it was either generated by Lua or by 'msghandler'.
*/
static int report (lua_State *L, int status) {
  if (status != LUA_OK) {
    const char *msg = lua_tostring(L, -1);
    l_message(progname, msg);
    lua_pop(L, 1);  /* remove message */
  }
  return status;
}

/*
** Message handler used to run all chunks
*/
static int msghandler (lua_State *L) {
  const char *msg = lua_tostring(L, 1);
  if (msg == NULL) {  /* is error object not a string? */
    if (luaL_callmeta(L, 1, "__tostring") &&  /* does it have a metamethod */
        lua_type(L, -1) == LUA_TSTRING)  /* that produces a string? */
      return 1;  /* that is the message */
    else
      msg = lua_pushfstring(L, "(error object is a %s value)",
                               luaL_typename(L, 1));
  }
  luaL_traceback(L, L, msg, 1);  /* append a standard traceback */
  return 1;  /* return the traceback */
}


/*
** Interface to 'lua_pcall', which sets appropriate message function
** and C-signal handler. Used to run all chunks.
*/
static int docall (lua_State *L, int narg, int nres) {
  int status;
  int base = lua_gettop(L) - narg;  /* function index */
  lua_pushcfunction(L, msghandler);  /* push message handler */
  lua_insert(L, base);  /* put it under function and args */
  globalL = L;  /* to be available to 'laction' */
  signal(SIGINT, laction);  /* set C-signal handler */
  status = lua_pcall(L, narg, nres, base);
  signal(SIGINT, SIG_DFL); /* reset C-signal handler */
  lua_remove(L, base);  /* remove message handler from the stack */
  return status;
}


static int dochunk (lua_State *L, int status) {
  if (status == LUA_OK) status = docall(L, 0, 0);
  return report(L, status);
}


/* mark in error messages for incomplete statements */
#define EOFMARK         "<eof>"
#define marklen         (sizeof(EOFMARK)/sizeof(char) - 1)

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    lua_State *L = luaL_newstate();
    if (L == NULL) {
        return 0;
    }
    dochunk(L, luaL_loadbuffer(L, data, size, "test"));

    lua_close(L);
    return 0;
}
