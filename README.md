# ATV contestnummer generator

Een programma in C voor Windows en Linux dat een contestkaart of PM5544-testbeeld als JPG maakt. Er worden standaard **altijd beide builds** gemaakt.

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

**Info → Over dit programma** toont het doel, uitleg over het gebruik, de auteur, het versienummer en de compilatiedatum. De huidige versie is **1.1.0**. Versie en auteur staan centraal in `src/app_info.h`; de datum wordt tijdens compilatie vastgelegd en is niet de datum waarop je het programma start.

## Gebruik: Contest

Kies bovenaan bij **Beeldtype** voor **Contest** (standaard).

1. Plaats `atv-contestnummer.exe` in een map waarin je mag schrijven en start het programma.
2. Vul je roepnaam en Maidenheadlocator in. Locators van 4, 6, 8 en 10 tekens worden geaccepteerd. Zet **Locator in beeld** uit als je geen locator wilt tonen; het veld mag dan leeg blijven.
3. Kies **Automatisch** of **Zelf intypen**. Bij automatisch staat er meteen na het openen een code in het voorbeeld. Met **Nieuw nummer** kies je een andere code. Ook bij omschakelen van zelf intypen naar automatisch verschijnt direct een nieuwe code. Exporteren bewaart precies de zichtbare code en maakt geen nieuwe code aan. Bij zelf intypen zijn precies vier cijfers vereist; voorloopnullen blijven behouden.
4. Kies beeldverhouding, resolutie en frequentieband. Het voorbeeld volgt de invoer; de automatische code is al vóór het exporteren zichtbaar.
5. Klik op **Exporteer JPG**. Het bestand wordt naast de `.exe` opgeslagen als `roepnaam-code-band-breedtexhoogte.jpg`, bijvoorbeeld `PE1ITR-1957-436MHz-1920x1440.jpg`. Bij een bestaand bestand vraagt het programma of je het wilt vervangen.

Een `/` in een roepnaam wordt uitsluitend in de bestandsnaam vervangen door `_`. Op het beeld blijft de slash staan. De uitvoer is witte, vetgedrukte tekst op zwart: roepnaam bovenaan, de vier cijfers groot in het midden en de optionele locator eronder. **De gekozen band staat altijd rechtsonder**, ook zonder locator. Met **Inverse (kleuren omwisselen)** worden alle letters, inclusief de band, zwart op een witte achtergrond. Het voorbeeld past zich direct aan. Inverse bestanden krijgen het achtervoegsel `-inverse`, bijvoorbeeld `PE1ITR-1957-436MHz-1920x1440-inverse.jpg`, zodat beide versies naast elkaar kunnen bestaan. Met **Cijfersom in beeld** verschijnt linksonder `de som is 22` bij code `1957` (1 + 9 + 5 + 7). Deze optie staat standaard uit en werkt ook zonder locator. Bij een onvolledige of ongeldige code blijft de som in het voorbeeld verborgen. De band en de som gebruiken dezelfde lettergrootte: 6% van de beeldhoogte. Op de kleinste resoluties wordt de beschikbare regelhoogte benut voor leesbaarheid. De bestandsnaam blijft bij deze optie hetzelfde; bij opnieuw opslaan vraagt het programma of het bestaande bestand vervangen mag worden. Met **Code rechtsboven (DATV)** verschijnt dezelfde viercijferige code ook klein rechtsboven, zodat die zichtbaar blijft wanneer alleen de bovenkant van het beeld wordt ontvangen. De optie staat standaard uit en volgt de inverse-instelling. De kleine code gebruikt dezelfde lettergrootte als band en som. De roepnaam krijgt bij deze optie ruimte onder de extra regel om overlap te voorkomen. Voorloopnullen blijven behouden; bij een ongeldige of onvolledige code blijft de extra code verborgen. De bestandsnaam verandert niet door deze optie. De letters schalen met de resolutie en worden bij lange tekst passend gemaakt.

