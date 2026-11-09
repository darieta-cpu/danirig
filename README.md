# DaniRig

App **standalone** per chitarra live, Mac (universal: Apple Silicon + Intel) e Windows.

**Catena del segnale** (ogni blocco è un modulo con on/off, parametri e crossfade senza click):

```
Ingresso → Noise Gate → **Compressor** → **Multi Mod** → Overdrive (stile TS) → Distortion (stile DS-1) → **Pedale NAM** → Ampli NAM (A2) → Cabinet IR → Delay (tap) → Reverb → Uscita
```

| Modulo | Cosa fa |
|---|---|
| **Noise Gate** | Gate senza lookahead (zero latenza), soglia/release/range, isteresi e hold |
| **Compressor** | Compressore stile MXR Dyna Comp: **Sens** e **Output** (più Sens = più sustain, con make-up automatico parziale), attacco veloce, rilascio medio, ginocchio morbido. In più **Mix** per la compressione parallela (100 % = come il pedale). Zero latenza. Parte spento |
| **Multi Mod** | Pedale multi-modulazione in stile TC Plethora X1 con tre modi: **Phase** (4 stadi allpass spazzolati, stile Phase 90, con un po' di rigenerazione che cresce con Depth), **Chorus** (BBD mono, stile CE-2), **Tremolo** (profondità in dB, onda sinusoidale leggermente "squadrata"). Cambiando modo, Rate e Depth si portano ai valori tipici di quel modo. Rate, Depth, Level e **tap tempo** con suddivisione (1/2, 1/4, 1/8 puntato, 1/8, 1/16): il tap imposta la frequenza dell'LFO e ne riallinea la fase. Parte spento |
| **NAM Pedal** | Un secondo caricatore NAM, **davanti all'ampli**, per il profilo di un pedale reale (overdrive, boost, fuzz...). Gain/Output, auto-level, A2 Full/Lite, precedente/successivo nella cartella. Parte spento |
| **Overdrive** | Modello analogico di uno stadio op-amp con diodi nel feedback + gobba sui medi, oversampling 4x |
| **Distortion** | Booster a transistor + op-amp ad alto gain + clipping duro a diodi + tone "scoop", oversampling 4x |
| **NAM Amp** | Carica `.nam` (A1 e **A2**, con selettore Full/Lite per i modelli A2), EQ a 3 bande, auto-level, caricamento in background con crossfade tra modelli, precedente/successivo nella cartella |
| **Cabinet** | Convoluzione IR a latenza zero (JUCE), low/high cut, livello, precedente/successivo nella cartella |
| **Delay** | Tap tempo, suddivisioni (1/4, 1/8 puntato, 1/8, terzina), repeats, tone, mod, modo Digital/Analog, ping-pong |
| **Reverb** | FDN a 8 linee con diffusione, damping, pre-delay, low cut, leggera modulazione |

**Riordinare i moduli**: trascina un pedale dalla fascia alta del pannello (dove c'è la maniglia a puntini). Si riordinano liberamente i moduli *prima dell'ampli* (Gate, Compressor, Multi Mod, NAM Pedal, Overdrive, Distortion) e quelli *dopo il cab* (Delay, Reverb). Ampli e cabinet restano al loro posto. Lo schema "SIGNAL CHAIN" mostra l'ordine reale; quando cambi ordine c'è un fade-in di 12 ms per evitare click.

## Banchi, patch e scene (la fascia in basso)

**5 banchi x 3 patch = 15 patch**, ognuno con 3 scene. In basso trovi:

- il **display LCD**: a sinistra il numero *banco-patch* in grande (es. `2-3`), a destra il nome del patch caricato e, sotto, il nome dell'eventuale scena attiva (`--` e "(no patch)" se non è caricato nulla);
- il **selettore di banco** `<  BANK n  >`, con cinque puntini: pieno = banco che contiene patch, anello verde = banco del patch caricato, punto grande = banco visualizzato sugli switch;
- il selettore **PATCH / SCENE**;
- i **tre switch a pedale** (1-2-3), con LED, nome modificabile (doppio click) e pulsante **SAVE**.

Come funziona:

- **PATCH**: gli switch lavorano sul banco visualizzato. Premendone uno si richiama uno stato *completo*: parametri, on/off, ordine dei moduli, modello NAM, pedale NAM e IR. Se due patch usano lo stesso modello/IR, questo non viene ricaricato. Sfogliare un altro banco *non* cambia il suono: cambia solo cosa fanno gli switch; il patch caricato resta finché non ne premi un altro.
- **SCENE**: ogni switch richiama un'istantanea dei soli parametri e on/off *dentro al patch caricato* (come gli "snapshot" dei multieffetto): cambio istantaneo, nessun caricamento. Le scene appartengono al patch in cui le salvi.
- **SAVE** memorizza lo stato attuale nello slot (nella modalità selezionata). Gli slot vuoti si riconoscono dal nome "(empty)"; i nomi predefiniti sono `Patch banco-slot` (es. `Patch 2-1`).
- Tutto viene salvato con le impostazioni, nei preset `.dprig` e nel progetto della DAW (plugin). I preset salvati con la versione a 3 slot si aprono nel banco 1.
- Un patch che non ha modello NAM o IR non scarica quelli già caricati: li lascia come sono.

## Preset portatili (EXPORT) e file mancanti

- **SAVE** salva il preset `.dprig` con i *percorsi assoluti* dei file: va bene sullo stesso computer, ma se sposti o rinomini i file non vengono ritrovati.
- **EXPORT** crea una cartella con il nome che scegli (es. `MioSuono/`) con dentro `MioSuono.dprig`, `models/` (i `.nam` dell'ampli e del pedale, compresi quelli usati dai patch del banco) e `ir/` (le IR), con percorsi **relativi**. Copia l'intera cartella dove vuoi (un altro computer, un backup, un amico) e carica il `.dprig` con **LOAD**: i file vengono ritrovati accanto al preset. Due file con lo stesso nome ma contenuto diverso vengono rinominati (`amp.nam`, `amp_2.nam`).
- **LOAD** accetta sia preset normali sia preset esportati.
- Se al caricamento (di un preset, di un patch, dell'ultimo stato all'avvio o di un progetto della DAW) manca un modello NAM o un'IR, compare un messaggio con il percorso del file mancante; gli altri parametri vengono caricati comunque e il modello già presente resta com'è.

Le impostazioni (e l'ordine dei moduli, i percorsi di modelli e IR) vengono salvate all'uscita e ripristinate all'avvio. `SAVE`/`LOAD` salvano/caricano preset `.dprig`.

## Plugin (VST3 e AU)

Oltre all'app, la build genera il rig come plugin, con la stessa interfaccia e lo stesso motore:

| Sistema | Formati | Dove |
|---|---|---|
| macOS (universal) | **VST3** e **AU** | artefatto `DaniRig-Plugins-macOS-VST3-AU` (zip con `DaniRig.vst3`, `DaniRig.component` e `install-plugins-mac.command`) |
| Windows (x64) | **VST3** | incluso nell'installer `DaniRig-Setup-*.exe` (componente opzionale, installato in `C:\Program Files\Common Files\VST3`) e come zip `DaniRig-VST3-Windows` |

- **Mac**: estrai lo zip e fai doppio click su `install-plugins-mac.command` (copia i plugin in `~/Library/Audio/Plug-Ins/` e toglie la quarantena). In Reaper: *Options > Preferences > Plug-ins > VST* (o *AU*) > *Re-scan*.
- **AU esiste solo su Mac**; il **VST2** non è più compilabile (Steinberg ha ritirato l'SDK), quindi il formato "VST" è il VST3.
- Ingresso mono o stereo (con il menu *Input* si sceglie il canale della chitarra), uscita stereo.
- Lo stato (parametri, ordine dei moduli, percorsi di modelli NAM e IR) viene salvato con il progetto della DAW.
- In questa prima versione i parametri **non** sono esposti all'automazione della DAW: si regolano dall'interfaccia.
- Nel plugin non c'è il pulsante *Audio Settings* (il device lo gestisce la DAW).

## Build in locale (Mac o Windows)

Servono CMake ≥ 3.22 e un compilatore C++20 (Xcode/Command Line Tools, oppure Visual Studio 2022).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target DaniRig DaniRigPlugin_VST3            # + DaniRigPlugin_AU su Mac
```

JUCE 9.0.5 e il core NAM vengono scaricati da soli (FetchContent). L'app esce in `build/DaniRig_artefacts/`, i plugin in `build/DaniRigPlugin_artefacts/`.

## Build con GitHub Actions (senza Xcode)

Fai push del progetto su un repository GitHub: il workflow `.github/workflows/build.yml` produce

- `DaniRig-macOS-universal.dmg` (app, arm64 + x86_64, firma ad-hoc)
- `DaniRig-Plugins-macOS-universal.zip` (VST3 + AU universali con script di installazione)
- `DaniRig-Setup-<versione>.exe` (installer Windows: app + plugin VST3, runtime MSVC statico)
- `DaniRig-VST3-Windows.zip` (solo il plugin VST3)

e lancia anche i test DSP su Linux. Gli artefatti si scaricano dalla pagina dell'esecuzione (tab *Actions*).

**Primo avvio su Mac**: l'app non è notarizzata, quindi tasto destro → *Apri* (oppure `xattr -cr /Applications/DaniRig.app`). Al primo avvio macOS chiede il permesso per il microfono: è l'ingresso audio della tua interfaccia, va concesso.

## Latenza: come tenerla bassa

- **Buffer 64–128 campioni a 48 kHz** (parte da 128 @ 48 kHz). La finestra mostra buffer, latenza dichiarata dal driver e latenza aggiunta dall'elaborazione interna.
- **48 kHz** è il rate nativo di quasi tutti i profili NAM: se il device lavora a un altro rate, il modello gira ricampionato (resampler corto, ~0.3 ms) e viene aggiunto circa un buffer di latenza. Per la latenza minima usa 48 kHz.
- **Windows e ASIO**: per la latenza minima serve ASIO, ma JUCE può usarlo solo se compilato con l'SDK Steinberg. Scarica l'SDK dal sito sviluppatori Steinberg, estrailo in `third_party/asiosdk/` (deve esistere `third_party/asiosdk/common/iasiodrv.h`) e rifai la build: il CMake lo trova da solo (oppure `-DASIO_SDK_DIR=<cartella>`). Dall'ottobre 2025 l'SDK è con doppia licenza (GPLv3 oppure licenza proprietaria Steinberg, gratuita): per uso personale non cambia nulla, se distribuisci l'app ad altri leggi i termini. Senza ASIO restano WASAPI Exclusive e Low Latency, che JUCE offre tra i tipi di device.
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
  Main.cpp, MainComponent.*     app standalone: finestra e device audio
  plugin/                       PluginProcessor/Editor (VST3 e AU)
  ui/RigUI.*                    interfaccia completa (condivisa da app e plugin)
  RigEngine.*                   callback audio e catena di moduli (mono → stereo)
  dsp/                          NoiseGate, DynaComp, MultiMod, Pedals (TS + DS-1), NamAmp, CabIR, Delay, Reverb,
                                Resampler, FracDelay, Biquad, Module/Param
  assets/                       icona (make_icon.py la rigenera: icon_1024.png, icon_256.png, icon.ico)
  ui/                           Controls (manopole, footswitch, meter), Panels (un pannello per modulo)
installer/DaniRig.iss           installer Windows (Inno Setup)
```

Aggiungere un modulo: crea una classe che estende `MonoModule` o `StereoModule`, dichiara i parametri nel costruttore (`addFloat/addToggle/addChoice`), aggiungila a `monoChain`/`stereoChain` in `RigEngine` e crea il pannello in `ui/Panels.h`.

## Note

- **Licenze**: JUCE è AGPLv3 oppure con licenza commerciale/gratuita personale (con schermata "Made with JUCE" nella versione gratuita): controlla di essere in regola per come distribuisci l'app. Il core NAM è MIT.
- I due pedali sono modelli **ispirati** a Tube Screamer e DS-1 (non cloni di componente per componente): i valori dei componenti sono approssimati.
- L'ordine dei moduli è già un semplice vettore nel motore; il drag & drop per riordinarli nella UI non è ancora implementato.
