# DATV Streambridge — Linux

Een zelfstandig programma naast de contestnummergenerator. Eigen venster,
proces, instellingen en executable; geen onderdeel van `dist/` of de bestaande
Windows-distributie. Het leest dezelfde DVB-keuzelijsten en capaciteitsberekening
uit `../src/core.c`, maar gebruikt eigen instellingen en een eigen live muxer.

Start vanuit de repository:

```sh
streambridge/bin/datv-streambridge
```

## Larix via RTSP aansluiten

1. Verbind telefoon en laptop met hetzelfde wifi-/hotspotnetwerk.
2. Kies **Ingang → RTSP — ontvangst van Larix**. Laat het luisteradres op
   `rtsp://0.0.0.0:8554/live` staan en klik op **Opslaan en starten**.
3. Kopieer het getoonde adres met het **laptop-IP** naar een nieuwe Larix-verbinding,
   bijvoorbeeld `rtsp://192.168.1.100:8554/live`. Gebruik geen `0.0.0.0` op de telefoon.
4. Kies in Larix **RTSP via TCP**, **H.264/AVC (8-bit 4:2:0)** en **AAC**.
   Start de uitzending. De status wordt **RTSP-beeld actief**.
5. De DVB-, resolutie-, bitrate- en callsigninstellingen hieronder gelden ook voor RTSP.

Dit is een ingebouwde RTSP **publish/RECORD-ontvanger**; er is geen losse mediaserver
nodig. Ondersteund: één H.264-videotrack en optioneel één AAC-track (MPEG4-GENERIC
of MP4A-LATM), in beide trackvolgordes. De eerste actieve publicatie behoudt de ingang;
een tweede wordt geweigerd. Audio die niet wordt meegestuurd levert stilte op als
uitvoeraudio aanstaat. Bij uitvoeraudio uit wordt inkomende AAC genegeerd.
RTSP- en SRT-adressen worden afzonderlijk bewaard bij wisselen van ingang.

RTSP gebruikt hier TCP, standaard poort 8554, met pad `/live`; poort en pad zijn
instelbaar. De firewall moet deze TCP-poort toelaten. Deze eerste versie is voor
het lokale netwerk, zonder RTSP-login of TLS. Het is geen RTSP-client voor het
ophalen van een IP-camera en biedt geen PLAY-uitgang aan kijkers.
Bij uitval blijft de zenderstream lopen met het blauwe scherm en stilte;
na opnieuw publiceren herstelt beeld/geluid automatisch. Een verbonden maar
beeldloze publicatie wordt na 10 seconden vrijgegeven zodat opnieuw verbinden kan.

## Telefoon via SRT aansluiten

1. Verbind telefoon en laptop met hetzelfde wifi-/hotspotnetwerk.
2. Laat de laptop luisteren op `srt://:4001?mode=listener`.
3. Het venster toont de lokale IPv4-adressen met interfacenaam en kopieerbare
   `srt://LAPTOP-IP:4001?mode=caller`-adressen. Kies het adres van dat netwerk.
   De lijst ververst elke vijf seconden. VPN-/ethernetadressen kunnen ook zichtbaar zijn.
4. Zet de telefoon op SRT **Caller**, MPEG-TS met **H.264 8-bit 4:2:0 en AAC**.
   De standaard SRT-latency is 200 ms, aanpasbaar. Indien een lokale firewall
   de ontvangst blokkeert, moet de gekozen UDP-luisterpoort worden toegestaan.
5. Vul je **callsign** in (standaard staat er `NOCALL`). Deze wordt zowel
   service name als provider name en verschijnt onder “No Signal” op het blauwe scherm.
   Vul het IPv4-adres en de UDP-invoerpoort van de DATV-zender in. De standaard
   uitvoer is loopback `127.0.0.1:10000`; kies zelf je zenderadres.
6. Kies DVB-S/S2/T, symbolrate of bandbreedte, FEC en toepasselijke pilots/GI.
   Stel **dezelfde parameters ook op de modulator** in. Deze app bestuurt geen RF-hardware.
