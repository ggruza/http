# Serwer HTTP

## Uruchamianie
Program uruchamia się następująco:
serwer <nazwa-katalogu-z-plikami> <plik-z-serwerami-skorelowanymi> [<numer-portu-serwera>]

Parametr z nazwą katalogu jest parametrem obowiązkowym i może być podany jako ścieżka bezwzględna lub względna. W przypadku ścieżki względnej serwer próbuje odnaleźć wskazany katalog w bieżącym katalogu roboczym.

Parametr wskazujący na listę serwerów skorelowanych jest parametrem obowiązkowym i jego zastosowanie zostanie wyjaśnione w dalszej części (Skorelowane serwery HTTP).

Parametr z numerem portu serwera jest parametrem opcjonalnym i wskazuje numer portu na jakim serwer powinien nasłuchiwać połączeń od klientów. Domyślny numer portu to 8080.

Po uruchomieniu serwer powinien odnajduje wskazany katalog z plikami i rozpoczyna nasłuchiwanie na połączenia TCP od klientów na wskazanym porcie. Jeśli otwarcie katalogu, odczyt z katalogu, bądź otwarcie gniazda sieciowego nie powiodą się, to program kończy swoje działanie z kodem błędu EXIT_FAILURE.

Serwer po ustanowieniu połączenia z klientem oczekuje na żądanie klienta. Serwer kończy połączenie w przypadku przesłania przez klienta niepoprawnego żądania. W takim przypadku serwer wysła komunikat o błędzie, ze statusem 400, a następnie zakończyć połączenie. Jeśli żądanie klienta było jednak poprawne, to serwer będzie oczekiwać na ewentualne kolejne żądanie tego samego klienta lub zakończenie połączenia przez klienta.

## Format komunikacji
Wszystkie komunikaty wysyłane do klienta mają postać:

HTTP-message   = start-line

                      *( header-field CRLF )

                      CRLF

                      [ message-body ]
Taką samą postać powinny mieć również komunikaty wysyłane przez klienta do serwera.
Jeśli serwer otrzyma żądanie niezgodne z powyższą specyfikacją, to odpowiada błędem numer 400. Szczegóły konstruowania odpowiedzi serwera opisane zostały w dalszej części.

### Pierwsza linia komunikatu - start-line

Żądanie klienta powinny rozpoczynać się od status-line postaci request-line takiej, że:

request-line = method SP request-target SP HTTP-version CRLF

gdzie:

    method - jest tokenem wskazującym metodę żądaną do wykonania na serwerze przez klienta. Metody akceptowane przez serwer to: GET oraz HEAD. Nazwa metody jest wrażliwa na wielkość znaków. Więcej o obsłudze metod w dalszej części.
    SP - jest pojedynczym znakiem spacji.
    request-target - identyfikuje zasób na którym klient chciałby wykonać wskazaną wcześniej metodę. Nazwa zasobu nie może zawierać znaku spacji. Nazwy plików mogą zawierać wyłącznie znaki [a-zA-Z0-9.-], a zatem nazwa zasobu może zawierać wyłącznie znaki [a-zA-Z0-9.-/].
    HTTP-version - serwer akceptuje tylko wersję HTTP opisaną ciągiem znaków HTTP/1.1
    CRLF - ciąg dwóch znaków o wartościach ASCII równych 13 i 10.

Odpowiedź serwera ma także postać HTTP-message, jednak w przypadku komunikatów z serwera start-line przybiera postać status-line:

status-line = HTTP-version SP status-code SP reason-phrase CRLF

gdzie:

    HTTP-version - w przypadku niniejszego serwera, ponownie, zawsze będzie to ciąg znaków: HTTP/1.1.
    SP - jest pojedynczym znakiem spacji,
    status-code - jest numerem reprezentującym kod statusu odpowiedzi serwera. Status może wskazywać na poprawne wykonanie akcji po stronie serwera bądź jej niepowodzenie. Więcej o obsługiwanych przez serwer kodach w dalszej części.
    reason-phrase - jest opisem tekstowym zwróconego statusu. Serwer zawsze uzupełnia to pole niezerowej długości napisem opisującym powód błędu.
    CRLF - ciąg dwóch znaków o wartościach ASCII równych 13 i 10.

### Nagłówki żądania i odpowiedzi - header-field

W dalszej części formatu wiadomości, wymienianych pomiędzy niniejszym serwerem a klientami, następuje sekcja nagłówków. Sekcja składa się z zera lub więcej wystąpień linii postaci:

header-field   = field-name ":" OWS field-value OWS

gdzie:

    fields-name - jest nazwą nagłówka, nieczułą na wielkość liter. W dalszej części zostaną wymienione nagłówki obsługiwane przez implementację serwera.
    ":" - oznacza literalnie znak dwukropka.
    OWS - oznacza dowolną liczbę znaków spacji (w szczególności także brak znaku spacji).
    field-value - jest wartością nagłówka zdefiniowaną adekwatnie dla każdego z dozwolonych nagłówków protokołu HTTP. W dalszej części treści opisane są także oczekiwane wartości i znaczenie nagłówków.


