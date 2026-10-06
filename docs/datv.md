# DATV: TS-bestand en UDP-uitvoer

## Bediening

1. Vul de roepnaam, locator en code in en kies een resolutie tot **1920 × 1080 (16:9)** of **1920 × 1440 (4:3)**.
2. Kies **Config → Genius level 2**. Standaard start het programma op level 1.
3. Kies **File → Exporteer TS-proefbestand...**.
4. Kies **DVB-S, DVB-S2 of DVB-T**, SR/BW, FEC en eventueel pilots of GI.
   De **totale TS-bitrate in bit/s** wordt automatisch berekend. Dit is niet
   alleen de videobitrate. Zie [de berekening](dvb-bitrate.md).
5. Stel beeldduur, beelden per seconde, GOP-lengte en transportbuffer in. **GOP 1** maakt ieder
   beeld een zelfstandig IDR-beeld. Bij 5 beelden/s en GOP 5 komt iedere seconde
   een IDR-beeld. Kies een opslaglocatie; bij vervangen volgt een bevestiging.

De export gebruikt het huidige beeld, inclusief kleuren, band, cijfersom en extra
code rechtsboven. PM5544 is ook beschikbaar, maar vraagt mogelijk meer bitrate.
Het nummer verandert niet bij export. **Config → Huidige instellingen opslaan**
bewaart de huidige kaart en TS-/UDP-instellingen in `atv-contestnummer.conf` naast
het programma. Bij opstarten wordt dit bestand automatisch geladen, zonder UDP
te starten. Zonder bestand blijven de normale standaardinstellingen gelden.

Grenzen: bitrate 30080–2000000 bit/s, beeldduur 1–60 s, 1–25 beelden/s, GOP 1–250
beelden; transportbuffer 1000–10000 ms (standaard 1000 ms). Het gekozen beeld moet even afmetingen hebben, maximaal 1920 × 1440.
De resolutie is niet aan een vaste SR-grens gekoppeld: de encoder controleert of
het beeld bij de gekozen bitrate, beeldfrequentie en GOP past. Bij te weinig
ruimte volgt een melding; kies dan meer transportbuffer, minder pixels, minder beelden/s of een langere GOP.
De export draait op een achtergrondthread en publiceert pas een compleet bestand.

## Encoder en transport

Voor de aanbeveling wit op zwart, de vergelijking bij 125 ksym/s en de
betekenis voor reflecties van 2–3 seconden, zie
[Kleurkeuze voor een DATV-contestbeeld](contest-kleurkeuze.md).

**OpenH264 2.6.0** is statisch ingebouwd. De MPEG-TS-muxer is eigen gedeelde code in
`src/datv.cpp`. Linux gebruikt voor optionele live audio de systeemlibraries
`libpulse`, `libavcodec` en `libavutil`; er worden geen FFmpeg-processen gestart.
Windows blijft één verspreidbare executable, zonder losse codec-DLL.
**Info → OpenH264-licentie** bevat de ingebouwde bibliotheeklicentie.

De TS bevat één H.264 Constrained Baseline-videokanaal met vierkante pixels en
BT.601-kleuromzetting, zonder B-frames. Zonder ingeschakelde Linux-audio is er geen audio-PID. Servicenaam én provider
komen uit de roepnaam, inclusief een eventuele `/`. Service-ID, TS-ID en
original-network-ID zijn 1; video/PCR-PID is `0x0100`, PMT-PID `0x1000`.
PAT en PMT worden ongeveer iedere 200 ms herhaald. SDT wordt na 800 ms opnieuw
aangevraagd, met ruimte voor pakketplanning tot circa een seconde.
Elk IDR-beeld bevat opnieuw SPS/PPS-decoderinformatie.

De encoder probeert vaste QP-waarden van 24 t/m 48, in stappen van 4. De eerste
kwaliteit waarbij **alle** beelden binnen de transportplanning passen wordt
gebruikt; beelden worden niet overgeslagen. De status toont de QP, het aantal
beelden en de grootste IDR in bytes. Een hogere QP betekent sterkere compressie.
De passingstest garandeert op zichzelf geen leesbaarheid: beoordeel vooral code
en roepnaam in het afgespeelde beeld. Een onhaalbare combinatie geeft een
foutmelding en vervangt geen bestaand bestand.

