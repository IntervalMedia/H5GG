#ifndef JJ_Header_h
#define JJ_Header_h

/// use -fvisibility=hidden to hide symbols by default
// #pragma GCC visibility push(hidden)
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#pragma GCC diagnostic ignored "-Wdeprecated-register"

#import <Foundation/Foundation.h>
#include <mach-o/dyld.h>
#include <mach/mach.h>
#include <mach/vm_region.h>
#include <sys/mman.h>
#include <pthread.h>
#include <ext/hash_map>
#include <unordered_map>
#include <vector>
#include <map>
#include <set>
#include <string.h>
#include <arm_neon.h>

using namespace std;

extern "C" kern_return_t mach_vm_region
(
     vm_map_t target_task,
     mach_vm_address_t *address,
     mach_vm_size_t *size,
     vm_region_flavor_t flavor,
     vm_region_info_t info,
     mach_msg_type_number_t *infoCnt,
     mach_port_t *object_name
);

extern "C" kern_return_t mach_vm_region_recurse
(
    vm_map_t target_task,
    mach_vm_address_t *address,
    mach_vm_size_t *size,
    natural_t *nesting_depth,
    vm_region_recurse_info_t info,
    mach_msg_type_number_t *infoCnt
);

extern "C" kern_return_t mach_vm_protect
(
 vm_map_t target_task,
 mach_vm_address_t address,
 mach_vm_size_t size,
 boolean_t set_maximum,
 vm_prot_t new_protection
);

#define JJLog(...) // NSLog(__VA_ARGS__)

enum JJ_Search_Type
{
    JJ_Search_Type_Error = 0,
    JJ_Search_Type_Double = 1,
    JJ_Search_Type_ULong  = 2,
    JJ_Search_Type_SLong  = 3,
    JJ_Search_Type_Float  = 4,
    JJ_Search_Type_UInt   = 5,
    JJ_Search_Type_SInt   = 6,
    JJ_Search_Type_UShort = 7,
    JJ_Search_Type_SShort = 8,
    JJ_Search_Type_UByte  = 9,
    JJ_Search_Type_SByte  = 10,
    JJ_Search_Type_Max    = 11
};

enum {
    JJ_Search_Type_CString = 100,
    JJ_Search_Type_UTF16   = 101
};

typedef struct _result_region {
    uint64_t region_base;
    size_t region_size;
    vector<uint32_t> slides;
    vector<int8_t> types;

    _result_region(uint64_t base, size_t size)
    {
        region_base = base;
        region_size = size;
    }
} result_region;

typedef struct _result {
    vector<result_region*> regions;
    size_t count;
} Result;

typedef struct _addrRange {
    uint64_t start;
    uint64_t end;
} AddrRange;


// ============================================================================
// SIMD Scalar Fallback
// ============================================================================
static inline const uint8_t* jj_scalar_search(
    const uint8_t* hay, size_t hayLen,
    const uint8_t* needle, size_t needleLen)
{
    if (needleLen == 0 || hayLen < needleLen)
        return nullptr;

    uint8_t first = needle[0];
    size_t limit = hayLen - needleLen;

    for (size_t i = 0; i <= limit; i++)
    {
        if (hay[i] == first &&
            memcmp(hay + i, needle, needleLen) == 0)
            return hay + i;
    }
    return nullptr;
}


// ============================================================================
// SIMD ASCII Search
// ============================================================================
// ...existing code...
static inline const uint8_t* jj_neon_search_ascii(
    const uint8_t* hay, size_t hayLen,
    const uint8_t* needle, size_t needleLen)
{
    if (needleLen == 0 || hayLen < needleLen)
        return nullptr;

    if (needleLen < 16)
        return jj_scalar_search(hay, hayLen, needle, needleLen);

    const uint8_t first = needle[0];
    uint8x16_t firstVec = vdupq_n_u8(first);

    size_t limit = hayLen - needleLen;
    size_t i = 0;

    while (i + 16 <= hayLen && i <= limit)
    {
        uint8x16_t chunk = vld1q_u8(hay + i);
        uint8x16_t cmp = vceqq_u8(chunk, firstVec);

        uint8_t lanes[16];
        vst1q_u8(lanes, cmp);

        for (int j = 0; j < 16; j++)
        {
            size_t pos = i + (size_t)j;
            if (pos > limit)
                break;

            if (lanes[j] == 0xFF &&
                memcmp(hay + pos, needle, needleLen) == 0)
                return hay + pos;
        }

        i += 16;
    }

    return nullptr;
}



