#include "game/hsfformat.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dolphin.h>
#include <ext_math.h>
#include <unordered_set>

extern "C" {
#include "port/byteswap.h"
}

static std::unordered_set<void *> sVisitedPtrs;

template <typename T> [[nodiscard]] constexpr T bswap16(T val) noexcept
{
    static_assert(sizeof(T) == sizeof(u16));
    union {
        u16 u;
        T t;
    } v { .t = val };
#if __GNUC__
    v.u = __builtin_bswap16(v.u);
#elif _WIN32
    v.u = _byteswap_ushort(v.u);
#else
    v.u = (v.u << 8) | ((v.u >> 8) & 0xFF);
#endif
    return v.t;
}

template <typename T> [[nodiscard]] constexpr T bswap32(T val) noexcept
{
    static_assert(sizeof(T) == sizeof(u32));
    union {
        u32 u;
        T t;
    } v { .t = val };
#if __GNUC__
    v.u = __builtin_bswap32(v.u);
#elif _WIN32
    v.u = _byteswap_ulong(v.u);
#else
    v.u = ((v.u & 0x0000FFFF) << 16) | ((v.u & 0xFFFF0000) >> 16) | ((v.u & 0x00FF00FF) << 8) | ((v.u & 0xFF00FF00) >> 8);
#endif
    return v.t;
}

template <typename T> [[nodiscard]] constexpr T bswap64(T val) noexcept
{
    static_assert(sizeof(T) == sizeof(u64));
    union {
        u64 u;
        T t;
    } v { .t = val };
#if __GNUC__
    v.u = __builtin_bswap64(v.u);
#elif _WIN32
    v.u = _byteswap_uint64(v.u);
#else
    static_assert(false, "bswap 64bit not implemented on this target");
#endif
    return v.t;
}

static void bswap16_unaligned(u8 *ptr)
{
    u8 temp = ptr[0];
    ptr[0] = ptr[1];
    ptr[1] = temp;
}

static void bswap32_unaligned(u8 *ptr)
{
    u8 temp = ptr[0];
    ptr[0] = ptr[3];
    ptr[3] = temp;
    temp = ptr[1];
    ptr[1] = ptr[2];
    ptr[2] = temp;
}


// --- relocation bounds checking -------------------------------------------
//
// Every pointer field in an HSF or animation file is stored as a 32-bit offset
// and only resolved later, by adding the address of the loaded buffer (see
// offset_ptr below and hsfload.c). Nothing has ever checked those offsets. A
// wrong field map -- an offset read at the wrong position, or swapped when it
// should not have been -- produces a wild value that is written somewhere it
// should not be and surfaces much later as garbled geometry or silence, with
// neither build able to catch it.
//
// reloc() is the one place all 58 of those conversions go through, so with
// BYTESWAP_VALIDATE defined it can check each one against the size of the file
// actually being converted and say which value was wrong. Off by default; the
// release path is the same cast it always was.
//
// msmSysCheckGrpHead in msmsys.c does the same thing for the msm group header,
// and was until now the only such check in the port.

static const void *sBufBase = nullptr;
static u32 sBufSize = 0;
static u32 sRelocBad = 0;
static u32 sRelocFiller = 0;

extern "C" void byteswap_set_bounds(const void *base, u32 size)
{
    sBufBase = base;
    sBufSize = size;
    sRelocBad = 0;
    sRelocFiller = 0;
}

extern "C" u32 byteswap_bad_reloc_count(void) { return sRelocBad; }

extern "C" u32 byteswap_filler_reloc_count(void) { return sRelocFiller; }

template <typename T> static inline T *reloc(u32 raw)
{
#ifdef BYTESWAP_VALIDATE
    // 0 is a null field, not an offset, and is expected. So is 0xffffffff:
    // -1 is this format's "absent" marker, which Search*Ptr returns NULL for,
    // AttributeLoad tests names against, and ObjectLoad finds the root by.
    //
    // 0xcccccccc is the exporter's filler, left in fields that mean nothing for
    // the record they sit in -- an object's vertex/normal/material pointers when
    // the object is a joint, HSFCENV::name, which nothing reads. Sweeping every
    // HSF on the Mario Party 5 disc produces 60,477 of them and not one other
    // out-of-range value, so reporting each one buries the finding it is there
    // to surface. Counted rather than printed: a wrong field map yields
    // arbitrary values, not exactly this one.
    if (raw == 0xcccccccc) {
        ++sRelocFiller;
    } else if (raw != 0 && raw != 0xffffffff && sBufSize != 0 && raw >= sBufSize) {
        ++sRelocBad;
        OSReport("byteswap> offset 0x%08x out of bounds (buffer %p, size 0x%x)\n",
                 raw, sBufBase, sBufSize);
    }
#endif
    return reinterpret_cast<T *>(static_cast<uintptr_t>(raw));
}

template <typename B> void *offset_ptr(B &base)
{
    return reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(&base));
}

template <typename B, typename T> T *offset_ptr(B &base, T *ptr)
{
    return reinterpret_cast<T *>(reinterpret_cast<uintptr_t>(&base) + reinterpret_cast<uintptr_t>(ptr));
}
template <typename B, typename T> T *offset_ptr(B &base, T *ptr, void *extra)
{
    return reinterpret_cast<T *>(reinterpret_cast<uintptr_t>(&base) + reinterpret_cast<uintptr_t>(ptr) + reinterpret_cast<uintptr_t>(extra));
}

