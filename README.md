# lab-ctrl - Laboratório de Controle

O **lab-ctrl** é uma plataforma web integrada a bancadas físicas de Laboratório de Controle. Ele permite realizar experimentos práticos de controle em malha fechada e identificação de sistemas dinâmicos diretamente pelo navegador, em tempo real, conectando o computador ao microcontrolador da bancada via cabo USB.

---

## Aplicações Web

Acesse diretamente pelo navegador (Google Chrome ou Microsoft Edge):

- **Portal Principal (Hub)**: https://lab-ctrl.vercel.app/
- **Controle de Motor DC**: https://lab-ctrl-motor.vercel.app/
- **Identificação de Motor DC**: https://lab-ctrl-identification.vercel.app/

---

## Plantas Disponíveis

| Planta | Aplicações Web | Documentação Técnica e Firmware |
| :--- | :--- | :--- |
| **Motor DC com Encoder** | [Controle](https://lab-ctrl-motor.vercel.app/) / [Identificação](https://lab-ctrl-identification.vercel.app/) | [Documentação do Motor DC](firmwares/motor/README.md) |

Para detalhes técnicos sobre hardware, pinagem da ESP32, protocolo de comunicação serial e instruções de gravação do código na placa, consulte a documentação específica da planta na tabela acima.

---

## Organização do Repositório

```text
lab-ctrl/
├── firmwares/                      # Códigos embarcados (ESP32) de cada bancada física
│   └── motor/                      # Firmware e documentação técnica da planta de Motor DC
│       ├── include/                # Cabeçalhos e configurações do firmware
│       ├── lib/                    # Bibliotecas de acionamento do motor e leitura do encoder
│       ├── src/                    # Código-fonte principal
│       ├── platformio.ini          # Configuração do projeto PlatformIO
│       └── README.md               # Documentação técnica detalhada da planta do motor
│
├── site/                           # Interfaces web da plataforma
│   ├── index.html                  # Portal principal
│   ├── favicon.svg                 # Ícone da plataforma
│   ├── motor-control/              # Aplicação web de controle do motor
│   └── motor-identification/       # Aplicação web de identificação do motor
│
├── .gitignore
├── LICENSE                         # Licença MIT
└── README.md                       # Apresentação geral do projeto
```

---

## Licença

Distribuído sob a licença [MIT](https://opensource.org/license/mit). Consulte o arquivo [LICENSE](LICENSE) para mais informações.
