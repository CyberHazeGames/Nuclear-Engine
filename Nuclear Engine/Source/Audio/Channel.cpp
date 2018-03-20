#include <Audio\Channel.h>
#include <FMOD\includer.h> 

#ifndef FMOD_NOT_INCLUDED
#include <FMOD\inc\fmod.hpp>
#include <FMOD\inc\fmod_errors.h>
namespace NuclearEngine
{
	namespace Audio
	{
		FMOD::Channel ** Channel::GetChannel()
		{
			return &channel;
		}
	}
}
#else
namespace NuclearEngine
{
	namespace Audio
	{
		FMOD::Channel ** Channel::GetChannel()
		{
			return nullptr;
		}
	}
}
#endif