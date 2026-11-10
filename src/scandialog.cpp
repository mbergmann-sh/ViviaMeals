// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "scandialog.h"
#include "icons.h"
#include "imagecropdialog.h"
#include "importdialogs.h"
#include "scanner.h"

#include <QApplication>
#include <QBuffer>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProcessEnvironment>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QSplitter>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

// ================================================================ ScanCanvas

ScanCanvas::ScanCanvas(QWidget *parent) : QWidget(parent)
{
    setMouseTracking(false);
}

void ScanCanvas::setImage(const QImage &img)
{
    m_img = img;
    m_regions.clear();
    m_scaled = QPixmap();
    setZoom(m_zoom);
}

void ScanCanvas::setRegions(const QList<QRect> &r)
{
    m_regions = r;
    update();
}

void ScanCanvas::setZoom(double z)
{
    m_zoom = qBound(0.05, z, 4.0);
    m_scaled = QPixmap();
    if (!m_img.isNull()) {
        const qreal dpr = devicePixelRatioF();
        const QSize target = (QSizeF(m_img.size()) * m_zoom * dpr).toSize();
        m_scaled = QPixmap::fromImage(m_img.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        m_scaled.setDevicePixelRatio(dpr);
    }
    resize(sizeHint());
    updateGeometry();
    update();
}

QSize ScanCanvas::sizeHint() const
{
    if (m_img.isNull()) return { 300, 400 };
    return (QSizeF(m_img.size()) * m_zoom).toSize();
}

QPoint ScanCanvas::toImage(const QPoint &p) const
{
    return QPoint(qBound(0, int(p.x() / m_zoom), m_img.width()), qBound(0, int(p.y() / m_zoom), m_img.height()));
}

void ScanCanvas::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    if (m_img.isNull()) {
        p.setPen(palette().color(QPalette::PlaceholderText));
        p.drawText(rect(), Qt::AlignCenter | Qt::TextWordWrap, tr("Noch keine Seite.\nScannen Sie ein Rezept oder öffnen Sie ein Bild."));
        return;
    }
    p.drawPixmap(0, 0, m_scaled);
    const QColor orange = Icons::accent();
    auto drawRegion = [&](const QRect &r, bool dashed) {
        const QRectF v(r.x() * m_zoom, r.y() * m_zoom, r.width() * m_zoom, r.height() * m_zoom);
        QColor fill = orange;
        fill.setAlpha(50);
        p.fillRect(v, fill);
        QPen pen(orange, 2, dashed ? Qt::DashLine : Qt::SolidLine);
        p.setPen(pen);
        p.drawRect(v.adjusted(1, 1, -1, -1));
    };
    for (const QRect &r : m_regions) drawRegion(r, false);
    if (m_dragging) drawRegion(QRect(m_dragStart, m_dragEnd).normalized(), true);
}

void ScanCanvas::mousePressEvent(QMouseEvent *e)
{
    if (!m_editable || m_img.isNull() || e->button() != Qt::LeftButton) return;
    const QPoint ip = toImage(e->pos());
    for (int i = 0; i < m_regions.size(); ++i) {
        if (m_regions.at(i).contains(ip)) {
            m_regions.removeAt(i);   // Klick in einen Bereich: kein Foto
            update();
            emit regionsChanged();
            return;
        }
    }
    m_dragging = true;
    m_dragStart = m_dragEnd = ip;
}

void ScanCanvas::mouseMoveEvent(QMouseEvent *e)
{
    if (!m_dragging) return;
    m_dragEnd = toImage(e->pos());
    update();
}

void ScanCanvas::mouseReleaseEvent(QMouseEvent *e)
{
    if (!m_dragging) return;
    m_dragging = false;
    m_dragEnd = toImage(e->pos());
    const QRect r = QRect(m_dragStart, m_dragEnd).normalized();
    if (r.width() > 30 && r.height() > 30) {
        m_regions << r;
        emit regionsChanged();
    }
    update();
}

// ================================================================ ScanImageView

ScanImageView::ScanImageView(QWidget *parent) : QScrollArea(parent), m_canvas(new ScanCanvas)
{
    setWidget(m_canvas);
    setWidgetResizable(false);
    setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    setBackgroundRole(QPalette::Dark);
}

void ScanImageView::setImage(const QImage &img)
{
    m_canvas->setImage(img);
    if (m_fit) applyFit();
}

