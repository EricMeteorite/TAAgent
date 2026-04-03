# Niagara Data Channels 深度解析

Data Channels 是 Niagara 的跨系统通信机制，允许不同的 Niagara 系统之间、以及 Blueprint/C++ 与 Niagara 之间进行数据交换。

---

## 核心架构

### 类层次结构

```
UNiagaraDataChannel (Asset)
├── UNiagaraDataChannel_Global        // 全局数据通道
├── UNiagaraDataChannel_Islands       // 空间分岛
├── UNiagaraDataChannel_MapBase       // 基于 Map 键的分桶
│   └── UNiagaraDataChannel_GameplayBurst  // 游戏玩法爆发效果
└── (可自定义子类)

UNiagaraDataChannelHandler (运行时处理器)
├── UNiagaraDataChannelHandler_Global
├── UNiagaraDataChannelHandler_Islands
├── UNiagaraDataChannelHandler_MapBase
│   └── UNiagaraDataChannelHandler_GameplayBurst
└── (与 Channel 类型匹配)

FNiagaraDataChannelData (实际数据存储)
├── GameData (Blueprint/C++ 读写)
├── CPUSimData (Niagara CPU 模拟)
└── GPUData (Niagara GPU 模拟)
```

### 数据流向

```
┌─────────────────┐     ┌──────────────────┐     ┌─────────────────┐
│ Blueprint/C++   │────▶│   Game Data      │────▶│ Subscribers     │
│ Writer          │     │   (CPU Buffer)   │     │ (Delegate)      │
└─────────────────┘     └──────────────────┘     └─────────────────┘
                               │
        ┌──────────────────────┼──────────────────────┐
        ▼                      ▼                      ▼
┌───────────────┐     ┌───────────────┐     ┌───────────────┐
│ CPU Sim Data  │     │ CPU Sim Data  │     │  GPU Buffer   │
│ (Niagara CPU) │     │ (Previous)    │     │ (Niagara GPU) │
└───────────────┘     └───────────────┘     └───────────────┘
```

---

## 核心组件

### 1. UNiagaraDataChannel (资产)

定义数据通道的变量布局和行为模式。

```cpp
// 头文件: NiagaraDataChannel.h
UCLASS(Abstract, BlueprintType)
class NIAGARA_API UNiagaraDataChannel : public UObject
{
    // 通道变量定义
    UPROPERTY(EditDefaultsOnly, Category = "Variables")
    TArray<FNiagaraDataChannelVariable> ChannelVariables;
    
    // 版本控制
    UPROPERTY()
    FGuid VersionGuid;
    
    // 布局信息 (编译后)
    mutable FNiagaraDataChannelLayoutInfoPtr LayoutInfo;
    
    // 访问上下文类型
    UPROPERTY(EditDefaultsOnly, Category = "Settings")
    TNDCAccessContextType AccessContextType;
    
    // 创建运行时处理器
    virtual UNiagaraDataChannelHandler* CreateHandler(UWorld* OwningWorld) const;
    
    // 获取布局信息
    const FNiagaraDataChannelLayoutInfoPtr GetLayoutInfo() const;
    
    // 验证通道有效性
    bool IsValid() const;
};
```

### 2. FNiagaraDataChannelVariable

定义通道中的单个变量：

```cpp
USTRUCT()
struct FNiagaraDataChannelVariable
{
    UPROPERTY(EditAnywhere, Category = "Variable")
    FName Name;                    // 变量名
    
    UPROPERTY()
    FNiagaraTypeDefinition Type;   // 类型定义
    
    UPROPERTY()
    FGuid Version;                 // 版本 GUID (用于序列化兼容)
    
    // 数据通道特有的类型映射
    enum EType : uint8
    {
        Float, Vector2D, Vector, Vector4,
        Quat, Color, Bool, Int,
        ID, SpawnInfo, Position
    };
};
```

### 3. FNiagaraDataChannelLayoutInfo

