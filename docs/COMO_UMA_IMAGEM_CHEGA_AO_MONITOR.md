# Como uma imagem chega ao monitor

Este documento acompanha o caso concreto: o usuário arrasta o mouse e vê o cubo mudar de orientação. Ele separa o que o projeto realmente faz de simplificações didáticas. Implementações de driver, compositor e hardware são proprietárias e podem otimizar o caminho.

## 1. Antes do movimento

A aplicação já criou uma janela (`HWND`), um Device D3D12, uma Command Queue e uma swap chain flip-discard com três buffers. A GPU selecionada tem recursos para ler a malha, constantes e shaders. O loop principal alterna entre tratar mensagens, atualizar estado e submeter frames.

CPU e GPU não compartilham um único “ponteiro de execução”. A CPU executa instruções do programa. A GPU consome comandos submetidos à sua fila de acordo com driver, escalonador do Windows (WDDM) e hardware. Um frame da GPU pode estar atrasado em relação à lógica que a CPU já começou a preparar.

## 2. Quando o mouse se move

### 2.1 Do dispositivo ao Windows

O sensor do mouse mede deslocamento e envia relatórios pelo transporte do dispositivo (por exemplo USB ou Bluetooth). O stack de drivers de entrada do Windows processa esses relatórios. Para a interação clássica de janela usada aqui, o sistema produz mensagens como `WM_LBUTTONDOWN`, `WM_MOUSEMOVE`, `WM_LBUTTONUP` e `WM_MOUSEWHEEL` na fila associada à thread da janela.

Isso é uma descrição funcional. Taxas de polling, aceleração, Raw Input, composição do cursor e detalhes de HID não são modelados pelo projeto.

### 2.2 Message loop e WindowProcedure

`PeekMessage`, `TranslateMessage` e `DispatchMessage` rodam na CPU. `DispatchMessage` chama `Application::WindowProcedure`; esta encaminha para `HandleMessage`.

Com o botão esquerdo pressionado, o programa guarda a posição anterior e calcula `deltaX`/`deltaY`. Esses deltas alteram os alvos:

- `targetYaw += deltaX * sensibilidade`;
- `targetPitch += deltaY * sensibilidade`, limitado a ±1,35 radiano;
- a roda altera `targetDistance`, limitada entre 3 e 12.

Capturar o mouse com `SetCapture` mantém o arraste coerente mesmo se o cursor cruza brevemente a área cliente.

### 2.3 Suavização

No próximo `Update`, os valores visíveis aproximam-se dos alvos por interpolação exponencial dependente de `deltaSeconds`. Isso evita um movimento abrupto e mantém comportamento semelhante em taxas de frame diferentes.

Quando não há drag e auto-rotação está ligada, o alvo de yaw avança lentamente. Espaço alterna essa função; `R` redefine os alvos.

## 3. Da câmera a números para a GPU

A câmera orbital calcula sua posição cartesiana com seno e cosseno e usa `XMMatrixLookAtLH` para produzir a matriz **View**. A CPU também possui:

- **Model:** transformação do cubo no mundo; neste projeto é identidade;
- **Projection:** perspectiva left-handed com FOV, aspect ratio, near e far planes.

O produto é `Model * View * Projection` na convenção de vetores-linha do DirectXMath. A matriz é gravada em `SceneConstants`. HLSL lê a representação como column-major e usa `mul(modelViewProjection, position)`. Esse detalhe de layout é importante: uma transposição adicional aqui distorceria a perspectiva.

## 4. Por que há uma Constant Buffer por frame

Uma constant buffer é memória lida pelo shader. Se houvesse apenas uma e a CPU escrevesse a câmera do frame N+1 enquanto a GPU ainda usa os dados do frame N, o resultado seria uma corrida: a GPU poderia ler uma mistura de valores antigos e novos.

O projeto associa uma constant buffer persistemente mapeada a cada um dos três frame contexts. Antes de copiá-la, `WaitForFrame` verifica a fence do contexto. Só após a GPU concluir aquele valor a CPU pode reutilizar o allocator e sobrescrever as constantes.

A alocação é arredondada para 256 bytes. Isso não significa que a estrutura contenha 256 bytes úteis; é a granularidade de posicionamento de Constant Buffer Views exigida por D3D12.

## 5. A CPU grava uma descrição do frame

`Renderer::Render` não percorre pixels. Ele registra operações numa `ID3D12GraphicsCommandList`:

1. espera o frame context somente se necessário;
2. reseta `ID3D12CommandAllocator` e Command List;
3. declara a transição do back buffer de `PRESENT` para `RENDER_TARGET`;
4. limpa cor e profundidade;
5. define RTV/DSV, viewport e scissor;
6. define PSO e Root Signature;
7. vincula constant, vertex e index buffers;
8. chama `DrawIndexedInstanced(36, 1, ...)`;
9. declara a transição inversa;
10. fecha a lista.

