local assert = require('assert')
local errno = require('errno')
local opendir = require('opendir')

local function test_opendir()
    -- test that get directory stream
    local dir, err = assert(opendir('./test/testdir/bardir'))
    assert.is_nil(err)
    assert.match(tostring(dir), 'dir*: ')
    assert(dir:closedir())

    -- test that get directory stream from symlink
    dir, err = assert(opendir('./test/testdir_symlink/bardir'))
    assert.is_nil(err)
    assert.match(tostring(dir), 'dir*: ')
    assert(dir:closedir())

    -- test that return error
    dir, err = opendir('./unknowndir')
    assert.is_nil(dir)
    assert.equal(err.type, errno.ENOENT)

    -- test that throws an error
    err = assert.throws(opendir, 1)
    assert.match(err, '#1 .+ [(]string expected', false)
end

local function test_opendir_nofollow()
    -- test that get directory stream
    local dir, err = assert(opendir('./test/testdir', false))
    assert(dir, err)
    assert.is_nil(err)
    assert.match(tostring(dir), 'dir*: ')
    assert(dir:closedir())

    -- test that get directory stream
    dir, err = assert(opendir('./test/testdir/foo/../bar/..', false))
    assert(dir, err)
    assert.is_nil(err)
    assert.match(tostring(dir), 'dir*: ')
    assert(dir:closedir())

    -- test that cannot get directory stream from symlink
    dir, err = opendir('./test/testdir_symlink/bardir', false)
    assert.is_nil(dir)
    assert.equal(err.type, errno.ENOTDIR)

    -- test that return ENAMETOOLONG error
    dir, err = opendir(string.rep('x', 4096), false)
    assert.is_nil(dir)
    assert.equal(err.type, errno.ENAMETOOLONG)

    -- test that throws an error
    err = assert.throws(opendir, './test/testdir', {})
    assert.match(err, '#2 .+ [(]boolean expected', false)
end

local function test_closedir()
    local dir = assert(opendir('./test/testdir'))

    -- test that closedir can be called more than once
    assert(dir:closedir())
    assert(dir:closedir())
end

local function test_readdir()
    local dir = assert(opendir('./test/testdir'))

    -- test that return directory entry
    local entries = {}
    local entry, err = assert(dir:readdir())
    while entry do
        entries[entry] = true
        entry = dir:readdir()
    end
    assert(not err, err)
    assert.equal(entries, {
        ['.'] = true,
        ['..'] = true,
        ['foo.txt'] = true,
        ['bardir'] = true,
    })

    -- test that return nil
    assert.is_nil(dir:readdir())

    -- test that return error after closedir
    assert(dir:closedir())
    entry, err = dir:readdir()
    assert.is_nil(entry)
    assert.equal(err.type, errno.EBADF)
end

local function test_rewinddir()
    local dir = assert(opendir('./test/testdir'))
    local entries = {}
    local entry = dir:readdir()
    while entry do
        entries[entry] = true
        entry = dir:readdir()
    end
    assert.equal(entries, {
        ['.'] = true,
        ['..'] = true,
        ['foo.txt'] = true,
        ['bardir'] = true,
    })

    -- test that resets the reading position to the top
    assert(dir:rewinddir())
    entries = {}
    entry = dir:readdir()
    while entry do
        entries[entry] = true
        entry = dir:readdir()
    end
    assert.equal(entries, {
        ['.'] = true,
        ['..'] = true,
        ['foo.txt'] = true,
        ['bardir'] = true,
    })

    -- test that return error after closedir
    assert(dir:closedir())
    local ok, err = dir:rewinddir()
    assert.is_false(ok)
    assert.equal(err.type, errno.EBADF)
end

for _, fn in ipairs({
    test_opendir,
    test_opendir_nofollow,
    test_closedir,
    test_readdir,
    test_rewinddir,
}) do
    assert(pcall(fn))
end
