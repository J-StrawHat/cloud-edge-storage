/**
 * @file cloudOptThead.h
 * @author cyh, zwx
 * @brief cloud main thead 
 * @version 0.1
 * @date 2023-07-16
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#ifndef CLOUD_OPT_THREAD_H
#define CLOUD_OPT_THREAD_H

// for upload 
#include "dataWriter.h"
#include "dataReceiver.h"
#include "absIndex.h"
#include "enclaveIndex.h"

// for basic build block
#include "factoryDatabase.h"
#include "absDatabase.h"
#include "configure.h"
#include "clientVar.h"
#include "raUtil.h"

// for restore
#include "enclaveRecvDecoder.h"

extern Configure config;
class CloudOptThread {
    private:
        string myName_ = "CloudOptThread";
        string logFileName_ = "cloud-log";

        // handlers passed from outside
        SSLConnection* dataSecureChannel_;
        AbsDatabase* fp2ChunkDB_;

        // for RA
        RAUtil* raUtil_;

        // for upload
        DataReceiver* dataReceiverObj_;
        AbsIndex* absIndexObj_;
        DataWriter* dataWriterObj_;
        StorageCore* storageCoreObj_;

        // for restore
        EnclaveRecvDecoder* recvDecoderObj_;

        // for SGX related
        sgx_enclave_id_t eidSGX_;

        // index type
        int indexType_;

        // the number of received client requests 
        uint64_t totalUploadReqNum_ = 0;
        uint64_t totalRestoreReqNum_ = 0;

        // store the client information 
        unordered_map<int, boost::mutex*> clientLockIndex_;

        // for log file
        ofstream logFile_;

        std::mutex clientLockSetLock_;

        /**
         * @brief check the file status
         * 
         * @param fullRecipePath the full recipe path
         * @param optType the operation type
         * @return true success
         * @return false fail
         */
        bool CheckFileStatus(string& fullRecipePath, int optType);

    public:
        /**
         * @brief Construct a new Cloud Opt Thread object
         * 
         * @param dataSecureChannel data security communication channel
         * @param fp2ChunkDB the index
         * @param eidSGX sgx enclave id
         * @param indexType index type
         */
        CloudOptThread(SSLConnection* dataSecureChannel, AbsDatabase* fp2ChunkDB,
            sgx_enclave_id_t eidSGX, int indexType);

        /**
         * @brief Destroy the Cloud Opt Thread object
         * 
         */
        ~CloudOptThread();

        /**
         * @brief the main process
         * 
         * @param clientSSL the client ssl
         */
        void Run(SSL* clientSSL);
};

#endif