编译后的布局信息：

```cpp
struct FNiagaraDataChannelLayoutInfo
{
    // CPU 数据布局
    FNiagaraDataSetCompiledData DataSetCompiledData;
    
    // GPU 数据布局 (可能有差异)
    FNiagaraDataSetCompiledData DataSetCompiledDataGPU;
    
    // Game Data 布局
    FNiagaraDataChannelGameDataLayout GameDataLayout;
    
    // 变量索引映射
    TMap<FNiagaraVariableBase, int32> VariableIndices;
};
```

---

## 数据存储

### FNiagaraDataChannelData

运行时数据存储的核心类：

```cpp
class FNiagaraDataChannelData
{
    // 布局引用
    FNiagaraDataChannelLayoutInfoPtr LayoutInfo;
    
    // Blueprint/C++ 数据
    FNiagaraDataChannelGameDataPtr GameData;
    
    // Niagara CPU 模拟数据
    FNiagaraDataSet* CPUSimData;
    
    // 上一帧数据 (可选)
    FNiagaraDataBufferRef PrevCPUSimData;
    
    // Game Data 暂存 (用于 GPU 上传)
    FNiagaraDataSet* GameDataStaging;
    
    // GPU 渲染线程代理
    FNiagaraDataChannelDataRTProxy* RTProxy;
    
    // 发布请求队列
    TArray<FNiagaraDataChannelPublishRequest> PublishRequests;
    TArray<FNiagaraDataChannelPublishRequest> PublishRequestsFromGPU;
    TArray<FNiagaraDataChannelPublishRequest> PublishRequestsForGPU;
    
    // 临界区保护
    FCriticalSection PublishCritSec;
};
```

### 数据可见性控制

```cpp
struct FNiagaraDataChannelPublishRequest
{
    // 数据源
    FNiagaraDataChannelGameDataPtr GameData;  // Blueprint 数据
    FNiagaraDataBufferRef Data;                // Niagara 数据
    
    // 可见性标志
    bool bVisibleToGame = true;     // Blueprint 可读
    bool bVisibleToCPUSims = true;  // CPU Niagara 可读
    bool bVisibleToGPUSims = true;  // GPU Niagara 可读
    
    // 调试信息
    FString DebugSource;
    FLwcTile LwcTile;  // 大世界坐标分块
};
```

---

## Channel 类型

### 1. Global Channel

最简单的全局数据通道，所有访问者共享同一份数据。

```cpp
// NiagaraDataChannel_Global.h
UCLASS()
class UNiagaraDataChannel_Global : public UNiagaraDataChannel
{
    virtual UNiagaraDataChannelHandler* CreateHandler(UWorld* OwningWorld) const override
    {
        return NewObject<UNiagaraDataChannelHandler_Global>(OwningWorld);
    }
};

// Handler 实现
class UNiagaraDataChannelHandler_Global : public UNiagaraDataChannelHandler
{
    FNiagaraDataChannelDataPtr Data;  // 单一数据存储
    
    virtual FNiagaraDataChannelDataPtr FindData(
        FNiagaraDataChannelSearchParameters SearchParams,
        ENiagaraResourceAccess AccessType) override
    {
        if (!Data.IsValid())
        {
            Data = CreateData();
        }
        return Data;  // 所有访问者返回同一份数据
    }
};
```

**使用场景**：
- 全局事件广播
- 跨关卡数据共享
- 简单的 Blueprint → Niagara 通信

### 2. Islands Channel

空间分岛数据通道，根据位置将数据分割到不同的 "岛屿" 中。