// ============================================================================
// SIMD UTF16 Search
// ============================================================================

static inline const uint8_t* jj_neon_search_utf16(
    const uint8_t* hay, size_t hayLenBytes,
    const uint8_t* needle, size_t needleLenBytes)
{
    if (needleLenBytes == 0 || hayLenBytes < needleLenBytes)
        return nullptr;

    if (needleLenBytes < 4)
        return jj_scalar_search(hay, hayLenBytes, needle, needleLenBytes);

    uint16_t firstCode;
    memcpy(&firstCode, needle, sizeof(uint16_t));

    uint16x8_t firstVec = vdupq_n_u16(firstCode);

    size_t limit = hayLenBytes - needleLenBytes;
    size_t i = 0;

    while (i + 16 <= hayLenBytes && i <= limit)
    {
        uint8x16_t bytes = vld1q_u8(hay + i);
        uint16x8_t chunk = vreinterpretq_u16_u8(bytes);
        uint16x8_t cmp = vceqq_u16(chunk, firstVec);

        uint16_t lanes[8];
        vst1q_u16(lanes, cmp);

        for (int j = 0; j < 8; j++)
        {
            size_t posBytes = i + ((size_t)j * 2);
            if (posBytes > limit)
                break;

            if (lanes[j] == 0xFFFF &&
                memcmp(hay + posBytes, needle, needleLenBytes) == 0)
                return hay + posBytes;
        }

        i += 16;
    }

    return nullptr;
}


// ============================================================================
// SIMD Entry Points
// ============================================================================
static inline uint64_t jj_search_ascii(
    uint64_t buffer, uint64_t size, const char* target)
{
    const uint8_t* hay = (const uint8_t*)buffer;
    const uint8_t* needle = (const uint8_t*)target;
    size_t len = strlen(target);

    auto* p = jj_neon_search_ascii(hay, size, needle, len);
    return p ? (uint64_t)p : 0;
}

static inline uint64_t jj_search_utf16(
    uint64_t buffer, uint64_t sizeBytes,
    const uint16_t* target, size_t lenChars)
{
    const uint8_t* hay = (const uint8_t*)buffer;
    const uint8_t* needle = (const uint8_t*)target;
    size_t needBytes = lenChars * 2;

    auto* p = jj_neon_search_utf16(hay, sizeBytes, needle, needBytes);
    return p ? (uint64_t)p : 0;
}


// ============================================================================
// RAII REGION MAPPER
// ============================================================================
class JJRegionMap
{
public:
    mach_port_t task;
    uint64_t remoteBase = 0;
    uint64_t remoteSize = 0;
    void* localPtr = nullptr;
    bool remapped = false;

    JJRegionMap(mach_port_t t, uint64_t base, uint64_t size)
    : task(t), remoteBase(base), remoteSize(size)
    {
        mapRegion();
    }

    ~JJRegionMap()
    {
        unmapRegion();
    }

    inline bool valid() const { return localPtr != nullptr; }

private:

