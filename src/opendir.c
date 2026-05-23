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
#include <limits.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
// lua
#include <lauxlib.h>
#include <lua.h>
// external libraries
#include "lauxhlib.h"
#include "lua_errno.h"

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

// Returns the length of the next path segment in [*cur, end).
// Advances *cur past the segment. *seg points to the start of the segment.
// Consecutive '/' characters are skipped. Returns 0 at end of string.
// Does NOT interpret '.' or '..' — passes them through as-is.
static size_t get_segment(const char **cur, const char *end, const char **seg)
{
    const char *p = *cur;

    while (p < end && *p == '/') {
        p++;
    }
    if (p >= end) {
        *cur = p;
        return 0;
    }
    *seg = p;
    while (p < end && *p != '/') {
        p++;
    }
    *cur = p;
    return (size_t)(p - *seg);
}

// Opens a directory by traversing path one segment at a time using openat(2).
// When nofollow is non-zero, O_NOFOLLOW is passed to every openat call so
// that symlinks at any position in the path are rejected with ENOTDIR.
// When nofollow is zero, symlinks are followed at every position.
// Each openat call is atomic with respect to the previously opened fd,
// providing TOCTOU safety for the traversed prefix.
static int opendir_toctou(lua_State *L, char *path, size_t len, char *pathbuf,
                          size_t pathbuf_siz, int nofollow)
{
    const char *cur = path;
    const char *end = path + len;
    const char *seg = NULL;
    size_t slen     = 0;
    int flags       = O_DIRECTORY | O_CLOEXEC | (nofollow ? O_NOFOLLOW : 0);
    int dirfd       = AT_FDCWD;

    if (len == 0) {
        errno = EINVAL;
        return -1;
    }
    if (len > pathbuf_siz) {
        errno = ENAMETOOLONG;
        return -1;
    }

    if (path[0] == '/') {
        dirfd = open("/", flags);
        if (dirfd < 0) {
            return -1;
        }
    }

    while ((slen = get_segment(&cur, end, &seg)) > 0) {
        int newfd;
        memcpy(pathbuf, seg, slen);
        pathbuf[slen] = '\0';
        newfd         = openat(dirfd, pathbuf, flags);
        if (dirfd != AT_FDCWD) {
            close(dirfd);
        }
        dirfd = newfd;
        if (dirfd < 0) {
            return -1;
        }
    }

    lua_settop(L, 0);
    DIR **dir = lua_newuserdata(L, sizeof(DIR *));
    if ((*dir = fdopendir(dirfd))) {
        lauxh_setmetatable(L, DIR_MT);
        return 0;
    }
    close(dirfd);
    return -1;
}

static int opendir_lua(lua_State *L)
{
    size_t len         = 0;
    const char *path   = lauxh_checklstring(L, 1, &len);
    int follow_symlink = lauxh_optboolean(L, 2, 1);
    int toctou         = lauxh_optboolean(L, 3, 0);
    size_t pathbuf_siz = (size_t)lua_tointeger(L, lua_upvalueindex(1));
    char *pathbuf      = lua_touserdata(L, lua_upvalueindex(2));

    if (toctou) {
        if (opendir_toctou(L, (char *)path, len, pathbuf, pathbuf_siz,
                           !follow_symlink) == 0) {
            return 1;
        }
    } else if (follow_symlink) {
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
    long pathmax       = pathconf(".", _PC_PATH_MAX);
    size_t pathbuf_siz = (pathmax != -1) ? (size_t)pathmax : PATH_MAX;

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

    // upvalue 1: path buffer size
    lua_pushinteger(L, (lua_Integer)pathbuf_siz);
    // upvalue 2: path buffer (held by closure until state closes)
    lua_newuserdata(L, pathbuf_siz + 1);
    lua_pushcclosure(L, opendir_lua, 2);

    return 1;
}
