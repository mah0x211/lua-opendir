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


## dir, err = opendir( name [, follow_symlink] )

open a directory stream corresponding to the directory `name`.

**Parameters**

- `name:string`: directory name.
- `follow_symlink:boolean`: follow symbolic links. (default: `true`)

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


