// ViviaMeals – Eingabefeld für die Zubereitung mit Formatleiste (siehe instructionsedit.h)
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "instructionsedit.h"

#include "icons.h"
#include "markup.h"

#include <QAction>
#include <QMimeData>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextList>
#include <QToolBar>
#include <QVBoxLayout>

namespace {

// Eingefügter Text aus Browser oder Textverarbeitung: nur unsere Formate übernehmen
class Edit : public QTextEdit
{
public:
    using QTextEdit::QTextEdit;

protected:
    void insertFromMimeData(const QMimeData *src) override
    {
        if (src->hasHtml()) {
            QTextDocument d;
            d.setHtml(src->html());
            insertHtml(Markup::toHtml(InstructionsEdit::markupFromDocument(&d)));
            return;
        }
        if (src->hasText()) {
            insertPlainText(src->text());
            return;
        }
        QTextEdit::insertFromMimeData(src);
    }

    bool canInsertFromMimeData(const QMimeData *src) const override
    {
        return src->hasHtml() || src->hasText();   // keine Bilder in der Zubereitung
    }
};

enum { Bold, Italic, Underline };

} // namespace

InstructionsEdit::InstructionsEdit(QWidget *parent)
    : QWidget(parent)
    , m_edit(new Edit)
    , m_bar(new QToolBar)
{
    auto *l = new QVBoxLayout(this);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(2);
    m_bar->setIconSize(QSize(18, 18));
    m_bar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    l->addWidget(m_bar);
    l->addWidget(m_edit);
    m_edit->setAcceptRichText(true);
    m_edit->setTabChangesFocus(true);

    auto make = [this](const QString &icon, const QString &text, const QKeySequence &key) {
        QAction *a = m_bar->addAction(Icons::get(icon), text);
        a->setCheckable(true);
        if (!key.isEmpty()) {
            a->setShortcut(key);
            a->setShortcutContext(Qt::WidgetWithChildrenShortcut);
            a->setToolTip(QStringLiteral("%1 (%2)").arg(text, key.toString(QKeySequence::NativeText)));
            addAction(a);
        }
        return a;
    };
    m_bold = make(QStringLiteral("format-bold"), tr("Fett"), QKeySequence::Bold);
    m_italic = make(QStringLiteral("format-italic"), tr("Kursiv"), QKeySequence::Italic);
    m_underline = make(QStringLiteral("format-underline"), tr("Unterstrichen"), QKeySequence::Underline);
    m_bar->addSeparator();
    m_bullets = make(QStringLiteral("list-bullet"), tr("Aufzählung"), QKeySequence(tr("Ctrl+Shift+8")));
    m_numbers = make(QStringLiteral("list-number"), tr("Nummerierte Liste"), QKeySequence(tr("Ctrl+Shift+7")));

    connect(m_bold, &QAction::triggered, this, [this](bool on) { setCharFormat(Bold, on); });
    connect(m_italic, &QAction::triggered, this, [this](bool on) { setCharFormat(Italic, on); });
    connect(m_underline, &QAction::triggered, this, [this](bool on) { setCharFormat(Underline, on); });
    connect(m_bullets, &QAction::triggered, this, [this] { toggleList(false); });
    connect(m_numbers, &QAction::triggered, this, [this] { toggleList(true); });
    connect(m_edit, &QTextEdit::currentCharFormatChanged, this, [this] { updateButtons(); });
    connect(m_edit, &QTextEdit::cursorPositionChanged, this, [this] { updateButtons(); });
}

void InstructionsEdit::setMarkup(const QString &markup)
{
    m_edit->setHtml(Markup::toHtml(markup));
    m_edit->document()->setModified(false);
    QTextCursor c = m_edit->textCursor();
    c.movePosition(QTextCursor::Start);
    m_edit->setTextCursor(c);
    updateButtons();
}

QString InstructionsEdit::markup() const
{
    return markupFromDocument(m_edit->document());
}

void InstructionsEdit::setPlaceholderText(const QString &text)
{
    m_edit->setPlaceholderText(text);
}

