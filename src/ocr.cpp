// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "ocr.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>

#include <algorithm>

namespace Ocr {

namespace {

QString exeName()
{
#ifdef Q_OS_WIN
    return QStringLiteral("tesseract.exe");
#else
    return QStringLiteral("tesseract");
#endif
}

bool usable(const QString &f)
{
    const QFileInfo fi(f);
    return fi.exists() && fi.isFile() && fi.isExecutable();
}

QString runShort(const QString &exe, const QStringList &args)
{
    QProcess p;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (const QString &e : environment(exe)) env.insert(e.section(QLatin1Char('='), 0, 0), e.section(QLatin1Char('='), 1));
    p.setProcessEnvironment(env);
    p.start(exe, args);
    if (!p.waitForStarted(5000) || !p.waitForFinished(15000)) {
        p.kill();
        return QString();
    }
    // ältere Versionen schreiben --version nach stderr
    return QString::fromUtf8(p.readAllStandardOutput() + '\n' + p.readAllStandardError());
}

} // namespace

QStringList searchLocations()
{
    QStringList l;
    l << QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("tesseract/") + exeName());
#if defined(Q_OS_WIN)
    const QString pf = QString::fromLocal8Bit(qgetenv("ProgramFiles"));
    const QString pf86 = QString::fromLocal8Bit(qgetenv("ProgramFiles(x86)"));
    const QString local = QString::fromLocal8Bit(qgetenv("LOCALAPPDATA"));
    l << (pf.isEmpty() ? QStringLiteral("C:/Program Files") : QDir::fromNativeSeparators(pf)) + QStringLiteral("/Tesseract-OCR/tesseract.exe");
    if (!pf86.isEmpty()) l << QDir::fromNativeSeparators(pf86) + QStringLiteral("/Tesseract-OCR/tesseract.exe");
    if (!local.isEmpty()) {
        l << QDir::fromNativeSeparators(local) + QStringLiteral("/Programs/Tesseract-OCR/tesseract.exe");
        l << QDir::fromNativeSeparators(local) + QStringLiteral("/Tesseract-OCR/tesseract.exe");
    }
#elif defined(Q_OS_MACOS)
    l << QStringLiteral("/opt/homebrew/bin/tesseract") << QStringLiteral("/usr/local/bin/tesseract") << QStringLiteral("/opt/local/bin/tesseract");
#else
    l << QStringLiteral("/usr/bin/tesseract") << QStringLiteral("/usr/local/bin/tesseract");
#endif
    return l;
}

QString findTesseract(const QString &configured)
{
    if (!configured.trimmed().isEmpty()) {
        QString c = configured.trimmed();
        if (QFileInfo(c).isDir()) c = QDir(c).filePath(exeName());
        if (usable(c)) return QDir::toNativeSeparators(QFileInfo(c).absoluteFilePath());
    }
    const QStringList locs = searchLocations();
    // mitgelieferte Version zuerst, dann PATH, dann die üblichen Orte
    if (usable(locs.first())) return QDir::toNativeSeparators(locs.first());
    const QString inPath = QStandardPaths::findExecutable(QStringLiteral("tesseract"));
    if (!inPath.isEmpty()) return QDir::toNativeSeparators(inPath);
    for (int i = 1; i < locs.size(); ++i)
        if (usable(locs.at(i))) return QDir::toNativeSeparators(locs.at(i));
    return QString();
}

QStringList environment(const QString &exe)
{
    // Mitgelieferte bzw. kopierte Installation: Sprachdaten liegen neben dem Programm
    if (qEnvironmentVariableIsSet("TESSDATA_PREFIX") || exe.isEmpty()) return {};
    const QString td = QDir(QFileInfo(exe).absolutePath()).filePath(QStringLiteral("tessdata"));
    if (QFileInfo(td).isDir()) return { QStringLiteral("TESSDATA_PREFIX=") + QDir::toNativeSeparators(td) };
    return {};
}

QString version(const QString &exe)
{
    if (exe.isEmpty()) return QString();
    static const QRegularExpression re(QStringLiteral("tesseract\\s+v?(\\d+\\.\\d+(?:\\.\\d+)?)"), QRegularExpression::CaseInsensitiveOption);
    const auto m = re.match(runShort(exe, { QStringLiteral("--version") }));
    return m.hasMatch() ? m.captured(1) : QString();
}

QStringList languages(const QString &exe)
{
    QStringList l;
    if (exe.isEmpty()) return l;
    const QStringList lines = runShort(exe, { QStringLiteral("--list-langs") }).split(QLatin1Char('\n'));
    static const QRegularExpression code(QStringLiteral("^[A-Za-z_]{3,}(?:_[A-Za-z]+)?$"));
    for (const QString &raw : lines) {
        const QString s = raw.trimmed();
        if (s.isEmpty() || s.contains(QLatin1Char(' ')) || !code.match(s).hasMatch() || s == QLatin1String("osd")) continue;
        if (!l.contains(s)) l << s;
    }
    return l;
}

QString defaultLanguages(const QStringList &installed)
{
    QStringList want;
    for (const char *c : { "deu", "eng" })
        if (installed.contains(QLatin1String(c))) want << QLatin1String(c);
    if (want.isEmpty() && !installed.isEmpty()) want << installed.first();
    return want.isEmpty() ? QStringLiteral("deu+eng") : want.join(QLatin1Char('+'));
}

QStringList arguments(const QString &imageFile, const QString &langs)
{
    // --psm 3: automatische Seitenaufteilung (Spalten, Blöcke); --dpi: Scans ohne Auflösungsangabe
    return { imageFile, QStringLiteral("stdout"), QStringLiteral("-l"), langs.isEmpty() ? QStringLiteral("deu+eng") : langs,
             QStringLiteral("--psm"), QStringLiteral("3"), QStringLiteral("--dpi"), QStringLiteral("300"), QStringLiteral("tsv") };
}