// template <typename B, typename T> static inline void bswap(B &base, T &data);
// template <typename B, typename P> void bswap(B &base, P *&ptr)
// {
//     ptr = bswap32(ptr);
// }
// template <typename B, typename T> void bswap(B &base, T *&ptr, s32 count)
// {
//     ptr = bswap32(ptr);
//     if (ptr == nullptr) {
//         return;
//     }
//     T *objBase = offset_ptr(base, ptr);
//     for (s32 i = 0; i < count; ++i) {
//         if (sVisitedPtrs.contains(objBase)) {
//             continue;
//         }
//         sVisitedPtrs.insert(objBase);
//         bswap(base, *objBase);
//         ++objBase;
//     }
// }
// template <typename B, typename T> void bswap_list(B &base, T **&ptr)
// {
//     ptr = bswap32(ptr);
//     if (ptr == nullptr) {
//         return;
//     }
//     T **objBase = offset_ptr(base, ptr);
//     while (*objBase != nullptr) {
//         bswap(base, *objBase, 1);
//         ++objBase;
//     }
// }
// template <typename B, typename T> void bswap_list(B &base, T *(&ptr)[])
// {
//     T **objBase = ptr;
//     while (*objBase != nullptr) {
//         bswap(base, *objBase, 1);
//         ++objBase;
//     }
// }
template <typename B, typename T> static void bswap_flat(B &base, T *start, s32 count)
{
    T *objBase = start;
    for (s32 i = 0; i < count; ++i) {
        bswap(base, objBase[i]);
    }
}
template <typename B> void bswap(B &base, f32 &v)
{
    v = bswap32(v);
}
template <typename B> void bswap(B &base, s64 &v)
{
    v = bswap64(v);
}
template <typename B> void bswap(B &base, s32 &v)
{
    v = bswap32(v);
}
template <typename B> void bswap(B &base, u32 &v)
{
    v = bswap32(v);
}
template <typename B> void bswap(B &base, s16 &v)
{
    v = bswap16(v);
}
template <typename B> void bswap(B &base, u16 &v)
{
    v = bswap16(v);
}
template <typename B> void bswap(B &base, u8 &v)
{
    // no-op
}
template <typename B> void bswap(B &base, s8 &v)
{
    // no-op
}
template <typename B> void bswap(B &base, char &v)
{
    // no-op
}
template <typename B> void bswap(B &base, Vec &vec)
{
    bswap(base, vec.x);
    bswap(base, vec.y);
    bswap(base, vec.z);
}
template <typename B> void bswap(B &base, S16Vec &vec)
{
    bswap(base, vec.x);
    bswap(base, vec.y);
    bswap(base, vec.z);
}
template <typename B> void bswap(B &base, Vec2f &vec)
{
    bswap(base, vec.x);
    bswap(base, vec.y);
}

template <typename B> void bswap(B &base, AnimData32b &obj, ANIMDATA &dest)
{
    bswap(base, obj.bankNum);
    bswap(base, obj.patNum);
    bswap(base, obj.bmpNum);
    bswap(base, obj.useNum);
    bswap(base, obj.bank);
    bswap(base, obj.pat);
    bswap(base, obj.bmp);

    dest.bankNum = obj.bankNum;
    dest.patNum = obj.patNum;
    dest.bmpNum = obj.bmpNum;
    dest.useNum = obj.useNum;
    dest.bank = reloc<ANIMBANK>(obj.bank);
    dest.pat = reloc<ANIMPAT>(obj.pat);
    dest.bmp = reloc<ANIMBMP>(obj.bmp);
}

template <typename B> void bswap(B &base, AnimBankData32b &obj, ANIMBANK &dest)
{
    bswap(base, obj.timeNum);
    bswap(base, obj.unk);
    bswap(base, obj.frame);

    dest.timeNum = obj.timeNum;
    dest.unk = obj.unk;
    dest.frame = reloc<ANIMFRAME>(obj.frame);
}

template <typename B> void bswap(B &base, AnimPatData32b &obj, ANIMPAT &dest)
{
    bswap(base, obj.layerNum);
    bswap(base, obj.centerX);
    bswap(base, obj.centerY);
    bswap(base, obj.sizeX);
    bswap(base, obj.sizeY);
    bswap(base, obj.layer);

    dest.layerNum = obj.layerNum;
    dest.centerX = obj.centerX;
    dest.centerY = obj.centerY;
    dest.sizeX = obj.sizeX;
    dest.sizeY = obj.sizeY;
    dest.layer = reloc<ANIMLAYER>(obj.layer);
}

template <typename B> void bswap(B &base, AnimBmpData32b &obj, ANIMBMP &dest)
{
    bswap(base, obj.pixSize);
    bswap(base, obj.dataFmt);
    bswap(base, obj.palNum);
    bswap(base, obj.sizeX);
    bswap(base, obj.sizeY);
    bswap(base, obj.dataSize);
    bswap(base, obj.palData);
    bswap(base, obj.data);

    dest.pixSize = obj.pixSize;
    dest.dataFmt = obj.dataFmt;
    dest.palNum = obj.palNum;
    dest.sizeX = obj.sizeX;
    dest.sizeY = obj.sizeY;
    dest.dataSize = obj.dataSize;
    dest.palData = reloc<void>(obj.palData);
    dest.data = reloc<void>(obj.data);
}

