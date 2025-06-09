#include "libpak.h"

#include <stdlib.h>

int main() {
    PakCompressor pak = pakCompressorInit("test.pak", 6, (PakAllocator){NULL, NULL, NULL});
    pakCompressorAddFile(&pak, "a.txt");
    pakCompressorAddFile(&pak, "b.txt");
    pakCompressorAddFile(&pak, "c.txt");
    pakCompressorFinish(&pak);

    pakDecompress("test.pak", (PakAllocator){NULL, NULL, NULL});

    PakReader reader = pakReaderInit("test.pak", (PakAllocator){NULL, NULL, NULL});
    PakElementData btxt = pakReaderReadData(&reader, "b.txt");
    for (size_t i = 0; i < btxt.dataSize; i++) printf("%c", ((char*)btxt.data)[i]);
    pakReaderFreeData(&reader, btxt.data);
    pakReaderFree(&reader);
}
