#ifndef XXFS_INODE_CACHE_HPP_
#define XXFS_INODE_CACHE_HPP_

#include "Common.hpp"

namespace xxfs {

class Xxfs;

class InodeCache : NoCopyMove {
private:
    struct X_ActInoClu : NoCopy {
        ShrPtr<InodeCluster> spc;
        uint64_t cInoLookup[kciPerClu] {};
        uint64_t cCluLookup = 0;

        X_ActInoClu(ShrPtr<InodeCluster> &&spc_);
    };

public:
    inline InodeCache(Xxfs *px) : x_px {px} {}

    Inode *At(uint32_t lin);
    Inode *IncLookup(uint32_t lin);
    void DecLookup(uint32_t lin, uint64_t cLookup);

private:
    Xxfs *const x_px;
    std::unordered_map<uint32_t, X_ActInoClu> x_map;

};

}

#endif