QString InstructionsEdit::markupFromDocument(const QTextDocument *doc)
{
    QList<Markup::Block> blocks;
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
        Markup::Block mb;
        if (QTextList *list = b.textList()) {
            const QTextListFormat::Style st = list->format().style();
            const bool bullet = st == QTextListFormat::ListDisc || st == QTextListFormat::ListCircle || st == QTextListFormat::ListSquare;
            mb.kind = bullet ? Markup::Block::Bullet : Markup::Block::Number;
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
            mb.number = qMax(1, list->format().start()) + list->itemNumber(b);
#else
            mb.number = 1 + list->itemNumber(b);
#endif
        }
        bool empty = true;
        for (QTextBlock::iterator it = b.begin(); !it.atEnd(); ++it) {
            const QTextFragment f = it.fragment();
            if (!f.isValid()) continue;
            const QTextCharFormat cf = f.charFormat();
            Markup::Run r;
            r.text = f.text();
            r.text.replace(QChar::LineSeparator, QLatin1Char('\n'));
            r.text.remove(QChar::ObjectReplacementCharacter);   // Bilder o. Ä.
            r.bold = cf.fontWeight() >= QFont::DemiBold;
            r.italic = cf.fontItalic();
            r.underline = cf.fontUnderline() && !cf.isAnchor();   // Links sind nur unterstrichen dargestellt
            if (!r.text.trimmed().isEmpty()) empty = false;
            mb.runs << r;
        }
        if (empty) continue;   // Leerzeilen trennen ohnehin Absätze
        // Rand-Leerraum des Absatzes entfernen
        while (!mb.runs.isEmpty() && mb.runs.first().text.trimmed().isEmpty()) mb.runs.removeFirst();
        while (!mb.runs.isEmpty() && mb.runs.last().text.trimmed().isEmpty()) mb.runs.removeLast();
        if (!mb.runs.isEmpty()) {
            QString &first = mb.runs.first().text;
            while (!first.isEmpty() && first.at(0).isSpace()) first.remove(0, 1);
            QString &last = mb.runs.last().text;
            while (!last.isEmpty() && last.at(last.size() - 1).isSpace()) last.chop(1);
        }
        blocks << mb;
    }
    return Markup::serialize(blocks);
}

void InstructionsEdit::setCharFormat(int which, bool on)
{
    QTextCharFormat f;
    if (which == Bold) f.setFontWeight(on ? QFont::Bold : QFont::Normal);
    else if (which == Italic) f.setFontItalic(on);
    else f.setFontUnderline(on);
    QTextCursor c = m_edit->textCursor();
    if (!c.hasSelection()) {
        // ohne Markierung: Wort unter dem Cursor, sonst gilt das Format für das weitere Schreiben
        c.select(QTextCursor::WordUnderCursor);
        if (c.selectedText().trimmed().isEmpty()) c = m_edit->textCursor();
    }
    c.mergeCharFormat(f);
    m_edit->mergeCurrentCharFormat(f);
    m_edit->setFocus();
}

void InstructionsEdit::toggleList(bool numbered)
{
    const QTextListFormat::Style style = numbered ? QTextListFormat::ListDecimal : QTextListFormat::ListDisc;
    QTextCursor c = m_edit->textCursor();
    QTextList *current = c.currentList();
    const bool currentNumbered = current && !(current->format().style() == QTextListFormat::ListDisc
                                              || current->format().style() == QTextListFormat::ListCircle
                                              || current->format().style() == QTextListFormat::ListSquare);
    c.beginEditBlock();
    if (current && currentNumbered == numbered) {
        // Liste für die markierten Absätze aufheben
        QTextDocument *doc = m_edit->document();
        QTextBlock b = doc->findBlock(c.selectionStart());
        const QTextBlock end = doc->findBlock(c.selectionEnd());
        while (b.isValid()) {
            if (QTextList *l = b.textList()) l->remove(b);
            QTextCursor bc(b);
            QTextBlockFormat bf = b.blockFormat();
            bf.setIndent(0);
            bc.setBlockFormat(bf);
            if (b == end) break;
            b = b.next();
        }
    } else if (current) {
        QTextListFormat f = current->format();
        f.setStyle(style);
        current->setFormat(f);
    } else {
        QTextListFormat f;
        f.setStyle(style);
        f.setIndent(1);
        c.createList(f);
    }
    c.endEditBlock();
    m_edit->setFocus();
    updateButtons();
}

void InstructionsEdit::updateButtons()
{
    const QTextCharFormat f = m_edit->currentCharFormat();
    m_bold->setChecked(f.fontWeight() >= QFont::DemiBold);
    m_italic->setChecked(f.fontItalic());
    m_underline->setChecked(f.fontUnderline());
    QTextList *l = m_edit->textCursor().currentList();
    const bool bullet = l
        && (l->format().style() == QTextListFormat::ListDisc || l->format().style() == QTextListFormat::ListCircle
            || l->format().style() == QTextListFormat::ListSquare);
    m_bullets->setChecked(l && bullet);
    m_numbers->setChecked(l && !bullet);
}
