#ifndef XXFS_MAPPED_CACHE_HPP_
#define XXFS_MAPPED_CACHE_HPP_

#include "Common.hpp"

#include "Raii.hpp"

namespace xxfs {

template<uint32_t kCapacity>
class ClusterCache : NoCopyMove {
public:
    inline ClusterCache(int fd) noexcept : x_fd {fd} {
        x_aLinked[0].idxPrev = kCapacity;
        for (uint32_t i = 1; i <= kCapacity; ++i) {
            x_aLinked[i].idxPrev = i - 1;
        }
        for (uint32_t i = 0; i < kCapacity; ++i)
            x_aLinked[i].idxNext = i + 1;
        x_aLinked[kCapacity].idxNext = 0;
    }

    template<class tObj>
    inline ShrPtr<tObj> At(uint32_t lcn) noexcept {
        auto it = x_map.find(lcn);
        uint32_t idx;
        if (it == x_map.end()) {
            idx = x_aLinked[kCapacity].idxNext;
            if (x_aMapped[idx]) {
                x_map.erase(x_aLcn[idx]);
                x_aMapped[idx].reset();
            }
            x_aLcn[idx] = lcn;
            x_aMapped[idx] = ShrMap<void>(x_fd, lcn);
            x_map.emplace(lcn, idx);
        }
        else
            idx = it->second;
        X_LnkRemove(idx);
        X_LnkAddTail(idx);
        assert(x_map.size() <= (size_t) kCapacity);
        return std::reinterpret_pointer_cast<tObj>(x_aMapped[idx]);
    }

    inline void Touch(uint32_t lcn) noexcept {
        auto it = x_map.find(lcn);
        if (it == x_map.end())
            return;
        auto idx = it->second;
        X_LnkRemove(idx);
        X_LnkAddTail(idx);
        assert(x_map.size() <= (size_t) kCapacity);
    }

    inline void Remove(uint32_t lcn) noexcept {
        auto it = x_map.find(lcn);
        if (it == x_map.end())
            return;
        auto idx = it->second;
        X_LnkRemove(idx);
        X_LnkAddHead(idx);
        x_map.erase(it);
        x_aMapped[idx].reset();
        assert(x_map.size() <= (size_t) kCapacity);
    }

private:
    constexpr void X_LnkRemove(uint32_t idx) noexcept {
        auto idxPrev = x_aLinked[idx].idxPrev;
        auto idxNext = x_aLinked[idx].idxNext;
        x_aLinked[idxPrev].idxNext = idxNext;
        x_aLinked[idxNext].idxPrev = idxPrev;
    }

    constexpr void X_LnkAddHead(uint32_t idx) noexcept {
        auto idxNext = x_aLinked[kCapacity].idxNext;
        x_aLinked[kCapacity].idxNext = idx;
        x_aLinked[idxNext].idxPrev = idx;
        x_aLinked[idx].idxPrev = kCapacity;
        x_aLinked[idx].idxNext = idxNext;
    }

    constexpr void X_LnkAddTail(uint32_t idx) noexcept {
        auto idxPrev = x_aLinked[kCapacity].idxPrev;
        x_aLinked[kCapacity].idxPrev = idx;
        x_aLinked[idxPrev].idxNext = idx;
        x_aLinked[idx].idxPrev = idxPrev;
        x_aLinked[idx].idxNext = kCapacity;
    }

private:
    struct X_Node {
        uint32_t idxPrev;
        uint32_t idxNext;
    };

private:
    int x_fd;
    X_Node x_aLinked[kCapacity + 1];
    uint32_t x_aLcn[kCapacity];
    ShrPtr<void> x_aMapped[kCapacity];
    std::unordered_map<uint32_t, uint32_t> x_map;

};

}

#endif
