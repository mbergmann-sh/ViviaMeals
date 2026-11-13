// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "icons.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QWidget>
#include <QFile>
#include <QHash>
#include <QIconEngine>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QSvgRenderer>

namespace Icons {

namespace {
QHash<QString, QIcon> &cache()
{
    static QHash<QString, QIcon> c;
    return c;
}
QHash<qint64, QString> &names()      // cacheKey eines ausgegebenen Symbols → Name
{
    static QHash<qint64, QString> n;
    return n;
}

QPixmap render(const QByteArray &svg, int size, qreal dpr)
{
    QSvgRenderer r(svg);
    QPixmap pm(int(size * dpr), int(size * dpr));
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    r.render(&p);
    p.end();
    pm.setDevicePixelRatio(dpr);
    return pm;
}
} // namespace

QColor accent() { return QColor(0xE0, 0x62, 0x0D); }
QColor primary() { return QColor(0x14, 0x74, 0xBC); }

QIcon app()
{
    QIcon i;
    i.addFile(QStringLiteral(":/icons/viviameals.png"));
    return i;
}

namespace {
// Symbol-Engine: rendert das SVG erst, wenn eine Größe tatsächlich gebraucht wird, und merkt sich das Ergebnis.
// (Früher wurden je Symbol 20 Pixmaps im Voraus gerendert – beim Programmstart rund 40 % der Rechenzeit.)
class SvgIconEngine : public QIconEngine
{
public:
    SvgIconEngine(const QByteArray &normal, const QByteArray &disabled) : m_normal(normal), m_disabled(disabled) {}
    SvgIconEngine(const SvgIconEngine &o) : QIconEngine(o), m_normal(o.m_normal), m_disabled(o.m_disabled), m_cache(o.m_cache) {}

    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override { return scaledPixmap(size, mode, state, 1.0); }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State, qreal scale) override
#else
    QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State, qreal scale)
#endif
    {
        const int side = qMax(1, qMin(size.width(), size.height()));
        const bool off = mode == QIcon::Disabled;
        const quint64 key = (quint64(side) << 32) | (quint64(qRound(scale * 100)) << 1) | (off ? 1 : 0);
        auto it = m_cache.constFind(key);
        if (it != m_cache.constEnd()) return *it;
        QPixmap pm = render(off ? m_disabled : m_normal, side, scale);
        m_cache.insert(key, pm);
        return pm;
    }
    void paint(QPainter *p, const QRect &rect, QIcon::Mode mode, QIcon::State state) override
    {
        const qreal dpr = p->device() ? p->device()->devicePixelRatioF() : 1.0;
        p->drawPixmap(rect, scaledPixmap(rect.size(), mode, state, dpr));
    }
    QSize actualSize(const QSize &size, QIcon::Mode, QIcon::State) override
    {
        const int side = qMin(size.width(), size.height());
        return QSize(side, side);
    }
    QList<QSize> availableSizes(QIcon::Mode, QIcon::State) override { return { { 16, 16 }, { 20, 20 }, { 24, 24 }, { 32, 32 }, { 48, 48 } }; }
    QIconEngine *clone() const override { return new SvgIconEngine(*this); }
    QString key() const override { return QStringLiteral("ViviaMealsSvg"); }
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    void virtual_hook(int id, void *data) override
    {
        if (id == QIconEngine::ScaledPixmapHook) {
            auto *arg = static_cast<QIconEngine::ScaledPixmapArgument *>(data);
            arg->pixmap = scaledPixmap(arg->size, arg->mode, arg->state, arg->scale);
            return;
        }
        QIconEngine::virtual_hook(id, data);
    }
#endif

private:
    QByteArray m_normal, m_disabled;
    QHash<quint64, QPixmap> m_cache;
};
} // namespace

QIcon get(const QString &name)
{
    auto it = cache().constFind(name);
    if (it != cache().constEnd()) return *it;
    QFile f(QStringLiteral(":/icons/ui/%1.svg").arg(name));
    if (!f.open(QIODevice::ReadOnly)) return {};
    const QByteArray src = f.readAll();
    const QPalette pal = QApplication::palette();
    const QColor fg = pal.color(QPalette::WindowText);
    const QColor disabled = pal.color(QPalette::Disabled, QPalette::WindowText);
    // Akzent auf dunklem Hintergrund etwas heller
    const QColor acc = pal.color(QPalette::Window).lightness() < 110 ? QColor(0xF5, 0x8A, 0x3C) : accent();
    auto colored = [&](const QColor &c, const QColor &a) {
        QByteArray s = src;
        s.replace("#COLOR", c.name().toLatin1());
        s.replace("#ACCENT", a.name().toLatin1());
        return s;
    };
    const QIcon icon(new SvgIconEngine(colored(fg, acc), colored(disabled, disabled)));
    cache().insert(name, icon);
    names().insert(icon.cacheKey(), name);
    return icon;
}

QPixmap illustration(const QString &name, const QSize &size, qreal dpr)
{
    QFile f(QStringLiteral(":/illustrations/%1.svg").arg(name));
    if (!f.open(QIODevice::ReadOnly)) return {};
    QByteArray svg = f.readAll();
    const QPalette pal = QApplication::palette();
    const bool dark = pal.color(QPalette::Window).lightness() < 110;
    svg.replace("#COLOR", pal.color(QPalette::WindowText).name().toLatin1());
    svg.replace("#ACCENT", dark ? QByteArray("#f58a3c") : accent().name().toLatin1());
    svg.replace("#BLUE", dark ? QByteArray("#4a9ee0") : primary().name().toLatin1());
    svg.replace("#GREEN", dark ? QByteArray("#4cc28a") : QByteArray("#2e9e6b"));
    QSvgRenderer r(svg);
    QSize target = r.defaultSize();
    target.scale(size, Qt::KeepAspectRatio);
    QPixmap pm(int(target.width() * dpr), int(target.height() * dpr));
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    r.render(&p);
    p.end();
    pm.setDevicePixelRatio(dpr);
    return pm;
}

void clearCache()
{
    cache().clear();
}

void refreshWidgets()
{
    const QHash<qint64, QString> old = names();
    clearCache();
    names().clear();
    for (QWidget *top : QApplication::topLevelWidgets()) {
        for (QAbstractButton *b : top->findChildren<QAbstractButton *>()) {
            const QString n = old.value(b->icon().cacheKey());
            if (!n.isEmpty()) b->setIcon(get(n));
        }
        for (QAction *a : top->findChildren<QAction *>()) {
            const QString n = old.value(a->icon().cacheKey());
            if (!n.isEmpty()) a->setIcon(get(n));
        }
    }
}

} // namespace Icons
