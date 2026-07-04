import { plural } from "../types";
import type { MessageCatalogShape } from "../types";
import { enMessages } from "./en";

type MessageCatalog = MessageCatalogShape<typeof enMessages>;

export const plMessages = {
  app: {
    brand: "MatrixHub",
    title: "MatrixHub",
    settingsHeading: "Ustawienia",
    chartsHeading: "Wykresy",
    addDeviceSheet: {
      title: "Dodaj urządzenie",
      dialogLabel: "Dodaj urządzenie",
      close: "Zamknij",
      closeLabel: "Zamknij dodawanie urządzenia",
    },
    emptyState: {
      title: "Nie ma jeszcze urządzeń",
      copy: "Otwórz dodawanie urządzenia i zapisz pierwszy lokalny adres. Pojawi się tutaj jako kompaktowa karta.",
      action: "Dodaj urządzenie",
    },
    header: {
      back: "Wróć",
      backToDevices: "Wróć do urządzeń",
      addDevice: "Dodaj urządzenie",
      quickActions: "Szybkie akcje",
      openActions: "Otwórz szybkie akcje",
      closeActions: "Zamknij szybkie akcje",
    },
    startup: {
      title: "Uruchamianie",
      copy: "Wczytywanie zapisanych urządzeń i stanu sesji...",
    },
    toast: {
      problem: "Problem",
      done: "Gotowe",
    },
  },
  common: {
    waiting: "Oczekiwanie",
    save: "Zapisz",
    open: "Otwórz",
    remove: "Usuń",
    refresh: "Odśwież",
    retry: "Spróbuj ponownie",
    cachedUntilLiveRefresh:
      "Wyświetlane są zapisane wartości do czasu nadejścia odświeżenia na żywo.",
  },
  forms: {
    addDevice: {
      name: "Nazwa",
      address: "Adres",
      namePlaceholder: "Czujnik w biurze",
      addressPlaceholder:
        "matrixhub.local, 192.168.4.1 lub https://matrixhub.local",
      saving: "Zapisywanie...",
      saveDevice: "Zapisz urządzenie",
    },
    auth: {
      username: "Użytkownik",
      password: "Hasło",
      signingIn: "Logowanie...",
      signIn: "Zaloguj",
    },
  },
  deviceCard: {
    realtime: {
      active: "Aktualizacje na żywo aktywne",
      reconnecting: "Ponowne łączenie aktualizacji na żywo",
      starting: "Uruchamianie aktualizacji na żywo",
      paused: "Aktualizacje na żywo wstrzymane",
      ready: "Aktualizacje na żywo gotowe",
    },
    rename: {
      titleLabel: "Nazwa urządzenia",
      actionsLabel: "Akcje zmiany nazwy urządzenia",
      saveTitle: "Zapisz nazwę",
      cancelEdit: "Anuluj edycję nazwy",
      cancel: "Anuluj",
      editTitle: "Edytuj nazwę",
    },
    actions: {
      label: "Akcje urządzenia",
      openMenu: "Otwórz akcje urządzenia",
      closeMenu: "Zamknij akcje urządzenia",
      reconnect: "Połącz ponownie",
      settings: "Ustawienia",
      openDevice: "Otwórz urządzenie",
      signOut: "Wyloguj",
    },
    cachedNote:
      "Wyświetlane są ostatnie znane dane do czasu nadejścia odświeżenia na żywo.",
    sensors: {
      title: "Czujniki",
      waitingForData: "Oczekiwanie na dane z czujników na żywo",
      openCharts: "Otwórz wykresy dla {label}",
    },
    sections: {
      led: "LED",
      bluetooth: "Bluetooth",
      shelly: "Shelly",
    },
    ble: {
      noRecentSample: "Brak ostatniej próbki z termometru",
      lastSample: "Ostatnia próbka {time}",
      summaryLabel: "{label} {reading}. {status}",
      temperatureBadge: "T",
      humidityBadge: "H",
      batteryBadge: "Bat.",
    },
    shelly: {
      relayCount: plural({
        one: "{value} przekaźnik",
        few: "{value} przekaźniki",
        many: "{value} przekaźników",
        other: "{value} przekaźników",
      }),
      on: "Wł.",
      off: "Wył.",
      offlineSuffix: "offline",
      queued: "W kolejce...",
      turnOn: "Włącz przekaźnik",
      turnOff: "Wyłącz przekaźnik",
      adminRequired: "Wymagany dostęp administratora",
    },
  },
  matrix: {
    quickControl: {
      brightnessLabel: "Jasność matrycy LED",
    },
    quickSave: {
      saving: "Zapisywanie LED...",
      saved: "LED zapisany.",
    },
    panel: {
      title: "Matryca LED",
      currentState: "Bieżący stan matrycy LED",
      liveRefreshFailed: "Odświeżanie na żywo nie powiodło się. {message}",
      loading: "Wczytywanie ustawień matrycy...",
      mode: "Tryb",
      effects: "Efekty",
      menu: "Menu",
      brightness: "Jasność",
      scrollSpeed: "Prędkość przewijania",
      ledEffects: "Efekty LED",
      animatedBackground: "Animowana warstwa tła",
      adminRequired:
        "Do edycji ustawień matrycy wymagany jest dostęp administratora.",
      appearAfterRestore:
        "Ustawienia matrycy pojawią się po przywróceniu sesji.",
      retryRefresh: "Odśwież ustawienia matrycy, aby spróbować ponownie.",
    },
    mode: {
      solid: "Stały",
      icon: "Ikona",
      scroll: "Przewijanie",
      unknown: "Nieznany",
    },
    state: {
      enabled: "Włączone",
      disabled: "Wyłączone",
    },
  },
  settingsPanel: {
    heading: "Ustawienia urządzenia",
    detailsLabel: "Szczegóły urządzenia",
    statusLabel: "Stan urządzenia",
    signOut: "Wyloguj",
  },
  overview: {
    telemetry: {
      co2: "CO2",
      temperature: "Temp.",
      humidity: "Wilgotność",
    },
    charts: {
      copy: "Trend z ostatnich próbek na żywo dla wybranego urządzenia.",
      min: "Min",
      max: "Max",
      range: "Zakres",
      scale: "Skala",
    },
    device: {
      user: "Użytkownik",
      signedOut: "Wylogowano",
      uptime: "Czas pracy",
      live: "Na żywo",
      wifi: "Wi-Fi",
      unknown: "Nieznane",
    },
    realtime: {
      connected: "Połączono",
      retrying: "Ponawianie",
      starting: "Uruchamianie",
      paused: "Wstrzymane",
      idle: "Bezczynne",
      waitingForFirstSample: "Oczekiwanie na pierwszą próbkę",
      waiting: "Oczekiwanie",
      waitingForData: "oczekiwanie na dane",
      cached: "Z pamięci",
      readFailed: "Odczyt nieudany",
      sampled: "Odczytano",
      stale: "Nieaktualne {time}",
    },
    wifi: {
      connected: "Połączono",
      offline: "Offline",
      rescueAp: "AP ratunkowy",
      connecting: "Łączenie",
      waiting: "Oczekiwanie",
    },
  },
  language: {
    label: "Język",
    pickerLabel: "Wybór języka",
    openPicker: "Otwórz wybór języka",
    closePicker: "Zamknij wybór języka",
    useLanguage: "Użyj języka {language}",
    auto: "Automatycznie",
    autoHint: "Zgodnie z językiem przeglądarki",
    activeAuto: "Auto ({language})",
    names: {
      en: "English",
      pl: "Polski",
    },
  },
  theme: {
    paletteLabel: "Paleta stylów",
    styles: "Style",
    openPalette: "Otwórz paletę stylów",
    useStyle: "Użyj stylu {style}",
    names: {
      business: "Biznes",
      corporate: "Korporacyjny",
      night: "Noc",
      black: "Czarny",
      luxury: "Luksus",
      dracula: "Dracula",
      forest: "Las",
      coffee: "Kawa",
      dim: "Przygaszony",
      sunset: "Zachód słońca",
      halloween: "Halloween",
      synthwave: "Synthwave",
      light: "Jasny",
      nord: "Nord",
      retro: "Retro",
    },
  },
  errors: {
    "permissions/host_denied":
      "Nie przyznano uprawnienia hosta dla tego urządzenia.",
    "validation/device_address_required": "Adres urządzenia jest wymagany.",
    "validation/device_address_invalid":
      "Wpisz poprawny adres IP urządzenia, hostname lub pełny adres http(s).",
    "validation/username_required": "Użytkownik jest wymagany.",
    "validation/password_required": "Hasło jest wymagane.",
    "validation/hostname_required": "Hostname jest wymagany.",
    "validation/hostname_too_short": "Hostname musi mieć co najmniej 3 znaki.",
    "validation/hostname_too_long": "Hostname może mieć maksymalnie 32 znaki.",
    "validation/hostname_invalid":
      "Hostname może zawierać tylko litery, cyfry i myślniki.",
    "validation/connection_mode_invalid": "Wybierz poprawny tryb STA.",
    "auth/invalid_credentials": "Nieprawidłowa nazwa użytkownika lub hasło.",
    "request/network":
      "Połączenie nie powiodło się. Jeśli urządzenie używa certyfikatu self-signed, otwórz je raz w zwykłej karcie i zaufaj certyfikatowi.",
    "request/timeout": "Urządzenie nie odpowiedziało na czas.",
    "request/failed": "Żądanie nie powiodło się.",
  },
  controller: {
    signedOut: "Wylogowano.",
    sessionExpired: "Sesja wygasła. Zaloguj się ponownie.",
    deviceSaved:
      "Urządzenie zapisane. Zaloguj się, aby wczytać podgląd na żywo.",
    addDeviceFirst: "Najpierw dodaj urządzenie.",
    signedIn: "Zalogowano. Przywracanie podglądu i telemetrii na żywo.",
    matrixAdminRequired:
      "Do zmiany ustawień matrycy LED wymagany jest dostęp administratora.",
    shellyAdminRequired:
      "Do sterowania Shelly wymagany jest dostęp administratora.",
    deviceTitleEmpty: "Nazwa urządzenia nie może być pusta.",
    deviceTitleSaved: "Nazwa urządzenia zapisana.",
    matrixSettingsSaved: "Ustawienia matrycy LED zapisane.",
    shellyCommandQueued: "Polecenie Shelly dodane do kolejki.",
  },
  overviewActions: {
    currentAddress: "bieżącym adresem",
    reconnectRequestedWithAddress:
      "Żądanie ponownego połączenia wysłane. Urządzenie zgłasza Wi-Fi pod adresem {address}.",
    reconnectRequestedAccepted:
      "Żądanie ponownego połączenia wysłane. Urządzenie przyjęło zadanie odzyskiwania połączenia.",
  },
  time: {
    lessThanMinuteAgo: "<1 min temu",
    ago: "{value} temu",
    units: {
      minute: {
        short: plural({
          one: "{value} min",
          few: "{value} min",
          many: "{value} min",
          other: "{value} min",
        }),
      },
      hour: {
        short: plural({
          one: "{value} godz.",
          few: "{value} godz.",
          many: "{value} godz.",
          other: "{value} godz.",
        }),
      },
      day: {
        short: plural({
          one: "{value} d",
          few: "{value} d",
          many: "{value} d",
          other: "{value} d",
        }),
      },
    },
  },
} as const satisfies MessageCatalog;
