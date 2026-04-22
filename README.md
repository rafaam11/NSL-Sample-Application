# NSL-Sample-Application

NanoSystems 사의 ToF LiDAR 제품군(**NSL-3130AA / NSL-1110AA / NSL-3140AA**)용 Windows · Linux 샘플 애플리케이션입니다. OpenCV 창에 거리(Distance), 진폭(Amplitude), 흑백(Grayscale) 영상을 좌우로 나란히 표시하고, 선택 사항으로 SSD-MobileNet v2를 사용한 실시간 사람 검출까지 지원합니다. USB(CDC/Vendor) 및 Ethernet(제어 TCP, 데이터 UDP) 연결을 모두 지원합니다.

---

## 목차

1. [저장소 구조](#1-저장소-구조)
2. [빠른 시작](#2-빠른-시작-quick-start)
3. [환경 준비](#3-환경-준비)
4. [빌드](#4-빌드)
5. [실행 방법](#5-실행-방법)
6. [커맨드라인 옵션](#6-커맨드라인-옵션)
7. [런타임 키보드 · 마우스 조작](#7-런타임-키보드--마우스-조작)
8. [딥러닝 모드 (선택 기능)](#8-딥러닝-모드-선택-기능)
9. [소스 구조 및 아키텍처](#9-소스-구조-및-아키텍처)
10. [nsl_lib API 레퍼런스 요약](#10-nsl_lib-api-레퍼런스-요약)
11. [트러블슈팅](#11-트러블슈팅)
12. [라이선스 · 문의](#12-라이선스--문의)

---

## 1. 저장소 구조

```
NSL-Sample-Application/
├── README.md                       ← 본 문서
├── .gitmodules                     ← 서브모듈 3개 정의
├── deepLearning/                   ← SSD-MobileNet v2 모델 (선택 기능)
│   ├── frozen_inference_graph.pb
│   └── ssd_mobilenet_v2.pbtxt
├── OPENCV/                         ← 서브모듈: OpenCV 4.5.4 배포 헬퍼 리포
├── PCL/                            ← 서브모듈: PCL 1.12.0 인스톨러 헬퍼 리포
└── NSL-Sample-Application/
    ├── CMakeLists.txt              ← Linux/aarch64 빌드 설정
    ├── main.cpp                    ← 진입점·메인 루프
    ├── videoSource.h / .cpp        ← CLI 파싱, 캡처, 시각화, 키 입력 처리
    ├── nsl_lib/                    ← 서브모듈: LiDAR SDK
    │   ├── include/nanolib.h       ← 공개 C/C++ API
    │   ├── lib/{windows,linux-7.5,aarch-7.5,mac-12.0.5}/{shared,static}/
    │   ├── script/install_libusb_{linux,macos}.sh
    │   └── DOC/ROBOSCAN(NSL-3140AA) SDK Software Manual.pdf
    ├── WinBuild/WinBuild.sln       ← Visual Studio 2019 솔루션
    └── WinBinary/binary/           ← 사전 빌드된 Windows 실행 파일
        ├── WinBuild.exe
        └── nanolib.dll
```

서브모듈 3개의 역할은 다음과 같습니다.

| 서브모듈 | 경로 | 역할 |
|---|---|---|
| `nsl_lib` | `NSL-Sample-Application/nsl_lib` | LiDAR 제어 SDK (빌드에 **필수**) |
| `PCL` | `PCL` | PCL 1.12.0 AllInOne 인스톨러 배포 리포 (Windows 설치 참고용) |
| `OPENCV` | `OPENCV` | OpenCV 4.5.4 배포 리포 (Windows 설치 참고용) |

---

## 2. 빠른 시작 (Quick Start)

### Windows — 사전 빌드 바이너리로 바로 실행

```powershell
# 1) 사전 준비: PCL 1.12.0 AllInOne, OpenCV 4.5.4 설치 후 PATH 등록 (3장 참고)
# 2) 실행 (카메라 IP가 192.168.0.220일 때)
cd NSL-Sample-Application\WinBinary\binary
WinBuild.exe ipaddr 192.168.0.220
```

### Linux — 소스 빌드 후 실행

```bash
git clone --recurse-submodules https://github.com/nano-roboscan/NSL-Sample-Application.git
cd NSL-Sample-Application/NSL-Sample-Application
mkdir build && cd build
cmake ..
make
./nsl-dev ipaddr 192.168.0.220
```

> CMakeLists.txt는 기본값으로 **aarch64(ARM64, 예: Jetson)** 라이브러리에 링크하도록 설정되어 있습니다. x86_64 Linux 환경은 [4.2절](#42-linux--aarch64-cmake)을 참고하여 경로를 전환하세요.

---

## 3. 환경 준비

### 3.1 공통 선행 작업

서브모듈까지 포함해서 clone합니다.

```bash
git clone --recurse-submodules https://github.com/nano-roboscan/NSL-Sample-Application.git
```

이미 clone한 경우:

```bash
git submodule update --init --recursive
```

### 3.2 Windows 환경

| 요소 | 버전 · 경로 |
|---|---|
| Visual Studio | **2019** (Platform Toolset `v143`, VS 2022 호환) + Windows SDK 10.0 |
| PCL (Point Cloud Library) | **1.12.0** — `PCL-1.12.0-AllInOne-msvc2019-win64.exe` |
| OpenCV | **4.5.4** |
| OpenNI2 | PCL 설치 시 포함 |

PCL 인스톨러 실행 후 생성되는 경로 (vcxproj의 기본 가정):

- include 경로: `C:\local\PCL 1.12.0\`
- 런타임 DLL 경로: `C:\Program Files\PCL 1.12.0\bin`
- OpenCV: `C:\local\OpenCv454\` (혹은 `OPENCV_DIR` 환경변수로 지정)

**실행 전 반드시 PATH에 DLL 경로를 추가**하세요.

```text
PATH=C:\Program Files\PCL 1.12.0\bin;C:\Program Files\OpenNI2\Redist;%OPENCV_DIR%bin;%PATH%;
```

### 3.3 Linux 환경

Ubuntu 18.04 + gcc 7.5 기준으로 nsl_lib가 빌드되어 있습니다. 기본 패키지 설치:

```bash
sudo apt install cmake build-essential libopencv-dev libomp-dev
```

**USB 접속 시 udev 규칙** — 펌웨어 버전에 따라 두 가지 방법이 있습니다.

1) 신규 펌웨어(≥ 3.16, 2026-01 이후 생산분): 제공 스크립트로 libusb 설치 + udev 규칙 등록

```bash
sudo ./NSL-Sample-Application/nsl_lib/script/install_libusb_linux.sh
```

스크립트는 VID `1fc9` / PID `0099, 0094` 규칙을 `/etc/udev/rules.d/99-myusb-sdk.rules`에 등록하고 `/dev/ttyLidar` 심볼릭 링크를 생성합니다.

2) 기존 펌웨어(수동 등록):

```bash
sudo vi /etc/udev/rules.d/defined_lidar.rules
# 아래 한 줄 추가
KERNEL=="ttyACM*", ATTRS{idVendor}=="1fc9", ATTRS{idProduct}=="0094", MODE:="0777",SYMLINK+="ttyLidar"

sudo service udev reload
sudo service udev restart
```

### 3.4 macOS 환경 (참고)

`nsl_lib/script/install_libusb_macos.sh`로 Homebrew 기반 libusb를 설치할 수 있으나, 제공되는 `libnanolib.dylib`는 서명되지 않았습니다. 앱 배포 시 직접 서명하세요.

---

## 4. 빌드

### 4.1 Windows (Visual Studio)

1. `NSL-Sample-Application\WinBuild\WinBuild.sln` 열기
2. 구성을 **Release \| x64** 또는 **Debug \| x64** 로 선택 (x86은 템플릿만 존재하므로 사용 금지)
3. 빌드 → 산출물: `WinBuild\x64\<Config>\WinBuild.exe`

주요 링커 의존성:

- `nanolib.lib` (from `NSL-Sample-Application/nsl_lib/lib/windows/shared/`)
- `opencv_world454.lib` / `opencv_world454d.lib`
- PCL 1.12, VTK 9.0, Boost 1.76, FLANN, Qhull, OpenNI2

주요 전처리 매크로: `_WINDOWS`, `BOOST_ALL_NO_LIB`, `FLANN_STATIC`, `DISABLE_*`, `_CRT_SECURE_NO_WARNINGS`.

> 참고: `SUPPORT_DEEPLEARNING` 매크로는 `videoSource.h:41`에 **소스 단에서 하드코딩**되어 있어 항상 활성화된 상태로 빌드됩니다. 비활성화하려면 해당 `#define`을 주석 처리한 뒤 재빌드하세요.

### 4.2 Linux / aarch64 (CMake)

CMakeLists.txt 23~26라인의 플랫폼 라이브러리 경로는 주석으로 토글합니다. 기본값은 `aarch-7.5`입니다.

```cmake
# find_library(NSL_LIB_PATH NAMES nanolib PATHS nsl_lib/lib/linux-7.5/shared REQUIRED)  # x86_64 Linux
  find_library(NSL_LIB_PATH NAMES nanolib PATHS nsl_lib/lib/aarch-7.5/shared REQUIRED)  # ARM64 (기본)
# find_library(NSL_LIB_PATH NAMES nanolib PATHS nsl_lib/lib/windows/shared REQUIRED)    # Windows
```

빌드:

```bash
cd NSL-Sample-Application/NSL-Sample-Application
mkdir build && cd build
cmake ..
make
```

산출물은 `build/nsl-dev`이며 rpath가 `$ORIGIN`으로 설정되어 같은 디렉터리에 `libnanolib.so`를 두면 배포 가능합니다.

---

## 5. 실행 방법

### 5.1 기본 실행

```bash
# Ethernet (기본 IP)
./nsl-dev ipaddr 192.168.0.220

# USB (Linux)
./nsl-dev ipaddr /dev/ttyLidar

# 도움말
./nsl-dev -help

# 조합 예시
./nsl-dev -captureType 1 ipaddr 192.168.0.220
./nsl-dev -captureType 3 -intTime 1200 -amplitudeMin 80 -maxDistance 8000
```

인자 없이 실행하면 `ipaddr=192.168.0.220`, `captureType=3` (DISTANCE + AMPLITUDE) 기본값으로 시도합니다.

### 5.2 Windows 사전 빌드 바이너리

`WinBinary/binary/WinBuild.exe`와 동일 폴더에 `nanolib.dll`이 이미 포함되어 있습니다. PCL · OpenCV · OpenNI2 DLL은 3.2절의 PATH 등록이 선행되어야 로딩됩니다.

```powershell
WinBinary\binary\WinBuild.exe ipaddr 192.168.0.220
```

---

## 6. 커맨드라인 옵션

`NSL-Sample-Application/videoSource.cpp`의 `createApp()`이 파싱하는 옵션 전체 목록입니다.

| 옵션 | 기본값 | 허용 범위 | 설명 |
|---|---|---|---|
| `-help` | — | — | 내장 도움말 출력 후 종료 |
| `ipaddr <addr>` | `192.168.0.220` | IP 또는 `/dev/ttyLidar` | 센서 접속 주소 |
| `-captureType <n>` | `3` | 1\~8 (아래 표) | 동작 모드 |
| `-intTime <μs>` | `800` | 0\~4000 | Distance Integration Time |
| `-grayintTime <μs>` | `100` | 0\~50000 | Grayscale Integration Time |
| `-maxDistance <mm>` | `12500` | 0\~12500 | 컬러맵 최대 거리 |
| `-amplitudeMin <n>` | `50` | 30\~1000 | Amplitude 하한 (미만 = `LOW_AMPLITUDE`) |
| `-edgeThresHold <n>` | `0` | 0=off / 1\~10000 | Edge 검출 임계값 |
| `-medianFilterSize <n>` | `0` | 0\~99 | Median 필터 커널 크기 |
| `-medianIter <n>` | `0` | 0\~10000 | Median 필터 반복 횟수 |
| `-gaussIter <n>` | `0` | 0\~10000 | Gauss 필터 반복 횟수 |
| `-medianFilterEnable <0\|1>` | `0` | — | Median 필터 on/off |
| `-averageFilterEnable <0\|1>` | `0` | — | Average 필터 on/off |
| `-temporalFactor <n>` | `0` | 0\~1000 | Temporal 필터 계수 |
| `-temporalThresHold <n>` | `0` | 0\~1000 | Temporal 필터 임계값 |
| `-interferenceEnable <0\|1>` | `0` | — | Interference 필터 on/off |
| `-interferenceLimit <n>` | `0` | 0\~1000 | Interference 한계값 |
| `-inputSize <0\|1>` | `0` | 0=320, 1=416 | 딥러닝 입력 해상도 힌트 |
| `-thresh <f>` | `0.4` | 0.0\~1.0 | 딥러닝 confidence 임계값 |

> `-help` 출력에는 `-medianFilter`로 표기되지만 실제 파서 키는 `-medianFilterSize`입니다 (`videoSource.cpp:151`).

#### `-captureType` 상세

| 값 | 모드 |
|---|---|
| 1 | `DISTANCE_MODE` — 거리만 |
| 2 | `GRAYSCALE_MODE` — 흑백만 |
| 3 | `DISTANCE_AMPLITUDE_MODE` (기본) — 거리 + Amplitude |
| 4 | `DISTANCE_GRAYSCALE_MODE` — 거리 + Grayscale |
| 5 | `RGB_MODE` |
| 6 | `RGB_DISTANCE_MODE` |
| 7 | `RGB_DISTANCE_AMPLITUDE_MODE` |
| 8 | `RGB_DISTANCE_GRAYSCALE_MODE` |

---

## 7. 런타임 키보드 · 마우스 조작

`videoSource.cpp`의 `prockey()`가 처리하는 키 이벤트입니다. 실행 중에 `h` 키를 누르면 콘솔에도 동일한 목록이 출력됩니다.

| 키 | 동작 |
|---|---|
| `b` / `B` | Grayscale 모드 전환 |
| `d` / `D` | Distance + Grayscale 모드 |
| `a` / `A` | Amplitude + Distance 모드 |
| `e` / `E` | Amplitude(log) + Distance 모드 (ColorRange 토글) |
| `t` / `T` | Grayscale LED 조명 on/off |
| `s` / `S` | Saturation 표시 on/off |
| `f` / `F` | ADC Overflow 표시 on/off |
| `0` | HDR off |
| `1` | HDR Spatial |
| `2` | HDR Temporal |
| `h` / `H` | 콘솔에 단축키 도움말 출력 |
| `ESC` | 종료 (`stopLidar()` → `nsl_close()`) |
| 마우스 좌클릭 | 클릭한 픽셀의 좌표와 거리값을 캡션에 표시 |

---

## 8. 딥러닝 모드 (선택 기능)

샘플 앱은 SSD-MobileNet v2 TensorFlow 모델로 사람(class=1)을 실시간 검출해 초록 박스로 표시합니다. 활성화 조건은 다음과 같습니다.

1. **컴파일 타임**: `videoSource.h:41`의 `#define SUPPORT_DEEPLEARNING`이 정의되어 있어야 합니다 (기본 활성). 끄려면 해당 줄을 주석 처리하고 재빌드하세요.
2. **런타임 파일 경로**: 실행 파일 기준 상대 경로에 모델이 있어야 합니다.
   - Linux: `../../deepLearning/frozen_inference_graph.pb`, `../../deepLearning/ssd_mobilenet_v2.pbtxt`
   - Windows: `..\..\..\deepLearning\frozen_inference_graph.pb`, `..\..\..\deepLearning\ssd_mobilenet_v2.pbtxt`
   - 파일이 없으면 `mismatch deepLearning weight files !!!` 메시지가 출력되고 딥러닝만 비활성화됩니다 (앱은 계속 동작).
3. **CUDA 가속 (선택)**: 빌드에 `HAVE_CV_CUDA` 매크로를 추가하면 `DNN_BACKEND_CUDA / DNN_TARGET_CUDA`로 동작하며, 아니면 CPU 백엔드를 사용합니다.

실행 시 `-thresh 0.4` 옵션으로 confidence 임계값을 조정할 수 있습니다.

---

## 9. 소스 구조 및 아키텍처

### 9.1 실행 흐름

```text
main.cpp
 ├─ createApp(argc, argv, &camOpt)          ─ videoSource.cpp
 │    ├─ print_help            (-help 시 종료)
 │    ├─ initAppCfg            CLI 파싱 · CaptureOptions 기본값
 │    └─ nsl_open(ipaddr, &nslDevConfig, FUNC_ON)  →  handle
 ├─ lidarSrc->initDeepLearning(&camOpt)     [SUPPORT_DEEPLEARNING 일 때]
 ├─ lidarSrc->setLidarOption(&camOpt)
 │    └─ nsl_setIntegrationTime / MinAmplitude / Filter
 │       nsl_setGrayscaleillumination / ColorRange / streamingOn
 │
 └─ while (!signal_recieved) {
        captureLidar(3000 ms)   → nsl_getPointCloudData → pcdData
        deepLearning(frameMat)  → cv::dnn::Net SSD-MobileNet v2
        drawCaption(...)        → cv::hconcat + cv::imshow
        prockey() == 27 ? break → cv::waitKey(1)
    }
    stopLidar()   → nsl_streamingOff(handle)
    ~videoSource() → nsl_close()
```

### 9.2 핵심 데이터 구조

| 타입 | 정의 위치 | 역할 |
|---|---|---|
| `CaptureOptions` | `videoSource.h:44-79` | 앱 전역 설정 — CLI 값, OpenCV Mat, 딥러닝 파라미터, `NslConfig`를 묶은 구조체 |
| `NslConfig` | `nanolib.h:300-387` | LiDAR 장치 설정 (integration time, ROI, HDR, filter, modulation, binning, correction 등) |
| `NslPCD` | `nanolib.h:389-408` | 프레임 데이터 — `amplitude[H][W]`, `distance2D[H][W]`, `distance3D[MAX_OUT][H][W]` 포인트클라우드 |

### 9.3 videoSource 클래스 주요 메서드

| 메서드 | 역할 |
|---|---|
| `createApp` (전역 팩토리) | `-help` 처리 → 윈도우 생성 → CLI 파싱 → `nsl_open` |
| `setLidarOption` | 센서 파라미터 전체 반영 후 `nsl_streamingOn` |
| `captureLidar` | `nsl_getPointCloudData`로 한 프레임 수신 → 컬러맵 적용 → 640×480으로 리사이즈 |
| `drawCaption` | 좌우 분할 캡션 합성, 마우스 crosshair, FPS 오버레이, `imshow` |
| `prockey` | `cv::waitKey(1)` 이벤트 처리 (ESC=27이면 메인 루프가 종료) |
| `initDeepLearning` / `deepLearning` | 모델 로드 및 프레임별 사람 검출 |
| `getDistanceString` | 거리값 → 문자열 변환 (`LOW_AMPLITUDE`, `ADC_OVERFLOW`, `SATURATION`, `INTERFERENCE`, `EDGE_DETECTED`, `BAD_PIXEL` 등 특수 코드 처리) |
| `stopLidar` / 소멸자 | `nsl_streamingOff` → `nsl_close` |

---

## 10. nsl_lib API 레퍼런스 요약

`nsl_lib/include/nanolib.h` 단일 헤더에 모든 공개 API가 노출됩니다. 아래는 대표적인 사용 패턴이며, 전체 함수·옵션은 `nsl_lib/DOC/ROBOSCAN(NSL-3140AA) SDK Software Manual.pdf`를 참고하세요.

### 10.1 연결과 스트리밍 생명주기

```cpp
#include "nanolib.h"
using namespace NslOption;

NslConfig cfg{};
cfg.lidarAngle = 0;
cfg.lensType   = LENS_TYPE::LENS_SF;

int h = nsl_open("192.168.0.220", &cfg, FUNCTION_OPTIONS::FUNC_ON);  // 또는 "/dev/ttyLidar"
nsl_setIntegrationTime(h, 800);
nsl_setMinAmplitude(h, 50);
nsl_streamingOn(h, OPERATION_MODE_OPTIONS::DISTANCE_AMPLITUDE_MODE);

NslPCD frame;
while (nsl_getPointCloudData(h, &frame, 3000) == NSL_ERROR_TYPE::NSL_SUCCESS) {
    // frame.distance2D[y][x], frame.amplitude[y][x], frame.distance3D[...][y][x]
}

nsl_streamingOff(h);
nsl_closeHandle(h);
```

### 10.2 자주 쓰는 함수

- **핸들**: `nsl_open`, `nsl_closeHandle`, `nsl_close`
- **스트리밍**: `nsl_streamingOn`, `nsl_streamingOff`, `nsl_requestSingleFrame`
- **데이터**: `nsl_getPointCloudData`, `nsl_getPointCloudRgbData`
- **센서 파라미터 (set/get 페어)**: `nsl_setIntegrationTime`, `nsl_setMinAmplitude`, `nsl_setHdrMode`, `nsl_setFilter`, `nsl_set3DFilter`, `nsl_setRoi`, `nsl_setBinning`, `nsl_setModulation`, `nsl_setFrameRate`, `nsl_setUdpSpeed`, `nsl_setCorrection`, `nsl_setAutoIntegrationTime`, `nsl_setIpAddress`, `nsl_setLedSegment`, `nsl_setLidarAngle`
- **조명 · 포화 · 간섭**: `nsl_setGrayscaleillumination`, `nsl_setAdcOverflowSaturation`, `nsl_setDualBeam`
- **시각화 보조**: `nsl_setColorRange`, `nsl_getDistanceColor`, `nsl_getAmplitudeColor`
- **정보**: `nsl_getBitInfo`, `nsl_getCurrentConfig`, `nsl_getSdkVersion`, `nsl_saveConfiguration`
- **원시 명령**: `nsl_sendRawCommand`

### 10.3 주요 `NslOption` enum

`FUNCTION_OPTIONS` (FUNC_ON/OFF), `HDR_OPTIONS` (NONE/SPATIAL/TEMPORAL), `MODULATION_OPTIONS` (1.5/3/6/12/24 MHz), `OPERATION_MODE_OPTIONS` (DISTANCE / GRAYSCALE / DIST+AMPL / RGB 계열), `LIDAR_TYPE_OPTIONS` (TYPE_A=320×240 / TYPE_B=800×600), `LENS_TYPE`, `NSL_ERROR_TYPE` (`NSL_SUCCESS=0`, 음수=에러), `NslVec3b` (BGR). 상세 값 목록은 `nanolib.h:46-298` 참조.

### 10.4 플랫폼별 제공 라이브러리

| 플랫폼 | Shared | Static |
|---|---|---|
| Windows | `nanolib.dll`, `nanolib.lib` + `libusb-1.0.dll`, `libusb-1.0.lib` | `nanolib.lib`, `libusb-1.0.lib` |
| Linux x86_64 (Ubuntu 18.04, gcc 7.5) | `libnanolib.so` | `libnanolib.a` |
| Linux aarch64 | `libnanolib.so` | `libnanolib.a` |
| macOS 12.0.5 | `libnanolib.dylib` (미서명) | `libnanolib.a` |

> **Windows DLL 링크 시 주의**: `NSLTOF_API`는 기본 빈 매크로로 정의되어 있습니다 (`nanolib.h:4-6`). Windows에서 `nanolib.dll`을 사용할 때는 컴파일러 전처리 정의에 **`NSLTOF_API=__declspec(dllimport)`**를 추가하세요. static 라이브러리나 Linux · macOS에서는 별도 설정이 필요 없습니다.

---

## 11. 트러블슈팅

| 증상 | 확인 사항 |
|---|---|
| `nsl_open` 실패 (Ethernet) | IP 접근성 (`ping 192.168.0.220`), 방화벽, 센서 전원 |
| `nsl_open` 실패 (USB) | `ls /dev/ttyACM*`, udev 규칙 적용, 펌웨어 ≥3.16이면 libusb 설치 (3.3절) |
| Windows에서 `The code execution cannot proceed because nanolib.dll / opencv_world454.dll was not found` | PATH 미등록 (3.2절) |
| Linux 빌드에서 `cannot find -lnanolib` | `CMakeLists.txt` 23~26라인의 플랫폼 경로 주석 상태 확인 (4.2절) |
| 콘솔에 `mismatch deepLearning weight files !!!` | `deepLearning/` 상대 경로가 실행 파일 기준과 일치하는지 확인 (8장) |
| 화면이 까맣거나 프레임이 뜸 | `-intTime` 상향, `-amplitudeMin` 하향, HDR 모드 전환 (`0`/`1`/`2` 키) |
| 거리 영상에 `LOW_AMPLITUDE` / `ADC_OVERFLOW` / `SATURATION` 많음 | 조명 환경 재확인, `s`/`f` 키로 표시 옵션 토글, `nsl_setAdcOverflowSaturation` 조정 |

---

## 12. 라이선스 · 문의

- 샘플 소스코드의 라이선스 조건은 각 소스 파일 상단의 저작권·허가 문구를 따릅니다 (MIT 스타일).
- 비표준 환경(다른 gcc 버전, 특수 배포판 등)에서 `nanolib` 빌드가 필요한 경우 **sales@nanosys.kr** 로 문의하세요.
- SDK 상세 규격은 `NSL-Sample-Application/nsl_lib/DOC/ROBOSCAN(NSL-3140AA) SDK Software Manual.pdf` 를 참조하세요.
