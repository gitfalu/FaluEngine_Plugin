#include "SpriteSlicer.h"

#include <algorithm>
#include <cmath>

namespace FaluEngine::SpriteSlicer
{
	glm::vec4 clampRect(const glm::vec4& rect, const glm::ivec2& texSize)
	{
		const float tw = static_cast<float>((std::max)(texSize.x, 1));
		const float th = static_cast<float>((std::max)(texSize.y, 1));
		// Roundとmaxを使用して不正な値を修正
		float x = std::round(rect.x), y = std::round(rect.y);
		float w = (std::max)(std::round(rect.z), 1.0f);
		float h = (std::max)(std::round(rect.w), 1.0f);

		w = (std::min)(w, tw);
		h = (std::min)(h, th);
		x = std::clamp(x, 0.0f, tw - w);
		y = std::clamp(y, 0.0f, th - h);
		return { x,y,w,h };
	}

	std::vector<SpriteSlice> sliceByCellSize(const glm::ivec2& texSize, const CellGrid& grid, 
		const std::string& prefix)
	{
		std::vector<SpriteSlice> out;
		if (texSize.x <= 0 || texSize.y <= 0 || grid.cell.x <= 0 || grid.cell.y <= 0) return out;
		// 最低値保障
		const int padX = (std::max)(grid.padding.x, 0);
		const int padY = (std::max)(grid.padding.y, 0);
		int index = 0;
		// セルを配置した際にはみ出して配置されないようにループを回す
		for (int y = (std::max)(grid.offset.y, 0); y + grid.cell.y <= texSize.y; y += grid.cell.y + padY)
		{
			for (int x = (std::max)(grid.offset.x, 0); x + grid.cell.x <= texSize.x; x += grid.cell.x + padX)
			{
				SpriteSlice s;
				// セルごとの名前を与えられた文字列の末尾に番号をつけて設定
				s.name = prefix + "_" + std::to_string(index++);
				// セルごとの情報を入れる
				s.rect = { static_cast<float>(x),static_cast<float>(y),
						static_cast<float>(grid.cell.x),static_cast<float>(grid.cell.y) };
				out.push_back(std::move(s));
			}
		}

		return out;
	}

	std::vector<SpriteSlice> sliceByCount(const glm::ivec2& texSize, const glm::ivec2& count, 
		const glm::ivec2& padding, const std::string& prefix)
	{
		if (count.x <= 0 || count.y <= 0 || texSize.x <= 0 || texSize.y <= 0) return {};

		const int padX = (std::max)(padding.x, 0);
		const int padY = (std::max)(padding.y, 0);
		const int cw = (texSize.x - padX * (count.x - 1)) / count.x;
		const int ch = (texSize.y - padY * (count.y - 1)) / count.y;
		if (cw <= 0 || ch <= 0) return {};

		CellGrid g;
		g.cell = { cw,ch };
		g.padding = { padX,padY };
		auto slices = sliceByCellSize(texSize, g, prefix);
		// 端数で余分なセルが増えないよう指定数に切り詰める
		const size_t want = static_cast<size_t>(count.x) * static_cast<size_t>(count.y);
		if (slices.size() > want) slices.resize(want);

		return slices;
	}

	std::string makeUniqueName(const std::vector<SpriteSlice>& slices, const std::string& base, 
		int ignoreIndex)
	{
		const std::string root = base.empty() ? "sprite" : base;
		auto exists = [&](const std::string& n) {
			for (size_t i = 0; i < slices.size(); ++i)
				if (static_cast<int>(i) != ignoreIndex && slices[i].name == n) return true;
			return false;
			};
		if (!exists(root)) return root;
		for (int i = 1; i < 100000; ++i)
		{
			std::string candiate = root + "_" + std::to_string(i);
			if (!exists(candiate)) return candiate;
		}

		return root;
	}

	size_t countCellGrid(const glm::ivec2& texSize, const CellGrid& grid)
	{
		if (texSize.x <= 0 || texSize.y <= 0 || grid.cell.x <= 0 || grid.cell.y <= 0) return 0;
		// 入力値に負の数がないように調整
		const long long padX = (std::max)(grid.padding.x, 0);
		const long long padY = (std::max)(grid.padding.y, 0);
		const long long offX = (std::max)(grid.offset.x, 0);
		const long long offY = (std::max)(grid.offset.y, 0);
		// 分割時のピクセル数を分割開始位置が下回っている場合分割しない
		// 理由はピクセル数が最大の分割可能回数なのでそれを分割開始位置の変更に寄って下回ると分割できない
		const long long availX = texSize.x - offX, availY = texSize.y - offY;
		if (availX < grid.cell.x || availY < grid.cell.y) return 0;
		// 不正値をなくした状態でセル数をカウント
		const long long cols = (availX - grid.cell.x) / (grid.cell.x + padX) + 1;
		const long long rows = (availY - grid.cell.y) / (grid.cell.y + padY) + 1;
		// セルの合計数を算出して返す
		return static_cast<size_t>(cols * rows);
	}

