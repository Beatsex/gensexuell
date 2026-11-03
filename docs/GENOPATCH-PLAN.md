# genopatch — Ausführungsplan für Claude

Dieser Plan beschreibt die **sound → seed**-Pipeline von gensexuell (Feature-Extraktion, neuronales
Netz, evolutionäre Suche) und die Schrittfolge, mit der sie weiterentwickelt wird. Er ist so
geschrieben, dass ein Coding-Agent (Claude) ihn ohne Vorwissen abarbeiten kann: Fakten, Stand,
Guardrails, offene Schritte, exakte Befehle, Verifikation.

> Hinweis: Der ursprüngliche „genopatch"-Plan lag nicht im Repo. Die erledigten Schritte G1–G3 sind
> aus Code und Commits rekonstruiert und als erledigt verifiziert. Die offenen Schritte ab G4 sind
> **Vorschläge** und vor der Umsetzung mit dem Menschen abzustimmen.

---

## 0. Projekt in einem Satz

gensexuell ist ein lokaler Synth (C++/JUCE + Web-UI). Der Nutzer gibt eine **Referenz-Audio** vor,
die App sucht per Netz + Evolutionsstrategie die **Gene** (Parameter 0..1), die möglichst gleich
klingen („sound → seed"). Ziel von genopatch ist, diese Suche qualitativ und strukturell zu verbessern.

---

## 1. Wo der Code liegt

| Datei | Rolle |
|---|---|
| `Source/Engine.h` | Voice/Engine, Gene, `Genome`, `Params`, `seedParams`, `dirsFor`, `renderNote`, `detectPitch`. Plain C++, geteilt von Plugin/Tests/DatasetGen. |
| `Source/Analysis.h` | **Kern von genopatch**: `FeatConfig`, `features()`, `melBankFor()`, `nfeat()`, `featLen()`, `distance()`, `similarity()`, `patchFeatures()`, `Net`, `Search`. |
| `Source/Plugin.cpp` | JUCE-Plugin + WebView-Bridge: `genoSet/genoRun/genoRestart/genoPoll/genoGet/genoPlayRef`. |
| `Source/Words.h` | Text → Seed (eigene FFT/Desc, nutzt `FFT_N`/`FEAT_SR` nur lesend). |
| `tools/DatasetGen.cpp` | rendert zufällige Patches → `(features, genes)`-Datensatz. |
| `tools/train/train.py` | trainiert das Netz (numpy, MLP), schreibt `Resources/net.bin`. |
| `tests/EngineTest.cpp` | alle Checks: Genetik-Parität zur UI, Voice-Modi, Sound→Seed, Text→Seed, Performance, **Feature-Dimension**. |
| `Resources/index.html` | Web-UI; muss bei Engine-Änderungen synchron bleiben (`seedParams`, `dirsFor`, `KEYS`, `BASE`, `MASTER`). |
| `Resources/net.bin` | trainiertes Netz, Format „SDNN". |

### Feature-/Netz-Fakten (Default)
- `FEAT_SR = 24000`, `FEAT_LEN = 48000` (2 s), `FFT_N = 1024`, `MEL = 40`, `FRAMES = 20`, `NFEAT = 801` (seit G4; vorher 32 × 16 / 513).
- Feature-Vektor = `melBands × frames` normalisierte Log-Mel-Zellen + 1 Note (`nfeat(cfg)`).
- `netGenes()` = alle Gene außer Master-Genen und `spread` → aktuell **40** Ausgänge.
- Netz-Format: `"SDNN"`, int32 Layerzahl, je Layer `in,out`, Gewichte `[out][in]`, Bias `[out]`, optional `"SIGN"` + uint32 `featSignature`. ReLU, Sigmoid-Ausgang. **Die Eingangsbreite steht in der Datei** (`Net::inDim()`), nicht mehr als Compile-Konstante.
- `Search`: 4 Stränge, Evolutionsstrategie, `patchFeatures` + `distance` als Fitness.

---

## 2. Branch- und Commit-Konvention

- Feature-Branches heißen `genopatch/g<N>-<kurzname>` (z. B. `genopatch/g1-feature-config`).
- `main` ist die Basis und hinkt absichtlich hinterher (zuletzt `9939e68`).
- Pro Schritt **ein** Fokus-Commit, deutsch/englisch gemischt ok. Tests müssen vor dem Commit grün sein.
- **Niemals** `net.bin` ohne echte Feature-/Genom-Änderung neu erzeugen (unnötiger Binär-Churn).

---

## 3. Stand (verifiziert)

| Schritt | Inhalt | Status | Commit |
|---|---|---|---|
| G1 | `FeatConfig` eingeführt; Genom 35 → 54; neue Gene verdrahtet; Netz neu trainiert | **erledigt** | `26d4e77` |
| G2 | Per-Strang-Live-Status (`Search::Status`, `genoPoll`, Such-Infobox) | **erledigt** | `fc1f23b` |
| G3 | Feature-Dimension dynamisch: `FeatConfig` steuert `features()`/`nfeat()`/`featLen()`, `Features = std::vector<float>`, `Net::inDim()`, `Search::cfg`, `DatasetGen` nutzt `nfeat(cfg)` | **erledigt** | `f1e85c5` (Branch `genopatch/g3-dynamic-feature-dim`) |
| G5 | `genoPoll` liefert `netDim`/`featDim`/`netHint`, UI zeigt sie an und warnt bei Abweichung | **erledigt** | `e6ec83a` (Branch `genopatch/g5-runtime-dim-check`) |
| G4 | Defaults auf 40 × 20 (`NFEAT = 801`), Datensatz neu, Netz neu trainiert und signiert | **erledigt** | Branch `genopatch/g4-resolution` |
| G7 | End-to-End-Test: signiertes Netz und Suche mit Custom-Config (24 × 8 + 1) im `EngineTest` | **erledigt** | Branch `genopatch/g7-e2e-custom-config` |
| G6 | `FeatConfig::version` + `featSignature()`; Signatur im Datensatz-Header und als `SIGN`-Anhang im Netz; `Net::load` lehnt falsche Signatur, falsche Genzahl und kaputte Dateien mit `loadError()` ab | **erledigt** | Branch `genopatch/g6-config-version` |

Verifikation für G3: `EngineTest` grün; `first guess: net 66.5 %` **identisch** zu vor G3 → Default-Features
bit-genau, `net.bin` weiter gültig. `DatasetGen` separat gebaut und mit 2000 Patches geprüft
(Header `nfeat=513 ngenes=40`).

Verifikation für G5: `EngineTest` grün, `first guess: net 66.5 %, neutral 34.2 %` unverändert (der
Suchtreffer schwankt zwischen Läufen, hier 88.5–91.4 %). Neue Checks: Netz falscher Eingangsbreite lädt,
meldet seine Breite und schätzt nur neutral.
Standalone mit Testnetz (Eingangsbreite 8) von Hand geprüft: Infobox zeigt `NET 8 ≠ FEAT 513: NEUTRAL START`,
die Suche läuft. Passender Fall nur im Browser mit gesetzten Werten geprüft. **Nicht** bestätigt: der Toast.

Verifikation für G6: `EngineTest` grün, `first guess: net 66.5 %, neutral 34.2 %` unverändert, `net.bin` nicht
neu erzeugt (es hat keine Signatur und lädt weiter). Neue Checks: falsche Signatur, falsche Genzahl und
abgeschnittene Dateien werden mit Meldung abgelehnt. Kette mit 2000 Patches und 1 Epoche geprüft: Datensatz-Header
und Netz-Anhang tragen dieselbe Signatur (`260418764`), `EngineTest` lädt dieses Netz. Danach `net.bin` per Git
wiederhergestellt. **Nicht** im laufenden Plugin geprüft: die Ablehnungsmeldung in Toast und Infobox.

Verifikation für G4: Datensatz 150.000 Patches (`nfeat 801`, `ngenes 40`, Signatur `3878685456`), 40 Epochen,
`val MSE 0.0595` vs. Baseline `0.0706` (altes Netz: `0.0594` vs. `0.0707`). `EngineTest` grün, neue Referenz
`first guess: net 64.4 %, neutral 34.2 %`; die alten 66.5 % sind nicht vergleichbar. Gegenprobe alt gegen neu auf
denselben ungesehenen Patches, gleiches Trainings-Budget: erster Treffer in alten Features 71.1 / 71.2 %, in neuen
69.5 / 70.2 %, Gen-MSE 0.0619 / 0.0621. Suche 12 × 5 s: in alten Features 86.4 / 86.5 %, in neuen 83.0 / 86.9 %,
15315 / 14597 Patches. Ergebnis: nicht schlechter, kaum besser; beim ersten Treffer im Rahmen des Rauschens. Der
Vorsprung in den neuen Features ist teils erwartbar, weil das neue Netz in diesem Maß sucht.

Verifikation für G7: `EngineTest` grün. Custom-Config 193 Features, 3 s Suche: 863 Patches, bester Treffer 82.3 %
(Zufallspatch 0.0 %), alle Scores endlich. Das Netz ist ein synthetisches Nullnetz, der Test prüft also die Kette,
nicht die Qualität einer Schätzung.

---

## 4. Guardrails (nicht verhandelbar)

1. **Rückwärtskompatibilität:** Das Default-`FeatConfig` muss die Features, auf denen `Resources/net.bin`
   trainiert ist (seit G4 20 × 40 / 24 kHz, per Entscheidung des Menschen), **bit-identisch** erzeugen. Tripwire: `nfeat(FeatConfig{}) == NFEAT` und `featLen(FeatConfig{}) == FEAT_LEN`
   im `EngineTest`. `Resources/net.bin` bleibt dadurch gültig.
2. **UI-Parität:** Jede Änderung an `seedParams`/`dirsFor`/Genen muss in `Resources/index.html`
   gespiegelt werden, sonst weichen Seeds/Branches ab. `EngineTest` prüft Referenzwerte.
3. **Tests grün:** `EngineTest` muss `all checks passed` melden. Kein Commit mit FAIL.
4. **Netz-Treffer:** Der erste Netz-Treffer muss den Neutralstart schlagen (Test erzwingt das).
   Der aktuelle Wert (64.4 % vs. 34.2 %) ist die Referenz bei unverändertem Default-Feature.
5. **Dimensionen konsequent:** Referenz, Guess, Kinder und Netz müssen dieselbe `nfeat(cfg)` haben.
   `Net::predict` fällt bei falscher Größe auf `neutralGenes()` zurück; `distance` liefert bei
   unterschiedlichen Dimensionen 1.0 (nie vergleichen).
6. **`tools/train/dataset.bin` ist gitignored** (hunderte MB). Nicht committen.
7. **Keine Produkt-/Vorbild-Nennungen:** In Code, Kommentaren, UI und Doku keine Namen
   kommerzieller Synths oder Vorbilder erwähnen — das Konzept generisch beschreiben.

---

## 5. Restplan (offene Schritte)

Reihenfolge und Abhängigkeit:

```
G4 (Auflösung + Retrain) ──▶ G7 (End-to-End-Test)     [beide ändern die Feature-Basis]
G5 (Laufzeit-Dim-Check)  ──▶ G6 (Config-/Netz-Versionierung)   [G6 baut auf G5 auf]
```
Empfehlung: **zuerst G5** (klein, kein Retrain, sofortiger Nutzen), dann G6, dann G4, zuletzt G7.
G4 ist das einzige, das Datensatz + Netz neu erzeugt.

### G4 — Auflösung erhöhen und Netz neu trainieren
**Ziel:** mehr Frequenz-/Zeitauflösung → bessere Suchqualität.
**Änderung:** in `Source/Analysis.h` die `FeatConfig`-Defaults anheben, z. B. `melBands = 40`,
`frames = 20` (`NFEAT = 801`). `FFT_N`/`sampleRate` nur bewusst ändern (ändert die Filterbank);
`windowSize` an `fftN` anpassen. Danach `NFEAT`-Konstante auf den neuen Default setzen (sie ist der
Nennwert; sonst schlägt der Tripwire-Check `nfeat(FeatConfig{}) == NFEAT` an — genau sein Zweck).
**Befehle:** §6 Datensatz + Training.
**Akzeptanz:** `EngineTest` grün; Trainingsausgabe `val MSE < baseline`; erster Netz-Treffer
deutlich > 34.2 %. Da sich die Feature-Basis ändert, ist der alte 66.5 %-Wert nicht 1:1 vergleichbar
— neuen Wert als neue Referenz notieren.
**Risiko/Gegenprobe:** alt vs. neu mit gleichem Trainings-Budget vergleichen; kein Commit, wenn neu
schlechter. Danach `Resources/net.bin` committen.

### G5 — Netz-/Feature-Config zur Laufzeit prüfen und anzeigen
**Ziel:** stille Fehlkonfiguration (Netz-Dim ≠ Feature-Dim) sichtbar machen.
**Änderung:** in `Source/Plugin.cpp` beim Laden von `net.bin` `net.inDim()` gegen
`nfeat(proc.search.cfg)` prüfen; bei Abweichung im `genoPoll`-Info-Objekt Felder `netDim`/`featDim`
und einen Hinweis setzen. In `Resources/index.html` in `drawInfo` den Zustand ausgeben, z. B.
`NET 513 · FEAT 513` bzw. Warnung bei Abweichung.
**Akzeptanz:** passende Config → keine Warnung; manipuliertes `net.bin` (falsche Eingangsbreite) →
Suche startet neutral und die UI warnt.
**Risiko:** gering; keine Trainingsänderung.

### G6 — Config-/Netz-Versionierung
**Ziel:** verhindern, dass Datensatz, Netz und Code auseinanderlaufen.
**Änderung:** `FeatConfig` um Version/Hash erweitern; diesen im Datensatz-Header
(`tools/DatasetGen.cpp`) und/oder im `SDNN`-Container ablegen (`tools/train/train.py` schreibt ihn
mit). `Net::load` lehnt ein Netz mit unpassender Signatur mit klarer Meldung ab (statt stillem
Neutralstart).
**Akzeptanz:** Tests decken „falsche Signatur → `load` schlägt fehl / klare Meldung" ab.
**Abhängigkeit:** baut auf G5 auf.

### G7 — End-to-End-Test mit Custom-Feature-Config
**Ziel:** die dynamische Dimension durch die ganze Kette testen, nicht nur punktuell.
**Änderung:** im `EngineTest` ein winziges, synthetisches Netz mit Custom-`nfeat(cfg)` erzeugen
(z. B. 24 × 8 + 1), `Search::cfg` passend setzen, kurze Suche laufen lassen.
**Akzeptanz:** Suche terminiert, alle Scores endlich, bester Treffer > Zufall.
**Abhängigkeit:** nach G4 (neue Basis) sinnvoll, unabhängig lauffähig.

---

## 6. Exakte Befehle (Windows / PowerShell)

Alle Befehle aus dem Repo-Root `H:\gensexuell neu` ausführen.

### Bauen + testen (alles)
```powershell
cmd /c build.cmd
```
`build.cmd` ruft `vcvars64.bat`, hängt CMake + Ninja in den PATH, konfiguriert und baut
VST3/CLAP/LV2/Standalone + `EngineTest`, und führt `EngineTest.exe` aus. Erfolg = `all checks passed`.
Meldet `cmd` „build.cmd nicht gefunden" (Umgebung mit `NoDefaultCurrentDirectoryInExePath`), hilft
`cmd /c .\build.cmd`. Ist C: voll, `TEMP`/`TMP` vorher auf H: setzen. Das gilt auch beim Start der App:
Der WebView2-Datenordner liegt in `%TEMP%`, bei vollem C: bleibt das Fenster sonst schwarz.

### Einzel-Target bauen (z. B. DatasetGen)
`DatasetGen` baut `build.cmd` **nicht** mit. Es braucht die vcvars-Umgebung. Am einfachsten ein
kleines Temp-Skript:

```bat
@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set "PATH=%VSINSTALLDIR%Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%VSINSTALLDIR%Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
cd /d "H:\gensexuell neu"
cmake --build build --target DatasetGen || exit /b 1
build\DatasetGen.exe "%TEMP%\g3_ds.bin" %1
```
Ohne vcvars schlägt `cl.exe` mit `C1083 "algorithm": No such file or directory` fehl.

### Datensatz erzeugen
```powershell
cmd /c tools\dataset.cmd <count>
```
Schreibt `tools\train\dataset.bin`. Aktueller Datensatz: **150.000** Patches (`nfeat 801`,
`ngenes 40`, ≈ 505 MB), Laufzeit hier 6 min. `train.py` lehnt Datensätze ohne Signatur (vor G6) ab. `count` passend zur
Ziel-Auflösung wählen; mehr Features → mehr Bytes/Sample.

### Netz trainieren
```powershell
py tools\train\train.py tools\train\dataset.bin 40
```
`py` (Python 3.13, numpy 2.2.6) verwenden — `python` ist auf diesem Rechner nicht verlinkt.
Liest `nfeat`/`ngenes` aus dem Header, trainiert MLP `nfeat → 512 → 256 → ngenes` (Adam, 40 Epochen)
und schreibt `Resources\net.bin`. Laufzeit grob 5–15 min. Erwartete Ausgabe: `val MSE` vs
`baseline`, am Ende `wrote …`.

### Nur testen (ohne Vollbuild)
```powershell
build\EngineTest.exe
```
Muss aus dem Repo-Root laufen (öffnet `Resources/net.bin` relativ).

---

## 7. Verifikations-Checkliste (vor jedem Commit)

- [ ] `cmd /c build.cmd` → Build ok, `all checks passed`.
- [ ] Bei Feature-/Netz-Änderung: Dataset-Header prüfen (`nfeat`, `ngenes` stimmen mit `nfeat(cfg)`
      und `netGenes().size()` überein).
- [ ] `net.bin` nur angefasst, wenn Feature-/Genom-Dimension sich wirklich geändert hat.
- [ ] Bei Engine-/Gen-Änderung: `Resources/index.html` synchron (`KEYS`, `BASE`, `MASTER`,
      `seedParams`, `dirsFor`).
- [ ] Erster-Netz-Treffer- und Suchtreffer-Werte aus der Testausgabe notieren (Regression).
- [ ] `git status` sauber, nur beabsichtigte Dateien geändert.

---

## 8. Arbeitsauftrag für Claude (Kurzfassung)

1. `git switch -c genopatch/g<N>-<name>` (von `main` oder dem letzten genopatch-Branch).
2. Betroffene Datei(en) ändern; Guardrails aus §4 einhalten.
3. `cmd /c build.cmd`; bei Feature-/Netz-Änderung zusätzlich §6 Datensatz + Training.
4. `docs/GENOPATCH-PLAN.md` (diese Datei) mit dem neuen Status/den gemessenen Werten fortschreiben:
   Schritt in der Tabelle §3 als erledigt markieren, ggf. neue offene Schritte ergänzen.
5. Commit mit klarer Botschaft (`G<N>: …`), `main` nicht ohne Freigabe mergen.
