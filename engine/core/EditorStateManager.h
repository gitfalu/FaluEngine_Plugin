#pragma once
#include <string>
#include <functional>
#include <FaluEngine/EngineExport.h>

namespace FaluEngine
{
	class Scene;

	enum class FALU_ENGINE_API PlayState
	{
		Editing,
		Playing,
		Paused,
	};

	class FALU_ENGINE_API EditorStateManager
	{
	public:
		static EditorStateManager& get();

		void play(Scene& scene);
		void pause();
		void resume();
		void stop(Scene& scene);

		void setOnBeforeStop(std::function<void()> callback) {
			m_onBeforeShop = std::move(callback);
		}

		[[nodiscard]] PlayState getState() const noexcept { return m_state; }
		[[nodiscard]] bool isPlaying() const noexcept { return m_state == PlayState::Playing; }
		[[nodiscard]] bool isPaused() const noexcept { return m_state == PlayState::Paused; }
		[[nodiscard]] bool isEditing() const noexcept { return m_state == PlayState::Editing; }
	private:
		EditorStateManager() = default;

		PlayState m_state = PlayState::Editing;
		std::string m_snapshotJson;
		std::function<void()> m_onBeforeShop;
	};

}
