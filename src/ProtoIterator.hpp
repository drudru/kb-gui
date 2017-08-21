
#pragma once


template <typename T>
struct ProtoIterator
{
    virtual T    get_next() = 0;
    virtual void rewind()   = 0;
};

