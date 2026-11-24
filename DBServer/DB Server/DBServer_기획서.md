# DBServer

## 1. 서버 목적 — Why DBServer?

이를 통해 서버 간 책임을 명확히 분리하고, 데이터 처리 구조를 확장할 수 있는 기반을 마련한다.

## 2. 설계 방향 — Design Direction

DBServer를 GameServer와 독립된 서버로 구성하고, 서버 간 통신을 통해 데이터 처리 요청과 결과를 전달한다.

DB 요청을 별도의 작업 단위로 구성하여 비동기 처리 구조로 실행한다.

트랜잭션을 활용하여 여러 작업의 일관성을 보장하고, 실패 시 데이터 상태를 보호할 수 있도록 설계한다.

- GameServer와 DBServer의 역할 분리
- 서버 간 네트워크 통신 기반 데이터 처리
- DB 작업의 비동기 처리
- 트랜잭션 기반 데이터 정합성 관리
- 데이터 요청 및 응답 흐름의 명확한 구성

## 3. 서버 역할 — Responsibilities

### GameServer

- 게임 로직 및 플레이어 상태 관리
- 게임 콘텐츠에 따른 데이터 요청 생성
- DBServer에 데이터 처리 요청 전달
- DB 처리 결과에 따른 게임 로직 반영

### DBServer

- GameServer의 데이터 요청 수신 및 처리
- DB 작업 단위 생성 및 비동기 실행
- MSSQL 데이터 조회, 삽입, 갱신 및 삭제
- Stored Procedure를 통한 데이터 처리
- 트랜잭션 관리 및 처리 결과 전달

### MSSQL

- 게임 데이터 영속성 관리
- 데이터 조회 및 갱신
- Stored Procedure 기반 데이터 처리
- 트랜잭션을 통한 데이터 일관성 보장

## 4. 데이터 처리 구조 — Architecture

DBServer는 네트워크 처리와 DB 작업 실행을 분리하여 구성한다.

GameServer에서 전달된 요청은 DBServer에서 처리할 작업으로 구성되며, 작업 큐를 통해 DB 처리 단계로 전달된다. DB 작업은 별도의 실행 흐름에서 수행되며, 처리 결과는 다시 GameServer에 전달되어 게임 로직에 반영된다.

이를 통해 네트워크 요청 처리와 DB 작업이 서로 직접적으로 종속되지 않도록 구성한다.

## 5. 데이터 처리 흐름 — Process

1. GameServer에서 데이터 처리 요청 생성
2. DBServer로 요청 전달
3. 요청에 따른 DB 작업 생성
4. DB 작업 큐에 등록
5. DB Worker에서 작업을 가져와 DB 처리
6. MSSQL 쿼리 및 Stored Procedure 실행
7. 처리 결과 생성 및 GameServer로 전달
8. GameServer에서 결과에 따른 후속 처리

## 6. 데이터 관리 — Data Management

데이터베이스 작업은 Stored Procedure를 중심으로 구성하며, 서버에서 전달받은 데이터와 매개변수를 기반으로 처리한다.

여러 데이터 변경 작업이 하나의 논리적 작업에 포함되는 경우 트랜잭션을 적용하여 처리 결과의 일관성을 유지한다.

- DB 요청에 따른 데이터 조회 및 변경
- Stored Procedure 기반 데이터 처리
- 트랜잭션 시작, Commit, Rollback
- 데이터 처리 결과 및 실패 여부 전달
- 게임 데이터의 영속성 유지

## 7. 구현 범위 — Scope

- GameServer와 독립된 DBServer 구성
- IOCP 기반 네트워크 통신
- DB 요청 및 작업 단위 구성
- DB Job Queue 기반 비동기 처리
- DBConnection 및 DBBind 기반 DB 접근 구조
- MSSQL 연동 및 Stored Procedure 실행
- 트랜잭션 기반 데이터 처리
- GameServer와 DBServer 간 요청 및 결과 전달

## 8. 목표 및 검증 — Goals & Validation

DBServer를 통해 게임 서버의 데이터 처리 책임을 분리하고, 비동기 DB 작업 구조를 구축한다.

게임 데이터의 조회, 생성, 변경 및 복합 작업을 서버 간 통신으로 처리하고, 실제 실행 결과를 통해 데이터 정합성과 처리 흐름을 검증한다.

- DB 요청 및 응답의 정상 동작
- 비동기 DB 작업 처리
- 데이터 조회 및 변경 결과 확인
- 트랜잭션 성공 및 실패 처리 검증
- 예외 상황에서 데이터 정합성 확인
- GameServer와 DBServer 간 통합 동작 검증