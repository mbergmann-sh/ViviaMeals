// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "profiles.h"
#include "diary.h"
#include "recipe.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
#include <algorithm>
#include <cmath>

namespace {
QString tr(const char *s) { return QCoreApplication::translate("Profile", s); }
}

double Profile::currentKg() const { return weights.isEmpty() ? 0.0 : weights.last().kg; }

int Profile::age(const QDate &on) const { return qMax(1, on.year() - birthYear); }

double Profile::bmi(double kg) const
{
    if (kg < 0) kg = currentKg();
    const double m = heightCm / 100.0;
    return m > 0 && kg > 0 ? kg / (m * m) : 0.0;
}

double Profile::bmr() const
{
    const double kg = currentKg();
    if (kg <= 0 || heightCm <= 0) return 0;
    return 10.0 * kg + 6.25 * heightCm - 5.0 * age() + (female ? -161.0 : 5.0);
}

double Profile::tdee() const { return bmr() * pal; }

double Profile::minimumKcal() const { return female ? 1200.0 : 1500.0; }

double Profile::targetKcal(QString *hint) const
{
    const double t = tdee();
    if (t <= 0) return 0;
    if (goal == Goal::Keep) return t;
    double delta = ratePerWeek * 7000.0 / 7.0;
    if (goal == Goal::Lose) {
        if (bmi() > 0 && bmi() < 18.5) {
            if (hint) *hint = tr("Der BMI liegt unter 18,5 – eine Gewichtsabnahme wird nicht geplant. "
                                 "Bitte sprechen Sie mit Ihrer Ärztin oder Ihrem Arzt.");
            return t;
        }
        delta = qMin(delta, 1000.0);
        double target = t - delta;
        const double minK = qMax(minimumKcal(), bmr());
        if (target < minK) {
            target = minK;
            if (hint) *hint = tr("Die Zufuhr wurde auf %1 kcal begrenzt (nicht unter dem Grundumsatz bzw. dem allgemeinen Mindestwert). "
                                 "Das Wochenziel wird dadurch langsamer erreicht.").arg(qRound(minK));
        }
        return target;
    }
    delta = qMin(delta, 700.0);
    return t + delta;
}

QString Profile::bmiClass(double b)
{
    if (b <= 0) return {};
    if (b < 18.5) return tr("Untergewicht");
    if (b < 25) return tr("Normalgewicht");
    if (b < 30) return tr("Übergewicht");
    if (b < 35) return tr("Adipositas Grad I");
    if (b < 40) return tr("Adipositas Grad II");
    return tr("Adipositas Grad III");
}

QString Profile::goalName(Goal g)
{
    switch (g) {
    case Goal::Lose: return tr("Gewicht verlieren");
    case Goal::Keep: return tr("Gewicht halten");
    case Goal::Gain: return tr("Gewicht zunehmen");
    }
    return {};
}

QString Profile::dietName(Diet d)
{
    switch (d) {
    case Diet::All: return tr("alles (auch Fleisch und Fisch)");
    case Diet::Vegetarian: return tr("vegetarisch");
    case Diet::Vegan: return tr("vegan");
    }
    return {};
}

QList<QPair<double, QString>> Profile::palLevels()
{
    return { { 1.2, tr("1,2 – ausschließlich sitzend oder liegend") },
             { 1.4, tr("1,4 – überwiegend sitzend, kaum Freizeitaktivität") },
             { 1.6, tr("1,6 – sitzend, zeitweise gehend oder stehend") },
             { 1.8, tr("1,8 – überwiegend gehend oder stehend") },
             { 2.0, tr("2,0 – körperlich anstrengend, viel Sport") } };
}

QDate Profile::lastWeighDay(const QDate &today) const
{
    if (weighDay < 1 || weighDay > 7) return {};
    const int back = (today.dayOfWeek() - weighDay + 7) % 7;
    return today.addDays(-back);
}

bool Profile::weighReminderDue(const QDate &today) const
{
    const QDate d = lastWeighDay(today);
    if (!d.isValid()) return false;
    if (weighDone.isValid() && weighDone >= d) return false;
    for (const WeightEntry &e : weights)
        if (e.date >= d && e.date <= today) return false;   // schon gewogen
    return true;
}

void Profile::markWeighDone(const QDate &today)
{
    const QDate d = lastWeighDay(today);
    if (d.isValid()) weighDone = d;
}

void Profile::addWeight(const QDate &date, double kg, const QString &note)
{
    bool replaced = false;
    for (WeightEntry &e : weights)
        if (e.date == date) {
            e.kg = kg;
            if (!note.isEmpty()) e.note = note;
            replaced = true;
        }
    if (!replaced) weights << WeightEntry{ date, kg, note };
    std::sort(weights.begin(), weights.end(), [](const WeightEntry &a, const WeightEntry &b) { return a.date < b.date; });
    const QDate due = lastWeighDay();
    if (due.isValid() && date >= due && date <= QDate::currentDate()) markWeighDone();
}

