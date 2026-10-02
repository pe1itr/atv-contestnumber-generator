# TS-bitrate uit zendparameters

De calculator in `src/core.c` berekent de capaciteit voor volledige **188-byte
MPEG-TS-pakketten**. PSI/SI, PCR, PES en null-pakketten gebruiken een deel daarvan;
er wordt niet nogmaals een videopercentage of UDP/IP-overhead afgetrokken.
De bestaande encoder/muxer bepaalt vervolgens of het beeld daarin past.
Alle berekeningen gebruiken 64-bit gehele tussenresultaten en ronden pas het
eindresultaat naar beneden af op hele bit/s.

## Ondersteund profiel

- DVB-S: QPSK, SR 25/30/33/35/66/125/150/250/333/500 ksym/s; FEC 1/2, 2/3, 3/4, 5/6, 7/8.
- DVB-S2: dezelfde SR-keuzes; FEC 1/4, 1/3, 1/2, 3/5, 2/3, 3/4, 5/6, 8/9, 9/10.
  Normale 64800-bit FECFRAME, pilots aan/uit,
  CCM en een volledig gevulde DATAFIELD, zonder ISSY of null-packet deletion.
  Short frames en 8PSK zijn niet geïmplementeerd.
- DVB-T: QPSK, 2K, niet-hiërarchisch, FEC 1/2, 2/3, 3/4, 5/6, 7/8 (zoals DVB-S), GI 1/8, 1/16,
  1/32. Portsdown SR/BW 35k/40k/150k/250k/333k/500k staat voor respectievelijk
  35/40/150/250/333/500 kHz kanaalbandbreedte, niet voor OFDM-symbolen per seconde.
  Dit zijn geschaalde amateurbandbreedtes; geen claim dat deze kanalen onder
  de standaard DVB-T-kanaalbreedtes vallen. Bij 35k is FEC 1/2 niet
  beschikbaar: de berekende TS-bitrate ligt onder de programmagrens van
  30080 bit/s. De beschikbare FEC-keuzes beginnen daar bij 2/3.
  Bij 40k is FEC 1/2 alleen met GI 1/32 beschikbaar: 30160 bit/s.
  GI 1/8 en 1/16 leveren respectievelijk 27647 en 29273 bit/s en blijven
  onder de grens. Het PCR-interval bij 30160 bit/s is circa 99,735 ms;
  de bestaande timinggrens wordt niet verruimd. Ondersteuning van 40k op de
  aangesloten modulator en ontvanger moet afzonderlijk worden gecontroleerd.

## Formules

Met Rs in symbolen/s, B in Hz, code rate F en GI=1/g:

| Standaard | TS-bitrate vóór afronding |
|---|---|
| DVB-S | Rs × 2 × F × 188 / 204 |
| DVB-S2, zonder pilots | Rs × (Kbch − 80) / (32400 + 90) |
| DVB-S2, met pilots | Rs × (Kbch − 80) / (32400 + 90 + 22 × 36) |
| DVB-T, QPSK 2K | B × 423 / 544 × 2 × F × g / (g + 1) |

Voor S2 is Kbch respectievelijk 32208, 43040 en 48408 bij FEC 1/2, 2/3,
3/4. De 80 bits zijn de BBHEADER; 90 symbolen vormen de PLHEADER. Voor QPSK
zijn er 360 slots en 22 pilotblokken. Geen extra RS188/204-factor toepassen op
S2: de LDPC/BCH-overhead zit al in deze verhouding. CRC8 vervangt de TS-syncbyte.

Voor T combineert 423/544 de 1512 datacarriers, Tu=2048×7/(8B) en RS188/204.
De pilot- en TPS-carriers worden dus niet nog eens afgetrokken.

De aanvullende S2-Kbch-waarden zijn 16008 (1/4), 21408 (1/3),
38688 (3/5), 53840 (5/6), 57472 (8/9) en 58192 (9/10).
De keuzelijsten volgen de Portsdown-menu's: 2/5 en 4/5 staan wel in de
DVB-S2-norm maar niet in deze onderzochte menu's. 7/8 bestaat alleen voor S/T.
FEC-ID's 0, 1 en 2 behouden hun bestaande betekenis; nieuwe ID's zijn toegevoegd.
De interface sorteert op toenemende code rate en toont alleen geldige keuzes
met een berekende TS-capaciteit van minimaal 30080 bit/s. Een nog geldige keuze
blijft geselecteerd; anders kiest de interface de laagste beschikbare code rate.

Bronnen:

