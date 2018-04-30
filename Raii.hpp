#ifndef XXFS_RAII_HPP_
#define XXFS_RAII_HPP_

#include "Common.hpp"

namespace xxfs {
    
struct UnmapDeleter {
    inline void operator ()(void *pObj) const noexcept {
        munmap(pObj, kcbCluSize);
    }
};

template<class tObj>
inline ShrPtr<tObj> ShrMap(int fd, uint32_t lcn) {
    auto pVoid = mmap(
        nullptr, kcbCluSize, PROT_READ | PROT_WRITE,
        MAP_SHARED, fd, (off_t) kcbCluSize * lcn
    );
    if (pVoid == MAP_FAILED)
        RAISE("Failed to invoke mmap()", errno);
    return ShrPtr<tObj>(reinterpret_cast<tObj *>(pVoid), UnmapDeleter {});
}

template<class tObj>
inline void ShrSync(const ShrPtr<tObj> &spc) noexcept {
    if (spc)
        msync(spc.get(), kcbCluSize, MS_SYNC);
}

class RaiiFile {
public:
    constexpr RaiiFile() noexcept = default;
    
    RaiiFile(const RaiiFile &) = delete;

    inline RaiiFile(RaiiFile &&vRf) noexcept {
        vRf.Swap(*this);
    }

    constexpr RaiiFile(int fd) noexcept : x_fd {fd} {}

    inline ~RaiiFile() {
        Close();
    }

    RaiiFile &operator =(const RaiiFile &) = delete;

    inline RaiiFile &operator =(RaiiFile &&vRf) noexcept {
        RaiiFile(std::move(vRf)).Swap(*this);
        return *this;
    }

    constexpr int Get() const noexcept {
        return x_fd;
    }

    inline void Close() noexcept {
        if (x_fd != -1) {
            close(x_fd);
            x_fd = -1;
        }
    }

    inline void Swap(RaiiFile &vRf) noexcept {
        using std::swap;
        swap(x_fd, vRf.x_fd);
    }

private:
    int x_fd = -1;

};

template<class tObj, class tDel>
constexpr std::unique_ptr<tObj, tDel> WrapPtr(tObj *pObj, tDel fnDel) {
    return {pObj, fnDel};
}

#define RAII_GUARD(ptr_, del_) auto CONCAT(up_, __COUNTER__) = ::xxfs::WrapPtr(ptr_, del_)

}

#endif
