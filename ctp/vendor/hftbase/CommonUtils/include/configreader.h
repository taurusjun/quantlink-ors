/*
 * Company: way2wealth-illuminati
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-illuminati.com/
 *
 */
/*!
 * ---------------------------------------------------------------------------
 * Configfile: Simple config reader
 * Config file format
 *     1. Config file format should be key = value
 *     2. Lines starting with # or ; are treated as comments
 *     3. File can have sections and getmethods can be called with section name
 *     4. Sample
 * ==========sample file start ========
 *
 *  #global section
 *  BOOLEAN2 = FALSE
 *  STRING1  = lot of @#$%^&*()_+=-~`<>,.:;"/ special chars
 *  int1     = 12345789
 *
 *  # Single section
 *  [section2]
 *  s2_BOOLEAN1 = TRUE
 *  s2_BOOLEAN2 = FALSE
 *  s2_STRING1  = HELLO WORld!
 *  s2_double2  = 104.1234567
 *  s2_int1     = 12345789
 *  int1        = 98765
 *  many = hello,how,are,you,,,
 *
 *  # MultiSection - sharing same key value pairs
 *
 *  [section1, section5 , section6]
 *  s1_BOOLEAN2 = FALSE
 *  s1_STRING1  = HELLO wORld!
 *  s1_double2  = 104.123456
 *  s1_int1     = 12345789
 *  int1        = 987654321
 *  many        = many1,many2,how,are,you
 * ==========sample file ends ========
 *
 * APIs:
 *      GetInstance -- return the static object
 *      LoadCfg     -- This must be called at least once to load your config file, can be also used to load another config
 *      isLoaded    -- Will tell you if the config has some contents
 *      toString    -- get the entire config as string object (with some extra formatting text)
 *      getConfigFileName - get the current config file name (last loaded)
 *      getAllSections - get the sections defined in the config file
 *
 *      getMethods    GetMethods - Strict - These throw if values are not present in config
 *      ---------     ---------------------------------------------------------------------
 *      getBool       getBoolStrict
 *      getDouble     getDoubleStrict
 *      getInt32      getIntStrict
 *      getInt64      getInt64Strict       hack: getInt64_s (for section load) due to signature conflict
 *      getString     getStringStrict
 *      getStringList getStringListStrict
 *
 *
 * SampleUsage:
 *   Configfile::GetInstance().LoadCfg(argv[1]);
 *   string s = Configfile::GetInstance().getString("LOGFILE", "x");
 * or
 *   Configfile&cfg = Configfile::GetInstance();
 *   cfg.LoadCfg(argv[1])
 *   std::string logfileName = cfg.getString("LOGFILE", "x");
 *   int vertices = cfg.getIntStrict("GRAPH_SECTION", "VERTICES");
 *
 *---------------------------------------------------------------------------
 */
#ifndef __INFRA_CONFIG_FILE_H__
#define __INFRA_CONFIG_FILE_H__

#include <stdlib.h>
#include <string>
#include <map>
#include <fstream>
#include <strings.h>
#include <iostream>
#include <vector>
#include <mutex>
#include "stringutils.h"

namespace illuminati
{

    class Configfile
    {
    private:
        typedef std::map<std::string, std::string> Contents;
        typedef std::lock_guard<std::recursive_mutex> CfgLock;

        Contents m_globalContents;
        std::map<std::string, Contents> m_sectionContents;

        std::string m_cfgFile;
        bool m_valueFoundFlag;
        std::recursive_mutex m_mutex;

        bool m_isLoadingSection;
        std::string m_loadingSectionsName;

