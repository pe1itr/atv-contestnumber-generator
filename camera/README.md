# DATV Camera (Linux)

Apart experimenteerprogramma voor een USB-webcam of interne V4L2-camera op deze
Linux-computer. Alleen video: de roepnaam wordt vóór compressie in het beeld
gezet op een vaste zwarte balk met grote letters. De uitvoer is H.264 Main
zonder B-frames in een CBR MPEG-TS, als unicast
IPv4/UDP met zeven TS-pakketten per datagram (1316 bytes).

Start vanuit de repository:

```sh
make -C camera
./dist/datv-camera
```

De contestnummergenerator en Streambridge blijven aparte programma's.
`make all test-linux` in de hoofdmap bouwt/test de bestaande generator voor
Linux en Windows; het nieuwe cameraprogramma is op verzoek alleen Linux.

## Eerste proef

1. Sluit de camera aan, open het programma en kies `/dev/video0` of de juiste
   andere camera. De tooltip toont de cameranaam. De lijst wordt bij het openen
   opgebouwd; een pad kan ook handmatig worden ingevuld.
2. Vul je roepnaam in (maximaal 16 hoofdletters/cijfers, `/` en `-` toegestaan).
3. Kies DVB-S/S2 en de symbolrate, of DVB-T en de bandbreedte, daarna FEC,
   pilots en guard interval. Ongeldige FEC-keuzes worden automatisch gefilterd.
4. Begin met **Balans** en **Lokale preview**. Dit gebruikt de volledige
   encoder én TS-planner, maar verzendt geen UDP. Het venster toont het werkelijk
   gedecodeerde H.264-beeld, vergroot zonder extra verscherping.
5. Stop, vergelijk **Detail** en **Beweging**. Gebruik eventueel **Lokale
   TS-opname** om proeven te bewaren. Een bestaand bestand wordt niet overschreven.
6. Vul het IPv4-adres en de UDP-poort van de zender in en klik **Start UDP naar
   zender**. Stel de modulator zelf op exact dezelfde DVB-parameters in.

De preview toont compressie, **niet** de ontvangen uitzending, TS-buffervertraging
of eventuele pakketverliezen. Bij Stoppen kan het laatste PES onvolledig zijn;
voor een analyseerbare afgeronde opname gebruik je `--seconds`.

## Detail en beweging bij weinig bits

| Startprofiel | Raster bij 4:3 | Beelden/s | Sleutelbeeldinterval |
|---|---|---:|---:|
| Detail | 320 × 240 | 2 | 5 s |
| Balans | 240 × 180 | 4 | 5 s |
| Beweging | 160 × 120 | 6 | 5 s |

Bij 16:9 volgt de hoogte automatisch. Dit zijn startpunten voor vergelijking,
geen beloofde optimuminstellingen. Buitenbeeld met gras, bomen en bewegende
bladeren is aanzienlijk moeilijker dan een stilstaande achtergrond met één
zwaaiende persoon. Zet de camera vast en gebruik voldoende licht: ruis en
camerabeweging kosten veel bits. De lichte ruimtelijke ruisfilter staat standaard
aan; sterker filteren maakt het beeld zachter. Zwart-wit is een extra experiment.

Instelbaar: even breedte 160–640, 2–12 beelden/s, sleutelbeeldinterval van 1–15
seconden, kleur/zwart-wit, ruisfilter en videobudget. Een lager fps kan meer detail
per beeld overhouden. Een langer sleutelbeeldinterval bespaart bits, maar laat een
nieuwe ontvanger langer wachten en maakt herstel na pakketverlies trager.
De bitrate van de transportstream blijft altijd de berekende DVB-capaciteit.

Het automatische videobudget reserveert 15 kbit/s voor PSI/SI, 900 bit/s per
beeld/s voor PES/afronding en vervolgens 25% marge. PCR kan een videopakket delen.
Dit is een conservatieve startschatting, geen garantie voor elke scène. De
x264-encoder gebruikt een 600-ms VBV, medium preset, één thread, CABAC/Main,
geen B-frames en geen extra sleutelbeelden bij scèneovergangen. De mux controleert
voor ieder pakket de decodeerdeadline; bij overschrijding stopt de uitvoer met
uitleg, in plaats van ongemerkt beschadigde videoframes weg te gooien.

De transportbuffer geeft 2 s decodeervoorsprong. Daar komen circa 0,75 s
voorbuffer, camera-, encoder- en ontvangervertraging bij. Bij de lage bitrates
komt één UDP-datagram ongeveer iedere 0,35 s; dat bevat zeven TS-pakketten die
de zender op het juiste tempo moet uitspelen. De preview heeft deze vertraging niet.

## DVB-keuzes

De berekening en keuzelijsten komen rechtstreeks uit `../src/core.c`, net als
bij `atv-contestnummer`. Zie [de bitrate-uitleg](../docs/dvb-bitrate.md).

| Instelling | TS bit/s |
|---|---:|
| DVB-S, 33 ksym/s, FEC 1/2 | 30411 |
| DVB-S, 35 ksym/s, FEC 1/2 | 32254 |
| DVB-S2, 33 ksym/s, FEC 1/2, pilots aan, normale frames | 31855 |
| DVB-S2, 35 ksym/s, FEC 1/2, pilots uit, normale frames | 34610 |
| DVB-T, BW 35 kHz, QPSK 2K, FEC 2/3, GI 1/8 | 32254 |
| DVB-T, BW 40 kHz, QPSK 2K, FEC 1/2, GI 1/32 | 30160 |