```cpp
UCLASS()
class UNiagaraDataChannel_Islands : public UNiagaraDataChannel
{
    // 岛屿模式
    UPROPERTY(EditDefaultsOnly, Category = "Settings")
    ENiagraDataChannel_IslandMode Mode;
    // - AlignedStatic: 对齐的静态岛屿
    // - Dynamic: 动态扩展岛屿
    
    // 岛屿尺寸设置
    UPROPERTY(EditDefaultsOnly, Category = "Settings")
    FVector InitialExtents;  // 初始范围
    
    UPROPERTY(EditDefaultsOnly, Category = "Settings")
    FVector MaxExtents;      // 最大范围
    
    UPROPERTY(EditDefaultsOnly, Category = "Settings")
    FVector PerElementExtents;  // 每元素扩展
    
    // 处理系统
    UPROPERTY(EditDefaultsOnly, Category = "Settings")
    TArray<TSoftObjectPtr<UNiagaraSystem>> Systems;
    
    // 岛屿池大小
    UPROPERTY(EditDefaultsOnly, Category = "Settings")
    int32 IslandPoolSize = 16;
};

// 岛屿数据结构
struct FNDCIsland
{
    FBoxCenterAndExtent Bounds;      // 岛屿边界
    FNiagaraDataChannelDataPtr Data; // 岛屿数据
    TArray<UNiagaraComponent*> SpawnedComponents;  // 生成的系统
    
    // 岛屿生命周期
    bool IsBeingUsed() const;
    bool HasData() const;
    bool Contains(FVector Location) const;
    bool TryGrow(FVector Location, FVector PerElementExtents, FVector MaxExtents);
};
```

**岛屿查找逻辑**：

```cpp
FNDCIsland* UNiagaraDataChannelHandler_Islands::FindOrCreateIsland(
    const FNiagaraDataChannelSearchParameters& SearchParams,
    ENiagaraResourceAccess AccessType)
{
    FVector Location = SearchParams.GetLocation();
    
    // 1. 检查是否是处理系统
    if (SearchParams.GetOwner())
    {
        for (int32 i : ActiveIslands)
        {
            if (IslandPool[i].IsHandlerSystem(SearchParams.GetOwner()))
            {
                return &IslandPool[i];
            }
        }
    }
    
    // 2. 根据模式查找岛屿
    if (Channel->GetMode() == ENiagraDataChannel_IslandMode::AlignedStatic)
    {
        // 静态模式：查找包含位置的岛屿
        for (int32 i : ActiveIslands)
        {
            if (IslandPool[i].Contains(Location))
            {
                return &IslandPool[i];
            }
        }
    }
    else // Dynamic
    {
        // 动态模式：查找可扩展的岛屿
        for (int32 i : ActiveIslands)
        {
            if (IslandPool[i].TryGrow(Location, PerElementExtents, MaxExtents))
            {
                return &IslandPool[i];
            }
        }
    }
    
    // 3. 创建新岛屿
    int32 NewIndex = ActivateNewIsland(Location);
    IslandPool[NewIndex].OnAcquired(Location);
    return &IslandPool[NewIndex];
}
```

**使用场景**：
- 大规模空间数据
- LOD 分级处理
- 局部特效系统

### 3. Map Channel

基于键值的数据通道，使用 Map 键对数据进行分桶。

```cpp
UCLASS(Abstract)
class UNiagaraDataChannel_MapBase : public UNiagaraDataChannel
{
    // 默认处理系统
    UPROPERTY(EditDefaultsOnly, Category = "Settings")
    TSoftObjectPtr<UNiagaraSystem> DefaultSystemToSpawn;
};

// Map 键
struct FNDCMapKey
{
    TArray<uint8> Data;  // 序列化的键数据
};

// Map 条目
struct FNDCMapEntryBase
{
    UNiagaraDataChannelHandler_MapBase* Owner;
    FNiagaraDataChannelDataPtr Data;
    TArray<TObjectPtr<UNiagaraComponent>> SpawnedComponents;
    float LastUsedTime;
    
    virtual void Init(FNDCAccessContextInst& AccessContext, 
                      UNiagaraDataChannelHandler_MapBase* InOwner,
                      const FNDCMapKey& Key);
    virtual bool BeginFrame(float DeltaTime, FNiagaraWorldManager* OwningWorld, const FNDCMapKey& Key);
    virtual void Tick(float DeltaTime, ETickingGroup TickGroup, FNiagaraWorldManager* OwningWorld, const FNDCMapKey& Key);
};
```

