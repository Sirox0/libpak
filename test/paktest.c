#include "libpak.h"

int main() {
    PakCompressor pak = pakCompressorInit("test.pak", 6);
    pakCompressorAddFile(&pak, "a.txt");
    pakCompressorAddFile(&pak, "b.txt");
    pakCompressorAddFile(&pak, "c.txt");
    pakCompressorFinish(&pak);

    pakDecompress("test.pak");

    PakReader reader = pakReaderInit("test.pak");
    PakElementData btxt = pakReaderRead(&reader, "b.txt");
    pakReaderFree(&reader);
    printf("%s", (char*)btxt.data);
    free(btxt.data);
}
