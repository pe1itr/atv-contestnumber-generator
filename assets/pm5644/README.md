# PM5644 EPROM-afgeleide beeldtabellen

Bron: https://github.com/PE5PVB/PAL-sign
Commit: `e42b295095a3bb28424c6561306d01e9916ed9dc`
Bestanden: `Firmware/src/testcards/pm5644g00_*.inc.h` en
`Firmware/src/testcards/pm5644g924_*.inc.h` (pal, idx, len, start, line).
De bestanden zijn ongewijzigd gekopieerd; SHA256.json legt de hashes vast.
PAL-sign vermeldt GPL versie 3 of later; de meegeleverde LICENSE is een
ongewijzigde kopie. Auteur PAL-sign: PE5PVB.

Dit zijn geen ruwe EPROM-dumps. PAL-sign heeft de dumps uit PhilipsPatternRom
naar BT.601 YCbCr 4:2:2 omgezet, chroma uitgelijnd en als gepaletteerde runs
opgeslagen. G924 is bovendien naar het 720x576-raster herschaald en in
luminantieniveaus gereduceerd. Zie het bronbestand
`Firmware/tools/convert_rom_pattern.py` in bovengenoemde commit.
De licentievermelding van PAL-sign is geen afzonderlijke verklaring over
rechten op de oorspronkelijke Philips-ROM-inhoud.

Onze build decodeert variant 2 (zonder datum-/klokinserts) met
`tools/pm5644_rom.py`, zet studio-range BT.601 om naar full-range RGB en
bouwt dit verliesloos in via `tools/embed_pm5544.py`. Er is geen JPEG-tussenstap.
G00 wordt als 4:3 weergegeven, G924 als anamorf 16:9; beide rasters zijn
720x576. De rijvolgorde blijft behouden. De oorspronkelijke PAL-testvelden
blijven onderdeel van het beeld, maar schalen, RGB-conversie en H.264/JPEG
maken hiervan geen gekalibreerde analoge PAL-meetbron.

De afbeeldingen en tabellen zijn alleen tijdens het bouwen nodig. Beide
programma's bevatten de beelddata; voor Windows blijft één exe voldoende.
