# NVR Monitor

Monitor desktop de câmeras RTSP escrito em C com FFmpeg, SDL2 e SQLite. Cada
câmera possui uma thread própria de conexão e decodificação; a renderização SDL
permanece na thread principal. O vídeo é apresentado com atraso controlado de
aproximadamente dois segundos usando uma fila de pacotes comprimidos.

## Recursos atuais

- streams RTSP H.264/H.265 sem processos `ffplay`;
- UDP ou TCP configurável por câmera;
- mosaicos automáticos de 1, 2, 4, 6, 8 ou 9 posições;
- duplo clique para tela cheia e `Esc` para voltar;
- cadastro, edição e exclusão de perfis pela interface;
- reconexão automática com espera progressiva;
- perfis persistidos em SQLite;
- ícones e integração com o menu de aplicativos;
- pacotes Linux, instalador Windows e arquivo portátil Windows.

## Obter o código com Git

```sh
git clone https://github.com/Hangell/nvr-monitor.git
cd nvr-monitor
```

Para atualizar uma cópia existente:

```sh
git pull --ff-only
```

## Linux

### Dependências no Debian/Ubuntu

```sh
sudo apt update
sudo apt install build-essential cmake ninja-build pkg-config git \
  libsdl2-dev libsqlite3-dev sqlite3 \
  libavformat-dev libavcodec-dev libavutil-dev libswscale-dev
```

### Compilar e executar sem instalar

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/nvr-monitor
```

O banco padrão fica em:

```text
~/.local/share/nvr-monitor/cameras.db
```

Se `XDG_DATA_HOME` estiver definido, ele será usado no lugar de `~/.local/share`.
Também é possível escolher outro banco:

```sh
./build/nvr-monitor --database /caminho/para/cameras.db
```

### Instalar a partir do código

```sh
./scripts/install-linux.sh
nvr-monitor
```

Por padrão a instalação usa `/usr/local`. Para outro prefixo:

```sh
PREFIX="$HOME/.local" ./scripts/install-linux.sh
```

### Desinstalar uma instalação feita pelo script

Use o mesmo diretório de build empregado na instalação:

```sh
./scripts/uninstall-linux.sh
```

Se foi usado um diretório diferente:

```sh
BUILD_DIR=meu-build ./scripts/uninstall-linux.sh
```

Para uma instalação em `$HOME/.local`, repita também o prefixo:

```sh
PREFIX="$HOME/.local" ./scripts/uninstall-linux.sh
```

### Gerar e instalar um pacote `.deb`

```sh
./scripts/package-linux.sh
sudo apt install ./build-package-linux/nvr-monitor_*.deb
```

Desinstalação do pacote:

```sh
sudo apt remove nvr-monitor
```

A desinstalação preserva os perfis. Para também apagar os dados do usuário,
remova manualmente `~/.local/share/nvr-monitor` depois de confirmar que não
precisa mais do banco.

## Windows 10/11

O build oficial para Windows usa o terminal **MSYS2 UCRT64**, que fornece versões
compatíveis do compilador, SDL2, FFmpeg e SQLite.

1. Instale o [MSYS2](https://www.msys2.org/).
2. Abra o terminal **MSYS2 UCRT64**.
3. Atualize e instale as dependências:

```sh
pacman -Syu
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

Clone e gere o instalador:

```sh
git clone https://github.com/Hangell/nvr-monitor.git
cd nvr-monitor
./scripts/package-windows.sh
```

Os resultados ficam em `build-package-windows/`:

- `.exe`: instalador NSIS com atalhos e desinstalador;
- `.zip`: versão portátil com as DLLs necessárias.

O banco padrão do Windows fica em:

```text
%APPDATA%\NVR Monitor\cameras.db
```

Para desinstalar, use **Configurações → Aplicativos → NVR Monitor →
Desinstalar** ou execute o desinstalador criado pelo NSIS na pasta de instalação.
Os perfis são preservados; a pasta em `%APPDATA%` só deve ser removida manualmente
se os dados não forem mais necessários.

## Build automático no GitHub

O workflow [`.github/workflows/build.yml`](.github/workflows/build.yml) executa
build e testes em Linux e Windows a cada pull request e push para `main`. Os
artefatos gerados são:

- Linux: pacote `.deb` e arquivo `.tar.gz`;
- Windows: instalador `.exe` e pacote portátil `.zip`.

Uma tag também executa o empacotamento:

```sh
git tag v0.1.0
git push origin v0.1.0
```

Os pacotes ficam disponíveis na seção **Artifacts** da execução do GitHub
Actions. O workflow não publica uma GitHub Release automaticamente.

## Uso

Na janela, **+ ADD CAMERA** abre o cadastro e **LAYOUT** seleciona a grade. Clique
uma vez no nome de uma câmera para editar sua conexão ou excluí-la. A exclusão
exige confirmação. Um duplo clique no vídeo abre a câmera em tela cheia; `Esc`
retorna à grade e `Q` encerra.

Para câmeras Yoosee, ative o NVR no aplicativo do celular e selecione UDP. Use
`/onvif2` na grade quando disponível e `/onvif1` como stream principal. O
`ffplay` serve apenas para diagnosticar uma URL; o NVR usa diretamente as
bibliotecas FFmpeg.

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
A decodificação atual é feita por software, sem aceleração de vídeo por hardware.

## Segurança dos perfis

O SQLite recebe permissão `0600` no Linux, mas não oferece criptografia. Usuário
e senha ficam persistidos no banco local. Não compartilhe o arquivo
`cameras.db`. A integração futura recomendada é armazenar as senhas no chaveiro
do sistema com Secret Service/libsecret no Linux e Credential Manager no Windows.

Arquivos `*.db` e diretórios de build são ignorados pelo Git para impedir que
credenciais e binários sejam enviados ao repositório.
