# Kleurkeuze voor een DATV-contestbeeld

Voor het overbrengen van een leesbaar contestnummer raden we **witte letters
op een zwarte achtergrond** aan. Dit vraagt in de onderzochte beelden minder
gecomprimeerde beeldgegevens dan gele letters op een blauwe achtergrond.
Schakel hiervoor **Blauw/geel** en **Inverse** uit. De aanbeveling staat ook
in het hoofdvenster van beide programmaversies.

Het doel van deze vergelijking is een compleet, leesbaar beeld binnen een kort
ontvangstvenster, bij een **vaste symbolrate van 125 ksym/s**. Minder benodigde
beeldgegevens betekent daarbij niet minder uitgezonden symbolen of een lagere
totale TS-bitrate. De beschikbare ruimte wordt door de muxer opgevuld.

## Meting op 160 × 120

Onderzocht op 23 september 2026 met de Linux-renderer en de gedeelde
OpenH264-encoder en TS-muxer. Alleen het kleurenpalet verschilde:

- Roepnaam `PE1ITR`, contestcode `3971`, locator `JO21QK`, band `436 MHz`.
- Locator, cijfersom en kleine code rechtsboven ingeschakeld.
- EBU-kleurenbalken, EIT en teletekst uitgeschakeld.
- Wit `RGB(255,255,255)` op zwart `RGB(0,0,0)` tegenover geel
  `RGB(255,255,0)` op donkerblauw `RGB(0,0,128)`.
- Resolutie 160 × 120, 4 beelden/s, GOP 2.
- DVB-S2, QPSK, FEC 1/2, normale 64.800-bit FECFRAMEs, pilots aan,
  125.000 symbolen/s: berekende TS-bitrate **120.665 bit/s**.
- Beide kleuren kregen automatisch **QP 24**. Dit is dezelfde
  encoderinstelling, geen bewijs van dezelfde waargenomen beeldkwaliteit.

Per kleur zijn via lokale UDP-loopback 100 datagrammen van 1.316 bytes
opgevangen: **131.600 TS-bytes**. Voor de vergelijking zijn de eerste
17 complete zelfstandige videobeelden (IDRs) onderzocht.

| Grootheid per IDR | Wit op zwart | Geel op blauw |
|---|---:|---:|
| H.264-beeldgegevens, inclusief AVC-headers | 2.682 bytes | 4.173 bytes |
| TS-pakketten met IDR-payload | 15 | 24 |
| Aaneengesloten pakketbereik, inclusief tussenliggend verkeer | 15–18 pakketten | 26–29 pakketten |
| Dat pakketbereik in bytes | 2.820–3.384 | 4.888–5.452 |
| Berekende DVB-S2-frames die dat bereik raakt | **1–2** | **2–3** |
| Gemiddelde over alle byte-uitlijningen en de 17 IDRs | **1,792814** | **2,274710** |

De IDR-beeldgegevens zijn **35,73% kleiner**. Het volledige IDR-pakketbereik
beslaat in het frame-indelingsmodel gemiddeld **21,18% minder DVB-S2-frames**:

`100 × (1 − 1,792814037965784 / 2,2747099835950317) = 21,18494%`.

Dit percentage geldt voor dit beeld, deze instellingen en het onderstaande
model. Het is geen universeel percentage voor zwart-witbeelden.

## Van TS-pakketten naar DVB-S2-frames

Bij het onderzochte profiel bevat een frame een DATAFIELD van
`(32.208 − 80) / 8 = 4.016 bytes`. De 80 bits zijn de BBHEADER. Een fysiek
frame bevat `32.400 + 90 + 22 × 36 = 33.282 symbolen`, inclusief PLHEADER
en pilots. Bij 125 ksym/s duurt dat **266,256 ms**.

