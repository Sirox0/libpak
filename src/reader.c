#include "libpak.h"
#include <libdeflate.h>
#include <sha1.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

PakReader pakReaderInit(char* pakFilePath) {
    PakReader reader = {};
    reader.file = fopen(pakFilePath, "rb");
    if (reader.file == NULL) {
        printf("failed to open pak file\n");
        exit(1);
    }

    // get the header offset
    uint64_t headerOffset;
    fread(&headerOffset, sizeof(uint64_t), 1, reader.file);
    
    fseek(reader.file, 0, SEEK_END);
    size_t headerSize = ftell(reader.file) - headerOffset;
    fseek(reader.file, headerOffset, SEEK_SET);

    reader.header = (PakElementHeader*)malloc(headerSize);
    reader.headerCount = headerSize / sizeof(PakElementHeader);
    reader.compressedDataPoolSize = PAK_MEMORY_CHUNK_SIZE;
    reader.compressedDataPool = malloc(reader.compressedDataPoolSize);
    reader.decompressor = libdeflate_alloc_decompressor();

    fread(reader.header, headerSize, 1, reader.file);
    return reader;
}

uint32_t sha1HashEqual(uint8_t* a, uint8_t* b) {
    for (size_t i = 0; i < 20; i++) {
        if (a[i] != b[i]) return 0;
    }
    return 1;
}

PakElementData pakReaderRead(PakReader* reader, char* name) {
    uint8_t nameHash[20] = {};
    {
        SHA1_CTX ctx;
        SHA1Init(&ctx);
        SHA1Update(&ctx, (unsigned char*)name, strlen(name));
        SHA1Final(nameHash, &ctx);
    }

    PakElementHeader* element = NULL;
    for (size_t i = 0; i < reader->headerCount; i++) {
        if (sha1HashEqual(nameHash, reader->header[i].nameHash)) {
            element = &reader->header[i];
            break;
        }
    }
    if (element == NULL) {
        printf("the pak file the reader is bound to does not contain specified element\n");
        exit(1);
    }

    if (reader->compressedDataPoolSize < element->compressedSize) {
        do {
            reader->compressedDataPoolSize += PAK_MEMORY_CHUNK_SIZE;
        } while (reader->compressedDataPoolSize < element->compressedSize);
        reader->compressedDataPool = realloc(reader->compressedDataPool, reader->compressedDataPoolSize);
    }

    fseek(reader->file, element->offset, SEEK_SET);
    fread(reader->compressedDataPool, element->compressedSize, 1, reader->file);
    
    PakElementData ret = {};
    ret.data = malloc(element->decompressedSize);
    ret.dataSize = element->decompressedSize;
    if (libdeflate_deflate_decompress(reader->decompressor, reader->compressedDataPool, element->compressedSize, ret.data, element->decompressedSize, NULL) != 0) {
        printf("failed to decompress an element\n");
        exit(1);
    }
    return ret;
}

void pakReaderFree(PakReader* reader) {
    libdeflate_free_decompressor(reader->decompressor);
    free(reader->compressedDataPool);
    free(reader->header);
}
