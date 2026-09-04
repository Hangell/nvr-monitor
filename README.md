# NVR Monitor

Protótipo de monitor NVR em C. Cada câmera possui sua própria thread de conexão e
decodificação por FFmpeg; a criação das texturas e toda renderização SDL ficam na
thread principal. A fila limitada mantém no máximo dois quadros e descarta os mais
antigos para evitar acúmulo de latência.

## Dependências

- compilador C11, CMake 3.20+
- SDL2
- FFmpeg: libavformat, libavcodec, libavutil e libswscale
- pthreads

Em Debian/Ubuntu:

```sh
sudo apt install build-essential cmake pkg-config libsdl2-dev \
  libavformat-dev libavcodec-dev libavutil-dev libswscale-dev
```

## Configuração e execução

```sh
cp config/cameras.example.json config/cameras.json
# opcional: assistente interativo para adicionar/editar uma câmera
./build/nvr-monitor --config config/cameras.json --configure
export NVR_CAMERA_ENTRADA_PASSWORD='sua senha'
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/nvr-monitor --config config/cameras.json
```

A senha não é salva no JSON nem aparece nos logs. `password_env` contém apenas o
nome da variável de ambiente usada em tempo de execução. Uma integração futura
com Secret Service/libsecret pode substituir esse mecanismo sem alterar o modelo
de câmera.

Cada câmera aceita `"transport": "udp"` ou `"tcp"`. O mosaico usa `grid_path`
(`/onvif2` no exemplo). Um duplo clique abre a câmera em tela única e reconecta em
`main_path` (`/onvif1`). `Esc` retorna à grade e `Q` encerra.

Na janela, **+ ADD CAMERA** abre o cadastro e **LAYOUT** permite escolher modo
automático ou 1, 2, 4, 6, 8 e 9 posições. Automaticamente, uma câmera ocupa toda
a área; duas ficam lado a lado; 3–4 usam 2x2; 5–6 usam 3x2; 7–8 usam 4x2; e nove
usam 3x3.

Em câmeras Yoosee, ative antes a opção NVR no aplicativo do celular e escolha UDP.
O `ffplay` pode ser usado somente para diagnosticar a URL; o monitor não cria
processos `ffplay`. Caracteres especiais da senha precisam estar percent-encoded
apenas no comando de teste. No formulário do NVR, informe a senha original.

## Escopo atual

- RTSP direto pelas bibliotecas FFmpeg, sem processos `ffplay`
- H.264/H.265 conforme os decodificadores disponíveis no FFmpeg
- conversão para BGRA e texturas SDL
- mosaicos 1x1, 2x2 e 3x3, preservando a proporção do vídeo
- reconexão automática com espera exponencial limitada a 30 segundos
- estado visual por borda: verde online, amarela conectando e vermelha offline

Áudio, gravação, movimento, aceleração por hardware e chaveiro do Linux ficam para
as próximas etapas.