void ScanImageView::applyFit()
{
    const QImage &img = m_canvas->image();
    if (img.isNull()) return;
    const int w = viewport()->width() - 4;
    m_canvas->setZoom(qMax(0.05, double(w) / img.width()));
}

void ScanImageView::zoomIn()
{
    m_fit = false;
    m_canvas->setZoom(m_canvas->zoom() * 1.25);
}

void ScanImageView::zoomOut()
{
    m_fit = false;
    m_canvas->setZoom(m_canvas->zoom() / 1.25);
}

void ScanImageView::fitWidth()
{
    m_fit = true;
    applyFit();
}

void ScanImageView::resizeEvent(QResizeEvent *e)
{
    QScrollArea::resizeEvent(e);
    if (m_fit) applyFit();
}

void ScanImageView::wheelEvent(QWheelEvent *e)
{
    if (e->modifiers() & Qt::ControlModifier) {
        if (e->angleDelta().y() > 0) zoomIn();
        else if (e->angleDelta().y() < 0) zoomOut();
        e->accept();
        return;
    }
    QScrollArea::wheelEvent(e);
}

namespace {

QToolButton *toolButton(const QString &icon, const QString &tip, QWidget *parent)
{
    auto *b = new QToolButton(parent);
    b->setIcon(Icons::get(icon));
    b->setToolTip(tip);
    b->setAutoRaise(true);
    return b;
}

QImage loadImage(const QString &file, QString *error)
{
    QImageReader reader(file);
    reader.setAutoTransform(true);   // Handyfotos: EXIF-Drehung
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    reader.setAllocationLimit(1024); // große Scans (600 dpi, A4 farbig)
#endif
    QImage img = reader.read();
    if (img.isNull() && error) *error = reader.errorString();
    return img;
}

} // namespace

// ================================================================ ScanReferencePanel

ScanReferencePanel::ScanReferencePanel(const QList<QImage> &pages, QWidget *parent) : QWidget(parent), m_pages(pages)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    auto *bar = new QHBoxLayout;
    auto *box = new QComboBox;
    for (int i = 0; i < pages.size(); ++i) box->addItem(tr("Seite %1 von %2").arg(i + 1).arg(pages.size()));
    box->setVisible(pages.size() > 1);
    bar->addWidget(box);
    bar->addStretch(1);
    auto *zin = toolButton(QStringLiteral("zoom-in"), tr("Vergrößern (Strg+Mausrad)"), this);
    auto *zout = toolButton(QStringLiteral("zoom-out"), tr("Verkleinern"), this);
    auto *fit = toolButton(QStringLiteral("zoom-fit"), tr("An Breite anpassen"), this);
    bar->addWidget(zin);
    bar->addWidget(zout);
    bar->addWidget(fit);
    lay->addLayout(bar);
    m_view = new ScanImageView;
    lay->addWidget(m_view, 1);
    if (!pages.isEmpty()) m_view->setImage(pages.first());
    connect(box, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int i) {
        if (i >= 0 && i < m_pages.size()) m_view->setImage(m_pages.at(i));
    });
    connect(zin, &QToolButton::clicked, m_view, &ScanImageView::zoomIn);
    connect(zout, &QToolButton::clicked, m_view, &ScanImageView::zoomOut);
    connect(fit, &QToolButton::clicked, m_view, &ScanImageView::fitWidth);
}

// ================================================================ ScanDialog

