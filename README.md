# infalsus-con

ESP32-C3 기반 USB HID 게임 컨트롤러. 6개 버튼(키보드)과 슬라이드 포텐셔미터(마우스 X축)를 지원합니다.

## 하드웨어

- **MCU**: ESP32-C3 DevKitC-02
- **버튼**: 택트 스위치 x6
- **슬라이더**: SC-1009G (B10K x2, 듀얼 리니어 포텐셔미터)

## 배선도 (Wiring)

### 버튼 (Active LOW, 내부 풀업 사용)

각 버튼의 한쪽 핀을 GPIO에, 다른 쪽을 GND에 연결합니다.

| 버튼 | GPIO | 매핑 키 |
|------|------|---------|
| 1 | GPIO1 | Left Shift |
| 2 | GPIO2 | A |
| 3 | GPIO3 | S |
| 4 | GPIO4 | D |
| 5 | GPIO5 | F |
| 6 | GPIO6 | Space |

### 슬라이더 (SC-1009G)

듀얼 포텐셔미터이므로 한쪽 트랙만 사용합니다. 핀이 3개인 쪽(또는 6핀 중 한쪽 3핀)을 사용하세요.

```
[슬라이더 핀 배치 - 한쪽 트랙 3핀]

  핀1 (한쪽 끝)  →  3.3V
  핀2 (와이퍼)   →  GPIO0
  핀3 (다른 끝)  →  GND
```

> **주의**: 반드시 **3.3V**를 사용하세요. 5V를 연결하면 ESP32-C3의 ADC가 손상될 수 있습니다.

### USB 연결

ESP32-C3의 네이티브 USB 포트(USB 마이크로/Type-C)를 PC에 직접 연결합니다.
UART 브리지 포트가 아닌 **네이티브 USB 포트**를 사용해야 합니다.

## 빌드 & 업로드

```bash
# PlatformIO CLI
pio run              # 빌드
pio run -t upload    # 업로드
pio device monitor   # 시리얼 모니터
```

## 키 매핑 변경

`include/config.h`에서 `BUTTON_KEYS` 배열을 수정하세요.

## 파라미터 조정

`include/config.h`에서 조정 가능한 값:

- `DEBOUNCE_MS`: 버튼 디바운스 시간 (기본: 20ms)
- `SLIDER_DEADZONE`: 슬라이더 미세 떨림 무시 범위 (기본: 30)
- `MOUSE_SPEED`: 슬라이더→마우스 이동 속도 배율 (기본: 8)
- `POLL_INTERVAL_MS`: 입력 폴링 주기 (기본: 2ms)
