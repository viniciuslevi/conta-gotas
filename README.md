# Contador de Gotas Infravermelho

Firmware para ESP32 que conta gotas utilizando um par óptico infravermelho (TIL32 emissor + TIL78 receptor) no modo barreira — o feixe é interrompido pela gota ao passar entre os componentes.

## Hardware

### Lista de componentes

| Componente | Qtd | Especificação |
|---|---|---|
| ESP32 | 1 | Qualquer variante (DevKit, WROOM, etc) |
| TIL32 | 1 | LED emissor infravermelho |
| TIL78 | 1 | Fototransistor receptor IR |
| Resistor 220Ω | 1 | Para o TIL32 (emissor) |
| Resistor 10kΩ | 1 | Pull-down no TIL78 (receptor) |
| Buzzer ativo | 1 | 3.3V, 2 pinos |
| Cabos jumper | — | Para ligações na protoboard |

### Esquema de ligação

```
  3.3V ──────┬──────────────────────────────────┬─────────────
             │                                  │
           [220Ω]                             [10kΩ]
             │                                  │
         ┌───┴───┐                         ┌───┴───┐
         │ TIL32 │                         │ TIL78 │
         │ Emiss │                         │ Recep │
         │  IR   │        feixe IR         │  IR   │
         │  ◄────┼─────────────────────────┼───►   │
         └───┬───┘                         └───┬───┘
             │                                  │
           GND                               GND ──── GPIO 34 (ESP32)


  BUZZER ATIVO (2 pinos):

  GPIO 25 ──── (+) BUZZER (-) ──── GND


  ESP32 - PINOS UTILIZADOS:

  ┌─────────────────────┐
  │                     │
  │  3V3 ───────────────┤─── Alimentação (TIL32 + pull-up TIL78)
  │  GND ───────────────┤─── Terra comum
  │  GPIO 34 ───────────┤─── Saída do TIL78 (analógico)
  │  GPIO 25 ───────────┤─── Buzzer ativo
  │  USB ───────────────┤─── Programação + Serial Monitor
  │                     │
  └─────────────────────┘
```

### Posicionamento dos sensores

```
        vista lateral:

   conta-gotas
       │
       ▼
  ─────┼──────────────────────
       │
       │    gota
       │     │
  TIL32│     ▼               TIL78
  [====|===⌒⌒⌒⌒⌒⌒⌒⌒⌒====|====]
  IR ──►── feixe cortado ──►── receptor
       │
  ─────┼──────────────────────
       │
    suporte / protoboard
```

O TIL32 e TIL78 ficam frente a frente com o conta-gotas entre eles, a ~1-2cm de distância. A gota deve cair através do feixe IR. O LED do TIL32 aponta direto para a lente do TIL78.

## Firmware

### Método de detecção: Pico em Janela com Baseline Adaptativa

O firmware funciona em 3 etapas:

**1. Calibração automática** — coleta 200 amostras com feixe livre (sem gotas):
- Calcula a **baseline** (média) → valor "normal" do sensor
- Calcula o **ruído** (desvio padrão σ) → variação natural do sinal
- Define o **limiar** = maior valor entre `(ruído × MARGEM_SIGMA)` e `LIMIAR_MINIMO_ADC`

**2. Monitoramento** — a cada 1ms lê o ADC e registra o pico de desvio em janelas de 50ms:

```
Tempo:  ├── 50ms ├── 50ms ├── 50ms ├──
Pico:      2       7       1       3
Limiar:    5       5       5       5
Detectou:  NÃO     SIM     NÃO     NÃO
                              ↑
                    gota causou pico=7
```

**3. Debounce** — após detectar, ignora novos sinais por 150ms (evita dupla contagem)

A baseline é recalculada automaticamente a cada 15s para compensar drift do sensor e mudanças ambientais.

### Saída esperada no Serial Monitor

```
==========================================
  Contador de Gotas IR v3.3
  Pico em janela + Buzzer
==========================================

CALIBRACAO: mantenha o feixe LIVRE (sem gotas)

Calibrando... 50/200 (raw: 1845)
Calibrando... 100/200 (raw: 1847)
Calibrando... 150/200 (raw: 1844)

=== Calibracao concluida ===
Baseline: 1845.3 | Ruido: 1.52 | Limiar: 5 ADC
Aguardando gotas...

[DBG] pico=2 lim=5 baseline=1845
[DBG] pico=1 lim=5 baseline=1845
>>> DETECTADA: pico=8
Gota #1 | Intervalo: 0 ms | Fluxo inst: 0.0 gotas/min | Fluxo media: 0.0 gotas/min
[DBG] pico=1 lim=5 baseline=1845
>>> DETECTADA: pico=12
Gota #2 | Intervalo: 520 ms | Fluxo inst: 115.4 gotas/min | Fluxo media: 115.4 gotas/min
[Janela 60s] Gotas: 12 | Fluxo: 12.0 gotas/min
```