ScanDialog::ScanDialog(Start start, QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Rezept scannen"));
    setObjectName(QStringLiteral("ScanDialog"));
    QSettings s;
    m_tesseract = Ocr::findTesseract(s.value(QStringLiteral("scan/tesseract")).toString());
    m_langs = s.value(QStringLiteral("scan/languages")).toString();

    auto *lay = new QVBoxLayout(this);
    // ---- Werkzeugleiste
    auto *bar = new QHBoxLayout;
    const Scanner::Program prog = Scanner::current();
    m_scanBtn = new QPushButton(Icons::get(QStringLiteral("scan")), tr("&Scannen …"));
    m_scanBtn->setToolTip(prog.native || !prog.program.isEmpty() ? tr("Scannen mit: %1").arg(prog.name)
                                                                 : tr("Kein Scanprogramm eingestellt (Extras → Einstellungen → Scannen)"));
    m_openBtn = new QPushButton(Icons::get(QStringLiteral("image-open")), tr("Bild ö&ffnen …"));
    m_openBtn->setToolTip(tr("Gescannte Seite oder Foto eines Rezepts laden (PNG, JPEG, TIFF, BMP, WebP)"));
    m_pageBox = new QComboBox;
    m_removeBtn = new QPushButton(Icons::get(QStringLiteral("delete")), tr("Seite entfernen"));
    bar->addWidget(m_scanBtn);
    bar->addWidget(m_openBtn);
    bar->addSpacing(12);
    bar->addWidget(m_pageBox);
    bar->addWidget(m_removeBtn);
    bar->addStretch(1);
    lay->addLayout(bar);

    auto *split = new QSplitter(Qt::Horizontal);
    split->setObjectName(QStringLiteral("scanSplit"));
    // ---- links: Seite
    auto *left = new QWidget;
    auto *ll = new QVBoxLayout(left);
    ll->setContentsMargins(0, 0, 0, 0);
    auto *vbar = new QHBoxLayout;
    auto *hint = new QLabel(tr("Orange = Foto (wird nicht als Text gelesen). Klick entfernt, Ziehen fügt hinzu."));
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: palette(placeholder-text);"));
    vbar->addWidget(hint, 1);
    auto *zin = toolButton(QStringLiteral("zoom-in"), tr("Vergrößern (Strg+Mausrad)"), this);
    auto *zout = toolButton(QStringLiteral("zoom-out"), tr("Verkleinern"), this);
    auto *fit = toolButton(QStringLiteral("zoom-fit"), tr("An Breite anpassen"), this);
    vbar->addWidget(zin);
    vbar->addWidget(zout);
    vbar->addWidget(fit);
    ll->addLayout(vbar);
    m_view = new ScanImageView;
    m_view->canvas()->setEditable(true);
    ll->addWidget(m_view, 1);
    auto *photoBox = new QGroupBox(tr("Rezeptbild"));
    auto *pl = new QHBoxLayout(photoBox);
    m_photoLabel = new QLabel(tr("kein Bild"));
    m_photoLabel->setFixedSize(120, 90);
    m_photoLabel->setAlignment(Qt::AlignCenter);
    m_photoLabel->setFrameShape(QFrame::StyledPanel);
    pl->addWidget(m_photoLabel);
    auto *pb = new QVBoxLayout;
    m_photoBtn = new QPushButton(Icons::get(QStringLiteral("edit")), tr("Aus der Seite &zuschneiden …"));
    m_photoBtn->setToolTip(tr("Ausschnitt der Seite als Rezeptbild verwenden (vorausgewählt: das größte erkannte Foto)"));
    m_photoClear = new QPushButton(Icons::get(QStringLiteral("delete")), tr("Kein Rezeptbild"));
    pb->addWidget(m_photoBtn);
    pb->addWidget(m_photoClear);
    pb->addStretch(1);
    pl->addLayout(pb, 1);
    ll->addWidget(photoBox);
    split->addWidget(left);

    // ---- rechts: Texterkennung
    auto *right = new QWidget;
    auto *rl = new QVBoxLayout(right);
    rl->setContentsMargins(0, 0, 0, 0);
    auto *sbar = new QHBoxLayout;
    m_status = new QLabel;
    m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::RichText);
    m_status->setOpenExternalLinks(true);
    sbar->addWidget(m_status, 1);
    m_busy = new QProgressBar;
    m_busy->setRange(0, 0);
    m_busy->setMaximumWidth(120);
    m_busy->hide();
    sbar->addWidget(m_busy);
    m_ocrBtn = new QPushButton(Icons::get(QStringLiteral("ocr")), tr("Text &erkennen"));
    m_ocrBtn->setToolTip(tr("Texterkennung (neu) starten – z. B. nach dem Ändern der Fotobereiche"));
    sbar->addWidget(m_ocrBtn);
    rl->addLayout(sbar);
    m_tabs = new QTabWidget;
    auto *rec = new QWidget;
    auto *form = new QFormLayout(rec);
    m_title = new QLineEdit;
    form->addRow(tr("&Titel:"), m_title);
    m_yield = new QLineEdit;
    m_yield->setPlaceholderText(tr("z. B. 4 Portionen"));
    form->addRow(tr("&Portionen:"), m_yield);
    m_ingredients = new QPlainTextEdit;
    m_ingredients->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_ingredients->setPlaceholderText(tr("eine Zutat je Zeile"));
    form->addRow(tr("Zu&taten:"), m_ingredients);
    m_directions = new QPlainTextEdit;
    m_directions->setPlaceholderText(tr("Arbeitsschritte – Absätze durch eine Leerzeile trennen"));
    form->addRow(tr("Zu&bereitung:"), m_directions);
    m_refillBtn = new QPushButton(Icons::get(QStringLiteral("refresh")), tr("Felder neu aus dem erkannten Text füllen"));
    m_refillBtn->hide();
    form->addRow(QString(), m_refillBtn);
    m_tabs->addTab(rec, Icons::get(QStringLiteral("recipe")), tr("Rezept"));
    m_rest = new QPlainTextEdit;
    m_rest->setPlaceholderText(tr("Zeilen, die weder Zutat noch Zubereitung sind (Einleitung, Zeiten, Nährwertangaben)"));
    m_tabs->addTab(m_rest, tr("Nicht zugeordnet"));
    m_raw = new QPlainTextEdit;
    m_raw->setReadOnly(true);
    m_tabs->addTab(m_raw, tr("Erkannter Text"));
    rl->addWidget(m_tabs, 1);
    split->addWidget(right);
    split->setStretchFactor(0, 4);
    split->setStretchFactor(1, 5);
    lay->addWidget(split, 1);
    const QByteArray st = s.value(QStringLiteral("scan/split")).toByteArray();
    if (!st.isEmpty()) split->restoreState(st);
    connect(this, &QDialog::finished, this, [split] { QSettings().setValue(QStringLiteral("scan/split"), split->saveState()); });

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    bb->button(QDialogButtonBox::Ok)->setText(tr("Im &Editor öffnen"));
    bb->button(QDialogButtonBox::Ok)->setIcon(Icons::get(QStringLiteral("edit")));
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, this, &ScanDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);

    m_poll = new QTimer(this);
    m_poll->setInterval(800);
    connect(m_poll, &QTimer::timeout, this, &ScanDialog::pollFile);
    connect(m_scanBtn, &QPushButton::clicked, this, &ScanDialog::scan);
    connect(m_openBtn, &QPushButton::clicked, this, &ScanDialog::openImage);
    connect(m_removeBtn, &QPushButton::clicked, this, &ScanDialog::removePage);
    connect(m_pageBox, qOverload<int>(&QComboBox::activated), this, &ScanDialog::showPage);
    connect(m_ocrBtn, &QPushButton::clicked, this, [this] {
        for (Page &p : m_pages) p.done = false;
        recognize();
    });
    connect(m_refillBtn, &QPushButton::clicked, this, [this] { fillFields(true); });
    connect(zin, &QToolButton::clicked, m_view, &ScanImageView::zoomIn);
    connect(zout, &QToolButton::clicked, m_view, &ScanImageView::zoomOut);
    connect(fit, &QToolButton::clicked, m_view, &ScanImageView::fitWidth);
    connect(m_view->canvas(), &ScanCanvas::regionsChanged, this, [this] {
        if (m_current < 0) return;
        m_pages[m_current].photos = m_view->canvas()->regions();
        m_pages[m_current].done = false;
        setStatus(tr("Fotobereiche geändert – „Text erkennen“ liest die Seite neu."));
        updateButtons();
    });
    connect(m_photoBtn, &QPushButton::clicked, this, &ScanDialog::choosePhoto);
    connect(m_photoClear, &QPushButton::clicked, this, [this] { setPhoto(QImage()); });
    auto edited = [this] {
        if (!m_filling) m_edited = true;
    };
    connect(m_title, &QLineEdit::textEdited, this, edited);
    connect(m_yield, &QLineEdit::textEdited, this, edited);
    connect(m_ingredients, &QPlainTextEdit::textChanged, this, edited);
    connect(m_directions, &QPlainTextEdit::textChanged, this, edited);

    if (m_tesseract.isEmpty()) {
        setStatus(tr("<b>Tesseract fehlt</b> – ohne Texterkennung können Sie das Rezept anhand der Seite selbst "
                     "eintippen.<br>%1").arg(Ocr::installHint().toHtmlEscaped()),
                  true);
    } else {
        if (m_langs.isEmpty()) m_langs = Ocr::defaultLanguages(Ocr::languages(m_tesseract));
        setStatus(tr("Scannen Sie eine Rezeptseite oder öffnen Sie ein Bild bzw. Foto."));
    }
    updateButtons();
    resize(1280, 820);
    if (start == StartScan) QTimer::singleShot(0, this, &ScanDialog::scan);
    else if (start == StartOpen) QTimer::singleShot(0, this, &ScanDialog::openImage);
}

