/*
 *
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 */


#define LOG_TAG "AHAL_AudioExtension_QTI"

#include <android-base/logging.h>
#include <dlfcn.h>
#include <dlfcn.h>
#include <log/log.h>
#include "extensions/hal_ecnr.h"
#include "extensions/hal_ecnr_tune.h"



void * txThread( void * pvArg )
{
    int ret = 0;
    int nBytesSent = 0;
    int nTxRingbufferRead = 0;
    char txSocketBuffer[TX_SOCKET_BUFFER_SIZE] ={0,};
    tECNR_TuneIFData * pECNR_TuneIFData = (tECNR_TuneIFData*)pvArg;

    LOG(DEBUG) << __func__ << " Enter";
    if ( pECNR_TuneIFData == NULL) {
        LOG(ERROR) << __func__ << " End : pECNR_TuneIFData is NULL";
           return NULL;
    }
    if (pECNR_TuneIFData->pWaitSemaTx == NULL) {
        LOG(ERROR) << __func__ << " End : pWaitSemaTx is NULL";
        return NULL;
    }

    while (pECNR_TuneIFData->tx_thread_running) {
        LOG(DEBUG) << __func__ << " sem_wait";
        ret = sem_wait( pECNR_TuneIFData->pWaitSemaTx );
        LOG(DEBUG) << __func__ << " sem_wait is done";

        if ( ret ) {
            LOG(INFO) << __func__ << " sem_wait is failed, stopping txThread";
            pECNR_TuneIFData->tx_thread_running = false;
            continue;
        } else {
            if ( pECNR_TuneIFData->tx_thread_running ) {
                if ( pECNR_TuneIFData->tuneIntefaceConnection )
                {
                    nTxRingbufferRead = pECNR_TuneIFData->txRingbuffer->read( txSocketBuffer, TX_SOCKET_BUFFER_SIZE );
                    LOG(INFO) << __func__ << " read from txRingbuffer " << nTxRingbufferRead <<"bytes";
                    if ( nTxRingbufferRead > 0 ) {
                        nBytesSent = send(pECNR_TuneIFData->client_socket, txSocketBuffer, nTxRingbufferRead, 0 );
                        if ( nBytesSent < 0 ) {
                            LOG(INFO) << __func__ << " failed to send data error : " << nBytesSent;
                            pECNR_TuneIFData->tx_thread_running = false;
                        } else {
                            if ( nBytesSent < nTxRingbufferRead ) {
                                LOG(ERROR) << __func__ << " " << nTxRingbufferRead - nBytesSent   << "bytes are dropped";
                            }
                        }
                    }
                }
            }
        }
    }

    LOG(DEBUG) << __func__ << " End";
    return NULL;
}

void * rxThread( void * pvArg )
{
    int ret = 0;
    int nBytesReceived  = 0;
    int nRxRingbufferWritten  = 0;
    char rxSocketBuffer[RX_SOCKET_BUFFER_SIZE]= {0,};
    tECNR_TuneIFData * pECNR_TuneIFData = (tECNR_TuneIFData*)pvArg;

    LOG(DEBUG) << __func__ << " Enter";
    if ( NULL == pECNR_TuneIFData )
    {
        LOG(ERROR) << __func__ << " End : pECNR_TuneIFData is NULL";
       return NULL;
    }

    while ( pECNR_TuneIFData->rx_thread_running ) {
        if ( !pECNR_TuneIFData->tuneIntefaceConnection )
        {
            ret = listen( pECNR_TuneIFData->server_socket, NUM_SOCKET_CONNECTION );
            if ( ret )
            {
                LOG(ERROR) << __func__ << " failed to listen, stopping rxThread";
                pECNR_TuneIFData->rx_thread_running = false;
            } else {
                LOG(INFO) << __func__ << " Waiting for connection on port " << ntohs(pECNR_TuneIFData->server_address.sin_port);
                pECNR_TuneIFData->client_socket = accept(pECNR_TuneIFData->server_socket,
                                                    NULL,
                                                    NULL );

                if ( pECNR_TuneIFData->client_socket == EINVALID_SOCKET ) {
                    LOG(ERROR) << __func__ << " failed to accept connection, stopping rxThread";
                    pECNR_TuneIFData->rx_thread_running = false;
                } else {
                    pECNR_TuneIFData->tuneIntefaceConnection = true;
                    LOG(INFO) << __func__ << " Connection is accepted on port " << ntohs(pECNR_TuneIFData->server_address.sin_port);
                }
            }
        }else {
            nBytesReceived = recv(pECNR_TuneIFData->client_socket, rxSocketBuffer, RX_SOCKET_BUFFER_SIZE, 0 );
            if ( nBytesReceived <= 0 ) {
                LOG(ERROR) << __func__ << " failed to recv, wait for new connection";
                pECNR_TuneIFData->tuneIntefaceConnection = false;
            } else {
                LOG(INFO) << __func__ << " nBytesReceived " << nBytesReceived;

                nRxRingbufferWritten = pECNR_TuneIFData->rxRingbuffer->write( rxSocketBuffer, nBytesReceived );
                if ( nBytesReceived != nRxRingbufferWritten ) {
                     LOG(ERROR) << __func__ << " Requested bytes to write : " << nBytesReceived << ", but written bytes : " << nRxRingbufferWritten;
                }
            }
        }
    }
    LOG(DEBUG) << __func__ << " End";
    return NULL;
}

