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

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <memory.h>
#include <assert.h>
#include <string.h>


typedef enum {
    LIBPAK_INIT_COMPRESSION = 0b1,
    LIBPAK_INIT_READING = 0b10
} LibpakInitFlags;

typedef struct {
    uint64_t offset;
    uint64_t size;
    uint64_t uncompressedSize;
    char path[128];
} PakHashMapEntry;

typedef struct {
    char identifier[4];
    uint64_t zstdDictSize;
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
} PakReader;

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
    \param[in] len the length of the array \p key
    \return the hash of \p key
*/
uint32_t libpakMurmurHash3_32(uint8_t* key, uint64_t len);

// compression API
/*!
    \brief begin creating a pak archive
    \param[in] path the path where the archive will be saved
    \param[in] maxFiles the maximum number of files added to this archive through libpakAddFileToArchive
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
    \param[in] zstdDictSize the size in bytes of zstd dictionary for the archive, can be 0 for no dictionary
    \param[in] zstdDictSampleCount the number of first files added to archive to use for making a dictionary,
        ignored if \p zstdDictSize is 0
    \param[in] hashMapSlotCount the number of slots in the hashmap of the archive entries,
        higher numbers can increase the speed of finding the file via libpakReadItem and libpakGetItemSize,
        but will also make the hashmap use more memory. hashmap size is always \p hashMapSlotCount * sizeof(PakHashMapEntry)
    \return 0 on success, 1 if the amount of hashmap slots was insufficient
*/
uint32_t libpakEndArchive(PakCompressor *compressor, int32_t zstdCompressionLevel, uint64_t zstdDictSize, uint64_t zstdDictSampleCount, uint64_t hashMapSlotCount);

// reading API
/*!
    \brief create a PakReader for the pak archive at path \p path
    \param[in] path the path to a pak archive
    \return a PakReader object, used as an argument for libpakReadItem, libpakGetItemSize and libpakDestroyArchiveReader
*/
PakReader libpakCreateArchiveReader(char* path);
/*!
    \brief get the uncompressed size of an item with path \p path inside of the pak archive bound to the reader
    \param[in] reader a pointer to PakReader object created via libpakCreateArchiveReader
    \param[in] path the path of the file to get the size of (same as when compressing),
        file paths inside pak archive are capped to 128 characters
    \return the size of the file at path \p path, if such file exists, 0 otherwise
*/
uint64_t libpakGetItemSize(PakReader *reader, char path[128]);
/*!
    \brief read an item with path \p path inside of the pak archive bound to the reader
    \param[in] reader a pointer to PakReader object created via libpakCreateArchiveReader
    \param[in] path the path of the file to read (same as when compressing),
        file paths inside pak archive are capped to 128 characters
    \param[out] buf a buffer to which data of the file would be stored, must be atleast the size,
        returned by libpakGetItemSize on the same path,
    \return 0 on success, 1 if no file with path \p path was found in the archive bound to reader \p reader
*/
uint32_t libpakReadItem(PakReader *reader, char path[128], void *buf);
/*!
    \brief destroy a PakReader object created with libpakCreateArchiveReader
    \param[in] reader the PakReader to free
*/
void libpakDestroyArchiveReader(PakReader *reader);

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