template <typename B> void bswap(B &base, ANIMFRAME &obj)
{
    bswap(base, obj.pat);
    bswap(base, obj.time);
    bswap(base, obj.shiftX);
    bswap(base, obj.shiftY);
    bswap(base, obj.flip);
    bswap(base, obj.pad);
}

template <typename B> void bswap(B &base, ANIMLAYER &obj)
{
    bswap(base, obj.alpha);
    bswap(base, obj.flip);
    bswap(base, obj.bmpNo);
    bswap(base, obj.startX);
    bswap(base, obj.startY);
    bswap(base, obj.sizeX);
    bswap(base, obj.sizeY);
    bswap(base, obj.shiftX);
    bswap(base, obj.shiftY);
    bswap_flat(base, obj.vtx, sizeof(obj.vtx) / sizeof(s16));
}

template <typename B> void bswap(B &base, HSFSECTION &obj)
{
    bswap(base, obj.ofs);
    bswap(base, obj.count);
}

template <typename B> void bswap(B &base, HSFHEADER &obj)
{
    bswap(base, obj.scene);
    bswap(base, obj.color);
    bswap(base, obj.material);
    bswap(base, obj.attribute);
    bswap(base, obj.vertex);
    bswap(base, obj.normal);
    bswap(base, obj.st);
    bswap(base, obj.face);
    bswap(base, obj.object);
    bswap(base, obj.bitmap);
    bswap(base, obj.palette);
    bswap(base, obj.motion);
    bswap(base, obj.cenv);
    bswap(base, obj.skeleton);
    bswap(base, obj.part);
    bswap(base, obj.cluster);
    bswap(base, obj.shape);
    bswap(base, obj.mapAttr);
    bswap(base, obj.matrix);
    bswap(base, obj.symbol);
    bswap(base, obj.string);
}

template <typename B> void bswap(B &base, HsfCluster32b &obj, HSFCLUSTER &dest)
{
    bswap(base, obj.name[0]);
    bswap(base, obj.name[1]);
    bswap(base, obj.targetName);
    bswap(base, obj.part);
    bswap(base, obj.index);
    bswap_flat(base, obj.weight, sizeof(obj.weight) / sizeof(float));
    bswap(base, obj.type);
    bswap(base, obj.vertexCnt);
    bswap(base, obj.vertex);

    dest.name[0] = reloc<char>(obj.name[0]);
    dest.name[1] = reloc<char>(obj.name[1]);

    dest.targetName = reloc<char>(obj.targetName);
    dest.part = reloc<HSFPART>(obj.part);
    dest.index = obj.index;
    std::copy(std::begin(obj.weight), std::end(obj.weight), dest.weight);

    dest.adjusted = obj.adjusted;
    dest.unk95 = obj.unk95;
    dest.type = obj.type;
    dest.vertexNum = obj.vertexCnt;
    dest.vertex = reloc<HSFBUFFER *>(obj.vertex);
}

template <typename B> void bswap(B &base, HsfAttribute32b &obj, HSFATTRIBUTE &dest)
{
    bswap(base, obj.name);
    bswap(base, obj.unk04);
    bswap(base, obj.kColor);
    bswap(base, obj.nbtTpLvl);
    bswap(base, obj.unk20);
    bswap(base, obj.unk28);
    bswap(base, obj.unk2C);
    bswap(base, obj.unk30);
    bswap(base, obj.unk34);
    bswap(base, obj.wrapS);
    bswap(base, obj.wrapT);
    bswap(base, obj.maxLod);
    bswap(base, obj.flag);
    bswap(base, obj.bitmap);

    dest.name = reloc<char>(obj.name);
    dest.animWorkP = reloc<struct hsfdraw_struct_01>(obj.unk04);
    std::copy(std::begin(obj.unk8), std::end(obj.unk8), dest.unk8);
    dest.kColor = obj.kColor;
    std::copy(std::begin(obj.unk10), std::end(obj.unk10), dest.unk10);
    dest.nbtTpLvl = obj.nbtTpLvl;
    std::copy(std::begin(obj.unk18), std::end(obj.unk18), dest.unk18);
    dest.unk20 = obj.unk20;
    std::copy(std::begin(obj.unk24), std::end(obj.unk24), dest.unk24);
    dest.scale.x = obj.unk28;
    dest.scale.y = obj.unk2C;
    dest.trans.x = obj.unk30;
    dest.trans.y = obj.unk34;
    std::copy(std::begin(obj.unk38), std::end(obj.unk38), dest.unk38);
    dest.wrapS = obj.wrapS;
    dest.wrapT  = obj.wrapT;
    std::copy(std::begin(obj.unk6C), std::end(obj.unk6C), dest.unk6C);
    dest.maxLod = obj.maxLod;
    dest.flag = obj.flag;
    dest.bitmap = reloc<HSFBITMAP>(obj.bitmap);
}

