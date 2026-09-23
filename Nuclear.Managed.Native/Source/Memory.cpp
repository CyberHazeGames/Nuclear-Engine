#include "Nuclear/Managed/Memory.hpp"

namespace Nuclear::Managed {

	void* Memory::AllocHGlobal(size_t InSize)
	{
#ifdef NUCLEAR_MANAGED_WINDOWS
		return LocalAlloc(LMEM_FIXED | LMEM_ZEROINIT, InSize);
#else
		return malloc(InSize);
#endif
	}

	void Memory::FreeHGlobal(void* InPtr)
	{
#ifdef NUCLEAR_MANAGED_WINDOWS
		LocalFree(InPtr);
#else
		free(InPtr);
#endif
	}

	UCChar* Memory::StringToCoTaskMemAuto(UCStringView InString)
	{
		size_t length = InString.length() + 1;
		size_t size = length * sizeof(UCChar);

		// TODO(Emily): This (and a few other places) misuse the WC/MB split assuming Windows is the only WC platform.
		//				If we want to decide that is a general assumption we can entirely remove the `NUCLEAR_MANAGED_WIDE_CHARS`
		//				Macro and its exclusive effects.
#ifdef NUCLEAR_MANAGED_WINDOWS
		auto* buffer = static_cast<UCChar*>(CoTaskMemAlloc(size));

		if (buffer != nullptr)
		{
			memset(buffer, 0xCE, size);
			wcscpy(buffer, InString.data());
		}
#else
		UCChar* buffer;

		if ((buffer = static_cast<UCChar*>(calloc(length, sizeof(UCChar)))))
		{
			strcpy(buffer, InString.data());
		}
#endif

		return buffer;
	}

	void Memory::FreeCoTaskMem(void* InMemory)
	{
#ifdef NUCLEAR_MANAGED_WINDOWS
		CoTaskMemFree(InMemory);
#else
		FreeHGlobal(InMemory);
#endif
	}

}
