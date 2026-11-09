# lab-ctrl — Laboratório de Controle

Plataforma integrada de hardware e software para ensaios experimentais e aplicações práticas de Laboratório de Controle. Conecta plantas didáticas (motores DC, encoders de quadratura e sensores) diretamente ao navegador através da **Web Serial API** em alta velocidade (460.800 baud), permitindo controle digital em malha fechada, identificação dinâmica de sistemas e telemetria gráfica em tempo real.

- **Hub Central (Portal de Serviços)**: https://lab-ctrl.vercel.app/
- **Módulo de Controle do Motor**: https://lab-ctrl-motor.vercel.app/
- **Módulo de Identificação de Sistemas**: https://lab-ctrl-identification.vercel.app/
- **Repositório GitHub**: https://github.com/celiobenj/lab-ctrl

---

## Módulos e Funcionalidades

### 1. Módulo de Controle (`/control`)
- **Controle Digital por Equação de Diferenças**: Suporte a controladores discretos lineares de ordem 1 a 10 ($C(z)$), sintonizados remotamente sem necessidade de reprogramar o microcontrolador.
- **Grandezas de Controle**:
  - **Velocidade Angular**: malha fechada em rad/s com medição por contagem periódica de pulsos.
  - **Posição Angular**: malha fechada com medição de ângulo absoluto em radianos/graus.
- **Período de Amostragem ($T_s$) Configurável**: Período de execução do controlador ajustável dinamicamente (1 a 1000 ms, padrão 35 ms) com aviso de alta taxa de dados para $T_s < 5\text{ ms}$.
- **Fontes de Referência**:
  - **Interna**: Sequência cíclica de degraus de amplitude e intervalo configuráveis.
  - **Externa**: Leitura analógica em tempo real via potenciômetro (ADC1, pino 34), com mapeamento linear e gauge visual.
- **Visualização Gráfica Temporal**: Gráficos de referência vs. medição e esforço de controle ($u$ em PWM) com janelas deslizantes ajustáveis (0.5 s, 1 s, 5 s, 15 s, 30 s, 60 s ou tudo), cursores duplos e exportação CSV.

### 2. Módulo de Identificação de Sistemas (`/identification`)
- **Excitação Pseudoaleatória de Alta Qualidade (Xoshiro128++)**:
  - Substitui geradores clássicos LFSR com viés de módulo por um algoritmo PRNG moderno com período de $2^{128}-1 \approx 3.4 \times 10^{38}$ números e aprovação formal em testes estatísticos Qui-Quadrado ($\chi^2$).
  - **Mapeamento Uniforme**: Distribuição equiprovável rigorosa entre múltiplos níveis de PWM através de produto de ponto fixo em 64 bits (sem viés de módulo).
  - **Controle de Semente (Seed)**: Modo aleatório com entropia do gerador de ruído térmico/RF de hardware da ESP32 (`esp_random()`), ou semente positiva fixa expandida via SplitMix64 para ensaios reprodutíveis.
- **Configurações de Ensaio**: Período de amostragem ($T_s$), parâmetro `stretch` (sustentação de amostras por nível) e níveis de PWM bipolares ou unipolares.
- **Exportação de Dados**: Geração de séries temporais limpas em CSV prontas para identificação paramétrica (ARX, ARMAX, Box-Jenkins, subespaço) no MATLAB, Python ou Octave.

---

## Arquitetura e Tecnologias

- **Microcontrolador**: ESP32 (Xtensa Dual-Core 240 MHz) programado no framework Arduino / PlatformIO.
- **Arquitetura FreeRTOS Dual-Core**:
  - **Núcleo 0 (`serial_comm_task`)**: Leitura serial contínua a 460.800 baud, parser JSON, filtragem de ruído, comando de parada de emergência prioritário (`'s'`) e transmissão não-bloqueante de telemetria.
  - **Núcleo 1 (`controller_task`)**: FSM determinística (IDLE, RUNNING, STOPPING, IDENT), amostragem precisa no período $T_s$ configurado, equação de diferenças e modulação PWM em 5 kHz no driver L298N.
  - **Filas FreeRTOS (`QueueHandle_t`)**: Barramento thread-safe para envio assíncrono de comandos e pacotes de telemetria entre núcleos com esvaziamento imediato ao reiniciar ensaios.
