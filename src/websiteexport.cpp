// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "websiteexport.h"
#include "recipedb.h"
#include "recipehtml.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QRegularExpression>
#include <QSaveFile>
#include <algorithm>

namespace WebsiteExport {

namespace {

QString tr(const char *s) { return QCoreApplication::translate("WebsiteExport", s); }
QString esc(const QString &s) { return s.toHtmlEscaped(); }

bool writeText(const QString &path, const QString &text, QString *error)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        if (error) *error = QStringLiteral("%1: %2").arg(path, f.errorString());
        return false;
    }
    f.write(text.toUtf8());
    if (!f.commit()) {
        if (error) *error = f.errorString();
        return false;
    }
    return true;
}

const char *kCss = R"CSS(/* ViviaMeals – Website (erzeugt) */
:root { --blau:#1474BC; --orange:#E0620D; --gruen:#2e9e6b; --text:#222; --grau:#6b6b6b; --linie:#dde3ea; --hell:#f4f7fa; }
* { box-sizing:border-box; }
body { margin:0; font-family:system-ui,-apple-system,"Segoe UI",Roboto,"Helvetica Neue",Arial,sans-serif; color:var(--text); background:#fff; line-height:1.5; }
a { color:var(--blau); text-decoration:none; } a:hover { text-decoration:underline; }
header.top { background:linear-gradient(180deg,#1A82CC,#0F67AB); color:#fff; padding:14px 20px; display:flex; flex-wrap:wrap; align-items:center; gap:16px; }
header.top a.brand { color:#fff; display:flex; align-items:center; gap:12px; font-size:1.45rem; font-weight:700; }
header.top a.brand img { width:44px; height:44px; }
header.top .sub { font-size:.9rem; opacity:.85; font-weight:400; display:block; }
header.top form { margin-left:auto; display:flex; gap:6px; }
header.top input[type=search] { padding:8px 10px; border-radius:6px; border:0; min-width:220px; font-size:1rem; }
header.top button { padding:8px 14px; border:0; border-radius:6px; background:var(--orange); color:#fff; font-weight:600; cursor:pointer; }
.layout { display:flex; max-width:1200px; margin:0 auto; }
nav.themes { width:230px; flex:none; padding:18px 14px; border-right:1px solid var(--linie); }
nav.themes h2 { font-size:.85rem; text-transform:uppercase; letter-spacing:.05em; color:var(--grau); margin:0 0 8px; }
nav.themes ul { list-style:none; margin:0; padding:0; }
nav.themes li a { display:flex; justify-content:space-between; padding:5px 8px; border-radius:6px; color:var(--text); }
nav.themes li a:hover, nav.themes li a.active { background:var(--hell); color:var(--blau); text-decoration:none; }
nav.themes .count { color:var(--grau); font-size:.85rem; }
main { flex:1; padding:18px 24px 40px; min-width:0; }
h1 { color:var(--blau); margin:.2em 0 .3em; line-height:1.2; }
h2 { color:var(--blau); border-bottom:2px solid var(--linie); padding-bottom:4px; margin-top:1.6em; }
.grid { display:grid; grid-template-columns:repeat(auto-fill,minmax(210px,1fr)); gap:16px; }
.card { border:1px solid var(--linie); border-radius:10px; overflow:hidden; background:#fff; display:flex; flex-direction:column; transition:box-shadow .15s; }
.card:hover { box-shadow:0 4px 14px rgba(0,0,0,.08); }
.card .thumb { aspect-ratio:4/3; background:var(--hell) center/cover no-repeat; display:flex; align-items:center; justify-content:center; color:#b7c4d2; font-size:2.4rem; }
.card .body { padding:10px 12px 12px; }
.card .title { font-weight:600; color:var(--text); }
.card .kcal { color:var(--orange); font-weight:600; font-size:.92rem; }
.tags { display:flex; flex-wrap:wrap; gap:4px; margin:6px 0; }
.tag { background:var(--blau); color:#fff; border-radius:4px; padding:1px 7px; font-size:.78rem; }
.tag.veg { background:var(--gruen); }
.recipe-head { display:flex; gap:24px; align-items:flex-start; flex-wrap:wrap-reverse; }
.recipe-head .info { flex:1; min-width:240px; }
.recipe-head img { width:min(360px,100%); border-radius:10px; }
.big-kcal { color:var(--orange); font-size:1.7rem; font-weight:700; }
.muted, .small, .note { color:var(--grau); } .small { font-size:.85rem; }
table.ingredients { border-collapse:collapse; }
table.ingredients td { padding:3px 8px; vertical-align:top; }
table.ingredients td.amount { text-align:right; font-weight:600; white-space:nowrap; }
table.ingredients tr.group td { padding-top:12px; font-weight:700; color:var(--blau); }
table.nutrition { border-collapse:collapse; min-width:min(520px,100%); }
table.nutrition th, table.nutrition td { border:1px solid var(--linie); padding:5px 10px; text-align:right; }
table.nutrition th:first-child, table.nutrition td:first-child { text-align:left; }
table.nutrition thead th { background:var(--hell); }
table.nutrition .sub { padding-left:1.2em; display:inline-block; }
footer { border-top:1px solid var(--linie); color:var(--grau); font-size:.85rem; padding:14px 20px; text-align:center; }
.hidden { display:none !important; }
@media (max-width:760px) { .layout { flex-direction:column; } nav.themes { width:auto; border-right:0; border-bottom:1px solid var(--linie); }
  nav.themes ul { display:flex; flex-wrap:wrap; gap:4px; } header.top form { margin-left:0; width:100%; } header.top input[type=search] { flex:1; min-width:0; } }
@media print { header.top form, nav.themes, footer { display:none; } .layout { display:block; } }
)CSS";

const char *kJs = R"JS(// ViviaMeals – Sofortsuche ohne Server (erzeugt). Ohne JavaScript übernimmt suche.php.
(function () {
  var form = document.getElementById('vm-search'); if (!form) return;
  var input = form.querySelector('input[type=search]');
  var root = form.getAttribute('data-root') || '';
  var grid = document.getElementById('vm-grid');
  if (!grid) {   // Rezeptseite: lokal (ohne PHP) zur Sofortsuche der Startseite springen
    form.addEventListener('submit', function (e) { if (location.protocol === 'file:') { e.preventDefault();
      location.href = root + 'index.html?q=' + encodeURIComponent(input.value); } });
    return;
  }
  var cards = Array.prototype.slice.call(grid.querySelectorAll('.card'));
  var info = document.getElementById('vm-info');
  function norm(s) { return (s || '').toLowerCase(); }
  function filter() {
    var words = norm(input.value).split(/\s+/).filter(Boolean), shown = 0;
    cards.forEach(function (c) {
      var hay = norm(c.getAttribute('data-search'));
      var ok = words.every(function (w) { return hay.indexOf(w) >= 0; });
      c.classList.toggle('hidden', !ok); if (ok) shown++;
    });
    if (info) info.textContent = words.length ? shown + ' Rezept' + (shown === 1 ? '' : 'e') + ' gefunden' : '';
  }
  input.addEventListener('input', filter);
  form.addEventListener('submit', function (e) { e.preventDefault(); filter(); });
  var q = new URLSearchParams(location.search).get('q'); if (q) { input.value = q; filter(); }
})();
)JS";

const char *kPhp = R"PHP(<?php
// ViviaMeals – Suche auf dem Server (erzeugt). Benötigt PHP 7.4 oder neuer.
declare(strict_types=1);
header('Content-Type: text/html; charset=utf-8');
$q = trim((string)($_GET['q'] ?? ''));
$data = json_decode((string)@file_get_contents(__DIR__ . '/daten/rezepte.json'), true);
$site = $data['site'] ?? ['title' => 'Rezepte', 'subtitle' => '', 'themes' => []];
$recipes = $data['recipes'] ?? [];
function h(string $s): string { return htmlspecialchars($s, ENT_QUOTES, 'UTF-8'); }
function low(string $s): string { return function_exists('mb_strtolower') ? mb_strtolower($s, 'UTF-8') : strtolower($s); }
$words = array_values(array_filter(preg_split('/\s+/u', low($q)) ?: []));
$hits = [];
foreach ($recipes as $r) {
    $hay = low($r['title'] . ' ' . implode(' ', $r['tags']) . ' ' . implode(' ', $r['ingredients']));
    $ok = true;
    foreach ($words as $w) { if (strpos($hay, $w) === false) { $ok = false; break; } }
    if ($ok && $words) $hits[] = $r;
}
?><!DOCTYPE html>
<html lang="de"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Suche: <?= h($q) ?> – <?= h($site['title']) ?></title><link rel="stylesheet" href="assets/style.css"></head>
<body><header class="top"><a class="brand" href="index.html"><img src="assets/logo.svg" alt=""><span><?= h($site['title']) ?>
<span class="sub"><?= h($site['subtitle']) ?></span></span></a>
<form action="suche.php" method="get" role="search"><input type="search" name="q" value="<?= h($q) ?>" placeholder="Rezept, Zutat oder Thema …"><button>Suchen</button></form></header>
<div class="layout"><nav class="themes"><h2>Themen</h2><ul>
<?php foreach ($site['themes'] as $t): ?><li><a href="<?= h($t['url']) ?>"><span><?= h($t['label']) ?></span><span class="count"><?= (int)$t['count'] ?></span></a></li><?php endforeach; ?>
</ul></nav><main><h1>Suche</h1>
<p class="muted"><?= $words ? count($hits) . ' Rezept' . (count($hits) === 1 ? '' : 'e') . ' für „' . h($q) . '“' : 'Bitte einen Suchbegriff eingeben.' ?></p>
<div class="grid"><?php foreach ($hits as $r): ?><a class="card" href="<?= h($r['url']) ?>">
<div class="thumb"<?= $r['image'] ? ' style="background-image:url(\'' . h($r['image']) . '\')"' : '' ?>><?= $r['image'] ? '' : '🍽' ?></div>
<div class="body"><div class="title"><?= h($r['title']) ?></div><?php if ($r['kcal'] > 0): ?><div class="kcal"><?= (int)$r['kcal'] ?> kcal je Portion</div><?php endif; ?></div></a>
<?php endforeach; ?></div></main></div><footer><?= h($site['footer'] ?? '') ?></footer></body></html>
)PHP";

const char *kLogo = R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256"><rect x="8" y="8" width="240" height="240" rx="48" fill="#fff" fill-opacity=".14"/><circle cx="128" cy="140" r="78" fill="#fff"/><circle cx="128" cy="140" r="56" fill="none" stroke="#1474BC" stroke-width="8"/><path d="M128 72c26-30 62-30 74-26-2 30-26 56-60 58" fill="#E0620D"/><path d="M60 196l136-108" stroke="#0F67AB" stroke-width="0"/></svg>)SVG";

struct Item {
    QString id, title, slug, file, image;
    QStringList tags, ingredients;
    double kcal = 0;
};

QString shell(const Options &opt, const QString &root, const QString &title, const QString &nav, const QString &body)
{
    QString h;
    h += QStringLiteral("<!DOCTYPE html>\n<html lang=\"de\"><head><meta charset=\"utf-8\">"
                        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
                        "<meta name=\"generator\" content=\"ViviaMeals\">");
    h += QStringLiteral("<title>%1</title><link rel=\"stylesheet\" href=\"%2assets/style.css\"></head><body>\n")
             .arg(esc(title == opt.title ? title : title + QStringLiteral(" – ") + opt.title), root);
    h += QStringLiteral("<header class=\"top\"><a class=\"brand\" href=\"%1index.html\"><img src=\"%1assets/logo.svg\" alt=\"\"><span>%2"
                        "<span class=\"sub\">%3</span></span></a>")
             .arg(root, esc(opt.title), esc(opt.subtitle));
    h += QStringLiteral("<form id=\"vm-search\" data-root=\"%1\" action=\"%1suche.php\" method=\"get\" role=\"search\">"
                        "<input type=\"search\" name=\"q\" placeholder=\"%2\" aria-label=\"%3\"><button>%3</button></form></header>\n")
             .arg(root, tr("Rezept, Zutat oder Thema …"), tr("Suchen"));
    h += QStringLiteral("<div class=\"layout\">%1<main>%2</main></div>\n").arg(nav, body);
    h += QStringLiteral("<footer>%1</footer><script src=\"%2assets/suche.js\"></script></body></html>\n")
             .arg(esc(tr("Erstellt mit ViviaMeals am %1").arg(QDate::currentDate().toString(QStringLiteral("dd.MM.yyyy")))
                      + (opt.author.isEmpty() ? QString() : QStringLiteral(" · ") + opt.author)),
                  root);
    return h;
}

QString card(const Item &it, const QString &root)
{
    QString tags;
    for (const QString &t : it.tags.mid(0, 4))
        tags += QStringLiteral("<span class=\"tag%1\">%2</span>")
                    .arg(t == QLatin1String("vegan") || t == QLatin1String("vegetarisch") ? QStringLiteral(" veg") : QString(),
                         esc(RecipeHtml::tagLabel(t)));
    const QString search = (it.title + QLatin1Char(' ') + it.tags.join(QLatin1Char(' ')) + QLatin1Char(' ') + it.ingredients.join(QLatin1Char(' '))).toLower();
    return QStringLiteral("<a class=\"card\" href=\"%1%2\" data-search=\"%3\"><div class=\"thumb\"%4>%5</div>"
                          "<div class=\"body\"><div class=\"title\">%6</div>%7<div class=\"tags\">%8</div></div></a>\n")
        .arg(root, it.file, esc(search),
             it.image.isEmpty() ? QString() : QStringLiteral(" style=\"background-image:url('%1%2')\"").arg(root, it.image),
             it.image.isEmpty() ? QStringLiteral("🍽") : QString(), esc(it.title),
             it.kcal > 0 ? QStringLiteral("<div class=\"kcal\">%1</div>").arg(tr("%1 kcal je Portion").arg(qRound(it.kcal))) : QString(), tags);
}

} // namespace

QString slug(const QString &text)
{
    QString s = text.toLower();
    s.replace(QStringLiteral("ä"), QStringLiteral("ae"));
    s.replace(QStringLiteral("ö"), QStringLiteral("oe"));
    s.replace(QStringLiteral("ü"), QStringLiteral("ue"));
    s.replace(QStringLiteral("ß"), QStringLiteral("ss"));
    s = s.normalized(QString::NormalizationForm_D);
    static const QRegularExpression kRe216_0(QStringLiteral("[\\x{0300}-\\x{036f}]"));
    s.remove(kRe216_0);
    static const QRegularExpression kRe217_0(QStringLiteral("[^a-z0-9]+"));
    s.replace(kRe217_0, QStringLiteral("-"));
    static const QRegularExpression kRe218_0(QStringLiteral("^-+|-+$"));
    s.remove(kRe218_0);
    if (s.size() > 60) s = s.left(60);
    return s.isEmpty() ? QStringLiteral("rezept") : s;
}

bool exportSite(const QString &folder, const Options &opt, QString *error, const std::function<bool(int, int)> &progress)
{
    RecipeDb &db = RecipeDb::instance();
    QDir out(folder);
    if (!out.exists() && !QDir().mkpath(folder)) {
        if (error) *error = tr("Der Zielordner kann nicht angelegt werden.");
        return false;
    }
    for (const char *d : { "rezepte", "themen", "bilder", "assets", "daten" }) out.mkpath(QLatin1String(d));

    // Rezepte sammeln
    QList<Item> items;
    QHash<QString, int> used;
    QStringList ids;
    for (const RecipeSummary &s : db.summaries())
        if (opt.onlyIds.isEmpty() || opt.onlyIds.contains(s.id)) ids << s.id;
    const int total = int(ids.size());
    RecipeHtml::Style st;
    st.forWeb = true;
    st.imageWidth = 360;
    int done = 0;
    for (const QString &id : ids) {
        Recipe r;
        if (!db.load(id, &r)) continue;
        Item it;
        it.id = id;
        it.title = r.title;
        QString sl = slug(r.title);
        if (used.contains(sl)) sl += QStringLiteral("-%1").arg(++used[sl]);
        else used.insert(sl, 1);
        it.slug = sl;
        it.file = QStringLiteral("rezepte/%1.html").arg(sl);
        it.tags = r.tags;
        for (const Ingredient &i : r.ingredients)
            if (!i.heading) it.ingredients << i.name;
        it.kcal = r.nutrition.valid() ? r.nutrition.perServing.kcal : 0;
        if (!r.image.isEmpty()) {
            QImage img;
            img.loadFromData(QByteArray::fromBase64(r.image.toLatin1()));
            if (!img.isNull()) {
                it.image = QStringLiteral("bilder/%1.jpg").arg(sl);
                img.save(out.filePath(it.image), "JPG", 85);
            }
        }
        // Rezeptseite
        st.imageSrc = it.image.isEmpty() ? QString() : QStringLiteral("../") + it.image;
        QString body;
        body += QStringLiteral("<div class=\"recipe-head\"><div class=\"info\"><h1>%1</h1>").arg(esc(r.title));
        body += QStringLiteral("<p><b>%1</b>").arg(esc(r.servingsText()));
        if (r.prepMinutes > 0) body += QStringLiteral(" · ") + tr("%1 Minuten").arg(r.prepMinutes);
        body += QStringLiteral("</p>");
        if (opt.nutrition && it.kcal > 0)
            body += QStringLiteral("<p><span class=\"big-kcal\">%1 kcal</span> <span class=\"muted\">%2 kJ %3</span></p>")
                        .arg(qRound(r.nutrition.perServing.kcal))
                        .arg(qRound(r.nutrition.perServing.kJ()))
                        .arg(r.yieldUnit.isEmpty() ? tr("je Portion") : tr("je %1").arg(esc(r.yieldUnit)));
        body += QStringLiteral("<div class=\"tags\">");
        for (const QString &t : r.tags)
            body += QStringLiteral("<a class=\"tag%1\" href=\"../themen/%2.html\">%3</a>")
                        .arg(t == QLatin1String("vegan") || t == QLatin1String("vegetarisch") ? QStringLiteral(" veg") : QString(), slug(t),
                             esc(RecipeHtml::tagLabel(t)));
        body += QStringLiteral("</div></div>");
        if (!it.image.isEmpty()) body += QStringLiteral("<img src=\"../%1\" alt=\"%2\">").arg(it.image, esc(r.title));
        body += QStringLiteral("</div>");
        body += QStringLiteral("<h2>%1</h2>").arg(tr("Zutaten")) + RecipeHtml::ingredientsTable(r, st);
        body += QStringLiteral("<h2>%1</h2>").arg(tr("Zubereitung"));
        if (!RecipeHtml::languageNote(r).isEmpty())
            body += QStringLiteral("<p class=\"muted\"><i>%1</i></p>").arg(esc(RecipeHtml::languageNote(r)));
        body += RecipeHtml::instructionsHtml(r);
        if (opt.nutrition) body += QStringLiteral("<h2>%1</h2>").arg(tr("Nährwerte")) + RecipeHtml::nutritionTable(r, st);
        if (!r.source.isEmpty()) body += QStringLiteral("<p class=\"small\">%1 %2</p>").arg(tr("Quelle:"), esc(r.source));
        static const QRegularExpression kRe294_0(QStringLiteral("\\s+"));
        it.ingredients.replaceInStrings(kRe294_0, QStringLiteral(" "));
        items << it;
        // Navigation wird nach dem Sammeln geschrieben – Rezeptseite vorläufig merken
        if (!writeText(out.filePath(it.file), body, error)) return false;
        ++done;
        if (progress && !progress(done, total)) {
            if (error) *error = tr("Abgebrochen.");
            return false;
        }
    }

    // Themen (Tags) mit Anzahl
    QMap<QString, QList<int>> themes;
    for (int i = 0; i < items.size(); ++i)
        for (const QString &t : items[i].tags) themes[t].append(i);
    auto nav = [&](const QString &root, const QString &active) {
        QString n = QStringLiteral("<nav class=\"themes\"><h2>%1</h2><ul>").arg(tr("Themen"));
        n += QStringLiteral("<li><a href=\"%1index.html\"%2><span>%3</span><span class=\"count\">%4</span></a></li>")
                 .arg(root, active.isEmpty() ? QStringLiteral(" class=\"active\"") : QString(), tr("Alle Rezepte"))
                 .arg(items.size());
        for (auto it = themes.constBegin(); it != themes.constEnd(); ++it)
            n += QStringLiteral("<li><a href=\"%1themen/%2.html\"%3><span>%4</span><span class=\"count\">%5</span></a></li>")
                     .arg(root, slug(it.key()), it.key() == active ? QStringLiteral(" class=\"active\"") : QString(),
                          esc(RecipeHtml::tagLabel(it.key())))
                     .arg(it.value().size());
        n += QStringLiteral("</ul></nav>");
        return n;
    };
    // Rezeptseiten in den Rahmen setzen
    for (const Item &it : items) {
        QFile f(out.filePath(it.file));
        if (!f.open(QIODevice::ReadOnly)) continue;
        const QString body = QString::fromUtf8(f.readAll());
        f.close();
        if (!writeText(out.filePath(it.file), shell(opt, QStringLiteral("../"), it.title, nav(QStringLiteral("../"), QString()), body), error))
            return false;
    }
    // Startseite
    auto grid = [&](const QList<int> &idx, const QString &root) {
        QString g = QStringLiteral("<p id=\"vm-info\" class=\"muted\"></p><div class=\"grid\" id=\"vm-grid\">");
        for (int i : idx) g += card(items[i], root);
        g += QStringLiteral("</div>");
        return g;
    };
    QList<int> all;
    for (int i = 0; i < items.size(); ++i) all << i;
    const QString indexBody = QStringLiteral("<h1>%1</h1><p class=\"muted\">%2</p>")
                                  .arg(esc(opt.title), items.size() == 1 ? tr("1 Rezept") : tr("%1 Rezepte").arg(items.size()));
    if (!writeText(out.filePath(QStringLiteral("index.html")), shell(opt, QString(), opt.title, nav(QString(), QString()), indexBody + grid(all, QString())), error))
        return false;
    for (auto it = themes.constBegin(); it != themes.constEnd(); ++it) {
        const QString body = QStringLiteral("<h1>%1</h1><p class=\"muted\">%2 Rezept(e)</p>").arg(esc(RecipeHtml::tagLabel(it.key()))).arg(it.value().size())
                             + grid(it.value(), QStringLiteral("../"));
        if (!writeText(out.filePath(QStringLiteral("themen/%1.html").arg(slug(it.key()))),
                       shell(opt, QStringLiteral("../"), RecipeHtml::tagLabel(it.key()), nav(QStringLiteral("../"), it.key()), body), error))
            return false;
    }
    // Suchindex für suche.php und suche.js
    QJsonArray rec;
    for (const Item &it : items)
        rec.append(QJsonObject{ { QStringLiteral("title"), it.title },
                                { QStringLiteral("url"), it.file },
                                { QStringLiteral("image"), it.image },
                                { QStringLiteral("kcal"), qRound(it.kcal) },
                                { QStringLiteral("tags"), QJsonArray::fromStringList(it.tags) },
                                { QStringLiteral("ingredients"), QJsonArray::fromStringList(it.ingredients) } });
    QJsonArray th;
    for (auto it = themes.constBegin(); it != themes.constEnd(); ++it)
        th.append(QJsonObject{ { QStringLiteral("label"), RecipeHtml::tagLabel(it.key()) },
                               { QStringLiteral("url"), QStringLiteral("themen/%1.html").arg(slug(it.key())) },
                               { QStringLiteral("count"), int(it.value().size()) } });
    const QJsonObject site{ { QStringLiteral("title"), opt.title },
                            { QStringLiteral("subtitle"), opt.subtitle },
                            { QStringLiteral("themes"), th },
                            { QStringLiteral("footer"), tr("Erstellt mit ViviaMeals") } };
    if (!writeText(out.filePath(QStringLiteral("daten/rezepte.json")),
                   QString::fromUtf8(QJsonDocument(QJsonObject{ { QStringLiteral("site"), site }, { QStringLiteral("recipes"), rec } }).toJson()), error))
        return false;
    if (!writeText(out.filePath(QStringLiteral("assets/style.css")), QString::fromUtf8(kCss), error)) return false;
    if (!writeText(out.filePath(QStringLiteral("assets/suche.js")), QString::fromUtf8(kJs), error)) return false;
    if (!writeText(out.filePath(QStringLiteral("assets/logo.svg")), QString::fromUtf8(kLogo), error)) return false;
    // Logo des Programms bevorzugen
    QFile appLogo(QStringLiteral(":/icons/viviameals.svg"));
    if (appLogo.open(QIODevice::ReadOnly)) writeText(out.filePath(QStringLiteral("assets/logo.svg")), QString::fromUtf8(appLogo.readAll()), error);
    if (!writeText(out.filePath(QStringLiteral("suche.php")), QString::fromUtf8(kPhp), error)) return false;
    writeText(out.filePath(QStringLiteral("LIESMICH.txt")),
              tr("Diese Website wurde von ViviaMeals erzeugt.\n\n"
                 "Hochladen: den gesamten Ordnerinhalt per FTP/SFTP auf den Webserver kopieren.\n"
                 "Die Suche funktioniert im Browser sofort (JavaScript) und zusätzlich serverseitig über suche.php (PHP 7.4+).\n"
                 "Startseite: index.html\n"),
              nullptr);
    return true;
}

} // namespace WebsiteExport
