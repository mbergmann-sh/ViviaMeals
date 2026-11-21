// ViviaMeals Mobil – Rezeptfotos (siehe recipeimageprovider.h)
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "recipeimageprovider.h"

#include "recipe.h"
#include "recipedb.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

RecipeImageProvider::RecipeImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

QImage RecipeImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    // nur das Bildfeld lesen; RecipeDb::fileFor ist eine reine Pfadberechnung
    const QString recipeId = id.section(QLatin1Char('?'), 0, 0);
    QFile f(RecipeDb::instance().fileFor(recipeId));
    QImage img;
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        const QByteArray data = QByteArray::fromBase64(o.value(QStringLiteral("image")).toString().toLatin1());
        img.loadFromData(data);
    }
    if (!img.isNull() && requestedSize.isValid() && (requestedSize.width() > 0 || requestedSize.height() > 0)) {
        const QSize target(requestedSize.width() > 0 ? requestedSize.width() : img.width() * requestedSize.height() / qMax(1, img.height()),
                           requestedSize.height() > 0 ? requestedSize.height() : img.height() * requestedSize.width() / qMax(1, img.width()));
        if (target.width() < img.width()) img = img.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    if (size) *size = img.size();
    return img;
}
