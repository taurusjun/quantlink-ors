/*
 *
 * Callback.h
 *
 *  Created on: 13-Aug-2012
 *      Author: Varun
 */

#ifndef CALLBACK_H_
#define CALLBACK_H_

#include "CallbackBase.h"

template <typename Client, typename MemFunc, typename ArgType, typename ReturnType>
class Callback : public CallbackBase<ArgType, ReturnType>
{
public:
    Callback(Client &client, MemFunc pMemFunc) : CallbackBase<ArgType, ReturnType>(thunk, &client, &pMemFunc)
    {
    }

private:
    static ReturnType thunk(const CallbackData &data_, ArgType param_)
    {
        Client *client = (Client *)(data_.getClient());
        MemFunc &memFunc = (*(MemFunc *)(data_.getCallFunc()));
        return (client->*memFunc)(param_);
    }
};

// No Return Type but Arguments
template <typename Client, typename MemFunc, typename ArgType>
class Callback<Client, MemFunc, ArgType, void> : public CallbackBase<ArgType, void>
{
public:
    Callback(Client &client, MemFunc pMemFunc) : CallbackBase<ArgType, void>(thunk, &client, &pMemFunc)
    {
    }

private:
    static void thunk(const CallbackData &data_, ArgType param_)
    {
        Client *client = (Client *)(data_.getClient());
        MemFunc &memFunc = (*(MemFunc *)(data_.getCallFunc()));
        (client->*memFunc)(param_);
    }
};

// No Argument, No ReturnType
template <typename Client, typename MemFunc>
class Callback<Client, MemFunc, void, void> : public CallbackBase<void, void>
{
public:
    Callback(Client &client, MemFunc pMemFunc) : CallbackBase<void, void>(thunk, &client, &pMemFunc)
    {
    }

private:
    static void thunk(const CallbackData &data_)
    {
        Client *client = (Client *)(data_.getClient());
        MemFunc &memFunc = (*(MemFunc *)(data_.getCallFunc()));
        (client->*memFunc)();
    }
};

// Return type but no arguments
template <typename Client, typename MemFunc, typename ReturnType>
class Callback<Client, MemFunc, void, ReturnType> : public CallbackBase<void, ReturnType>
{
public:
    Callback(Client &client, MemFunc pMemFunc) : CallbackBase<void, ReturnType>(thunk, &client, &pMemFunc)
    {
    }

private:
    static ReturnType thunk(const CallbackData &data_)
    {
        Client *client = (Client *)(data_.getClient());
        MemFunc &memFunc = (*(MemFunc *)(data_.getCallFunc()));
        return (client->*memFunc)();
    }
};

/*
 * Helper macros
 */

template <typename Client, typename ArgType, typename ReturnType>
Callback<Client, ReturnType (Client::*)(ArgType), ArgType, ReturnType>
make_callback(CallbackBase<ArgType, ReturnType> *callbackBase, Client &client, ReturnType (Client::*const &memFunc)(ArgType))
{
    typedef ReturnType (Client::*MEMFUNC)(ArgType);
    return Callback<Client, MEMFUNC, ArgType, ReturnType>(client, memFunc);
}

// No Return type but Arguments
template <typename Client, typename ArgType>
Callback<Client, void (Client::*)(ArgType), ArgType, void>
make_callback(CallbackBase<ArgType, void> *callbackBase, Client &client, void (Client::*const &memFunc)(ArgType))
{
    typedef void (Client::*MEMFUNC)(ArgType);
    return Callback<Client, MEMFUNC, ArgType, void>(client, memFunc);
}

// No Return type, NO arguments
template <typename Client>
Callback<Client, void (Client::*)(void), void, void>
make_callback(CallbackBase<void, void> *callbackBase, Client &client, void (Client::*const &memFunc)(void))
{
    typedef void (Client::*MEMFUNC)(void);
    return Callback<Client, MEMFUNC, void, void>(client, memFunc);
}

// Return type but no arguments
template <typename Client, typename ReturnType>
Callback<Client, ReturnType (Client::*)(void), void, ReturnType>
make_callback(CallbackBase<void, ReturnType> *callbackBase, Client &client, ReturnType (Client::*const &memFunc)(void))
{
    typedef ReturnType (Client::*MEMFUNC)(void);
    return Callback<Client, MEMFUNC, void, ReturnType>(client, memFunc);
}

/*
 * For const member functions
 */
// template <typename T, typename Client, typename ReturnType, typename CallType, typename ArgType>
// Callback<T, Client, ReturnType(CallType::*)(ArgType) const >
// make_callback (
// 		CallbackBase<T>* callbackBase,
// 		Client& client,
// 		ReturnType(CallType::* const& memFunc)(ArgType) const
// )
// {
//     typedef ReturnType(CallType::*MEMFUNC)(ArgType) const;
//     return Callback<T, Client, MEMFUNC>(client, memFunc);
// }
//
// template specialization for void
// template <typename Client, typename ReturnType, typename CallType>
// Callback<void, Client, ReturnType(CallType::*)(void) const >
// make_callback (
// 		CallbackBase<void>* callbackBase,
// 		Client& client,
// 		ReturnType(CallType::* const& memFunc)(void) const
// )
// {
//     typedef ReturnType(CallType::*MEMFUNC)(void) const;
//     return Callback<void, Client, MEMFUNC>(client, memFunc);
// }

#endif /* CALLBACK_H_ */
