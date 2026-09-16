# TS-bitrate uit zendparameters

De calculator in `src/core.c` berekent de capaciteit voor volledige **188-byte
MPEG-TS-pakketten**. PSI/SI, PCR, PES en null-pakketten gebruiken een deel daarvan;
er wordt niet nogmaals een videopercentage of UDP/IP-overhead afgetrokken.
De bestaande encoder/muxer bepaalt vervolgens of het beeld daarin past.
Alle berekeningen gebruiken 64-bit gehele tussenresultaten en ronden pas het
eindresultaat naar beneden af op hele bit/s.

## Ondersteund profiel

- DVB-S: QPSK, SR 35/66/125/150/333/500 ksym/s, FEC 1/2, 2/3, 3/4.
- DVB-S2: dezelfde keuzes, normale 64800-bit FECFRAME, pilots aan/uit,
  CCM en een volledig gevulde DATAFIELD, zonder ISSY of null-packet deletion.
  Short frames en 8PSK zijn niet geïmplementeerd.
- DVB-T: QPSK, 2K, niet-hiërarchisch, dezelfde FEC-keuzes, GI 1/8, 1/16,
  1/32. Portsdown SR/BW 150k/250k/333k/500k staat voor respectievelijk
  150/250/333/500 kHz kanaalbandbreedte, niet voor OFDM-symbolen per seconde.
  Dit zijn geschaalde amateurbandbreedtes; geen claim dat deze kanalen onder
  de standaard DVB-T-kanaalbreedtes vallen.

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

De transportondergrens is 32000 bit/s, zodat DVB-S 35k FEC 1/2 past.
Bij 32000 bit/s geven twee TS-pakketten 94 ms tussen PCR's. SDT wordt na 800 ms opnieuw aangevraagd om ook bij lage bitrates marge voor
pakketplanning te houden. De bestaande encoder-fitcontrole blijft vereist: een geldige RF-combinatie garandeert niet
dat een complexe kaart bij iedere resolutie, fps en GOP verzonden kan worden.
