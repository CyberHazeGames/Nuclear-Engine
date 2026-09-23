#pragma once

#include <string_view>
#include <string>

#include <cstdint>
#include <cwchar>

#define NUCLEAR_MANAGED_DEPRECATE_MSG_P(s, x) s ". See `" x "`"

#define NUCLEAR_MANAGED_GLOBAL_ALC_MSG "Global type cache has been superseded by Assembly/ALC-local type APIs"
#define NUCLEAR_MANAGED_GLOBAL_ALC_MSG_P(x) NUCLEAR_MANAGED_DEPRECATE_MSG_P(NUCLEAR_MANAGED_GLOBAL_ALC_MSG, #x)

#define NUCLEAR_MANAGED_LEAK_UC_TYPES_MSG "Global namespace string type abstraction will be removed"
#define NUCLEAR_MANAGED_LEAK_UC_TYPES_MSG_P(x) NUCLEAR_MANAGED_DEPRECATE_MSG_P(NUCLEAR_MANAGED_LEAK_UC_TYPES_MSG, #x)

#ifdef _WIN32
	#define NUCLEAR_MANAGED_WINDOWS
#elif defined(__APPLE__)
	#define NUCLEAR_MANAGED_APPLE
#endif

#ifdef NUCLEAR_MANAGED_WINDOWS
	#define NUCLEAR_MANAGED_CALLTYPE __cdecl
	#define NUCLEAR_MANAGED_HOSTFXR_NAME "hostfxr.dll"

	// TODO(Emily): On Windows shouldn't this use the `UNICODE` macro?
	#ifdef _WCHAR_T_DEFINED
		#define NUCLEAR_MANAGED_WIDE_CHARS
	#endif
#else
	#define NUCLEAR_MANAGED_CALLTYPE

	#ifdef NUCLEAR_MANAGED_APPLE
		#define NUCLEAR_MANAGED_HOSTFXR_NAME "libhostfxr.dylib"
	#else
		#define NUCLEAR_MANAGED_HOSTFXR_NAME "libhostfxr.so"
	#endif
#endif

#ifdef NUCLEAR_MANAGED_WIDE_CHARS
	#define NUCLEAR_MANAGED_STR(s) L##s

	using CharType [[deprecated(NUCLEAR_MANAGED_LEAK_UC_TYPES_MSG_P(Nuclear::Managed::UCChar))]] = wchar_t;
	using StringView [[deprecated(NUCLEAR_MANAGED_LEAK_UC_TYPES_MSG_P(Nuclear::Managed::UCStringView))]] = std::wstring_view;

	namespace Nuclear::Managed {
		using UCChar = wchar_t;
		using UCStringView = std::wstring_view;
		using UCString = std::wstring;
	}
#else
	#define NUCLEAR_MANAGED_STR(s) s

	using CharType [[deprecated(NUCLEAR_MANAGED_LEAK_UC_TYPES_MSG_P(Nuclear::Managed::UCChar))]] = char;
	using StringView [[deprecated(NUCLEAR_MANAGED_LEAK_UC_TYPES_MSG_P(Nuclear::Managed::UCStringView))]] = std::string_view;

	namespace Nuclear::Managed {
		using UCChar = char;
		using UCStringView = std::string_view;
		using UCString = std::string;
	}
#endif

#define NUCLEAR_MANAGED_UNMANAGED_CALLERS_ONLY ((const UCChar*) (-1ULL))

namespace Nuclear::Managed {

	using Bool32 = uint32_t;
	static_assert(sizeof(Bool32) == 4);

	enum class TypeAccessibility
	{
		Public,
		Private,
		Protected,
		Internal,
		ProtectedPublic,
		PrivateProtected
	};

	using TypeId = int32_t;
	using ManagedHandle = int32_t;

	struct InternalCall
	{
		// TODO(Emily): Review all `UCChar*` refs to see if they could be `UCStringView`.
		const UCChar* Name;
		void* NativeFunctionPtr;
	};

}
