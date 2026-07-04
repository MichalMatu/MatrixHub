# Xiaomi Token Extraction & Flashing Guide

Ten folder zawiera narzędzie do wydobywania "Bind Key" (tokena szyfrującego) z chmury Xiaomi. Jest to niezbędne dla urządzeń takich jak termometr **LYWSD03MMC** z nowszym firmwarem (2.x), aby móc połączyć się z nimi przez Bluetooth i wgrać customowy firmware (np. PVVX).

## 1. Jak uruchomić narzędzie?

Narzędzie posiada własne środowisko wirtualne w folderze `venv`, więc nie musisz nic instalować w systemie.

Aby uruchomić skrypt, wpisz w terminalu (będąc w głównym katalogu projektu):

```bash
# Aktywacja środowiska i uruchomienie skryptu
source tools/xiaomi_token_extractor/venv/bin/activate
python tools/xiaomi_token_extractor/app/token_extractor.py
```

## 2. Instrukcja obsługi skryptu

1.  **Chmura**: Po uruchomieniu zostaniesz zapytany o serwer. Wybierz region, na którym masz zarejestrowane urządzenia w aplikacji Xiaomi Home.
    *   Jeśli jesteś w Polsce/Europie, zazwyczaj wybierz **`de`** (Niemcy/Europa).
2.  **Logowanie**: Podaj swój adres e-mail i hasło do konta Xiaomi.
3.  **Weryfikacja CAPTCHA** (częsty przypadek):
    *   Jeśli skrypt nie może zalogować się automatycznie, wyświetli długi link URL.
    *   **Skopiuj ten link** i otwórz go w zwykłej przeglądarce internetowej na komputerze.
    *   Rozwiąż CAPTCHA (przepisz kod z obrazka).
    *   Po pomyślnej weryfikacji, skrypt w terminalu powinien automatycznie wykryć sukces i kontynuować. Jeśli nie, postępuj zgodnie z instrukcjami na ekranie (czasem trzeba wkleić wynikowy URL).
4.  **Wynik**:
    *   Skrypt wylistuje wszystkie Twoje urządzenia przypisane do konta.
    *   Znajdź na liście swój termometr (`LYWSD03MMC`).
    *   Skopiuj wartości: **`MAC`** oraz **`Bind Key`** (32 znaki).

## 3. Co dalej? (Flashowanie Custom Firmware)

Mając `Bind Key`, możesz wgrać alternatywne oprogramowanie, które "uwolni" termometr (zwiększy częstotliwość odświeżania, usunie szyfrowanie, poprawi żywotność baterii i współpracę z ESP32).

1.  Otwórz w przeglądarce flasher (polecany PVVX): [TelinkMiFlasher](https://pvvx.github.io/ATC_MiThermometer/TelinkMiFlasher.html)
2.  Kliknij **Connect** i wybierz swój termometr z listy Bluetooth.
    *   *Uwaga: Jeśli termometr prosi o "Activation" lub nie chce się połączyć, to właśnie tutaj potrzebny jest Bind Key.*
3.  Zostaniesz poproszony o **Login Key** / **Bind Key**. Wklej klucz uzyskany ze skryptu.
4.  Po pomyślnym połączeniu kliknij **Do Activation** (jeśli wymagane).
5.  Wybierz **Custom Firmware** (np. wersję `pvvx` dla najlepszej kompatybilności z BLE Home Assistant/ESP32).
6.  Kliknij **Start Flashing**.

Po wgraniu customowego softu, `Bind Key` nie będzie już potrzebny do odczytu danych przez ESP32 (chyba że włączysz szyfrowanie w ustawieniach custom firmware).

## 4. Zalecane ustawienia PVVX dla ESP32-S3

Po wgraniu firmware PVVX, połącz się ponownie z termometrem przez TelinkMiFlasher i ustaw:

| Ustawienie | Wartość | Opis |
|------------|---------|------|
| **Advertising type** | `BTHome v2` | Standard obsługiwany przez ESP32-S3 |
| **Encrypted beacon** | **Wyłączone** | Brak szyfrowania = prostsze dekodowanie |
| **Advertising interval** | `2500 ms` | Domyślna, można zmniejszyć do 1000 ms |
| **Measure interval** | `4` (= 10 s) | Częstotliwość pomiarów |

Kliknij **Send Config** po zmianie ustawień.

## 5. Integracja z ESP32-S3

Projekt ESP32-S3 automatycznie wykrywa termometry PVVX/ATC na podstawie:
- **Nazwy urządzenia**: `ATC_*`, `LYWSD*`
- **Service UUID**: `0xFCD2` (BTHome v2)

### Jak dodać termometr do monitorowania:

1. Włącz **Discovery Mode** w UI (`/bluetooth` lub API)
2. ESP32-S3 wykryje termometr i wyświetli go na liście
3. Dodaj MAC termometru do whitelisty (w ustawieniach BLE sensorów)
4. Termometr będzie monitorowany i dane będą dostępne przez API

### Wspierane formaty:
- ✅ **BTHome v2** (PVVX/ATC firmware)
- ✅ **ThermoPro TP357** (Manufacturer Data)

### Pliki parsera:
- `src/ble/scanner/BtHomeParser.h` - Parser BTHome v2
- `src/ble/scanner/TpParser.h` - Parser ThermoPro TP357
- `src/ble/scanner/BleScanner.cpp` - Logika wykrywania i parsowania

## 6. Rozwiązywanie problemów

### ESP32 nie widzi termometru
- Sprawdź czy termometr ma wgrany PVVX firmware
- Upewnij się że **Encrypted beacon** jest wyłączone
- Sprawdź czy **Advertising type** to `BTHome v2`

### Dane są szyfrowane
W logach ESP32 zobaczysz:
```
WARN: BTHome encrypted packet from XX:XX:XX:XX:XX:XX - disable encryption in PVVX settings
```
Rozwiązanie: Połącz się z termometrem przez TelinkMiFlasher i wyłącz **Encrypted beacon**.
