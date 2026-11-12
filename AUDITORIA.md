# Auditoria de segurança do fork — SonyHeadphonesClient

- **Fork:** `rafaelholder/SonyHeadphonesClient` (de `mos9527/SonyHeadphonesClient`, licença MIT)
- **Commit auditado:** `ce2e54c` (branch `v1-compat`, 11/11/2026)
- **Escopo:** o que entra no build para **Windows x64**: `CMakeLists.txt`, `contrib/`, `client/` (núcleo + `Platform/Windows`), `libmdr/`, `libmdr-bt/src/Windows`, `tooling/*.cpp` e o CI (`.github/workflows/cmake.yml`).
- **Fora do escopo:** as plataformas Android, iOS, macOS, Linux, OpenHarmony e Web. As dependências de terceiros (SDL, ImGui, fmt) não foram lidas linha a linha (ver o item 3).
- **Método:** leitura manual dos pontos de entrada e do backend Bluetooth, além de buscas por padrões de comportamento malicioso. Os comandos estão no item 6.

## Veredito

**Não encontrei nenhum código malicioso nem suspeito no escopo.** Fiz buscas, não uma prova formal. Os riscos que sobram estão na cadeia de suprimentos (item 3), não no código do projeto.

## 1. O que o programa faz no seu PC

| Recurso | O que faz | Evidência |
|---|---|---|
| Rede | **Só Bluetooth.** Abre um socket `AF_BTH` / `BTHPROTO_RFCOMM` para o MAC do fone, com `SO_BTH_AUTHENTICATE` e `SO_BTH_ENCRYPT` ligados. Nenhum socket TCP/UDP, nenhum HTTP. | `libmdr-bt/src/Windows/ConnectionWindows.cpp:61-125` |
| BLE | Usa as APIs WinRT `Windows.Devices.Bluetooth*` e nenhuma outra. | `libmdr-bt/src/Windows/ConnectionWindowsBLE.cpp:3-32` |
| Escrita em disco | Duas funções, ambas disparadas por você: "salvar arquivo" (diálogo do SDL) e exportar a captura de pacotes do Debugger. | `client/SDLMain.cpp:519`, `client/Debugger.cpp:1082` |
| Leitura de disco | Lê fontes CJK do sistema por GDI (`GetFontData`) e mapeia arquivos em modo `PAGE_READONLY`. | `client/Platform/Windows/ClientPlatform.cpp:50-160` |
| Execução de processos, registro, injeção | **Nada.** Não usa `CreateProcess`, `ShellExecute`, `system`, `RegSet*`, `LoadLibrary`/`GetProcAddress`, `VirtualAlloc`/`VirtualProtect`, `WriteProcessMemory` nem hooks de teclado. | busca 6.2 |
| Telemetria e URLs | Nenhuma no build Windows. As URLs do código são comentários. O build Web baixa fontes Noto do jsDelivr, mas está fora do escopo. | busca 6.2; `client/Platform/Emscripten/config.js.in:8-11` |
| Comandos ao fone | O caminho "firmware update" só **lê** parâmetros (`UPDT_GET_PARAM`) e nunca grava firmware. | `libmdr/src/HeadphonesV1.cpp:187-200` |

## 2. Itens verificados sem achado

- **Trojan Source** (caracteres bidi ou invisíveis, CVE-2021-42574): nenhuma ocorrência.
- **Assembly inline e strings de shellcode** (`__asm`, `\xNN\xNN\xNN`): nenhuma ocorrência.
- **Submódulos git:** nenhum.
- **Binários versionados:**
  - os `.bin` em `tests/` são capturas de pacotes MDR, lidas como dado pelos testes de replay;
  - há ícones `.png`, `.ico` e `.icns`;
  - há o `gradle-wrapper.jar`, usado só no build Android (fora do escopo).
- **Fonte embutida** `client/Fonts/PlexSansIcon.c`: são 74 KB em base85 comprimido, gerados por `tooling/BinaryEmbed.cpp` a partir de `PlexSansIcon.ttf`. É dado que o ImGui interpreta, não código. ⚠️ Não conferi byte a byte se o blob é idêntico ao `.ttf`.
- **Código gerado** (`*/Generated/*`): o job `pre-check` do CI regenera tudo e falha se houver diferença (`git diff --exit-code`). Esse job passou no commit `ce2e54c` (`gh run list`).
- **O que roda durante o build:** `git rev-parse` em `DetectGitInfo.cmake`, para pegar a versão. Também roda `cmake -E copy_if_different`, que copia a DLL para os testes. Nenhum script baixado é executado.

## 3. Riscos de cadeia de suprimentos (não são malware, mas valem endurecer)