template <typename B> void bswap(B &base, HsfMaterial32b &obj, HSFMATERIAL &dest)
{
    bswap(base, obj.name);
    bswap(base, obj.pass);
    bswap(base, obj.hiliteScale);
    bswap(base, obj.unk18);
    bswap(base, obj.invAlpha);
    bswap_flat(base, obj.unk20, sizeof(obj.unk20) / sizeof(float));
    bswap(base, obj.refAlpha);
    bswap(base, obj.unk2C);
    bswap(base, obj.flags);
    bswap(base, obj.attrNum);
    bswap(base, obj.attr);

    dest.name = reloc<char>(obj.name);
    std::copy(std::begin(obj.unk4), std::end(obj.unk4), dest.unk4);
    dest.pass = obj.pass;
    dest.vtxMode = obj.vtxMode;
    std::copy(std::begin(obj.litColor), std::end(obj.litColor), dest.litColor);
    std::copy(std::begin(obj.color), std::end(obj.color), dest.color);
    std::copy(std::begin(obj.shadowColor), std::end(obj.shadowColor), dest.shadowColor);
    dest.hiliteScale = obj.hiliteScale;
    dest.unk18 = obj.unk18;
    dest.invAlpha = obj.invAlpha;
    std::copy(std::begin(obj.unk20), std::end(obj.unk20), dest.unk20);
    dest.refAlpha = obj.refAlpha;
    dest.unk2C = obj.unk2C;
    dest.flags = obj.flags;
    dest.attrNum = obj.attrNum;
    dest.attr = reloc<intptr_t>(obj.attr);
}

template <typename B> void bswap(B &base, HSFSCENE &obj)
{
    u32 fogType = static_cast<u32>(obj.fogType);
    fogType = bswap32(fogType);
    obj.fogType = static_cast<GXFogType>(fogType);
    bswap(base, obj.fogStart);
    bswap(base, obj.fogEnd);
}

template <typename B> void bswap(B &base, HsfBuffer32b &obj, HSFBUFFER &dest)
{
    bswap(base, obj.name);
    bswap(base, obj.count);
    bswap(base, obj.data);

    dest.name = reloc<char>(obj.name);
    dest.count = obj.count;
    dest.data = reloc<void>(obj.data);
}

template <typename B> void bswap(B &base, HsfMatrix32b &obj, HSFMATRIX &dest)
{
    bswap(base, obj.base_idx);
    bswap(base, obj.count);

    dest.base_idx = obj.base_idx;
    dest.count = obj.count;

    dest.data = reinterpret_cast<Mtx *>(&obj + 1);
    u32 matricesToByteSwap = dest.base_idx + dest.count + dest.base_idx * dest.count;
    for (u32 i = 0; i < matricesToByteSwap; i++) {
        for (u32 j = 0; j < 3; j++) {
            bswap_flat(base, dest.data[i][j], 4);
        }
    }
}

template <typename B> void bswap(B &base, HsfPalette32b &obj, HSFPALETTE &dest)
{
    bswap(base, obj.name);
    bswap(base, obj.unk);
    bswap(base, obj.palSize);
    bswap(base, obj.data);

    dest.name = reloc<char>(obj.name);
    dest.unk = obj.unk;
    dest.palSize = obj.palSize;
    dest.data = reloc<u16>(obj.data);
}

template <typename B> void bswap(B &base, HsfPart32b &obj, HSFPART &dest)
{
    bswap(base, obj.name);
    bswap(base, obj.num);
    bswap(base, obj.vertex);

    dest.name = reloc<char>(obj.name);
    dest.num = obj.num;
    dest.vertex = reloc<u16>(obj.vertex);
}

template <typename B> void bswap(B &base, HsfBitmap32b &obj, HSFBITMAP &dest)
{
    bswap(base, obj.name);
    bswap(base, obj.maxLod);
    bswap(base, obj.sizeX);
    bswap(base, obj.sizeY);
    bswap(base, obj.palSize);
    bswap(base, obj.palData);
    bswap(base, obj.unk);
    bswap(base, obj.data);

    dest.name = reloc<char>(obj.name);
    dest.maxLod = obj.maxLod;
    dest.dataFmt = obj.dataFmt;
    dest.pixSize = obj.pixSize;
    dest.sizeX = obj.sizeX;
    dest.sizeY = obj.sizeY;
    dest.palSize = obj.palSize;
    dest.tint = obj.tint;
    dest.palData = reloc<u16>(obj.palData);
    dest.unk = obj.unk;
    dest.data = reloc<void>(obj.data);
}

template <typename B> void bswap(B &base, HsfMapAttr32b &obj, HSFMAPATTR &dest)
{
    bswap(base, obj.minX);
    bswap(base, obj.minZ);
    bswap(base, obj.maxX);
    bswap(base, obj.maxZ);
    bswap(base, obj.data);
    bswap(base, obj.dataLen);

    dest.minX = obj.minX;
    dest.minZ = obj.minZ;
    dest.maxX = obj.maxX;
    dest.maxZ = obj.maxZ;
    dest.data = reloc<u16>(obj.data);
    dest.dataLen = obj.dataLen;

}

template <typename B> void bswap(B &base, HSFTRANSFORM &obj)
{
    bswap(base, obj.pos);
    bswap(base, obj.rot);
    bswap(base, obj.scale);
}

template <typename B> void bswap(B &base, HsfSkeleton32b &obj, HSFSKELETON &dest)
{
    bswap(base, obj.name);
    bswap(base, obj.transform);

    dest.name = reloc<char>(obj.name);
    dest.transform = obj.transform;
}

template <typename B> void bswap(B &base, HsfShape32b &obj, HSFSHAPE &dest)
{
    bswap(base, obj.name);
    bswap(base, obj.num16[0]);
    bswap(base, obj.num16[1]);
    bswap(base, obj.vertex);

    dest.name = reloc<char>(obj.name);
    dest.num16[0] = obj.num16[0];
    dest.num16[1] = obj.num16[1];
    dest.vertex = reloc<HSFBUFFER *>(obj.vertex);
}