### Command Allocator não é a lista

O allocator fornece armazenamento para a gravação. A lista representa a sequência lógica. Resetar o allocator cedo demais pode invalidar memória ainda referenciada pela GPU; por isso sua reutilização é protegida por fence.

### Resource Barrier não copia o buffer

A barrier informa ordem/estado de uso. Um back buffer usado para apresentação não pode simplesmente receber escrita de render target sem a transição explícita. O runtime/driver/hardware implementam as dependências e mudanças de layout/cache necessárias; não se deve ensinar que toda barrier é necessariamente uma cópia física.

## 6. Submissão, runtime e driver

Após `Close`, a CPU passa a lista para `ExecuteCommandLists` na Direct Command Queue. A chamada submete trabalho, não espera que todos os pixels estejam prontos.

Direct3D 12 é a API/contrato. O runtime valida e organiza aspectos da chamada; o driver do fabricante traduz/compila o necessário para os formatos de comando e gerenciamento do hardware. O scheduler gráfico do Windows participa do compartilhamento/preempção de GPU entre processos. A descrição “API → driver → GPU” é útil, mas não implica uma chamada simples e síncrona para cada estágio.

## 7. O que a GPU faz

### 7.1 Input Assembler

O Input Assembler lê os 24 registros `Vertex { position, color }` e os índices de 16 bits. Cada grupo de três índices forma um triângulo. O cubo usa 12 triângulos.

Os oito cantos matemáticos foram duplicados por face. Isso permite cores independentes e seria igualmente necessário para normais duras distintas em iluminação por face.

### 7.2 Vertex Shader

O Vertex Shader executa conceitualmente uma invocação por vértice referenciado. Ele multiplica posição pela MVP e produz coordenadas homogêneas de clip. A GPU explora paralelismo; a ordem mental “um vértice depois do outro” não descreve a microarquitetura real.

### 7.3 Montagem, clipping e rasterização

O estágio fixo reúne saídas em triângulos, elimina/corta regiões fora do volume visível, divide por W e mapeia para viewport. O rasterizador determina quais amostras de pixels são cobertas e interpola a cor nos triângulos.

“Fragmento” é um termo didático comum. Em Direct3D, costuma-se falar em pixel shader invocation/sample; multisampling e helper lanes tornam “um fragmento = exatamente um pixel físico” uma simplificação.

### 7.4 Pixel Shader e Output Merger

O Pixel Shader devolve `float4(color, 1)`. O depth test compara a profundidade com o depth buffer para que faces atrás não cubram faces à frente. O Output Merger grava a cor no render target, formato `R8G8B8A8_UNORM`.

O shader não escreve no monitor. Ele produz valores para um recurso de imagem em memória controlada pelo subsistema gráfico.

## 8. Render target, framebuffer e back buffer

**Render target** é um recurso que recebe resultados de cor. **Depth buffer** armazena profundidade, não cor apresentada. “Framebuffer” é um termo amplo para o conjunto lógico de attachments/armazenamento de um frame; Direct3D 12 usa termos mais específicos como recursos, RTV e DSV.

Um **back buffer** é uma das imagens de apresentação da swap chain. Neste projeto, o back buffer atual também é o render target. A RTV é um descritor: uma visão que diz como tratar o recurso para escrita, não outra cópia da imagem.

## 9. Triple buffering e frames in flight

A swap chain tem três back buffers. O índice retornado por `GetCurrentBackBufferIndex` escolhe qual recurso será renderizado. Em paralelo, outro buffer pode estar enfileirado para apresentação e outro pode estar disponível ou ainda ocupado.

“Três buffers” não garante que três frames estejam simultaneamente executando shaders. Significa que existem três slots de recursos/contextos capazes de sustentar sobreposição e absorver diferenças de ritmo, sujeitos a VSync, fila de apresentação, latência e trabalho da GPU.

Double buffering aplica a mesma ideia com dois buffers e menor margem. Triple buffering costuma reduzir bloqueios por falta de buffer, ao custo de memória e potencial de maior fila/latência se não controlada.

## 10. Fence: como sabemos que acabou

Depois da submissão/apresentação, a CPU escolhe um `Fence Value` monotônico e chama `CommandQueue::Signal(fence, value)`. O signal está ordenado na queue: quando a GPU alcança aquele ponto, o valor concluído avança.

Ao reencontrar o frame context, a CPU compara `GetCompletedValue()` com o valor guardado. Se estiver atrás, registra um evento com `SetEventOnCompletion` e espera. Se já concluiu, não bloqueia.

Esse mecanismo demonstra `CPU != GPU`: se fossem uma única sequência síncrona, não seria necessário observar conclusão assíncrona para reutilizar recursos.

