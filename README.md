# VALORANT.Source — source-only mirror

> 이 리포는 **소스 코드 전용 미러**입니다.
> 전체 프로젝트(에셋 포함)와 **풀 README**는 원본 리포에서 확인하세요:
> 👉 [VALORANT (Main Repo)](https://github.com/chungheonLee0325/VALORANT)

포함: `Source/`, `Config/`, `Plugins/*/Source`, `Valorant.uproject`, `Doc/`
제외: `Content/`, `Binaries/`, `Intermediate/`

---

<details><summary><b>Full README (from main repo)</b></summary>

# VALORANT - Multiplay Hypher FPS 게임 (UE, Multiplayer, GAS)

## 🎯 프로젝트 요약
언리얼 엔진 5와 C++ Gameplay Ability System(GAS)을 기반으로 한 **멀티플레이어 FPS 전투 시스템**을 설계·구현했습니다.  
Phoenix, Sage, KAY/O, Jett — 4명의 Agent × C/Q/E 총 12종 스킬을 멀티플레이 환경에서 완벽히 동기화하였으며, Spike·상점·라운드 진행·네트워크 구조까지 포함한 **GAS 기반 확장형 전투 아키텍처**를 구축했습니다.

---

## **✨ 프로젝트 개요 및 기여 내역**

이 프로젝트는 2개월간 7명의 팀원과 함께 진행되었습니다. 저는 팀의 시스템 아키텍트 및 게임플레이 프로그래머로서 아래 시스템들의 설계와 구현을 전담했습니다.

* **담당 역할**: 시스템 아키텍트, 게임플레이 프로그래머  
* **주요 기여 내역**:  
  * **GAS 기반 전투 아키텍처 설계**: Gameplay Ability System을 활용하여 확장 가능한 스킬 프레임워크를 처음부터 설계하고, 4명의 요원과 12종의 모든 스킬을 직접 구현했습니다.  
  * **핵심 게임플레이 시스템 구현**: 서버 권위 스파이크 시스템, 라운드 기반 경제 시스템, 정교한 섬광 판정 로직 등 README에 기술된 모든 핵심 시스템을 주도적으로 개발했습니다.  
  * **멀티플레이 동기화**: 모든 스킬과 상호작용이 멀티플레이 환경에서 완벽하게 동기화되도록 네트워크 로직을 책임지고 구현했습니다.

---

### 💡 프로젝트 탐색 가이드
> 이 README는 프로젝트의 핵심을 요약한 '쇼케이스'입니다. 전체적인 개요를 파악한 뒤, 더 깊은 기술적 내용이 궁금하다면 Tech Docs를 확인해 보세요.

| 문서 | 역할                | 내용                          |
| :--- |:------------------|:----------------------------|
| 📋 [Project Gallery](https://github.com/chungheonLee0325/chungheonLee0325) | Root (전체 개요)      | 주요 프로젝트 목록, 핵심 역량 요약        |
| 📁 **Repository README** | **What (개요)**     | 프로젝트 요약, 데모 영상, 핵심 기능, 아키텍처 |
| 🔗 [Tech Docs](https://github.com/chungheonLee0325/VALORANT/wiki) | How & Why (상세 구현) | 코드 분석, 설계 과정, 기술 회고, 트러블슈팅  |

---

## 🎬 프로젝트 시연 영상
프로젝트의 주요 결과물과 핵심 기능을 한눈에 볼 수 있는 영상입니다.

 <p align="center">
 <a href="https://youtu.be/Ym0MJUSHHbc">
 <img src="Doc/Gifs/VALORITH_Overview.gif" alt="프로젝트 하이라이트 영상 GIF" width="100%">
 </a>
 </p>
 <p align="center">
 <a href="https://youtu.be/Ym0MJUSHHbc"><b>▶ YouTube에서 고화질로 시청하기</b></a>
 </p>
---

## ✨ 구현된 핵심 시스템 (Implemented Core Systems)

### 1. Gameplay Ability System (GAS) 기반 스킬 프레임워크


> 모든 스킬의 기반이 되는 확장 가능한 프레임워크를 설계했습니다.

- **3단계 상태 머신**: 모든 스킬은 `Preparing` → `Waiting` → `Executing`의 일관된 상태를 가지며, 각 상태에 맞는 애니메이션, 이펙트, 사운드가 자동으로 처리됩니다.
- **활성화 타입 분리**: 스킬 키를 누르면 즉시 발동하는 `Instant` 타입과, 추가 입력을 기다리는 `WithPrepare` 타입을 분리하여 다양한 스킬을 효율적으로 구현했습니다.
- **입력 하이재킹**: 스킬 준비 상태(`Waiting`)에서는 기존의 공격/조준 입력을 가로채, 스킬의 후속 동작(예: 좌클릭-직선 발사 / 우클릭-곡선 발사)으로 동적으로 재정의합니다.

---

### 2. 서버 권위 스파이크 시스템

https://github.com/user-attachments/assets/360060ed-4a26-43bf-b9f8-7558dcd7411b

> 게임의 승패를 결정하는 핵심 목표인 스파이크의 모든 로직을 서버 권위적으로 구현했습니다.

- **상태 머신**: `소지`, `설치 중`, `설치 완료`, `해체 중`, `폭발` 등 모든 상태를 서버에서 직접 관리하여 데이터 정합성을 보장합니다.
- **반 해체 (Half-Defuse)**: 해체 진행률이 50%를 넘으면 체크포인트가 저장되어, 잠시 중단했다가 다시 해체할 때 이어서 진행할 수 있습니다.
- **상황인지 기반 상호작용**: 플레이어는 단일 상호작용 키(`F`)만으로 상황에 따라 스파이크 `줍기`, `설치`, `해체`가 자동으로 분기 처리되는 편리한 UX를 경험합니다.

---

### 3. 정교한 섬광(Flash) 시스템


> 단순히 화면을 하얗게 만드는 것을 넘어, 플레이어의 대응을 유도하는 정교한 판정 시스템을 구현했습니다.

- **복합 판정 로직**: 폭발 지점과의 `거리`, 플레이어의 `시야각`, 그리고 중간 `장애물` 유무를 모두 계산하여 섬광 효과의 강도와 지속시간을 차등 적용합니다.
- **복합 시각 효과**: 화면 전체를 덮는 `Post-Process` 효과와 함께, 폭발 방향을 알려주는 방사형 `UMG 위젯`을 결합하여 플레이어의 상황 인지를 돕습니다.

---

### 4. 안전한 상점 및 경제 시스템

https://github.com/user-attachments/assets/c4ddd177-9b8b-4776-bab3-dbbabbb3c2f8

> 라운드 기반의 전략성을 더하는 상점과 경제 시스템을 치팅에 안전한 구조로 설계했습니다.

- **서버 검증 구매**: 모든 구매 요청은 서버에서 플레이어의 크레딧을 직접 확인하고 처리하여, 클라이언트 변조를 통한 부당 구매를 원천적으로 차단합니다.
- **자동 환불 규칙**: 플레이어가 동일 카테고리의 다른 무기를 구매할 경우, 이전에 구매했던 미사용 무기의 가격을 자동으로 계산하여 환불해주는 편의 기능을 구현했습니다.
- **라운드 기반 보상**: 라운드 승패, 연속 패배, 킬, 스파이크 설치 등 다양한 조건에 따라 크레딧을 차등 지급하는 경제 규칙을 구현했습니다.

---

## 📖 상세 기술 위키 (Technical Wiki)

> 본 프로젝트의 상세한 아키텍처, 전체 시스템 설계, 각 클래스의 역할, 핵심 코드 분석, 그리고 프로젝트 회고에 대한 내용은 아래 기술 위키에서 확인하실 수 있습니다.
> 
> ### **➡️ [프로젝트 기술 위키 바로가기 (Click here for the Project's Technical Wiki)](https://github.com/chungheonLee0325/VALORANT/wiki)**

---

## ⌨️ 주요 조작키
* **이동:** W, A, S, D
* **시점 조작:** 마우스 이동
* **기본 공격 / 스킬 발동:** 마우스 좌클릭
* **스킬 보조 입력:** 마우스 우클릭
* **점프:** 스페이스 바
* **질주:** Shift
* **걷기 / 회피:** Ctrl
* **재장전:** R
* **무기 전환:** 숫자키 1~4, 마우스 휠
* **스킬 C:** C
* **스킬 Q:** Q
* **스킬 E:** E
* **스파이크 설치:** 숫자키 4
* **스파이크 해체 / 상호작용:** F
* **상점 열기 / 닫기:** B
> 스킬 준비 상태에서는 **입력 라우팅**이 변경됩니다(무기 발사 / 스킬 보조 입력 → 스킬 후속 입력).

---

## 🛠️ 설치 & 실행
1) **요구사항**: UE 5.5, Visual Studio 2022(C++), (멀티 테스트 시) Steam 실행
2) **빌드**: `Valorant.uproject` → *Generate Visual Studio project files* → `Valorant.sln` 열기 →  
   구성 `Development Editor`로 `Valorant` 빌드
3) **실행**: 에디터에서 `Lobby` 또는 `Game` 맵 열기 → **Play**
  - Net Mode: *Listen Server / Client* 또는 Standalone 다중 인스턴스

</details>

