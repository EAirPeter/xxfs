#ifndef XXFS_XXFS_HPP_
#define XXFS_XXFS_HPP_

#include "Common.hpp"

#include "BitmapAllocator.hpp"
#include "ClusterCache.hpp"
#include "OpenedFile.hpp"
#include "OpenedDir.hpp"
#include "Raii.hpp"

namespace xxfs {

class Xxfs {
private:
    friend class PathCache;
    friend class InodeCache;
    friend FilePtrR;
    friend FilePtrW;
    
public:
    Xxfs(RaiiFile &&vRf, ShrPtr<MetaCluster> &&spcMeta);

public:
    uint32_t LinAt(const char *pszPath);
    uint32_t LinPar(const char *&pszPath);

    // returns inode number, increase cLookup when hit
    //uint32_t Lookup(FileStat &vStat, uint32_t linPar, const char *pszName);
    //void Forget(uint32_t lin, uint64_t cLookup);
    void GetAttr(FileStat &vStat, uint32_t lin);
    //void SetAttr(FileStat &vStat, FileStat *pStat, uint32_t lin, int nFlags, fuse_file_info *pInfo);
    //const char *ReadLink(uint32_t lin);
    void ReadLink(uint32_t lin, char *pBuf, size_t cbSize);
    //uint32_t MkDir(FileStat &vStat, uint32_t linPar, const char *pszName);
    void MkDir(uint32_t linPar, const char *pszName);
    void Unlink(uint32_t linPar, const char *pszName);
    void RmDir(uint32_t linPar, const char *pszName);
    //uint32_t SymLink(FileStat &vStat, const char *pszLink, uint32_t linPar, const char *pszName);
    void SymLink(const char *pszLink, uint32_t linPar, const char *pszName);
    void Rename(
        uint32_t linPar, const char *pszName,
        uint32_t linNewPar, const char *pszNewName,
        unsigned uFlags
    );
    //void Link(FileStat &vStat, uint32_t lin, uint32_t linNewPar, const char *pszNewName);
    void Link(uint32_t lin, uint32_t linNewPar, const char *pszNewName);
    void Truncate(uint32_t lin, off_t cbNewSize);
    OpenedFile *Open(uint32_t lin, fuse_file_info *pInfo);
    uint64_t Read(OpenedFile *pFile, void *pBuf, uint64_t cbSize, uint64_t cbOff);
    uint64_t Write(OpenedFile *pFile, const void *pBuf, uint64_t cbSize, uint64_t cbOff);
    void Release(OpenedFile *pFile) noexcept;
    void FSync(OpenedFile *pFile) noexcept;
    OpenedDir *OpenDir(uint32_t lin);
    void ReadDir(OpenedDir *pDir, void *pBuf, fuse_fill_dir_t fnFill, off_t vOff);
    void ReleaseDir(OpenedDir *pDir) noexcept;
    void StatFs(VfsStat &vStat) const noexcept;
    //std::pair<uint32_t, OpenedFile *> Create(FileStat &vStat, uint32_t linPar, const char *pszName);
    OpenedFile *Create(uint32_t linPar, const char *pszName);

public:
    // get count of free cluster
    uint32_t AvailClu() const noexcept;

private:
    Inode *X_GetInode(uint32_t lin) noexcept;

private:
    // allocate a free cluster and update meta cluster
    ShrPtr<InodeCluster> Y_MapInoClu(uint32_t vcn) noexcept;
    uint32_t Y_AllocIno();
    // invoked when both lookup count and link count are 0
    void Y_UnlinkIno(uint32_t lin, Inode *pi) noexcept;
    // noexcept, assume mmap does not fail
    template<class tObj>
    inline ShrPtr<tObj> Y_Map(uint32_t lcn) noexcept {
        return x_vCluCache.At<tObj>(lcn);
    }

    inline void Y_Touch(uint32_t lcn) noexcept {
        x_vCluCache.Touch(lcn);
    }

private:
    // allocate a free cluster and update inode if lin is 0
    // invokes Y_AllocClu
    template<class tObj>
    inline ShrPtr<tObj> Y_FileAllocClu(Inode *pi, uint32_t &lcn) {
        if (x_spcMeta->ccUsed >= x_spcMeta->ccTotal)
            throw Exception {ENOSPC};
        lcn = x_vCluAlloc.Alloc();
        ++x_spcMeta->ccUsed;
        ++pi->ccSize;
        auto &&spc = Y_Map<tObj>(lcn);
        memset(spc.get(), 0, kcbCluSize);
        return std::move(spc);
    }
    // only used in shrink
    // check if each lcn is 0, do not double free
    void Y_FileFreeClu(Inode *pi, uint32_t &lcn) noexcept;
    void Y_FileFreeIdx1(Inode *pi, uint32_t &lcn, uint32_t vcnFrom = 0) noexcept;
    void Y_FileFreeIdx2(Inode *pi, uint32_t &lcn, uint32_t vcnFrom = 0) noexcept;
    void Y_FileFreeIdx3(Inode *pi, uint32_t &lcn, uint32_t vcnFrom = 0) noexcept;
    // invoked when fsync and close
    void Y_FileShrink(Inode *pi) noexcept;

private:
    constexpr static void X_FillStat(FileStat &vStat, uint32_t lin, Inode *pNod) noexcept;

private:
    RaiiFile x_vRf;
    ShrPtr<MetaCluster> x_spcMeta;
    ClusterCache<256> x_vCluCache;
    ClusterCache<64> x_vInoCluCache;
    BitmapAllocator x_vCluAlloc;
    BitmapAllocator x_vInoAlloc;
    
};

}

#endif
