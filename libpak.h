/*  libpak.h - a simple arhive format with zstd
    see LICENSE or end of this file for the licensing

    Add the next line before including libpak.h in *one* of your source files to create the implementation:
    #define LIBPAK_IMPLEMENTATION

    possible define options:
    // disable runtime assertions in libpak
    #define LIBPAK_NO_ASSERTIONS

    // provide custom *assert* function to libpak
    #define LIBPAK_ASSERT my_assert

    // provide custom *malloc* function to libpak
    #define LIBPAK_MALLOC my_malloc

    // provide custom *free* function to libpak
    #define LIBPAK_FREE my_free


    Pak file structure:
    PakHeader
    HashMap
    zstd dictionary
    zstd-compressed data


    see declarations below for documentation
    see test/libpaktest.c for an example of the API
*/

#ifndef LIBPAK_H
#define LIBPAK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <zstd.h>
#include <zdict.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <memory.h>
#include <assert.h>
#include <string.h>


typedef enum {
    LIBPAK_INIT_COMPRESSION = 0b1,
    LIBPAK_INIT_DECOMPRESSION = 0b10
} LibpakInitFlags;

typedef struct {
    size_t offset;
    size_t size;
    size_t uncompressedSize;
    char path[128];
} PakHashMapEntry;

typedef struct {
    char identifier[4];
    size_t zstdDictSize;
    uint64_t hashMapSlotCount;
    uint64_t hashMapSavedSlots;
} PakHeader;

typedef struct {
    FILE *file;

    uint32_t curEntryCount;
    PakHashMapEntry *entries;
} PakCompressor;

typedef struct {
    FILE *file;

    PakHeader header;

    PakHashMapEntry *hashmap;

    ZSTD_DDict *zstdDDict;
} PakArchive;

typedef struct {
    size_t size;
    void *data;
} PakItem;

/*!
    \brief initialize libpak
    \param[in] flags a bitmask of LibpakInitFlags (must provide one to initialize decompression or compression)
*/
void libpakInit(LibpakInitFlags flags);
/*!
    \brief deinitialize libpak
*/
void libpakDeinit();

/*!
    \brief hashing function used internally by libpak
    \param[in] key an array of uint8_t of length \p len
    \param[in] len length of \p key array
    \return the hash of \p key
*/
uint32_t libpakMurmurHash3_32(uint8_t* key, size_t len);

// compression API
/*!
    \brief begin creating a pak archive
    \param[in] path the path where the archive will be saved
    \param[in] maxFiles maximum number of files added to this archive through libpakAddFileToArchive
    \return a PakCompressor object, used as an argument for libpakAddFileToArchive and libpakEndArchive
*/
PakCompressor libpakBeginArchive(char* path, uint32_t maxFiles);
/*!
    \brief add a file to pak archive
    \param[in] compressor a pointer to PakCompressor object created via libpakBeginArchive
    \param[in] path the path to the file to add, file paths inside pak archive are capped to 128 characters
*/
void libpakAddFileToArchive(PakCompressor *compressor, char path[128]);
/*!
    WARNING: \p compressor passed to this function is no longer a valid PakCompressor object,
        unless a non-zero error code is returned
    \brief end creating a pak archive
    \param[in] compressor a pointer to PakCompressor object created via libpakBeginArchive
    \param[in] zstdCompressionLevel zstd compression level used for the archive,
        as of writing this documentation, acceptable range is [-7; 22]
    \param[in] zstdDictSize size in bytes of zstd dictionary for the archive, can be 0 for no dictionary
    \param[in] zstdDictSampleCount the number of first files added to archive to use for making a dictionary,
        ignored if \p zstdDictSize is 0
    \param[in] hashMapSlotCount number of slots in the hashmap of the archive entries,
        higher numbers can increase the speed of finding the file via libpakReadItemFromArchive,
        but will also make the hashmap use more memory. hashmap size is always \p hashMapSlotCount * sizeof(PakHashMapEntry)
    \return an error code, error string will be printed to stderr if non-zero.
        error codes:
        0 - no error,
        1 - insufficient amount of hashmap slots
*/
uint32_t libpakEndArchive(PakCompressor *compressor, int32_t zstdCompressionLevel, size_t zstdDictSize, uint64_t zstdDictSampleCount, uint64_t hashMapSlotCount);