De transportplanning gebruikt de gekozen transportbuffer: de eerste PTS ligt
op 1000–10000 ms. Standaard is dit 1000 ms. Een bestand met 10 s beeldduur en
1000 ms buffer bevat circa 11 s transportdata, afgerond op zeven TS-pakketten. Null-pakketten vullen de beschikbare bitrate op. Het bestand bestaat
uit gewone 188-byte TS-pakketten, met een totale lengte deelbaar door 1316.
Bij bestandsexport wordt geen UDP-verkeer verstuurd; de aparte UDP-uitvoer staat hieronder beschreven.

PCR's zijn berekend op basis van pakketpositie en TS-bitrate. De herhaling mikt
op maximaal 40 ms waar de pakketfrequentie dat toelaat. Bij lage bitrates houden
we minimaal twee pakketten tussen PCR's om ook tabellen te kunnen versturen:
bij 60.000 bit/s is dat circa 50,1 ms. Dat blijft onder de MPEG-TS-grens van 100 ms,
en ook de huidige DVB-grens. De oude 40-ms-beperking is in 2005 verwijderd;
40 ms blijft hier een gekozen doelwaarde. Zie [ETSI TR 101 290, pagina 24](https://www.etsi.org/deliver/etsi_tr/101200_101299/101290/01.04.01_60/tr_101290v010401p.pdf).

## Afspelen en analyseren

### EIT-programma-informatie

Via **Config → EIT...** vul je optioneel **stad**, **operatornaam** (elk maximaal
40 tekens) en **stationsomschrijving** (maximaal 240 tekens) in. Roepnaam en
locator worden automatisch uit het hoofdvenster overgenomen, ook als de locator
niet op de kaart wordt getoond. De velden zijn eenregelig; accenten en andere
BMP-tekens zijn toegestaan, emoji niet. **Toepassen** bewaart de tekst deze sessie;
**Config → Huidige instellingen opslaan** bewaart haar na herstart.

Vink in **TS-proefbestand** of **DATV UDP-uitvoer** de optie
**EIT-programma-informatie meesturen** aan. Beide keuzes worden afzonderlijk
bewaard en staan standaard uit, ook bij het laden van oudere configuraties.
De titel is `ROEPNAAM - LOCATOR`, de korte beschrijving is de stad en de uitgebreide
beschrijving bevat operatornaam en stationsomschrijving. Lege extra velden
worden weggelaten. Zonder locator wordt alleen de roepnaam als titel gebruikt.
Dit is programma-/EPG-informatie, geen ondertitel of tekstlaag in de video.
VLC kan deze informatie in de programmagids tonen; de precieze weergave verschilt
per speler.

### Teletekstpagina 100

Open **Config → Teletekst** om de tekst van pagina 100 te bewerken (maximaal 23
regels van 40 tekens). **Opslaan** bewaart de pagina meteen in
`atv-contestnummer.conf`. In zowel **TS-proefbestand** als **DATV UDP-uitvoer**
staat **Teletekstpagina 100 meesturen** direct onder de EIT-keuze. De pagina
wordt dan als DVB-teletekst in de MPEG-TS opgenomen en elke twee seconden
herhaald. Hiervoor gaat ongeveer 7,5 kbit/s van de ingestelde TS-bitrate naar
teletekst; de totale TS-bitrate blijft gelijk.

De muxer verstuurt EIT present/following actual op PID `0x12` en TDT op PID `0x14`.
Er is één huidig stationinformatievenster van UTC-middernacht tot de volgende
UTC-middernacht, plus een lege following-sectie; er is geen geplande volgende
uitzending. De systeemklok bij Start/export bepaalt de datum. Tijdens UDP loopt
de UTC-tijd mee met de transportklok. Bij UTC-middernacht veranderen event-ID en
versie automatisch. Gewijzigde stationinformatie na Stop/Start krijgt binnen
dezelfde programmasessie een nieuwe EIT-versie. Er is geen live tekstbewerking:
stop de stream, wijzig de gegevens en start opnieuw.

Tekst gebruikt UTF-8-selector `0x15` met `short_event_descriptor` en zo nodig
meerdere `extended_event_descriptor`s. De muxer kan secties over meerdere
TS-pakketten verdelen en controleert bij de voorbereiding en tijdens UDP de
herhaling: PAT/PMT maximaal 500 ms, SDT en beide EIT-secties maximaal 2 s,
TDT maximaal 30 s. Tussen EIT-secties zit minstens 25 ms. De totale ingestelde
TS-bitrate blijft gelijk; extra tabellen kosten beschikbare transportcapaciteit.
Bij onvoldoende ruimte volgt vóór de uitvoer een foutmelding. Verhoog dan de
bitrate of verkort de omschrijving. Zeer lange teksten kunnen bij 32 kbit/s niet
passen, ook bij een eenvoudig beeld.

Referenties: [EN 300 468 V1.19.1](https://www.etsi.org/deliver/etsi_en/300400_300499/300468/01.19.01_60/en_300468v011901p.pdf)
§§5.1.4.1, 5.2.3–5.2.5, 6.2.15, 6.2.37 en bijlage A;
[TS 101 211 V1.13.1](https://www.etsi.org/deliver/etsi_ts/101200_101299/101211/01.13.01_60/ts_101211v011301p.pdf)
§§4.1.4.1 en 4.4.1–4.4.2. Dit blijft een vereenvoudigd DATV-profiel: onder andere
NIT ontbreekt. Deze toevoeging betekent geen volledige DVB-conformiteitsclaim.

`make test-eit` controleert EIT/TDT, UTF-8, CRC, continuïteit, bitrategrenzen,
lange teksten, UTC-dagovergang en versieaanpassing. De bijbehorende Windows-test
is `build/test-eit.exe`; `checks/check_eit.py` analyseert beide uitvoerplatforms.
`python3 checks/check_eit.py --udp [--windows]` controleert EIT via UDP-loopback.

### Spelers en streamanalyse

```sh
ffplay mijn-contest.ts
vlc mijn-contest.ts
tsanalyze mijn-contest.ts
tsp -I file mijn-contest.ts -P continuity -P pcrverify --bitrate 60000 --jitter-max 1 -O drop
```

Gebruik bij `pcrverify` dezelfde bitrate als bij de export. TSDuck berekent de
TS-bitrate uit PCR's. De `format.bit_rate` van ffprobe kan hoger lijken doordat
die de totale bestandsgrootte deelt door de videoduur, zonder de decoder-aanloop.

Op beide platforms bestaat een testinterface met een vast referentiebeeld:
`PE1ITR`, `JO21QK`, code `1957`, 436 MHz, cijfersom en extra code rechtsboven.
De argumenten zijn achtereenvolgens bestand, bitrate, beeldduur, fps, GOP,
breedte, hoogte en optioneel de transportbuffer in ms. Getallen zijn optioneel;
standaard: 120000, 10, 5, 5, 320, 240, 1000.
Deze testinterface weigert bestaande bestanden te overschrijven.

```sh
./dist/atv-contestnummer --ts-test proef-gop.ts 60000 10 5 5 320 240
./dist/atv-contestnummer --ts-test proef-idr.ts 60000 10 2 1 320 240
make test-ts
```

`make test-ts` controleert CRC's, tabellen, continuïteit, PCR's, beelddeadlines,
IDR/SPS/PPS en ongeldige invoer. Indien geïnstalleerd worden ffprobe, FFmpeg en
TSDuck als onafhankelijke testtools gebruikt. Zes profielen worden gecontroleerd,
inclusief korte ontvangstvensters vanaf willekeurige pakketposities. Een verse
decoder moet daarin een beeld met dezelfde pixelhash als in het volledige bestand
kunnen herstellen. Dit simuleert instappen, niet alle vormen van RF-pakketverlies.

GUI-tests bouwen:

```sh
make build/test-datv-linux-ui build/test-datv-windows-ui.exe
```

Voer `build/test-datv-linux-ui` uit in een grafische Linux-sessie en de Windows-test
via Wine of Windows. Ze bedienen de levelkeuze, instellingen en opslagvensters.
`make test-linux` controleert de gedeelde regels en 680 gerenderde JPG's.

## UDP naar Portsdown

Schakel **Config → Genius level 2** in en kies **File → DATV UDP-uitvoer...**. Vul het numerieke
IPv4-adres van Portsdown en de poort in (standaard **10000**). Alleen unicast IPv4
wordt ondersteund; geen hostnamen, IPv6, multicast of globale broadcast.
De TS-bitrate is opnieuw de volledige bitrate in bit/s die Portsdown opgeeft.
De UDP-instellingen beginnen met **120000 bit/s, 10 beelden/s en GOP 2**.
Deze waarden zijn een uitgangspunt; lagere bitrates kunnen een kleiner beeld,
lagere beeldfrequentie of langere GOP nodig hebben.

**Start** neemt het huidige beeld en de roepnaam over en bereidt de encoder voor.
Vervolgens loopt de uitvoer door tot **Stop**. Sluiten (ook het kruisje) stopt de
stream en sluit de socket. De velden zijn tijdens voorbereiding en uitzending
vergrendeld. De status toont doeladres, bitrate, verzonden pakketten en gekozen QP.
"UDP-uitvoer actief" bevestigt dat pakketten worden verstuurd, niet dat de zender
ze ontvangt: UDP geeft daar geen ontvangstbevestiging voor.

Om de code, band of andere beeldinhoud te wijzigen: Stop, sluit het UDP-venster,
pas het beeld aan en kies opnieuw Start. Instellingen blijven deze sessie bewaard.
Met **Toepassen en sluiten** kun je UDP-instellingen overnemen zonder te zenden.
Kies daarna **Config → Huidige instellingen opslaan** voor bewaren na herstart.
**Sluiten** annuleert nog niet toegepaste wijzigingen.
Er is geen automatische start bij openen van het programma. TS-exportduur heeft
geen invloed op UDP: de stream loopt door zolang je hem aan laat staan.

De worker encodeert een gesloten GOP en hergebruikt die beelddata. Bij GOP 1 worden
twee IDR-beelden gebruikt zodat opeenvolgende `idr_pic_id`-waarden verschillen,
zoals H.264 voorschrijft. De muxer genereert daarbij **nieuwe, doorlopende PTS,
PCR en continuïteitstellers**: er wordt geen TS-bestand met terugspringende klok
herhaald. De klokken lopen ook correct door wanneer de 33-bit tijdstempels omklappen.
Elke expliciete nieuwe Start begint een nieuwe stream, met discontinuïteitsmarkering
op de eerste PCR/video- en tabelpakketten.

Elk UDP-datagram bevat precies **1316 bytes = 7 × 188 bytes** gewone MPEG-TS,
zonder RTP-header. IP- en UDP-headers komen daar bovenop. De tussenpoos is
`1316 × 8 / TS-bitrate` seconden: bij 120000 bit/s circa **87,733 ms**, bij
60000 bit/s circa **175,467 ms**. Verzending gebruikt absolute monotone deadlines,
zodat afrondingen van slaaptijden niet langzaam optellen. Windows gebruikt tijdens
uitvoer een timerresolutie van 1 ms en geeft die bij stoppen weer vrij.

Voorbereiding controleert meerdere herhalingen en minimaal 30 seconden transport;
een onhaalbare bitrate wordt vóór de eerste verzending geweigerd. Ook tijdens
uitvoer bewaakt de muxer de beelddeadlines. Poortweigeringen (Linux ECONNREFUSED,
Windows WSAECONNREFUSED/WSAECONNRESET) worden intern geteld; de stream loopt
door zodat een later gestarte IPTS-ontvanger kan instappen. Het geweigerde datagram
wordt overgeslagen, zonder hertransmissie. Windows opent hiervoor de socket opnieuw.
De pakketteller telt lokaal geslaagde verzendingen, geen bevestigde ontvangst.
De status toont alleen een korte uitleg bij een recente poortweigering, zonder
historische foutteller. Na drie seconden zonder nieuwe weigering verdwijnt de
waarschuwing; dit betekent niet dat de ontvanger ontvangst heeft bevestigd.
Andere socketfouten, volle verzendbuffers en
een achterstand van meer dan één datagramperiode stoppen met een melding.
Na slaapstand worden achterstallige pakketten dus niet in een grote reeks ingehaald.
Start daarna opnieuw. Stop kan de laatste video-PES afbreken; er worden nooit
halve UDP-datagrammen verstuurd.

### Lokale netwerkcontrole

```sh
make test-udp
make build/udp-sender.exe build/test-udp-errors.exe
python3 checks/check_udp.py --windows
```

De tests gebruiken uitsluitend `127.0.0.1` en een vrije lokale poort. Ze meten de
werkelijke aankomsttijden en pakketgroottes bij 60, 120 en 240 kbit/s en bij de
vaste keuzes 115196 en 123607 bit/s, controleren
doorlopende TS-timing over meerdere GOP's en meer dan tien seconden, en testen
Stop en annuleren tijdens voorbereiding. FFmpeg verifieert de gedecodeerde beelden,
TSDuck controleert continuïteit en PCR. Voor GOP 1 worden ook de wisselende
IDR-identificaties gecontroleerd. De venstertests controleren Start/Stop, opnieuw
starten, ongeldige adressen en sluiten tijdens uitzending, op beide platforms.

De UDP-test start de ontvanger ook later en sluit/heropent de poort tijdens de
uitzending. Na herstel worden pacing en gedecodeerde beelden met een gezonde
stream vergeleken. Gerichte foutinjectie controleert de weigeringafhandeling en
dat andere fouten fataal blijven. Wine kan ICMP-poortweigeringen onderdrukken;
de test meldt die systeemdekking dan expliciet als niet geverifieerd, naast de
verplichte Windows-foutinjectietest. Dit vervangt geen proef met echte Windows
en de Portsdown.

## Vervolg

Modulatorprofielen, doorlopend wijzigen van de uitgezonden kaart en permanente
nummeropslag per band/contest zijn vervolgstappen. De lokale netwerkproeven
vervangen geen test met Portsdown en echte RF-ontvangst.

## Lokaal compressievoorbeeld

`datv_preview_start` gebruikt dezelfde doorlopende GOP-proefplanning en QP-keuze
als `datv_udp_start`. In de voorbeeldtak wordt geen socket geopend en worden geen
pakketten verstuurd. Na de kwaliteitskeuze decodeert OpenH264 de eerste IDR uit de
werkelijk gecodeerde frames; de vaste PES-header wordt vóór decodering verwijderd.
De decoderpixels worden uit limited-range BT.601 YUV420 naar RGB omgerekend en
naast het origineel getoond. Er wordt geen alternatieve encoderinstelling gebruikt.

De asynchrone taak ondersteunt stoppen en opruimen via de bestaande jobfuncties;
`datv_preview_image` kopieert alleen een voltooid voorbeeld. `DATV_STOPPED` betekent
voor deze lokale taak dat de verwerking gereed is, `DATV_FAILED` bevat een fout.
Het voorbeeld is één IDR, geen simulatie van RF-ontvangst of bewijs van leesbaarheid.

### Grafische transporttijdlijn

**File → DATV UDP-uitvoer → Beeld controleren** toont bij de gekozen bitrate,
fps, GOP en het huidige testbeeld twee tijdlijnen: de eerste seconde en één GOP
plus de gekozen transportbuffer. Sluit het voorbeeld, verander de instellingen en
kies opnieuw **Beeld controleren** om te vergelijken. Er wordt niets uitgezonden.

- Blauw: pakketten van zelfstandige IDR-beelden; groen: P-vervolgbeelden.
- Oranje: overige TS-pakketten, waaronder tabellen, teletekst en losse PCR's.
- Grijs: null-pakketten die vrije capaciteit opvullen.
- Paars: het contestrichtpunt van **1000 ms**, een adviesmarker voor de operator.
- Zwart: het einde van het eerste volledige IDR-beeld; rood: waar de planning faalt.

De kleuren tonen tijdsaandelen per klein tijdvak; bij uitzoomen kunnen meerdere
pakkettypen boven elkaar staan. Videopakketten bevatten ook hun TS-/PES-headers
en eventuele PCR. De streepjes onder de balk tonen frameperioden vanaf de
encoder-vrijgave; bij lange GOP's zijn deze gegroepeerd. De getallen zijn ms.

**Zelfstandig beeld (IDR)** is de hoofdwaarde: het minimum en maximum van de
volledig overgebrachte IDR-beelden tijdens de proefplanning, vanaf het begin van
het eerste videopakket tot het einde van het laatste pakket van die IDR.
Tussenliggende overhead telt mee. Dit is geen GOP-duur of wachttijd op een IDR.
De contestindicatie vergelijkt het gemeten maximum met 1000 ms, zonder de
operator te verbieden langere overdrachten te kiezen. De marker is een gekozen
contestrichtpunt, geen algemene natuurkundige grens voor vliegtuigscatter.

**Eerste volledige beeld** is de tijd vanaf TS-start tot het einde van het laatste
TS-pakket van de eerste IDR, inclusief tussenliggende overhead. Het betreft de
CBR-transportklok, geen gemeten UDP-ontvangsttijd. UDP bundelt zeven TS-pakketten;
netwerkvertraging, signaalvergrendeling en decodervertraging zijn niet inbegrepen.
De eerste geplande presentatietijd is gelijk aan de gekozen transportbuffer. De werkelijke beeldgrootte is
pas bekend na H.264-compressie; alleen resolutie en bitrate volstaan dus niet.

Een videoframe is geen TS-pakket. Eén volledig IDR-beeld mag al over meerdere
**frameperioden** worden verstuurd, zolang het voor zijn deadline aankomt.
Een P-beeld beschrijft veranderingen ten opzichte van eerdere beelden en is niet
zelfstandig te decoderen. De frameperiode is `1000 / fps` ms; het IDR-interval is
`1000 × GOP / fps` ms. Dat interval is niet de overdrachtstijd van een IDR.
Bij later instappen kan daarnaast wachten op een volgende IDR nodig zijn.

Bij een onhaalbare combinatie toont het voorbeeld de mislukte planning bij de
laatst beproefde QP en het nummer van het beeld waar de planning stopt. Alleen
voltooide overdrachten krijgen een gemeten eerste-beeldtijd. De aanvullende
waarde `8000 × IDR-bytes / TS-bitrate` is een **ondergrens zonder transportoverhead**,
geen voorspelling van een werkende stream. Extra buffertijd geeft een groot IDR meer tijd, maar lost een blijvend tekort
aan gemiddelde capaciteit niet op. De buffer wordt afzonderlijk voor TS-export
en UDP bewaard via Config → Huidige instellingen opslaan. Oudere configuraties
blijven 1000 ms gebruiken.

Bij buffers groter dan 1000 ms bewaakt de muxer bovendien conservatief de
opgestapelde PES-bytes tegen de AVC-CPB-capaciteit van het SPS-level. Bytes worden
pas op hun PTS vrijgegeven (geen B-frames); PES-headers tellen voorzichtigheidshalve
mee. Dit is geen volledige T-STD-simulatie. De bovengrens van 10000 ms houdt
rekening met gewone AVC, zonder een speciaal still-picture-profiel te claimen.
Referenties: [H.222.0 (05/2006)](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-H.222.0-200605-S!!PDF-E&lang=s&type=items)
§2.14.3.1 en [H.264 (08/2024)](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-H.264-202408-S!!PDF-E&lang=e&type=items)
§A.3.1, tabel A-1.
Ontvangercompatibiliteit bij lange buffers moet ook op de echte ontvanger worden
gecontroleerd.

`build/test-timeline` en `build/test-timeline.exe` controleren de tijdlijn tegen
werkelijke muxpakketten, inclusief een onhaalbare combinatie, lange GOP, langere overdracht en decoderbuffergrens.


## Optionele live audio op Linux (Genius level 3)

Kies **Config → Genius level 3 (Linux audio)** en open **File → DATV UDP output**.
Audio is standaard uit. Kies DVB-S of DVB-S2 met **333 of 500 ksym/s**, vink
**Include Linux audio** aan en gebruik `ffmix.monitor` of vul een bestaande
PulseAudio/PipeWire-bron in.
Bij een virtuele sink `ffmix` is de opnamebron **`ffmix.monitor`**. Een directe
microfoonbron is ook mogelijk. `pactl list short sources` toont de beschikbare
namen; de toepassing opent uitsluitend de ingevulde bron en schakelt bij verlies
niet ongemerkt over naar een andere microfoon.

Keuzes zijn **48.000 bit/s mono** en **96.000 bit/s stereo**, beide AAC-LC met
48.000 samples/s. Bitrate en samplefrequentie zijn verschillende instellingen.
De ontvanger moet AAC-LC in LATM/LOAS ondersteunen. Windows blijft voorlopig
zonder audio; bij laden van een Linux-configuratie wordt level 3 daar level 2
en wordt audio uitgeschakeld. TS-bestandsexport blijft zonder audio.

Bij **Start met audio** maakt de toepassing automatisch de virtuele sink
`ffmix` aan wanneer de gekozen bron `ffmix.monitor` nog niet bestaat. Een bestaande
sink wordt hergebruikt. Andere bronnamen moeten al bestaan. De sink wordt pas
bij Start aangemaakt, niet bij Beeld controleren of Instellingen toepassen.
Na Start volstaat bijvoorbeeld:

```sh
ffmpeg -hide_banner -i https://icecast.omroep.nl/radio2-bb-aac -af volume=0.5 -f wav - | PULSE_SINK=ffmix paplay
```

Handmatig aanmaken kan ook:

```sh
pactl load-module module-null-sink sink_name=ffmix rate=48000 channels=2
```

Speel daarin audio af, bijvoorbeeld met het eigen radiostreamscript of:

```sh
PULSE_SINK=ffmix paplay opname.wav
```

De sink blijft bij Stop en sluiten bestaan, zodat externe spelers hun
bestemming behouden; herstarten gebruikt dezelfde sink. Stop tijdens een
lopende aanmaak kan de serveractie niet altijd annuleren, dus ook dan kan
`ffmix` blijven bestaan. Alle menging en bronkeuzes
kunnen buiten het programma blijven. Een stille sink levert stilte; het wegvallen
van de bron stopt de UDP-stream met een foutmelding. Stoppen onderbreekt ook het
wachten tijdens het openen van de audioserver.

De muxer reserveert conservatief **141.000 respectievelijk 211.500 bit/s TS**
voor audio, inclusief PES/TS-overhead en ruimte voor AAC-pieken: twee of drie
TS-pakketten per 1024 audiosamples. Ongebruikte gereserveerde plekken worden
null-pakketten. De totale ingestelde TS-bitrate verandert niet. De beeldcontrole
gebruikt exact dezelfde reservering; een lage FEC kan ook bij 333/500 ksym/s te
weinig ruimte overlaten. Er moet minimaal 40 kbit/s overblijven, waarna de
werkelijke beeldplanning nog moet slagen. Tijdens verzending worden deadlines
opnieuw bewaakt; een te grote audiopiek geeft een fout in plaats van een te late PES.

Audio gebruikt PID `0x0102`, PMT stream_type `0x11`, AAC-descriptor `7c 01 51`
en PES stream_id `0xc0`. Elk frame herhaalt StreamMuxConfig voor instappen in de
stream. Eén AAC-frame bevat 1024 samples, dus de PTS-stap is 1920 ticks bij 90 kHz.
De eerste audio-PTS is gelijk aan de eerste video-PTS. Audio wordt pas maximaal
100 ms vóór zijn PTS verstuurd, ook met een videotransportbuffer van 10 seconden.
Bij een lange videobuffer verschijnt de eerste audio-PES dus ook pas laat
(bij 10 s buffer rond 9,9 s). Ontvangers/analysetools met een korte detectietijd
kunnen audio aanvankelijk missen; gebruik bijvoorbeeld `-analyzeduration 15000000
-probesize 10000000` bij ffprobe. Een PID-aanwezigheidsalarm met een 5 s-drempel
kan tijdens deze opstartfase afgaan. Dit is een bekende beperking van dit profiel.
De capturebuffer is begrensd; een kleine correctie van maximaal één sample per
AAC-frame met lineaire interpolatie vangt verschil tussen audioklok en TS-klok op.
Er wordt een statisch beeld verstuurd: dit is geen synchronisatie van een externe
live videobron met de audio.

Bouwen vereist naast de bestaande libraries de ontwikkelpakketten voor
PulseAudio, libavcodec en libavutil (Debian/Ubuntu: `libpulse-dev libavcodec-dev
libavutil-dev`). Audio-opname vereist een actieve PulseAudio-server of PipeWire
met Pulse-compatibiliteit. Er is geen los `ffmpeg`-programma nodig voor gebruik.

`make test-audio` test een tijdelijke null-sink met stilte, toon en ruis, beide
bitrates, beide symbol rates, 10 seconden videobuffer, bronverlies en een ontbrekende
bron. De test gebruikt uitsluitend localhost en verwijdert zijn eigen sink.
Benodigd: `pactl`, `paplay`, FFmpeg/ffprobe en TSDuck. Captures en logs staan in
`build/audio-check/`. Deze controles bewijzen geen volledige DVB/ETSI-conformiteit
of de timing aan een fysieke RF-uitgang.


`make test-audio-sink` is een afzonderlijke, expliciete lifecycle-test: `ffmix`
moet vooraf ontbreken. De test laat de toepassing de sink aanmaken, speelt een
lokale toon af en controleert Stop/Start met dezelfde sink en ongewijzigde
standaardapparaten. Hij laat `ffmix` daarna bestaan, net als de toepassing.
