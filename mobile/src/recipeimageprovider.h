// ViviaMeals Mobil – Rezeptfotos für QML („image://recipe/<id>“), in einem Hintergrund-Thread geladen
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include <QQuickImageProvider>

class RecipeImageProvider : public QQuickImageProvider
{
public:
    RecipeImageProvider();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};
