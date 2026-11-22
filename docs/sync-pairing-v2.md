# Спаривание sync/2: одноразовый код + X25519

## Зачем

Старая схема: `secret = SHA256(sorted(deviceIds) + PIN)`, PIN — 6 цифр,
секрет постоянный. Из одного перехваченного `hello` с proof PIN
перебирается офлайн за секунды (deviceId публичны), а подсмотренный PIN
даёт постоянный доступ до Unpair.

Новая схема убирает и то, и другое:

- PIN больше не является ключом и не живёт постоянно;
- по сети код не ходит;
- долговременный секрет — 32 случайных байта, он не выводится из кода;
- код одноразовый, с TTL и ограничением попыток.

## Что показывается и вводится

Сервер (тот, кто слушает порт) генерирует **16-символьный base32-код**
(`A-Z`, `2-7`), 80 бит энтропии. Код показывается на экране сервера
(попап `Sync pairing` или окно устройств, кнопка `Accept`).

Клиент вводит код вручную (QR не нужен и не является заменой
криптографии). Ввод принимает дефисы, пробелы и нижний регистр.

Код живёт **5 минут**, одноразовый, после серии неверных попыток
(5) устройство блокируется на 5 минут. Новый код не обходит блокировку.

## Протокол

### 1. Запрос кода

Клиент шлёт обычный `POST /hello` с `pairRequest=true` (и `pair2=1`).
Сервер, если пары ещё нет (или proof не сошёлся), генерирует код,
показывает его оператору и отвечает:

```json
{
  "ok": true,
  "pair2": 1,
  "pairCodeRequired": true,
  "pairCodeExpiresAt": 1795297273095,
  "deviceId": "<server id>",
  "deviceName": "...",
  "groupId": "..."
}
```

При `auth-failed` (сервер уже с кем-то спарен) поля `pairCodeRequired`
и `deviceId` тоже приходят: клиент должен предложить код, а не
тупиковый отказ. Это позволяет переспариться после сброса одной стороны.

### 2. `POST /pair-begin`

Клиент генерирует эфемерную пару X25519 и 16-байтный nonce. Код
превращается в ключ:

```
codeKey = SHA256("mytetra-pair-code-v1" || base32decode(code))
codeMac = HMAC-SHA256(codeKey,
          "mytetra-pair-begin-v2" || 0 || serverId || 0 || clientId
          || 0 || base64(clientEph) || 0 || base64(clientNonce))
```

`|| 0 ||` — разделитель NUL, чтобы поля не склеивались неоднозначно.

Запрос:

```json
{
  "proto": "sync/1",
  "pair2": 1,
  "deviceId": "<client>",
  "deviceName": "...",
  "serverId": "<server>",
  "clientEph": "<base64 32B>",
  "clientNonce": "<base64 16B>",
  "codeMac": "<hex>"
}
```

Сервер проверяет `codeMac` сравнением без раннего выхода. Неверный
код — `pair-code-invalid`, истёк — `pair-code-expired`, нет активного
кода — `pair-code-missing`, лимит — `pair-rate-limited`.

### 3. Ответ сервера

Сервер генерирует свою эфемерную пару X25519 и nonce, считает:

```
shared    = X25519(serverPriv, clientEph)
transcript = "mytetra-pair-v2" || 0 || serverId || 0 || clientId
             || 0 || b64(clientEph) || 0 || b64(serverEph)
             || 0 || b64(clientNonce) || 0 || b64(serverNonce)
okm       = HKDF-SHA256(ikm=shared, salt=codeKey, info=transcript, 64)
K_enc     = okm[0..32)
K_mac     = okm[32..64)
secret    = random 32 bytes (hex, 64 символа)
cipher    = ChaCha20-Poly1305(K_enc, nonce, secret, aad=transcript)
          = nonce(12) || ciphertext || tag(16)
serverMac = HMAC-SHA256(K_mac,
            "mytetra-pair-finish-v2" || 0 || transcript
            || 0 || b64(cipher))
```

Ответ:

```json
{
  "ok": true,
  "serverEph": "<base64 32B>",
  "serverNonce": "<base64 16B>",
  "cipher": "<base64>",
  "serverMac": "<hex>",
  "codeExpiresAt": ...
}
```

Клиент проверяет `serverMac`, расшифровывает `cipher` и сохраняет
`secret`. Сервер сохраняет тот же секрет как **pending**.

### 4. Повышение pending -> current

Клиент шлёт `POST /hello` с proof от нового секрета. Сервер принимает
и текущий, и pending. Если proof сошёлся с pending:

1. pending повышается до текущего (`syncIdentity::promotePendingSecret`);
2. код гасится (`SyncPairing::consumeCode`);
3. обычный hello продолжается.

