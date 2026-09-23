#include "Nuclear/Managed/Attribute.hpp"
#include "Nuclear/Managed/Type.hpp"
#include "Nuclear/Managed/TypeCache.hpp"
#include "Nuclear/Managed/String.hpp"

#include "ManagedFunctions.hpp"

namespace Nuclear::Managed {

	Type& Attribute::GetType()
	{
		if (!m_Type)
		{
			Type type;
			s_ManagedFunctions.GetAttributeTypeFptr(m_Handle, &type.m_Id);
			m_Type = TypeCache::Get().CacheType(std::move(type));
		}

		return *m_Type;
	}

	template<>
	std::string Attribute::GetFieldValue(std::string_view InFieldName)
	{
		String result;
		GetFieldValueInternal(InFieldName, &result);
		return std::string(result);
	}

	template<>
	bool Attribute::GetFieldValue(std::string_view InFieldName)
	{
		Bool32 result;
		GetFieldValueInternal(InFieldName, &result);
		return result;
	}

	void Attribute::GetFieldValueInternal(std::string_view InFieldName, void* OutValue) const
	{
		auto fieldName = String::New(InFieldName);
		s_ManagedFunctions.GetAttributeFieldValueFptr(m_Handle, fieldName, OutValue);
		String::Free(fieldName);
	}

}