ScanDialog::~ScanDialog()
{
    for (QProcess *p : { m_ocr, m_ext }) {
        if (p) p->disconnect(this);   // keine Signale mehr an den halb abgebauten Dialog
        if (p && p->state() != QProcess::NotRunning) {
            p->kill();
            p->waitForFinished(2000);
        }
    }
    QFile::remove(Scanner::targetFile());
}

bool ScanDialog::busy() const
{
    return m_ocr && m_ocr->state() != QProcess::NotRunning;
}

QList<QImage> ScanDialog::pages() const
{
    QList<QImage> l;
    for (const Page &p : m_pages) l << p.img;
    return l;
}

void ScanDialog::setStatus(const QString &text, bool warning)
{
    m_busy->hide();
    m_status->setText(warning ? QStringLiteral("<span style=\"color:#c0392b\">%1</span>").arg(text) : text);
}

void ScanDialog::setBusy(const QString &text)
{
    m_status->setText(text);
    m_busy->show();
}

void ScanDialog::updateButtons()
{
    const bool running = m_ocr && m_ocr->state() != QProcess::NotRunning;
    m_pageBox->setVisible(m_pages.size() > 1);
    m_removeBtn->setEnabled(m_current >= 0 && !running);
    m_ocrBtn->setEnabled(!m_pages.isEmpty() && !m_tesseract.isEmpty() && !running);
    m_photoBtn->setEnabled(!m_pages.isEmpty());
    m_photoClear->setEnabled(!m_photo.isNull());
    bool stale = false;
    for (const Page &p : m_pages) stale |= !p.done;
    QFont f = m_ocrBtn->font();
    f.setBold(stale && !running && !m_tesseract.isEmpty());
    m_ocrBtn->setFont(f);
}