    public:
        /*!
         * GetInstance
         */
        static Configfile &GetInstance()
        {
            static Configfile s_cfg;
            return s_cfg;
        }
        /*!
         * LoadCfg
         */
        void LoadCfg(const std::string &cfgFile)
        {
            CfgLock lock(m_mutex);
            m_globalContents.clear();

            std::ifstream file(cfgFile.c_str(), std::ios::in);
            if (!file.good())
            {
                std::string msg = "Unable to read the file: " + cfgFile;
                std::cout << msg << std::endl;
                throw(msg);
            }

            m_cfgFile = cfgFile;

            std::string line;
            std::string key;
            std::string value;
            int posEqual;
            bool loadSection = false;
            std::vector<std::string> currentSections;
            while (std::getline(file, line))
            {
                line = trim(line);

                // blank lines
                if (!line.length())
                    continue;
                // comments
                if (line[0] == '#' || line[0] == ';')
                    continue;

                // section identifier
                if (line[0] == '[') // we got a new section
                {
                    std::string sectionNameDelimited = line;
                    replaceAll(sectionNameDelimited, std::string("["), std::string(""));
                    replaceAll(sectionNameDelimited, std::string("]"), std::string(""));
                    // normalise the section string
                    currentSections = stringtokens(sectionNameDelimited, ',');
                    loadSection = true;
                    continue;
                }

                // get key and value
                posEqual = line.find('=');
                key = trim(line.substr(0, posEqual));
                value = trim(line.substr(posEqual + 1));

                // if this is section wise setting then it goes to section else to global contents
                if (loadSection)
                {
                    // Copy the construct key=>values for current sections
                    for (auto it = currentSections.begin(); it != currentSections.end(); it++)
                    {
                        Contents &contents = m_sectionContents[*it];
                        contents[key] = value;
                    }
                }
                else
                {
                    m_globalContents[key] = value;
                }
            }
        }
        /*!
         * toString
         */
        std::string toString()
        {
            CfgLock lock(m_mutex);
            std::ostringstream ss;
            ss << "Config file " << getConfigFileName() << " contents:\n";
            ss << "Global contents:\n";
            ss << "----------------\n";
            for (auto it = m_globalContents.begin(); it != m_globalContents.end(); it++)
            {
                ss << it->first << " = " << it->second << "\n";
            }
            for (auto it = m_sectionContents.begin(); it != m_sectionContents.end(); it++)
            {
                ss << "\nSection contents [sectionName=" << it->first << "]\n";
                ss << "-----------------------------------------------------\n";
                for (auto itt = it->second.begin(); itt != it->second.end(); itt++)
                {
                    ss << itt->first << " = " << itt->second << "\n";
                }
            }
            return ss.str();
        }
        /*!
         * isLoaded - returns if there are some contents in the Config object
         */
        bool isLoaded()
        {
            CfgLock lock(m_mutex);
            return ((m_globalContents.size() > 0) || (m_sectionContents.size() > 0));
        }
        /*!
         * getConfigFileName
         */
        std::string getConfigFileName()
        {
            CfgLock lock(m_mutex);
            return m_cfgFile;
        }
        /*!
         * getAllSections
         */
        std::vector<std::string> getAllSections()
        {
            std::vector<std::string> sectionList;
            for (auto it = m_sectionContents.begin(); it != m_sectionContents.end(); it++)
            {
                sectionList.push_back(it->first);
            }
            return sectionList;
        }
        /*!
         * getBool
         */
        bool getBool(const char *key, bool defaultValue = false)
        {
            CfgLock lock(m_mutex);
            std::string value;
            if (getContentsValue(key, value))
            {
                if (0 == strcasecmp(value.c_str(), "TRUE"))
                {
                    return true;
                }
                else
                {
                    return false;
                }
            }
            else
            {
                return defaultValue;
            }
        }
        /*!
         * getBool -- section wrapper
         */
        bool getBool(const char *section, const char *key, bool defaultValue = false)
        {
            CfgLock lock(m_mutex);
            setCurrentSection(section);
            return getBool(key, defaultValue);
        }
        /*!
         * getBoolStrict
         */
        bool getBoolStrict(const char *key)
        {
            CfgLock lock(m_mutex);
            auto retValue = getBool(key);
            if (!m_valueFoundFlag)
            {
                std::cout << "FATAL: config error file=" << m_cfgFile << " key=" << key << std::endl;
                throw("");
            }
            return retValue;
        }
        /*!
         * getBoolStrict - section wrapper
         */
        bool getBoolStrict(const char *section, const char *key)
        {
            CfgLock lock(m_mutex);
            setCurrentSection(section);
            return getBoolStrict(key);
        }
        /*!
         * getDouble
         */
        double getDouble(const char *key, double defaultValue = 0.0f)
        {
            CfgLock lock(m_mutex);
            std::string value;
            if (getContentsValue(key, value))
            {
                return atof(value.c_str());
            }
            else
            {
                return defaultValue;
            }
        }
        /*!
         * getDouble -- section wrapper
         */
        double getDouble(const char *section, const char *key, double defaultValue = 0.0f)
        {
            CfgLock lock(m_mutex);
            setCurrentSection(section);
            return (getDouble(key, defaultValue));
        }
        /*!
         * getDoubleStrict
         */
        double getDoubleStrict(const char *key)
        {
            CfgLock lock(m_mutex);
            auto retValue = getDouble(key);
            if (!m_valueFoundFlag)
            {
                std::cout << "FATAL: config error file=" << m_cfgFile << " key=" << key << std::endl;
                throw("");
            }
            return retValue;
        }
        /*!
         * getDoubleStrict -- section wrapper
         */
        double getDoubleStrict(const char *section, const char *key)
        {
            CfgLock lock(m_mutex);
            setCurrentSection(section);
            return (getDoubleStrict(key));
        }
        /*!
         * getInt
         */
        int32_t getInt(const char *key, int32_t defaultValue = 0)
        {
            CfgLock lock(m_mutex);
            std::string value;
            if (getContentsValue(key, value))
            {
                return atoi(value.c_str());
            }
            else
            {
                return defaultValue;
            }
        }
        /*!
         * getInt -- section wrapper
         */
        int32_t getInt(const char *section, const char *key, int32_t defaultValue = 0)
        {
            CfgLock lock(m_mutex);
            setCurrentSection(section);
            return (getInt(key, defaultValue));
        }
        /*!
         * getIntStrict
         */
        int32_t getIntStrict(const char *key)
        {
            CfgLock lock(m_mutex);
            auto retValue = getInt(key);
            if (!m_valueFoundFlag)
            {
                std::cout << "FATAL: config error file=" << m_cfgFile << " key=" << key << std::endl;
                throw("");
            }
            return retValue;
        }
        /*!
         * getIntStrict -- section wrapper
         */
        int32_t getIntStrict(const char *section, const char *key)
        {
            CfgLock lock(m_mutex);
            setCurrentSection(section);
            return (getIntStrict(key));
        }
        /*!
         * getInt64
         */
        uint64_t getInt64(const char *key, uint64_t defaultValue = 0)
        {
            CfgLock lock(m_mutex);
            std::string value;
            if (getContentsValue(key, value))
            {
                return atol(value.c_str());
            }
            else
            {
                return defaultValue;
            }
        }
        /*!
         * getInt64 -- section wrapper
         */
        uint64_t getInt64_s(const char *section, const char *key, uint64_t defaultValue = 0)
        {
            CfgLock lock(m_mutex);
            setCurrentSection(section);
            return (getInt64(key, defaultValue));
        }
        /*!
         * getInt64Strict
         */
        uint64_t getInt64Strict(const char *key)
        {
            CfgLock lock(m_mutex);
            auto retValue = getInt64(key);
            if (!m_valueFoundFlag)
            {
                std::cout << "FATAL: config error file=" << m_cfgFile << " key=" << key << std::endl;
                throw("");
            }
            return retValue;
        }
        /*!
         * getInt64Strict -- section wrapper
         */
        uint64_t getInt64Strict(const char *section, const char *key)
        {
            CfgLock lock(m_mutex);
            setCurrentSection(section);
            return (getInt64Strict(key));
        }
        /*!
         * getString
         */
        std::string getString(const char *key, const char *defaultValue = "")
        {
            CfgLock lock(m_mutex);
            std::string value;
            if (getContentsValue(key, value))
            {
                return value;
            }
            else
            {
                return std::string(defaultValue);
            }
        }
        /*!
         * getString --section wrapper
         */
        std::string getString_s(const char *section, const char *key, const char *defaultValue = "")
        {
            CfgLock lock(m_mutex);
            setCurrentSection(section);
            return (getString(key, defaultValue));
        }
        /*!
         * getStringStrict
         */
        std::string getStringStrict(const char *key)
        {
            CfgLock lock(m_mutex);
            auto retValue = getString(key);
            if (!m_valueFoundFlag)
            {
                std::cout << "FATAL: config error file=" << m_cfgFile << " key=" << key << std::endl;
                throw("");
            }
            return retValue;
        }
        /*!
         * getStringStrict --section wrapper
         */
        std::string getStringStrict(const char *section, const char *key)
        {
            CfgLock lock(m_mutex);
            setCurrentSection(section);
            return (getStringStrict(key));
        }
        /*!
         * getStringList
         */
        std::vector<std::string> getStringList(const char *key)
        {
            CfgLock lock(m_mutex);
            std::vector<std::string> result;
            std::string value;
            if (getContentsValue(key, value))
            {
                size_t lastIndex = 0;
                size_t nextIndex = value.find(",");
                while (nextIndex != std::string::npos)
                {
                    result.push_back(trim(value.substr(lastIndex, nextIndex - lastIndex)));
                    lastIndex = nextIndex + 1;
                    nextIndex = value.find(",", lastIndex);
                }
                if (lastIndex < value.length())
                {
                    result.push_back(trim(value.substr(lastIndex)));
                }
                return result;
            }
            else
            {
                return result;
            }
        }
        /*!
         * getStringList -- section wrapper
         */
        std::vector<std::string> getStringList(const char *section, const char *key)
        {
            CfgLock lock(m_mutex);
            setCurrentSection(section);
            return (getStringList(key));
        }
        /*!
         * getStringList -- section wrapper
         */
        std::vector<std::string> getStringList(const char *section, std::string key)
        {
            CfgLock lock(m_mutex);
            setCurrentSection(section);
            return (getStringList(key.c_str()));
        }
        /*!
         * getStringListStrict
         */
        std::vector<std::string> getStringListStrict(const char *key)
        {
            CfgLock lock(m_mutex);
            auto retValue = getStringList(key);
            if (!m_valueFoundFlag)
            {
                std::cout << "FATAL: config error file=" << m_cfgFile << " key=" << key << std::endl;
                throw("");
            }
            return retValue;
        }
        /*!
         * getStringListStrict -- section wrapper
         */
        std::vector<std::string> getStringListStrict(const char *section, const char *key)
        {
            CfgLock lock(m_mutex);
            setCurrentSection(section);
            return (getStringListStrict(key));
        }

