#include "iconcache.h"

IconCache::IconCache()
{
}

IconCache::~IconCache()
{
}

IconCache &IconCache::Instance()
{
    static IconCache *ins = nullptr;
    if (!ins)
        ins = new IconCache();
    return *ins;
}

QIcon IconCache::icon(const QString &svgPath)
{
    auto it = _iconCache.find(svgPath);
    if (it != _iconCache.end())
        return it.value();

    QIcon ic(svgPath);
    _iconCache.insert(svgPath, ic);
    return ic;
}

QPixmap IconCache::pixmap(const QString &svgPath, const QSize &size)
{
    return icon(svgPath).pixmap(size);
}

void IconCache::clearCache()
{
    _iconCache.clear();
}
