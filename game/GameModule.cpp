#include "FaluEngine/IPlugin.h"
#include "FaluEngine/NativeScriptRegistry.h"

class GameModule : public FaluEngine::IPlugin
{
public:
	[[nodiscard]] const char* getName() const noexcept override { return "GameCode"; }
	[[nodiscard]] const char* getVersion() const noexcept override { return "1.0"; }

	bool onLoad() override
	{
		return true;
	}

	void onUpdate(float deltaTime) override {}

	void onUnload() override
	{
		FaluEngine::NativeScriptRegistry::get().clear();
	}

	std::vector<std::pair<std::string,
		std::function<std::unique_ptr<FaluEngine::NativeScript>()>>> 
		getScriptFactories() override
	{
		return FaluEngine::NativeScriptRegistry::get().exportAll();
	}
};

extern "C" __declspec(dllexport) FaluEngine::IPlugin* createPlugin()
{
	return new GameModule();
}

extern "C" __declspec(dllexport) void destroyPlugin(FaluEngine::IPlugin* plugin)
{
	delete plugin;
}