### Upload

1. Abra a pasta `conta-gotas/` na Arduino IDE
2. Selecione a placa ESP32 correspondente
3. Selecione a porta serial correta
4. Compile e grave

## Ajuste de Sensibilidade

### Parâmetros ajustáveis

```cpp
const float MARGEM_SIGMA = 3.0;              // quanto ruído tolera
const int LIMIAR_MINIMO_ADC = 5;              // piso absoluto do limiar
const unsigned long JANELA_AMOSTRA_MS = 50;   // janela de captura do pico
const unsigned long DEBOUNCE_MS = 150;        // trava entre detecções
```

### Tabela de referência rápida

| Parâmetro | Efeito ao aumentar | Efeito ao diminuir |
|---|---|---|
| `MARGEM_SIGMA` | Menos sensível — limiar mais alto, menos falsos positivos | Mais sensível — limiar mais baixo, detecta gotas menores |
| `LIMIAR_MINIMO_ADC` | Menos sensível — piso mínimo mais alto | Mais sensível — permite limiares baixos |
| `JANELA_AMOSTRA_MS` | Captura gotas mais lentas, mais chance de acumular ruído | Responde mais rápido, pode perder gotas lentas |
| `DEBOUNCE_MS` | Mais tempo entre detecções (evita dupla contagem) | Permite contar gotas mais próximas |

### Receitas de ajuste

**Muito sensível (falsos positivos):**
```cpp
const float MARGEM_SIGMA = 4.0;       // era 3.0
const int LIMIAR_MINIMO_ADC = 8;       // era 5
```

**Pouco sensível (não detecta):**
```cpp
const float MARGEM_SIGMA = 2.0;       // era 3.0
const int LIMIAR_MINIMO_ADC = 3;       // era 5
const unsigned long JANELA_AMOSTRA_MS = 80; // era 50
```

### Como diagnosticar com o debug

Com `MODO_DEBUG = true`, a linha abaixo aparece a cada 300ms:
```
[DBG] pico=2 lim=5 baseline=1845
```

| Situação | O que observar | Ação |
|---|---|---|
| Falsos positivos em repouso | `pico` próximo de `lim` sem gota | Aumentar `MARGEM_SIGMA` ou `LIMIAR_MINIMO_ADC` |
| Não detecta gotas | `pico` fica abaixo de `lim` quando a gota cai | Diminuir `MARGEM_SIGMA` e `LIMIAR_MINIMO_ADC` |
| Conta 2x a mesma gota | Dois `>>> DETECTADA` próximos | Aumentar `DEBOUNCE_MS` |
| Perde gotas rápidas | `>>> DETECTADA` não aparece | Aumentar `JANELA_AMOSTRA_MS` |

### Fluxograma de calibração

```
Início
  │
  ▼
Ligar com feixe LIVRE
  │
  ▼
Aguardar "Calibracao concluida"
  │
  ▼
Observar [DBG] pico por 30s sem gotas
  │
  ├─ pico sempre < lim ──► OK, ir para teste com gotas
  │
  └─ pico >= lim ──► Aumentar MARGEM_SIGMA ou LIMIAR_MINIMO_ADC
                      e regravar firmware
  │
  ▼
Deixar cair gotas
  │
  ├─ ">>> DETECTADA" aparece ──► OK, ajustar debounce se necessário
  │
  └─ Não aparece ──► Diminuir MARGEM_SIGMA e LIMIAR_MINIMO_ADC
                      e regravar firmware
```

## Limitações

O par TIL32/TIL78 opera em infravermelho, que atravessa líquidos transparentes (água, soro) com pouca atenuação. Para esses casos, considere:

- **Isolamento óptico** — tubo opaco envolvendo o par sensor elimina luz ambiente
- **Sensor de luz visível** — LED branco + LDR ou fototransistor visível detecta refração da gota
- **Laser** — módulo KY-008 + LDR gera sinal forte mesmo com água transparente

O firmware atual funciona com qualquer um desses sensores sem mudanças de código — basta trocar os componentes e ajustar o limiar.
