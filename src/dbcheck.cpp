// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "dbcheck.h"

#include "recipe.h"
#include "recipedb.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QLocale>
#include <QPainter>

#include <algorithm>

namespace DbCheck {

namespace {

QString tr(const char *s)
{
    return QCoreApplication::translate("DbCheck", s);
}

bool isEmptyRecipe(const Recipe &r)
{
    return r.title.trimmed().isEmpty() && r.ingredients.isEmpty() && r.instructions.trimmed().isEmpty();
}

// zum Verschieben: Ordner „aussortiert/JJJJ-MM-TT-hhmmss“ (einmal je Durchgang)
QString sortedFolder()
{
    const QString base = QDir(RecipeDb::instance().folder()).filePath(QStringLiteral("aussortiert"));
    const QString dir = QDir(base).filePath(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd-hhmmss")));
    QDir().mkpath(dir);
    return dir;
}

bool moveInto(const QString &file, const QString &dir, QString *error)
{
    QString target = QDir(dir).filePath(QFileInfo(file).fileName());
    for (int n = 2; QFile::exists(target); ++n)
        target = QDir(dir).filePath(QStringLiteral("%1-%2.%3").arg(QFileInfo(file).completeBaseName()).arg(n).arg(QFileInfo(file).suffix()));
    if (QFile::rename(file, target)) return true;
    // über Laufwerksgrenzen hinweg (sollte im selben Ordner nicht vorkommen): kopieren und löschen
    if (QFile::copy(file, target) && QFile::remove(file)) return true;
    if (error) *error = tr("„%1“ ließ sich nicht verschieben.").arg(QFileInfo(file).fileName());
    return false;
}

} // namespace

bool Report::clean() const
{
    return corrupt.isEmpty() && strays.isEmpty() && empty.isEmpty() && brokenImages.isEmpty() && bigImages.isEmpty();
}

QString sizeText(qint64 bytes)
{
    const QLocale de(QLocale::German, QLocale::Germany);
    if (bytes >= 1024LL * 1024 * 1024) return de.toString(bytes / (1024.0 * 1024 * 1024), 'f', 1) + QStringLiteral(" GB");
    if (bytes >= 1024LL * 1024) return de.toString(bytes / (1024.0 * 1024), 'f', 1) + QStringLiteral(" MB");
    if (bytes >= 1024) return de.toString(bytes / 1024.0, 'f', 0) + QStringLiteral(" KB");
    return de.toString(bytes) + QStringLiteral(" Bytes");
}

Report analyze(const Progress &progress)
{
    Report rep;
    RecipeDb &db = RecipeDb::instance();
    if (!db.isOpen()) return rep;
    const QDir dir(db.recipesFolder());
    const QFileInfoList all = dir.entryInfoList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDir::Name);
    const int total = int(all.size());
    int done = 0;
    for (const QFileInfo &fi : all) {
        if (progress && !progress(done, total)) {
            rep.cancelled = true;
            return rep;
        }
        ++done;
        if (fi.suffix().compare(QLatin1String("vmr"), Qt::CaseInsensitive) != 0) {
            rep.strays << fi.fileName();
            continue;
        }
        ++rep.files;
        rep.totalBytes += fi.size();
        Recipe r;
        QString err;
        if (fi.size() == 0 || !Recipe::load(fi.absoluteFilePath(), &r, &err)) {
            rep.corrupt << fi.fileName();
            rep.corruptReasons << (fi.size() == 0 ? tr("Datei ist leer") : err.isEmpty() ? tr("nicht lesbar") : err);
            continue;
        }
        const QString id = fi.completeBaseName();
        if (isEmptyRecipe(r)) {
            rep.empty << id;
            continue;
        }
        if (r.image.isEmpty()) continue;
        const QByteArray data = QByteArray::fromBase64(r.image.toLatin1());
        QBuffer buf;
        buf.setData(data);
        buf.open(QIODevice::ReadOnly);
        QImageReader reader(&buf);
        const QSize size = reader.size();   // nur den Kopf lesen, nicht das ganze Bild
        if (data.isEmpty() || !reader.canRead() || !size.isValid() || size.isEmpty()) {
            rep.brokenImages << id;
            continue;
        }
        if (std::max(size.width(), size.height()) > kMaxSide || data.size() > kMaxBytes) {
            rep.bigImages << BigImage{ id, r.title, size, data.size() };
            rep.bigImageBytes += data.size();
        }
    }
    if (progress) progress(total, total);
    return rep;
}

bool shrinkImage(QString *base64, qint64 *savedBytes)
{
    if (!base64 || base64->isEmpty()) return false;
    const QByteArray data = QByteArray::fromBase64(base64->toLatin1());
    QImage img;
    if (!img.loadFromData(data)) return false;
    if (std::max(img.width(), img.height()) > kTargetSide)
        img = img.scaled(kTargetSide, kTargetSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QByteArray out;
    QBuffer buf(&out);
    buf.open(QIODevice::WriteOnly);
    // Transparenz bleibt nicht erhalten: auf Weiß setzen statt Schwarz
    if (img.hasAlphaChannel()) {
        QImage white(img.size(), QImage::Format_RGB32);
        white.fill(Qt::white);
        QPainter painter(&white);
        painter.drawImage(0, 0, img);
        painter.end();
        img = white;
    }
    if (!img.convertToFormat(QImage::Format_RGB32).save(&buf, "JPEG", 85) || out.size() >= data.size()) return false;
    if (savedBytes) *savedBytes = data.size() - out.size();
    *base64 = QString::fromLatin1(out.toBase64());
    return true;
}

Result apply(const Report &report, const Actions &actions, const Progress &progress)
{
    Result res;
    RecipeDb &db = RecipeDb::instance();
    if (!db.isOpen()) return res;
    const int total = (actions.shrinkImages ? int(report.bigImages.size()) : 0) + (actions.removeBrokenImages ? int(report.brokenImages.size()) : 0)
                      + (actions.moveCorrupt ? int(report.corrupt.size()) : 0) + (actions.moveStrays ? int(report.strays.size()) : 0)
                      + (actions.moveEmpty ? int(report.empty.size()) : 0) + 1;
    int done = 0;
    auto step = [&]() { return !progress || progress(++done, total); };
    db.beginBatch();
    if (actions.shrinkImages) {
        for (const BigImage &b : report.bigImages) {
            Recipe r;
            qint64 saved = 0;
            if (db.load(b.id, &r) && shrinkImage(&r.image, &saved)) {
                QString err;
                if (db.save(r, &err)) {
                    ++res.shrunk;
                    res.savedBytes += saved;
                } else {
                    res.errors << err;
                }
            }
            if (!step()) break;
        }
    }
    if (actions.removeBrokenImages) {
        for (const QString &id : report.brokenImages) {
            Recipe r;
            if (db.load(id, &r)) {
                r.image.clear();
                QString err;
                if (db.save(r, &err)) ++res.imagesRemoved;
                else res.errors << err;
            }
            if (!step()) break;
        }
    }
    db.endBatch();
    QStringList toMove;
    if (actions.moveCorrupt) toMove << report.corrupt;
    if (actions.moveStrays) toMove << report.strays;
    if (actions.moveEmpty)
        for (const QString &id : report.empty) toMove << id + QStringLiteral(".vmr");
    if (!toMove.isEmpty()) {
        res.sortedFolder = sortedFolder();
        const QDir dir(db.recipesFolder());
        for (const QString &f : toMove) {
            QString err;
            if (moveInto(dir.filePath(f), res.sortedFolder, &err)) ++res.moved;
            else res.errors << err;
            if (!step()) break;
        }
    }
    db.reindex();
    if (progress) progress(total, total);
    return res;
}

} // namespace DbCheck
