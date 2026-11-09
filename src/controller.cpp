// ============================================================
/// @file controller.cpp
/// @brief Implementacao da FSM, loop de controle e identificacao no Nucleo 1.
// ============================================================

#include "controller.h"
#include "encoder_isr.h"
#include "prng.h"

static Motor*         _motor       = nullptr;
static Encoder*       _encoder     = nullptr;
static QueueHandle_t  _qEvents     = nullptr;
static QueueHandle_t  _qPlot       = nullptr;

static FsmState       _state       = STATE_IDLE;
static ControlParams  _params;
static IdentParams    _identParams;
static Xoshiro128PlusPlus _prng;
static uint16_t           _prbsStretchCount = 0;
static int            _currentIdentPwm  = 0;

static float          _e_hist[MAX_ORDER + 1] = {}; ///< e[k], e[k-1], ...
static float          _u_hist[MAX_ORDER + 1] = {}; ///< u[k-1], u[k-2], ...

static int            _refIndex    = 0;
static float          _currentRef  = 0.0;
static unsigned long  _timerRef     = 0;
static unsigned long  _tInicio      = 0;  ///< Momento em ms em que o ensaio atual iniciou
static unsigned long  timer         = 0;


// -------------------------------------------------------
/// @brief Reinicia os buffers historicos e estados do encoder.
// -------------------------------------------------------
static void _reset_state() {
    for (int i = 0; i <= MAX_ORDER; i++) {
        _e_hist[i] = 0.0f;
        _u_hist[i] = 0.0f;
    }
    encoder_reset();
    _refIndex   = 0;
    if (_params.refType == REF_EXTERNAL) {
        float potNorm = constrain((float)analogRead(POT_PIN) / 4095.0f, 0.0f, 1.0f);
        _currentRef   = _params.refMin + potNorm * (_params.refMax - _params.refMin);
    } else {
        _currentRef = (_params.levelCount > 0) ? _params.levels[0] : 0.0f;
    }
    if (_qPlot != nullptr) {
        xQueueReset(_qPlot);
    }
    _timerRef     = millis();
    _tInicio      = millis();
    timer         = _tInicio;
}

// -------------------------------------------------------
/// @brief Calcula a saida do controlador pela equacao de diferencas.
// -------------------------------------------------------
static float _compute_control() {
    float u = 0.0f;
    for (int i = 0; i < _params.order; i++) {
        u += _params.coeff_e[i] * _e_hist[i];      // c_ei * e[k-i]
        u += _params.coeff_u[i] * _u_hist[i + 1];  // c_ui * u[k-1-i]
    }
    return u;
}

// ============================================================
// API publica
// ============================================================

void controller_init(Motor* motor, Encoder* encoder, QueueHandle_t qEvents, QueueHandle_t qPlot) {
    _motor   = motor;
    _encoder = encoder;
    _qEvents = qEvents;
    _qPlot   = qPlot;
    _state   = STATE_IDLE;
    pinMode(POT_PIN, INPUT);
}

FsmState controller_get_state() {
    return _state;
}