// reading API
/*!
    \brief load a pak archive at path \p path
    \param[in] path the path to a pak archive to load
    \return a PakArchive object, used as an argument for libpakReadItemFromArchive and libpakFreeArchive
*/
PakArchive libpakLoadArchive(char* path);
/*!
    \brief read an item in pak archive \p arc with path \p path
    \param[in] arc a pointer to PakArchive object created via libpakLoadArchive
    \return a PakItem object with the data of the requested file in the archive,
        if an entry with path \p path does not exist in the archive, (PakItem){0, NULL} is returned
*/
PakItem libpakReadItemFromArchive(PakArchive *arc, char path[128]);
/*!
    \brief free a PakItem object created with libpakReadItemFromArchive
    \param[in] item the PakItem to free
*/
void libpakFreeItem(PakItem *item);
/*!
    \brief free a PakArchive object created with libpakLoadArchive
    \param[in] arc the PakArchive to free
*/
void libpakFreeArchive(PakArchive *arc);

#ifdef LIBPAK_IMPLEMENTATION

#ifndef LIBPAK_ASSERT
#define LIBPAK_ASSERT assert
#endif

#ifndef LIBPAK_MALLOC
#define LIBPAK_MALLOC malloc
#endif

#ifndef LIBPAK_FREE
#define LIBPAK_FREE free
#endif

// taken from wikipedia: https://en.wikipedia.org/wiki/MurmurHash
static inline uint32_t murmur_32_scramble(uint32_t k) {
    k *= 0xcc9e2d51;
    k = (k << 15) | (k >> 17);
    k *= 0x1b873593;
    return k;
}

uint32_t murmur3_32(uint8_t* key, size_t len, uint32_t seed) {
	uint32_t h = seed;
    uint32_t k;

    for (size_t i = len >> 2; i; i--) {
        memcpy(&k, key, sizeof(uint32_t));
        key += sizeof(uint32_t);
        h ^= murmur_32_scramble(k);
        h = (h << 13) | (h >> 19);
        h = h * 5 + 0xe6546b64;
    }

    k = 0;
    for (size_t i = len & 3; i; i--) {
        k <<= 8;
        k |= key[i - 1];
    }

    h ^= murmur_32_scramble(k);
	h ^= len;
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;
	return h;
}

uint32_t libpakMurmurHash3_32(uint8_t* key, size_t len) {
    return murmur3_32(key, len, 0xCA4951E9);
}

ZSTD_CCtx *zstdCCtx = NULL;
ZSTD_DStream *zstdDStream = NULL;

size_t zstdInputStreamSize = 0;
void *zstdInputStream = NULL;
size_t zstdOutputStreamSize = 0;
void *zstdOutputStream = NULL;

void libpakInit(LibpakInitFlags flags) {

    if (flags & LIBPAK_INIT_COMPRESSION) zstdCCtx = ZSTD_createCCtx();
    if (flags & LIBPAK_INIT_DECOMPRESSION) zstdDStream = ZSTD_createDStream();

    zstdInputStreamSize = ZSTD_CStreamInSize();
    zstdOutputStreamSize = ZSTD_CStreamOutSize();
    zstdInputStream = LIBPAK_MALLOC(zstdInputStreamSize + zstdOutputStreamSize);
    zstdOutputStream = zstdInputStream + zstdInputStreamSize;
}

void libpakDeinit() {
    LIBPAK_FREE(zstdInputStream);

    if (zstdDStream != NULL) ZSTD_freeDStream(zstdDStream);
    if (zstdCCtx != NULL) ZSTD_freeCCtx(zstdCCtx);
}

PakCompressor libpakBeginArchive(char* path, uint32_t maxFiles) {
    #ifndef LIBPAK_NO_ASSERTIONS
    LIBPAK_ASSERT(zstdCCtx != NULL);
    #endif

    FILE* file = fopen(path, "wb");
    #ifndef LIBPAK_NO_ASSERTIONS
    LIBPAK_ASSERT(file != NULL);
    #endif
    
    PakCompressor archive = {
        .file = file,
        .curEntryCount = 0,
        .entries = LIBPAK_MALLOC(sizeof(PakHashMapEntry) * maxFiles)
    };

    return archive;
}

