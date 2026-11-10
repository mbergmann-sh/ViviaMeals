// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "recipeview.h"
#include "icons.h"
#include "recipehtml.h"
#include "themes.h"
#include "uiscale.h"

#include <QApplication>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QScrollBar>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
const QUrl kImageUrl(QStringLiteral("viviameals://rezeptbild.jpg"));

RecipeHtml::Style styleFromPalette(int imageWidth)
{
    RecipeHtml::Style st;
    const QPalette pal = QApplication::palette();
    const bool dark = pal.color(QPalette::Window).lightness() < 110;
    st.text = pal.color(QPalette::Text);
    st.muted = dark ? QColor(0x9a, 0x9a, 0x9a) : QColor(0x6b, 0x6b, 0x6b);
    st.primary = dark ? QColor(0x4a, 0x9e, 0xe0) : Icons::primary();
    st.accent = dark ? QColor(0xF5, 0x8A, 0x3C) : Icons::accent();
    st.border = dark ? QColor(0x55, 0x55, 0x55) : QColor(0xd0, 0xd6, 0xde);
    st.headBg = dark ? QColor(0x2d, 0x33, 0x3a) : QColor(0xee, 0xf3, 0xf8);
    st.imageWidth = imageWidth;
    st.fontScale = UiScale::factor();
    return st;
}
}

RecipeHtml::Style RecipeView::paletteStyle(int imageWidth)
{
    return styleFromPalette(imageWidth);
}

RecipeView::RecipeView(QWidget *parent) : QWidget(parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(4);
    auto *bar = new QHBoxLayout;
    bar->setContentsMargins(6, 4, 6, 0);
    m_servLabel = new QLabel(tr("Portionen:"));
    bar->addWidget(m_servLabel);
    m_servings = new QDoubleSpinBox;
    m_servings->setDecimals(1);
    m_servings->setRange(0.5, 500);
    m_servings->setSingleStep(1);
    m_servings->setToolTip(tr("Mengen für eine andere Portionenzahl umrechnen"));
    bar->addWidget(m_servings);
    m_reset = new QToolButton;
    m_reset->setIcon(Icons::get(QStringLiteral("refresh")));
    m_reset->setToolTip(tr("Portionen des Rezepts"));
    m_reset->setAutoRaise(true);
    bar->addWidget(m_reset);
    bar->addStretch(1);
    m_fav = new QToolButton;
    m_fav->setCheckable(true);
    m_fav->setAutoRaise(true);
    m_fav->setIcon(Icons::get(QStringLiteral("star")));
    m_fav->setToolTip(tr("Favorit"));
    bar->addWidget(m_fav);
    auto *edit = new QToolButton;
    edit->setIcon(Icons::get(QStringLiteral("edit")));
    edit->setText(tr("Bearbeiten"));
    edit->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    edit->setAutoRaise(true);
    bar->addWidget(edit);
    lay->addLayout(bar);

    m_browser = new QTextBrowser;
    m_browser->setOpenExternalLinks(true);
    m_browser->setFrameShape(QFrame::StyledPanel);
    lay->addWidget(m_browser, 1);

    m_resizeTimer = new QTimer(this);
    m_resizeTimer->setSingleShot(true);
    m_resizeTimer->setInterval(150);
    connect(m_resizeTimer, &QTimer::timeout, this, &RecipeView::render);
    connect(m_servings, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this] { render(); });
    connect(m_reset, &QToolButton::clicked, this, [this] { m_servings->setValue(m_recipe.servings); });
    connect(edit, &QToolButton::clicked, this, &RecipeView::editRequested);
    connect(m_fav, &QToolButton::toggled, this, [this](bool on) {
        m_fav->setIcon(Icons::get(on ? QStringLiteral("star-filled") : QStringLiteral("star")));
        if (m_has && on != m_recipe.favorite) {
            m_recipe.favorite = on;
            emit favoriteToggled(on);
            render();
        }
    });
    for (QWidget *w : { static_cast<QWidget *>(m_servLabel), static_cast<QWidget *>(m_servings), static_cast<QWidget *>(m_reset),
                        static_cast<QWidget *>(m_fav), static_cast<QWidget *>(edit) })
        w->setEnabled(false);
}

void RecipeView::setRecipe(const Recipe &r)
{
    m_recipe = r;
    m_has = true;
    for (QWidget *w : findChildren<QWidget *>()) w->setEnabled(true);
    m_servings->blockSignals(true);
    m_servings->setValue(r.servings);
    m_servings->blockSignals(false);
    m_fav->blockSignals(true);
    m_fav->setChecked(r.favorite);
    m_fav->setIcon(Icons::get(r.favorite ? QStringLiteral("star-filled") : QStringLiteral("star")));
    m_fav->blockSignals(false);
    m_servLabel->setText(r.yieldUnit.isEmpty() ? tr("Portionen:") : r.yieldUnit + QLatin1Char(':'));
    // Bild als Ressource des Dokuments (QTextBrowser kennt keine data:-URIs zuverlässig)
    QImage img;
    if (!r.image.isEmpty()) img.loadFromData(QByteArray::fromBase64(r.image.toLatin1()));
    m_browser->document()->addResource(QTextDocument::ImageResource, kImageUrl, img);
    render();
    m_browser->moveCursor(QTextCursor::Start);
}

void RecipeView::showMessage(const QString &title, const QString &html)
{
    m_has = false;
    for (QWidget *w : findChildren<QWidget *>())
        if (w != m_browser && w != m_browser->viewport()) w->setEnabled(false);
    m_browser->setEnabled(true);
    m_browser->setHtml(RecipeHtml::emptyPage(styleFromPalette(300), title, html));
}

void RecipeView::render()
{
    if (!m_has) return;
    const int w = m_browser->viewport()->width();
    m_lastWidth = w;
    // Foto: etwa 40 % der Breite, zwischen 160 und 420 Pixel
    const int imgW = qBound(160, int(w * 0.40), 420);
    RecipeHtml::Style st = styleFromPalette(imgW);
    st.imageSrc = kImageUrl.toString();
    const int scroll = m_browser->verticalScrollBar() ? m_browser->verticalScrollBar()->value() : 0;
    m_browser->setHtml(RecipeHtml::render(m_recipe, st, m_servings->value()));
    if (m_browser->verticalScrollBar()) m_browser->verticalScrollBar()->setValue(scroll);
}

void RecipeView::fillDocument(QTextDocument *doc) const
{
    if (!m_has) return;
    RecipeHtml::Style st;     // helle Farben für den Druck
    st.imageWidth = 240;
    QImage img;
    if (!m_recipe.image.isEmpty()) img.loadFromData(QByteArray::fromBase64(m_recipe.image.toLatin1()));
    doc->addResource(QTextDocument::ImageResource, kImageUrl, img);
    st.imageSrc = kImageUrl.toString();
    doc->setHtml(RecipeHtml::render(m_recipe, st, m_servings->value()));
}

void RecipeView::refresh()
{
    m_fav->setIcon(Icons::get(m_fav->isChecked() ? QStringLiteral("star-filled") : QStringLiteral("star")));
    if (m_has) render();
}

void RecipeView::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    if (m_has && qAbs(m_browser->viewport()->width() - m_lastWidth) > 30) m_resizeTimer->start();
}

void RecipeView::changeEvent(QEvent *e)
{
    QWidget::changeEvent(e);
    if (e->type() == QEvent::PaletteChange || e->type() == QEvent::FontChange) {
        if (m_has) m_resizeTimer->start();
    }
}

double RecipeView::servings() const
{
    return m_servings ? m_servings->value() : m_recipe.servings;
}
