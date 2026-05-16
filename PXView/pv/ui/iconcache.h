#pragma once

#include <QHash>
#include <QIcon>
#include <QPixmap>
#include <QString>

class IconCache
{
private:
    IconCache();
    ~IconCache();
    IconCache(const IconCache &) = delete;
    IconCache &operator=(const IconCache &) = delete;

public:
    static IconCache &Instance();

    QPixmap pixmap(const QString &svgPath, const QSize &size = QSize(16, 16));
    QIcon icon(const QString &svgPath, const QSize &size = QSize(16, 16));

    void clearCache();

private:
    QHash<QString, QPixmap> _cache;
    QSize _defaultSize;
};