7. Kies een even breedte van 160 tot en met **1920 pixels** en beeldverhouding **4:3** of **16:9**, fps en GOP.
   De hoogte wordt automatisch op een even aantal beeldlijnen berekend;
   de actuele resolutie staat onder de instellingen. Pixelverhouding in de
   stream behoudt exact 4:3 of 16:9 bij afronding. Klik op **Opslaan en starten**.

4001 is de gekozen standaard voor deze app, geen verplichte SRT-poort.
Bij gebruik van een andere poort volgen de getoonde telefoonadressen die wijziging.
De laptop kan ook als Caller werken, bijvoorbeeld
`srt://192.168.1.50:4001?mode=caller`; dan moet de bron Listener zijn.

## Uitvoer bepaalt het budget

De volledige 188-byte TS-capaciteit wordt uit het DVB-profiel berekend. De
invoerbitrate wordt niet blind doorgestuurd: beeld wordt gedecodeerd, geschaald,
naar de uitvoerframerate omgezet en opnieuw gecodeerd; geluid wordt gedecodeerd,
naar 48 kHz stereo omgezet en opnieuw gecodeerd naar AAC-LC.

- Audio `-1`: automatisch **96 kbit/s** vanaf 300 kbit/s TS-capaciteit,
  **64 kbit/s** vanaf 220 kbit/s, anders **48 kbit/s**.
- Audio `0`: uit. Handmatig audio: **48–512 kbit/s**; lager dan 48 wordt geweigerd.
- Video `0`: automatisch de resterende ruimte; anders eigen kbit/s-keuze.
- Eerst gaan audio, audio-transportoverhead en 60 kbit/s conservatieve reserve
  voor PCR/PSI/SI van de TS-capaciteit af. Van de rest is maximaal 85% voor de
  video-encoder beschikbaar. De resterende marge dekt onder andere PES/TS-overhead.
- AAC wordt per 1, 2 of 4 access units verpakt (afhankelijk van de bitrate).
  De budgetberekening houdt rekening met headers en maximale pakketvulling.
- Onvoldoende ruimte voor minimaal 10 kbit/s video geeft een fout vóór starten.
  Daardoor zijn niet alle lage RF-profielen bruikbaar voor live beeld én geluid.
- Een hoge resolutie bij een lage videobitrate levert weinig detail. Kies zelf
  een passende uitvoerresolutie; de app verandert die keuze niet stilzwijgend.

Ondersteunde RF-keuzes: QPSK DVB-S, QPSK DVB-S2 met normale FECFRAME/CCM en
pilots aan/uit, DVB-T QPSK 2K niet-hiërarchisch. Zie
[de gedeelde berekeningen](../docs/dvb-bitrate.md). Geen 8PSK, short frames of
handmatige RF-frequentieregeling.

Instellingen staan apart in `$XDG_CONFIG_HOME/datv-streambridge.ini` (standaard
`~/.config/datv-streambridge.ini`). Opslaan gebeurt atomair met bestandsmodus
0600 voor nieuwe bestanden. SRT-URL-opties, waaronder eventuele geheimen, staan
in dat eigen configuratiebestand. Er worden geen contestinstellingen gelezen.

## Transport en grenzen van de verificatie

De applicatie gebruikt **geen FFmpeg/libav-muxer, -encoder of -decoder**.
GStreamer levert SRT, de RTSP RECORD-server, RTP-depayloading, demuxing, OpenH264-decodering en x264/FDK-AAC.
De decoders zijn expliciet gekozen; er is geen automatische libav-selectie. De eerste ondersteunde en geteste invoer
is één H.264/AAC-service in MPEG-TS over SRT, of H.264 met optioneel AAC via RTSP/TCP-publicatie. Andere codecs vereisen een aanvullende invoeradapter en worden voorlopig geweigerd.
Ontbrekende decoders geven een fout, zonder FFmpeg-terugval.

