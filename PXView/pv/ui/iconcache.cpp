#include "iconcache.h"
#include <QSvgRenderer>
#include <QPainter>

IconCache::IconCache()
    : _defaultSize(16, 16)
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

QPixmap IconCache::pixmap(const QString &svgPath, const QSize &size)
{
    QString key = svgPath + QString("@%1x%2").arg(size.width()).arg(size.height());

    auto it = _cache.find(key);
    if (it != _cache.end())
        return it.value();

    QSvgRenderer renderer(svgPath);
    if (!renderer.isValid())
        return QPixmap();

    QSize renderSize = size.isValid() ? size : _defaultSize;
    QPixmap pm(renderSize);
    pm.fill(Qt::transparent);

    QPainter painter(&pm);
    renderer.render(&painter);
    painter.end();

    _cache.insert(key, pm);
    return pm;
}

QIcon IconCache::icon(const QString &svgPath, const QSize &size)
{
    return QIcon(pixmap(svgPath, size));
}

void IconCache::clearCache()
{
    _cache.clear();
}