Deze waarden volgen uit [ETSI EN 302 307-1 V1.4.1](https://www.etsi.org/deliver/etsi_en/302300_302399/30230701/01.04.01_60/en_30230701v010401p.pdf),
§5.1.4–5.1.6 en tabel 4, §5.3 en tabel 5a, en §5.5. Het model gebruikt
volledig gevulde DATAFIELDs, zonder ISSY of null-pakketverwijdering. De
CRC-8 vervangt de TS-syncbyte; dit levert geen besparing van één byte per
188-byte TS-pakket op. Zie ook [de bitrateberekening](dvb-bitrate.md).

Een IDR begint niet noodzakelijk op een DVB-framegrens. Daarom is alleen
`beeldbytes / 4.016`, naar boven afgerond, onvoldoende. De meting telt het
hele bereik van het eerste tot en met het laatste TS-pakket met IDR-payload,
inclusief PES/TS-overhead en tussenliggende SI/PCR-pakketten.

Met `start` als bytepositie van het eerste pakket, `end` als de exclusieve
eindpositie van het laatste pakket en `fase` van 0 t/m 4.015 is het aantal:

`floor((end − 1 + fase) / 4.016) − floor((start + fase) / 4.016) + 1`.

Alle 4.016 bytefasen en alle 17 IDRs wegen even zwaar in het gemiddelde.
De gemeten verdeling maakt de berekening ook zonder de tijdelijke captures
opnieuw uitvoerbaar:

| Lengte van het IDR-pakketbereik | Aantal wit/zwart | Aantal geel/blauw |
|---|---:|---:|
| 2.820 bytes | 2 | 0 |
| 3.008 bytes | 1 | 0 |
| 3.196 bytes | 10 | 0 |
| 3.384 bytes | 4 | 0 |
| 4.888 bytes | 0 | 8 |
| 5.076 bytes | 0 | 2 |
| 5.264 bytes | 0 | 2 |
| 5.452 bytes | 0 | 5 |

Voor een bereik met lengte `L` is het gemiddelde over alle fasen
`1 + (L − 1) / 4.016`. De werkelijke uitlijning in de modulator is niet
gemeten. Het model telt volledige TS-pakketten; eventueel wachten op de
CRC in het volgende pakket door een ontvanger valt buiten dit model.

## Wat betekent dit bij een reflectie van 2–3 seconden?

Bij 4 beelden/s en GOP 2 begint iedere halve seconde een zelfstandig beeld.
Tijdens 2–3 seconden worden dus ongeveer **4–6 IDR-beelden aangeboden**;
dat garandeert niet dat ze allemaal volledig ontvangen of getoond worden.

Als het signaal gedurende dat venster goed genoeg blijft, kan geel/blauw net
zo bruikbaar zijn. Wit/zwart vraagt per IDR minder DVB-frames en kan daardoor
voordeel hebben bij onderbrekingen of aan de randen van het ontvangstvenster.
Het zegt niet dat de kans op verlies van een afzonderlijk DVB-frame lager is.
Ook blijven locktijd, het inlezen van PAT/PMT, ontvangerbuffers en het
moment waarop het beeld wordt getoond van belang.

**21% minder benodigde frames betekent niet 21% meer geslaagde verbindingen
of 21% kortere tijd tot een leesbaar beeld.** Die ontvangstverbetering vereist
metingen met de zender en ontvanger. De totale RF-framefrequentie blijft bij
dezelfde zendinstellingen gelijk.

## Eerdere vergelijking op 240 × 180

Bij dezelfde tekst en 4 beelden/s, GOP 2, maar 240 × 180 en voor beide kleuren
vast QP 28, was de H.264-videobitrate 61.632 bit/s voor wit/zwart tegenover
91.136 bit/s voor geel/blauw: **32,37% minder videodata**. Dit betreft de
hele herhaalde GOP, niet het aantal DVB-frames per IDR.

Bij automatische kwaliteitskeuze op 120.665 bit/s koos wit/zwart daar QP 24
en geel/blauw QP 28. De besparing kan dus ook worden benut voor minder sterke
compressie. De percentages voor 240 × 180 en 160 × 120 meten verschillende
grootheden en mogen niet door elkaar worden gebruikt.

## Verificatie en grenzen

De projectreviewer `udp_ts_validator` controleerde de meetopzet, captures en
frameberekening onafhankelijk. De bestaande `check_udp.verify`-controles
met FFmpeg, ffprobe en TSDuck slaagden. Dit is geen volledige
MPEG-TS/DVB/ETSI-conformiteitsbeoordeling.

De UDP-captures zijn gemeten; de DVB-frame-indeling is berekend. De fysieke
modulatoruitvoer, ontvangstverliezen en leesbaarheid bij reflecties zijn niet
gemeten. Windows is voor deze kleurmeting niet onderzocht; verschillen in
letterweergave kunnen de compressieresultaten beïnvloeden. Andere codes,
lettergroottes, resoluties, extra testpatronen en zendinstellingen kunnen
andere uitkomsten geven. Gebruik **Beeld controleren** om de leesbaarheid
na compressie te beoordelen; die functie simuleert geen ontvangstverlies.
