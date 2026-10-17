cmake_minimum_required(VERSION 3.22)
project(RollX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(RollX
    COMPANY_NAME "YourName"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE Yrnm
    PLUGIN_CODE Rllx
    FORMATS VST3 Standalone
    PRODUCT_NAME "ROLL-X")

juce_generate_juce_header(RollX)

target_sources(RollX PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(RollX PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_MP3AUDIOFORMAT=1)

target_link_libraries(RollX
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

#!/bin/bash
# Compila ROLL-X per macOS (Apple Silicon + Intel) e crea l'installer .pkg
set -e
cd "$(dirname "$0")"

VERSION="0.1.0"
NAME="ROLL-X"

cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release

SRC="build/RollX_artefacts/Release/VST3/${NAME}.vst3"
DEST="pkgroot/Library/Audio/Plug-Ins/VST3"

rm -rf pkgroot
mkdir -p "$DEST"
cp -R "$SRC" "$DEST/"

# firma "ad hoc" (necessaria su Apple Silicon, non richiede account sviluppatore)
codesign --force --deep -s - "$DEST/${NAME}.vst3"

pkgbuild --root pkgroot \
         --identifier "com.yourname.rollx.vst3" \
         --version "$VERSION" \
         --install-location "/" \
         "${NAME}-${VERSION}.pkg"

echo "Creato: ${NAME}-${VERSION}.pkg"

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
