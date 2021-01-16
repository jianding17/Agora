#include "memory_manage.h"

namespace Agora_memory {
inline size_t padded_alloc_size(Alignment_t alignment, size_t size)
{
    size_t align = static_cast<size_t>(alignment);
    size_t padded_size = size;
    size_t padding = align - (size % align);

    if (padding < align) {
        padded_size += padding;
    }
    return padded_size;
}

void* padded_aligned_alloc(Alignment_t alignment, size_t size)
{
    return std::aligned_alloc(
        static_cast<size_t>(alignment), padded_alloc_size(alignment, size));
}
};