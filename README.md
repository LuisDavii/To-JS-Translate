# To JS Translate (PtoJS)

Tradutor da linguagem **PtoJS** (`.tojs`, sintaxe inspirada em Python com blocos por indentacao) para **JavaScript (ES6+)**, desenvolvido em **C** com **Flex** e **Bison** para a disciplina de Compiladores.

- **Modalidade:** Tradutor (`.tojs` -> `.js` executavel em Node.js)
- **Equipe:** Claybson Jordao, Luis Davi e Sabrina Gabrielly
- **Documentacao completa:**
  - [Especificacao da Linguagem e Gramatica Formal](docs/ESPECIFICACAO.md) (tokens, BNF, precedencia, variaveis, regras semanticas e exemplos)
  - [Documentacao Tecnica da Arquitetura](docs/ARQUITETURA.md) (pipeline, modulos, AST, tabela de simbolos, geracao de codigo e decisoes de projeto)

## Pipeline

```text
arquivo.tojs -> lexer.l (analise lexica) -> parser.y (analise sintatica + AST)
             -> semantic.c (tipos e tabela de simbolos) -> codegen.c -> saida.js
```

## Requisitos e Compilacao

### Dependencias
- `gcc` (C99/C11)
- `flex` (analisador lexico)
- `bison` (analisador sintatico)
- `make` e `bash`
- `node` (Node.js, para executar o JavaScript gerado e validar os testes)

### Compilacao e Execucao (Linux / macOS / WSL / MSYS2)

```bash
make                                      # gera parser.tab.c, lex.yy.c e o executavel ./tojs
./tojs exemplos/01_basico.tojs saida.js   # traduz para JavaScript
node saida.js                             # executa o JS gerado

./tojs --ast arquivo.tojs                 # imprime a AST e traduz
./tojs --ast-only arquivo.tojs            # apenas analisa e imprime a AST
./tojs --symbols arquivo.tojs             # imprime a tabela de simbolos e traduz
make clean                                # remove artefatos gerados
```

> **Windows (MSYS2 UCRT64):** garanta que `gcc`, `flex`, `bison`, `make` e `node` estejam no `PATH` e execute os comandos acima no terminal MSYS2 UCRT64.

## A Linguagem em Resumo

| Recurso | Exemplo |
|---|---|
| Atribuicao (termina com `;`) | `x = 10;` |
| Saida | `print(x);` |
| Tipos | inteiro, real (`1.5`), string (`"a"` ou `'a'`), `True`/`False`, `None` |
| Aritmetica | `+ - * / %`, unarios `+ -`, parenteses |
| Comparacao | `== != < > <= >=` (nao encadeia: `1 < 2 < 3` e erro) |
| Logica | `and`, `or`, `not` |
| Condicional | `if cond:` + bloco indentado + `else:` opcional |
| Repeticao | `while cond:` + bloco indentado |
| Comentario | `# ate o fim da linha` |

Blocos sao delimitados por indentacao (como em Python). Consulte [docs/ESPECIFICACAO.md](docs/ESPECIFICACAO.md) para a especificacao completa de tokens, gramatica BNF, precedencia e regras semanticas.

## Exemplos de Execucao

Ficam em `exemplos/`. Cada arquivo traz, nos comentarios `# SAIDA:`, a saida esperada.

| Arquivo | Demonstra |
|---|---|
| `01_basico.tojs` | tipos primitivos, atribuicao e `print` |
| `02_aritmetica.tojs` | operadores e precedencia |
| `03_condicional.tojs` | `if` / `else` e comparacoes |
| `04_repeticao.tojs` | `while` (soma 1..10 e fatorial de 5) |
| `05_logica.tojs` | `and`, `or`, `not` |
| `06_aninhado.tojs` | `if` dentro de `while` |
| `07_strings.tojs` | aspas simples/duplas e concatenacao |

Exemplo (`exemplos/04_repeticao.tojs`):

```python
i = 1;
soma = 0;

while i <= 10:
    soma = soma + i;
    i = i + 1;

print(soma);
```

JavaScript gerado:

```javascript
let i = 1;
let soma = 0;
while (i <= 10) {
    soma = soma + i;
    i = i + 1;
}
console.log(soma);
```

