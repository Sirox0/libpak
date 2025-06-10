#ifndef LIBPAK_H
#define LIBPAK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <libdeflate.h>

// generated during configuration with cmake
#include "libpakExport.h"

typedef struct {
    void* (*alloc)(size_t size);
    void* (*realloc)(void* mem, size_t nsize);
    void (*free)(void* mem);
} PakAllocator;

typedef struct {
    uint8_t nameHash[32]; // sha256 takes 256 bits
    uint64_t offset; // number of bytes before the data of this element in the pak file
    uint64_t compressedSize;
    uint64_t decompressedSize;
} PakElementHeader;

typedef struct {
    void* data;
    size_t dataSize;
} PakElementData;

typedef struct {
    PakAllocator allocator;
    FILE* file;
    struct libdeflate_compressor* compressor;
    PakElementHeader* header;
    size_t headerSize;
    size_t headerCount;
    void* compressedDataPool;
    size_t compressedDataPoolSize;
} PakCompressor;

typedef struct {
    PakAllocator allocator;
    FILE* file;
    struct libdeflate_decompressor* decompressor;
    PakElementHeader* header;
    size_t headerCount;
    void* compressedDataPool;
    size_t compressedDataPoolSize;
} PakReader;

#define PAK_MEMORY_CHUNK_SIZE 8192
#define PAK_ELEMENT_CHUNK_SIZE 128

// supported compression levels range is [0; 12]
// allocator can be NULL
PAK_EXPORT PakCompressor pakCompressorInit(char* pakFilePath, uint8_t compressionLevel, PakAllocator allocator);
PAK_EXPORT void pakCompressorAddData(PakCompressor* compressor, char* name, void* data, size_t size);
PAK_EXPORT void pakCompressorAddFile(PakCompressor* compressor, char* path);
PAK_EXPORT void pakCompressorFinish(PakCompressor* compressor);

PAK_EXPORT void pakDecompress(char* pakFilePath, PakAllocator allocator);

PAK_EXPORT PakReader pakReaderInit(char* pakFilePath, PakAllocator allocator);
PAK_EXPORT PakElementData pakReaderReadData(PakReader* reader, char* name);
PAK_EXPORT void pakReaderFreeData(PakReader* reader, PakElementData* data);
PAK_EXPORT void pakReaderFree(PakReader* reader);

#ifdef __cplusplus
}
#endif

#endif