void controller_task(void* pvParameters) {
    (void)pvParameters;
    unsigned long idlePotTimer = 0;

    for (;;) {
        // --- Processamento de eventos recebidos da fila ---
        // Se estiver IDLE, aguarda ate 10 ms por eventos.
        // Se estiver rodando (RUNNING ou IDENT), aguarda ate 1 ms (1 tick)
        // para nao bloquear a amostragem, mas acordar IMEDIATAMENTE quando
        // um comando de parada (EVT_STOP) ou reconfiguracao chegar.
        TickType_t waitTicks = (_state == STATE_IDLE) ? pdMS_TO_TICKS(10) : pdMS_TO_TICKS(1);

        FsmEventMessage msg;
        if (xQueueReceive(_qEvents, &msg, waitTicks) == pdPASS) {
            do {
                if (msg.type == EVT_APPLY) {
                    _params = msg.params;
                    _motor->setSpeed(0);
                    _reset_state();
                    _state = STATE_RUNNING;
                } else if (msg.type == EVT_IDENT) {
                    _identParams = msg.identParams;
                    _motor->setSpeed(0);
                    _reset_state();
                    _prng.seed(_identParams.seed);
                    _prbsStretchCount = _identParams.stretch; // Forca selecao do primeiro nivel no primeiro tick
                    _currentIdentPwm  = 0;
                    _state = STATE_IDENT;
                    _tInicio = millis();
                    timer    = _tInicio;
                } else if (msg.type == EVT_STOP) {
                    _motor->setSpeed(0);
                    _reset_state();
                    _state = STATE_IDLE;
                } else if (msg.type == EVT_RESET_TIME) {
                    if (_qPlot != nullptr) {
                        xQueueReset(_qPlot);
                    }
                    _tInicio      = millis();
                    timer         = _tInicio;
                }
            } while (xQueueReceive(_qEvents, &msg, 0) == pdPASS);
        }

        // --- Maquina de Estados (FSM) ---
        switch (_state) {
            case STATE_IDLE: {
                // Envia leitura do potenciometro a cada 100 ms em repouso
                unsigned long now = millis();
                if (now - idlePotTimer >= 100) {
                    idlePotTimer = now;
                    if (_qPlot != nullptr) {
                        float potNorm = constrain((float)analogRead(POT_PIN) / 4095.0f, 0.0f, 1.0f);
                        PlotSample sample;
                        sample.t_ms          = 0;
                        sample.ref           = 0.0f;
                        sample.medida        = 0.0f;
                        sample.pwm           = 0;
                        sample.mode          = _params.mode;
                        sample.potNorm       = potNorm;
                        sample.isIdleSample  = true;
                        sample.isIdentSample = false;
                        xQueueSend(_qPlot, &sample, 0);
                    }
                }
                break;
            }

            case STATE_STOPPING:
                _motor->setSpeed(0);
                _reset_state();
                _state = STATE_IDLE;
                break;

            case STATE_RUNNING: {
                unsigned long now = millis();
                if (now - timer >= (unsigned long)_params.sampleTimeMs) {
                    timer = now;

                    // Leitura do potenciometro em tempo real
                    float potNorm = constrain((float)analogRead(POT_PIN) / 4095.0f, 0.0f, 1.0f);

                    if (_params.refType == REF_EXTERNAL) {
                        _currentRef = _params.refMin + potNorm * (_params.refMax - _params.refMin);
                    } else {
                        // Atualizacao da referencia interna por degraus
                        if (now - _timerRef >= (unsigned long)_params.intervalMs) {
                            if (_params.levelCount > 0) {
                                _refIndex   = (_refIndex + 1) % _params.levelCount;
                                _currentRef = _params.levels[_refIndex];
                            }
                            _timerRef = now;
                        }
                    }

                    // Deslocamento dos historicos e[k-1], u[k-1], etc.
                    for (int i = MAX_ORDER; i > 0; i--) {
                        _e_hist[i] = _e_hist[i - 1];
                        _u_hist[i] = _u_hist[i - 1];
                    }

                    // Medicao e calculo do erro atual
                    float medida = 0.0f;
                    if (_params.mode == MODE_SPEED) {
                        medida     = _encoder->get_omega(_params.sampleTimeMs);
                        _e_hist[0] = _currentRef - medida;
                    } else {
                        medida     = encoder_get_angle_rad() * RAD_TO_DEG;
                        _e_hist[0] = _currentRef - medida;
                    }

                    // Calculo da equacao de diferencas
                    float u_now = _compute_control();

                    // Saturacao do sinal de controle
                    int pwm_out = 0;
                    float u_hist_val = u_now;
                    
                    if (_params.mode == MODE_SPEED) {
                        u_now   = constrain(u_now, 0.0f, 255.0f);
                        pwm_out = (int)u_now;
                        u_hist_val = u_now; // Speed control saturava o historico
                    } else {
                        float u_sat = constrain(u_now, -255.0f, 255.0f);
                        pwm_out = (int)u_sat;
                        u_hist_val = u_now; // Position control nao saturava o historico
                    }

                    _u_hist[0] = u_hist_val;
                    _motor->setSpeed(pwm_out);

                    // Enfileira cada amostra calculada de acordo com o Ts configurado
                    if (_qPlot != nullptr) {
                        PlotSample sample;
                        sample.t_ms          = now - _tInicio;
                        sample.ref           = _currentRef;
                        sample.medida        = medida;
                        sample.pwm           = pwm_out;
                        sample.mode          = _params.mode;
                        sample.potNorm       = potNorm;
                        sample.isIdleSample  = false;
                        sample.isIdentSample = false;
                        xQueueSend(_qPlot, &sample, 0);
                    }
                }
                break;
            }

            case STATE_IDENT: {
                unsigned long now = millis();
                if (now - timer >= (unsigned long)_identParams.sampleTimeMs) {
                    timer = now;

                    // Sorteio pseudo-aleatorio uniforme do nivel a cada expiracao do stretch (Xoshiro128++)
                    if (_prbsStretchCount >= _identParams.stretch) {
                        _prbsStretchCount = 0;
                        if (_identParams.levelCount > 0) {
                            int idx = _prng.nextRange(_identParams.levelCount);
                            _currentIdentPwm = _identParams.levels[idx];
                        }
                    } else {
                        _prbsStretchCount++;
                    }

                    int pwm_out = _currentIdentPwm;
                    _motor->setSpeed(pwm_out);

                    // Medicao da resposta da planta
                    float medida = 0.0f;
                    if (_identParams.measureSpeed) {
                        medida = _encoder->get_omega(_identParams.sampleTimeMs);
                    } else {
                        medida = encoder_get_angle_rad(); // em radianos
                    }

                    // Enfileira cada amostra calculada de acordo com o Ts configurado
                    if (_qPlot != nullptr) {
                        PlotSample sample;
                        sample.t_ms          = now - _tInicio;
                        sample.ref           = 0.0f;
                        sample.medida        = medida;
                        sample.pwm           = pwm_out;
                        sample.mode          = _identParams.measureSpeed ? MODE_SPEED : MODE_POSITION;
                        sample.potNorm       = 0.0f;
                        sample.isIdleSample  = false;
                        sample.isIdentSample = true;
                        xQueueSend(_qPlot, &sample, 0);
                    }
                }
                break;
            }
        }
    }
}
