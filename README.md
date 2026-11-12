# SonyHeadphonesClient — fork para o WH-1000XM5 no Windows

Fork pessoal do [mos9527/SonyHeadphonesClient](https://github.com/mos9527/SonyHeadphonesClient), que por sua vez sucede o [cliente original do Plutoberth](https://github.com/Plutoberth/SonyHeadphonesClient). O objetivo é controlar o **Sony WH-1000XM5** pelo **Windows**: cancelamento de ruído, bateria e equalizador. Para isso, o fork acrescenta ícone na bandeja, atalhos globais, reconexão automática e presets de EQ próprios.

O desenvolvimento acontece na branch [`xm5`](https://github.com/rafaelholder/SonyHeadphonesClient/tree/xm5). A `v1-compat` espelha o upstream.

[![Build](https://github.com/rafaelholder/SonyHeadphonesClient/actions/workflows/cmake.yml/badge.svg?branch=xm5)](https://github.com/rafaelholder/SonyHeadphonesClient/actions/workflows/cmake.yml?query=branch%3Axm5)

![Janela do cliente](./docs/images/sc-readme-hero.png)

## O que este fork acrescenta

| Recurso | Como funciona |
|---|---|
| **Ícone na bandeja** | Headphone em pixel art, preto com contorno claro (para aparecer na barra escura), e uma barrinha de bateria embaixo: verde acima de 50%, amarela de 21 a 50%, vermelha até 20% e azul carregando. Desconectado, o ícone fica apagado. A dica do mouse mostra modelo, bateria e modo. |
| **Menu da bandeja** | Troca entre cancelamento de ruído, som ambiente e desligado; Próximo modo; submenu **Equalizador**; Reconectar automaticamente; Iniciar com o Windows; Abrir janela; Sair. |
| **Fechar para a bandeja** | O X da janela só esconde o app. Para encerrar, use **Sair** no menu. |
| **Atalhos globais** | **Ctrl+Alt+H**: próximo modo (cancelamento → ambiente → desligado). **Ctrl+Alt+J**: abre a janela, ou esconde se ela estiver em foco. Funcionam com qualquer janela em foco. |
| **Reconexão automática** | O app lembra o último fone. A cada 3 s pergunta ao Windows se ele está conectado, lendo o cache local do Bluetooth, sem procurar o fone pelo rádio. Quando o Windows conecta o áudio, o app abre o canal de controle sozinho. Se o fone cair, o app volta a esperar em vez de mostrar o aviso de desconectado. |
| **Iniciar com o Windows** | Opcional, marcado no menu da bandeja. Cria uma entrada por usuário em `HKCU\...\Run` com `--tray` e não precisa de administrador. |
| **Presets de EQ** | Na aba Som, a seção **Meus presets** aplica, salva o EQ atual com um nome e exclui presets. Os mesmos presets aparecem no submenu Equalizador da bandeja. Ao aplicar, o app coloca o fone no preset manual da Sony e envia as 5 bandas e o Clear Bass. |
| **Instalador e desinstalador** | `Setup.exe` feito com Inno Setup ([`installer/`](./installer/SonyHeadphonesClient.iss)): instala por usuário, sem administrador, e aparece em *Aplicativos* com desinstalador. Para desenvolvimento há também [`tooling/install-windows.ps1`](./tooling/install-windows.ps1). |

O controle original continua igual: bateria, cancelamento de ruído e som ambiente com nível, EQ de 5 bandas, Clear Bass, DSEE, desligar o fone e o depurador de protocolo. O multiponto e o Speak-to-Chat existem, mas o upstream ainda não os testou no XM5. A tabela de suporte do XM5 está em [`docs/device-support/WH-1000XM5.md`](./docs/device-support/WH-1000XM5.md).

### Por que Ctrl+Alt+H e J

No layout **US-Internacional**, Ctrl+Alt equivale a AltGr: AltGr+, gera "ç" e AltGr+N gera "ñ". H e J não geram caractere nenhum no AltGr, então os atalhos não atrapalham a digitação em português. Se outro programa já usar uma das combinações, ela não é registrada e o menu deixa de mostrar a dica dela.

## Instalação

### Pelo instalador (recomendado)

O CI gera a cada push um `SonyHeadphonesClient-Setup-<versão>-x64.exe`, disponível como artefato **SonyHeadphonesClient-Setup-win-x64** na aba [Actions](https://github.com/rafaelholder/SonyHeadphonesClient/actions). Para gerar localmente, use `tooling/build-installer.ps1` (veja abaixo).

- **Sem administrador:** instala só para o seu usuário em `%LOCALAPPDATA%\Programs\SonyHeadphonesClient`, com atalho no menu Iniciar.
- **Opções do assistente:** atalho na área de trabalho e **Iniciar com o Windows**. A caixa já vem marcada se o autostart estiver ligado, e desmarcar desliga.
- **Atualizar:** rode um instalador mais novo por cima. Ele fecha o app aberto e mantém o fone salvo e os presets.
- **Desinstalar:** em *Configurações › Aplicativos › SonyHeadphonesClient (WH-1000XM5)*. O desinstalador fecha o app e remove o programa, os atalhos e o "Iniciar com o Windows". Depois pergunta se deve apagar também o `settings.ini`; a resposta padrão é manter.
- **Modo silencioso:** use `/VERYSILENT /TASKS=autostart`. Uma desinstalação silenciosa (`unins000.exe /VERYSILENT`) mantém as configurações.

⚠️ O instalador não tem assinatura digital, então o SmartScreen mostra "O Windows protegeu o computador" na primeira vez. Para seguir, clique em *Mais informações › Executar assim mesmo*.

### Compilando você mesmo

O `.exe` é compilado a partir deste código, e o CI também gera o executável solto como artefato a cada push.

### Requisitos

- Windows 10 ou 11 x64, com o fone já pareado em *Configurações › Bluetooth*
- [CMake](https://cmake.org/) 3.31 ou mais novo
- Visual Studio Build Tools 2022, com a carga "Desenvolvimento para desktop com C++" e o Windows 11 SDK

```powershell
winget install --id Kitware.CMake --exact
winget install --id Microsoft.VisualStudio.2022.BuildTools --exact --override "--quiet --wait --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.Windows11SDK.26100"
```

### Compilar, testar e instalar

```powershell
git clone -b xm5 https://github.com/rafaelholder/SonyHeadphonesClient.git
cd SonyHeadphonesClient
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DMDR_BUILD_CODEGEN=OFF
cmake --build build --config Release --target SonyHeadphonesClient mdr_tests
ctest --test-dir build -C Release
pwsh -File tooling\install-windows.ps1
```

O script de instalação:

1. fecha o cliente, se estiver aberto;
2. copia o `.exe` para `%LOCALAPPDATA%\Programs\SonyHeadphonesClient` e confere o SHA-256 da cópia;
3. cria um atalho no menu Iniciar;
4. se o "Iniciar com o Windows" já estiver ligado, aponta a entrada para a cópia instalada (nunca liga essa opção sozinho);
5. abre o cliente na bandeja.

Rode o script de novo depois de cada recompilação. Use `-NoStart` para não abrir o cliente ao final e `-Uninstall` para remover o programa, o atalho e a entrada de inicialização (o `settings.ini` é mantido).

O `.exe` só importa DLLs do próprio Windows. Não precisa do runtime do Visual C++.

Para gerar o instalador a partir do `.exe` compilado, instale o [Inno Setup 6](https://jrsoftware.org/isinfo.php) e rode o script. O resultado sai em `build\installer\`. A definição do instalador fica em [`installer/SonyHeadphonesClient.iss`](./installer/SonyHeadphonesClient.iss), que precisa ficar salvo em UTF-8 com BOM, senão os acentos das mensagens se perdem.

```powershell
winget install --id JRSoftware.InnoSetup --exact
pwsh -File tooling\build-installer.ps1
```

### Primeiro uso

1. Abra o cliente e conecte o WH-1000XM5 pela janela. Isso salva o fone para a reconexão automática.
2. Se quiser, marque **Iniciar com o Windows** no menu da bandeja.
3. A partir daí, o app sobe na bandeja e conecta sozinho sempre que o Windows conectar o fone.

Feche o app da Sony no celular enquanto usa o cliente no PC, para os dois não disputarem o canal de controle.

## Linha de comando

| Opção | Efeito |
|---|---|
| `--tray` | Inicia escondido na bandeja. É o que o "Iniciar com o Windows" usa. Se a bandeja não puder ser criada, a janela aparece normalmente. |
| `--con` | Abre um console com o log de diagnóstico. |
| `--locale <default\|sc\|tc\|jp\|kr>` | Força o idioma da interface (do upstream). |
| `--font <arquivo>` | Usa uma fonte externa (do upstream). |
| `-h`, `--help` | Mostra a ajuda. |

## Onde ficam os dados

| O quê | Onde |
|---|---|
| Programa instalado | `%LOCALAPPDATA%\Programs\SonyHeadphonesClient\SonyHeadphonesClient.exe` |
| Configurações | `%APPDATA%\rafaelholder\SonyHeadphonesClient\settings.ini` |
| Iniciar com o Windows | Valor `SonyHeadphonesClient` em `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`, só quando marcado |

O `settings.ini` é texto puro e pode ser editado com o app fechado:

```ini
last_device=XX:XX:XX:XX:XX:XX
last_protocol=v2
auto_connect=1
eq_presets_initialized=1
# eq_preset=<nome>|<clear bass>|<banda 1>,<banda 2>,...
eq_preset=Plano|0|0,0,0,0,0
eq_preset=Graves|6|2,0,0,0,0
eq_preset=Voz e podcast|-2|-2,1,3,2,0
eq_preset=Jogos (passos)|-3|-3,-1,2,4,2
```

No XM5, as bandas são 400 Hz, 1 kHz, 2,5 kHz, 6,3 kHz e 16 kHz, e o app ajusta os valores ao limite que o fone informa. Os quatro presets iniciais são só ponto de partida e não foram afinados de ouvido. Linhas malformadas são ignoradas, com limite de 16 presets e 10 bandas.

## Segurança

Antes de qualquer mudança, o código do upstream foi auditado (escopo: o build Windows). O relatório, com evidências e os comandos para reproduzir, está em [`AUDITORIA.md`](./AUDITORIA.md). Em resumo:

- **Rede:** só Bluetooth (socket RFCOMM com autenticação e criptografia). Não há HTTP nem telemetria.
- **Sistema:** o app não executa outros programas, não injeta código e não usa hook de teclado. Os atalhos usam `RegisterHotKey`, que só recebe as duas combinações registradas.
- **Gravações:** só o `settings.ini`, a entrada `Run` (quando você marca a opção) e os arquivos que você mandar salvar.
- **Dependências:** SDL, ImGui e fmt são baixados no build fixados pelo commit exato, e não pela tag.
- **CI do fork:**
  - só verificação de formatação e código gerado, build Windows x64 e testes;
  - actions fixadas por SHA e permissão só de leitura;
  - sem deploy no GitHub Pages.

## Compatibilidade

- **Testado:** WH-1000XM5 no Windows 11 x64.
- **Outros fones Sony:** o protocolo vem do upstream e deve funcionar, mas os recursos deste fork não foram testados com eles. A troca de modo, por exemplo, segue o que o fone informa que suporta.
- **Outras plataformas:** Linux, macOS, Android, iOS e Web continuam no código do upstream. Este fork não compila nem testa essas plataformas, e a bandeja, os atalhos, o autostart e a reconexão automática só existem no Windows. Para elas, use o [upstream](https://github.com/mos9527/SonyHeadphonesClient).

## Sincronizar com o upstream

```bash
git fetch upstream
git diff xm5...upstream/v1-compat --stat
git merge upstream/v1-compat
```

Antes do merge, revise o diff do upstream com os mesmos critérios do [`AUDITORIA.md`](./AUDITORIA.md). Se o `contrib/CMakeLists.txt` do upstream mudar de versão de dependência, fixe o novo commit por SHA.

## Para desenvolvedores

- [`client/Tray.cpp`](./client/Tray.cpp): bandeja, ícone e submenu de EQ, via `SDL_tray` do SDL já fixado.
- [`client/Hotkeys.cpp`](./client/Hotkeys.cpp): atalhos globais.
- [`client/Settings.cpp`](./client/Settings.cpp): leitura e gravação do `settings.ini`.
- [`client/Autostart.cpp`](./client/Autostart.cpp): entrada de "Iniciar com o Windows".
- Em [`client/Client.cpp`](./client/Client.cpp): reconexão automática (`UpdateAutoConnect`, `RecoverAutoDisconnect`) e presets (`clientApplyEqPreset`).
- [`tooling/make-pixel-icon.py`](./tooling/make-pixel-icon.py): gera o `app.ico` (só biblioteca padrão). Mantenha o desenho igual ao `kHeadphoneSprite` em `client/Tray.cpp`.
- O CI exige o **clang-format 22.1.8**: `uv run --no-project --with clang-format==22.1.8 clang-format -i <arquivos>`.

Detalhes de arquitetura, do protocolo MDR e do build das outras plataformas estão no [SHC Developer's Guide](./DEVELOPER.md) do upstream.

## Créditos e licença

- Código base: [mos9527](https://github.com/mos9527/SonyHeadphonesClient) e colaboradores, sobre o trabalho do [Plutoberth](https://github.com/Plutoberth/SonyHeadphonesClient).
- Licença [MIT](./LICENSE), mantida neste fork.
- Os recursos deste fork foram desenvolvidos com auxílio do Claude Code. Os commits trazem o crédito de coautoria.

*Projeto pessoal, sem qualquer vínculo com a Sony Corporation nem endosso dela.*
