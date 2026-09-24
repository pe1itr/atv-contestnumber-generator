# ATV contestnummer generator

De **ATV contestnummer generator** is een tool voor amateurtelevisiestations die
meedoen aan **ATV- en DATV-wedstrijden**. Tijdens deze wedstrijden wisselen stations
een geheime viercijferige code uit via het uitgezonden beeld. Met dit programma
maak je een contesttestbeeld met die code, je roepnaam, locator en gebruikte band.

Je kunt het contesttestbeeld op twee manieren gebruiken:

- **Opslaan als JPG-bestand** in verschillende resoluties en beeldverhoudingen
  (4:3 en 16:9), om het met je eigen televisieapparatuur uit te zenden.
- **Via UDP als MPEG-TS-stream naar een DVB-zender sturen**, zodat het beeld
  rechtstreeks als bron voor een DATV-uitzending kan dienen. Het programma
  berekent de TS-bitrate uit de gekozen DVB-S-, DVB-S2- of DVB-T-parameters.

Het programma is beschikbaar voor **Windows en Linux**. Naast de contestkaart
kun je PM5544-, PM5644- en FUBK-testbeelden met je eigen roepnaam en locator maken.
De functies voor MPEG-TS-bestanden en UDP-uitvoer worden zichtbaar via
**Config → Genius level 2**.