    void mapRegion()
    {
        if (remoteSize == 0)
            return;

        mach_vm_address_t rBase = remoteBase;
        mach_vm_size_t rSize = remoteSize;
        vm_region_extended_info info = {0};
        mach_msg_type_number_t infoCnt = VM_REGION_EXTENDED_INFO_COUNT;
        mach_port_t object = MACH_PORT_NULL;

        kern_return_t kr = mach_vm_region(
            task, &rBase, &rSize, VM_REGION_EXTENDED_INFO,
            (vm_region_info_t)&info, &infoCnt, &object);

        if (kr != KERN_SUCCESS)
        {
            localPtr = nullptr;
            return;
        }

        if (info.user_tag == VM_MEMORY_MALLOC_NANO)
        {
            localPtr = (void*)remoteBase;
            remapped = false;
            return;
        }

        vm_address_t dest = 0;
        vm_prot_t cur, max;

        kr = vm_remap(
            mach_task_self(), &dest, remoteSize, 0,
            VM_FLAGS_ANYWHERE, task, remoteBase,
            false, &cur, &max, VM_INHERIT_NONE);

        if (kr != KERN_SUCCESS)
        {
            localPtr = nullptr;
            remapped = false;
            return;
        }

        localPtr = (void*)dest;
        remapped = true;
    }

    void unmapRegion()
    {
        if (localPtr && remapped)
        {
            vm_deallocate(mach_task_self(),
                          (vm_address_t)localPtr, remoteSize);
        }
        localPtr = nullptr;
        remapped = false;
    }
};


// ============================================================================
// Region Enumerator
// ============================================================================
struct JJRegionInfo
{
    uint64_t base;
    uint64_t size;
    vm_prot_t protection;
    uint32_t userTag;
};

static inline vector<JJRegionInfo> jj_enumerate_regions(
    mach_port_t task, uint64_t start,
    uint64_t end, uint64_t stackBegin,
    uint64_t stackEnd)
{
    vector<JJRegionInfo> out;

    mach_vm_address_t addr = start;
    mach_vm_size_t rSize = 0;
    natural_t depth = 1;

    while (addr < end)
    {
        addr += rSize;

        vm_region_submap_info_64 info;
        mach_msg_type_number_t infoCnt = VM_REGION_SUBMAP_INFO_COUNT_64;

        kern_return_t kr = mach_vm_region_recurse(
            task, &addr, &rSize, &depth,
            (vm_region_recurse_info_t)&info, &infoCnt);

        if (kr != KERN_SUCCESS)
            break;

        if (info.is_submap)
        {
            rSize = 0;
            depth++;
            continue;
        }

        uint64_t rEnd = addr + rSize;

        if ((stackBegin >= addr && stackBegin < rEnd) ||
            (stackEnd > addr && stackBegin <= rEnd))
        {
            continue;
        }

        if ((info.protection & VM_PROT_WRITE) == 0)
            continue;

        JJRegionInfo ri;
        ri.base = addr;
        ri.size = rSize;
        ri.protection = info.protection;
        ri.userTag = info.user_tag;
        out.push_back(ri);
    }

    return out;
}

// ============================================================================
// Numeric Type Size Helper
// ============================================================================
static inline int jj_type_size(int type)
{
    switch (type)
    {
        case JJ_Search_Type_Double: return 8;
        case JJ_Search_Type_ULong:  return 8;
        case JJ_Search_Type_SLong:  return 8;
        case JJ_Search_Type_Float:  return 4;
        case JJ_Search_Type_UInt:   return 4;
        case JJ_Search_Type_SInt:   return 4;
        case JJ_Search_Type_UShort: return 2;
        case JJ_Search_Type_SShort: return 2;
        case JJ_Search_Type_UByte:  return 1;
        case JJ_Search_Type_SByte:  return 1;
        case JJ_Search_Type_CString: return 1;
        case JJ_Search_Type_UTF16:   return 2;
        default: return 0;
    }
}

static inline size_t jj_utf16_cstr_bytes(const uint16_t *value)
{
    if (!value)
        return 0;

    size_t chars = 0;
    while (value[chars] != 0)
        chars++;

    return (chars + 1) * sizeof(uint16_t);
}

