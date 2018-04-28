#include "Common.hpp"

#include "InodeCache.hpp"
#include "Xxfs.hpp"

namespace xxfs {

inline InodeCache::X_ActInoClu::X_ActInoClu(ShrPtr<InodeCluster> &&spc_) : spc(std::move(spc_)) {}

Inode *InodeCache::At(uint32_t lin) {
    auto vin = lin % kciPerClu;
    auto vcn = lin / kciPerClu;
    auto it = x_map.find(vcn);
    if (it == x_map.end())
        throw Exception {ENOENT};
    return &it->second.spc->aInos[vin];
}

Inode *InodeCache::IncLookup(uint32_t lin) {
    auto vin = lin % kciPerClu;
    auto vcn = lin / kciPerClu;
    auto it = x_map.find(vcn);
    if (it == x_map.end()) {
        auto &&res = x_map.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(vcn),
            std::forward_as_tuple(x_px->Y_MapInoClu(vcn))
        );
        it = res.first;
    }
    auto &vAct = it->second;
    ++vAct.cCluLookup;
    ++vAct.cInoLookup[vin];
    return &vAct.spc->aInos[vin];
}

void InodeCache::DecLookup(uint32_t lin, uint64_t cLookup) {
    auto vin = lin % kciPerClu;
    auto vcn = lin / kciPerClu;
    auto it = x_map.find(vcn);
    if (it == x_map.end())
        throw Exception {ENOENT};
    auto &vAct = it->second;
    if (!--vAct.cInoLookup[vin]) {
        auto pi = &vAct.spc->aInos[vin];
        if (!pi->cLink)
            x_px->Y_EraseIno(lin, pi);
    }
    if (!--vAct.cCluLookup)
        x_map.erase(it);
}

}
