#ifndef SIGNALCALLBACK_H
#define SIGNALCALLBACK_H

#include <string.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <commonutils.h>
#include "c11compatible.h"

namespace ors
{
    namespace callback
    {
        class CallbackData
        {
        public:
            typedef void (CallbackData::*memfunc)();
            CallbackData(const void *pClient_, const void *ppMemfunc_) : _pClient(pClient_)
            {
                memcpy(_ppMemfunc, ppMemfunc_, sizeof(memfunc));
            }
            const void *pClient() const
            {
                return _pClient;
            }
            const void *ppMemfunc() const
            {
                return _ppMemfunc;
            }

        protected:
            char _ppMemfunc[sizeof(memfunc)];
            const void *_pClient;
        };

        typedef void (CallbackData::*CallbackMemFunc)();

        template <typename ReturnType, typename... Param>
        class FunctorBase
        {
        public:
            CallbackData data;
            typedef ReturnType (*Fptr)(CallbackData, Param...);
            Fptr func;

            FunctorBase(const void *client, const void *memfunc, Fptr f) : data(client, memfunc), func(f)
            {
            }

            template <typename... T>
            ReturnType operator()(T... args)
            {
                return (func)(data, args...);
            }
        };

        template <typename Client, typename Memfunc, typename ReturnType, typename... Param>
        class Functor : public FunctorBase<ReturnType, Param...>
        {
        public:
            Functor(Client *client, Memfunc *memfunc) : FunctorBase<ReturnType, Param...>(client, memfunc, hook)
            {
            }

            template <typename... T>
            static ReturnType hook(CallbackData data, T... args)
            {
                Client *pClient = (Client *)data.pClient();
                Memfunc &pmemfunc = (*(Memfunc *)(data.ppMemfunc()));
                return (pClient->*pmemfunc)(args...);
            }
        };

        template <typename Memfunc, typename ReturnType, typename... Param>
        class Functor<void *, Memfunc, ReturnType, Param...> : public FunctorBase<ReturnType, Param...>
        {
        public:
            Functor(Memfunc *memfunc) : FunctorBase<ReturnType, Param...>(NULL, memfunc, hook)
            {
            }

            template <typename... T>
            static ReturnType hook(CallbackData data, T... args)
            {
                Memfunc &pmemfunc = (*(Memfunc *)(data.ppMemfunc()));
                return (*pmemfunc)(args...);
            }
        };

        class MultiSignalCallbackSource
        {
            struct MultiSignalFunc
            {
                void *func;
                uint32_t token;
                MultiSignalFunc(void *f, uint32_t t) : func(f), token(t) {}
            };
            std::unordered_map<int, std::vector<MultiSignalFunc> *> signalMap;

        public:
            uint32_t registerSignal(int signalId, void *callback)
            {
                if (callback == nullptr)
                    return 0;
                std::vector<MultiSignalFunc> *signalHandlerList;
                auto index = signalMap.find(signalId);
                if (index != signalMap.end())
                {
                    signalHandlerList = index->second;
                }
                else
                {
                    signalHandlerList = new std::vector<MultiSignalFunc>;
                    signalMap.insert(std::make_pair(signalId, signalHandlerList));
                }
                uint32_t token = CommonUtils::generateRandomNumber<>();
                signalHandlerList->push_back(MultiSignalFunc(callback, token));
                std::cout << "registered signal: " << token << "\n";
                return token;
            }

            void unregisterSignal(int signalId, uint32_t token)
            {
                auto index = signalMap.find(signalId);
                if (index == signalMap.end())
                    return;
                for (auto i = index->second->begin(); i != index->second->end();)
                {
                    if (i->token == token)
                    {
                        std::cout << "Found token, removing callback.\n";
                        i = index->second->erase(i);
                    }
                    else
                        i++;
                }
                if (index->second->size() == 0)
                {
                    std::cout << "callback list is empty, removing it.\n";
                    delete index->second;
                    signalMap.erase(index);
                }
            }

