# Especificacao da Linguagem PtoJS (`To JS Translate`)

## 1. Nome, Finalidade e Modalidade

- **Nome da linguagem:** **PtoJS** (extensao `.tojs`), implementada pelo tradutor **To JS Translate** (`tojs`).
- **Finalidade:** Oferecer uma linguagem imperativa, didatica e de sintaxe inspirada em Python (blocos delimitados por indentacao, tipagem inferida por atribuicao, operadores logicos textuais `and`, `or`, `not` e literais `True`, `False`, `None`), voltada a demonstrar todas as etapas de um compilador tradicional.
- **Modalidade escolhida:** **Tradutor** (transpilador) de `.tojs` para **JavaScript (ES6+)**.
- **Justificativa do destino (JavaScript):** JavaScript possui execucao imediata via Node.js (sem etapa adicional de compilacao nativa), suporta dinamicamente os tipos da linguagem (`number`, `string`, `boolean`, `null`) e permite mapear diretamente construcoes estruturadas (`if`/`else`, `while`, `console.log`), preservando a semantica de PtoJS e facilitando a validacao automatizada dos programas gerados.

---

## 2. Analise Lexica: Tokens e Regras

O analisador lexico e especificado em [`lexer.l`](../lexer.l) (Flex).

### 2.1. Palavras Reservadas

| Lexema | Token | Descricao |
|---|---|---|
| `if` | `IF` | Inicio de estrutura condicional |
| `else` | `ELSE` | Ramo alternativo de condicional |
| `while` | `WHILE` | Inicio de laco de repeticao |
| `print` | `PRINT` | Comando de exibicao de resultado |
| `True` | `TRUE` | Literal booleano verdadeiro |
| `False` | `FALSE` | Literal booleano falso |
| `None` | `NONE` | Literal nulo |
| `and` | `AND` | Operador logico conjuncao |
| `or` | `OR` | Operador logico disjuncao |
| `not` | `NOT` | Operador logico negacao |

### 2.2. Operadores e Delimitadores

| Lexema | Token | Categoria |
|---|---|---|
| `==` | `EQ` | Igualdade |
| `!=` | `NE` | Diferenca |
| `>=` | `GE` | Maior ou igual |
| `<=` | `LE` | Menor ou igual |
| `>` | `GT` | Maior que |
| `<` | `LT` | Menor que |
| `=` | `ASSIGN` | Atribuicao |
| `+` | `PLUS` | Soma / positivo unario |
| `-` | `MINUS` | Subtracao / negativo unario |
| `*` | `STAR` | Multiplicacao |
| `/` | `SLASH` | Divisao |
| `%` | `MOD` | Modulo (resto) |
| `(` | `LPAREN` | Abertura de parenteses |
| `)` | `RPAREN` | Fechamento de parenteses |
| `:` | `COLON` | Inicio de bloco composto |
| `,` | `COMMA` | Separador (reservado) |
| `;` | `FIM_COMANDO` | Terminador obrigatorio de comando simples |

### 2.3. Literais e Identificadores

| Token | Expressao Regular | Exemplos |
|---|---|---|
| `REAL` | `[0-9]+\.[0-9]+` | `1.65`, `0.5`, `10.0` |
| `INTEGER` | `[0-9]+` | `0`, `42`, `007` |
| `STRING` | `\"([^\\\"\n]\|\\.)*\"` ou `\'([^\\\'\n]\|\\.)*\'` | `"Sabrina"`, `'ola\n'` |
| `IDENT` | `[a-zA-Z_][a-zA-Z0-9_]*` | `x`, `soma_total`, `_aux1` |

> **Observacao:** `REAL` tem prioridade sobre `INTEGER` por ser declarado antes e por correspondencia mais longa. Palavras reservadas tem prioridade sobre `IDENT`.

### 2.4. Tokens Sinteticos de Estrutura (Indentacao e Quebra de Linha)

| Token | Descricao |
|---|---|
| `NEWLINE` | Fim de linha logica (`\n` ou `\r\n`). Tambem e emitido no `EOF` se a ultima linha contiver tokens sem quebra de linha final. |
| `INDENT` | Aumento no nivel de indentacao no inicio de uma linha nao vazia (cada `\t` equivale a 4 espacos). |
| `DEDENT` | Reducao no nivel de indentacao; pode emitir multiplos `DEDENT` consecutivos ao fechar varios blocos aninhados ou no `EOF`. |

### 2.5. Espacos em Branco e Comentarios

- **Comentarios:** iniciam com `#` e estendem-se ate o fim da linha (`#[^\r\n]*`), sendo ignorados pelo lexer.
- **Linhas vazias ou somente com comentarios:** nao alteram a pilha de indentacao.
- **Espacos e tabulacoes no meio da linha (`[ \t\r]+`):** ignorados.

### 2.6. Erros Lexicos

