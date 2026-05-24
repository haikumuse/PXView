#include "iconcache.h"
#include <QPainter>
#include <QColor>
#include <QIconEngine>

class TintedIconEngine : public QIconEngine {
public:
    TintedIconEngine(const QString &svgPath, const QColor &color)
        : _baseIcon(svgPath), _color(color), _svgPath(svgPath) {}

    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state) override {
        QPixmap pix = pixmap(rect.size(), mode, state);
        painter->drawPixmap(rect, pix);
    }

    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override {
        QPixmap pix = _baseIcon.pixmap(size, mode, state);
        if (!pix.isNull() && _color.isValid()) {
            QPainter p(&pix);
            p.setCompositionMode(QPainter::CompositionMode_SourceIn);
            p.fillRect(pix.rect(), _color);
            p.end();
        }
        return pix;
    }

    QIconEngine *clone() const override {
        return new TintedIconEngine(_svgPath, _color);
    }
    
    QSize actualSize(const QSize &size, QIcon::Mode mode, QIcon::State state) override {
        return _baseIcon.actualSize(size, mode, state);
    }

private:
    QIcon _baseIcon;
    QColor _color;
    QString _svgPath;
};

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

QIcon IconCache::tintedIcon(const QString &svgPath, const QColor &color, const QSize &size)
{
    Q_UNUSED(size);
    QString key = svgPath + "_" + color.name();
    auto it = _iconCache.find(key);
    if (it != _iconCache.end())
        return it.value();

    QIcon ic(svgPath);
    if (ic.isNull()) {
        _iconCache.insert(key, ic);
        return ic;
    }

    QIcon tinted(new TintedIconEngine(svgPath, color));
    _iconCache.insert(key, tinted);
    return tinted;
}

QPixmap IconCache::pixmap(const QString &svgPath, const QSize &size)
{
    return icon(svgPath).pixmap(size);
}

void IconCache::clearCache()
{
    _iconCache.clear();
}
