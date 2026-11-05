# DaniRig

App **standalone** per chitarra live, Mac (universal: Apple Silicon + Intel) e Windows.

**Catena del segnale** (ogni blocco è un modulo con on/off, parametri e crossfade senza click):

```
Ingresso → Noise Gate → Overdrive (stile TS) → Distortion (stile DS-1) → Ampli NAM (A2) → Cabinet IR → Delay (tap) → Reverb → Uscita
```

| Modulo | Cosa fa |
|---|---|
| **Noise Gate** | Gate senza lookahead (zero latenza), soglia/release/range, isteresi e hold |
| **Overdrive** | Modello analogico di uno stadio op-amp con diodi nel feedback + gobba sui medi, oversampling 4x |
| **Distortion** | Booster a transistor + op-amp ad alto gain + clipping duro a diodi + tone "scoop", oversampling 4x |
| **NAM Amp** | Carica `.nam` (A1 e **A2**, con selettore Full/Lite per i modelli A2), EQ a 3 bande, auto-level, caricamento in background con crossfade tra modelli, precedente/successivo nella cartella |
| **Cabinet** | Convoluzione IR a latenza zero (JUCE), low/high cut, livello, precedente/successivo nella cartella |
| **Delay** | Tap tempo, suddivisioni (1/4, 1/8 puntato, 1/8, terzina), repeats, tone, mod, modo Digital/Analog, ping-pong |
| **Reverb** | FDN a 8 linee con diffusione, damping, pre-delay, low cut, leggera modulazione |

Le impostazioni (e il percorso di modello/IR) vengono salvate all'uscita e ripristinate all'avvio. `SAVE`/`LOAD` salvano/caricano preset `.dprig`.

## Build in locale (Mac o Windows)

Servono CMake ≥ 3.22 e un compilatore C++20 (Xcode/Command Line Tools, oppure Visual Studio 2022).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target DaniRig
```

JUCE 9.0.5 e il core NAM vengono scaricati da soli (FetchContent). L'app esce in `build/DaniRig_artefacts/`.

## Build con GitHub Actions (senza Xcode)

Fai push del progetto su un repository GitHub: il workflow `.github/workflows/build.yml` produce

- `DaniRig-macOS-universal.dmg` (arm64 + x86_64, firma ad-hoc)
- `DaniRig-Setup-<versione>.exe` (installer Windows, runtime MSVC statico)

e lancia anche i test DSP su Linux. Gli artefatti si scaricano dalla pagina dell'esecuzione (tab *Actions*).

**Primo avvio su Mac**: l'app non è notarizzata, quindi tasto destro → *Apri* (oppure `xattr -cr /Applications/DaniRig.app`). Al primo avvio macOS chiede il permesso per il microfono: è l'ingresso audio della tua interfaccia, va concesso.

## Latenza: come tenerla bassa

- **Buffer 64–128 campioni a 48 kHz** (parte da 128 @ 48 kHz). La finestra mostra buffer, latenza dichiarata dal driver e latenza aggiunta dall'elaborazione interna.
- **48 kHz** è il rate nativo di quasi tutti i profili NAM: se il device lavora a un altro rate, il modello gira ricampionato (resampler corto, ~0.3 ms) e viene aggiunto circa un buffer di latenza. Per la latenza minima usa 48 kHz.
- **Windows**: usa un driver **ASIO** (quello della tua interfaccia, oppure ASIO4ALL/FlexASIO). Per compilare con ASIO serve l'SDK Steinberg (non ridistribuibile): `-DASIO_SDK_DIR=<cartella dell'SDK>`. Senza ASIO restano disponibili WASAPI (anche in modalità low-latency) e DirectSound.
- **Mac**: CoreAudio va benissimo con interfacce class-compliant; scegli la tua interfaccia come ingresso *e* uscita (evita aggregate device).
- Evita di usare il microfono interno come ingresso: con le casse si innesca il feedback.
- Il modello A2 **Lite** costa molto meno CPU del Full.

## Test DSP

```bash
cmake -S . -B build -DDANIRIG_BUILD_TESTS=ON
cmake --build build --target DaniRigTests
./build/DaniRigTests_artefacts/DaniRigTests [percorso/modello.nam]
```

Verificano gate, stabilità dei pedali a tutti i livelli, resampler, tempi del delay e tap tempo, decadimento del riverbero e il caricamento di un modello NAM (a 48 kHz e a 44.1 kHz, con swap Full→Lite a caldo).

## Struttura

```
Source/
  Main.cpp, MainComponent.*     finestra, barra superiore, audio settings, preset
  RigEngine.*                   callback audio e catena di moduli (mono → stereo)
  dsp/                          NoiseGate, Pedals (TS + DS-1), NamAmp, CabIR, Delay, Reverb,
                                Resampler, FracDelay, Biquad, Module/Param
  ui/                           Controls (manopole, footswitch, meter), Panels (un pannello per modulo)
installer/DaniRig.iss           installer Windows (Inno Setup)
```

Aggiungere un modulo: crea una classe che estende `MonoModule` o `StereoModule`, dichiara i parametri nel costruttore (`addFloat/addToggle/addChoice`), aggiungila a `monoChain`/`stereoChain` in `RigEngine` e crea il pannello in `ui/Panels.h`.

## Note

- **Licenze**: JUCE è AGPLv3 oppure con licenza commerciale/gratuita personale (con schermata "Made with JUCE" nella versione gratuita): controlla di essere in regola per come distribuisci l'app. Il core NAM è MIT.
- I due pedali sono modelli **ispirati** a Tube Screamer e DS-1 (non cloni di componente per componente): i valori dei componenti sono approssimati.
- L'ordine dei moduli è già un semplice vettore nel motore; il drag & drop per riordinarli nella UI non è ancora implementato.
