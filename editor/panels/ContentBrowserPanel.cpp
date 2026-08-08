#include "ContentBrowserPanel.h"
#include "asset/loaders/AnimationCache.h"
#include "scene/Scene.h"
#include "scene/Entity.h"
#include "scene/Component.h"
#include "scene/SceneManager.h"
#include "core/PathResolver.h"
#include <imgui.h>
#include <algorithm>
#include <cctype>

namespace Editor{
	void ContentBrowserPanel::init(const std::filesystem::path& rootPath)
	{
		m_rootPath = rootPath;
		m_currentPath = rootPath;
		refresh();
	}

	bool ContentBrowserPanel::draw(FaluEngine::Scene* scene, entt::entity selected)
	{
		bool sceneChanged = false;
		ImGui::Begin("Content Browser");

		{
			std::vector<std::filesystem::path> crumbs;
			std::filesystem::path p = m_currentPath;
			while (true) {
				crumbs.push_back(p);
				if (p == m_rootPath) break;
				auto parent = p.parent_path();
				if (parent == p) break;
				p = parent;
			}
			std::reverse(crumbs.begin(), crumbs.end());

			for (size_t i = 0; i < crumbs.size(); ++i) {
				std::string label = (crumbs[i] == m_rootPath)
					? "assets"
					: crumbs[i].filename().string();

				if (crumbs[i] == m_currentPath) {
					ImGui::TextColored({ 1.0f,1.0f,1.0f,1.0f }, "%s", label.c_str());
				}
				else {
					ImGui::PushStyleColor(ImGuiCol_Button, { 0.0f,0.0f,0.0f,0.0f });
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.3f,0.3f,0.3f,1.0f });
					ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 0.4f,0.4f,0.4f,1.0f });
					ImGui::PushStyleColor(ImGuiCol_Text, { 0.6f,0.8f,1.0f,1.0f });

					if (ImGui::SmallButton(label.c_str()))
						navigateTo(crumbs[i]);