int HalECNRExtension::audio_extn_setupECNR_TuneIF(tECNR_TuneIFData* pECNR_TuneIFData, int portid)
{
    int ret = 0;
    if (property_get_bool(ECNR_TUNE_FEATURE_PROP, false)){
        if (pECNR_TuneIFData == NULL){
            LOG(ERROR) << __func__ << " END : pECNR_TuneIFData is NULL";
            ret = EINVAL_ARG;
            return ret;
        }
        int iSocketOption;
        pECNR_TuneIFData->rx_thread_running = true;
        pECNR_TuneIFData->tx_thread_running = true;
        pECNR_TuneIFData->tuneIntefaceConnection = 0;
        pECNR_TuneIFData->pWaitSemaTx = NULL;
        pECNR_TuneIFData->rxRingbuffer = NULL;
        pECNR_TuneIFData->txRingbuffer = NULL;
        pECNR_TuneIFData->rxRingbuffer = new(std::nothrow) HalRingBuffer<char>(RX_RINGBUFFER_SIZE);
        if (NULL == pECNR_TuneIFData->rxRingbuffer)
        {
            LOG(ERROR) << __func__ << " failed to create rxRingbuffer";
            ret = ENOMEM_BUFFER;
            goto destroy_ring_buffer;
        }
         pECNR_TuneIFData->txRingbuffer = new(std::nothrow) HalRingBuffer<char>(TX_RINGBUFFER_SIZE);
        if (NULL == pECNR_TuneIFData->txRingbuffer)
        {
            LOG(ERROR) << __func__ << " failed to create txRingbuffer";
            ret = ENOMEM_BUFFER;
            goto destroy_ring_buffer;

        }
        pECNR_TuneIFData->server_socket = socket(AF_INET, SOCK_STREAM, 0 );

        if (pECNR_TuneIFData->server_socket == EINVALID_SOCKET) {
            LOG(ERROR) << __func__ << " socket creation failed with server portid " << portid << " error : "<< strerror(errno);
            ret = errno;
            goto destroy_ring_buffer;
        }
        LOG(DEBUG) << __func__ << " setup server with port id "<<portid;

        iSocketOption =1;
        ret = setsockopt( pECNR_TuneIFData->server_socket, SOL_SOCKET, SO_REUSEADDR, &iSocketOption, sizeof(int));
        if (ret) {
            LOG(ERROR) << __func__ << " failed to setsockopt";
        }
        pECNR_TuneIFData->server_address.sin_family = AF_INET;
        pECNR_TuneIFData->server_address.sin_addr.s_addr = INADDR_ANY;
        pECNR_TuneIFData->server_address.sin_port = htons(portid);
        ret = bind( pECNR_TuneIFData->server_socket, (struct sockaddr*)&(pECNR_TuneIFData->server_address), sizeof(sockaddr_in));
        if ( ret < 0 ) {
            LOG(ERROR) << __func__ << " failed to bind socket";
            ret = errno;
            goto shutdown_socketserver;
        }
        pECNR_TuneIFData->pWaitSemaTx = (sem_t*) malloc(sizeof(sem_t));
        if (!pECNR_TuneIFData->pWaitSemaTx) {
            LOG(ERROR) << __func__ << " failed to allock memory for sem";
            goto shutdown_socketserver;
       }
        ret = sem_init(pECNR_TuneIFData->pWaitSemaTx, 0, 0);
        if ( ret ) {
            LOG(ERROR) << __func__ << " failed to init sema";
            goto shutdown_socketserver;
        }
       ret = pthread_create(&(pECNR_TuneIFData->receivingThread),
                        (const pthread_attr_t *) NULL,
                        rxThread, pECNR_TuneIFData);
       if ( ret ) {
            LOG(ERROR) << __func__ << " failed to run receivingThread";
            pECNR_TuneIFData->receivingThread = 0;
            pECNR_TuneIFData->rx_thread_running = false;
            goto shutdown_socketserver;
       }
       ret = pthread_create(&(pECNR_TuneIFData->transmittingThread),
                        (const pthread_attr_t *) NULL,
                        txThread, pECNR_TuneIFData);
       if ( ret ) {
            LOG(ERROR) << __func__ << " failed to run transmittingThread";
            pECNR_TuneIFData->transmittingThread = 0;
            pECNR_TuneIFData->tx_thread_running = false;
            goto stop_rxThread;
        }

        pECNR_TuneIFData->enabled = true;
   }
   LOG(DEBUG) << __func__ << " End : "<< pECNR_TuneIFData->enabled ;
   return ret;

stop_rxThread :
    if (pECNR_TuneIFData->rx_thread_running){
        pECNR_TuneIFData->rx_thread_running = false;
        pthread_join(pECNR_TuneIFData->receivingThread, 0 );
    }
    pECNR_TuneIFData->receivingThread = 0;
    sem_destroy(pECNR_TuneIFData->pWaitSemaTx );
shutdown_socketserver:
    shutdown(pECNR_TuneIFData->server_socket, ECNR_SOCKET_SHUT_RDWR);
    close(pECNR_TuneIFData->server_socket);
    pECNR_TuneIFData->server_socket = EINVALID_SOCKET;
destroy_ring_buffer:
    if(pECNR_TuneIFData->rxRingbuffer != NULL)
    {
        delete(std::nothrow, pECNR_TuneIFData->rxRingbuffer);
        pECNR_TuneIFData->rxRingbuffer = NULL;
    }
    if(pECNR_TuneIFData->txRingbuffer != NULL)
    {
        delete(std::nothrow, pECNR_TuneIFData->txRingbuffer);
        pECNR_TuneIFData->txRingbuffer = NULL;
    }
    pECNR_TuneIFData->enabled = false;
    LOG(ERROR) << __func__ << " End : error " << ret;
    return ret;
}

