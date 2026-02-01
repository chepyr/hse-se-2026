# Diagrams Source (Mermaid)

---

## 1. Component Diagram

Диаграмма компонентов показывает общую архитектуру интерпретатора и распределение ответственности между основными частями системы.  
**Shell (REPL)** является точкой входа и управляет жизненным циклом приложения, окружением и кодами возврата.  
**Frontend** объединяет лексический и синтаксический анализ.  
**Lexer** выполняет разбор строки на токены с учётом кавычек и подстановок переменных.  
**Parser** строит объектное представление команды (AST).  
**Executor** отвечает за исполнение пайплайнов, создание процессов и работу с файловыми дескрипторами.  
**BuiltinRegistry** и **ExternalRunner** реализуют соответственно встроенные и внешние команды.  
**Environment** хранит переменные окружения и предоставляет их снимок для запуска процессов.


```mermaid
flowchart LR
  Shell["Shell / REPL
  читает строку + prompt
  владеет Environment
  хранит last_exit_code
  обрабатывает EOF / exit"]

  Frontend["Frontend (Фасад)
  единая точка входа
  вызывает Lexer и Parser"]

  Lexer["Lexer
  кавычки + $-подстановка
  выдаёт токены Word / Pipe"]

  Parser["Parser
  строит Pipeline / CommandSpec (AST)"]

  Executor["Executor
  выполняет Pipeline
  fork / exec / pipe / dup2 / wait"]

  Builtins["BuiltinRegistry
  имя → обработчик"]

  External["ExternalRunner
  поиск в PATH + execve"]

  Env["Environment
  map имя→значение
  snapshot()"]

  Shell --> Frontend
  Frontend --> Lexer
  Lexer --> Parser
  Parser --> Executor

  Executor --> Builtins
  Executor --> External
  Executor --> Env
  External --> Env

```

---

## 2. Main Flow

Диаграмма основного потока исполнения описывает жизненный цикл программы от запуска до завершения.  
После инициализации окружения и регистрации встроенных команд приложение входит в цикл **REPL**: читает строку ввода, выполняет её разбор и исполнение.  
При ошибке разбора или пустой строке цикл продолжается.  
Если команда запрашивает завершение (`exit`) или встречается **EOF**, цикл прерывается, и процесс завершается с последним кодом возврата.


```mermaid
flowchart TD
  Start["Start main"] --> InitEnv["Init Environment<br/>(copy environ)"]
  InitEnv --> RegBuiltins["Register builtins"]
  RegBuiltins --> Loop["REPL: read line"]

  Loop --> EOF{"EOF?"}
  EOF -- yes --> End["End"]
  EOF -- no --> Parse["Frontend.parse(line)"]

  Parse --> ParseErr{"parse error?"}
  ParseErr -- yes --> Loop
  ParseErr -- no --> Empty{"empty line?"}
  Empty -- yes --> Loop
  Empty -- no --> Exec["Executor.run(pipeline)"]

  Exec --> ExitReq{"request_exit?"}
  ExitReq -- yes --> End
  ExitReq -- no --> Loop
```

---

## 3. Lexer State Machine


Диаграмма автомата лексического анализатора иллюстрирует правила разбора входной строки на слова и операторы.  
Lexer различает обычный режим, одинарные и двойные кавычки.  
В одинарных кавычках подстановки переменных отключены, в двойных — разрешены.  
При формировании токена слова выполняется `$`-подстановка и отслеживается наличие незакавыченного символа `=`, что позволяет позже корректно распознавать присваивания.  
Пустые кавычки формируют пустой аргумент, а не удаляются.

```mermaid
stateDiagram-v2
  [*] --> Normal

  Normal --> InSingleQuote: "'"
  InSingleQuote --> Normal: "'"

  Normal --> InDoubleQuote: '"'
  InDoubleQuote --> Normal: '"'

  Normal --> Normal: ws / emit Word
  Normal --> Normal: '|' / emit Pipe
  InSingleQuote --> InSingleQuote: any char (no $-subst)
  InDoubleQuote --> InDoubleQuote: any char (with $-subst)
```

---

## 4. AST Structure

Диаграмма структуры **AST** показывает объектное представление результата синтаксического анализа.  
**Pipeline** содержит одну или несколько команд, каждая из которых описывается структурой **CommandSpec**.  
Команда включает список аргументов, возможные префиксные присваивания переменных окружения и флаг «только присваивание».  
Такое представление отделяет этап разбора от этапа исполнения и позволяет **Executor** принимать решения о способе запуска команды.


```mermaid
classDiagram
  class Pipeline {
    +vector<CommandSpec> commands
  }

  class CommandSpec {
    +vector<string> argv
    +vector<Assignment> prefix_env
    +bool is_assignment_only
  }

  class Assignment {
    +string name
    +string value
  }

  Pipeline "1" *-- "1..*" CommandSpec : commands
  CommandSpec "1" *-- "0..*" Assignment : prefix_env
```

---

## 5. Pipes / FD

Диаграмма пайплайна демонстрирует взаимодействие процессов через файловые дескрипторы.  
Для **N** команд создаётся **N−1** канал, и каждый дочерний процесс получает перенаправленные стандартные потоки ввода и вывода через `dup2`.  
Родительский процесс закрывает неиспользуемые концы каналов и ожидает завершения всех дочерних процессов.  
Такой механизм обеспечивает передачу данных между командами так же, как в традиционных Unix-shell.


```mermaid
flowchart LR
  C0["cmd0 child
  STDIN <- terminal
  STDOUT -> pipe0.w"]

  C1["cmd1 child
  STDIN <- pipe0.r
  STDOUT -> pipe1.w"]

  C2["cmd2 child
  STDIN <- pipe1.r
  STDOUT -> terminal"]

  P0["pipe0 (r,w)"]
  P1["pipe1 (r,w)"]

  C0 -->|write| P0
  P0 -->|read| C1
  C1 -->|write| P1
  P1 -->|read| C2
```

---

## 6. Builtin vs External

Диаграмма выбора способа исполнения команды описывает логику **Executor**.  
Если выполняется пайплайн, каждая команда запускается в отдельном дочернем процессе, независимо от того, является ли она встроенной или внешней, после чего родитель ожидает завершения всех процессов и возвращает код последней команды.  
Для одиночной команды возможны три случая: присваивание переменных окружения (выполняется в родителе), встроенная команда (выполняется в родителе для возможности изменения состояния shell) и внешняя программа (запускается через `fork` и `execve` с последующим `waitpid`).

```mermaid
flowchart TD
  Start(["Start execute"]) --> IsPipe{"pipeline?"}

  %% Pipeline path: always spawn N children, then wait all (return last code)
  IsPipe -- yes --> SpawnAll["spawn N children<br/>(builtin/external in child)"]
  SpawnAll --> WaitAll["waitpid for all children"]
  WaitAll --> EndPipe(["return last cmd code"])

  %% Single-command path
  IsPipe -- no --> IsAssign{"assignment-only?"}
  IsAssign -- yes --> ApplyEnv["apply to Environment"] --> EndSingleA(["return 0"])

  IsAssign -- no --> IsBuiltin{"builtin?"}
  IsBuiltin -- yes --> RunBuiltinParent["run builtin in parent"] --> EndSingleB(["return code"])

  IsBuiltin -- no --> ForkExec["fork + exec external"] --> WaitOne["waitpid child"] --> EndSingleC(["return code"])

```
