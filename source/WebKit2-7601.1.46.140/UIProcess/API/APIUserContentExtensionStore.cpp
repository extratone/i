/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "APIUserContentExtensionStore.h"

#if ENABLE(CONTENT_EXTENSIONS)

#include "APIUserContentExtension.h"
#include "NetworkCacheData.h"
#include "NetworkCacheDecoder.h"
#include "NetworkCacheEncoder.h"
#include "NetworkCacheFileSystem.h"
#include "SharedMemory.h"
#include "WebCompiledContentExtension.h"
#include <WebCore/ContentExtensionCompiler.h>
#include <WebCore/ContentExtensionError.h>
#include <string>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/WorkQueue.h>

using namespace WebKit::NetworkCache;

namespace API {

UserContentExtensionStore& UserContentExtensionStore::defaultStore()
{
    static UserContentExtensionStore* defaultStore = adoptRef(new UserContentExtensionStore).leakRef();
    return *defaultStore;
}

Ref<UserContentExtensionStore> UserContentExtensionStore::storeWithPath(const WTF::String& storePath)
{
    return adoptRef(*new UserContentExtensionStore(storePath));
}

UserContentExtensionStore::UserContentExtensionStore()
    : UserContentExtensionStore(defaultStorePath())
{
}

UserContentExtensionStore::UserContentExtensionStore(const WTF::String& storePath)
    : m_storePath(storePath)
    , m_compileQueue(WorkQueue::create("UserContentExtensionStore Compile Queue", WorkQueue::Type::Concurrent))
    , m_readQueue(WorkQueue::create("UserContentExtensionStore Read Queue"))
    , m_removeQueue(WorkQueue::create("UserContentExtensionStore Remove Queue"))
{
}

UserContentExtensionStore::~UserContentExtensionStore()
{
}

static String constructedPath(const String& base, const String& identifier)
{
    return WebCore::pathByAppendingComponent(base, "ContentExtension-" + WebCore::encodeForFileName(identifier));
}

const size_t ContentExtensionFileHeaderSize = sizeof(uint32_t) + 4 * sizeof(uint64_t);
struct ContentExtensionMetaData {
    uint32_t version { UserContentExtensionStore::CurrentContentExtensionFileVersion };
    uint64_t actionsSize { 0 };
    uint64_t filtersWithoutDomainsBytecodeSize { 0 };
    uint64_t filtersWithDomainBytecodeSize { 0 };
    uint64_t domainFiltersBytecodeSize { 0 };
    
