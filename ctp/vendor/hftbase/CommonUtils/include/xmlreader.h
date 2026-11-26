/*
 * Company: way2wealth-illuminati
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-illuminati.com/
 *
 */

#ifndef XMLCONFIGREADER_H
#define XMLCONFIGREADER_H

#include "tinyxml2.h"
#include <string>

namespace illuminati
{
    class XmlReader
    {
    private:
        tinyxml2::XMLDocument doc;
        tinyxml2::XMLElement *element;

    public:
        bool LoadConfig(std::string filename)
        {
            auto status = doc.LoadFile(filename.c_str());
            if (!status)
                element = doc.FirstChildElement();
            return !status;
        }

        bool parse(const char *buffer, size_t len)
        {
            auto status = doc.Parse(buffer, len);
            if (!status)
                element = doc.FirstChildElement();
            return !status;
        }

        XmlReader *childElement(std::string name)
        {
            auto el = element->FirstChildElement(name.c_str());
            if (el)
            {
                element = el;
                return this;
            }
            return 0;
        }

        XmlReader *nextElement(std::string name)
        {
            auto el = element->NextSiblingElement(name.c_str());
            if (el)
            {
                element = el;
                return this;
            }
            return 0;
        }

        XmlReader *parentElement()
        {
            auto el = element->Parent()->ToElement();
            if (el)
            {
                element = el;
                return this;
            }
            return 0;
        }

        std::string getString(std::string key = "")
        {
            // tinyxml2::TiXmlHandle handle(&doc);
            if (!element)
                throw "Invalid reader state";
            if (key.size() == 0)
            {
                const char *value = element->GetText();
                if (value)
                    return value;
                else
                    return std::string("");
            }
            auto el = element->FirstChildElement(key.c_str());
            if (el)
            {
                const char *value = el->GetText();
                if (value)
                    return value;
            }
            return std::string("");
        }

        int getInt(std::string key = "", int defaultValue = 0)
        {
            std::string value = getString(key);
            int ivalue = defaultValue;
            if (value.size() > 0)
            {
                ivalue = std::stol(value);
            }
            return ivalue;
        }

        int64_t getInt64(std::string key = "", int64_t defaultValue = 0)
        {
            std::string value = getString(key);
            int64_t ivalue = defaultValue;
            if (value.size() > 0)
            {
                ivalue = std::stoll(value);
            }
            return ivalue;
        }

        uint64_t getUInt64(std::string key = "", uint64_t defaultValue = 0)
        {
            std::string value = getString(key);
            uint64_t ivalue = defaultValue;
            if (value.size() > 0)
            {
                ivalue = std::stoull(value);
            }
            return ivalue;
        }

        double getDouble(std::string key = "", double defaultValue = 0.0)
        {
            std::string value = getString(key);
            double dvalue = defaultValue;
            if (value.size() > 0)
                dvalue = std::stod(value);
            return dvalue;
        }

        float getFloat(std::string key = "", float defaultValue = 0.0f)
        {
            std::string value = getString(key);
            float dvalue = defaultValue;
            if (value.size() > 0)
                dvalue = std::stof(value);
            return dvalue;
        }

        bool getBool(std::string key = "", bool defaultValue = true)
        {
            std::string value = getString(key);
            bool bvalue = defaultValue;
            if (value.size() > 0)
                bvalue = (strcasecmp(value.c_str(), "TRUE") == 0);
            return bvalue;
        }

        std::string getStringAttribute(std::string key, std::string node = "", std::string defaultValue = "")
        {
            if (!element)
                throw "Invalid reader state";
            tinyxml2::XMLElement *el = element;
            if (node.size() > 0)
            {
                el = element->FirstChildElement(node.c_str());
                if (!el)
                    return defaultValue;
            }
            const char *value = el->Attribute(key.c_str());
            if (value)
                return value;
            return defaultValue;
        }

        int getIntAttribute(std::string key, std::string node = "", int defaultValue = 0)
        {
            std::string val = getStringAttribute(key, node, "");
            if (val.size() == 0)
                return defaultValue;
            return std::stoi(val);
        }

        int64_t getInt64Attribute(std::string key, std::string node = "", int64_t defaultValue = 0)
        {
            std::string val = getStringAttribute(key, node, "");
            if (val.size() == 0)
                return defaultValue;
            return std::stoll(val);
        }

        uint64_t getUInt64Attribute(std::string key, std::string node = "", uint64_t defaultValue = 0)
        {
            std::string val = getStringAttribute(key, node, "");
            if (val.size() == 0)
                return defaultValue;
            return std::stoull(val);
        }

        bool getBoolAttribute(std::string key, std::string node = "", bool defaultValue = true)
        {
            std::string val = getStringAttribute(key, node, "");
            if (val.size() == 0)
                return defaultValue;
            return (strcasecmp(val.c_str(), "TRUE") == 0);
        }

        float getFloatAttribute(std::string key, std::string node = "", float defaultValue = 0.0f)
        {
            std::string val = getStringAttribute(key, node, "");
            if (val.size() == 0)
                return defaultValue;
            return std::stof(val);
        }

        double getFloatAttribute(std::string key, std::string node = "", double defaultValue = 0.0)
        {
            std::string val = getStringAttribute(key, node, "");
            if (val.size() == 0)
                return defaultValue;
            return std::stod(val);
        }
    };
}
#endif // XMLCONFIGREADER_H