            template <typename Ret = void *, typename... T>
            std::vector<Ret> invokeSignal(int signalId, T... param)
            {
                std::vector<Ret> returnVal;
                auto index = signalMap.find(signalId);
                if (index != signalMap.end())
                {
                    auto data = index->second;
                    if (data != nullptr)
                    {
                        for (int i = 0; i < data->size(); i++)
                        {
                            auto cb = (::ors::callback::FunctorBase<Ret, T...> *)(data->at(i).func);
                            returnVal.push_back((*cb)(param...));
                        }
                    }
                }
                return returnVal;
            }
        };
    }
}

#define DEFINE_SIGNAL(...) enum Signals \
{                                       \
    __VA_ARGS__                         \
};

#define SIGNAL_INIT(n)      \
    void *callbackArray[n]; \
    _REGISTER_SIGNAL_       \
    _INVOKE_SIGNAL_         \
    _INVOKE_SIGNAL_RETURNTYPE_

#define EMIT(signalName) invokeSignal(signalName);
#define EMIT_PARAM(signalName, ...) invokeSignal(signalName, __VA_ARGS__);
#define _INVOKE_SIGNAL_RETURNTYPE_                                                                    \
    template <typename ReturnType, typename... T>                                                     \
    ReturnType invokeSignal(Signals signalName, T... args)                                            \
    {                                                                                                 \
        auto cb = (::ors::callback::FunctorBase<ReturnType, T...> *)(callbackArray[signalName]); \
        return (*cb)(args...);                                                                        \
    }

#define _INVOKE_SIGNAL_                                                                         \
    template <typename... T>                                                                    \
    void invokeSignal(Signals signalName, T... args)                                            \
    {                                                                                           \
        auto cb = (::ors::callback::FunctorBase<void, T...> *)(callbackArray[signalName]); \
        (*cb)(args...);                                                                         \
    }

#define _REGISTER_SIGNAL_                                       \
    uint32_t registerSignal(Signals signalName, void *callback) \
    {                                                           \
        callbackArray[signalName] = callback;                   \
        return 1;                                               \
    }

template <class source, class dest, class memfunc, class SignalType, typename... T>
uint32_t connect(source *src, dest *d, memfunc func, SignalType signalName, T...)
{
    return src->registerSignal(signalName, new ::ors::callback::Functor<dest, memfunc, void, T...>(d, &func));
}

template <class source, class dest, class memfunc, class SignalType, typename ReturnType, typename... T>
uint32_t connectWithReturn(source *src, dest *d, memfunc func, SignalType signalName, ReturnType dummy, T...)
{
    return src->registerSignal(signalName, new ::ors::callback::Functor<dest, memfunc, ReturnType, T...>(d, &func));
}

template <class source, class memfunc, class SignalType, typename... T>
uint32_t connect(source *src, memfunc func, SignalType signalName, T...)
{
    return src->registerSignal(signalName, new ::ors::callback::Functor<void *, memfunc, void, T...>(&func));
}

template <class source, class memfunc, class SignalType, typename... T>
uint32_t connectNonMember(source *src, memfunc func, SignalType signalName, T...)
{
    return src->registerSignal(signalName, new ::ors::callback::Functor<void *, memfunc, void, T...>(&func));
}

template <class source, class dest, class memfunc, class SignalType, typename... T>
uint32_t connectWithParam(source *src, dest *d, memfunc func, SignalType signalName, T...)
{
    return src->registerSignal(signalName, new ::ors::callback::Functor<dest, memfunc, void, T...>(d, &func));
}

template <class source, class memfunc, class SignalType, typename ReturnType, typename... T>
uint32_t connectFuncWithReturn(source *src, memfunc func, SignalType signalName, ReturnType dummy, T...)
{
    return src->registerSignal(signalName, new ::ors::callback::Functor<void *, memfunc, ReturnType, T...>(&func));
}

template <class source, class dest, class memfunc, class SignalType, typename Param>
uint32_t connectWithParamRef(source *src, dest *d, memfunc func, SignalType signalName, Param &p)
{
    return src->registerSignal(signalName, new ::ors::callback::Functor<dest, memfunc, void, Param>(d, &func));
}

#define CONNECT(src, dest, func, name) connect<decltype(*src), decltype(*dest), decltype(func)>(src, dest, func, #name)

#define CONNECT_PARAM(src, dest, func, name, paramType) connectWithParam<decltype(*src), decltype(*dest), decltype(func), paramType>(src, dest, func, #name)

#endif
