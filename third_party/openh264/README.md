OpenH264 2.6.0 van Cisco: https://github.com/cisco/openh264/tree/v2.6.0

De build downloadt de ongewijzigde broncode via `tools/prepare_openh264.py`,
controleert de vastgelegde SHA-256 en bouwt een statische bibliotheek per platform.
Er wordt geen door Cisco voorgebouwde codec geladen of meegeleverd.

`LICENSE` is de originele BSD-broncodelicentie. `license_text.inc` bevat dezelfde
tekst als C-string, met versievermelding, voor Info > OpenH264-licentie in beide
programma's. De licentietekst blijft zo beschikbaar bij verspreiding van alleen
de executable. Bij een bibliotheekupdate moeten beide teksten worden bijgewerkt.
