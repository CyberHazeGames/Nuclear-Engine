#pragma once

#include "Core.hpp"
#include "StableVector.hpp"

namespace Nuclear::Managed {
	class Type;

	class [[deprecated(NUCLEAR_MANAGED_GLOBAL_ALC_MSG)]] TypeCache
	{
	public:
		[[deprecated(NUCLEAR_MANAGED_GLOBAL_ALC_MSG)]]
		static TypeCache& Get();

		[[deprecated(NUCLEAR_MANAGED_GLOBAL_ALC_MSG)]]
		Type* CacheType(Type&& InType);

		[[deprecated(NUCLEAR_MANAGED_GLOBAL_ALC_MSG_P(ManagedAssembly::GetLocalType))]]
		Type* GetTypeByName(std::string_view InName) const;

		[[deprecated(NUCLEAR_MANAGED_GLOBAL_ALC_MSG)]]
		Type* GetTypeByID(TypeId InTypeID) const;

		[[deprecated(NUCLEAR_MANAGED_GLOBAL_ALC_MSG)]]
		void Clear();

	private:
		StableVector<Type> m_Types;
		std::unordered_map<std::string, Type*> m_NameCache;
		std::unordered_map<TypeId, Type*> m_IDCache;
	};

}