Met **Blauw/geel** krijgt het contestbeeld een donkerblauwe achtergrond (`#000080`) en helder gele tekst (`#FFFF00`). De kleuren hebben een groot helderheidsverschil voor zwart-witontvangst. **Inverse** wisselt ze om: donkerblauwe tekst op geel. Dit geldt ook voor locator, band, cijfersom en de extra DATV-code; het voorbeeld volgt direct. De optie staat standaard uit en is niet beschikbaar bij PM5544. Gekleurde bestanden krijgen `-blauw-geel` vóór het eventuele `-inverse`, bijvoorbeeld `PE1ITR-1957-436MHz-1920x1440-blauw-geel-inverse.jpg`.

Automatische codes volgen deze regels: eerste cijfer 1–9, vier verschillende cijfers, en geen naast elkaar staande cijfers die precies één verschillen. Handmatig ingevoerde codes hoeven alleen uit vier cijfers te bestaan. De generator bewaart geen codehistorie; een code kan bij een latere generatie opnieuw voorkomen.

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
4. Klik op **Exporteer JPG**. De uitvoer komt naast het programma, bijvoorbeeld `PE1ITR-PM5544-1920x1440.jpg`. Een slash wordt in de bestandsnaam een underscore.

De roepnaam staat in het bovenste zwarte vlak en de locator in het onderste. Een eigen ingebouwd 5×7-bloklettertype geeft de tekst een jaren-tachtiguitstraling, op beide platforms identiek. Lange tekst wordt verkleind zodat deze binnen het zwarte vlak blijft.

Contestcode, automatische nummerkeuze, band, inverse, blauw/geel, cijfersom, extra DATV-code en het locatorvinkje zijn in deze modus uitgeschakeld. De locator wordt altijd getoond. Bij terugschakelen naar Contest zijn je eerdere instellingen weer beschikbaar.

De PM5544-bronbeelden staan in `assets/pm5544.jpg` en `assets/pm5544w.jpg`. Het 720×576-beeld wordt naar de gekozen 4:3-resolutie geschaald; het 1280×720-beeld naar de gekozen 16:9-resolutie. De bronpixels worden tijdens het bouwen verliesloos als RGB-runs ingebouwd. **Geen losse testbeelden, fontbestanden of Python nodig bij het gebruiken van het programma.** Verspreid alleen het programma voor het gewenste platform (Linux gebruikt wel de eerder genoemde systeembibliotheken).

## Bouwen

Op Linux met een C-compiler, MinGW-w64, `pkg-config`, Python 3 met Pillow en de ontwikkelpakketten voor GTK 3 en Pango/Cairo:

```sh
make
```

`make` bouwt zowel `dist/atv-contestnummer` als `dist/atv-contestnummer.exe`. `make linux` en `make windows` zijn beschikbaar voor gerichte tussentijdse builds; lever bij afronding altijd beide bestanden. Op Fedora zijn de ontwikkelpakketten onder meer `gcc`, `make`, `pkgconf-pkg-config`, `gtk3-devel` en `mingw64-gcc`; op Debian/Ubuntu `build-essential`, `pkg-config`, `libgtk-3-dev` en `gcc-mingw-w64-x86-64`.

Op Windows met MinGW-w64 (`gcc` en `windres` in `PATH`) en Python 3 met Pillow (`python` in `PATH`): `build-windows.bat` bouwt alleen de Windows-versie. Gebruik voor de volledige levering daarnaast `make` in een Linux-omgeving, bijvoorbeeld WSL met bovengenoemde afhankelijkheden. Alle dynamische imports van de Windows-exe zijn Windows-systeembibliotheken.

De JPG-encoder gebruikt [Windows Imaging Component](https://learn.microsoft.com/en-us/windows/win32/wic/-wic-creating-encoder) met kwaliteit 98%. Het programma schrijft eerst een tijdelijk bestand in de uitvoermap en vervangt het doelbestand pas na geslaagde compressie.

Linux gebruikt GdkPixbuf voor JPG-compressie, eveneens met kwaliteit 98%, en schrijft ook eerst naar een tijdelijk bestand voordat de definitieve naam wordt geplaatst.