De eigen muxer produceert 188-byte TS, zeven pakketten per UDP-datagram,
vaste totale bitrate en null stuffing. PCR wordt afgeleid van de exacte
bytepositie in de CBR-TS; niet van aankomstjitter van de SRT-bron. De invoerdemux
negeert de PCR voor zijn klokcorrecties (`ignore-pcr=true`) en behoudt de
PES-tijdlijn. Zo verschuift GStreamers opstartcorrectie de audio-/videotijden
niet ten opzichte van onze eigen klok. De UDP-thread
gebruikt absolute monotone deadlines. Een gemiste datagramperiode (minimaal
5 ms tolerantie) stopt de uitvoer; er is geen onbeperkte inhaalburst.

Video: H.264 Main, geen B-frames, gesloten GOP, SPS/PPS bij IDR, interne VBV
400 ms. Encoder-identificatie-SEI wordt verwijderd om de eerste slice dicht
bij de AU-start te houden; andere SEI blijft behouden. ES-priority wordt
bij de eerste IDR-slice gemarkeerd. HRD-signalering staat expliciet uit:
de relatie tussen HRD-removal-timing en de nieuwe transportklok is niet bewezen.
De uitvoerencoder heeft een eigen vaste klok. Een aparte ontvangpijplijn levert
ruwe videoframes en 48 kHz PCM-geluid met 300 ms invoerbuffer. Bij elke nieuwe
verbinding worden de brontijden op de doorlopende lokale tijdlijn geplaatst.
Dezelfde H.264- en AAC-encoder blijven actief tijdens omschakelen; de muxer,
PCR, PTS en continuity counters worden niet opnieuw gestart.
Video heeft één seconde transport-planningsruimte; AAC wordt dichter bij zijn
PTS verstuurd om langdurige audiobuffering te voorkomen. Dit is **geen volledige
T-STD/HRD-bufferverificatie**, noch een meting van lip-sync op echte zendapparatuur.

### No Signal en automatisch herstel

Ook voordat een telefoon verbinding maakt, is de TS-uitvoer actief. Bij een
verbroken SRT-/RTSP-verbinding of langer dan één seconde zonder vers videoframe verschijnt
een **blauw scherm met witte “No Signal” en je callsign**. Ingeschakelde AAC-audio
blijft aanwezig met stilte; de ingestelde TS-bitrate en transportklok lopen door.
De ontvanger ziet de omschakeling na de normale encoder-/transportvertraging.

De ontvangpijplijn wordt bij een verbindingsfout opnieuw opgebouwd en probeert
opnieuw te verbinden/luisteren. Zodra er weer gedecodeerde bronframes beschikbaar
zijn, wordt automatisch teruggeschakeld. Ontbrekende audiostukken worden met
stilte gevuld. De bron hoeft niet dezelfde tijdstempelbasis te gebruiken als vóór
de onderbreking. Er wordt geen extern proces of zender herstart.

Ernstige **uitvoerfouten** (bijvoorbeeld encoder-overbelasting, overschreden
transportdeadline, mislukte UDP-send of een te late UDP-thread) stoppen nog wel
met een foutmelding. Een blauw scherm kan een defecte uitvoerverbinding niet oplossen.

PAT/PMT/SDT/NIT, lege EIT present/following en TDT worden gegenereerd.
Service/TS/original-network/network-ID zijn voorlopig 1; video/PCR PID 256,
AAC PID 257, PMT PID 4096. CAT/versleuteling, teletekst en overname van een
invoer-EPG worden niet aangeboden. De NIT bevat een servicelijst, maar geen
RF-delivery-descriptor: RF-frequentiegegevens zijn niet bekend in deze app.

Dit is een **beperkt DATV-serviceprofiel**, geen certificering van volledige
DVB-broadcastconformiteit. Vrij gekozen lage resoluties/framerates, waaronder
de standaard 320×240, vallen niet automatisch onder de DVB-SDTV-profielen.
Volledige SI/AVC-, decoderbuffer-, langdurige klokdrift- en fysieke zendercontrole
blijven nodig voor een broadcastclaim. Loopbackmetingen bewijzen geen timing
na de netwerkkaart, modulator of RF-uitgang.

