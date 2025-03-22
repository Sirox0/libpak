#ifndef LIBPAK_H
#define LIBPAK_H

#include <stdio.h>
#include <libdeflate.h>

typedef struct {
    uint8_t nameHash[20]; // sha1 takes 160 bits
    size_t compressedSize;
    size_t decompressedSize;
} ElementHeader;

typedef struct {
    FILE* file;
    struct libdeflate_compressor* compressor;
    ElementHeader* header;
    size_t headerSize;
    size_t headerCount;
    void* compressedDataPool;
    size_t compressedDataPoolSize;
} PakCompressor;

#define PAK_COMPRESSOR_MEMORY_CHUNK_SIZE 1024

// supported compression levels range is [0; 12]
PakCompressor pakCompressorInit(char* pakFileName, uint8_t compressionLevel);
void pakCompressorAddData(PakCompressor compressor, char* name, void* data, size_t size);
void pakCompressorAddFile(PakCompressor compressor, char* path);
void pakCompressorFinish(PakCompressor compressor);

#endif
