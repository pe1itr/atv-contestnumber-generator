# Projectafspraken

- Lever bij wijzigingen altijd zowel een Linux-build (`dist/atv-contestnummer`) als een Windows-build (`dist/atv-contestnummer.exe`). Dit is een expliciete gebruikerswens.
- `make` bouwt beide platforms. Gebruik `make test-linux` voor de gedeelde logica en de Linux-JPG-integratietest. Controleer gewijzigde Windows-functionaliteit ook via Wine als dat beschikbaar is; vermeld wanneer een platform niet kon worden getest.
- Houd invoervelden, bandkeuzes, coderegels, bestandsnamen en beeldindeling gelijk op beide platforms. Gebruik `src/core.c` voor gedeelde regels en keuzelijsten.
- De Windows-versie blijft één verspreidbare `.exe` zonder losse meegeleverde afhankelijkheden. De Linux-versie gebruikt GTK 3, Pango/Cairo en GdkPixbuf.
- PM5544-bronbeelden staan in `assets/` en worden met `tools/embed_pm5544.py` ingebouwd; deze bestanden en het ingebouwde bloklettertype mogen geen runtime-afhankelijkheden worden. Gebruik de gedeelde renderer in `src/pm5544.c` op beide platforms.
- Communiceer met de gebruiker in het Nederlands.

## Verplichte UDP/MPEG-TS-review

- Start bij iedere wijziging die de TS- of UDP-uitvoer kan beïnvloeden de subagent `udp_ts_validator`. Dit geldt ook voor encoderinstellingen, bitrate, timing, PSI/SI, configuratie, platformcode, afhankelijkheden en de streamtests zelf.
- De agent volgt `docs/agents/udp-ts-validator.md`. Geef hem de wijzigingsdiff, het beoogde streamprofiel en beschikbare testresultaten. Als de omgeving geen benoemde custom agents ondersteunt, start een gewone subagent met dezelfde taakbeschrijving. Zijn subagents niet beschikbaar, voer de beschreven review zelf uit en vermeld die beperking.
- Laat de review de definitieve code en uitvoer beoordelen vóór afronding; laat relevante correcties opnieuw beoordelen. Rapporteer afwijkingen en ontbrekende verificatie expliciet. Geslaagde regressietests alleen zijn geen bewijs van volledige ETSI-conformiteit.
- Deze instructie start geen permanente achtergrondbewaking of CI-taak; zij verplicht de uitvoerende agent om bij relevante wijzigingen de review uit te voeren.