Bij 35-kHz DVB-T past FEC 1/2 niet boven de transportgrens van 30080 bit/s.
40 kHz met FEC 1/2 past alleen met GI 1/32. S2 gebruikt normale frames; het
programma configureert geen modulator en genereert geen RF/IQ.

## Opslag en commandoregel

Instellingen worden bij starten opgeslagen in `$XDG_CONFIG_HOME/datv-camera.ini`
(of `~/.config/datv-camera.ini`). Ze staan los van de andere programma's.
UDP start nooit automatisch bij het openen van het venster. Testbron,
uitvoermodus, opnamepad en testduur worden niet als autostart bewaard.

```sh
./dist/datv-camera --list-cameras
./dist/datv-camera --probe
./dist/datv-camera --write-defaults /tmp/camera.ini
./dist/datv-camera --headless --config /tmp/camera.ini --test-source \
  --pattern 1 --preset 1 --callsign PE1ITR --seconds 15 --capture /tmp/camera-proef.ts
./dist/datv-camera --headless --device /dev/video0 --callsign PE1ITR \
  --ip 192.168.1.50 --port 10000
```

`--preset 0/1/2` kiest detail/balans/beweging. `--pattern 0/1/2` kiest bij
`--test-source` een bewegende bal, SMPTE/detail met beweging, of zware beeldruis.
`--preview` onderdrukt UDP. `--capture` vervangt UDP door een lokale opname;
`--seconds` rondt lopende PES-pakketten af. SIGINT/SIGTERM stopt headless.
Zonder `--headless` opent het venster; `--test-source` kan daarbij alvast de
virtuele beeldbron selecteren.

## Afhankelijkheden en bouwen

C++17, GTK 3, Pango/Cairo en GStreamer 1.x met de ontwikkelpakketten
`gstreamer-app-1.0` en `gstreamer-video-1.0`. Runtimeplugins: `v4l2src`,
`decodebin`, `videoconvert`, `videoscale`, `videorate`, `videotestsrc`, `x264enc`,
`h264parse`, `avdec_h264`, `appsrc`, `appsink`. Er wordt geen extern FFmpeg-proces
gebruikt om te streamen. FFmpeg/ffprobe/TSDuck dienen alleen voor tests.

Op deze computer gebruikt de Makefile zo nodig de bestaande lokale GStreamer-SDK
in `../streambridge/build/sdk`. Met normale systeemontwikkelpakketten werkt
`make -C camera SDK_ROOT=/nonexistent` ook. De binary gebruikt geïnstalleerde
Linux-bibliotheken en plugins, en is geen statisch portable pakket. x264 en de
bijbehorende GStreamer-plugin hebben GPL-licentievoorwaarden.

Bronnen: [GStreamer x264enc](https://gstreamer.freedesktop.org/documentation/x264/index.html),
[V4L2 camera-invoer](https://gstreamer.freedesktop.org/documentation/video4linux2/v4l2src.html).

## Transportprofiel en verificatie

De bestaande Streambridge-muxer wordt hergebruikt met een expliciete,
video-only camera-optie. De standaard voor Streambridge blijft ongewijzigd.
PID's: PAT 0, NIT 16, SDT 17, EIT 18, TDT 20, AVC/PCR 256, PMT 4096,
null 8191. De roepnaam staat zowel in beeld als in SDT service/provider.
Geen AAC, teletekst, encryptie of RTSP/SRT-ingang.

Voor de lage bitrate gebruikt het cameraprofiel andere tabelherhaaldoelen:
PAT/PMT 300 ms met voorrang tegen verdringing, SDT 1 s, NIT 5 s, lege EIT p/f
ieder 1,2 s en TDT 20 s. Packetplanning kan die intervallen verlengen; tests
meten de werkelijke intervallen en beide EIT-secties afzonderlijk. PCR blijft
maximaal 100 ms; 40 ms is bij circa 30 kbit/s fysiek niet altijd haalbaar.

Dit is een **experimenteel DATV-profiel**, geen volledige DVB-conformiteitsclaim.
De kleine rasters en lage fps vallen buiten het gewone DVB-AVC SDTV-profiel;
PAT/PMT voldoen niet aan de 100-ms distribution-eis. RF-delivery-descriptors
moeten zo nodig door de modulator worden geleverd. Zelfgekozen sleutelbeeldintervallen
boven 5 s overschrijden bovendien de DVB-AVC RAP-grens van 5 s; de drie presets
gebruiken 5 s. Fysieke RF-timing en het
volledige T-STD/AVC-buffer-decoder-model zijn niet bewezen door een loopbacktest.
Zie [de onafhankelijke review](review.md) voor resultaten en beperkingen.

```sh
make -C camera test
make -C camera test-ui   # vereist een grafische sessie
make all test-linux
```

De cameratest encodeert alle drie presets op de zes RF-instellingen hierboven,
een langere ruisproef, 12 fps bij de laagste bitrate en 640 × 360 zwart-wit
bij 894305 bit/s, lokale UDP-pacing/herstart, preview zonder UDP en foutpaden.
Captures, FFprobe-/TSDuck-analyses en voorbeeldframes staan onder
`camera/build/checks-*`. De GUI-test controleert presets, FEC-filtering, preview,
instellingen opslaan, stoppen/herstarten en 16:9; screenshot: `camera/build/ui-preview.png`.
Een echte camera en de uiteindelijke zender/ontvanger moeten ter plaatse worden getest.
