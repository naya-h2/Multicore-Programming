# Multicore-Programming
멀티코어 프로그래밍 과목 프로젝트

## MyShell
- Linux shell을 직접 구현해보는 프로젝트입니다.
### 📌 Phase1 : Building and Testing Your Shell
- fork와 signal을 이용해 간단한 shell의 커맨드를 구현하였습니다.

## Concurrent Stock Server
- 여러 client들의 동시 접속 및 서비스를 위한 주식 서버를 구현하는 프로젝트입니다.
### 📌 Task1 : Event-driven Approach
- select()를 이용하여 event가 발생한 connection fd를 찾는 방식으로 구현하였습니다.
### 📌 Task2 : Thread-based Approach
- worker thread를 미리 생성한 뒤, client와 연결하는 방식으로 구현하였습니다.
- semaphore를 사용하였습니다.

## Dynamic Memory Allocator
- C언어의 Dynamic Memory Allocator인 malloc, free, realloc을 구현하는 프로젝트입니다.
- Explicit list 방식을 사용하여 구현하였습니다.
- 설계한 block의 구조와 block list의 구조는 /Project3/document.pdf 파일을 참고해주세요.