int HalECNRExtension::audio_extn_close_TuneIF(tECNR_TuneIFData* pECNR_TuneIFData)
{
    int ret = 0;
    if(pECNR_TuneIFData == NULL){
        LOG(ERROR) << __func__ << " END : pECNR_TuneIFData is NULL";
        ret = EINVAL_ARG;
        return ret;
    }
    if(pECNR_TuneIFData->enabled){
        LOG(DEBUG) << __func__ << " release resource for ecnrTuneIF";
        if(pECNR_TuneIFData->tuneIntefaceConnection){
            LOG(DEBUG) << __func__ << " Try to shutdown client socket";
            shutdown(pECNR_TuneIFData->client_socket, ECNR_SOCKET_SHUT_RDWR);
            close(pECNR_TuneIFData->client_socket);
            pECNR_TuneIFData->client_socket = EINVALID_SOCKET;
        }
        LOG(DEBUG) << __func__ << " Try to shutdown socket server";
        shutdown(pECNR_TuneIFData->server_socket, ECNR_SOCKET_SHUT_RDWR);
        close(pECNR_TuneIFData->server_socket);
        pECNR_TuneIFData->server_socket = EINVALID_SOCKET;
        if (pECNR_TuneIFData->rx_thread_running) {
            pECNR_TuneIFData->rx_thread_running = false;
            LOG(DEBUG) << __func__ << " Try to join receivingThread";
            pthread_join(pECNR_TuneIFData->receivingThread, 0 );
       }
        if (!pECNR_TuneIFData->tx_thread_running){
            LOG(DEBUG) << __func__ << " Try to join transmittingThread";
            pECNR_TuneIFData->tx_thread_running = false;
            ret = sem_post(pECNR_TuneIFData->pWaitSemaTx);
            if (ret){
                LOG(DEBUG) << __func__ << " sem_post is failed ";
            }
            pthread_join(pECNR_TuneIFData->transmittingThread, 0 );
        }
        sem_destroy(pECNR_TuneIFData->pWaitSemaTx);
        if(pECNR_TuneIFData->pWaitSemaTx){
            LOG(DEBUG) << __func__ << " free sem memory ";
            free(pECNR_TuneIFData->pWaitSemaTx);
            pECNR_TuneIFData->pWaitSemaTx = NULL;
        }
        if(pECNR_TuneIFData->rxRingbuffer != NULL)
        {
            delete(std::nothrow, pECNR_TuneIFData->rxRingbuffer);
            pECNR_TuneIFData->rxRingbuffer = NULL;
        }
        if(pECNR_TuneIFData->txRingbuffer != NULL)
        {
            delete(std::nothrow, pECNR_TuneIFData->txRingbuffer);
            pECNR_TuneIFData->txRingbuffer = NULL;
        }
        pECNR_TuneIFData->enabled = false;
    }else {
//        LOG(DEBUG) << __func__ << " skip to release resource for pECNR_TuneIFData ";
   }
   return 0;
}

