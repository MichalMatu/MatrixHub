# GitHub release workflow

Ten plik opisuje zasade pracy z galeziami `develop` i `main` w MatrixHub.

## Zalozenie

- Na galezi `develop` odbywa sie codzienna praca nad kodem.
- Na galezi `develop` zostaje pelna historia zmian: male commity, poprawki,
  eksperymenty i przygotowanie release.
- Galaz `main` jest linia wydan.
- Na galezi `main` maja byc tylko pojedyncze commity wersji, np.
  `Version 1.1.0`.
- Nie przenosimy na `main` calej historii commitow z `develop`.
- Release na `main` jest snapshotem aktualnego kodu z `develop` plus bump
  wersji.

## Trigger release

Gdy uzytkownik wklei ten plik do czatu, oznacza to:

- `develop` osiagnal stan gotowy do wydania nowej wersji.
- Nalezy przygotowac nowy commit wersji na `main`.
- Aktualny kod z `develop` ma zostac przeniesiony na `main` jako jeden snapshot.
- Wersja aplikacji ma zostac podbita przed commitem.
- Po zakonczeniu pracy aktywna galaz ma byc `main`.

## Procedura dla Codex

1. Sprawdz stan repozytorium:
   - `git status --short --branch`
   - `git fetch --prune origin`
2. Upewnij sie, ze praca release startuje z czystego drzewa albo jasno wyjasnij
   uzytkownikowi, ktore lokalne zmiany blokuja release.
3. Przejdz na `develop` i zweryfikuj aktualny kod:
   - uruchom `pio test -e native`
   - uruchom pelny release build `pio run -e waveshare_esp32s3_matrix`
4. Ustal nowa wersje:
   - jesli uzytkownik podal wersje, uzyj jej,
   - jesli nie podal wersji, podbij patch version, np. `1.0.0` -> `1.0.1`.
5. Zaktualizuj wszystkie pliki wersji uzywane przez projekt, w szczegolnosci:
   - `platformio.ini`
   - `interface/package.json`
   - `interface/package-lock.json`, jesli zawiera wersje pakietu aplikacji.
6. Utworz na `main` jeden commit snapshotowy z aktualnym kodem z `develop` i
   podbita wersja.
7. Commit na `main` nazwij w formacie:
   - `Version X.Y.Z`
8. Wypchnij `main` na `origin/main`, jesli push jest mozliwy.
9. Po release zostaw repozytorium na galezi `main`.

## Wazne ograniczenia

- Nie rob fast-forward merge z `develop` do `main`, jezeli przeniosloby to
  wiele commitow developerskich na `main`.
- Nie rob zwyklego merge commit z `develop` do `main`.
- `main` ma pokazywac tylko kolejne wersje jako pojedyncze commity.
- `develop` jest miejscem, gdzie zostaje szczegolowa historia zmian.
- Nie nadpisuj ani nie cofaj lokalnych zmian uzytkownika bez wyraznej prosby.
- Nie merguj `main` z powrotem do `develop` tylko po to, aby zsynchronizowac snapshot wydania.
- Po wydaniu zsynchronizuj do `develop` tylko metadane bazowej wersji (np. `APP_VERSION` i wersje pakietu), jesli release podbil je wylacznie na `main`.
- Rozbieznosc historii `main` i `develop` jest oczekiwana w tym modelu: `main` przechowuje snapshoty wydan, a `develop` szczegolowa historie rozwoju.
