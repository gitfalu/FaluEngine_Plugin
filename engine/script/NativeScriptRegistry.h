#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include <vector>
#include "NativeScript.h"

namespace FaluEngine
{
	class NativeScriptRegistry
	{
	public:
		using FactoryFn = std::function<std::unique_ptr<NativeScript>()>;

		static NativeScriptRegistry& get()
		{
			static NativeScriptRegistry instance;
			return instance;
		}

		void registerScript(const std::string& name, FactoryFn factory)
		{
			m_factories[name] = std::move(factory);
		}

		std::unique_ptr<NativeScript> create(const std::string& name)
		{
			auto it = m_factories.find(name);
			return it != m_factories.end() ? it->second() : nullptr;
		}

		std::vector<std::string> getRegisteredNames() const
		{
			std::vector<std::string> names;
			names.reserve(m_factories.size());
			for (auto& [name, fn] : m_factories) names.push_back(name);
			return names;
		}

		void clear() { m_factories.clear(); }
	private:
		std::unordered_map<std::string, FactoryFn> m_factories;
	};


	#define REGISTER_NATIVE_SCRIPT(TypeName) \
		namespace { \
			struct TypeName##Registrar { \
				TypeName##Registrar() { \
					FaluEngine::NativeScriptRegistry::get().registerScript( \
						#TypeName, []() { return std::make_unique<TypeName>(); }); \
				}\
			}; \
			static TypeName##Registrar g_##TypeName##_registrar; \
		}
}
