/**
 * @file ecallBaseline.cc
 * @author Zuoru YANG (zryang@cse.cuhk.edu.hk)
 * @brief implement the interface of baseline index
 * base : Prototype/src/Enclave/ecallSrc/ecallIndex/ecallInEnclave.cc
 * @version 0.1
 * @date 2023-05-05
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include "cloudIndex.h" 

// TODO enclave 有 namespace Enclave/OutEnclave，Cloud 以哪个为模板？
// src
// Prototype/src/Enclave/include/commonEnclave.h
// Prototype/src/Enclave/ecallSrc/ecallUtil/commonEnclave.cc
// modify
// Prototype/src/Cloud/commonCloud.h
// Prototype/src/Cloud/commonCloud.cc
// namespace : Enclave -> Cloud

/**
 * @brief Construct a new FingerPrint Index object
 * 
 */
CloudIndex::CloudIndex(AbsDatabase* indexStore, int indexType) : AbsIndex(indexStore) {
    #ifdef CLOUD_BASE_LINE
    if (ENABLE_SEALING) {
        if (!this->LoadDedupIndex()) {
            Cloud::Logging(myName_.c_str(), "do not need to load the previous index.\n"); 
        } 
    }
    #endif
    Cloud::Logging(myName_.c_str(), "init the CloudIndex.\n");
}

/**
 * @brief Destroy the FingerPrint Index object
 * 
 */
CloudIndex::~CloudIndex() {
    #ifdef CLOUD_BASE_LINE
    if (ENABLE_SEALING) {
        this->PersistDedupIndex();
    }
    #endif
    Cloud::Logging(myName_.c_str(), "========CloudIndex Info========\n");
    Cloud::Logging(myName_.c_str(), "logical chunk num: %lu\n", _logicalChunkNum);
    Cloud::Logging(myName_.c_str(), "logical data size: %lu\n", _logicalDataSize);
    Cloud::Logging(myName_.c_str(), "unique chunk num: %lu\n", _uniqueChunkNum);
    Cloud::Logging(myName_.c_str(), "unique data size: %lu\n", _uniqueDataSize);
    Cloud::Logging(myName_.c_str(), "compressed data size: %lu\n", _compressedDataSize);
    Cloud::Logging(myName_.c_str(), "===================================\n");
}

#ifdef CLOUD_BASE_LINE
/**
 * @brief persist the deduplication index to the disk
 * 
 * @return true success
 * @return false fail
 */
bool CloudIndex::PersistDedupIndex() {
    size_t offset = 0;
    bool persistenceStatus = false;
    uint8_t* tmpBuffer;
    size_t itemSize = 0; // Fp2Chunk num
    size_t maxBufferSize;

    // persist the index
    // init the file output stream : Prototype/src/Enclave/ocallSrc/storeOCall.cc
    Ocall_InitWriteSealedFile(&persistenceStatus, SEALED_BASELINE_INDEX_PATH);
    if (persistenceStatus == false) { // 初始化文件输出流失败
        Ocall_SGX_Exit_Error("CloudIndex: cannot init the index sealed file.");
        return false; // added 2023/05/05
    }
    
    // init buffer
    maxBufferSize = Cloud::sendChunkBatchSize_ * (CHUNK_HASH_SIZE + CONTAINER_ID_LENGTH); // CHUNK_HASH_SIZE + sizeof(RecipeEntry_t)
    tmpBuffer = (uint8_t*) malloc(maxBufferSize);
    itemSize = CloudIndexObj_.size();

    // persist the item number 先存 Fp2Chunk 的数量，方便以后读
    Cloud::WriteBufferToFile((uint8_t*)&itemSize, sizeof(itemSize), SEALED_BASELINE_INDEX_PATH);

    // start to persist the index item 再按预设buffer大小，分批写入数据
    for (auto it = CloudIndexObj_.begin(); it != CloudIndexObj_.end(); it++) {
        // fp
        memcpy(tmpBuffer + offset, it->first.c_str(), CHUNK_HASH_SIZE);
        offset += CHUNK_HASH_SIZE;
        // container address
        memcpy(tmpBuffer + offset, it->second.c_str(), CONTAINER_ID_LENGTH);
        offset += CONTAINER_ID_LENGTH;
        if (offset == maxBufferSize) {
            // the buffer is full, write to the file
            Cloud::WriteBufferToFile(tmpBuffer, offset, SEALED_BASELINE_INDEX_PATH);
            offset = 0;
        }
    }

    // 写入最后一批不足整个 buffer 的数据
    if (offset != 0) {
        // handle the tail data
        Cloud::WriteBufferToFile(tmpBuffer, offset, SEALED_BASELINE_INDEX_PATH);
        offset = 0;
    }

    // 关闭文件输出流，释放buffer
    Ocall_CloseWriteSealedFile(SEALED_BASELINE_INDEX_PATH);
    free(tmpBuffer);
    return true;
}

