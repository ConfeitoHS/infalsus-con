# infalsus-con

> **이 브랜치는 더 이상 업데이트하지 않습니다.** nice!nano 지원은 `main` 브랜치로 합쳐졌습니다. `main`에서 `pio run -e nrf52840-hw1_1` (초기 배선은 `nrf52840-hw1_0`)로 빌드하세요.

RP2040 Pro Micro 또는 nice!nano(nRF52840) 기반 USB HID 게임 컨트롤러. 버튼 6개(키보드)와 슬라이드 포텐셔미터(마우스 X)를 지원하고, 버튼마다 LED가 켜집니다.

## 하드웨어

- **MCU**: RP2040 Pro Micro (Sea-Picro · SparkFun Pro Micro RP2040 · 클론) 또는 nice!nano v2 / SuperMini nRF52840
- **버튼**: 택트 스위치 ×6 → 핀과 GND 사이 (내부 풀업)
- **LED**: ×6, GPIO → 220Ω → LED → GND 직결 (nice!nano는 2N2222 경유)
- **슬라이더**: 10kΩ 리니어 포텐셔미터, 양 끝 3.3V/GND, 가운데 핀 → ADC
- **콘덴서**: 100nF (슬라이더 신호–GND), 10µF (3.3V–GND)

핀 배정과 배선도는 `include/config.h` 상단 주석을 보세요.

| 부품 | Pro Micro 위치 | nice!nano 라벨 | RP2040 GPIO | 키 |
|---|---|---|---|---|
| SW1–SW6 | A1 A0 D15 D14 D16 D10 (오른쪽) | 002 115 113 111 010 009 | 27 26 22 20 23 21 | Shift A S D F Space |
| LED1–LED6 | D4–D9 (왼쪽) | 022 024 100 011 104 106 | 4–9 | (같은 순서) |
| 슬라이더 | A2 | 029 | 28 | 마우스 X |
| 케이블 감지 | A3 | 031 | 29 | 4극 잭 R2 (꽂히면 GND) |

## 빌드 & 업로드

빌드 환경 이름은 `보드-hw버전`입니다. 기본 환경은 `rp2040-hw1_1`입니다.

| 환경 | 보드 | 하드웨어 |
|---|---|---|
| `rp2040-hw1_1` | RP2040 Pro Micro | hw1.1 (버튼 오른쪽, LED 왼쪽) |
| `nrf52840-hw1_1` | nice!nano | hw1.1 (버튼 오른쪽, LED 왼쪽) |
| `nrf52840-hw1_0` | nice!nano | hw1.0 (버튼 왼쪽, LED 오른쪽, 초기 배선) |

```bash
pio run              # 빌드 → .pio/build/rp2040-hw1_1/firmware.uf2
pio run -t upload    # 업로드
pio device monitor   # 시리얼 모니터 (115200) — 슬라이더 ADC 값 출력
```

드래그앤드롭: BOOTSEL을 누른 채 USB를 꽂으면 `RPI-RP2` 드라이브가 뜨고, 거기에 `firmware.uf2`를 복사합니다. 시리얼 모니터가 열려 있으면 업로드가 실패합니다.

nice!nano 보드는 `pio run -e nrf52840-hw1_1 [-t upload]`. RST를 GND에 두 번 빠르게 터치하면 `NICENANO` 드라이브가 뜨고 `.pio/build/nrf52840-hw1_1/firmware.uf2`를 복사하면 됩니다.

nice!nano용 보드 정의(`boards/`)와 핀 variant(`variants/nice_nano/`)가 저장소에 포함돼 있고, `scripts/install_variant.py`가 빌드 전에 프레임워크로 복사합니다.

### hw1.0 (초기 배선) 보드

버튼이 왼쪽(D2–D7), LED가 오른쪽(D10 D16 D14 D15 A0 A1)에 있는 초기 nice!nano 보드는 `nrf52840-hw1_0` 환경으로 빌드합니다. RP2040은 hw1.1만 지원합니다.

```bash
pio run -e nrf52840-hw1_0
```

### 리셋 / 진단 펌웨어

```bash
pio run -e reset -t upload
```