O lexer identifica e reporta com numero de linha:
- **Caractere inesperado** (ex.: `@`, `$`, `!` isolado, aspas nao fechadas na mesma linha): `Erro lexico na linha N: caractere inesperado '...'`.
- **Indentacao inconsistente** (retorno para um nivel que nao existe na pilha de indentacao): `Erro lexico na linha N: indentacao inconsistente`.
- **Indentacao excessiva** (profundidade superior a 255 niveis): `Erro: indentacao muito profunda na linha N`.

---

## 3. Gramatica Formal (BNF)

A gramatica formal abaixo corresponde diretamente a especificacao LALR(1) em [`parser.y`](../parser.y), livre de conflitos shift/reduce e reduce/reduce.

```bnf
<programa>             ::= <comandos>

<comandos>             ::= ε
                         | <comandos> <comando>

<comando>              ::= <comando_simples> FIM_COMANDO NEWLINE
                         | <comando_composto>
                         | NEWLINE

<comando_simples>      ::= <atribuicao>
                         | <saida>

<comando_composto>     ::= <condicional>
                         | <repeticao>

<atribuicao>           ::= IDENT ASSIGN <expressao>

<saida>                ::= PRINT LPAREN <expressao> RPAREN

<condicional>          ::= IF <expressao> COLON NEWLINE INDENT <comandos> DEDENT
                         | IF <expressao> COLON NEWLINE INDENT <comandos> DEDENT
                           ELSE COLON NEWLINE INDENT <comandos> DEDENT

<repeticao>            ::= WHILE <expressao> COLON NEWLINE INDENT <comandos> DEDENT

<expressao>            ::= <expressao_or>

<expressao_or>         ::= <expressao_or> OR <expressao_and>
                         | <expressao_and>

<expressao_and>        ::= <expressao_and> AND <expressao_not>
                         | <expressao_not>

<expressao_not>        ::= NOT <expressao_not>
                         | <comparacao>

<comparacao>           ::= <expressao_aritmetica>
                         | <expressao_aritmetica> EQ <expressao_aritmetica>
                         | <expressao_aritmetica> NE <expressao_aritmetica>
                         | <expressao_aritmetica> GT <expressao_aritmetica>
                         | <expressao_aritmetica> LT <expressao_aritmetica>
                         | <expressao_aritmetica> GE <expressao_aritmetica>
                         | <expressao_aritmetica> LE <expressao_aritmetica>

<expressao_aritmetica> ::= <expressao_aritmetica> PLUS <termo>
                         | <expressao_aritmetica> MINUS <termo>
                         | <termo>

<termo>                ::= <termo> STAR <fator>
                         | <termo> SLASH <fator>
                         | <termo> MOD <fator>
                         | <fator>

<fator>                ::= PLUS <fator>
                         | MINUS <fator>
                         | <primario>

<primario>             ::= IDENT
                         | INTEGER
                         | REAL
                         | STRING
                         | TRUE
                         | FALSE
                         | NONE
                         | LPAREN <expressao> RPAREN
```

---

## 4. Precedencia e Associatividade dos Operadores

Listados da **maior** para a **menor** precedencia (a estratificacao da gramatica e as diretivas do Bison garantem a mesma hierarquia):

| Nivel | Operadores | Descricao | Associatividade |
|---:|---|---|---|
| 1 (maior) | `( ... )` | Agrupamento por parenteses | — |
| 2 | `+x`, `-x` | Unarios aritmeticos (`UPLUS`, `UMINUS`) | Direita |
| 3 | `*`, `/`, `%` | Multiplicacao, divisao e modulo | Esquerda |
| 4 | `+`, `-` | Adicao (ou concatenacao) e subtracao | Esquerda |
| 5 | `==`, `!=`, `<`, `>`, `<=`, `>=` | Comparacoes relacionais e de igualdade | Nao associativo (`1 < 2 < 3` e erro sintatico) |
| 6 | `not` | Negacao logica | Direita |
| 7 | `and` | Conjuncao logica | Esquerda |
| 8 (menor) | `or` | Disjuncao logica | Esquerda |

---

## 5. Regras de Declaracao e Utilizacao de Variaveis

1. **Declaracao implicita por atribuicao:** Nao ha palavra-chave de declaracao (`int`, `let`, etc.). Uma variavel passa a existir na tabela de simbolos no momento de sua primeira atribuicao (`x = expr;`).
2. **Uso antes da atribuicao e erro semantico:** Referenciar um identificador em qualquer expressao antes de ele ter recebido um valor gera erro semantico (`variavel 'x' utilizada antes de receber valor`).
3. **Escopo unico (modelo Python):** Todas as variaveis pertencem ao escopo global do programa. Uma variavel criada dentro de um bloco `if`, `else` ou `while` permanece registrada apos o bloco; na geracao de codigo JavaScript, o tradutor faz *hoisting* (`let x;` no topo) para variaveis cuja primeira atribuicao ocorre em profundidade > 0.
4. **Reatribuicao e tipagem dinamica controlada:** Uma variavel pode receber um novo valor de outro tipo. Quando ramos distintos de um `if`/`else` ou iteracoes de um `while` atribuem tipos diferentes a mesma variavel, o analisador semantico unifica os tipos:
   - `inteiro` + `real` -> `real`
   - tipos distintos incompativeis -> `dinamico`
