/**
 * @file cloudIndex.h
 * @author Zuoru YANG (zryang@cse.cuhk.edu.hk), modified by cyh
 * @brief define the interface of baseline cloud index
 * @version 0.1
 * @date 2023-05-05
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#ifndef CLOUD_BASE_LINE
#define CLOUD_BASE_LINE
#define ENABLE_SEALING 1 // 是否使用文件存储

#include "enclaveBase.h"
// base : Prototype/src/Enclave/include/ecallInEnclave.h -> Prototype/src/Enclave/include/enclaveBase.h
#include "commonCloud.h" // namespace 
// base : #include "commonEnclave.h" 

#define SEALED_BASELINE_INDEX_PATH "cloud-baseline-seal-index" // "baseline-seal-index"

typedef struct {
    uint8_t chunkFp[CHUNK_HASH_SIZE];
    char containerID[CONTAINER_ID_LENGTH]; // 我们在 container 里记录了 offset + length
    // RecipeEntry_t address; // containerID + offset + length
} FpIdx_t; // <- BinValue_t;

class CloudIndex : public CLoudBase {
    private:
        string myName_ = "CloudIndex";
        unordered_map<string, string> CloudIndexObj_; // <chunkFp, containerID>

        // for persistence
        // ?文件读写参考/直接调用? Prototype/src/Enclave/include/storeOCall.h
        // Prototype/src/Enclave/ocallSrc/storeOCall.cc
        // extern ofstream outSealedFile_; // 输出文件流
        // extern ifstream inSealedFile_; // 输入文件流

        /**
         * @brief 查询是否存在 fp idx
         *  不存在需要后续接收 chunk 时，载入 container 对应的 ID
         *      CloudIndexObj_.insert({keyStr, valueStr});
         * @param recvFpBuf the recv Fp buffer
         * @return 上传 fp 是否存在
         */
        std::vector<bool> QueryFpIndex(SendMsgBuffer_t* recvFpBuf);

        /**
         * @brief persist the deduplication index to the disk
         *      memcpy() f.write
         * @return true success
         * @return false fail
         */
        bool PersistDedupIndex();

        /**
         * @brief read the hook index from sealed data
         *      memcpy() f.read
         * @return true success
         * @return false fail
         */
        bool LoadDedupIndex();

    public:

        /**
         * @brief Construct a new FingerPrint Index object
         * 
         */
        CloudIndex();

        /**
         * @brief Destroy the FingerPrint Index object
         * 
         */
        ~CloudIndex();

        /**
         * @brief process one batch
         * 
         * @param recvFpBuf the recv chunk buffer
         * @param upOutSGX the pointer to enclave-related var
         */
        void ProcessOneBatch(SendMsgBuffer_t* recvFpBuf, UpOutSGX_t* upOutSGX);

        /**
         * @brief process the tailed batch when received the end of the recipe flag
         * 
         * @param upOutSGX the pointer to enclave-related var
         */
        void ProcessTailBatch(UpOutSGX_t* upOutSGX);
};

#endif