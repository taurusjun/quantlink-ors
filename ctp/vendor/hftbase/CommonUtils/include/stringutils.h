/*
 * Company: way2wealth-illuminati
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-illuminati.com/
 *
 */

#ifndef ILLUMINATI_STRING_UTILS_H
#define ILLUMINATI_STRING_UTILS_H

#include <string>
#include <vector>
#include <sstream>
#include <iostream>

#include <string.h>

namespace illuminati
{
    //********************************************************************************
    // unarytrim
    //********************************************************************************
    static std::string unarytrim(std::string const &source)
    {
        char const *delims = " \t\r\n";
        std::string result(source);
        std::string::size_type index = result.find_last_not_of(delims);
        if (index != std::string::npos)
        {
            result.erase(++index);
        }
        index = result.find_first_not_of(delims);
        if (index != std::string::npos)
        {
            result.erase(0, index);
        }
        else
        {
            result.erase();
        }
        return result;
    }
    //********************************************************************************
    // trim
    //********************************************************************************
    static std::string trim(std::string const &source, char const *delims = " \t\r\n")
    {
        std::string result(source);
        std::string::size_type index = result.find_last_not_of(delims);
        if (index != std::string::npos)
        {
            index++;
            if (index < result.length())
            {
                result.erase(index);
            }
        }
        index = result.find_first_not_of(delims);
        if (index != std::string::npos)
        {
            result.erase(0, index);
        }
        else
        {
            result.erase();
        }
        return result;
    }

    //********************************************************************************
    // replaceAll
    //********************************************************************************
    static inline void replaceAll(std::string &str, const std::string &what, const std::string &with)
    {
        if (what.empty())
            return;

        size_t start_pos = 0;
        size_t whatLen = what.length();
        size_t withLen = with.length();

        while ((start_pos = str.find(what, start_pos)) != std::string::npos)
        {
            str.replace(start_pos, whatLen, with);
            start_pos += withLen;
        }
    }
    //********************************************************************************
    // replaceAll
    //********************************************************************************
    static inline void replaceAll(std::string &str,
                                  const char *what, const int whatLen,
                                  const char *with, const int withLen)
    {
        size_t start_pos = 0;
        while ((start_pos = str.find(what, start_pos)) != std::string::npos)
        {
            str.replace(start_pos, whatLen, with);
            start_pos += withLen;
        }
    }
    //********************************************************************************
    // GetFileWithExtFromPath
    //********************************************************************************
    static inline std::string GetFileWithExtFromPath(std::string file)
    {
        int index = file.find_last_of("(/\\");
        std::string fileName = file.substr(index + 1);
        return fileName;
    }
    //********************************************************************************
    // cstringTokens
    //********************************************************************************
    static inline std::vector<std::string> cstringtokens(char *str_, char delim_)
    {
        std::vector<std::string> list_;
        char de[2];
        de[0] = delim_;
        de[1] = '\0';
        char *tok = strtok(str_, de);
        while (tok != NULL)
        {
            list_.push_back(trim(std::string(tok)));
            tok = strtok(NULL, de);
        }
        return list_;
    }
    //********************************************************************************
    // cstringTokens_r
    //********************************************************************************
    static inline std::vector<std::string> cstringtokens_r(char *str_, char delim_)
    {
        char *ptrptr = str_;
        std::vector<std::string> list_;
        char de[2];
        de[0] = delim_;
        de[1] = '\0';
        char *tok = strtok_r(ptrptr, de, &ptrptr);
        while (tok != NULL)
        {
            list_.push_back(trim(std::string(tok)));
            tok = strtok_r(ptrptr, de, &ptrptr);
        }
        return list_;
    }
    //********************************************************************************
    // stringTokens
    //********************************************************************************
    static inline std::vector<std::string> stringtokens(const std::string &str_, char delim_)
    {
        std::stringstream sstr(str_);
        std::string token;
        std::vector<std::string> list_;
        while (std::getline(sstr, token, delim_))
        {
            list_.push_back(trim(token));
        }
        return list_;
    }
    /*!
     * intTocharsWPadding
     */
    template <class IntType>
    inline __attribute__((always_inline)) char *intTocharsWPadding(char *buf, const size_t len, IntType t,
                                                                   size_t &finalLength, const size_t width = 0, const char paddingChar = '0')
    {
        char *p = buf + len;
        *--p = '\0';
        do
        {
            *--p = '0' + (char)(t % 10);
            t /= 10;
        } while (t > 0);

        const char *stop_p = buf + len - width - 1;
        if (stop_p < buf)
            stop_p = buf;
        while (p > stop_p)
            *--p = paddingChar;
        finalLength = (buf + len) - p;
        return p;
    }

    /*!
     * intTochars
     * finalLength includes null character. (i.e. finallength = strlen + 1)
     */
    template <class IntType>
    inline __attribute__((always_inline)) char *intTochars(char *buf, const size_t len, IntType t, size_t &finalLength)
    {
        char *p = buf + len;
        *--p = '\0';
        do
        {
            *--p = '0' + (char)(t % 10);
            t /= 10;
        } while (t > 0);

        finalLength = (buf + len) - p;
        return p;
    }

}
#endif