int HalECNRExtension::audio_extn_get_TuneIO_buffer(tECNR_TuneIFData* pECNR_TuneIFData, tECNR_TuneIO* pECNR_TuneIO)
{
   int ret_getTuneIObuffer = -1;
   int nTuneInBytes = 0;
   if(pECNR_TuneIFData == NULL || pECNR_TuneIO == NULL) {
         LOG(ERROR) << __func__ << " invalied input parameters ";
           return ret_getTuneIObuffer;
   }
   if(pECNR_TuneIFData->enabled){
        if ( pECNR_TuneIFData->tuneIntefaceConnection ) {
            nTuneInBytes = pECNR_TuneIFData->rxRingbuffer->avaiableWrittenBufferSize();
            if ( nTuneInBytes > 0 )
                nTuneInBytes = pECNR_TuneIFData->rxRingbuffer->read(pECNR_TuneIFData->pECNRTuneBufferIn, TUNE_BUFFER_RX_SIZE);
            //LOG(INFO) << __func__ << " read from rxRingbuffer " << nTuneInBytes << "bytes";
            if ( nTuneInBytes >= 0 ) {
                pECNR_TuneIO->InBuffer = pECNR_TuneIFData->pECNRTuneBufferIn;
                pECNR_TuneIO->InBufferSize = TUNE_BUFFER_RX_SIZE;
                pECNR_TuneIO->InBufferUsedSize = (unsigned int)nTuneInBytes;

                pECNR_TuneIO->OutBuffer = pECNR_TuneIFData->pECNRTuneBufferOut;
                pECNR_TuneIO->OutBufferSize = TUNE_BUFFER_TX_SIZE;
                pECNR_TuneIO->OutBufferUsedSize = 0;
                ret_getTuneIObuffer = nTuneInBytes;
                if (nTuneInBytes > 0)
                    LOG(INFO) << __func__ << " fill InBuffer " << nTuneInBytes  << "bytes";
           } else {
                LOG(ERROR) << __func__ << " failed to get available data error : " << nTuneInBytes ;
           }
        }
   }
   return ret_getTuneIObuffer;
}
int HalECNRExtension::audio_extn_feedback_TuneIO_buffer(tECNR_TuneIFData* pECNR_TuneIFData, tECNR_TuneIO* pECNR_TuneIO)
{
    int ret = 0;
    int nTuneOutBytes = 0;
    int nTxRingbufferWritten = 0 ;

    if (pECNR_TuneIFData == NULL ||pECNR_TuneIO == NULL ) {
         LOG(ERROR) << __func__ << " invalied input parameters ";
           return nTxRingbufferWritten;
    }
    nTuneOutBytes = pECNR_TuneIO->OutBufferUsedSize;
    if(pECNR_TuneIFData->enabled){
        if ( pECNR_TuneIFData->tuneIntefaceConnection ) {
            if ( nTuneOutBytes > 0 ) {
                nTxRingbufferWritten = pECNR_TuneIFData->txRingbuffer->write( pECNR_TuneIFData->pECNRTuneBufferOut, nTuneOutBytes);
                //LOG(INFO) << __func__ << " fill txRingbuffer " << nTxRingbufferWritten << "bytes";
                if ( nTxRingbufferWritten != nTuneOutBytes ){
                     LOG(ERROR) << __func__ << " Requested bytes to write : " << nTuneOutBytes << ", but written bytes : " << nTxRingbufferWritten;
                }
                ret = sem_post(pECNR_TuneIFData->pWaitSemaTx);
                LOG(DEBUG) << __func__ << " sem_post to send tuneIObuffer";
                if (ret){
                    LOG(ERROR) << __func__ << " sem_post returns error ";
                }
            }
        }
    }
   return nTxRingbufferWritten;
}
