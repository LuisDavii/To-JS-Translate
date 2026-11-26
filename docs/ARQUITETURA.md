# Documentacao Tecnica da Arquitetura — To JS Translate (`PtoJS`)

Este documento descreve a arquitetura interna, os modulos, as estruturas de dados e as principais decisoes de implementacao do tradutor **To JS Translate**.

---

## 1. Visao Geral do Pipeline

O compilador segue uma arquitetura classica em **quatro fases** acopladas por uma **Arvore Sintatica Abstrata (AST)** explicita e uma **Tabela de Simbolos**:

```text
  arquivo.tojs
       │
       ▼
  [1] Analise Lexica (lexer.l -> lex.yy.c)
       │  Tokens + INDENT / DEDENT / NEWLINE + localizacao de linha
       ▼
  [2] Analise Sintatica & Construcao da AST (parser.y -> parser.tab.c)
       │  Node *program_root (AST explicita)
       ▼
  [3] Analise Semantica & Tabela de Simbolos (semantic.c)
       │  Validacao de tipos/variaveis + SymbolTable preenchida
       ▼
  [4] Geracao de Codigo JavaScript (codegen.c)
       │  Percorre a AST + SymbolTable -> emite ES6+
       ▼
  saida.js  ──►  execucao via Node.js
```

O controlador principal ([`main.c`](../main.c)) orquestra o fluxo, trata os argumentos da linha de comando (`--ast`, `--ast-only`, `--symbols`), garante que nenhum arquivo `.js` seja criado em caso de erro em qualquer fase e libera toda a memoria alocada (`symtab_free`, `ast_free`).

---

## 2. Organizacao dos Modulos

| Arquivo | Responsabilidade |
|---|---|
| [`lexer.l`](../lexer.l) | Especificacao Flex: tokenizacao, pilha de indentacao (`INDENT`/`DEDENT`), comentarios e erros lexicos. |
| [`parser.y`](../parser.y) | Especificacao Bison: gramatica LALR(1), precedencia, acoes semanticas que constroem a AST e `yyerror`. |
| [`ast.h`](../ast.h), [`ast.c`](../ast.c) | Definicao dos nos da AST (`Node`, `NodeType`), construtores, contagem, impressao em arvore e liberacao recursiva. |
| [`semantic.h`](../semantic.h), [`semantic.c`](../semantic.c) | Tabela de simbolos (`Symbol`, `SymbolTable`), inferencia/verificacao de tipos, snapshots de fluxo e reporte de erros semanticos. |
| [`codegen.h`](../codegen.h), [`codegen.c`](../codegen.c) | Gerador de codigo JavaScript a partir da AST e da tabela de simbolos. |
| [`main.c`](../main.c) | Interface CLI, abertura de arquivos e orquestracao das fases. |
| [`Makefile`](../Makefile) | Automacao de build (`bison`, `flex`, `gcc -Wall -Wextra -g`) e execucao dos testes (`make test`). |
| [`run_tests.sh`](../run_tests.sh) | Runner automatizado para `exemplos/`, `tests/validos/` e `tests/invalidos/` com validacao via Node.js. |

---

## 3. Detalhamento Tecnico das Fases

### 3.1. Analise Lexica (`lexer.l`)

- **Indentacao significativa (estilo Python):**
  - Mantem uma pilha estatica `indent_stack[256]` inicializada com nivel `0` e um contador `pending_dedents`.
  - Utiliza o estado exclusivo `<BOL>` (*Beginning of Line*) ativado no inicio da entrada (`YY_USER_INIT`) e apos cada quebra de linha.
  - Em `<BOL>`, linhas vazias e linhas contendo apenas comentarios sao ignoradas sem alterar a pilha.
  - Ao encontrar espacos/tabs antes de um token real, calcula a coluna (`\t` = 4 espacos):
    - Se `col > topo`: empilha `col` e retorna `INDENT`.
    - Se `col < topo`: desempilha ate encontrar `col`, acumula `pending_dedents` e retorna `DEDENT`. Se `col` nao coincidir com nenhum nivel da pilha, aborta com erro lexico de indentacao inconsistente.
- **Wrapper `yylex()`:** Intercepta chamadas do Bison para drenar `pending_dedents` antes de chamar `lexer_next()` e rastreia `line_has_token` para emitir um `NEWLINE` sintetico no `<<EOF>>` quando o arquivo nao termina com quebra de linha, seguido dos `DEDENT` necessarios para fechar blocos abertos.
- **Rastreamento de linhas:** `YY_USER_ACTION` atualiza `yylloc.first_line` e `yylloc.last_line` com `yylineno`.

### 3.2. Analise Sintatica (`parser.y`)

- **Gramatica LALR(1) sem conflitos:** Compila com Bison (`bison -d`) com **0 conflitos shift/reduce** e **0 conflitos reduce/reduce**.
- **Mensagens detalhadas:** Usa `%define parse.error verbose` e `%locations` para informar linha exata e tokens esperados/encontrados.
- **Seguranca de memoria em erros sintaticos:** Define `%destructor` para `<text>` (`free`) e `<node>` (`ast_free`), evitando vazamento de memoria quando o parser descarta simbolos da pilha em caso de erro.
- **Precedencia explicita:** A hierarquia de expressoes e modelada por nao-terminais estratificados (`expressao_or` -> `expressao_and` -> `expressao_not` -> `comparacao` -> `expressao_aritmetica` -> `termo` -> `fator` -> `primario`), impedindo comparacoes encadeadas (`1 < 2 < 3`).

### 3.3. Arvore Sintatica Abstrata (`ast.h` / `ast.c`)

