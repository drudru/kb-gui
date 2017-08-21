
#pragma once

#include "common.h"

#include "ProtoIterator.hpp"


#pragma pack(push, 1)
struct KBConstStringList : ProtoIterator<char *>
{

private:
    char ** _list  = nullptr;
    U16     _index = 0;
    U8      _done  = 1;
public:

    char * get_next()
    {
        if (_done)
            panic();

        char * result = _list[_index];

        if (result == NULL)
            _done = 1;

        _index++;

        return result;
    }

    void   rewind()
    {
        if (!_list)
            panic();

        _index = 0;
        _done  = 0;
    }

    void set_list(const char * list[])
    {
        if (!list)
            panic();

        _list = (char **)list;
        rewind();
    }
};

#pragma pack(pop)
