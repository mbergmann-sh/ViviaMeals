// VitaMeals – Gewichtsverlauf als Grafik (eigene QPainter-Zeichnung wie in VitaBreathe, kein Qt Charts)
// SPDX-License-Identifier: MIT
#pragma once

#include "profiles.h"

#include <QWidget>

class WeightChart : public QWidget
{
    Q_OBJECT
public:
    explicit WeightChart(QWidget *parent = nullptr);
    void setProfile(const Profile &p);
    QSize sizeHint() const override { return { 520, 260 }; }
    QSize minimumSizeHint() const override { return { 260, 160 }; }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    Profile m_p;
};
