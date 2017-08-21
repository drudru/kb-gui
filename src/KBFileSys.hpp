
#pragma once

#include "common.h"
#include <sys/types.h>
#include <dirent.h>

#include "ProtoIterator.hpp"


struct KBFilePath
{
private:
    char * _path = nullptr;
    U32    _len  = 0;
public:

    KBFilePath(const char * path)
    {
        if (!path)
            panic();

        _len = strlen(path);
        if (_len > 80)
            panic();

        if (path[0] != '/')
            panic();

        _path = (char *) malloc(1024);
        if (_path == NULL)
            panic();

        strcpy(_path, path);

        // Remove trailing slash
        if (_len > 2 && (_path[_len - 1] == '/'))
        {
            _path[_len - 1] = 0;
            _len--;
        }
    }

    ~KBFilePath()
    {
        if (_path)
            free(_path);
    }

    // Delete the copy constructor and copy assignment operator
    KBFilePath (const KBFilePath&) = delete;
    KBFilePath& operator= (const KBFilePath&) = delete;

    // move constructor
    KBFilePath (KBFilePath&& other)
        : _path(other._path), _len (other._len)
    {
        other._path = nullptr;
        other._len  = 0;
    }
    // move assignment constructor
    KBFilePath& operator= (KBFilePath&& other)
    {
        if (_path)
            free(_path);
        _path = other._path;
        _len  = other._len;

        other._path = nullptr;
        other._len  = 0;
        return *this;
    }

    char * path()
    {
        return _path;
    }

    U32    len()
    {
        return _len;
    }

    KBFilePath add(char * path)
    {
        char buff[1024];
        strcpy(buff, _path);

        U32 plen = strlen(path);

        if ((_len + 1 + plen) >= 1024)
            panic();

        // Append the path separator
        buff[_len]     = '/';
        buff[_len + 1] = 0;
        strcat(buff, path);

        return KBFilePath{buff};
    }

    /**
     * basename - return the last element in the path
     * Example: Path '/abc/123' will return '123'
     */
    char * basename()
    {
        // The special case
        if (_len == 1)
            return _path;

        char * p = _path + (_len - 1);

        while (*p != '/')
            p--;

        return p;
    }

    /**
     * dirname - return the super directory element in the path
     * Example: Path '/abc/123' will return '/abc'
     */
    /* To Be Implemented */
};

struct KBFileDir : ProtoIterator<struct dirent *>
{
private:
    KBFilePath * _path = nullptr;
    DIR        * _pdir = nullptr;
    bool         _done = true;
public:

    KBFileDir(KBFilePath * path)
    {
        if (!path || (path->path() == NULL))
            panic();

        _path = path;
        _pdir = opendir(path->path());

        if (_pdir == NULL)
            panic();

        _done = false;
    }

    struct dirent * get_next()
    {
        if (_done)
            panic();

        struct dirent * pdirent = readdir(_pdir);

        if (pdirent == NULL)
            _done = true;
        //else
        //    printf(" dir: %s\n", pdirent->d_name);

        return pdirent;
    }

    void   rewind()
    {
        rewinddir(_pdir);
        _done = false;
    }

    ~KBFileDir()
    {
        if (_pdir)
            closedir(_pdir);
    }
};

struct KBHumanDir : ProtoIterator<char *>
{
private:
    KBFileDir     * _pdir = nullptr;
    bool            _done = true;
    struct dirent * _pdirent = nullptr;
    char            _buff[18];
public:

    KBHumanDir(KBFileDir * pdir)
    {
        if (!pdir)
            panic();

        _pdir = pdir;
        _done = false;
    }

    char * get_next()
    {
        if (_done)
            panic();

        while (true)
        {
            _pdirent = _pdir->get_next();

            if (_pdirent == NULL)
            {
                _done = true;
                return NULL;
            }

            if ((_pdirent->d_name[0] == '.') || (_pdirent->d_name[0] == '_'))
            {
                continue;
            }

            _buff[17] =  0;
            _buff[16] = 15; // Asterisk like symbol to indicate name overflow
            switch (_pdirent->d_type)
            {
                case DT_REG:
                    _buff[0] = ' ';
                    break;
                case DT_DIR:
                    _buff[0] = '/';
                    break;
                default:
                    continue;
            }

            printf("Hdir: %s type: %d\n", _pdirent->d_name, _pdirent->d_type);

            int len = strlen(_pdirent->d_name);
            // memcpy(dst, src, len)
            if (len <= 15)
                memcpy(_buff + 1, _pdirent->d_name, len + 1); // copies null
            else
                memcpy(_buff + 1, _pdirent->d_name,      15); // does not copy null

            break;
        }

        return _buff;
    }

    void   rewind()
    {
        _pdir->rewind();
        _done = false;
        _pdirent = nullptr;
    }

    struct dirent * get_dirent()
    {
        return _pdirent;
    }
};

struct KBMmapFile
{
private:
    void * _p    = nullptr;
    int    _size = 0;
public:

    KBMmapFile()
    {
    }

    ~KBMmapFile()
    {
        if (_p)
            munmap(_p, _size);
    }

    bool map(char * pathname)
    {
        int fd = open(pathname, O_RDONLY);
        if (fd < 0)
            return false;
    
        struct stat inode;
        {
            int res = fstat(fd, &inode);
            if (res < 0) panic();
        }
    
        _size = inode.st_size;
        // FIX - round up a page for null termination
        _p = mmap (0, _size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
        if (_p == MAP_FAILED) panic();

        ((char *)_p)[_size] = 0;  // Null terminate

        close(fd);

        return true;
    }

    char * ptr()
    {
        return (char *)_p;
    }

    int   size()
    {
        return _size;
    }
};

