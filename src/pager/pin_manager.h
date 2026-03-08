#ifndef LILIUMDB_PIN_MANAGER_H
#define LILIUMDB_PIN_MANAGER_H

#include "common/types.h"

namespace LiliumDB {

class PinManager {
public:
    virtual ~PinManager() = default;

    virtual void markDirty(PageNum pageNum) noexcept = 0;
    virtual void pinPage(PageNum pageNum) noexcept = 0;
    virtual void unpinPage(PageNum pageNum) noexcept = 0;
};

} // namespace LiliumDB

#endif