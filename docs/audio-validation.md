# Linux-audio: onafhankelijke UDP/TS-review

Beoordeeld op 6 oktober 2026: werkboomwijziging tegen
`c2df548ea604072f3634d704c88223717c7c4b7c`, inclusief `src/audio_linux.h`,
gedeelde muxer/configuratie, Linux-bediening, Windows-configuratieovername,
Makefile en gewijzigde tests. Eerdere gebruikerswijzigingen in README en
`streambridge/` vallen buiten deze beoordeling. De reviewer wijzigde geen
productiecode of testgrenzen.

Definitieve onderzochte bronbestanden, SHA-256:

- `src/datv.cpp`: `3ae42a67f723b3cd6d885398be7710032c5a4899571fae1d77ce1b832b02b699`
- `src/audio_linux.h`: `f7a8cd07c77f40d32cfe9eecf7b83c2c3beea281c40820556702ae312a9815cb`

Profiel: één onversleutelde H.264-service, optioneel Linux AAC-LC, 48 kHz,
mono 48 kbit/s of stereo 96 kbit/s; LATM/LOAS, audio-PID 0x0102, PMT
stream_type 0x11, descriptor `7c 01 51`, PES stream_id 0xc0. Bediening alleen
Genius 3, DVB-S/S2, 333/500 ksym/s. Ruwe IPv4-unicast-UDP, zeven TS-pakketten
van 188 bytes per datagram; geen RTP.

## MPEG-TS

**VOLDOET binnen de hieronder gemeten scope; volledige conformiteit NIET
GEVERIFIEERD.** Onafhankelijke parser controleerde alle acht finale captures:
sync/TEI/scrambling, continuity counters, adaptationlengten, PSI/SI-CRC,
PCR-reservedbits/extensie, PES-lengten, PTS-markerbits en stappen, LATM/ASC-
bitvelden, kanaalconfiguratie, payloadlengten en padding. Geen fout gevonden.
De audio-PES is telkens compleet vóór PTS. AAC wordt foutloos gedecodeerd;
alle volledige video-PES eveneens (80/160/80/80/80/140/48/48 beelden).
De laatste onvolledige video-PES is bij deze afzonderlijke videodecode
weggelaten: de capturetest knipt op een complete audio-PES.

| Capture | TS bit/s | Audio | Buffer | AAC-frames | Min. PTS-voorsprong | Max. PCR-interval |
| --- | ---: | --- | ---: | ---: | ---: | ---: |
| 333-mono | 306882 | 48k mono | 1 s | 336 | 55,853 ms | 78,415 ms |
| 333-s2-mono-long | 213395 | 48k mono | 1 s | 1836 | 55,658 ms | 77,528 ms |
| 333-stereo-plain | 306882 | 96k stereo | 1 s | 336 | 69,114 ms | 39,207 ms |
| 333-stereo | 306882 | 96k stereo | 1 s | 335 | 70,556 ms | 78,415 ms |
| 500-mono | 460784 | 48k mono | 1 s | 335 | 83,903 ms | 78,336 ms |
| 500-stereo-buffer10 | 460784 | 96k stereo | 10 s | 194 | 78,328 ms | 78,336 ms |
| auto-ffmix-create | 306882 | 96k stereo | 1 s | 194 | 71,995 ms | 78,415 ms |
| auto-ffmix-reuse | 306882 | 96k stereo | 1 s | 194 | 71,995 ms | 78,415 ms |

Alle gevallen hebben EIT/teletekst behalve `plain`. Beeld 160×120,
GOP 2, 10 fps; lange S2-run 4 fps en 40 s. Dezelfde S2-capaciteit bij
10 fps werd terecht vóór verzending afgewezen wegens onvoldoende beeldruimte.
De overige runs duren 8 s, respectievelijk 14 s bij de 10 s buffer.
Stimulus: stilte, toon en ruis via een tijdelijke Pulse-sink. De twee
automatische sinktests duren elk 5 s en gebruiken een doorgaande lokale toon.

Gereconstrueerde CBR-PCR-fout is kleiner dan één 27MHz-tick. Maximale gemeten
PAT/PMT-repetitie 225,535 ms, SDT 831,660 ms, EIT-secties 521,550 ms;
TDT-repetitie in de lange run 20,03031 s. Grootste audio-PES 656 bytes;
conservatief gemeten audio-bufferpiek 1740 bytes. Dit is geen volledig
T-STD-bufferbewijs.