/**
 * @brief read the hook index from sealed data
 * 
 * @return true success
 * @return false fail
 */
bool CloudIndex::LoadDedupIndex() {
    size_t itemNum; // Fp2Chunk num
    string keyStr; // fp
    keyStr.resize(CHUNK_HASH_SIZE, 0);
    string valueStr; // container id
    valueStr.resize(CONTAINER_ID_LENGTH, 0); 
    size_t offset = 0;
    size_t maxBufferSize = 0;
    uint8_t* tmpBuffer;

    size_t sealedDataSize; // 文件大小
    Ocall_InitReadSealedFile(&sealedDataSize, SEALED_BASELINE_INDEX_PATH); 
    if (sealedDataSize == 0) { // 文件大小为0，写入失败
        return false;
    }

    // read the item number; 先读 Fp2Chunk num
    Cloud::ReadFileToBuffer((uint8_t*)&itemNum, sizeof(itemNum), SEALED_BASELINE_INDEX_PATH); 

    // init buffer
    maxBufferSize = Cloud::sendChunkBatchSize_ * (CHUNK_HASH_SIZE + CONTAINER_ID_LENGTH);
    tmpBuffer = (uint8_t*) malloc(maxBufferSize);

    size_t expectReadBatchNum = (itemNum / Cloud::sendChunkBatchSize_); // 期望处理完整批次数量
    for (size_t i = 0; i < expectReadBatchNum; i++) {
        Cloud::ReadFileToBuffer(tmpBuffer, maxBufferSize, SEALED_BASELINE_INDEX_PATH);
        for (size_t item = 0; item < Cloud::sendChunkBatchSize_; 
            item++) {
            memcpy(&keyStr[0], tmpBuffer + offset, CHUNK_HASH_SIZE);
            offset += CHUNK_HASH_SIZE;
            memcpy(&valueStr[0], tmpBuffer + offset, CONTAINER_ID_LENGTH);
            offset += CONTAINER_ID_LENGTH;

            // update the index
            CloudIndexObj_.insert({keyStr, valueStr});
        } 
        offset = 0;
    }

    // 处理不足一个完整批次的数据
    size_t remainItemNum = itemNum - CloudIndexObj_.size();
    if (remainItemNum != 0) {
        Cloud::ReadFileToBuffer(tmpBuffer, maxBufferSize, SEALED_BASELINE_INDEX_PATH);
        for (size_t i = 0; i < remainItemNum; i++) {
            memcpy(&keyStr[0], tmpBuffer + offset, CHUNK_HASH_SIZE);
            offset += CHUNK_HASH_SIZE;
            memcpy(&valueStr[0], tmpBuffer + offset, CONTAINER_ID_LENGTH);
            offset += CONTAINER_ID_LENGTH;  

            // update the index
            CloudIndexObj_.insert({keyStr, valueStr});
        }
        offset = 0;
    }

    // 关闭文件输入流，释放 buffer
    Ocall_CloseReadSealedFile(SEALED_BASELINE_INDEX_PATH);
    free(tmpBuffer);

    // check the index size consistency 
    if (CloudIndexObj_.size() != itemNum) {
        Ocall_SGX_Exit_Error("CloudIndex: load the index error.");
    }
    return true;
}
#endif

/**
 * @brief process one batch
 * 原 server 数据直接交给 enclave 处理，现 cloud 当场处理
 * 
 * @param buffer the input buffer
 * @param payloadSize the payload size
 * @param upOutSGX the pointer to enclave-related var
 */