До первого успешного hello с новым секретом сервер продолжает
принимать и старый текущий секрет (переходный период), а при повторе
`pair-begin` отдаёт **тот же** pending-секрет, а не новый. Это
переживает потерю ответа и рестарт сервера: pending хранится в
`AppConfig`, код — в памяти (после рестарта нужен новый код).

## Криптография

| Примитив | Десктоп | Android |
|---|---|---|
| X25519 | Monocypher 4.x (`crypto_x25519`) | Tink (`X25519`) |
| HKDF-SHA256 | Qt HMAC (RFC 5869) | Tink (`Hkdf`) |
| ChaCha20-Poly1305 | Monocypher (`crypto_aead_init_ietf`/`write`) | Tink (`ChaCha20Poly1305`) |
| SHA-256/HMAC | Qt | JCA |
| Случайность | `QRandomGenerator::system()` | `SecureRandom` |

Зависимости: Monocypher (BSD-2/CC0, вендорен в
`thirdParty/monocypher`), Tink for Android (Apache 2.0, Maven
`com.google.crypto.tink:tink-android:1.13.0`). OpenSSL не нужен:
на десктопе его заголовков нет, а для Windows-портабла не хочется
тащить import-libs.

Общего кода нет; совместимость доказывается **общими векторами**:

- X25519 — RFC 7748 §6.1;
- HKDF-SHA256 — RFC 5869 Test Case 1;
- ChaCha20-Poly1305 — RFC 8439 §2.8.2;
- base32 — RFC 4648;
- полный вектор `pair-begin`/`pair-finish` (фиксированные ключи,
  nonce, код, id) — одинаковые `codeMac`, `cipher`, `serverMac`,
  `secret` в C++ и Kotlin.

Тесты:

```
make -C app/tests run
# sync_pair_crypto_test, sync_pairing_test
JAVA_HOME=... ./gradlew :app:testFreeDebugUnitTest --tests "*SyncPairingTest*"
```

## Почему не PAKE

При 80 битах кода офлайн-перебор захваченного рукопожатия нереален, а
активный MITM получает не больше одной онлайн-попытки (лимит). PAKE
(SPAKE2/OPAQUE) нужен для **коротких** PIN, где иначе один перехват
даёт офлайн-оракул. Если когда-нибудь понадобится 6-значный код,
протокол придётся заменить на PAKE; текущий 80-битный код — осознанный
компромисс без новой тяжёлой зависимости.

## Модель угроз: что защищено

- **Офлайн-перебор из захваченного handshake** — 80 бит + X25519,
  нереально.
- **Подсмотренный/сфотографированный код после спаривания** — бесполезен:
  он одноразовый и погашен, долговременный секрет случайный.
- **Перехват `pair-begin`/ответа** — без кода и эфемерных ключей
  долговременный секрет не достать; MAC/тег ловят подмену.
- **Подмена сервера/MITM** — клиент проверяет `serverMac`, выведенный из
  кода и транскрипта; без кода подделать нельзя.
- **Кража старого PIN** — PIN больше не используется в GUI; легаси-PIN
  остался только для headless-стендов.

## Что остаётся

- **Физическая камера в окно спаривания**: кто видит код на экране
  сервера в течение 5 минут, может успеть спариться. Это не лечится
  криптографией; помогает TTL, одноразовость, лимит и то, что код
  показывается только по явному запросу.
- **Скомпрометированная конечная точка**: если злоумышленник уже читает
  экран/память сервера, он прочитает и код, и секрет.
- **HTTP без TLS в LAN**: трафик не шифруется транспортом; данные
  защищены proof'ами, но метаданные (id, имена) видны. Это отдельная
  задача (TLS/Noise), не в этой волне.
- **Headless-режим** печатает код в stdout (`SYNC PAIR CODE ...`) —
  консоль должна быть защищена; это сделано для автоматизации, где
  попапа нет.
- **Фингерпринт пары** от секрета реализован
  (`SyncIdentity::pairFingerprint`), но пока не обязателен в UI; сверка
  кода на экране остаётся основным человеческим контролем.

## Совместимость

- Новый клиент шлёт `pair2=1`; новый сервер отвечает `pair2=1` и умеет
  `/pair-begin`.
- Старый клиент (без `pair2`) и старый сервер продолжают работать по
  прежней PIN-схеме, если она настроена (`--sync-pair`). GUI нового
  десктопа PIN не предлагает.
- Старый клиент, пытающийся спариться с новым GUI-сервером без
  `--sync-pair`, получит `auth-failed`: обновите обе стороны.
- Пара, созданная старым PIN-путём, продолжает работать: секрет просто
  остаётся текущим, pending появляется только после кода.

## Файлы

- `app/src/libraries/sync/SyncPairCrypto.{h,cpp}` — примитивы, base32,
  HKDF, AEAD.