### Treść komunikatu - message-body

Ostatnim elementem w zdefiniowanym formacie komunikatów, wymienianych pomiędzy serwerem a klientem, jest ich treść (ciało):

                      [ message-body ]

Występowanie treści w komunikacie determinuje wystąpienie wcześniej nagłówka Content-Length. Ponieważ w założeniu niniejsz implementacja obsługuje tylko metody GET i HEAD, to serwer ma prawo z góry odrzucać wszystkie komunikaty klienta, które zawierają ciało komunikatu. Odrzucenie żądania klienta skutkuje wysłaniem przez serwer wiadomości z błędem numer 400.

Serwer wysyłając treść komunikatu musi wysła także nagłówek Content-length z odpowiednią wartością, szczegóły w dalszej części.

## Zasoby serwera

Serwer podczas startu odczytuje obowiązkowy parametr z nazwą katalogu i używa wskazanego katalogu jako miejsca, z którego rozpocznie przeszukiwanie wskazywanych przez klientów zasobów. Serwer traktuje wszystkie identyfikatory zasobów wskazywane przez klientów jako nazwy plików we wskazanym katalogu. Serwer odnajduje w trakcie swojego działania także pliki, które zostały dodane/usunięte/zmodyfikowane po chwili uruchomienia serwera.

Serwer zakłada, że poprawne żądania klientów o zasoby zawsze rozpoczynają się od znaku “/”, który jednocześnie wskazuje iż rozpoczyna się poszukiwania pliku od korzenia jakim jest katalog przekazany w parametrze podczas uruchomienia serwera. Jeśli zapytanie klienta nie spełnia tego założenie, to serwer odpowiada błędem o numerze 400.

Serwer traktuje kolejne wystąpienia znaku “/”, w identyfikatorze zasobu (request-target), jako znak oddzielający poszczególne nazwy katalogów w ścieżce do pliku. Serwer próbóje odnaleźć plik zgodnie ze ścieżką otrzymaną od klienta w identyfikatorze zasobu. W przypadku gdy plik nie zostanie odnaleziony, bądź plik nie jest plikiem zwykłym, do którego serwer ma uprawnienia odczytu, to serwer zachowuje się zgodnie z wymaganiem opisanym w dalszej części (Skorelowane serwery HTTP).

## Obsługa błędów

Serwer stara się zawsze wysłać komunikat zwrotny do klienta. Komunikat informuje klienta o statusie przetworzenia jego żądania. Sewer zwraca następujące kody (status-code):

    200 - ten kod informuje klienta, że jego żądanie zostało w pełni i poprawnie wykonane przez serwer. Ten kod jest używany w szczególności kiedy przesyłamy do klienta zawartość szukanego przez klienta pliku.
    302 - ten kod oznacza, że szukany przez klienta zasób został tymczasowo przeniesiony pod inną lokalizację. Tego kodu jest używany do implementacji nietypowego rozszerzenia serwera, opisanym w dalszej części (Skorelowane serwery HTTP).
    400 - ten kod serwer wysła zawsze, i tylko w przypadku, kiedy żądanie serwera nie spełnia oczekiwanego formatu, bądź zawiera elementy, które niniejsza specyfikacja wyklucza jako akceptowane.
    404 - ten kod informuje klienta, że żądany przez niego zasób nie został odnaleziony. Serwer wysyła ten błąd w przypadku nieodnalezienia żądanego przez klienta pliku - z uwzględnieniem jednak dodatkowej logiki opisanej w dalszej części specyfikacji (Skorelowane serwery HTTP).
    500 - ten kod reprezentuje błąd po stronie serwera w przetwarzaniu żądania klienta. Błąd ten serwer wysła do klienta w przypadku problemów występujących po stronie z serwera nie wynikających z błędu po stronie klienta. Ten kod błędu oznacza generyczny błąd pracy serwera.
    501 - ten kod błędu serwer zwraca w przypadku żądań klienta wykraczających poza zakres implementacji niniejszego serwera. Dla przykładu wszystkie żądania z metodami różnymi od GET i HEAD. Kod ten informuje klienta, że serwer nie zaimplementował obsługi otrzymanego żądania.

## Obsługiwane nagłówki