// ---------------------------------------------------------------- Seiten holen

void ScanDialog::scan()
{
    const Scanner::Program p = Scanner::current();
    const QString target = Scanner::targetFile();
    QFile::remove(target);
    QSettings s;
    if (p.native) {
        setBusy(tr("Windows-Scandialog ist geöffnet …"));
        m_status->repaint();
        QString err;
        const Scanner::Status st = Scanner::acquireNative(target, s.value(QStringLiteral("scan/color"), true).toBool(), &err);
        if (st == Scanner::Status::Ok) {
            QString lerr;
            const QImage img = loadImage(target, &lerr);
            QFile::remove(target);
            if (img.isNull()) setStatus(tr("Das gescannte Bild kann nicht gelesen werden: %1").arg(lerr), true);
            else addImage(img, tr("Scan"));
        } else if (st == Scanner::Status::Cancelled) {
            setStatus(tr("Scannen abgebrochen."));
        } else {
            setStatus(err.toHtmlEscaped(), true);
            QMessageBox::warning(this, tr("Scannen"),
                                 tr("%1\n\nAlternativ können Sie die Seite mit der Software Ihres Scanners oder mit dem Handy "
                                    "fotografieren und über „Bild öffnen …“ laden.").arg(err));
        }
        return;
    }
    if (p.program.isEmpty()) {
        QMessageBox::information(this, tr("Scannen"), tr("Es ist kein Scanprogramm eingestellt. Wählen Sie eines unter "
                                                          "Extras → Einstellungen → Scannen, oder laden Sie ein Bild mit „Bild öffnen …“."));
        return;
    }
    if (!Scanner::installed(p)) {
#if defined(Q_OS_LINUX)
        const QString more = tr("\n\nInstallation z. B. mit „sudo apt install xsane“ (Debian/Ubuntu) bzw. „sudo dnf install xsane“ "
                                "(Fedora). Ein anderes Programm lässt sich unter Extras → Einstellungen → Scannen wählen.");
#else
        const QString more = tr("\n\nEin anderes Programm lässt sich unter Extras → Einstellungen → Scannen wählen.");
#endif
        QMessageBox::warning(this, tr("Scannen"), tr("Das Scanprogramm „%1“ ist nicht installiert.").arg(p.program) + more);
        return;
    }
    QStringList args;
    for (const QString &a : p.args) args << QString(a).replace(QStringLiteral("%f"), QDir::toNativeSeparators(target));
    if (p.toStdout) {
        if (m_ext && m_ext->state() != QProcess::NotRunning) return;
        delete m_ext;
        m_ext = new QProcess(this);
        m_ext->setStandardOutputFile(target);
        connect(m_ext, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this, target](int code, QProcess::ExitStatus es) {
            m_scanBtn->setEnabled(true);
            if (es != QProcess::NormalExit || code != 0) {
                QString err = QString::fromLocal8Bit(m_ext->readAllStandardError()).trimmed();
                if (err.isEmpty()) err = tr("Fehlercode %1").arg(code);
                setStatus(tr("Scannen fehlgeschlagen: %1").arg(err).toHtmlEscaped(), true);
                QFile::remove(target);
                return;
            }
            QString lerr;
            const QImage img = loadImage(target, &lerr);
            QFile::remove(target);
            if (img.isNull()) setStatus(tr("Das gescannte Bild kann nicht gelesen werden: %1").arg(lerr), true);
            else addImage(img, tr("Scan"));
        });
        m_ext->start(p.program, args);
        m_scanBtn->setEnabled(false);
        setBusy(p.hint);
        return;
    }
    if (!QProcess::startDetached(p.program, args)) {
        setStatus(tr("„%1“ kann nicht gestartet werden.").arg(p.program), true);
        return;
    }
    if (p.writesFile) {
        m_lastSize = -1;
        m_poll->start();
        setStatus(p.hint);
    } else {
        setStatus(p.hint);
    }
}

