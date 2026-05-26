#include "PoolAllocator.h"
#include <stdexcept>
#include <cstring>

namespace FaluEngine {

PoolAllocator::PoolAllocator(std::size_t blockSize, std::size_t blockCount)
    : m_blockSize(std::max(blockSize, sizeof(void*)))  // フリーリストポインタが入る最小サイズ
    , m_blockCount(blockCount)
    , m_freeCount(blockCount)
{
    m_memory = new uint8_t[m_blockSize * m_blockCount];

    // フリーリストを初期化 (各ブロックの先頭に次のブロックへのポインタを埋める)
    m_freeList = reinterpret_cast<void**>(m_memory);
    void** cur = m_freeList;
    for (std::size_t i = 0; i < m_blockCount - 1; ++i) {
        *cur = reinterpret_cast<uint8_t*>(cur) + m_blockSize;
        cur  = reinterpret_cast<void**>(*cur);
    }
    *cur = nullptr; // 末尾
}

PoolAllocator::~PoolAllocator() {
    delete[] m_memory;
}

void* PoolAllocator::allocate() {
    if (!m_freeList)
        throw std::bad_alloc();

    void* block = m_freeList;
    m_freeList  = reinterpret_cast<void**>(*m_freeList);
    --m_freeCount;
    return block;
}

void PoolAllocator::deallocate(void* ptr) noexcept {
    assert(ptr >= m_memory && ptr < m_memory + m_blockSize * m_blockCount
           && "deallocate: pointer out of pool range");

    *reinterpret_cast<void**>(ptr) = m_freeList;
    m_freeList = reinterpret_cast<void**>(ptr);
    ++m_freeCount;
}

} // namespace FaluEngine