Meetreferentie: [ETSI TR 101 290 V1.4.1 (2020-06)](https://www.etsi.org/deliver/etsi_tr/101200_101299/101290/01.04.01_60/tr_101290v010401p.pdf),
§5.2.1 tabel 5.0a en §5.2.2 tabel 5.0b: PSI, continuity, PCR, PTS;
PCR-repetitie maximaal 100 ms en nauwkeurigheid ±500 ns. De oude DVB-grens
van 40 ms is verwijderd (noot 2); dit project gebruikt die als streefwaarde.
Bestandsreconstructie bewijst geen fysieke netwerktiming of RF-PCR-nauwkeurigheid.

[ITU-T H.222.0](https://www.itu.int/rec/T-REC-H.222.0) vermeldt 04/2025 als
actuele editie; volledige betaalde tekst is niet verkregen. Systems
§§2.4.3/2.4.4 worden hier via officiële ETSI-verwijzingen getoetst.
De AAC-levelwaarde 0x51 volgt tabel 2-74 van de officieel geïndexeerde
[editie 08/2023](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-H.222.0-202308-S%21%21PDF-E&lang=e&type=items).
Ook volledige ISO/IEC 14496-3-tekst is niet onderzocht; LATM-syntaxcontrole
en decoderacceptatie vervangen geen integrale codecconformiteit.

## DVB/ETSI

**VOLDOET voor onderzochte AAC-signalering.** `src/datv.cpp:174` en
`audio_pes()` signaleren LATM/LOAS en herhalen configuratie per frame.
[TS 101 154 V2.9.1 (2025-05)](https://www.etsi.org/deliver/etsi_ts/101100_101199/101154/02.09.01_60/ts_101154v020901p.pdf),
§4.1.6.1, §6.4.1, §6.4.2.1 en §6.5 beschrijven streamsignalering,
LATM, AAC-profiel/level en instappen.
[EN 300 468 V1.19.1 (2025-02)](https://www.etsi.org/deliver/etsi_en/300400_300499/300468/01.19.01_60/en_300468v011901p.pdf),
normatieve annex H, vereist de aanwezige AAC-descriptor.

**AFWIJKING/beperking tijdens opstart:** `src/datv.cpp:412` stelt audio uit
tot bufferduur minus 100 ms. Bij 10 s buffer is audio-PID ongeveer 9,9 s
afwezig hoewel de PMT het aankondigt. De aanbevolen PID_error-meetperiode
van maximaal 5 s uit TR 101 290 §5.2.1 kan dan alarm geven. Korte
decoderprobes kunnen het audiokanaal missen. Dit is gedocumenteerd in
`docs/datv.md`; de 5 s-meetinstelling is geen universele MPEG-TS-norm.

**AFWIJKING ten opzichte van volledig DVB-SI-profiel, bestaand:** NIT
ontbreekt; zonder EIT-optie ontbreken ook EIT/TDT.
[TS 101 211 V1.13.1 (2021-05)](https://www.etsi.org/deliver/etsi_ts/101200_101299/101211/01.13.01_60/ts_101211v011301p.pdf),
§4.1.1, §4.1.4.1 en §4.1.5 beschrijven de relevante SI-verplichtingen;
TOT is optioneel (§4.1.6). CAT is **NIET VAN TOEPASSING** op de
scramblingvoorwaarde uit TR 101 290 tabel 5.0b voor deze onversleutelde service.
Het bestaande kleine AVC-beeld met 4/10 fps is geen aangetoond volledig
DVB-AVC-profiel (TS 101 154 §5.6); volledige SPS/VUI/HRD-toets is
**NIET GEVERIFIEERD**.

## Projectafspraken, tests en correcties

**VOLDOET binnen gecontroleerde scope.** Beide `dist/atv-contestnummer`
en `dist/atv-contestnummer.exe` zijn gebouwd. Audio is Linux-only zoals
gevraagd; Windows schakelt audio uit bij overname van Linux-instellingen.
Gedeelde toelatingsregels staan in `src/core.c`. De hoofd-agent rapporteert
geslaagde Linux-/Windows-configuratiebediening en timeline-tests, inclusief
audio en 10 s buffer.

Beoordeelde uitvoer: `make all test-linux`, `make test-ts`, `make test-udp`,
bouw Windows-sender/foutinjectietest, `python3 checks/check_udp.py --windows`,
`python3 checks/check_audio.py` (de test achter `make test-audio`) en de gerichte `audio-plain`-capture. Regressielogs staan in
`/tmp/atv-audio-{finalbuild,regression,wine-udp,integration-final,plain-final}.log`.
Captures, TSDuck-uitvoer en onafhankelijke meetresultaten staan onder
`build/audio-check/`; `independent-review.jsonl` bevat de tabelmetingen.
Onafhankelijke parser: `/tmp/audio_review.py`.

FFmpeg/ffprobe 7.1.5, TSDuck 3.44-4578 en Wine 11.0 Staging waren beschikbaar.
UDP-tests controleren 1316-byte datagrams, pacing, pakketverlies en snelle stop.
De finale audio-suite bevestigt tijdige fouten bij ontbrekende bron,
onvoldoende capaciteit en bronverlies. Alleen localhost werd gebruikt.

Tijdens review gevonden en gecorrigeerd: onverwachte Pulse-bronfallback
(`LinuxAudio::poll`, nu exacte apparaatnaamcontrole); te vroege AAC-priming
bij lange videobuffer (`LinuxAudio::next`, nu vlak vóór eerste audio);
ontbrekende headerafhankelijkheden in Linux-testtargets. Definitieve code en
captures zijn na deze correcties opnieuw beoordeeld.

## Aanvullende review: automatische ffmix-sink

**VOLDOET voor gemeten aanmaak en hergebruik.** `LinuxAudio::ensure_source`
zoekt de gevraagde bron eerst asynchroon op. Alleen een ontbrekende
`ffmix.monitor` met `PA_ERR_NOENTITY` activeert één aanvraag voor
`module-null-sink`, naam `ffmix`, 48 kHz, twee kanalen. Een bestaande bron
wordt hergebruikt; een ontbrekende aangepaste bron blijft een fout.
Na moduleload volgt opnieuw bronlookup en daarna exacte apparaatnaamcontrole.
De bestaande vijfsecondenlimiet begrenst het wachten. Productiecode gebruikt
geen shell of subprocess voor deze handelingen.

`python3 checks/check_audio_sink.py` is geslaagd: `ffmix` was vooraf afwezig,
de toepassing maakte één bijbehorende module/sink aan, Stop liet deze bestaan,
dezelfde speler bleef tussen twee streams actief en herstart behield dezelfde
sink-/module-ID. Standaardbron en standaarduitgang bleven gelijk. De test
verwijdert geen bestaande gebruikerssink en laat de aangemaakte sink bestaan.
De twee captures zijn onafhankelijk opnieuw geparseerd en gedecodeerd.
De volledige gewone audiomatrix inclusief foutpaden slaagt ook na deze
lookupwijziging. Logs:
`/tmp/atv-audio-autosink-{test,regression,build,finalbuild}.log`.
Beide distributiebinaries zijn na de wijziging beschikbaar.

De destructor annuleert en ontkoppelt operationhandles vóór context/mainloop
worden vernietigd. Officiële lokale PulseAudio-headers
`/usr/include/pulse/operation.h` en `introspect.h` beschrijven deze API's;
online Doxygen was tijdens de review niet bereikbaar. De annulering voorkomt
callbacks naar het vernietigde object, maar garandeert geen annulering van
server-side moduleload. Stop tijdens aanmaak kan dus nog een blijvende sink
opleveren, passend bij de gekozen levensduur.

**NIET GEVERIFIEERD:** gerichte Stop precies tijdens moduleload, geweigerde
moduleload, native PulseAudio naast de gebruikte audiosessie en gelijktijdige
aanmaak door meerdere processen. Lookup plus load is niet atomisch; de
herlookup garandeert de gebruikte bronnaam, maar voorkomt niet aantoonbaar
iedere mogelijke extra sink bij concurrerende aanmaak. Geen nieuwe afwijking
in de ongewijzigde MPEG-TS-muxer aangetroffen; bovenstaande DVB-beperkingen
blijven gelden.

**NIET GEVERIFIEERD:** native Windows-netwerkgedrag (Wine levert geen echte
ICMP-poortweigeringen; afzonderlijke foutinjectie slaagt), fysieke ontvanger/RF,
alle FEC/fps/resolutiecombinaties, 33-bits-wraparound, urenlange captureklokdrift,
geïnjecteerde audio-overflow/underrun en volledige T-STD/HRD. De 40 s-run
bewijst geen onbeperkte driftcorrectie.

Advies: akkoord voor deze Linux-audiotoevoeging binnen het beperkte beschreven
DATV-profiel. Geen openstaande blokkerende regressie gevonden. Geen claim van
volledige MPEG-TS- of DVB/ETSI-conformiteit buiten de gemeten scope.
