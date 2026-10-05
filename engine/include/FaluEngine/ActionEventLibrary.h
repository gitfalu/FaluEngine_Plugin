/*****************************************************************//**
 * \file   ActionEventLibrary.h
 * \brief  
 * 
 * \author tsunn
 * \date   October 2026
 *********************************************************************/
#include <FaluEngine/ActionEventTrack.h>
#include <FaluEngine/EngineExport.h>

#include <memory>
#include <string>
#include <unordered_map>

namespace FaluEngine
{
	class FALU_ENGINE_API ActionEventLibrary
	{
	public:
		static ActionEventLibrary& get();

		/// @brief ランタイム用。なければnullptr
		/// @param meshPath 
		/// @param clipName 
		/// @return 
		[[nodiscard]] std::shared_ptr<const ActionEventTrack> find(const std::string& meshPath, const std::string& clipName);

		/// @brief エディタ用。なければからトラックを作る。呼ぶたびにrevisionが進み、ランタイム側が再取得する
		/// @param meshPath 
		/// @param clipName 
		/// @return 
		[[nodiscard]] std::shared_ptr<ActionEventTrack> edit(const std::string& meshPath, const std::string& clipName);

		/// @brief 編集後に呼ぶ
		/// @param meshPath 
		void markEdited(const std::string& meshPath);

		[[nodiscard]] bool isDirty(const std::string& meshPath) const;

		/// @brief サイドカーへ保存(一時ファイル->リネームで途中失敗しても元ファイルを壊さない)
		/// @param meshPath 
		/// @param errorOut 
		/// @return 
		bool save(const std::string& meshPath, std::string& errorOut = nullptr);

		void reload(const std::string& meshPath);

		[[nodiscard]] uint32_t revision() const noexcept { return m_revision; }

		[[nodiscard]] static std::string sidecarAssetPath(const std::string& meshPath);

	private:
		ActionEventLibrary() = default;

		struct FileEntry
		{
			std::unordered_map<std::string, std::shared_ptr<ActionEventTrack>> tracks;
			bool dirty = false;
		};

		FileEntry& ensureLoaded(const std::string& meshPath);
		
		std::unordered_map<std::string, FileEntry> m_files;
		uint32_t m_revision = 0;
	};
}