void libpakAddFileToArchive(PakCompressor *compressor, char path[128]) {
    PakHashMapEntry entry = {
        .offset = 0, // this is filled by libpakEndArchive
        .size = 0, // this is filled by libpakEndArchive
        .uncompressedSize = 0 // this is filled by libpakEndArchive
    };

    compressor->entries[compressor->curEntryCount] = entry;
    memset(&compressor->entries[compressor->curEntryCount].path, 0, sizeof(char) * 128);
    strncpy(compressor->entries[compressor->curEntryCount].path, path, 128);

    compressor->curEntryCount++;
}

uint32_t libpakEndArchive(PakCompressor *compressor, int32_t zstdCompressionLevel, size_t zstdDictSize, uint64_t zstdDictSampleCount, uint64_t hashMapSlotCount) {
    #ifndef LIBPAK_NO_ASSERTIONS
    LIBPAK_ASSERT(compressor->curEntryCount >= zstdDictSampleCount);
    #endif

    ZSTD_CCtx_setParameter(zstdCCtx, ZSTD_c_compressionLevel, zstdCompressionLevel);

    PakHeader header = {
        .identifier = "LPAK",
        .zstdDictSize = 0,
        .hashMapSlotCount = hashMapSlotCount,
        .hashMapSavedSlots = compressor->curEntryCount
    };

    size_t curOffset = sizeof(PakHeader) + (sizeof(uint64_t) + sizeof(PakHashMapEntry)) * compressor->curEntryCount;

    fseek(compressor->file, curOffset, SEEK_SET);

    ZSTD_CDict *cdict = NULL;

    if (zstdDictSampleCount > 0) {
        void *zstdDict = LIBPAK_MALLOC(zstdDictSize);
        void *sampleFilesContents[zstdDictSampleCount] = {};
        FILE *files[zstdDictSampleCount] = {};
        size_t sampleSizes[zstdDictSampleCount] = {};
        size_t totalSamplesSize = 0;

        for (uint64_t i = 0; i < zstdDictSampleCount; i++) {
            files[i] = fopen(compressor->entries[i].path, "rb");
            #ifndef LIBPAK_NO_ASSERTIONS
            LIBPAK_ASSERT(files[i] != NULL);
            #endif

            fseek(files[i], 0, SEEK_END);
            sampleSizes[i] = ftell(files[i]);
            fseek(files[i], 0, SEEK_SET);

            totalSamplesSize += sampleSizes[i];
        }

        for (uint64_t i = 0; i < zstdDictSampleCount; i++) {
            if (i == 0) sampleFilesContents[i] = LIBPAK_MALLOC(totalSamplesSize);
            else sampleFilesContents[i] = sampleFilesContents[i-1] + sampleSizes[i-1];

            fread(sampleFilesContents[i], sampleSizes[i], 1, files[i]);
        }

        zstdDictSize = ZDICT_trainFromBuffer(zstdDict, zstdDictSize, sampleFilesContents[0], sampleSizes, zstdDictSampleCount);
        #ifndef LIBPAK_NO_ASSERTIONS
        LIBPAK_ASSERT(!ZSTD_isError(zstdDictSize));
        #endif

        header.zstdDictSize = zstdDictSize;

        fwrite(zstdDict, zstdDictSize, 1, compressor->file);

        curOffset += zstdDictSize;

        cdict = ZSTD_createCDict(zstdDict, zstdDictSize, zstdCompressionLevel);

        LIBPAK_FREE(zstdDict);

        ZSTD_CCtx_refCDict(zstdCCtx, cdict);

        for (uint64_t i = 0; i < zstdDictSampleCount; i++) {
            compressor->entries[i].offset = curOffset;
            compressor->entries[i].uncompressedSize = sampleSizes[i];
            
            ZSTD_inBuffer in = {
                .src = sampleFilesContents[i],
                .size = sampleSizes[i],
                .pos = 0
            };

            ZSTD_EndDirective mode = ZSTD_e_continue;

            size_t size = 0;

            while (1) {
                if (in.pos == in.size) mode = ZSTD_e_end;

                ZSTD_outBuffer out = {
                    .dst = zstdOutputStream,
                    .size = zstdOutputStreamSize,
                    .pos = 0
                };

                size_t remaining = ZSTD_compressStream2(zstdCCtx, &out, &in, mode);
                #ifndef LIBPAK_NO_ASSERTIONS
                LIBPAK_ASSERT(!ZSTD_isError(remaining));
                #endif

                fwrite(zstdOutputStream, out.pos, 1, compressor->file);
                size += out.pos;

                if (mode == ZSTD_e_end && remaining == 0) break;
            }

            compressor->entries[i].size = size;
            curOffset += size;

            fclose(files[i]);
        }

        LIBPAK_FREE(sampleFilesContents[0]);
    }

    for (uint64_t i = zstdDictSampleCount; i < compressor->curEntryCount; i++) {
        compressor->entries[i].offset = curOffset;

        ZSTD_EndDirective mode = ZSTD_e_continue;

        size_t size = 0;

        FILE *file = fopen(compressor->entries[i].path, "rb");
        #ifndef LIBPAK_NO_ASSERTIONS
        LIBPAK_ASSERT(file != NULL);
        #endif

        while (mode != ZSTD_e_end) {
            size_t sizeRead = fread(zstdInputStream, 1, zstdInputStreamSize, file);
            compressor->entries[i].uncompressedSize += sizeRead;

            ZSTD_inBuffer in = {
                .src = zstdInputStream,
                .size = zstdInputStreamSize,
                .pos = 0
            };

            if (sizeRead < zstdInputStreamSize) mode = ZSTD_e_end;

            while (1) {
                ZSTD_outBuffer out = {
                    .dst = zstdOutputStream,
                    .size = zstdOutputStreamSize,
                    .pos = 0
                };

                size_t remaining = ZSTD_compressStream2(zstdCCtx, &out, &in, mode);
                #ifndef LIBPAK_NO_ASSERTIONS
                LIBPAK_ASSERT(!ZSTD_isError(remaining));
                #endif

                fwrite(zstdOutputStream, out.pos, 1, compressor->file);
                size += out.pos;

                if (mode == ZSTD_e_end ? (remaining == 0) : (in.pos == in.size)) break;
            }
        }

        compressor->entries[i].size = size;
        curOffset += size;

        fclose(file);
    }

    PakHashMapEntry *hashmap = LIBPAK_MALLOC(sizeof(PakHashMapEntry) * hashMapSlotCount);
    memset(hashmap, 0, sizeof(PakHashMapEntry) * hashMapSlotCount);

    uint64_t collisionCount = 0;
    for (uint64_t i = 0; i < compressor->curEntryCount; i++) {
        uint32_t hash = libpakMurmurHash3_32((uint8_t*)(compressor->entries[i].path), 128);
        uint32_t slot = hash % hashMapSlotCount;

        uint32_t startingSlot = slot;

        if (strcmp(hashmap[slot].path, "") != 0) collisionCount++;

        while (strcmp(hashmap[slot].path, "") != 0) {
            slot = (slot + 1) % hashMapSlotCount;

            if (slot == startingSlot) {
                fprintf(stderr, "hashMapSlotCount provided to libpakEndArchive was not sufficient, try increasing\n");

                LIBPAK_FREE(hashmap);
                ZSTD_freeCDict(cdict);

                return 1;
            }
        }

        hashmap[slot].offset = compressor->entries[i].offset;
        hashmap[slot].size = compressor->entries[i].size;
        hashmap[slot].uncompressedSize = compressor->entries[i].uncompressedSize;
        strncpy(hashmap[slot].path, compressor->entries[i].path, 128);
    }

    fseek(compressor->file, 0, SEEK_SET);

    fwrite(&header, sizeof(PakHeader), 1, compressor->file);

    for (uint64_t i = 0; i < hashMapSlotCount; i++) {
        if (strcmp(hashmap[i].path, "") != 0) {
            struct {
                uint64_t i;
                PakHashMapEntry entry;
            } data = {i, hashmap[i]};

            fwrite(&data, sizeof(uint64_t) + sizeof(PakHashMapEntry), 1, compressor->file);
        }
    }

    printf("pak file creation statistics: %zu hash collisions\n", collisionCount);


    LIBPAK_FREE(hashmap);
    ZSTD_freeCDict(cdict);

    LIBPAK_FREE(compressor->entries);
    fclose(compressor->file);

    return 0;
}

