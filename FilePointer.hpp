#ifndef XXFS_FILE_POINTER_HPP_
#define XXFS_FILE_POINTER_HPP_

#include "Common.hpp"

#include "Xxfs.hpp"

namespace xxfs {

// allocations of cluster (for a file) are handled in this class
class FilePointer : NoCopyMove {
public:
    constexpr FilePointer() noexcept :
        x_bVld {false}, x_bVld1 {false}, x_bVld2 {false}, x_bVld3 {false}
    {}

    inline void Sync() const noexcept {
        ShrSync(x_sp);
        ShrSync(x_sp1);
        ShrSync(x_sp2);
        ShrSync(x_sp3);
    }

    template<class tObj, bool kAlloc = false>
    inline ShrPtr<tObj> Seek(Xxfs *px, Inode *pi, uint32_t vcn) noexcept(!kAlloc) {
        return std::reinterpret_pointer_cast<tObj>(X_Seek<kAlloc>(px, pi, vcn));
    }

private:
    template<bool kAlloc>
    ShrPtr<void> X_Seek(Xxfs *px, Inode *pi, uint32_t vcn) noexcept(!kAlloc);

private:
    uint32_t x_vcn = 0;
    uint32_t x_vcn1 = 0;
    uint32_t x_vcn2 = 0;
    uint32_t x_bVld : 1;
    uint32_t x_bVld1 : 1;
    uint32_t x_bVld2 : 1;
    uint32_t x_bVld3 : 1;
    uint32_t : 0;
    ShrPtr<void> x_sp;
    ShrPtr<IndexCluster> x_sp1;
    ShrPtr<IndexCluster> x_sp2;
    ShrPtr<IndexCluster> x_sp3;

};

extern template ShrPtr<void> FilePointer::X_Seek<false>(Xxfs *px, Inode *pi, uint32_t vcn) noexcept;
extern template ShrPtr<void> FilePointer::X_Seek<true>(Xxfs *px, Inode *pi, uint32_t vcn);

}

#endif
