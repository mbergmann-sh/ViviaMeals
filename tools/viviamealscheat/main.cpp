// ViviaMealsCheat – ersetzt die Angabe „Quelle“ in allen Rezepten einer ViviaMeals-Datenbank
//
// Vorgabe: die Quelle ALLER Rezepte wird durch „Micha B.“ ersetzt (ohne Suchtext). Wahlweise nur ein
// bestimmter Text oder nur ganz übereinstimmende Quellen.
// Datenbank (viviameals.vmdb bzw. vitameals.vmdb) per Dateidialog öffnen, zu ersetzenden Text und neuen
// Text eingeben, Vorschau ansehen, nach Sicherheitsabfrage ausführen. Vorher werden die betroffenen
// Rezeptdateien auf Wunsch in <Datenbank>/sicherung-quelle-<Datum> kopiert. Nur das Feld „source“
// (und „modified“) der .vmr-Dateien wird geändert; alle anderen Angaben bleiben unverändert.
// ViviaMeals erkennt geänderte Dateien beim nächsten Öffnen der Datenbank selbst (Index nach Dateizeit).
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace {

struct Entry {
    QString file;
    QString title;
    QString source;
    QJsonObject json;
};

enum Mode { Contains, Whole, All };

class Window : public QWidget
{
public:
    Window()
    {
        setWindowTitle(QStringLiteral("ViviaMealsCheat – Quelle in ViviaMeals-Rezepten ersetzen"));
        auto *lay = new QVBoxLayout(this);

        auto *dbRow = new QHBoxLayout;
        m_dbEdit = new QLineEdit;
        m_dbEdit->setReadOnly(true);
        m_dbEdit->setPlaceholderText(QStringLiteral("noch keine Datenbank geöffnet"));
        auto *open = new QPushButton(QStringLiteral("Datenbank &öffnen …"));
        dbRow->addWidget(new QLabel(QStringLiteral("Datenbank:")));
        dbRow->addWidget(m_dbEdit, 1);
        dbRow->addWidget(open);
        lay->addLayout(dbRow);

        auto *srcBox = new QGroupBox(QStringLiteral("Vorhandene Quellen (Klick: nur diesen Text ersetzen)"));
        auto *sl = new QVBoxLayout(srcBox);
        m_sources = new QTableWidget(0, 2);
        m_sources->setHorizontalHeaderLabels({ QStringLiteral("Quelle"), QStringLiteral("Rezepte") });
        m_sources->verticalHeader()->hide();
        m_sources->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_sources->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_sources->setSelectionMode(QAbstractItemView::SingleSelection);
        m_sources->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_sources->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        m_sources->setWordWrap(false);
        sl->addWidget(m_sources);
        lay->addWidget(srcBox, 1);

        auto *form = new QFormLayout;
        m_find = new QLineEdit;
        m_find->setPlaceholderText(QStringLiteral("z. B. Gustav Knuth"));
        m_find->setEnabled(false);   // Vorgabe: alle Quellen ersetzen
        m_find->setClearButtonEnabled(true);
        m_mode = new QComboBox;
        m_mode->addItem(QStringLiteral("Quelle bei ALLEN Rezepten ersetzen (ohne Suchtext)"), All);
        m_mode->addItem(QStringLiteral("nur einen bestimmten Text innerhalb der Quelle ersetzen"), Contains);
        m_mode->addItem(QStringLiteral("nur ersetzen, wenn die ganze Quelle übereinstimmt"), Whole);
        form->addRow(QStringLiteral("&Art:"), m_mode);
        form->addRow(QStringLiteral("&Zu ersetzender Text:"), m_find);
        m_replace = new QLineEdit(QStringLiteral("Micha B."));
        m_replace->setPlaceholderText(QStringLiteral("neue Quelle (leer = Quelle entfernen)"));
        m_replace->setClearButtonEnabled(true);
        form->addRow(QStringLiteral("&Neue Quelle:"), m_replace);
        m_case = new QCheckBox(QStringLiteral("Groß-/Kleinschreibung beachten"));
        m_case->setEnabled(false);
        form->addRow(QString(), m_case);
        m_backup = new QCheckBox(QStringLiteral("vorher Sicherungskopie der betroffenen Rezepte anlegen"));
        m_backup->setChecked(true);
        form->addRow(QString(), m_backup);
        lay->addLayout(form);

        m_preview = new QLabel;
        m_preview->setWordWrap(true);
        m_preview->setTextFormat(Qt::RichText);
        lay->addWidget(m_preview);

        auto *bb = new QDialogButtonBox;
        m_run = bb->addButton(QStringLiteral("&Ausführen …"), QDialogButtonBox::AcceptRole);
        auto *quit = bb->addButton(QStringLiteral("&Beenden"), QDialogButtonBox::RejectRole);
        lay->addWidget(bb);

        connect(open, &QPushButton::clicked, this, [this] { chooseDb(); });
        connect(quit, &QPushButton::clicked, this, &QWidget::close);
        connect(m_run, &QPushButton::clicked, this, [this] { run(); });
        connect(m_find, &QLineEdit::textChanged, this, [this] { updatePreview(); });
        connect(m_replace, &QLineEdit::textChanged, this, [this] { updatePreview(); });
        connect(m_case, &QCheckBox::toggled, this, [this] { updatePreview(); });
        connect(m_mode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
            m_find->setEnabled(mode() != All);
            m_case->setEnabled(mode() != All);
            updatePreview();
        });
        connect(m_sources, &QTableWidget::cellClicked, this, [this](int row, int) {
            if (mode() == All) m_mode->setCurrentIndex(m_mode->findData(Contains));
            m_find->setText(m_sources->item(row, 0)->data(Qt::UserRole).toString());
        });

        QSettings s;
        if (!restoreGeometry(s.value(QStringLiteral("geometry")).toByteArray())) resize(760, 600);
        const QString last = s.value(QStringLiteral("lastDb")).toString();
        if (!last.isEmpty() && QFileInfo::exists(last)) loadDb(last);
        updatePreview();
    }

protected:
    void closeEvent(QCloseEvent *e) override
    {
        QSettings().setValue(QStringLiteral("geometry"), saveGeometry());
        QWidget::closeEvent(e);
    }

private:
    Mode mode() const { return Mode(m_mode->currentData().toInt()); }
    Qt::CaseSensitivity cs() const { return m_case->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive; }

    void chooseDb()
    {
        const QString start = m_folder.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) : m_folder;
        const QString file = QFileDialog::getOpenFileName(this, QStringLiteral("ViviaMeals-Datenbank öffnen"), start,
                                                          QStringLiteral("ViviaMeals-Datenbank (viviameals.vmdb vitameals.vmdb *.vmdb);;Alle Dateien (*)"));
        if (!file.isEmpty()) loadDb(file);
    }

    void loadDb(const QString &file)
    {
        const QFileInfo fi(file);
        const QString folder = fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath();
        const QDir rez(QDir(folder).filePath(QStringLiteral("rezepte")));
        if (!rez.exists()) {
            QMessageBox::warning(this, windowTitle(), QStringLiteral("Im Ordner %1 gibt es keinen Unterordner „rezepte“ – das ist keine ViviaMeals-Datenbank.")
                                                          .arg(QDir::toNativeSeparators(folder)));
            return;
        }
        QApplication::setOverrideCursor(Qt::WaitCursor);
        QList<Entry> list;
        int bad = 0;
        QDirIterator it(rez.absolutePath(), { QStringLiteral("*.vmr") }, QDir::Files);
        while (it.hasNext()) {
            const QString f = it.next();
            QFile in(f);
            if (!in.open(QIODevice::ReadOnly)) {
                ++bad;
                continue;
            }
            const QJsonDocument d = QJsonDocument::fromJson(in.readAll());
            const QString fmt = d.object().value(QStringLiteral("format")).toString();
            if (!d.isObject() || (fmt != QLatin1String("ViviaMeals-Recipe") && fmt != QLatin1String("VitaMeals-Recipe"))) {
                ++bad;
                continue;
            }
            Entry e;
            e.file = f;
            e.json = d.object();
            e.title = e.json.value(QStringLiteral("title")).toString();
            e.source = e.json.value(QStringLiteral("source")).toString();
            list << e;
        }
        QApplication::restoreOverrideCursor();
        m_folder = folder;
        m_entries = list;
        m_dbEdit->setText(QDir::toNativeSeparators(folder) + QStringLiteral("  (%1 Rezepte%2)").arg(list.size()).arg(bad ? QStringLiteral(", %1 unlesbar").arg(bad) : QString()));
        QSettings().setValue(QStringLiteral("lastDb"), file);
        fillSources();
        updatePreview();
    }

    void fillSources()
    {
        QMap<QString, int> counts;
        for (const Entry &e : m_entries) counts[e.source] += 1;
        m_sources->setRowCount(0);
        for (auto it = counts.constBegin(); it != counts.constEnd(); ++it) {
            const int r = m_sources->rowCount();
            m_sources->insertRow(r);
            QString shown = it.key().isEmpty() ? QStringLiteral("(keine Angabe)") : it.key();
            shown.replace(QLatin1Char('\n'), QStringLiteral(" ⏎ "));
            auto *item = new QTableWidgetItem(shown);
            item->setData(Qt::UserRole, it.key());
            item->setToolTip(it.key());
            m_sources->setItem(r, 0, item);
            auto *c = new QTableWidgetItem(QString::number(it.value()));
            c->setTextAlignment(Qt::AlignCenter);
            m_sources->setItem(r, 1, c);
        }
        m_sources->sortItems(1, Qt::DescendingOrder);
    }

    // neue Quelle für ein Rezept; gleich der alten = nicht betroffen
    QString newSource(const QString &old) const
    {
        const QString find = m_find->text();
        const QString repl = m_replace->text().trimmed();
        switch (mode()) {
        case All: return repl;
        case Whole: return old.trimmed().compare(find.trimmed(), cs()) == 0 ? repl : old;
        case Contains: {
            if (find.isEmpty()) return old;
            QString s = old;
            s.replace(find, m_replace->text(), cs());
            return s == old ? old : s.trimmed();
        }
        }
        return old;
    }

    QList<int> affected() const
    {
        QList<int> l;
        if (mode() != All && m_find->text().isEmpty()) return l;
        for (int i = 0; i < m_entries.size(); ++i)
            if (newSource(m_entries[i].source) != m_entries[i].source) l << i;
        return l;
    }

    void updatePreview()
    {
        const QList<int> a = affected();
        m_run->setEnabled(!a.isEmpty());
        if (m_entries.isEmpty()) {
            m_preview->setText(QStringLiteral("Bitte zuerst eine Datenbank öffnen (Datei <i>viviameals.vmdb</i> im Datenbankordner)."));
            return;
        }
        if (a.isEmpty()) {
            m_preview->setText(mode() != All && m_find->text().isEmpty() ? QStringLiteral("Bitte den zu ersetzenden Text eingeben.")
                                                                         : QStringLiteral("Kein Rezept betroffen."));
            return;
        }
        const Entry &e = m_entries[a.first()];
        auto esc = [](QString s) { return s.replace(QLatin1Char('\n'), QStringLiteral(" ⏎ ")).toHtmlEscaped(); };
        const QString after = newSource(e.source);
        m_preview->setText(QStringLiteral("<b>%1 von %2 Rezepten</b> werden geändert. Beispiel „%3“:<br>„%4“ → „%5“")
                               .arg(a.size())
                               .arg(m_entries.size())
                               .arg(esc(e.title), esc(e.source), after.isEmpty() ? QStringLiteral("<i>(Quelle entfernt)</i>") : esc(after)));
    }

    void run()
    {
        const QList<int> a = affected();
        if (a.isEmpty()) return;
        QString what = mode() == All ? QStringLiteral("Die Quelle von %1 Rezepten wird durch „%2“ ersetzt (alle, deren Quelle noch anders lautet).").arg(a.size()).arg(m_replace->text().trimmed())
                                     : QStringLiteral("In %1 Rezepten wird „%2“ durch „%3“ ersetzt.").arg(a.size()).arg(m_find->text(), m_replace->text().trimmed());
        if (m_replace->text().trimmed().isEmpty()) what += QStringLiteral("\nLeere Quellen werden entfernt.");
        QMessageBox box(QMessageBox::Warning, windowTitle(),
                        what + QStringLiteral("\n\nBitte ViviaMeals vorher schließen (oder die Datenbank danach neu öffnen).\n%1\n\nWirklich ausführen?")
                                   .arg(m_backup->isChecked() ? QStringLiteral("Die betroffenen Rezepte werden vorher gesichert.")
                                                              : QStringLiteral("ACHTUNG: Es wird KEINE Sicherungskopie angelegt!")),
                        QMessageBox::Yes | QMessageBox::Cancel, this);
        box.setDefaultButton(QMessageBox::Cancel);
        box.button(QMessageBox::Yes)->setText(QStringLiteral("Ja, ersetzen"));
        if (box.exec() != QMessageBox::Yes) return;

        QString backupDir;
        if (m_backup->isChecked()) {
            backupDir = QDir(m_folder).filePath(QStringLiteral("sicherung-quelle-%1/rezepte").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"))));
            if (!QDir().mkpath(backupDir)) {
                QMessageBox::critical(this, windowTitle(), QStringLiteral("Der Sicherungsordner kann nicht angelegt werden:\n%1").arg(QDir::toNativeSeparators(backupDir)));
                return;
            }
            for (int i : a) {
                const QString f = m_entries[i].file;
                if (!QFile::copy(f, QDir(backupDir).filePath(QFileInfo(f).fileName()))) {
                    QMessageBox::critical(this, windowTitle(), QStringLiteral("Sicherung fehlgeschlagen bei %1 – es wurde nichts geändert.").arg(QFileInfo(f).fileName()));
                    return;
                }
            }
        }
        QApplication::setOverrideCursor(Qt::WaitCursor);
        int done = 0;
        QStringList errors;
        const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
        for (int i : a) {
            Entry &e = m_entries[i];
            const QString src = newSource(e.source);
            QJsonObject o = e.json;
            if (src.isEmpty()) o.remove(QStringLiteral("source"));
            else o[QStringLiteral("source")] = src;
            o[QStringLiteral("modified")] = now;
            QSaveFile out(e.file);
            if (!out.open(QIODevice::WriteOnly) || out.write(QJsonDocument(o).toJson(QJsonDocument::Indented)) < 0 || !out.commit()) {
                errors << QStringLiteral("%1: %2").arg(e.title, out.errorString());
                continue;
            }
            e.json = o;
            e.source = src;
            ++done;
        }
        QApplication::restoreOverrideCursor();
        fillSources();
        updatePreview();
        QString msg = QStringLiteral("%1 Rezepte geändert.").arg(done);
        if (!backupDir.isEmpty()) msg += QStringLiteral("\nSicherung: %1").arg(QDir::toNativeSeparators(QFileInfo(backupDir).absolutePath()));
        if (!errors.isEmpty()) {
            msg += QStringLiteral("\n\nFehler bei %1 Rezepten:\n%2").arg(errors.size()).arg(errors.mid(0, 10).join(QLatin1Char('\n')));
            QMessageBox::warning(this, windowTitle(), msg);
        } else {
            QMessageBox::information(this, windowTitle(), msg);
        }
    }

    QString m_folder;
    QList<Entry> m_entries;
    QLineEdit *m_dbEdit, *m_find, *m_replace;
    QTableWidget *m_sources;
    QComboBox *m_mode;
    QCheckBox *m_case, *m_backup;
    QLabel *m_preview;
    QPushButton *m_run;
};

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("mbergmann-sh"));
    QApplication::setApplicationName(QStringLiteral("ViviaMealsCheat"));
    QApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/viviamealscheat.svg")));
    Window w;
    w.show();
    return app.exec();
}
