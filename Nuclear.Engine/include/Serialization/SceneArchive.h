#pragma once
#include <ECS/ECS.h>

namespace Nuclear
{
	namespace Serialization
	{

		//https://github.com/skypjack/entt/wiki/Crash-Course:-entity-component-system#archives
		template<class Serializer>
		class SceneOutputArchive
		{
		public:
			SceneOutputArchive(Serializer* const ser) : Ser(ser)
			{
			}

			//Serialization
			ZPP_BITS_INLINE constexpr auto operator()(entt::entity e)
			{
				return (*Ser)(static_cast<Uint32>(e));
			}

			ZPP_BITS_INLINE constexpr auto operator()(std::underlying_type_t<entt::entity> size)
			{
				return (*Ser)(size);
			}

			template <class T> inline
				constexpr auto operator()(entt::entity e, const T& c)
			{
				(*Ser)(static_cast<Uint32>(e));
				return (*Ser)(c);
			}

			Serializer* const Ser;

		};

		template<class Serializer>
		class SceneInputArchive
		{
		public:
			SceneInputArchive(Serializer* const ser) : Ser(ser)
			{
			}

			//De-Serialization
			ZPP_BITS_INLINE constexpr auto operator()(entt::entity& e)
			{
				entt::id_type id;
				auto result = (*Ser)(id);
				e = static_cast<entt::entity>(id);
				return result;
			}
			ZPP_BITS_INLINE constexpr auto operator()(std::underlying_type_t<entt::entity>& s)
			{
				//	entt::id_type size;
				return (*Ser)(s);

				//	s = static_cast<std::underlying_type_t<entt::entity>>(size);
			}

			template <class T> inline
				constexpr auto operator()(entt::entity& e, T& c)
			{
				entt::id_type id;
				(*Ser)(id);
				e = static_cast<entt::entity>(id);
				return (*Ser)(c);
			}

			Serializer* const Ser;
		};
	}
}