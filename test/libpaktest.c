#define LIBPAK_IMPLEMENTATION
#include <libpak.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    libpakInit(LIBPAK_INIT_COMPRESSION | LIBPAK_INIT_DECOMPRESSION);

    PakCompressor compressor = libpakBeginArchive("test.pak", 3);

    libpakAddFileToArchive(&compressor, "a.txt");
    libpakAddFileToArchive(&compressor, "b.txt");
    libpakAddFileToArchive(&compressor, "c.txt");

    assert(libpakEndArchive(&compressor, 22, 0, 0, 100) == 0);

    PakReader reader = libpakCreateArchiveReader("test.pak");

    size_t itemASize = libpakGetItemSize(&reader, "a.txt");
    size_t itemBSize = libpakGetItemSize(&reader, "b.txt");
    size_t itemCSize = libpakGetItemSize(&reader, "c.txt");

    void *itemA = malloc(itemASize);
    void *itemB = malloc(itemBSize);
    void *itemC = malloc(itemCSize);
    libpakReadItem(&reader, "a.txt", itemA);
    libpakReadItem(&reader, "b.txt", itemB);
    libpakReadItem(&reader, "c.txt", itemC);

    FILE *fileA = fopen("a_decompressed.txt", "wb");
    FILE *fileB = fopen("b_decompressed.txt", "wb");
    FILE *fileC = fopen("c_decompressed.txt", "wb");

    fwrite(itemA, itemASize, 1, fileA);
    fwrite(itemB, itemBSize, 1, fileB);
    fwrite(itemC, itemCSize, 1, fileC);

    fclose(fileC);
    fclose(fileB);
    fclose(fileA);

    free(itemC);
    free(itemB);
    free(itemA);

    libpakDestroyArchiveReader(&reader);

    libpakDeinit();
}