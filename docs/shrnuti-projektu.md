# Detektor LZI - shrnutí projektu

## O projektu
- Projekt je demonstrační detektor zvýšené stresové odezvy na Arduino Uno.
- Neslouží jako skutečný zdravotnický nebo forenzní přístroj.
- Výsledek Pravda/Lež je orientační a vychází z jednoduché heuristiky.

## Použitý hardware
- Arduino Uno
- LCD1602 RGB po I2C
- GSR senzor na A0
- AD8232 na A1, LO- na D2, LO+ na D3
- Bzučák na D5
- Zelené tlačítko start na D8
- Červené tlačítko stop na D9
- Gravity Heart Rate + SpO2 senzor po I2C na adrese 0x57

## Ovládání
- Po zapnutí se zobrazí "Detektor lži" a "Připraveno".
- Zeleným tlačítkem na D8 se spustí měření.
- Červeným tlačítkem na D9 se měření pozastaví.
- Po pozastavení se zobrazí "Detektor lži" a "Pozastaveno".

## Co systém měří
- G = GSR, tedy kožní vodivost související se stresem a pocením.
- H = heart rate, tep v úderech za minutu.
- E = aktuální hodnota ECG signálu z AD8232.
- O2 = saturace kyslíku v krvi v procentech.
- T = teplota senzoru ve stupních Celsia.

## Chování displeje a podsvícení
- Žluté podsvícení = menu nebo kalibrace.
- Zelené podsvícení = stav Pravda.
- Červené podsvícení = stav Lež.
- OFF u ECG znamená, že elektrody AD8232 nejsou správně připojené.

## Vyhodnocení Pravda / Lež
- Po spuštění měření probíhá krátká kalibrace výchozích hodnot.
- Systém si uloží běžnou hodnotu GSR a tepu.
- Pokud se GSR nebo tep výrazně zvednou nad základ, vyhodnotí se Lež.
- Pokud jsou hodnoty blízko základu, vyhodnotí se Pravda.

## Zvuková signalizace
- Po startu zařízení se přehraje úvodní melodie.
- Při spuštění měření zazní krátký startovní signál.
- Při pozastavení měření zazní krátký signál pozastavení.
- Při detekci srdeční špičky bzučák krátce pípne.

## Technická poznámka
- Tlačítka jsou zapojena mezi digitální pin a GND.
- V programu je použit režim INPUT_PULLUP, proto není potřeba externí odpor.