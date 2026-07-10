#define LIBPAK_IMPLEMENTATION
#include <libpak.h>

int main() {
    libpakInit(LIBPAK_INIT_COMPRESSION | LIBPAK_INIT_DECOMPRESSION);

    PakCompressor compressor = libpakBeginArchive("test.pak", 3);

    libpakAddFileToArchive(&compressor, "a.txt");
    libpakAddFileToArchive(&compressor, "b.txt");
    libpakAddFileToArchive(&compressor, "c.txt");

    assert(libpakEndArchive(&compressor, 22, 0, 0, 100) == 0);

    PakArchive arc = libpakLoadArchive("test.pak");

    PakItem itemA = libpakReadItemFromArchive(&arc, "a.txt");
    PakItem itemB = libpakReadItemFromArchive(&arc, "b.txt");
    PakItem itemC = libpakReadItemFromArchive(&arc, "c.txt");

    FILE *fileA = fopen("a_decompressed.txt", "wb");
    FILE *fileB = fopen("b_decompressed.txt", "wb");
    FILE *fileC = fopen("c_decompressed.txt", "wb");

    fwrite(itemA.data, itemA.size, 1, fileA);
    fwrite(itemB.data, itemB.size, 1, fileB);
    fwrite(itemC.data, itemC.size, 1, fileC);

    fclose(fileC);
    fclose(fileB);
    fclose(fileA);

    libpakFreeItem(&itemC);
    libpakFreeItem(&itemB);
    libpakFreeItem(&itemA);

    libpakFreeArchive(&arc);

    libpakDeinit();
}