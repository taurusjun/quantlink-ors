#ifndef RMSLOGINSTRUCT_H
#define RMSLOGINSTRUCT_H

#include "rmsencrypter.h"

namespace illuminati
{

    const int MAX_KEY_SIZE = 200;
    const int MAX_PASSWORD_SIZE = 100;

    struct RmsLoginStruct
    {
        bool status;
        char key[MAX_KEY_SIZE];
        char password[MAX_PASSWORD_SIZE];

        void load(std::string file)
        {
            if (file.size() == 0)
            {
                ILOG(INFO) << "Unable to load rms key file: " << file << "\n";
                throw "Unable to load rms key file";
            }
            FILE *fp = fopen(file.c_str(), "rb");
            if (fp == NULL)
            {
                ILOG(INFO) << "Unable to load rms key file: " << file << "\n";
                throw "Unable to load rms key file";
            }
            fseek(fp, 0, SEEK_END);
            size_t size = ftell(fp);
            fseek(fp, 0, SEEK_SET);

            if (size == 0)
            {
                ILOG(FATAL) << "Firmwide configuration is empty: " << file << "\n";
                throw "Firmwide configuration is empty";
            }

            char *buffer = new char[size + 1];
            size_t read = fread(buffer, 1, size, fp);
            if (read != size)
            {
                ILOG(FATAL) << "Error in reading firmwide configuration: " << file << "\n";
                throw("Error in reading firmwide configuration");
            }
            char *outputbuffer = new char[size + 1];
            illuminati::security::XORCipher cipher(RMS_ENCRYPTION_KEY);
            cipher.decrypt(buffer, size, outputbuffer);
            outputbuffer[size] = 0;
            ILOG(INFO) << "After decrypting rms login struct: \n";
            ILOG(INFO) << outputbuffer << "\n";

            illuminati::XmlReader reader;
            reader.parse(outputbuffer, size);
            delete[] outputbuffer;
            delete[] buffer;
            std::string passwd = reader.childElement("PASSWORD")->getString();
            ILOG(INFO) << "Password: " << passwd << "\n";
            strcpy(password, passwd.c_str());
            std::string strkey = reader.nextElement("KEY")->getString();
            strcpy(key, strkey.c_str());
            ILOG(INFO) << "Key: " << key << "\n";
        }
    };
}

#endif
