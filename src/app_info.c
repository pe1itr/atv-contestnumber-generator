#include "app_info.h"
#include <stdio.h>
#include <string.h>

const char *app_info_text(void) {
    static char text[2048];
    const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char month[4] = {0};
    int day, year, number = 1;
    if (sscanf(__DATE__, "%3s %d %d", month, &day, &year) != 3) {
        day = 0; year = 0;
    }
    for (int i=0; i<12; ++i) if (!strncmp(month, months+3*i, 3)) number = i+1;
    snprintf(text, sizeof(text),
        "ATV contestnummer generator\n"
        "Versie: %s\nCompilatiedatum: %04d-%02d-%02d\nAuteur: %s\n\n"
        "Doel\n"
        "Maak JPG-beelden voor ATV- en DATV-contests of een PM5544-testbeeld "
        "met je eigen roepnaam en Maidenheadlocator.\n\n"
        "Gebruik\n"
        "Kies Contest of PM5544 en vul je roepnaam en locator in. "
        "Locators mogen 4, 6, 8, 10 of 12 tekens bevatten. "
        "Kies daarna de beeldverhouding en resolutie (standaard 320 x 240).\n\n"
        "In Contest staat een automatisch nummer meteen in beeld. "
        "Klik op Nieuw nummer voor een andere code, of kies Zelf intypen. "
        "Je kunt de band, inverse weergave, blauw/geel, locator, cijfersom en extra code "
        "rechtsboven instellen. Export bewaart precies de zichtbare code.\n\n"
        "Bij automatisch verandert het nummer bij een ander locatorvak "
        "(eerste zes tekens); meer precisie binnen hetzelfde vak behoudt het nummer.\n\n"
        "In PM5544 verschijnen de roepnaam en locator in de zwarte vlakken "
        "van het ingebouwde testbeeld. Contestopties zijn dan uitgeschakeld.\n\n"
        "Gebruik File > Exporteer JPG of de exportknop. Het JPG-bestand "
        "wordt naast het programma opgeslagen, met maximaal zes locatortekens "
        "en de resolutie in de naam. "
        "Kies File > Exporteren naar... om zelf een map en bestandsnaam te kiezen. "
        "Bij een bestaand bestand wordt gevraagd of je het wilt vervangen.",
        APP_VERSION, year, number, day, APP_AUTHOR);
    return text;
}
