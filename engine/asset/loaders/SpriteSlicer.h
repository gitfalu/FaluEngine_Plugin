#pragma once
#include "SpriteAsset.h"
#include <glm/glm.hpp>
#include <cstddef>
#include <string>
#include <unordered_set>
#include <vector>
#include <FaluEngine/EngineExport.h>

// スプライトエディタが使う、UIに依存しない計算ロジック
// 座標はすべてのテクスチャのピクセル座標
namespace FaluEngine::SpriteSlicer
{
	/// @brief セルサイズ指定のグリッド分割パラメータ
	struct CellGrid
	{
		glm::ivec2 cell = { 32,32 }; // セルのサイズ(px)
		glm::ivec2 offset = { 0,0 }; // 分割開始位置(px)
		glm::ivec2 padding = { 0,0 }; // セル間の余白(px)
	};

	/// @brief 矩形をテクスチャに収め、整数にスナップする。最小サイズは1x1
	/// @param rect 
	/// @param texSize 
	/// @return 
	FALU_GRAPHICS_API glm::vec4 clampRect(const glm::vec4& rect, const glm::ivec2& texSize);

	/// @brief セルサイズ指定で分割する。端に余ったセル未満の領域は作らない
	/// @param texSize 
	/// @param grid 
	/// @param prefix 
	/// @return 
	FALU_GRAPHICS_API std::vector<SpriteSlice> sliceByCellSize(const glm::ivec2& texSize,
		const CellGrid& grid, const std::string& prefix);

	/// @brief 列数＊行数で等分割する。割り切れない端数は最後の列/行に含めない
	/// @param texSize 
	/// @param count 
	/// @param padding 
	/// @param prefix 
	/// @return 
	FALU_GRAPHICS_API std::vector<SpriteSlice> sliceByCount(const glm::ivec2& texSize,
		const glm::ivec2& count, const glm::ivec2& padding, const std::string& prefix);

	/// @brief 既存の名前と重複しない名前を返す(重複時は末尾に番号をつける)
	/// @param slices 
	/// @param base 
	/// @param ignoreIndex 
	/// @return 
	FALU_GRAPHICS_API std::string makeUniqueName(const std::vector<SpriteSlice>& slices,
		const std::string& base, int ignoreIndex = -1);

	/// @brief 一回の分割・1　シートで扱うスライス数の上限
	inline constexpr size_t kMaxSlices = 8192;

	/// @brief セルサイズ分割で作られるスライス数
	/// @param texSize 
	/// @param grid 
	/// @return 
	FALU_GRAPHICS_API size_t countCellGrid(const glm::ivec2& texSize, const CellGrid& grid);

	/// @brief 分割結果をdstの後ろに追加する。名前の重複は集合を使って　O(n)で解消する
	/// １件ずつmakeUniqueNameを呼ぶと　O(n^2)になり大量分割で固まる
	/// @param dst 
	/// @param add 
	/// @return 
	FALU_GRAPHICS_API void appendSlicesUnique(std::vector<SpriteSlice>& dst,
		std::vector<SpriteSlice>&& add);

	enum class Handle
	{
		None,Body,Left,Right,Top,Bottom,TopLeft,TopRight,BottomLeft,BottomRight
	};

	/// @brief 点が矩形のどの部分に当たるか
	/// @param rect 
	/// @param p 
	/// @param handlePx 
	/// @return 
	FALU_GRAPHICS_API Handle hitHandle(const glm::vec4& rect, const glm::vec2& p,float handlePx);

	/// @brief ハンドルをドラッグした結果の矩形
	/// @param orig 
	/// @param h 
	/// @param delta 
	/// @param trexSize 
	/// @return 
	FALU_GRAPHICS_API glm::vec4 dragRect(const glm::vec4& orig, Handle h, const glm::vec2& delta,
		const glm::ivec2& texSize);

	/// @brief 指定スライスを順に並べたクリップを作成
	/// @param sheetPath 
	/// @param sliceIndices 
	/// @param fps 
	/// @param loop 
	/// @return 
	FALU_GRAPHICS_API SpriteClipAsset makeClipFromSlices(const std::string& sheetPath,
		const std::vector<int>& sliceIndices, float fps, bool loop);
}