// ============================================================
/// @file encoder_isr.cpp
/// @brief Implementacao otimizada das rotinas de interrupcao do encoder.
///        Nao utiliza operacoes de ponto flutuante dentro das ISRs para
///        evitar context switch de registradores FPU e gargalo de CPU.
// ============================================================

#include "encoder_isr.h"

static Encoder*     _enc        = nullptr;
static signed long  _pontoAtual = 0;   ///< Posicao acumulada em pulsos

// -------------------------------------------------------
/// @brief ISR chamada nas transicoes de nivel do canal A.
///        Apenas operacoes inteiras atomicas de contagem.
// -------------------------------------------------------
static void IRAM_ATTR _isr_canal_A() {
    if (digitalRead(ENC_A) == HIGH) {
        (digitalRead(ENC_B) == LOW) ? (_pontoAtual++, _enc->pulses_motor++)
                                    : (_pontoAtual--, _enc->pulses_motor--);
    } else {
        (digitalRead(ENC_B) == HIGH) ? (_pontoAtual++, _enc->pulses_motor++)
                                     : (_pontoAtual--, _enc->pulses_motor--);
    }
}

// -------------------------------------------------------
/// @brief ISR chamada nas transicoes de nivel do canal B.
///        Apenas operacoes inteiras atomicas de contagem.
// -------------------------------------------------------
static void IRAM_ATTR _isr_canal_B() {
    if (digitalRead(ENC_B) == HIGH) {
        (digitalRead(ENC_A) == HIGH) ? (_pontoAtual++, _enc->pulses_motor++)
                                     : (_pontoAtual--, _enc->pulses_motor--);
    } else {
        (digitalRead(ENC_A) == LOW) ? (_pontoAtual++, _enc->pulses_motor++)
                                    : (_pontoAtual--, _enc->pulses_motor--);
    }
}

// -------------------------------------------------------

void encoder_isr_init(Encoder* enc) {
    _enc = enc;
    pinMode(ENC_A, INPUT);
    pinMode(ENC_B, INPUT);
    attachInterrupt(digitalPinToInterrupt(ENC_A), _isr_canal_A, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENC_B), _isr_canal_B, CHANGE);
}

float encoder_get_angle_rad() {
    if (_enc == nullptr || _enc->ENC_COUNT_REV == 0) return 0.0f;
    // O calculo do angulo em radianos e executado apenas quando solicitado pelo laco de controle,
    // eliminando dezenas de milhares de divisoes float por segundo dentro das ISRs.
    return (float)(TWO_PI * _pontoAtual) / (float)_enc->ENC_COUNT_REV;
}

void encoder_reset() {
    noInterrupts();
    _pontoAtual = 0;
    if (_enc != nullptr) {
        _enc->pulses_motor          = 0;
        _enc->ActualPoint_enc_motor = 0;
    }
    interrupts();
}
