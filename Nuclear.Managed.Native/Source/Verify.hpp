#pragma once

#define NUCLEAR_MANAGED_SOURCE_LOCATION const char* file = __FILE__; int line = __LINE__

#if defined(__GNUC__)
	#define NUCLEAR_MANAGED_DEBUG_BREAK __builtin_trap()
#elif defined(_MSC_VER)
	#define NUCLEAR_MANAGED_DEBUG_BREAK __debugbreak()
#else
	#define NUCLEAR_MANAGED_DEBUG_BREAK	
#endif

#define NUCLEAR_MANAGED_VERIFY(expr) do {\
						if(!(expr))\
						{\
							NUCLEAR_MANAGED_SOURCE_LOCATION;\
							std::cerr << "[Nuclear.Managed.Native]: Assert Failed! Expression: " << #expr << " at " << file << ":" << line << "\n";\
							NUCLEAR_MANAGED_DEBUG_BREAK;\
						}\
					} while(0)
