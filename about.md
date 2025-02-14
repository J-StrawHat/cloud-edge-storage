## TODO

- [ ] 关于cloud的config
- [x] dbeCloud.cc （sgx相关代码暂时放着，避免影响整个系统的正常运行）
- [x] cloudOptThread.h
- [ ] cloudOptThread.cc

## 继承关系

AbsIndex([.h](Prototype/include/absIndex.h)):
- AbsDatabase* indexStore_ / indexStoreObj_ / fp2ChunkDB
- StorageCore* storageCoreObj_
- implement: 
    - absIndexObj_：由EnclaveIndex(fp2ChunkDB_, indexType_, eidSGX_)（[.h](Prototype/include/enclaveIndex.h), [.c](Prototype/src/Index/enclaveIndex.cc)）实现

dataReceiverObj_
- storageCoreObj_

## 调用逻辑

[dbeServer](Prototype/src/App/dbeServer.cc)
- 创建`ServerOptThread`对象，并调用`ServerOptThread::Run()`

[serverOptThread](Prototype/src/Server/serverOptThread.cc)
- `ServerOptThread()`：初始化各种相关的对象，包括`dataWriterObj_`、`storageCoreObj_`、`absIndexObj_`、`dataReceiverObj_`
- `Run()`：
    - recv Remote Attestation：调用`dataSecureChannel_->ReceiveData()`，内容写入至`recvBuf`
    - recv Session key：调用`dataSecureChannel_->ReceiveData()`
        - `messageType` 为 `SESSION_KEY_INIT`
        - 可获得`recvBuf.header->clientID`
    - check unique Client ID
    - send Client Session Reply：调用`dataSecureChannel_->SendData()`
        - messageType 为 `SESSION_KEY_REPLY`
    - receive Client Login Msg：调用`dataSecureChannel_->ReceiveData()`
    - confirm Client Request Type：根据`messageType`确定`optType`
    - conver the file name hash：将`recvBuf.dataBuffer`转换为`fileHashBuf`，然后再转换为`fileName`，并通过拼接得到`recipePath`
    - file **recipe** path & check file status：调用 `ServerOptThread::CheckFileStatus()`
        - 无状态（即文件不存在）：调用`dataSecureChannel_->SendData()`，其中`messageType`为`SERVER_FILE_NON_EXIST`
        - 有状态：
    - init Client vars and do sth according to **optType**
        - `UPLOAD_OPT`
            - 创建`outClient`对象，并调用`Ecall_Init_Client()`进行初始化
            - 创建线程，绑定`DataReceiver::Run()`（L283）
            - （若是单客户端）绑定`DataWriter::Run()`
            - response to the client：调用`dataSecureChannel_->SendData()`，其中`messageType` 为 `SERVER_LOGIN_RESPONSE`
        - `DOWNLOAD_OPT`
            ...
    - clean up Client vars
    - print the info

> TIPS:
> - `_recvChunkBuf`中的`sendBuffer`是由 网络协议头 + `dataBuffer`，而`dataBuffer`是包含了一batch的chunks
>
> - `_inputMQ` 为MessageQueue类，装了`CONTAINER_QUEUE_SIZE`数量的containers

[dataReceiver](Prototype/src/Server/dataReceiver.cc)
- `Run()`：
    - receive data：调用`dataSecureChannel_->ReceiveData()`，其中内容写入到`recvChunkBuf->sendBuffer`
    - 根据`recvChunkBuf->header->messageType`判断是否上传到最后一个文件
        - `CLIENT_UPLOAD_CHUNK`：调用`absIndexObj_->ProcessOneBatch()`，并且`batchNum_`自增
        - `CLIENT_UPLOAD_RECIPE_END`：
            - 调用`absIndexObj_->ProcessTailBatch()`
            - 持久化file recipe：调用`storageCoreObj_->FinalizeRecipe()`
            - `recipeEndNum`自增
    - （最后一个Container）调用`Ocall_WriteContainer()`，同时将`outClient->_inputMQ->done_`置为`true`

[dataWriter](Prototype/src/Server/dataWriter.cc)
- `Run()`：
    - 不断地（直至队列为空且`jobDoneFlag`为`true`）从`inputMQ`排出container，并通过`SaveToFile()`，将该container进行持久化；
- `SaveToFile()`：
    - 拼接container完整的写路径，然后将其`body`写入到该路径中。

[enclaveIndex](Prototype/src/Index/enclaveIndex.cc)
- `ProcessOneBatch()` 
    - Ecall_ProcChunkBatch(): 内置的
    - Ocall_UpdateOutIndex()
    
[storeOCall](Prototype/src/Enclave/ocallSrc/storeOCall.cc)
- `Ocall_UpdateOutIndex()`：更新外部去重的索引
    - 遍历`outQuery->outQueryBase`中的每一项，若发现`entry->dedupFlag`为`UNIQUE`，则调用`indexStoreObj_->InsertBothBuffer()`
- `Ocall_WriteContainer()`：将container buffer的containers放到message queue
    - （若是多客户端）调用`dataWriterObj_->SaveToFile()`
    - （若是单客户端）将out-enclave的Client中的`_curContainer`放入到`_inputMQ`
    - 对`_curContainer`进行重置
- `Ocall_GetReqContainers()`：
    - 调用`enclaveRecvDecoderObj_->GetReqContainers(outClientPtr)`

[inMemoryDatabase](Prototype/src/Database/inMemoryDatabase.cc)
- `InsertBothBuffer()`：在`indexObj_`中插入新的键值对

[storageCore](Prototype/src/Server/storageCore.cc)
- `FinalizeRecipe()`：调用`fileRecipeHandler.write()`(ofstream类的函数)

[enclaveRecvDecoder](Prototype/src/Server/enclaveRecvDecoder.cc)
- `GetReqContainers()`：
    - 从`outClient`中拿到`reqContainer`，并进一步获取`idBuffer`、`idNum`、`containerArray`
    - 遍历每一个container id：
        - 得到当前container的名字字符串
        - 检查当前container是否在cache：调用`containerCache->ExistsInCache()`
        - （若存在）从cache中拷贝至`containerArray[i]`
        - （不存在）得到当前container在磁盘中的路径，通过文件流`containerIn`，读入到`containerArray[i]`，同时更新cache

