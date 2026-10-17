# ROLL-X

Plug-in VST3 (e Standalone) che crea roll da un campione audio importato.

## Controlli
- **ROLL RATE** (knob sinistro): 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32, 1/64. Segue il BPM dell'host (120 se assente).
- **PITCH MOD** (knob centrale): rampa crescente da 0 a +N semitoni (0-12) nell'arco di una battuta; dopo la battuta resta al massimo. Insieme alla rampa cresce anche il volume dei colpi (crescendo).
- **REVERB** (knob destro = mix) + **SIZE** e **DECAY** (due knob piccoli sotto).
- **OUT** (in alto a destra): volume di uscita, da -24 a +6 dB.
- **IMPORT SAMPLE**: wav, aiff, flac, mp3 (max 30 s). Il percorso viene salvato nel progetto.
- **HOLD TO ROLL** (mouse) oppure qualsiasi nota MIDI tenuta premuta: il roll parte e dura finche' il tasto e' premuto.

## Build (Windows / macOS / Linux)
Servono CMake >= 3.22 e un compilatore C++17 (Visual Studio, Xcode o GCC/Clang).

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

JUCE viene scaricato in automatico (v8.0.4). I VST3 compilati si trovano in
`build/RollX_artefacts/Release/VST3/` (da copiare a mano nella cartella VST3 di sistema).

## Installer macOS (.pkg)
Su Mac: `chmod +x make_pkg.sh && ./make_pkg.sh` crea `ROLL-X-0.1.0.pkg` (arm64 + Intel).
Senza compilatore locale: carica il progetto su GitHub, la build parte da sola (scheda Actions)
e il `.pkg` si scarica da "Artifacts".

## Struttura
- `Source/PluginProcessor.*`: parametri, motore del roll (retrigger a griglia, rampa di pitch), caricamento campione
- `Source/Reverb.h`: riverbero a combs + allpass con size e decay indipendenti
- `Source/PluginEditor.*`: interfaccia (manopole custom, display con anteprima del roll)
