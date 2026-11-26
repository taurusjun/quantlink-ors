/*
 * CallbackBase.h
 *
 *  Created on: 13-Aug-2012
 *      Author: Varun
 */

#ifndef CALLBACKBASE_H_
#define CALLBACKBASE_H_

#include "CallbackData.h"

template <typename ArgType, typename ReturnType>
class CallbackBase
{
public:
    ReturnType operator()(ArgType param_) const { return _thunk(_data, param_); }
    CallbackBase() {}

protected:
    typedef ReturnType (*THUNK)(const CallbackData &, ArgType);
    CallbackBase(THUNK thunk_, const void *client, const void *memFunc) : _thunk(thunk_), _data(client, memFunc)
    {
    }

private:
    THUNK _thunk;
    CallbackData _data;
};

// Arguments but NO Return value
template <typename ArgType>
class CallbackBase<ArgType, void>
{
public:
    void operator()(ArgType param_) const { _thunk(_data, param_); }
    CallbackBase() {}

protected:
    typedef void (*THUNK)(const CallbackData &, ArgType);

    CallbackBase(THUNK thunk_, const void *client, const void *memFunc) : _thunk(thunk_), _data(client, memFunc)
    {
    }

private:
    THUNK _thunk;
    CallbackData _data;
};

// No arguments but Return value
template <typename ReturnType>
class CallbackBase<void, ReturnType>
{
public:
    ReturnType operator()(void) const { return _thunk(_data); }
    CallbackBase() {}

protected:
    typedef ReturnType (*THUNK)(const CallbackData &);

    CallbackBase(THUNK thunk_, const void *client, const void *memFunc) : _thunk(thunk_), _data(client, memFunc)
    {
    }

private:
    THUNK _thunk;
    CallbackData _data;
};

// No arguments, No return value
template <>
class CallbackBase<void, void>
{
public:
    void operator()(void) const { _thunk(_data); }
    CallbackBase() {}

protected:
    typedef void (*THUNK)(const CallbackData &);

    CallbackBase(THUNK thunk_, const void *client, const void *memFunc) : _thunk(thunk_), _data(client, memFunc)
    {
    }

private:
    THUNK _thunk;
    CallbackData _data;
};

#endif /* CALLBACKBASE_H_ */