template <typename B> void bswap(B &base, HSFCENVSINGLE &obj)
{
    bswap(base, obj.target);
    bswap(base, obj.pos);
    bswap(base, obj.posNum);
    bswap(base, obj.normal);
    bswap(base, obj.normalNum);
}

template <typename B> void bswap(B &base, HSFCENVDUALWEIGHT &obj)
{
    bswap(base, obj.weight);
    bswap(base, obj.pos);
    bswap(base, obj.posNum);
    bswap(base, obj.normal);
    bswap(base, obj.normalNum);
}

template <typename B> void bswap(B &base, HsfCenvDual32b &obj, HSFCENVDUAL &dest)
{
    bswap(base, obj.target1);
    bswap(base, obj.target2);
    bswap(base, obj.weightNum);
    bswap(base, obj.weight);

    dest.target1 = obj.target1;
    dest.target2 = obj.target2;
    dest.weightNum = obj.weightNum;
    dest.weight = reloc<HSFCENVDUALWEIGHT>(obj.weight);
}

template <typename B> void bswap(B &base, HSFCENVMULTIWEIGHT &obj)
{
    bswap(base, obj.target);
    bswap(base, obj.value);
}

template <typename B> void bswap(B &base, HsfCenvMulti32b &obj, HSFCENVMULTI &dest)
{
    bswap(base, obj.weightNum);
    bswap(base, obj.pos);
    bswap(base, obj.posNum);
    bswap(base, obj.normal);
    bswap(base, obj.normalNum);
    bswap(base, obj.weight);

    dest.weightNum = obj.weightNum;
    dest.pos = obj.pos;
    dest.posNum = obj.posNum;
    dest.normal = obj.normal;
    dest.normalNum = obj.normalNum;
    dest.weight = reloc<HSFCENVMULTIWEIGHT>(obj.weight);
}

template <typename B> void bswap(B &base, HsfCenv32b &obj, HSFCENV &dest)
{
    bswap(base, obj.name);
    bswap(base, obj.singleData);
    bswap(base, obj.dualData);
    bswap(base, obj.multiData);
    bswap(base, obj.singleCount);
    bswap(base, obj.dualCount);
    bswap(base, obj.multiCount);
    bswap(base, obj.vtxCount);
    bswap(base, obj.copyCount);

    dest.name = reloc<char>(obj.name);
    dest.singleData = reloc<HSFCENVSINGLE>(obj.singleData);
    dest.dualData = reloc<HSFCENVDUAL>(obj.dualData);
    dest.multiData = reloc<HSFCENVMULTI>(obj.multiData);
    dest.singleCount = obj.singleCount;
    dest.dualCount = obj.dualCount;
    dest.multiCount = obj.multiCount;
    dest.vtxCount = obj.vtxCount;
    dest.copyCount = obj.copyCount;
}

template <typename B> void bswap(B &base, HSFCAMERA &obj)
{
    bswap(base, obj.target);
    bswap(base, obj.pos);
    bswap(base, obj.upRot);
    bswap(base, obj.fov);
    bswap(base, obj.nnear);
    bswap(base, obj.ffar);
}

template <typename B> void bswap(B &base, HSFLIGHT &obj)
{
    bswap(base, obj.pos);
    bswap(base, obj.target);
    bswap(base, obj.type);
    bswap(base, obj.unk2C);
    bswap(base, obj.ref_distance);
    bswap(base, obj.ref_brightness);
    bswap(base, obj.cutoff);
}

template <typename B> void bswap(B &base, HsfObjectData32b &obj, HSFMESH &dest, u32 type)
{
    bswap(base, obj.parent);
    bswap(base, obj.childrenCount);
    bswap(base, obj.children);
    bswap(base, obj.base);
    bswap(base, obj.curr);
    bswap(base, obj.face);
    bswap(base, obj.vertex);
    bswap(base, obj.normal);
    bswap(base, obj.color);
    bswap(base, obj.st);
    bswap(base, obj.material);
    bswap(base, obj.attribute);
    bswap(base, obj.shapeNum);
    bswap(base, obj.shape);
    bswap(base, obj.clusterNum);
    bswap(base, obj.cluster);
    bswap(base, obj.cenvNum);
    bswap(base, obj.cenv);
    bswap(base, obj.vtxtop);
    bswap(base, obj.normtop);

    dest.parent = reloc<struct HsfObject_s>(obj.parent);
    dest.childrenCount = obj.childrenCount;
    dest.children = reloc<struct HsfObject_s *>(obj.children);
    dest.base = obj.base;
    dest.curr = obj.curr;
    dest.face = reloc<HSFBUFFER>(obj.face);
    dest.vertex = reloc<HSFBUFFER>(obj.vertex);
    dest.normal = reloc<HSFBUFFER>(obj.normal);
    dest.color = reloc<HSFBUFFER>(obj.color);
    dest.st = reloc<HSFBUFFER>(obj.st);
    dest.material = reloc<HSFMATERIAL>(obj.material);
    dest.attribute = reloc<HSFATTRIBUTE>(obj.attribute);
    dest.writeNum = obj.writeNum;
    dest.shapeType = obj.shapeType;
    dest.matPass = obj.matPass;
    dest.shapeNum = obj.shapeNum;
    dest.shape = reloc<HSFBUFFER *>(obj.shape);
    dest.clusterNum = obj.clusterNum;
    dest.cluster = reloc<HSFCLUSTER *>(obj.cluster);
    dest.cenvNum = obj.cenvNum;
    dest.cenv = reloc<HSFCENV>(obj.cenv);
    dest.vtxtop = reloc<HuVecF>(obj.vtxtop);
    dest.normtop = reloc<HuVecF>(obj.normtop);

    switch (type) {
        case HSF_OBJ_MESH:
            bswap(base, obj.mesh.min);
            bswap(base, obj.mesh.max);
            bswap(base, obj.mesh.baseMorph);
            bswap_flat(base, obj.mesh.morphWeight, std::size(obj.mesh.morphWeight));
            bswap(base, obj.mesh.unkF0);

            dest.mesh.min = obj.mesh.min;
            dest.mesh.max = obj.mesh.max;
            dest.mesh.baseMorph = obj.mesh.baseMorph;
            std::fill(std::begin(dest.mesh.morphWeight), std::end(dest.mesh.morphWeight), 0.0f);
            std::copy(std::begin(obj.mesh.morphWeight), std::end(obj.mesh.morphWeight), dest.mesh.morphWeight);
            break;
        case HSF_OBJ_REPLICA:
            bswap(base, obj.replica);

            dest.replica = reloc<struct HsfObject_s>(obj.replica);
            break;
        default:
            break;
    }
}

