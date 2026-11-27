/* Company: way2wealth-ors
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-ors.com/
 */

#ifndef _ILLUMINATI_SERIALIZER_H
#define _ILLUMINATI_SERIALIZER_H

namespace ors
{
    namespace infra
    {
        struct Serializer
        {
            virtual ~Serializer() {}

            // This should return the string representation of the object
            virtual std::string serialize() const = 0;

            // this should return a copy of the object
            virtual Serializer &clone() const = 0;
        };
    }
}

#endif