- **Interrupções do Encoder (`encoder_isr.cpp`)**: Leitura em quadratura em pinos dedicados, restrita a operações inteiras atômicas para eliminar sobrecarga na CPU e desvio temporal.
- **Frontend Web**:
  - HTML5 moderno, CSS3 responsivo e JavaScript modular nativo (ES Modules).
  - **Web Serial API** para comunicação direta com a porta serial USB a 460.800 baud.
  - **Chart.js** com renderização gráfica por Canvas e janelas de visualização customizáveis.
  - **KaTeX** para renderização matemática da equação de diferenças em tempo real.

---

## Formato de Comunicação Serial

A comunicação ocorre a **460.800 de baudrate** (8-N-1) com comandos JSON delimitados por `\n`.

### 1. Iniciar Controle em Malha Fechada (Navegador -> ESP32)
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

### 2. Iniciar Identificação de Sistemas (Navegador -> ESP32)
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
- `seed`: `0` para semente aleatória gerada por ruído de hardware da ESP32, ou inteiro positivo $\ge 1$ para semente determinística.

### 3. Comandos Adicionais de Operação
- `{"cmd":"stop"}` ou envio do caractere único `'s'` / `'S'`: Parada imediata do motor, reset das filas de telemetria e retorno da FSM para o estado IDLE.
- `{"cmd":"ping"}`: Verificação de conectividade e handshake serial (retorna `{"status":"ok"}`).
- `{"cmd":"reset_time"}`: Reinicia a base temporal ($t = 0\text{ ms}$) e esvazia amostras residuais da fila.

### 4. Telemetria (ESP32 -> Navegador)
Linhas de texto em padrão [Teleplot](https://teleplot.fr/):
- `>t:<ms>`: Timestamp em milissegundos decorridos desde o início do ensaio.
- `>ref:<valor>`: Valor instantâneo da referência.
- `>omega:<rad/s>` ou `>angulo:<rad>`: Medição atual da planta via encoder.
- `>u:<pwm>`: Sinal de controle calculado e aplicado ao motor.
- `>pot:<0.0 a 1.0>`: Leitura normalizada do potenciômetro (taxa controlada a cada 50 ms).

---

## Estrutura do Repositório

```text
motor_control_serial/
|-- src/                     # Tarefas de firmware FreeRTOS, FSM, ISRs e comunicação serial
|   |-- main.cpp             # Ponto de entrada e criação das tarefas/filas FreeRTOS
|   |-- controller.cpp       # Lógica da FSM de controle e identificação no Núcleo 1
|   |-- serial_comm.cpp      # Leitura, parsing JSON e envio de telemetria no Núcleo 0
|   `-- encoder_isr.cpp      # Rotinas de interrupção em quadratura otimizadas
|-- include/                 # Definições de tipos, parâmetros e geradores
|   |-- config.h             # Definição de pinos, baudrate e limites
|   |-- fsm.h                # Estados da FSM, estruturas de eventos e telemetria
|   |-- controller.h         # Declaração do controlador e loop principal
|   |-- serial_comm.h        # Declaração da tarefa serial
|   |-- encoder_isr.h        # Declaração das ISRs do encoder
|   `-- prng.h               # Gerador Xoshiro128++ e SplitMix64
|-- lib/
|   |-- Motor/               # Driver de modulação PWM L298N
|   `-- Encoder/             # Classe de conversão de pulsos de quadratura
|-- site/                    # Aplicação Web (Portal e Módulos)
|   |-- index.html           # Portal / Hub Central de serviços
|   |-- favicon.svg          # Ícone visual da plataforma
|   |-- control/             # Módulo Web de Controle do Motor
|   |   |-- index.html       # Interface do usuário para controle
|   |   |-- css/             # Estilos do módulo de controle
|   |   `-- js/              # Web Serial, gráficos, validadores e renderização
|   `-- identification/      # Módulo Web de Identificação de Sistemas
|       |-- index.html       # Interface do usuário para identificação
|       |-- css/             # Estilos do módulo de identificação
|       `-- js/              # Web Serial, gráficos e validação do sinal de excitação
|-- platformio.ini           # Configuração de build para ESP32 Dev Module
|-- LICENSE                  # Licença MIT
`-- README.md                # Documentação técnica do projeto
```

---

## Licença

Distribuído sob a licença [MIT](https://opensource.org/license/mit). Consulte o arquivo [LICENSE](LICENSE) para mais detalhes.
