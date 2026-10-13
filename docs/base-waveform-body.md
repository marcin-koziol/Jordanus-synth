# Bazowy waveform za mało mięsisty - analiza i zmiana (2026-10-13)

Notatka przeniesiona z innego czatu: problem "bazowy waveform jest za mało mięsisty, zobacz
pierwsze 0.25 s harmonics1.wav".

## Stan: zmiana JUŻ wprowadzona (niezacommitowana)

`plugins/Jordanus/dsp/LeadVoice.hpp`:

- w `process()` pod podstawową dochodzi warstwa "body": szereg 1/n od 2. harmonicznej do
  `maxPartial` (ten sam limit pasma co flażolety), skalowany przez `fGains[1]` - czyli znika
  razem z podstawową, gdy w trybie LOW flażolet ją ścisza:
  ```cpp
  float body = 0.0f;
  for (int n = 2; n <= maxPartial; ++n)
      body += fSin[n] / (float)n;
  float partials = fGains[1] * (fSin[1] + kBody * body);
  ```
- `static constexpr float kBody = 0.25f;` (obok `kSubScale`),
- diagram przepływu sygnału w komentarzu nagłówka: "+ 1/n body series".

Nie budowane jako plugin (`./install.sh` nie uruchamiany), nie słuchane. Sprawdzone tylko offline:
kompilacja nagłówka (`g++ -std=c++17 -Wall`, także `tools/dsp_render.cpp`) i analiza widma.

## Diagnoza (harmonics1.wav, 50-250 ms, A3 = 220 Hz, tryb LOW, kółko na dole)

Poziomy harmonicznych względem podstawowej [dB]:

| k | referencja | przed zmianą | po zmianie (kBody 0.25) |
|---|---|---|---|
| 2 | -27.6 | -27.6 | -20 |
| 3 | -28.9 | -18.8 | -15 |
| 4 | -23.0 | -39.4 | -22 |
| 5 | -24.0 | -35.5 | -22 |
| 6 | -24.6 | -52.7 | -25 |
| 7 | -44.9 | -52.0 | -26 |
| 8 | -43.0 | -66.6 | -27 |
| 11 | -28.4 | -84.8 | -29 |
| 17 | -30.8 | -130 | -28 |
| 18 | -21.3 | -135 | -26 |
| 19 | -25.0 | -145 | -26 |

- Przed zmianą baza to praktycznie czysty sinus przez tanh z `drive = 0.05` - shaper robi tylko
  2./3. harmoniczną, powyżej 7. nie ma nic, więc dzwon kolumny 4 kHz nie miał czego podbić.
- Referencja ma gęsty "kożuch" -25…-45 dB aż do ~5 kHz z garbem ~4 kHz.
- Współczynnik szczytu (peak/RMS): referencja 2.06, przed 1.32, po 1.50.
- Średni błąd widma (k 2-30, podłoga -60 dB): przed 20.8 dB, po 8.7 dB.
- Głośność: RMS +0.4 dB, szczyt +1.5 dB względem stanu przed zmianą.

### Dlaczego gładkie 1/n, a nie tabela dopasowana do jednego pliku

Porównanie początków wszystkich referencji (harmonics1/2/3, harmo4/5-combided, pitch):

- garb ~4 kHz jest w każdym nagraniu (3960, 3835-4130, 4620 Hz) → to kolumna, już modelowana
  (`fCabBell`), brakowało jej tylko materiału;
- niskie harmoniczne (2-10) zmieniają się między nagraniami od -10 do -45 dB → zależą od
  zagrania, nie od instrumentu; dopasowanie tabeli do harmonics1 byłoby przeuczeniem.

### Przegląd poziomów kBody (z osobnym tłumieniem 2. harmonicznej `b2` i bez)

| kBody | b2 | błąd | crest | k2..8 |
|---|---|---|---|---|
| 0.15 | 0.2 | 6.7 dB | 1.27 | -28 -16 -28 -25 -30 -29 -31 |
| 0.20 | 1.0 | 7.7 dB | 1.35 | -22 -16 -24 -24 -27 -27 -29 |
| 0.25 | 1.0 | 8.7 dB | 1.50 | -20 -15 -22 -22 -25 -26 -27 |
| 0.30 | 1.0 | 9.5 dB | 1.64 | -19 -15 -21 -22 -24 -25 -26 |
| 0.50 | 0.2 | 11.5 dB | 1.96 | -26 -13 -20 -19 -22 -22 -24 |

0.15 ma najmniejszy błąd liczbowy, ale 4. harmoniczna zostaje ~5 dB pod referencją; wybrane 0.25
jako "więcej mięsa" kosztem lekko za głośnych 7-10 względem harmonics1. Osobne tłumienie 2.
harmonicznej nic nie wnosiło (referencje mają ją od -10 do -40), więc nie weszło.

## Otwarte punkty

1. **Odsłuch** - plik A/B (referencja → przed → po, wyrównane RMS):
   `/tmp/claude-1000/-home-van-projects-avl/a5daa983-eceb-47e3-9a1a-8df6237da955/scratchpad/jord/ab_ref_old_new.wav`
   (katalog tymczasowy - może zniknąć; odtworzysz go `tools/dsp_render.cpp` + analiza jak wyżej).
2. **3. harmoniczna za głośna** (-15 vs -29 w harmonics1) - robi ją tanh przy `drive 0.05`, nie
   warstwa body. W innych referencjach 3. bywa -16…-29, więc nie ruszane.
3. **Pomysł:** wystawić `kBody` jako pokrętło (np. "Body"/"Meat") - wymaga parametru w
   `Params.hpp`, obsługi w `JordanusPlugin.cpp` i miejsca w UI.
4. Sprawdzić zachowanie przy flażoletach (HIGH trzyma podstawową na 0.6, więc body też zostaje
   słyszalne pod flażoletem) i przy subach.