QList<Word> parseTsv(const QByteArray &tsv)
{
    QList<Word> words;
    const QList<QByteArray> rows = tsv.split('\n');
    for (const QByteArray &row : rows) {
        const QList<QByteArray> c = row.split('\t');
        if (c.size() < 12 || c.at(0) != "5") continue;   // Ebene 5 = Wort
        Word w;
        w.text = QString::fromUtf8(c.mid(11).join('\t')).trimmed();
        if (w.text.isEmpty()) continue;
        w.block = c.at(2).toInt();
        w.par = c.at(3).toInt();
        w.line = c.at(4).toInt();
        w.box = QRect(c.at(6).toInt(), c.at(7).toInt(), c.at(8).toInt(), c.at(9).toInt());
        w.conf = c.at(10).toDouble();
        words << w;
    }
    return words;
}

QList<Line> toLines(const QList<Word> &words, double minConf)
{
    QList<Line> lines;
    QHash<QString, int> index;
    QList<int> wordCount;
    for (const Word &w : words) {
        const QString key = QStringLiteral("%1/%2/%3").arg(w.block).arg(w.par).arg(w.line);
        int i = index.value(key, -1);
        if (i < 0) {
            i = int(lines.size());
            index.insert(key, i);
            Line l;
            l.block = w.block;
            l.par = w.par;
            l.box = w.box;
            lines << l;
            wordCount << 0;
        }
        Line &l = lines[i];
        l.text += (l.text.isEmpty() ? QString() : QStringLiteral(" ")) + w.text;
        l.box = l.box.united(w.box);
        l.conf += w.conf;
        ++wordCount[i];
    }
    QList<Line> out;
    for (int i = 0; i < lines.size(); ++i) {
        Line l = lines.at(i);
        l.conf /= qMax(1, wordCount.at(i));
        // Rauschen (Bildreste, Rasterpunkte) verwerfen: niedrige Sicherheit ohne Buchstaben/Ziffern
        static const QRegularExpression alnum(QStringLiteral("[\\p{L}\\d]"));
        if (l.conf < minConf && !l.text.contains(alnum)) continue;
        if (l.conf < minConf / 2) continue;
        out << l;
    }
    return out;
}

QString plainText(const QList<Line> &lines)
{
    QStringList t;
    int block = -1, par = -1;
    for (const Line &l : lines) {
        if (!t.isEmpty() && (l.block != block || l.par != par)) t << QString();
        t << l.text;
        block = l.block;
        par = l.par;
    }
    return t.join(QLatin1Char('\n'));
}

bool recognize(const QImage &img, const QString &exe, const QString &langs, QList<Line> *lines, QString *error, int timeoutMs)
{
    auto fail = [error](const QString &e) {
        if (error) *error = e;
        return false;
    };
    if (exe.isEmpty()) return fail(QObject::tr("Tesseract ist nicht installiert."));
    // Bild über stdin übergeben – vermeidet Probleme mit Umlauten in Pfaden (Windows-Benutzernamen)
    QByteArray png;
    {
        QBuffer buf(&png);
        buf.open(QIODevice::WriteOnly);
        if (!img.save(&buf, "PNG")) return fail(QObject::tr("Bild kann nicht übergeben werden."));
    }
    QProcess p;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (const QString &e : environment(exe)) env.insert(e.section(QLatin1Char('='), 0, 0), e.section(QLatin1Char('='), 1));
    p.setProcessEnvironment(env);
    p.start(exe, arguments(QStringLiteral("stdin"), langs));
    if (!p.waitForStarted(10000)) return fail(QObject::tr("Tesseract kann nicht gestartet werden: %1").arg(p.errorString()));
    p.write(png);
    p.closeWriteChannel();
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        return fail(QObject::tr("Die Texterkennung hat zu lange gedauert."));
    }
    const QByteArray out = p.readAllStandardOutput();
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
        QString err = QString::fromUtf8(p.readAllStandardError()).trimmed();
        if (err.size() > 400) err = err.left(400) + QStringLiteral(" …");
        return fail(QObject::tr("Tesseract meldet einen Fehler: %1").arg(err));
    }
    if (lines) *lines = toLines(parseTsv(out));
    return true;
}

QString installHint()
{
#if defined(Q_OS_WIN)
    return QObject::tr("Für die Texterkennung wird das kostenlose Programm Tesseract benötigt. Unter Windows empfiehlt "
                       "sich der Installer der UB Mannheim (https://github.com/UB-Mannheim/tesseract/wiki); bei der "
                       "Installation unter „Additional language data“ die Sprache „German“ auswählen. Nach der "
                       "Installation findet ViviaMeals Tesseract in „C:\\Program Files\\Tesseract-OCR“ von selbst; "
                       "ein anderer Ort lässt sich unter Extras → Einstellungen → Scannen eintragen.");
#elif defined(Q_OS_MACOS)
    return QObject::tr("Für die Texterkennung wird das kostenlose Programm Tesseract benötigt. Unter macOS z. B. mit "
                       "Homebrew installieren: „brew install tesseract tesseract-lang“. Ein anderer Ort lässt sich "
                       "unter Extras → Einstellungen → Scannen eintragen.");
#else
    return QObject::tr("Für die Texterkennung wird das kostenlose Programm Tesseract benötigt. Installation z. B. unter "
                       "Debian/Ubuntu mit „sudo apt install tesseract-ocr tesseract-ocr-deu“, unter Fedora mit "
                       "„sudo dnf install tesseract tesseract-langpack-deu“. Ein anderer Ort lässt sich unter "
                       "Extras → Einstellungen → Scannen eintragen.");
#endif
}

} // namespace Ocr