    size_t fileSize() const
    {
        return ContentExtensionFileHeaderSize
            + actionsSize
            + filtersWithoutDomainsBytecodeSize
            + filtersWithDomainBytecodeSize
            + domainFiltersBytecodeSize;
    }
};

static Data encodeContentExtensionMetaData(const ContentExtensionMetaData& metaData)
{
    WebKit::NetworkCache::Encoder encoder;

    encoder << metaData.version;
    encoder << metaData.actionsSize;
    encoder << metaData.filtersWithoutDomainsBytecodeSize;
    encoder << metaData.filtersWithDomainBytecodeSize;
    encoder << metaData.domainFiltersBytecodeSize;

    ASSERT(encoder.bufferSize() == ContentExtensionFileHeaderSize);
    return Data(encoder.buffer(), encoder.bufferSize());
}

static bool decodeContentExtensionMetaData(ContentExtensionMetaData& metaData, const Data& fileData)
{
    bool success = false;
    fileData.apply([&metaData, &success, &fileData](const uint8_t* data, size_t size) {
        // The file data should be mapped into one continuous memory segment so the size
        // passed to the applier should always equal the data size.
        if (size != fileData.size())
            return false;

        WebKit::NetworkCache::Decoder decoder(data, size);
        if (!decoder.decode(metaData.version))
            return false;
        if (!decoder.decode(metaData.actionsSize))
            return false;
        if (!decoder.decode(metaData.filtersWithoutDomainsBytecodeSize))
            return false;
        if (!decoder.decode(metaData.filtersWithDomainBytecodeSize))
            return false;
        if (!decoder.decode(metaData.domainFiltersBytecodeSize))
            return false;
        success = true;
        return false;
    });
    return success;
}

static bool openAndMapContentExtension(const String& path, ContentExtensionMetaData& metaData, Data& fileData)
{
    fileData = mapFile(WebCore::fileSystemRepresentation(path).data());
    if (fileData.isNull())
        return false;

    if (!decodeContentExtensionMetaData(metaData, fileData))
        return false;

    return true;
}

static bool writeDataToFile(const Data& fileData, WebCore::PlatformFileHandle fd)
{
    bool success = true;
    fileData.apply([fd, &success](const uint8_t* data, size_t size) {
        if (WebCore::writeToFile(fd, (const char*)data, size) == -1) {
            success = false;
            return false;
        }
        return true;
    });
    
    return success;
}

static std::error_code compiledToFile(String&& json, const String& finalFilePath, ContentExtensionMetaData& metaData, Data& mappedData)
{
    using namespace WebCore::ContentExtensions;

    class CompilationClient final : public ContentExtensionCompilationClient {
    public:
        CompilationClient(WebCore::PlatformFileHandle fileHandle, ContentExtensionMetaData& metaData)
            : m_fileHandle(fileHandle)
            , m_metaData(metaData)
        {
            ASSERT(!metaData.actionsSize);
            ASSERT(!metaData.filtersWithoutDomainsBytecodeSize);
            ASSERT(!metaData.filtersWithDomainBytecodeSize);
            ASSERT(!metaData.domainFiltersBytecodeSize);
        }
        
        virtual void writeFiltersWithoutDomainsBytecode(Vector<DFABytecode>&& bytecode) override
        {
            ASSERT(!m_filtersWithDomainBytecodeWritten);
            ASSERT(!m_domainFiltersBytecodeWritten);
            m_filtersWithoutDomainsBytecodeWritten += bytecode.size();
            writeToFile(Data(bytecode.data(), bytecode.size()));
        }
        
        virtual void writeFiltersWithDomainsBytecode(Vector<DFABytecode>&& bytecode) override
        {
            ASSERT(!m_domainFiltersBytecodeWritten);
            m_filtersWithDomainBytecodeWritten += bytecode.size();
            writeToFile(Data(bytecode.data(), bytecode.size()));
        }
        
        virtual void writeDomainFiltersBytecode(Vector<DFABytecode>&& bytecode) override
        {
            m_domainFiltersBytecodeWritten += bytecode.size();
            writeToFile(Data(bytecode.data(), bytecode.size()));
        }

        virtual void writeActions(Vector<SerializedActionByte>&& actions) override
        {
            ASSERT(!m_filtersWithoutDomainsBytecodeWritten);
            ASSERT(!m_filtersWithDomainBytecodeWritten);
            ASSERT(!m_domainFiltersBytecodeWritten);
            ASSERT(!m_actionsWritten);
            m_actionsWritten += actions.size();
            writeToFile(Data(actions.data(), actions.size()));
        }
        
        virtual void finalize() override
        {
            m_metaData.actionsSize = m_actionsWritten;
            m_metaData.filtersWithoutDomainsBytecodeSize = m_filtersWithoutDomainsBytecodeWritten;
            m_metaData.filtersWithDomainBytecodeSize = m_filtersWithDomainBytecodeWritten;
            m_metaData.domainFiltersBytecodeSize = m_domainFiltersBytecodeWritten;
            
            Data header = encodeContentExtensionMetaData(m_metaData);
            if (!m_fileError && WebCore::seekFile(m_fileHandle, 0ll, WebCore::FileSeekOrigin::SeekFromBeginning) == -1) {
                WebCore::closeFile(m_fileHandle);
                m_fileError = true;
            }
            writeToFile(header);
        }
        
        bool hadErrorWhileWritingToFile() { return m_fileError; }

    private:
        void writeToFile(const Data& data)
        {
            if (!m_fileError && !writeDataToFile(data, m_fileHandle)) {
                WebCore::closeFile(m_fileHandle);
                m_fileError = true;
            }
        }
        
        WebCore::PlatformFileHandle m_fileHandle;
        ContentExtensionMetaData& m_metaData;
        size_t m_filtersWithoutDomainsBytecodeWritten { 0 };
        size_t m_filtersWithDomainBytecodeWritten { 0 };
        size_t m_domainFiltersBytecodeWritten { 0 };
        size_t m_actionsWritten { 0 };
        bool m_fileError { false };
    };

    auto temporaryFileHandle = WebCore::invalidPlatformFileHandle;
    String temporaryFilePath = WebCore::openTemporaryFile("ContentExtension", temporaryFileHandle);
    if (temporaryFileHandle == WebCore::invalidPlatformFileHandle)
        return UserContentExtensionStore::Error::CompileFailed;
    
    char invalidHeader[ContentExtensionFileHeaderSize];
    memset(invalidHeader, 0xFF, sizeof(invalidHeader));
    // This header will be rewritten in CompilationClient::finalize.
    if (WebCore::writeToFile(temporaryFileHandle, invalidHeader, sizeof(invalidHeader)) == -1)
        return UserContentExtensionStore::Error::CompileFailed;

    CompilationClient compilationClient(temporaryFileHandle, metaData);
    
    if (auto compilerError = compileRuleList(compilationClient, WTF::move(json)))
        return compilerError;
    if (compilationClient.hadErrorWhileWritingToFile())
        return UserContentExtensionStore::Error::CompileFailed;
    
    mappedData = adoptAndMapFile(temporaryFileHandle, 0, metaData.fileSize());
    if (mappedData.isNull())
        return UserContentExtensionStore::Error::CompileFailed;

    if (!WebCore::moveFile(temporaryFilePath, finalFilePath))
        return UserContentExtensionStore::Error::CompileFailed;

    return { };
}

static RefPtr<API::UserContentExtension> createExtension(const String& identifier, const ContentExtensionMetaData& metaData, const Data& fileData)
{
    auto sharedMemory = WebKit::SharedMemory::create(const_cast<uint8_t*>(fileData.data()), fileData.size(), WebKit::SharedMemory::Protection::ReadOnly);
    auto compiledContentExtensionData = WebKit::WebCompiledContentExtensionData(
        WTF::move(sharedMemory),
        fileData,
        ContentExtensionFileHeaderSize,
        metaData.actionsSize,
        ContentExtensionFileHeaderSize
            + metaData.actionsSize,
        metaData.filtersWithoutDomainsBytecodeSize,
        ContentExtensionFileHeaderSize
            + metaData.actionsSize
            + metaData.filtersWithoutDomainsBytecodeSize,
        metaData.filtersWithDomainBytecodeSize,
        ContentExtensionFileHeaderSize
            + metaData.actionsSize
            + metaData.filtersWithoutDomainsBytecodeSize
            + metaData.filtersWithDomainBytecodeSize,
        metaData.domainFiltersBytecodeSize
    );
    auto compiledContentExtension = WebKit::WebCompiledContentExtension::create(WTF::move(compiledContentExtensionData));
    return API::UserContentExtension::create(identifier, WTF::move(compiledContentExtension));
}

void UserContentExtensionStore::lookupContentExtension(const WTF::String& identifier, std::function<void(RefPtr<API::UserContentExtension>, std::error_code)> completionHandler)
{
    RefPtr<UserContentExtensionStore> self(this);
    StringCapture identifierCapture(identifier);
    StringCapture pathCapture(m_storePath);

    m_readQueue->dispatch([self, identifierCapture, pathCapture, completionHandler] {
        auto path = constructedPath(pathCapture.string(), identifierCapture.string());
        
        ContentExtensionMetaData metaData;
        Data fileData;
        if (!openAndMapContentExtension(path, metaData, fileData)) {
            RunLoop::main().dispatch([self, completionHandler] {
                completionHandler(nullptr, Error::LookupFailed);
            });
            return;
        }
        
        if (metaData.version != UserContentExtensionStore::CurrentContentExtensionFileVersion) {
            RunLoop::main().dispatch([self, completionHandler] {
                completionHandler(nullptr, Error::VersionMismatch);
            });
            return;
        }
        
        RunLoop::main().dispatch([self, identifierCapture, fileData, metaData, completionHandler] {
            RefPtr<API::UserContentExtension> userContentExtension = createExtension(identifierCapture.string(), metaData, fileData);
            completionHandler(userContentExtension, { });
        });
    });
}

void UserContentExtensionStore::compileContentExtension(const WTF::String& identifier, WTF::String&& json, std::function<void(RefPtr<API::UserContentExtension>, std::error_code)> completionHandler)
{
    RefPtr<UserContentExtensionStore> self(this);
    StringCapture identifierCapture(identifier);
    StringCapture jsonCapture(WTF::move(json));
    StringCapture pathCapture(m_storePath);

    m_compileQueue->dispatch([self, identifierCapture, jsonCapture, pathCapture, completionHandler] () mutable {
        auto path = constructedPath(pathCapture.string(), identifierCapture.string());

        ContentExtensionMetaData metaData;
        Data fileData;
        auto error = compiledToFile(jsonCapture.releaseString(), path, metaData, fileData);
        if (error) {
            RunLoop::main().dispatch([self, error, completionHandler] {
                completionHandler(nullptr, error);
            });
            return;
        }

        RunLoop::main().dispatch([self, identifierCapture, fileData, metaData, completionHandler] {
            RefPtr<API::UserContentExtension> userContentExtension = createExtension(identifierCapture.string(), metaData, fileData);
            completionHandler(userContentExtension, { });
        });
    });
}

void UserContentExtensionStore::removeContentExtension(const WTF::String& identifier, std::function<void(std::error_code)> completionHandler)
{
    RefPtr<UserContentExtensionStore> self(this);
    StringCapture identifierCapture(identifier);
    StringCapture pathCapture(m_storePath);

    m_removeQueue->dispatch([self, identifierCapture, pathCapture, completionHandler] {
        auto path = constructedPath(pathCapture.string(), identifierCapture.string());

        if (!WebCore::deleteFile(path)) {
            RunLoop::main().dispatch([self, completionHandler] {
                completionHandler(Error::RemoveFailed);
            });
            return;
        }

        RunLoop::main().dispatch([self, completionHandler] {
            completionHandler({ });
        });
    });
}

void UserContentExtensionStore::synchronousRemoveAllContentExtensions()
{
    for (const auto& path : WebCore::listDirectory(m_storePath, "*"))
        WebCore::deleteFile(path);
}

const std::error_category& userContentExtensionStoreErrorCategory()
{
    class UserContentExtensionStoreErrorCategory : public std::error_category {
        const char* name() const noexcept override
        {
            return "user content extension store";
        }

        virtual std::string message(int errorCode) const override
        {
            switch (static_cast<UserContentExtensionStore::Error>(errorCode)) {
            case UserContentExtensionStore::Error::LookupFailed:
                return "Unspecified error during lookup.";
            case UserContentExtensionStore::Error::VersionMismatch:
                return "Version of file does not match version of interpreter.";
            case UserContentExtensionStore::Error::CompileFailed:
                return "Unspecified error during compile.";
            case UserContentExtensionStore::Error::RemoveFailed:
                return "Unspecified error during remove.";
            }

            return std::string();
        }
    };

    static NeverDestroyed<UserContentExtensionStoreErrorCategory> contentExtensionErrorCategory;
    return contentExtensionErrorCategory;
}

} // namespace API

#endif // ENABLE(CONTENT_EXTENSIONS)