5. **Palavras reservadas de JavaScript como identificadores:** Identificadores validos em PtoJS que colidem com palavras reservadas do JS (ex.: `var`, `new`, `class`, `console`) sao aceitos normalmente e recebem o prefixo `_tojs_` no codigo gerado.

---

## 6. Regras Semanticas

O analisador semantico ([`semantic.c`](../semantic.c)) percorre a AST e infere/valida os tipos: `inteiro`, `real`, `string`, `logico`, `None` e `dinamico`.

### 6.1. Matriz de Compatibilidade de Operadores

| Operacao | Operandos aceitos | Tipo resultante |
|---|---|---|
| `+` (binario) | `numerico + numerico` (`inteiro`/`real`) | `real` se houver `real`, senao `inteiro` |
| `+` (binario) | `string + string` | `string` (concatenacao) |
| `-`, `*`, `%` | `numerico` e `numerico` | `real` se houver `real`, senao `inteiro` |
| `/` | `numerico` e `numerico` | sempre `real` |
| `+x`, `-x` (unario) | `numerico` | mesmo tipo do operando |
| `<`, `>`, `<=`, `>=` | ambos `numerico` **ou** ambos `string` | `logico` |
| `==`, `!=` | ambos `numerico`, ambos `string`, ambos `logico`, ou qualquer comparacao com `None` | `logico` |
| `and`, `or` | `logico` e `logico` | `logico` |
| `not` | `logico` | `logico` |

> Operandos de tipo `dinamico` sao aceitos quando existe compatibilidade possivel com o operador; caso contrario, o erro e reportado.

### 6.2. Verificacoes Obrigatorias Implementadas

1. **Variavel nao definida:** Uso de identificador nao presente/inativo na tabela de simbolos.
2. **Condicoes estritamente logicas:** As expressoes de condicao de `if` e `while` devem resultar em tipo `logico` (ou `dinamico`). Exemplos invalidos: `if 1:`, `while "abc":`.
3. **Incompatibilidade de tipos em expressoes:** Rejeita operacoes como `"a" + 1`, `"a" - 1`, `True + 1`, `-"abc"`, `not 3`, `1 < "a"`, `1 == "a"`, `1 and 2`.
4. **Divisao e modulo por zero literal:** Rejeita `expr / 0`, `expr / 0.0`, `expr % 0` e `expr % 0.0` em tempo de analise semantica.
5. **Revalidacao de condicao de `while`:** Apos analisar o corpo do `while`, a condicao e verificada novamente para capturar casos em que o corpo altera o tipo de uma variavel usada na condicao.
6. **Acumulo de erros semanticos:** Diferentemente dos erros lexicos/sintaticos (que interrompem imediatamente), a analise semantica percorre toda a AST, relata todos os erros encontrados com numero de linha e cancela a geracao do `.js` se houver ao menos um erro.

---

## 7. Exemplos de Programas Validos e Invalidos

### 7.1. Programas Validos

**Exemplo 1 — Tipos, atribuicao, condicional e repeticao:**
```python
nota = 7;
if nota >= 7:
    print("Aprovado");
else:
    print("Reprovado");

i = 1;
soma = 0;
while i <= 5:
    soma = soma + i;
    i = i + 1;

print(soma);
```
Saida esperada ao executar o JS gerado com Node.js:
```text
Aprovado
15
```

**Exemplo 2 — Precedencia aritmetica e logica:**
```python
print(2 + 3 * 4);
print((2 + 3) * 4);
print(True or False and False);
```
Saida esperada:
```text
14
20
true
```

Mais exemplos completos estao em [`exemplos/`](../exemplos/) e [`tests/validos/`](../tests/validos/).

### 7.2. Programas Invalidos

**Erro lexico (caractere invalido):**
```python
x = 10 @ 2;
```
Resultado esperado: `Erro lexico na linha 1: caractere inesperado '@'` (codigo de saida != 0, nenhum `.js` gerado).

**Erro sintatico (falta de `;` ou comparacao encadeada):**
```python
x = 10
```
Resultado esperado: `Erro sintatico na linha 1: syntax error, unexpected NEWLINE, expecting FIM_COMANDO`.

**Erro semantico (variavel nao definida e tipos incompativeis):**
```python
print(nao_existe);
y = "texto" + 10;
if 42:
    print("erro");
```
Resultado esperado:
- `Erro semantico na linha 1: variavel 'nao_existe' utilizada antes de receber valor.`
- `Erro semantico na linha 2: operador '+' exige dois operandos numericos ou duas strings, mas recebeu string e inteiro.`
- `Erro semantico na linha 3: condicao de 'if' deve resultar em valor logico, mas recebeu inteiro.`
- `Traducao cancelada: 3 erro(s) semantico(s).`

A suíte completa de casos invalidos esta em [`tests/invalidos/`](../tests/invalidos/).