## 11. O que Present faz — e o que não faz

`IDXGISwapChain::Present(1, 0)` entrega uma solicitação de apresentação ao DXGI. O `1` associa a apresentação ao próximo intervalo vertical permitido (VSync); não é uma espera universal que prova que o fóton já saiu do monitor quando a função retorna.

A swap chain usa flip-discard. Em vez de depender de uma cópia bitblt tradicional da imagem inteira, buffers são entregues ao sistema de apresentação em um modelo de flip eficiente. Identidades/índices precisam ser acompanhados explicitamente em D3D12.

## 12. Windows, DWM e composição

Em uma janela normal, o Desktop Window Manager normalmente compõe superfícies de várias aplicações com desktop, transparências e efeitos. O flip model permite que DWM componha a partir do buffer apresentado com menos cópias; otimizações como independent flip/direct flip podem alterar o caminho quando as condições permitem.

Portanto:

- correto: “a aplicação apresenta uma superfície ao sistema; Windows/DWM gerencia a apresentação da janela”;
- simplificação inadequada: “Present copia sempre o back buffer para a tela”;
- também inadequado: “a CPU manda os pixels ao monitor”.

## 13. Display engine e scanout

Depois que uma superfície final foi escolhida/composta, um bloco de exibição da GPU (display engine) lê a imagem em uma cadência determinada pelo modo do monitor. Esse processo é chamado **scanout**. Ele gera o fluxo de pixels e sinais auxiliares necessários à saída.

A GPU que executa shaders e o display engine fazem parte do subsistema gráfico, mas são funções distintas. Um frame pode estar sendo renderizado enquanto outro passa por scanout.

## 14. HDMI/DisplayPort e o monitor

O controlador de saída codifica o fluxo segundo HDMI ou DisplayPort e o transmite pelo link físico. O protocolo carrega dados de vídeo e temporização; compressão DSC, HDR, chroma subsampling, taxa variável e conversões de cor podem existir conforme a configuração.

O receptor do monitor decodifica o sinal. O scaler/timing controller prepara os valores para o painel. Pixels físicos não mudam todos de maneira matematicamente instantânea: LCD/OLED têm eletrônica, varredura e tempos de resposta próprios. A imagem percebida resulta dessa última etapa física.

O projeto não detecta qual cabo está conectado nem mede scanout. HDMI/DisplayPort são explicados como caminhos típicos, não como fato inferido pela aplicação.

## 15. Resize: por que é delicado

Back buffers pertencem à swap chain e têm dimensões fixas. Ao receber um novo tamanho:

1. não renderizar tamanho zero/minimizado;
2. esperar trabalho que usa os buffers antigos;
3. liberar referências a esses buffers e ao depth buffer;
4. chamar `ResizeBuffers`;
5. recuperar recursos novos e recriar RTVs/DSV;
6. atualizar viewport e scissor;
7. usar o novo aspect ratio na projeção.

Tentar redimensionar enquanto ainda há referências pode fazer `ResizeBuffers` falhar. Continuar com a projeção antiga deformaria o cubo.

## 16. Linha do tempo resumida

```text
CPU frame N:   input → câmera → constantes → grava lista → submit → Present → Signal(N)
GPU frame N-1:        lê buffers → VS → raster → PS → render target → conclui Signal(N-1)
Display frame N-2:                         composição/seleção → scanout → link → painel
```

Os números N−1/N−2 são ilustrações, não uma medição fixa de latência. O ponto é a sobreposição possível.

## 17. Simplificações assumidas explicitamente

- O diagrama desenha uma cadeia linear, mas driver, scheduler, GPU e apresentação trabalham por filas e podem se sobrepor.
- “GPU executa uma lista” omite front-end de comandos, caches, work distribution e detalhes do fabricante.
- “Pixel Shader calcula pixels” omite samples, quads, helper lanes, MSAA e otimizações.
- “DWM compõe” descreve o caso geral de janela; caminhos de flip podem contornar trabalho de composição em condições específicas.
- “HDMI/DisplayPort leva pixels” resume codificação, pacotes, treinamento de link, cor e temporização.
- A barra mostra FPS medido pela CPU, frame buffer index e fence. Não mostra utilização nem duração de GPU.

## Referências técnicas

- [Work submission in Direct3D 12](https://learn.microsoft.com/windows/win32/direct3d12/command-queues-and-command-lists)
- [Important changes from Direct3D 11 to Direct3D 12](https://learn.microsoft.com/windows/win32/direct3d12/important-changes-from-directx-11-to-directx-12)
- [DXGI flip-model presentation](https://learn.microsoft.com/windows/win32/direct3ddxgi/dxgi-1-2-presentation-improvements)
- [`IDXGISwapChain::Present`](https://learn.microsoft.com/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-present)
