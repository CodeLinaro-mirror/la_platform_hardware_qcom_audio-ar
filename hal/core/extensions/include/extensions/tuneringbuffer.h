/*
 *
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 */

#pragma once
#include <stdlib.h>
#include <stdint.h>
#include <log/log.h>

class TuneRingBuffer
{

private:
    char*       m_buffer;
    uint32_t    m_bufferLength;
    bool     m_isFull;
    uint32_t    m_readIdx;
    uint32_t    m_writeIdx;
    uint32_t    m_size;

public:
    TuneRingBuffer(uint32_t capacity) : m_readIdx(0), m_writeIdx(0)
    {
        m_buffer = (char *)calloc(capacity, sizeof(char));
        m_bufferLength = (NULL != m_buffer) ? capacity : 0;
        m_isFull = false;
        LOG(INFO) << __func__ << " buffer alloc "<< m_bufferLength;
    }

    ~TuneRingBuffer()
    {
        LOG(INFO) << __func__ ;
        if (NULL != m_buffer)
        {
            free(m_buffer);
            m_buffer = NULL;
        }
    }

    inline uint32_t getBufferSize()
    {
        return m_bufferLength;
    }

    uint32_t getFreeBufferSize()
    {
        if (m_buffer != NULL ){
            if( m_readIdx == m_writeIdx) {
                return m_isFull ? 0 : m_bufferLength;
            } else if ( m_writeIdx > m_readIdx ) {
                return ( m_bufferLength + m_readIdx - m_writeIdx );
            } else {
                return ( m_readIdx - m_writeIdx);
            }
        } else {
            return 0;
        }
        return m_bufferLength - m_size;
    }
#if 0
    uint32_t avaiableWrittenBufferSize()
    {
        if (m_buffer != NULL ){
            if( m_writeIdx == m_readIdx) {
                return m_isFull ? m_bufferLength : 0;
            } else if ( m_writeIdx > m_readIdx ) {
                return ( m_writeIdx - m_readIdx );
            } else {
                return ( m_bufferLength + m_writeIdx- m_readIdx );
            }
        } else {
            return 0;
        }
    }
#endif
    void clear()
    {
        m_isFull = false;
        m_size = 0;
        m_readIdx = 0;
        m_writeIdx = 0;
    }

    uint32_t write(const char * packets, uint32_t packet_size)
    {
        uint32_t freebytes = 0;
        uint32_t byteToWrite = packet_size ;
        uint32_t writeBytes = 0 ;
        if (m_buffer != NULL && packets != NULL && packet_size > 0 ){
            freebytes = getFreeBufferSize();
            if ( freebytes < packet_size ) {
                byteToWrite = freebytes;
                LOG(ERROR) << __func__ << " No enough buffer space to write. Droping "<< (packet_size - byteToWrite) << "packets";
            }
            for(uint32_t i=0; i< byteToWrite; i++)
            {
                writeBytes++;
                m_buffer[m_writeIdx++] = packets[i];
                m_writeIdx %= m_bufferLength;

                if(m_writeIdx == m_readIdx)
                {
                    LOG(ERROR) << __func__ << " stop writing, buffer is full";
                    m_isFull = true;
                    break;
                }
            }
            m_size += writeBytes;

        } else {
                LOG(ERROR) << __func__ << " can not write to buffer due to invalid values";
        }
//        LOG(ERROR) << __func__ << " m_writeIdx "<< m_writeIdx << " m_readIdx "<< m_readIdx;
//        LOG(ERROR) << __func__ << " writeBytes "<< writeBytes << " remained bytes "<< m_size;
        return writeBytes;
    }

    uint32_t read(char * packets, uint32_t max_packet_size)
    {
        uint32_t readBytes = 0;
        uint32_t byteToRead = max_packet_size ;
        uint32_t filledbytes = 0 ;

        if (m_buffer != NULL && packets != NULL && max_packet_size > 0 ){
            filledbytes = m_bufferLength - getFreeBufferSize();
            if ( filledbytes < max_packet_size ) {
                byteToRead = filledbytes;
            }
            for(uint32_t i = 0; i< byteToRead; i++)
            {
                readBytes++;
                packets[i] = m_buffer[m_readIdx++];
                m_readIdx %= m_bufferLength;

                //if(m_writeIdx == m_readIdx)
                //{
                //    LOG(ERROR) << __func__ << " stop reading, no packet to read further";
                //    break;
                //}
            }
            m_size -= readBytes;
            m_isFull = false;
        } else {
                LOG(ERROR) << __func__ << " can not read to buffer due to invalid values";
        }
//        LOG(ERROR) << __func__ << " m_writeIdx "<< m_writeIdx << " m_readIdx "<< m_readIdx;
//        LOG(ERROR) << __func__ << " readBytes "<< readBytes << " remained bytes "<< m_size;
        return readBytes;
    }

};