static inline size_t jj_value_size(const void *value, int type)
{
    switch (type)
    {
        case JJ_Search_Type_CString:
            return value ? (strlen((const char *)value) + 1) : 0;
        case JJ_Search_Type_UTF16:
            return jj_utf16_cstr_bytes((const uint16_t *)value);
        default:
            return jj_type_size(type);
    }
}

static inline bool jj_numeric_match(
    uint64_t pos, uint64_t low, uint64_t high,
    void* target, int type, float tol)
{
    if (pos < low || pos + jj_type_size(type) > high)
        return false;

    switch (type)
    {
        case JJ_Search_Type_Float:
        {
            float v = *(float*)pos;
            float t = *(float*)target;
            return (v >= t - tol && v <= t + tol);
        }

        case JJ_Search_Type_Double:
        {
            double v = *(double*)pos;
            double t = *(double*)target;
            return (v >= t - tol && v <= t + tol);
        }

        case JJ_Search_Type_SByte:
            return (*(int8_t*)pos == *(int8_t*)target);

        case JJ_Search_Type_UByte:
            return (*(uint8_t*)pos == *(uint8_t*)target);

        case JJ_Search_Type_SShort:
            return (*(int16_t*)pos == *(int16_t*)target);

        case JJ_Search_Type_UShort:
            return (*(uint16_t*)pos == *(uint16_t*)target);

        case JJ_Search_Type_SInt:
            return (*(int32_t*)pos == *(int32_t*)target);

        case JJ_Search_Type_UInt:
            return (*(uint32_t*)pos == *(uint32_t*)target);

        case JJ_Search_Type_SLong:
            return (*(int64_t*)pos == *(int64_t*)target);

        case JJ_Search_Type_ULong:
            return (*(uint64_t*)pos == *(uint64_t*)target);
    }

    return false;
}

static inline bool jj_string_match(
    uint64_t pos, uint64_t low, uint64_t high,
    void* target, int type)
{
    if (type == JJ_Search_Type_CString)
    {
        size_t len = jj_value_size(target, type);
        if (len == 0 || pos < low || pos + len > high)
            return false;
        return memcmp((const void*)pos, target, len - 1) == 0;
    }

    if (type == JJ_Search_Type_UTF16)
    {
        size_t len = jj_value_size(target, type);
        if (len == 0 || pos < low || pos + len > high)
            return false;
        return memcmp((const void*)pos, target, len - sizeof(uint16_t)) == 0;
    }

    return false;
}


// ============================================================================
// Unified Scanner Class (SIMD + Numeric)
// ============================================================================
class JJScanner
{
public:

    static void ScanRegion(
        result_region*& outRegion,
        const JJRegionMap& map,
        uint64_t globalBase,
        uint64_t regionSize,
        void* target,
        int type,
        float floatTol)
    {
        if (!map.valid())
            return;

        uint64_t buffer = (uint64_t)map.localPtr;
        uint64_t size   = regionSize;

        if (type == JJ_Search_Type_CString)
        {
            const char* needle = (const char*)target;
            size_t needleLen = strlen(needle);

            uint64_t pos = buffer;
            uint64_t end = buffer + size;

            while (pos < end)
            {
                uint64_t found = jj_search_ascii(pos, end - pos, needle);
                if (!found)
                    break;

                uint32_t slide = (uint32_t)(found - buffer);

                if (!outRegion)
                    outRegion = new result_region(globalBase, regionSize);

                outRegion->slides.push_back(slide);
                pos = found + needleLen;
            }
            return;
        }

        if (type == JJ_Search_Type_UTF16)
        {
            const uint16_t* needle = (uint16_t*)target;
            size_t charCount = 0;
            while (needle[charCount] != 0)
                charCount++;

            uint64_t pos = buffer;
            uint64_t end = buffer + size;

            while (pos < end)
            {
                uint64_t found = jj_search_utf16(pos, end - pos, needle, charCount);
                if (!found)
                    break;

                uint32_t slide = (uint32_t)(found - buffer);

                if (!outRegion)
                    outRegion = new result_region(globalBase, regionSize);

                outRegion->slides.push_back(slide);
                pos = found + (charCount * 2);
            }
            return;
        }

        numericSearch(outRegion, buffer, globalBase, size, target, type, floatTol);
    }


private:

    static void numericSearch(
        result_region*& outRegion,
        uint64_t buffer,
        uint64_t base,
        uint64_t size,
        void* target,
        int type,
        float tol)
    {
        int len = jj_type_size(type);
        if (len <= 0)
            return;

        uint64_t pos = buffer;
        uint64_t end = buffer + size - len;

        while (pos <= end)
        {
            bool match = false;

            switch (type)
            {
                case JJ_Search_Type_Float:
                {
                    float v = *(float*)pos;
                    float t = *(float*)target;
                    if (v >= t - tol && v <= t + tol)
                        match = true;
                } break;

                case JJ_Search_Type_Double:
                {
                    double v = *(double*)pos;
                    double t = *(double*)target;
                    if (v >= t - tol && v <= t + tol)
                        match = true;
                } break;

                case JJ_Search_Type_SByte:
                    match = (*(int8_t*)pos == *(int8_t*)target);
                    break;

                case JJ_Search_Type_UByte:
                    match = (*(uint8_t*)pos == *(uint8_t*)target);
                    break;

                case JJ_Search_Type_SShort:
                    match = (*(int16_t*)pos == *(int16_t*)target);
                    break;

                case JJ_Search_Type_UShort:
                    match = (*(uint16_t*)pos == *(uint16_t*)target);
                    break;

                case JJ_Search_Type_SInt:
                    match = (*(int32_t*)pos == *(int32_t*)target);
                    break;

                case JJ_Search_Type_UInt:
                    match = (*(uint32_t*)pos == *(uint32_t*)target);
                    break;

                case JJ_Search_Type_SLong:
                    match = (*(int64_t*)pos == *(int64_t*)target);
                    break;

                case JJ_Search_Type_ULong:
                    match = (*(uint64_t*)pos == *(uint64_t*)target);
                    break;
            }

            if (match)
            {
                uint32_t slide = (uint32_t)(pos - buffer);
                if (!outRegion)
                    outRegion = new result_region(base, size);

                outRegion->slides.push_back(slide);
            }

            pos += len;
        }
    }
};

// ============================================================================
//  JJMemoryEngine — Public API (DROP-IN COMPATIBLE)
// ============================================================================

class JJMemoryEngine
{
public:
    mach_port_t task;
    Result* result;
    bool firstScanDone;
    float float_tolerance;
    int lastNumberType;

    JJMemoryEngine(mach_port_t t)
    : task(t)
    {
        result = new Result;
        result->count = 0;
        firstScanDone = false;
        float_tolerance = 0.0f;
        lastNumberType = JJ_Search_Type_Error;
    }

    ~JJMemoryEngine()
    {
        freeResults();
    }

    void SetFloatTolerance(float f)
    {
        float_tolerance = f;
    }


    // ------------------------------------------------------------------------
    // Core Scanning API (unchanged signatures)
    // ------------------------------------------------------------------------
    void JJScanMemory(AddrRange range, void* target, int type);


    void JJNearBySearch(size_t range, void* target, int type);



    // ------------------------------------------------------------------------
    // Read/Write API (unchanged)
    // ------------------------------------------------------------------------
    bool JJReadMemory(void* buf, uint64_t addr, int type)
    {
        size_t len = jj_type_size(type);
        if (len <= 0)
            return false;

        vm_size_t size = 0;
        kern_return_t kr = vm_read_overwrite(task, addr, len,
                                             (vm_address_t)buf, &size);
        return (kr == KERN_SUCCESS && size == len);
    }

