#include "ContentBrowserPanel.h"
#include "scene/Scene.h"
#include "scene/Entity.h"
#include "scene/Component.h"
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

	void ContentBrowserPanel::draw(FaluEngine::Scene* scene, entt::entity selected)
	{
		ImGui::Begin("Content Browser");

		{
			std::vector<std::filesystem::path> crumbs;
			std::filesystem::path p = m_currentPath;
			while (p != m_rootPath.parent_path()) {
				crumbs.push_back(p);
				if (p == m_rootPath) break;
				p = p.parent_path();
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

		ImGui::SameLine();

		if (m_gridView) {
			ImGui::SetNextItemWidth(80.0f);
			ImGui::SliderFloat("##IconSize", &m_iconSize, 48.0f, 128.0f, "%.0f");
		}

		ImGui::SameLine();

		ImGui::SetNextItemWidth(160.0f);
		ImGui::InputText("##Search", m_searchBuf, sizeof(m_searchBuf));
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Search files");

		ImGui::Separator();

		if (m_gridView) {
			drawGridView(scene, selected);
		}
		else
		{
			drawListView(scene, selected);
		}

		ImGui::End();
	}

	void ContentBrowserPanel::refresh()
	{
		m_entries.clear();
		if (!std::filesystem::exists(m_currentPath)) return;

		//-親ディレクトリの戻りエントリ
		if (m_currentPath != m_rootPath)
		{
			ContentEntry parent;
			parent.path = m_currentPath.parent_path();
			parent.name = "..";
			parent.type = AssetType::Folder;
			parent.isDirectory = true;
			m_entries.push_back(parent);
		}

		//-フォルダを先に、ファイルを後に並べる
		std::vector<ContentEntry> dirs, files;
		for (const auto& entry : std::filesystem::recursive_directory_iterator(m_currentPath))
		{
			ContentEntry ce;
			ce.path = entry.path();
			ce.name = entry.path().filename().string();
			ce.isDirectory = entry.is_directory();
			ce.type = detectType(entry.path());
			(ce.isDirectory ? dirs : files).push_back(ce);
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

	void ContentBrowserPanel::drawGridView(FaluEngine::Scene* scene, entt::entity selected)
	{
		float panelWidth = ImGui::GetContentRegionAvail().x;
		int columns = (std::max)(1,static_cast<int>(panelWidth / (m_iconSize + 16.0f)));

		if (ImGui::BeginTable("##Grid", columns))
		{
			for (auto& entry : m_entries)
			{
				ImGui::TableNextColumn();
				drawEntry(entry, scene, selected, true);
			}
			ImGui::EndTable();
		}
	}

	void ContentBrowserPanel::drawListView(FaluEngine::Scene* scene, entt::entity selected)
	{
		for (auto& entry : m_entries)
		{
			drawEntry(entry, scene, selected, false);
		}
	}

	void ContentBrowserPanel::drawEntry(const ContentEntry& entry, FaluEngine::Scene* scene, entt::entity selected, bool isGrid)
	{
		const std::string pathStr = entry.path.string();
		const char* icon = getTypeIcon(entry.type);
		ImVec4 color = getTypeColor(entry.type);

		if (m_searchBuf[0] != '\0') {
			std::string name = entry.name;
			std::string filter = m_searchBuf;
			std::transform(name.begin(), name.end(), name.begin(), ::tolower);
			std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);
			if (name.find(filter) == std::string::npos) return;
		}

		ImGui::PushID(pathStr.c_str());

		if (isGrid)
		{
			ImGui::BeginGroup();
			ImGui::PushStyleColor(ImGuiCol_Button, { 0.2f,0.2f,0.2f,1.0f });
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.3f,0.3f,0.3f,1.0f });

			ImGui::Button(icon, { m_iconSize,m_iconSize });

			if (!entry.isDirectory && ImGui::BeginDragDropSource()) {
				ImGui::SetDragDropPayload("ASSET_PATH", pathStr.c_str(),
					pathStr.size() + 1);
				m_draggedPath = pathStr;
				m_draggedType = entry.type;
				ImGui::TextColored(color, "%s %s", icon, entry.name.c_str());
				ImGui::EndDragDropSource();
			}

			ImGui::PopStyleColor(2);

			// ダブルクリック処理
			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
				if (entry.isDirectory) {
					navigateTo(entry.path);
				}
				else if (scene && selected != entt::null)
				{
					if (scene->registry().all_of<FaluEngine::MeshComponent>(selected)) {
						auto& m = scene->registry().get<FaluEngine::MeshComponent>(selected);
						if (entry.type == AssetType::Mesh)
						{
							m.meshPath = pathStr;
							m.cachedMesh = nullptr;
						}
						else if (entry.type == AssetType::Texture) {
							m.texturePath = pathStr;
							m.cachedTexture = nullptr;
						}
						else if (entry.type == AssetType::NormalMap) {
							m.normalMapPath = pathStr;
							m.cachedNormalMap = nullptr;
						}
					}
				}
			}

			std::string displayName = entry.name;
			if (displayName.size() > 10)
				displayName = displayName.substr(0, 9) + "..";
			ImGui::TextColored(color, "%s", displayName.c_str());
			if (ImGui::IsItemHovered() && entry.name.size() > 10)
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
					if (entry.isDirectory) {
						navigateTo(entry.path);
					}
					else if (scene && selected != entt::null) {
						if (scene->registry().all_of<FaluEngine::MeshComponent>(selected)) {
							auto& m = scene->registry().get<FaluEngine::MeshComponent>(selected);
							if (entry.type == AssetType::Mesh) {
								m.meshPath = pathStr;
								m.cachedMesh = nullptr;
							}
							else if (entry.type == AssetType::Texture) {
								m.texturePath = pathStr;
								m.cachedTexture = nullptr;
							}
							else if (entry.type == AssetType::NormalMap) {
								m.normalMapPath = pathStr;
								m.cachedNormalMap = nullptr;
							}
						}
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
	}

	void ContentBrowserPanel::navigateTo(const std::filesystem::path& path)
	{

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
}