template <typename B> void bswap(B &base, HsfObject32b &obj, HSFOBJECT &dest)
{
    bswap(base, obj.name);
    bswap(base, obj.type);
    bswap(base, obj.constData);
    bswap(base, obj.flags);

    dest.name = reloc<char>(obj.name);
    dest.type = obj.type;
    dest.constData = reloc<void>(obj.constData);
    dest.flags = obj.flags;

    switch (obj.type) {
        case HSF_OBJ_CAMERA:
            bswap(base, obj.camera);
            memcpy(&dest.camera, &obj.camera, sizeof(dest.camera));
            break;
        case HSF_OBJ_LIGHT:
            bswap(base, obj.light);
            memcpy(&dest.light, &obj.light, sizeof(dest.light));
            break;
        default:
            bswap(base, obj.data, dest.mesh, obj.type);
            break;
    }
}

template <typename B> void bswap(B &base, HsfBitmapKey32b &obj, HSFBITMAPKEY &dest)
{
    bswap(base, obj.time);
    bswap(base, obj.data);

    dest.time = obj.time;
    dest.data = reloc<HSFBITMAP>(obj.data);
}

template <typename B> void bswap(B &base, HsfTrack32b &obj, HSFTRACK &dest)
{
    bswap(base, obj.target);
    bswap(base, obj.curveType);
    bswap(base, obj.numKeyframes);

    dest.type = obj.type;
    dest.start = obj.start;
    dest.target = obj.target;
    dest.curveType = obj.curveType;
    dest.numKeyframes = obj.numKeyframes;

    if (obj.curveType == HSF_CURVE_CONST) {
        bswap(base, obj.value);
        dest.value = obj.value;
    }
    else {
        bswap(base, obj.data);
        dest.data = reloc<void>(obj.data);
    }

    // TODO is this right?
    if (obj.type == HSF_TRACK_CLUSTER_WEIGHT) {
        bswap(base, obj.clusterWeight);
        dest.clusterWeight = obj.clusterWeight;
    }
    else {
        bswap(base, obj.attrIdx);
        bswap(base, obj.channel);

        dest.attrIdx = obj.attrIdx;
        dest.channel = obj.channel;
    }
}

template <typename B> void bswap(B &base, HsfMotion32b &obj, HSFMOTION &dest)
{
    bswap(base, obj.name);
    bswap(base, obj.numTracks);
    bswap(base, obj.track);
    bswap(base, obj.maxTime);

    dest.name = reloc<char>(obj.name);
    dest.numTracks = obj.numTracks;
    dest.track = reloc<HSFTRACK>(obj.track);
    dest.maxTime = obj.maxTime;
}

template <typename B> void bswap(B &base, HsfFace32b &obj, HSFFACE &dest)
{
    bswap(base, obj.type);
    bswap(base, obj.mat);
    bswap(base, obj.nbt);

    dest.type = obj.type;
    dest.mat = obj.mat;
    dest.nbt = obj.nbt;

    // TODO or obj.type == 4?
    if ((obj.type & 7) == 4) {
        bswap(base, obj.strip.count);
        bswap(base, obj.strip.data);
        bswap_flat(base, obj.strip.indices[0], 3 * 4);
    
        dest.strip.count = obj.strip.count;
        dest.strip.data = reloc<s16>(obj.strip.data);
        std::copy(&obj.strip.indices[0][0], &obj.strip.indices[0][0] + 3 * 4, &dest.strip.indices[0][0]);
    }
    else {
        bswap_flat(base, obj.indices[0], 4 * 4);
        std::copy(&obj.indices[0][0], &obj.indices[0][0] + 4 * 4, &dest.indices[0][0]);
    }
}

