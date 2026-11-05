#pragma once

// Nuclear.Platform has no dependency on the engine DLL or its export macros.
#if defined(_WIN32)
#if defined(NUCLEARPLATFORM_EXPORTS)
#define NPAPI __declspec(dllexport)
#else
#define NPAPI __declspec(dllimport)
#endif
#else
#define NPAPI
#endif
