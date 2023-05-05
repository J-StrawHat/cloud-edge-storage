/**
 * @file plainDaE.cc
 * @author Zuoru YANG (zryang@cse.cuhk.edu.hk)
 * @brief implement the interface of plainDaE
 * @version 0.1
 * @date 2021-05-15
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include "../../include/plainDaE.h"

/**
 * @brief Construct a new Plain D A E object
 * 
 */
PlainDAE::PlainDAE(uint8_t* fileNameHash) : AbsDAE(fileNameHash) {
    tool::Logging(myName_.c_str(), "init the Plain-DAE.\n");
}

/**
 * @brief Destroy the Plain D A E object
 * 
 */
PlainDAE::~PlainDAE() {
}

/**
 * @brief process one segment of chunk
 * 
 * @param sendPlainChunkBuf the buffer to the plaintext chunk
 * @param sendCipherChunkBuf the buffer to the ciphertext chunk
 * @param sendRecipeBuf the buffer for the recipe <only store the key>
 */
void PlainDAE::ProcessBatchChunk(SendMsgBuffer_t* sendPlainChunkBuf,
    SendMsgBuffer_t* sendCipherChunkBuf, SendMsgBuffer_t* sendRecipeBuf) {
    uint32_t chunkNum = sendPlainChunkBuf->header->currentItemNum;
    size_t offset = 0;

    uint32_t chunkSize;
    uint8_t* chunkData;
    KeyRecipeEntry_t* tmpRecipe = (KeyRecipeEntry_t*) (sendRecipeBuf->dataBuffer + 
        sendRecipeBuf->header->dataSize);
    for (uint32_t i = 0; i < chunkNum; i++) {
        memcpy(&chunkSize, sendPlainChunkBuf->dataBuffer + offset, 
            sizeof(uint32_t));
        offset += sizeof(uint32_t);
        chunkData = sendPlainChunkBuf->dataBuffer + offset;
        offset += chunkSize;

        memset(tmpRecipe->key, 1, CHUNK_HASH_SIZE); // suppose the key is all '1'
        keyRecipeFile_.write((char*)tmpRecipe->key, CHUNK_HASH_SIZE);
        tmpRecipe++;

        // copy teh chunk to the cipher chunk buffer
        memcpy(sendCipherChunkBuf->dataBuffer + sendCipherChunkBuf->header->dataSize, 
            &chunkSize, sizeof(uint32_t));
        sendCipherChunkBuf->header->dataSize += sizeof(uint32_t);
        memcpy(sendCipherChunkBuf->dataBuffer + sendCipherChunkBuf->header->dataSize, 
            chunkData, chunkSize);
        sendCipherChunkBuf->header->dataSize += chunkSize;
        sendCipherChunkBuf->header->currentItemNum++;
    }

    return ;
}

/**
 * @brief close the connection with key manager
 * 
 */
void PlainDAE::CloseKMConnection() {
    return ;
}