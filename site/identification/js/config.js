// ============================================================
// config.js — painel de configuração e validação para identificação PRBS
// ============================================================

const _DEFAULTS = {
  speed: {
    levels:     '0, 255',
    ts:         '5',
    stretch:    '5',
    seed:       '1234',
    seedRandom: true,
  },
  position: {
    levels:     '-255, 255',
    ts:         '5',
    stretch:    '5',
    seed:       '1234',
    seedRandom: true,
  },
};

// ---- Conteúdo dos modais de ajuda -----------------------------------

export const HELP_CONTENT = {
  measure: {
    title: 'Grandeza Medida',
    html: `
      <p><strong>Velocidade Angular:</strong> Mede a velocidade de rotação do eixo do motor expressa em <strong>rad/s</strong>.
      É a grandeza ideal para identificar a função de transferência do motor em velocidade: 
      <code>G_v(s) = Ω(s) / U(s)</code>. Neste modo, o sinal de excitação PWM utiliza apenas valores não-negativos (0 a 255).</p>
      <p><strong>Posição Angular:</strong> Mede o ângulo acumulado do eixo em <strong>radianos</strong>.
      Adequada para modelar a dinâmica de posição: 
      <code>G_p(s) = θ(s) / U(s)</code> (que inclui o polo integrador puro em s = 0). Permite valores bipolares de PWM (-255 a 255).</p>
    `,
  },
  prbs: {
    title: 'Sinal de Excitação',
    html: `
      <p>Sinal pseudoaleatório gerado através do algoritmo <strong>Xoshiro128++</strong> para excitar a planta em múltiplas frequências.</p>
      <hr>
      <p><strong>Parâmetros:</strong></p>
      <p>• <strong>Níveis:</strong> Amplitudes de PWM (<code>0 a 255</code> em velocidade; <code>-255 a 255</code> em posição).</p>
      <p>• <strong>Ts:</strong> Período de amostragem em ms (padrão: 5 ms).</p>
      <p>• <strong>Stretch:</strong> Quantidade de amostras consecutivas por nível (padrão: 5).</p>
      <p>• <strong>Seed:</strong> Semente inicial (inteiro positivo) para reprodutibilidade, ou aleatório via hardware TRNG.</p>
    `,
  },
};

/** Verifica se a string é estritamente um único número inteiro dentro de [min, max]. */
function isStrictInteger(str, min = -Infinity, max = Infinity) {
  if (typeof str !== 'string' && typeof str !== 'number') return false;
  const s = String(str).trim();
  if (!s) return false;
  if (s.includes(',') || s.includes('.')) return false;
  if (!/^-?\d+$/.test(s)) return false;
  const n = Number(s);
  if (!Number.isSafeInteger(n)) return false;
  return n >= min && n <= max;
}

/** Parseia string de níveis separados por vírgula em int[]. Valida limites do modo e rejeita não-inteiros. */
function parseLevels(str, measure) {
  const trimmed = (str ?? '').trim();
  if (!trimmed) return null;
  const parts = trimmed.split(',');
  const result = [];
  for (const p of parts) {
    const s = p.trim();
    if (s === '') return null;
    // Rejeita decimais, caracteres ou texto
    if (!/^-?\d+$/.test(s)) return null;
    const v = Number(s);
    if (!Number.isSafeInteger(v)) return null;
    if (measure === 'speed') {
      if (v < 0 || v > 255) return null; // Apenas positivos / não-negativos em velocidade
    } else {
      if (v < -255 || v > 255) return null; // Entre -255 e 255 em posição
    }
    result.push(v);
  }
  if (result.length === 0 || result.length > 20) return null;
  return result;
}

export class ConfigPanel {
  /**
   * @param {Object} els Dicionário com referências DOM dos inputs
   * @param {Function} onValidityChange Callback (isValid: boolean) => void
   */
  constructor(els, onValidityChange) {
    this._els = els;
    this._onValidityChange = onValidityChange;

    this._currentMeasure = 'speed';
    this._modeState = {
      speed:    { ..._DEFAULTS.speed },
      position: { ..._DEFAULTS.position },
    };

    this.ts      = 5;
    this.stretch = 5;
    this.levels  = [0, 255];
    this.seed    = 0;
    this.isValid = true;
  }

  get measure() {
    return document.querySelector('input[name="measure"]:checked')?.value ?? 'speed';
  }

  init() {
    this._applyState(this._modeState[this.measure]);
    this._bindInputs();
    this.validate();
  }

  _bindInputs() {
    // Rádios de grandeza medida
    document.querySelectorAll('input[name="measure"]').forEach(r => {
      r.addEventListener('change', () => this._onMeasureChange());
    });

    // Inputs de configuração
    this._els.inputLevels?.addEventListener('input', () => this.validate());
    this._els.inputTs.addEventListener('input', () => this.validate());
    this._els.inputStretch.addEventListener('input', () => this.validate());
    this._els.inputSeed?.addEventListener('input', () => this.validate());
    this._els.checkboxSeedRandom?.addEventListener('change', () => {
      const isRandom = this._els.checkboxSeedRandom.checked;
      if (this._els.inputSeed) {
        this._els.inputSeed.disabled = isRandom;
      }
      this.validate();
    });
  }

