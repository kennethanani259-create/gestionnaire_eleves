#!/usr/bin/env python3
"""
Jeu de donnees de demonstration pour le Gestionnaire d'eleves.

Ce script ne fait QUE des appels a l'API REST publique du serveur : il ne
touche jamais directement la base SQLite. Il est donc une bonne illustration
de l'utilisation de l'API.

Usage :
    python3 scripts/seed_demo.py --password <mot-de-passe-admin-initial>

Le mot de passe administrateur initial est affiche dans les logs du serveur
au tout premier demarrage. Le script le remplace ensuite par un mot de passe
connu (par defaut : Admin123!) pour que la demonstration soit reproductible.
"""

import argparse
import json
import random
import sys
import urllib.error
import urllib.request

random.seed(20260101)  # jeu de donnees reproductible

NOMS = ["Dossou", "Kone", "Agbodjan", "Houngbo", "Zinsou", "Adjovi", "Sossou",
        "Tchibozo", "Gnonlonfoun", "Ahouandjinou", "Akpovi", "Dagba", "Bio",
        "Lokossou", "Amoussou", "Degbey", "Kpogli", "Vodounon", "Assogba",
        "Hounkpatin", "Mensah", "Quenum", "Tossou", "Sagbo", "Ayena"]
PRENOMS_F = ["Awa", "Bintou", "Chantal", "Delphine", "Edwige", "Fatou", "Grace",
             "Huguette", "Ines", "Judith", "Kafui", "Larissa", "Marlene"]
PRENOMS_M = ["Abel", "Bruno", "Cedric", "Didier", "Emmanuel", "Franck", "Gilles",
             "Hubert", "Ismael", "Jacques", "Kevin", "Landry", "Mathieu"]

MATIERES = [("Mathematiques", "MATH", 4.0), ("Francais", "FRAN", 4.0),
            ("Anglais", "ANGL", 2.0), ("Sciences de la vie", "SVT", 2.0),
            ("Histoire-Geographie", "HIGE", 2.0), ("Education physique", "EPS", 1.0)]

EVALS = ["HOMEWORK", "QUIZ", "EXAM", "CONTINUOUS"]


class Client:
    """Mini client HTTP JSON avec jeton Bearer."""

    def __init__(self, base):
        self.base = base.rstrip("/")
        self.token = None

    def call(self, method, path, payload=None):
        data = json.dumps(payload).encode() if payload is not None else None
        request = urllib.request.Request(self.base + path, data=data, method=method)
        request.add_header("Content-Type", "application/json")
        if self.token:
            request.add_header("Authorization", "Bearer " + self.token)
        try:
            with urllib.request.urlopen(request, timeout=30) as response:
                body = response.read().decode()
                return json.loads(body) if body else None
        except urllib.error.HTTPError as error:
            detail = error.read().decode()
            raise SystemExit(f"[ERREUR] {method} {path} -> HTTP {error.code} : {detail}")
        except urllib.error.URLError as error:
            raise SystemExit(f"[ERREUR] Serveur injoignable sur {self.base} ({error.reason})")

    def get(self, p):
        return self.call("GET", p)

    def post(self, p, payload):
        return self.call("POST", p, payload)