    public:
        /*!
         * method to set properties to the config object
         */
        void setProperty(const std::string &key, const std::string &value)
        {
            CfgLock lock(m_mutex);
            m_globalContents[key] = value;
        }

        void setProperty_s(const std::string &section, const std::string &key, const std::string &value)
        {
            CfgLock lock(m_mutex);
            auto it = m_sectionContents.find(section);
            if (it == m_sectionContents.end())
            {
                std::string errMsg = "NOT FOUND:config error file=" + m_cfgFile + ";section=" + section + ";key=" + key;
                std::cout << errMsg << std::endl;
                throw(errMsg);
            }
            else
            {
                Contents &pContents = it->second;
                m_isLoadingSection = false;
                m_loadingSectionsName = "";
                pContents[key] = value;
            }
        }

        /*!
         * method to set properties to the config object
         */
        void setPropertyWithSection(const std::string &section, const std::string &key, const std::string &value)
        {
            CfgLock lock(m_mutex);
            auto iter = m_sectionContents.find(section);
            if (iter == m_sectionContents.end())
            {
                Contents contents;
                iter = m_sectionContents.insert(make_pair(section, contents)).first;
            }
            iter->second[key] = value;
        }

        /*!
         * toFile
         */
        void toFile(std::string fileNameWithPath = "")
        {
            CfgLock lock(m_mutex);
            std::ostringstream ss;

            if (fileNameWithPath.size() == 0)
            {
                fileNameWithPath = getConfigFileName();
            }
            ss << "#Config file " << getConfigFileName() << " contents:\n";
            ss << "#Global contents:\n";
            for (auto it = m_globalContents.begin(); it != m_globalContents.end(); it++)
            {
                ss << it->first << " = " << it->second << "\n";
            }
            ss << "\n#Section contents \n";
            for (auto it = m_sectionContents.begin(); it != m_sectionContents.end(); it++)
            {
                ss << "[" << it->first << "]\n";
                for (auto itt = it->second.begin(); itt != it->second.end(); itt++)
                {
                    ss << itt->first << " = " << itt->second << "\n";
                }
            }

            std::ofstream fileObject;
            fileObject.open(fileNameWithPath.c_str(), std::ios::out);
            if (!fileObject.is_open())
            {
                std::cerr << "Error Opening file for write" << std::endl;
                return;
            }
            fileObject.write(ss.str().c_str(), ss.str().size());
            fileObject.flush();
            fileObject.close();
        }

