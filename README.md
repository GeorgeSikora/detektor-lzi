# Detektor LZI

Krátký projekt k maturitní práci: jednoduchý "detektor lži" na Arduino Uno.

## O čem je práce
- Cíl: ukázat základní měření fyziologických signálů při stresu.
- Přístup: kombinace více senzorů a jednoduché vizualizace na LCD.
- Výstup: živé hodnoty v Serial Monitoru a na LCD1602 RGB.
- Poznámka: projekt je demonstrační, ne zdravotnický přístroj.

## Použitý hardware
- Arduino Uno
- I2C LCD1602 RGB module
- GSR v1.2 (potivost) na A0
- DFRobot Gravity Heart Rate + Oximeter (I2C, MAX3010x)

## Zapojení (hlavní body)
- GSR SIG -> A0
- I2C sběrnice: SDA -> A4, SCL -> A5
- Napájení modulů: VCC a GND dle specifikace modulu

## Co dělá kód
- Inicializuje LCD přes I2C (text 0x3E, RGB backlight 0x60/0x62).
- Čte GSR hodnotu z A0 (průměrování vzorků pro stabilnější čísla).
- Čte tep a SpO2 ze senzoru na adrese 0x57.
- Zobrazuje data na LCD (2 řádky) a zároveň vypisuje stav do Serial Monitoru.
- Mění barvu podsvitu podle stavu měření.

## Jak spustit
- Build: `platformio run`
- Upload: `platformio run --target upload`
- Monitor: `platformio device monitor -b 115200`

## Struktura projektu
- `src/main.cpp` hlavní logika
- `platformio.ini` konfigurace desky a knihoven
