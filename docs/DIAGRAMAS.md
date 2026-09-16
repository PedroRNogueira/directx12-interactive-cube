# Diagramas Mermaid

Os diagramas distinguem comandos, dados de imagem e sincronização. As setas representam relações conceituais; não afirmam cópias obrigatórias em todos os drivers/hardwares.

## 1. Arquitetura completa

```mermaid
flowchart TB
    subgraph APP[Processo da aplicação]
        W[Win32 Window + Message Loop]
        U[Update: input, câmera, MVP]
        R[Renderer: recursos e comandos]
        W --> U --> R
    end
    subgraph CPU[CPU / memória do sistema]
        CB[Constant Buffer do frame]
        CL[Command List]
        Q[Command Queue]
        R --> CB
        R --> CL --> Q
    end
    subgraph STACK[Stack gráfico do Windows]
        API[D3D12 runtime + DXGI]
        DRV[Driver WDDM do adapter]
        SCH[GPU scheduler]
        Q --> API --> DRV --> SCH
    end
    subgraph GPU[GPU / subsistema gráfico]
        FE[Command processor]
        PIPE[Pipeline gráfico]
        RT[Render Target: back buffer]
        SC[Swap Chain flip-discard]
        SCH --> FE --> PIPE
        CB --> PIPE
        PIPE --> RT --> SC
    end
    subgraph DISPLAY[Apresentação]
        DWM[Windows / DWM]
        DE[Display engine + scanout]
        LINK[HDMI / DisplayPort]
        MON[Controlador e painel do monitor]
        SC -->|Present| DWM --> DE --> LINK --> MON
    end
```

## 2. Fluxo CPU/GPU sobreposto

```mermaid
sequenceDiagram
    participant CPU
    participant Queue as Command Queue
    participant GPU
    participant Display as Apresentação

    CPU->>CPU: Update frame N (input + MVP)
    CPU->>Queue: ExecuteCommandLists(N)
    CPU->>Display: Present(N)
    CPU->>Queue: Signal fence N
    Note over CPU,GPU: CPU pode preparar N+1
    par trabalho sobreposto
        CPU->>CPU: Update/grava frame N+1
        Queue->>GPU: comandos do frame N
        GPU->>GPU: VS → raster → PS → back buffer
    end
    GPU-->>Queue: alcança Signal N
    Display->>Display: compor/selecionar e apresentar
```

O bloco `Present` aparece vindo da CPU porque é uma chamada da aplicação, mas o processamento da apresentação continua de forma assíncrona no sistema.

## 3. Pipeline gráfico

```mermaid
flowchart LR
    VB[(Vertex Buffer<br/>posição + cor)] --> IA[Input Assembler]
    IB[(Index Buffer<br/>36 índices)] --> IA
    CB[(Constant Buffer<br/>MVP)] --> VS[Vertex Shader]
    IA --> VS
    VS --> PA[Primitive Assembly<br/>12 triângulos]
    PA --> CP[Clipping + perspectiva]
    CP --> RS[Rasterizer<br/>cobertura + interpolação]
    RS --> PS[Pixel Shader<br/>cor]
    PS --> OM[Output Merger<br/>depth test]
    DB[(Depth Buffer)] <--> OM
    OM --> RT[(Render Target<br/>back buffer)]
```

## 4. Swap chain tripla

```mermaid
stateDiagram-v2
    state "Buffer 0\nPRESENT / apresentação" as B0
    state "Buffer 1\nRENDER_TARGET / GPU" as B1
    state "Buffer 2\ndisponível ou em voo" as B2
    B0 --> B1: Present avança índice
    B1 --> B2: próximo frame
    B2 --> B0: rotação flip model
```

```mermaid
flowchart LR
    B0[Back buffer 0] --> SC[IDXGISwapChain3]
    B1[Back buffer 1] --> SC
    B2[Back buffer 2] --> SC
    SC -->|GetCurrentBackBufferIndex| CPU[Escolha do frame context]
    CPU -->|barrier| RT[Render target atual]
    RT -->|Present 1,0| OS[DXGI / Windows]
```

## 5. Mouse até o novo frame

```mermaid
flowchart TD
    M[Movimento físico do mouse] --> ID[Driver/stack de input]
    ID --> MSG[WM_MOUSEMOVE / WM_MOUSEWHEEL]
    MSG --> WP[WindowProcedure na CPU]
    WP --> CAM[Atualiza alvo de yaw, pitch ou distância]
    CAM --> SM[S suavização em Camera::Update]
    SM --> VIEW[Matriz View]
    MODEL[Matriz Model] --> MVP[M * V * P]
    VIEW --> MVP
    PROJ[Matriz Projection] --> MVP
    MVP --> CB[Constant Buffer do frame]
    CB --> VS[Vertex Shader]
    VS --> TRI[Triângulos em nova posição]
    TRI --> PIX[Raster + Pixel Shader]
    PIX --> BB[Back buffer]
    BB --> PRES[Present]
    PRES --> DWM[DWM / apresentação]
    DWM --> MON[Scanout → link → monitor]
```

## 6. Sincronização com Fence

```mermaid
sequenceDiagram
    participant CPU
    participant Alloc as Allocator/CB do buffer i
    participant Q as Command Queue
    participant GPU
    participant F as Fence

    CPU->>F: GetCompletedValue()
    alt valor concluído >= frame[i].fenceValue
        CPU->>Alloc: Reset + sobrescreve constantes
    else GPU ainda usa o contexto
        CPU->>F: SetEventOnCompletion(valor)
        CPU->>CPU: WaitForSingleObject
        GPU->>F: execução alcança valor
        F-->>CPU: evento sinalizado
        CPU->>Alloc: Reset + sobrescreve constantes
    end
    CPU->>Q: ExecuteCommandLists
    CPU->>Q: Signal(fence, próximo valor)
    Q->>GPU: executa em ordem
    GPU->>F: conclui Signal
```

## 7. Resize (diagrama extra)

```mermaid
flowchart LR
    WM[WM_SIZE] --> Z{largura/altura > 0?}
    Z -->|não| MIN[Pausar renderização]
    Z -->|sim| WAIT[WaitForGpu]
    WAIT --> REL[Soltar back buffers e depth]
    REL --> RB[ResizeBuffers]
    RB --> RTV[Recriar RTVs]
    RTV --> DSV[Recriar depth/DSV]
    DSV --> VP[Atualizar viewport, scissor e aspect]
```