- `app/src/libraries/sync/SyncPairing.{h,cpp}` — код, TTL, лимит,
  `pair-begin`/`pair-finish`.
- `app/src/libraries/sync/SyncPairFlow.{h,cpp}` — общий путь установки
  пары для GUI и сессии.
- `app/src/libraries/sync/SyncHandshake.cpp` — `pair2` в hello,
  `/pair-begin`, принятие pending, повышение.
- `app/src/views/sync/SyncPairDialogs.{h,cpp}` — ввод кода (клиент) и
  показ кода (сервер).
- `app/src/models/appConfig/AppConfig.cpp` — pending-секрет.
- Android: `SyncPairCrypto.kt`, `SyncPairing.kt`,
  `SyncProtocolClient.pairWithCode`, `SyncSessionRunner`.

## Дополнение: data encryption, TLS, QR (1.44.430)

### End-to-end шифрование данных (capability `enc:1`)

Секрет пары наконец используется не только как пропуск, но и как ключ
данных:

- клиент в `hello` объявляет `enc=1`, случайную `session` (16 байт) и
  `proof2 = HMAC(S, helloRequestChallenge)`, где challenge покрывает
  `clientId`, `clientTime`, `session`, `groupId`, `deviceName`,
  `pairRequest`, `enc`;
- старый `proof` остаётся для совместимости со старыми серверами;
- сервер проверяет `proof2`, эхом отдаёт `session` и `helloMac` —
  HMAC от фиксированного набора полей ответа (ok/paired/groupId/
  deviceName/pairCode*/warning/error/detail). Клиент проверяет MAC,
  поэтому ответ hello тоже аутентифицирован;
- из `S` + `session` + id выводятся `K_c2s`/`K_s2c`
  (`HKDF-SHA256`, salt `mytetra-data-v1`);
- тело каждого data-запроса/ответа шифруется ChaCha20-Poly1305;
  снаружи остаются `_enc`, `_dev`, `_ts`, `_s`, `_ct`. AEAD-тег
  заменяет старый `_proof` и покрывает тело;
- AAD привязывает route, оба id, session и timestamp; timestamp
  проверяется в пределах 5 секунд;
- сервер хранит активную сессию устройства (ставится на hello) и
  отвергает data-запросы со старой/чужой сессией;
- флаг `enc` липкий в списке устройств: если устройство уже
  шифровалось, а сервер вдруг предлагает plain, клиент отказывается
  (`enc-downgrade`), а не откатывается молча;
- старые клиенты/серверы без `enc` продолжают работать по старому
  HMAC-пути; новые — шифруют.

Тесты: `sync_data_crypto_test` (C++) и `SyncDataCryptoTest` (Kotlin)
с общими векторами; полный E2E на живом headless-сервере:
pairRequest → code → pair-begin → hello с proof2/helloMac → encrypted
`/manifest` → decrypt.

### TLS/HTTPS

- сервер: `--sync-tls-cert`/`--sync-tls-key` (PEM; Let's Encrypt
  fullchain/privkey или self-signed), `--sync-no-tls`;
- тот же порт говорит только TLS (не смешиваем plain/TLS на одном
  порту — никакого downgrade);
- discovery-ответ добавляет `tls`/`tlsFingerprint`;
- клиент: режим `off|prefer|require`, CA-подписанный сертификат
  проходит по системному store, self-signed — по пину SHA256
  (QR/TOFU); при `prefer` откат на HTTP только если сертификат
  вообще не предъявлялся, при ошибке сертификата — отказ;
- мобильный клиент: `HttpsURLConnection`, CA по умолчанию,
  self-signed — `X509TrustManager` с пином из QR;
- headless-сервер печатает `SYNC PAIR URI ...` с `tls`/`fp`.

TLS-сервер проверен E2E (Python-клиент по HTTPS); десктопный и
мобильный клиенты собираются и используют общий `SyncClient`/
`SyncProtocolClient`.

### QR

Сервер показывает QR в том же попапе, где код:

```
mytetra://pair?v=1&host=...&port=...&code=...&tls=1&fp=...&name=...
```

Код по-прежнему привязан к устройству, приславшему `pairRequest`,
поэтому QR имеет смысл только после запроса с телефона. Телефон
сканирует, подставляет host/port/TLS/пин и использует тот же
pair-begin. В headless QR заменён строкой `SYNC PAIR URI`.

### Запрет пар и привязка к IP

- `Block new pairings` (меню Sync и окно устройств): сервер не
  генерирует коды и не принимает `/pair-begin`; уже спаренные
  устройства синкаются. Включение отменяет незавершённые пары
  (коды + pending-секреты).
- `Bind code to client IP`: код принимается только с IP, приславшего
  `pairRequest`. Полезно в LAN с уникальными IP; не включать за
  NAT/обратным прокси и при смене IP клиента между запросом и
  `pair-begin`.