uint32_t murmur3_32(uint8_t* key, uint64_t len, uint32_t seed) {
	uint32_t h = seed;
    uint32_t k;

    for (uint64_t i = len >> 2; i; i--) {
        memcpy(&k, key, sizeof(uint32_t));
        key += sizeof(uint32_t);
        h ^= murmur_32_scramble(k);
        h = (h << 13) | (h >> 19);
        h = h * 5 + 0xe6546b64;
    }

    k = 0;
    for (uint64_t i = len & 3; i; i--) {
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

uint32_t libpakMurmurHash3_32(uint8_t* key, uint64_t len) {
    return murmur3_32(key, len, 0xCA4951E9);
}

ZSTD_CCtx *zstdCCtx = NULL;
ZSTD_DStream *zstdDStream = NULL;

uint64_t zstdInputStreamSize = 0;
void *zstdInputStream = NULL;
uint64_t zstdOutputStreamSize = 0;
void *zstdOutputStream = NULL;

void libpakInit(LibpakInitFlags flags) {

    if (flags & LIBPAK_INIT_COMPRESSION) zstdCCtx = ZSTD_createCCtx();
    if (flags & LIBPAK_INIT_READING) zstdDStream = ZSTD_createDStream();

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

    memset(archive.entries, 0, sizeof(PakHashMapEntry) * maxFiles);

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

uint32_t libpakEndArchive(PakCompressor *compressor, int32_t zstdCompressionLevel, uint64_t zstdDictSize, uint64_t zstdDictSampleCount, uint64_t hashMapSlotCount) {
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

    uint64_t curOffset = sizeof(PakHeader) + (sizeof(uint64_t) + sizeof(PakHashMapEntry)) * compressor->curEntryCount;

    fseek(compressor->file, curOffset, SEEK_SET);

    ZSTD_CDict *cdict = NULL;

    if (zstdDictSampleCount > 0) {
        void *zstdDict = LIBPAK_MALLOC(zstdDictSize);
        void *sampleFilesContents[zstdDictSampleCount] = {};
        FILE *files[zstdDictSampleCount] = {};
        uint64_t sampleSizes[zstdDictSampleCount] = {};
        uint64_t totalSamplesSize = 0;

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

            uint64_t size = 0;

            while (1) {
                if (in.pos == in.size) mode = ZSTD_e_end;

                ZSTD_outBuffer out = {
                    .dst = zstdOutputStream,
                    .size = zstdOutputStreamSize,
                    .pos = 0
                };

                uint64_t remaining = ZSTD_compressStream2(zstdCCtx, &out, &in, mode);
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

        uint64_t size = 0;

        FILE *file = fopen(compressor->entries[i].path, "rb");
        #ifndef LIBPAK_NO_ASSERTIONS
        LIBPAK_ASSERT(file != NULL);
        #endif

        while (mode != ZSTD_e_end) {
            uint64_t sizeRead = fread(zstdInputStream, 1, zstdInputStreamSize, file);
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

                uint64_t remaining = ZSTD_compressStream2(zstdCCtx, &out, &in, mode);
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
        if (strcmp(compressor->entries[i].path, "") == 0) continue;
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

PakReader libpakCreateArchiveReader(char* path) {
    FILE* file = fopen(path, "rb");
    #ifndef LIBPAK_NO_ASSERTIONS
    LIBPAK_ASSERT(file != NULL);
    #endif

    PakHeader header;
    fread(&header, sizeof(PakHeader), 1, file);
    #ifndef LIBPAK_NO_ASSERTIONS
    LIBPAK_ASSERT(strcmp(header.identifier, "LPAK") == 0);
    #endif

    PakReader reader = {
        .file = file,
        .header = header,
        .hashmap = LIBPAK_MALLOC(sizeof(PakHashMapEntry) * header.hashMapSlotCount),
        .zstdDDict = NULL
    };

    strncpy(reader.header.identifier, header.identifier, 4);

    for (uint64_t i = 0; i < header.hashMapSavedSlots; i++) {
        uint64_t idx;
        fread(&idx, sizeof(uint64_t), 1, file);
        fread(&reader.hashmap[idx], sizeof(PakHashMapEntry), 1, file);
    }

    if (header.zstdDictSize > 0) {
        void *ddict = LIBPAK_MALLOC(header.zstdDictSize);
        fread(ddict, header.zstdDictSize, 1, file);

        reader.zstdDDict = ZSTD_createDDict(ddict, header.zstdDictSize);
        LIBPAK_FREE(ddict);

        ZSTD_DCtx_refDDict(zstdDStream, reader.zstdDDict);
    }

    return reader;
}

uint64_t libpakGetItemSize(PakReader *reader, char path[128]) {
    int32_t hash = libpakMurmurHash3_32((uint8_t*)path, 128);
    uint32_t slot = hash % reader->header.hashMapSlotCount;

    uint32_t startingSlot = slot;

    while (strcmp(reader->hashmap[slot].path, path) != 0) {
        slot = (slot + 1) % reader->header.hashMapSlotCount;

        if (slot == startingSlot) {
            fprintf(stderr, "PakReader does not contain an entry with the path: %s\n", path);
            return 0;
        }
    }

    return reader->hashmap[slot].uncompressedSize;
}

uint32_t libpakReadItem(PakReader *reader, char path[128], void *buf) {
    uint32_t hash = libpakMurmurHash3_32((uint8_t*)path, 128);
    uint32_t slot = hash % reader->header.hashMapSlotCount;

    uint32_t startingSlot = slot;

    while (strcmp(reader->hashmap[slot].path, path) != 0) {
        slot = (slot + 1) % reader->header.hashMapSlotCount;

        if (slot == startingSlot) {
            fprintf(stderr, "PakReader does not contain an entry with the path: %s\n", path);
            return 1;
        }
    }

    fseek(reader->file, reader->hashmap[slot].offset, SEEK_SET);

    ZSTD_outBuffer out = {
        .dst = buf,
        .size = reader->hashmap[slot].uncompressedSize,
        .pos = 0
    };

    while (1) {
        uint64_t sizeRead = fread(zstdInputStream, 1, zstdInputStreamSize, reader->file);

        ZSTD_inBuffer in = {
            .src = zstdInputStream,
            .size = zstdInputStreamSize,
            .pos = 0
        };

        ZSTD_initDStream(zstdDStream);

        while (in.pos != in.size && out.pos != out.size) {
            uint64_t remaining = ZSTD_decompressStream(zstdDStream, &out, &in);
            #ifndef LIBPAK_NO_ASSERTIONS
            LIBPAK_ASSERT(!ZSTD_isError(remaining));
            #endif
        }

        if (sizeRead < zstdInputStreamSize) break;
    }

    return 0;
}

void libpakDestroyArchiveReader(PakReader *reader) {
    ZSTD_freeDDict(reader->zstdDDict);
    LIBPAK_FREE(reader->hashmap);
}

#endif

#ifdef __cplusplus
}
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