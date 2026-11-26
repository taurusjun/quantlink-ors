/*
 * CallbackData.h
 *
 *  Created on: 13-Aug-2012
 *      Author: Varun
 */

#ifndef CALLBACKDATA_H_
#define CALLBACKDATA_H_

#include <cstring>

class CallbackData
{
public:
    typedef void (CallbackData::*PMEMFUNC)(void);

    CallbackData() {}
    CallbackData(const void *client, const void *pMemFunc) : _client(client)
    {
        memcpy(_pMemFunc, pMemFunc, sizeof(PMEMFUNC));
    }

    const void *getClient() const { return _client; }

    const void *getCallFunc() const { return _pMemFunc; }

private:
    char _pMemFunc[sizeof(PMEMFUNC)];
    const void *_client;
};

#endif /* CALLBACKDATA_H_ */
