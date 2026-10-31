**Onafhankelijke UDP/MPEG-TS-review — 26 september 2026**


**Laatste aanvulling: RTSP-opstartbuffer en 25→30-fps-conversie**

De werkelijke telefoon voltooide na de eerdere reparaties de RTSP-handshake,
maar Streambridge verbrak daarna zelf de verbinding. De lokale reproductie
met latency 2000 ms, breedte 960 en 30 fps bevestigt de reden: een audio-burst
overschreed de raw-invoerbuffer voordat video beschikbaar kwam
(`larix-retry-reproduce.log`, audio-PTS 155074, video-PTS −1).
Daarnaast blokkeerde `videorate drop-only=true` conversie van een 25-fps-bron
naar 30-fps-uitvoer. Dit waren functionele ingangsafwijkingen; de fallback-TS
bleef beschikbaar na de eerdere crashcorrectie.

Definitieve diff tegen `build/verification/input-before-larix-retry.hpp`:
`src/input.hpp:89` laat videorate ook beelden dupliceren; regels 96–103
zetten beide RTSP-appsinks op `sync=true` met behoud van `async=false`.
De gedeelde pipelineklok doseert zo de gedecodeerde buffers voordat ze de
raw-tijdlijn bereiken. De vaste raw-buffergrenzen zijn niet verruimd.
Dit gedrag wordt onderbouwd door de
[officiële GstBaseSink-bron, klok­synchronisatie vóór render](https://raw.githubusercontent.com/GStreamer/gstreamer/main/subprojects/gstreamer/libs/gst/base/gstbasesink.c).
Een aanvullende waarschuwing registreert de reden bij opnieuw verbinden.
SRT houdt zijn oorspronkelijke sinkklokinstelling; videorate is wel gedeeld.

Definitieve `input.hpp` SHA-256:
`0a3959f20de9b5f20d7d9800bed5040f6325484fddf958fb55a71fb93c3b2ad8`.
RTSP-client, uitvoerencoder en mux behouden de hashes uit de vorige reviews.
Captures: `build/checks-1790453723644836319/`; volledige eindlog
`build/verification/larix-clock-final.log`, extra cadencecontrole
`larix-clock-cadence.log`. De gebruikersinstellingen zijn apart getest in
`build/checks-1790453695704006698/retry.ts`.

**MPEG-TS — VOLDOET binnen meetbereik:** onafhankelijke analyse van de drie
UDP-mediacaptures geeft telkens maximaal 37,573556 ms PCR-interval,
36,913 ns CBR-byteafwijking en nul extra discontinuity-indicators.
Dit blijft binnen 100 ms en ±500 ns volgens ETSI TR 101 290 V1.4.1 §5.2.2,
tabel 5.0b, officiële bron bij de basisreview. TSDuck op 2000-ms/30-fps:
479 PCR goed bij 1 µs, één eerste PCR niet toetsbaar, geen continuity-fouten.
De aparte bytepositiemeting onderbouwt de strengere 500-ns-toets.
Dezelfde capture en de gebruikerscapture decoderen onafhankelijk zonder
FFmpeg-meldingen. De suite controleert tevens TS/PTS/PSI/SI en decode.

De gebruikerscapture is daadwerkelijk 960×540 bij 30/1 fps. Bij 30 fps
zijn de gemeten video-PTS-stappen 2999/3000/3001 ticks door omzetting van
nanoseconden naar 90 kHz; de volledige tijdlijn wijkt maximaal één tick af
van `PTS[0]+index*3000`. Er is geen cumulatieve cadencedrift in de gemeten
capture. De twee 25-fps-regressies hebben exact 3600 ticks per frame.
De nieuwe cadenceassert toetst deze hele tijdlijn; bestaande PCR-, CC- en
decodegrenzen zijn niet versoepeld.

**DVB/ETSI — beperkingen blijven:** 960×540/30 fps is een beperkt DATV-profiel,
geen bewezen DVB-AVC-uitzendprofiel. De eerdere profiel/SI-afwijkingen en
ontbrekende T-STD/HRD- en fysieke verificatie blijven onverminderd gelden.
De ingangsbuffer en frameconversie voegen geen bewijs van normconformiteit toe.

**Projectafspraken — VOLDOET binnen tests:** Larix-simulatie bij 200 en
2000 ms, native RTSP, idle-sluiten en verlies/herstel slagen. De 2000-ms-proef
gebruikt een latere beeldmeetperiode vanwege de ingestelde inputvertraging;
transportcriteria blijven gelijk. Gemeten bitrate/maximaal UDP-interval:
Larix 200 ms 440308,371 bit/s / 24,015 ms; Larix 2000 ms met 30 fps
440306,767 bit/s / 24,203 ms; native 440308,294 bit/s / 24,448 ms.
Stilte-RMS is telkens 0; herstelde audio circa 18499/18472/18494.
SRT Caller/Listener en beide failovers slagen opnieuw (`larix-clock-srt.log`,
`build/checks-1790453726614337353/`). Beide contestbuilds en
`make all test-linux` slagen (`larix-clock-contest.log`, 946 JPG's).

Geen nieuwe blokkade binnen deze geteste scope. Een werkelijke nieuwe
Android-publicatie met de definitieve timingcorrectie is nog
**NIET GEVERIFIEERD**. Onafhankelijke bronklok-lipsync, langdurige drift en
fysieke netwerk-/RF-timing blijven niet gemeten. Onderstaande aanvullingen
beschrijven eerdere snapshots.

**Laatste aanvulling: crash bij programmatisch sluiten van RTSP-client**

De eerdere conclusie over RTSP geldt met deze noodzakelijke correctie.
`build/verification/mutex-crash-backtrace.log` toont SIGABRT via
`gst_rtsp_client_finalize` → `g_mutex_clear` tijdens `gst_rtsp_client_close`,
aangeroepen uit `RtspReceiver::disconnect`. De geleende clientreferentie
kon tijdens het sluiten verdwijnen terwijl de interne watch-mutex nog
vaststond. Ernst: hoog, de procescrash onderbrak ook de continue TS-uitvoer.
De pre-fix reproductie staat in
`build/checks-1790452725641092888/larix-control.log`.

De definitieve wijziging in `src/rtsp.hpp:57–64` houdt een tijdelijke sterke
referentie vast tot `gst_rtsp_client_close` terugkeert. Zowel `disconnect`
als destructor/client-filter gebruiken deze helper (regels 178 en 160).
De onafhankelijke bronreview vindt hier geen resterende lifetime-afwijking.
De [officiële GStreamer-bron, gst_rtsp_client_close](https://raw.githubusercontent.com/GStreamer/gstreamer/main/subprojects/gst-rtsp-server/gst/rtsp-server/rtsp-client.c)
laat de watch-destroy binnen de mutex zien; het concrete foutpad is bovendien
bewezen door de core en de reproductie op de gebruikte bibliotheek.
Dit is een objectlevensduurcorrectie, geen nieuwe MPEG/DVB-norminterpretatie.

Definitieve `rtsp.hpp` SHA-256:
`f9fd757568a805e8654e8d45059cf0c30bc57917ea1eece77ccd76512d18a0a5`.
Diffbasis: `build/verification/rtsp-before-mutex-fix.hpp`.
Uitvoer: `build/checks-1790452787899948165/`; log `mutex-rtsp.log`.
Outputencoder, tijdlijn en mux zijn ongewijzigd.

**MPEG-TS — VOLDOET binnen gemeten lopende uitvoer:** onafhankelijke analyse
van idle, Larix-simulatie en native-regressie geeft maximaal 37,573556 ms
PCR-interval, 36,913 ns CBR-byteafwijking en nul extra discontinuity-indicators.
De toepasselijke grenzen blijven 100 ms en ±500 ns uit ETSI TR 101 290
V1.4.1 §5.2.2, tabel 5.0b; bron bij de basisreview hieronder.
TSDuck op idle: 561 PCR goed bij 1-µs-testgrens, één eerste PCR niet toetsbaar,
geen continuity-fouten. De aparte bytepositiemeting onderbouwt 500 ns.
Beide 18-seconden UDP-mediacaptures doorstaan TS/PSI/SI/PTS-controles en
onafhankelijke `ffmpeg -v error -i ... -f null -` zonder meldingen.

**Bestaande stopbeperking — AFWIJKING bij afgebroken capture:**
`larix-idle.ts` wordt na de controletest abrupt met SIGTERM gestopt.
FFmpeg meldt daar `error while decoding MB 12 9, bytestream -36` in het
laatste videoframe. `src/engine.cpp:251–272` draint PES alleen bij de
ingestelde tijdslimiet, niet bij deze externe stop. Dit is geen foutloos
volledig decodeerbaar eindbestand; alleen de lopende TS-timing en structuur
worden voor deze idle-capture goedgekeurd. De twee afgeronde mediacaptures
vertonen deze fout niet. Geen nieuwe muxwijziging in deze crashfix.

De definitieve idlefixture gebruikt daarom een expliciete duur van 24 s en
het bestaande drainpad; productiecode en decodeercriteria blijven gelijk.
`build/verification/mutex-idle-ts.log` en
`build/checks-1790452873494088871/larix-idle.ts` slagen voor de volledige
TS/PTS/PSI/SI- en decodeercontrole. Onafhankelijk herhaald: FFmpeg zonder
meldingen; TSDuck geen continuity-fouten, 638 PCR goed bij 1 µs en één eerste
PCR niet toetsbaar. Dit bewijst de normale getimede afsluiting; het neemt de
hierboven benoemde beperking van abrupt stoppen niet weg.

**DVB/ETSI — eerdere beperkingen blijven:** geen nieuwe conformiteitsclaim;
ontbrekende RF-delivery-descriptor, HD-profielafwijkingen en onbewezen
T-STD/HRD-model blijven zoals hieronder beschreven. Fysieke timing en
langdurige belasting blijven **NIET GEVERIFIEERD**.

**Projectafspraken — sluitregressie VOLDOET binnen tests:** native en
Larix-SDP maken een volledige RECORD-verbinding zonder RTP; de ontvanger
sluit die zelf, blijft leven en accepteert de volgende verbinding. Dezelfde
helper wordt gebruikt voor het foutpad uit de core. Geautomatiseerde
mediaproeven bevestigen opnieuw beeld/geluid, No Signal/stilte en herstel.
Loopbackbitrate: 440305,714 en 440306,153 bit/s; grootste UDP-intervallen
24,285 en 24,498 ms. Stilte-RMS 0, herstelde audio circa 18494.
Build/unit-test slagen (`mutex-build.log`). De voorgaande volledige
`make all test-linux` slaagde ook (`larix-fix-contest.log`, 946 JPG's).

De aangetoonde crashoorzaak is gecorrigeerd; geen nieuwe blokkade voor deze
fix. Een nieuwe echte telefoonpublicatie na de crashcorrectie blijft
**NIET GEVERIFIEERD**. De volgende aanvullingen zijn historische snapshots.

**Laatste aanvulling: Larix Android SDP-compatibiliteit**

De telefoon-diagnose in `build/verification/larix-rtsp-diagnose.log` toont
Larix Android 1.5.13 met `RTP/AVP/TCP` in beide SDP-mediatracks en een
ANNOUNCE-antwoord 415. `src/rtsp.hpp:18–26` normaliseert nu uitsluitend deze
exacte schrijfwijze naar `RTP/AVP` op een SDP-kopie, vóór de basishandler.
Payloadnummers, H.264-fmtp en AAC 44100-Hz/mono-configuratie blijven intact;
SETUP blijft TCP-only (`src/rtsp.hpp:134`). De officiële GStreamer-bron,
[rtsp-media.c, default_handle_sdp](https://raw.githubusercontent.com/GStreamer/gstreamer/main/subprojects/gst-rtsp-server/gst/rtsp-server/rtsp-media.c),
controleert de profielnaam exact. Deze upstream-main-bron verklaart het
mechanisme; de fixture bevestigt het gedrag op de geïnstalleerde 1.26.11.
Dit is een gerichte interoperabiliteitsaanpassing, geen protocolcertificering.

Beoordeelde `rtsp.hpp` SHA-256:
`33c54428ea7e4192e4a04321918f1f920cc2cfaa10ac3c40a5322ca68aadf831`.
`input.hpp`, `engine.cpp` en `mux.hpp` zijn identiek aan de volgende review.
Log: `build/verification/larix-fix-integration.log`; definitieve captures:
`build/checks-1790452168949506643/`.

**MPEG-TS — VOLDOET binnen meetbereik:** beide 18-seconden UDP-captures,
`larix-sdp.ts` en `rtsp-native-after-larix-fix.ts`, hebben onafhankelijk gemeten
maximaal 37,573556 ms PCR-interval, maximaal 36,913 ns CBR-byteafwijking en
nul extra discontinuity-indicators. Dit blijft binnen 100 ms respectievelijk
±500 ns volgens ETSI TR 101 290 V1.4.1 §5.2.2, tabel 5.0b; de officiële bron
staat bij de basisreview hieronder. TSDuck `continuity` plus `pcrverify
--bitrate 440310 --jitter-max 1` op de Larix-capture: 479 PCR goed, één eerste
PCR niet toetsbaar, geen continuity-fouten. De strengere 500-ns-toets komt
uit de afzonderlijke bytepositiemeting. TS/PSI/SI/PTS en FFmpeg-decode slagen.

**DVB/ETSI — bestaande AFWIJKINGEN en NIET GEVERIFIEERD blijven:** codecuitvoer
en mux zijn ongewijzigd. De beperking tot het beschreven DATV-profiel, de
ontbrekende RF-delivery-descriptor, HD-Main/1440-profielafwijkingen en het
onbewezen T-STD/HRD-model blijven gelden. Fysieke netwerk-/RF-timing,
volledige RTP/RTCP-conformiteit en duurbelasting zijn niet hiermee bewezen.

**Projectafspraken — VOLDOET binnen tests:** de exacte telefoon-SDP-fixture
slaagt voor OPTIONS/ANNOUNCE/SETUP/RECORD/TEARDOWN; native `RTP/AVP` blijft
werken. `RTP/SAVP/TCP` en onbekend `RTP/OTHER` worden geweigerd, zodat de
normalisatie geen beveiligingsprofiel afzwakt. Een native publisher met de
Larix-protocolspelling en echte H.264 High plus AAC 44100-Hz/mono levert
beeld en geluid, blauw/stilte bij verlies en herstel zonder transportreset.
Deze mediasimulatie gebruikt een eigen SPS en is geen bitexacte Larix-bron.
De Larix-fixture meet 440309,059 bit/s en maximaal 24,082 ms UDP-interval;
native regressie 440307,558 bit/s en 24,055 ms. Stille audio meet RMS 0;
herstelde audio circa 18496 respectievelijk 18502. Meetpunt is loopback.
Build en unit-test slagen (`larix-fix-build.log`).

Geen nieuwe blokkade voor deze correctie. Een geslaagde nieuwe publicatie
vanaf de werkelijke telefoon na deze fix is **NIET GEVERIFIEERD**; de telefoon
leverde de foutdiagnose, de geautomatiseerde fixtures bewijzen de correctie.
De volgende aanvullingen beschrijven historische snapshots.

**Laatste aanvulling: RTSP-publicatie naast SRT**

Definitieve RTSP-code en uitvoer onafhankelijk beoordeeld. `src/rtsp.hpp`
implementeert een RECORD/TCP-ontvanger met SDP-afhankelijke H.264- en
AAC-depayloaders (MPEG4-GENERIC of MP4A-LATM), één actieve publicatie en een
private GLib-context. `src/input.hpp:99` voert de gedecodeerde beelden en PCM
naar de bestaande raw-tijdlijn. Dezelfde uitvoerencoder en mux blijven draaien
bij bronverlies en herstel. Cleanup brengt de oude media naar NULL vóór reset.
De clientcallbacks gebruiken de pool-mainloop bij `max-threads=0`, conform de
[officiële GStreamer-threadpooldocumentatie](https://gstreamer.freedesktop.org/documentation/gst-rtsp-server/rtsp-thread-pool.html#gst_rtsp_thread_pool_set_max_threads).

Bronhashes van deze herreview:

```text
rtsp.hpp   a25b38da4e7ae5a45e9a790b7189310de54b77fdb332b410ce3bf2d7013eaebd
input.hpp  e8d70ed9314a3c6de391af225457383ca71e84fd9c5b7fe0e8826e8b5f4e0977
engine.cpp f19ea68a9da010d318335366c4897bf9d3658cfcb7374e40eafa18b1c09f0ca7
mux.hpp    56e2436642cd125f121e3a5a930689bf5635b4b3a7ed8dc59ba511655ebc8765
```

Captures `build/checks-1790451034653129289/`, log
`build/verification/rtsp-final.log`: vijf publicatieproeven van 18 seconden
op 440310 bit/s, 320×240/25 fps. De tests gebruiken een echte GStreamer
`rtspclientsink`, ontvangen UDP op loopback en decoderen de opgenomen uitvoer.

| Geval | Gemeten bit/s | Grootste UDP-interval, ms |
|---|---:|---:|
| AAC MPEG4-GENERIC, abrupt verlies/herstel | 440309,339 | 24,620 |
| AAC LATM, TEARDOWN/herstel, omgekeerde trackvolgorde | 440307,787 | 24,526 |
| Alleen video, zonder AAC-uitvoer | 440307,860 | 24,430 |
| Audio aangeleverd, uitvoeraudio uit | 440307,813 | 24,627 |
| Geen bronaudio, wel stille AAC-uitvoer | 440308,647 | 24,554 |

**MPEG-TS — VOLDOET binnen meetbereik:** onafhankelijke analyse van alle vijf
captures geeft maximaal 37,573556 ms PCR-interval, 36,913 ns afwijking ten
opzichte van de exacte CBR-bytepositie en nul extra discontinuity-indicators
na opstart. TSDuck controleert de GENERIC- en LATM-captures ieder met 479 PCR's
zonder overschrijding van 1 µs en zonder continuity-fouten. De strengere
±500-ns-beoordeling steunt op de aparte bytepositiemeting, volgens de hieronder
aangehaalde TR 101 290 V1.4.1 §5.2.2. TS/PSI/SI/PTS- en decodeertests slagen.

**DVB/ETSI — bestaande beperkingen blijven:** RTSP is een extra ingang;
encoderinstellingen en muxer zijn hierbij niet gewijzigd. Dit voegt geen
bewijs toe voor volledige DVB-AVC-, SI-, T-STD/HRD- of fysieke conformiteit.
RTP wordt nu daadwerkelijk aan de ingang gebruikt, maar niet aan de TS-uitgang.
Volledige RTP/RTCP-protocolconformiteit en nauwkeurige A/V-synchronisatie met
onafhankelijke bronklokken zijn **NIET GEVERIFIEERD**.

**Projectafspraken — VOLDOET binnen tests:** abrupt verbreken en TEARDOWN
leveren automatisch No Signal, stilte en teruggekeerd beeld/geluid zonder
transportreset. De stille fase meet RMS 0; herstelde bronaudio circa 18497 en
18495. De suite verifieert weigering van een tweede publisher, ontbrekende of
uitgeschakelde audio, stoppen tijdens wachten en opnieuw binden van dezelfde
poort. De laatste bronhercontrole omvat begrenzing van het SDP-payloadnummer
tot 0–127, een RTSP-requestbodylimiet van 65536 bytes en periodieke opruiming
van verlopen sessies. Ongeldige SDP en een ongeldige grote payloadwaarde
worden geweigerd; een volgende OPTIONS-aanvraag werkt. Deze limieten zijn
implementatiekeuzes; er is geen algemene protocol- of beveiligingscertificering.
GTK/configuratieproeven slagen. De SRT-regressie met Caller, Listener
en beide failoverrollen slaagt opnieuw: `rtsp-srt-regression.log`, captures
`build/checks-1790450912978812513/`. De hoofd-agent rapporteert beide bestaande
builds en `make all test-linux` geslaagd (`rtsp-contest-linux.log`).

Geen nieuwe blokkade binnen het beperkte DATV-profiel. Werkelijke Larix op een
telefoon, langdurige RTSP-belasting, HD via RTSP en fysieke netwerk-/RF-timing
zijn **NIET GEVERIFIEERD**. De geteste native publicatie is geen vervanging voor
die interoperabiliteits- en duurmetingen. De volgende aanvullingen zijn
historische reviews met hun eigen bronversie en scope.

**Aanvulling: breedtes tot 1920 pixels**

De definitieve uitbreiding naar 1920 pixels breed is beoordeeld. Hoogte blijft
afgeleid: maximaal 1080 bij 16:9 en 1440 bij 4:3. GUI en validatie gebruiken
dezelfde grenzen. `src/engine.cpp:235` kiest de HD-markering wanneer breedte
groter is dan 720 of hoogte groter dan 576; `src/mux.hpp:58` gebruikt deze
keuze voor zowel SDT als NIT-servicelijst. De eerder gemelde vaste SD-markering
is daarmee gecorrigeerd. Elke fallbacktekstregel wordt nu afzonderlijk op
zichtbare inktgrenzen gecentreerd in `src/input.hpp:142`.

Bronhashes van die versie:

```text
engine.cpp 76546d2b7703b97355a60e98d0b5c8ed0e7fbbf8f0b10201c4e59a83b1b20ac4
engine.hpp 7ef7ebb9b0c3d674cf555014ff62bb897f43bb78e5f8bee13e1030e646817afe
input.hpp  eadd4895d29007710773816a740a529627e7f1b4a17b4c2fee190dee3330e1cd
main.cpp   63acc0de9938d3589ee87b9c45773eb83ca6fb69cea858e9b375be02db397017
mux.hpp    56e2436642cd125f121e3a5a930689bf5635b4b3a7ed8dc59ba511655ebc8765
```

Definitieve negen captures: `build/checks-1790449052809916817/`.
`build/verification/hd-aspect-final.log` slaagt voor zes fallbackrasters,
tekstcentrering per regel en drie bewegende HD-bronnen. De eerste mislukte
centreertest is bewaard in `hd-aspect.log`; onderstaande beoordeling gebruikt
de gecorrigeerde versie. Onafhankelijke ffprobe-, SI- en PCR-analyse:

| Raster | DAR / SAR | H.264-profiel en level | SDT én NIT service_type |
|---|---|---|---|
| 1280×720 | 16:9 / 1:1 | Main 3.1 | 0x19 |
| 1920×1080 | 16:9 / 1:1 | Main 4.0 | 0x19 |
| 1920×1440 | 4:3 / 1:1 | Main 5.0 | 0x19 |

Dit geldt voor zowel fallback als bewegende bron. De drie SD-regressies
320×240, 320×180 en 720×406 behouden 0x16. Alle negen captures hebben bij
440310 bit/s maximaal 37,573556 ms PCR-interval en 35,008 ns gereconstrueerde
PCR-afwijking. TSDuck op de bewegende 1920×1440-capture: geen continuity-fouten,
106 PCR's zonder overschrijding van 1 µs. De definitieve 1920×1440-screenshot
is onafhankelijk bekeken. De lage beschikbare bitrate blijft zichtbaar in
de beeldkwaliteit; deze tests bewijzen decodeerbaarheid, geen kwaliteitsnorm.

**MPEG-TS:** gerichte controles voldoen. **DVB/ETSI:** service_type-correctie
voldoet aan [EN 300 468 V1.19.1 §6.2.33, tabel 89 en §6.2.35,
tabel 91](https://www.etsi.org/deliver/etsi_en/300400_300499/300468/01.19.01_60/en_300468v011901p.pdf).
Er blijft een expliciete **AFWIJKING** tegenover volledige DVB-AVC-HD:
Main in plaats van High; 1920×1440/4:3 valt bovendien buiten het beschreven
HD-raster/aspectprofiel. Zie [TS 101 154 V2.10.1 §5.7.1.1, §5.7.1.2 en
§5.7.1.4, tabel 11](https://www.etsi.org/deliver/etsi_ts/101100_101199/101154/02.10.01_60/ts_101154v021001p.pdf).
**Projectafspraken:** uitgebreide GTK-grenstest en unit-tests slagen;
de hoofd-agent rapporteert tevens beide bestaande builds en `make test-linux`
geslaagd met 946 JPG's. Bestaande contestproductiecode is ongewijzigd.

Geen blokkade voor het expliciet beperkte DATV-profiel. HD-failover over SRT,
UDP-pacing onder langdurige HD-belasting en volledige decoderbufferconformiteit
zijn niet aanvullend geverifieerd. De overige hieronder beschreven beperkingen
blijven gelden. Oudere aanvullingen beschrijven hun eigen bronversie en scope.

**Aanvulling: breedte, beeldverhouding en tekstcentrering**

Deze eerdere herreview betreft de keuze 4:3/16:9 naast de breedte, automatisch
afgeleide even hoogte, bijbehorende SAR in ontvangst- en uitvoercaps en
centrering met Pango-ink-extents. De hieronder bewaarde failoverreview
beschrijft de voorafgaande versie; deze aanvulling legt de laatste wijzigingen
en aanvullende uitvoermetingen vast. Geen nieuwe blokkade gevonden.

SHA-256 van die versie:

```text
engine.cpp 1698e64b38f5889c6a6dd2bbc0ba5e7877ce38990c61bb07e42042b404c93994
engine.hpp fcf58da6ed5fc44689e1025c4c649c24066ef4d48f83bc43a9eb0c80a3265b4b
input.hpp  b9c2180e9d2b533fb0793cfd5b7a45fa621e5ceded2583d5d52b49c0af711c5e
main.cpp   13c82ef70b6d88907e19fb0eb083b48f7033944243a623ce22520dfc0ec50054
```

Codeplaatsen: `src/engine.hpp:12` berekent de even hoogte en SAR;
`src/engine.cpp:80` migreert oude onafhankelijke hoogte-instellingen;
`src/engine.cpp:171` en `src/input.hpp:94` gebruiken dezelfde raster-/SAR-keuze;
`src/input.hpp:143` centreert de zichtbare tekst. De muxer en UDP-pacing zijn
bij deze wijziging niet aangepast.

Captures: `build/checks-1790448288044899796/`. Alle drie fallbackproeven
slagen voor transport, decodering en DAR. De reviewer heeft de testcode gelezen,
SAR/DAR voor de afgeronde hoogte onafhankelijk met ffprobe gecontroleerd,
de PNG-pixelgrenzen onafhankelijk gemeten en de 720×406-afbeelding bekeken.

| Instelling | Raster | DAR | Afwijking tekstblokcentrum x/y, pixels |
|---|---|---|---|
| 320, 4:3 | 320×240 | 4:3 | 0 / 0 |
| 320, 16:9 | 320×180 | 16:9 | +0,5 / 0 |
| 720, 16:9 | 720×406 | 16:9 | −0,5 / +0,5 |

Voor 720×406 is SAR **406:405**, zodat afronding de bedoelde DAR niet verandert.
De gerichte test controleert ook beide tekstregels afzonderlijk binnen twee
pixels horizontaal. Dit is een projectcriterium, geen MPEG/DVB-normgrens.
Onafhankelijke PCR-meting op alle drie TS-captures: maximaal 37,573556 ms
interval en 35,008 ns bytepositieafwijking. Een extra bewegende native
720×406-bron decodeert; de onafhankelijke TSDuck-controle op `live-720-169.ts`
geeft 106 PCR's zonder overschrijding van 1 µs en geen continuity-fouten.

**MPEG-TS:** de gerichte controles voldoen. **DVB/ETSI:** correcte gemeten
DAR maakt deze rasters nog geen volledig DVB-SDTV-profiel; de hieronder
genoemde AVC/SI/bufferbeperkingen blijven gelden. De toepasselijke
aspectsignalering valt onder TS 101 154 §5.5.3.1; alle VUI-velden zijn niet
opnieuw normatief onderzocht. **Projectafspraken:** GUI- en unit-tests slagen;
de bestaande platformcode blijft ongewijzigd. Nieuwe SRT-failoverproeven voor
elke breedte/verhouding zijn niet uitgevoerd; de eerdere failovermetingen
blijven beperkt tot hun vastgelegde profiel.

Eindadvies: bruikbaar als afzonderlijk Linux-programma binnen het beschreven,
beperkte DATV-profiel. De onderzochte transportcontroles slagen. Dit rapport
verklaart de uitvoer niet volledig DVB-broadcastconform: decoderbuffermodel,
volledige SI/AVC-conformiteit, langdurige klokdrift en fysieke uitgangstiming
zijn niet geverifieerd.

**Beoordeelde wijziging en bewijs**

Beoordeeld zijn de nieuwe `src/engine.cpp`, `src/input.hpp`, `src/mux.hpp`, `src/avc.hpp`,
`src/engine.hpp`, `src/main.cpp`, `src/network.hpp`, Makefile, controles en
README in deze map. De bestaande productiecode onder `../src/` is niet
gewijzigd. De reviewer heeft geen productiecode of testgrenzen aangepast.
Het rapport betreft de werkboom zonder nieuwe commit. SHA-256 bij beoordeling:

```text
engine.cpp da1d299706868248053436cb77edc944dde05b01cb186e521ea3c8d9153da8cf
input.hpp  11ac4209e234a802ea22206930db6a7a8d665aa2342a03cf2562363992cf282f
mux.hpp    cc965936b532ad22263b62e2834f49bfb8da83b972a5fb4b4ed4371c0dde1817
avc.hpp    22efd3b1c33efe31b0495f3de4e12d6ba9619d389eb607f5e54bf250a9edc2aa
```

Definitieve captures: `build/checks-1790445662955881910/`. Oudere captures en
logs bevatten ontwikkelfouten, waaronder de inmiddels herstelde Caller-start;
die zijn geen bewijs van een fout in deze eindversie.
Deze eindserie bevat ook automatische AAC-proeven van 48 kbit/s bij
184313 bit/s TS en 64 kbit/s bij 223576 bit/s TS. De volledige hash- en
logverwijzingen staan in `build/verification/final.json`; de suite-uitvoer in
`build/verification/streambridge-fallback.log`.

Profiel: één onversleutelde service; H.264 Main zonder B-frames; standaard
320×240, 25 fps, GOP 25; AAC-LC 48 kHz stereo, standaard 96 kbit/s;
188-byte CBR-TS, zeven pakketten per onbewerkt unicast IPv4/UDP-datagram.
Getest: 115196 bit/s zonder audio, 440310 en 894305 bit/s met audio,
512 kbit/s AAC bij 894305 bit/s TS, een opname van 25 seconden en beide
SRT-verbindingsrollen, inclusief bronverlies en automatisch herstel gedurende
17 seconden per rol. De lage-bitrateproef gebruikt 5 fps. RTP is niet aanwezig.
Callsign is een afzonderlijk invoerveld, standaard `NOCALL`; het bepaalt zowel
service name als provider name en de tekst onder het blauwe No Signal-beeld.

De herziene architectuur houdt één uitvoerencoder en één transportklok actief.
`LiveInput` decodeert SRT afzonderlijk naar BGRx/PCM, vertaalt elke nieuwe
verbinding naar de lokale tijdlijn en buffert 300 ms. Vaste appsrc-feeders
leveren doorlopend videoframes en blokken van 1024 audiosamples. Ontbrekend
beeld levert blauw met witte tekst en stilte; herverbinden reset alleen de
ontvanger. De reviewer liet een synchronisatierisico corrigeren: uitsluitend
`picture()` verplaatst de videoqueue; `sound()` en `status()` controleren nu
zonder die queue te veranderen. De definitieve bron is hierop herbeoordeeld.

**MPEG-TS — VOLDOET binnen de gemeten scope; volledige basisconformiteit NIET GEVERIFIEERD**

De integratiecontroles onderzoeken synchronisatie, TEI, scrambling,
adaptationvelden, continuïteit, PSI/SI-CRC, PID-consistentie, PCR-velden,
PTS-markerbits en aanlevering vóór de PES-deadline. FFmpeg decodeert de
eindcaptures zonder fouten. Onafhankelijke TSDuck-runs op beide
`fallback-*.ts` rapporteren geen continuity-fouten en elk 453 gecontroleerde PCR's zonder
overschrijding van zijn ingestelde grens van 1 µs; de eerste PCR is niet
vergelijkbaar. Die toolgrens is ruimer dan de norm: daarom zijn PCR's daarnaast
onafhankelijk tegen hun exacte bytepositie in de gereconstrueerde CBR-TS gemeten.

| Capture | TS-bitrate, bit/s | Grootste PCR-interval, ms | Grootste absolute PCR-afwijking, ns |
|---|---:|---:|---:|
| default | 440310 | 37,573556 | 35,008 |
| low | 115196 | 39,168037 | 36,987 |
| high / audio512 | 894305 | 38,680333 | 36,553 |
| audio48 | 184313 | 32,640148 | 36,983 |
| audio64 | 223576 | 33,635111 | 36,829 |
| long | 440310 | 37,573556 | 36,913 |
| srt-listener / srt-caller | 440310 | 37,573556 | 35,008 |
| fallback-listener / fallback-caller | 440310 | 37,573556 | 36,896 |

De toegepaste klokgrenzen zijn maximaal 100 ms tussen PCR's en ±500 ns
nauwkeurigheid in CBR-TS. 40 ms is een streefwaarde, geen actuele harde
DVB-grens. Bron: [ETSI TR 101 290 V1.4.1 (2020-06), §5.2.2 tabel 5.0b,
noot 2 en §5.3.2](https://www.etsi.org/deliver/etsi_tr/101200_101299/101290/01.04.01_60/tr_101290v010401p.pdf).
Dit is een meetrichtlijn; onderliggende systeemeisen verwijzen naar H.222.0.

AAC-PTS-stappen zijn onafhankelijk gemeten: 3840 ticks per PES bij twee
1024-sample access units, 7680 ticks bij vier AU/PES in de 48/64-kbit/s-proeven,
en 1920 ticks bij de 512-kbit/s-proef met één AU/PES.
De unit-test controleert ook PTS-codering over de 33-bit wrap. Een volledige
lopende PCR/PTS-wrap na circa 26,5 uur is niet getest.

**DVB/ETSI — gedeeltelijk VOLDOET; profielafwijkingen en ontbrekende verificatie blijven**

PAT/PMT-consistentie en CRC's slagen. De grootste gemeten PAT/PMT-afstand is
235,008 ms; SDT 822,529 ms. In `long.ts` is NIT maximaal 4006,704 ms en TDT
20002,780 ms. EIT present/following is leeg maar aanwezig. Syntaxreferenties:
[EN 300 468 V1.19.1 (2025-02), §5.2.1, §5.2.3–5.2.5 en §6.2.33](https://www.etsi.org/deliver/etsi_en/300400_300499/300468/01.19.01_60/en_300468v011901p.pdf).
De beoordeling dekt niet alle descriptor- en toepasselijkheidseisen voor een
compleet RF-netwerk. Er is geen versleutelde service of conditional-accesspad.

Eerdere AVC-bevindingen zijn hersteld en opnieuw gemeten: Main in plaats van
Constrained Baseline, verwijdering van encoder-identificatie-SEI en ES-priority
bij de eerste IDR-slice. In alle elf eindcaptures staan AUD en eerste
IDR-slice in hetzelfde TS-pakket; de prioriteitsbit staat op dat pakket.
Referentie: [TS 101 154 V2.10.1 (2026-05), §4.1.5.2, §5.5.2.0 en
§5.6.1.1](https://www.etsi.org/deliver/etsi_ts/101100_101199/101154/02.10.01_60/ts_101154v021001p.pdf).

Open punten, afzonderlijk van de geslaagde pakketcontroles:

- **AFWIJKING bij een algemene DVB-SDTV-claim — middel:** vrije framerates
  en resoluties in `src/engine.cpp:43`, waaronder standaard 320×240,
  volgen niet automatisch het DVB-SDTV-profiel. TS 101 154 §5.6.2.2–5.6.2.3,
  tabellen 7 en 8 definiëren specifiekere combinaties. Dit blijft bewust een
  beperkt DATV-profiel; ontvangercompatibiliteit is geen algemene conformiteit.
- **NIET GEVERIFIEERD voor volledige RF-SI — middel:** de NIT in
  `src/mux.hpp:60` bevat geen RF-delivery-descriptor. EN 300 468 §5.2.1 en
  §6.2.13 beschrijven netwerk- en deliveryinformatie. Zonder de werkelijke
  RF-configuratie en modulatoruitvoer kan volledigheid niet worden vastgesteld.
- **NIET GEVERIFIEERD — belangrijk voor broadcastgebruik:**
  `src/mux.hpp:77` bewaakt PES-deadlines en verschuift AAC-aanlevering dichter
  naar PTS. Dit bewijst geen T-STD/CPB-bufferconformiteit. De interne VBV van
  400 ms en uitgeschakelde HRD in `src/engine.cpp:165` vormen evenmin dat bewijs.
  De volledige H.222.0-buffer- en H.264-HRD-modellen zijn niet gesimuleerd.
  De officiële [H.222.0-editie 08/2023](https://www.itu.int/rec/T-REC-H.222.0-202308-S)
  is geïdentificeerd; ophalen van de volledige PDF mislukte herhaaldelijk.
  Directe volledige normcontrole en H.264-annex-C-verificatie ontbreken.
- **NIET GEVERIFIEERD:** lange looptijd, bronklokdrift, lip-sync met meetbeeld,
  alle resoluties/fps/budgetgrenzen, pakketverlies, slaapstand en alle
  foutinjecties. De raw-audiofeeder gebruikt een vaste 48-kHz-klok; gaten in
  de gemapte bron-PCM worden stilte. De encoderuitvoer krijgt een
  1024-sampleklok vanaf de eerste tijdstempel; afwijkingen boven 100 ms stoppen
  de uitvoer. Deze 100 ms is een applicatiegrens, geen normatieve
  lip-syncgoedkeuring. Alleen videoverlies met doorlopende bron-audio en
  vele herverbindingscycli zijn niet afzonderlijk getest.

**UDP en projectafspraken — VOLDOET binnen loopbackscope**

| SRT-rol van bridge | Datagrammen | Gemeten bit/s | Kleinste/grootste interval, ms |
|---|---:|---:|---:|
| Listener | 210 | 440302,770 | 23,828 / 23,993 |
| Caller | 210 | 440307,004 | 23,798 / 24,047 |

Alle ontvangen datagrammen zijn 1316 bytes; decodeerbaarheid en TS-controles
slagen bij beide rollen. Meting: Linux-loopback, ontvangsttijd uit de monotone
Python-klok. Dit meet geen NIC-hardwaretijd, modulatorbuffer of RF-uitgang.
1316 bytes en vaste PID's zijn projectkeuzes, geen universele MPEG-eisen.

De definitieve invoer gebruikt `tsdemux ignore-pcr=true`: de bron-PES-tijdlijn
blijft basis voor transcoding, terwijl de eigen mux de uitvoer-PCR maakt.
Dit corrigeert de gemeten opstartverschuivingen bij Caller. De eerdere
AAC-sprongen kwamen overeen met GStreamers veranderende PCR-skewmapping;
beide rollen slagen met de uiteindelijke instelling. De bron-PCR wordt dus
niet als nauwkeurigheidsbewijs voor de uitvoer gebruikt.

De aanvullende 17-secondenproeven laten in beide rollen live beeld, bronverlies,
No Signal en automatisch teruggekeerd live beeld zien. De bron stopt rond
seconde 5 en wordt rond seconde 10 opnieuw gestart. De uitvoer blijft CBR:

| Failoverrol | Gemeten bit/s | Grootste UDP-interval, ms | AAC-RMS zonder bron / na herstel |
|---|---:|---:|---:|
| Listener | 440307,548 | 24,040 | 0 / 18493 |
| Caller | 440308,042 | 24,022 | 0 / 18497 |

Onafhankelijke hercontrole bevestigt nul extra discontinuity-indicators na
opstart, één SDT-versie per capture, doorlopende PCR en vaste AAC-PTS-stappen.
Continuity-controles slagen ook over de overgangen. Service name én provider
name zijn `PE1TEST` in de failovertests. De opgeslagen screenshot is
onafhankelijk bekeken: blauw met witte “No Signal” en “PE1TEST”. De
pixel- en geluidsmetingen bevestigen blauwe/stille tussenfase en teruggekeerd
beeld/geluid. Geen encoder- of muxreset is zichtbaar in deze uitvoer.

De nieuwe Linux-executable staat buiten `dist/`; dit volgt de expliciete
opdracht. Beide bestaande distributiebinaries zijn aanwezig en als ELF/PE
gecontroleerd. Volgens de hoofd-agent slaagde `make all test-linux`.
De reviewer heeft de volledige beschikbare TS/UDP-regressielogs voor Linux
en Windows/Wine beoordeeld; die slagen. Wine meldt wel dat echte
ICMP-foutaflevering niet is geverifieerd. Het Windows-foutinjectieprogramma
is gebouwd; een afzonderlijke nieuwe uitvoer daarvan is niet aangetoond.
Dit betreft bestaande contestcode, die niet is gewijzigd.

**Uitgevoerde en beoordeelde controles**

- Onafhankelijk uitgevoerd: `make -C streambridge test-unit`, `ffprobe` op
  de eerdere Caller-capture, TSDuck `continuity` en `pcrverify` op beide
  definitieve failovercaptures, eigen
  Python-analyse van PCR-byteposities, SI-intervallen, AAC-PTS en IDR-markering
  in alle elf definitieve captures, extra discontinuïteiten en SDT-versies;
  visuele controle van het definitieve No Signal-beeld en broncodehercontrole
  na de laatste correctie.
- Beoordeeld: definitieve `checks/integration.py`-uitvoer en bijbehorende
  captures/timing-JSON, beide failoverrollen, stoppen tijdens wachten,
  GTK-callsigntest (`gtk-fallback.log`), `make test-ts`,
  `make test-udp`, Windows-senderbuild en `checks/check_udp.py --windows`.
  Logs staan onder `build/verification/` en captures onder bovengenoemde map.
- Beschikbare analyzers: FFmpeg/ffprobe 7.1.5, TSDuck 3.44-4578,
  GStreamer 1.26.11, Wine 11.0 Staging. FFmpeg is uitsluitend analyzer in
  deze controles; het productiepad gebruikt expliciete native plugins.

Er is binnen deze gecontroleerde scope geen resterende blokkade voor het
beperkte DATV-programma. De genoemde afwijkingen en ontbrekende metingen
moeten opgelost respectievelijk aangevuld worden vóór een volledige
broadcastconformiteitsclaim.
