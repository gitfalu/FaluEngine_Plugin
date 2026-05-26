#pragma once
#include <cstddef>
#include <cstdint>
#include <cassert>

namespace FaluEngine {

// 固定サイズブロックのプールアロケータ。
// 同じサイズのオブジェクトを大量に生成する場面（パーティクル、コンポーネント等）で使う。
class PoolAllocator {
public:
    // blockSize: 1ブロックのバイト数, blockCount: 確保するブロック数
    PoolAllocator(std::size_t blockSize, std::size_t blockCount);
    ~PoolAllocator();

    // コピー・ムーブ禁止
    PoolAllocator(const PoolAllocator&)            = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;

    [[nodiscard]] void* allocate();
    void deallocate(void* ptr) noexcept;

    [[nodiscard]] std::size_t blockSize()  const noexcept { return m_blockSize; }
    [[nodiscard]] std::size_t blockCount() const noexcept { return m_blockCount; }
    [[nodiscard]] std::size_t freeCount()  const noexcept { return m_freeCount; }

private:
    std::size_t m_blockSize;
    std::size_t m_blockCount;
    std::size_t m_freeCount;
    uint8_t*    m_memory  = nullptr;
    void**      m_freeList = nullptr;
};

} // namespace FaluEngine