PakArchive libpakLoadArchive(char* path) {
    FILE* file = fopen(path, "rb");
    #ifndef LIBPAK_NO_ASSERTIONS
    LIBPAK_ASSERT(file != NULL);
    #endif

    PakHeader header;
    fread(&header, sizeof(PakHeader), 1, file);
    #ifndef LIBPAK_NO_ASSERTIONS
    LIBPAK_ASSERT(strcmp(header.identifier, "LPAK") == 0);
    #endif

    PakArchive arc = {
        .file = file,
        .header = header,
        .hashmap = LIBPAK_MALLOC(sizeof(PakHashMapEntry) * header.hashMapSlotCount),
        .zstdDDict = NULL
    };

    strncpy(arc.header.identifier, header.identifier, 4);

    for (uint64_t i = 0; i < header.hashMapSavedSlots; i++) {
        uint64_t idx;
        fread(&idx, sizeof(uint64_t), 1, file);
        fread(&arc.hashmap[idx], sizeof(PakHashMapEntry), 1, file);
    }

    if (header.zstdDictSize > 0) {
        void *ddict = LIBPAK_MALLOC(header.zstdDictSize);
        fread(ddict, header.zstdDictSize, 1, file);

        arc.zstdDDict = ZSTD_createDDict(ddict, header.zstdDictSize);
        LIBPAK_FREE(ddict);

        ZSTD_DCtx_refDDict(zstdDStream, arc.zstdDDict);
    }

    return arc;
}

