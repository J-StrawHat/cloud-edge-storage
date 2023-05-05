/**
 * @file serverOptThead.h
 * @author Zuoru YANG (zryang@cse.cuhk.edu.hk)
 * @brief server main thead 
 * @version 0.1
 * @date 2021-07-11
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#ifndef CLOUD_OPT_THREAD_H
#define CLOUD_OPT_THREAD_H

// for upload 
#include "../../include/dataWriter.h" // #include "dataWriter.h"
#include "../../include/dataReceiver.h" // #include "dataReceiver.h"
#include "../../include/absIndex.h" // #include "absIndex.h"
#include "../../include/enclaveIndex.h" // #include "enclaveIndex.h"

// for basic build block
#include "factoryDatabase.h"
#include "absDatabase.h"
#include "configure.h"
#include "clientVar.h"
#include "raUtil.h"

// for restore
#include "enclaveRecvDecoder.h"

extern Configure config;
class CloudOptThead {
    private:
        string myName_ = "CloudOptThead";
        string logFileName_ = "cloud-log";

        // handlers passed from outside
        SSLConnection* dataSecureChannel_;
        AbsDatabase* fp2ChunkDB_; // 用数据库存 fp -> chunk?

        // for RA Remote Attestation 远程信任认证
        RAUtil* raUtil_; // RA技术主要用于保护传输中的机密数据不被恶意窃取和篡改，以保证通信的安全性。

        // for upload
        DataReceiver* dataReceiverObj_; // dataReceiverObj_ 和 absIndexObj_ 都使用 storageCoreObj_
        AbsIndex* absIndexObj_; // ? new EnclaveIndex() -> CloudIndex
        DataWriter* dataWriterObj_; // ?
        StorageCore* storageCoreObj_; // update the file recipe to the disk

        // for restore 恢复
        EnclaveRecvDecoder* recvDecoderObj_; // new EnclaveRecvDecoder

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
         * @brief Construct a new Server Opt Thread object
         * 
         * @param dataSecureChannel data security communication channel
         * @param fp2ChunkDB the index
         * @param eidSGX sgx enclave id
         * @param indexType index type
         */
        CloudOptThead(SSLConnection* dataSecureChannel, AbsDatabase* fp2ChunkDB,
            sgx_enclave_id_t eidSGX, int indexType);

        /**
         * @brief Destroy the Server Opt Thread object
         * 
         */
        ~CloudOptThead();

        /**
         * @brief the main process
         * 
         * @param clientSSL the client ssl
         */
        void Run(SSL* clientSSL);
};

#endif