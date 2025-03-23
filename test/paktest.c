#include "libpak.h"

int main() {
    PakCompressor pak = pakCompressorInit("test.pak", 6);
    pakCompressorAddFile(&pak, "a.txt");
    pakCompressorAddFile(&pak, "b.txt");
    pakCompressorAddFile(&pak, "c.txt");
    pakCompressorFinish(&pak);

    pakDecompress("test.pak");
}
