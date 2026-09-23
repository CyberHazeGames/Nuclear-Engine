#pragma once

#include "Core.hpp"
#include "String.hpp"

namespace Nuclear::Managed {

	class Type;
	class Attribute;

	class FieldInfo
	{
	public:
		String GetName() const;
		Type& GetType();

		TypeAccessibility GetAccessibility() const;

		std::vector<Attribute> GetAttributes() const;

	private:
		ManagedHandle m_Handle = -1;
		Type* m_Type = nullptr;

		friend class Type;
	};
	
}
