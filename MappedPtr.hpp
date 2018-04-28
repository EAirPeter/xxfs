#ifndef XXFS_MAPPED_PTR_HPP_
#define XXFS_MAPPED_PTR_HPP_

#include "Common.hpp"

namespace xxfs {

/*
template<class tObj>
class MappedPtr {
public:
    constexpr MappedPtr() noexcept = default;

    inline MappedPtr(int fd, uint32_t lcn) :
        x_pObj {reinterpret_cast<tObj *>(Map(fd, lcn, kcbBlkSize))},
        x_pRefCnt {new uint32_t {1}}
    {}

    constexpr MappedPtr(const MappedPtr &vMptr) noexcept : x_pObj {vMptr.x_pObj}, x_pRefCnt {vMptr.x_pRefCnt} {
        if (*this)
            ++*x_pRefCnt;
    }

    template<class tArgObj>
    constexpr MappedPtr(const MappedPtr<tArgObj> &vMptr) noexcept :
        x_pObj {reinterpret_cast<tObj *>(vMptr.x_pObj)},
        x_pRefCnt {vMptr.x_pRefCnt} {
        if (*this)
            ++*x_pRefCnt;
    }
    
    constexpr MappedPtr(MappedPtr &&vMptr) noexcept {
        vMptr.Swap(*this);
    }

    template<class tArgObj>
    constexpr MappedPtr(MappedPtr<tArgObj> &&vMptr) noexcept {
        vMptr.Swap(*this);
    }

    inline ~MappedPtr() {
        Release();
    }

    constexpr MappedPtr &operator =(const MappedPtr &vMptr) noexcept {
        MappedPtr(vMptr).Swap(*this);
        return *this;
    }

    constexpr MappedPtr &operator =(const MappedPtr &&vMptr) noexcept {
        MappedPtr(std::move(vMptr)).Swap(*this);
        return *this;
    }

    inline void Release() noexcept {
        if (*this) {
            if (!--*x_pRefCnt) {
                munmap(x_pObj, kcbBlkSize);
                delete x_pRefCnt;
            }
            x_pRefCnt = nullptr;
        }
    }

    inline void Sync() const noexcept {
        if (*this)
            msync(x_pObj, kcbBlkSize, MS_SYNC);
    }

    template<class tArgObj>
    constexpr MappedPtr<tArgObj> As() const & noexcept {
        return {*this};
    }

    template<class tArgObj>
    constexpr MappedPtr<tArgObj> As() && noexcept {
        return {std::move(*this)};
    }

    constexpr tObj *Get() const noexcept {
        return x_pObj;
    }

    constexpr operator bool() const noexcept {
        return x_pRefCnt;
    }

    constexpr tObj *operator ->() const noexcept {
        return x_pObj;
    }

    constexpr tObj &operator *() const noexcept {
        return *x_pObj;
    }

    template<class tArgObj>
    constexpr void Swap(MappedPtr<tArgObj> &vMptr) noexcept {
        auto pObj = x_pObj;
        x_pObj = reinterpret_cast<tObj *>(vMptr.x_pObj);
        vMptr.x_pObj = reinterpret_cast<tArgObj *>(pObj);
        using std::swap;
        swap(x_pRefCnt, vMptr.x_pRefCnt);
    }

private:
    tObj *x_pObj = nullptr;
    uint32_t *x_pRefCnt = nullptr;
};*/

}

#endif
