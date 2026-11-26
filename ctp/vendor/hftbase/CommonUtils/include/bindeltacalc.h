/*
 * Company: way2wealth-illuminati
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-illuminati.com/
 */

#ifndef BINDELTACALC_H
#define BINDELTACALC_H

#include <bitset>
#include "marketdeltaNew.h"

namespace illuminati
{

    /*---------------------------------------------------------------------------
     * BinDeltaCalc:
     * < T > => The type that needs to converted to Deltas
     * < TBitMeaning > => The sizeof(type) here would represent 1 bit in bitbuffer
     *              the valeus can be int8_t, int16_t, int32_t etc.
     *              higher values would mean that you are using less bit storage
     *              to represent the diffs, but the diff buffer will coarse
     *----------------------------------------------------------------------------
     */
    template <typename T, typename TBitMeaning = int8_t>
    class BinDeltaCalc
    {
        static const int BITS_INA_BYTE = 8;
        static const int MD_BIT_REP = sizeof(TBitMeaning);
        static const int MD_MAX_BITS = (sizeof(T) / MD_BIT_REP);
        static const int BYTEOFFSET =
            (MD_MAX_BITS % BITS_INA_BYTE == 0) ? (MD_MAX_BITS / BITS_INA_BYTE) : ((MD_MAX_BITS + (BITS_INA_BYTE - (MD_MAX_BITS % BITS_INA_BYTE))) / BITS_INA_BYTE);

    private:
        size_t m_byteOffsetReqForBits;
        char m_dataBuffer[2 * sizeof(T)]; // will contain bit and data
        size_t m_dataBufferSize;

        std::bitset<MD_MAX_BITS> m_bits;

    public:
        bool hasDiffs() { return (m_dataBufferSize > m_byteOffsetReqForBits); }
        const char *getBuffer() { return m_dataBuffer; }
        size_t getBufferSize() { return m_dataBufferSize; }
        size_t getBitUsageInBytes() { return m_byteOffsetReqForBits; }
        //***********************************************************************
        // BinDeltaCalc
        //***********************************************************************
        BinDeltaCalc()
        {
            m_byteOffsetReqForBits = getBytesForBitSet();
            resetBuffer();
        }
        //***********************************************************************
        // ~BinDeltaCalc
        //***********************************************************************
        ~BinDeltaCalc() {}
        //***********************************************************************
        // createDelta - return delta size
        //***********************************************************************
        size_t createDelta(const T *struct1)
        {
            resetBuffer();

            char *l_buff = (m_dataBuffer + m_byteOffsetReqForBits);

            memcpy(l_buff, struct1, sizeof(T));
            m_bits.set(); // set all bits

            setBitBufferFromBitSet();
            m_dataBufferSize = m_byteOffsetReqForBits + sizeof(T);
            return (sizeof(T));
        }
        //***********************************************************************
        // createDelta - return delta size
        //***********************************************************************
        size_t createDelta(const T *prev, const T *curr)
        {
            resetBuffer();

            char *l_buff = (m_dataBuffer + m_byteOffsetReqForBits);

            const char *pStruct1 = (const char *)prev;
            const char *pStruct2 = (const char *)curr;
            m_bits.reset();

            int copyIndex = 0;
            for (int i = 0; i < MD_MAX_BITS; i++)
            {
                if (0 != (memcmp((pStruct1 + i * MD_BIT_REP), (pStruct2 + i * MD_BIT_REP), MD_BIT_REP)))
                {
                    m_bits.set(i, true);
                    memcpy((l_buff + copyIndex), (pStruct2 + i * MD_BIT_REP), MD_BIT_REP);
                    copyIndex += MD_BIT_REP;
                }
            }

            setBitBufferFromBitSet();
            m_dataBufferSize = m_byteOffsetReqForBits + copyIndex;
            return (copyIndex);
        }

        /********* Apply delta without copy **************/
        __attribute__((optimize("unroll-loops"))) static size_t applyDelta(T *dest, const char *deltaBuffer, const int deltaBuferSize);

