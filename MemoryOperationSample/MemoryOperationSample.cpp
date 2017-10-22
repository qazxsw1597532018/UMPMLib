// MemoryOperationSample.cpp : Defines the entry point for the console application.
//

#include "MemoryOperationSample.h"


int isAscii(int c)
{
	return((c >= 'A' && c <= 'z') || (c >= '0' && c <= '9') || c == 0x20 || c == '@' || c == '_' || c == '?');
}

int isPrintable(uint32_t uint32)
{
	if ((isAscii((uint32 >> 24) & 0xFF)) && (isAscii((uint32 >> 16) & 0xFF)) && (isAscii((uint32 >> 8) & 0xFF)) &&
		(isAscii((uint32) & 0xFF)))
		return true;
	else
		return false;
}

bool isInsideRam(uint64_t ptr, SFMemoryInfo* mi, int nRange) {
	bool isInside = false;
	for (int i = 0; i < nRange; i++)
		if (mi[i].Start <= ptr && ptr <= mi[i].End)
			return true;
	return false;
}


int main()
{
	if (!SFSetup())
		return false;

	SFMemoryInfo sfMemoryInfo[256];

	int nOfPhysicalSection = 0;

	SFGetMemoryInfo(&sfMemoryInfo[0], nOfPhysicalSection);

	auto hMemory = GetPhysicalMemoryHandle();
	char* myRam;
	uint64_t totalMemory = 0;
	auto max = 0ULL;
	for (auto i = 0; i < nOfPhysicalSection; i++) {
		totalMemory += sfMemoryInfo[i].Size;
		if (max < sfMemoryInfo[i].End)
			max = sfMemoryInfo[i].End;
	}

	printf("Mapping %lldGB of memory into userspace\n", totalMemory/1024/1024/1024);
	MapAllRam(hMemory, (void**)&myRam, max-0x1000);

	char *lpBuffer = myRam;
	static char bufPooltag[5] = { 0 };
	for(auto i = 0ULL;i< max-0x1000;i+=0x1000) {
		if (!isInsideRam(i,sfMemoryInfo, nOfPhysicalSection))
			continue;
		lpBuffer = myRam + i;
		char* lpCursor = lpBuffer;
		uint32_t previousSize = 0;
		while (1) {

			auto pPoolHeader = (PPOOL_HEADER)lpCursor;
			auto blockSize = (pPoolHeader->BlockSize << 4);
			auto previousBlockSize = (pPoolHeader->PreviousSize << 4);
			
			if (previousBlockSize != previousSize || blockSize == 0 || blockSize >= 0xFFF || !isPrintable(pPoolHeader->PoolTag & 0x7FFFFFFF))
				break;

			previousSize = blockSize;
			
			*(uint32_t*)bufPooltag = pPoolHeader->PoolTag;
			if (!strcmp(bufPooltag, "Proc")) {
				printf("Found a Proc pooltag @ %p\n", i + (lpCursor - lpBuffer));
			}
			lpCursor += blockSize;
			if (((lpCursor - lpBuffer) + sizeof(_POOL_HEADER) + sizeof(_OBJECT_HEADER)) > 0x1000)
				break;
			
		}
	}
	CloseHandle(hMemory);
    return 0;
}

