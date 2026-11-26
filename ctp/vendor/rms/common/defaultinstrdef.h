/*
 * Company: way2wealth-ors
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-ors.com/
 *
 */

#ifndef DEFAULTINSTRDEF_H
#define DEFAULTINSTRDEF_H

#include <sstream>
#include <string>

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

#endif // DEFAULTINSTRDEF_H