template <typename B> void bswap(B &base, GameStat &obj)
{
    bswap(base, obj.unk_00);
    bswap(base, obj.total_stars);
    bswap(base, obj.create_time);
    bswap_flat(base, obj.mg_custom, 2);
    bswap_flat(base, obj.mg_avail, 2);
    bswap_flat(base, obj.mg_record, 15);
    bswap_flat(base, obj.board_max_stars, 9);
    bswap_flat(base, obj.board_max_coins, 9);
}

template <typename B> void bswap(B &base, SystemState &obj)
{
    bswap(base, obj.bitfield2);
    bswap(base, obj.block_pos);
    bswap(base, obj.mg_next);
    bswap(base, obj.mg_type);
    bswap(base, obj.unk_38);
}

template <typename B> void bswap(B &base, PlayerState &obj)
{
    bswap(base, obj.bitfield1);
    bswap(base, obj.bitfield3);
    bswap(base, obj.space_curr);
    bswap(base, obj.space_prev);
    bswap(base, obj.space_next);
    bswap(base, obj.space_shock);
    bswap(base, obj.coins);
    bswap(base, obj.coins_mg);
    bswap(base, obj.coins_total);
    bswap(base, obj.coins_max);
    bswap(base, obj.coins_battle);
    bswap(base, obj.coin_collect);
    bswap(base, obj.coin_win);
    bswap(base, obj.stars);
    bswap(base, obj.stars_max);
}

void byteswap_clear_visited_ptrs()
{
    sVisitedPtrs.clear();
}

void byteswap_u16(u16 *src)
{
    bswap(*src, *src);
}

void byteswap_s16(s16 *src)
{
    bswap(*src, *src);
}

void byteswap_u32(u32 *src)
{
    bswap(*src, *src);
}

void byteswap_s32(s32 *src)
{
    bswap(*src, *src);
}

void byteswap_float(float *src)
{
    bswap(*src, *src);
}

void byteswap_vec(Vec *src)
{
    bswap(*src, *src);
}

void byteswap_vec2f(Vec2f *src)
{
    bswap(*src, *src);
}

void byteswap_hsfvec3f(HuVecF *src)
{
    bswap(*src, *src);
}

void byteswap_hsfvec2f(HuVec2f *src)
{
    auto *vec = reinterpret_cast<Vec2f *>(src);
    bswap(*vec, *vec);
}

void byteswap_animdata(void *src, ANIMDATA *dest)
{
    auto *anim = reinterpret_cast<AnimData32b *>(src);
    bswap(*anim, *anim, *dest);
}

void byteswap_animbankdata(AnimBankData32b *src, ANIMBANK *dest)
{
    bswap(*src, *src, *dest);
}

void byteswap_animpatdata(AnimPatData32b *src, ANIMPAT *dest)
{
    bswap(*src, *src, *dest);
}

void byteswap_animbmpdata(AnimBmpData32b *src, ANIMBMP *dest)
{
    bswap(*src, *src, *dest);
}

void byteswap_animframedata(ANIMFRAME *src)
{
    bswap(*src, *src);
}

void byteswap_animlayerdata(ANIMLAYER *src)
{
    bswap(*src, *src);
}

void byteswap_hsfheader(HSFHEADER *src)
{
    bswap(*src, *src);
}

void byteswap_hsfcluster(HsfCluster32b *src, HSFCLUSTER *dest)
{
    bswap(*src, *src, *dest);
}

void byteswap_hsfattribute(HsfAttribute32b *src, HSFATTRIBUTE *dest)
{
    bswap(*src, *src, *dest);
}

void byteswap_hsfmaterial(HsfMaterial32b *src, HSFMATERIAL *dest)
{
    bswap(*src, *src, *dest);
}

void byteswap_hsfscene(HSFSCENE *src)
{
    bswap(*src, *src);
}

void byteswap_hsfbuffer(HsfBuffer32b *src, HSFBUFFER *dest)
{
    bswap(*src, *src, *dest);
}

void byteswap_hsfmatrix(HsfMatrix32b *src, HSFMATRIX *dest)
{
    bswap(*src, *src, *dest);
}

void byteswap_hsfpalette(HsfPalette32b *src, HSFPALETTE *dest)
{
    bswap(*src, *src, *dest);
}

void byteswap_hsfpart(HsfPart32b *src, HSFPART *dest)
{
    bswap(*src, *src, *dest);
}

void byteswap_hsfbitmap(HsfBitmap32b *src, HSFBITMAP *dest)
{
    bswap(*src, *src, *dest);
}

void byteswap_hsfmapattr(HsfMapAttr32b *src, HSFMAPATTR *dest)
{
    bswap(*src, *src, *dest);
}

void byteswap_hsfskeleton(HsfSkeleton32b *src, HSFSKELETON *dest)
{
    bswap(*src, *src, *dest);
}

void byteswap_hsfshape(HsfShape32b *src, HSFSHAPE *dest)
{
    bswap(*src, *src, *dest);
}

void byteswap_hsfcenv_single(HSFCENVSINGLE *src)
{
    bswap(*src, *src);
}

void byteswap_hsfcenv_dual_weight(HSFCENVDUALWEIGHT *src)
{
    bswap(*src, *src);
}

void byteswap_hsfcenv_dual(HsfCenvDual32b *src, HSFCENVDUAL *dest)
{
    bswap(*src, *src, *dest);
}

void byteswap_hsfcenv_multi_weight(HSFCENVMULTIWEIGHT *src)
{
    bswap(*src, *src);
}

