# infalsus-con

nice!nano v2 (nRF52840) 기반 USB HID 게임 컨트롤러. 버튼 6개(키보드)와 슬라이드 포텐셔미터(마우스 X, 절대 좌표)를 지원하고, 버튼마다 LED가 켜집니다.

## 하드웨어

- **MCU**: nice!nano v2 (Pro Micro 호환 nRF52840)
- **버튼**: 택트 스위치 ×6 → 핀과 GND 사이 (내부 풀업)
- **LED**: ×6, 2N2222 트랜지스터로 구동 (GPIO → 4.7kΩ → B, C → LED → 220Ω → 3.3V)
- **슬라이더**: 10kΩ 리니어 포텐셔미터, 양 끝 3.3V/GND, 가운데 핀 → ADC
- **콘덴서**: 100nF (슬라이더 신호–GND), 10µF (3.3V–GND)

핀 배정과 배선도는 `include/config.h` 상단 주석을 보세요.

| 부품 | 보드 라벨 | 키 |
|---|---|---|
| SW1–SW6 | 002 115 113 111 010 009 (오른쪽) | Shift A S D F Space |
| LED1–LED6 | 022 024 100 011 104 106 (왼쪽) | (같은 순서) |
| 슬라이더 | 029 | 마우스 X |
| 슬라이더 케이블 감지 | 031 | 4극 잭 R2 (꽂히면 GND) |

## 빌드 & 업로드

```bash
pio run              # 빌드
pio run -t upload    # 업로드 (1200bps 터치로 부트로더 자동 진입)
pio device monitor   # 시리얼 모니터 (115200) — 슬라이더 ADC 값 출력
```

자동 진입이 안 되면 RST를 GND에 두 번 빠르게 터치해 `NICENANO` 드라이브가 뜬 상태에서 업로드하세요. 시리얼 모니터가 열려 있으면 업로드가 실패합니다.

nice!nano용 보드 정의(`boards/`)와 핀 variant(`variants/nice_nano/`)가 저장소에 포함돼 있고, `scripts/install_variant.py`가 빌드 전에 프레임워크로 복사합니다.

## 동작

- 부팅 시 LED1→6이 순서대로 한 번씩 켜집니다 (배선 확인용).
- 버튼을 누르면 해당 키가 전송되고 LED가 켜집니다. 6키 동시 입력, 1000Hz 폴링.
- 슬라이더 움직임이 마우스 X 이동(상대)으로 전송됩니다. 게임이 곡 시작에 커서를 가운데로 옮겨도 그 지점이 기준이 되므로 따로 맞출 필요가 없습니다. `MOUSE_MODE_RELATIVE 0`으로 바꾸면 절대 좌표 모드.
- 슬라이더 유닛은 3.5mm 4극 잭으로 분리됩니다. 케이블을 꽂으면 LED가 한 번, 뽑으면 두 번 깜빡이고, 뽑힌 동안은 마우스를 보내지 않습니다.

### LED 밝기 설정

1. USB를 꽂고 **10초 안에 버튼 6개를 동시에** 누른 채로 유지합니다.
2. LED가 전부 켜지면 누른 채로 슬라이더를 움직여 밝기를 조절합니다 (실시간 반영).
3. 버튼에서 손을 떼면 저장됩니다. LED가 두 번 깜빡이면 완료.

밝기는 내부 플래시에 저장되어 전원을 뽑아도 유지됩니다.

## 파라미터

`include/config.h`에서 조정:

- `BUTTON_KEYS`: 키 매핑
- `DEBOUNCE_MS`: 버튼 디바운스 (20)
- `MOUSE_INVERT_X`: 슬라이더 방향 (-1 / 1)
- `MOUSE_MODE_RELATIVE`: 1 = 상대 이동(기본), 0 = 절대 좌표
- `MOUSE_REL_PIXELS_PER_TRAVEL`: 상대 모드에서 슬라이더 전체 이동이 움직이는 픽셀 수 (1920)
- `SLIDER_ADC_MAX`: 슬라이더 끝에서 읽히는 ADC 최대값 (4060)
- `SLIDER_OVERSAMPLE`, `SLIDER_SMOOTHING_MIN/MAX`, `SLIDER_SPEED_GAIN`: 적응형 노이즈 필터
- `SLIDER_MIN_STEP`: 움직이는 중 이만큼 변해야 전송 (16)
- `SLIDER_WAKE_STEP`, `SLIDER_REST_MS`: 정지 판정 후 다시 움직임으로 인정하는 최소 이동(64)과 정지 판정 시간(150ms)
- `POLL_INTERVAL_MS`: 폴링 주기 (1)
- `BRIGHTNESS_WINDOW_MS`: 밝기 설정 모드 진입 가능 시간 (10000)