Serwer obsługuje nagłówki o następujących wartościach field-name:

    Connection - domyślnie połączenie nawiązane przez klienta pozostaje otwarte tak długo jak klient nie rozłączy się albo serwer nie uzna danego klienta za bezczynnego i nie zakończy połączenia. Klient w ramach jednego połączenie może przesłać do serwera więcej niż jeden komunikat i oczekiwać odpowiedzi serwera na każdy z wysłanych komunikatów. Odpowiedzi serwera następują w kolejności odpowiadającej przychodzącym żądaniom klienta. Nagłówek Connection ustawiony z wartością close pozwala zakończyć połączenie TCP po komunikacie odpowiedzi wysłanej przez serwer, następującej po komunikacie zawierającym wspomniany nagłówek ze wskazaną wartością. Zatem jeśli klient wyśle komunikat żądania z nagłówkiem Connection: close, to serwer kończy połączenie po wysłaniu komunikatu odpowiedzi.
    Content-Type - jest nagłówkiem opisującym jakiego rodzaju dane przesyłamy w treści (ciele) komunikatu HTTP. Implementacja serwera zawsze określa wysyłane pliki z serwera jako strumień bajtów application/octet-stream.
    Content-Length - wartość tego nagłówka, wyrażona nieujemną liczbą całkowitą, określa długość treści (ciała) komunikatu HTTP. Wartość tego nagłówka znajduje się w każdej odpowiedzi z serwera, która zawiera treść (ciało). Wartość tego nagłówka określa wyłącznie liczbę oktetów treści (ciała) komunikatu HTTP. Serwer obsługuje ten nagłówek także w komunikatach wysyłanych przez klienta.

Nagłówki nie wymienione powyżej, a otrzymane w komunikacie od klienta są ignorowane.

W przypadku wystąpienia więcej niż jednej linii nagłówka o tej samej wartości field-name, serwer potraktuje takie żądanie jako niepoprawne i odpowie statusem o numerze 400.

## Obsługiwane metody

Serwer obsługuje następujące metody:

    GET - w przypadku otrzymania żądania od klienta z tą metodą, serwer podejmuje próbę odnalezienia wskazanego zasobu (pliku) w katalogu jaki został przekazany jako parametr przy uruchomieniu programu. Jeśli plik zostanie odnaleziony, to serwer zwraca zawartość tego pliku poprzez wysłanie odpowiedniego komunikatu HTTP z treścią (ciałem) uzupełnionym oktetami odpowiadającymi bajtom odczytanym z pliku. Serwer ustawia typ zwracanego pliku w nagłówku Content-Type application/octet-stream dla dowolnego pliku.
    Jeśli plik nie zostanie odnaleziony na serwerze, to serwer zachowuje się zgodnie opisem w dalszej części specyfikacji (Skorelowane serwery HTTP).
    HEAD - w przypadku otrzymania żądania z tą metodą, serwer odpowiada dokładnie takim samym komunikatem jak gdyby otrzymał żądanie z metodą GET, z tą różnicą, że serwer przesyła komunikat bez treści (ciała). Odpowiedź serwera na żądanie HEAD jest taka sama jaką otrzymałby klient wykonując metodę GET na wskazanym zasobie, w szczególności nagłówki także zostają zwrócone przez serwer takie same jak w przypadku metody GET.

##Skorelowane serwery HTTP

Implementacja serwera przyjmuje obowiązkowy parametr wejściowy wskazujący (ścieżką bezwzględną bądź względną) na plik tekstowy o strukturze:

zasób TAB serwer TAB port

gdzie:

    zasób - to bezwzględna ścieżka do pliku.
    TAB - znak tabulacji.
    serwer - to adres IP serwera, na którym wskazany zasób się znajduje. Adres jest adresem IP w wersji 4.
    port - to numer portu, na którym serwer nasłuchuje na połączenia.

Serwer odszukuje plik na podstawie ścieżki bezwzględnej bądź ścieżki względnej w aktualnym katalogu roboczym. Jeśli plik nie zostanie odnaleziony, bądź nie możliwe było jego odczytanie, to serwer zakończy swoje działanie z kodem błędu EXIT_FAILURE. Implementacja serwera zakłada, że wczytywany plik posiada poprawną strukturę, zgodną z zadaną w niniejszej specyfikacji, a plik pusty także jest poprawnym plikiem. Implementacja serwera odczytuje zawartość pliku i w przypadku otrzymania od klienta żądania HTTP dotyczącego zasobu, którego serwer nie znalazł w plikach zlokalizowanych lokalnie, przeszukuje wczytaną z pliku tablicę i odszukuje żądany przez klienta zasób. Jeśli zasób nie występuje także we wczytanej tablicy, to serwer odpowiada statusem o numerze 404 do klienta. Jeśli jednak zasób znajduje się na liście, to wysła odpowiedź do klienta ze statusem numer 302 oraz ustawionym nagłówkiem Location, którego wartość jest tekstem reprezentującym adres HTTP do serwera zawierającego szukany zasób. Jeśli szukany zasób występuje więcej niż raz w tablicy wczytanej z pliku, to serwer korzysta z pierwszego wpisu występującego najwcześniej w pliku. Konstruowanie nowego adresu szukanego zasobu wykonywane jest  następującego:

PROT serwer COLON port zasób

gdzie:

    PROT - to napis http://.
    serwer - to adres IP serwera, odczytany z pliku.
    COLON - to znak dwukropka.
    port - to numer portu, odczytany z pliku.
    zasób - to bezwzględna ścieżka do zasobu (zaczynająca się od znaku slash /).
