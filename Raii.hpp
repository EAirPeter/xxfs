#ifndef XXFS_RAII_HPP_
#define XXFS_RAII_HPP_

#include "Common.hpp"

template<class tObj, class tRel, tObj kInvalid = tObj {}>
class RaiiObj {
public:
    constexpr RaiiObj() noexcept = default;

    template<class tArgObj>
    constexpr RaiiObj(tArgObj &&vObj) noexcept :
        x_vObj {std::forward<tArgObj>(vObj)}
    {}

    template<class tArgObj, class tArgRel>
    constexpr RaiiObj(tArgObj &&vObj, tArgRel &&fnRel) noexcept :
        x_vObj {std::forward<tArgObj>(vObj)},
        x_fnRel {std::forward<tArgRel>(fnRel)}
    {}

    RaiiObj(const RaiiObj &) = delete;

    constexpr RaiiObj(RaiiObj &&vOther) noexcept {
        vOther.Swap(*this);
    }

    inline ~RaiiObj() {
        Release();
    }

    RaiiObj &operator =(const RaiiObj &) = delete;

    constexpr RaiiObj &operator =(RaiiObj &&vOther) noexcept {
        Release();
        vOther.Swap(*this);
        return *this;
    }

    template<class tArgObj>
    constexpr RaiiObj &operator =(tArgObj &&vObj) noexcept {
        Reset(std::forward<tArgObj>(vObj));
        return *this;
    }

    constexpr tObj operator ->() const noexcept {
        return x_vObj;
    }

    constexpr std::add_lvalue_reference_t<std::remove_pointer_t<tObj>> operator *() const noexcept {
        return *x_vObj;
    }

    template<class tOff>
    constexpr std::add_lvalue_reference_t<std::remove_pointer_t<tObj>> operator [](tOff &&vOff) const noexcept {
        return x_vObj[std::forward<tOff>(vOff)];
    }

    constexpr operator bool() const noexcept {
        return x_vObj != kInvalid;
    }

    constexpr const tObj &Get() const noexcept {
        return x_vObj;
    }

    constexpr tObj &Get() noexcept {
        return x_vObj;
    }

    template<class tArgObj>
    constexpr void Reset(tArgObj &&vObj) noexcept {
        Release();
        x_vObj = std::forward<tArgObj>(vObj);
    }

    inline void Release() noexcept {
        if (x_vObj != kInvalid) {
            x_fnRel(x_vObj);
            x_vObj = kInvalid;
        }
    }

    constexpr void Swap(RaiiObj &vOther) noexcept {
        using std::swap;
        swap(x_vObj, vOther.x_vObj);
        swap(x_fnRel, vOther.x_fnRel);
    }

    friend constexpr void swap(RaiiObj &vLhs, RaiiObj &vRhs) noexcept {
        vLhs.Swap(vRhs);
    }

private:
    tObj x_vObj = kInvalid;
    tRel x_fnRel = tRel {};

};

template<class tObj>
struct DefaultReleaser {
    static_assert(sizeof(tObj));
    inline void operator ()(tObj *pObj) const noexcept {
        delete pObj;
    }
};

template<class tObj>
struct ArrayReleaser {
    static_assert(sizeof(tObj));
    inline void operator ()(tObj *pObj) const noexcept {
        delete[] pObj;
    }
};

struct PodReleaser {
    inline void operator ()(void *pObj) const noexcept {
        ::operator delete(pObj);
    }
};

struct UnmapReleaser {
    constexpr UnmapReleaser(size_t cbSize = 0) noexcept : x_cbSize {cbSize} {}
    inline void operator ()(void *pObj) const noexcept {
        munmap(pObj, x_cbSize);
    }
private:
    size_t x_cbSize;
};

// allocate use ::operator new
template<class tObj>
using DefPtr = RaiiObj<std::add_pointer_t<tObj>, DefaultReleaser<tObj>>;

// allocate use ::operator new[]
template<class tObj>
using DefArr = RaiiObj<std::add_pointer_t<std::remove_extent_t<tObj>>, ArrayReleaser<tObj>>;

// allocate use ::operator new
template<class tObj, std::enable_if_t<std::is_pod_v<tObj>, int> = 0>
using PodPtr = RaiiObj<std::add_pointer_t<tObj>, PodReleaser>;

/*
// allocate use ::operator new
template<class tObj>
using PodArr = PodPtr<tObj>;
*/

// allocate use mmap
template<class tObj, std::enable_if_t<std::is_pod_v<tObj>, int> = 0>
using MapPtr = RaiiObj<std::add_pointer_t<std::decay_t<tObj>>, UnmapReleaser>;

using RaiiFile = RaiiObj<int, decltype(&close), -1>;

template<class tObj>
inline MapPtr<tObj> MapAt(int fd, uint32_t blk, size_t cbSize) {
    auto pVoid = mmap(nullptr, cbSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t) blk << kcBlkBit);
    if (pVoid == MAP_FAILED)
        RAISE("Failed to map block", errno);
    return {(tObj *) pVoid, UnmapReleaser {cbSize}};
}

template<class tObj>
inline MapPtr<tObj> MapAt(int fd, uint32_t blk) {
    return MapAt<tObj>(fd, blk, kcbBlkSize);
}

template<class tObj, class tRel>
constexpr decltype(auto) WrapObj(tObj &&vObj, tRel &&vRel) {
    return RaiiObj<std::decay_t<tObj>, std::decay_t<tRel>> {
        std::forward<tObj>(vObj),
        std::forward<tRel>(vRel)
    };
}

template<class tObj, class tRel, std::decay_t<tObj> kInvalid>
constexpr decltype(auto) WrapObjInvalid(tObj &&vObj, tRel &&vRel) {
    return RaiiObj<std::decay_t<tObj>, std::decay_t<tRel>, kInvalid> {
        std::forward<tObj>(vObj),
        std::forward<tRel>(vRel)
    };
}

#define RAII_GUARD(obj_, rel_) auto CONCAT(ro_, __COUNTER__) = WrapObj(obj_, rel_)
#define RAII_GUARD_INVALID(obj_, rel_, invalid_) auto CONCAT(ro_, __COUNTER__) = \
    WrapObjInvalid<decltype((obj_)), decltype((rel_)), invalid_>(obj_, rel_)

#endif