    bool JJWriteMemory(void* address, void* src, int type)
    {
        size_t len = jj_value_size(src, type);
        if (len <= 0)
            return false;

        vm_region_basic_info_data_64_t info = {0};
        mach_msg_type_number_t infoCnt = VM_REGION_BASIC_INFO_COUNT_64;
        mach_vm_size_t rSize = 0;
        mach_vm_address_t rBase = (uint64_t)address;
        mach_port_t object;

        kern_return_t kr = mach_vm_region(task, &rBase, &rSize,
                                          VM_REGION_BASIC_INFO_64,
                                          (vm_region_info_t)&info,
                                          &infoCnt, &object);

        if (kr != KERN_SUCCESS)
            return false;

        mach_vm_address_t protectBase = 0;
        mach_vm_size_t protectSize = 0;

        if (!(info.protection & VM_PROT_WRITE))
        {
            uint64_t startAddr = (uint64_t)address;
            uint64_t endAddr = startAddr + len - 1;
            protectBase = startAddr & ~((uint64_t)PAGE_MASK);
            uint64_t protectEnd = (endAddr & ~((uint64_t)PAGE_MASK)) + PAGE_SIZE;
            protectSize = protectEnd - protectBase;

            kr = mach_vm_protect(task, protectBase, protectSize, false,
                                 info.protection |
                                 VM_PROT_WRITE |
                                 VM_PROT_COPY);

            if (kr != KERN_SUCCESS)
            {
                kr = mach_vm_protect(task, protectBase, protectSize, false,
                                     VM_PROT_READ |
                                     VM_PROT_WRITE |
                                     VM_PROT_COPY);
                if (kr != KERN_SUCCESS)
                    return false;
            }
        }

        bool ok = (vm_write(task, (vm_address_t)address,
                            (vm_offset_t)src, len) == KERN_SUCCESS);

        if (!ok && protectBase)
        {
            mach_vm_protect(task, protectBase, protectSize, false,
                            VM_PROT_READ | VM_PROT_WRITE | VM_PROT_COPY);

            ok = (vm_write(task, (vm_address_t)address,
                           (vm_offset_t)src, len) == KERN_SUCCESS);
        }

        if (protectBase)
            mach_vm_protect(task, protectBase, protectSize, false, info.protection);

        return ok;
    }


    int JJWriteAll(void* src, int type)
    {
        size_t len = jj_value_size(src, type);
        if (len <= 0)
            return 0;

        int count = 0;
        for (auto* region : result->regions)
        {
            if (!region)
                continue;

            for (uint32_t slide : region->slides)
            {
                uint64_t addr = region->region_base + slide;
                if (JJWriteMemory((void*)addr, src, type))
                    count++;
            }
        }
        return count;
    }


    // ------------------------------------------------------------------------
    // Results API (unchanged)
    // ------------------------------------------------------------------------
    size_t getResultsCount()
    {
        return result->count;
    }

    vector<void*> getResults(size_t count, size_t skip = 0)
    {
        vector<void*> out;
        int idx = 0;

        for (auto* region : result->regions)
        {
            if (!region) continue;

            for (uint32_t slide : region->slides)
            {
                if (idx >= skip && (idx - skip) < count)
                    out.push_back((void*)(region->region_base + slide));
                idx++;
            }
        }

        return out;
    }

    map<void*, int8_t> getResultsAndTypes(int count, int skip = 0)
    {
        map<void*, int8_t> out;
        int idx = 0;

        for (auto* region : result->regions)
        {
            if (!region) continue;

            bool hasTypes = !region->types.empty();

            for (int i = 0; i < region->slides.size(); i++)
            {
                if (idx >= skip && (idx - skip) < count)
                {
                    uint64_t addr = region->region_base + region->slides[i];
                    out[(void*)addr] = hasTypes ? region->types[i] : 0;
                }
                idx++;
            }
        }

        return out;
    }


private:

    // ------------------------------------------------------------------------
    // Free all results
    // ------------------------------------------------------------------------
    void freeResults()
    {
        if (!result)
            return;

        for (auto* region : result->regions)
        {
            if (!region) continue;
            delete region;
        }

        result->regions.clear();
        delete result;
        result = nullptr;
    }
};


