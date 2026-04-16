# lua-opendir

[![test](https://github.com/mah0x211/lua-opendir/actions/workflows/test.yml/badge.svg)](https://github.com/mah0x211/lua-opendir/actions/workflows/test.yml)
[![codecov](https://codecov.io/gh/mah0x211/lua-opendir/branch/master/graph/badge.svg)](https://codecov.io/gh/mah0x211/lua-opendir)

open a directory stream.

## Installation

```
luarocks install opendir
```

## Usage

```lua
local opendir = require('opendir')
local dir = assert(opendir('/tmp'))

-- read directory entries
local entry = dir:readdir()
while entry do
    print(entry)
    entry = dir:readdir()
end

dir:closedir()
```

## Error Handling

the following functions return the `error` object created by https://github.com/mah0x211/lua-errno module.


## dir, err = opendir( name [, follow_symlink [, toctou]] )

open a directory stream corresponding to the directory `name`.

**Parameters**

- `name:string`: directory name.
- `follow_symlink:boolean`: follow symbolic links. (default: `true`)
  - `true`: symbolic links are followed at all path components.
  - `false`: symbolic link at the final path component is rejected (`ENOTDIR`). Intermediate symbolic links are still followed (POSIX `O_NOFOLLOW` semantics).
- `toctou:boolean`: enable TOCTOU-safe traversal via `openat(2)`. (default: `false`)
  - When `true`, each path segment is opened with `openat(2)` relative to the previously opened directory file descriptor, eliminating the race window between path resolution steps.
  - When `follow_symlink=false`, `O_NOFOLLOW` is passed to every `openat(2)` call, so symbolic links at any position are rejected (`ENOTDIR`).

**Behavior by parameter combination**

| `follow_symlink` | `toctou` | symlink behavior |
|:---:|:---:|---|
| `true` | `false` | all symbolic links are followed |
| `true` | `true` | all symbolic links are followed, TOCTOU-safe |
| `false` | `false` | intermediate symbolic links followed; final component rejected (`ENOTDIR`) |
| `false` | `true` | symbolic links at all positions rejected (`ENOTDIR`) |

**Returns**

- `dir:dir*`: a directory stream.
- `err:error`: error object on failure.


## ok, err = dir:closedir()

close a directory stream.

**Returns**

- `ok:boolean`: `true` on success.
- `err:error`: error object on failure.


**NOTE** 

the directory stream will be closed automatically on GC.


## entry, err = dir:readdir()

get the next directory entry.

**Returns**

- `entry:string`: a directory entry.
- `err:error`: error object on failure.


## ok, err = dir:rewinddir()

reset the read location to the beginning of a directory.

**Returns**

- `ok:boolean`: `true` on success.
- `err:error`: error object on failure.