Instellingen gelden als snapshot vanaf Start. Wijzig je callsign of zendprofiel,
stop dan de uitvoer en start opnieuw. SRT-/RTSP-uitval vereist geen handmatige herstart.

## Bouwen en afhankelijkheden

```sh
make -C streambridge
streambridge/bin/datv-streambridge --probe
```

Benodigd: C++17-compiler, GTK 3/GLib-ontwikkeling, GStreamer **1.26 of nieuwer**
en gstreamer-app-, gstreamer-sdp-, gstreamer-rtsp- en **gst-rtsp-server**-ontwikkeling. Runtime-elementen: `srtsrc`, `tsdemux`,
`openh264dec`, `h264parse`, `x264enc`, `fdkaacdec`,
`fdkaacenc`, `aacparse`, conversie-/resample-/scale-/rate-elementen, `appsrc` en `appsink`.
RTSP vereist tevens `rtpbin`, `rtph264depay`, `rtpmp4gdepay` en `rtpmp4adepay`.
Cairo/Pango tekent het blauwe scherm met de callsign.
De Linux-executable gebruikt systeembibliotheken; dit is geen statische,
distributie-onafhankelijke binary.

Op deze Fedora-werkplek zijn de ontbrekende headers lokaal onder `build/sdk`
beschikbaar gemaakt. De Makefile kan die optionele SDK gebruiken. De ontbrekende
Fedora-SRT-plugin is meegeplaatst onder `bin/plugins/libgstsrt.so`; de executable
zoekt ook daar. De RTSP-serverbibliotheek staat lokaal in `bin/lib/`
(met LGPL-licentie), gevonden via de executable-RUNPATH. Houd ook die map
bij de executable, of installeer Fedora `gstreamer1-rtsp-server`
(ontwikkeling: `gstreamer1-rtsp-server-devel`; Debian/Ubuntu: `libgstrtspserver-1.0-dev`).
Houd de pluginmap bij de executable zolang de systeemplugin ontbreekt.
De bijbehorende LGPL-tekst staat ernaast. Deze lokale plugin vereist een passende
GStreamer- en libsrt-installatie. Een normale systeeminstallatie van de benodigde
pakketten heeft deze lokale aanvullingen niet nodig. Fedora levert de SRT-plugin
in `gstreamer1-plugins-bad-free-extras`; x264 en FDK-AAC kunnen aanvullende
pakketbronnen vereisen. `--probe` meldt ontbrekende componenten.

De GStreamer-SRT-plugin is LGPL; x264 is GPL en FDK-AAC heeft eigen licentievoorwaarden.
Zie de upstreamvoorwaarden bij verdere verspreiding. De in `bin/` meegeleverde
Fedora-plugin correspondeert met GStreamer 1.26.11; upstreambron:
https://gstreamer.freedesktop.org/src/gst-plugins-bad/gst-plugins-bad-1.26.11.tar.xz.

De bestaande hoofd-`make` blijft uitsluitend de Linux- en Windows-versies van
de contestnummergenerator bouwen. Het nieuwe programma heeft voorlopig alleen
Linux-uitvoer, overeenkomstig de opdracht.

## Commandoregel en tests

```sh
streambridge/bin/datv-streambridge --write-defaults /tmp/bridge.ini
streambridge/bin/datv-streambridge --headless --config /tmp/bridge.ini
# Lokale testopname; --capture vervangt UDP en overschrijft geen bestand:
streambridge/bin/datv-streambridge --headless --test-source --seconds 25 --capture /tmp/bridge.ts
make -C streambridge test
```

SIGINT/SIGTERM stopt de headless-versie. De testbron maakt bewegend beeld en een
toon. Een opname met `--seconds` rondt lopende PES-pakketten af. Een noodstop mag
een lopende PES afbreken. `--config` gebruikt interne DVB-keuze-ID's; genereer
bij voorkeur de configuratie met het venster.