### 4. GameplayBurst Channel

专为游戏玩法爆发效果设计的通道：

```cpp
UCLASS()
class UNiagaraDataChannel_GameplayBurst : public UNiagaraDataChannel_MapBase
{
    // 网格单元大小
    UPROPERTY(EditDefaultsOnly, Category = "Settings")
    FVector GridCellSize = FVector(1000.0f);
    
    // 系统边界填充
    UPROPERTY(EditDefaultsOnly, Category = "Settings")
    FVector SystemBoundsPadding = FVector(100.0f);
    
    // 附件设置
    UPROPERTY(EditDefaultsOnly, Category = "Settings")
    FNDCGameplayBurstAttachmentSettings AttachmentSettings;
};

// 访问上下文
USTRUCT(BlueprintType)
struct FNDCAccessContext_GameplayBurst : public FNDCAccessContext_MapBase
{
    UPROPERTY(BlueprintReadWrite)
    FVector Location;                         // 世界位置
    
    UPROPERTY(BlueprintReadWrite)
    FGameplayTag GameplayTag;                 // 游戏标签
    
    UPROPERTY(BlueprintReadWrite)
    bool bForceAttachToOwningComponent;       // 强制附加
    
    UPROPERTY(BlueprintReadWrite)
    bool bOverrideCellSize;                   // 覆盖单元大小
    UPROPERTY(BlueprintReadWrite)
    FVector CellSizeOverride;
    
    UPROPERTY(BlueprintReadWrite)
    bool bOverrideBoundsPadding;
    UPROPERTY(BlueprintReadWrite)
    FVector SystemBoundsPadding;
    
    // 输出
    UPROPERTY(BlueprintReadOnly)
    bool bAttachedToOwningComponent;          // 是否附加到组件
};

// 键生成逻辑
void UNiagaraDataChannelHandler_GameplayBurst::GenerateKey(
    FNDCAccessContextInst& AccessContext, 
    FNDCMapKeyWriter& KeyWriter) const
{
    auto& TypedContext = AccessContext.GetChecked<FNDCAccessContext_GameplayBurst>();
    
    // 判断是否使用附加路径
    bool bUseAttachedPath = AttachmentSettings.UseAttachedPathForComponent(Comp);
    TypedContext.bAttachedToOwningComponent = bUseAttachedPath;
    
    if (bUseAttachedPath)
    {
        // 附加路径：使用组件 ID 作为键
        KeyWriter << (uint8)1;  // Flags
        KeyWriter << Comp->GetUniqueID();
    }
    else
    {
        // 世界网格路径：使用网格坐标作为键
        FVector CellCoord = GetCellCoords(Location, CellSize.Reciprocal());
        KeyWriter << (uint8)0;  // Flags
        KeyWriter << CellCoord.X << CellCoord.Y << CellCoord.Z;
        KeyWriter << GameplayTag.GetTagName();
    }
}
```

**附件设置**：

```cpp
USTRUCT(BlueprintType)
struct FNDCGameplayBurstAttachmentSettings
{
    // 指定要附加的组件类型
    UPROPERTY(EditDefaultsOnly)
    TArray<TSubclassOf<USceneComponent>> ComponentTypes;
    
    // 触发附加的游戏标签
    UPROPERTY(EditDefaultsOnly)
    FGameplayTagContainer GameplayTags;
    
    // 速度阈值 (自动附加移动中的物体)
    UPROPERTY(EditDefaultsOnly)
    float SpeedThreshold = -1.0f;  // -1 表示禁用
    
    bool UseAttachedPathForComponent(const USceneComponent* Component) const;
};
```

