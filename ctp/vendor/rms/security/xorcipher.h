/*
 * xorcipher.h
 *
 *  Created on: 05-Sep-2013
 *      Author: jijith
 */

#ifndef XORCIPHER_H_
#define XORCIPHER_H_

#include <string>
#include "log/Log.h"
#include <cstdio>

namespace ors
{
    namespace security
    {
        class XORCipher
        {
        private:
            std::string key;

        public:
            XORCipher(std::string key_in) : key(key_in)
            {
            }

            void setEncryptionKey(std::string key_in)
            {
                key = key_in;
            }

            void encryptToFile(std::string filename, const char *buffer, size_t len)
            {
                char *outputBuffer = new char[len];
                encrypt(buffer, len, outputBuffer);
                FILE *fp = fopen(filename.c_str(), "wb");
                fwrite(outputBuffer, 1, len, fp);
                fclose(fp);
                delete[] outputBuffer;
            }

            // Encrypt the whole contents file

            void encrypt(std::string file, char *&outputbuffer, size_t &size)
            {
                FILE *fp = fopen(file.c_str(), "rb");
                fseek(fp, 0, SEEK_END);
                size = ftell(fp);
                fseek(fp, 0, SEEK_SET);

                if (size == 0)
                {
                    ORS_LOG(FATAL) << "Firmwide configuration is empty: " << file << "\n";
                    throw "Firmwide configuration is empty";
                }

                char *buffer = new char[size + 1];
                size_t read = fread(buffer, 1, size, fp);
                if (read != size)
                {
                    ORS_LOG(FATAL) << "Error in reading firmwide configuration: " << file << "\n";
                    throw("Error in reading firmwide configuration");
                }
                outputbuffer = new char[size + 1];
                encrypt(buffer, size, outputbuffer);
                outputbuffer[size] = '\0';
                delete[] buffer;
                return;
            }

            void decrypt(std::string file, char *&outputbuffer, size_t &len)
            {
                encrypt(file, outputbuffer, len);
            }

            void encrypt(const char *input, uint64_t inputLen,
                         char *output)
            {
                for (uint64_t i = 0; i < inputLen; i++)
                {
                    char keyByte = key.at(i % key.size());
                    output[i] = input[i] ^ keyByte;
                }
            }

            void decrypt(const char *input, uint64_t inputLen,
                         char *output)
            {
                encrypt(input, inputLen, output);
            }
        };
    }
}

#endif /* XORCIPHER_H_ */
