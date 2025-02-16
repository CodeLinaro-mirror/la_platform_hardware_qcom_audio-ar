/*
 *
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 */

#pragma once
#include <android-base/logging.h>
#include <stdlib.h>
#include <stdint.h>
#include <log/log.h>

template <typename T>
class HalRingBuffer
{

private:
    T*          m_buffer;
    uint32_t    m_bufferLength;
    bool        m_isFull;
    uint32_t    m_readIdx;
    uint32_t    m_writeIdx;
    uint32_t    m_size;

public:
    HalRingBuffer(uint32_t capacity) : m_readIdx(0), m_writeIdx(0)
    {
        m_buffer = (T *)calloc(capacity, sizeof(T));
        m_bufferLength = (NULL != m_buffer) ? capacity : 0;
        m_isFull = false;
        LOG(INFO) << __func__ << " m_bufferLength : "<< m_bufferLength << " sizeof item : " << sizeof(T);
    }

    ~HalRingBuffer()
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
        if (m_buffer != NULL) {
            if( m_readIdx == m_writeIdx) {
                return m_isFull ? 0 : m_bufferLength;
            } else if (m_writeIdx > m_readIdx) {
                return ( m_bufferLength + m_readIdx - m_writeIdx);
            } else {
                return ( m_readIdx - m_writeIdx);
            }
        } else {
            return 0;
        }
        return m_bufferLength - m_size;
    }

    uint32_t avaiableWrittenBufferSize()
    {
        if (m_buffer != NULL){
            if (m_writeIdx == m_readIdx) {
                return m_isFull ? m_bufferLength : 0;
            } else if (m_writeIdx > m_readIdx) {
                return ( m_writeIdx - m_readIdx);
            } else {
                return ( m_bufferLength + m_writeIdx- m_readIdx);
            }
        } else {
            return 0;
        }
        return m_size;
    }
    void clear()
    {
        m_isFull = false;
        m_size = 0;
        m_readIdx = 0;
        m_writeIdx = 0;
    }
    uint32_t write(const T* data, uint32_t length)
    {
        uint32_t availableLength = 0;
        uint32_t itemsToWrite = length;
        if (m_buffer != NULL && data != NULL && length > 0){
            availableLength = getFreeBufferSize();
            if (availableLength < length) {
                itemsToWrite = availableLength;
                LOG(INFO) << __func__ << " No enough buffer space to write. Droping "<< (length - itemsToWrite) << "items";
            }
            if (itemsToWrite > 0) {
                int currentLoop = std::min(itemsToWrite, static_cast<int>(m_bufferLength) - m_writeIdx);
                int nextLoop = itemsToWrite - currentLoop;

                std::memcpy((m_buffer+m_writeIdx), data, currentLoop * sizeof(T));
                if (nextLoop > 0) {
                    std::memcpy(m_buffer, data + currentLoop, nextLoop * sizeof(T));
                }
                m_writeIdx = (m_writeIdx + itemsToWrite) % m_bufferLength;
                if (m_writeIdx == m_readIdx) {
                    m_isFull = true;
                    LOG(ERROR) << __func__ << "buffer is full";
                }
                m_size += itemsToWrite;
            } else {
               LOG(ERROR) << __func__ << " can not write to buffer due to no free space";
               itemsToWrite = 0;
            }
        } else {
           LOG(ERROR) << __func__ << " can not write to buffer due to invalid values";
           itemsToWrite = 0;
        }
//        LOG(DEBUG) << __func__ << " m_writeIdx "<< m_writeIdx << " m_readIdx "<< m_readIdx;
//        LOG(DEBUG) << __func__ << " itemsToWrite "<< itemsToWrite << " remained items "<< m_size;
        return itemsToWrite;
    }

    uint32_t read(T * dest, uint32_t length)
    {
        uint32_t itemsToRead = length ;
        uint32_t filledItem = 0 ;

        if (m_buffer != NULL && dest != NULL && length > 0){
            filledItem = m_bufferLength - getFreeBufferSize();
            if (filledItem < length) {
                itemsToRead = filledItem;
                LOG(INFO) << __func__ << " no enough data to read, just read remained data";
            }
            if (itemsToRead > 0) {
                int currentLoop = std::min(itemsToRead, static_cast<int>(m_bufferLength) - m_readIdx);
                int nextLoop = itemsToRead - currentLoop;

                std::memcpy(dest, (m_buffer+m_readIdx), currentLoop * sizeof(T));
                if (nextLoop > 0) {
                    std::memcpy(dest + currentLoop, m_buffer, nextLoop * sizeof(T));
                }
                m_readIdx = (m_readIdx + itemsToRead) % m_bufferLength;
            } else {
                LOG(ERROR) << __func__ << "Not enough data to read";
                itemsToRead = 0;
            }
            m_size -= itemsToRead;
            m_isFull = false;
        } else {
            LOG(ERROR) << __func__ << " can not read to buffer due to invalid values";
            itemsToRead = 0;
        }
//        LOG(DEBUG) << __func__ << " m_writeIdx "<< m_writeIdx << " m_readIdx "<< m_readIdx;
//        LOG(DEBUG) << __func__ << " itemsToRead "<< itemsToRead << " remained items "<< m_size;
        return itemsToRead;
    }

};