    private:
        /*!
         * setCurrentSection -- always to be called before getting a key within a section
         * @param sectionName
         */
        void setCurrentSection(const char *section)
        {
            m_isLoadingSection = true;
            m_loadingSectionsName = section;
        }
        /*!
         * getContentsValue
         * @param key    [in]
         * @param value [out]
         */
        bool getContentsValue(const char *key, std::string &value)
        {
            m_valueFoundFlag = false;

            // resolve the content source as per the get... call
            Contents *pContents = NULL;
            if (m_isLoadingSection)
            {
                auto it = m_sectionContents.find(m_loadingSectionsName);
                if (it == m_sectionContents.end())
                {
                    std::cout << "FATAL:config error file=" << m_cfgFile << ";section=" << m_loadingSectionsName << ";key=" << key << std::endl;
                    throw("");
                }
                else
                {
                    pContents = &(it->second);
                    m_isLoadingSection = false;
                }
            }
            else
            {
                pContents = &m_globalContents;
            }

            // get the value for the key
            auto it = pContents->find(std::string(key));
            if (it == pContents->end())
            {
                // std::cout << "WARN: config file=" << m_cfgFile << " key="<< key << " is missing " << std::endl;
                return false;
            }
            else
            {
                value = it->second;
                m_valueFoundFlag = true;
            }
            return true;
        }
        /*!
         * Configfile
         */
        Configfile()
        {
            m_valueFoundFlag = false;
            m_cfgFile = "";
            m_isLoadingSection = false;
            m_loadingSectionsName = "";
        }
        Configfile(const Configfile &);
        Configfile &operator=(const Configfile &);
    };

} // namespace
#endif