---

## 访问机制

### Access Context (访问上下文)

UE 5.7+ 引入的访问上下文系统，替代旧的 SearchParameters：

```cpp
// 基类
USTRUCT(BlueprintType, Abstract)
struct FNDCAccessContextBase
{
    UPROPERTY(BlueprintReadWrite)
    TObjectPtr<UObject> Owner;  // 通常是 NiagaraComponent
    
    UPROPERTY(BlueprintReadWrite)
    bool bOverrideLocation = false;
    UPROPERTY(BlueprintReadWrite)
    FVector Location = FVector::ZeroVector;
};

// 实例包装
USTRUCT(BlueprintType)
struct FNDCAccessContextInst
{
    TNDCAccessContextType AccessContext;  // 类型化内存
    
    UScriptStruct* GetScriptStruct() const;
    
    template<typename T>
    const T* Get() const { return AccessContext.Get<T>(); }
};
```

### Blueprint API

```cpp
// 写入
UFUNCTION(BlueprintCallable, Category = "Niagara|DataChannel")
static UNiagaraDataChannelWriter* WriteToNiagaraDataChannel(
    const UObject* WorldContextObject,
    const UNiagaraDataChannelAsset* Channel,
    FNDCAccessContextInst& AccessContext,  // UE 5.7+
    int32 Count,
    bool bVisibleToBlueprint = true,
    bool bVisibleToNiagaraCPU = true,
    bool bVisibleToNiagaraGPU = true
);

// 读取
UFUNCTION(BlueprintCallable, Category = "Niagara|DataChannel")
static UNiagaraDataChannelReader* ReadFromNiagaraDataChannel(
    const UObject* WorldContextObject,
    const UNiagaraDataChannelAsset* Channel,
    FNDCAccessContextInst& AccessContext,
    bool bReadPreviousFrame = false
);

// 订阅
UFUNCTION(BlueprintCallable, Category = "Niagara|DataChannel")
static void SubscribeToNiagaraDataChannel(
    const UObject* WorldContextObject,
    const UNiagaraDataChannelAsset* Channel,
    FNDCAccessContextInst& AccessContext,
    const FOnNewNiagaraDataChannelPublish& UpdateDelegate,
    int32& UnsubscribeToken
);
```

### C++ Writer/Reader

```cpp
// Writer
class UNiagaraDataChannelWriter : public UObject
{
    FNiagaraDataChannelGameDataPtr Data;
    int32 StartIndex;
    
    // 写入方法
    void WriteFloat(FName VarName, int32 Index, double Value);
    void WriteVector(FName VarName, int32 Index, FVector Value);
    void WriteQuat(FName VarName, int32 Index, FQuat Value);
    void WriteLinearColor(FName VarName, int32 Index, FLinearColor Value);
    void WriteInt(FName VarName, int32 Index, int32 Value);
    void WriteBool(FName VarName, int32 Index, bool Value);
    void WriteSpawnInfo(FName VarName, int32 Index, FNiagaraSpawnInfo Value);
    void WritePosition(FName VarName, int32 Index, FVector Value);
    void WriteID(FName VarName, int32 Index, FNiagaraID Value);
};

// Reader
class UNiagaraDataChannelReader : public UObject
{
    FNiagaraDataChannelDataPtr Data;
    bool bReadingPreviousFrame;
    
    // 读取方法
    double ReadFloat(FName VarName, int32 Index, bool& IsValid);
    FVector ReadVector(FName VarName, int32 Index, bool& IsValid);
    FQuat ReadQuat(FName VarName, int32 Index, bool& IsValid);
    FLinearColor ReadLinearColor(FName VarName, int32 Index, bool& IsValid);
    int32 ReadInt(FName VarName, int32 Index, bool& IsValid);
    bool ReadBool(FName VarName, int32 Index, bool& IsValid);
    FNiagaraSpawnInfo ReadSpawnInfo(FName VarName, int32 Index, bool& IsValid);
    FVector ReadPosition(FName VarName, int32 Index, bool& IsValid);
    FNiagaraID ReadID(FName VarName, int32 Index, bool& IsValid);
    
    int32 Num() const;
};
```

