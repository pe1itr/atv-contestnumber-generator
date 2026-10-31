# Onafhankelijke UDP/MPEG-TS-review — 25 oktober 2026

Beoordeeld: het nieuwe **Linux**-programma DATV Camera, `camera/src/`,
build-/testbestanden en de opt-in camera-uitbreiding van
`../streambridge/src/mux.hpp`. De bestaande ongecommitteerde Streambridge en
README waren de uitgangssituatie. De reviewer wijzigde geen productiecode of
testgrenzen. Definitieve encoderhash:
`9072b87580b788e85ab70ce399c67db2ba95222f6b485d10961adc44df5c0fb8`;
muxhash: `8a48ec2122250b3ba5e73dae8a24f3599ef38440b908f1f6a2dc17eb7a459d3a`.

Profiel: één onversleutelde H.264 Main-videodienst, geen audio, 4:3/16:9,
2–12 fps, kleine experimentele rasters; CBR-TS met 188-byte pakketten,
zeven per unicast IPv4/UDP-datagram. Dit is geen volledig DVB-uitzendprofiel.
De code gebruikt de gedeelde DVB-capaciteitsberekening zonder deze te wijzigen.

## Bronnen en afbakening

- [ITU-T H.222.0, editie 04/2025](https://www.itu.int/rec/T-REC-H.222.0):
  MPEG-2 Systems. De actuele integrale norm is betaald en niet volledig
  geraadpleegd; volledige conformiteit blijft **NIET GEVERIFIEERD**.
  De relevante verwijzingen naar §2.4.3 (TS/PES), §2.4.4 (PSI), §2.7.2
  (PCR) en het decoderbuffer-model worden tevens door onderstaande ETSI-bronnen gegeven.
- [ETSI TR 101 290 V1.4.1, 2020-06](https://www.etsi.org/deliver/etsi_tr/101200_101299/101290/01.04.01_60/tr_101290v010401p.pdf):
  §5.2.1–5.2.3, monitoring van continuïteit, PSI/SI, CRC en tijdstempels.
  §5.2.2 tabel 5.0b/noot 2 bevestigt de algemene PCR-grens 100 ms;
  40 ms is geen huidige harde DVB-grens. PCR-nauwkeurigheid: ±500 ns.
  De officiële geïndexeerde passages waren beschikbaar; directe PDF-openingen
  liepen op een timeout. Monitoring is geen volledige normcertificering.
- [ETSI EN 300 468 V1.19.1, 2025-02](https://www.etsi.org/deliver/etsi_en/300400_300499/300468/01.19.01_60/en_300468v011901p.pdf):
  §5.2 voor SI-secties, §6.2.13 voor RF-delivery-descriptors en §6.2.33 voor
  de service-descriptor. NIT, SDT, lege EIT-present/following en TDT zijn aanwezig.
- [ETSI TS 101 154 V2.9.1, 2025-05](https://www.etsi.org/deliver/etsi_ts/101100_101199/101154/02.09.01_60/ts_101154v020901p.pdf):
  §4.1.5.3 PCR ≤100 ms; §4.1.7 PAT/PMT 100 ms aanbevolen, verplicht voor
  distribution; §5.5.5.1 RAP maximaal 5 s; §5.6.1.2 SDTV Main/High met
  level_idc 30; §5.6.2.2–3 en §5.6.3.2–3 voor SDTV-framerates/rasters.
  Dit zijn aanvullende DVB-eisen, geen vrijstelling voor amateurtoepassingen.
- [ITU-T H.264, actuele editie 06/2026](https://www.itu.int/rec/T-REC-H.264):
  de status is gecontroleerd. Een volledige Annex C/HRD-conformiteitsanalyse
  is niet uitgevoerd; decoderacceptatie vervangt die niet.

## MPEG-TS: VOLDOET binnen het gemeten bereik

De definitieve matrix bevat drie presets bij zes RF-instellingen:
DVB-S 33/35 ksym/s, DVB-S2 33/35 ksym/s en DVB-T 35/40 kHz. Alle achttien
opnamen doorstaan TS/CRC/continuïteit/PCR/PTS/deadlines, service-identificatie en
FFmpeg-decodering. Er is alleen video; de door ffprobe gerapporteerde EPG-data
zijn SI en geen audiostream. Log: `build/final-captures.log`; uitvoer:
`build/checks-1792964893311718379/`.

Onafhankelijke controle van de definitieve 60-s ruistest bij **30160 bit/s,
160×120/6 fps**:

| Meting | Waarde |
|---|---:|
| Grootste PCR-interval | 99,734778 ms |
| Grootste PCR-afwijking t.o.v. CBR-bytepositie | 36,94 ns |
| Grootste PAT-/PMT-interval | 398,939 ms |
| Grootste SDT-interval | 1,296552 s |
| Grootste NIT-interval | 5,285942 s |
| Grootste EIT-interval, elk van sectie 0 en 1 | 1,595756 s |
| Grootste TDT-interval | 20,246154 s |

TSDuck: 605 PCR goed, nul boven 1 µs en één eerste PCR niet toetsbaar;
geen continuity-fouten. De afzonderlijke bytepositieberekening toetst ook de
strengere 500-ns-grens. Alle 360 frames decoderen; de volledige PTS-tijdlijn
wijkt maximaal één 90-kHz-tick af van de ideale 6-fps-cadans. Nieuwe decoders
die op 0,35/2,75/7,9 s midden in de opname instappen herstellen aantoonbaar:
32/46/47 gedecodeerde framehashes komen overeen met de volledige referentie.
Zie `build/verification/final-long-noise-transport.json` en
`build/verification/final-acquisition.log`.

Ook de grenzen **30160 bit/s, 160×120/12 fps** en **894305 bit/s,
640×360/12 fps, zwart-wit, sterk filter, GOP 15 s** slagen. Onafhankelijk
PCR-maximum: 99,734778 respectievelijk 38,680333 ms; beide <37 ns CBR-afwijking.
Uitvoer: `build/checks-1792965001508773007/`.

Een onafhankelijke muxsimulatie en de permanente `checks/test_mux.cpp`
bestrijken **275 unieke ondersteunde bitrates, elk 600 s**, inclusief 30080.
Alle eerste tabelverschijningen en afzonderlijke herhalingen blijven binnen
het gekozen monitoringprofiel: PAT/PMT 500 ms, SDT/EIT 2 s, NIT 10 s, TDT 30 s.
Maxima over de matrix: PAT/PMT 435,210 ms, SDT 1,523713 s, NIT 5,585146 s,
EIT-0 1,664692 s, EIT-1 1,843589 s, TDT 20,570126 s.
Dit bewijst planning; het is geen 600-s camerabelastingstest per combinatie.

## DVB/ETSI: AFWIJKINGEN en ontbrekende verificatie

- **Bewuste profielafwijking:** `src/settings.cpp:13` en `:26–27` bieden lage
  fps/kleine rasters; gemeten AVC Main@L1.1 bij 240×180/4 fps is geen
  standaard DVB-AVC SDTV-profiel volgens TS 101 154 §5.6.1–3.
- **Bewuste timingafwijking:** `../streambridge/src/mux.hpp:131` gebruikt
  PAT/PMT-doelen van 300 ms. Dit haalt de 100-ms distribution-eis uit
  TS 101 154 §4.1.7 niet, ook wanneer de 500-ms-monitoringtest slaagt.
- **Optionele profielafwijking:** `src/settings.cpp:27` staat GOP tot 15 s toe;
  boven 5 s wijkt dit af van TS 101 154 §5.5.5.1. De drie presets gebruiken 5 s.
- **Bestaande muxbeperking:** NIT bevat netwerk-/servicelijsten maar geen
  RF-delivery-descriptor (`../streambridge/src/mux.hpp:67`). Afstemming en
  signalering aan de fysieke uitgang zijn **NIET GEVERIFIEERD**; volledige
  DVB-SI-conformiteit wordt niet geclaimd. CAT is bij deze onversleutelde dienst
  **NIET VAN TOEPASSING**. TDT is aanwezig; TOT/lokale tijdinformatie wordt niet
  geleverd. Er zijn geen andere diensten/netwerken of EPG-schema's opgegeven.
- **NIET GEVERIFIEERD:** volledig T-STD/AVC-HRD-buffer-model, fysieke PCR-timing,
  langdurige camera-/klokdrift, daadwerkelijk RF-pad en ontvangerinteroperabiliteit.
  De beperkte timestamp-wrap-unit-test is geen langdurige volledige streamtest.

Tijdens de review gevonden en gecorrigeerd: de eerste cameraplanner gaf
EIT-following-gaten van 2,2 s bij 30080 bit/s. Kortere camera-EIT-doelen en
controle van beide secties lossen dit op. De oorspronkelijke 1-fps-optie
voor bewegend beeld is verwijderd: zij gaf PTS-afstanden van 1000 ms,
voorbij de 700-ms-monitorgrens; de definitieve minimumwaarde is 2 fps.

## Projectafspraken en testuitvoering

Geslaagd: `make all test-linux` (hoofd-agent, gedeelde logica en 1118 JPG's),
`make test-ts`, `make test-udp`, `make -C streambridge test-unit`,
camera-build, GUI-test, camera-muxsimulatie en de hierboven genoemde
integratie-/grenstests. Regressielogs staan in `build/verification/`.
De bestaande bridge-standaard blijft ongewijzigd: de camera-optie staat standaard
uit. SEI-behoud, IDR-prioriteit en timestamp-wrap blijven door de bridge-unit-test
gedekt. Er zijn geen nieuwe permanente bewakingstaken gestart.

De Linux-tests vereisten deels uitvoering buiten de sandbox door geblokkeerde
D-Bus-/UDP-sockets. Tools beschikbaar: FFmpeg/ffprobe 7.1.5 en TSDuck 3.44-4578.
Een aanvullende bestaande Windows/Wine-regressie is op verzoek afgebroken
(exit 130); deze is geen afgerond testbewijs en niet vereist voor de nieuwe
Linux-functionaliteit. Er is geen Windows-cameraversie gemaakt of getest.

Een echte webcam/interne camera was niet beschikbaar voor deze review.
De GStreamer-testbron bewijst de verwerkings-/encoder-/transportketen, maar
niet de camera-driver, hardwareformaten of waargenomen kwaliteit van een echte
buitenscène met een zwaaiende persoon. Stop/SIGTERM mag een laatste PES afbreken;
alle volledig decodeerbaar geteste opnamen gebruiken het afgeronde `--seconds`-pad.

De **definitieve UDP-loopbacktest** slaagt tweemaal, inclusief herstart,
preview zonder UDP en tijdig stoppen (`build/final-network.log`, captures
`build/checks-1792965316707591448/`). Gemeten bitrate: 30409,892 en
30409,934 bit/s tegenover 30411 ingesteld. Datagramintervallen:
345,278–346,914 en 345,330–346,963 ms; alle datagrammen bevatten 1316 bytes.
De definitieve UDP-capture is ook onafhankelijk op CC/CRC/PCR-bytepositie en
PTS-deadlines gecontroleerd (`build/verification/final-udp-transport.json`).

Deze UDP-bundeling levert op de socket geen PCR om de 100 ms: dat geldt voor
de gereconstrueerde continue TS. De modulator moet de datagrammen bufferen en
op de ingestelde TS-bitrate uitklokken. Loopbackjitter is geen fysieke
PCR-nauwkeurigheidsmeting.

**Eindadvies:** geen resterende blokkade binnen het beschreven Linux-experiment.
MPEG-TS en projectafspraken voldoen binnen de aantoonbaar geteste scope.
Volledige DVB/ETSI-conformiteit wijkt af op de hierboven benoemde punten;
fysieke camera/RF en volledige decodermodellen blijven niet geverifieerd.