| # | Risco | Onde | Recomendação |
|---|---|---|---|
| S1 | As dependências são baixadas no configure **por tag**, que é mutável: fmt `12.1.0`, SDL `release-3.2.26` e ImGui `v1.92.7`. | `contrib/CMakeLists.txt:14-45` | Fixar pelo **SHA do commit** (`GIT_TAG <sha>`). |
| S2 | O YY-Thunks (zip binário) só entra no build "compatibility". Ele tem `URL_HASH SHA256`, o que é bom. | `contrib/CMakeLists.txt:6-11` | Não usar esse modo: o Windows 11 não precisa dele. |
| S3 | O CI usa actions por tag (`actions/checkout@v3`, `mymindstorm/setup-emsdk@v14`…). | `.github/workflows/cmake.yml` | Só importa se o CI rodar no fork. Nesse caso, fixar pelo SHA. |
| S4 | **No fork, o Actions está habilitado.** Um push em `v1-compat` dispararia o job `deploy`, que publica a versão Web no GitHub Pages da sua conta. | `cmake.yml`, job `deploy` | Remover `deploy`, `android` e `ios` do workflow do fork antes do primeiro push. |
| S5 | O SDL recebe um patch que desliga a `SDL_DYNAMIC_API`. O efeito é **positivo**: impede o carregamento de outra `SDL3.dll` via variável de ambiente. | `contrib/SDL-disable-dynapi.patch` | Manter. |

## 4. Conclusão sobre confiança

- O projeto é ativo e tem várias pessoas: 948 commits, cerca de 15 autores (`git shortlog`), commits do mantenedor no mesmo dia da auditoria e CI verde.
- O código que fala com o hardware é pequeno, e o Bluetooth do Windows tem cerca de 300 linhas.
- **Compilar o próprio binário a partir deste commit** elimina o risco de um `.exe` de release diferente do código-fonte.

## 4.1 Build local (11/11/2026, branch `xm5`)

- **Ferramentas:** CMake 4.4.4 e VS Build Tools 2022 (MSVC v143 + Windows 11 SDK 26100), instalados via winget com hash do instalador verificado.
- **S1 aplicado:** `contrib/CMakeLists.txt` agora fixa fmt `407c905e`, SDL `badbf8da` e ImGui `dac07199`. O `build-configure.log` confirma esses HEADs (`HEAD is now at …`).
- **Comandos de build:**
  - `cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DMDR_BUILD_CODEGEN=OFF`
  - `cmake --build build --config Release --target SonyHeadphonesClient mdr_tests`
- **Testes:** `ctest` passou em **6/6** (ABI + replay de WF-1000XM5, WF-LC900 ×3 e WH-1000XM3).
- **Avisos:** 2 avisos, ambos de flags de LTO no próprio SDL (D9025, C4652). Nenhum é do projeto.
- **Binário:** `build\client\Release\SonyHeadphonesClient.exe` (1,9 MB), SHA-256 `9FD72D8B70AA4FAC54330F93B0D2B4AE528ADC6DCC42B0366544AC15B733F95B`.

- **Teste com hardware:** o usuário confirmou que o binário conecta ao WH-1000XM5 e troca o modo de cancelamento de ruído (11/11/2026).

## 4.2 Mudanças próprias desta versão

- **Ícone na bandeja** (`client/Tray.cpp`, `client/Tray.hpp`):
  - usa só a API `SDL_tray` do SDL já fixado (sem dependência nova), além das funções `clientGetTrayStatus` e `clientSetNoiseMode` em `client/Client.cpp`;
  - não abre rede, não executa processos e não grava em disco;
  - os callbacks do menu só gravam em variáveis atômicas, e o acesso ao libmdr acontece sob o `gWorkerMutex` do loop principal (`client/SDLMain.cpp`);
  - testado pelo usuário com o WH-1000XM5 em 11/11/2026.
- **Atalhos globais** (`client/Hotkeys.cpp`, `client/Hotkeys.hpp`):
  - `RegisterHotKey`/`UnregisterHotKey` presos ao HWND da janela, mais `SDL_SetWindowsMessageHook` para receber o `WM_HOTKEY`;
  - não é hook de teclado (`SetWindowsHookEx`), então não lê outras teclas, só recebe as duas combinações registradas;
  - verificado com o app aberto: registrar as combinações por fora falha com o erro 1409 (já registrado); com o app fechado, ficam livres;
  - testado pelo usuário em 11/11/2026.
- **Reconexão automática e Iniciar com o Windows** (`client/Settings.*`, `client/Autostart.*`, `UpdateAutoConnect`/`RecoverAutoDisconnect` em `client/Client.cpp`, `clientPlatformIsBluetoothDeviceConnected` em `client/Platform/Windows/ClientPlatform.cpp`). Isto **muda o item 1 da tabela**, porque agora o app grava duas coisas fora do diálogo de "salvar":
  - o arquivo `%APPDATA%\rafaelholder\SonyHeadphonesClient\settings.ini`, com o endereço Bluetooth do último fone, o protocolo e o liga/desliga da reconexão. Ele é lido com limite de 4 KB e validação do formato do endereço;
  - o valor `SonyHeadphonesClient` em `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`, **só quando o usuário marca "Iniciar com o Windows"**, e removido quando ele desmarca. É por usuário e não pede administrador;
  - a presença do fone é consultada com `BluetoothGetDeviceInfo`, que lê o cache local do rádio sem inquiry nem paging. A conexão RFCOMM só é tentada quando o Windows já mostra o fone conectado;
  - testado pelo usuário em 11/11/2026: o `settings.ini` foi criado com `last_protocol=v2` e a chave Run aponta para `...\build\client\Release\SonyHeadphonesClient.exe --tray`.