	void appendSlicesUnique(std::vector<SpriteSlice>& dst, std::vector<SpriteSlice>&& add)
	{
		std::unordered_set<std::string> used;
		used.reserve(dst.size() + add.size());// 格納可能最低要素数を調整
		for (const auto& s : dst) used.insert(s.name); // 追加

		dst.reserve(dst.size() + add.size()); // 最低要素数調整
		for (auto& s : add)
		{ // 追加するスプライトデータごとに処理を行う
			const std::string base = s.name.empty() ? "sprite" : s.name;
			std::string name = base;
			// スプライトを一つのセルごとに番号をつける
			for (int i = 1; used.count(name) && i < 1000000; ++i)
				name = base + "_" + std::to_string(i);
			// 格納
			used.insert(name);
			s.name = std::move(name);
			dst.push_back(std::move(s));
		}
	}

	Handle hitHandle(const glm::vec4& rect, const glm::vec2& p, float handlePx)
	{
		const float hp = (std::max)(handlePx, 0.5f); // 移動量が最小値を下回っていないかチェック＆調整
		// 移動前のカーソルが中にあったかどうか
		const bool inX = p.x >= rect.x - hp && p.x <= rect.x + rect.z + hp;
		const bool inY = p.y >= rect.y - hp && p.y <= rect.y + rect.w + hp;
		if (!inX || !inY) return Handle::None;

		// 移動量の最小値より現在の位置と矩形の位置の距離が近いかをチェック
		const bool left		= std::fabs(p.x - rect.x)				<= hp;
		const bool right	= std::fabs(p.x - (rect.x + rect.z))	<= hp;
		const bool top		= std::fabs(p.y - rect.y)				<= hp;
		const bool bottom	= std::fabs(p.y - (rect.y + rect.w))	<= hp;

		// それぞれの操作を距離のチェックから操作内容を返す
		if (left && top) return Handle::TopLeft;
		if (right && top) return Handle::TopRight;
		if (left && bottom) return Handle::BottomLeft;
		if (right && bottom) return Handle::BottomRight;
		if (left) return Handle::Left;
		if (right) return Handle::Right;
		if (top) return Handle::Top;
		if (bottom) return Handle::Bottom;
		// 移動が起こっていない場合、選択オブジェクトの中にカーソルが存在するか
		const bool inside = p.x >= rect.x && p.x <= rect.x + rect.z && p.y >= rect.y && p.y <= rect.y + rect.w;
		return inside ? Handle::Body : Handle::None;
	}

	glm::vec4 dragRect(const glm::vec4& orig, Handle h, const glm::vec2& delta, 
		const glm::ivec2& texSize)
	{
		// 斜め移動や垂直、水平移動にも対応するために変数を４つ用意
		float l = orig.x, t = orig.y, r = orig.x + orig.z, b = orig.y + orig.w;
		// 各操作に応じて位置の移動を行う
		switch (h)
		{
		case FaluEngine::SpriteSlicer::Handle::Body:
			l += delta.x; r += delta.x; t += delta.y; b += delta.y; break;
		case Handle::Left:			l += delta.x; break;
		case Handle::Right:			r += delta.x; break;
		case Handle::Top:			t += delta.y; break;
		case Handle::Bottom:		b += delta.y; break;
		case Handle::TopLeft:		l += delta.x; t += delta.y; break;
		case Handle::TopRight:		r += delta.x; t += delta.y; break;
		case Handle::BottomLeft:	l += delta.x; b += delta.y; break;
		case Handle::BottomRight:	r += delta.x; b += delta.y; break;
		case Handle::None: return orig;
		}

		if (h == Handle::Body)
			return clampRect({ l,t,orig.z,orig.w }, texSize);// 大きさを保ったまま移動

		// リサイズ
		if (r - l < 1.0f) { if (h == Handle::Left || h == Handle::TopLeft || h == Handle::BottomLeft) l = r - 1.0f; else r = l + 1.0f; }
		if (b - t < 1.0f) { if (h == Handle::Top || h == Handle::TopLeft || h == Handle::TopRight) t = b - 1.0f; else b = t + 1.0f; }
		// 不正値除去を行う
		l = std::clamp(std::round(l), 0.0f, static_cast<float>(texSize.x) - 1.0f);
		t = std::clamp(std::round(t), 0.0f, static_cast<float>(texSize.y) - 1.0f);
		r = std::clamp(std::round(r), l + 1.0f, static_cast<float>(texSize.x));
		b = std::clamp(std::round(b), t + 1.0f, static_cast<float>(texSize.y));

		return { l,t,r - 1 ,b - t };
	}

	SpriteClipAsset makeClipFromSlices(const std::string& sheetPath, 
		const std::vector<int>& sliceIndices, float fps, bool loop)
	{
		SpriteClipAsset clip;
		clip.sheetPath = sheetPath;
		clip.loop = loop;
		const float dur = 1.0 / (std::max)(fps, 0.001f);
		for (int i : sliceIndices) clip.frames.push_back({ i,dur });
		return clip;
	}
}