  _onMeasureChange() {
    const newMeasure = this.measure;
    if (newMeasure === this._currentMeasure) return;

    // 1. Salva inputs do modo atual antes de trocar
    this._modeState[this._currentMeasure] = this._captureInputs();

    // 2. Aplica estado persistido ou padrão do novo modo
    this._applyState(this._modeState[newMeasure]);
    this._currentMeasure = newMeasure;

    // 3. Valida os novos campos
    this.validate();
  }

  _captureInputs() {
    return {
      levels:     this._els.inputLevels?.value ?? '',
      ts:         this._els.inputTs?.value ?? '5',
      stretch:    this._els.inputStretch?.value ?? '5',
      seed:       this._els.inputSeed?.value ?? '1234',
      seedRandom: this._els.checkboxSeedRandom?.checked ?? true,
    };
  }

  _applyState(state) {
    if (this._els.inputLevels) this._els.inputLevels.value = state.levels;
    if (this._els.inputTs)     this._els.inputTs.value     = state.ts;
    if (this._els.inputStretch)this._els.inputStretch.value= state.stretch;
    if (this._els.inputSeed)   this._els.inputSeed.value   = state.seed ?? '1234';
    if (this._els.checkboxSeedRandom) {
      this._els.checkboxSeedRandom.checked = state.seedRandom ?? true;
      if (this._els.inputSeed) {
        this._els.inputSeed.disabled = this._els.checkboxSeedRandom.checked;
      }
    }
  }

  validate() {
    const currentMeasure = this.measure;
    const parsedLevels = parseLevels(this._els.inputLevels?.value, currentMeasure);
    
    // Ts: estritamente inteiro entre 1 e 1000 ms (sem ponto, sem vírgula, sem negativo)
    const isTsValid = isStrictInteger(this._els.inputTs?.value, 1, 1000);

    // Stretch: estritamente inteiro >= 1 (sem ponto, sem vírgula, sem negativo, sem limite superior!)
    const isStretchValid = isStrictInteger(this._els.inputStretch?.value, 1, Infinity);

    // Seed: se não for aleatório, deve ser estritamente um inteiro positivo (>= 1)
    const isRandom = this._els.checkboxSeedRandom?.checked ?? true;
    let isSeedValid = true;
    if (!isRandom) {
      isSeedValid = isStrictInteger(this._els.inputSeed?.value, 1, 2147483647);
    }

    const isLevelsValid = parsedLevels !== null;

    if (this._els.inputLevels) this._els.inputLevels.classList.toggle('is-invalid', !isLevelsValid);
    this._els.inputTs.classList.toggle('is-invalid', !isTsValid);
    this._els.inputStretch.classList.toggle('is-invalid', !isStretchValid);
    if (this._els.inputSeed)   this._els.inputSeed.classList.toggle('is-invalid', !isRandom && !isSeedValid);

    if (this._els.tsWarning) {
      const rawTs = parseInt(this._els.inputTs.value, 10);
      this._els.tsWarning.hidden = !(isTsValid && rawTs < 5);
    }

    this.isValid = isLevelsValid && isTsValid && isStretchValid && isSeedValid;

    if (this.isValid) {
      this.levels  = parsedLevels;
      this.ts      = parseInt(this._els.inputTs.value, 10);
      this.stretch = parseInt(this._els.inputStretch.value, 10);
      this.seed    = isRandom ? 0 : parseInt(this._els.inputSeed.value, 10);
    }

    if (typeof this._onValidityChange === 'function') {
      this._onValidityChange(this.isValid);
    }

    return this.isValid;
  }

  /** Retorna o payload JSON pronto para envio à ESP32 */
  getPayload() {
    if (!this.validate()) return null;
    return {
      cmd: 'ident',
      measure: this.measure,
      sample_time_ms: this.ts,
      stretch: this.stretch,
      levels: this.levels,
      seed: this.seed,
    };
  }

  /** Restaura os valores padrão do modo ativo */
  resetDefaults() {
    const cur = this.measure;
    this._modeState[cur] = { ..._DEFAULTS[cur] };
    this._applyState(this._modeState[cur]);
    this.validate();
  }

  /** Atualiza o card de telemetria em tempo real no painel lateral */
  updateStats(sampleCount, elapsedSec) {
    if (this._els.statSamples) {
      this._els.statSamples.textContent = sampleCount.toLocaleString('pt-BR');
    }
    if (this._els.statTime) {
      this._els.statTime.textContent = `${elapsedSec.toFixed(1)} s`;
    }
  }
}