## Testes Automatizados

```bash
make test          # ou: bash run_tests.sh ./tojs
```

O script `run_tests.sh` executa **50 casos de teste** (7 exemplos + 13 casos validos + 30 casos invalidos), traduzindo e executando o JS gerado com Node.js:

- **Validos** (`exemplos/` e `tests/validos/`): a saida do JS gerado deve ser exatamente igual as linhas `# SAIDA:` (ou ao `.saida` ao lado). Cobrem comentarios, linhas vazias, unarios, `while` aninhado, identificadores reservados em JS (`var`, `new`, `class`, `console`), zeros a esquerda em inteiros e reais, programa vazio, arquivo sem newline final, indentacao com tab, precedencia logica e variaveis declaradas apenas em blocos condicionais.
- **Invalidos** (`tests/invalidos/`): o tradutor deve retornar erro, nao gerar o arquivo `.js` e imprimir a mensagem indicada em `# ESPERA:`.

| Prefixo | Fase | Casos |
|---|---|---|
| `l` | lexica | caractere invalido (`@`, `$`, `!`), string nao fechada, indentacao inconsistente |
| `s` | sintatica | falta de `;`, falta de `:`, `print` sem parenteses, parentese aberto, `else` sem `if`, atribuicao sem valor, bloco sem indentacao, operador duplicado, comparacao encadeada, `x = y = 3`, palavra reservada como variavel |
| `m` | semantica | variavel nao definida, uso antes da atribuicao, `"a" + 1`, `"a" - 1`, condicao nao logica (`if`/`while`), `not 3`, `1 < "a"`, `1 == "a"`, `1 and 2`, divisao/modulo por zero, `-"a"`, `True + 1` |

## Estrutura do Repositorio

```text
.
├── lexer.l              # Especificacao lexica (Flex)
├── parser.y             # Especificacao sintatica e construcao da AST (Bison)
├── ast.h / ast.c        # Arvore Sintatica Abstrata (AST)
├── semantic.h / .c      # Tabela de simbolos e analise semantica
├── codegen.h / .c       # Gerador de codigo JavaScript
├── main.c               # Driver CLI (--ast, --ast-only, --symbols)
├── Makefile             # Build e execucao de testes
├── run_tests.sh         # Suíte de testes automatizados
├── docs/
│   ├── ESPECIFICACAO.md # Especificacao formal da linguagem e gramatica BNF
│   └── ARQUITETURA.md   # Arquitetura tecnica e decisoes de implementacao
├── exemplos/            # 7 programas de demonstracao
└── tests/
    ├── validos/         # 13 casos de borda validos
    └── invalidos/       # 30 casos invalidos (lexicos, sintaticos e semanticos)
```

## Decisoes e Limitacoes Conhecidas

- **Escopo unico** (como Python): variavel atribuida so em um ramo do `if` continua valida depois dele. O gerador declara `let x;` no topo do JS nesses casos (testes `v11` e `v12`).
- O tradutor para no **primeiro erro lexico ou sintatico**; erros semanticos sao acumulados e contados antes de cancelar a traducao.
- Comparacoes `==` e `!=` sao traduzidas para `===` e `!==` em JavaScript para evitar coercao implicita de tipos.
- Reais sao impressos pelo JS sem `.0` (`3.0` aparece como `3`).
- Identificadores que colidem com palavras reservadas do JS (ou `console`) recebem o prefixo `_tojs_`.

## Declaracao de Uso de Inteligencia Artificial

Conforme a Secao 8 do enunciado da disciplina, registramos o uso de ferramentas de IA durante o desenvolvimento:

- **Ferramentas utilizadas:** Assistentes de codigo baseados em LLM (Google Gemini / Antigravity).
- **Atividades em que foram empregadas:** Revisao de casos de borda na analise lexica/semantica/geracao de codigo, ampliacao da suíte de testes automatizados e estruturacao da documentacao formal da linguagem e arquitetura.
- **Responsabilidade:** Todos os integrantes revisaram, testaram e validaram o codigo-fonte, as especificacoes do Flex/Bison e a documentacao entregue, responsabilizando-se integralmente pelo funcionamento e compreensao tecnica do projeto.