        //***********************************************************************
        // copyStructNApplyDelta:
        // if there is no source (or previous buffer) pass NULL
        //***********************************************************************
        size_t copyStructNApplyDelta(const T *source, T *dest, const char *deltaBuffer, const int deltaBuferSize)
        {
            // resetBuffer();

            if (dest == NULL || deltaBuffer == NULL)
            {
                std::cerr << "dest or deltaBufer cannot be NULL" << std::endl;
                throw("Exception");
            }

            // Set the internal buffer and set the states
            // memcpy(m_dataBuffer, deltaBufer, deltaBuferSize);
            // m_dataBufferSize = deltaBuferSize;
            // setBitSetFromBuffer();

            // construct the new data
            if (source != NULL)
                memcpy(dest, source, sizeof(T));

            char *pdest = (char *)dest;
            const char *l_buff = (deltaBuffer + m_byteOffsetReqForBits);

            int copyIndex = 0;

            const char *buf = deltaBuffer;
            size_t totalcnt = 0;
            for (size_t i = 0; i < m_byteOffsetReqForBits / sizeof(unsigned long); i++)
            {
                unsigned long a = *((unsigned long *)buf);

                totalcnt = i * (sizeof(unsigned long) * 8);
                while (a != 0)
                {
                    int cnt = __builtin_ctzl(a);
                    totalcnt += cnt;

                    memcpy((pdest + (totalcnt * MD_BIT_REP)), (l_buff + copyIndex), MD_BIT_REP);
                    copyIndex += MD_BIT_REP;
                    a >>= (cnt + 1);
                    totalcnt++;
                }
                buf += sizeof(unsigned long);
            }

            if (m_byteOffsetReqForBits % sizeof(unsigned long) != 0)
            {
                unsigned long a = 0;
                memcpy((char *)&a, buf, m_byteOffsetReqForBits % sizeof(unsigned long));

                totalcnt = (m_byteOffsetReqForBits / sizeof(unsigned long)) * (sizeof(unsigned long) * 8);
                while (a != 0)
                {
                    int cnt = __builtin_ctzl(a);
                    totalcnt += cnt;

                    memcpy((pdest + (totalcnt * MD_BIT_REP)), (l_buff + copyIndex), MD_BIT_REP);
                    copyIndex += MD_BIT_REP;
                    a >>= (cnt + 1);
                    totalcnt++;
                }
                buf += sizeof(unsigned long);
            }
            return (copyIndex);
        }

    private:
        //***********************************************************************
        // just to make sure we are clean
        //***********************************************************************
        inline void resetBuffer() __attribute__((always_inline))
        {
            memset(m_dataBuffer, 0, sizeof(m_dataBuffer));
            m_dataBufferSize = 0;
        }
        //***********************************************************************
        // setBitBufferFromBitSet
        //***********************************************************************
        inline void setBitBufferFromBitSet() __attribute__((always_inline))
        {
            for (size_t i = 0; i < m_byteOffsetReqForBits; i++)
            {
                m_dataBuffer[i] = 0;
                int offset = i * BITS_INA_BYTE;
                for (int bit = 0; bit < BITS_INA_BYTE; bit++)
                {
                    m_dataBuffer[i] |= m_bits[offset + bit] << bit;
                }
            }
        }
        //***********************************************************************
        // setBitSetFromBuffer
        //***********************************************************************
        inline void setBitSetFromBuffer() __attribute__((always_inline))
        {
            for (size_t i = 0; i < m_byteOffsetReqForBits; i++)
            {
                char byte = m_dataBuffer[i];
                int offset = i * BITS_INA_BYTE;

                for (int bit = 0; bit < BITS_INA_BYTE; bit++)
                {
                    m_bits[offset + bit] = byte & 1;
                    byte >>= 1;
                }
            }
        }
        //***********************************************************************
        // getBytesForBitSet
        //***********************************************************************
        inline int getBytesForBitSet() __attribute__((always_inline))
        {
            if (MD_MAX_BITS < BITS_INA_BYTE)
                return sizeof(char);

            int bits = 0;
            if (0 != (MD_MAX_BITS % BITS_INA_BYTE))
                bits = (MD_MAX_BITS + (BITS_INA_BYTE - (MD_MAX_BITS % BITS_INA_BYTE)));
            else
                bits = MD_MAX_BITS;

            return (bits / BITS_INA_BYTE);
        }
    };