Download de nieuwste Windows-versie: [atv-contestnummer.exe](https://github.com/pe1itr/atv-contestnumber-generator/releases/latest/download/atv-contestnummer.exe). Alle uitgaven staan bij [GitHub Releases](https://github.com/pe1itr/atv-contestnumber-generator/releases).

Voor Windows verspreid je uitsluitend `dist/atv-contestnummer.exe`: installatie, PHP, .NET, losse fonts en meegeleverde DLL's zijn niet nodig. Deze versie gebruikt de ingebouwde Windows-functies voor vensters, letters en JPG-compressie en is voor 64-bits Windows (Windows 10/11).

De native Linux-versie is `dist/atv-contestnummer` en gebruikt GTK 3, Pango/Cairo en GdkPixbuf, die in de ontwikkelomgeving beschikbaar zijn. Start vanuit de projectmap:

```sh
./dist/atv-contestnummer
```

Beide versies hebben dezelfde invoer, resoluties, coderegels, bandkeuzes en beeldindeling, inclusief de band rechtsonder. In Contest-modus gebruikt Linux de lokaal beschikbare Arial of een vervangend lettertype; de exacte letterweergave kan daardoor iets afwijken. Ook op Linux komen de JPG-bestanden naast het programma, onafhankelijk van de huidige werkmap.

## Menubalk en programma-informatie

**File → Exporteer JPG** slaat het huidige beeld op, met dezelfde invoercontroles en bestandsnaam als de exportknop.

**File → Exporteren naar...** opent een opslagvenster met de gegenereerde JPG-bestandsnaam al ingevuld. Navigeer naar de gewenste map en klik op **Opslaan**. Je kunt ook de bestandsnaam aanpassen. Bij een bestaand bestand vraagt het programma om bevestiging; **Annuleren** schrijft niets. Dit werkt voor Contest en PM5544. De knop **Exporteer JPG** en **File → Exporteer JPG** blijven in de programmamap opslaan.

**File → Quit** sluit het programma af.

**Config → Huidige instellingen opslaan** bewaart de huidige invoer, het contestnummer,
beeldtype, beeldverhouding, resolutie, band, beeldopties, Genius-level en alle TS- en
UDP-instellingen in `atv-contestnummer.conf`, naast het uitvoerbare bestand.
Bij de volgende start wordt dit bestand automatisch ingelezen, ook als je het
programma vanuit een andere werkmap start. Zonder bestand gelden de normale
standaardinstellingen. Bij een ongeldig of onleesbaar bestand volgt een melding en
blijven de standaardinstellingen actief. Opnieuw opslaan vervangt het eerdere bestand;
de map moet beschrijfbaar zijn. Linux en Windows gebruiken hetzelfde bestandsformaat.
Oudere configuraties worden bij laden omgezet naar de nieuwe resolutievolgorde;
de gekozen beeldafmetingen blijven behouden. Opslaan gebruikt formaatversie 3.

In het UDP-venster bewaart **Toepassen en sluiten** de ingevoerde instellingen voor
de huidige sessie zonder te zenden. Kies daarna **Config → Huidige instellingen
opslaan** om ze ook voor een volgende start te bewaren. **Sluiten** annuleert nog niet
toegepaste wijzigingen en stopt een eventuele stream. Na opstarten staat UDP altijd
uit; je start het uitzenden zelf. Ook een automatisch contestnummer wordt bij laden
behouden. Alleen de huidige kaart wordt opgeslagen, geen afzonderlijke nummers per band.

**Info → Over dit programma** toont het doel, uitleg over het gebruik, de auteur, het versienummer en de compilatiedatum. De huidige versie is **1.7.0**. Versie en auteur staan centraal in `src/app_info.h`; de datum wordt tijdens compilatie vastgelegd en is niet de datum waarop je het programma start.

## Gebruik: Contest

Kies bovenaan bij **Beeldtype** voor **Contest** (standaard).

1. Plaats `atv-contestnummer.exe` in een map waarin je mag schrijven en start het programma.
2. Vul je roepnaam en Maidenheadlocator in. Locators van 4, 6, 8, 10 en 12 tekens worden geaccepteerd. Zet **Locator in beeld** uit als je geen locator wilt tonen; het veld mag dan leeg blijven.
3. Kies **Automatisch** of **Zelf intypen**. Bij automatisch staat er meteen na het openen een code in het voorbeeld. Met **Nieuw nummer** kies je een andere code. Ook bij omschakelen van zelf intypen naar automatisch verschijnt direct een nieuwe code. Exporteren bewaart precies de zichtbare code en maakt geen nieuwe code aan. Bij zelf intypen zijn precies vier cijfers vereist; vier gelijke cijfers en volledige oplopende of aflopende reeksen zijn niet toegestaan. Voorloopnullen blijven behouden.
4. Kies beeldverhouding, resolutie en frequentieband. Het voorbeeld volgt de invoer; de automatische code is al vóór het exporteren zichtbaar.
5. Klik op **Exporteer JPG**. Het bestand wordt naast de `.exe` opgeslagen als `roepnaam-locator-code-band-breedtexhoogte.jpg`, bijvoorbeeld `PE1ITR-JO21QK-1957-436MHz-1920x1440.jpg`. Bij een bestaand bestand vraagt het programma of je het wilt vervangen.

Een `/` in een roepnaam wordt uitsluitend in de bestandsnaam vervangen door `_`. Op het beeld blijft de slash staan. De uitvoer is witte, vetgedrukte tekst op zwart: roepnaam bovenaan, de vier cijfers groot in het midden en de optionele locator eronder. **De gekozen band staat altijd rechtsonder**, ook zonder locator. Met **Inverse (kleuren omwisselen)** worden alle letters, inclusief de band, zwart op een witte achtergrond. Het voorbeeld past zich direct aan. Inverse bestanden krijgen het achtervoegsel `-inverse`, bijvoorbeeld `PE1ITR-JO21QK-1957-436MHz-1920x1440-inverse.jpg`, zodat beide versies naast elkaar kunnen bestaan. Met **Cijfersom in beeld** verschijnt linksonder `Som=22` bij code `1957` (1 + 9 + 5 + 7). Deze optie staat standaard uit en werkt ook zonder locator. Bij een onvolledige of ongeldige code blijft de som in het voorbeeld verborgen. De band en de som gebruiken dezelfde lettergrootte: 6% van de beeldhoogte. Op de kleinste resoluties wordt de beschikbare regelhoogte benut voor leesbaarheid. De bestandsnaam blijft bij deze optie hetzelfde; bij opnieuw opslaan vraagt het programma of het bestaande bestand vervangen mag worden. Met **Code rechtsboven (DATV)** verschijnt dezelfde viercijferige code ook klein rechtsboven, zodat die zichtbaar blijft wanneer alleen de bovenkant van het beeld wordt ontvangen. De optie staat standaard uit en volgt de inverse-instelling. De kleine code gebruikt dezelfde lettergrootte als band en som. De roepnaam krijgt bij deze optie ruimte onder de extra regel om overlap te voorkomen. Voorloopnullen blijven behouden; bij een ongeldige of onvolledige code blijft de extra code verborgen. De bestandsnaam verandert niet door deze optie. De letters schalen met de resolutie en worden bij lange tekst passend gemaakt.

Met **Blauw/geel** krijgt het contestbeeld een donkerblauwe achtergrond (`#000080`) en helder gele tekst (`#FFFF00`). De kleuren hebben een groot helderheidsverschil voor zwart-witontvangst. **Inverse** wisselt ze om: donkerblauwe tekst op geel. Dit geldt ook voor locator, band, cijfersom en de extra DATV-code; het voorbeeld volgt direct. De optie staat standaard uit en is niet beschikbaar bij PM5544. Gekleurde bestanden krijgen `-blauw-geel` vóór het eventuele `-inverse`, bijvoorbeeld `PE1ITR-JO21QK-1957-436MHz-1920x1440-blauw-geel-inverse.jpg`.

De volledige locator verschijnt in het beeld; de bestandsnaam gebruikt maximaal de eerste zes tekens. Bijvoorbeeld: `JO21QK86DW12` wordt `JO21QK` in de naam. Dit geldt ook voor PM5544 en **Exporteren naar...**. Een ingevulde locator komt ook in de bestandsnaam als **Locator in beeld** uit staat. Bij een leeg veld wordt het locatoronderdeel weggelaten; een ingevulde ongeldige locator moet eerst worden gecorrigeerd.

Voor roverstations verandert bij **Automatisch** het contestnummer wanneer een andere geldige locator van minstens zes tekens wordt ingevoerd en de eerste zes tekens verschillen. Extra precisie binnen hetzelfde vak verandert het nummer niet. Onvolledige invoer tijdens het typen verandert het nummer evenmin. Bij **Zelf intypen** blijft het nummer ongewijzigd. Er wordt geen geschiedenis per locatie of band bewaard.

Automatische codes volgen deze regels: eerste cijfer 1–9, vier verschillende cijfers, en geen naast elkaar staande cijfers die precies één verschillen. Handmatig ingevoerde codes bestaan uit vier cijfers, maar mogen niet alle vier gelijk zijn (2222) of een volledige oplopende/aflopende reeks vormen (4567, 5432). Codes zoals 1122 en 0195 zijn toegestaan. Ongeldige codes blokkeren JPG-export, TS-export en het openen van de UDP-uitvoer. De generator bewaart geen codehistorie; een code kan bij een latere generatie opnieuw voorkomen.

| 4:3 | 16:9 |
| --- | --- |
| 120 × 90 | 120 × 68 |
| 160 × 120 | 160 × 90 |
| 320 × 240 | 320 × 180 |
| 640 × 480 | 640 × 360 |
| 800 × 600 | 800 × 450 |
| 1024 × 768 | 960 × 540 |
| 1080 × 810 | 1024 × 576 |
| 1280 × 960 | 1280 × 720 |
| 1600 × 1200 | 1600 × 900 |
| 1920 × 1440 | 1920 × 1080 |

Bij 120 pixels breed is de 16:9-hoogte afgerond van 67,5 naar 68 pixels. De kleine resoluties zijn beschikbaar in beide beeldmodi.

Bandkeuzes: 50 MHz, 70 MHz, 144 MHz, 436 MHz, 1152 MHz, 2330 MHz, 3.4 GHz, 5.7 GHz, 10 GHz, 24 GHz en 47 GHz. Dit zijn beeldlabels; het programma bestuurt geen zender. De lijst staat in `src/core.c`.

## Gebruik: PM5544

1. Kies bovenaan bij **Beeldtype** voor **PM5544**.
2. Vul je roepnaam en locator in. Beide zijn in deze modus verplicht.
3. Kies **4:3** of **16:9** en de gewenste resolutie. Het bijbehorende testbeeld verschijnt direct in het voorbeeld.
4. Klik op **Exporteer JPG**. De uitvoer komt naast het programma, bijvoorbeeld `PE1ITR-JO21QK-PM5544-1920x1440.jpg`. Een slash wordt in de bestandsnaam een underscore.

De roepnaam staat in het bovenste zwarte vlak en de locator in het onderste. Een eigen ingebouwd 5×7-bloklettertype geeft de tekst een jaren-tachtiguitstraling, op beide platforms identiek. Lange tekst wordt verkleind zodat deze binnen het zwarte vlak blijft.

Contestcode, automatische nummerkeuze, band, inverse, blauw/geel, cijfersom, extra DATV-code en het locatorvinkje zijn in deze modus uitgeschakeld. De locator wordt altijd getoond. Bij terugschakelen naar Contest zijn je eerdere instellingen weer beschikbaar.

De PM5544-bronbeelden staan in `assets/pm5544.jpg` en `assets/pm5544w.jpg`. Het 720×576-beeld wordt naar de gekozen 4:3-resolutie geschaald; het 1280×720-beeld naar de gekozen 16:9-resolutie. De bronpixels worden tijdens het bouwen verliesloos als RGB-runs ingebouwd. **Geen losse testbeelden, fontbestanden of Python nodig bij het gebruiken van het programma.** Verspreid alleen het programma voor het gewenste platform (Linux gebruikt wel de eerder genoemde systeembibliotheken).

## DATV-proefbestand (Genius level 2)

Kies **Config → Genius level 2**, stel het beeld in op maximaal **640 × 480** en
kies **File → Exporteer TS-proefbestand...**. Kies DVB-S, DVB-S2 of DVB-T en de
zendparameters; de totale TS-bitrate in **bit/s** wordt berekend. Stel daarnaast
beeldduur, beelden/s en GOP-lengte in (1 = alleen IDR-beelden).
De export bevat H.264-video zonder audio; servicenaam en provider volgen de
roepnaam, service-ID is 1. Het nummer verandert niet bij export.

De encoder OpenH264 en de eigen TS-muxer zijn ingebouwd, zonder FFmpeg of losse
codec-DLL. Via **Info → OpenH264-licentie** is de bibliotheeklicentie beschikbaar.
**File → DATV UDP-uitvoer...** opent IP-adres, poort (standaard 10000), TS-bitrate,
beelden/s en GOP met **Start/Stop**. Start zendt het huidige beeld doorlopend;
sluiten stopt ook. Gebruik een numeriek unicast IPv4-adres. De UDP-instellingen
starten met 10 beelden/s en GOP 2. Voor een ander beeld: Stop, venster sluiten,
beeld aanpassen en opnieuw Start. Permanente nummeropslag per band volgt later.
Zie [DATV-testinstructies en technische grenzen](docs/datv.md).

De UDP-stream blijft doorlopen als de doelpoort tijdelijk weigert (Linux-code
111), bijvoorbeeld wanneer de Portsdown-IPTS-ingang nog niet gestart is.
Een recente poortweigering verschijnt kort als uitleg, zonder foutcode of teller.
Na drie seconden zonder nieuwe weigering verdwijnt die melding. Start de IPTS-ingang
of IPTS Viewer op de Portsdown om het actuele beeld op te pakken; oude pakketten
worden niet opnieuw verstuurd. De pakketteller telt lokaal geslaagde verzendingen,
geen bevestigde ontvangst. Andere netwerkfouten stoppen de stream nog steeds.

De resolutielijst bevat ook 240 pixels breed: 240 × 180 (4:3) en 240 × 136
(16:9, afgerond op een even hoogte voor H.264). Ze staan in de oplopende lijst
tussen 160 en 320 pixels breed.

TS-export en UDP hebben dezelfde bitrateberekening met **QPSK**:

- **DVB-S/S2:** symbol rates **35, 66, 125, 150, 333 en 500 ksym/s**.
- **FEC:** **1/2, 2/3 en 3/4**.
- **DVB-S2:** normale frames, **pilots aan/uit**.
- **DVB-T:** **2K**, **SR/BW (Portsdown) 150k, 250k, 333k en 500k**,
  met **GI 1/8, 1/16 of 1/32**. De Portsdown-SR/BW-waarde betekent hier
  bandbreedte in kHz.

De getoonde bitrate is alleen-lezen en verandert direct met de parameters.
Tijdens uitzending zijn deze instellingen geblokkeerd. De berekening stelt de
modulator niet op afstand in: kies op Portsdown dezelfde waarden. Standaard is
DVB-S, 125 ksym/s, FEC 1/2: **115196 bit/s**. Bij DVB-S2 is dat **123607 bit/s**
zonder pilots en **120665 bit/s** met pilots. Zie
[formules en vergelijking met Portsdown](docs/dvb-bitrate.md).

Zendparameters worden afzonderlijk voor TS en UDP opgeslagen. Oude configuraties
blijven leesbaar. Bekende S/S2-bitrates worden herkend; bij een oude handmatige
waarde zonder exacte overeenkomst starten de keuzelijsten met de standaard.
Bij het openen van een TS-/UDP-venster verschijnt de berekende waarde; pas Verder,
Toepassen of Start neemt die over. Annuleren/sluiten en Beeld controleren bewaren
geen gewijzigde zendparameters. UDP start nooit automatisch.

## Bouwen

De eerste build downloadt circa 58 MB OpenH264-broncode en controleert de vaste
SHA-256. De download wordt bewaard in `build/downloads/`; volgende builds kunnen
offline werken. `tools/prepare_openh264.py` bouwt de bronmappen voor Linux en Windows
afzonderlijk op. De bibliotheek wordt statisch gelinkt. De applicatie blijft C;
de encoder/muxer-module gebruikt C++17.

Op Linux met C- en C++-compilers, MinGW-w64 (ook g++), NASM, `pkg-config`, Python 3.12+ met Pillow en de ontwikkelpakketten voor GTK 3 en Pango/Cairo:

```sh
make
```

`make` bouwt zowel `dist/atv-contestnummer` als `dist/atv-contestnummer.exe`. `make linux` en `make windows` zijn beschikbaar voor gerichte tussentijdse builds; lever bij afronding altijd beide bestanden. Op Fedora zijn de ontwikkelpakketten onder meer `gcc`, `make`, `pkgconf-pkg-config`, `gtk3-devel` en `mingw64-gcc`; op Debian/Ubuntu `build-essential`, `pkg-config`, `libgtk-3-dev` en `gcc-mingw-w64-x86-64`.

Op Windows vanuit een MSYS2/MinGW-w64-omgeving (`make`, `sh`, `gcc`, `g++`, `ar`, `windres` en `nasm` in `PATH`) en Python 3.12+ met Pillow (`python` in `PATH`): `build-windows.bat` bouwt alleen de Windows-versie. Gebruik voor de volledige levering daarnaast `make` in een Linux-omgeving, bijvoorbeeld WSL met bovengenoemde afhankelijkheden. Alle dynamische imports van de Windows-exe zijn Windows-systeembibliotheken.

De JPG-encoder gebruikt [Windows Imaging Component](https://learn.microsoft.com/en-us/windows/win32/wic/-wic-creating-encoder) met kwaliteit 98%. Het programma schrijft eerst een tijdelijk bestand in de uitvoermap en vervangt het doelbestand pas na geslaagde compressie.

Linux gebruikt GdkPixbuf voor JPG-compressie, eveneens met kwaliteit 98%, en schrijft ook eerst naar een tijdelijk bestand voordat de definitieve naam wordt geplaatst.

De configuratiebestandscontroles draaien mee met `make test-linux`. De venstertests
voor opslaan/heropenen en UDP-instellingen toepassen zonder uitzending bouw je met:

```sh
make build/test-config-linux-ui build/test-config-windows-ui.exe build/test-config.exe
build/test-config-linux-ui
wine build/test-config.exe
wine build/test-config-windows-ui.exe
```

Voer de venstertests uit in een grafische sessie. Ze gebruiken tijdelijke bestanden;
de Windows-venstertest gebruikt een configuratie naast het testprogramma in `build/`
en weigert een reeds aanwezig configuratiebestand te overschrijven.

Met **EBU boven** en **EBU onder** schakel je onafhankelijk smalle EBU-kleurenstroken
in het contestbeeld in. Elke strook beslaat 9% van de beeldhoogte, over de volle
breedte, tot aan de roepnaam of vanaf de locator. De hoekcode, frequentieband en
cijfersom blijven leesbaar met een achtergrond in de gekozen beeldkleur.
De stroken werken in het voorbeeld, JPG en DATV (TS/UDP), en worden via Config
mee opgeslagen. In PM5544-modus zijn deze opties uitgeschakeld.
Voor een TS-rendercontrole met beide stroken gebruik je `--ts-ebu-test` met
dezelfde argumenten als `--ts-test`.

## FUBK

Kies **FUBK** bij **Beeldtype** voor het bijbehorende bronbeeld in 4:3 of 16:9.
In de zwarte middenbalk staat links de roepnaam en rechts de locator, met het
zelfde ingebouwde bloklettertype als PM5544. FUBK vereist een geldige locator
van minimaal zes tekens en toont de eerste zes; een langere invoer blijft bewaard.
De contestopties zijn bij FUBK uitgeschakeld. Het voorbeeld, JPG-export en DATV
(TS/UDP) gebruiken hetzelfde beeld. Bestandsnamen bevatten `FUBK`; de keuze
wordt via **Config → Huidige instellingen opslaan** bewaard.

De aangeleverde bestanden `assets/FuBK-Testbild.png` en `assets/FuBK_wide.jpg`
worden tijdens bouwen ingebouwd, zonder losse runtime-bestanden. De plaatsing
volgt het [WDR-voorbeeld](https://pe1itr.com/tv-dx/vhf-h/VHFCHE11_WDR1%20teutoburger%20wald.htm).
Met `--ts-fubk-test` en dezelfde argumenten als `--ts-test` maak je een TS-test
met het FUBK-beeld.

## Leesbaarheid vóór het uitzenden beoordelen

Voor DATV-contests raden we **wit op zwart** aan. Bij de onderzochte
160 × 120-kaart besloeg een zelfstandig beeld in het berekende DVB-S2-model
gemiddeld circa 21% minder frames dan geel op blauw. Dit is geen gemeten
ontvangstwinst; bij 2–3 seconden goede ontvangst kunnen beide bruikbaar zijn.
Zie [kleurkeuze, metingen en betekenis bij korte reflecties](docs/contest-kleurkeuze.md)
voor de instellingen, berekening en beperkingen.

Open **DATV: UDP-uitvoer** (via Genius level 2), stel zendparameters, beelden per seconde
en GOP in en klik **Beeld controleren**. Je hoeft hiervoor geen IP-adres of
poort in te vullen. Links staat het originele beeld; rechts hetzelfde beeld na
H.264-compressie en decodering. Beide beelden gebruiken dezelfde vergroting;
met **2× vergroten** en de gekoppelde schuifbalken vergelijk je kleine letters.
De controle toont ook de berekende uitzendtijd van het eerste volledige beeld
in milliseconden vanaf TS-start. Deze tijd volgt uit de werkelijk gecodeerde
beeldgrootte en TS-planning bij de ingestelde bitrate, inclusief tussenliggende
TS-overhead. Het is geen beeldinterval (1000/fps) of tijd tot zichtbare ontvangst:
UDP-pacing, netwerkvertraging, wachten op een volgend IDR bij later afstemmen en
buffering/decodering in de ontvanger zijn niet inbegrepen.
Voor vliegtuigreflecties toont het venster ook de IDR-interval (1000 × GOP / fps)
en een ruwe schatting van wachten op een IDR plus de uitzendtijd van
het eerste beeld. Dit is een indicatie, geen ontvangstgarantie: latere IDR-beelden
en de tussenliggende TS-overhead kunnen verschillen. Signaalvergrendeling,
ontvangstverliezen en ontvangervertraging vragen extra tijd.
Sluit de vergelijking om de instellingen te veranderen en opnieuw te controleren.

De controle verstuurt niets en wijzigt geen opgeslagen instellingen. Ze gebruikt
precies dezelfde kwaliteitskeuze en controle van de beschikbare bitrate als de
UDP-uitzending. De getoonde QP is extra informatie, geen oordeel over leesbaarheid:
resolutie, lettergrootte, beeldinhoud, bitrate, beeldfrequentie en GOP tellen mee.

Rechts zie je het eerste volledige videobeeld. Andere videobeelden kunnen
verschillen; ontvangstverliezen en beeldbewerking door de ontvanger worden niet
nagebootst. De ingebouwde OpenH264-decoder vereist geen extra installatie.

De vergelijkingstests zijn beschikbaar met:

```sh
make build/test-quality-linux-ui build/test-quality-windows-ui.exe
build/test-quality-linux-ui
wine build/test-quality-windows-ui.exe
```


## PM5644 uit EPROM-afgeleide beelddata

Kies **PM5644** bij **Beeldtype**. Voor 4:3 gebruikt het programma de Philips
PM5644 G00; voor 16:9 de G924. Roepnaam en volledige locator verschijnen in
hun oorspronkelijke zwarte naamvelden. Contestopties zijn uitgeschakeld.
JPG-export, kwaliteitsvoorbeeld, TS-export en UDP gebruiken hetzelfde beeld.
De bestandsnaam bevat `PM5644`, bijvoorbeeld
`PE1ITR-JO21QK-PM5644-320x240.jpg`. De keuze wordt in de configuratie bewaard;
bestaande keuzes behouden hun opgeslagen nummer.

De bron is de EPROM-afgeleide YCbCr-beelddata uit PAL-sign, **geen JPG of opname
van de analoge uitgang**. De build decodeert de meegeleverde tabellen naar RGB
zonder JPEG-tussenstap. Beide uitvoeringen gebruiken variant 2 zonder
klok-/datuminserts. Zie [herkomst, verwerking en licentie](assets/pm5644/README.md).
Alle data worden ingebouwd in zowel Linux als de zelfstandige Windows-exe.
De meegeleverde tabellen volstaan; de PAL-sign-checkout is niet nodig.

De uitgangsrasters zijn 720×576 met verschillende weergaveverhoudingen. Schalen
naar de gekozen resolutie en JPEG/H.264-compressie kunnen de fijne testpatronen
veranderen; dit is geen gekalibreerde analoge PAL-signaalgenerator.
`--ts-pm5644-test` accepteert dezelfde argumenten als `--ts-test` en maakt een
PM5644-proefbestand zonder GUI of netwerkuitzending.
