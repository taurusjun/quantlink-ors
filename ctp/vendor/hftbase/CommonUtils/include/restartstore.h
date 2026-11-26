/*
 * Company: way2wealth-illuminati
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-illuminati.com/
 */

#ifndef RESTART_STORE_H
#define RESTART_STORE_H

#include "fileutils.h"
#include "stringutils.h"
#include <fstream>
#include <unordered_map>

static const char RESTART_STORE_VALUES[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz~!@#$%^&*()_+={}[]";
class RestartKeyStore
{
private:
    std::string m_fileName;
    std::unordered_map<std::string, int> m_content;
    std::ofstream outfile;

private:
    /*!
     * updateStore
     */
    void updateStore()
    {
        outfile.open(m_fileName, std::ios::trunc);
        for (auto it = m_content.begin(); it != m_content.end(); it++)
        {
            outfile << it->first << "=" << it->second << "\n";
        }
        outfile.close();
    }

public:
    /*!
     * init
     */
    void init(std::string restartStoreFile)
    {
        m_content.clear();
        m_fileName = restartStoreFile;

        if (illuminati::fileutils::fileExists(restartStoreFile))
        {
            std::ifstream infile(restartStoreFile, std::ios::in);

            if (!infile.good())
            {
                std::cout << "RestartKeyStore::Unable to open the restartStoreFile" << std::endl;
                throw("error");
            }

            std::string line, key;
            int value, posEqual;
            while (std::getline(infile, line))
            {
                line = illuminati::trim(line);
                if (!line.length())
                {
                    continue;
                }
                posEqual = line.find('=');
                key = illuminati::trim(line.substr(0, posEqual));
                value = std::stoi(illuminati::trim(line.substr(posEqual + 1))) % (sizeof(RESTART_STORE_VALUES) - 1);
                m_content[key] = value;
            }
            infile.close();
        }
        else
        {
            outfile.open(restartStoreFile, std::ios::trunc);
            if (!outfile.good())
            {
                std::cout << "RestartKeyStore::Unable to create the restartStoreFile" << std::endl;
                throw("error");
            }
            outfile.close();
        }
    }

    /*!
     * getRestartKey
     */
    char getRestartKey(std::string identifier)
    {
        auto it = m_content.find(identifier);
        if (it == m_content.end())
        { // key not found
            m_content[identifier] = 0;
        }
        else
        {
            it->second = (it->second + 1) % (sizeof(RESTART_STORE_VALUES) - 1);
        }
        updateStore();
        return (RESTART_STORE_VALUES[m_content[identifier]]);
    }

    /*!
     * peekRestartKey
     */
    bool peekRestartKey(std::string identifier, char &value)
    {
        auto it = m_content.find(identifier);
        if (it == m_content.end())
        { // key not found
            return false;
        }
        else
        {
            value = RESTART_STORE_VALUES[it->second];
            return true;
        }
    }
};

#endif