nice!nano용. 저장된 설정(LED 밝기)을 지우고, LED를 계속 순서대로 돌리며, 시리얼 모니터에 버튼·슬라이더·케이블 상태를 초당 4번 출력합니다. 배선을 점검하거나 보드를 초기 상태로 되돌릴 때 쓰고, 끝나면 일반 펌웨어를 다시 올리세요. UICR(USB 전압 설정 포함)은 건드리지 않습니다.

## 동작

- 부팅 시 LED1→6이 순서대로 한 번씩 켜집니다 (배선 확인용).
- 버튼을 누르면 해당 키가 전송되고 LED가 켜집니다. 6키 동시 입력, 1000Hz 폴링.
- 슬라이더 움직임이 마우스 X 이동(상대)으로 전송됩니다. `MOUSE_MODE_RELATIVE 0`으로 바꾸면 절대 좌표 모드.
- **수동 센터 보정**: 게임이 커서를 가운데로 옮긴 직후 **A S D F + (Shift 또는 Space) 다섯 개를 동시에, 0.5초 안에 4번** 누르면, 슬라이더가 가운데에서 떨어진 만큼을 상대 이동 한 번으로 보내 게임 커서와 슬라이더를 맞춥니다. LED가 두 번 깜빡이면 전송 완료.
- 슬라이더 유닛은 3.5mm 4극 잭으로 분리됩니다. 케이블을 꽂으면 LED가 한 번, 뽑으면 두 번 깜빡이고, 뽑힌 동안은 마우스를 보내지 않습니다.

### LED 밝기 설정

1. USB를 꽂고 **10초 안에 버튼 6개를 동시에** 누른 채로 유지합니다.
2. LED가 전부 켜지면 누른 채로 슬라이더를 움직여 밝기를 조절합니다 (실시간 반영).
3. 버튼에서 손을 떼면 저장됩니다. LED가 두 번 깜빡이면 완료.

밝기는 내부 플래시에 저장되어 전원을 뽑아도 유지됩니다.

## 웹 설정 페이지

크롬이나 엣지에서 **https://confeitohs.github.io/infalsus-con/config/** 를 열고 "컨트롤러 연결"을 누르면 LED 밝기, 슬라이더 감도(전체 이동 픽셀 수), 방향 반전, 센터 위치를 바꾸고 저장할 수 있습니다. 설치할 것은 없고, 윈도우·맥·리눅스 모두 됩니다. 오른쪽 위에서 한국어·English·简体中文·日本語를 고를 수 있고, 처음에는 브라우저 언어를 따릅니다. 펌웨어 1.3.0 이상이 필요합니다.

- 페이지 파일은 메인 브랜치의 `docs/config/index.html`이고, 이 브랜치의 펌웨어도 같은 페이지를 씁니다. 보드가 없어도 "데모 모드"로 화면을 확인할 수 있습니다.
- 페이지는 `info` 응답을 보고 그 펌웨어가 지원하는 설정만 보여 줍니다. 통신 형식(`proto`)이 다르면 연결을 거부합니다.
- 시리얼 모니터 등 다른 프로그램이 포트를 열고 있으면 연결되지 않습니다.
- 주소가 열리지 않으면 GitHub 저장소 **Settings → Pages**에서 Source를 **Deploy from a branch**, 브랜치 `main`, 폴더 `/docs`로 지정하세요.

## 설정 명령어 (USB 시리얼)

펌웨어 1.3.0부터 USB 시리얼(115200)로 설정을 읽고 바꿀 수 있습니다. 설정 도구가 이 명령어를 쓰고, 시리얼 모니터에서 직접 입력해도 됩니다. 한 줄에 명령 하나이고, 응답은 항상 `{`로 시작하는 JSON 한 줄입니다.