					ImGui::PopStyleColor(4);
				}

				if (i < crumbs.size() - 1) {
					ImGui::SameLine(0, 2);
					ImGui::TextDisabled(">");
					ImGui::SameLine(0, 2);
				}
			}
		}

		ImGui::SameLine();

		if (ImGui::SmallButton(m_gridView ? "[Grid]" : "[List]"))
			m_gridView = !m_gridView;

		ImGui::SameLine();

		if (ImGui::SmallButton("[Refresh]")) refresh();


		if (m_gridView) {
			ImGui::SameLine();
			ImGui::SetNextItemWidth(80.0f);
			ImGui::SliderFloat("##IconSize", &m_iconSize, 48.0f, 128.0f, "%.0f");
		}

		ImGui::SameLine();

		ImGui::SetNextItemWidth(160.0f);
		ImGui::InputText("##Search", m_searchBuf, sizeof(m_searchBuf));
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Search files");

		ImGui::Separator();

		//==== 左右ペイン =======
		//-左ペイン：フォルダツリー
		ImGui::BeginChild("##FolderTree", { m_leftPaneWidth,0 }, true);
		drawFolderTree(m_rootPath);
		ImGui::EndChild();

		// リサイザー
		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Button, { 0.3f,0.3f,0.3f,1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.5f,0.5f,0.5f,1.0f });
		ImGui::Button("##Resizer", { 4.0f,-1.0f });
		ImGui::PopStyleColor(2);
		if (ImGui::IsItemActive())
		{
			m_leftPaneWidth += ImGui::GetIO().MouseDelta.x;
			m_leftPaneWidth = std::clamp(m_leftPaneWidth,80.0f,400.0f);
		}

		//-右ペイン：ファイル一覧
		ImGui::SameLine();
		ImGui::BeginChild("FilePanel", { 0,0 }, false);
		sceneChanged = drawFilePanel(scene,selected);
		ImGui::EndChild();

		ImGui::End();

		if (m_hasPendingNavigate)
		{
			m_currentPath = m_pendingNavigate;
			refresh();
			m_hasPendingNavigate = false;
		}

		return sceneChanged;
	}

	void ContentBrowserPanel::refresh()
	{
		m_entries.clear();
		if (!std::filesystem::exists(m_currentPath)) return;

		//-フォルダを先に、ファイルを後に並べる
		std::vector<ContentEntry> dirs, files;
		std::error_code ec;

		auto it = std::filesystem::directory_iterator(m_currentPath,ec);
		if (ec) {
			LOG_ERROR("ContentBrowser: failed to open directory '{}': {}",
				m_currentPath.string(), ec.message());
			return;
		}

		const std::filesystem::directory_iterator end;

		while(it != end)
		{		
			std::error_code entryEc;
			bool isDir = it->is_directory(entryEc);
			if (entryEc)
			{
				LOG_WARN("ContentBrowser: skipping inaccessible entry '{}': {}",
					it->path().string(), entryEc.message());
			}
			else
			{
				ContentEntry ce;
				ce.path = it->path();
				ce.name = it->path().filename().string();
				ce.isDirectory = isDir;
				ce.type = detectType(it->path());
				(ce.isDirectory ? dirs : files).push_back(ce);
			}

			it.increment(ec);
			if (ec)
			{
				LOG_WARN("ContentBrowser: stopped enumerationg '{}': '{}'",
					m_currentPath.string(), ec.message());
				break;
			}
		}

		//-アルファベット順にソート
		auto sortFn = [](const ContentEntry& a, const ContentEntry& b) {
			return a.name < b.name;
			};
		std::sort(dirs.begin(), dirs.end(), sortFn);
		std::sort(files.begin(), files.end(), sortFn);

		for (auto& d : dirs)m_entries.push_back(d);
		for (auto& f : files) m_entries.push_back(f);

	}

	void ContentBrowserPanel::drawFolderTree(const std::filesystem::path& path)
	{
		if (!std::filesystem::exists(path)) return;

		std::string name = (path == m_rootPath)
			? "assets" : path.filename().string();

		bool hasSubDirs = false;
		const std::filesystem::directory_iterator end;
		std::error_code ec;
		auto it = std::filesystem::directory_iterator(path, ec);
		while (!ec && it != end)
		{
			std::error_code entryEc;
			if (it->is_directory(entryEc) && !entryEc) { hasSubDirs = true; break; }
			it.increment(ec);
		}
		ImGuiTreeNodeFlags flags =
			ImGuiTreeNodeFlags_OpenOnArrow |
			ImGuiTreeNodeFlags_SpanAvailWidth;

		if (!hasSubDirs) flags |= ImGuiTreeNodeFlags_Leaf;
		if (path == m_currentPath)
			flags |= ImGuiTreeNodeFlags_Selected;
		if (path == m_rootPath)
			flags |= ImGuiTreeNodeFlags_DefaultOpen;
		
		ImGui::PushStyleColor(ImGuiCol_Text, { 1.0f,0.8f,0.2f,1.0f });
		bool opened = ImGui::TreeNodeEx(
			path.string().c_str(), flags, "%s", name.c_str());
		ImGui::PopStyleColor();

		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
			navigateTo(path);

		if (opened)
		{
			std::vector<std::filesystem::path> subDirs;
			std::error_code ec;
			auto it = std::filesystem::directory_iterator(path, ec);

			while(!ec && it != end)
			{
				std::error_code entryEc;
				
				if (it->is_directory(entryEc) && !entryEc) {
					subDirs.push_back(it->path());
				}
				it.increment(ec);
			}
			std::sort(subDirs.begin(), subDirs.end());
			for (auto& sub : subDirs)
				drawFolderTree(sub);

			ImGui::TreePop();
		}
	}

	bool ContentBrowserPanel::drawFilePanel(FaluEngine::Scene* scene, entt::entity selected)
	{
		return m_gridView ?
			drawGridView(scene, selected) : 
			drawListView(scene, selected);
	}

	bool ContentBrowserPanel::drawGridView(FaluEngine::Scene* scene, entt::entity selected)
	{
		bool changed = false;
		float panelWidth = ImGui::GetContentRegionAvail().x;
		int columns = (std::max)(1,static_cast<int>(panelWidth / (m_iconSize + 16.0f)));

		if (ImGui::BeginTable("##Grid", columns))
		{
			for (auto& entry : m_entries)
			{
				ImGui::TableNextColumn();
				if (drawEntry(entry, scene, selected, true))
					changed = true;
			}
			ImGui::EndTable();
		}
		return changed;
	}

	bool ContentBrowserPanel::drawListView(FaluEngine::Scene* scene, entt::entity selected)
	{
		bool changed = false;
		for (auto& entry : m_entries)
		{
			if (drawEntry(entry, scene, selected, false))
				changed = true;
		}
		return changed;
	}

	bool ContentBrowserPanel::drawEntry(const ContentEntry& entry, FaluEngine::Scene* scene, entt::entity selected, bool isGrid)
	{
		bool sceneChanged = false;

		const std::string pathStr = entry.path.string();
		const char* icon = getTypeIcon(entry.type);
		ImVec4 color = getTypeColor(entry.type);

		if (m_searchBuf[0] != '\0') {
			std::string name = entry.name;
			std::string filter = m_searchBuf;
			std::transform(name.begin(), name.end(), name.begin(), ::tolower);
			std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);
			if (name.find(filter) == std::string::npos) return false;
		}

		ImGui::PushID(pathStr.c_str());

		if (isGrid)
		{
			ImGui::BeginGroup();
			ImGui::PushStyleColor(ImGuiCol_Button, { 0.2f,0.2f,0.2f,1.0f });
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.35f,0.35f,0.35f,1.0f });
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 0.45f,0.45f,0.45f,1.0f });

			ImGui::PushStyleColor(ImGuiCol_Text, color);
			ImGui::Button(icon, { m_iconSize,m_iconSize });
			ImGui::PopStyleColor(4);

			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
			{
				if (entry.isDirectory)
					navigateTo(entry.path);
				else if (entry.type == AssetType::Scene)
				{
					FaluEngine::SceneManager::get().loadSceneFromFile(entry.path.string());
					sceneChanged = true;
				}
				else
					applyToEntity(entry, scene, selected);
			}

			//-ドラック操作
			if (!entry.isDirectory && ImGui::BeginDragDropSource()) {
				ImGui::SetDragDropPayload("ASSET_PATH", pathStr.c_str(),
					pathStr.size() + 1);
				m_draggedPath = pathStr;
				m_draggedType = entry.type;
				ImGui::TextColored(color, "%s %s", icon, entry.name.c_str());
				ImGui::EndDragDropSource();
			}

			std::string displayName = entry.name;
			if (displayName.size() > 10)
				displayName = displayName.substr(0, 9) + "..";
			ImGui::TextColored(color, "%s", displayName.c_str());
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", entry.name.c_str());

			ImGui::EndGroup();
		}
		else
		{
			//=== リスト表示 ======
			ImGui::TextColored(color, "%s", icon);
			ImGui::SameLine();

			bool selected_item = false;
			if (ImGui::Selectable(entry.name.c_str(), &selected_item,
				ImGuiSelectableFlags_AllowDoubleClick)) {
				if (ImGui::IsMouseDoubleClicked(0)) {
					if (entry.isDirectory)
						navigateTo(entry.path);
					else if (entry.type == AssetType::Scene)
					{
						FaluEngine::SceneManager::get().loadSceneFromFile(entry.path.string());
						sceneChanged = true;
					}
					else
					{
						applyToEntity(entry, scene, selected);
					}
				}
			}

			if (!entry.isDirectory && ImGui::BeginDragDropSource()) {
				ImGui::SetDragDropPayload("ASSET_PATH", pathStr.c_str(),
					pathStr.size() + 1);
				m_draggedPath = pathStr;
				m_draggedType = entry.type;
				ImGui::TextColored(color, "%s %s", icon, entry.name.c_str());
				ImGui::EndDragDropSource();
			}

			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", pathStr.c_str());
		}
		ImGui::PopID();
		return sceneChanged;
	}

	void ContentBrowserPanel::navigateTo(const std::filesystem::path& path)
	{
		if (std::filesystem::exists(path) && std::filesystem::is_directory(path))
		{
			m_pendingNavigate = path;
			m_hasPendingNavigate = true;
		}
	}

	AssetType ContentBrowserPanel::detectType(const std::filesystem::path& path)
	{
		if (std::filesystem::is_directory(path))return AssetType::Folder;

		std::string ext = path.extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

		if (ext == ".obj" || ext == ".fbx" || ext == ".gltf" || ext == ".glb")
			return AssetType::Mesh;

		if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
			ext == ".dds" || ext == ".tga" || ext == ".bmp") {
			// ファイル名に"normal" "norm" "nrm" が含まれれば NormalMap
			std::string stem = path.stem().string();
			std::transform(stem.begin(), stem.end(), stem.begin(), ::tolower);
			if (stem.find("normal") != std::string::npos ||
				stem.find("_norm") != std::string::npos ||
				stem.find("_nrm") != std::string::npos)
				return AssetType::NormalMap;
			return AssetType::Texture;
		}

		if (ext == ".hlsl") return AssetType::Shader;
		if (ext == ".scene")return AssetType::Scene;
		if (ext == ".lua")return AssetType::Script;
		if (ext == ".mat") return AssetType::Material;

		return AssetType::Unknown;
	}

	const char* ContentBrowserPanel::getTypeIcon(AssetType type)
	{
		switch (type)
		{
		case Editor::AssetType::Unknown:	return "[DIR]";
		case Editor::AssetType::Mesh:		return "[OBJ]";
		case Editor::AssetType::Texture:	return "[TEX]";
		case Editor::AssetType::NormalMap:	return "[NRM]";
		case Editor::AssetType::Shader:		return "[SHD]";
		case Editor::AssetType::Scene:		return "[SCN]";
		case Editor::AssetType::Script:		return "[LUA]";
		default: return "[   ]";
		}
	}

	ImVec4 ContentBrowserPanel::getTypeColor(AssetType type)
	{
		switch (type)
		{
		case Editor::AssetType::Unknown: return { 1.0f,0.8f,0.2f,1.0f };
		case Editor::AssetType::Mesh: return { 0.4f,0.8f,1.0f,1.0f };
		case Editor::AssetType::Texture: return { 0.6f,1.0f,0.4f,1.0f };
		case Editor::AssetType::NormalMap: return { 0.8f,0.6f,1.0f,1.0f };
		case Editor::AssetType::Shader: return { 1.0f,0.5f,0.2f,1.0f };
		case Editor::AssetType::Scene: return { 1.0f,1.0f,0.4f,1.0f };
		case Editor::AssetType::Script: return { 0.4f,1.0f,0.8f,1.0f };
		default: return { 0.7f,0.7f,0.7f,1.0f };
		}

	}
	void ContentBrowserPanel::applyToEntity(const ContentEntry& entry, FaluEngine::Scene* scene, entt::entity selected)
	{
		if (!scene || selected == entt::null) return;
		if (!scene->registry().all_of<FaluEngine::MeshComponent>(selected)) return;

		auto& m = scene->registry().get<FaluEngine::MeshComponent>(selected);

		const std::string pathStr = entry.path.string();
		switch (entry.type)
		{
		case AssetType::Mesh:
		{
			m.meshPath = pathStr;
			m.cachedMesh = nullptr;

			// Animation付きのモデルの場合Animatorを自動追加
			auto& clips = FaluEngine::AnimationCache::get().getAnimations(pathStr);
			if (!clips.empty())
			{
				FaluEngine::Entity e(selected, scene);
				if (!e.hasComponent<FaluEngine::AnimatorComponent>())
				{
					auto& animator = e.addComponent<FaluEngine::AnimatorComponent>();
					animator.currentClipName = clips[0]->name.c_str();
					animator.playing = true;
					animator.loop = true;
				}
			}
			break;
		}
		case AssetType::Texture:
			break;
		case AssetType::NormalMap:
			break;
		case AssetType::Material:
			m.materialPath = pathStr;
			m.cachedMaterial = nullptr;
			break;
		default: break;
		}
	}
}