Cada no (`struct Node`) armazena:
- `type`: discriminador `NodeType` (`N_LIST`, `N_ASSIGN`, `N_PRINT`, `N_IF`, `N_WHILE`, `N_INT`, `N_REAL`, `N_STRING`, `N_BOOL`, `N_NONE`, `N_ID`, `N_BINARY`, `N_UNARY`).
- `line`: numero da linha no codigo-fonte original (usado nas mensagens semanticas e na impressao da AST).
- Valores lexicos/semanticos: `text`, `int_value`, `number`, `boolean`.
- Ponteiros estruturais: `left`, `right` (operandos/condicoes), `body`, `else_body` (blocos), `next` (encadeamento de comandos em sequencia).

A AST e efetivamente consumida tanto pela analise semantica quanto pela geracao de codigo, e pode ser inspecionada com `--ast` ou `--ast-only`.

### 3.4. Tabela de Simbolos e Analise Semantica (`semantic.h` / `semantic.c`)

- **Estrutura da Tabela de Simbolos:** Lista encadeada de `Symbol` contendo:
  - `name`: identificador da variavel.
  - `type`: `ValueType` inferido (`VAL_INT`, `VAL_REAL`, `VAL_STRING`, `VAL_BOOL`, `VAL_NONE`, `VAL_DYNAMIC`, `VAL_ERROR`).
  - `decl_line`: linha da primeira atribuicao.
  - `decl_depth`: profundidade de bloco na primeira atribuicao (`0` = topo, `> 0` = dentro de `if`/`else`/`while`).
  - `active`: indica se a variavel ja recebeu valor no fluxo atual.
  - `used`: indica se a variavel foi lida em alguma expressao.
- **Controle de fluxo com Snapshots (`SymbolSnapshot`):**
  - Antes de analisar um `if`/`else`, tira um snapshot da tabela.
  - Analisa o bloco `then`, salva o estado final de `then`, restaura o snapshot inicial e analisa o bloco `else`.
  - Ao final, faz o *merge* dos estados: variaveis definidas em ambos os ramos tem seus tipos unificados (`unify_types`); variaveis definidas em apenas um ramo permanecem ativas (semantica de escopo unico).
  - Em `while`, analisa a condicao, analisa o corpo, revalida a condicao (para detectar mutacoes de tipo invalidas) e unifica tipos com o estado anterior ao laco.
- **Inspecao:** A flag `--symbols` imprime a tabela completa (nome, tipo inferido, linha, nivel e status de uso).

### 3.5. Geracao de Codigo JavaScript (`codegen.h` / `codegen.c`)

O gerador percorre diretamente a AST (`program_root`) e consulta a `SymbolTable`:
1. **Hoisting para escopo unico:** Para cada simbolo com `decl_depth > 0`, emite `let <ident>;` no topo do arquivo JS antes dos comandos, garantindo que variaveis criadas dentro de `if`/`else`/`while` continuem acessiveis fora do bloco em JavaScript.
2. **Escape de identificadores reservados do JS:** `is_js_reserved()` verifica palavras reservadas do ECMAScript e `console`; em caso de colisao, emite `_tojs_<nome>`.
3. **Parentetizacao minima:** `expr_prec()` e `emit_expr()` inserem parenteses apenas quando necessario para preservar a precedencia e associatividade da AST (ex.: subtracao/divisao no filho direito, operadores unarios adjacentes).
4. **Mapeamento semantico fiel:**
   - `and` -> `&&`, `or` -> `||`, `not` -> `!`
   - `==` -> `===`, `!=` -> `!==` (igualdade estrita, evitando coercao implicita do JS)
   - `True` -> `true`, `False` -> `false`, `None` -> `null`
   - `print(expr);` -> `console.log(expr);`
   - Zeros a esquerda em literais inteiros (`007` -> `7`) e reais (`00.5` -> `0.5`) sao normalizados para evitar erros de sintaxe/octal no JS.

---

## 4. Estrategia de Tratamento de Erros

| Fase | Comportamento | Saida de codigo |
|---|---|---|
| Lexica | Interrompe no primeiro erro com linha e causa | `exit(1)`, nenhum `.js` gerado |
| Sintatica | Interrompe no primeiro erro com `yyerror` detalhado e linha | `exit(1)`, nenhum `.js` gerado |
| Semantica | Acumula todos os erros da AST com linha e descricao | `exit(1)` com resumo da contagem, nenhum `.js` gerado |

---

## 5. Estrategia de Testes e Validacao

O script [`run_tests.sh`](../run_tests.sh) (executado via `make test`) automatiza a verificacao de ponta a ponta:
- **Casos validos (`exemplos/` e `tests/validos/`):** traduz cada `.tojs` para um `.js` temporario, executa com `node` e compara a saida padrao linha a linha com os comentarios `# SAIDA:` (ou arquivo `.saida` correspondente).
- **Casos invalidos (`tests/invalidos/`):** verifica tres condicoes simultaneamente:
  1. O tradutor retorna codigo de saida diferente de zero;
  2. Nenhum arquivo `.js` e criado;
  3. A mensagem em `stderr` contem o trecho especificado em `# ESPERA:`.

---

## 6. Decisoes de Projeto e Limitacoes Conhecidas

- **Escopo unico:** Adotado para manter fidelidade ao comportamento de blocos `if`/`while` em Python, sem exigir declaracao previa fora dos blocos.
- **Exibicao de reais no JS:** Como JavaScript utiliza o tipo IEEE-754 `Number` para inteiros e reais, numeros como `3.0` sao exibidos pelo `console.log` como `3`.
- **Escopo da linguagem:** Nao inclui `elif`, funcoes, listas ou entrada interativa, mantendo o foco na solidez e completude de todas as etapas obrigatorias do compilador.
