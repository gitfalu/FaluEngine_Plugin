#pragma once
#include <imgui.h>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "asset/loaders/SpriteAsset.h"
#include "asset/loaders/SpriteSlicer.h"

namespace FaluEngine { struct TextureAsset; }

namespace Editor
{
	class SpriteEditorPanel
	{
	public:
		/// @brief アセットパス(.fsprite)を開く。未保存の変更があれば確認ダイアログを出す
		/// @param assetPath 
		void open(const std::string& assetPath);
		void draw();

		[[nodiscard]] bool isOpen() const noexcept { return m_visible; }
		[[nodiscard]] bool isDirty() const noexcept { return m_dirty; }

	private:
		/// @brief 
		enum class Tool { Select, Create };
		/// @brief  スライスを行う形式
		enum class SliceMode { CellSize, Count };

		//===== 読み書き ======
		bool load(const std::string& assetPath);
		bool save();
		void revert();
		void refreshTexture();

		//==== 編集操作(すべてpushUndoを経由する) ====
		void pushUndo();
		void undo();
		void redo();
		void applyAutoSlice();
		void deleteSelected();
		void markDirty() { m_dirty = true; }

		//==== 描画 =====
		void drawToolbar();
		void drawCanvas(const ImVec2& size);
		void drawSidePanel(const ImVec2& size);
		void drawUnsavedPopup();
		void handleShortcuts();
		void fitToView(const ImVec2& viewSize);

		//==== 座標変換 =====
		ImVec2 texToScreen(const ImVec2& origin, const glm::vec2& t) const;
		glm::vec2 screenToTex(const ImVec2& origin, const ImVec2& s) const;

		//==== 状態 =====
		bool m_visible = false;
		std::string m_path; // 開いているアセットのパス
		FaluEngine::SpriteSheetAsset m_sheet; // 編集用コピー
		std::shared_ptr<FaluEngine::TextureAsset> m_texture;
		std::string m_loadedTexturePath;
		bool m_dirty = false;
		bool m_editUndoPushed = false; // ウィジェット連続編集中にUndoを一回だけ積むためのフラグ
		std::string m_status;

		// 表示
		float m_zoom = 1.0f;
		ImVec2 m_pan = { 0.0f,0.0f };
		bool m_needFit = true;
		ImVec2 m_lastCanvasSize = { 0.0f,0.0f };
		ImVec2 m_canvasOrigin = { 0.0f,0.0f }; // キャンバス左上

		// 選択
		std::set<int> m_selected;
		int m_primary = -1;
		Tool m_tool = Tool::Select;

		// ドラッグ
		FaluEngine::SpriteSlicer::Handle m_dragHandle = FaluEngine::SpriteSlicer::Handle::None;
		bool m_dragging = false;
		bool m_creating = false;
		glm::vec2 m_dragStartTex = { 0.0f,0.0f };
		std::vector<std::pair<int, glm::vec4>> m_dragOrig; // 選択スライスの開始時矩形

		// Undo/Redo
		static constexpr size_t kMaxUndo = 64;
		std::vector<std::vector<FaluEngine::SpriteSlice>> m_undoStack;
		std::vector<std::vector<FaluEngine::SpriteSlice>> m_redoStack;

		// 自動分割
		SliceMode m_sliceMode = SliceMode::CellSize;
		int m_cellSize[2] = { 32,32 };
		int m_offset[2] = { 0,0 };
		int m_padding[2] = { 0,0 };
		int m_count[2] = { 4,1 };
		char m_prefix[64] = "sprite";
		bool m_replaceOnSlice = true;

		// クリップ作成
		float m_clipFps = 12.0f;
		bool m_clipLoop = true;

		// 未保存確認
		bool m_askUnsaved = false;
		std::string m_pendingOpen;
		bool m_pendingClose = false;
	};
}