def main():
    parser = argparse.ArgumentParser(description="Donnees de demonstration")
    parser.add_argument("--url", default="http://127.0.0.1:8080", help="URL du serveur")
    parser.add_argument("--password", required=True, help="Mot de passe admin actuel")
    parser.add_argument("--new-password", default="Admin123!",
                        help="Mot de passe admin apres execution (defaut: Admin123!)")
    parser.add_argument("--students", type=int, default=24, help="Nombre d'eleves a creer")
    args = parser.parse_args()

    api = Client(args.url)

    # ---------------------------------------------------------- Connexion
    session = api.post("/api/auth/login",
                       {"username": "admin", "password": args.password})
    api.token = session["token"]
    print("Connecte en tant que", session["user"]["username"])

    # Mot de passe connu, pour que la demonstration soit reproductible.
    if args.new_password and args.new_password != args.password:
        api.post("/api/auth/password",
                 {"current_password": args.password, "new_password": args.new_password})
        session = api.post("/api/auth/login",
                           {"username": "admin", "password": args.new_password})
        api.token = session["token"]
        print("Mot de passe administrateur fixe a :", args.new_password)

    # ------------------------------------------------- Comptes de demonstration
    existing = {u["username"] for u in (api.get("/api/users").get("items") or [])}
    for username, role, full_name in [("prof", "TEACHER", "Prof. Martin Ahouandjinou"),
                                      ("lecteur", "VIEWER", "Mme Rose Sossou")]:
        if username not in existing:
            api.post("/api/users", {"username": username, "email": username + "@ecole.local",
                                    "password": "Demo1234!", "full_name": full_name,
                                    "role": role})
            print(f"Utilisateur cree : {username} / Demo1234! ({role})")

    # ------------------------------------------------------- Annee scolaire
    years = api.get("/api/school-years").get("items") or []
    current = next((y for y in years if y.get("is_current")), None)
    if current is None:
        current = api.post("/api/school-years",
                           {"label": "2025-2026", "start_date": "2025-09-01",
                            "end_date": "2026-06-30", "is_current": True})
    print("Annee scolaire :", current["label"])

    # ------------------------------------------------------------- Classes
    wanted = [("6eme A", "6eme"), ("5eme B", "5eme"), ("Terminale C", "Terminale")]
    classes_by_name = {c["name"]: c for c in (api.get("/api/classes").get("items") or [])}
    classes = []
    for name, level in wanted:
        if name in classes_by_name:
            classes.append(classes_by_name[name])
            continue
        classes.append(api.post("/api/classes", {"name": name, "level": level,
                                                 "school_year_id": current["id"],
                                                 "capacity": 35}))
        print("Classe creee :", name)

    # ------------------------------------------------------------ Matieres
    subjects = {}
    for classroom in classes:
        present = {s["code"] for s in (api.get(f"/api/classes/{classroom['id']}/subjects")
                                       .get("items") or [])}
        created = []
        for name, code, coefficient in MATIERES:
            if code in present:
                continue
            created.append(api.post("/api/subjects",
                                    {"name": name, "code": code,
                                     "coefficient": coefficient,
                                     "class_id": classroom["id"]}))
        subjects[classroom["id"]] = (api.get(f"/api/classes/{classroom['id']}/subjects")
                                     .get("items") or [])
        if created:
            print(f"{len(created)} matieres creees pour {classroom['name']}")

    # -------------------------------------------------------------- Eleves
    students = []
    for index in range(args.students):
        classroom = classes[index % len(classes)]
        female = index % 2 == 0
        first = random.choice(PRENOMS_F if female else PRENOMS_M)
        student = api.post("/api/students", {
            "first_name": first,
            "last_name": random.choice(NOMS),
            "gender": "F" if female else "M",
            "birth_date": f"{random.randint(2008, 2013)}-{random.randint(1, 12):02d}-"
                          f"{random.randint(1, 28):02d}",
            "class_id": classroom["id"],
            "address": f"{random.randint(1, 250)} rue de Cotonou",
            "phone": f"+229 9{random.randint(1000000, 9999999)}",
            "email": None,
            "guardian_name": "M./Mme " + random.choice(NOMS),
            "guardian_phone": f"+229 9{random.randint(1000000, 9999999)}",
            "enrollment_date": "2025-09-15",
            "status": "ACTIVE" if index % 11 else "INACTIVE",
        })
        students.append((student, classroom))
    print(f"{len(students)} eleves crees")

    # --------------------------------------------------------------- Notes
    grade_count = 0
    for student, classroom in students:
        # Niveau propre a chaque eleve, pour obtenir un classement realiste.
        level = random.uniform(7.0, 17.5)
        for subject in subjects[classroom["id"]]:
            for term in (1, 2):
                for _ in range(2):
                    score = max(0.0, min(20.0, random.gauss(level, 2.2)))
                    api.post("/api/grades", {
                        "student_id": student["id"],
                        "subject_id": subject["id"],
                        "score": round(score, 2),
                        "max_score": 20,
                        "term": term,
                        "eval_type": random.choice(EVALS),
                        "eval_date": f"2025-{10 if term == 1 else 12}-"
                                     f"{random.randint(1, 28):02d}",
                        "comment": None,
                    })
                    grade_count += 1
    print(f"{grade_count} notes enregistrees")

    # ------------------------------------------------------------ Presences
    sessions = 0
    for classroom in classes:
        roster = [s for s, c in students if c["id"] == classroom["id"]]
        for day in range(1, 16):
            entries = []
            for student in roster:
                draw = random.random()
                status = ("ABSENT" if draw < 0.06 else
                          "EXCUSED" if draw < 0.10 else
                          "LATE" if draw < 0.16 else "PRESENT")
                entries.append({"student_id": student["id"], "status": status})
            api.post("/api/attendance/bulk", {
                "class_id": classroom["id"],
                "date": f"2025-11-{day:02d}",
                "time": "08:00",
                "entries": entries,
            })
            sessions += 1
    print(f"{sessions} appels enregistres")

    # ------------------------------------------------------------- Controle
    dashboard = api.get("/api/dashboard")
    print("\n--- Tableau de bord ---")
    print("  Eleves          :", dashboard["student_count"])
    print("  Classes         :", dashboard["class_count"])
    print("  Moyenne generale:", dashboard["general_average"])
    print("  Taux de presence:", dashboard["attendance_rate"], "%")
    best = dashboard.get("best_student") or {}
    print("  Meilleur eleve  :", best.get("full_name"), best.get("average"))
    print("\nDonnees de demonstration pretes. Connexion : admin /", args.new_password)
    return 0


if __name__ == "__main__":
    sys.exit(main())
