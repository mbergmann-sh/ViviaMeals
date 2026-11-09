// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "coaching.h"
#include "recipe.h"

#include <QCoreApplication>
#include <cmath>

namespace Coaching {

namespace {
QString tr(const char *s) { return QCoreApplication::translate("Coaching", s); }
QString esc(const QString &s) { return s.toHtmlEscaped(); }
} // namespace

QString motivation(const Profile &p)
{
    if (p.weights.isEmpty())
        return tr("Tragen Sie Ihr aktuelles Gewicht ein – ab dem zweiten Eintrag zeigt VitaMeals Ihren Fortschritt.");
    const WeightEntry &first = p.weights.first();
    const WeightEntry &last = p.weights.last();
    const double diff = last.kg - first.kg;                  // + zugenommen, − abgenommen
    const int days = int(first.date.daysTo(last.date));
    const QString since = first.date.toString(QStringLiteral("dd.MM.yyyy"));
    QString s;
    switch (p.goal) {
    case Profile::Goal::Lose:
        if (p.weights.size() < 2 || days < 1)
            s = tr("Der Anfang ist gemacht! Kleine, dauerhafte Änderungen bringen mehr als eine strenge Diät – "
                   "0,25 bis 0,5 kg pro Woche sind ein gesundes Tempo.");
        else if (diff <= -0.2) {
            s = tr("Seit dem %1 haben Sie %2 kg abgenommen – weiter so!").arg(since, Fmt::number(-diff));
            if (p.targetKg > 0 && last.kg > p.targetKg)
                s += QLatin1Char(' ') + tr("Bis zum Ziel sind es noch %1 kg.").arg(Fmt::number(last.kg - p.targetKg));
            else if (p.targetKg > 0)
                s += QLatin1Char(' ') + tr("Sie haben Ihr Zielgewicht erreicht – jetzt geht es ums Halten. Stellen Sie das Ziel auf „Gewicht halten“ um.");
        } else
            s = tr("Das Gewicht bewegt sich gerade kaum – das ist normal. Wasser, Salz und Verdauung lassen die Waage um 1–2 kg "
                   "schwanken. Entscheidend ist der Verlauf über mehrere Wochen. Bleiben Sie dran!");
        break;
    case Profile::Goal::Gain:
        if (p.weights.size() < 2 || days < 1)
            s = tr("Gesund zunehmen braucht Geduld: 0,25 bis 0,5 kg pro Woche, am besten mit regelmäßigem Krafttraining, "
                   "damit vor allem Muskeln aufgebaut werden.");
        else if (diff >= 0.2) {
            s = tr("Seit dem %1 haben Sie %2 kg zugenommen – prima!").arg(since, Fmt::number(diff));
            if (p.targetKg > 0 && last.kg < p.targetKg)
                s += QLatin1Char(' ') + tr("Bis zum Ziel sind es noch %1 kg.").arg(Fmt::number(p.targetKg - last.kg));
        } else
            s = tr("Noch keine Zunahme? Ergänzen Sie eine Zwischenmahlzeit oder etwas mehr gesundes Fett (Nüsse, Öl, Avocado) – "
                   "kleine Extras summieren sich.");
        break;
    case Profile::Goal::Keep:
        if (p.weights.size() >= 2 && std::fabs(diff) < 1.5)
            s = tr("Ihr Gewicht ist seit dem %1 stabil (Änderung %2 kg) – genau so soll es sein.")
                    .arg(since, (diff >= 0 ? QStringLiteral("+") : QString()) + Fmt::number(diff));
        else if (p.weights.size() >= 2)
            s = tr("Seit dem %1 hat sich Ihr Gewicht um %2 kg verändert. Ein Blick auf Portionsgrößen und Bewegung hilft, "
                   "wieder ins Gleichgewicht zu kommen.")
                    .arg(since, (diff >= 0 ? QStringLiteral("+") : QString()) + Fmt::number(diff));
        else
            s = tr("Gewicht halten heißt: so viel essen, wie der Körper verbraucht. Einmal pro Woche wiegen zeigt früh, "
                   "wenn sich etwas verschiebt.");
        break;
    }
    return s;
}

QString explanation(Profile::Goal goal)
{
    switch (goal) {
    case Profile::Goal::Lose:
        return tr("Abnehmen gelingt, wenn der Körper etwas weniger Energie bekommt, als er verbraucht. Ein Kilogramm Körperfett "
                  "entspricht rund 7 000 kcal – bei 500 kcal weniger am Tag sind das etwa 0,5 kg pro Woche. Größere Defizite "
                  "machen hungrig, kosten Muskeln und führen oft zum Jo-Jo-Effekt. Am besten sättigen Gemüse, Obst, "
                  "Vollkorn, Hülsenfrüchte und eiweißreiche Lebensmittel (Quark, Joghurt, Fisch, Eier): viel Volumen, "
                  "wenig Energie. Bewegung hilft doppelt – sie verbraucht Energie und erhält die Muskeln.");
    case Profile::Goal::Gain:
        return tr("Zunehmen gelingt mit einem kleinen Energieüberschuss von etwa 300–500 kcal am Tag. Damit vor allem Muskeln "
                  "und nicht nur Fett zugelegt werden, gehören ausreichend Eiweiß und Krafttraining dazu. Gut geeignet sind "
                  "nährstoffreiche, energiedichte Lebensmittel wie Nüsse, Saaten, Pflanzenöle, Avocado, Vollkorn, Milchprodukte "
                  "und Hülsenfrüchte – statt Süßigkeiten und Fast Food. Bei ungewolltem Gewichtsverlust bitte ärztlich abklären lassen.");
    case Profile::Goal::Keep:
        break;
    }
    return tr("Gewicht halten heißt: Energiezufuhr und Verbrauch sind im Gleichgewicht. Eine abwechslungsreiche, überwiegend "
              "pflanzliche Ernährung nach den DGE-Empfehlungen, regelmäßige Mahlzeiten und tägliche Bewegung machen das "
              "leicht. Wöchentliches Wiegen zeigt früh, wenn es in eine Richtung kippt.");
}

QList<Swap> swaps(Profile::Goal goal)
{
    if (goal == Profile::Goal::Gain)
        return {
            { tr("drei Mahlzeiten"), tr("fünf bis sechs kleinere Mahlzeiten"), tr("leichter mehr zu essen, ohne Völlegefühl") },
            { tr("viel trinken vor dem Essen"), tr("erst essen, dann trinken"), tr("Getränke füllen den Magen") },
            { tr("Magerquark, fettarme Milch"), tr("Quark 20–40 %, Vollmilch, Naturjoghurt 3,5 %"), tr("mehr Energie und Eiweiß") },
            { tr("Obst pur"), tr("Obst mit Nüssen oder 1 EL Nussmus"), tr("+ 100–150 kcal, gesunde Fette") },
            { tr("Salat ohne Dressing"), tr("Salat mit Rapsöl, Kernen, Avocado oder Feta"), tr("+ 150–250 kcal") },
            { tr("Wasser als Zwischenmahlzeit"), tr("Smoothie mit Haferflocken, Banane und Milch"), tr("ca. 350 kcal zum Trinken") },
            { tr("Süßigkeiten, Chips"), tr("Studentenfutter, Vollkorn-Müsliriegel, Käsebrot"), tr("Energie mit Nährstoffen statt leerer Kalorien") },
            { tr("nur Ausdauersport"), tr("2–3 × pro Woche Krafttraining"), tr("die Zunahme wird zu Muskeln") },
        };
    QList<Swap> l{
        { tr("Butter auf dem Brot (10 g ≈ 75 kcal)"), tr("Tomatenmark, Senf, Kräuterquark oder Frischkäse light"), tr("spart 50–65 kcal je Brot") },
        { tr("Limonade, Cola, Saft (0,33 l ≈ 140 kcal)"), tr("Wasser, ungesüßter Tee, Schorle 1 : 3"), tr("spart bis 140 kcal je Glas, kein Zucker") },
        { tr("Sahne in der Soße (100 ml ≈ 290 kcal)"), tr("Milch mit etwas Speisestärke, Joghurt (zum Schluss) oder Gemüsepüree"), tr("spart 200 kcal und mehr") },
        { tr("Fruchtjoghurt (150 g, ca. 20 g Zucker)"), tr("Naturjoghurt mit frischem Obst"), tr("halb so viel Zucker") },
        { tr("Chips (30 g ≈ 160 kcal)"), tr("Gemüsesticks mit Joghurt-Dip oder eine kleine Handvoll Nüsse"), tr("satt statt nur Kalorien") },
        { tr("Salami, fette Wurst"), tr("Putenbrust, Hüttenkäse, Kräuterquark, Gemüse-Aufstrich"), tr("weniger Fett und Salz") },
        { tr("Mayonnaise (1 EL ≈ 100 kcal)"), tr("Joghurt-Dressing, Senf, Essig und wenig Rapsöl"), tr("spart rund 80 kcal") },
        { tr("Weißbrot, helle Nudeln"), tr("Vollkornbrot, Vollkornnudeln, Naturreis"), tr("mehr Ballaststoffe – länger satt") },
        { tr("Zucker in Kaffee und Tee"), tr("Schritt für Schritt weniger; Zimt oder Vanille"), tr("der Geschmack gewöhnt sich in 2–3 Wochen") },
        { tr("Frittiertes, Paniertes"), tr("Backofen, beschichtete Pfanne, Öl mit dem Pinsel"), tr("spart viel Fett") },
        { tr("großer Teller, Nachschlag"), tr("kleinerer Teller, halbe Beilage, doppelt Gemüse"), tr("gleich satt, deutlich weniger Energie") },
        { tr("süßes Frühstück (Croissant, Nougatcreme)"), tr("Haferflocken mit Beeren, Vollkornbrot mit Quark"), tr("hält bis mittags vor") },
    };
    if (goal == Profile::Goal::Keep) l = l.mid(0, 8);
    return l;
}

QStringList habits(Profile::Goal goal)
{
    QStringList h{
        tr("Täglich 5 Portionen Gemüse und Obst – davon 3 Gemüse (DGE)."),
        tr("Vollkorn statt Weißmehl; mindestens einmal pro Woche Hülsenfrüchte (Linsen, Bohnen, Kichererbsen)."),
        tr("Täglich eine kleine Handvoll Nüsse (ca. 25 g); Rapsöl als Standard-Speiseöl."),
        tr("Milchprodukte täglich, Fisch einmal pro Woche; Fleisch und Wurst höchstens 300 g pro Woche."),
        tr("Rund 1,5 Liter am Tag trinken – am besten Wasser oder ungesüßten Tee."),
        tr("Zucker und Salz sparsam (höchstens 6 g Salz am Tag); Kräuter und Gewürze statt Salz."),
        tr("Langsam und bewusst essen – das Sättigungsgefühl kommt nach etwa 15–20 Minuten."),
        tr("Mindestens 150 Minuten Bewegung pro Woche (z. B. 5 × 30 Minuten zügiges Gehen oder Radfahren)."),
    };
    switch (goal) {
    case Profile::Goal::Lose:
        h << tr("Einmal pro Woche wiegen, immer am selben Tag, morgens vor dem Frühstück – tägliches Wiegen verunsichert nur.")
          << tr("Zu jeder Mahlzeit eine Eiweißquelle (Quark, Joghurt, Eier, Hülsenfrüchte, Fisch) – das sättigt am längsten.")
          << tr("Nicht hungrig einkaufen; den Wochenplan und die Einkaufsliste nutzen.")
          << tr("Kein „Verbot“: Lieblingsessen in kleiner Portion einplanen statt ganz zu streichen.")
          << tr("Ausreichend schlafen – Schlafmangel steigert den Appetit.");
        break;
    case Profile::Goal::Gain:
        h << tr("Feste Essenszeiten, auch wenn der Hunger fehlt; eine Mahlzeit nicht ausfallen lassen.")
          << tr("Eiweiß über den Tag verteilen (je Mahlzeit z. B. Milchprodukte, Eier, Hülsenfrüchte, Fisch).")
          << tr("Einmal pro Woche wiegen; nimmt das Gewicht ohne Grund ab, ärztlich abklären lassen.");
        break;
    case Profile::Goal::Keep:
        h << tr("Einmal pro Woche wiegen; bei mehr als 1–2 kg Abweichung früh gegensteuern.")
          << tr("Ausnahmen ausgleichen: nach einem Festessen am nächsten Tag etwas leichter essen.");
        break;
    }
    return h;
}

QString tipsHtml(const Profile &p, const QString &primary, const QString &accent)
{
    QString h = QStringLiteral("<h2 style=\"color:%1;\">%2</h2>").arg(primary, esc(Profile::goalName(p.goal)));
    h += QStringLiteral("<p style=\"color:%1;\"><b>%2</b></p>").arg(accent, esc(motivation(p)));
    h += QStringLiteral("<p>%1</p>").arg(esc(explanation(p.goal)));
    h += QStringLiteral("<h3 style=\"color:%1;\">%2</h3>").arg(primary, tr("Statt … lieber …"));
    h += QStringLiteral("<table cellspacing=\"0\" cellpadding=\"4\" border=\"1\" style=\"border-collapse:collapse;\" width=\"100%\">");
    h += QStringLiteral("<tr><th align=\"left\">%1</th><th align=\"left\">%2</th><th align=\"left\">%3</th></tr>").arg(tr("statt"), tr("lieber"), tr("Effekt"));
    for (const Swap &s : swaps(p.goal))
        h += QStringLiteral("<tr><td>%1</td><td><b>%2</b></td><td>%3</td></tr>").arg(esc(s.instead), esc(s.better), esc(s.why));
    h += QStringLiteral("</table>");
    h += QStringLiteral("<h3 style=\"color:%1;\">%2</h3><ul>").arg(primary, tr("Gute Gewohnheiten"));
    for (const QString &x : habits(p.goal)) h += QStringLiteral("<li>%1</li>").arg(esc(x));
    h += QStringLiteral("</ul>");
    h += QStringLiteral("<p><small>%1</small></p>")
             .arg(tr("Grundlage: DGE-Empfehlungen „Gut essen und trinken“ (2024) und WHO-Bewegungsempfehlungen. "
                     "Allgemeine Hinweise für gesunde Erwachsene – kein Medizinprodukt, keine individuelle Beratung. "
                     "Bei Erkrankungen, in Schwangerschaft und Stillzeit oder bei Essstörungen bitte ärztlich beraten lassen."));
    return h;
}

} // namespace Coaching
