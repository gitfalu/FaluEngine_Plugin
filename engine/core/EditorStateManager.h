#pragma once
#include <string>
#include <functional>

namespace FaluEngine
{
	class Scene;

	enum class PlayState
	{
		Editing,
		Playing,
		Paused,
	};

	class EditorStateManager
	{
	public:
		static EditorStateManager& get() {
			static EditorStateManager instance;
			return instance;
		}

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