    template <typename T, typename TBitMeaning>
    size_t BinDeltaCalc<T, TBitMeaning>::applyDelta(T *dest, const char *deltaBuffer, const int deltaBuferSize)
    {
        // resetBuffer();

        if (dest == NULL || deltaBuffer == NULL)
        {
            std::cerr << "dest or deltaBufer cannot be NULL" << std::endl;
            throw("Exception");
        }

        char *pdest = (char *)dest;
        const char *l_buff = (deltaBuffer + BYTEOFFSET);

        int copyIndex = 0;

        const char *buf = deltaBuffer;
        size_t totalcnt = 0;
        for (size_t i = 0; i < BYTEOFFSET / sizeof(unsigned long); i++)
        {
            unsigned long a = *((unsigned long *)buf);

            totalcnt = i * (sizeof(unsigned long) * 8);
            while (a != 0)
            {
                int cnt = __builtin_ctzl(a);
                totalcnt += cnt;

                memcpy((pdest + (totalcnt * MD_BIT_REP)), (l_buff + copyIndex), MD_BIT_REP);
                copyIndex += MD_BIT_REP;
                if (cnt + 1 >= (sizeof(unsigned long) * 8))
                    break;
                a >>= (cnt + 1);
                totalcnt++;
            }
            buf += sizeof(unsigned long);
        }

        if (BYTEOFFSET % sizeof(unsigned long) != 0)
        {
            unsigned long a = 0;
            memcpy((char *)&a, buf, BYTEOFFSET % sizeof(unsigned long));

            totalcnt = (BYTEOFFSET / sizeof(unsigned long)) * (sizeof(unsigned long) * 8);
            while (a != 0)
            {
                int cnt = __builtin_ctzl(a);
                totalcnt += cnt;

                memcpy((pdest + (totalcnt * MD_BIT_REP)), (l_buff + copyIndex), MD_BIT_REP);
                copyIndex += MD_BIT_REP;
                a >>= (cnt + 1);
                totalcnt++;
            }
            buf += sizeof(unsigned long);
        }
        return (copyIndex);
    }

    template <>
    inline
        __attribute__((optimize("unroll-loops")))
        size_t
        BinDeltaCalc<infra::DataWrapperV3, int8_t>::applyDelta(infra::DataWrapperV3 *dest, const char *deltaBuffer, const int deltaBuferSize)
    {
        // try more specific optimizations here

        if (dest == NULL || deltaBuffer == NULL)
        {
            std::cerr << "dest or deltaBufer cannot be NULL" << std::endl;
            throw("Exception");
        }

        char *pdest = (char *)dest;
        const char *l_buff = (deltaBuffer + BYTEOFFSET);

        int copyIndex = 0;

        const char *buf = deltaBuffer;
        size_t totalcnt = 0;
        for (size_t i = 0; i < BYTEOFFSET / sizeof(unsigned long); i++)
        {
            unsigned long a = *((unsigned long *)buf);

            totalcnt = i * (sizeof(unsigned long) * 8);
            while (a != 0)
            {
                int cnt = __builtin_ctzl(a);
                totalcnt += cnt;

                pdest[totalcnt] = l_buff[copyIndex];

                copyIndex++;
                if (cnt + 1 >= (sizeof(unsigned long) * 8))
                    break;
                a >>= (cnt + 1);
                totalcnt++;
            }
            buf += sizeof(unsigned long);
        }

        if (BYTEOFFSET % sizeof(unsigned long) != 0)
        {
            unsigned long a = 0;
            memcpy((char *)&a, buf, BYTEOFFSET % sizeof(unsigned long));

            totalcnt = (BYTEOFFSET / sizeof(unsigned long)) * (sizeof(unsigned long) * 8);
            while (a != 0)
            {
                int cnt = __builtin_ctzl(a);
                totalcnt += cnt;

                pdest[totalcnt] = l_buff[copyIndex];
                // memcpy( (pdest + (totalcnt * MD_BIT_REP)), (l_buff + copyIndex), MD_BIT_REP);
                copyIndex++; //= MD_BIT_REP;
                a >>= (cnt + 1);
                totalcnt++;
            }
            buf += sizeof(unsigned long);
        }
        return (copyIndex);
    }
} // illuminati

#endif // BINDELTACALC_H