// ============================================================================
// Orchestrator: FirstScan / ScanAgain
// ============================================================================
class JJScanOrchestrator
{
public:

    static void FirstScan(
        class JJMemoryEngine* engine,
        AddrRange range,
        void* target,
        int type)
    {
        engine->result->count = 0;
        engine->result->regions.clear();

        uint64_t stackSize = pthread_get_stacksize_np(pthread_self());
        uint64_t stackAddr = (uint64_t)pthread_get_stackaddr_np(pthread_self());
        uint64_t stackBase = stackAddr > stackSize ? (stackAddr - stackSize) : 0;

        auto regions = jj_enumerate_regions(
            engine->task,
            range.start,
            range.end,
            stackBase,
            stackAddr);

        for (auto& r : regions)
        {
            JJRegionMap map(engine->task, r.base, r.size);
            if (!map.valid())
                continue;

            result_region* newRegion = nullptr;

            JJScanner::ScanRegion(
                newRegion,
                map,
                r.base,
                r.size,
                target,
                type,
                engine->float_tolerance);

            if (newRegion)
            {
                newRegion->slides.shrink_to_fit();
                engine->result->regions.push_back(newRegion);
                engine->result->count += newRegion->slides.size();
            }
        }

        engine->result->regions.shrink_to_fit();
    }


