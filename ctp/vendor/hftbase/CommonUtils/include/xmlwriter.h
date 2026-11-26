/*
 * xmlwriter.h
 *
 *  Created on: 29-Aug-2013
 *      Author: illuminati
 */

#ifndef XMLWRITER_H_
#define XMLWRITER_H_

#include <string>
#include "tinyxml2.h"
#include <sstream>

class XmlWriter
{
    tinyxml2::XMLElement *element;
    tinyxml2::XMLDocument document;

public:
    XmlWriter()
    {
        this->element = 0;
    }

    bool LoadConfig(std::string filename)
    {
        auto status = document.LoadFile(filename.c_str());
        if (!status)
            element = document.FirstChildElement();
        return !status;
    }
    bool parse(const char *buffer, size_t len)
    {
        auto status = document.Parse(buffer, len);
        if (!status)
            element = document.FirstChildElement();
        return !status;
    }

    XmlWriter *childElement(std::string name)
    {
        auto el = element->FirstChildElement(name.c_str());
        if (el)
        {
            element = el;
            return this;
        }
        return 0;
    }

    XmlWriter *nextElement(std::string name)
    {
        auto el = element->NextSiblingElement(name.c_str());
        if (el)
        {
            element = el;
            return this;
        }
        return 0;
    }

    XmlWriter *parentElement()
    {
        auto el = element->Parent()->ToElement();
        if (el)
        {
            element = el;
            return this;
        }
        return 0;
    }

    bool isHead()
    {
        return (!this->element);
    }

    XmlWriter *addChildElement(const char *elementTag, const char *value = "")
    {
        if (isHead())
        {
            this->element = document.NewElement(elementTag);
            element->LinkEndChild(document.NewText(value));
            document.LinkEndChild(element);
            return this;
        }
        else
        {
            auto el = document.NewElement(elementTag);
            this->element->InsertEndChild(el);
            el->LinkEndChild(document.NewText(value));
            return this;
        }
    }

    XmlWriter *addChildElement(const char *elementTag, double value)
    {
        std::stringstream tempVal;
        tempVal << value;
        if (isHead())
        {
            this->element = document.NewElement(elementTag);
            element->LinkEndChild(document.NewText(tempVal.str().c_str()));
            document.LinkEndChild(element);
            return this;
        }
        else
        {
            auto el = document.NewElement(elementTag);
            this->element->InsertEndChild(el);
            el->LinkEndChild(document.NewText(tempVal.str().c_str()));
            return this;
        }
    }

    XmlWriter *addChildElement(const char *elementTag, char value)
    {
        std::stringstream tempVal;
        tempVal << value;
        if (isHead())
        {
            this->element = document.NewElement(elementTag);
            element->LinkEndChild(document.NewText(tempVal.str().c_str()));
            document.LinkEndChild(element);
            return this;
        }
        else
        {
            auto el = document.NewElement(elementTag);
            this->element->InsertEndChild(el);
            el->LinkEndChild(document.NewText(tempVal.str().c_str()));
            return this;
        }
    }

    XmlWriter *addChildElement(const char *elementTag, int value)
    {
        std::stringstream tempVal;
        tempVal << value;
        if (isHead())
        {
            this->element = document.NewElement(elementTag);
            element->LinkEndChild(document.NewText(tempVal.str().c_str()));
            document.LinkEndChild(element);
            return this;
        }
        else
        {
            auto el = document.NewElement(elementTag);
            this->element->InsertEndChild(el);
            el->LinkEndChild(document.NewText(tempVal.str().c_str()));
            return this;
        }
    }

    std::string getString(std::string key = "")
    {
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

    void addAttribute(const char *attributeTag, const char *attributeValue)
    {
        this->element->SetAttribute(attributeTag, attributeValue);
    }

    void addAttribute(const char *attributeTag, const int &attributeValue)
    {
        this->element->SetAttribute(attributeTag, attributeValue);
    }

    void addAttribute(const char *&attributeTag, const double &attributeValue)
    {
        this->element->SetAttribute(attributeTag, attributeValue);
    }

    void addValue(const std::string &value)
    {
        this->element->DeleteChild(this->element->FirstChild());
        this->element->LinkEndChild(document.NewText(value.c_str()));
    }

    void addValue(const int &value)
    {
        std::stringstream tempVal;
        tempVal << value;
        this->element->DeleteChild(this->element->FirstChild());
        this->element->LinkEndChild(document.NewText(tempVal.str().c_str()));
    }

    void addValue(const double &value)
    {
        std::stringstream tempVal;
        tempVal << value;
        this->element->DeleteChild(this->element->FirstChild());
        this->element->LinkEndChild(document.NewText(tempVal.str().c_str()));
    }

    void deleteChildren()
    {
        element->DeleteChildren();
    }

    void write(const std::string &filePath)
    {
        document.SaveFile(filePath.c_str());
    }

    char *writeToBuffer(size_t &len)
    {
        tinyxml2::XMLPrinter stream;
        document.Print(&stream);
        len = stream.CStrSize();
        char *buffer = new char[len];
        memcpy((void *)buffer, (void *)(stream.CStr()), len);
        return buffer;
    }
};

#endif /* XMLWRITER_H_ */