De integratietest controleert native AV-encoding, lage/hoge TS-bitrate,
SRT Caller én Listener naar UDP-loopback, continuïteit, CRC, tabellen,
bytepositie-PCR, PTS/deadlines, werkelijke A/V-decodering, gemeten UDP-bitrate en
stoppen tijdens wachten, bronuitval/herstel zonder TS-reset, beide callsignvelden,
blauwe fallbackframes en stille/teruggekeerde audio. FFmpeg/ffprobe en TSDuck zijn uitsluitend testanalyzers.
Captures en meetrapporten blijven onder `build/checks-*`. Zie
[review.md](review.md) voor de aantoonbare scope en resterende beperkingen.

Bronnen: [GStreamer SRT](https://gstreamer.freedesktop.org/documentation/srt/srtsrc.html),
[x264enc](https://gstreamer.freedesktop.org/documentation/x264/index.html),
[SRT Caller/Listener-adressen](https://github.com/Haivision/srt/blob/master/docs/apps/srt-live-transmit.md).

De configuratie bewaart `width` en `aspect` (43 of 169). Oude bestanden met
`height` worden ingelezen met de dichtstbijzijnde ondersteunde beeldverhouding.
Op het blauwe scherm staan “No Signal” en de callsign elk horizontaal
gecentreerd; het zichtbare tekstblok staat als geheel verticaal in het midden.

Voorbeelden: 1280 × 720 en 1920 × 1080 bij 16:9; 1920 × 1440 bij 4:3.
Een grotere resolutie verhoogt het ingestelde TS-bitratebudget niet automatisch.

SDT en NIT melden AVC HD (0x19) voor rasters breder dan 720 of hoger dan
576 pixels; kleinere rasters melden AVC SD (0x16). Dit is servicemarkering,
geen volledige DVB-HD-conformiteitsclaim: de encoder gebruikt Main en
1920 × 1440 is geen standaard DVB-HD-raster.

RTSP-test: `python3 -u streambridge/checks/rtsp_smoke.py`. Deze test vereist
`rtspclientsink`, `rtpmp4gpay` en `rtpmp4apay` voor de native testzender.
Op deze werkplek staat de optionele testplugin onder `build/plugins/`.
De test controleert RTSP/TCP-publicatie, MPEG4-GENERIC/LATM, gewijzigde trackvolgorde,
video-only, gedempte/ontbrekende audio, abrupte uitval en TEARDOWN, herverbinden,
weigeren van een tweede publicatie, continu UDP en stoppen/hergebruik van de poort.
Werkelijke Larix-telefooninteroperabiliteit moet nog op een telefoon worden getest.

Voorbeeld zonder venster:

```sh
streambridge/bin/datv-streambridge --headless --rtsp rtsp://0.0.0.0:8554/live
```

In de INI: `input=0` kiest SRT, `input=1` kiest RTSP; `srt` en `rtsp` bewaren
de bijbehorende adressen. Oude INI-bestanden zonder deze velden blijven SRT gebruiken.
De RTSP-server is GStreamer 1.26.11 (LGPL); bron:
https://gstreamer.freedesktop.org/src/gst-rtsp-server/gst-rtsp-server-1.26.11.tar.xz.

### Larix Android: fout “Unknown connection failure”

Een gemeten Larix 1.5.13/Android 16-aanmelding gebruikte `RTP/AVP/TCP` in de
SDP-mediabeschrijving. De oudere Streambridge-build antwoordde daarop met
`415 Unsupported Media Type`, hoewel de aangeboden H.264/AAC wel ondersteund is.
De ontvanger vertaalt nu uitsluitend deze SDP-notatie intern naar `RTP/AVP`;
het daadwerkelijke transport blijft TCP en codec-/audioparameters blijven intact.
Na bijwerken moet Streambridge volledig opnieuw worden geopend om de nieuwe
ontvanger te laden. Het gewone telefoonadres blijft `rtsp://LAPTOP-IP:8554/live`.

Gerichte regressietest (ook H.264 High en 44,1-kHz mono-AAC, uitval/herstel):

```sh
make -C streambridge build/larix-publisher
python3 -u streambridge/checks/rtsp_larix.py
```
