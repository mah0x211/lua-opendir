/**
 *  Copyright (C) 2022 Masatoshi Fukunaga
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a
 *  copy of this software and associated documentation files (the "Software"),
 *  to deal in the Software without restriction, including without limitation
 *  the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *  and/or sell copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 *  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
// lua
#include <lua_errno.h>

#define DIR_MT "dir"

static int rewinddir_lua(lua_State *L)
{
    DIR *dir = *((DIR **)luaL_checkudata(L, 1, DIR_MT));

    if (dir) {
        rewinddir(dir);
        lua_pushboolean(L, 1);
        return 1;
    }
    // dir has already been closed
    lua_pushboolean(L, 0);
    errno = EBADF;
    lua_errno_new(L, errno, "rewinddir");
    return 2;
}

static int readdir_lua(lua_State *L)
{
    DIR *dir             = *((DIR **)luaL_checkudata(L, 1, DIR_MT));
    struct dirent *entry = NULL;

    errno = 0;
    if (!dir) {
        errno = EBADF;
    } else if ((entry = readdir(dir))) {
        lua_pushstring(L, entry->d_name);
        return 1;
    }

    lua_pushnil(L);
    if (errno) {
        // got error
        lua_errno_new(L, errno, "readdir");
        return 2;
    }
    return 1;
}

static int closedir_lua(lua_State *L)
{
    DIR **dir = (DIR **)luaL_checkudata(L, 1, DIR_MT);

    if (*dir) {
        DIR *dirp = *dir;

        *dir = NULL;
        if (closedir(dirp) != 0) {
            lua_pushboolean(L, 0);
            lua_errno_new(L, errno, "closedir");
            return 2;
        }
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int tostring_lua(lua_State *L)
{
    luaL_checkudata(L, 1, DIR_MT);
    lua_pushfstring(L, DIR_MT ": %p", lua_topointer(L, 1));
    return 1;
}

static int gc_lua(lua_State *L)
{
    DIR *dir = *((DIR **)luaL_checkudata(L, 1, DIR_MT));

    if (dir) {
        closedir(dir);
    }

    return 0;
}

static int opendir_lua(lua_State *L)
{
    size_t len         = 0;
    const char *path   = lauxh_checklstring(L, 1, &len);
    int follow_symlink = lauxh_optboolean(L, 2, 1);

    if (follow_symlink) {
        DIR **dir = lua_newuserdata(L, sizeof(DIR *));
        if ((*dir = opendir(path))) {
            luaL_getmetatable(L, DIR_MT);
            lua_setmetatable(L, -2);
            return 1;
        }
    } else {
        // POSIX: O_NOFOLLOW applies to the final path component only
        int fd = open(path, O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
        if (fd != -1) {
            DIR **dir = lua_newuserdata(L, sizeof(DIR *));
            if ((*dir = fdopendir(fd))) {
                lauxh_setmetatable(L, DIR_MT);
                return 1;
            }
            close(fd);
        }
    }

    lua_pushnil(L);
    lua_errno_new(L, errno, "opendir");
    return 2;
}

LUALIB_API int luaopen_opendir(lua_State *L)
{
    lua_errno_loadlib(L);

    // create metatable
    if (luaL_newmetatable(L, DIR_MT)) {
        struct luaL_Reg mmethod[] = {
            {"__gc",       gc_lua      },
            {"__tostring", tostring_lua},
            {NULL,         NULL        }
        };
        struct luaL_Reg method[] = {
            {"closedir",  closedir_lua },
            {"readdir",   readdir_lua  },
            {"rewinddir", rewinddir_lua},
            {NULL,        NULL         }
        };

        // metamethods
        for (struct luaL_Reg *ptr = mmethod; ptr->name; ptr++) {
            lua_pushcfunction(L, ptr->func);
            lua_setfield(L, -2, ptr->name);
        }
        // methods
        lua_newtable(L);
        for (struct luaL_Reg *ptr = method; ptr->name; ptr++) {
            lua_pushcfunction(L, ptr->func);
            lua_setfield(L, -2, ptr->name);
        }
        lua_setfield(L, -2, "__index");
        lua_pop(L, 1);
    }

    lua_pushcfunction(L, opendir_lua);

    return 1;
}
