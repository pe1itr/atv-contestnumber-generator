# Projectafspraken

- Lever bij wijzigingen altijd zowel een Linux-build (`dist/atv-contestnummer`) als een Windows-build (`dist/atv-contestnummer.exe`). Dit is een expliciete gebruikerswens.
- `make` bouwt beide platforms. Gebruik `make test-linux` voor de gedeelde logica en de Linux-JPG-integratietest. Controleer gewijzigde Windows-functionaliteit ook via Wine als dat beschikbaar is; vermeld wanneer een platform niet kon worden getest.
- Houd invoervelden, bandkeuzes, coderegels, bestandsnamen en beeldindeling gelijk op beide platforms. Gebruik `src/core.c` voor gedeelde regels en keuzelijsten.
- De Windows-versie blijft één verspreidbare `.exe` zonder losse meegeleverde afhankelijkheden. De Linux-versie gebruikt GTK 3, Pango/Cairo en GdkPixbuf.
- PM5544-bronbeelden staan in `assets/` en worden met `tools/embed_pm5544.py` ingebouwd; deze bestanden en het ingebouwde bloklettertype mogen geen runtime-afhankelijkheden worden. Gebruik de gedeelde renderer in `src/pm5544.c` op beide platforms.
- Communiceer met de gebruiker in het Nederlands.
