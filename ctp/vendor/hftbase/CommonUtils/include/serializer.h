/* Company: way2wealth-illuminati
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-illuminati.com/
 */

#ifndef _ILLUMINATI_SERIALIZER_H
#define _ILLUMINATI_SERIALIZER_H

namespace illuminati
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
