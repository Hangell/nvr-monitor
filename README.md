# NVR Monitor

Monitor de câmeras RTSP para Linux e Windows, escrito em C com FFmpeg, SDL2 e
SQLite. Permite cadastrar câmeras, acompanhar várias imagens em uma grade e
abrir uma câmera em tela cheia. As conexões ficam salvas para o próximo uso.

**Para editar uma conexão salva, basta clicar uma vez sobre o nome da câmera
na grade, alterar os campos e clicar em SALVAR.**

## Por onde começar

| Objetivo | Instruções |
| --- | --- |
| Compilar e instalar pelo Git no Linux | [Linux: instalação pelo Git](#linux-instalação-pelo-git) |
| Compilar e instalar pelo Git no Windows | [Windows: instalação pelo Git](#windows-instalação-pelo-git) |
| Baixar um build gerado pelo GitHub | [Pacotes do GitHub Actions](#pacotes-do-github-actions) |
| Cadastrar ou editar uma câmera | [Como usar](#como-usar) |
| Localizar os perfis salvos | [Dados e conexões salvas](#dados-e-conexões-salvas) |

## Recursos

- Vídeo RTSP H.264/H.265, com UDP ou TCP configurável por câmera.
- Grades de 1, 2, 4, 6, 8 ou 9 posições.
- Duplo clique no vídeo para ampliar; `Esc` para voltar.
- Cadastro, edição e exclusão de conexões pela interface.
- Reconexão automática quando uma câmera perde a conexão.
- Perfis persistidos em SQLite e integração com o menu de aplicativos.
- Pacotes Linux, instalador Windows e pacote Windows com as DLLs necessárias.

O vídeo tem atraso controlado de aproximadamente dois segundos. Cada câmera
possui uma thread de conexão e decodificação; a interface usa a thread principal.
Não são necessários processos `ffplay` para exibir as câmeras.

## Linux: instalação pelo Git

Os comandos abaixo usam Debian, Ubuntu ou Linux Mint. Execute-os no terminal.
Em outras distribuições, instale os pacotes equivalentes antes de compilar.

### 1. Instalar as ferramentas e dependências

```sh
sudo apt update
sudo apt install build-essential cmake ninja-build pkg-config git \
  libsdl2-dev libsqlite3-dev sqlite3 \
  libavformat-dev libavcodec-dev libavutil-dev libswscale-dev
```

### 2. Baixar o código e instalar

```sh
git clone https://github.com/Hangell/nvr-monitor.git
cd nvr-monitor
./scripts/install-linux.sh
```

O script compila em modo Release, executa os testes e instala em `/usr/local`.
A senha de administrador é solicitada na etapa de instalação. Execute o script
como seu usuário normal; ele chama `sudo` quando necessário.

### 3. Abrir o programa

Procure **NVR Monitor** no menu de aplicativos ou execute:

```sh
nvr-monitor
```

O script instala a entrada no menu. Para um atalho na área de trabalho, use a
opção do seu ambiente gráfico para adicionar o aplicativo à área de trabalho.

### Atualizar uma instalação feita pelo Git

Feche o NVR Monitor e, dentro da pasta do repositório, execute:

```sh
git pull --ff-only
./scripts/install-linux.sh
nvr-monitor
```

Não é necessário desinstalar antes de atualizar. O script substitui a versão
instalada e preserva as conexões. Apenas `git pull` ou compilar não atualiza o
executável usado pelo atalho: é preciso executar a instalação e reabrir o programa.
Se o Git indicar alterações locais ou divergência, resolva-as antes de continuar.

### Compilar e testar sem instalar

Dentro da pasta do repositório:

```sh
cmake -S . -B build-local -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-local --parallel 2
ctest --test-dir build-local --output-on-failure
./build-local/nvr-monitor
```

Esse executável é separado da versão instalada no sistema. O limite de duas
compilações simultâneas reduz a carga durante o build.

### Instalar somente para seu usuário

```sh
PREFIX="$HOME/.local" ./scripts/install-linux.sh
"$HOME/.local/bin/nvr-monitor"
```

Para usar o comando `nvr-monitor` e atalhos com esse prefixo, `$HOME/.local/bin`
deve estar no `PATH` da sessão gráfica. Se também existir uma instalação em
`/usr/local`, confira qual está sendo usada com `command -v nvr-monitor`.

### Gerar um pacote Linux

Como alternativa à instalação pelo script, gere os pacotes:

```sh
./scripts/package-linux.sh
```

Os arquivos `.deb` e `.tar.gz` ficam em `build-package-linux/`. Para instalar o
`.deb` em Debian, Ubuntu ou Mint:

```sh
sudo apt install ./build-package-linux/nvr-monitor_*.deb
```

Se houver mais de um `.deb` na pasta, informe o nome exato do pacote desejado.
Escolha um método de instalação para facilitar futuras atualizações e remoções.

### Desinstalar

Para a instalação feita pelo script, na pasta do repositório:

```sh
./scripts/uninstall-linux.sh
```

O script usa o manifesto de `build-release/`; mantenha essa pasta para a
remoção. Caso tenha usado outro diretório de build, informe o mesmo valor:

```sh
BUILD_DIR=meu-build ./scripts/uninstall-linux.sh
```

Para a instalação apenas do usuário:

```sh
PREFIX="$HOME/.local" ./scripts/uninstall-linux.sh
```

Para a instalação feita por `.deb`:

```sh
sudo apt remove nvr-monitor
```

Essas opções preservam as conexões salvas.

## Windows: instalação pelo Git

O build usa **MSYS2 UCRT64** e os pacotes de 64 bits listados abaixo.
Execute os comandos desta seção nesse terminal, e não no PowerShell ou no CMD.

### 1. Preparar o MSYS2

Instale o [MSYS2](https://www.msys2.org/) e abra **MSYS2 UCRT64** pelo menu
Iniciar. Atualize os pacotes:

```sh
pacman -Syu
```

Se a atualização pedir para fechar o terminal, confirme, reabra **MSYS2 UCRT64**
e execute `pacman -Syu` novamente para concluir. Esse procedimento está descrito
na [documentação de atualização do MSYS2](https://www.msys2.org/docs/updating/).

Instale as ferramentas e dependências:

```sh
pacman -S --needed git \
  mingw-w64-ucrt-x86_64-toolchain \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-pkgconf \
  mingw-w64-ucrt-x86_64-SDL2 \
  mingw-w64-ucrt-x86_64-ffmpeg \
  mingw-w64-ucrt-x86_64-sqlite3 \
  mingw-w64-ucrt-x86_64-nsis
```

Aceite a seleção padrão quando o gerenciador perguntar quais componentes do
grupo de ferramentas instalar.

### 2. Baixar o código e gerar o instalador

```sh
git clone https://github.com/Hangell/nvr-monitor.git
cd nvr-monitor
./scripts/package-windows.sh
```

O script compila em modo Release, executa os testes e gera em
`build-package-windows/`:

| Arquivo | Como usar |
| --- | --- |
| `.exe` | Abra o instalador e siga as etapas; depois inicie pelo atalho criado. |
| `.zip` | Extraia todo o conteúdo e abra `bin/nvr-monitor.exe` dentro da pasta extraída. Mantenha as DLLs junto do executável. |

Para abrir a pasta dos resultados no Explorador de Arquivos, execute no MSYS2:

```sh
explorer.exe build-package-windows
```

A versão extraída do ZIP também salva os perfis na pasta de dados do usuário;
os perfis não ficam dentro do ZIP nem ao lado do executável.

### Compilar e testar sem gerar instalador

Depois de instalar as dependências e clonar o repositório, execute no UCRT64:

```sh
cmake -S . -B build-local -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-local --parallel 2
ctest --test-dir build-local --output-on-failure
./build-local/nvr-monitor.exe
```

Execute esse build pelo terminal UCRT64, que disponibiliza as DLLs das
dependências no `PATH`. Para executar fora desse terminal, use o instalador ou
o ZIP gerado pelo script de empacotamento.

### Atualizar

Feche o NVR Monitor. Na pasta do repositório, pelo terminal UCRT64:

```sh
git pull --ff-only
./scripts/package-windows.sh
```

Abra o novo instalador `.exe` gerado. Se usa o ZIP, extraia o novo pacote em
uma nova pasta e atualize seu atalho para o executável dessa pasta.
Gerar o build não substitui automaticamente o programa instalado. As conexões
salvas são preservadas.

### Desinstalar

Use **Configurações → Aplicativos → NVR Monitor → Desinstalar** ou execute o
desinstalador na pasta de instalação. Se usa o ZIP, feche o programa e remova a
pasta extraída. Os dados do usuário são preservados nos dois casos.

## Pacotes do GitHub Actions

O [workflow de build](.github/workflows/build.yml) compila, testa e empacota Linux
e Windows em pushes para `main`, tags `v*`, pull requests e execução manual.

Para obter um pacote de uma execução concluída:

1. Abra a aba **Actions** do repositório e selecione **Build and package**.
2. Escolha uma execução bem-sucedida da revisão desejada.
3. Na seção **Artifacts**, baixe `nvr-monitor-linux` ou `nvr-monitor-windows`.
4. Extraia o arquivo baixado para acessar os pacotes gerados.

O artefato Linux contém `.deb` e `.tar.gz`; o Windows contém o instalador `.exe`
e o pacote `.zip`. Use as instruções de instalação da plataforma acima.
O workflow não publica uma GitHub Release automaticamente.

## Como usar

### Adicionar uma câmera

Clique em **+ ADD CAMERA**, preencha os dados da conexão e clique em **SALVAR**.
Use **LAYOUT** para escolher a quantidade de posições na grade.

### Editar uma conexão salva

**Basta clicar uma vez sobre o nome exibido na imagem da câmera.** Na tela de
edição, altere os dados necessários e clique em **SALVAR**.

Se a câmera estiver ampliada, pressione `Esc` para voltar à grade e então
clique no nome. Não é necessário excluir e cadastrar a câmera novamente.
Para excluir, abra a mesma tela e use **DELETAR**, seguido da confirmação.

### Controles

| Ação | Controle |
| --- | --- |
| Editar a conexão salva | Um clique no nome da câmera, na grade. |
| Ampliar uma câmera | Duplo clique na imagem, fora da faixa do nome. |
| Voltar à grade | `Esc` com a câmera ampliada. |
| Fechar um painel | `Esc` com o painel aberto. |
| Encerrar o programa | `Q` na grade, sem painel aberto, ou fechar a janela. |

Para câmeras Yoosee, ative o NVR no aplicativo do celular e selecione UDP. Use
`/onvif2` na grade quando disponível e `/onvif1` como stream principal. O
`ffplay` serve apenas para diagnosticar uma URL; o NVR usa diretamente as
bibliotecas FFmpeg.

## Dados e conexões salvas

| Sistema | Banco padrão |
| --- | --- |
| Linux | `~/.local/share/nvr-monitor/cameras.db` |
| Windows | `%APPDATA%\NVR Monitor\cameras.db` |

No Linux, se `XDG_DATA_HOME` estiver definido, ele substitui `~/.local/share`.
Para fazer backup, feche o programa e copie o banco para um local seguro.
Atualizar ou desinstalar o aplicativo não apaga esse arquivo.

Também é possível escolher outro banco:

```sh
nvr-monitor --database /caminho/para/cameras.db
```

No Windows, use `nvr-monitor.exe --database "C:\caminho\cameras.db"` com o
executável instalado. Remova o banco manualmente apenas se quiser apagar as
conexões salvas.

## Solução de problemas de instalação

| Situação | O que conferir |
| --- | --- |
| `cmake`, compilador ou biblioteca não encontrado | Instale as dependências da sua plataforma antes de compilar. |
| CMake informa conflito de gerador | Use uma nova pasta de build; não misture Ninja e Makefiles na mesma pasta. |
| `nvr-monitor: command not found` no Linux | Conclua a instalação ou execute o caminho do binário compilado; confira o `PATH` se usou um prefixo personalizado. |
| O atalho continua abrindo uma versão antiga | Feche o programa, reinstale o build atualizado e confira o destino do atalho. No Linux, `command -v nvr-monitor` mostra o destino no terminal. |
| DLL ausente no Windows | Extraia o ZIP inteiro ou use o instalador. Para builds sem empacotamento, execute pelo UCRT64. |
| O script Windows solicita MSYS2 UCRT64 | Abra o terminal UCRT64 pelo menu Iniciar e execute o script nele. |
| A conexão precisa de um ajuste | Clique no nome da câmera na grade, edite os campos e salve. |

## Consumo de recursos e múltiplos monitores

A apresentação é limitada a no máximo 25 quadros por segundo e a janela só é
redesenhada quando há imagens novas ou alterações na interface. Minimizada ou
oculta, ela não renderiza nem converte imagens para BGRA. Na visualização de uma
única câmera, a conversão das demais também fica suspensa. As conexões RTSP e a
decodificação continuam para preservar as referências do vídeo e permitir a
retomada sem reconexão; portanto o consumo de CPU não chega necessariamente a zero.

Perder o foco não suspende o vídeo: a janela pode continuar visível em outro
monitor. Isso também vale para monitores conectados por adaptadores USB, como
Wavlink. O limite de atualização independe do VSync e da frequência do monitor.
A decodificação tenta usar automaticamente a GPU compatível com o codec: VA-API
no Linux, D3D11VA/DXVA2 no Windows, VideoToolbox no macOS e CUDA para NVIDIA.
É necessário que o FFmpeg e os drivers instalados ofereçam suporte ao backend.
A GPU aguarda o primeiro quadro-chave ao conectar para iniciar com referências válidas.
Sem GPU compatível, o programa usa CPU automaticamente. Se a aceleração falhar
durante o stream, passa para CPU até a próxima conexão; a imagem pode aguardar
o próximo quadro-chave. Os logs informam a seleção e o fallback.

A conversão para BGRA ainda usa CPU; quadros de câmeras ocultas não são
transferidos da GPU para conversão. A aceleração pode reduzir a carga de CPU,
mas a temperatura depende do hardware e da quantidade de streams.

## Segurança dos perfis

O SQLite recebe permissão `0600` no Linux, mas não oferece criptografia. Usuário
e senha ficam persistidos no banco local. Não compartilhe o arquivo
`cameras.db`. A integração futura recomendada é armazenar as senhas no chaveiro
do sistema com Secret Service/libsecret no Linux e Credential Manager no Windows.

Arquivos `*.db` e diretórios de build são ignorados pelo Git para impedir que
credenciais e binários sejam enviados ao repositório.
