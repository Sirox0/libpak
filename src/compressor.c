#include <libpak.h>
#include <sha1.h>
#include <libdeflate.h>
#include <stdlib.h>
#include <string.h>

PakCompressor pakCompressorInit(char* pakFileName, uint8_t compressionLevel) {
    PakCompressor compressor = {};
    compressor.file = fopen(pakFileName, "wb");
    if (compressor.file == NULL) {
        printf("failed to create file\n");
        exit(1);
    }

    // leave the space for an offset to header (which is actually a footer)
    fseek(compressor.file, sizeof(size_t), SEEK_SET);

    compressor.compressor = libdeflate_alloc_compressor(compressionLevel);
    compressor.header = malloc(sizeof(ElementHeader) * PAK_COMPRESSOR_MEMORY_CHUNK_SIZE);
    compressor.headerSize = PAK_COMPRESSOR_MEMORY_CHUNK_SIZE;
    compressor.headerCount = 1;
    compressor.compressedDataPool = malloc(sizeof(uint8_t) * PAK_COMPRESSOR_MEMORY_CHUNK_SIZE);
    compressor.compressedDataPoolSize = PAK_COMPRESSOR_MEMORY_CHUNK_SIZE;
    return compressor;
}

void pakCompressorAddData(PakCompressor compressor, char* name, void* data, size_t size) {
    ElementHeader header = {};
    // hash the name
    {
        SHA1_CTX ctx;
        SHA1Init(&ctx);
        SHA1Update(&ctx, (unsigned char*)name, strlen(name));
        SHA1Final(header.nameHash, &ctx);
    }
    header.decompressedSize = size;

    // reallocate more memory in case current is not enough
    size_t maxCompressedSize = libdeflate_deflate_compress_bound(compressor.compressor, size);
    if (compressor.compressedDataPoolSize < maxCompressedSize) {
        do {
            compressor.compressedDataPoolSize += PAK_COMPRESSOR_MEMORY_CHUNK_SIZE;
        } while (compressor.compressedDataPoolSize < maxCompressedSize);
        compressor.compressedDataPool = realloc(compressor.compressedDataPool, compressor.compressedDataPoolSize);
    }

    header.compressedSize = libdeflate_deflate_compress(compressor.compressor, data, size, compressor.compressedDataPool, compressor.compressedDataPoolSize);
    if (header.compressedSize == 0) {
        printf("failed to compress the data\n");
        exit(1);
    }

    fwrite(compressor.compressedDataPool, header.compressedSize, 1, compressor.file);
    
    // add the header to compressor header list
    if (compressor.headerCount == compressor.headerSize) {
        compressor.headerSize += PAK_COMPRESSOR_MEMORY_CHUNK_SIZE;
        compressor.header = realloc(compressor.header, compressor.headerSize * sizeof(ElementHeader));
    }
    compressor.header[compressor.headerCount] = header;
    compressor.headerCount++;
}

void pakCompressorAddFile(PakCompressor compressor, char* path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        printf("failed to open file\n");
        exit(1);
    }

    // get file size
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);

    void* data = malloc(sizeof(uint8_t) * size);
    fread(data, size, 1, file);
    fclose(file);

    pakCompressorAddData(compressor, path, data, size);
    free(data);
}

void pakCompressorFinish(PakCompressor compressor) {
    size_t headerOffset = ftell(compressor.file);

    fwrite(compressor.header, compressor.headerCount * sizeof(ElementHeader), 1, compressor.file);
    
    fseek(compressor.file, 0, SEEK_SET);
    fwrite(&headerOffset, sizeof(size_t), 1, compressor.file);

    // free the compressor
    fclose(compressor.file);
    free(compressor.compressedDataPool);
    free(compressor.header);
    libdeflate_free_compressor(compressor.compressor);
}
