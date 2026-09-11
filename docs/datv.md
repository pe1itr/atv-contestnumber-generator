# DATV: TS-bestand en UDP-uitvoer

## Bediening

1. Vul de roepnaam, locator en code in en kies een resolutie tot **640 × 480**.
2. Kies **Config → Genius level 2**. Standaard start het programma op level 1.
3. Kies **File → Exporteer TS-proefbestand...**.
4. Vul de **totale TS-bitrate in bit/s** in die Portsdown opgeeft. Dit is niet de
   symboolsnelheid en niet alleen de videobitrate. `60000` betekent 60 kbit/s;
   neem voor je eigen opstelling de daadwerkelijke Portsdown-waarde over.
5. Stel beeldduur, beelden per seconde en GOP-lengte in. **GOP 1** maakt ieder
   beeld een zelfstandig IDR-beeld. Bij 5 beelden/s en GOP 5 komt iedere seconde
   een IDR-beeld. Kies een opslaglocatie; bij vervangen volgt een bevestiging.

De export gebruikt het huidige beeld, inclusief kleuren, band, cijfersom en extra
code rechtsboven. PM5544 is ook beschikbaar, maar vraagt mogelijk meer bitrate.
Het nummer verandert niet bij export. Instellingen worden in deze eerste versie
alleen gedurende de huidige programmasessie onthouden.

Grenzen: bitrate 48000–2000000 bit/s, beeldduur 1–60 s, 1–25 beelden/s, GOP 1–250
beelden. Het gekozen beeld moet even afmetingen hebben, maximaal 640 × 480.
De export draait op een achtergrondthread en publiceert pas een compleet bestand.

## Encoder en transport

**OpenH264 2.6.0** is statisch ingebouwd. De MPEG-TS-muxer is eigen gedeelde code in
`src/datv.cpp`. Er worden geen FFmpeg-programma's of FFmpeg-bibliotheken gebruikt.
Windows blijft één verspreidbare executable, zonder losse codec-DLL.
**Info → OpenH264-licentie** bevat de ingebouwde bibliotheeklicentie.

De TS bevat één H.264 Constrained Baseline-videokanaal met vierkante pixels en
BT.601-kleuromzetting, zonder B-frames en zonder audio-PID. Servicenaam én provider
komen uit de roepnaam, inclusief een eventuele `/`. Service-ID, TS-ID en
original-network-ID zijn 1; video/PCR-PID is `0x0100`, PMT-PID `0x1000`.
PAT en PMT worden ongeveer iedere 200 ms herhaald, SDT iedere seconde.
Elk IDR-beeld bevat opnieuw SPS/PPS-decoderinformatie.

De encoder probeert vaste QP-waarden van 24 t/m 48, in stappen van 4. De eerste
kwaliteit waarbij **alle** beelden binnen de transportplanning passen wordt
gebruikt; beelden worden niet overgeslagen. De status toont de QP, het aantal
beelden en de grootste IDR in bytes. Een hogere QP betekent sterkere compressie.
De passingstest garandeert op zichzelf geen leesbaarheid: beoordeel vooral code
en roepnaam in het afgespeelde beeld. Een onhaalbare combinatie geeft een
foutmelding en vervangt geen bestaand bestand.

De transportplanning gebruikt één seconde decoderbuffer: de eerste PTS is 1 s.
Een bestand met 10 s beeldduur bevat circa 11 s transportdata, afgerond op zeven
TS-pakketten. Null-pakketten vullen de beschikbare bitrate op. Het bestand bestaat
uit gewone 188-byte TS-pakketten, met een totale lengte deelbaar door 1316.
Bij bestandsexport wordt geen UDP-verkeer verstuurd; de aparte UDP-uitvoer staat hieronder beschreven.

PCR's zijn berekend op basis van pakketpositie en TS-bitrate. De herhaling mikt
op maximaal 40 ms waar de pakketfrequentie dat toelaat. Bij lage bitrates houden
we minimaal twee pakketten tussen PCR's om ook tabellen te kunnen versturen:
bij 60.000 bit/s is dat circa 50,1 ms. Dat blijft onder de MPEG-TS-grens van 100 ms,
en ook de huidige DVB-grens. De oude 40-ms-beperking is in 2005 verwijderd;
40 ms blijft hier een gekozen doelwaarde. Zie [ETSI TR 101 290, pagina 24](https://www.etsi.org/deliver/etsi_tr/101200_101299/101290/01.04.01_60/tr_101290v010401p.pdf).

## Afspelen en analyseren

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
breedte en hoogte. Getallen zijn optioneel; standaard: 120000, 10, 5, 5, 320, 240.
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
uitvoer bewaakt de muxer de beelddeadlines. Socketfouten, volle verzendbuffers en
een achterstand van meer dan één datagramperiode stoppen met een melding.
Na slaapstand worden achterstallige pakketten dus niet in een grote reeks ingehaald.
Start daarna opnieuw. Stop kan de laatste video-PES afbreken; er worden nooit
halve UDP-datagrammen verstuurd.

### Lokale netwerkcontrole

```sh
make test-udp
make build/udp-sender.exe
python3 checks/check_udp.py --windows
```

De tests gebruiken uitsluitend `127.0.0.1` en een vrije lokale poort. Ze meten de
werkelijke aankomsttijden en pakketgroottes bij 60, 120 en 240 kbit/s, controleren
doorlopende TS-timing over meerdere GOP's en meer dan tien seconden, en testen
Stop en annuleren tijdens voorbereiding. FFmpeg verifieert de gedecodeerde beelden,
TSDuck controleert continuïteit en PCR. Voor GOP 1 worden ook de wisselende
IDR-identificaties gecontroleerd. De venstertests controleren Start/Stop, opnieuw
starten, ongeldige adressen en sluiten tijdens uitzending, op beide platforms.

## Vervolg

Modulatorprofielen, doorlopend wijzigen van de uitgezonden kaart en permanente
nummeropslag per band/contest zijn vervolgstappen. De lokale netwerkproeven
vervangen geen test met Portsdown en echte RF-ontvangst.