void ScanDialog::pollFile()
{
    const QString target = Scanner::targetFile();
    const QFileInfo fi(target);
    if (!fi.exists() || fi.size() == 0) {
        m_lastSize = -1;
        return;
    }
    if (fi.size() != m_lastSize) {   // wird noch geschrieben
        m_lastSize = fi.size();
        return;
    }
    QString err;
    const QImage img = loadImage(target, &err);
    if (img.isNull()) return;   // unvollständig – beim nächsten Mal
    QFile::remove(target);
    m_lastSize = -1;
    addImage(img, tr("Scan"));
}

void ScanDialog::openImage()
{
    QSettings s;
    const QString dir = s.value(QStringLiteral("scan/dir"), QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)).toString();
    QStringList filters;
    for (const QByteArray &f : QImageReader::supportedImageFormats()) filters << QStringLiteral("*.") + QString::fromLatin1(f);
    const QStringList files = QFileDialog::getOpenFileNames(this, tr("Rezeptseite öffnen"), dir,
                                                            tr("Bilder (%1);;Alle Dateien (*)").arg(filters.join(QLatin1Char(' '))));
    if (files.isEmpty()) return;
    s.setValue(QStringLiteral("scan/dir"), QFileInfo(files.first()).absolutePath());
    for (const QString &f : files) {
        QString err;
        const QImage img = loadImage(f, &err);
        if (img.isNull()) {
            QMessageBox::warning(this, tr("Bild öffnen"), tr("„%1“ kann nicht gelesen werden: %2").arg(QFileInfo(f).fileName(), err));
            continue;
        }
        addImage(img, QFileInfo(f).fileName());
    }
}

bool ScanDialog::addImage(const QImage &raw, const QString &origin)
{
    Page p;
    p.img = RecipeScan::normalize(raw);
    if (p.img.isNull()) return false;
    p.photos = RecipeScan::findPhotos(p.img);
    m_pages << p;
    m_pageBox->addItem(QString());
    for (int i = 0; i < m_pages.size(); ++i) m_pageBox->setItemText(i, tr("Seite %1 von %2").arg(i + 1).arg(m_pages.size()));
    showPage(int(m_pages.size()) - 1);
    // erstes gefundenes Foto als Rezeptbild vorschlagen
    if (m_photo.isNull() && !p.photos.isEmpty()) {
        QImage ph = p.img.copy(p.photos.first());
        if (ph.width() > ImageCropDialog::maxWidth()) ph = ph.scaledToWidth(ImageCropDialog::maxWidth(), Qt::SmoothTransformation);
        setPhoto(ph);
    }
    Q_UNUSED(origin)
    recognize();
    return true;
}

