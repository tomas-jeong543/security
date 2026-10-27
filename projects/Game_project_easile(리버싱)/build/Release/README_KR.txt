EASILE 실행 중 메모리 CRC32 측정 도구
=================================

목적: 게임을 수정하지 않고 실행 중인 GameClient.exe의 메모리에서
GameWorld::Update / GameWorld::Initialize 함수 영역 CRC32를 측정합니다.

구성:
- measure_easile_memory_crc.py : Windows ReadProcessMemory 사용, 읽기 전용
- README_KR.txt : 사용 설명서

사용 방법:
1. Visual Studio에서 Release x64로 빌드한 GameClient.exe를 실행합니다.
2. DLL Injection을 하지 않은 정상 게임 상태를 유지합니다.
3. 이 ZIP을 압축 해제한 폴더에서 PowerShell을 실행합니다.
4. 다음을 입력합니다.
   py -3-64 .\measure_easile_memory_crc.py
   또는: python .\measure_easile_memory_crc.py

프로세스 이름을 직접 지정하려면:
   py -3-64 .\measure_easile_memory_crc.py --name GameClient.exe
PID를 직접 지정하려면:
   py -3-64 .\measure_easile_memory_crc.py --pid 1234

현재 고정 검사 영역 (IDA에서 확인한 최신 주소 기준):
- Update: RVA=0x144C0, SIZE=0x13BB
- Initialize: RVA=0x10D70, SIZE=0x685

위 값은 새 빌드에서 변경될 수 있습니다. 그때는 스크립트 맨 위 REGIONS를 수정하세요.
측정된 CRC는 실행 중 메모리 기준이며 원본 디스크 EXE CRC가 아닙니다.
기준 CRC를 C++에 기입하고 재빌드하면 주소/바이트가 변할 수 있으므로 재확인이 필요합니다.

오류:
- 프로세스를 찾을 수 없음: 게임 실행 여부 및 프로세스 이름 확인
- 둘 이상 검색됨: --pid로 지정
- 접근 거부: 권한 확인 후 필요시 관리자 PowerShell 사용
- 64비트 Python 요구: 64비트 Python 설치 및 실행

주의: 이 도구는 메모리를 읽기만 하며 대상 프로세스를 수정하지 않습니다.