void CloudIndex::ProcessOneBatch(SendMsgBuffer_t* recvFpBuf,
    UpOutSGX_t* upOutSGX) {
    // the in-enclave info
    EnclaveClient* sgxClient = (EnclaveClient*)upOutSGX->sgxClient; // Prototype/src/Enclave/include/ecallClient.h
    EVP_CIPHER_CTX* cipherCtx = sgxClient->_cipherCtx;
    EVP_MD_CTX* mdCtx = sgxClient->_mdCtx;
    uint8_t* recvBuffer = sgxClient->_recvBuffer; // ? 数据怎么是用 upOutSGX->sgxClient->_recvBuffer 携带的？？？ EnclaveClient 负责 free
    uint8_t* sessionKey = sgxClient->_sessionKey;
    Recipe_t* inRecipe = &sgxClient->_inRecipe;
    
    // decrypt the received data with the session key
    cryptoObj_->SessionKeyDec(cipherCtx, recvFpBuf->dataBuffer,
        recvFpBuf->header->dataSize, sessionKey, recvBuffer);

    // get the fp num 当前批次 fp num 
    uint32_t chunkNum = recvFpBuf->header->currentItemNum; // SendMsgBuffer_t : Baseline/include/chunkStructure.h

    // start to process each chunk
    string tmpChunkAddressStr;
    tmpChunkAddressStr.resize(CONTAINER_ID_LENGTH, 0);
    size_t currentOffset = 0;
    uint32_t tmpChunkSize = 0;
    string tmpHashStr;
    tmpHashStr.resize(CHUNK_HASH_SIZE, 0);

    // 读 fp -> recvBuffer
    for (size_t i = 0; i < chunkNum; i++) {
        // compute the hash over the plaintext chunk
        memcpy(&tmpChunkSize, recvBuffer + currentOffset, sizeof(uint32_t));
        currentOffset += sizeof(uint32_t);

        cryptoObj_->GenerateHash(mdCtx, recvBuffer + currentOffset, 
            tmpChunkSize, (uint8_t*)&tmpHashStr[0]);
        
        if (CloudIndexObj_.count(tmpHashStr) != 0) {
            // it is duplicate chunk
            tmpChunkAddressStr.assign(CloudIndexObj_[tmpHashStr]);
        } else {
            // it is unique chunk
            _uniqueChunkNum++;
            _uniqueDataSize += tmpChunkSize;

            // process one unique chunk
            this->ProcessUniqueChunk((RecipeEntry_t*)&tmpChunkAddressStr[0], 
                recvBuffer + currentOffset, tmpChunkSize, upOutSGX);

            // update the index 
            CloudIndexObj_[tmpHashStr] = tmpChunkAddressStr;
        }

        this->UpdateFileRecipe(tmpChunkAddressStr, inRecipe, upOutSGX);
        currentOffset += tmpChunkSize;

        // update the statistic
        _logicalDataSize += tmpChunkSize;
        _logicalChunkNum++;
    }

    return ;
}

/**
 * @brief process the tailed batch when received the end of the recipe flag
 * 
 * @param upOutSGX the pointer to enclave-related var
 */
void CloudIndex::ProcessTailBatch(UpOutSGX_t* upOutSGX) {
    // the in-enclave info
    EnclaveClient* sgxClient = (EnclaveClient*)upOutSGX->sgxClient;
    Recipe_t* inRecipe = &sgxClient->_inRecipe;
    EVP_CIPHER_CTX* cipherCtx = sgxClient->_cipherCtx;
    uint8_t* masterKey = sgxClient->_masterKey;

    if (inRecipe->recipeNum != 0) {
        // the out-enclave info
        Recipe_t* outRecipe = (Recipe_t*)upOutSGX->outRecipe;
        cryptoObj_->EncryptWithKey(cipherCtx, inRecipe->entryList,
            inRecipe->recipeNum * CONTAINER_ID_LENGTH, masterKey, 
            outRecipe->entryList);
        outRecipe->recipeNum = inRecipe->recipeNum;
        Ocall_UpdateFileRecipe(upOutSGX->outClient);
        inRecipe->recipeNum = 0;
    }

    if (sgxClient->_inContainer.curSize != 0) {
        memcpy(upOutSGX->curContainer->body, sgxClient->_inContainer.buf,
            sgxClient->_inContainer.curSize);
        upOutSGX->curContainer->currentSize = sgxClient->_inContainer.curSize;
    }

    return ;
}