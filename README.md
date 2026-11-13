# To JS Translate (PtoJS)

Tradutor de uma linguagem estilo Python (`.tojs`) para JavaScript, escrito em C com Flex e Bison.

## Pipeline

```
arquivo.tojs -> lexer.l (analise lexica) -> parser.y (analise sintatica + AST)
             -> semantic.c (tipos e tabela de simbolos) -> codegen.c -> saida.js
```

## Como compilar e usar

```bash
make                                   # gera o executavel ./tojs
./tojs exemplos/01_basico.tojs saida.js   # traduz
node saida.js                          # executa o JS gerado

./tojs --ast arquivo.tojs              # imprime a AST e traduz
./tojs --ast-only arquivo.tojs         # so imprime a AST
./tojs --symbols arquivo.tojs          # imprime a tabela de simbolos
```

## A linguagem em resumo

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

Blocos sao delimitados por indentacao (como em Python). Nao ha `elif`, funcoes nem listas.

## Exemplos de execucao

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

Exemplo (`04_repeticao.tojs`):

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

## Testes

```bash
make test          # ou: ./run_tests.sh
```

O script traduz e executa (com Node) cada caso e compara com o resultado esperado.

**Validos** (`exemplos/` e `tests/validos/`): a saida do JS gerado deve ser igual as linhas `# SAIDA:` do arquivo (ou ao `.saida` ao lado). Cobrem comentarios, linhas vazias, unarios, `while` aninhado, identificadores reservados em JS (`var`, `new`, `class`), zeros a esquerda, programa vazio, arquivo sem newline final, indentacao com tab e precedencia logica.

**Invalidos** (`tests/invalidos/`): o tradutor deve falhar, nao gerar o `.js` e imprimir a mensagem indicada em `# ESPERA:`.

| Prefixo | Fase | Casos |
|---|---|---|
| `l` | lexica | caractere invalido (`@`, `$`, `!`), string nao fechada, indentacao inconsistente |
| `s` | sintatica | falta de `;`, falta de `:`, `print` sem parenteses, parentese aberto, `else` sem `if`, atribuicao sem valor, bloco sem indentacao, operador duplicado, comparacao encadeada, `x = y = 3`, palavra reservada como variavel |
| `m` | semantica | variavel nao definida, `"a" + 1`, `"a" - 1`, condicao nao logica (`if`/`while`), `not 3`, `1 < "a"`, `1 == "a"`, `1 and 2`, `-"a"`, `True + 1` |

## Decisoes e limitacoes conhecidas

- **Escopo unico** (como Python): variavel atribuida so em um ramo do `if` continua valida depois dele. O gerador declara `let x;` no topo do JS nesses casos (testes `v11` e `v12`).
- O tradutor para no **primeiro erro lexico ou sintatico**; erros semanticos sao acumulados e contados.
- Reais sao impressos pelo JS sem `.0` (`3.0` aparece como `3`).
- Identificadores que sao palavras reservadas do JS recebem o prefixo `_tojs_`.