### 高效 C++ Accessor

用于性能关键代码的模板化访问器：

```cpp
// 基类
class FNDCAccessorBase
{
    TArray<FNDCVarAccessorBase*> VariableAccessors;
    
    void Init(const UNiagaraDataChannel* DataChannel);
};

// 变量访问器
template<typename T>
class TNDCAccessor : public FNDCVarAccessorBase
{
    int32 VarOffset;  // 变量偏移
    
public:
    void Init(const UNiagaraDataChannel* DataChannel)
    {
        VarOffset = LayoutInfo->GetGameDataLayout().GetVariableIndex(Variable);
    }
    
    T Read(const FNiagaraDataChannelGameData* GameData, int32 Index) const
    {
        return GameData->GetVariableBuffer(VarOffset)->Read<T>(Index);
    }
    
    void Write(FNiagaraDataChannelGameData* GameData, int32 Index, const T& Value) const
    {
        GameData->GetVariableBuffer(VarOffset)->Write<T>(Index, Value);
    }
};

// Writer 示例
class FNDCWriter_MyChannel : public FNDCWriterBase
{
public:
    TNDCAccessor<FVector> Position{GET_NDC_MEMBER(Position), this, true};
    TNDCAccessor<FLinearColor> Color{GET_NDC_MEMBER(Color), this, false};
    TNDCAccessor<float> Scale{GET_NDC_MEMBER(Scale), this, false};
    
    void WriteData(FVector InPosition, FLinearColor InColor, float InScale)
    {
        Position.Write(Data, StartIndex, InPosition);
        Color.Write(Data, StartIndex, InColor);
        Scale.Write(Data, StartIndex, InScale);
    }
};
```

---

## Tick Group 与数据顺序

### Tick Group 控制

Data Channel 通过 Tick Group 控制读写顺序：

```cpp
UCLASS()
class UNiagaraDataChannel : public UObject
{
    // 是否强制执行 Tick Group 顺序
    UPROPERTY(EditDefaultsOnly, Category = "Settings")
    bool bEnforceTickGroupReadWriteOrder = true;
    
    // 最终写入 Tick Group
    UPROPERTY(EditDefaultsOnly, Category = "Settings")
    ETickingGroup FinalWriteTickGroup = TG_PrePhysics;
    
    // 是否保留上一帧数据
    UPROPERTY(EditDefaultsOnly, Category = "Settings")
    bool bKeepPreviousFrameData = false;
};
```

### 帧生命周期

```
Frame N
├── TG_PrePhysics
│   ├── BeginFrame()
│   │   └── Reset GPU PublishRequests
│   ├── ConsumePublishRequests(TG_PrePhysics)
│   │   └── 处理 PrePhysics 阶段的写入
│   └── Niagara TG_PrePhysics 系统可以读取
│
├── TG_StartPhysics
│   └── ConsumePublishRequests(TG_StartPhysics)
│
├── TG_DuringPhysics
│   └── ...
│
├── TG_EndPhysics
│   └── ...
│
├── TG_PostPhysics
│   └── ...
│
├── TG_PostUpdateWork
│   └── ...
│
└── TG_LastDemotable
    ├── ConsumePublishRequests(TG_LastDemotable)  // 最后一次处理
    └── EndFrame()
        └── 上传数据到 GPU
```

---

## GPU 数据传输

### CPU → GPU 数据流

