#include "libpak.h"
#include <sha1.h>
#include <libdeflate.h>
#include <stdlib.h>
#include <string.h>

PakCompressor pakCompressorInit(char* pakFilePath, uint8_t compressionLevel) {
    PakCompressor compressor = {};
    compressor.file = fopen(pakFilePath, "wb");
    if (compressor.file == NULL) {
        printf("failed to create file\n");
        exit(1);
    }

    // leave the space for an offset to header (which is actually a footer)
    fseek(compressor.file, sizeof(uint64_t), SEEK_SET);

    compressor.compressor = libdeflate_alloc_compressor(compressionLevel);
    compressor.header = malloc(sizeof(PakElementHeader) * PAK_ELEMENT_CHUNK_SIZE);
    compressor.headerSize = PAK_ELEMENT_CHUNK_SIZE;
    compressor.headerCount = 0;
    compressor.compressedDataPool = malloc(PAK_MEMORY_CHUNK_SIZE);
    compressor.compressedDataPoolSize = PAK_MEMORY_CHUNK_SIZE;
    return compressor;
}

void pakCompressorAddData(PakCompressor* compressor, char* name, void* data, size_t size) {
    PakElementHeader header = {};
    // hash the name
    {
        SHA1_CTX ctx;
        SHA1Init(&ctx);
        SHA1Update(&ctx, (unsigned char*)name, strlen(name));
        SHA1Final(header.nameHash, &ctx);
    }
    header.decompressedSize = size;
    header.offset = ftell(compressor->file);

    // reallocate more memory in case current is not enough
    size_t maxCompressedSize = libdeflate_deflate_compress_bound(compressor->compressor, size);
    if (compressor->compressedDataPoolSize < maxCompressedSize) {
        do {
            compressor->compressedDataPoolSize += PAK_MEMORY_CHUNK_SIZE;
        } while (compressor->compressedDataPoolSize < maxCompressedSize);
        compressor->compressedDataPool = realloc(compressor->compressedDataPool, compressor->compressedDataPoolSize);
    }

    header.compressedSize = libdeflate_deflate_compress(compressor->compressor, data, size, compressor->compressedDataPool, compressor->compressedDataPoolSize);
    if (header.compressedSize == 0) {
        printf("failed to compress the data\n");
        exit(1);
    }

    fwrite(compressor->compressedDataPool, header.compressedSize, 1, compressor->file);
    
    // add the header to compressor header list
    if (compressor->headerCount == compressor->headerSize) {
        compressor->headerSize += PAK_ELEMENT_CHUNK_SIZE;
        compressor->header = realloc(compressor->header, compressor->headerSize * sizeof(PakElementHeader));
    }
    compressor->header[compressor->headerCount] = header;
    compressor->headerCount++;
}

void pakCompressorAddFile(PakCompressor* compressor, char* path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        printf("failed to open file\n");
        exit(1);
    }

    // get file size
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);

    void* data = malloc(size);
    fread(data, size, 1, file);
    fclose(file);

    pakCompressorAddData(compressor, path, data, size);
    free(data);
}

void pakCompressorFinish(PakCompressor* compressor) {
    uint64_t headerOffset = ftell(compressor->file);

    fwrite(compressor->header, compressor->headerCount * sizeof(PakElementHeader), 1, compressor->file);
    
    fseek(compressor->file, 0, SEEK_SET);
    fwrite(&headerOffset, sizeof(uint64_t), 1, compressor->file);

    // free the compressor
    fclose(compressor->file);
    free(compressor->compressedDataPool);
    free(compressor->header);
    libdeflate_free_compressor(compressor->compressor);
}
