#include "libpak.h"
#include <libdeflate.h>
#include <stdio.h>
#include <stdlib.h>

void pakDecompress(char* pakFilePath) {
    FILE* file = fopen(pakFilePath, "rb");
    if (file == NULL) {
        printf("failed to open pak file\n");
        exit(1);
    }

    // get the header offset
    uint64_t headerOffset;
    fread(&headerOffset, sizeof(uint64_t), 1, file);
    
    fseek(file, 0, SEEK_END);
    size_t headerSize = ftell(file) - headerOffset;
    fseek(file, headerOffset, SEEK_SET);

    PakElementHeader* header = (PakElementHeader*)malloc(headerSize);
    fread(header, headerSize, 1, file);

    struct libdeflate_decompressor* decompressor = libdeflate_alloc_decompressor();
    size_t compressedDataSize = PAK_MEMORY_CHUNK_SIZE;
    size_t decompressedDataSize = PAK_MEMORY_CHUNK_SIZE;
    void* compressedData = malloc(PAK_MEMORY_CHUNK_SIZE);
    void* decompressedData = malloc(PAK_MEMORY_CHUNK_SIZE);

    for (size_t i = 0; i < headerSize / sizeof(PakElementHeader); i++) {
    fseek(file, header[i].offset, SEEK_SET);

        // make sure we have enough memory
        if (compressedDataSize < header[i].compressedSize) {
            do {
                compressedDataSize += PAK_MEMORY_CHUNK_SIZE;
            } while (compressedDataSize < header[i].compressedSize);
            compressedData = realloc(compressedData, compressedDataSize);
        }
        if (decompressedDataSize < header[i].decompressedSize) {
            do {
                decompressedDataSize += PAK_MEMORY_CHUNK_SIZE;
            } while (decompressedDataSize < header[i].decompressedSize);
            decompressedData = realloc(decompressedData, decompressedDataSize);
        }
        
        fread(compressedData, header[i].compressedSize, 1, file);
        if (libdeflate_deflate_decompress(decompressor, compressedData, header[i].compressedSize, decompressedData, header[i].decompressedSize, NULL) != 0) {
            printf("failed to decompress an element\n");
            exit(1);
        }

        char name[40] = "";
        for (uint32_t j = 0; j < 20; j++) snprintf(name + j * 2, 40 - j * 2, "%02x", header[i].nameHash[j]);
        FILE* outFile = fopen(name, "wb");
        if (outFile == NULL) {
            printf("failed to create file\n");
            exit(1);
        }

        fwrite(decompressedData, header[i].decompressedSize, 1, outFile);

        fclose(outFile);
    }

    free(decompressedData);
    free(compressedData);
    libdeflate_free_decompressor(decompressor);
}