void ScanDialog::showPage(int i)
{
    if (i < 0 || i >= m_pages.size()) {
        m_current = -1;
        m_view->setImage(QImage());
        updateButtons();
        return;
    }
    m_current = i;
    m_pageBox->setCurrentIndex(i);
    m_view->setImage(m_pages.at(i).img);
    m_view->canvas()->setRegions(m_pages.at(i).photos);
    updateButtons();
}

void ScanDialog::removePage()
{
    if (m_current < 0) return;
    m_pages.removeAt(m_current);
    m_pageBox->removeItem(m_current);
    for (int i = 0; i < m_pages.size(); ++i) m_pageBox->setItemText(i, tr("Seite %1 von %2").arg(i + 1).arg(m_pages.size()));
    showPage(qMin(m_current, int(m_pages.size()) - 1));
    fillFields(false);
}

// ---------------------------------------------------------------- Texterkennung

void ScanDialog::recognize()
{
    if (m_tesseract.isEmpty() || (m_ocr && m_ocr->state() != QProcess::NotRunning)) {
        updateButtons();
        return;
    }
    runNext();
}

void ScanDialog::runNext()
{
    m_ocrPage = -1;
    for (int i = 0; i < m_pages.size(); ++i)
        if (!m_pages.at(i).done) {
            m_ocrPage = i;
            break;
        }
    if (m_ocrPage < 0) {
        fillFields(false);
        updateButtons();
        return;
    }
    if (!m_ocr) {
        m_ocr = new QProcess(this);
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        for (const QString &e : Ocr::environment(m_tesseract)) env.insert(e.section(QLatin1Char('='), 0, 0), e.section(QLatin1Char('='), 1));
        m_ocr->setProcessEnvironment(env);
        connect(m_ocr, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this] { ocrFinished(); });
        connect(m_ocr, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
            if (e != QProcess::FailedToStart) return;
            setStatus(tr("Tesseract kann nicht gestartet werden (%1): %2").arg(m_tesseract, m_ocr->errorString()), true);
            updateButtons();
        });
    }
    const Page &p = m_pages.at(m_ocrPage);
    QByteArray png;
    {
        QBuffer buf(&png);
        buf.open(QIODevice::WriteOnly);
        RecipeScan::blank(p.img, p.photos).convertToFormat(QImage::Format_Grayscale8).save(&buf, "PNG");
    }
    setBusy(m_pages.size() > 1 ? tr("Texterkennung Seite %1 von %2 …").arg(m_ocrPage + 1).arg(m_pages.size()) : tr("Texterkennung läuft …"));
    m_ocr->start(m_tesseract, Ocr::arguments(QStringLiteral("stdin"), m_langs));
    if (m_ocr->waitForStarted(10000)) {
        m_ocr->write(png);
        m_ocr->closeWriteChannel();
    }
    updateButtons();
}

void ScanDialog::ocrFinished()
{
    const int i = m_ocrPage;
    if (i < 0 || i >= m_pages.size()) return;
    if (m_ocr->exitStatus() != QProcess::NormalExit || m_ocr->exitCode() != 0) {
        QString err = QString::fromUtf8(m_ocr->readAllStandardError()).trimmed();
        if (err.size() > 300) err = err.left(300) + QStringLiteral(" …");
        m_pages[i].done = true;   // nicht endlos wiederholen
        setStatus(tr("Texterkennung fehlgeschlagen: %1").arg(err).toHtmlEscaped(), true);
        updateButtons();
        return;
    }
    m_pages[i].lines = Ocr::toLines(Ocr::parseTsv(m_ocr->readAllStandardOutput()));
    m_pages[i].done = true;
    QTimer::singleShot(0, this, &ScanDialog::runNext);
}

