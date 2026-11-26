/*
 * SingletonXmlReader.h
 *
 *  Created on: 19-Aug-2013
 *      Author: sunder
 */

#include "xmlreader.h"

#ifndef SINGLETONXMLREADER_H_
#define SINGLETONXMLREADER_H_
namespace illuminati
{
    class SingletonXMLReader : public illuminati::XmlReader
    {
    private:
        SingletonXMLReader()
        {
        }
        SingletonXMLReader(const SingletonXMLReader &);
        SingletonXMLReader &operator=(const SingletonXMLReader &);

    public:
        static SingletonXMLReader &Instance()
        {
            static SingletonXMLReader instance;
            return instance;
        }
        void topMostParent()
        {
            while (XmlReader::parentElement() != 0)
                ;
        }
    };
}
#endif /* SINGLETONXMLREADER_H_ */
