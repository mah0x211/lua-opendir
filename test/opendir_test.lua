local testcase = require('testcase')
local errno = require('error').errno
local opendir = require('opendir')

function testcase.opendir()
    -- test that get directory stream
    local dir, err, eno = opendir('./testdir/bardir')
    assert(dir, err)
    assert.is_nil(eno)
    assert.match(tostring(dir), 'dir*: ')
    assert(dir:closedir())

    -- test that get directory stream from symlink
    dir, err, eno = opendir('./testdir_symlink/bardir')
    assert(dir, err)
    assert.is_nil(eno)
    assert.match(tostring(dir), 'dir*: ')
    assert(dir:closedir())

    -- test that return error
    dir, err, eno = opendir('./unknowndir')
    assert.is_nil(dir)
    assert.match(err, 'unknowndir.+ No .+ directory', false)
    assert.equal(errno[eno], errno.ENOENT)

    -- test that throws an error
    err = assert.throws(opendir, 1)
    assert.match(err, '#1 .+ [(]string expected', false)
end

function testcase.opendir_nofollow()
    -- test that get directory stream
    local dir, err, eno = opendir('./testdir', false)
    assert(dir, err)
    assert.is_nil(eno)
    assert.match(tostring(dir), 'dir*: ')
    assert(dir:closedir())

    -- test that cannot get directory stream from symlink
    dir, err, eno = opendir('./testdir_symlink/bardir', false)
    assert.is_nil(dir)
    assert.is_string(err)
    assert.equal(errno[eno], errno.ENOTDIR)

    -- test that throws an error
    err = assert.throws(opendir, './testdir', {})
    assert.match(err, '#2 .+ [(]boolean expected', false)
end

function testcase.closedir()
    local dir = assert(opendir('./testdir'))

    -- test that closedir can be called more than once
    assert(dir:closedir())
    assert(dir:closedir())
end

function testcase.readdir()
    local dir = assert(opendir('./testdir'))

    -- test that return directory entry
    local entries = {}
    local entry, err, eno = dir:readdir()
    while entry do
        entries[entry] = true
        entry = dir:readdir()
    end
    assert(not err, err)
    assert.is_nil(eno)
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
    entry, err, eno = dir:readdir()
    assert.is_nil(entry)
    assert.is_string(err)
    assert.equal(errno[eno], errno.EBADF)
end

function testcase.rewinddir()
    local dir = assert(opendir('./testdir'))
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
    local ok, err, eno = dir:rewinddir()
    assert.is_false(ok)
    assert.is_string(err)
    assert.equal(errno[eno], errno.EBADF)
end

