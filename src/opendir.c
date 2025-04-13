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
#include <sys/stat.h>
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

static int normalize(lua_State *L, char *path, size_t len)
{
    int top    = 0;
    char *head = path;
    char *tail = path + len;
    char *p    = head;

CHECK_FIRST:
    switch (*p) {
    case '/':
ADD_SEGMENT:
        // add segments
        luaL_checkstack(L, 2, NULL);
        if ((uintptr_t)head < (uintptr_t)p) {
            lua_pushlstring(L, head, (uintptr_t)p - (uintptr_t)head);
            top++;
        }
        // add '/'
        lua_pushliteral(L, "/");
        top++;

        // skip multiple slashes
        while (*p == '/') {
            p++;
        }
        head = p;
        if (*p != '.') {
            break;
        }

    case '.':
        // foud '.' segment
        if (p[1] == '/' || p[1] == 0) {
            p += 1;
        }
        // found '..' segment
        else if (p[1] == '.' && (p[2] == '/' || p[2] == 0)) {
            p += 2;
            switch (top) {
            case 1:
                // remove previous segment if it is not slash
                if (*lua_tostring(L, 1) != '/') {
                    lua_settop(L, 0);
                    top = 0;
                }
                break;

            default:
                // remove previous segment with trailing-slash
                if (top > 1 && strcmp(lua_tostring(L, -2), "..") != 0) {
                    lua_pop(L, 2);
                    top -= 2;
                    break;
                }

            case 0:
                // add '..' segment
                luaL_checkstack(L, 2, NULL);
                lua_pushliteral(L, "..");
                lua_pushliteral(L, "/");
                top += 2;
                break;
            }
        } else {
            // allow segments that started with '.' character
            break;
        }

        // skip multiple slashes
        while (*p == '/') {
            p++;
        }
        head = p;
        goto CHECK_FIRST;
    }

    // search '/' character
    while (*p) {
        if (*p == '/') {
            goto ADD_SEGMENT;
        }
        p++;
    }

    // found NULL before the end of the string
    if (p != tail) {
        errno = EILSEQ;
        return -1;
    }

    // add last-segment
    if ((uintptr_t)head < (uintptr_t)tail) {
        luaL_checkstack(L, 1, NULL);
        lua_pushlstring(L, head, (uintptr_t)tail - (uintptr_t)head);
        top++;
    } else if (!top) {
        errno = EINVAL;
        return -1;
    }

    // remove trailing-slash
    if (top > 1 && *(char *)lua_tostring(L, top) == '/') {
        lua_pop(L, 1);
    }

    // check a last segment
    path = (char *)lua_tostring(L, -1);
    if (strcmp(path, "/") == 0 || strcmp(path, "..") == 0) {
        errno = EINVAL;
        return -1;
    }

    return 0;
}

static size_t PATHBUF_SIZ = PATH_MAX;
static char *PATHBUF      = NULL;

static int opendir_nofollow(lua_State *L, char *path, size_t len)
{
    int fd  = -1;
    int top = 0;

    // check path length
    if (len > PATHBUF_SIZ) {
        errno = ENAMETOOLONG;
        return -1;
    }
    // normalize a path
    path      = memcpy(PATHBUF, path, len);
    path[len] = 0;
    lua_settop(L, 0);
    if (normalize(L, path, len) != 0) {
        return -1;
    }
    top = lua_gettop(L);

    // verify the existence of each path segment
    len = 0;
    for (int idx = 1; idx <= top; idx++) {
        size_t slen     = 0;
        const char *seg = lua_tolstring(L, idx, &slen);
        struct stat buf = {0};

        memcpy(path + len, seg, slen);
        len += slen;
        path[len] = 0;
        if (idx != top) {
            switch (slen) {
            case 2:
                // ignore '.' and '..' segment
                if (seg[0] == '.' && seg[2] == '.') {
                    continue;
                }
                break;
            case 1:
                if (*seg == '/') {
                    continue;
                }
            }
        }

        if (lstat(path, &buf) != 0) {
            return -1;
        } else if (!S_ISDIR(buf.st_mode)) {
            // non directory exists
            errno = ENOTDIR;
            return -1;
        }
    }

    lua_settop(L, 0);
    fd = open(path, O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (fd != -1) {
        DIR **dir = lua_newuserdata(L, sizeof(DIR *));
        if ((*dir = fdopendir(fd))) {
            lauxh_setmetatable(L, DIR_MT);
            return 0;
        }
    }
    return -1;
}

static int opendir_follow(lua_State *L, const char *path)
{
    DIR **dir = lua_newuserdata(L, sizeof(DIR *));

    if ((*dir = opendir(path))) {
        luaL_getmetatable(L, DIR_MT);
        lua_setmetatable(L, -2);
        return 0;
    }
    return -1;
}

static int opendir_lua(lua_State *L)
{
    size_t len         = 0;
    const char *path   = lauxh_checklstring(L, 1, &len);
    int follow_symlink = lauxh_optboolean(L, 2, 1);

    if (follow_symlink ? opendir_follow(L, path) :
                         opendir_nofollow(L, (char *)path, len)) {
        lua_pushnil(L);
        lua_errno_new(L, errno, "opendir");
        return 2;
    }
    return 1;
}

LUALIB_API int luaopen_opendir(lua_State *L)
{
    long pathmax = pathconf(".", _PC_PATH_MAX);

    lua_errno_loadlib(L);

    // set the maximum number of bytes in a pathname
    if (pathmax != -1) {
        PATHBUF_SIZ = pathmax;
    }
    // allocate the buffer for getcwd
    PATHBUF = lua_newuserdata(L, PATHBUF_SIZ + 1);
    // holds until the state closes
    luaL_ref(L, LUA_REGISTRYINDEX);

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
