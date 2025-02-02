#pragma once
#include <NE_Common.h>

namespace Nuclear
{
	namespace Interfaces
	{
		class NEAPI NonCopyable
		{
		protected:
			NonCopyable() = default;
			~NonCopyable() = default;


			NonCopyable(const NonCopyable&) = delete;
			NonCopyable& operator = (const NonCopyable &) = delete;
		};
	}
} 
