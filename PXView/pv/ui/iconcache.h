#pragma once

#include <QHash>
#include <QIcon>
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

    QIcon icon(const QString &svgPath);
    QPixmap pixmap(const QString &svgPath, const QSize &size = QSize(16, 16));

    void clearCache();

private:
    QHash<QString, QIcon> _iconCache;
};