- [ETSI EN 300 421 V1.1.2](https://www.etsi.org/deliver/etsi_en/300400_300499/300421/01.01.02_60/en_300421v010102p.pdf), §4.4.2–4.5: DVB-S-codering en QPSK.
- [ETSI EN 302 307-1 V1.4.1](https://www.etsi.org/deliver/etsi_en/302300_302399/30230701/01.04.01_60/en_30230701v010401p.pdf), §5.1.4–5.1.6, §5.3 tabellen 5a/5b, §5.5: S2 baseband-, FEC- en fysieke frames.
- [ETSI EN 300 744 V1.6.2](https://www.etsi.org/deliver/etsi_en/300700_300799/300744/01.06.02_60/en_300744v010602p.pdf), §4.3.4.2, §4.4 en §4.7.2: DVB-T carriers en tijdstructuur.

## Vergelijking met Portsdown4

Onderzocht: lokale checkout `/home/rhardenb/repos/portsdown4`, commit
`2bc70af268a09dbac21884858c1119aa49306e54`,
[`src/gui/rpidatvtouch4.c`, functie `CalcTSBitrate()`](https://github.com/davecrump/portsdown4/blob/2bc70af268a09dbac21884858c1119aa49306e54/src/gui/rpidatvtouch4.c).
De S- en T-formules komen overeen. Portsdown gebruikt float-tussenresultaten;
onze exacte integerberekening vermijdt tussentijdse afronding. Een afzonderlijke
C-controle met de oorspronkelijke float-rekenvolgorde gaf bij alle 54
ondersteunde S/T-combinaties dezelfde gehele bitrate.

Voor S2 vraagt het infoscherm `dvb2iq` om de bitrate maar geeft de pilot- en
short-frame-opties niet mee. Het zendstartscript `scripts/a.sh` geeft `-p` en
`-v` wel door volgens de configuratie. Daarom is het infoscherm bij ingeschakelde
pilots niet de referentie voor de werkelijk beschikbare capaciteit. Onze
pilotkeuze telt de bijbehorende overhead mee; normale frames staan expliciet
vermeld in beide vensters.

| Instellingen | Berekende TS-bitrate (bit/s) |
|---|---:|
| S, 125k, FEC 1/2 | 115196 |
| S2, 125k, FEC 1/2, zonder pilots | 123607 |
| S2, 125k, FEC 1/2, met pilots | 120665 |
| T, 40k, FEC 1/2, GI 1/32 | 30160 |
| T, 35k, FEC 2/3, GI 1/8 | 32254 |
| T, 35k, FEC 2/3, GI 1/16 | 34152 |
| T, 35k, FEC 2/3, GI 1/32 | 35187 |
| T, 150k, FEC 1/2, GI 1/8 | 103676 |
| T, 150k, FEC 1/2, GI 1/16 | 109775 |
| T, 150k, FEC 1/2, GI 1/32 | 113101 |
| S, 35k, FEC 1/2 | 32254 |

Het programma berekent alleen de invoerbitrate voor de modulator; het wijzigt
geen Portsdown- of zenderinstellingen. Zet beide op dezelfde parameters.

## Configuratie en ondergrens

Configuratieversie 3 bewaart TS- en UDP-zendparameters afzonderlijk. Oude
versies 1 en 2 worden ingelezen; exacte bekende S/S2-waarden worden herkend.
Andere oude handmatige waarden blijven als vorige bitrate bewaard, met de
standaard DVB-keuzes als uitgangspunt. In de dialoog is altijd de nieuwe
berekende waarde zichtbaar; pas accepteren/toepassen/starten neemt die over.
De test-CLI blijft een expliciete bitrate accepteren voor transporttests.

De transportondergrens is 30080 bit/s: twee TS-pakketten duren dan precies
100 ms. Daaronder kan deze CBR-muxer de MPEG-TS-PCR-grens niet halen naast PSI/SI.
Bij 25 ksym/s is 2/3 de laagste beschikbare code rate voor S en S2.
Bij 30 ksym/s is dat 2/3 voor S, maar 3/5 voor S2.
S2 met FEC 1/2 past vanaf 31 ksym/s zonder pilots of 32 ksym/s met pilots;
S met FEC 1/2 past vanaf 33 ksym/s. De filtering volgt de berekening, inclusief
pilots, en geen vaste symbolrateblokkade. De calculator kan lagere uitkomsten
berekenen; export en UDP weigeren die.
De nieuwe keuzes zijn achteraan toegevoegd om opgeslagen keuze-indexen te behouden.
40 ms blijft een streefwaarde; de huidige MPEG-TS/DVB-PCR-grens is 100 ms
(ETSI TR 101 290 V1.4.1, §5.2.2, tabel 5.0b en noot 2).
Teletekst blijft minimaal 60000 bit/s vereisen.
SDT wordt na 800 ms opnieuw aangevraagd om ook bij lage bitrates marge voor
pakketplanning te houden. De bestaande encoder-fitcontrole blijft vereist: een geldige RF-combinatie garandeert niet
dat een complexe kaart bij iedere resolutie, fps en GOP verzonden kan worden.

## Portsdown 2019, 2020 en 4: broncodeonderzoek

Onderzocht op 23 september 2026, als broncodeanalyse zonder fysieke RF-test:

| Versie | Repository en onderzochte commit |
|---|---|
| 2019 (Stretch) | [BATC/portsdown, e93bb2d](https://github.com/BritishAmateurTelevisionClub/portsdown/tree/e93bb2de1cba5a013527fa2ac54315700c1ab319) |
| 2020 (Buster) | [BATC/portsdown-buster, 6fa1c02](https://github.com/BritishAmateurTelevisionClub/portsdown-buster/tree/6fa1c02345ef8e0cdf6a1cc3b17dd0d2cf2039b9) |
| 4 | [davecrump/portsdown4, 2bc70af](https://github.com/davecrump/portsdown4/tree/2bc70af268a09dbac21884858c1119aa49306e54) |
| Externe encoder voor 2020/4 | [F5OEO/libdvbmod, eff68b3](https://github.com/F5OEO/libdvbmod/tree/eff68b37047196e5bc732b08fa7f73c2bf94107e) |

Dit zijn concrete bronversies; een anders bijgewerkte SD-kaart of externe
Pluto-/Express-firmware kan hiervan afwijken. Installatiescripts van 2020/4
halen libdvbmod zonder vaste commit op. De onderzochte bibliotheek zet
`broadcasting=1`, `ccm_acm=CCM`, `issyi=0` en `npd=0`
(`libdvbmod.cpp`, `DVB-S2/DVB2.cpp`). Er is dus geen ingeschakelde
standaardconforme null-pakketverwijdering met reconstructie die de TS-grens omzeilt.

In alle drie GUI-bronnen heeft `SRCheck` bij het instellen van TX-presets een
ondergrens van **30 ksym/s**. 25 ksym/s kan deze generator berekenen en uitvoeren,
maar is geen via dat ongewijzigde touchscreen instelbare waarde. Handmatige
configuratie is geen bewijs dat de aangesloten modulator/ontvanger die SR aankan.
31 en 32 zijn wel via die menu's invoerbaar.

`scripts/a.sh` geeft `IPTSIN` door via netcat en `videots` aan de modulator.
Bij 2020/4 gebruikt het Lime-pad `limesdr_dvb`; de null-verwijdering in die bron
is uitgecommentarieerd. Pilots en normale/korte frames komen via `-p`/`-v` mee.
Kies **normale frames** voor de capaciteit die deze generator aangeeft.
Portsdown 4 heeft daarnaast `IPTSIN264/265` met FFmpeg-remux en een Pluto-pad
via FLV/RTMP. Die veranderen de TS of verplaatsen de muxing naar externe firmware;
ze zijn niet gelijk aan directe doorvoer van onze gecontroleerde TS.

**2019-aandachtspunt:** het Lime-IPTS-pad gebruikt `dvb2iq2`, gebouwd uit
`src/DvbTsToIQ/DvbTsToIQ2.cpp`. Regels 207–208 verwijderen null-pakketten;
het live-underflowpad voegt andere null-pakketten in en vermeldt zelf dat PCR
opnieuw gestempeld zou moeten worden. ISSY/NPD staan ook in de meegeleverde
2019-bibliotheek uit. Zodra null-pakketten tussen PCR's verdwijnen, kan dit de
CBR-PCR-timing veranderen. Dit is geen standaardconforme NPD-oplossing en geen
reden om 25/30 met FEC 1/2 beschikbaar te maken. Onze uitgangstests bewijzen
niet de timing na die oude modulator.

### Ondergrens: transport tegenover bedieningsmenu

Onderstaande gehele ksym/s-grenzen zijn berekend uit 30080 bit/s, QPSK en
normale S2-frames. Het zijn noodzakelijke transportgrenzen, geen garantie voor
beeldfit of RF-ontvangst. Een modulator-/ontvangergrens kan hoger liggen.

| FEC | DVB-S | DVB-S2 zonder pilots | DVB-S2 met pilots |
|---|---:|---:|---:|
| 1/4 | — | 62 | 63 |
| 1/3 | — | 46 | 47 |
| 1/2 | 33 | 31 | 32 |
| 3/5 | — | 26 | 26 |
| 2/3 | 25 | 23 | 24 |
| 3/4 | 22 | 21 | 21 |
| 5/6 | 20 | 19 | 19 |
| 7/8 | 19 | — | — |
| 8/9 | — | 18 | 18 |
| 9/10 | — | 17 | 18 |

De theoretische minima 17/18/19 ksym/s vallen onder de onderzochte
Portsdown-menuondergrens en zijn daarom geen nieuwe bedieningskeuzes in deze app.
Voor ongewijzigde Portsdown-menu's is het laagste onderzochte startpunt **30 ksym/s**:
S met minimaal FEC 2/3 of S2 met minimaal FEC 3/5. Voor FEC 1/2 zijn 31/32 S2
of 33 S de relevante nieuwe/bestaande keuzes.

De 100-ms-grens staat in [ETSI TS 101 154 V2.9.1, §4.1.5.3](https://www.etsi.org/deliver/etsi_ts/101100_101199/101154/02.09.01_60/ts_101154v020901p.pdf).
Volledige DVB-SI/AVC- en fysieke RF-conformiteit wordt hiermee niet geclaimd;
zie ook het beperkte serviceprofiel in `docs/datv.md`.