```cpp
void FNiagaraDataChannelData::EndFrame(UNiagaraDataChannelHandler* Owner)
{
    // 收集需要发送到 GPU 的数据
    TArray<FNiagaraDataBufferRef> BuffersForGPU;
    
    if (NumGPUSpawningReaders > 0 && AutoUploadGPUSpawnData)
    {
        // 如果 CPU 数据被 GPU 系统用于生成粒子
        // 则自动上传所有 CPU 数据
        BuffersForGPU.Emplace(CPUSimData->GetCurrentData());
    }
    
    // 添加显式标记为 GPU 可见的数据
    for (auto& PublishRequest : PublishRequestsForGPU)
    {
        if (PublishRequest.Data->GetNumInstances() > 0)
        {
            BuffersForGPU.Emplace(PublishRequest.Data);
        }
    }
    
    // 发送到渲染线程
    if (BuffersForGPU.Num() > 0)
    {
        ENQUEUE_RENDER_COMMAND(FDataChannelProxyEndFrame) (
            [RT_Proxy, BuffersForGPU](FRHICommandListImmediate& CmdList)
            {
                RT_Proxy->AddBuffersFromCPU(BuffersForGPU);
            });
    }
}
```

### GPU → CPU 读回

```cpp
// GPU 发布到 CPU
void FNiagaraDataChannelData::PublishFromGPU(const FNiagaraDataChannelPublishRequest& Request)
{
    check(IsInGameThread());
    PublishRequestsFromGPU.Add(Request);
}

// 在下一帧 BeginFrame 处理
void FNiagaraDataChannelData::BeginFrame(UNiagaraDataChannelHandler* Owner)
{
    // 拉取 GPU 发布请求
    PublishRequests.Append(PublishRequestsFromGPU);
    PublishRequestsFromGPU.Reset();
}
```

---

## 调试与性能

### Console Variables

```ini
; 启用/禁用 Data Channels
fx.Niagara.DataChannels.Enabled 1

; 延迟初始化 Handler
fx.Niagara.DataChannels.AllowLazyHandlerInit 1

; 晚写入警告
fx.Niagara.DataChannels.WarnOnLateWrites 1

; 自动上传 GPU 生成数据
fx.Niagara.DataChannels.AutoUploadGPUSpawnData 1

; 累积写入优化
fx.Niagara.DataChannels.EnableAccumulatedWrites 1

; Map 条目池设置
fx.Niagara.DataChannels.NDCMapBase.InitialSize 0
fx.Niagara.DataChannels.NDCMapBase.FreeUnusedTime 60.0

; 异步加载
fx.Niagara.DataChannels.AllowAsyncLoad 1
fx.Niagara.DataChannels.BlockAsyncLoadOnUse 1
```

### Stats

```cpp
DECLARE_CYCLE_STAT(TEXT("FNiagaraDataChannelManager::BeginFrame"), STAT_DataChannelManager_BeginFrame, STATGROUP_NiagaraDataChannels);
DECLARE_CYCLE_STAT(TEXT("FNiagaraDataChannelManager::EndFrame"), STAT_DataChannelManager_EndFrame, STATGROUP_NiagaraDataChannels);
DECLARE_CYCLE_STAT(TEXT("FNiagaraDataChannelManager::Tick"), STAT_DataChannelManager_Tick, STATGROUP_NiagaraDataChannels);
DECLARE_CYCLE_STAT(TEXT("UNiagaraDataChannelHandler_Global::Tick"), STAT_DataChannelHandler_Global_Tick, STATGROUP_NiagaraDataChannels);
```

### 调试绘制

```cpp
// Islands 调试绘制
struct FNDCIslandDebugDrawSettings
{
    bool ShowBounds = false;
    FLinearColor BoundsColor = FLinearColor::Green;
    float BoundsThickness = 2.0f;
};

if (Channel->GetDebugDrawSettings().ShowBounds)
{
    Island.DebugDrawBounds();
}
```

---

## 最佳实践

### 1. 选择合适的 Channel 类型

| 类型 | 适用场景 |
|------|----------|
| Global | 全局事件、简单通信 |
| Islands | 大世界空间数据、LOD 系统 |
| GameplayBurst | 爆发效果、击中反馈、伤害数字 |
| 自定义 Map | 需要自定义分桶逻辑的场景 |