- **Presets de EQ do usuário** (`client/Settings.*`, funções `clientApplyEqPreset`, `SaveCurrentEqAsPreset` e `MatchingEqPreset` em `client/Client.cpp`, submenu "Equalizador" em `client/Tray.cpp`):
  - ficam no mesmo `settings.ini`, como linhas `eq_preset=nome|clear bass|bandas`. A leitura é estrita: inteiros entre −100 e 100, no máximo 10 bandas e 16 presets, e o nome sai sem `|` nem caracteres de controle;
  - ao aplicar um preset, o app troca o fone para um preset customizável (Manual/Custom) e envia as bandas e o Clear Bass ajustados ao limite que o fone informa;
  - testado pelo usuário com o WH-1000XM5 em 11/11/2026: o som mudou ao aplicar os presets.
- **Ícone de headphone pixelado** (`kHeadphoneSprite`/`DrawHeadphone`/`traySetWindowIcon` em `client/Tray.cpp`, `client/Platform/Windows/app.ico` gerado por `tooling/make-pixel-icon.py`):
  - o script usa só a biblioteca padrão do Python (`zlib` e `struct`) e escreve PNG dentro do ICO;
  - a janela recebe o ícone por `SDL_SetWindowIcon` (`WM_SETICON`), em 6 tamanhos;
  - na bandeja, o ícone mantém a barra de bateria;
  - verificado pelo `WM_GETICON` da janela e pelo usuário na barra de tarefas em 11/11/2026.
- **CI do fork** (`.github/workflows/cmake.yml`), que resolve os itens **S3** e **S4**:
  - ficaram só o `pre-check` (clang-format, headers renderizados e codegen) e o build Windows x64 com os testes;
  - saíram o deploy no GitHub Pages, a versão Web, Android, iOS, macOS, Linux e as demais variantes de Windows;
  - as actions estão fixadas por SHA e o workflow só tem permissão de leitura (`permissions: contents: read`);
  - o código do fork foi formatado com o clang-format 22.1.8, a mesma versão do CI, e o repositório inteiro passa no `--dry-run --Werror`.
- **Instalação local** (`tooling/install-windows.ps1`):
  - copia o `.exe` para `%LOCALAPPDATA%\Programs\SonyHeadphonesClient` e confere o SHA-256 da cópia;
  - cria um atalho no menu Iniciar;
  - se a entrada Run já existir, aponta para a cópia instalada. Nunca liga o autostart sozinho;
  - `-Uninstall` desfaz tudo, menos o `settings.ini`;
  - o `.exe` importa apenas DLLs do sistema (`dumpbin /dependents`: KERNEL32, USER32, ADVAPI32, WS2_32, as api-ms-win-*…), sem runtime do Visual C++ nem DLL do projeto.

## 5. Pendências

- ⚠️ Conferir o blob da fonte contra o `.ttf`.
- ⚠️ Ler o código das dependências: elas vêm dos repositórios oficiais, e o S1 reduz o risco.
- ⚠️ Repetir a auditoria a cada merge do upstream, com `git diff ce2e54c..upstream/v1-compat`.

## 6. Comandos usados (para reproduzir)

```bash
# 6.1 estrutura e binários
git ls-files | grep -iE '\.(exe|dll|so|dylib|a|lib|bin|zip|jar|pyc|wasm|node)$'
# 6.2 APIs perigosas (excluindo outras plataformas, tests, docs)
git grep -nIiE 'curl|winhttp|wininet|URLDownload|InternetOpen|https?://|WSAStartup|AF_INET|getaddrinfo|connect\(|CreateProcess|ShellExecute|WinExec|system\(|popen|RegSet|RegCreate|LoadLibrary|GetProcAddress|VirtualAlloc|VirtualProtect|WriteProcessMemory|CreateRemoteThread|SetWindowsHookEx|GetAsyncKeyState|BCrypt|base64|telemetry|analytics'
# 6.3 Trojan Source
git grep -nIP '[\x{202A}-\x{202E}\x{2066}-\x{2069}\x{200B}\x{200E}\x{200F}]'
# 6.4 escrita em disco
git grep -nIE 'fopen|SDL_IOFrom|SDL_OpenURL|CreateFileW|WriteFile|getenv' -- client libmdr libmdr-bt/src/Windows
# 6.5 código executado no build
git grep -nE 'execute_process|add_custom_command|ExternalProject' -- '*.cmake' '*CMakeLists.txt'
```