void ScanDialog::fillFields(bool force)
{
    QList<Ocr::Line> all;
    int words = 0;
    for (int i = 0; i < m_pages.size(); ++i) {
        for (Ocr::Line l : m_pages.at(i).lines) {
            l.block += i * 10000;   // Absätze verschiedener Seiten nicht verbinden
            all << l;
            words += int(l.text.count(QLatin1Char(' ')) + 1);
        }
    }
    m_raw->setPlainText(Ocr::plainText(all));
    m_parts = RecipeScan::split(all);
    if (m_edited && !force) {
        m_refillBtn->show();
        setStatus(tr("Texterkennung fertig. Ihre Änderungen bleiben erhalten – „Felder neu füllen“ übernimmt den neu erkannten Text."));
        return;
    }
    m_filling = true;
    m_title->setText(m_parts.text.title);
    m_yield->setText(m_parts.text.yield);
    m_ingredients->setPlainText(m_parts.text.ingredientLines.join(QLatin1Char('\n')));
    m_directions->setPlainText(m_parts.text.directionLines.join(QLatin1Char('\n')));
    m_rest->setPlainText(m_parts.unassigned.join(QLatin1Char('\n')));
    m_filling = false;
    m_edited = false;
    m_refillBtn->hide();
    if (m_pages.isEmpty()) {
        setStatus(tr("Scannen Sie eine Rezeptseite oder öffnen Sie ein Bild bzw. Foto."));
    } else if (all.isEmpty()) {
        setStatus(tr("Auf der Seite wurde kein Text erkannt. Ist das Bild scharf und richtig herum? Ist ein Bereich fälschlich als "
                     "Foto markiert, entfernen Sie ihn mit einem Klick."), true);
    } else {
        setStatus(tr("%1 Wörter erkannt: %2 Zutaten, %3 Absätze Zubereitung. Bitte prüfen und ergänzen – dann „Im Editor öffnen“.")
                      .arg(words)
                      .arg(m_parts.text.ingredientLines.size())
                      .arg(m_parts.text.directionLines.count(QString()) + (m_parts.text.directionLines.isEmpty() ? 0 : 1)));
    }
}

// ---------------------------------------------------------------- Rezeptbild

void ScanDialog::choosePhoto()
{
    if (m_current < 0) return;
    const Page &p = m_pages.at(m_current);
    // ganze Seite zum Zuschneiden; ist ein Foto markiert, nur dieser Bereich (mit etwas Rand)
    QImage src = p.img;
    if (!p.photos.isEmpty()) src = p.img.copy(p.photos.first().adjusted(-40, -40, 40, 40).intersected(p.img.rect()));
    ImageCropDialog d(src, this);
    if (d.exec() != QDialog::Accepted) return;
    setPhoto(d.result());
}

void ScanDialog::setPhoto(const QImage &img)
{
    m_photo = img;
    if (img.isNull()) {
        m_photoLabel->setPixmap(QPixmap());
        m_photoLabel->setText(tr("kein Bild"));
    } else {
        m_photoLabel->setPixmap(QPixmap::fromImage(img.scaled(m_photoLabel->size() - QSize(4, 4), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    }
    updateButtons();
}

// ---------------------------------------------------------------- Übernehmen

void ScanDialog::accept()
{
    if (m_title->text().trimmed().isEmpty()) {
        QMessageBox::information(this, windowTitle(), tr("Bitte geben Sie einen Titel für das Rezept ein."));
        m_tabs->setCurrentIndex(0);
        m_title->setFocus();
        return;
    }
    RecipeScan::Parts parts = m_parts;
    parts.text.title = m_title->text().trimmed();
    parts.text.yield = m_yield->text().trimmed();
    parts.text.ingredientLines = m_ingredients->toPlainText().split(QLatin1Char('\n'));
    parts.text.directionLines = m_directions->toPlainText().split(QLatin1Char('\n'));
    QSettings s;
    Importers::Options opt;
    opt.translate = s.value(QStringLiteral("import/translate"), true).toBool();
    opt.autoTags = s.value(QStringLiteral("import/autoTags"), true).toBool();
    opt.fixUmlauts = s.value(QStringLiteral("import/fixUmlauts"), true).toBool();
    opt.mealTags = s.value(QStringLiteral("import/mealTags"), true).toBool();
    opt.nutrientTags = s.value(QStringLiteral("import/nutrientTags"), true).toBool();
    opt.translator = ImportFlow::deeplTranslator();
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_recipe = RecipeScan::toRecipe(parts, opt);
    QApplication::restoreOverrideCursor();
    m_recipe.sourceFormat = QStringLiteral("Scan");
    if (!m_photo.isNull()) {
        QByteArray ba;
        QBuffer buf(&ba);
        buf.open(QIODevice::WriteOnly);
        m_photo.convertToFormat(QImage::Format_RGB32).save(&buf, "PNG");
        m_recipe.image = QString::fromLatin1(ba.toBase64());
    }
    QDialog::accept();
}