    static void ScanAgain(
        class JJMemoryEngine* engine,
        AddrRange range,
        void* target,
        int type)
    {
        size_t newCount = 0;

        for (int i = 0; i < engine->result->regions.size(); i++)
        {
            result_region* old = engine->result->regions[i];

            if (!old)
                continue;

            JJRegionMap map(engine->task, old->region_base, old->region_size);
            if (!map.valid())
            {
                delete old;
                engine->result->regions[i] = nullptr;
                continue;
            }

            result_region* newRegion = nullptr;
            if (old->slides.empty())
            {
                delete old;
                engine->result->regions[i] = nullptr;
                continue;
            }

            for (int slideIndex = 0; slideIndex < old->slides.size(); slideIndex++)
            {
                uint32_t slide = old->slides[slideIndex];
                uint64_t addr = old->region_base + slide;

                if (addr < range.start || addr >= range.end)
                    continue;

                bool match = false;
                if (type == JJ_Search_Type_CString || type == JJ_Search_Type_UTF16)
                {
                    match = jj_string_match((uint64_t)map.localPtr + slide,
                                            (uint64_t)map.localPtr,
                                            (uint64_t)map.localPtr + old->region_size,
                                            target,
                                            type);
                }
                else
                {
                    match = jj_numeric_match((uint64_t)map.localPtr + slide,
                                             (uint64_t)map.localPtr,
                                             (uint64_t)map.localPtr + old->region_size,
                                             target,
                                             type,
                                             engine->float_tolerance);
                }

                if (!match)
                    continue;

                if (!newRegion)
                    newRegion = new result_region(old->region_base, old->region_size);

                newRegion->slides.push_back(slide);
                if (!old->types.empty() && slideIndex < old->types.size())
                    newRegion->types.push_back(old->types[slideIndex]);
            }

            delete old;
            engine->result->regions[i] = newRegion;

            if (newRegion)
                newCount += newRegion->slides.size();
        }

        engine->result->regions.erase(
            remove(engine->result->regions.begin(),
                   engine->result->regions.end(),
                   (result_region*)nullptr),
            engine->result->regions.end());

        engine->result->regions.shrink_to_fit();
        engine->result->count = newCount;
    }
};

    void JJMemoryEngine::JJScanMemory(AddrRange range, void* target, int type)
    {
        if (type <= JJ_Search_Type_Error)
            return;

        lastNumberType = type;

        if (firstScanDone)
        {
            JJScanOrchestrator::ScanAgain(this, range, target, type);
        }
        else
        {
            JJScanOrchestrator::FirstScan(this, range, target, type);
            firstScanDone = true;
        }
    }

    void JJMemoryEngine::JJNearBySearch(size_t range, void* target, int type)
    {
        if (type <= JJ_Search_Type_Error)
            return;

        int len = jj_type_size(type);
        if (len <= 0)
            return;

        size_t newCount = 0;
        range -= (range % len);
        range += len;

        for (int i = 0; i < result->regions.size(); i++)
        {
            result_region* region = result->regions[i];
            if (!region)
                continue;

            bool hasTypes = !region->types.empty();
            bool needType = hasTypes || type != lastNumberType;

            result_region* newRegion = nullptr;

            JJRegionMap map(task, region->region_base, region->region_size);
            if (!map.valid())
            {
                delete region;
                result->regions[i] = nullptr;
                continue;
            }

            uint64_t buf = (uint64_t)map.localPtr;

            long lastpos = 0;
            int lastold = 0;

            for (int j = 0; j < region->slides.size(); j++)
            {
                uint32_t curslide = region->slides[j];
                long start = curslide - range;
                long end   = curslide + range;

                if (start < 0) start = 0;
                if (end > region->region_size) end = region->region_size;
                if (lastpos > start) start = lastpos;
                lastpos = end;

                uint64_t data = buf + start;
                size_t sz = end - start;

                size_t foundcount = 0;
                uint32_t foundfirst = 0;
                uint32_t foundlast = 0;

                uint64_t pos = data;
                uint64_t limit = data + sz;

                while (pos < limit)
                {
                    uint64_t found = 0;

                    if (type == JJ_Search_Type_CString)
                    {
                        const char* needle = (const char*)target;
                        found = jj_search_ascii(pos, limit - pos, needle);
                    }
                    else if (type == JJ_Search_Type_UTF16)
                    {
                        const uint16_t* n = (const uint16_t*)target;
                        size_t chars = 0;
                        while (n[chars] != 0)
                            chars++;
                        found = jj_search_utf16(pos, limit - pos, n, chars);
                    }
                    else
                    {
                        if (jj_numeric_match(pos, data, limit, target, type, float_tolerance))
                            found = pos;
                    }

                    if (!found)
                        break;

                    uint32_t slide = (uint32_t)(found - buf);

                    if (foundcount == 0)
                        foundfirst = slide;
                    foundlast = slide;
                    foundcount++;

                    if (!newRegion)
                        newRegion = new result_region(region->region_base, region->region_size);

                    lastpos = end;
                    pos = found + len;
                }

                if (foundcount)
                {
                    for (int o = lastold; o < region->slides.size(); o++)
                    {
                        uint32_t oldslide = region->slides[o];

                        long fd0 = (foundfirst - range);
                        long fu0 = (foundfirst + range);
                        long fd1 = (foundlast - range);
                        long fu1 = (foundlast + range);

                        if ((oldslide > fd0 && oldslide < fu0) ||
                            (oldslide > fd1 && oldslide < fu1))
                        {
                            if (!newRegion)
                                newRegion = new result_region(region->region_base, region->region_size);

                            newRegion->slides.push_back(oldslide);

                            if (needType)
                            {
                                if (hasTypes)
                                    newRegion->types.push_back(region->types[o]);
                                else
                                    newRegion->types.push_back(lastNumberType);
                            }

                            lastold = o + 1;
                        }
                    }
                }
            }

            delete region;
            result->regions[i] = newRegion;
            if (newRegion)
            {
                newRegion->slides.shrink_to_fit();
                newRegion->types.shrink_to_fit();
                newCount += newRegion->slides.size();
            }
        }

        result->regions.erase(
            remove(result->regions.begin(), result->regions.end(), (result_region*)nullptr),
            result->regions.end());

        result->regions.shrink_to_fit();
        result->count = newCount;
    }

#endif /* JJ_Header_h */
