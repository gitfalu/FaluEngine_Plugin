#pragma once
#include <imgui.h>
#include <filesystem>
#include <string>
#include <vector>
#include <entt/entt.hpp>

namespace FaluEngine
{
	class Scene;
}

namespace Editor {

	enum class AssetType {
		Unknown,
		Mesh, // .obj,.fbx,.gltf
		Texture, // .png,.jpg,.dds,.tga 
		NormalMap, // normal
		Shader, // .hlsl 
		Scene, // .scene
		Script, // .lua
		Folder, 
	};

	struct ContentEntry {
		std::filesystem::path path;
		std::string name;
		AssetType type = AssetType::Unknown;
		bool isDirectory = false;
	};

	class ContentBrowserPanel {
	public:
		void init(const std::filesystem::path& rootPath);
		void draw(FaluEngine::Scene* scene, entt::entity selected);

		[[nodiscard]] const std::string& getDraggedPath() const noexcept { return m_draggedPath; }
		[[nodiscard]] AssetType getDraggedAssetType() const noexcept { return m_draggedType; }
		void clearDragged() { m_draggedPath.clear(); }

	private:
		void refresh();
		void drawGridView(FaluEngine::Scene* scene, entt::entity selected);
		void drawListView(FaluEngine::Scene* scene, entt::entity selected);
		void drawEntry(const ContentEntry& entry,
			FaluEngine::Scene* scene, entt::entity selected,
			bool isGrid);

		void navigateTo(const std::filesystem::path& path);
		AssetType detectType(const std::filesystem::path& path);
		const char* getTypeIcon(AssetType type);
		ImVec4 getTypeColor(AssetType type);

	private:
		std::filesystem::path m_rootPath;
		std::filesystem::path m_currentPath;
		std::vector<ContentEntry> m_entries;

		bool m_gridView = true;
		float m_iconSize = 80.0f;

		std::string m_draggedPath;
		AssetType m_draggedType = AssetType::Unknown;

		char m_searchBuf[256] = {};
	};
}
