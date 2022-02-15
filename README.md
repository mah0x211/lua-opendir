# lua-opendir

[![test](https://github.com/mah0x211/lua-opendir/actions/workflows/test.yml/badge.svg)](https://github.com/mah0x211/lua-opendir/actions/workflows/test.yml)

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


## dir, err, errno = opendir( name [, follow_symlink] )

open a directory stream corresponding to the directory `name`.

**Parameters**

- `name:string`: directory name.
- `follow_symlink:boolean`: follow symbolic links. (default: `true`)

**Returns**

- `dir:dir*`: a directory stream.
- `err:string`: error message on failure.
- `errno:integer`: error number.


## ok, err, errno = dir:closedir()

close a directory stream.

**Returns**

- `ok:boolean`: `true` on success.
- `err:string`: error message on failure.
- `errno:integer`: error number.


**NOTE** 

the directory stream will be closed automatically on GC.


## entry, err, errno = dir:readdir()

get the next directory entry.

**Returns**

- `entry:string`: a directory entry.
- `err:string`: error message on failure.
- `errno:integer`: error number.


## ok, err, errno = dir:rewinddir()

reset the read location to the beginning of a directory.

**Returns**

- `ok:boolean`: `true` on success.
- `err:string`: error message on failure.
- `errno:integer`: error number.