QString Profile::dayName(int weekday)
{
    static const char *n[] = { "", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag", "Sonntag" };
    return weekday >= 1 && weekday <= 7 ? QCoreApplication::translate("Profile", n[weekday]) : QString();
}

QJsonObject Profile::toJson() const
{
    QJsonArray w;
    for (const WeightEntry &e : weights)
        w.append(QJsonObject{ { QStringLiteral("date"), e.date.toString(Qt::ISODate) },
                              { QStringLiteral("kg"), e.kg },
                              { QStringLiteral("note"), e.note } });
    return QJsonObject{ { QStringLiteral("id"), id },
                        { QStringLiteral("name"), name },
                        { QStringLiteral("female"), female },
                        { QStringLiteral("birthYear"), birthYear },
                        { QStringLiteral("heightCm"), heightCm },
                        { QStringLiteral("pal"), pal },
                        { QStringLiteral("goal"), int(goal) },
                        { QStringLiteral("targetKg"), targetKg },
                        { QStringLiteral("ratePerWeek"), ratePerWeek },
                        { QStringLiteral("diet"), int(diet) },
                        { QStringLiteral("excluded"), QJsonArray::fromStringList(excluded) },
                        { QStringLiteral("likedTags"), QJsonArray::fromStringList(likedTags) },
                        { QStringLiteral("snack"), snack },
                        { QStringLiteral("weighDay"), weighDay },
                        { QStringLiteral("weighDone"), weighDone.toString(Qt::ISODate) },
                        { QStringLiteral("weights"), w },
                        { QStringLiteral("lastPlan"), lastPlan } };
}

Profile Profile::fromJson(const QJsonObject &o)
{
    Profile p;
    p.id = o.value(QStringLiteral("id")).toString();
    p.name = o.value(QStringLiteral("name")).toString();
    p.female = o.value(QStringLiteral("female")).toBool(true);
    p.birthYear = o.value(QStringLiteral("birthYear")).toInt(1980);
    p.heightCm = o.value(QStringLiteral("heightCm")).toDouble(170);
    p.pal = o.value(QStringLiteral("pal")).toDouble(1.4);
    p.goal = Goal(qBound(0, o.value(QStringLiteral("goal")).toInt(1), 2));
    p.targetKg = o.value(QStringLiteral("targetKg")).toDouble();
    p.ratePerWeek = o.value(QStringLiteral("ratePerWeek")).toDouble(0.5);
    p.diet = Diet(qBound(0, o.value(QStringLiteral("diet")).toInt(), 2));
    for (const auto &v : o.value(QStringLiteral("excluded")).toArray()) p.excluded << v.toString();
    for (const auto &v : o.value(QStringLiteral("likedTags")).toArray()) p.likedTags << v.toString();
    p.snack = o.value(QStringLiteral("snack")).toBool();
    p.weighDay = qBound(0, o.value(QStringLiteral("weighDay")).toInt(), 7);
    p.weighDone = QDate::fromString(o.value(QStringLiteral("weighDone")).toString(), Qt::ISODate);
    for (const auto &v : o.value(QStringLiteral("weights")).toArray()) {
        const QJsonObject e = v.toObject();
        WeightEntry w{ QDate::fromString(e.value(QStringLiteral("date")).toString(), Qt::ISODate), e.value(QStringLiteral("kg")).toDouble(),
                       e.value(QStringLiteral("note")).toString() };
        if (w.date.isValid() && w.kg > 0) p.weights << w;
    }
    std::sort(p.weights.begin(), p.weights.end(), [](const WeightEntry &a, const WeightEntry &b) { return a.date < b.date; });
    p.lastPlan = o.value(QStringLiteral("lastPlan")).toObject();
    if (p.id.isEmpty()) p.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    return p;
}

// ---------------------------------------------------------------- Profiles

Profiles &Profiles::instance()
{
    static Profiles p;
    return p;
}

Profiles::Profiles() { reload(); }

QString Profiles::file() const
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty()) dir = QDir::homePath() + QStringLiteral("/.viviameals");
    QDir().mkpath(dir);
    return QDir(dir).filePath(QStringLiteral("profile.json"));
}

void Profiles::reload()
{
    m_list.clear();
    QFile f(file());
    if (!f.open(QIODevice::ReadOnly)) return;
    for (const auto &v : QJsonDocument::fromJson(f.readAll()).object().value(QStringLiteral("profiles")).toArray())
        m_list << Profile::fromJson(v.toObject());
}

void Profiles::save()
{
    QJsonArray a;
    for (const Profile &p : m_list) a.append(p.toJson());
    QSaveFile f(file());
    if (!f.open(QIODevice::WriteOnly)) return;
    f.write(QJsonDocument(QJsonObject{ { QStringLiteral("format"), QStringLiteral("ViviaMeals-Profiles") },
                                       { QStringLiteral("profiles"), a } })
                .toJson());
    f.commit();
}

Profile *Profiles::find(const QString &id)
{
    for (Profile &p : m_list)
        if (p.id == id) return &p;
    return nullptr;
}

QString Profiles::add(const Profile &p)
{
    Profile n = p;
    if (n.id.isEmpty()) n.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_list << n;
    save();
    return n.id;
}

void Profiles::update(const Profile &p)
{
    for (Profile &x : m_list)
        if (x.id == p.id) x = p;
    save();
}

void Profiles::remove(const QString &id)
{
    Diary::Book::removeFor(id);   // Ernährungstagebuch der Person mit entfernen
    m_list.erase(std::remove_if(m_list.begin(), m_list.end(), [&](const Profile &p) { return p.id == id; }), m_list.end());
    save();
}