### 2. 控制数据可见性

```cpp
// 只在 Blueprint 中使用
WriteToNiagaraDataChannel(Context, Channel, Count, 
    true,   // bVisibleToGame
    false,  // bVisibleToCPU
    false   // bVisibleToGPU
);

// 只在 GPU Niagara 中使用
WriteToNiagaraDataChannel(Context, Channel, Count,
    false,  // bVisibleToGame
    false,  // bVisibleToCPU
    true    // bVisibleToGPU
);
```

### 3. 使用高效的 Accessor

```cpp
// 避免每次查找变量名
// ❌ 慢
Writer->WriteVector(FName("Position"), Index, Position);

// ✅ 快 - 预缓存偏移
class FMyChannelWriter : public FNDCWriterBase
{
    TNDCAccessor<FVector> Position{FName("Position"), this, true};
};
```

### 4. 合理设置 Tick Group

```cpp
// 在物理前写入
Channel->FinalWriteTickGroup = TG_PrePhysics;

// 保留上一帧数据用于平滑
Channel->bKeepPreviousFrameData = true;
```

### 5. 批量写入

```cpp
// ✅ 一次性写入多个元素
UNiagaraDataChannelWriter* Writer = WriteToNiagaraDataChannel(
    Context, Channel, AccessContext, Count);
for (int32 i = 0; i < Count; i++)
{
    Writer->WriteVector("Position", i, Positions[i]);
}

// ❌ 多次单元素写入
for (int32 i = 0; i < Count; i++)
{
    UNiagaraDataChannelWriter* Writer = WriteToNiagaraDataChannel(
        Context, Channel, AccessContext, 1);  // 每次创建新的
    Writer->WriteVector("Position", 0, Positions[i]);
}
```

---

## 扩展指南

### 创建自定义 Channel 类型

1. **定义 Channel 资产**：

```cpp
UCLASS()
class UNiagaraDataChannel_MyCustom : public UNiagaraDataChannel
{
    UPROPERTY(EditDefaultsOnly, Category = "Settings")
    float MyCustomSetting;
    
    virtual UNiagaraDataChannelHandler* CreateHandler(UWorld* OwningWorld) const override
    {
        return NewObject<UNiagaraDataChannelHandler_MyCustom>(OwningWorld);
    }
};
```

2. **定义 Handler**：

```cpp
UCLASS()
class UNiagaraDataChannelHandler_MyCustom : public UNiagaraDataChannelHandler
{
    // 自定义数据结构
    TMap<FMyCustomKey, FNiagaraDataChannelDataPtr> DataMap;
    
    virtual FNiagaraDataChannelDataPtr FindData(
        FNDCAccessContextInst& AccessContext,
        ENiagaraResourceAccess AccessType) override
    {
        FMyCustomKey Key = GenerateKey(AccessContext);
        if (!DataMap.Contains(Key))
        {
            DataMap.Add(Key, CreateData());
        }
        return DataMap[Key];
    }
};
```

3. **定义访问上下文**：

```cpp
USTRUCT(BlueprintType)
struct FNDCAccessContext_MyCustom : public FNDCAccessContextBase
{
    UPROPERTY(BlueprintReadWrite)
    FMyCustomKey CustomKey;
};
```

---

## 与 DataInterface 对比

| 特性 | Data Channel | DataInterface |
|------|--------------|---------------|
| 通信方向 | 双向 | 单向 (系统→模块) |
| 数据源 | Blueprint/C++ | 引擎资源 (纹理、网格等) |
| 空间感知 | Islands/Map | 无 (需手动处理) |
| 订阅机制 | Delegate 回调 | 无 |
| 性能开销 | 较低 (批量处理) | 取决于具体类型 |
| 适用场景 | 跨系统通信 | 资源访问 |
