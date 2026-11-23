# Firmware - Planta de Motor DC com Encoder

Firmware de tempo real para controle digital em malha fechada e identificação dinâmica de sistemas de um Motor DC com encoder incremental em quadratura, desenvolvido para **ESP32** sobre **FreeRTOS Dual-Core**.

Este firmware atende simultaneamente às duas aplicações web da bancada do motor no projeto `lab-ctrl`:
- **Controle em Malha Fechada**: https://lab-ctrl-motor.vercel.app/ ([código-fonte web](../../site/motor-control/))
- **Identificação de Sistemas**: https://lab-ctrl-identification.vercel.app/ ([código-fonte web](../../site/motor-identification/))

---

## 1. Hardware e Pinagem (`include/config.h`)

| Componente / Sinal | Pino ESP32 | Descrição |
| :--- | :---: | :--- |
| **Motor IN1** | `GPIO 27` | Sentido de giro 1 (Ponte H L298N) |
| **Motor IN2** | `GPIO 14` | Sentido de giro 2 (Ponte H L298N) |
| **Motor EN (PWM)** | `GPIO 12` | Modulação PWM de velocidade (5 kHz, 8 bits: 0 a 255) |
| **Encoder Canal A** | `GPIO 18` | Interrupção externa de quadratura (Canal A) |
| **Encoder Canal B** | `GPIO 19` | Interrupção externa de quadratura (Canal B) |
| **Potenciômetro** | `GPIO 34` | Entrada analógica ADC1 para referência externa |

---

## 2. Módulos e Funcionalidades

### Controle em Malha Fechada (`STATE_RUNNING`)
- **Equação de Diferenças Configurável**: Suporte a controladores discretos lineares de ordem 1 a 10 ($C(z)$), recebendo vetores de coeficientes do erro (`coeff_e`) e do esforço anterior (`coeff_u`).
- **Grandezas Controladas**:
  - **Velocidade Angular (`speed`)**: malha fechada em $\text{rad/s}$ (atuação PWM unipolar de $0$ a $255$).
  - **Posição Angular (`position`)**: malha fechada em graus/radianos (atuação PWM bipolar de $-255$ a $255$).
- **Período de Amostragem ($T_s$) Dinâmico**: Ajustável entre $1\text{ ms}$ e $1000\text{ ms}$ (padrão $35\text{ ms}$).
- **Referência Interna ou Externa**: Sequência cíclica de degraus temporizados ou leitura contínua do potenciômetro no pino 34.

### Identificação Dinâmica de Sistemas (`STATE_IDENT`)
- **Gerador Pseudoaleatório Xoshiro128++ (`include/prng.h`)**:
  - Período de $2^{128}-1 \approx 3.4 \times 10^{38}$ amostras, aprovado em testes de uniformidade Qui-Quadrado ($\chi^2$).
  - **Mapeamento sem Viés de Módulo**: Seleção equiprovável entre múltiplos níveis de PWM via produto de ponto fixo de 64 bits:
    $$\text{idx} = \left\lfloor \frac{U_{32} \times \text{levelCount}}{2^{32}} \right\rfloor$$
  - **Semente (Seed)**: Quando `seed = 0`, utiliza o gerador de ruído térmico/RF de hardware da ESP32 (`esp_random()`); quando `seed > 0`, expande a semente deterministicamente via **SplitMix64** para ensaios reprodutíveis.
- **Parâmetro `stretch`**: Define por quantas amostras consecutivas de $T_s$ cada nível sorteado é mantido antes do próximo sorteio.

---

## 3. Arquitetura FreeRTOS Dual-Core

- **Núcleo 0 (`serial_comm_task`)**:
  - Opera a **460.800 baud** (8-N-1).
  - Filtra ruídos de linha, interpreta pacotes JSON e monitora o caractere único de parada de emergência (`'s'` ou `'S'`).
  - Despacha amostras da fila `qPlotData` de forma não-bloqueante (1 pacote por iteração) e limita o envio da leitura do potenciômetro a cada $50\text{ ms}$ para preservar banda serial em $T_s < 5\text{ ms}$.
- **Núcleo 1 (`controller_task`)**:
  - Executa a Máquina de Estados Finita (`STATE_IDLE`, `STATE_RUNNING`, `STATE_STOPPING`, `STATE_IDENT`) no período exato `sampleTimeMs`.
  - Esvazia imediatamente as filas de plotagem ao receber comandos de parada, reinício de tempo ou novo ensaio, evitando amostras residuais.
- **ISRs de Quadratura (`src/encoder_isr.cpp`)**:
  - Rotinas alocadas em `IRAM_ATTR` restritas a incrementos/decrementos inteiros atômicos. A conversão para ponto flutuante (radianos ou $\text{rad/s}$) ocorre apenas quando requisitada pelo laço de controle no Núcleo 1.

---

## 4. Protocolo de Comunicação Serial (460.800 baud)

### 4.1. Iniciar Controle em Malha Fechada (Navegador $\rightarrow$ ESP32)
```json
{
  "mode": "speed",
  "sample_time_ms": 35,
  "ref_type": "internal",
  "ref_min": 0.0,
  "ref_max": 15.0,
  "order": 2,
  "coeff_e": [7.98, -6.781],
  "coeff_u": [0.999, 0.0],
  "levels": [5.0, 12.0, 6.0, 2.0],
  "interval_s": 6
}
```

### 4.2. Iniciar Identificação de Sistemas (Navegador $\rightarrow$ ESP32)
```json
{
  "cmd": "ident",
  "measure": "speed",
  "sample_time_ms": 5,
  "stretch": 5,
  "levels": [0, 128, 255],
  "seed": 0
}
```

### 4.3. Comandos Operacionais
- `{"cmd":"stop"}` ou caractere `'s'` / `'S'`: Interrompe o motor imediatamente, limpa a fila de telemetria e retorna para `STATE_IDLE`.
- `{"cmd":"ping"}`: Handshake de conexão (responde `{"status":"ok"}`).
- `{"cmd":"reset_time"}`: Zera o cronômetro do ensaio ($t = 0\text{ ms}$) e limpa amostras pendentes.

### 4.4. Telemetria (ESP32 $\rightarrow$ Navegador)
Enviada linha a linha no formato [Teleplot](https://teleplot.fr/):
- `>t:<ms>`: Tempo decorrido em milissegundos desde o início do ensaio.
- `>ref:<valor>`: Valor atual da referência (no modo controle).
- `>omega:<rad/s>` ou `>angulo:<rad>`: Medição instantânea do encoder.
- `>u:<pwm>` ou `>u_ident:<pwm>`: Esforço de controle / excitação PWM aplicado ao motor.
- `>pot:<0.0000 a 1.0000>`: Posição normalizada do potenciômetro.

---

## 5. Compilação e Upload

A partir da raiz do repositório:
```bash
pio run -d firmwares/motor
pio run -d firmwares/motor -t upload
pio device monitor -d firmwares/motor
```
Ou executando diretamente dentro de `firmwares/motor/`:
```bash
pio run -t upload
```
