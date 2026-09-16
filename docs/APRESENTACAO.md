# Roteiro de apresentação — até 10 minutos

## Antes de começar

Deixe a aplicação aberta com o cubo, painel visível e rotação automática ligada. Tenha `Mesh.cpp`, `BezierSurface.cpp`, `Application::Update` e `Renderer::Render` marcados. A mensagem central é: **a CPU descreve trabalho e coordena; a GPU executa o pipeline e produz uma imagem; o sistema de apresentação leva essa imagem ao monitor**. Tessellation e iluminação são demonstrações curtas desse caminho, não um novo tema.

## 0:00–1:00 — A pergunta

> “Quando mexo o mouse, como essa ação termina como pixels diferentes no painel?”

- Um cubo bonito não responde a isso sozinho.
- Vamos seguir dados e comandos desde Win32 até o subsistema de exibição.
- A CPU não desenha o cubo diretamente no monitor e `Present` não envia pixels pelo HDMI.

Mostre rapidamente o cubo, mas deixe a explicação como protagonista.

## 1:00–2:00 — DirectX, Direct3D e Device

- DirectX é uma família; Direct3D é sua API gráfica 3D.
- DXGI enumera adapters e cria a swap chain.
- O adapter corresponde à implementação gráfica/GPU; o `ID3D12Device` é a interface lógica para criar recursos e pipeline.
- A aplicação chama a API, o runtime e o driver traduzem/validam o necessário, e o hardware recebe trabalho compatível.

Mostre na barra de título o nome real do adapter.

## 2:00–4:00 — Trabalho da CPU

Abra `Mesh.cpp`:

- o cubo não chega à GPU como “cubo”;
- são 24 vértices com posição/cor/normal e 36 índices;
- 8 cantos conceituais viram 24 vértices para atributos independentes por face;
- os índices formam 12 triângulos.

Abra `Application::Update`:

- Win32 entrega mensagens de mouse à CPU;
- a câmera altera yaw, pitch e distância;
- a CPU calcula `Model * View * Projection`;
- os dados vão à Constant Buffer do frame.

Abra o começo de `Renderer::Render`:

- espera apenas se o frame context ainda estiver em uso;
- reseta allocator/list;
- grava comandos, não pixels.

## 4:00–6:00 — GPU e pipeline

Mostre a sequência em `Renderer::Render`:

1. barrier `PRESENT → RENDER_TARGET`;
2. clear de cor e profundidade;
3. PSO, Root Signature, viewport e buffers;
4. `DrawIndexedInstanced(36, ...)` no cubo ou `DrawInstanced(16, ...)` no patch;
5. barrier `RENDER_TARGET → PRESENT`;
6. `Close` e `ExecuteCommandLists`.

Explique o pipeline:

- Input Assembler lê vertex/index buffers;
- Vertex Shader aplica MVP;
- montagem forma triângulos;
- rasterizer descobre amostras cobertas e interpola cores;
- Pixel Shader produz cor;
- Output Merger usa depth e grava o render target.

Em cerca de 40 segundos, selecione **Superfície Bézier** e wireframe:

- a CPU enviou somente 16 control points;
- Hull Shader fornece o fator;
- tessellator fixed-function cria domínios;
- Domain Shader avalia Bézier e normal;
- mostre fator 1 e depois 8 ou 16: a GPU gera a densidade visível.

Volte ao sólido. Desligue/ligue iluminação e specular: o Pixel Shader combina ambient, diffuse e specular. Isso é realismo básico, não fotorrealismo.

## 6:00–7:30 — Swap chain, Present e sincronização

- O render target atual é um dos três back buffers.
- Triple buffering permite sobreposição: GPU pode trabalhar em um contexto enquanto CPU prepara outro.
- `Present(1,0)` solicita apresentação com VSync; o flip model integra o buffer ao caminho do Windows/DWM.
- O DWM normalmente compõe a janela no desktop; depois o display engine faz scanout.
- O sinal passa por HDMI/DisplayPort e o controlador do monitor atualiza o painel.

Sobre a fence:

- cada submissão recebe um valor crescente (aponte o valor no título);
- a queue sinaliza quando alcança esse ponto;
- antes de reutilizar allocator/constant buffer, a CPU verifica a conclusão;
- sem isso, CPU poderia sobrescrever memória ainda lida pela GPU.

## 7:30–9:00 — Demonstração “controles até pixels”

1. Segure e arraste horizontalmente: mensagem `WM_MOUSEMOVE` → yaw → View → MVP → constant buffer → Vertex Shader → novo frame.
2. Arraste verticalmente: pitch limitado evita inversões extremas.
3. Use a roda: distância limitada entre 3 e 12.
4. Pressione `R`: câmera volta à posição inicial.
5. Pressione Espaço: auto-rotação pausa; CPU e GPU continuam produzindo frames, mas com a câmera parada.
6. Use o painel para Cubo/Superfície, wireframe e luz. O ImGui altera dados/PSO; não substitui o pipeline.
7. Redimensione: espera GPU, `ResizeBuffers`, recria RTV/DSV e corrige aspect ratio.

Ressalte que o FPS é CPU/frames apresentados, e que não existe métrica falsa de tempo de GPU.

## 9:00–10:00 — Síntese

Recite o fluxo:

> “Aplicação na CPU processa input, calcula matrizes e grava uma Command List. A Queue submete. Driver/runtime conectam a API ao hardware. A GPU transforma vértices ou tessella o patch, rasteriza triângulos, calcula iluminação no Pixel Shader e escreve um back buffer. A swap chain apresenta pelo Windows; display engine e link de vídeo levam o frame ao monitor. Fences impedem que CPU e GPU colidam ao reutilizar recursos.”

Feche com três ideias:

1. CPU e GPU são processadores diferentes e assíncronos.
2. A GPU recebe números, recursos, estados e comandos — não objetos abstratos.
3. A imagem existe em memória antes de ser apresentada; renderizar e exibir são etapas relacionadas, mas distintas.

## Perguntas prováveis

**Por que 24 vértices se um cubo tem 8 cantos?** Para cada face poder ter atributos próprios; um mesmo canto geométrico precisa de cópias quando cor/normal divergem.

**Tessellation 32 significa 32 triângulos?** Não. É um fator usado nas bordas/interior do domínio quad; a quantidade gerada depende das regras do tessellator. O wireframe é a demonstração mais segura.

**Blinn-Phong é fotorrealista?** Não. É um modelo local didático de ambient+diffuse+specular; não inclui sombras, PBR, reflexos globais ou ray tracing.

**Fence é VSync?** Não. Fence sincroniza progresso de trabalho CPU/GPU. VSync relaciona apresentação ao ciclo vertical do display.

**Render target é o monitor?** Não. É uma imagem/recurso em memória. O monitor só recebe o resultado mais tarde pelo caminho de apresentação.

**Triple buffering garante três frames renderizando ao mesmo tempo?** Não. Permite até três contextos/back buffers em rotação; o grau real de sobreposição depende de filas, latência e carga.

**O DWM sempre copia a imagem?** Não. Flip model e otimizações modernas podem evitar cópias. “Compor” é a explicação geral, não uma promessa de uma cópia específica.