PakItem libpakReadItemFromArchive(PakArchive *arc, char path[128]) {
    uint32_t hash = libpakMurmurHash3_32((uint8_t*)path, 128);
    uint32_t slot = hash % arc->header.hashMapSlotCount;

    uint32_t startingSlot = slot;

    while (strcmp(arc->hashmap[slot].path, path) != 0) {
        slot = (slot + 1) % arc->header.hashMapSlotCount;

        if (slot == startingSlot) {
            fprintf(stderr, "PakArchive does not contain item with the path: %s\n", path);
            return (PakItem){0, NULL};
        }
    }

    fseek(arc->file, arc->hashmap[slot].offset, SEEK_SET);

    PakItem item = {arc->hashmap[slot].uncompressedSize, LIBPAK_MALLOC(arc->hashmap[slot].uncompressedSize)};

    ZSTD_outBuffer out = {
        .dst = item.data,
        .size = item.size,
        .pos = 0
    };

    while (1) {
        size_t sizeRead = fread(zstdInputStream, 1, zstdInputStreamSize, arc->file);

        ZSTD_inBuffer in = {
            .src = zstdInputStream,
            .size = zstdInputStreamSize,
            .pos = 0
        };

        ZSTD_initDStream(zstdDStream);

        while (in.pos != in.size && out.pos != out.size) {
            size_t remaining = ZSTD_decompressStream(zstdDStream, &out, &in);
            #ifndef LIBPAK_NO_ASSERTIONS
            LIBPAK_ASSERT(!ZSTD_isError(remaining));
            #endif
        }

        if (sizeRead < zstdInputStreamSize) break;
    }

    return item;
}

void libpakFreeItem(PakItem *item) {
    LIBPAK_FREE(item->data);
}

void libpakFreeArchive(PakArchive *arc) {
    ZSTD_freeDDict(arc->zstdDDict);
    LIBPAK_FREE(arc->hashmap);
}

#ifdef __cplusplus
}
#endif

#endif
#endif


/*
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <https://unlicense.org>
*/