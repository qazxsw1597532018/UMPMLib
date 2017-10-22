#include "Superfetch.h"
#include "SuperfetchNative.h"
#include <Windows.h>

PCHAR Priorities[8] =
{
	"Idle",
	"Very Low",
	"Low",
	"Background",
	"Background",
	"Normal",
	"SuperFetch",
	"SuperFetch"
};

PCHAR ShortPfnList[TransitionPage + 1] =
{
	"Zero",
	"Free",
	"Standby",
	"Modified",
	"Mod No-Write",
	"Bad",
	"Active",
	"Transition"
};

PCHAR UseList[MMPFNUSE_KERNELSTACK + 1] =
{
	"Process Private",
	"Memory Mapped File",
	"Page File Mapped",
	"Page Table",
	"Paged Pool",
	"Non Paged Pool",
	"System PTE",
	"Session Private",
	"Metafile",
	"AWE Pages",
	"Driver Lock Pages",
	"Kernel Stack"
};

PCHAR ShortUseList[MMPFNUSE_KERNELSTACK + 1] =
{
	"Process Private",
	"Mem Mapped File",
	"PF Mapped File",
	"Page Table",
	"Paged Pool",
	"Non Paged Pool",
	"System PTE",
	"Session Private",
	"Metafile",
	"AWE ",
	"Driver Locked",
	"Kernel Stack"
};

SIZE_T MmPageCounts[TransitionPage + 1];
SIZE_T MmUseCounts[MMPFNUSE_KERNELSTACK + 1];
SIZE_T MmPageUseCounts[MMPFNUSE_KERNELSTACK + 1][TransitionPage + 1];
SIZE_T MmHighestPhysicalPage;
ULONG MmHighestPhysicalPageNumber;
PPF_PFN_PRIO_REQUEST MmPfnDatabase;
RTL_BITMAP MmVaBitmap, MmPfnBitMap;
PPF_PRIVSOURCE_QUERY_REQUEST MmPrivateSources;
LIST_ENTRY MmProcessListHead;
ULONG MmProcessCount;
LIST_ENTRY MmFileListHead;
ULONG MmFileCount;
ULONG MmPfnDatabaseSize;
HANDLE PfiFileInfoHandle;
PPF_MEMORY_RANGE_INFO MemoryRanges;

void PfiBuildSuperfetchInfo(IN PSUPERFETCH_INFORMATION SuperfetchInfo, IN PVOID Buffer, IN ULONG Length, IN SUPERFETCH_INFORMATION_CLASS InfoClass) {
	SuperfetchInfo->Version = SUPERFETCH_VERSION;
	SuperfetchInfo->Magic = SUPERFETCH_MAGIC;
	SuperfetchInfo->Data = Buffer;
	SuperfetchInfo->Length = Length;
	SuperfetchInfo->InfoClass = InfoClass;
}

NTSTATUS PfiQueryMemoryRanges() {
	NTSTATUS Status;
	SUPERFETCH_INFORMATION SuperfetchInfo;

	ULONG ResultLength = 0;

	PF_MEMORY_RANGE_INFO MemoryRangeInfo;
	MemoryRangeInfo.Version = 1;

	PfiBuildSuperfetchInfo(&SuperfetchInfo, &MemoryRangeInfo, sizeof(MemoryRangeInfo), SuperfetchMemoryRangesQuery);

	Status = NtQuerySystemInformation(SystemSuperfetchInformation, &SuperfetchInfo, sizeof(SuperfetchInfo), &ResultLength);
	if (Status == STATUS_BUFFER_TOO_SMALL) {

		MemoryRanges = static_cast<PPF_MEMORY_RANGE_INFO>(HeapAlloc(GetProcessHeap(), 0, ResultLength));
		MemoryRanges->Version = 1;

		PfiBuildSuperfetchInfo(&SuperfetchInfo, MemoryRanges, ResultLength, SuperfetchMemoryRangesQuery);

		Status = NtQuerySystemInformation(SystemSuperfetchInformation, &SuperfetchInfo, sizeof(SuperfetchInfo), &ResultLength);
		if (!NT_SUCCESS(Status))
			return Status;
	}
	else {
		MemoryRanges = &MemoryRangeInfo;
	}

	return STATUS_SUCCESS;
}

bool SFSetup()
{
	BOOLEAN old;
	auto status = RtlAdjustPrivilege(SE_PROF_SINGLE_PROCESS_PRIVILEGE, TRUE, FALSE, &old);
	status |= RtlAdjustPrivilege(SE_DEBUG_PRIVILEGE, TRUE, FALSE, &old);
	if (!NT_SUCCESS(status))
		return false;

	SYSTEM_BASIC_INFORMATION basicInfo;

	status = NtQuerySystemInformation(SystemBasicInformation,
		&basicInfo, sizeof(SYSTEM_BASIC_INFORMATION), nullptr);
	if (!NT_SUCCESS(status)) 
		return false;

	MmHighestPhysicalPageNumber = basicInfo.HighestPhysicalPageNumber;

	return true;
}

bool SFGetMemoryInfo(SFMemoryInfo* pInfo, int& rCount)
{
	if (!NT_SUCCESS(PfiQueryMemoryRanges()))
		return false;

	rCount = 0;
	PPHYSICAL_MEMORY_RUN Node;
	for (ULONG i = 0; i < MemoryRanges->RangeCount; i++) {
		Node = reinterpret_cast<PPHYSICAL_MEMORY_RUN>(&MemoryRanges->Ranges[i]);
		pInfo[i].Start = Node->BasePage << PAGE_SHIFT;
		pInfo[i].End = (Node->BasePage + Node->PageCount) << PAGE_SHIFT;
		pInfo[i].PageCount = Node->PageCount;
		pInfo[i].Size = ((Node->PageCount << PAGE_SHIFT) >> 10) * 1024; // kb to byte
		rCount++;
	}
	return true;
}