#pragma once
#include <string>


namespace FaluEngine
{
	class Scene;

	class SceneSerializr
	{
	public:
		explicit SceneSerializr(Scene& scene) : m_scene(scene) {}

		/// @brief シリアライズ
		/// @param path 
		/// @return 
		bool serialize(const std::string& path) const;

		bool deserialize(const std::string& path);

	private:
		Scene& m_scene;

	};
}
