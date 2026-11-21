# ViviaMeals – Hilfen für die Beispielrezepte (siehe make_samples.py)
RECIPES = []

def R(title, servings, minutes, tags, ingredients, instructions, unit=""):
    RECIPES.append({
        "title": title.strip(),
        "servings": servings,
        "yieldUnit": unit,
        "prepMinutes": minutes,
        "tags": [t.strip() for t in tags.split(",") if t.strip()],
        "ingredients": ingredients.strip(),
        "instructions": instructions.strip(),
    })
