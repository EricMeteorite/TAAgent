# TAAgent 参考资料索引

这个目录保存了从旧的 `.codebuddy` 目录中迁移出来、对项目本身仍有参考价值的文档。

迁移原则如下：

- 保留通用技术参考、工作流说明、分析笔记。
- 丢弃 CodeBuddy 专用的 agent 配置、rules、SKILL 封装和 prompt 元信息。
- 尽量按主题重组目录，避免继续沿用 `.codebuddy` 的工具专属结构。

当前目录说明：

- `renderdoc/`: RenderDoc 逆向分析、顶点格式、案例分析。
- `mcp-tools/`: RenderDoc MCP 与 Unreal Render MCP 的工具说明。
- `niagara/`: Niagara 架构、模块、模板与 Fluids 相关分析。
- `materials/`: 材质节点与材质函数参考。
- `rendering/`: UE 渲染管线专题分析。
- `lookdev/`: LookDev、HDRI、物理灯光与常用 Actor 参数参考。
- `ue-cli/`: 基于 UE Python/CLI 的参考资料。

说明：

- 这些文档主要是项目内技术参考资料，不再依赖 CodeBuddy 才能使用。
- 如果后续发现其中某些内容与正式项目文档重复，可以再继续合并到 `README.md` 或 `docs/` 下的专题文档。