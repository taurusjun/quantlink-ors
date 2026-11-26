/*
 * Company: way2wealth-illuminati
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-illuminati.com/
 *
 */

#ifndef DEFAULTINSTRDEF_H
#define DEFAULTINSTRDEF_H

#include <sstream>
#include <string>

namespace illuminati
{
    namespace ors
    {
        struct DataAddendum
        {
            char ISINLastChar;
        };

        class DefaultInstrDef
        {
        public:
            double RoundLot;
            double Factor;
            DataAddendum AdditionalData;

            std::string toString()
            {
                std::stringstream ss;
                ss << RoundLot << "|" << Factor;
                return ss.str();
            }
        };
    }
}

#endif // DEFAULTINSTRDEF_H