void byteswap_hsfcenv_multi(HsfCenvMulti32b *src, HSFCENVMULTI *dest)
{
    bswap(*src, *src, *dest);
}

void byteswap_hsfcenv(HsfCenv32b *src, HSFCENV *dest)
{
    bswap(*src, *src, *dest);
}

void byteswap_hsfobject(HsfObject32b *src, HSFOBJECT *dest)
{
    bswap(*src, *src, *dest);
}

void byteswap_hsfbitmapkey(HsfBitmapKey32b *src, HSFBITMAPKEY *dest)
{
    bswap(*src, *src, *dest);
}

void byteswap_hsftrack(HsfTrack32b *src, HSFTRACK *dest)
{
    bswap(*src, *src, *dest);
}

void byteswap_hsfmotion(HsfMotion32b *src, HSFMOTION *dest)
{
    bswap(*src, *src, *dest);
}

void byteswap_hsfface(HsfFace32b *src, HSFFACE *dest)
{
    bswap(*src, *src, *dest);
}

// ---------------------------------------------------------------------------
// MSM sound manager
//
// The .msm/.pdt files are raw big-endian disc data. MSM reads them straight
// into structs, so each table is swapped in place immediately after its read
// (see src/msm/msmsys.c, msmmus.c, msmse.c, msmstream.c).
// ---------------------------------------------------------------------------

void byteswap_msmheader(MSM_HEADER *src)
{
    // Every field is s32, including the trailing pad.
    s32 *w = reinterpret_cast<s32 *>(src);
    for (size_t i = 0; i < sizeof(MSM_HEADER) / sizeof(s32); i++) {
        bswap(*src, w[i]);
    }
}

void byteswap_msminfo(MSM_INFO *src)
{
    bswap(*src, src->musMax);
    bswap(*src, src->seMax);
    bswap(*src, src->minMem);
    bswap(*src, src->aramSize);
    bswap(*src, src->grpBufSizeA);
    bswap(*src, src->grpBufSizeB);
    bswap(*src, src->dummyMusSize);
    bswap(*src, src->unk24);
    // voices/music/sfx/grpMax, the stack depths and baseGrp[] are all bytes.
}

void byteswap_msmauxparam(MSM_AUXPARAM *src, s32 count)
{
    for (s32 i = 0; i < count; i++) {
        // `type` and its padding are bytes; the union behind them is entirely
        // 4-byte fields, so swapping it word-wise is correct whichever member
        // is live (an f32 swapped as u32 is the same operation).
        u32 *w = reinterpret_cast<u32 *>(&src[i]) + 1;
        const size_t words = (sizeof(MSM_AUXPARAM) - sizeof(u32)) / sizeof(u32);
        for (size_t j = 0; j < words; j++) {
            bswap(src[i], w[j]);
        }
    }
}

void byteswap_msmgrpinfo(MSM_GRP_INFO *src, s32 count)
{
    for (s32 i = 0; i < count; i++) {
        bswap(src[i], src[i].gid);
        bswap(src[i], src[i].dataOfs);
        bswap(src[i], src[i].dataSize);
        bswap(src[i], src[i].sampOfs);
        bswap(src[i], src[i].sampSize);
    }
}

void byteswap_msmgrphead(MSM_GRP_HEAD *src)
{
    // Offsets from the head of the group buffer to the musyx pool/proj/sdir/song
    // blocks; msmSysPushGroup() turns them into pointers.
    bswap(*src, src->poolOfs);
    bswap(*src, src->projOfs);
    bswap(*src, src->sdirOfs);
    bswap(*src, src->sngOfs);
}

void byteswap_msmmus(MSM_MUS *src, s32 count)
{
    for (s32 i = 0; i < count; i++) {
        bswap(src[i], src[i].sgid);
        bswap(src[i], src[i].sid);
        bswap(src[i], src[i].songOfs);
        bswap(src[i], src[i].songSize);
    }
}

void byteswap_msmse(MSM_SE *src, s32 count)
{
    for (s32 i = 0; i < count; i++) {
        bswap(src[i], src[i].gid);
        bswap(src[i], src[i].fxId);
        bswap(src[i], src[i].pitchBend);
    }
}

void byteswap_msmstreamheader(MSM_STREAM_HEADER *src)
{
    bswap(*src, src->version);
    bswap(*src, src->streamMax);
    bswap(*src, src->chanMax);
    bswap(*src, src->sampleFrq);
    bswap(*src, src->maxBufs);
    bswap(*src, src->streamPackListOfs);
    bswap(*src, src->adpcmParamOfs);
    bswap(*src, src->streamPackOfs);
    bswap(*src, src->sampleOfs);
}

void byteswap_msmstreampack(MSM_STREAM_PACK *src, s32 count)
{
    for (s32 i = 0; i < count; i++) {
        bswap(src[i], src[i].frq);
        bswap(src[i], src[i].loopOfsEnd);
        bswap(src[i], src[i].loopOfsStart);
        for (s32 j = 0; j < 2; j++) {
            bswap(src[i], src[i].stream[j].sampleOfs);
            bswap(src[i], src[i].stream[j].adpcmParamIdx);
        }
    }
}

void byteswap_gamestat(GameStat *src)
{
    bswap(*src, *src);
}

void byteswap_systemstate(SystemState *src)
{
    bswap(*src, *src);
}

void byteswap_playerstate(PlayerState *src)
{
    bswap(*src, *src);
}
