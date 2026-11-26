/*
 * Company: way2wealth-illuminati
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-illuminati.com/
 *
 */

#ifndef ILLUMINATI_FILE_UTILS_H
#define ILLUMINATI_FILE_UTILS_H

#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace illuminati
{
    namespace fileutils
    {

        /*!
         * TODO: fixPathAtEnd Normalise the file path
         */
        static std::string &fixPathAtEnd(std::string &dir)
        {
            if (dir.c_str()[dir.length() - 1] != '/')
            {
                dir += "/";
            }
            return dir;
        }
        /*!
         * fileExists
         */
        static bool fileExists(std::string fileOrDir)
        {
            struct stat st;

            if (stat(fileOrDir.c_str(), &st) != 0)
            {
                return false;
            }

            return true;
        }
        /*!
         * createDir
         */
        static int createDir(std::string dirPath)
        {
            if (!fileExists(dirPath.c_str()))
            {
                return mkdir(dirPath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
            }
            return 0;
        }
        /*!
         * checks if file exits
         */
        static bool fileAvailable(const std::string &filepath)
        {
            return (access(filepath.c_str(), F_OK | R_OK) != -1) ? true : false;
        }

#define BRAZIL_SUFFIX "BRL"
#define FTCOLO_SUFFIX "FTCOLO"
#define RUSSIA_SUFFIX "RSA"
#define CME_SUFFIX "CME"
#define LME_SUFFIX "LME"
#define LSE_SUFFIX "LSE"
#define NYSE_SUFFIX "NYSE"
#define SGX_SUFFIX "SGX"
#define NSE_SUFFIX "COLO"
#define VIKROLI_SUFFIX "VIK"
#define CHINA_SUFFIX "CHINA"
#define BSE_SUFFIX "BSE"
#define DGCX_SUFFIX "DGCX"
#define MCX_SUFFIX "MCX"
#define MCXSX_SUFFIX "MCXSX"

        static std::string getDataFilePath(const std::string &parentDir, const std::string &date, const std::string &filePrefix)
        {
            std::string dataFile = "";
            std::string::size_type pos = 0;

            if ((pos = filePrefix.find("_On_")) != std::string::npos || (pos = filePrefix.find("_ON_")) != std::string::npos || (pos = filePrefix.find("_on_")) != std::string::npos)
            {
                std::string rootPrefix = filePrefix.substr(0, pos);
                std::string childPrefix = filePrefix.substr(pos + 4, filePrefix.length());

                dataFile = parentDir + "/" + date + "/" + rootPrefix + "_DATA.dat";
            }
            else
            {
                dataFile = parentDir + "/" + date + "/" + filePrefix + "_" + "DATA.dat";
            }

            return dataFile;
        }

        static std::string getDataFilePathRaw(const std::string &parentDir, const std::string &date, const std::string &filePrefix)
        {
            std::string dataFile = "";
            std::string::size_type pos = 0;
            if ((pos = filePrefix.find("_On_")) != std::string::npos || (pos = filePrefix.find("_ON_")) != std::string::npos || (pos = filePrefix.find("_on_")) != std::string::npos)
            {
                std::string rootPrefix = filePrefix.substr(0, pos);
                std::string childPrefix = filePrefix.substr(pos + 4, filePrefix.length());

                dataFile = parentDir + "/" + date + "/" + rootPrefix + "_" + childPrefix + "_DATA.dat";
            }
            else
            {
                dataFile = parentDir + "/" + date + "/" + filePrefix + "_" + "DATA.dat";
            }

            return dataFile;
        }

        static std::string getMetaDataFilePathRaw(const std::string &parentDir, const std::string &date, const std::string &filePrefix)
        {
            std::string dataFile = "";
            std::string::size_type pos = 0;
            if ((pos = filePrefix.find("_On_")) != std::string::npos || (pos = filePrefix.find("_ON_")) != std::string::npos || (pos = filePrefix.find("_on_")) != std::string::npos)
            {
                std::string rootPrefix = filePrefix.substr(0, pos);
                std::string childPrefix = filePrefix.substr(pos + 4, filePrefix.length());

                dataFile = parentDir + "/" + date + "/" + rootPrefix + "_" + childPrefix + "_METADATA.dat";
            }
            else
            {
                dataFile = parentDir + "/" + date + "/" + filePrefix + "_" + "METADATA.dat";
            }

            return dataFile;
        }

        static std::string getMetaDataFilePath(const std::string &parentDir, const std::string &date, const std::string &filePrefix)
        {
            std::string dataFile = "";
            std::string::size_type pos = 0;

            if ((pos = filePrefix.find("_On_")) != std::string::npos || (pos = filePrefix.find("_ON_")) != std::string::npos || (pos = filePrefix.find("_on_")) != std::string::npos)
            {
                std::string rootPrefix = filePrefix.substr(0, pos);
                std::string childPrefix = filePrefix.substr(pos + 4, filePrefix.length());

                dataFile = parentDir + "/" + date + "/" + rootPrefix + "_METADATA.dat";
            }
            else
            {
                dataFile = parentDir + "/" + date + "/" + filePrefix + "_" + "METADATA.dat";
            }

            return dataFile;
        }

        static std::string getTsDataFilePath(const std::string &parentDir, const std::string &date, const std::string &filePrefix)
        {
            std::string tsDataFile = "";
            std::string::size_type pos = 0;

            if ((pos = filePrefix.find("_On_")) != std::string::npos || (pos = filePrefix.find("_ON_")) != std::string::npos || (pos = filePrefix.find("_on_")) != std::string::npos)
            {
                std::string rootPrefix = filePrefix.substr(0, pos);
                std::string childPrefix = filePrefix.substr(pos + 4, filePrefix.length());

                if (childPrefix == BRAZIL_SUFFIX)
                {
                    tsDataFile = parentDir + "/" + date + "/" + rootPrefix + "_BRL_TSDATA.dat";
                }
                else if (childPrefix == FTCOLO_SUFFIX)
                {
                    tsDataFile = parentDir + "/" + date + "/" + rootPrefix + "_FTCOLO_TSDATA.dat";
                }
                else if (childPrefix == RUSSIA_SUFFIX)
                {
                    tsDataFile = parentDir + "/" + date + "/" + rootPrefix + "_RSA_TSDATA.dat";
                }
                else if (childPrefix == CME_SUFFIX)
                {
                    tsDataFile = parentDir + "/" + date + "/" + rootPrefix + "_CME_TSDATA.dat";
                }
                else if (childPrefix == LME_SUFFIX)
                {
                    tsDataFile = parentDir + "/" + date + "/" + rootPrefix + "_LME_TSDATA.dat";
                }
                else if (childPrefix == NYSE_SUFFIX)
                {
                    tsDataFile = parentDir + "/" + date + "/" + rootPrefix + "_NYSE_TSDATA.dat";
                }
                else if (childPrefix == LSE_SUFFIX)
                {
                    tsDataFile = parentDir + "/" + date + "/" + rootPrefix + "_LSE_TSDATA.dat";
                }
                else if (childPrefix == SGX_SUFFIX)
                {
                    tsDataFile = parentDir + "/" + date + "/" + rootPrefix + "_SGX_TSDATA.dat";
                }
                else if (childPrefix == NSE_SUFFIX)
                {
                    tsDataFile = parentDir + "/" + date + "/" + rootPrefix + "_COLO_TSDATA.dat";
                }
                else if (childPrefix == VIKROLI_SUFFIX)
                {
                    tsDataFile = parentDir + "/" + date + "/" + rootPrefix + "_TSDATA.dat";
                }
                else if (childPrefix == CHINA_SUFFIX)
                {
                    tsDataFile = parentDir + "/" + date + "/" + rootPrefix + "_CHINA_TSDATA.dat";
                }
                else if (childPrefix == BSE_SUFFIX)
                {
                    tsDataFile = parentDir + "/" + date + "/" + rootPrefix + "_BSE_TSDATA.dat";
                }
                else if (childPrefix == DGCX_SUFFIX)
                {
                    tsDataFile = parentDir + "/" + date + "/" + rootPrefix + "_DGCX_TSDATA.dat";
                }
                else if (childPrefix == MCX_SUFFIX)
                {
                    tsDataFile = parentDir + "/" + date + "/" + rootPrefix + "_MCX_TSDATA.dat";
                }
                else
                {
                    tsDataFile = parentDir + "/" + date + "/" + rootPrefix + "_" + childPrefix + "_TSDATA.dat";
                }
            }
            else
            {
                if ((pos = filePrefix.find("NSE")) != std::string::npos)
                    tsDataFile = parentDir + "/" + date + "/" + filePrefix + "_COLO_TSDATA.dat";
                else
                    tsDataFile = parentDir + "/" + date + "/" + filePrefix + "_" + "TSDATA.dat";
            }

            return tsDataFile;
        }

        static std::pair<std::string, std::string> getTsMetaDataFilePath(const std::string &parentDir, const std::string &date, const std::string &filePrefix)
        {
            std::string tsDataFile = "";
            std::string::size_type pos = 0;
            std::string rootPrefix = "";
            if ((pos = filePrefix.find("_On_")) != std::string::npos || (pos = filePrefix.find("_ON_")) != std::string::npos || (pos = filePrefix.find("_on_")) != std::string::npos)
            {
                rootPrefix = filePrefix.substr(0, pos);
                std::string childPrefix = filePrefix.substr(pos + 4, filePrefix.length());

                if (childPrefix == BRAZIL_SUFFIX)
                {
                    tsDataFile = parentDir + "/" + date + "/" + rootPrefix + "_BRL_TSMETADATA.dat";
                }
                else if (childPrefix == FTCOLO_SUFFIX)
                {
                    tsDataFile = parentDir + "/" + date + "/" + rootPrefix + "_FTCOLO_TSMETADATA.dat";
                }
                else if (childPrefix == RUSSIA_SUFFIX)
                {
                    tsDataFile = parentDir + "/" + date + "/" + rootPrefix + "_RSA_TSMETADATA.dat";
                }
                else if (childPrefix == CME_SUFFIX)
                {
                    tsDataFile = parentDir + "/" + date + "/" + rootPrefix + "_CME_TSMETADATA.dat";
                }
                else if (childPrefix == LME_SUFFIX)
                {
                    tsDataFile = parentDir + "/" + date + "/" + rootPrefix + "_LME_TSMETADATA.dat";
                }
                else if (childPrefix == NYSE_SUFFIX)
                {
                    tsDataFile = parentDir + "/" + date + "/" + rootPrefix + "_NYSE_TSMETADATA.dat";
                }
                else if (childPrefix == LSE_SUFFIX)
                {
                    tsDataFile = parentDir + "/" + date + "/" + rootPrefix + "_LSE_TSMETADATA.dat";
                }
                else if (childPrefix == SGX_SUFFIX)
                {
                    tsDataFile = parentDir + "/" + date + "/" + rootPrefix + "_SGX_TSMETADATA.dat";
                }
                else if (childPrefix == NSE_SUFFIX)
                {
                    tsDataFile = parentDir + "/" + date + "/" + rootPrefix + "_COLO_TSMETADATA.dat";
                }
                else if (childPrefix == VIKROLI_SUFFIX)
                {
                    tsDataFile = parentDir + "/" + date + "/" + rootPrefix + "_TSMETADATA.dat";
                }
                else if (childPrefix == CHINA_SUFFIX)
                {
                    tsDataFile = parentDir + "/" + date + "/" + rootPrefix + "_CHINA_TSMETADATA.dat";
                }
                else if (childPrefix == BSE_SUFFIX)
                {
                    tsDataFile = parentDir + "/" + date + "/" + rootPrefix + "_BSE_TSMETADATA.dat";
                }
                else if (childPrefix == DGCX_SUFFIX)
                {
                    tsDataFile = parentDir + "/" + date + "/" + rootPrefix + "_DGCX_TSMETADATA.dat";
                }
                else if (childPrefix == MCX_SUFFIX)
                {
                    tsDataFile = parentDir + "/" + date + "/" + rootPrefix + "_MCX_TSMETADATA.dat";
                }
                else
                {
                    tsDataFile = parentDir + "/" + date + "/" + rootPrefix + "_" + childPrefix + "_TSMETADATA.dat";
                }
            }
            else
            {
                if ((pos = filePrefix.find("NSE")) != std::string::npos)
                    tsDataFile = parentDir + "/" + date + "/" + filePrefix + "_COLO_TSMETADATA.dat";
                else
                    tsDataFile = parentDir + "/" + date + "/" + filePrefix + "_" + "TSMETADATA.dat";
            }

            return make_pair(tsDataFile, parentDir + "/" + date + "/" + rootPrefix + "_METADATA.dat");
        }
    }
}
#endif