| 명령 | 동작 |
|---|---|
| `info` | 펌웨어 버전, 보드, 하드웨어 버전(배선), 슬라이더 종류, 지원 설정(`caps`), 범위, 기본값, 현재값 |
| `get` | 현재 설정 |
| `set brightness <0-255>` | LED 밝기. 바꾸면 LED 전체가 잠깐 켜져 미리 보여 줌 |
| `set px <100-30000>` | 슬라이더 전체 이동에 해당하는 픽셀 수 (기본 3840) |
| `set reverse <0\|1>` | 슬라이더 좌우 반전 |
| `set center <0-32767>` | 센터 보정 조합이 센터로 삼는 슬라이더 위치 (기본 16384 = 가변저항 한가운데) |
| `center` | 지금 슬라이더 위치를 센터로 설정 |
| `pos` | 지금 슬라이더 위치 (0-32767, 연결 안 됨이면 -1) |
| `save` | 현재 설정을 플래시에 저장 |
| `revert` | 저장된 설정으로 되돌림 |
| `defaults` | 기본값으로 바꿈 (`save`해야 저장) |

`set`은 바로 적용되지만 `save` 전에는 저장되지 않습니다. 응답의 `dirty`가 `true`면 저장 안 된 변경이 있다는 뜻입니다.

`info` 응답 예:

```json
{"ok":true,"type":"info","name":"infalsus-con","fw":"1.3.0","proto":1,"board":"nrf52840-nicenano","hw":"1.1","slider":"pot","buttons":6,"caps":["brightness","px","reverse","center"],"limits":{"brightness":[0,255],"px":[100,30000],"reverse":[0,1],"center":[0,32767]},"defaults":{"brightness":255,"px":3840,"reverse":0,"center":16384},"settings":{"brightness":255,"px":3840,"reverse":0,"center":16384},"dirty":false}
```

설정 도구는 `proto`로 명령어 형식 호환을 확인하고, `board`·`hw`·`slider`·`caps`를 보고 이 하드웨어에서 가능한 옵션만 보여 주면 됩니다. 이전 펌웨어에서 저장한 LED 밝기는 처음 부팅할 때 그대로 가져옵니다.

`hw`는 배선 버전입니다. `nrf52840-hw1_0`으로 빌드하면 `1.0`, 나머지는 `1.1`로 보고됩니다. `board`는 nice!nano가 `nrf52840-nicenano`, RP2040이 `rp2040-promicro`입니다.

## 파라미터

`include/config.h`에서 조정:

- `BUTTON_KEYS`: 키 매핑
- `DEBOUNCE_MS`: 버튼 디바운스 (20)
- `FW_VERSION`, `BOARD_ID`, `HW_REV`, `SLIDER_TYPE`: `info`로 보고되는 식별 정보
- `MOUSE_INVERT_X`: 이 배선의 기본 슬라이더 방향 (-1 / 1). `reverse` 설정이 여기서 한 번 더 뒤집음
- `MOUSE_MODE_RELATIVE`: 1 = 상대 이동(기본), 0 = 절대 좌표
- `MOUSE_REL_PIXELS_PER_TRAVEL`: `px` 설정의 기본값 (3840). 범위는 `PX_PER_TRAVEL_MIN`/`MAX`
- `SLIDER_CENTER_DEFAULT`: `center` 설정의 기본값 (16384 = 가변저항 전기적 한가운데)
- `RECENTER_CHORD_MASK`, `RECENTER_CHORD_ANY_MASK`, `RECENTER_TAPS`, `RECENTER_WINDOW_MS`: 센터 보정 제스처 (ASDF + Shift/Space / 4번 / 500ms)
- `SLIDER_ADC_MAX`: 슬라이더 끝에서 읽히는 ADC 최대값 (4060)
- `SLIDER_OVERSAMPLE`, `SLIDER_SMOOTHING_MIN/MAX`, `SLIDER_SPEED_GAIN`: 적응형 노이즈 필터
- `SLIDER_MIN_STEP`: 움직이는 중 이만큼 변해야 전송 (16)
- `SLIDER_WAKE_STEP`, `SLIDER_REST_MS`: 정지 판정 후 다시 움직임으로 인정하는 최소 이동(64)과 정지 판정 시간(150ms)
- `POLL_INTERVAL_MS`: 폴링 주기 (1)
- `BRIGHTNESS_WINDOW_MS`: 밝기 설정 모드 진입 가능 시간 (10000)
- `LED_ACTIVE_LOW`: LED가 핀 LOW에서 켜지는 배선이면 1 (부팅 시 전부 켜지고 누르면 꺼지는 증상)
