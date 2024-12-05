/*
 *
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 */

#pragma once

#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "extensions/tuneringbuffer.h"

#define ECNR_TUNE_FEATURE_PROP "vendor.audio.feature.ecnr_tune.enable"
#define RX_SOCKET_BUFFER_SIZE 3144
#define RX_RINGBUFFER_SIZE (4 * RX_SOCKET_BUFFER_SIZE)
#define TUNE_BUFFER_RX_SIZE RX_SOCKET_BUFFER_SIZE

#define TX_SOCKET_BUFFER_SIZE 2780
#define TX_RINGBUFFER_SIZE (8 * TX_SOCKET_BUFFER_SIZE)
#define TUNE_BUFFER_TX_SIZE TX_SOCKET_BUFFER_SIZE

#define iPROCESS_MILLISECOND_SLEEP 2000

#define EINVALID_SOCKET    -1
#define EINVAL_ARG    -22
#define ENOMEM_BUFFER    -12


#define ECNR_SOCKET_SHUT_RD    0
#define ECNR_SOCKET_SHUT_WR    1
#define ECNR_SOCKET_SHUT_RDWR  2

#define NUM_SOCKET_CONNECTION 1

#define ECNR_PORT_ID_HFP_DL_2012    2012
#define ECNR_PORT_ID_HFP_UL_2013    2013
#define ECNR_PORT_ID_VOIP_TX_2014   2014
#define ECNR_PORT_ID_VOIP_RX_2015   2015
#define ECNR_PORT_ID_VR_TX_2016     2016

typedef struct tECNR_TuneIFData
{
    TuneRingBuffer* rxRingbuffer  = NULL;
    TuneRingBuffer* txRingbuffer  = NULL;
    sem_t* pWaitSemaTx = NULL;
    pthread_t receivingThread = NULL;
    pthread_t transmittingThread   = NULL;
    bool rx_thread_running = false;
    bool tx_thread_running = false;
    bool tuneIntefaceConnection = false;
    int server_socket = -1;
    int client_socket = -1;
    sockaddr_in server_address;
    bool enabled = false;
    char pECNRTuneBufferIn[TUNE_BUFFER_RX_SIZE];
    char pECNRTuneBufferOut[TUNE_BUFFER_TX_SIZE];
}tECNR_TuneIFData;

